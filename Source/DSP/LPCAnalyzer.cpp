#include "LPCAnalyzer.h"
#include <cmath>
#include <algorithm>

namespace DSP {

LPCAnalyzer::LPCAnalyzer()
    : mMaxOrder(24),
      mPrevSample(0.0f)
{
    setup(mMaxOrder);
}

void LPCAnalyzer::setup(int maxOrder)
{
    mMaxOrder = maxOrder;
    mAutocorr.resize(mMaxOrder + 1, 0.0f);
}

void LPCAnalyzer::resetPreEmphasis()
{
    mPrevSample = 0.0f;
}

void LPCAnalyzer::applyPreEmphasis(const float* input, float* output, int size, float coeff)
{
    for (int i = 0; i < size; ++i)
    {
        float current = input[i];
        output[i] = current - coeff * mPrevSample;
        mPrevSample = current;
    }
}

void LPCAnalyzer::applyWindow(const float* input, float* output, int size)
{
    // 必要に応じてハミング窓バッファの再作成
    if (mWindow.size() != static_cast<size_t>(size))
    {
        mWindow.resize(size);
        float angleArg = 2.0f * 3.141592653589793f / static_cast<float>(size - 1);
        for (int i = 0; i < size; ++i)
        {
            mWindow[i] = 0.54f - 0.46f * std::cos(static_cast<float>(i) * angleArg);
        }
    }

    for (int i = 0; i < size; ++i)
    {
        output[i] = input[i] * mWindow[i];
    }
}

void LPCAnalyzer::autocorrelation(const float* signal, int size, std::vector<float>& r, int order)
{
    r.assign(order + 1, 0.0f);
    for (int k = 0; k <= order; ++k)
    {
        float sum = 0.0f;
        for (int n = 0; n < size - k; ++n)
        {
            sum += signal[n] * signal[n + k];
        }
        r[k] = sum;
    }
}

bool LPCAnalyzer::levinsonDurbin(const std::vector<float>& r, std::vector<float>& a, float& E, int order)
{
    a.assign(order + 1, 0.0f);
    a[0] = 1.0f; // A(z)のz^0の係数は1.0

    E = r[0];
    if (E <= 1e-10f)
    {
        // エネルギーが極めて低い場合はゼロ（無音）
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
        
        // 反射係数 lambda の絶対値が 1.0 以上にならないように制限
        if (std::abs(lambda) >= 1.0f)
        {
            lambda = (lambda > 0.0f) ? 0.999f : -0.999f;
        }

        a[i] = lambda;
        for (int j = 1; j < i; ++j)
        {
            a[j] = a_prev[j] - lambda * a_prev[i - j];
        }

        E *= (1.0f - lambda * lambda);
        if (E <= 1e-10f)
        {
            E = 1e-10f;
        }

        a_prev = a;
    }

    return true;
}

bool LPCAnalyzer::analyze(const float* frame, int frameSize, std::vector<float>& coeffs, float& gain, int order)
{
    order = std::clamp(order, 1, mMaxOrder);

    // バッファ確保
    mPreEmphasizedFrame.resize(frameSize);
    mWindowedFrame.resize(frameSize);

    // 1. プリエンファシス
    applyPreEmphasis(frame, mPreEmphasizedFrame.data(), frameSize);

    // 2. ハミング窓適用
    applyWindow(mPreEmphasizedFrame.data(), mWindowedFrame.data(), frameSize);

    // 3. 自己相関計算
    autocorrelation(mWindowedFrame.data(), frameSize, mAutocorr, order);

    // 4. Levinson-Durbin
    float E = 0.0f;
    std::vector<float> a;
    bool success = levinsonDurbin(mAutocorr, a, E, order);

    if (success)
    {
        coeffs = a;
        gain = std::sqrt(E);
    }
    else
    {
        coeffs.assign(order + 1, 0.0f);
        coeffs[0] = 1.0f;
        gain = 0.0f;
    }

    return success;
}

} // namespace DSP
