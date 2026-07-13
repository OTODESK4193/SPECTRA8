// ==========================================
// File: FilterbankVocoder.cpp
// アナログ風フィルターバンク・ボコーダー (スカラー設計)
// ==========================================
#include "FilterbankVocoder.h"

FilterbankVocoder::FilterbankVocoder()
{
    reset();
}

void FilterbankVocoder::prepare(double sampleRate)
{
    mSampleRate = sampleRate;

    // 分析側の周波数設計 (幾何的配置: 80Hz - 7500Hz)
    const float fMin = 80.0f;
    const float fMax = 7500.0f;
    const float mMin = 2595.0f * std::log10(1.0f + fMin / 700.0f);
    const float mMax = 2595.0f * std::log10(1.0f + fMax / 700.0f);

    for (int i = 0; i < kMaxBands; ++i)
    {
        float mVal = mMin + (mMax - mMin) * (static_cast<float>(i) / (kMaxBands - 1));
        float freq = 700.0f * (std::pow(10.0f, mVal / 2595.0f) - 1.0f);
        mBandF0[(size_t)i] = freq;
    }

    // Subtractive(減算型)用のバンド端周波数: 隣接バンド中心の幾何平均 (対数軸上の中点)
    for (int i = 1; i < kMaxBands; ++i)
        mBandEdges[(size_t)i] = std::sqrt(mBandF0[(size_t)(i - 1)] * mBandF0[(size_t)i]);
    mBandEdges[0] = mBandF0[0] * mBandF0[0] / mBandEdges[1];                     // 対数軸外挿
    mBandEdges[kMaxBands] = juce::jmin(7800.0f,
        mBandF0[kMaxBands - 1] * mBandF0[kMaxBands - 1] / mBandEdges[kMaxBands - 1]);

    // 分析側 LR4-LPF 係数 (Q=0.707固定, エッジ周波数は不変のため事前計算)
    const float kButterworth = 1.41421356f; // k = 1/Q, Q = 0.707
    for (int i = 0; i <= kMaxBands; ++i)
    {
        const float g = std::tan(3.14159265f * mBandEdges[(size_t)i] / (float)kInternalSampleRate);
        mEdgeG[(size_t)i] = g;
        mEdgeA1[(size_t)i] = 1.0f / (1.0f + g * (g + kButterworth));
    }

    // 分析側の各バンドをピーク0dBに揃える正規化ゲイン (エッジ固定のため事前計算)
    for (int i = 0; i < kMaxBands; ++i)
        mBandNorm[(size_t)i] = computeBandNorm(mBandEdges[(size_t)i], mBandEdges[(size_t)i + 1]);

    reset();
}

void FilterbankVocoder::reset()
{
    for (int i = 0; i < kMaxBands; ++i)
    {
        mAnalSvf[(size_t)i][0].reset();
        mAnalSvf[(size_t)i][1].reset();
        mAnalSub[(size_t)i].reset();

        mSynthSvfL[(size_t)i][0].reset();
        mSynthSvfL[(size_t)i][1].reset();
        mSynthSvfR[(size_t)i][0].reset();
        mSynthSvfR[(size_t)i][1].reset();

        mSynthSubL[(size_t)i].reset();
        mSynthSubR[(size_t)i].reset();

        mEnvValues[(size_t)i] = 0.0f;
    }
}

