// ==========================================
// File: FxChain.h
// 4スロット直列FXチェーン (Granular FxPanel と同じくGUIからD&Dで並べ替え)
//
// FX 5種 — EDMのColor Bassサウンドを主眼に選定:
//  - Spectral Resonator : 8本のチューンド・コムフィルタ。
//                         CHORD = ROOT+コード種でピッチ配置 (音が和音で鳴る)
//                         FREE  = ディレイ時間をms直指定 (超ショートDelay+高FB)
//                         ※この2つは同じDSP。1/f秒のディレイ+高FB = チューンドコム。
//  - Multiband Drive    : 3バンド分割 → 各帯域に歪み → 再合成
//  - Formant Gate       : 16ステップのリズムゲート + ステップ毎に母音フォルマント
//  - Chorus             : 4ボイス・アンサンブル (幅出し)
//  - Reverb             : FDN風の簡易リバーブ
//
// 設計方針: 全FXが「ホストレート・ステレオ・サンプル単位」で動作する。
// ボコーダー本体(16kHz内部処理)とは独立した後段なので、帯域制限を受けない。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include <cmath>
#include <cstdint>   // uint16_t (パターンマスク)。JuceHeader経由の間接includeに頼らない

// ------------------------------------------
// 共通: 1次/2次フィルタ部品
// ------------------------------------------
namespace fxutil
{
    // 状態変数フィルタ (LP/BP/HP同時出力)
    struct Svf
    {
        float ic1 = 0.0f, ic2 = 0.0f;
        void reset() noexcept { ic1 = ic2 = 0.0f; }

        // fcNorm = fc / sampleRate (0..0.49), Q >= 0.3
        void process(float in, float fcNorm, float Q, float& lp, float& bp, float& hp) noexcept
        {
            const float g = std::tan(juce::MathConstants<float>::pi
                                   * juce::jlimit(0.0005f, 0.49f, fcNorm));
            const float k = 1.0f / juce::jmax(0.3f, Q);
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float a2 = g * a1;
            const float a3 = g * a2;

            const float v3 = in - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;

            lp = v2;
            bp = v1;
            hp = in - k * v1 - v2;
        }
    };

    // 1次ローパス (ダンピング用)
    struct OnePole
    {
        float z = 0.0f;
        void reset() noexcept { z = 0.0f; }
        float lp(float in, float coeff) noexcept   // coeff: 0(素通り)..0.99(強く鈍る)
        {
            z += (1.0f - coeff) * (in - z);
            return z;
        }
    };

    // 線形補間つきディレイライン
    struct DelayLine
    {
        std::vector<float> buf;
        int writePos = 0;

        void prepare(int maxSamples)
        {
            buf.assign((size_t)juce::jmax(4, maxSamples), 0.0f);
            writePos = 0;
        }
        void reset() noexcept { std::fill(buf.begin(), buf.end(), 0.0f); writePos = 0; }

        void write(float v) noexcept
        {
            buf[(size_t)writePos] = v;
            if (++writePos >= (int)buf.size()) writePos = 0;
        }
        // delaySamples は 1.0 以上・バッファ長未満
        float read(float delaySamples) const noexcept
        {
            const int n = (int)buf.size();
            float d = juce::jlimit(1.0f, (float)(n - 2), delaySamples);
            float rp = (float)writePos - d;
            while (rp < 0.0f) rp += (float)n;

            const int i0 = (int)rp;
            const int i1 = (i0 + 1 >= n) ? 0 : i0 + 1;
            const float fr = rp - (float)i0;
            return buf[(size_t)i0] * (1.0f - fr) + buf[(size_t)i1] * fr;
        }
    };

    // 1st-Order Thiran Allpass Interpolated Delay Line for Zero High-Frequency Loss
    struct ThiranDelayLine
    {
        std::vector<float> buf;
        int writePos = 0;
        float apX1 = 0.0f;
        float apY1 = 0.0f;

        void prepare(int maxSamples)
        {
            buf.assign((size_t)juce::jmax(16, maxSamples), 0.0f);
            reset();
        }
        void reset() noexcept
        {
            std::fill(buf.begin(), buf.end(), 0.0f);
            writePos = 0;
            apX1 = 0.0f;
            apY1 = 0.0f;
        }
        void write(float v) noexcept
        {
            buf[(size_t)writePos] = v;
            if (++writePos >= (int)buf.size()) writePos = 0;
        }
        float readThiran(float delaySamples) noexcept
        {
            const int n = (int)buf.size();
            float d = juce::jlimit(2.0f, (float)(n - 3), delaySamples);

            int intD = (int)std::floor(d);
            float delta = d - (float)intD;
            if (delta < 0.5f) {
                intD -= 1;
                delta += 1.0f;
            }
            float eta = (1.0f - delta) / (1.0f + delta);

            int rp = writePos - intD;
            while (rp < 0) rp += n;
            while (rp >= n) rp -= n;

            float x = buf[(size_t)rp];
            float y = eta * x + apX1 - eta * apY1;
            apX1 = x;
            apY1 = y;
            return y;
        }
    };

    inline float softClip(float x) noexcept
    {
        // tanh近似 (発散防止の最終段として全FX出力に噛ませる)
        if (x < -3.0f) return -1.0f;
        if (x >  3.0f) return  1.0f;
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }
}

// ==========================================
// 1. Spectral Resonator (MIDI Mode - ColorBass Edition ported from Colors)
//    8本のチューンド・コムフィルタ。MIDI鍵盤の押鍵周波数に完全追従。
//    1次Thiranオールパス補間により高域ロールオフゼロ・完全フラット特性。
// ==========================================
class SpectralResonator
{
public:
    static constexpr int kNumVoices = 8;

    void prepare(double sr)
    {
        sampleRate = sr;
        const int maxLen = (int)(sr / 20.0) + 32;
        for (auto& v : voicesL) v.dl.prepare(maxLen);
        for (auto& v : voicesR) v.dl.prepare(maxLen);
        reset();
    }

