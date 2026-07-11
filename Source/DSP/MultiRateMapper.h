#pragma once
#include <vector>
#include <memory>
#include <juce_dsp/juce_dsp.h>

namespace DSP {

class MultiRateMapper {
public:
    MultiRateMapper();
    ~MultiRateMapper() = default;

    // 初期化
    // nativeSampleRate: DAWホストのサンプリングレート (例: 44100〜192000)
    void setup(double nativeSampleRate);

    // 入力バッファ（ホストサンプリングレート）を 16kHz にダウンサンプリングする。
    // 入力: inputSamples (ホストSR)
    // 出力: downsampledSamples (16kHz)
    void downsample(const float* inputSamples, int numSamples, std::vector<float>& downsampledSamples);

    // 16kHzで合成された信号をホストサンプリングレートにアップサンプリングする。
    // 入力: input16k (16kHz)
    // 出力: outputFs (ホストSR、サイズ numOutputSamples に直接書き込む)
    void upsample(const std::vector<float>& input16k, int numOutputSamples, float* outputFs);

private:
    double mNativeSampleRate;
    double mRatio; // 16000.0 / mNativeSampleRate

    // ローパスフィルタ (7500Hz遮断)
    std::unique_ptr<juce::dsp::IIR::Filter<float>> mAntiAliasFilter; // ダウンサンプル用
    std::unique_ptr<juce::dsp::IIR::Filter<float>> mAntiImageFilter; // アップサンプル用

    double mSourceSamplePosition;
    double mDestSamplePosition;
    std::vector<float> mFilterBuffer;
};

} // namespace DSP
