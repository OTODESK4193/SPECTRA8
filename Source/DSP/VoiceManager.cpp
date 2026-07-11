#include "VoiceManager.h"
#include <cmath>
#include <algorithm>

namespace DSP {

VoiceManager::VoiceManager()
    : mSampleRate(44100.0)
{
    setup(mSampleRate);
}

void VoiceManager::setup(double sampleRate)
{
    mSampleRate = sampleRate;
    
    // 全状態初期化
    for (int i = 0; i < 8; ++i)
    {
        mVoiceBlock.phaseL[i] = 0.0f;
        mVoiceBlock.phaseR[i] = 0.0f;
        mVoiceBlock.phaseIncL[i] = 0.0f;
        mVoiceBlock.phaseIncR[i] = 0.0f;
        mVoiceBlock.detuneCoeffL[i] = 1.0f;
        mVoiceBlock.detuneCoeffR[i] = 1.0f;
        mVoiceBlock.noteNo[i] = 0.0f;
        mVoiceBlock.gate[i] = 0.0f;
        mVoiceBlock.age[i] = 0.0f;
        mVoiceBlock.triggerRand[i] = 0.0f;
        mVoiceBlock.wavetablePosition[i] = 0.0f;
        mVoiceBlock.formantShift[i] = 0.0f;
        mVoiceBlock.detuneWidth[i] = 0.0f;
        
        // 0以外のXorshift初期シード
        mVoiceBlock.xorState[i] = 123456789 + i * 987654;

        mActiveStates[i] = 0.0f;
        mEnvelopeValues[i] = 0.0f;
        mNoiseMix[i] = 0.0f;
        mAdsrStage[i] = AdsrStage::Idle;
        mAdsrValue[i] = 0.0f;
    }
}

// Xorshift32 スカラー版
static inline uint32_t xorshift32_scalar(uint32_t& state)
{
    state ^= (state << 13);
    state ^= (state >> 17);
    state ^= (state << 5);
    return state;
}

// [0.0f, 1.0f) のスカラー乱数
static inline float randomFloat01_scalar(uint32_t& state)
{
    uint32_t r = xorshift32_scalar(state);
    uint32_t mantissa = r & 0x007FFFFF;
    uint32_t exponent = mantissa | 0x3F800000;
    float* fPtr = reinterpret_cast<float*>(&exponent);
    return *fPtr - 1.0f;
}

int VoiceManager::allocateVoice(uint8_t /*note*/)
{
    // 1. Inactive（未使用）ボイスを探す
    for (int i = 0; i < 8; ++i)
    {
        if (mActiveStates[i] == 0.0f)
        {
            return i;
        }
    }

    // 2. Released（鍵盤が離され、リリース中）ボイスから最古（最大のage）のものを探す
    int oldestReleasedIdx = -1;
    float maxReleasedAge = -1.0f;
    for (int i = 0; i < 8; ++i)
    {
        if (mActiveStates[i] == 1.0f && mVoiceBlock.gate[i] == 0.0f)
        {
            if (mVoiceBlock.age[i] > maxReleasedAge)
            {
                maxReleasedAge = mVoiceBlock.age[i];
                oldestReleasedIdx = i;
            }
        }
    }
    if (oldestReleasedIdx != -1)
    {
        return oldestReleasedIdx;
    }

    // 3. すべてが発音中 (Active) のため、最も古いボイスを強制スティーリング (最古優先)
    int oldestActiveIdx = 0;
    float maxActiveAge = mVoiceBlock.age[0];
    for (int i = 1; i < 8; ++i)
    {
        if (mVoiceBlock.age[i] > maxActiveAge)
        {
            maxActiveAge = mVoiceBlock.age[i];
            oldestActiveIdx = i;
        }
    }

    return oldestActiveIdx;
}

void VoiceManager::releaseVoice(uint8_t note)
{
    // 同じノート番号を持つボイスをリリース状態に移行させる
    for (int i = 0; i < 8; ++i)
    {
        if (mActiveStates[i] == 1.0f && static_cast<uint8_t>(mVoiceBlock.noteNo[i]) == note)
        {
            mVoiceBlock.gate[i] = 0.0f;
            mAdsrStage[i] = AdsrStage::Release;
        }
    }
}

void VoiceManager::processMidiEvents(MidiQueue& midiQueue, PolyphonicVoiceSoA& dspState)
{
    NoteEvent event;
    while (midiQueue.tryPop(event))
    {
        if (event.type == NoteEvent::NoteOn)
        {
            // すでに同じノートが発音中なら一度リリース
            releaseVoice(event.note);

            int idx = allocateVoice(event.note);
            if (idx >= 0 && idx < 8)
            {
                // ボイス初期化
                mVoiceBlock.noteNo[idx] = static_cast<float>(event.note);
                mVoiceBlock.gate[idx] = 1.0f;
                mVoiceBlock.age[idx] = 0.0f;
                mActiveStates[idx] = 1.0f;
                mAdsrStage[idx] = AdsrStage::Attack;
                mAdsrValue[idx] = 0.0f;
                
                // トリガーランダムの生成
                mVoiceBlock.triggerRand[idx] = randomFloat01_scalar(mVoiceBlock.xorState[idx]);

                // 左右独立の初期位相ランダム化 (0〜2048)
                dspState.phaseL[idx] = randomFloat01_scalar(mVoiceBlock.xorState[idx]) * 2048.0f;
                dspState.phaseR[idx] = randomFloat01_scalar(mVoiceBlock.xorState[idx]) * 2048.0f;

                // 48バンド 4次ZDF SVF直列フィルタ状態変数のゼロクリア
                for (int b = 0; b < PolyphonicVoiceSoA::kNumBands; ++b)
                {
                    dspState.filterS1_S1_L[b][idx] = 0.0f;
                    dspState.filterS1_S2_L[b][idx] = 0.0f;
                    dspState.filterS1_S1_R[b][idx] = 0.0f;
                    dspState.filterS1_S2_R[b][idx] = 0.0f;

                    dspState.filterS2_S1_L[b][idx] = 0.0f;
                    dspState.filterS2_S2_L[b][idx] = 0.0f;
                    dspState.filterS2_S1_R[b][idx] = 0.0f;
                    dspState.filterS2_S2_R[b][idx] = 0.0f;
                }
            }
        }
        else if (event.type == NoteEvent::NoteOff)
        {
            releaseVoice(event.note);
        }
    }
}

void VoiceManager::updateVoices(float attackTime, float decayTime, float sustainLevel, float releaseTime)
{
    // パラメータを時間からサンプル数に変換 (最小値を 0.001 秒としてガード)
    float attSamples = std::max(0.001f, attackTime) * static_cast<float>(mSampleRate);
    float decSamples = std::max(0.001f, decayTime) * static_cast<float>(mSampleRate);
    float relSamples = std::max(0.001f, releaseTime) * static_cast<float>(mSampleRate);
    float susLevel = std::clamp(sustainLevel, 0.0f, 1.0f);

    for (int i = 0; i < 8; ++i)
    {
        if (mActiveStates[i] == 0.0f)
        {
            mEnvelopeValues[i] = 0.0f;
            mAdsrValue[i] = 0.0f;
            mAdsrStage[i] = AdsrStage::Idle;
            continue;
        }

        // age インクリメント
        mVoiceBlock.age[i] += 1.0f;

        // ADSRの状態遷移マシン
        switch (mAdsrStage[i])
        {
            case AdsrStage::Attack:
            {
                mAdsrValue[i] += 1.0f / attSamples;
                if (mAdsrValue[i] >= 1.0f)
                {
                    mAdsrValue[i] = 1.0f;
                    mAdsrStage[i] = AdsrStage::Decay;
                }
                break;
            }
            case AdsrStage::Decay:
            {
                mAdsrValue[i] -= (1.0f - susLevel) / decSamples;
                if (mAdsrValue[i] <= susLevel)
                {
                    mAdsrValue[i] = susLevel;
                    mAdsrStage[i] = AdsrStage::Sustain;
                }
                break;
            }
            case AdsrStage::Sustain:
            {
                mAdsrValue[i] = susLevel;
                // gate が 0 になったら NoteOff が入ったので Release へ (プロセスイベントで設定)
                if (mVoiceBlock.gate[i] == 0.0f)
                {
                    mAdsrStage[i] = AdsrStage::Release;
                }
                break;
            }
            case AdsrStage::Release:
            {
                mAdsrValue[i] -= susLevel / relSamples;
                if (mAdsrValue[i] <= 0.0f)
                {
                    mAdsrValue[i] = 0.0f;
                    mActiveStates[i] = 0.0f; // ボイスの完全終了
                    mAdsrStage[i] = AdsrStage::Idle;
                }
                break;
            }
            case AdsrStage::Idle:
            default:
            {
                mAdsrValue[i] = 0.0f;
                mActiveStates[i] = 0.0f;
                break;
            }
        }

        mEnvelopeValues[i] = mAdsrValue[i];
    }
}

void VoiceManager::syncToDspState(PolyphonicVoiceSoA& dspState, float detuneWidthCents, float pitchTranspose, float tracking, float currentF0)
{
    // ノートに基づく位相増分の算出
    for (int i = 0; i < 8; ++i)
    {
        if (mActiveStates[i] == 1.0f)
        {
            // MIDIノート+トランスポーズから周波数を算出
            float freqMidi = 440.0f * std::pow(2.0f, ((mVoiceBlock.noteNo[i] + pitchTranspose) - 69.0f) / 12.0f);
            
            // tracking パラメータに基づき、MIDIピッチと入力音声ピッチの間でブレンド
            // (1.0 = 完全入力ピッチ追従、0.0 = 完全MIDIピッチ固定)
            float freq0 = freqMidi + (currentF0 - freqMidi) * tracking;
            
            // 複数ボイスがアクティブな場合（ユニゾンやAUTOモード時など）の厚みを出すピッチ分散
            // ボイス0: センター, ボイス1: 低め（-0.6倍）, ボイス2: 高め（+0.6倍）, ボイス3: 低め（-0.3倍）など
            float unisonOffset = 0.0f;
            if (i == 1) unisonOffset = -0.6f;
            else if (i == 2) unisonOffset = 0.6f;
            else if (i == 3) unisonOffset = -0.3f;
            else if (i == 4) unisonOffset = 0.3f;
            else if (i == 5) unisonOffset = -0.9f;
            else if (i == 6) unisonOffset = 0.9f;

            // WidthとTriggerRandomによる左右デチューン幅 + ユニゾン用オフセット
            // 各ボイス内でL/Rもデチューンし、さらにボイス間でもデチューンさせることで厚みを出す
            float detuneVal = detuneWidthCents * (1.0f + 0.3f * unisonOffset) + mVoiceBlock.triggerRand[i] * 10.0f;
            
            // ボイス間の根本的な周波数のズレ (unisonOffset を周波数に反映)
            // デチューン幅 detuneWidthCents が 0 の時は完全に同調する
            float voiceDetuneCents = detuneWidthCents * unisonOffset;
            float voiceFreqScale = std::pow(2.0f, voiceDetuneCents / 1200.0f);
            float baseFreq = freq0 * voiceFreqScale;

            // Cents から倍率への変換 (各ボイス内での左右のデチューン)
            float coeffL = std::pow(2.0f, -detuneVal / 1200.0f);
            float coeffR = std::pow(2.0f, detuneVal / 1200.0f);

            float freqL = baseFreq * coeffL;
            float freqR = baseFreq * coeffR;

            // 位相増分: inc = freq / sampleRate * tableSize
            dspState.phaseIncrL[i] = (freqL / static_cast<float>(mSampleRate)) * 2048.0f;
            dspState.phaseIncrR[i] = (freqR / static_cast<float>(mSampleRate)) * 2048.0f;
        }
        else
        {
            dspState.phaseIncrL[i] = 0.0f;
            dspState.phaseIncrR[i] = 0.0f;
        }
    }
}

void VoiceManager::setVoiceActive(int voiceIdx, bool active)
{
    if (voiceIdx >= 0 && voiceIdx < 8)
    {
        mActiveStates[voiceIdx] = active ? 1.0f : 0.0f;
    }
}

void VoiceManager::setVoiceFrequency(int voiceIdx, float freqHz)
{
    if (voiceIdx >= 0 && voiceIdx < 8)
    {
        mVoiceBlock.noteNo[voiceIdx] = 69.0f + 12.0f * std::log2(std::max(20.0f, freqHz) / 440.0f);
    }
}

void VoiceManager::setVoiceEnvelope(int voiceIdx, float envVal)
{
    if (voiceIdx >= 0 && voiceIdx < 8)
    {
        mEnvelopeValues[voiceIdx] = std::clamp(envVal, 0.0f, 1.0f);
    }
}

__m256 VoiceManager::getActiveVoicesMask() const
{
    return _mm256_load_ps(mActiveStates);
}

__m256 VoiceManager::getVoiceEnvelopes() const
{
    return _mm256_load_ps(mEnvelopeValues);
}

__m256 VoiceManager::getNoiseMix() const
{
    // 全ボイス共通の仮固定、あるいは有声判定に基づく
    // Phase 1 ではひとまず 0.0f でトーンのみ。有声無声判定で動的に変える部分は
    // 後続の分析パイプラインから updateVoices 経由などで書き込む。
    return _mm256_load_ps(mNoiseMix);
}

int VoiceManager::getNumActiveVoices() const
{
    int count = 0;
    for (int i = 0; i < 8; ++i)
    {
        if (mActiveStates[i] == 1.0f)
        {
            count++;
        }
    }
    return count;
}

} // namespace DSP