    void reset()
    {
        for (auto& v : voicesL) { v.dl.reset(); v.damp.reset(); v.diffuser.reset(); }
        for (auto& v : voicesR) { v.dl.reset(); v.damp.reset(); v.diffuser.reset(); }
    }

    void setParams(float decaySec, float damp01, float shimmer01, float inharm01, float spread01,
                   float outGainDb, float shiftSt, const float* midiHz, int numMidi) noexcept
    {
        // Logarithmic decay range: 0.0005s (0.5ms snap/click) to 3.0s (lush sustain)
        mDecaySec = juce::jlimit(0.0005f, 3.0f, decaySec);
        mDamp = juce::jlimit(0.01f, 0.95f, damp01 * 0.92f);
        mShimmer = juce::jlimit(0.0f, 1.0f, shimmer01);
        mInharm = juce::jlimit(0.0f, 1.0f, inharm01);
        mSpread = juce::jlimit(0.0f, 1.0f, spread01);
        mOutGainLinear = juce::Decibels::decibelsToGain(juce::jlimit(-24.0f, 12.0f, outGainDb));
        mShiftSt = juce::jlimit(0.0f, 24.0f, shiftSt);

        if (numMidi <= 0) return;

        const float shiftRatio = std::pow(2.0f, mShiftSt / 12.0f);

        for (int i = 0; i < kNumVoices; ++i)
        {
            int n = juce::jlimit(1, kNumVoices, numMidi);
            float baseF = midiHz[i % n] * shiftRatio;
            int oct = i / n;
            float f = juce::jlimit(20.0f, 10000.0f, baseF * (float)(1 << oct));
            float delaySamples = (float)sampleRate / f;

            if (mInharm > 0.001f)
            {
                float B = mInharm * 0.005f;
                float harmN = (float)(i + 1);
                delaySamples /= std::sqrt(1.0f + B * harmN * harmN);
            }

            float det = mSpread * 0.02f * ((i % 2 == 0) ? 1.0f : -1.0f);
            voicesL[(size_t)i].targetDelay = delaySamples * (1.0f + det);
            voicesR[(size_t)i].targetDelay = delaySamples * (1.0f - det);

            for (auto* vv : { &voicesL[(size_t)i], &voicesR[(size_t)i] })
            {
                float D = juce::jmax(2.0f, vv->targetDelay);
                // Allow fbGain to drop down to 0.0f for ultra-short decays (0.5ms transient generation)
                vv->fbGain = std::pow(10.0f, -3.0f * D / (mDecaySec * (float)sampleRate));
                vv->fbGain = juce::jlimit(0.0f, 0.995f, vv->fbGain);
            }
        }
    }

    void process(float inL, float inR, float& outL, float& outR) noexcept
    {
        float sumL = 0.0f, sumR = 0.0f;

        for (int i = 0; i < kNumVoices; ++i)
        {
            auto& vL = voicesL[(size_t)i];
            auto& vR = voicesR[(size_t)i];

            vL.delaySamples += (vL.targetDelay - vL.delaySamples) * 0.02f;
            vR.delaySamples += (vR.targetDelay - vR.delaySamples) * 0.02f;

            // Thiran Allpass interpolation maintains 100% flat high-frequency response
            float curL = vL.dl.readThiran(vL.delaySamples);
            float curR = vR.dl.readThiran(vR.delaySamples);

            // Shimmer High-Frequency Allpass Diffusion
            if (mShimmer > 0.001f)
            {
                float shimG = 0.45f * mShimmer;
                curL = vL.diffuser.process(curL, shimG);
                curR = vR.diffuser.process(curR, shimG);
            }

            float dL = vL.damp.lp(curL, mDamp);
            float dR = vR.damp.lp(curR, mDamp);

            // Energy-preserving unity scaling: sqrt(1 - g^2) prevents resonance blowout while maintaining strong punch
            float inGainL = std::sqrt(1.0f - juce::jmin(0.96f, vL.fbGain * vL.fbGain));
            float inGainR = std::sqrt(1.0f - juce::jmin(0.96f, vR.fbGain * vR.fbGain));

            vL.dl.write(inL * inGainL + fxutil::softClip(dL * vL.fbGain));
            vR.dl.write(inR * inGainR + fxutil::softClip(dR * vR.fbGain));

            float pan = ((float)i / (float)(kNumVoices - 1)) * 2.0f - 1.0f;
            pan *= mSpread;
            float gL = 0.5f * (1.0f - pan);
            float gR = 0.5f * (1.0f + pan);

            sumL += curL * gL;
            sumR += curR * gR;
        }

        // Robust output scale (approx 0.62 at unity outGain) so Resonator matches input power
        float scale = (1.75f / std::sqrt((float)kNumVoices)) * mOutGainLinear;
        outL = fxutil::softClip(sumL * scale);
        outR = fxutil::softClip(sumR * scale);
    }

    // In-place processing overload
    void process(float& l, float& r) noexcept
    {
        float outL = 0.0f, outR = 0.0f;
        process(l, r, outL, outR);
        l = outL;
        r = outR;
    }

private:
    struct Voice
    {
        fxutil::ThiranDelayLine dl;
        fxutil::OnePole damp;
        struct SimpleDiffuser {
            float z1 = 0.0f;
            inline float process(float in, float g) noexcept {
                float w = in - g * z1;
                float out = z1 + g * w;
                z1 = w;
                return out;
            }
            inline void reset() noexcept { z1 = 0.0f; }
        } diffuser;
        float targetDelay = 100.0f;
        float delaySamples = 100.0f;
        float fbGain = 0.9f;
    };

    double sampleRate = 44100.0;
    std::array<Voice, kNumVoices> voicesL, voicesR;
    float mDecaySec = 0.5f, mDamp = 0.3f, mShimmer = 0.0f, mInharm = 0.0f, mSpread = 0.8f;
    float mOutGainLinear = 1.0f;
    float mShiftSt = 0.0f;
};

