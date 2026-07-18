// ==========================================
// File: FilterbankVocoder.h
// アナログ風フィルターバンク・ボコーダー (スカラー設計)
//
//  - BPF Bank (2次SVFカスケード) / Subtractive LR4 (4次Linkwitz-Riley)
//  - 8〜48バンドの動的追従
//  - フォルマント・シフト/ストレッチによる合成側フィルタ周波数の制御
//  - 帯域交互ステレオ・パンニング
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include <cmath>

class FilterbankVocoder
{
public:
    static constexpr int kMaxBands = 48;
    static constexpr double kInternalSampleRate = 16000.0;

    FilterbankVocoder();
    ~FilterbankVocoder() = default;

    void prepare(double sampleRate);
    void reset();

    // 1サンプル処理
    // modulator: 分析側に入力する音声サンプル
    // carrierL/R: 合成側のステレオキャリア入力サンプル
    // outL/R: ボコーディング後のステレオ出力サンプル (書き戻し)
    // resonance: バンド幅スケール (0.3=太い/緩い 〜 1.0=標準 〜 3.0=鋭い)。
    //            バンド毎の基準Q(バンド間隔連動)に乗算される。BPF Bankで有効。
    void processSample(float modulator, float carrierL, float carrierR,
                       float& outL, float& outR,
                       int bandCount, float character, float resonance,
                       float formantShift, float formantStretch,
                       int filterbankType, float stereoWidth,
                       const std::array<std::atomic<float>, kMaxBands>& bandGains,
                       std::array<std::atomic<float>, kMaxBands>& bandLevelsForUi) noexcept;

    // 分析専用パス (合成は行わない)。
    // LPCモードのようにフィルターバンク合成を実行しないときでも、
    // BANDS EQ のアナライザー(入力音声の帯域レベルメーター)を動かし続けるために使う。
    // 内部の分析フィルタ状態と mEnvValues、そして bandLevelsForUi のみを更新する。
    void analyzeForMeter(float modulator, int bandCount, float character, float resonance,
                         int filterbankType,
                         std::array<std::atomic<float>, kMaxBands>& bandLevelsForUi) noexcept
    {
        const int activeBands = juce::jlimit(8, kMaxBands, bandCount);
        for (int i = 0; i < activeBands; ++i)
        {
            updateAnalysisBand(i, modulator, filterbankType, character, resonance);
            bandLevelsForUi[(size_t)i].store(mEnvValues[(size_t)i] * kMeterGain);
        }
    }

private:
    // 分析側(モジュレーター)の1バンド分の処理。mEnvValues[i] を更新する。
    // processSample と analyzeForMeter の両方から呼ばれる共通ロジック。
    // mEnvValues は「正規化された帯域振幅」(中心利得0dBのフィルタ出力包絡) を保持する。
    inline void updateAnalysisBand(int i, float modulator, int filterbankType,
                                   float character, float resonance) noexcept
    {
        const float f0 = mBandF0[(size_t)i];
        // バンド間隔連動Q (定オーバーラップ設計) × Resonanceスケール。
        // melスケール配置では低域の間隔が狭く高域は広いため、固定Qだと
        // 低域に感度の谷・高域に過剰オーバーラップが生じる。バンド幅を
        // 隣接エッジ幅に連動させ、全帯域で均一なカバレッジにする。
        const float Q = juce::jlimit(1.0f, 40.0f, mBandQ[(size_t)i] * resonance);

        float analOut = 0.0f;
        float g_anal, k_anal, a1_anal;
        computeFilterCoeffs(f0, Q, g_anal, k_anal, a1_anal);

        if (filterbankType == 0) // Bandpass Bank
        {
            // ZDF SVF 1段目 (BPF)
            auto& s1 = mAnalSvf[(size_t)i][0];
            float v1 = a1_anal * (s1.s1 + g_anal * (modulator - s1.s2));
            float y_bp = v1;
            float y_lp = s1.s2 + g_anal * v1;
            s1.s1 = 2.0f * y_bp - s1.s1;
            s1.s2 = 2.0f * y_lp - s1.s2;

            // ZDF SVF 2段目 (BPF)
            auto& s2 = mAnalSvf[(size_t)i][1];
            float v1_2 = a1_anal * (s2.s1 + g_anal * (y_bp - s2.s2));
            float y_bp_2 = v1_2;
            float y_lp_2 = s2.s2 + g_anal * v1_2;
            s2.s1 = 2.0f * y_bp_2 - s2.s1;
            s2.s2 = 2.0f * y_lp_2 - s2.s2;

            // 中心利得正規化: SVF-BPカスケードの中心利得はQ²。1/Q²を掛けて
            // 中心0dBに揃える(Q可変化してもバンド間・Resonance変更時のレベルが暴れない)。
            analOut = y_bp_2 / (Q * Q);
        }
        else // Subtractive (バンド端エッジの8次HPF→8次LPF直列 + ピーク正規化)
        {
            auto& sub = mAnalSub[(size_t)i];
            const float gLo  = mEdgeG[(size_t)i];      // 下端エッジ (HPF)
            const float a1Lo = mEdgeA1[(size_t)i];
            const float gHi  = mEdgeG[(size_t)i + 1];  // 上端エッジ (LPF)
            const float a1Hi = mEdgeA1[(size_t)i + 1];

            float y = modulator;
            for (int sec = 0; sec < 4; ++sec) y = processLpfSection(sub.hiLp[(size_t)sec], y, gHi, a1Hi);
            for (int sec = 0; sec < 4; ++sec) y = processHpfSection(sub.loHp[(size_t)sec], y, gLo, a1Lo);

            // 正規化(ピーク0dB)。タイプ間の聴感レベル整合は出力側メイクアップで行う。
            analOut = y * mBandNorm[(size_t)i];
        }

        // エンベロープ追従（キャラクター値でアタック/リリースタイムを調整）
        float env = std::abs(analOut);
        float att = 0.005f + character * 0.045f;
        float rel = 0.001f + character * 0.009f;

        float coeff = (env > mEnvValues[(size_t)i]) ? att : rel;
        mEnvValues[(size_t)i] += coeff * (env - mEnvValues[(size_t)i]);
    }

