#include "OscillatorBank.h"
#include "SimdUtils.h"
#include <cmath>
#include <algorithm>

namespace DSP {

    OscillatorBank::OscillatorBank()
        : mSampleRate(44100.0)
    {
        setup(mSampleRate);
    }

    void OscillatorBank::setup(double sampleRate)
    {
        mSampleRate = sampleRate;
        generateWavetables();
    }

    void OscillatorBank::generateWavetables()
    {
        mWavetableSaw.resize(PolyphonicVoiceSoA::kWaveTableSize);
        mWavetablePulse.resize(PolyphonicVoiceSoA::kWaveTableSize);
        mWavetableTri.resize(PolyphonicVoiceSoA::kWaveTableSize);

        for (int i = 0; i < PolyphonicVoiceSoA::kWaveTableSize; ++i)
        {
            float phase = static_cast<float>(i) / static_cast<float>(PolyphonicVoiceSoA::kWaveTableSize);
            mWavetableSaw[i] = 2.0f * phase - 1.0f;
            mWavetablePulse[i] = (phase < 0.5f) ? 1.0f : -1.0f;

            if (phase < 0.25f)       mWavetableTri[i] = 4.0f * phase;
            else if (phase < 0.75f)  mWavetableTri[i] = 2.0f - 4.0f * phase;
            else                     mWavetableTri[i] = -4.0f + 4.0f * phase;
        }
    }

