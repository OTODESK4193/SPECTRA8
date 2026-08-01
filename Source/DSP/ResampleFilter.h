// ==========================================
// File: ResampleFilter.h
// 16kHz 内部処理のための帯域制限フィルタ (8次バターワース LPF / ホストレートで動作)
//
//  【なぜ必要か】
//   本機は内部を 16kHz で処理するため、入口で 1/3 (48kHz時) にデシメーションし、
//   出口で補間して戻す。ところが旧実装はどちらも「線形補間だけ」で、
//   帯域制限フィルタが一切入っていなかった。
//
//   入口: 8kHz を超える成分が 100% そのまま可聴域へ折り返す。
//         実測では 20kHz の純音が減衰ゼロで 4kHz に出現した。
//         声のサ行・息・シンバル等(5〜16kHz)が非調和なノイズに化け、
//         それがボコーダーの帯域分析を通って「ジリジリ」として鳴っていた。
//   出口: 16kHz の信号を線形補間で引き伸ばすと 16k±f にイメージが残り、
//         8〜16kHz に金属的な付帯音として乗る。
//
//   ※ PitchTracker は同じデシメーションの前に 6.8kHz の LPF を持っている
//     (PitchTracker.h)。メインのオーディオ経路にだけ無かった、という抜け。
//
//  【設計】
//   8次バターワース = RBJ の2次LPFを4段カスケード。48dB/oct。
//   カットオフは 7.6kHz (フィルターバンクの最上band 7.5kHz を残しつつ、
//   可聴域へ深く折り返す 12〜16kHz を 32〜52dB 落とす)。
//   ホストサンプルレートで動作させるので、デシメーション前/補間後に掛ける。
//   状態は1チャンネルぶん。ステレオは2つ用意すること。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>

class ResampleFilter
{
public:
    static constexpr int  kNumSections = 4;      // 2次 × 4 = 8次
    static constexpr float kDefaultCutoffHz = 7600.0f;

    void prepare(double hostSampleRate, float cutoffHz = kDefaultCutoffHz) noexcept
    {
        const double sr = juce::jmax(8000.0, hostSampleRate);

        // ナイキストに近すぎると係数が破綻するので上限を掛ける
        const double fc = juce::jlimit(1000.0, sr * 0.45, (double)cutoffHz);

        // 8次バターワースの各段のQ: Q_k = 1 / (2 cos((2k+1)π/16))
        static constexpr double kQ[kNumSections] = { 0.50979558, 0.60134489,
                                                     0.89997622, 2.56291545 };

        const double w0 = juce::MathConstants<double>::twoPi * fc / sr;
        const double cw = std::cos(w0);
        const double sw = std::sin(w0);

        for (int i = 0; i < kNumSections; ++i)
        {
            const double alpha = sw / (2.0 * kQ[i]);
            const double a0 = 1.0 + alpha;
            auto& c = mCoef[(size_t)i];
            c.b0 = (float)(((1.0 - cw) * 0.5) / a0);
            c.b1 = (float)((1.0 - cw) / a0);
            c.b2 = c.b0;
            c.a1 = (float)((-2.0 * cw) / a0);
            c.a2 = (float)((1.0 - alpha) / a0);
        }
        reset();
    }

    void reset() noexcept
    {
        for (auto& s : mState) { s.x1 = s.x2 = s.y1 = s.y2 = 0.0f; }
    }

    // 1サンプル処理 (ホストレート)
    inline float processSample(float x) noexcept
    {
        for (int i = 0; i < kNumSections; ++i)
        {
            const auto& c = mCoef[(size_t)i];
            auto& s = mState[(size_t)i];
            const float y = c.b0 * x + c.b1 * s.x1 + c.b2 * s.x2
                          - c.a1 * s.y1 - c.a2 * s.y2;
            s.x2 = s.x1; s.x1 = x;
            s.y2 = s.y1; s.y1 = y;
            x = y;
        }
        return x;
    }

    // バッファ一括処理 (in-place可)
    inline void process(const float* src, float* dst, int numSamples) noexcept
    {
        for (int n = 0; n < numSamples; ++n)
            dst[n] = processSample(src[n]);
    }

private:
    struct Coef  { float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f; };
    struct State { float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f; };

    std::array<Coef,  kNumSections> mCoef {};
    std::array<State, kNumSections> mState {};
};
