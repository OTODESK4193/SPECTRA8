#pragma once
#include <vector>
#include <cstdint>
#include <immintrin.h>
#include "VoiceState.h"
#include "MidiQueue.h"

namespace DSP {

class VoiceManager {
public:
    VoiceManager();
    ~VoiceManager() = default;

    // 初期化
    void setup(double sampleRate);

    // MIDIキューからイベントを取得してボイス状態を更新する
    void processMidiEvents(MidiQueue& midiQueue, PolyphonicVoiceSoA& dspState);

    // ボイス状態（ADSRエンベロープなど）を 1サンプル分更新する
    void updateVoices(float attackTime, float decayTime, float sustainLevel, float releaseTime);

    // SoAのデータをAVX2レンダリング用構造体に書き出す（同期）
    void syncToDspState(PolyphonicVoiceSoA& dspState, float detuneWidthCents);

    // オートモード用：特定のボイスの状態を直接書き換える
    void setVoiceActive(int voiceIdx, bool active);
    void setVoiceFrequency(int voiceIdx, float freqHz);
    void setVoiceEnvelope(int voiceIdx, float envVal);

    // 各ボイスのアクティブフラグ (__m256 形式) を取得
    __m256 getActiveVoicesMask() const;
    
    // 各ボイスのエンベロープ/音量 (__m256 形式) を取得
    __m256 getVoiceEnvelopes() const;

    // 各ボイスの現在の有声/無声ノイズミックス量を取得
    __m256 getNoiseMix() const;

    // 現在のアクティブボイス数を返す
    int getNumActiveVoices() const;

private:
    // 新しいノートを割り当てる (最古優先スティーリング)
    int allocateVoice(uint8_t note);

    // 指定されたノートのボイスをリリースする
    void releaseVoice(uint8_t note);

    double mSampleRate;

    // VoiceSoABlock 構造体（ボイスの状態）
    VoiceSoABlock mVoiceBlock;
    
    // 各ボイスの実質アクティブ状態 (1.0f = Active, 0.0f = Inactive)
    alignas(32) float mActiveStates[8];
    
    // 現在の音量エンベロープ値 [0.0f, 1.0f]
    alignas(32) float mEnvelopeValues[8];
    
    // 各ボイスのノイズ比率 (0.0f = トーンのみ, 1.0f = ノイズのみ)
    alignas(32) float mNoiseMix[8];
    
    // ADSRの状態管理 (8ボイス分)
    enum class AdsrStage {
        Idle,
        Attack,
        Decay,
        Sustain,
        Release
    };
    AdsrStage mAdsrStage[8];
    float mAdsrValue[8];
};

} // namespace DSP
