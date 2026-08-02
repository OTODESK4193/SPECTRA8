// ==========================================
// File: AirBand.h
// 高域「エア」バンド合成 (2026-08-02 / 案A)
//
//  ボコーダーの内部処理は 16kHz なので、原理的に 8kHz より上が一切出ない
//  (ナイキスト)。実測すると入力音声のエネルギーの約8%が 8-12kHz にあり、
//  ここは歯擦音の抜けと息の空気感を決める帯域なので、失うと数字以上に
//  「こもった」印象になる。
//
//  本モジュールは内部レートを上げずにこの帯域を取り戻す:
//    1. 原音(ホストSR)を 7.5kHz 以上の3バンドに分ける
//    2. バンド毎に包絡を追従する (att 2ms / rel 40ms)
//    3. 同じバンドフィルタを通した白色雑音を、その包絡の形で鳴らす
//
//  【なぜ3バンドに分けるか】
//  1バンド(広帯域包絡×フラットな雑音)で試すと、声の高域が 10kHz 以上で
//  急に落ちるのに対し合成側はナイキストまで平坦なため、実測で
//  14-20kHz が +12.8dB 過剰・肝心の 8-10kHz は +1.4dB しか戻らなかった。
//  バンド毎に包絡を持たせると、声そのもののロールオフに自動で追従する。
//
//  原音そのものを混ぜるのではなく包絡だけを借りて完全に合成するので、
//  DRY の漏れにはならない。8kHz より上は聴覚がピッチを知覚しないため、
//  雑音で置き換えるのは知覚的にも妥当 (コーデックのSBR、および
//  Roland VP-330 系ボコーダーの最上段ノイズバンドと同じ考え方)。
//
//  L/R は独立した乱数列を使うので、エア成分だけ自然に広がる。
//
//  JUCE非依存・アロケーションなし・RTセーフ。
// ==========================================
#pragma once

#include <cmath>
#include <algorithm>

class AirBand
{
public:
    // バンド境界。下端はボコーダー出力の帯域上限 (ResampleFilter 7.6kHz) に合わせる。
    static constexpr int   kNumBands = 3;
    // 上端を 17kHz で止める。それ以上は大半の人に聞こえない上、
    // 帯域が広いぶん合成雑音のエネルギーだけが増えて無駄にヘッドルームを食う。
    static constexpr float kEdges[kNumBands + 1] = { 7500.0f, 10000.0f, 13500.0f, 17000.0f };

    void prepare(double sampleRate) noexcept
    {
        mSr = (sampleRate > 8000.0) ? sampleRate : 44100.0;
        const double nyq = mSr * 0.5;

        for (int b = 0; b < kNumBands; ++b)
        {
            auto& d = mDesign[b];
            const double lo = std::min((double)kEdges[b], nyq * 0.9);
            const double hi = (double)kEdges[b + 1];

            makeHighpass(lo, d.hp);
            d.useLp = (hi < nyq * 0.95);
            if (d.useLp)
                makeLowpass(std::min(hi, nyq * 0.9), d.lp);

            // このバンドがサンプルレート的に成立しない (下端がナイキスト近傍) なら殺す
            d.active = (lo < nyq * 0.85);
        }

        // 包絡追従: att 2ms / rel 40ms。
        //  速すぎると歯擦音の粒がガサつき、遅すぎると子音に間に合わない。
        mAtt = 1.0f - (float)std::exp(-1.0 / (0.002 * mSr));
        mRel = 1.0f - (float)std::exp(-1.0 / (0.040 * mSr));

        // 【重要】バンド毎に「そのフィルタを通した白色雑音の実効値」を実測して
        //  1.0 に正規化しておく。帯域幅が狭いほど通過後の実効値は小さくなるので
        //  (7.5-10kHz なら √(2500/22050)=0.34、14kHz以上なら 0.60)、
        //  これを補正しないと狭いバンドだけ音が小さくなる。
        //  実際に測ったフィルタ応答をそのまま使うので、サンプルレートが変わっても正しい。
        measureNoiseGain();

        reset();
    }

    void reset() noexcept
    {
        for (auto& b : mState)
        {
            b.dry = {};
            b.nzL = {};
            b.nzR = {};
            b.env = 0.0f;
        }
        mRngL = 0x2545F491u;
        mRngR = 0x9E3779B9u;
    }