// ==========================================
// 2. Multiband Drive
//   3バンド(Low/Mid/High)に分けて各帯域を個別に歪ませる。
//   全帯域を一括で歪ませると低域が濁って潰れるのを避けるのが目的。
// ==========================================
class MultibandDrive
{
public:
    enum Shape { Tanh = 0, Fold, Bitcrush, NumShapes };
    static juce::StringArray getShapeNames() { return { "Tanh", "Fold", "Crush" }; }

    void prepare(double sr) { sampleRate = sr; reset(); }
    void reset()
    {
        for (auto& f : splitL) f.reset();
        for (auto& f : splitR) f.reset();
    }

    void setParams(int shape, float drive, float loMix, float midMix, float hiMix) noexcept
    {
        mShape = juce::jlimit(0, (int)NumShapes - 1, shape);
        mDrive = juce::jlimit(1.0f, 40.0f, drive);
        mLo = juce::jlimit(0.0f, 1.0f, loMix);
        mMid = juce::jlimit(0.0f, 1.0f, midMix);
        mHi = juce::jlimit(0.0f, 1.0f, hiMix);
    }

    void process(float& l, float& r) noexcept
    {
        l = processOne(l, splitL);
        r = processOne(r, splitR);
    }

private:
    float shapeOne(float x) const noexcept
    {
        switch (mShape)
        {
        case Fold:
        {
            // 三角波状に折り返す (倍音が派手に増える)。
            // ※反復で折り返す実装はDriveが大きいと回数が足りず折り切れずに
            //   +18dBの暴走を起こす。周期4の三角波として一発で畳む。
            float y = x + 1.0f;
            y -= 4.0f * std::floor(y * 0.25f);      // [0,4) へ巻き取る
            y = (y < 2.0f) ? y : (4.0f - y);        // 0→2→0 の三角
            return y - 1.0f;                        // [-1,+1]
        }
        case Bitcrush:
        {
            const float levels = 8.0f;
            return std::round(juce::jlimit(-1.0f, 1.0f, x) * levels) / levels;
        }
        default:
            return fxutil::softClip(x);
        }
    }

    float processOne(float in, std::array<fxutil::Svf, 2>& f) noexcept
    {
        // 300Hz / 2500Hz の2点で3分割
        float lp1, bp1, hp1, lp2, bp2, hp2;
        f[0].process(in, 300.0f / (float)sampleRate, 0.707f, lp1, bp1, hp1);
        f[1].process(hp1, 2500.0f / (float)sampleRate, 0.707f, lp2, bp2, hp2);

        const float low = lp1, mid = lp2, high = hp2;

        // 帯域毎に歪ませてからミックス量で戻す。1/driveで音量を揃える。
        const float g = 1.0f / std::sqrt(mDrive);
        const float dLow  = shapeOne(low  * mDrive) * g;
        const float dMid  = shapeOne(mid  * mDrive) * g;
        const float dHigh = shapeOne(high * mDrive) * g;

        const float sum = low  * (1.0f - mLo)  + dLow  * mLo
                        + mid  * (1.0f - mMid) + dMid  * mMid
                        + high * (1.0f - mHi)  + dHigh * mHi;
        // 3帯域が同位相で重なった時の突出を抑える最終段
        return fxutil::softClip(sum);
    }

    double sampleRate = 44100.0;
    std::array<fxutil::Svf, 2> splitL, splitR;
    int mShape = 0;
    float mDrive = 4.0f, mLo = 0.5f, mMid = 1.0f, mHi = 0.7f;
};

// ==========================================
// 3. Formant Gate (Click-Free Hann S-Curve Vocal Rhythm Gate - 50 Patterns ported from Colors)
//    DAWタイムライン完全同期(PPQ)、C1滑らかなHann S-Curve包絡線、Legatoタイ結合対応
// ==========================================
class FormantGate
{
public:
    static constexpr int kNumSteps = 16;
    static constexpr int kTotalPatterns = 50;

