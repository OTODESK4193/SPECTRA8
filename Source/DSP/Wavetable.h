// ==========================================
// File: Wavetable.h
// モーフィング・ウェーブテーブル（開発計画書 第2版 ③-④準拠）
//
//  WT POS 0.0→1.0 で Sine → Triangle → Square → Sawtooth → FM金属波形
//  へ滑らかにモーフィング。
//  各フレームは 2048 サンプル・10段のミップレベル（FFT帯域制限）を持ち、
//  高音域でのエイリアシングを防止する。
//  テーブル生成はコンストラクタ（非リアルタイム）でのみ行う。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>

class MorphWavetable
{
public:
    static constexpr int kTableSize = 2048;
    static constexpr int kNumFrames = 5;   // Sine, Tri, Square, Saw, FM
    static constexpr int kNumMips = 10;    // level k は 1024>>k 倍音まで

    MorphWavetable()
    {
        juce::dsp::FFT fft(11); // 2048

        std::array<float, kTableSize> raw {};

        for (int frame = 0; frame < kNumFrames; ++frame)
        {
            // --- 理想波形（フルレゾリューション）を生成 ---
            for (int n = 0; n < kTableSize; ++n)
            {
                const float ph = (float)n / (float)kTableSize;            // 0..1
                const float th = ph * juce::MathConstants<float>::twoPi;
                switch (frame)
                {
                case 0: raw[(size_t)n] = std::sin(th); break;                              // Sine
                case 1: raw[(size_t)n] = 1.0f - 4.0f * std::abs(ph - 0.5f); break;         // Triangle
                case 2: raw[(size_t)n] = (ph < 0.5f) ? 1.0f : -1.0f; break;                // Square
                case 3: raw[(size_t)n] = 2.0f * ph - 1.0f; break;                          // Saw
                case 4: raw[(size_t)n] = std::sin(th + 2.2f * std::sin(2.0f * th)          // FM金属波形
                                                     + 0.6f * std::sin(5.0f * th)); break;
                default: break;
                }
            }

            // --- FFT → ミップごとに帯域制限 → IFFT ---
            std::vector<float> freq((size_t)kTableSize * 2, 0.0f);
            std::copy(raw.begin(), raw.end(), freq.begin());
            fft.performRealOnlyForwardTransform(freq.data());

            for (int mip = 0; mip < kNumMips; ++mip)
            {
                const int maxHarm = juce::jmax(1, 1024 >> mip);
                std::vector<float> spec = freq; // interleaved (re,im)

                for (int bin = 0; bin < kTableSize / 2; ++bin)
                {
                    if (bin > maxHarm)
                    {
                        spec[(size_t)bin * 2] = 0.0f;
                        spec[(size_t)bin * 2 + 1] = 0.0f;
                    }
                }
                // DC除去
                spec[0] = 0.0f;
                spec[1] = 0.0f;

                fft.performRealOnlyInverseTransform(spec.data());

                // 正規化（最大振幅を1.0に）
                float peak = 1e-9f;
                for (int n = 0; n < kTableSize; ++n)
                    peak = juce::jmax(peak, std::abs(spec[(size_t)n]));
                const float norm = 1.0f / peak;

                auto& dst = tables[(size_t)frame][(size_t)mip];
                for (int n = 0; n < kTableSize; ++n)
                    dst[(size_t)n] = spec[(size_t)n] * norm;
                dst[(size_t)kTableSize] = dst[0]; // ラップ用ガードサンプル
            }
        }
    }

    // phase: 0..1, morphPos: 0..1, phaseIncPerSample: f0/sampleRate
    float sample(float phase, float morphPos, float phaseIncPerSample) const noexcept
    {
        // ミップ選択: 再生周波数で許容される最大倍音数から決定
        const float maxHarmF = 0.5f / juce::jmax(1.0e-6f, phaseIncPerSample); // sr/(2*f0)
        int mip = 0;
        while (mip < kNumMips - 1 && (float)(1024 >> mip) > maxHarmF)
            ++mip;

        // フレームのモーフィング位置
        const float fpos = juce::jlimit(0.0f, 1.0f, morphPos) * (float)(kNumFrames - 1);
        const int f0i = juce::jlimit(0, kNumFrames - 1, (int)fpos);
        const int f1i = juce::jmin(kNumFrames - 1, f0i + 1);
        const float ffrac = fpos - (float)f0i;

        const float idx = phase * (float)kTableSize;
        const int i0 = juce::jlimit(0, kTableSize - 1, (int)idx);
        const float frac = idx - (float)i0;

        const auto& tA = tables[(size_t)f0i][(size_t)mip];
        const auto& tB = tables[(size_t)f1i][(size_t)mip];

        const float a = tA[(size_t)i0] + frac * (tA[(size_t)i0 + 1] - tA[(size_t)i0]);
        const float b = tB[(size_t)i0] + frac * (tB[(size_t)i0 + 1] - tB[(size_t)i0]);
        return a + ffrac * (b - a);
    }

private:
    // [frame][mip][sample] (+1 ガードサンプル)
    std::array<std::array<std::array<float, kTableSize + 1>, kNumMips>, kNumFrames> tables {};
};
