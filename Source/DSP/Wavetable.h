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
#include <vector>
#include <memory>
#include <atomic>
#include <cmath>

class MorphWavetable
{
public:
    static constexpr int kTableSize = 2048;
    static constexpr int kNumFrames = 5;   // Sine, Tri, Square, Saw, FM
    static constexpr int kNumMips = 10;    // level k は 1024>>k 倍音まで
    static constexpr int kMaxCustomFrames = 64;   // カスタムWT最大フレーム数 (Serum互換 2048smp/frame)

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

    // ==========================================================
    // カスタムWavetable (ユーザーwavロード)
    //  - loadCustomFromBuffer はメッセージスレッド専用 (FFT/確保あり)
    //  - 音声スレッドは mCustom の atomicポインタを読むだけ (ロックフリー)
    //  - 旧テーブルは mAllSets に保持し続け解放しない (残参照対策。ロードは希少)
    // ==========================================================
    struct CustomSet
    {
        int numFrames = 0;
        std::vector<float> data;   // [frame][mip][kTableSize+1]
        const float* table(int frame, int mip) const noexcept
        {
            return &data[(size_t)((frame * kNumMips + mip) * (kTableSize + 1))];
        }
    };

    // mono/numSamples を 2048サンプル/フレームで分割してカスタムテーブル化。
    // 2048未満は1周期波形とみなし2048へ線形リサンプル。成功で true。
    bool loadCustomFromBuffer(const float* mono, int numSamples)
    {
        if (mono == nullptr || numSamples < 16)
            return false;

        const int frames = juce::jlimit(1, kMaxCustomFrames, numSamples / kTableSize);
        auto set = std::make_unique<CustomSet>();
        set->numFrames = frames;
        set->data.assign((size_t)(frames * kNumMips * (kTableSize + 1)), 0.0f);

        juce::dsp::FFT fft(11); // 2048
        std::array<float, kTableSize> raw {};

        for (int fi = 0; fi < frames; ++fi)
        {
            if (numSamples >= kTableSize)
            {
                std::copy(mono + fi * kTableSize, mono + (fi + 1) * kTableSize, raw.begin());
            }
            else
            {
                // 1周期波形を2048へ線形リサンプル
                for (int n = 0; n < kTableSize; ++n)
                {
                    const double pos = (double)n * (numSamples - 1) / (double)(kTableSize - 1);
                    const int i0 = (int)pos;
                    const double fr = pos - i0;
                    raw[(size_t)n] = (float)((double)mono[i0] * (1.0 - fr)
                                           + (double)mono[juce::jmin(numSamples - 1, i0 + 1)] * fr);
                }
            }
            buildMips(raw, fft, [&set, fi](int mip) {
                return &set->data[(size_t)((fi * kNumMips + mip) * (kTableSize + 1))]; });
        }

        const CustomSet* ptr = set.get();
        mAllSets.push_back(std::move(set));
        mCustom.store(ptr, std::memory_order_release);
        return true;
    }

    void clearCustom() noexcept { mCustom.store(nullptr, std::memory_order_release); }
    bool hasCustom() const noexcept { return mCustom.load(std::memory_order_relaxed) != nullptr; }

    // GUI波形表示用: morphPos位置の波形を n 点へ縮小して書き出す (メッセージスレッド用)
    void getDisplayWave(float morphPos, float* out, int n) const
    {
        for (int i = 0; i < n; ++i)
            out[i] = sample((float)i / (float)n, morphPos, 1.0f / 512.0f);
    }

