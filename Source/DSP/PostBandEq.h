// ==========================================
// File: PostBandEq.h
// LPCモード用ポストEQ（グラフィックEQ）
//
//   BANDS EQ の帯域ゲイン(mBandGains)を、LPC合成後のステレオ出力へ
//   ピーキングフィルタ(RBJ biquad)のカスケードとして適用する。
//
//  - 中心周波数は FilterbankVocoder と同一の mel 配置(80-7500Hz, 48点)。
//    → BANDS EQ のメーター/カーブ表示と周波数が完全に一致する。
//  - 全バンド 0dB (リニア 1.0) のとき数学的に透過(パススルー)。
//  - Q は隣接バンド間隔から自動算出。バンド数が少ないほど広い(なだらか)。
//  - JUCE非依存・アロケーションなし・RTセーフ(ヘッダオンリー)。
//    係数はブロックレートで updateCoeffs()、音声処理は毎サンプル process()。
// ==========================================
#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <algorithm>

class PostBandEq
{
public:
    static constexpr int kMaxBands = 48;

    // 非RT文脈で呼ぶ（サンプルレート設定 + 初期レイアウト）
    void prepare(double sampleRate) noexcept
    {
        mSr = (sampleRate > 1000.0) ? sampleRate : 16000.0;
        mLayoutBands = 0;              // 次の rebuildLayout を必ず走らせる
        rebuildLayout(kMaxBands);
        reset();
    }

    // 中心周波数とQを「アクティブなバンド数」で再スパンする。
    //
    //  【重要】以前は常に48分割の先頭 bands 本だけを使っていたため、
    //  BANDS=12 のとき EQ が実際に効くのは 80〜653Hz だけで、
    //  画面表示(FilterbankVocoder と同じ 80〜7500Hz の12分割)と対応が取れていなかった。
    //  例: 画面上 2114Hz のバンドを動かすと、実際には 353Hz が動いていた。
    //  FilterbankVocoder::rebuildLayout と同じ設計に揃える。
    void rebuildLayout(int bands) noexcept
    {
        bands = std::min(kMaxBands, std::max(8, bands));
        if (bands == mLayoutBands)
            return;
        mLayoutBands = bands;

        const float fMin = 80.0f, fMax = 7500.0f;
        const float mMin = 2595.0f * std::log10(1.0f + fMin / 700.0f);
        const float mMax = 2595.0f * std::log10(1.0f + fMax / 700.0f);
        for (int i = 0; i < bands; ++i)
        {
            const float mv = mMin + (mMax - mMin) * ((float)i / (float)(bands - 1));
            mF0[(size_t)i] = 700.0f * (std::pow(10.0f, mv / 2595.0f) - 1.0f);
        }

        // Q = 中心±半バンドが -3dB で交差する程度。隣接バンドの比から算出。
        // バンド数が少ないほど間隔が広い = Qが低い(なだらか)になり、全域が隙間なく覆われる。
        for (int i = 0; i < bands; ++i)
        {
            const float lo = (i > 0) ? mF0[(size_t)(i - 1)]
                                     : mF0[0] * mF0[0] / mF0[1];                       // 対数外挿
            const float hi = (i < bands - 1) ? mF0[(size_t)(i + 1)]
                                     : mF0[(size_t)i] * mF0[(size_t)i] / mF0[(size_t)(i - 1)];
            float bwOct = std::log2(std::sqrt(hi / lo));   // 全幅で約1バンド分のオクターブ幅
            bwOct = std::max(0.05f, bwOct);
            const float twoBW = std::pow(2.0f, bwOct);
            mQ[(size_t)i] = std::sqrt(twoBW) / (twoBW - 1.0f);
        }

        // 【変更】ここで reset() すると、BANDS を1つ動かしただけで全biquadの状態が
        // ゼロへ飛び、信号が不連続になって「プツッ」と鳴っていた。
        // ピーキングEQの状態を引き継いだ方が連続性が保たれ、実害も無いため残す。
        mGainPrimed = false;   // ゲイン平滑だけ新レイアウトで取り直す
    }

    void reset() noexcept
    {
        for (auto& s : mStL) s.reset();
        for (auto& s : mStR) s.reset();
    }

    // ブロックレートで係数を更新する。bandGains はリニア(1.0 = 0dB)。
    //  smoothCoef: 帯域ゲインの1極平滑係数 (0〜1)。呼び出し側がブロック長から
    //  coef = 1 - exp(-blockSec / 0.02) として渡す。1.0 で平滑なし(従来動作)。
    //  EQ をドラッグ中はゲインがブロック毎に階段状に跳んでジッパーノイズになるため。
    void updateCoeffs(int bandCount,
                      const std::array<std::atomic<float>, kMaxBands>& bandGains,
                      float smoothCoef = 1.0f) noexcept
    {
        mActive = std::min(kMaxBands, std::max(8, bandCount));
        rebuildLayout(mActive);   // バンド数が変わった時だけ再スパン(内部で早期return)

        const float sc = std::min(1.0f, std::max(0.0f, smoothCoef));
        constexpr float pi = 3.14159265358979f;
        for (int i = 0; i < mActive; ++i)
        {
            const float target = std::max(1.0e-4f, bandGains[(size_t)i].load());
            float& g = mGainSm[(size_t)i];
            g = mGainPrimed ? (g + sc * (target - g)) : target;

            float gLin = std::max(1.0e-4f, g);         // -80dB 下限
            const float A = std::sqrt(gLin);           // ピーキング: A = 10^(dB/40)

            float w0 = 2.0f * pi * mF0[(size_t)i] / (float)mSr;
            w0 = std::min(w0, 3.13f);                   // ナイキスト手前でクランプ
            const float cw = std::cos(w0);
            const float sw = std::sin(w0);
            const float alpha = sw / (2.0f * mQ[(size_t)i]);

            const float a0  = 1.0f + alpha / A;
            const float inv = 1.0f / a0;
            Coeff& c = mC[(size_t)i];
            c.b0 = (1.0f + alpha * A) * inv;
            c.b1 = (-2.0f * cw)       * inv;
            c.b2 = (1.0f - alpha * A) * inv;
            c.a1 = (-2.0f * cw)       * inv;
            c.a2 = (1.0f - alpha / A) * inv;
        }
        mGainPrimed = true;
    }

    // 毎サンプル・ステレオ処理（LPC合成後に呼ぶ）
    inline void process(float& l, float& r) noexcept
    {
        for (int i = 0; i < mActive; ++i)
        {
            l = mStL[(size_t)i].tick(mC[(size_t)i], l);
            r = mStR[(size_t)i].tick(mC[(size_t)i], r);
        }
    }

private:
    struct Coeff { float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f; };

    struct State
    {
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
        void reset() noexcept { x1 = x2 = y1 = y2 = 0.0f; }
        inline float tick(const Coeff& c, float x) noexcept
        {
            const float y = c.b0 * x + c.b1 * x1 + c.b2 * x2 - c.a1 * y1 - c.a2 * y2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return y;
        }
    };

    double mSr = 16000.0;
    int mActive = 0;
    int mLayoutBands = 0;   // 現在のレイアウトが何バンド用か
    std::array<float, kMaxBands> mF0 {};
    std::array<float, kMaxBands> mQ {};
    std::array<Coeff, kMaxBands> mC {};
    std::array<State, kMaxBands> mStL {};
    std::array<State, kMaxBands> mStR {};

    // 帯域ゲインの平滑値 (ブロック毎に1極で追従)
    std::array<float, kMaxBands> mGainSm {};
    bool mGainPrimed = false;
};