    void OscillatorBank::processSampleAVX2(PolyphonicVoiceSoA& state,
        __m256 activeVoicesMask,
        __m256 envelopes,
        __m256 noiseMix,
        __m256 noiseBuffer,
        const float* modulatorEnvelopes,
        float /*formantShift*/,
        int currentNumBands,
        const float* g_coeffs,
        const float* k_coeffs,
        const float* a1_coeffs,
        float& outL,
        float& outR)
    {
        const float* tablePtrSaw = mWavetableSaw.data();

        // 1. LEFT オシレーター波形生成
        __m256 phaseL_vec = _mm256_load_ps(state.phaseL);
        __m256i idx0_L_raw = _mm256_cvttps_epi32(phaseL_vec);
        __m256 idx0_L_float = _mm256_cvtepi32_ps(idx0_L_raw);
        __m256 t_L = _mm256_sub_ps(phaseL_vec, idx0_L_float);

        __m256i maskVec = _mm256_set1_epi32(PolyphonicVoiceSoA::kWaveTableMask);
        __m256i idx0_L = _mm256_and_si256(idx0_L_raw, maskVec);
        __m256i idx1_L = _mm256_add_epi32(idx0_L, _mm256_set1_epi32(1));
        idx1_L = _mm256_and_si256(idx1_L, maskVec);

        __m256 y0_L = _mm256_i32gather_ps(tablePtrSaw, idx0_L, 4);
        __m256 y1_L = _mm256_i32gather_ps(tablePtrSaw, idx1_L, 4);

        __m256 tmp_L = _mm256_fnmadd_ps(t_L, y0_L, y0_L);
        __m256 oscL = _mm256_fmadd_ps(t_L, y1_L, tmp_L);

        // 2. RIGHT オシレーター波生成
        __m256 phaseR_vec = _mm256_load_ps(state.phaseR);
        __m256i idx0_R_raw = _mm256_cvttps_epi32(phaseR_vec);
        __m256 idx0_R_float = _mm256_cvtepi32_ps(idx0_R_raw);
        __m256 t_R = _mm256_sub_ps(phaseR_vec, idx0_R_float);

        __m256i idx0_R = _mm256_and_si256(idx0_R_raw, maskVec);
        __m256i idx1_R = _mm256_add_epi32(idx0_R, _mm256_set1_epi32(1));
        idx1_R = _mm256_and_si256(idx1_R, maskVec);

        __m256 y0_R = _mm256_i32gather_ps(tablePtrSaw, idx0_R, 4);
        __m256 y1_R = _mm256_i32gather_ps(tablePtrSaw, idx1_R, 4);

        __m256 tmp_R = _mm256_fnmadd_ps(t_R, y0_R, y0_R);
        __m256 oscR = _mm256_fmadd_ps(t_R, y1_R, tmp_R);

        // 3. 有声音(オシレーター)と無声音(ノイズ)のブレンド
        __m256 oneMinusNoiseMix = _mm256_sub_ps(_mm256_set1_ps(1.0f), noiseMix);
        __m256 excitationL = _mm256_fmadd_ps(oneMinusNoiseMix, oscL, _mm256_mul_ps(noiseMix, noiseBuffer));
        __m256 excitationR = _mm256_fmadd_ps(oneMinusNoiseMix, oscR, _mm256_mul_ps(noiseMix, noiseBuffer));

        // 4. ZDF SVF フィルタバンクによる変調とボイス加算
        alignas(32) float actMask[8];
        alignas(32) float excL[8];
        alignas(32) float excR[8];
        alignas(32) float voiceEnvelopes[8];

        _mm256_store_ps(actMask, activeVoicesMask);
        _mm256_store_ps(excL, excitationL);
        _mm256_store_ps(excR, excitationR);
        _mm256_store_ps(voiceEnvelopes, envelopes);

        float sumL = 0.0f;
        float sumR = 0.0f;

        int activeBands = std::clamp(currentNumBands, 8, static_cast<int>(PolyphonicVoiceSoA::kNumBands));

        for (int v = 0; v < 8; ++v)
        {
            if (actMask[v] > 0.0f)
            {
                float vL = excL[v];
                float vR = excR[v];

                float voiceSumL = 0.0f;
                float voiceSumR = 0.0f;

                for (int i = 0; i < activeBands; ++i)
                {
                    float g = g_coeffs[i];
                    float k = k_coeffs[i];
                    float a1 = a1_coeffs[i];

                    // --- ZDF SVF (LEFT) 正確な Pirkle / Zavalishin 積分モデル ---
                    // セクション 1
                    float s1_L_s1 = state.filterS1_S1_L[i][v];
                    float s2_L_s1 = state.filterS1_S2_L[i][v];

                    float hp_L_s1 = a1 * (vL - k * s1_L_s1 - s2_L_s1);
                    float v1_L_s1 = g * hp_L_s1 + s1_L_s1;
                    float v2_L_s1 = g * v1_L_s1 + s2_L_s1;

                    state.filterS1_S1_L[i][v] = 2.0f * v1_L_s1 - s1_L_s1;
                    state.filterS1_S2_L[i][v] = 2.0f * v2_L_s1 - s2_L_s1;

                    // セクション 2
                    float s1_L_s2 = state.filterS2_S1_L[i][v];
                    float s2_L_s2 = state.filterS2_S2_L[i][v];

                    float hp_L_s2 = a1 * (v1_L_s1 - k * s1_L_s2 - s2_L_s2);
                    float v1_L_s2 = g * hp_L_s2 + s1_L_s2;
                    float v2_L_s2 = g * v1_L_s2 + s2_L_s2;

                    state.filterS2_S1_L[i][v] = 2.0f * v1_L_s2 - s1_L_s2;
                    state.filterS2_S2_L[i][v] = 2.0f * v2_L_s2 - s2_L_s2;


                    // --- ZDF SVF (RIGHT) 正確な Pirkle / Zavalishin 積分モデル ---
                    // セクション 1
                    float s1_R_s1 = state.filterS1_S1_R[i][v];
                    float s2_R_s1 = state.filterS1_S2_R[i][v];

                    float hp_R_s1 = a1 * (vR - k * s1_R_s1 - s2_R_s1);
                    float v1_R_s1 = g * hp_R_s1 + s1_R_s1;
                    float v2_R_s1 = g * v1_R_s1 + s2_R_s1;

                    state.filterS1_S1_R[i][v] = 2.0f * v1_R_s1 - s1_R_s1;
                    state.filterS1_S2_R[i][v] = 2.0f * v2_R_s1 - s2_R_s1;

                    // セクション 2
                    float s1_R_s2 = state.filterS2_S1_R[i][v];
                    float s2_R_s2 = state.filterS2_S2_R[i][v];

                    float hp_R_s2 = a1 * (v1_R_s1 - k * s1_R_s2 - s2_R_s2);
                    float v1_R_s2 = g * hp_R_s2 + s1_R_s2;
                    float v2_R_s2 = g * v1_R_s2 + s2_R_s2;

                    state.filterS2_S1_R[i][v] = 2.0f * v1_R_s2 - s1_R_s2;
                    state.filterS2_S2_R[i][v] = 2.0f * v2_R_s2 - s2_R_s2;

                    float modEnv = modulatorEnvelopes[i];

                    // 帯域減衰を補正し、音の明瞭度を最大化
                    voiceSumL += v1_L_s2 * modEnv * 4.0f;
                    voiceSumR += v1_R_s2 * modEnv * 4.0f;
                }

                sumL += voiceSumL * voiceEnvelopes[v];
                sumR += voiceSumR * voiceEnvelopes[v];
            }
        }

        __m256 bitMask = _mm256_cmp_ps(activeVoicesMask, _mm256_setzero_ps(), _CMP_GT_OQ);

        // 位相更新
        __m256 phaseIncrL_vec = _mm256_load_ps(state.phaseIncrL);
        __m256 phaseIncrR_vec = _mm256_load_ps(state.phaseIncrR);
        phaseL_vec = _mm256_add_ps(phaseL_vec, phaseIncrL_vec);
        phaseR_vec = _mm256_add_ps(phaseR_vec, phaseIncrR_vec);

        phaseL_vec = _mm256_and_ps(phaseL_vec, bitMask);
        phaseR_vec = _mm256_and_ps(phaseR_vec, bitMask);

        __m256 sizeVec = _mm256_set1_ps(static_cast<float>(PolyphonicVoiceSoA::kWaveTableSize));
        __m256 cmpL = _mm256_cmp_ps(phaseL_vec, sizeVec, _CMP_GE_OQ);
        __m256 subL = _mm256_and_ps(cmpL, sizeVec);
        phaseL_vec = _mm256_sub_ps(phaseL_vec, subL);

        __m256 cmpR = _mm256_cmp_ps(phaseR_vec, sizeVec, _CMP_GE_OQ);
        __m256 subR = _mm256_and_ps(cmpR, sizeVec);
        phaseR_vec = _mm256_sub_ps(phaseR_vec, subR);

        _mm256_store_ps(state.phaseL, phaseL_vec);
        _mm256_store_ps(state.phaseR, phaseR_vec);

        outL = sumL;
        outR = sumR;
    }

} // namespace DSP