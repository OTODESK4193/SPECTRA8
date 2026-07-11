#include "TrueEnvelope.h"
#include <cmath>
#include <algorithm>

namespace DSP {

TrueEnvelope::TrueEnvelope()
    : mFftOrder(10),
      mFftSize(1024)
{
    setup(mFftOrder);
}

void TrueEnvelope::setup(int fftOrder)
{
    mFftOrder = fftOrder;
    mFftSize = 1 << mFftOrder;
    mFft = std::make_unique<juce::dsp::FFT>(mFftOrder);

    mLogSpectrum.resize(mFftSize, 0.0f);
    mLogEnvelope.resize(mFftSize, 0.0f);
    mTempLogSpectrum.resize(mFftSize, 0.0f);
    mCepstrum.resize(mFftSize, 0.0f);
    mPrevCepstrum.resize(mFftSize, 0.0f);
    mLifterWindow.resize(mFftSize, 0.0f);
    
    // juce::dsp::FFT の実数変換は 2 * N サイズのバッファを必要とするため、十分な量を確保
    mFftBuffer.resize(mFftSize * 2, 0.0f);
}

void TrueEnvelope::estimate(const float* windowedFrame, int windowSize, float f0, std::vector<float>& envelope, float sampleRate)
{
    // デフォルトピッチを設定 (無声判定や異常値の場合)
    if (f0 < 50.0f || f0 > 800.0f)
    {
        f0 = 150.0f;
    }

    // 1. FFT実行のためのバッファ準備
    std::fill(mFftBuffer.begin(), mFftBuffer.end(), 0.0f);
    std::copy(windowedFrame, windowedFrame + std::min(mFftSize, windowSize), mFftBuffer.begin());

    // 前方FFT（実数のみ）を実行
    mFft->performRealOnlyForwardTransform(mFftBuffer.data());

    // 複素スペクトルから対数振幅スペクトル A_0 を計算
    // DC成分 と ナイキスト成分は実数。それ以外は実部・虚部が交互に並ぶ。
    mLogSpectrum[0] = std::log(std::max(std::abs(mFftBuffer[0]), 1e-10f));
    mLogSpectrum[mFftSize / 2] = std::log(std::max(std::abs(mFftBuffer[1]), 1e-10f));
    
    for (int k = 1; k < mFftSize / 2; ++k)
    {
        float re = mFftBuffer[2 * k];
        float im = mFftBuffer[2 * k + 1];
        float mag = std::sqrt(re * re + im * im);
        float logMag = std::log(std::max(mag, 1e-10f));
        mLogSpectrum[k] = logMag;
        mLogSpectrum[mFftSize - k] = logMag; // 対称スペクトル
    }

    // 遮断クエフレンシー Pc の計算 (Pc = alpha * Fs / (2 * F0))
    float Pc = sampleRate / (2.0f * f0);
    // クランプして範囲内に抑える
    Pc = std::clamp(Pc, 5.0f, static_cast<float>(mFftSize / 4));
    int pcInt = static_cast<int>(std::round(Pc));

    // リフタ窓の構築 (Low-pass lifter)
    std::fill(mLifterWindow.begin(), mLifterWindow.end(), 0.0f);
    for (int r = 0; r < mFftSize; ++r)
    {
        int dist = (r <= mFftSize / 2) ? r : mFftSize - r;
        if (dist < pcInt)
        {
            mLifterWindow[r] = 1.0f;
        }
        else if (dist == pcInt)
        {
            mLifterWindow[r] = 0.5f;
        }
        else
        {
            mLifterWindow[r] = 0.0f;
        }
    }

    // 初期の包絡をスペクトル自体で初期化
    mLogEnvelope = mLogSpectrum;
    std::fill(mPrevCepstrum.begin(), mPrevCepstrum.end(), 0.0f);

    const int maxIterations = 8;
    const float threshold = 2.0f / 8.68588f; // 2.0 dB を Neper (自然対数) スケールに変換

    for (int iter = 0; iter < maxIterations; ++iter)
    {
        // A_i(k) = max(A_0(k), V_{i-1}(k))
        for (int k = 0; k < mFftSize; ++k)
        {
            mTempLogSpectrum[k] = std::max(mLogSpectrum[k], mLogEnvelope[k]);
        }

        // IFFTのために対数スペクトルを配置
        std::fill(mFftBuffer.begin(), mFftBuffer.end(), 0.0f);
        mFftBuffer[0] = mTempLogSpectrum[0];
        mFftBuffer[1] = mTempLogSpectrum[mFftSize / 2];
        for (int k = 1; k < mFftSize / 2; ++k)
        {
            mFftBuffer[2 * k] = mTempLogSpectrum[k]; // 実部
            mFftBuffer[2 * k + 1] = 0.0f;            // 虚部（対称対数スペクトルなので0）
        }

        // 逆FFT（IFFT）を実行してリアルケプストラムを得る
        mFft->performRealOnlyInverseTransform(mFftBuffer.data());

        // ケプストラムの取得 (逆FFTのスケーリング 1/N を適用)
        float invN = 1.0f / static_cast<float>(mFftSize);
        for (int r = 0; r < mFftSize; ++r)
        {
            mCepstrum[r] = mFftBuffer[r] * invN;
        }

        // ステップ幅加速化 (Step-size Acceleration)
        if (iter > 0)
        {
            float E_in = 0.0f;
            float E_out = 0.0f;
            for (int r = 0; r < mFftSize; ++r)
            {
                float val = mCepstrum[r] * mCepstrum[r];
                if (mLifterWindow[r] > 0.0f)
                {
                    E_in += val;
                }
                else
                {
                    E_out += val;
                }
            }
            
            float alpha_acc = 1.0f;
            if (E_in > 1e-10f)
            {
                alpha_acc = std::sqrt((E_in + E_out) / E_in);
            }
            alpha_acc = std::clamp(alpha_acc, 1.0f, 3.0f); // 発散を防ぐためにクランプ

            for (int r = 0; r < mFftSize; ++r)
            {
                mCepstrum[r] = alpha_acc * (mCepstrum[r] - mPrevCepstrum[r]) + mPrevCepstrum[r];
            }
        }

        mPrevCepstrum = mCepstrum;

        // リフタリングの適用
        for (int r = 0; r < mFftSize; ++r)
        {
            mCepstrum[r] *= mLifterWindow[r];
        }

        // 再びFFTを行い、対数包絡スペクトルを得る
        std::fill(mFftBuffer.begin(), mFftBuffer.end(), 0.0f);
        std::copy(mCepstrum.begin(), mCepstrum.end(), mFftBuffer.begin());

        mFft->performRealOnlyForwardTransform(mFftBuffer.data());

        // 新しい対数振幅包絡 V_i(k)
        mLogEnvelope[0] = mFftBuffer[0];
        mLogEnvelope[mFftSize / 2] = mFftBuffer[1];
        for (int k = 1; k < mFftSize / 2; ++k)
        {
            float re = mFftBuffer[2 * k];
            float im = mFftBuffer[2 * k + 1];
            mLogEnvelope[k] = std::sqrt(re * re + im * im);
            mLogEnvelope[mFftSize - k] = mLogEnvelope[k];
        }

        // 収束判定
        float maxDiff = 0.0f;
        for (int k = 0; k < mFftSize; ++k)
        {
            maxDiff = std::max(maxDiff, mLogSpectrum[k] - mLogEnvelope[k]);
        }

        if (maxDiff < threshold)
        {
            break;
        }
    }

    // 包絡スペクトルをリニア振幅スケールに復元
    envelope.resize(mFftSize / 2 + 1);
    for (int k = 0; k <= mFftSize / 2; ++k)
    {
        envelope[k] = std::exp(mLogEnvelope[k]);
    }
}

} // namespace DSP
