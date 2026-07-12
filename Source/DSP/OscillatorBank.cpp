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

namespace {
    inline __m256 polyBlepCorrectionAVX2(__m256 phase, __m256 dt)
    {
        __m256 zero = _mm256_setzero_ps();
        __m256 one = _mm256_set1_ps(1.0f);

        // condition 1: phase < dt
        __m256 cond1 = _mm256_cmp_ps(phase, dt, _CMP_LT_OQ);
        __m256 t1 = _mm256_div_ps(phase, dt);
        __m256 val1 = _mm256_sub_ps(_mm256_sub_ps(_mm256_add_ps(t1, t1), _mm256_mul_ps(t1, t1)), one);

        // condition 2: phase > 1.0 - dt
        __m256 oneMinusDt = _mm256_sub_ps(one, dt);
        __m256 cond2 = _mm256_cmp_ps(phase, oneMinusDt, _CMP_GT_OQ);
        __m256 t2 = _mm256_div_ps(_mm256_sub_ps(phase, one), dt);
        __m256 val2 = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(t2, t2), _mm256_add_ps(t2, t2)), one);

        // Merge using blendv
        __m256 result = _mm256_blendv_ps(zero, val1, cond1);
        result = _mm256_blendv_ps(result, val2, cond2);

        return result;
    }
}

    void OscillatorBank::processSampleAVX2(PolyphonicVoiceSoA& state,
        __m256 activeVoicesMask,
        __m256 envelopes,
        __m256 noiseMix,
        __m256 noiseBuffer,
        const float* modulatorEnvelopes,
        float formantShift,
        float stereoWidth,
        float character,
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
        (void)pulseWidth;
        (void)wavetablePosition;
        (void)k_coeffs;
        (void)a2_coeffs;

        // ----------------------------------------------------
        // Mode 0: 従来の Filterbank モード (100% オリジナル動作維持)
        // ----------------------------------------------------
        if (vocoderMode == 0)
        {
            const float* tablePtrSaw = mWavetableSaw.data();

            // 1. LEFT オシレーター波形生成 (Wavetable補間)
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


            // 2. RIGHT オシレーター波生成 (Wavetable補間)
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

            // 双一次周波数ワーピング (BFW) の事前準備
            std::vector<float> w_orig(activeBands);
            for (int k = 0; k < activeBands; ++k)
            {
                w_orig[k] = 2.0f * std::atan(g_coeffs[k]);
            }

            // formantShift (-24 to 24) を alpha (-0.5 to 0.5) にマッピング
            float alpha = std::tanh(formantShift / 24.0f * 0.45f);

            std::vector<float> warpedEnvelopes(activeBands, 0.0f);
            for (int i = 0; i < activeBands; ++i)
            {
                float w = w_orig[i];
                float sin_w = std::sin(w);
                float cos_w = std::cos(w);
                
                // ワーピング角周波数の計算
                float warped_w = w + 2.0f * std::atan((alpha * sin_w) / (1.0f - alpha * cos_w + 1e-9f));
                warped_w = std::clamp(warped_w, w_orig[0], w_orig[activeBands - 1]);
                
                // 元の周波数テーブル内で二分探索して補間インデックスを見つける
                auto it = std::lower_bound(w_orig.begin(), w_orig.end(), warped_w);
                int idx1 = static_cast<int>(std::distance(w_orig.begin(), it));
                int idx0 = std::max(0, idx1 - 1);
                if (idx1 >= activeBands) idx1 = activeBands - 1;
                
                float denom = w_orig[idx1] - w_orig[idx0];
                float frac = 0.0f;
                if (denom > 1e-6f)
                {
                    frac = (warped_w - w_orig[idx0]) / denom;
                }
                
                warpedEnvelopes[i] = modulatorEnvelopes[idx0] * (1.0f - frac) + modulatorEnvelopes[idx1] * frac;
            }

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
                        float a1 = a1_coeffs[i]; // 1.0f / (1.0f + g * (g + k))

                        // --- ZDF SVF (LEFT) - 4次直列 (S1 -> S2) 正確なVA構造へ修正 ---
                        float s1_L_s1 = state.filterS1_S1_L[i][v];
                        float s2_L_s1 = state.filterS1_S2_L[i][v];
                        float v1_L_s1 = a1 * (s1_L_s1 + g * (vL - s2_L_s1));
                        float y_bp_L_s1 = v1_L_s1;
                        float y_lp_L_s1 = s2_L_s1 + g * v1_L_s1;

                        state.filterS1_S1_L[i][v] = 2.0f * y_bp_L_s1 - s1_L_s1;
                        state.filterS1_S2_L[i][v] = 2.0f * y_lp_L_s1 - s2_L_s1;

                        // セクション 2 (S1 -> S2)
                        float s1_L_s2 = state.filterS2_S1_L[i][v];
                        float s2_L_s2 = state.filterS2_S2_L[i][v];
                        float v1_L_s2 = a1 * (s1_L_s2 + g * (y_bp_L_s1 - s2_L_s2));
                        float y_bp_L_s2 = v1_L_s2;
                        float y_lp_L_s2 = s2_L_s2 + g * v1_L_s2;

                        state.filterS2_S1_L[i][v] = 2.0f * y_bp_L_s2 - s1_L_s2;
                        state.filterS2_S2_L[i][v] = 2.0f * y_lp_L_s2 - s2_L_s2;


                        // --- ZDF SVF (RIGHT) - 4次直列 (S1 -> S2) 正確なVA構造へ修正 ---
                        float s1_R_s1 = state.filterS1_S1_R[i][v];
                        float s2_R_s1 = state.filterS1_S2_R[i][v];
                        float v1_R_s1 = a1 * (s1_R_s1 + g * (vR - s2_R_s1));
                        float y_bp_R_s1 = v1_R_s1;
                        float y_lp_R_s1 = s2_R_s1 + g * v1_R_s1;

                        state.filterS1_S1_R[i][v] = 2.0f * y_bp_R_s1 - s1_R_s1;
                        state.filterS1_S2_R[i][v] = 2.0f * y_lp_R_s1 - s2_R_s1;

                        // セクション 2 (S1 -> S2)
                        float s1_R_s2 = state.filterS2_S1_R[i][v];
                        float s2_R_s2 = state.filterS2_S2_R[i][v];
                        float v1_R_s2 = a1 * (s1_R_s2 + g * (y_bp_R_s1 - s2_R_s2));
                        float y_bp_R_s2 = v1_R_s2;
                        float y_lp_R_s2 = s2_R_s2 + g * v1_R_s2;

                        state.filterS2_S1_R[i][v] = 2.0f * y_bp_R_s2 - s1_R_s2;
                        state.filterS2_S2_R[i][v] = 2.0f * y_lp_R_s2 - s2_R_s2;


                        // 双一次周波数ワーピングされたエンベロープを使用
                        float modEnv = warpedEnvelopes[i];

                        // 周波数帯域別オルタネーティング・パンニング
                        // 低域 (80Hz以下) はセンターに固定し、高域に向かって徐々にパンニングを広げる
                        float bandFreq = 16000.0f / 3.14159265f * std::atan(g);
                        float bandWidthScale = std::clamp((bandFreq - 80.0f) / 120.0f, 0.0f, 1.0f);
                        float currentWidth = stereoWidth * bandWidthScale;

                        float theta = 3.14159265f / 4.0f; // 45度
                        if (i % 2 == 0)
                        {
                            theta += currentWidth * (3.14159265f / 4.0f);
                        }
                        else
                        {
                            theta -= currentWidth * (3.14159265f / 4.0f);
                        }
                        float panL = std::cos(theta);
                        float panR = std::sin(theta);

                        voiceSumL += y_bp_L_s2 * modEnv * panL;
                        voiceSumR += y_bp_R_s2 * modEnv * panR;
                    }

                    sumL += voiceSumL * voiceEnvelopes[v];
                    sumR += voiceSumR * voiceEnvelopes[v];
                }
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

            // 前回の-48dB改善値：音量を通常範囲にするためスケーリングを掛ける (ゲイン減衰 * 0.012f)
            outL = sumL * 0.012f;
            outR = sumR * 0.012f;
        }
        // ----------------------------------------------------
        // Mode 1: 新しい LPC モード (LPC分析・合成)
        // ----------------------------------------------------
        else
        {
            // --- 1. キャリア波形生成 (PolyBLEP 適用) ---
            __m256 sizeVec = _mm256_set1_ps(static_cast<float>(PolyphonicVoiceSoA::kWaveTableSize));
            __m256 phaseL_vec = _mm256_load_ps(state.phaseL);
            __m256 phaseR_vec = _mm256_load_ps(state.phaseR);
            __m256 phaseIncrL_vec = _mm256_load_ps(state.phaseIncrL);
            __m256 phaseIncrR_vec = _mm256_load_ps(state.phaseIncrR);

            __m256 normPhaseL = _mm256_div_ps(phaseL_vec, sizeVec);
            __m256 normPhaseR = _mm256_div_ps(phaseR_vec, sizeVec);
            __m256 dtL = _mm256_div_ps(phaseIncrL_vec, sizeVec);
            __m256 dtR = _mm256_div_ps(phaseIncrR_vec, sizeVec);

            __m256 oscL, oscR;

            if (waveform == 0) // PolyBLEP Saw
            {
                __m256 two = _mm256_set1_ps(2.0f);
                __m256 one = _mm256_set1_ps(1.0f);
                oscL = _mm256_sub_ps(_mm256_sub_ps(_mm256_mul_ps(two, normPhaseL), one), polyBlepCorrectionAVX2(normPhaseL, dtL));
                oscR = _mm256_sub_ps(_mm256_sub_ps(_mm256_mul_ps(two, normPhaseR), one), polyBlepCorrectionAVX2(normPhaseR, dtR));
            }
            else if (waveform == 1) // PolyBLEP Pulse
            {
                __m256 half = _mm256_set1_ps(0.5f);
                __m256 naiveSquareL = _mm256_blendv_ps(_mm256_set1_ps(-1.0f), _mm256_set1_ps(1.0f), _mm256_cmp_ps(normPhaseL, half, _CMP_LT_OQ));
                __m256 naiveSquareR = _mm256_blendv_ps(_mm256_set1_ps(-1.0f), _mm256_set1_ps(1.0f), _mm256_cmp_ps(normPhaseR, half, _CMP_LT_OQ));
                
                __m256 normPhaseL_shifted = _mm256_blendv_ps(_mm256_add_ps(normPhaseL, half), _mm256_sub_ps(normPhaseL, half), _mm256_cmp_ps(normPhaseL, half, _CMP_GE_OQ));
                __m256 normPhaseR_shifted = _mm256_blendv_ps(_mm256_add_ps(normPhaseR, half), _mm256_sub_ps(normPhaseR, half), _mm256_cmp_ps(normPhaseR, half, _CMP_GE_OQ));

                __m256 blep0L = polyBlepCorrectionAVX2(normPhaseL, dtL);
                __m256 blep5L = polyBlepCorrectionAVX2(normPhaseL_shifted, dtL);
                oscL = _mm256_sub_ps(_mm256_add_ps(naiveSquareL, blep0L), blep5L);

                __m256 blep0R = polyBlepCorrectionAVX2(normPhaseR, dtR);
                __m256 blep5R = polyBlepCorrectionAVX2(normPhaseR_shifted, dtR);
                oscR = _mm256_sub_ps(_mm256_add_ps(naiveSquareR, blep0R), blep5R);
            }
            else // Wavetable
            {
                const float* tablePtr = mCustomWavetable.data();
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

            // --- 2. 有声/無声 & 残差ブレンド ---
            __m256 oneMinusNoiseMix = _mm256_sub_ps(_mm256_set1_ps(1.0f), noiseMix);
            __m256 residualVec = _mm256_load_ps(state.lpcResidual);

            __m256 wP = _mm256_set1_ps(character);
            __m256 wR = _mm256_sub_ps(_mm256_set1_ps(1.0f), wP);

            // excitation = oneMinusNoiseMix * (wP * osc + wR * residual) + noiseMix * noiseBuffer
            __m256 voicedExcL = _mm256_add_ps(_mm256_mul_ps(wP, oscL), _mm256_mul_ps(wR, residualVec));
            __m256 voicedExcR = _mm256_add_ps(_mm256_mul_ps(wP, oscR), _mm256_mul_ps(wR, residualVec));

            __m256 excitationL = _mm256_add_ps(_mm256_mul_ps(oneMinusNoiseMix, voicedExcL), _mm256_mul_ps(noiseMix, noiseBuffer));
            __m256 excitationR = _mm256_add_ps(_mm256_mul_ps(oneMinusNoiseMix, voicedExcR), _mm256_mul_ps(noiseMix, noiseBuffer));

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
            float sumL = SimdUtils::horizontalSum(outVoiceL);
            float sumR = SimdUtils::horizontalSum(outVoiceR);

            __m256 bitMask = _mm256_cmp_ps(activeVoicesMask, _mm256_setzero_ps(), _CMP_GT_OQ);

            // オシレーター位相の更新
            phaseL_vec = _mm256_add_ps(phaseL_vec, phaseIncrL_vec);
            phaseR_vec = _mm256_add_ps(phaseR_vec, phaseIncrR_vec);

            phaseL_vec = _mm256_and_ps(phaseL_vec, bitMask);
            phaseR_vec = _mm256_and_ps(phaseR_vec, bitMask);

            // ブランクレス位相ラッピング
            __m256 cmpL = _mm256_cmp_ps(phaseL_vec, sizeVec, _CMP_GE_OQ);
            __m256 subL = _mm256_and_ps(cmpL, sizeVec);
            phaseL_vec = _mm256_sub_ps(phaseL_vec, subL);

            __m256 cmpR = _mm256_cmp_ps(phaseR_vec, sizeVec, _CMP_GE_OQ);
            __m256 subR = _mm256_and_ps(cmpR, sizeVec);
            phaseR_vec = _mm256_sub_ps(phaseR_vec, subR);

            _mm256_store_ps(state.phaseL, phaseL_vec);
            _mm256_store_ps(state.phaseR, phaseR_vec);

            // LPCモードの出力スケーリング
            outL = sumL * 0.06f;
            outR = sumR * 0.06f;
        }
    }

} // namespace DSP