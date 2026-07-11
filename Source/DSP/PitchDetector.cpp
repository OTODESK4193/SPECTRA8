#include "PitchDetector.h"
#include <cmath>
#include <algorithm>

namespace DSP {

PitchDetector::PitchDetector()
    : mWindowSize(512),
      mMaxDelay(320),
      mMinDelay(20),
      mIsVoiced(false),
      mPeriodicityScore(1.0f)
{
    mDifference.resize(mMaxDelay, 0.0f);
    mNormalizedDifference.resize(mMaxDelay, 0.0f);
}

void PitchDetector::reset()
{
    std::fill(mDifference.begin(), mDifference.end(), 0.0f);
    std::fill(mNormalizedDifference.begin(), mNormalizedDifference.end(), 0.0f);
    mIsVoiced = false;
    mPeriodicityScore = 1.0f;
}

float PitchDetector::detectPitch(const float* signal, int signalLength, float sampleRate)
{
    // 必要サンプル数が足りない場合は0を返す
    if (signalLength < mWindowSize + mMaxDelay)
    {
        mIsVoiced = false;
        mPeriodicityScore = 1.0f;
        return 0.0f;
    }

    difference(signal, signalLength);
    cumulativeMeanNormalizedDifference();
    
    // 絶対閾値を用いてラグ tau を探索
    int tau = absoluteThreshold(0.15f);
    
    if (tau != -1)
    {
        // 放物線補間でさらに精度を高める
        float betterTau = parabolicInterpolation(tau);
        mPeriodicityScore = mNormalizedDifference[tau];
        
        // 周期性スコアが一定以下なら有声とみなす
        mIsVoiced = (mPeriodicityScore < 0.25f);
        
        if (mIsVoiced && betterTau > 0.0f)
        {
            float freq = sampleRate / betterTau;
            // 声の可聴域 (50Hz - 800Hz) に収まっているか確認
            if (freq >= 40.0f && freq <= 1000.0f)
            {
                return freq;
            }
        }
    }
    
    mIsVoiced = false;
    mPeriodicityScore = 1.0f;
    return 0.0f;
}

void PitchDetector::difference(const float* signal, int signalLength)
{
    // YIN Step 1: 差分関数
    // d_t(tau) = sum_{j=1}^{W} (x_j - x_{j+tau})^2
    for (int tau = 0; tau < mMaxDelay; ++tau)
    {
        float diff = 0.0f;
        for (int j = 0; j < mWindowSize; ++j)
        {
            float val = signal[j] - signal[j + tau];
            diff += val * val;
        }
        mDifference[tau] = diff;
    }
}

void PitchDetector::cumulativeMeanNormalizedDifference()
{
    // YIN Step 2: 累積平均正規化差分関数
    // d'_t(0) = 1
    // d'_t(tau) = d_t(tau) / ((1/tau) * sum_{j=1}^{tau} d_t(j))
    mNormalizedDifference[0] = 1.0f;
    float runningSum = 0.0f;
    
    for (int tau = 1; tau < mMaxDelay; ++tau)
    {
        runningSum += mDifference[tau];
        if (runningSum > 0.0f)
        {
            mNormalizedDifference[tau] = mDifference[tau] / (runningSum / static_cast<float>(tau));
        }
        else
        {
            mNormalizedDifference[tau] = 1.0f;
        }
    }
}

int PitchDetector::absoluteThreshold(float threshold)
{
    // YIN Step 3: 絶対閾値
    // 閾値を下回る最初の極小値を探す
    for (int tau = mMinDelay; tau < mMaxDelay - 1; ++tau)
    {
        if (mNormalizedDifference[tau] < threshold)
        {
            // 極小値であることを確認 (減少から増加に転じる点)
            if (mNormalizedDifference[tau] < mNormalizedDifference[tau - 1] &&
                mNormalizedDifference[tau] < mNormalizedDifference[tau + 1])
            {
                return tau;
            }
        }
    }

    // 閾値を下回る極小値が見つからなかった場合、全体の最小値（グローバル最小値）を採用
    int globalMinTau = -1;
    float minVal = 1e10f;
    for (int tau = mMinDelay; tau < mMaxDelay - 1; ++tau)
    {
        if (mNormalizedDifference[tau] < minVal &&
            mNormalizedDifference[tau] < mNormalizedDifference[tau - 1] &&
            mNormalizedDifference[tau] < mNormalizedDifference[tau + 1])
        {
            minVal = mNormalizedDifference[tau];
            globalMinTau = tau;
        }
    }
    
    return globalMinTau;
}

float PitchDetector::parabolicInterpolation(int tau)
{
    // YIN Step 4: 放物線補間
    // 極小値の周辺の3点から放物線近似を行い、真の頂点位置を算出する
    if (tau <= 0 || tau >= mMaxDelay - 1)
    {
        return static_cast<float>(tau);
    }
    
    float alpha = mNormalizedDifference[tau - 1];
    float beta = mNormalizedDifference[tau];
    float gamma = mNormalizedDifference[tau + 1];
    
    float denominator = 2.0f * (alpha - 2.0f * beta + gamma);
    if (std::abs(denominator) > 1e-5f)
    {
        float offset = (alpha - gamma) / denominator;
        return static_cast<float>(tau) + offset;
    }
    
    return static_cast<float>(tau);
}

} // namespace DSP
