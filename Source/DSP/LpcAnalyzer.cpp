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

float LpcAnalyzer::analyzeFrame(const float* x, int order, float* kOut, double gamma) noexcept
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
    int reached = 0;                  // 実際に完了した次数（E打ち切り対応）

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

        reached = i;
        E *= (1.0 - ki * ki);
        if (E < 1e-9)
        {
            E = std::max(E, 0.0);
            break;                    // 以降の k は 0 のまま（前フレーム保持はしない）
        }
    }

    // 4b. 帯域拡張（M3 character→γ）: a_k ← a_k·γ^k で極半径を γ 倍に縮小。
    //     ラティス合成は反射係数 k を使うため、拡張後の a を step-down で k へ再変換する。
    //     γ<1 は極を単位円内へ縮めるので |k|<1 が保たれ安定。
    if (gamma < 0.99999 && reached >= 1)
    {
        double ae[kMaxOrder + 1] = {};
        double g = 1.0;
        for (int i = 1; i <= reached; ++i) { g *= gamma; ae[i] = a[i] * g; }

        // step-down 再帰（Levinson の逆写像）: 各段の反射係数 = その段の最高次係数
        for (int i = reached; i >= 1; --i)
        {
            double ki = ae[i];
            ki = std::min(kReflClamp, std::max(-kReflClamp, ki));
            kOut[i - 1] = (float)ki;

            const double d = 1.0 - ki * ki;
            if (d < 1e-9)
                break;                // 数値的に不安定な段以降は現状の k を保持
            double aprev[kMaxOrder + 1] = {};
            for (int j = 1; j < i; ++j)
                aprev[j] = (ae[j] - ki * ae[i - j]) / d;
            for (int j = 1; j < i; ++j)
                ae[j] = aprev[j];
        }
    }

    // 5. ゲイン G = sqrt(E_P)
    return (float)std::sqrt(std::max(E, 0.0));
}
