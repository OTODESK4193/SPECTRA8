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
#include "ScaleSnap.h"

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
                        float noiseColorHz, int detuneMode,
                        float bendAmt, float bendShift,
                        float syncAmt, float syncShift,
                        float vocAmt, float vocShift) noexcept;

    // M.PITCH / PITCH Q をMIDIモードのボイスにも適用するための設定。
    //  Autoモードでは PluginProcessor 側の activePitch に既に適用済みなので、
    //  エンジン側の適用は isMidiMode のときだけ行う (二重適用を避ける)。
    void setPitchShaping(float masterPitchSt, float quantAmt, int key, int scale) noexcept
    {
        mMasterPitchSt = juce::jlimit(-24.0f, 24.0f, masterPitchSt);
        mQuantAmt = juce::jlimit(0.0f, 1.0f, quantAmt);
        mQuantKey = juce::jlimit(0, 11, key);
        mQuantScale = juce::jlimit(0, ScaleSnap::kNumScales - 1, scale);
    }

    // カスタムWavetableロード用アクセス (メッセージスレッドからのロード専用)
    MorphWavetable& getWavetable() noexcept { return mWavetable; }
    const MorphWavetable& getWavetable() const noexcept { return mWavetable; }

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

        // 可変Q・中心利得0dB正規化のBPF (Morph Vocodeのフォルマント用)
        float processBPF_Q(float in, float fc, float sampleRate, float Q) noexcept
        {
            float safeFc = juce::jlimit(20.0f, sampleRate * 0.45f, fc);
            float g = std::tan(3.14159265f * safeFc / sampleRate);
            float k = 1.0f / juce::jmax(0.3f, Q);
            float h = 1.0f / (1.0f + g * (g + k));
            float v1 = (s1 + g * (in - s2)) * h;
            float v2 = s2 + g * v1;
            s1 = 2.0f * v1 - s1;
            s2 = 2.0f * v2 - s2;
            return v1 * k;   // 中心利得を1.0に正規化
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

        // PITCH Q のヒステリシス状態 (ボイス毎。-1 = 未保持)
        int quantNoteHeld = -1;
    };

    void triggerVoice(int noteNumber, float velocity) noexcept;
    float processVoiceSample(Voice& v, int channel, float phaseInc) noexcept;
    float applyPolyBlep(float phase, float phaseInc) const noexcept;

    // Morph位相ワープ (BassSynth WavetableOscillator::applyPhaseWarp より移植)。
    // 戻り値=ワープ後位相。sMul にはSyncモードの境界フェード用振幅係数を乗算する。
    float applyMorphPhase(float phase, float phaseInc, float& sMul) const noexcept;
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
    int   mDetuneMode = 0;   // 0=Classic 1=Linear 2=Exp 3=Drift 4=Chorus
    float mNoiseMix = 0.0f;
    float mNoiseColor = 1000.0f; // 新設: ノイズ音程 (BPF Cutoff)
    float mLofi = 0.0f;
    float mPortaTime = 0.0f;

    // M.PITCH / PITCH Q (MIDIモードでのみエンジン側が適用する)
    float mMasterPitchSt = 0.0f;
    float mQuantAmt = 0.0f;
    int   mQuantKey = 0;
    int   mQuantScale = 0;
    
    // ADSR
    float mAttack = 0.01f;
    float mDecay = 0.1f;
    float mSustain = 0.8f;
    float mRelease = 0.2f;

    // パラメータ・スムージング (制御ブロック毎の階段状変化→サンプル毎一次平滑 τ≈5ms)
    float mDetuneSm = 0.0f;
    float mNoiseSm = 0.0f;

    // Detune Mode用ステート (Drift=ボイス毎ランダムウォーク / Chorus=ボイス毎LFO位相)
    std::array<float, kMaxVoices> mDriftVal {};
    std::array<float, kMaxVoices> mChorusPhase {};

    // ---- Morph (BassSynthより移植。3種は独立ノブで同時併用可能) ----
    //  適用順: Bend(位相ワープ) → Sync(位相繰り返し) → Vocode(フォルマントBPF)
    //  各Amt=0 でその段は完全にバイパス。
    bool  mBendOn = false;
    bool  mSyncOn = false;
    // Bend事前計算 (BassSynth precomputeWarp mode1)
    float mBendSym = 0.5f;
    float mBendB = 1.0f;
    // Sync事前計算 (BassSynth precomputeWarp mode3 + shift位相オフセット)
    float mSyncSt = 1.0f;
    float mSyncShiftHalf = 0.0f;
    // Vocode: 母音フォルマント (倍音番号。中心Hz=倍音番号×基音)
    //  BassSynth SpectralMorphProcessor mode9 の A-I-U-E-O テーブルを補間した値
    float mVocHarm[3] = { 32.0f, 55.0f, 120.0f };
    float mVocAmt = 0.0f;
    // Vocode用フォルマントBPF (3基×L/R)
    SvfFilter mVocFilterL[3];
    SvfFilter mVocFilterR[3];

    // LoFiサンプルレートダウン用ステート
    float mLofiRateCounter = 0.0f;
    float mLofiLastValL = 0.0f;
    float mLofiLastValR = 0.0f;

    // ノイズフィルタ
    SvfFilter mNoiseFilterL;
    SvfFilter mNoiseFilterR;

    juce::Random mRng;
};
