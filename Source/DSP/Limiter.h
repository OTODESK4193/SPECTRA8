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
        kMixCoef = (float)(1.0 - std::exp(-1.0 / (0.020 * sr)));   // ON/OFF 20ms
        gain = 1.0f;
        bypassMix = -1.0f;
    }

    void reset() { gain = 1.0f; bypassMix = -1.0f; }

    // ON/OFF をクロスフェードしながら処理する。
    //  enabled を直接分岐すると、リミッターが効いている最中に OFF にした瞬間
    //  ゲインが 1.0 へ跳んで「ボッ」と鳴っていた。20ms でリミット後/前を混ぜる。
    void processBlended(float* left, float* right, int numSamples, bool enabled) noexcept
    {
        const float target = enabled ? 1.0f : 0.0f;
        if (bypassMix < 0.0f)
            bypassMix = target;                      // 初回は即時反映

        // 完全に片側へ落ち着いていて目標も同じなら、余計な処理をしない
        if (bypassMix == target && (target == 0.0f))
        {
            // 【2026-08-02 修正】素通し中もゲインは 1.0 に戻しておく。
            //  以前はここで即 return していたため、リミッターが深く効いている最中に
            //  OFF にすると gain が小さいまま凍結し、次に ON へ戻した瞬間に
            //  その古いゲインから 120ms かけて復帰する = 音量が一瞬凹んでいた。
            gain = 1.0f;
            return;                                  // OFF で安定 = 素通し
        }

        for (int n = 0; n < numSamples; ++n)
        {
            const float dryL = left[n], dryR = right[n];

            const float peak = juce::jmax(std::abs(dryL), std::abs(dryR));
            const float gNeeded = (peak > kCeiling) ? (kCeiling / peak) : 1.0f;
            if (gNeeded < gain) gain = gNeeded;
            else                gain += relCoef * (gNeeded - gain);

            const float wetL = juce::jlimit(-kCeiling, kCeiling, dryL * gain);
            const float wetR = juce::jlimit(-kCeiling, kCeiling, dryR * gain);

            bypassMix += kMixCoef * (target - bypassMix);
            left[n]  = dryL * (1.0f - bypassMix) + wetL * bypassMix;
            right[n] = dryR * (1.0f - bypassMix) + wetR * bypassMix;
        }
        if (std::abs(bypassMix - target) < 1.0e-4f)
            bypassMix = target;
    }

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
    // ON/OFF クロスフェード用 (0=素通し / 1=リミット後)。-1 = 未初期化
    float bypassMix = -1.0f;
    float kMixCoef = 0.0f;   // 20ms 相当。prepare で算出
};
