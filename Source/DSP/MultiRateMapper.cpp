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
    mNativeSampleRate = std::max(8000.0, nativeSampleRate);
    mRatio = 16000.0 / mNativeSampleRate;
    mSourceSamplePosition = 0.0;

    // アンチエイリアス用ローパスフィルタの初期化 (7500Hzカットオフ)
    // 16kHzサンプリングレートのナイキスト周波数は8000Hzなので、7500Hzあたりで遮断する
    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass(mNativeSampleRate, 7500.0f);
    mAntiAliasFilter = std::make_unique<juce::dsp::IIR::Filter<float>>(coeffs);
    
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = mNativeSampleRate;
    spec.maximumBlockSize = 2048;
    spec.numChannels = 1;
    mAntiAliasFilter->prepare(spec);
    
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

    int M_src = order16k / 2;
    int M_tgt = targetOrder / 2;
    int numDummyM = M_tgt - M_src;

    float scaleRatio = 16000.0f / static_cast<float>(mNativeSampleRate);

    std::vector<float> lsfsP;
    std::vector<float> lsfsQ;
    lsfsP.reserve(M_tgt);
    lsfsQ.reserve(M_tgt);

    // 1. 元のLSPをPとQに分離し、角度ドメインに変換してFs空間にスケール
    for (int i = 0; i < M_src; ++i)
    {
        float xP = lsp16k[2 * i];
        float xQ = lsp16k[2 * i + 1];

        lsfsP.push_back(std::acos(std::clamp(xP, -0.999f, 0.999f)) * scaleRatio);
        lsfsQ.push_back(std::acos(std::clamp(xQ, -0.999f, 0.999f)) * scaleRatio);
    }

    // 2. 高域ダミー極を追加 (PとQが交互になるようにずらして配置)
    if (numDummyM > 0)
    {
        float omegaStart = 3.14159265f * scaleRatio;
        float omegaEnd = 3.14159265f;
        float step = (omegaEnd - omegaStart) / static_cast<float>(numDummyM);

        for (int k = 0; k < numDummyM; ++k)
        {
            // Pのダミーは区間の前半、Qのダミーは後半に配置して交互性を保つ
            float pOmega = omegaStart + (static_cast<float>(k) + 0.25f) * step;
            float qOmega = omegaStart + (static_cast<float>(k) + 0.75f) * step;

            lsfsP.push_back(pOmega);
            lsfsQ.push_back(qOmega);
        }
    }

    // 3. それぞれソート
    std::sort(lsfsP.begin(), lsfsP.end());
    std::sort(lsfsQ.begin(), lsfsQ.end());

    // 4. 交互にマージして単一の配列にする
    std::vector<float> mergedLsfs(targetOrder);
    for (int i = 0; i < M_tgt; ++i)
    {
        mergedLsfs[2 * i] = lsfsP[i];
        mergedLsfs[2 * i + 1] = lsfsQ[i];
    }

    // 5. ガードバンドの適用 (PとQの交互関係を崩さないように全体を微調整)
    float minDistance = 0.25f * 3.14159265f / static_cast<float>(targetOrder + 1);
    
    // Pが常にQよりわずかに小さいため、全体ソートをしても交互性は維持されます
    std::sort(mergedLsfs.begin(), mergedLsfs.end());

    // 前進パス
    mergedLsfs[0] = std::max(mergedLsfs[0], minDistance);
    for (int i = 1; i < targetOrder; ++i)
    {
        mergedLsfs[i] = std::max(mergedLsfs[i], mergedLsfs[i - 1] + minDistance);
    }

    // 後退パス
    mergedLsfs[targetOrder - 1] = std::min(mergedLsfs[targetOrder - 1], 3.14159265f - minDistance);
    for (int i = targetOrder - 2; i >= 0; --i)
    {
        mergedLsfs[i] = std::min(mergedLsfs[i], mergedLsfs[i + 1] - minDistance);
    }

    // 6. 余弦ドメイン (LSP) に逆変換 (角度昇順なので、cosは降順になる)
    for (int i = 0; i < targetOrder; ++i)
    {
        lspFs[i] = std::cos(mergedLsfs[i]);
    }
}

} // namespace DSP
