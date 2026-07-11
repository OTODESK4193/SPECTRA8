#pragma once

#include <vector>
#include <cstdint>
#include <immintrin.h>
#include "VoiceState.h"
#include "MidiQueue.h"

namespace DSP {

    class OscillatorBank {
    public:
        OscillatorBank();
        ~OscillatorBank() = default;

        // 初期化とWavetableの生成
        void setup(double sampleRate);

        // 8ボイス並列のオシレーターおよびLPC合成フィルターを一括処理する (AVX2 SIMD)
        // ★数理モデル刷新に伴い、引数を13個に完全統一（a2_coeffsを削除）
        void processSampleAVX2(PolyphonicVoiceSoA& state,
            __m256 activeVoicesMask,
            __m256 envelopes,
            __m256 noiseMix,
            __m256 noiseBuffer,
            const float* modulatorEnvelopes,
            float formantShift,
            int currentNumBands,
            const float* g_coeffs,
            const float* k_coeffs,
            const float* a1_coeffs,
            float& outL,
            float& outR);

    private:
        void generateWavetables();

        double mSampleRate;

        // アライメントされたウェーブテーブルバッファ (L1キャッシュ常駐用、サイズ2048)
        alignas(32) std::vector<float> mWavetableSaw;
        alignas(32) std::vector<float> mWavetablePulse;
        alignas(32) std::vector<float> mWavetableTri;
    };

} // namespace DSP