    static juce::StringArray getRateNames()
    {
        return { "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    }

    static juce::StringArray getPatternNames()
    {
        return {
            // Category 1: Straight (10)
            "1/2 Beat", "1/4 Beat", "1/8 Beat", "1/16 Beat", "1/32 Beat",
            "Off-Beat 8th", "Off-Beat 16th", "Pulse 25%", "Pulse 75%", "March 4-on-floor",
            // Category 2: Triplet & Swing (8)
            "1/4 Triplet", "1/8 Triplet", "1/16 Triplet", "Shuffle Light", "Shuffle Heavy",
            "Triplet Burst", "Gallop", "Waltz",
            // Category 3: Dotted & Polyrhythm (8)
            "1/4 Dotted", "1/8 Dotted", "1/16 Dotted", "Syncopated Dotted",
            "Polyrhythm 3:4", "Polyrhythm 3:2", "Hemiola", "Tresillo 3-3-2",
            // Category 4: ColorBass & EDM (12)
            "Dubstep Wobble", "Riddim Chug", "Future Bass Pump", "Trance Gate Classic",
            "Sidechain Pump", "Psytrance 16th", "Trap Hihat Roll", "Half-Time Chop",
            "Drum'n'Bass Chop", "UK Garage Bounce", "Bounce Groove", "Stutter Drop",
            // Category 5: Glitch & Complex (12)
            "Euclid 3/8", "Euclid 5/8", "Euclid 5/16", "Euclid 7/16", "Euclid 9/16",
            "Binary Counter", "Fibonacci", "Accelerated Burst", "Decelerated Drop",
            "Micro Stutter", "Chaos Squelch", "Morse Code"
        };
    }

    void prepare(double sr)
    {
        sampleRate = sr;
        slewCoeff = (float)(1.0 - std::exp(-1.0 / (sampleRate * 0.0008)));
        reset();
    }

    void reset()
    {
        phase = 0.0;
        stepIdx = 0;
        prevStepIdx = 0;
        curEnv = 0.0f;
        for (auto& f : fmtL) f.reset();
        for (auto& f : fmtR) f.reset();
        curF1 = 700.0f; curF2 = 1100.0f; curF3 = 2400.0f;
    }

    void setParams(int rateIdx, int patternIdx, float depth01, float decay01, float vowel01) noexcept
    {
        mRate = juce::jlimit(0, 6, rateIdx);
        mPattern = juce::jlimit(0, kTotalPatterns - 1, patternIdx);
        mDepth = juce::jlimit(0.0f, 1.0f, depth01);
        mDecay = juce::jlimit(0.05f, 1.0f, decay01);
        mVowel = juce::jlimit(0.0f, 1.0f, vowel01);
    }

    void process(float& l, float& r, double bpm = 120.0, double ppqPosition = 0.0, bool isPlaying = false) noexcept
    {
        // Rate step length in musical beats (1 beat = 1 Quarter note)
        static const double kStepBeats[7] = {
            2.0,            // 1/2
            1.0,            // 1/4
            0.5,            // 1/8
            1.0 / 3.0,      // 1/8T
            0.25,           // 1/16
            1.0 / 6.0,      // 1/16T
            0.125           // 1/32
        };
        const double stepBeats = kStepBeats[juce::jlimit(0, 6, mRate)];
        const double safeBpm = (bpm > 20.0 && bpm < 400.0) ? bpm : 120.0;

        int curStep = 0;
        int nextStep = 0;
        float p = 0.0f;

        if (isPlaying && ppqPosition >= 0.0)
        {
            // Sample-accurate phase quantization to DAW song timeline
            double stepPos = ppqPosition / stepBeats;
            double intPart = std::floor(stepPos);
            p = (float)(stepPos - intPart);
            curStep = ((int)intPart % kNumSteps + kNumSteps) % kNumSteps;
            nextStep = (curStep + 1) % kNumSteps;
            stepIdx = curStep;
        }
        else
        {
            // Free-running mode using accurate project tempo (BPM)
            double stepHz = (safeBpm / 60.0) / stepBeats;
            phase += stepHz / sampleRate;
            while (phase >= 1.0)
            {
                phase -= 1.0;
                stepIdx = (stepIdx + 1) % kNumSteps;
            }
            curStep = stepIdx;
            nextStep = (stepIdx + 1) % kNumSteps;
            p = (float)phase;
        }

        const int prevStep = (curStep - 1 + kNumSteps) % kNumSteps;
        const bool prevOn = getPatternStep(mPattern, prevStep);
        const bool curOn = getPatternStep(mPattern, curStep);
        const bool nextOn = getPatternStep(mPattern, nextStep);

        // Step duration in seconds
        const double stepSec = (60.0 / safeBpm) * stepBeats;

        // Guaranteed minimum fade duration (>= 3.5ms) to eliminate audio clicks
        const float fadeRatio = (float)juce::jlimit(0.015, 0.35, 0.0035 / stepSec);

        // Decay controls gate open length:
        // When mDecay >= 0.95: Legato mode with tie preservation across adjacent active steps
        // When mDecay < 0.95: Staccato / rhythmic articulation with guaranteed silence landing
        const bool isLegato = (mDecay >= 0.95f);
        const float openRatio = isLegato ? 1.0f : juce::jmap(mDecay, 0.05f, 0.95f, 0.15f, 0.90f);
        const float gateLen = juce::jmax(fadeRatio * 2.0f, openRatio);
        const float releaseStart = gateLen - fadeRatio;

        float stepEnv = 0.0f;

        if (curOn)
        {
            const bool tiedFromPrev = prevOn && isLegato;
            const bool tiedToNext = nextOn && isLegato;

            if (p < fadeRatio)
            {
                if (tiedFromPrev)
                    stepEnv = 1.0f;
                else
                    stepEnv = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * (p / fadeRatio)));
            }
            else if (p >= releaseStart)
            {
                if (tiedToNext)
                    stepEnv = 1.0f;
                else if (p < gateLen)
                {
                    const float fallP = (p - releaseStart) / fadeRatio;
                    stepEnv = 0.5f * (1.0f + std::cos(juce::MathConstants<float>::pi * juce::jlimit(0.0f, 1.0f, fallP)));
                }
                else
                {
                    stepEnv = 0.0f;
                }
            }
            else
            {
                stepEnv = 1.0f;
            }
        }
        else
        {
            stepEnv = 0.0f;
        }

        // One-pole slew filter (~0.8ms time constant) to guarantee C1-smooth envelope
        curEnv += slewCoeff * (stepEnv - curEnv);
        const float gain = 1.0f - mDepth * (1.0f - curEnv);

        // Vowel Formant SVF Filters with Slew Smoothing
        if (mVowel > 0.001f)
        {
            static const float kVowels[5][3] = {
                {  730.0f, 1090.0f, 2440.0f }, // /a/
                {  270.0f, 2290.0f, 3010.0f }, // /i/
                {  300.0f,  870.0f, 2240.0f }, // /u/
                {  530.0f, 1840.0f, 2480.0f }, // /e/
                {  570.0f,  840.0f, 2410.0f }, // /o/
            };
            const int v = stepIdx % 5;
            const float targetF1 = kVowels[v][0];
            const float targetF2 = kVowels[v][1];
            const float targetF3 = kVowels[v][2];

            curF1 += (targetF1 - curF1) * 0.01f;
            curF2 += (targetF2 - curF2) * 0.01f;
            curF3 += (targetF3 - curF3) * 0.01f;

            float freqs[3] = { curF1, curF2, curF3 };
            float wetL = 0.0f, wetR = 0.0f;

            for (int j = 0; j < 3; ++j)
            {
                const float fc = juce::jlimit(20.0f, (float)sampleRate * 0.45f, freqs[j]) / (float)sampleRate;
                float lp, bp, hp;
                fmtL[(size_t)j].process(l, fc, 6.0f, lp, bp, hp);
                wetL += bp;
                fmtR[(size_t)j].process(r, fc, 6.0f, lp, bp, hp);
                wetR += bp;
            }

            l = l * (1.0f - mVowel) + wetL * 2.0f * mVowel;
            r = r * (1.0f - mVowel) + wetR * 2.0f * mVowel;
        }

        l = fxutil::softClip(l * gain);
        r = fxutil::softClip(r * gain);
    }

