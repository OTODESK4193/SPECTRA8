// ==========================================
// File: AnalyzerDSP.cpp
// ==========================================
#include "AnalyzerDSP.h"

AnalyzerDSP::AnalyzerDSP()
    : juce::Thread("SPECTRA8 Analyzer")
{
    mBands.resize((size_t)kNumBands);
    mPeaks = std::make_unique<std::atomic<float>[]>((size_t)kNumBands);
    for (int i = 0; i < kNumBands; ++i)
        mPeaks[(size_t)i].store(-120.0f, std::memory_order_relaxed);

    mRing.resize((size_t)kBufferSize, 0.0f);
    mLocal.reserve((size_t)kBufferSize);

    startThread();
}

AnalyzerDSP::~AnalyzerDSP()
{
    signalThreadShouldExit();
    notify();
    stopThread(2000);
}

void AnalyzerDSP::prepare(double newSampleRate)
{
    mSampleRate = (newSampleRate > 1000.0) ? newSampleRate : 44100.0;

    // ミッドレートを約11-12kHzに揃える
    if (mSampleRate < 60000.0)       mMidDecim = 4;
    else if (mSampleRate < 120000.0) mMidDecim = 8;
    else                             mMidDecim = 16;
    mMidSampleRate = mSampleRate / (double)mMidDecim;

    // ミッドバンド上限2kHzに対し fc=2.6kHz。
    // ミッド帯へ折り返す成分(>= midSR-2kHz ≈ 9kHz)を約86dB以上抑制し、
    // 2kHzでの通過帯域の落ち込みは-0.07dB未満に収まる。
    mAaMid.design(2600.0, mSampleRate);
    mAaMid.reset();
    mMidCounter = 0;

    // バンド配置: kFreqMin〜kFreqMax を対数等間隔。
    //  Q=24 固定 (本家の中高域と同じ)。定Qなので低域ほど時間窓が長くなるが、
    //  エンベロープの時定数も周期に比例させているため脈打たない。
    const double fNyq = mSampleRate * 0.45;
    mHighBandStart = kNumBands;

    for (int i = 0; i < kNumBands; ++i)
    {
        double fc = (double)bandFreq(i);
        fc = std::min(fc, fNyq);

        const bool isMidRate = (fc < kMidBandMaxFreq);
        if (!isMidRate && mHighBandStart == kNumBands)
            mHighBandStart = i;

        const double rate = isMidRate ? mMidSampleRate : mSampleRate;
        // ミッドレート側でもナイキストを超えないよう保護
        fc = std::min(fc, rate * 0.45);

        mBands[(size_t)i].updateCoeffs(fc, 24.0, rate);
        mBands[(size_t)i].reset();
    }

    mWritePos.store(0, std::memory_order_relaxed);
    mReadPos = 0;
    std::fill(mRing.begin(), mRing.end(), 0.0f);
    mLocal.clear();
}

void AnalyzerDSP::pushAudio(const float* data, int numSamples) noexcept
{
    if (data == nullptr || numSamples <= 0)
        return;

    const int wp = mWritePos.load(std::memory_order_relaxed);
    for (int i = 0; i < numSamples; ++i)
        mRing[(size_t)((wp + i) & kBufferMask)] = data[i];
    mWritePos.store(wp + numSamples, std::memory_order_release);
}

void AnalyzerDSP::run()
{
    while (!threadShouldExit())
    {
        wait(5);   // 5ms間隔。滑らかな追従と負荷のバランス点

        if (threadShouldExit())
            break;

        const int currentWrite = mWritePos.load(std::memory_order_acquire);
        int available = currentWrite - mReadPos;

        if (available > kBufferSize)
        {
            mReadPos = currentWrite - kBufferSize;   // オーバーフロー時は最新側へ飛ばす
            available = kBufferSize;
        }

        if (available > 0)
        {
            mLocal.resize((size_t)available);
            for (int i = 0; i < available; ++i)
                mLocal[(size_t)i] = mRing[(size_t)((mReadPos + i) & kBufferMask)];
            mReadPos = currentWrite;

            processInternal(mLocal.data(), available);
        }
        else
        {
            // 無音区間でも包絡を減衰させ、残留表示(特に低域)を自然にフェードさせる。
            // 完全に落ちきったら更新を止めて無駄な負荷をなくす。
            double maxEnv = 0.0;
            for (const auto& b : mBands)
                maxEnv = std::max(maxEnv, b.env);

            if (maxEnv > 1.0e-5)   // ≈-100dB。表示フロア以下は見えない
            {
                const int n = std::max(1, (int)(mSampleRate * 0.005));
                mLocal.assign((size_t)n, 0.0f);
                processInternal(mLocal.data(), n);
            }
        }
    }
}

void AnalyzerDSP::processInternal(const float* data, int numSamples)
{
    auto runBand = [](Band& b, double in) noexcept
    {
        const double absVal = std::abs(b.process(in));
        // 立ち上がりは速く・戻りは緩やかに (ピークが見やすい)
        const double c = (absVal > b.env) ? b.attackCoef : b.releaseCoef;
        b.env = c * b.env + (1.0 - c) * absVal;
    };

    for (int i = 0; i < numSamples; ++i)
    {
        const double x = (double)data[i];

        // 高域 (>=2kHz): フルレート
        for (int b = mHighBandStart; b < kNumBands; ++b)
            runBand(mBands[(size_t)b], x);

        // アンチエイリアス → 1/mMidDecim デシメーション → 低中域
        const double xm = mAaMid.process(x);
        if (++mMidCounter >= mMidDecim)
        {
            mMidCounter = 0;
            for (int b = 0; b < mHighBandStart; ++b)
                runBand(mBands[(size_t)b], xm);
        }
    }

    // 0dBFSサイン入力が0dB表示になるようキャリブレーション。
    // 整流平均とアタック/リリース特性による系統誤差ぶんを補正する。
    constexpr double kCalibrationDb = 1.0;

    for (int b = 0; b < kNumBands; ++b)
    {
        const double e = std::max(1.0e-9, mBands[(size_t)b].env);
        mPeaks[(size_t)b].store((float)(20.0 * std::log10(e) + kCalibrationDb),
                                std::memory_order_relaxed);
    }
}
