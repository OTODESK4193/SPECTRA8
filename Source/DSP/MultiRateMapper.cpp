#include "MultiRateMapper.h"
#include <cmath>
#include <algorithm>

namespace DSP {

MultiRateMapper::MultiRateMapper()
    : mNativeSampleRate(44100.0),
      mRatio(16000.0 / 44100.0),
      mSourceSamplePosition(0.0)
{
    setup(mNativeSampleRate);
}

void MultiRateMapper::setup(double nativeSampleRate)
{
    mNativeSampleRate = nativeSampleRate;
    mRatio = 16000.0 / mNativeSampleRate;
    mSourceSamplePosition = 0.0;

    // アンチエイリアス用ローパスフィルタの初期化 (7500Hzカットオフ)
    // 16kHzサンプリングレートのナイキスト周波数は8000Hzなので、7500Hzあたりで遮断する
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(mNativeSampleRate, 7500.0f);
    mAntiAliasFilter = std::make_unique<juce::dsp::IIR::Filter<float>>(coeffs);
    mAntiAliasFilter->reset();
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
    // 目標出力サンプル数の見積もり
    int maxOutputSamples = static_cast<int>(std::ceil(static_cast<double>(numSamples) * mRatio)) + 2;
    downsampledSamples.reserve(maxOutputSamples);

    while (mSourceSamplePosition < static_cast<double>(numSamples))
    {
        int index1 = static_cast<int>(mSourceSamplePosition);
        int index2 = std::min(index1 + 1, numSamples - 1);
        float frac = static_cast<float>(mSourceSamplePosition - static_cast<double>(index1));

        float sample = (1.0f - frac) * mFilterBuffer[index1] + frac * mFilterBuffer[index2];
        downsampledSamples.push_back(sample);

        mSourceSamplePosition += (1.0 / mRatio); // 次の16kHzサンプルに相当するソース位置
    }

    // 次のブロックのためにインデックスをオフセット
    mSourceSamplePosition -= static_cast<double>(numSamples);
    if (mSourceSamplePosition < 0.0)
    {
        mSourceSamplePosition = 0.0;
    }
}

void MultiRateMapper::mapLSF(const std::vector<float>& lsp16k, int order16k, std::vector<float>& lspFs, int targetOrder)
{
    // lspFs は targetOrder (24次) のサイズにする
    lspFs.resize(targetOrder);

    // 1. 16kHz LSP を角度 (LSF) に変換し、ホストサンプリングレート Fs の角度空間に射影
    std::vector<float> mappedLsfs;
    mappedLsfs.reserve(targetOrder);

    float scaleRatio = 16000.0f / static_cast<float>(mNativeSampleRate);

    for (int i = 0; i < order16k; ++i)
    {
        // 降順 LSP根 を 昇順 LSF角度 に変換
        float x = lsp16k[i];
        float omega16k = std::acos(x);
        
        // Fs空間にリマッピング: w_Fs = w_16k * (16k / Fs)
        float omegaFs = omega16k * scaleRatio;
        mappedLsfs.push_back(omegaFs);
    }

    // 2. 8000Hz から Fs/2 (ナイキスト) までの高域帯域にダミー根を合成
    int numDummyRoots = targetOrder - order16k;
    if (numDummyRoots > 0)
    {
        // 8000Hz 以上のホストSRでの角度
        float omegaStart = 3.14159265f * scaleRatio; // 8kHz/Nyquist_16k = pi * 16k/Fs
        float omegaEnd = 3.14159265f; // ナイキスト (Fs/2)
        
        for (int k = 0; k < numDummyRoots; ++k)
        {
            float frac = static_cast<float>(k + 1) / static_cast<float>(numDummyRoots + 1);
            float dummyOmega = omegaStart + frac * (omegaEnd - omegaStart);
            mappedLsfs.push_back(dummyOmega);
        }
    }

    // 3. 全てのLSF角度をソートして交互配置を保証
    std::sort(mappedLsfs.begin(), mappedLsfs.end());

    // LSFガードバンド適用 (隣り合う極の近接防止)
    float minDistance = 0.05f * 3.14159265f / static_cast<float>(targetOrder + 1);
    
    // 前進パス
    mappedLsfs[0] = std::max(mappedLsfs[0], minDistance);
    for (int i = 1; i < targetOrder; ++i)
    {
        mappedLsfs[i] = std::max(mappedLsfs[i], mappedLsfs[i - 1] + minDistance);
    }

    // 後退パス
    mappedLsfs[targetOrder - 1] = std::min(mappedLsfs[targetOrder - 1], 3.14159265f - minDistance);
    for (int i = targetOrder - 2; i >= 0; --i)
    {
        mappedLsfs[i] = std::min(mappedLsfs[i], mappedLsfs[i + 1] - minDistance);
    }

    // 4. 余弦ドメイン (LSP) に逆変換 (角度昇順なので、cosは降順になる)
    for (int i = 0; i < targetOrder; ++i)
    {
        lspFs[i] = std::cos(mappedLsfs[i]);
    }
    
    // 安全のため降順ソートを確認
    std::sort(lspFs.begin(), lspFs.end(), std::greater<float>());
}

} // namespace DSP