    static bool getPatternStep(int pattern, int step) noexcept
    {
        // 50 Rhythmic Patterns across 5 Categories
        static const uint16_t kPatternMasks[kTotalPatterns] = {
            // Category 1: Straight (10)
            0xAAAA, // 0: 1/2 Beat (alternating)
            0x8888, // 1: 1/4 Beat
            0xCCCC, // 2: 1/8 Beat
            0xFFFF, // 3: 1/16 Beat (Continuous pulse)
            0xEEEE, // 4: 1/32 Beat pulse
            0x5555, // 5: Off-Beat 8th
            0x2222, // 6: Off-Beat 16th
            0x1111, // 7: Pulse 25%
            0x7777, // 8: Pulse 75%
            0x9999, // 9: March 4-on-floor

            // Category 2: Triplet & Swing (8)
            0x9249, // 10: 1/4 Triplet
            0xB6DB, // 11: 1/8 Triplet
            0xEDDB, // 12: 1/16 Triplet
            0x8A8A, // 13: Shuffle Light
            0x8282, // 14: Shuffle Heavy
            0xEA20, // 15: Triplet Burst
            0x9292, // 16: Gallop
            0x4924, // 17: Waltz (3/4 time)

            // Category 3: Dotted & Polyrhythm (8)
            0x8421, // 18: 1/4 Dotted
            0x9249, // 19: 1/8 Dotted
            0x4924, // 20: 1/16 Dotted
            0x8892, // 21: Syncopated Dotted
            0x8A42, // 22: Polyrhythm 3:4
            0x94A5, // 23: Polyrhythm 3:2
            0x6969, // 24: Hemiola
            0x8924, // 25: Tresillo 3-3-2 (Classic Caribbean / Reggaeton)

            // Category 4: ColorBass & EDM (12)
            0xEA00, // 26: Dubstep Wobble
            0x00A0, // 27: Riddim Chug
            0x0888, // 28: Future Bass Pump
            0xA5A5, // 29: Trance Gate Classic
            0x000F, // 30: Sidechain Pump
            0x7777, // 31: Psytrance 16th
            0xFFF0, // 32: Trap Hihat Roll
            0x8080, // 33: Half-Time Chop
            0x9090, // 34: Drum'n'Bass Chop
            0x9A48, // 35: UK Garage Bounce
            0x69B2, // 36: Bounce Groove
            0xFE02, // 37: Stutter Drop

            // Category 5: Glitch & Complex (12)
            0x9248, // 38: Euclid 3/8
            0xAD5B, // 39: Euclid 5/8
            0x8892, // 40: Euclid 5/16
            0xAA92, // 41: Euclid 7/16
            0xADA5, // 42: Euclid 9/16
            0x5A5A, // 43: Binary Counter
            0x9421, // 44: Fibonacci
            0xF842, // 45: Accelerated Burst
            0x248F, // 46: Decelerated Drop
            0xF0F0, // 47: Micro Stutter
            0x6B3D, // 48: Chaos Squelch
            0xA8AE  // 49: Morse Code
        };

        const int pIdx = juce::jlimit(0, kTotalPatterns - 1, pattern);
        return ((kPatternMasks[pIdx] >> (step % 16)) & 1) != 0;
    }

private:
    double sampleRate = 44100.0;
    double phase = 0.0;
    int stepIdx = 0, prevStepIdx = 0;
    float curEnv = 0.0f;
    float slewCoeff = 0.025f;
    int mRate = 4, mPattern = 0;
    float mDepth = 0.8f, mVowel = 0.5f, mDecay = 0.5f;
    std::array<fxutil::Svf, 3> fmtL, fmtR;
    float curF1 = 700.0f, curF2 = 1100.0f, curF3 = 2400.0f;
};

// ==========================================
// 4. Chorus (4ボイス・アンサンブル)
// ==========================================
class EnsembleChorus
{
public:
    void prepare(double sr)
    {
        sampleRate = sr;
        dlL.prepare((int)(sr * 0.05) + 8);
        dlR.prepare((int)(sr * 0.05) + 8);
        reset();
    }
    void reset() { dlL.reset(); dlR.reset(); for (auto& p : lfoPhase) p = 0.0f; }

    void setParams(float rateHz, float depthMs, float width, float mix) noexcept
    {
        mRate = juce::jlimit(0.02f, 8.0f, rateHz);
        mDepth = juce::jlimit(0.1f, 12.0f, depthMs);
        mWidth = juce::jlimit(0.0f, 1.0f, width);
        mMix = juce::jlimit(0.0f, 1.0f, mix);
    }

    void process(float& l, float& r) noexcept
    {
        dlL.write(l);
        dlR.write(r);

        float wetL = 0.0f, wetR = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            // ボイス毎にレートを少しずらす (完全同期だとフランジャーになる)
            lfoPhase[(size_t)i] += mRate * (1.0f + 0.17f * (float)i) / (float)sampleRate;
            if (lfoPhase[(size_t)i] >= 1.0f) lfoPhase[(size_t)i] -= 1.0f;

            const float mod = std::sin(lfoPhase[(size_t)i] * juce::MathConstants<float>::twoPi);
            const float baseMs = 8.0f + 3.0f * (float)i;
            const float dMs = baseMs + mod * mDepth;
            const float dSamp = dMs * 0.001f * (float)sampleRate;

            // 偶数ボイスをL寄り、奇数をR寄りに振る
            const float panL = (i % 2 == 0) ? 1.0f : (1.0f - mWidth);
            const float panR = (i % 2 == 0) ? (1.0f - mWidth) : 1.0f;
            wetL += dlL.read(dSamp) * panL;
            wetR += dlR.read(dSamp) * panR;
        }
        wetL *= 0.35f;
        wetR *= 0.35f;