    // dryMono : 原音 (エア成分の抽出元)
    // outL/R  : ここへ加算する (ボコーダー出力)
    // amount  : 0..1。1.0 で「原音のエア帯域と同じレベル」
    inline void process(float dryMono, float& outL, float& outR, float amount) noexcept
    {
        // 乱数は amount によらず毎サンプル進める (0%→上げた瞬間に不連続にならない)
        const float nL = nextNoise(mRngL);
        const float nR = nextNoise(mRngR);

        float addL = 0.0f, addR = 0.0f;

        for (int b = 0; b < kNumBands; ++b)
        {
            const auto& d = mDesign[b];
            if (!d.active)
                continue;
            auto& s = mState[b];

            // 1) 原音のこのバンドの包絡
            const float hp = bandFilter(dryMono, d, s.dry);
            const float mag = std::abs(hp);
            s.env += (mag > s.env ? mAtt : mRel) * (mag - s.env);

            if (amount <= 0.0001f)
                continue;   // フィルタと包絡は回し続ける

            // 2) 同じバンドの白色雑音を包絡の形で鳴らす。
            //    整流平均(env) → 実効値の換算 √(π/2)=1.2533 と、
            //    そのバンドを通った雑音を単位実効値へ揃える正規化を掛ける。
            const float g = s.env * 1.2533141f * d.noiseNorm;
            addL += bandFilter(nL, d, s.nzL) * g;
            addR += bandFilter(nR, d, s.nzR) * g;
        }

        outL += addL * amount;
        outR += addR * amount;
    }

private:
    struct Biquad { float x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
    struct Coeffs { float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

    struct Design
    {
        Coeffs hp {};
        Coeffs lp {};
        bool   useLp = false;
        bool   active = false;
        float  noiseNorm = 1.0f;   // このバンドを通した白色雑音を実効値1へ揃える係数
    };
    struct BandState
    {
        // [0][1]=HP 2段(24dB/oct), [2][3]=LP 2段。
        //  12dB/oct では隣のバンドの漏れが大きく、特に最上段の包絡が
        //  8-12kHz の強い成分に引っ張られて実測 +17dB も過剰になった。
        struct Pair { Biquad f[4]; };
        Pair dry {}, nzL {}, nzR {};
        float env = 0.0f;
    };

    // 各バンドのフィルタに白色雑音を通し、実効値が 1.0 になる係数を求める。
    // prepare() からのみ呼ぶ (非リアルタイム)。
    void measureNoiseGain() noexcept
    {
        constexpr int kWarm = 2048;
        constexpr int kMeas = 32768;
        for (int b = 0; b < kNumBands; ++b)
        {
            auto& d = mDesign[b];
            d.noiseNorm = 1.0f;
            if (!d.active)
                continue;

            unsigned int rng = 0x1234567u + (unsigned int)b * 7919u;
            BandState::Pair st {};
            for (int i = 0; i < kWarm; ++i)
                bandFilter(nextNoise(rng), d, st);

            double acc = 0.0;
            for (int i = 0; i < kMeas; ++i)
            {
                const double v = (double)bandFilter(nextNoise(rng), d, st);
                acc += v * v;
            }
            const double rms = std::sqrt(acc / (double)kMeas);
            d.noiseNorm = (rms > 1.0e-9) ? (float)(1.0 / rms) : 0.0f;
        }
    }

    void makeHighpass(double fc, Coeffs& c) noexcept
    {
        const double w0 = 6.283185307179586 * fc / mSr;
        const double cw = std::cos(w0), sw = std::sin(w0);
        const double alpha = sw / (2.0 * 0.70710678);
        const double a0 = 1.0 + alpha;
        c.b0 = (float)(((1.0 + cw) * 0.5) / a0);
        c.b1 = (float)((-(1.0 + cw)) / a0);
        c.b2 = c.b0;
        c.a1 = (float)((-2.0 * cw) / a0);
        c.a2 = (float)((1.0 - alpha) / a0);
    }
    void makeLowpass(double fc, Coeffs& c) noexcept
    {
        const double w0 = 6.283185307179586 * fc / mSr;
        const double cw = std::cos(w0), sw = std::sin(w0);
        const double alpha = sw / (2.0 * 0.70710678);
        const double a0 = 1.0 + alpha;
        c.b0 = (float)(((1.0 - cw) * 0.5) / a0);
        c.b1 = (float)((1.0 - cw) / a0);
        c.b2 = c.b0;
        c.a1 = (float)((-2.0 * cw) / a0);
        c.a2 = (float)((1.0 - alpha) / a0);
    }

    static inline float biquad(float in, const Coeffs& c, Biquad& s) noexcept
    {
        const float y = c.b0 * in + c.b1 * s.x1 + c.b2 * s.x2 - c.a1 * s.y1 - c.a2 * s.y2;
        s.x2 = s.x1; s.x1 = in;
        s.y2 = s.y1; s.y1 = y + 1.0e-20f;   // デノーマル対策
        return y;
    }
    static inline float bandFilter(float in, const Design& d, BandState::Pair& p) noexcept
    {
        float v = biquad(biquad(in, d.hp, p.f[0]), d.hp, p.f[1]);   // HP 24dB/oct
        if (d.useLp)
            v = biquad(biquad(v, d.lp, p.f[2]), d.lp, p.f[3]);      // LP 24dB/oct
        return v;
    }

    static inline float nextNoise(unsigned int& st) noexcept
    {
        st ^= st << 13; st ^= st >> 17; st ^= st << 5;
        return (float)((int)st) * (1.0f / 2147483648.0f);   // -1..1 一様
    }

    double mSr = 44100.0;
    float mAtt = 0.0f, mRel = 0.0f;
    Design mDesign[kNumBands] {};
    BandState mState[kNumBands] {};
    unsigned int mRngL = 0x2545F491u;
    unsigned int mRngR = 0x9E3779B9u;
};
