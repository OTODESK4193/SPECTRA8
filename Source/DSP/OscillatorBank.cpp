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
                                       __m256 noiseMix,
                                       __m256 noiseBuffer,
                                       float& outL,
                                       float& outR)
{
    // テーブルへのポインタ (今回は Saw テーブルを使用)
    const float* tablePtrSaw = mWavetableSaw.data();

    // 1. LEFT オシレーター波形生成 (Wavetable補間)
    __m256 phaseL_vec = _mm256_load_ps(state.phaseL);
    __m256i idx0_L = _mm256_cvttps_epi32(phaseL_vec);
    __m256i idx1_L = _mm256_add_epi32(idx0_L, _mm256_set1_epi32(1));

    __m256i maskVec = _mm256_set1_epi32(PolyphonicVoiceSoA::kWaveTableMask);
    idx0_L = _mm256_and_si256(idx0_L, maskVec);
    idx1_L = _mm256_and_si256(idx1_L, maskVec);

    __m256 y0_L = _mm256_i32gather_ps(tablePtrSaw, idx0_L, 4);
    __m256 y1_L = _mm256_i32gather_ps(tablePtrSaw, idx1_L, 4);

    __m256 idx0_L_float = _mm256_cvtepi32_ps(idx0_L);
    __m256 t_L = _mm256_sub_ps(phaseL_vec, idx0_L_float);

    // FMA補間: out_L = y0_L * (1 - t_L) + y1_L * t_L
    __m256 tmp_L = _mm256_fnmadd_ps(t_L, y0_L, y0_L);
    __m256 oscL = _mm256_fmadd_ps(t_L, y1_L, tmp_L);


    // 2. RIGHT オシレーター波形生成 (Wavetable補間)
    __m256 phaseR_vec = _mm256_load_ps(state.phaseR);
    __m256i idx0_R = _mm256_cvttps_epi32(phaseR_vec);
    __m256i idx1_R = _mm256_add_epi32(idx0_R, _mm256_set1_epi32(1));

    idx0_R = _mm256_and_si256(idx0_R, maskVec);
    idx1_R = _mm256_and_si256(idx1_R, maskVec);

    __m256 y0_R = _mm256_i32gather_ps(tablePtrSaw, idx0_R, 4);
    __m256 y1_R = _mm256_i32gather_ps(tablePtrSaw, idx1_R, 4);

    __m256 idx0_R_float = _mm256_cvtepi32_ps(idx0_R);
    __m256 t_R = _mm256_sub_ps(phaseR_vec, idx0_R_float);

    // FMA補間
    __m256 tmp_R = _mm256_fnmadd_ps(t_R, y0_R, y0_R);
    __m256 oscR = _mm256_fmadd_ps(t_R, y1_R, tmp_R);


    // 3. 有声音(オシレーター)と無声音(ノイズ)のブレンド
    // x = (1.0 - noiseMix) * osc + noiseMix * noise
    __m256 oneMinusNoiseMix = _mm256_sub_ps(_mm256_set1_ps(1.0f), noiseMix);
    __m256 excitationL = _mm256_fmadd_ps(oneMinusNoiseMix, oscL, _mm256_mul_ps(noiseMix, noiseBuffer));
    __m256 excitationR = _mm256_fmadd_ps(oneMinusNoiseMix, oscR, _mm256_mul_ps(noiseMix, noiseBuffer));


    // 4. LPC合成IIRフィルタリング (24次のループ)
    // y[n] = x[n] - sum_{i=1}^P a_i * y[n-i]
    uint32_t wPtr = state.filterWritePtr;
    __m256 yL_sum = excitationL;
    __m256 yR_sum = excitationR;
    int rPtr = static_cast<int>(wPtr);

    for (int i = 0; i < PolyphonicVoiceSoA::kLpcOrder; ++i)
    {
        rPtr--;
        if (rPtr < 0)
        {
            rPtr = PolyphonicVoiceSoA::kLpcOrder - 1;
        }

        __m256 histL = _mm256_load_ps(&state.filterHistoryL[rPtr][0]);
        __m256 coeffL = _mm256_load_ps(&state.filterCoeffsL[i][0]);
        yL_sum = _mm256_fnmadd_ps(coeffL, histL, yL_sum);

        __m256 histR = _mm256_load_ps(&state.filterHistoryR[rPtr][0]);
        __m256 coeffR = _mm256_load_ps(&state.filterCoeffsR[i][0]);
        yR_sum = _mm256_fnmadd_ps(coeffR, histR, yR_sum);
    }

    // 5. ボイス有効無効マスクの適用
    yL_sum = _mm256_mul_ps(yL_sum, activeVoicesMask);
    yR_sum = _mm256_mul_ps(yR_sum, activeVoicesMask);

    // 6. 出力履歴を円形バッファに書き戻す
    _mm256_store_ps(&state.filterHistoryL[wPtr][0], yL_sum);
    _mm256_store_ps(&state.filterHistoryR[wPtr][0], yR_sum);

    // 円形バッファポインタをインクリメント
    state.filterWritePtr = (wPtr + 1) % PolyphonicVoiceSoA::kLpcOrder;


    // 7. オシレーター位相の更新
    __m256 phaseIncrL_vec = _mm256_load_ps(state.phaseIncrL);
    __m256 phaseIncrR_vec = _mm256_load_ps(state.phaseIncrR);
    phaseL_vec = _mm256_add_ps(phaseL_vec, phaseIncrL_vec);
    phaseR_vec = _mm256_add_ps(phaseR_vec, phaseIncrR_vec);

    // ブランチレス位相ラッピング
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


    // 8. 左右それぞれ8ボイスを水平加算してスカラーミックス出力を得る
    outL = SimdUtils::horizontalSum(yL_sum);
    outR = SimdUtils::horizontalSum(yR_sum);
}

} // namespace DSP
