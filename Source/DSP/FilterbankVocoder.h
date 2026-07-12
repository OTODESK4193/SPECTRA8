// ==========================================
// File: FilterbankVocoder.h
// アナログ風フィルターバンク・ボコーダー (スカラー設計)
//
//  - BPF Bank (2次SVFカスケード) / Subtractive LR4 (4次Linkwitz-Riley)
//  - 8〜48バンドの動的追従
//  - フォルマント・シフト/ストレッチによる合成側フィルタ周波数の制御
//  - 帯域交互ステレオ・パンニング
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include <cmath>

class FilterbankVocoder
{
public:
    static constexpr int kMaxBands = 48;
    static constexpr double kInternalSampleRate = 16000.0;

    FilterbankVocoder();
    ~FilterbankVocoder() = default;

    void prepare(double sampleRate);
    void reset();

    // 1サンプル処理
    // modulator: 分析側に入力する音声サンプル
    // carrierL/R: 合成側のステレオキャリア入力サンプル
    // outL/R: ボコーディング後のステレオ出力サンプル (書き戻し)
    void processSample(float modulator, float carrierL, float carrierR,
                       float& outL, float& outR,
                       int bandCount, float character,
                       float formantShift, float formantStretch,
                       int filterbankType, float stereoWidth,
                       const std::array<std::atomic<float>, kMaxBands>& bandGains,
                       std::array<std::atomic<float>, kMaxBands>& bandLevelsForUi) noexcept;

private:
    struct SVFState
    {
        float s1 = 0.0f;
        float s2 = 0.0f;
        void reset() { s1 = 0.0f; s2 = 0.0f; }
    };

    struct LR4State
    {
        SVFState lpf1, lpf2; // LPF 4次用の2段カスケード
        SVFState hpf1, hpf2; // HPF 4次用の2段カスケード
        void reset() { lpf1.reset(); lpf2.reset(); hpf1.reset(); hpf2.reset(); }
    };

    void computeFilterCoeffs(float fc, float Q, float& g, float& k, float& a1) noexcept
    {
        g = std::tan(3.14159265f * fc / (float)kInternalSampleRate);
        k = 1.0f / Q;
        a1 = 1.0f / (1.0f + g * (g + k));
    }

    double mSampleRate = 44100.0;

    // 分析側バンド周波数
    std::array<float, kMaxBands> mBandF0 {};

    // フィルタ状態変数
    std::array<std::array<SVFState, 2>, kMaxBands> mAnalSvf {}; // 2段カスケード用
    std::array<LR4State, kMaxBands> mAnalLr4 {};

    std::array<std::array<SVFState, 2>, kMaxBands> mSynthSvfL {};
    std::array<std::array<SVFState, 2>, kMaxBands> mSynthSvfR {};
    std::array<LR4State, kMaxBands> mSynthLr4L {};
    std::array<LR4State, kMaxBands> mSynthLr4R {};

    // 包絡線追従
    std::array<float, kMaxBands> mEnvValues {};
};
