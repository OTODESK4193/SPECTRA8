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
        float formantShift,
        int currentNumBands,
        const float* g_coeffs,
        const float* k_coeffs,
        const float* a1_coeffs,
        const float* /*a2_coeffs*/,
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

        // 4. フィルタバンクによる変調とボイス加算
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

                    // --- ZDF SVF (LEFT) - 4次直列 (S1 -> S2) オリジナル完全復旧 ---
                    float s1_L_s1 = state.filterS1_S1_L[i][v];
                    float s2_L_s1 = state.filterS1_S2_L[i][v];
                    float v1_L_s1 = a1 * (g * (vL - s2_L_s1) - s1_L_s1);
                    float y_bp_L_s1 = v1_L_s1;
                    float y_lp_L_s1 = g * v1_L_s1 + s2_L_s1;

                    state.filterS1_S1_L[i][v] = 2.0f * y_bp_L_s1 - s1_L_s1;
                    state.filterS1_S2_L[i][v] = 2.0f * y_lp_L_s1 - s2_L_s1;

                    float s1_L_s2 = state.filterS2_S1_L[i][v];
                    float s2_L_s2 = state.filterS2_S2_L[i][v];
                    float v1_L_s2 = a1 * (g * (y_bp_L_s1 - s2_L_s2) - s1_L_s2);
                    float y_bp_L_s2 = v1_L_s2;
                    float y_lp_L_s2 = g * v1_L_s2 + s2_L_s2;

                    state.filterS2_S1_L[i][v] = 2.0f * y_bp_L_s2 - s1_L_s2;
                    state.filterS2_S2_L[i][v] = 2.0f * y_lp_L_s2 - s2_L_s2;

                    // --- ZDF SVF (RIGHT) - 4次直列 (S1 -> S2) オリジナル完全復旧 ---
                    float s1_R_s1 = state.filterS1_S1_R[i][v];
                    float s2_R_s1 = state.filterS1_S2_R[i][v];
                    float v1_R_s1 = a1 * (g * (vR - s2_R_s1) - s1_R_s1);
                    float y_bp_R_s1 = v1_R_s1;
                    float y_lp_R_s1 = g * v1_R_s1 + s2_R_s1;

                    state.filterS1_S1_R[i][v] = 2.0f * y_bp_R_s1 - s1_R_s1;
                    state.filterS1_S2_R[i][v] = 2.0f * y_lp_R_s1 - s2_R_s1;

                    float s1_R_s2 = state.filterS2_S1_R[i][v];
                    float s2_R_s2 = state.filterS2_S2_R[i][v];
                    float v1_R_s2 = a1 * (g * (y_bp_R_s1 - s2_R_s2) - s1_R_s2);
                    float y_bp_R_s2 = v1_R_s2;
                    float y_lp_R_s2 = g * v1_R_s2 + s2_R_s2;

                    state.filterS2_S1_R[i][v] = 2.0f * y_bp_R_s2 - s1_R_s2;
                    state.filterS2_S2_R[i][v] = 2.0f * y_lp_R_s2 - s2_R_s2;

                    // ★鉄壁のフォルマントシフト写像アルゴリズム（半音ベースのメル尺度線形スライド）
                    // shiftがプラス＝高域バンドが低域のエンベロープを読みに行く
                    float srcIdx = static_cast<float>(i) - formantShift;

                    float modEnv = 0.0f;
                    // 配列の境界外にアクセスした場合は、張り付き（クランプ）ではなく、
                    // 「音量を0.0f（無音）に減衰させる」ことで、極端な設定（±24）でも破綻せず、
                    // フォルマントが綺麗に高域・低域へ抜けていく超自然なボコーディングを達成
                    if (srcIdx >= 0.0f && srcIdx < static_cast<float>(activeBands - 1))
                    {
                        int idx0 = static_cast<int>(srcIdx);
                        int idx1 = idx0 + 1;
                        float frac = srcIdx - static_cast<float>(idx0);
                        modEnv = modulatorEnvelopes[idx0] * (1.0f - frac) + modulatorEnvelopes[idx1] * frac;
                    }
                    else if (srcIdx < 0.0f)
                    {
                        if (srcIdx > -1.0f) {
                            modEnv = modulatorEnvelopes[0] * (1.0f + srcIdx); // 緩やかなフェードアウト
                        }
                    }
                    else
                    {
                        float overshoot = srcIdx - static_cast<float>(activeBands - 1);
                        if (overshoot < 1.0f) {
                            modEnv = modulatorEnvelopes[activeBands - 1] * (1.0f - overshoot);
                        }
                    }

                    voiceSumL += y_bp_L_s2 * modEnv;
                    voiceSumR += y_bp_R_s2 * modEnv;
                }

                sumL += voiceSumL * voiceEnvelopes[v];
                sumR += voiceSumR * voiceEnvelopes[v];
            }
        }

        __m256 bitMask = _mm256_cmp_ps(activeVoicesMask, _mm256_setzero_ps(), _CMP_GT_OQ);

        // オシレーター位相の更新
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