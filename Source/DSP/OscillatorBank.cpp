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
        mCustomWavetable.resize(PolyphonicVoiceSoA::kWaveTableSize);

        for (int i = 0; i < PolyphonicVoiceSoA::kWaveTableSize; ++i)
        {
            float phase = static_cast<float>(i) / static_cast<float>(PolyphonicVoiceSoA::kWaveTableSize);
            mWavetableSaw[i] = 2.0f * phase - 1.0f;
            mWavetablePulse[i] = (phase < 0.5f) ? 1.0f : -1.0f;

            // 3. 三角波
            if (phase < 0.25f)
            {
                mWavetableTri[i] = 4.0f * phase;
            }
            else if (phase < 0.75f)
            {
                mWavetableTri[i] = 2.0f - 4.0f * phase;
            }
            else
            {
                mWavetableTri[i] = -4.0f + 4.0f * phase;
            }

            // 初期カスタムWavetableとして三角波をコピーしておく (将来の拡張用)
            mCustomWavetable[i] = mWavetableTri[i];
        }
    }

    void OscillatorBank::processSampleAVX2(PolyphonicVoiceSoA& state,
        __m256 activeVoicesMask,
        __m256 envelopes,
        __m256 noiseMix,
        __m256 noiseBuffer,
        const float* modulatorEnvelopes,
        float formantShift,
        float formantStretch,
        int vocoderMode,
        int currentNumBands,
        int waveform,
        float pulseWidth,
        float wavetablePosition,
        const float* g_coeffs,
        const float* k_coeffs,
        const float* a1_coeffs,
        const float* a2_coeffs,
        float& outL,
        float& outR)
    {
        __m256 oscL = _mm256_setzero_ps();
        __m256 oscR = _mm256_setzero_ps();

        __m256 phaseL_vec = _mm256_load_ps(state.phaseL);
        __m256 phaseR_vec = _mm256_load_ps(state.phaseR);

        if (waveform == 0) // Sawtooth
        {
            const float* tablePtr = mWavetableSaw.data();

            // LEFT
            __m256i idx0_L_raw = _mm256_cvttps_epi32(phaseL_vec);
            __m256 idx0_L_float = _mm256_cvtepi32_ps(idx0_L_raw);
            __m256 t_L = _mm256_sub_ps(phaseL_vec, idx0_L_float);
            __m256i maskVec = _mm256_set1_epi32(PolyphonicVoiceSoA::kWaveTableMask);
            __m256i idx0_L = _mm256_and_si256(idx0_L_raw, maskVec);
            __m256i idx1_L = _mm256_add_epi32(idx0_L, _mm256_set1_epi32(1));
            idx1_L = _mm256_and_si256(idx1_L, maskVec);
            __m256 y0_L = _mm256_i32gather_ps(tablePtr, idx0_L, 4);
            __m256 y1_L = _mm256_i32gather_ps(tablePtr, idx1_L, 4);
            __m256 tmp_L = _mm256_fnmadd_ps(t_L, y0_L, y0_L);
            oscL = _mm256_fmadd_ps(t_L, y1_L, tmp_L);

            // RIGHT
            __m256i idx0_R_raw = _mm256_cvttps_epi32(phaseR_vec);
            __m256 idx0_R_float = _mm256_cvtepi32_ps(idx0_R_raw);
            __m256 t_R = _mm256_sub_ps(phaseR_vec, idx0_R_float);
            __m256i idx0_R = _mm256_and_si256(idx0_R_raw, maskVec);
            __m256i idx1_R = _mm256_add_epi32(idx0_R, _mm256_set1_epi32(1));
            idx1_R = _mm256_and_si256(idx1_R, maskVec);
            __m256 y0_R = _mm256_i32gather_ps(tablePtr, idx0_R, 4);
            __m256 y1_R = _mm256_i32gather_ps(tablePtr, idx1_R, 4);
            __m256 tmp_R = _mm256_fnmadd_ps(t_R, y0_R, y0_R);
            oscR = _mm256_fmadd_ps(t_R, y1_R, tmp_R);
        }
        else if (waveform == 1) // Pulse
        {
            __m256 thresh = _mm256_set1_ps(pulseWidth * static_cast<float>(PolyphonicVoiceSoA::kWaveTableSize));
            
            // LEFT
            __m256 cmpL = _mm256_cmp_ps(phaseL_vec, thresh, _CMP_LT_OQ);
            oscL = _mm256_blendv_ps(_mm256_set1_ps(-1.0f), _mm256_set1_ps(1.0f), cmpL);

            // RIGHT
            __m256 cmpR = _mm256_cmp_ps(phaseR_vec, thresh, _CMP_LT_OQ);
            oscR = _mm256_blendv_ps(_mm256_set1_ps(-1.0f), _mm256_set1_ps(1.0f), cmpR);
        }
        else // Wavetable (waveform == 2)
        {
            const float* tableTri = mWavetableTri.data();
            const float* tableCustom = mCustomWavetable.data();

            __m256 posVec = _mm256_set1_ps(wavetablePosition);
            __m256 oneMinusPos = _mm256_sub_ps(_mm256_set1_ps(1.0f), posVec);

            __m256i maskVec = _mm256_set1_epi32(PolyphonicVoiceSoA::kWaveTableMask);

            // --- LEFT ---
            __m256i idx0_L_raw = _mm256_cvttps_epi32(phaseL_vec);
            __m256 idx0_L_float = _mm256_cvtepi32_ps(idx0_L_raw);
            __m256 t_L = _mm256_sub_ps(phaseL_vec, idx0_L_float);
            __m256i idx0_L = _mm256_and_si256(idx0_L_raw, maskVec);
            __m256i idx1_L = _mm256_add_epi32(idx0_L, _mm256_set1_epi32(1));
            idx1_L = _mm256_and_si256(idx1_L, maskVec);

            __m256 y0_L_tri = _mm256_i32gather_ps(tableTri, idx0_L, 4);
            __m256 y1_L_tri = _mm256_i32gather_ps(tableTri, idx1_L, 4);
            __m256 tmp_L_tri = _mm256_fnmadd_ps(t_L, y0_L_tri, y0_L_tri);
            __m256 oscL_tri = _mm256_fmadd_ps(t_L, y1_L_tri, tmp_L_tri);

            __m256 y0_L_cust = _mm256_i32gather_ps(tableCustom, idx0_L, 4);
            __m256 y1_L_cust = _mm256_i32gather_ps(tableCustom, idx1_L, 4);
            __m256 tmp_L_cust = _mm256_fnmadd_ps(t_L, y0_L_cust, y0_L_cust);
            __m256 oscL_cust = _mm256_fmadd_ps(t_L, y1_L_cust, tmp_L_cust);

            oscL = _mm256_fmadd_ps(oneMinusPos, oscL_tri, _mm256_mul_ps(posVec, oscL_cust));

            // --- RIGHT ---
            __m256i idx0_R_raw = _mm256_cvttps_epi32(phaseR_vec);
            __m256 idx0_R_float = _mm256_cvtepi32_ps(idx0_R_raw);
            __m256 t_R = _mm256_sub_ps(phaseR_vec, idx0_R_float);
            __m256i idx0_R = _mm256_and_si256(idx0_R_raw, maskVec);
            __m256i idx1_R = _mm256_add_epi32(idx0_R, _mm256_set1_epi32(1));
            idx1_R = _mm256_and_si256(idx1_R, maskVec);

            __m256 y0_R_tri = _mm256_i32gather_ps(tableTri, idx0_R, 4);
            __m256 y1_R_tri = _mm256_i32gather_ps(tableTri, idx1_R, 4);
            __m256 tmp_R_tri = _mm256_fnmadd_ps(t_R, y0_R_tri, y0_R_tri);
            __m256 oscR_tri = _mm256_fmadd_ps(t_R, y1_R_tri, tmp_R_tri);

            __m256 y0_R_cust = _mm256_i32gather_ps(tableCustom, idx0_R, 4);
            __m256 y1_R_cust = _mm256_i32gather_ps(tableCustom, idx1_R, 4);
            __m256 tmp_R_cust = _mm256_fnmadd_ps(t_R, y0_R_cust, y0_R_cust);
            __m256 oscR_cust = _mm256_fmadd_ps(t_R, y1_R_cust, tmp_R_cust);

            oscR = _mm256_fmadd_ps(oneMinusPos, oscR_tri, _mm256_mul_ps(posVec, oscR_cust));
        }

        // 3. 有声音(オシレーター)と無声音(ノイズ)のブレンド
        __m256 oneMinusNoiseMix = _mm256_sub_ps(_mm256_set1_ps(1.0f), noiseMix);
        __m256 excitationL = _mm256_fmadd_ps(oneMinusNoiseMix, oscL, _mm256_mul_ps(noiseMix, noiseBuffer));
        __m256 excitationR = _mm256_fmadd_ps(oneMinusNoiseMix, oscR, _mm256_mul_ps(noiseMix, noiseBuffer));

        float sumL = 0.0f;
        float sumR = 0.0f;

        if (vocoderMode == 0) // Filterbank Mode (ZDF SVF 48 Bands)
        {
            // 4. ZDF SVF フィルタバンクによる変調とボイス加算
            alignas(32) float actMask[8];
            alignas(32) float excL[8];
            alignas(32) float excR[8];
            alignas(32) float voiceEnvelopes[8];

            _mm256_store_ps(actMask, activeVoicesMask);
            _mm256_store_ps(excL, excitationL);
            _mm256_store_ps(excR, excitationR);
            _mm256_store_ps(voiceEnvelopes, envelopes);

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
                        float a1 = a1_coeffs[i];

                        // --- ZDF SVF (LEFT) - 4次直列 (S1 -> S2) ---
                        float s1_L_s1 = state.filterS1_S1_L[i][v];
                        float s2_L_s1 = state.filterS1_S2_L[i][v];
                        float v1_L_s1 = a1 * (s1_L_s1 + g * (vL - s2_L_s1));
                        float y_bp_L_s1 = v1_L_s1;
                        float y_lp_L_s1 = s2_L_s1 + g * v1_L_s1;

                        state.filterS1_S1_L[i][v] = 2.0f * y_bp_L_s1 - s1_L_s1;
                        state.filterS1_S2_L[i][v] = 2.0f * y_lp_L_s1 - s2_L_s1;

                        float s1_L_s2 = state.filterS2_S1_L[i][v];
                        float s2_L_s2 = state.filterS2_S2_L[i][v];
                        float v1_L_s2 = a1 * (s1_L_s2 + g * (y_bp_L_s1 - s2_L_s2));
                        float y_bp_L_s2 = v1_L_s2;
                        float y_lp_L_s2 = s2_L_s2 + g * v1_L_s2;

                        state.filterS2_S1_L[i][v] = 2.0f * y_bp_L_s2 - s1_L_s2;
                        state.filterS2_S2_L[i][v] = 2.0f * y_lp_L_s2 - s2_L_s2;


                        // --- ZDF SVF (RIGHT) - 4次直列 (S1 -> S2) ---
                        float s1_R_s1 = state.filterS1_S1_R[i][v];
                        float s2_R_s1 = state.filterS1_S2_R[i][v];
                        float v1_R_s1 = a1 * (s1_R_s1 + g * (vR - s2_R_s1));
                        float y_bp_R_s1 = v1_R_s1;
                        float y_lp_R_s1 = s2_R_s1 + g * v1_R_s1;

                        state.filterS1_S1_R[i][v] = 2.0f * y_bp_R_s1 - s1_R_s1;
                        state.filterS1_S2_R[i][v] = 2.0f * y_lp_R_s1 - s2_R_s1;

                        float s1_R_s2 = state.filterS2_S1_R[i][v];
                        float s2_R_s2 = state.filterS2_S2_R[i][v];
                        float v1_R_s2 = a1 * (s1_R_s2 + g * (y_bp_R_s1 - s2_R_s2));
                        float y_bp_R_s2 = v1_R_s2;
                        float y_lp_R_s2 = s2_R_s2 + g * v1_R_s2;

                        state.filterS2_S1_R[i][v] = 2.0f * y_bp_R_s2 - s1_R_s2;
                        state.filterS2_S2_R[i][v] = 2.0f * y_lp_R_s2 - s2_R_s2;


                        // フォルマントシフト＆ストレッチ写像
                        float centerBand = static_cast<float>(activeBands - 1) * 0.5f;
                        float srcIdx = centerBand + (static_cast<float>(i) - centerBand) / formantStretch - formantShift;
                        
                        float modEnv = 0.0f;
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
                                modEnv = modulatorEnvelopes[0] * (1.0f + srcIdx);
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
        }
        else // LPCMode (16th order voice-parallel LPC synthesis)
        {
            __m256 yL = excitationL;
            __m256 yR = excitationR;

            // 16次のLPC合成フィルタをAVX2で一括実行
            for (int i = 0; i < 16; ++i)
            {
                __m256 coeff = _mm256_load_ps(&state.lpcCoeffs[i][0]);
                __m256 histL = _mm256_load_ps(&state.lpcHistoryL[i][0]);
                __m256 histR = _mm256_load_ps(&state.lpcHistoryR[i][0]);
                
                yL = _mm256_fnmadd_ps(coeff, histL, yL);
                yR = _mm256_fnmadd_ps(coeff, histR, yR);
            }

            // 履歴バッファをシフト
            for (int i = 15; i > 0; --i)
            {
                _mm256_store_ps(&state.lpcHistoryL[i][0], _mm256_load_ps(&state.lpcHistoryL[i - 1][0]));
                _mm256_store_ps(&state.lpcHistoryR[i][0], _mm256_load_ps(&state.lpcHistoryR[i - 1][0]));
            }
            _mm256_store_ps(&state.lpcHistoryL[0][0], yL);
            _mm256_store_ps(&state.lpcHistoryR[0][0], yR);

            // 音量エンベロープを乗算
            __m256 outVoiceL = _mm256_mul_ps(yL, envelopes);
            __m256 outVoiceR = _mm256_mul_ps(yR, envelopes);

            // アクティブボイスにマスク
            outVoiceL = _mm256_and_ps(outVoiceL, activeVoicesMask);
            outVoiceR = _mm256_and_ps(outVoiceR, activeVoicesMask);

            // 全ボイスの和を計算
            sumL = SimdUtils::horizontalSum(outVoiceL);
            sumR = SimdUtils::horizontalSum(outVoiceR);
        }

        __m256 bitMask = _mm256_cmp_ps(activeVoicesMask, _mm256_setzero_ps(), _CMP_GT_OQ);

        // 7. オシレーター位相の更新
        __m256 phaseIncrL_vec = _mm256_load_ps(state.phaseIncrL);
        __m256 phaseIncrR_vec = _mm256_load_ps(state.phaseIncrR);
        phaseL_vec = _mm256_add_ps(phaseL_vec, phaseIncrL_vec);
        phaseR_vec = _mm256_add_ps(phaseR_vec, phaseIncrR_vec);

        phaseL_vec = _mm256_and_ps(phaseL_vec, bitMask);
        phaseR_vec = _mm256_and_ps(phaseR_vec, bitMask);

        // ブランクレス位相ラッピング
        __m256 sizeVec = _mm256_set1_ps(static_cast<float>(PolyphonicVoiceSoA::kWaveTableSize));
        __m256 cmpL = _mm256_cmp_ps(phaseL_vec, sizeVec, _CMP_GE_OQ);
        __m256 subL = _mm256_and_ps(cmpL, sizeVec);
        phaseL_vec = _mm256_sub_ps(phaseL_vec, subL);

        __m256 cmpR = _mm256_cmp_ps(phaseR_vec, sizeVec, _CMP_GE_OQ);
        __m256 subR = _mm256_and_ps(cmpR, sizeVec);
        phaseR_vec = _mm256_sub_ps(phaseR_vec, subR);

        _mm256_store_ps(state.phaseL, phaseL_vec);
        _mm256_store_ps(state.phaseR, phaseR_vec);

        // WET音量を通常範囲にするため適切なスケーリングを掛ける (ゲイン減衰 * 0.012f)
        outL = sumL * 0.012f;
        outR = sumR * 0.012f;
    }

} // namespace DSP