// ==========================================
// File: ExcitationEngine.h
// キャリア励起信号生成エンジン (8ボイス・スカラー設計)
//
//  - 8音ポリフォニック（古い順スチール・アロケータ）
//  - PolyBLEPによるアンチエイリアシング (Saw / Pulse)
//  - Wavetableモーフィング (MorphWavetableを使用)
//  - Lo-Fiエフェクト (量子化、サンプルレートダウン、ピッチ量子化)
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
                        float attackSec, float decaySec, float sustainVal, float releaseSec) noexcept;

    void processSample(float& outL, float& outR, float externalPitchHz, bool isMidiMode) noexcept;

private:
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
    void applyLoFi(float& l, float& r) noexcept;

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

    juce::Random mRng;
};
