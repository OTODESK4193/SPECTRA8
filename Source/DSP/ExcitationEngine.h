// ==========================================
// File: ExcitationEngine.h
// キャリア励起信号生成エンジン (8ボイス・スカラー設計)
//
//  - 8音ポリフォニック（古い順スチール・アロケータ）
//  - PolyBLEPによるアンチエイリアシング (Saw / Pulse)
//  - Wavetableモーフィング (MorphWavetableを使用)
//  - Lo-Fiエフェクト (ビット量子化 + ピッチ同期サンプル&ホールド)
//  - ADSRエンベロープ
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include <cmath>
#include "Wavetable.h"

class ExcitationEngine
{
public:
    static constexpr int kMaxVoices = 8;
    static constexpr double kInternalSampleRate = 16000.0;

    ExcitationEngine();
    ~ExcitationEngine() = default;

    void prepare(double sampleRate);
    void reset();

    void noteOn(int noteNumber, float velocity) noexcept;
    void noteOff(int noteNumber) noexcept;
    void allNotesOff() noexcept;

    void syncParameters(int waveform, float wtPos, float pulseWidth, float detuneCents, 
                        float noiseMix, float lofi, float portaTimeSec,
                        float attackSec, float decaySec, float sustainVal, float releaseSec,
                        float noiseColorHz) noexcept;

    void processSample(float& outL, float& outR, float externalPitchHz, bool isMidiMode) noexcept;

private:
    struct SvfFilter
    {
        float s1 = 0.0f;
        float s2 = 0.0f;
        void reset() { s1 = 0.0f; s2 = 0.0f; }
        float processBPF(float in, float fc, float sampleRate) noexcept
        {
            // fc を安全な範囲 (ナイキスト周波数の 90% 以下) に制限して tan の発散(NaN)を防止
            float safeFc = juce::jlimit(20.0f, sampleRate * 0.45f, fc);
            float g = std::tan(3.14159265f * safeFc / sampleRate);
            float r = 0.5f; // Q = 1.0相当 (1/(2Q))
            float h = 1.0f / (1.0f + g * (g + 2.0f * r));
            float v0 = in;
            float v1 = (s1 + g * (v0 - s2)) * h;
            float v2 = s2 + g * v1;
            s1 = 2.0f * v1 - s1;
            s2 = 2.0f * v2 - s2;
            return v1;
        }
    };

    struct Voice
    {
        bool active = false;
        int noteNumber = -1;
        float velocity = 0.0f;
        uint32_t triggerTime = 0;

        // ピッチとポルタメント
        float targetFreq = 130.0f;
        float currentFreq = 130.0f;

        // 位相
        float phaseL = 0.0f;
        float phaseR = 0.0f;

        // ADSR状態
        enum AdsrStage { Idle, Attack, Decay, Sustain, Release };
        AdsrStage stage = Idle;
        float envValue = 0.0f;
        float releaseStartVal = 0.0f;
    };

    void triggerVoice(int noteNumber, float velocity) noexcept;
    float processVoiceSample(Voice& v, int channel, float phaseInc) noexcept;
    float applyPolyBlep(float phase, float phaseInc) const noexcept;
    // pitchHz: ピッチ同期S&Hの基準基音 (ホールドレート = N×pitchHz)
    void applyLoFi(float& l, float& r, float pitchHz) noexcept;

    MorphWavetable mWavetable; // 共有ウェーブテーブル

    std::array<Voice, kMaxVoices> mVoices {};
    uint32_t mTriggerCounter = 0;
    float mLastTriggeredFreq = 130.0f; // ポルタメント開始用

    // パラメータ
    int mWaveform = 0; // 0: Saw, 1: Pulse, 2: Wavetable
    float mWtPos = 0.0f;
    float mPulseWidth = 0.5f;
    float mDetuneCents = 0.0f;
    float mNoiseMix = 0.0f;
    float mNoiseColor = 1000.0f; // 新設: ノイズ音程 (BPF Cutoff)
    float mLofi = 0.0f;
    float mPortaTime = 0.0f;
    
    // ADSR
    float mAttack = 0.01f;
    float mDecay = 0.1f;
    float mSustain = 0.8f;
    float mRelease = 0.2f;

    // LoFiサンプルレートダウン用ステート
    float mLofiRateCounter = 0.0f;
    float mLofiLastValL = 0.0f;
    float mLofiLastValR = 0.0f;

    // ノイズフィルタ
    SvfFilter mNoiseFilterL;
    SvfFilter mNoiseFilterR;

    juce::Random mRng;
};
