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

    // 16kHzで得られた LSP 根を、ホストサンプリングレート Fs の 24次 LSP 空間に再射影し、
    // 高域にダミー根を追加して安定したロールオフを形成する (LSFルートワーピング手法A)。
    // lsp16k: 16kHz分析で得られた LSP 根 (サイズ order16k、降順)
    // lspFs: ホストSR用の 24次 LSP 根 (サイズ 24、降順)
    void mapLSF(const std::vector<float>& lsp16k, int order16k, std::vector<float>& lspFs, int targetOrder = 24);

private:
    double mNativeSampleRate;
    double mRatio; // 16000.0 / mNativeSampleRate

    // ローパスフィルタ (アンチエイリアシング用、ホストSRでの 7500Hz 遮断)
    std::unique_ptr<juce::dsp::IIR::Filter<float>> mAntiAliasFilter;

    double mSourceSamplePosition;
    std::vector<float> mFilterBuffer;
};

} // namespace DSP