    struct SVFState
    {
        float s1 = 0.0f;
        float s2 = 0.0f;
        void reset() { s1 = 0.0f; s2 = 0.0f; }
    };

    // 出力メイクアップ (タイプ別)。分析・合成とも中心利得0dBに正規化したため、
    // 旧実装(BPF: 分析Q²×合成Q²×トリム0.003236 / Sub: 分析×256×トリム)と
    // 同一の最終音量になるよう等価換算した較正値。
    //   BPF: 100×100×0.003236 = 32.36  /  Sub: 256×0.003236 = 0.828
    static constexpr float kBpfMakeup    = 32.36f;
    static constexpr float kSubOutMakeup = 0.828f;

    // BANDS EQ メーター表示用ゲイン。正規化後の帯域振幅は旧実装のQ²(=100)倍
    // 小さくなったため、表示スケールを旧実装と揃える。
    static constexpr float kMeterGain = 100.0f;

    struct SubBandState
    {
        // 減算型バンド = 8次HPF(下端エッジ) → 8次LPF(上端エッジ) の直列 + ピーク正規化
        // (2次Butterworth Q=0.707 を4段カスケード、スロープ48dB/oct)
        std::array<SVFState, 4> hiLp; // 上端エッジ用 LPF セクション
        std::array<SVFState, 4> loHp; // 下端エッジ用 HPF セクション
        void reset() { for (auto& s : hiLp) s.reset(); for (auto& s : loHp) s.reset(); }
    };

    void computeFilterCoeffs(float fc, float Q, float& g, float& k, float& a1) noexcept
    {
        g = std::tan(3.14159265f * fc / (float)kInternalSampleRate);
        k = 1.0f / Q;
        a1 = 1.0f / (1.0f + g * (g + k));
    }

    // 2次Butterworth (Q=0.707) LPFセクション (ZDF SVF)。2段カスケードでLR4-LPFを構成する。
    static float processLpfSection(SVFState& s, float x, float g, float a1) noexcept
    {
        const float v1 = a1 * (s.s1 + g * (x - s.s2));
        const float lp = s.s2 + g * v1;
        s.s1 = 2.0f * v1 - s.s1;
        s.s2 = 2.0f * lp - s.s2;
        return lp;
    }

    // 2次Butterworth (Q=0.707) HPFセクション (ZDF SVF)。2段カスケードでLR4-HPFを構成する。
    static float processHpfSection(SVFState& s, float x, float g, float a1) noexcept
    {
        const float v1 = a1 * (s.s1 + g * (x - s.s2));
        const float lp = s.s2 + g * v1;
        const float hp = x - 1.41421356f * v1 - lp;
        s.s1 = 2.0f * v1 - s.s1;
        s.s2 = 2.0f * lp - s.s2;
        return hp;
    }

    // バンド中心 (エッジの幾何平均) での利得を 0dB に揃える正規化ゲインの解析計算
    // Butterworth2次×2段: |LPF| = 1/(1+(f/fc)^4), |HPF| = (f/fc)^4/(1+(f/fc)^4)
    // 4段カスケード(8次)はその2乗
    static float computeBandNorm(float eLo, float eHi) noexcept
    {
        const float fc = std::sqrt(eLo * eHi);
        const float tl = fc / eLo, th = fc / eHi;
        const float tl2 = tl * tl, th2 = th * th;
        const float xl4 = tl2 * tl2, xh4 = th2 * th2;
        const float m1 = (xl4 / (1.0f + xl4)) * (1.0f / (1.0f + xh4));
        const float mag = m1 * m1; // 8次 = 4次特性の2乗
        return (mag > 0.005f) ? (1.0f / mag) : 200.0f; // 安全上限 +46dB
    }

    double mSampleRate = 44100.0;

    // 分析側バンド周波数
    std::array<float, kMaxBands> mBandF0 {};

    // Subtractive(減算型)用のバンド端周波数 (kMaxBands+1 エッジ) と分析側固定係数
    std::array<float, kMaxBands + 1> mBandEdges {};
    std::array<float, kMaxBands + 1> mEdgeG {};
    std::array<float, kMaxBands + 1> mEdgeA1 {};
    std::array<float, kMaxBands> mBandNorm {}; // 分析側ピーク正規化ゲイン
    std::array<float, kMaxBands> mBandQ {};    // バンド間隔連動の基準Q (f0/エッジ幅)

    // フィルタ状態変数
    std::array<std::array<SVFState, 2>, kMaxBands> mAnalSvf {}; // 2段カスケード用
    std::array<SubBandState, kMaxBands> mAnalSub {};

    std::array<std::array<SVFState, 2>, kMaxBands> mSynthSvfL {};
    std::array<std::array<SVFState, 2>, kMaxBands> mSynthSvfR {};
    std::array<SubBandState, kMaxBands> mSynthSubL {};
    std::array<SubBandState, kMaxBands> mSynthSubR {};

    // 包絡線追従
    std::array<float, kMaxBands> mEnvValues {};
};
