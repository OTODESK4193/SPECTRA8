// ==========================================
// File: Limiter.h
// 最終段ブリックウォール・リミッター
//  - 天井(Ceiling)は内部固定 -0.1 dBFS。出力は数学的に必ず天井以下。
//  - 瞬間アタック(そのサンプルが天井を超えるなら即座にゲインを下げて捕捉)
//    → 突発的なピークも取りこぼさず天井へ抑える。
//  - リリースは緩やか(ポンピング抑制)。最終段にハードクリップの保険。
//  - レイテンシ0(先読みなし)のためPDC不要。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <cmath>

class BrickLimiter
{
public:
    // 内部天井: -0.1 dBFS = 10^(-0.1/20)
    static constexpr float kCeiling = 0.988553f;

    void prepare(double sampleRate)
    {
        const double sr = juce::jmax(8000.0, sampleRate);
        // リリース 120ms(1極). アタックは瞬間(係数不要)。
        relCoef = (float)(1.0 - std::exp(-1.0 / (0.120 * sr)));
        gain = 1.0f;
    }

    void reset() { gain = 1.0f; }

    // ceiling 引数は互換のため残すが、既定は内部天井 -0.1dBFS。
    void process(float* left, float* right, int numSamples, float ceiling = kCeiling)
    {
        const float ceil = juce::jlimit(0.01f, 1.0f, ceiling);
        for (int n = 0; n < numSamples; ++n)
        {
            const float peak = juce::jmax(std::abs(left[n]), std::abs(right[n]));

            // このサンプルを天井以下に収めるのに必要なゲイン
            const float gNeeded = (peak > ceil) ? (ceil / peak) : 1.0f;

            // 瞬間アタック(必要なら即座に下げる) / 緩やかリリース(ゆっくり戻す)
            if (gNeeded < gain)
                gain = gNeeded;                          // ピークを取りこぼさず捕捉
            else
                gain += relCoef * (gNeeded - gain);      // 復帰は緩やか

            // gain<=ceil/peak より |out|<=ceil が保証される。最終ハードクリップは浮動小数の保険。
            left[n]  = juce::jlimit(-ceil, ceil, left[n]  * gain);
            right[n] = juce::jlimit(-ceil, ceil, right[n] * gain);
        }
    }

private:
    float relCoef = 0.0f;
    float gain = 1.0f;
};