    // phase: 0..1, morphPos: 0..1, phaseIncPerSample: f0/sampleRate
    float sample(float phase, float morphPos, float phaseIncPerSample) const noexcept
    {
        // ミップ選択: 再生周波数で許容される最大倍音数から決定
        const float maxHarmF = 0.5f / juce::jmax(1.0e-6f, phaseIncPerSample); // sr/(2*f0)
        int mip = 0;
        while (mip < kNumMips - 1 && (float)(1024 >> mip) > maxHarmF)
            ++mip;

        const float idx = phase * (float)kTableSize;
        const int i0 = juce::jlimit(0, kTableSize - 1, (int)idx);
        const float frac = idx - (float)i0;

        // カスタムテーブルが有効ならそちらを使用 (ロックフリー)
        if (const CustomSet* cs = mCustom.load(std::memory_order_relaxed))
        {
            const float fpos = juce::jlimit(0.0f, 1.0f, morphPos) * (float)(cs->numFrames - 1);
            const int f0i = juce::jlimit(0, cs->numFrames - 1, (int)fpos);
            const int f1i = juce::jmin(cs->numFrames - 1, f0i + 1);
            const float ffrac = fpos - (float)f0i;

            const float* tA = cs->table(f0i, mip);
            const float* tB = cs->table(f1i, mip);
            const float a = tA[i0] + frac * (tA[i0 + 1] - tA[i0]);
            const float b = tB[i0] + frac * (tB[i0 + 1] - tB[i0]);
            return a + ffrac * (b - a);
        }

        // 内蔵テーブル (Sine→Tri→Square→Saw→FM)
        const float fpos = juce::jlimit(0.0f, 1.0f, morphPos) * (float)(kNumFrames - 1);
        const int f0i = juce::jlimit(0, kNumFrames - 1, (int)fpos);
        const int f1i = juce::jmin(kNumFrames - 1, f0i + 1);
        const float ffrac = fpos - (float)f0i;

        const auto& tA = tables[(size_t)f0i][(size_t)mip];
        const auto& tB = tables[(size_t)f1i][(size_t)mip];

        const float a = tA[(size_t)i0] + frac * (tA[(size_t)i0 + 1] - tA[(size_t)i0]);
        const float b = tB[(size_t)i0] + frac * (tB[(size_t)i0 + 1] - tB[(size_t)i0]);
        return a + ffrac * (b - a);
    }

private:
    // 1フレームの生波形から全ミップ (FFT帯域制限+正規化+ガードサンプル) を構築。
    // dstForMip(mip) は書き込み先 (kTableSize+1 要素) を返すコールバック。
    template <typename DstFn>
    static void buildMips(const std::array<float, kTableSize>& raw, juce::dsp::FFT& fft, DstFn dstForMip)
    {
        std::vector<float> freq((size_t)kTableSize * 2, 0.0f);
        std::copy(raw.begin(), raw.end(), freq.begin());
        fft.performRealOnlyForwardTransform(freq.data());

        for (int mip = 0; mip < kNumMips; ++mip)
        {
            const int maxHarm = juce::jmax(1, 1024 >> mip);
            std::vector<float> spec = freq;

            for (int bin = 0; bin < kTableSize / 2; ++bin)
            {
                if (bin > maxHarm)
                {
                    spec[(size_t)bin * 2] = 0.0f;
                    spec[(size_t)bin * 2 + 1] = 0.0f;
                }
            }
            spec[0] = 0.0f;   // DC除去
            spec[1] = 0.0f;

            fft.performRealOnlyInverseTransform(spec.data());

            float peak = 1e-9f;
            for (int n = 0; n < kTableSize; ++n)
                peak = juce::jmax(peak, std::abs(spec[(size_t)n]));
            const float norm = 1.0f / peak;

            float* dst = dstForMip(mip);
            for (int n = 0; n < kTableSize; ++n)
                dst[n] = spec[(size_t)n] * norm;
            dst[kTableSize] = dst[0]; // ラップ用ガードサンプル
        }
    }

    // [frame][mip][sample] (+1 ガードサンプル)
    std::array<std::array<std::array<float, kTableSize + 1>, kNumMips>, kNumFrames> tables {};

    // カスタムテーブル (atomic差し替え。旧テーブルは保持し続ける)
    std::atomic<const CustomSet*> mCustom { nullptr };
    std::vector<std::unique_ptr<CustomSet>> mAllSets;
};
