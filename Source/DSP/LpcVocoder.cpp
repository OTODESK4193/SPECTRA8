// ==========================================
// File: LpcVocoder.cpp
// フェーズ2計画書v2 §4/§5 準拠
// ==========================================
#include "LpcVocoder.h"
#include <cmath>
#include <algorithm>

void LpcVocoder::prepare(double /*hostSampleRate*/)
{
    mAnalyzer.prepare();

    // ゲイン平滑化係数（コントロールブロック = 32smp @16kHz = 2ms）
    const double blockDur = (double)kCtrlBlock / kInternalSampleRate;
    mGAttCoef = (float)std::exp(-blockDur / 0.005);   // att 5ms
    mGRelCoef = (float)std::exp(-blockDur / 0.030);   // rel 30ms

    setWindowType(mWindowType);
    reset();
}

void LpcVocoder::reset()
{
    mLatticeL.reset();
    mLatticeR.reset();
    mRing.fill(0.0f);
    mKTarget.fill(0.0f);
    mKCur.fill(0.0f);
    mKInc.fill(0.0f);
    mGTarget = mGSmooth = mGCur = mGInc = 0.0f;
    mWritePos = 0;
    mFilled = 0;
    mHopCounter = 0;
    mCtrlCounter = 0;
}

void LpcVocoder::setWindowType(int type) noexcept
{
    mWindowType = std::min(2, std::max(0, type));
    mAnalyzer.setWindowType(mWindowType);
    const float e = mAnalyzer.getWindowEnergy(mWindowType);
    mExcNorm = (e > 1e-12f) ? 1.0f / std::sqrt(e) : 1.0f;
}

void LpcVocoder::processSample(float modulator, float carrierL, float carrierR,
                               float& outL, float& outR,
                               int order, bool freeze) noexcept
{
    order = std::min(LpcAnalyzer::kMaxOrder, std::max(1, order));

    // 次数変更時は状態をリセット（残留状態による不整合を防止）
    if (order != mCurOrder)
    {
        mCurOrder = order;
        mLatticeL.reset();
        mLatticeR.reset();
        mKTarget.fill(0.0f);
        mKCur.fill(0.0f);
        mKInc.fill(0.0f);
    }

    // 1. モジュレーターをリングバッファへ
    mRing[(size_t)mWritePos] = modulator;
    mWritePos = (mWritePos + 1) & (kRingSize - 1);
    if (mFilled < kRingSize)
        ++mFilled;

    // 2. フレーム毎の分析（ターゲット更新）
    if (++mHopCounter >= kHopSamples)
    {
        mHopCounter = 0;
        if (!freeze && mFilled >= LpcAnalyzer::kWindowSize)
        {
            const int start = (mWritePos - LpcAnalyzer::kWindowSize + kRingSize) & (kRingSize - 1);
            for (int n = 0; n < LpcAnalyzer::kWindowSize; ++n)
                mFrame[(size_t)n] = mRing[(size_t)((start + n) & (kRingSize - 1))];

            const float g = mAnalyzer.analyzeFrame(mFrame.data(), order, mKTarget.data());
            mGTarget = g * mExcNorm;   // 励起レベル正規化（per-sample残差RMS相当へ）
        }
    }

    // 3. コントロールブロック毎に補間ランプを再計算
    if (mCtrlCounter == 0)
    {
        // ゲインの非対称平滑化（att 5ms / rel 30ms）
        const float coef = (mGTarget > mGSmooth) ? mGAttCoef : mGRelCoef;
        mGSmooth = mGTarget + (mGSmooth - mGTarget) * coef;

        constexpr float inv = 1.0f / (float)kCtrlBlock;
        for (int p = 0; p < order; ++p)
            mKInc[(size_t)p] = (mKTarget[(size_t)p] - mKCur[(size_t)p]) * inv;
        mGInc = (mGSmooth - mGCur) * inv;
    }
    if (++mCtrlCounter >= kCtrlBlock)
        mCtrlCounter = 0;

    // 4. サンプル毎の線形ランプ（|k|<1 は補間途中でも保存 → 常時安定）
    for (int p = 0; p < order; ++p)
        mKCur[(size_t)p] += mKInc[(size_t)p];
    mGCur += mGInc;

    // 5. ラティス合成
    const float g = mGCur * kMakeupGain;
    outL = mLatticeL.processSample(g * carrierL, mKCur.data(), order);
    outR = mLatticeR.processSample(g * carrierR, mKCur.data(), order);
}
