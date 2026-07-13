// ==========================================
// File: LpcAnalyzer.cpp
// フェーズ2計画書v2 §5.2 準拠（M0 Pythonプロトタイプとゴールデンテストで一致検証済み）
// ==========================================
#include "LpcAnalyzer.h"

void LpcAnalyzer::prepare()
{
    constexpr double twoPi = 6.283185307179586476925286766559;
    const double Nm1 = (double)(kWindowSize - 1);

    for (int n = 0; n < kWindowSize; ++n)
    {
        const double t = (double)n / Nm1;
        mWindows[Hann][(size_t)n]     = (float)(0.5 - 0.5 * std::cos(twoPi * t));
        mWindows[Hamming][(size_t)n]  = (float)(0.54 - 0.46 * std::cos(twoPi * t));
        mWindows[Blackman][(size_t)n] = (float)(0.42 - 0.5 * std::cos(twoPi * t)
                                               + 0.08 * std::cos(2.0 * twoPi * t));
    }

    for (int t = 0; t < 3; ++t)
    {
        double e = 0.0;
        for (int n = 0; n < kWindowSize; ++n)
            e += (double)mWindows[(size_t)t][(size_t)n] * (double)mWindows[(size_t)t][(size_t)n];
        mWinEnergy[(size_t)t] = (float)e;
    }

    // ラグ窓: r(j) *= exp(-1/2 (2πσj/fs)^2), σ=50Hz
    for (int j = 0; j <= kMaxOrder; ++j)
    {
        const double arg = twoPi * kSigmaLagHz * (double)j / kInternalSampleRate;
        mLagWindow[(size_t)j] = std::exp(-0.5 * arg * arg);
    }
}

float LpcAnalyzer::analyzeFrame(const float* x, int order, float* kOut) noexcept
{
    order = std::min(kMaxOrder, std::max(1, order));

    for (int i = 0; i < order; ++i)
        kOut[i] = 0.0f;

    // 1. 窓掛け（double へ昇格）
    const auto& w = mWindows[(size_t)mWindowType];
    for (int n = 0; n < kWindowSize; ++n)
        mScratch[(size_t)n] = (double)x[n] * (double)w[(size_t)n];

    // 2. 自己相関 r(0..P)
    double r[kMaxOrder + 1];
    for (int j = 0; j <= order; ++j)
    {
        double acc = 0.0;
        const int lim = kWindowSize - j;
        for (int n = 0; n < lim; ++n)
            acc += mScratch[(size_t)n] * mScratch[(size_t)(n + j)];
        r[j] = acc;
    }

    // 無音フレーム: k全0 (透過化), G=0（§5.2-6）
    if (r[0] < kSilenceThresh)
        return 0.0f;

    // 3. 数値衛生: 白色雑音補正 + ラグ窓
    r[0] = r[0] * 1.0001 + 1e-9;
    for (int j = 0; j <= order; ++j)
        r[j] *= mLagWindow[(size_t)j];

    // 4. Levinson-Durbin（kクランプ、E打ち切り）
    double a[kMaxOrder + 1] = {};     // a[1..i]、A(z)=1+Σa_i z^-i
    double anew[kMaxOrder + 1] = {};
    double E = r[0];

    for (int i = 1; i <= order; ++i)
    {
        double acc = r[i];
        for (int j = 1; j < i; ++j)
            acc += a[j] * r[i - j];

        double ki = -acc / E;
        ki = std::min(kReflClamp, std::max(-kReflClamp, ki));
        kOut[i - 1] = (float)ki;

        for (int j = 1; j < i; ++j)
            anew[j] = a[j] + ki * a[i - j];
        anew[i] = ki;
        for (int j = 1; j <= i; ++j)
            a[j] = anew[j];

        E *= (1.0 - ki * ki);
        if (E < 1e-9)
        {
            E = std::max(E, 0.0);
            break;                    // 以降の k は 0 のまま（前フレーム保持はしない）
        }
    }

    // 5. ゲイン G = sqrt(E_P)
    return (float)std::sqrt(std::max(E, 0.0));
}