void FilterbankVocoder::processSample(float modulator, float carrierL, float carrierR,
                                      float& outL, float& outR,
                                      int bandCount, float character,
                                      float formantShift, float formantStretch,
                                      int filterbankType, float stereoWidth,
                                      const std::array<std::atomic<float>, kMaxBands>& bandGains,
                                      std::array<std::atomic<float>, kMaxBands>& bandLevelsForUi) noexcept
{
    const int activeBands = juce::jlimit(8, kMaxBands, bandCount);
    
    // フォルマント・シフト倍率
    const float shiftFactor = std::pow(2.0f, formantShift / 12.0f);
    const float Q = 10.0f; // BPF用のクオリティファクタ
    const float k_lr4 = 1.41421356f; // Q = 0.707 (LPF 2次 sections)

    float sumL = 0.0f;
    float sumR = 0.0f;

    for (int i = 0; i < activeBands; ++i)
    {
        const float f0 = mBandF0[(size_t)i];

        // ----------------------------------------------------
        // 1. 分析側（モジュレーター）処理
        // ----------------------------------------------------
        // 分析ロジックは analyzeForMeter と共通化 (updateAnalysisBand)。
        // mEnvValues[i] を更新する。
        updateAnalysisBand(i, modulator, filterbankType, character);

        // UIレベルメーター用に通知
        bandLevelsForUi[(size_t)i].store(mEnvValues[(size_t)i]);

        // ----------------------------------------------------
        // 2. 合成側（キャリア）処理
        // ----------------------------------------------------
        // フォルマント・シフトとストレッチの適用
        float f0_synth = f0 * formantStretch * shiftFactor;
        f0_synth = juce::jlimit(50.0f, 7800.0f, f0_synth);

        float g_synth, k_synth, a1_synth;
        computeFilterCoeffs(f0_synth, Q, g_synth, k_synth, a1_synth);

        float carrierOutL = 0.0f;
        float carrierOutR = 0.0f;

        if (filterbankType == 0) // Bandpass Bank
        {
            // LEFT
            auto& sL1 = mSynthSvfL[(size_t)i][0];
            float vL1 = a1_synth * (sL1.s1 + g_synth * (carrierL - sL1.s2));
            float y_bp_L1 = vL1;
            float y_lp_L1 = sL1.s2 + g_synth * vL1;
            sL1.s1 = 2.0f * y_bp_L1 - sL1.s1;
            sL1.s2 = 2.0f * y_lp_L1 - sL1.s2;

            auto& sL2 = mSynthSvfL[(size_t)i][1];
            float vL2 = a1_synth * (sL2.s1 + g_synth * (y_bp_L1 - sL2.s2));
            float y_bp_L2 = vL2;
            float y_lp_L2 = sL2.s2 + g_synth * vL2;
            sL2.s1 = 2.0f * y_bp_L2 - sL2.s1;
            sL2.s2 = 2.0f * y_lp_L2 - sL2.s2;

            carrierOutL = y_bp_L2;

            // RIGHT
            auto& sR1 = mSynthSvfR[(size_t)i][0];
            float vR1 = a1_synth * (sR1.s1 + g_synth * (carrierR - sR1.s2));
            float y_bp_R1 = vR1;
            float y_lp_R1 = sR1.s2 + g_synth * vR1;
            sR1.s1 = 2.0f * y_bp_R1 - sR1.s1;
            sR1.s2 = 2.0f * y_lp_R1 - sR1.s2;

            auto& sR2 = mSynthSvfR[(size_t)i][1];
            float vR2 = a1_synth * (sR2.s1 + g_synth * (y_bp_R1 - sR2.s2));
            float y_bp_R2 = vR2;
            float y_lp_R2 = sR2.s2 + g_synth * vR2;
            sR2.s1 = 2.0f * y_bp_R2 - sR2.s1;
            sR2.s2 = 2.0f * y_lp_R2 - sR2.s2;

            carrierOutR = y_bp_R2;
        }
        else // Subtractive (合成側もバンド端エッジの8次HPF→8次LPF直列 + ピーク正規化)
        {
            // フォルマント・シフト/ストレッチをバンド端周波数に適用
            float eHi = mBandEdges[(size_t)i + 1] * formantStretch * shiftFactor;
            eHi = juce::jlimit(60.0f, 7800.0f, eHi);
            float eLo = mBandEdges[(size_t)i] * formantStretch * shiftFactor;
            eLo = juce::jlimit(50.0f, eHi * 0.98f, eLo); // 上端との逆転を防止

            const float gLo_s  = std::tan(3.14159265f * eLo / (float)kInternalSampleRate);
            const float a1Lo_s = 1.0f / (1.0f + gLo_s * (gLo_s + k_lr4));
            const float gHi_s  = std::tan(3.14159265f * eHi / (float)kInternalSampleRate);
            const float a1Hi_s = 1.0f / (1.0f + gHi_s * (gHi_s + k_lr4));
            const float norm_s = computeBandNorm(eLo, eHi);

            // LEFT
            auto& subL = mSynthSubL[(size_t)i];
            float yL = carrierL;
            for (int sec = 0; sec < 4; ++sec) yL = processLpfSection(subL.hiLp[(size_t)sec], yL, gHi_s, a1Hi_s);
            for (int sec = 0; sec < 4; ++sec) yL = processHpfSection(subL.loHp[(size_t)sec], yL, gLo_s, a1Lo_s);
            carrierOutL = yL * norm_s;

            // RIGHT
            auto& subR = mSynthSubR[(size_t)i];
            float yR = carrierR;
            for (int sec = 0; sec < 4; ++sec) yR = processLpfSection(subR.hiLp[(size_t)sec], yR, gHi_s, a1Hi_s);
            for (int sec = 0; sec < 4; ++sec) yR = processHpfSection(subR.loHp[(size_t)sec], yR, gLo_s, a1Lo_s);
            carrierOutR = yR * norm_s;
        }

        // 変調 (EQゲイン適用)
        const float gain = bandGains[(size_t)i].load();
        const float modulatedL = carrierOutL * mEnvValues[(size_t)i] * gain;
        const float modulatedR = carrierOutR * mEnvValues[(size_t)i] * gain;

        // ステレオパンニング処理 (帯域交互パンニング)
        // 80Hz以下は定位保護のためセンターに固定、高域にいくほどパン幅を広げる
        const float bandWidthScale = std::clamp((f0 - 80.0f) / 120.0f, 0.0f, 1.0f);
        const float currentWidth = stereoWidth * bandWidthScale;

        float theta = 3.14159265f / 4.0f; // 45度
        if (i % 2 == 0)
            theta += currentWidth * (3.14159265f / 4.0f);
        else
            theta -= currentWidth * (3.14159265f / 4.0f);

        const float panL = std::cos(theta);
        const float panR = std::sin(theta);

        sumL += modulatedL * panL;
        sumR += modulatedR * panR;
    }

    outL = sumL;
    outR = sumR;
}
