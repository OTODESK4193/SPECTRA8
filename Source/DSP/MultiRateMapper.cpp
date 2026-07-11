#include "MultiRateMapper.h"
#include <cmath>
#include <algorithm>

namespace DSP {

MultiRateMapper::MultiRateMapper()
    : mNativeSampleRate(44100.0),
      mRatio(16000.0 / 44100.0),
      mSourceSamplePosition(0.0),
      mDestSamplePosition(0.0)
{
    setup(mNativeSampleRate);
}

void MultiRateMapper::setup(double nativeSampleRate)
{
    mNativeSampleRate = std::max(8000.0, nativeSampleRate);
    mRatio = 16000.0 / mNativeSampleRate;
    mSourceSamplePosition = 0.0;
    mDestSamplePosition = 0.0;

    // 1. ダウンサンプル用アンチエイリアスフィルタ (7500Hz遮断, ホストSRでプロセス)
    auto coeffsDown = juce::dsp::IIR::Coefficients<float>::makeLowPass(mNativeSampleRate, 7500.0f);
    mAntiAliasFilter = std::make_unique<juce::dsp::IIR::Filter<float>>(coeffsDown);
    
    juce::dsp::ProcessSpec specDown;
    specDown.sampleRate = mNativeSampleRate;
    specDown.maximumBlockSize = 2048;
    specDown.numChannels = 1;
    mAntiAliasFilter->prepare(specDown);
    mAntiAliasFilter->reset();

    // 2. アップサンプル用アンチイメージングフィルタ (7500Hz遮断, ホストSRでプロセス)
    auto coeffsUp = juce::dsp::IIR::Coefficients<float>::makeLowPass(mNativeSampleRate, 7500.0f);
    mAntiImageFilter = std::make_unique<juce::dsp::IIR::Filter<float>>(coeffsUp);

    juce::dsp::ProcessSpec specUp;
    specUp.sampleRate = mNativeSampleRate;
    specUp.maximumBlockSize = 2048;
    specUp.numChannels = 1;
    mAntiImageFilter->prepare(specUp);
    mAntiImageFilter->reset();
}

void MultiRateMapper::downsample(const float* inputSamples, int numSamples, std::vector<float>& downsampledSamples)
{
    downsampledSamples.clear();
    if (numSamples <= 0)
    {
        return;
    }

    // 1. ローパスフィルタ処理 (アンチエイリアシング)
    mFilterBuffer.resize(numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        mFilterBuffer[i] = mAntiAliasFilter->processSample(inputSamples[i]);
    }

    // 2. 線形補間による 16kHz へのリサンプリング
    int maxOutputSamples = static_cast<int>(std::ceil(static_cast<double>(numSamples) * mRatio)) + 2;
    downsampledSamples.reserve(maxOutputSamples);

    while (mSourceSamplePosition < static_cast<double>(numSamples))
    {
        int index1 = static_cast<int>(mSourceSamplePosition);
        int index2 = std::min(index1 + 1, numSamples - 1);
        float frac = static_cast<float>(mSourceSamplePosition - static_cast<double>(index1));

        float sample = (1.0f - frac) * mFilterBuffer[index1] + frac * mFilterBuffer[index2];
        downsampledSamples.push_back(sample);

        mSourceSamplePosition += (1.0 / mRatio); // 次の16kHzサンプルに対応する位置
    }

    // 次のブロックのためにインデックスをオフセット
    mSourceSamplePosition -= static_cast<double>(numSamples);
    if (mSourceSamplePosition < 0.0)
    {
        mSourceSamplePosition = 0.0;
    }
}

void MultiRateMapper::upsample(const std::vector<float>& input16k, int numOutputSamples, float* outputFs)
{
    if (numOutputSamples <= 0 || input16k.empty())
    {
        return;
    }

    int inputSize = static_cast<int>(input16k.size());

    // 1. 線形補間によりホストSRへアップサンプリング
    for (int i = 0; i < numOutputSamples; ++i)
    {
        double srcPos = mDestSamplePosition;
        int index1 = static_cast<int>(srcPos);
        int index2 = std::min(index1 + 1, inputSize - 1);
        float frac = static_cast<float>(srcPos - static_cast<double>(index1));

        float sample = 0.0f;
        if (index1 < inputSize)
        {
            sample = (1.0f - frac) * input16k[index1] + frac * input16k[index2];
        }

        // 2. アンチイメージング用ローパスフィルタを適用
        outputFs[i] = mAntiImageFilter->processSample(sample);

        mDestSamplePosition += mRatio; // ホスト1サンプルあたりの16kサンプル進行度 (mRatio)
    }

    // 次のブロックのためにオフセット
    mDestSamplePosition -= static_cast<double>(inputSize);
    if (mDestSamplePosition < 0.0)
    {
        mDestSamplePosition = 0.0;
    }
}

} // namespace DSP
