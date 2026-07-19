// ==========================================
// File: AnalyzerDSP.h
// 高精度スペクトラムアナライザー (HighPrecisionEQ より移植)
//
//  定Qバンドパス・フィルタバンク + エンベロープフォロワー方式。
//  FFTと違い時間分解能と周波数分解能を帯域ごとに最適化できるため、
//  低域が滑らかで、かつ高域の追従が速い表示が得られる。
//
//  【SPECTRA8向けの再設計】
//   本家は 1Hz〜25kHz を800バンドで見るEQ用途だが、SPECTRA8のBANDS EQ表示は
//   80〜7500Hz(melバンド)なので、40Hz〜16kHz を 320バンドに絞ってCPUを節約する。
//   本家の肝である「マルチレート処理」はそのまま踏襲:
//     - 2kHz以上のバンド : フルレート
//     - 2kHz未満のバンド : 1/4 デシメーション (8次Butterworthのアンチエイリアス付き)
//   単純間引きだと折り返しが低域に偽スペクトルとして乗るため、AAフィルタは必須。
//
//  処理はバックグラウンドスレッドで行い、オーディオスレッドは
//  リングバッファへの書き込みのみ (ロックフリー・アロケーションなし)。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <algorithm>

class AnalyzerDSP : public juce::Thread
{
public:
    static constexpr int kNumBands = 320;
    static constexpr float kFreqMin = 40.0f;
    static constexpr float kFreqMax = 16000.0f;

    AnalyzerDSP();
    ~AnalyzerDSP() override;

    void prepare(double sampleRate);

    // オーディオスレッドから呼ぶ (ロックフリー)
    void pushAudio(const float* data, int numSamples) noexcept;

    // GUIスレッドから呼ぶ。band番号 → dB値
    float getBandDb(int band) const noexcept
    {
        return mPeaks[(size_t)juce::jlimit(0, kNumBands - 1, band)].load(std::memory_order_relaxed);
    }
    // band番号 → 中心周波数
    static float bandFreq(int band) noexcept
    {
        const float t = (float)juce::jlimit(0, kNumBands - 1, band) / (float)(kNumBands - 1);
        return kFreqMin * std::pow(kFreqMax / kFreqMin, t);
    }
    // 周波数 → 連続band位置 (描画時の補間用)
    static float freqToBandPos(float hz) noexcept
    {
        const float t = std::log(juce::jlimit(kFreqMin, kFreqMax, hz) / kFreqMin)
                      / std::log(kFreqMax / kFreqMin);
        return t * (float)(kNumBands - 1);
    }
    // 補間付きでdBを取得 (滑らかな曲線描画用)
    float getDbAtFreq(float hz) const noexcept
    {
        const float pos = freqToBandPos(hz);
        const int i0 = juce::jlimit(0, kNumBands - 2, (int)pos);
        const float fr = pos - (float)i0;
        return getBandDb(i0) * (1.0f - fr) + getBandDb(i0 + 1) * fr;
    }

    void run() override;

private:
    void processInternal(const float* data, int numSamples);

    // TPT SVF バンドパス + 動的時定数エンベロープフォロワー
    struct Band
    {
        double g = 0.0, h = 0.0, R = 0.0;
        double ic1eq = 0.0, ic2eq = 0.0;
        double env = 0.0;
        double attackCoef = 0.0, releaseCoef = 0.0;

        void updateCoeffs(double fc, double Q, double sr) noexcept
        {
            const double wd = 2.0 * juce::MathConstants<double>::pi * fc;
            const double T = 1.0 / sr;
            const double wa = (2.0 / T) * std::tan(wd * T / 2.0);
            g = wa * T / 2.0;
            R = 1.0 / (2.0 * Q);
            h = 1.0 / (1.0 + 2.0 * R * g + g * g);

            // 動的時定数: 低域ほど長く取る (1周期に満たない窓では包絡が脈打つため)
            const double period = 1.0 / std::max(1.0, fc);
            const double attackTime  = std::max(0.010, period * 0.2);
            const double releaseTime = std::max(0.150, period * 2.0);
            attackCoef  = std::exp(-1.0 / (attackTime  * sr));
            releaseCoef = std::exp(-1.0 / (releaseTime * sr));
        }

        void reset() noexcept { ic1eq = ic2eq = env = 0.0; }

        inline double process(double x) noexcept
        {
            double hp = (x - (2.0 * R + g) * ic1eq - ic2eq) * h;
            double bp = hp * g + ic1eq;

            if (!std::isfinite(bp)) { bp = 0.0; ic1eq = 0.0; ic2eq = 0.0; }

            const double lp = bp * g + ic2eq;
            ic1eq = 2.0 * bp - ic1eq;
            ic2eq = 2.0 * lp - ic2eq;
            return bp * 2.0 * R;
        }
    };

    // デシメーション前段のアンチエイリアス (8次Butterworth = TPT SVF LPF ×4段)。
    // 単純間引きだと折り返しが -10〜-25dB しか落ちず、中域成分が低域に偽スペクトルとして乗る。
    struct AntiAliasFilter
    {
        struct Section { double g = 0.0, h = 0.0, R = 0.0, ic1 = 0.0, ic2 = 0.0; };
        std::array<Section, 4> sections;

        void design(double fc, double sr) noexcept
        {
            static constexpr double kQs[4] = { 0.5097956, 0.6013449, 0.8999762, 2.5629154 };
            const double g0 = std::tan(juce::MathConstants<double>::pi * fc / sr);
            for (int s = 0; s < 4; ++s)
            {
                auto& sec = sections[(size_t)s];
                sec.g = g0;
                sec.R = 1.0 / (2.0 * kQs[s]);
                sec.h = 1.0 / (1.0 + 2.0 * sec.R * sec.g + sec.g * sec.g);
                sec.ic1 = sec.ic2 = 0.0;
            }
        }
        void reset() noexcept { for (auto& s : sections) { s.ic1 = 0.0; s.ic2 = 0.0; } }

        inline double process(double x) noexcept
        {
            for (auto& sec : sections)
            {
                const double hp = (x - (2.0 * sec.R + sec.g) * sec.ic1 - sec.ic2) * sec.h;
                const double bp = sec.g * hp + sec.ic1;
                const double lp = sec.g * bp + sec.ic2;
                sec.ic1 = 2.0 * bp - sec.ic1;
                sec.ic2 = 2.0 * lp - sec.ic2;
                x = lp;
            }
            return x;
        }
    };

    std::vector<Band> mBands;
    std::unique_ptr<std::atomic<float>[]> mPeaks;

    static constexpr int kBufferSize = 32768;   // 約0.68秒@48k。表示用途には十分
    static constexpr int kBufferMask = kBufferSize - 1;
    std::vector<float> mRing;
    std::vector<float> mLocal;
    std::atomic<int> mWritePos { 0 };
    int mReadPos = 0;

    double mSampleRate = 44100.0;

    // マルチレート
    static constexpr double kMidBandMaxFreq = 2000.0;
    int    mMidDecim = 4;
    double mMidSampleRate = 44100.0 / 4.0;
    int    mMidCounter = 0;
    int    mHighBandStart = 0;      // このindex以降がフルレート処理

    AntiAliasFilter mAaMid;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AnalyzerDSP)
};