        l = l * (1.0f - mMix) + wetL * mMix;
        r = r * (1.0f - mMix) + wetR * mMix;
    }

private:
    double sampleRate = 44100.0;
    fxutil::DelayLine dlL, dlR;
    std::array<float, 4> lfoPhase {};
    float mRate = 0.6f, mDepth = 4.0f, mWidth = 0.7f, mMix = 0.5f;
};

// ==========================================
// 5. Reverb (4本コムディフューザ + 2段オールパス、L/R非対称)
//   SIZE / DAMP に加えて PREDELAY / WIDTH / LOW CUT / MOD を持つ。
//   MOD はコム長を微揺らしして、固定長コム特有の金属的な付帯音を散らす。
// ==========================================
class SimpleReverb
{
public:
    void prepare(double sr)
    {
        sampleRate = sr;
        preL.prepare((int)(sr * 0.25) + 8);   // プリディレイ最大200ms
        preR.prepare((int)(sr * 0.25) + 8);
        // 互いに素に近い長さ(ms)にして金属的な癖を避ける
        static const float combMs[4] = { 29.7f, 37.1f, 41.1f, 43.7f };
        static const float apMs[2]   = {  5.0f,  1.7f };
        for (int i = 0; i < 4; ++i)
        {
            combL[(size_t)i].prepare((int)(combMs[i] * 0.001f * sr) + 4);
            combR[(size_t)i].prepare((int)((combMs[i] + 1.3f) * 0.001f * sr) + 4);
            combLenL[(size_t)i] = combMs[i] * 0.001f * (float)sr;
            combLenR[(size_t)i] = (combMs[i] + 1.3f) * 0.001f * (float)sr;
        }
        for (int i = 0; i < 2; ++i)
        {
            apL[(size_t)i].prepare((int)(apMs[i] * 0.001f * sr) + 4);
            apR[(size_t)i].prepare((int)(apMs[i] * 0.001f * sr) + 4);
            apLen[(size_t)i] = apMs[i] * 0.001f * (float)sr;
        }
        reset();
    }
    void reset()
    {
        preL.reset(); preR.reset();
        for (auto& d : combL) d.reset();
        for (auto& d : combR) d.reset();
        for (auto& d : apL) d.reset();
        for (auto& d : apR) d.reset();
        for (auto& f : dampL) f.reset();
        for (auto& f : dampR) f.reset();
        hpL.reset(); hpR.reset();
        modPhase = 0.0f;
    }

    // size/damp/mix に加えて predelayMs / width / lowCutHz / modAmt
    void setParams(float size, float damp, float mix,
                   float predelayMs, float width, float lowCutHz, float modAmt) noexcept
    {
        mSize = juce::jlimit(0.0f, 1.0f, size);
        mDamp = juce::jlimit(0.0f, 0.95f, damp);
        mMix = juce::jlimit(0.0f, 1.0f, mix);
        mFeedback = 0.70f + mSize * 0.28f;   // 最大0.98 (1未満を厳守)

        mPredelay = juce::jlimit(0.0f, 200.0f, predelayMs) * 0.001f * (float)sampleRate;
        mWidth = juce::jlimit(0.0f, 1.0f, width);
        mLowCut = juce::jlimit(20.0f, 1000.0f, lowCutHz) / (float)sampleRate;
        mModAmt = juce::jlimit(0.0f, 1.0f, modAmt);
    }

    void process(float& l, float& r) noexcept
    {
        // --- プリディレイ (初期反射までの間合い) ---
        preL.write(l);
        preR.write(r);
        const float inL = (mPredelay > 1.0f) ? preL.read(mPredelay) : l;
        const float inR = (mPredelay > 1.0f) ? preR.read(mPredelay) : r;

        // --- コム長の微揺らし (固定長コムの金属的な癖を散らす) ---
        modPhase += 0.35f / (float)sampleRate;
        if (modPhase >= 1.0f) modPhase -= 1.0f;
        const float lfo = std::sin(modPhase * juce::MathConstants<float>::twoPi);

        float wetL = 0.0f, wetR = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            // ボイス毎に位相をずらした微小変調 (最大±0.3%)
            const float ph = lfo * std::cos((float)i * 1.1f);
            const float mL = combLenL[(size_t)i] * (1.0f + mModAmt * 0.003f * ph);
            const float mR = combRenR(i) * (1.0f - mModAmt * 0.003f * ph);

            const float dL = combL[(size_t)i].read(mL);
            const float dR = combR[(size_t)i].read(mR);
            // 帰還にソフトクリップを噛ませる (Resonator と同じ保険)。
            // fb<1 なので発散はしないが、SIZE 最大では定常利得が 1/(1-0.98)≒50倍(+34dB)
            // まで積み上がりうる。ここで頭打ちにしておく。
            combL[(size_t)i].write(fxutil::softClip(inL + dampL[(size_t)i].lp(dL, mDamp) * mFeedback));
            combR[(size_t)i].write(fxutil::softClip(inR + dampR[(size_t)i].lp(dR, mDamp) * mFeedback));
            wetL += dL;
            wetR += dR;
        }
        wetL *= 0.25f;
        wetR *= 0.25f;

        // オールパスで拡散
        for (int i = 0; i < 2; ++i)
        {
            wetL = allpass(apL[(size_t)i], apLen[(size_t)i], wetL);
            wetR = allpass(apR[(size_t)i], apLen[(size_t)i], wetR);
        }

        // --- ローカット (残響で低域が濁るのを防ぐ。1次HPF = 原音 - LPF) ---
        wetL -= hpL.lp(wetL, 1.0f - juce::jlimit(0.0f, 0.9f, mLowCut * 6.2831853f));
        wetR -= hpR.lp(wetR, 1.0f - juce::jlimit(0.0f, 0.9f, mLowCut * 6.2831853f));

