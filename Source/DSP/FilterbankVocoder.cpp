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

    reset();
}

void FilterbankVocoder::reset()
{
    for (int i = 0; i < kMaxBands; ++i)
    {
        mAnalSvf[(size_t)i][0].reset();
        mAnalSvf[(size_t)i][1].reset();
        mAnalLr4[(size_t)i].reset();

        mSynthSvfL[(size_t)i][0].reset();
        mSynthSvfL[(size_t)i][1].reset();
        mSynthSvfR[(size_t)i][0].reset();
        mSynthSvfR[(size_t)i][1].reset();

        mSynthLr4L[(size_t)i].reset();
        mSynthLr4R[(size_t)i].reset();

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
        float analOut = 0.0f;
        float g_anal, k_anal, a1_anal;
        computeFilterCoeffs(f0, Q, g_anal, k_anal, a1_anal);

        if (filterbankType == 0) // Bandpass Bank
        {
            // ZDF SVF 1段目 (BPF)
            auto& s1 = mAnalSvf[(size_t)i][0];
            float v1 = a1_anal * (s1.s1 + g_anal * (modulator - s1.s2));
            float y_bp = v1;
            float y_lp = s1.s2 + g_anal * v1;
            s1.s1 = 2.0f * y_bp - s1.s1;
            s1.s2 = 2.0f * y_lp - s1.s2;

            // ZDF SVF 2段目 (BPF)
            auto& s2 = mAnalSvf[(size_t)i][1];
            float v1_2 = a1_anal * (s2.s1 + g_anal * (y_bp - s2.s2));
            float y_bp_2 = v1_2;
            float y_lp_2 = s2.s2 + g_anal * v1_2;
            s2.s1 = 2.0f * y_bp_2 - s2.s1;
            s2.s2 = 2.0f * y_lp_2 - s2.s2;

            analOut = y_bp_2;
        }
        else // Subtractive / LR4
        {
            auto& lr4 = mAnalLr4[(size_t)i];
            const float a1_lr4 = 1.0f / (1.0f + g_anal * (g_anal + k_lr4));

            // Section 1 (LPF)
            float v1_s1 = a1_lr4 * (lr4.lpf1.s1 + g_anal * (modulator - lr4.lpf1.s2));
            float y_lp_s1 = lr4.lpf1.s2 + g_anal * v1_s1;
            lr4.lpf1.s1 = 2.0f * v1_s1 - lr4.lpf1.s1;
            lr4.lpf1.s2 = 2.0f * y_lp_s1 - lr4.lpf1.s2;

            // Section 2 (HPF)
            float v1_s2 = a1_lr4 * (lr4.hpf1.s1 + g_anal * (y_lp_s1 - lr4.hpf1.s2));
            float y_lp_s2 = lr4.hpf1.s2 + g_anal * v1_s2;
            float y_hp = y_lp_s1 - k_lr4 * v1_s2 - y_lp_s2;
            lr4.hpf1.s1 = 2.0f * v1_s2 - lr4.hpf1.s1;
            lr4.hpf1.s2 = 2.0f * y_lp_s2 - lr4.hpf1.s2;

            analOut = y_hp;
        }

        // エンベロープ追従（キャラクター値でアタック/リリースタイムを調整）
        // character: 0.0 (遅い) 〜 1.0 (極めて速い)
        float env = std::abs(analOut);
        float baseAttack = 0.02f;  // 約20ms相当
        float baseRelease = 0.003f; // 約150ms相当
        float att = juce::jlimit(0.001f, 0.2f, baseAttack * (character * 4.0f + 0.1f));
        float rel = juce::jlimit(0.0002f, 0.05f, baseRelease * (character * 4.0f + 0.1f));

        float coeff = (env > mEnvValues[(size_t)i]) ? att : rel;
        mEnvValues[(size_t)i] += coeff * (env - mEnvValues[(size_t)i]);

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
        else // Subtractive / LR4
        {
            const float a1_lr4_synth = 1.0f / (1.0f + g_synth * (g_synth + k_lr4));

            // LEFT
            auto& lr4L = mSynthLr4L[(size_t)i];
            float vL_s1 = a1_lr4_synth * (lr4L.lpf1.s1 + g_synth * (carrierL - lr4L.lpf1.s2));
            float y_lp_L_s1 = lr4L.lpf1.s2 + g_synth * vL_s1;
            lr4L.lpf1.s1 = 2.0f * vL_s1 - lr4L.lpf1.s1;
            lr4L.lpf1.s2 = 2.0f * y_lp_L_s1 - lr4L.lpf1.s2;

            float vL_s2 = a1_lr4_synth * (lr4L.hpf1.s1 + g_synth * (y_lp_L_s1 - lr4L.hpf1.s2));
            float y_lp_L_s2 = lr4L.hpf1.s2 + g_synth * vL_s2;
            float y_hp_L = y_lp_L_s1 - k_lr4 * vL_s2 - y_lp_L_s2;
            lr4L.hpf1.s1 = 2.0f * vL_s2 - lr4L.hpf1.s1;
            lr4L.hpf1.s2 = 2.0f * y_lp_L_s2 - lr4L.hpf1.s2;

            carrierOutL = y_hp_L;

            // RIGHT
            auto& lr4R = mSynthLr4R[(size_t)i];
            float vR_s1 = a1_lr4_synth * (lr4R.lpf1.s1 + g_synth * (carrierR - lr4R.lpf1.s2));
            float y_lp_R_s1 = lr4R.lpf1.s2 + g_synth * vR_s1;
            lr4R.lpf1.s1 = 2.0f * vR_s1 - lr4R.lpf1.s1;
            lr4R.lpf1.s2 = 2.0f * y_lp_R_s1 - lr4R.lpf1.s2;

            float vR_s2 = a1_lr4_synth * (lr4R.hpf1.s1 + g_synth * (y_lp_R_s1 - lr4R.hpf1.s2));
            float y_lp_R_s2 = lr4R.hpf1.s2 + g_synth * vR_s2;
            float y_hp_R = y_lp_R_s1 - k_lr4 * vR_s2 - y_lp_R_s2;
            lr4R.hpf1.s1 = 2.0f * vR_s2 - lr4R.hpf1.s1;
            lr4R.hpf1.s2 = 2.0f * y_lp_R_s2 - lr4R.hpf1.s2;

            carrierOutR = y_hp_R;
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
