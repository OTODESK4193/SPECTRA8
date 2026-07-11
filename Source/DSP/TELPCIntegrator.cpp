#include "TELPCIntegrator.h"
#include <cmath>
#include <algorithm>

namespace DSP {

TELPCIntegrator::TELPCIntegrator()
    : mFftOrder(10),
      mFftSize(1024),
      mNumBins(513)
{
    setup(mFftOrder);
}

void TELPCIntegrator::setup(int fftOrder)
{
    mFftOrder = fftOrder;
    mFftSize = 1 << mFftOrder;
    mNumBins = mFftSize / 2 + 1;

    mFft = std::make_unique<juce::dsp::FFT>(mFftOrder);
    mBarkFilter.setup(mFftSize, 16000.0f); // 16kHzで動作

    mPowerSpectrum.resize(mNumBins, 0.0f);
    mBarkLowerBound.resize(mNumBins, 0.0f);
    mCombinedSpectrum.resize(mNumBins, 0.0f);
    mAutocorrBuffer.resize(mFftSize * 2, 0.0f);
}

bool TELPCIntegrator::integrate(const std::vector<float>& teEnvelope,
                                 const std::vector<float>& barkEnergies,
                                 float gamma,
                                 int order,
                                 std::vector<float>& lpcCoeffs,
                                 float& gain)
{
    // パラメータ範囲制限
    gamma = std::clamp(gamma, 0.0f, 1.0f);
    order = std::clamp(order, 1, 24);

    // 1. True Envelopeの振幅エンベロープをパワースケールに変換
    for (int k = 0; k < mNumBins; ++k)
    {
        mPowerSpectrum[k] = teEnvelope[k] * teEnvelope[k];
    }

    // 2. Barkフィルタバンクによる下限パワースペクトルの再構成
    mBarkFilter.reconstructLowerBound(barkEnergies, mBarkLowerBound);

    // 3. パワー領域で結合 (低域の過小評価・谷の埋め合わせ)
    for (int k = 0; k < mNumBins; ++k)
    {
        mCombinedSpectrum[k] = gamma * mPowerSpectrum[k] + (1.0f - gamma) * mBarkLowerBound[k];
    }

    // 4. 実数逆FFTを実行して自己相関 R(tau) を算出する
    std::fill(mAutocorrBuffer.begin(), mAutocorrBuffer.end(), 0.0f);
    
    // IFFTバッファへの配置 (DC、ナイキスト、および中間ビンの実部)
    mAutocorrBuffer[0] = mCombinedSpectrum[0];
    mAutocorrBuffer[1] = mCombinedSpectrum[mNumBins - 1]; 
    for (int k = 1; k < mNumBins - 1; ++k)
    {
        mAutocorrBuffer[2 * k] = mCombinedSpectrum[k]; 
        mAutocorrBuffer[2 * k + 1] = 0.0f; // 虚部は対称性よりゼロ
    }

    mFft->performRealOnlyInverseTransform(mAutocorrBuffer.data());

    // 逆FFTのスケーリング適用 (1/N)
    float invN = 1.0f / static_cast<float>(mFftSize);
    std::vector<float> r(order + 1, 0.0f);
    for (int tau = 0; tau <= order; ++tau)
    {
        r[tau] = mAutocorrBuffer[tau] * invN;
    }

    // 5. 自己相関 r から Levinson-Durbin再帰法でLPC係数とゲインを算出
    lpcCoeffs.assign(order + 1, 0.0f);
    lpcCoeffs[0] = 1.0f; // a_0 = 1.0

    float E = r[0];
    if (E <= 1e-10f)
    {
        gain = 0.0f;
        return false;
    }

    std::vector<float> a_prev(order + 1, 0.0f);
    a_prev[0] = 1.0f;

    for (int i = 1; i <= order; ++i)
    {
        float sum = 0.0f;
        for (int j = 1; j < i; ++j)
        {
            sum += a_prev[j] * r[i - j];
        }
        
        float lambda = (r[i] - sum) / E;
        
        // 反射係数をクランプして安定性を維持
        if (std::abs(lambda) >= 1.0f)
        {
            lambda = (lambda > 0.0f) ? 0.999f : -0.999f;
        }

        lpcCoeffs[i] = lambda;
        for (int j = 1; j < i; ++j)
        {
            lpcCoeffs[j] = a_prev[j] - lambda * a_prev[i - j];
        }

        E *= (1.0f - lambda * lambda);
        if (E <= 1e-10f)
        {
            E = 1e-10f;
        }

        a_prev = lpcCoeffs;
    }

    gain = std::sqrt(E);
    return true;
}

} // namespace DSP