        // --- WIDTH (M/Sでサイド成分を伸縮) ---
        const float mid = (wetL + wetR) * 0.5f;
        const float side = (wetL - wetR) * 0.5f * (mWidth * 2.0f);
        wetL = mid + side;
        wetR = mid - side;

        l = l * (1.0f - mMix) + wetL * mMix;
        r = r * (1.0f - mMix) + wetR * mMix;
    }

private:
    static float allpass(fxutil::DelayLine& d, float len, float in) noexcept
    {
        const float g = 0.5f;
        const float delayed = d.read(len);
        const float v = in + delayed * g;
        d.write(v);
        return delayed - v * g;
    }

    float combRenR(int i) const noexcept { return combLenR[(size_t)i]; }

    double sampleRate = 44100.0;
    fxutil::DelayLine preL, preR;
    std::array<fxutil::DelayLine, 4> combL, combR;
    std::array<fxutil::DelayLine, 2> apL, apR;
    std::array<fxutil::OnePole, 4> dampL, dampR;
    fxutil::OnePole hpL, hpR;
    std::array<float, 4> combLenL {}, combLenR {};
    std::array<float, 2> apLen {};
    float mSize = 0.5f, mDamp = 0.4f, mMix = 0.3f, mFeedback = 0.84f;
    float mPredelay = 0.0f, mWidth = 0.5f, mLowCut = 0.005f, mModAmt = 0.3f;
    float modPhase = 0.0f;
};

// ==========================================
// FxChain — 4スロット直列
// ==========================================
class FxChain
{
public:
    // FX5種を全部同時に挿せるようスロットも5本
    static constexpr int kNumSlots = 5;

    enum Type { None = 0, Resonator, Drive, Gate, Chorus, Reverb, NumTypes };

    static juce::StringArray getTypeNames()
    {
        return { "---", "Resonator", "Drive", "Gate", "Chorus", "Reverb" };
    }

    struct SlotParams
    {
        int type = None;
        float amount = 1.0f;   // このスロットのDry/Wet
    };

    struct Params
    {
        std::array<SlotParams, kNumSlots> slot;

        // Resonator (Colors完全移植: MIDIモード専用)
        float resShift = 0.0f;
        float resDecay = 0.5f;
        float resDamp = 30.0f;
        float resShimmer = 0.0f;
        float resInharm = 0.0f;
        float resSpread = 80.0f;
        float resOutGain = 0.0f;
        std::array<float, 8> midiHz {};   // MIDIモード時の押鍵周波数 (低い順)
        int   numMidiHz = 0;

        // Drive
        int   drvShape = 0;
        float drvDrive = 4.0f;
        float drvLow = 0.4f, drvMid = 1.0f, drvHigh = 0.7f;

        // Gate (Colors完全移植: 50パターン、PPQ同期、S-Curve)
        int   gateRate = 4;
        int   gatePattern = 0;
        float gateDepth = 80.0f;
        float gateDecay = 50.0f;
        float gateVowel = 50.0f;

        // Chorus
        float choRate = 0.6f, choDepth = 4.0f, choWidth = 0.7f;

        // Reverb
        float revSize = 0.5f, revDamp = 0.4f;
        float revPredelay = 20.0f, revWidth = 0.6f, revLowCut = 200.0f, revMod = 0.3f;

        double bpm = 120.0;
        double ppqPosition = 0.0;
        bool   isPlaying = false;
    };

    void prepare(double sr)
    {
        sampleRate = sr;
        mResonator.prepare(sr);
        mDrive.prepare(sr);
        mGate.prepare(sr);
        mChorus.prepare(sr);
        mReverb.prepare(sr);

        // Amount / Type 切替の平滑係数。
        //  Amount はブロック毎の階段だとジッパーノイズになるので τ=15ms、
        //  Type 切替は一度 Amount を 0 まで落としてから差し替えるので τ=8ms。
        const double s = (sr > 1000.0) ? sr : 48000.0;
        mAmtCoef  = (float)(1.0 - std::exp(-1.0 / (0.015 * s)));
        mSwapCoef = (float)(1.0 - std::exp(-1.0 / (0.008 * s)));
        reset();
    }

    void reset()
    {
        mResonator.reset();
        mDrive.reset();
        mGate.reset();
        mChorus.reset();
        mReverb.reset();
        for (auto& s : mSlotRt)
            s = {};
    }

