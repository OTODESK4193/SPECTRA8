// ==========================================
// File: LpcLattice.h
// 全極ラティス（格子）合成フィルタ（ヘッダオンリー、フェーズ2計画書v2 §5.3）
//
//   f = u(n)
//   for i = P..1:  f = f − k_i·b[i−1];  b[i] = k_i·f + b[i−1]
//   y(n) = f;  b[0] = f
//
// 安定性:
//  - 時不変では全|k|<1 で構造的に安定（TMS5220 と同方式）
//  - 時変kのエネルギーポンピング対策として状態飽和 ±kStateSat を適用
//    （M0 Python敵対テストで発散を確認 → 飽和で構造的にBIBO有界化。
//      通常の音声動作では飽和は一切介入しない）
//  - デノーマル対策: b[0] に 1e-20 注入（juce::ScopedNoDenormals と併用）
//
// JUCE非依存・アロケーションなし・毎サンプル O(P)。
// ==========================================
#pragma once

#include <array>

class LpcLattice
{
public:
    static constexpr int   kMaxOrder = 16;
    static constexpr float kStateSat = 8.0f;

    void reset() noexcept { mB.fill(0.0f); }

    // u: 励起入力（G適用済み）、k: 反射係数[order]、order: 1..kMaxOrder
    inline float processSample(float u, const float* k, int order) noexcept
    {
        float f = u;
        for (int p = order; p >= 1; --p)
        {
            const float kp = k[p - 1];
            const float bPrev = mB[(size_t)(p - 1)];
            f -= kp * bPrev;
            mB[(size_t)p] = sat(kp * f + bPrev);
        }
        f = sat(f);
        mB[0] = f + 1e-20f;
        return f;
    }

private:
    static inline float sat(float v) noexcept
    {
        return (v > kStateSat) ? kStateSat : ((v < -kStateSat) ? -kStateSat : v);
    }

    std::array<float, kMaxOrder + 1> mB {};
};
