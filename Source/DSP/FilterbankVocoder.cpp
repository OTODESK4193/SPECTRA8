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
    mCurBands = 0;              // 次のprocessで必ず再構築させる
    rebuildLayout(kMaxBands);
    reset();
}

// バンドレイアウト構築: bands 本で 80〜7500Hz 全域をmelスケール分割する。
// 旧実装は48バンド固定レイアウトの下から bandCount 本だけを使っていたため、
// BANDS を下げると高域が消えていた(8バンド時は80〜数百Hzのみ)。
// バンド数変更時に全域を再スパンし、「8バンドでも80-7500Hzを8分割」にする。
void FilterbankVocoder::rebuildLayout(int bands) noexcept
{
    bands = juce::jlimit(8, kMaxBands, bands);
    if (bands == mCurBands)
        return;
    mCurBands = bands;

    // 分析側の周波数設計 (melスケール配置: 80Hz - 7500Hz)
    const float fMin = 80.0f;
    const float fMax = 7500.0f;
    const float mMin = 2595.0f * std::log10(1.0f + fMin / 700.0f);
    const float mMax = 2595.0f * std::log10(1.0f + fMax / 700.0f);

    for (int i = 0; i < bands; ++i)
    {
        float mVal = mMin + (mMax - mMin) * (static_cast<float>(i) / (float)(bands - 1));
        float freq = 700.0f * (std::pow(10.0f, mVal / 2595.0f) - 1.0f);
        mBandF0[(size_t)i] = freq;
    }

    // バンド端周波数: 隣接バンド中心の幾何平均 (対数軸上の中点)
    for (int i = 1; i < bands; ++i)
        mBandEdges[(size_t)i] = std::sqrt(mBandF0[(size_t)(i - 1)] * mBandF0[(size_t)i]);
    mBandEdges[0] = mBandF0[0] * mBandF0[0] / mBandEdges[1];                     // 対数軸外挿
    mBandEdges[(size_t)bands] = juce::jmin(7800.0f,
        mBandF0[(size_t)(bands - 1)] * mBandF0[(size_t)(bands - 1)] / mBandEdges[(size_t)(bands - 1)]);

    // バンド間隔連動の基準Q: Q_i = f0_i / (上端エッジ - 下端エッジ)。
    // 定オーバーラップ設計。バンド数が少ないほど1本あたりの帯域が広く=Qが低くなり、
    // 少バンドでも全域が隙間なくカバーされる。
    for (int i = 0; i < bands; ++i)
    {
        const float bw = mBandEdges[(size_t)i + 1] - mBandEdges[(size_t)i];
        mBandQ[(size_t)i] = juce::jlimit(1.5f, 24.0f, mBandF0[(size_t)i] / juce::jmax(1.0f, bw));
    }

    // レイアウトが変わったのでフィルタ状態をリセット (残留状態による不整合防止)
    reset();
}

void FilterbankVocoder::reset()
{
    for (int i = 0; i < kMaxBands; ++i)
    {
        mAnalSvf[(size_t)i][0].reset();
        mAnalSvf[(size_t)i][1].reset();

        mSynthSvfL[(size_t)i][0].reset();
        mSynthSvfL[(size_t)i][1].reset();
        mSynthSvfR[(size_t)i][0].reset();
        mSynthSvfR[(size_t)i][1].reset();

        mEnvValues[(size_t)i] = 0.0f;
    }
}

void FilterbankVocoder::processSample(float modulator, float carrierL, float carrierR,
                                      float& outL, float& outR,
                                      int bandCount, float character, float resonance,
                                      float formantShift, float formantStretch,
                                      float stereoWidth,
                                      const std::array<std::atomic<float>, kMaxBands>& bandGains,
                                      std::array<std::atomic<float>, kMaxBands>& bandLevelsForUi) noexcept
{
    const int activeBands = juce::jlimit(8, kMaxBands, bandCount);
    rebuildLayout(activeBands);   // バンド数変更時のみ全域を再スパン

    // フォルマント・シフト倍率
    const float shiftFactor = std::pow(2.0f, formantShift / 12.0f);

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
        updateAnalysisBand(i, modulator, character, resonance);

        // UIレベルメーター用に通知 (正規化後の帯域振幅 × 表示スケール)
        bandLevelsForUi[(size_t)i].store(mEnvValues[(size_t)i] * kMeterGain);

        // ----------------------------------------------------
        // 2. 合成側（キャリア）処理
        // ----------------------------------------------------
        // フォルマント・シフトとストレッチの適用
        float f0_synth = f0 * formantStretch * shiftFactor;
        f0_synth = juce::jlimit(50.0f, 7800.0f, f0_synth);

        // バンド間隔連動Q × Resonance (分析側と同一)
        const float qBand = juce::jlimit(1.0f, 40.0f, mBandQ[(size_t)i] * resonance);
        const float invQ2 = 1.0f / (qBand * qBand);

        float g_synth, k_synth, a1_synth;
        computeFilterCoeffs(f0_synth, qBand, g_synth, k_synth, a1_synth);

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

        const float carrierOutL = y_bp_L2 * invQ2; // 中心利得0dBに正規化

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

        const float carrierOutR = y_bp_R2 * invQ2; // 中心利得0dBに正規化

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

    // 出力メイクアップ。分析・合成の中心利得0dB正規化後に、
    // 旧実装と同一の最終音量へ揃える較正値(§音量ユニティ化)。
    // バンド数補償: バンドが少ないほど隣接オーバーラップの加算利得が減るため、
    // sqrt(48/bands) で概ね一定の出力レベルに揃える (48バンド時=1.0で従来同一)。
    const float bandComp = std::sqrt((float)kMaxBands / (float)activeBands);
    const float mk = kBpfMakeup * bandComp;
    outL = sumL * mk;
    outR = sumR * mk;
}