    // ブロック先頭で1回だけ呼ぶ。
    //  smoothCoef: 連続値パラメータの1極平滑係数。呼び出し側がブロック長から
    //  coef = 1 - exp(-blockSec/0.03) として渡す。1.0 で平滑なし。
    //  DECAY や SIZE のような連続パラメータはブロック毎の階段だと段差が聞こえるため、
    //  ここでいったん均してから各FXへ渡す (バッファ1024smpなら21msの段差が消える)。
    void syncParameters(const Params& p, float smoothCoef = 1.0f) noexcept
    {
        const float c = juce::jlimit(0.0f, 1.0f, smoothCoef);
        auto sm = [c](float& cur, float tgt) { cur += c * (tgt - cur); };

        if (!mParamsPrimed)
        {
            mParams = p;
            mParamsPrimed = true;
        }
        else
        {
            // 整数・離散値はそのまま反映 (平滑すると中間値が生まれて破綻する)
            mParams.slot        = p.slot;
            mParams.drvShape    = p.drvShape;
            mParams.gateRate    = p.gateRate;
            mParams.gatePattern = p.gatePattern;
            mParams.midiHz      = p.midiHz;
            mParams.numMidiHz   = p.numMidiHz;
            mParams.bpm         = p.bpm;
            mParams.ppqPosition = p.ppqPosition;
            mParams.isPlaying   = p.isPlaying;

            // 連続値は1極で追従させる
            sm(mParams.resShift,   p.resShift);
            sm(mParams.resDecay,   p.resDecay);
            sm(mParams.resDamp,    p.resDamp);
            sm(mParams.resShimmer, p.resShimmer);
            sm(mParams.resInharm,  p.resInharm);
            sm(mParams.resSpread,  p.resSpread);
            sm(mParams.resOutGain, p.resOutGain);

            sm(mParams.drvDrive,   p.drvDrive);    sm(mParams.drvLow,    p.drvLow);
            sm(mParams.drvMid,     p.drvMid);      sm(mParams.drvHigh,   p.drvHigh);

            sm(mParams.gateDepth,  p.gateDepth);
            sm(mParams.gateDecay,  p.gateDecay);
            sm(mParams.gateVowel,  p.gateVowel);

            sm(mParams.choRate,    p.choRate);     sm(mParams.choDepth,  p.choDepth);
            sm(mParams.choWidth,   p.choWidth);
            sm(mParams.revSize,    p.revSize);     sm(mParams.revDamp,   p.revDamp);
            sm(mParams.revPredelay,p.revPredelay); sm(mParams.revWidth,  p.revWidth);
            sm(mParams.revLowCut,  p.revLowCut);   sm(mParams.revMod,    p.revMod);
        }

        // PPQ時間進行の設定
        mCurPpq = p.ppqPosition;
        const double safeBpm = (p.bpm > 20.0 && p.bpm < 400.0) ? p.bpm : 120.0;
        mPpqPerSample = (safeBpm / 60.0) / (sampleRate > 1000.0 ? sampleRate : 44100.0);
        mIsPlaying = p.isPlaying;

        // 以降は平滑済みの mParams を各FXへ渡す (p ではないことに注意)
        const Params& q = mParams;
        mResonator.setParams(q.resDecay, q.resDamp * 0.01f,
                             q.resShimmer * 0.01f, q.resInharm * 0.01f,
                             q.resSpread * 0.01f, q.resOutGain,
                             q.resShift, q.midiHz.data(), q.numMidiHz);
        mDrive.setParams(q.drvShape, q.drvDrive, q.drvLow, q.drvMid, q.drvHigh);
        mGate.setParams(q.gateRate, q.gatePattern,
                        q.gateDepth * 0.01f, q.gateDecay * 0.01f, q.gateVowel * 0.01f);
        // Mixは各slotのAmountで管理するのでFX内部のMixは常に1.0
        mChorus.setParams(q.choRate, q.choDepth, q.choWidth, 1.0f);
        mReverb.setParams(q.revSize, q.revDamp, 1.0f,
                          q.revPredelay, q.revWidth, q.revLowCut, q.revMod);
    }

    // スロット順に直列適用。各スロットのAmountがそのFXのDry/Wet。
    void processSample(float& l, float& r) noexcept
    {
        for (int s = 0; s < kNumSlots; ++s)
        {
            const auto& sp = mParams.slot[(size_t)s];
            auto& rt = mSlotRt[(size_t)s];

            // --- Type 切替: 一度ミックスを 0 へ落としてから差し替える ---
            if (sp.type != rt.curType)
            {
                rt.swapping = true;
                rt.mix += mSwapCoef * (0.0f - rt.mix);
                if (rt.mix < 0.001f)
                {
                    rt.mix = 0.0f;
                    rt.curType = sp.type;
                    rt.swapping = false;
                    resetSlotEffect(sp.type);   // 新しいFXは綺麗な状態から始める
                }
            }
            else if (!rt.swapping)
            {
                const float target = (rt.curType == None)
                                       ? 0.0f : juce::jlimit(0.0f, 1.0f, sp.amount);
                rt.mix += mAmtCoef * (target - rt.mix);
            }

            if (rt.curType == None)
                continue;

            // Amount=0 でも本体は常に走らせる (残響の尾を保つため)
            float wl = l, wr = r;
            switch (rt.curType)
            {
            case Resonator: mResonator.process(wl, wr); break;
            case Drive:     mDrive.process(wl, wr);     break;
            case Gate:      mGate.process(wl, wr, mParams.bpm, mCurPpq, mIsPlaying); break;
            case Chorus:    mChorus.process(wl, wr);    break;
            case Reverb:    mReverb.process(wl, wr);    break;
            default: continue;
            }

            const float a = rt.mix;
            if (a > 0.0f)
            {
                l = l * (1.0f - a) + wl * a;
                r = r * (1.0f - a) + wr * a;
            }
        }

        mCurPpq += mPpqPerSample;
    }

    // 現在どれかのスロットが処理を必要としているか。
    //  Amount=0 でも状態を進め続けたいので、Type が入っていれば true を返す。
    bool isActive() const noexcept
    {
        for (int s = 0; s < kNumSlots; ++s)
            if (mParams.slot[(size_t)s].type != None || mSlotRt[(size_t)s].curType != None
                || mSlotRt[(size_t)s].mix > 0.0f)
                return true;
        return false;
    }

private:
    // スロット差し替え時に、そのFXの内部状態だけを初期化する
    void resetSlotEffect(int type) noexcept
    {
        switch (type)
        {
        case Resonator: mResonator.reset(); break;
        case Drive:     mDrive.reset();     break;
        case Gate:      mGate.reset();      break;
        case Chorus:    mChorus.reset();    break;
        case Reverb:    mReverb.reset();    break;
        default: break;
        }
    }

    // スロット毎のランタイム状態 (平滑済みミックスと現在有効なFX種別)
    struct SlotRt
    {
        int   curType  = None;
        float mix      = 0.0f;
        bool  swapping = false;
    };
    std::array<SlotRt, kNumSlots> mSlotRt {};
    float mAmtCoef  = 0.002f;
    float mSwapCoef = 0.004f;
    bool  mParamsPrimed = false;   // 初回は平滑せず即値で取り込む

    double sampleRate = 44100.0;
    double mCurPpq = 0.0;
    double mPpqPerSample = 0.0;
    bool   mIsPlaying = false;

    Params mParams;
    SpectralResonator mResonator;
    MultibandDrive mDrive;
    FormantGate mGate;
    EnsembleChorus mChorus;
    SimpleReverb mReverb;
};
