// ==========================================
// File: Limiter.h
// シンプルな最終段ブリックリミッター（開発計画書 第2版 ①準拠）
// ピーク追従型ゲインリダクション（att 1ms / rel 80ms）+ ハードクリップ保険
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <cmath>

class BrickLimiter
{
public:
    void prepare(double sampleRate)
    {
        const double sr = juce::jmax(8000.0, sampleRate);
        attCoef = (float)(1.0 - std::exp(-1.0 / (0.001 * sr)));
        relCoef = (float)(1.0 - std::exp(-1.0 / (0.080 * sr)));
        envelope = 0.0f;
    }

    void reset() { envelope = 0.0f; }

    void process(float* left, float* right, int numSamples, float ceilingLinear = 0.985f)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            const float peak = juce::jmax(std::abs(left[n]), std::abs(right[n]));
            envelope += (peak > envelope ? attCoef : relCoef) * (peak - envelope);

            float gain = 1.0f;
            if (envelope > ceilingLinear)
                gain = ceilingLinear / envelope;

            left[n] = juce::jlimit(-1.2f, 1.2f, left[n] * gain);
            right[n] = juce::jlimit(-1.2f, 1.2f, right[n] * gain);
        }
    }

private:
    float attCoef = 0.0f, relCoef = 0.0f;
    float envelope = 0.0f;
};
