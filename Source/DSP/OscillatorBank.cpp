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

        // 1. ノコギリ波
        mWavetableSaw[i] = 2.0f * phase - 1.0f;

        // 2. パルス波
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
    }
}

void OscillatorBank::processSampleAVX2(PolyphonicVoiceSoA& state,
                                       __m256 activeVoicesMask,
                                       __m256 envelopes,
                                       __m256 noiseMix,
                                       __m256 noiseBuffer,
                                       const float* modulatorEnvelopes,
                                       float formantShift,
                                       const float* b0_coeffs,
                                       const float* b2_coeffs,
                                       const float* a1_coeffs,
                                       const float* a2_coeffs,
                                       float& outL,
                                       float& outR)
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


    // 4. 20バンド・バンドパス・フィルタバンクによる変調とボイス加算
    float actMask[8];
    float excL[8];
    float excR[8];
    float voiceEnvelopes[8];

    _mm256_storeu_ps(actMask, activeVoicesMask);
    _mm256_storeu_ps(excL, excitationL);
    _mm256_storeu_ps(excR, excitationR);
    _mm256_storeu_ps(voiceEnvelopes, envelopes);

    float sumL = 0.0f;
    float sumR = 0.0f;

    for (int v = 0; v < 8; ++v)
    {
        if (actMask[v] > 0.0f)
        {
            float vL = excL[v];
            float vR = excR[v];

            float voiceSumL = 0.0f;
            float voiceSumR = 0.0f;

            for (int i = 0; i < PolyphonicVoiceSoA::kNumBands; ++i)
            {
                // Biquad フィルタ実行 (LEFT) - 4次直列 (S1 -> S2)
                // --- セクション 1 ---
                float xL_s1 = vL;
                float x1_L_s1 = state.filterS1_X1_L[i][v];
                float x2_L_s1 = state.filterS1_X2_L[i][v];
                float y1_L_s1 = state.filterS1_Y1_L[i][v];
                float y2_L_s1 = state.filterS1_Y2_L[i][v];

                float yL_s1 = b0_coeffs[i] * xL_s1 + b2_coeffs[i] * x2_L_s1 - a1_coeffs[i] * y1_L_s1 - a2_coeffs[i] * y2_L_s1;
                if (std::isnan(yL_s1) || std::isinf(yL_s1)) yL_s1 = 0.0f;

                state.filterS1_X2_L[i][v] = x1_L_s1;
                state.filterS1_X1_L[i][v] = xL_s1;
                state.filterS1_Y2_L[i][v] = y1_L_s1;
                state.filterS1_Y1_L[i][v] = yL_s1;

                // --- セクション 2 ---
                float xL_s2 = yL_s1;
                float x1_L_s2 = state.filterS2_X1_L[i][v];
                float x2_L_s2 = state.filterS2_X2_L[i][v];
                float y1_L_s2 = state.filterS2_Y1_L[i][v];
                float y2_L_s2 = state.filterS2_Y2_L[i][v];

                float yL_s2 = b0_coeffs[i] * xL_s2 + b2_coeffs[i] * x2_L_s2 - a1_coeffs[i] * y1_L_s2 - a2_coeffs[i] * y2_L_s2;
                if (std::isnan(yL_s2) || std::isinf(yL_s2)) yL_s2 = 0.0f;

                state.filterS2_X2_L[i][v] = x1_L_s2;
                state.filterS2_X1_L[i][v] = xL_s2;
                state.filterS2_Y2_L[i][v] = y1_L_s2;
                state.filterS2_Y1_L[i][v] = yL_s2;


                // Biquad フィルタ実行 (RIGHT) - 4次直列 (S1 -> S2)
                // --- セクション 1 ---
                float xR_s1 = vR;
                float x1_R_s1 = state.filterS1_X1_R[i][v];
                float x2_R_s1 = state.filterS1_X2_R[i][v];
                float y1_R_s1 = state.filterS1_Y1_R[i][v];
                float y2_R_s1 = state.filterS1_Y2_R[i][v];

                float yR_s1 = b0_coeffs[i] * xR_s1 + b2_coeffs[i] * x2_R_s1 - a1_coeffs[i] * y1_R_s1 - a2_coeffs[i] * y2_R_s1;
                if (std::isnan(yR_s1) || std::isinf(yR_s1)) yR_s1 = 0.0f;

                state.filterS1_X2_R[i][v] = x1_R_s1;
                state.filterS1_X1_R[i][v] = xR_s1;
                state.filterS1_Y2_R[i][v] = y1_R_s1;
                state.filterS1_Y1_R[i][v] = yR_s1;

                // --- セクション 2 ---
                float xR_s2 = yR_s1;
                float x1_R_s2 = state.filterS2_X1_R[i][v];
                float x2_R_s2 = state.filterS2_X2_R[i][v];
                float y1_R_s2 = state.filterS2_Y1_R[i][v];
                float y2_R_s2 = state.filterS2_Y2_R[i][v];

                float yR_s2 = b0_coeffs[i] * xR_s2 + b2_coeffs[i] * x2_R_s2 - a1_coeffs[i] * y1_R_s2 - a2_coeffs[i] * y2_R_s2;
                if (std::isnan(yR_s2) || std::isinf(yR_s2)) yR_s2 = 0.0f;

                state.filterS2_X2_R[i][v] = x1_R_s2;
                state.filterS2_X1_R[i][v] = xR_s2;
                state.filterS2_Y2_R[i][v] = y1_R_s2;
                state.filterS2_Y1_R[i][v] = yR_s2;


                // フォルマントシフト写像
                float srcIdx = static_cast<float>(i) - formantShift;
                srcIdx = std::clamp(srcIdx, 0.0f, static_cast<float>(PolyphonicVoiceSoA::kNumBands - 1));
                int idx0 = static_cast<int>(srcIdx);
                int idx1 = std::min(PolyphonicVoiceSoA::kNumBands - 1, idx0 + 1);
                float frac = srcIdx - idx0;
                float modEnv = modulatorEnvelopes[idx0] * (1.0f - frac) + modulatorEnvelopes[idx1] * frac;

                voiceSumL += yL_s2 * modEnv;
                voiceSumR += yR_s2 * modEnv;
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

    // 非アクティブボイスの位相もNaN/異常値化を防ぐためビットクリア
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

    // 配列状態に書き戻す
    _mm256_store_ps(state.phaseL, phaseL_vec);
    _mm256_store_ps(state.phaseR, phaseR_vec);

    // 8. 左右のチャネル出力を引数に書き戻し (無音不具合の解消)
    outL = sumL;
    outR = sumR;
}

} // namespace DSP
