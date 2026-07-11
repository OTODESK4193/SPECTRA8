#include "BarkFilterBank.h"
#include <cmath>
#include <algorithm>

namespace DSP {

BarkFilterBank::BarkFilterBank()
    : mFftSize(1024),
      mNumBins(513),
      mSampleRate(16000.0f),
      mNumBands(24)
{
    setup(mFftSize, mSampleRate);
}

void BarkFilterBank::setup(int fftSize, float sampleRate)
{
    mFftSize = fftSize;
    mNumBins = fftSize / 2 + 1;
    mSampleRate = sampleRate;
    
    calculateFilterWeights();
}

float BarkFilterBank::hzToBark(float hz)
{
    // 近似式: B(f) = 6.0 * asinh(f / 600.0)
    return 6.0f * std::asinh(hz / 600.0f);
}

float BarkFilterBank::barkToHz(float bark)
{
    // 逆変換: f = 600.0 * sinh(b / 6.0)
    return 600.0f * std::sinh(bark / 6.0f);
}

void BarkFilterBank::calculateFilterWeights()
{
    mFilterWeights.assign(mNumBands, std::vector<float>(mNumBins, 0.0f));

    float maxFreq = mSampleRate / 2.0f;
    float maxBark = hzToBark(maxFreq);
    
    // 各バンドの中心Bark値の計算
    std::vector<float> centerBarks(mNumBands);
    for (int i = 0; i < mNumBands; ++i)
    {
        centerBarks[i] = static_cast<float>(i) * (maxBark / static_cast<float>(mNumBands - 1));
    }

    float binWidthHz = mSampleRate / static_cast<float>(mFftSize);

    for (int band = 0; band < mNumBands; ++band)
    {
        float c = centerBarks[band];
        float l = (band > 0) ? centerBarks[band - 1] : -c; // 左隣のバンド中心
        float r = (band < mNumBands - 1) ? centerBarks[band + 1] : maxBark + (maxBark - c); // 右隣のバンド中心

        for (int bin = 0; bin < mNumBins; ++bin)
        {
            float freq = static_cast<float>(bin) * binWidthHz;
            float b = hzToBark(freq);

            if (b >= l && b <= r)
            {
                float weight = 0.0f;
                if (b <= c)
                {
                    weight = (b - l) / (c - l);
                }
                else
                {
                    weight = (r - b) / (r - c);
                }
                mFilterWeights[band][bin] = weight;
            }
            else
            {
                mFilterWeights[band][bin] = 0.0f;
            }
        }
    }
}

void BarkFilterBank::process(const float* powerSpectrum, std::vector<float>& bandEnergies)
{
    bandEnergies.assign(mNumBands, 0.0f);

    for (int band = 0; band < mNumBands; ++band)
    {
        float sum = 0.0f;
        float norm = 0.0f;
        
        for (int bin = 0; bin < mNumBins; ++bin)
        {
            float w = mFilterWeights[band][bin];
            sum += powerSpectrum[bin] * w;
            norm += w;
        }
        
        if (norm > 0.0f)
        {
            bandEnergies[band] = sum / norm;
        }
        else
        {
            bandEnergies[band] = 0.0f;
        }
    }
}

void BarkFilterBank::reconstructLowerBound(const std::vector<float>& bandEnergies, std::vector<float>& lowerBoundSpectrum)
{
    lowerBoundSpectrum.assign(mNumBins, 0.0f);

    std::vector<float> weightSums(mNumBins, 0.0f);

    for (int band = 0; band < mNumBands; ++band)
    {
        float energy = bandEnergies[band];
        for (int bin = 0; bin < mNumBins; ++bin)
        {
            float w = mFilterWeights[band][bin];
            lowerBoundSpectrum[bin] += energy * w;
            weightSums[bin] += w;
        }
    }

    for (int bin = 0; bin < mNumBins; ++bin)
    {
        if (weightSums[bin] > 0.0f)
        {
            lowerBoundSpectrum[bin] /= weightSums[bin];
        }
        // 微小な値でフロアリング
        lowerBoundSpectrum[bin] = std::max(lowerBoundSpectrum[bin], 1e-10f);
    }
}

} // namespace DSP
