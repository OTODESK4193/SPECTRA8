// ==========================================
// File: ExcitationEngine.cpp
// キャリア励起信号生成エンジン (8ボイス・スカラー設計)
// ==========================================
#include "ExcitationEngine.h"

ExcitationEngine::ExcitationEngine()
{
    reset();
}

void ExcitationEngine::prepare(double sampleRate)
{
    // 内部は常に 16kHz で動作するため、サンプルレート低減等の一部の定数を調整
    reset();
}

void ExcitationEngine::reset()
{
    mTriggerCounter = 0;
    mLastTriggeredFreq = 130.0f;
    mLofiRateCounter = 0.0f;
    mLofiLastValL = 0.0f;
    mLofiLastValR = 0.0f;

    for (auto& v : mVoices)
    {
        v.active = false;
        v.noteNumber = -1;
        v.velocity = 0.0f;
        v.triggerTime = 0;
        v.targetFreq = 130.0f;
        v.currentFreq = 130.0f;
        v.phaseL = 0.0f;
        v.phaseR = 0.0f;
        v.stage = Voice::Idle;
        v.envValue = 0.0f;
        v.releaseStartVal = 0.0f;
    }
}

void ExcitationEngine::noteOn(int noteNumber, float velocity) noexcept
{
    if (velocity <= 0.0f)
    {
        noteOff(noteNumber);
        return;
    }

    triggerVoice(noteNumber, velocity);
}

void ExcitationEngine::noteOff(int noteNumber) noexcept
{
    for (auto& v : mVoices)
    {
        if (v.active && v.noteNumber == noteNumber)
        {
            v.stage = Voice::Release;
            v.releaseStartVal = v.envValue;
        }
    }
}

void ExcitationEngine::allNotesOff() noexcept
{
    for (auto& v : mVoices)
    {
        if (v.active)
        {
            v.stage = Voice::Release;
            v.releaseStartVal = v.envValue;
        }
    }
}

void ExcitationEngine::syncParameters(int waveform, float wtPos, float pulseWidth, float detuneCents, 
                                    float noiseMix, float lofi, float portaTimeSec,
                                    float attackSec, float decaySec, float sustainVal, float releaseSec) noexcept
{
    mWaveform = juce::jlimit(0, 2, waveform);
    mWtPos = juce::jlimit(0.0f, 1.0f, wtPos);
    mPulseWidth = juce::jlimit(0.05f, 0.95f, pulseWidth);
    mDetuneCents = juce::jlimit(0.0f, 1200.0f, detuneCents);
    mNoiseMix = juce::jlimit(0.0f, 1.0f, noiseMix);
    mLofi = juce::jlimit(0.0f, 1.0f, lofi);
    mPortaTime = juce::jlimit(0.0f, 2.0f, portaTimeSec);

    mAttack = juce::jmax(0.001f, attackSec);
    mDecay = juce::jmax(0.001f, decaySec);
    mSustain = juce::jlimit(0.0f, 1.0f, sustainVal);
    mRelease = juce::jmax(0.001f, releaseSec);
}

void ExcitationEngine::triggerVoice(int noteNumber, float velocity) noexcept
{
    const float freq = 440.0f * std::pow(2.0f, (static_cast<float>(noteNumber) - 69.0f) / 12.0f);
    mTriggerCounter++;

    // 1. 同一ノート番号が既に発音中ならリトリガー
    for (auto& v : mVoices)
    {
        if (v.active && v.noteNumber == noteNumber)
        {
            v.velocity = velocity;
            v.targetFreq = freq;
            v.triggerTime = mTriggerCounter;
            v.stage = Voice::Attack;
            mLastTriggeredFreq = freq;
            return;
        }
    }

    // 2. 空きボイスを探す
    for (auto& v : mVoices)
    {
        if (!v.active || v.stage == Voice::Idle)
        {
            v.active = true;
            v.noteNumber = noteNumber;
            v.velocity = velocity;
            v.targetFreq = freq;
            v.triggerTime = mTriggerCounter;
            v.stage = Voice::Attack;

            // ポルタメント処理
            if (mPortaTime > 0.001f)
                v.currentFreq = mLastTriggeredFreq;
            else
                v.currentFreq = freq;

            mLastTriggeredFreq = freq;
            return;
        }
    }

    // 3. 空きがなければ最も古いボイスをスチール (古い順スチール)
    int oldestIdx = 0;
    uint32_t oldestTime = 0xFFFFFFFF;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (mVoices[(size_t)i].triggerTime < oldestTime)
        {
            oldestTime = mVoices[(size_t)i].triggerTime;
            oldestIdx = i;
        }
    }

    auto& v = mVoices[(size_t)oldestIdx];
    v.active = true;
    v.noteNumber = noteNumber;
    v.velocity = velocity;
    v.targetFreq = freq;
    v.triggerTime = mTriggerCounter;
    v.stage = Voice::Attack;
    v.envValue = 0.0f; // スチールなのでリセット

    if (mPortaTime > 0.001f)
        v.currentFreq = mLastTriggeredFreq;
    else
        v.currentFreq = freq;

    mLastTriggeredFreq = freq;
}

float ExcitationEngine::applyPolyBlep(float phase, float phaseInc) const noexcept
{
    // 不連続点の前後 dt サンプル範囲での補正
    if (phase < phaseInc)
    {
        float t = phase / phaseInc;
        return t + t - t * t - 1.0f;
    }
    else if (phase > 1.0f - phaseInc)
    {
        float t = (phase - 1.0f) / phaseInc;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

float ExcitationEngine::processVoiceSample(Voice& v, int channel, float phaseInc) noexcept
{
    float phase = (channel == 0) ? v.phaseL : v.phaseR;

    float out = 0.0f;
    if (mWaveform == 0) // Saw
    {
        out = 2.0f * phase - 1.0f;
        out -= applyPolyBlep(phase, phaseInc);
    }
    else if (mWaveform == 1) // Pulse
    {
        out = (phase < mPulseWidth) ? 1.0f : -1.0f;
        out += applyPolyBlep(phase, phaseInc);
        out -= applyPolyBlep(std::fmod(phase + (1.0f - mPulseWidth), 1.0f), phaseInc);
    }
    else // Wavetable
    {
        out = mWavetable.sample(phase, mWtPos, phaseInc);
    }

    return out;
}

void ExcitationEngine::applyLoFi(float& l, float& r) noexcept
{
    if (mLofi <= 0.0f) return;

    // 1. ビット数量子化
    // lofi=0: 24bit (実質変化なし), lofi=1: 1bit
    const float bits = 24.0f - 23.0f * mLofi;
    const float levels = std::pow(2.0f, bits);
    l = std::round(l * levels) / levels;
    r = std::round(r * levels) / levels;

    // 2. サンプルレート低減 (サンプル・アンド・ホールド)
    // lofi=0: 16kHz (ホールドなし), lofi=1: 約100Hz相当
    const float targetSr = 16000.0f * std::pow(0.00625f, mLofi); // 16000 -> 100
    const float sampleHoldPeriod = 16000.0f / targetSr;

    mLofiRateCounter += 1.0f;
    if (mLofiRateCounter >= sampleHoldPeriod)
    {
        mLofiRateCounter = std::fmod(mLofiRateCounter, sampleHoldPeriod);
        mLofiLastValL = l;
        mLofiLastValR = r;
    }
    else
    {
        l = mLofiLastValL;
        r = mLofiLastValR;
    }
}

void ExcitationEngine::processSample(float& outL, float& outR, float externalPitchHz, bool isMidiMode) noexcept
{
    float voiceSumL = 0.0f;
    float voiceSumR = 0.0f;
    int activeVoiceCount = 0;

    // ポルタメント係数
    const float portaCoeff = (mPortaTime > 0.001f)
        ? static_cast<float>(1.0 - std::exp(-1.0 / (mPortaTime * kInternalSampleRate)))
        : 1.0f;

    // ADSR用 1サンプルあたりのインクリメント量
    const float attStep = 1.0f / (mAttack * (float)kInternalSampleRate);
    const float decStep = 1.0f / (mDecay * (float)kInternalSampleRate);
    const float relStep = 1.0f / (mRelease * (float)kInternalSampleRate);

    for (int i = 0; i < kMaxVoices; ++i)
    {
        auto& v = mVoices[(size_t)i];

        // 非MIDIモード（Autoモード）の場合、ボイス0〜2をアクティブにしてユニゾンデチューン効果を得る
        if (!isMidiMode)
        {
            if (i > 2)
            {
                v.active = false;
                v.stage = Voice::Idle;
                v.envValue = 0.0f;
                v.velocity = 0.0f;
                continue;
            }
            v.active = true;
            v.stage = Voice::Sustain;
            v.envValue = (i == 0) ? 1.0f : 0.8f;
            v.velocity = 1.0f;
            v.targetFreq = externalPitchHz;
        }

        if (!v.active || v.stage == Voice::Idle) continue;

        activeVoiceCount++;

        // 1. ピッチポルタメント適用
        v.currentFreq += portaCoeff * (v.targetFreq - v.currentFreq);

        float pitch = v.currentFreq;
        
        // Lo-Fi ピッチ量子化 (lofi > 0.5 の場合に最も近い半音スケールにクランプ)
        if (mLofi > 0.5f)
        {
            float note = std::round(12.0f * std::log2(pitch / 440.0f) + 69.0f);
            pitch = 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f);
        }

        // 2. デチューン計算
        // ボイスペアにステレオ広がりと分厚さを出すためのユニゾンデチューン
        float detuneCents = mDetuneCents;
        // ボイスインデックスに基づくデチューン比率の分散
        float detuneSign = (i % 2 == 0) ? -1.0f : 1.0f;
        float detuneScale = 0.15f + 0.1f * static_cast<float>(i / 2);
        float detuneOffset = detuneCents * detuneSign * detuneScale;

        float freqL = pitch * std::pow(2.0f, detuneOffset / 1200.0f);
        float freqR = pitch * std::pow(2.0f, -detuneOffset / 1200.0f);

        // 範囲制限
        freqL = juce::jlimit(20.0f, 7800.0f, freqL);
        freqR = juce::jlimit(20.0f, 7800.0f, freqR);

        float phaseIncL = freqL / (float)kInternalSampleRate;
        float phaseIncR = freqR / (float)kInternalSampleRate;

        // 3. ボイス別ADSRエンベロープ更新 (MIDIモード時のみ)
        if (isMidiMode)
        {
            switch (v.stage)
            {
            case Voice::Attack:
                v.envValue += attStep;
                if (v.envValue >= 1.0f)
                {
                    v.envValue = 1.0f;
                    v.stage = Voice::Decay;
                }
                break;

            case Voice::Decay:
                v.envValue -= decStep * (1.0f - mSustain);
                if (v.envValue <= mSustain)
                {
                    v.envValue = mSustain;
                    v.stage = Voice::Sustain;
                }
                break;

            case Voice::Sustain:
                v.envValue = mSustain;
                break;

            case Voice::Release:
                v.envValue -= relStep * v.releaseStartVal;
                if (v.envValue <= 0.0f)
                {
                    v.envValue = 0.0f;
                    v.stage = Voice::Idle;
                    v.active = false;
                }
                break;

            default:
                v.envValue = 0.0f;
                break;
            }
        }
        else
        {
            // Autoモード時はADSR設定を無視してキャリア音量を最大固定
            v.envValue = (i == 0) ? 1.0f : 0.8f;
        }

        // 4. 波形生成
        float voiceL = processVoiceSample(v, 0, phaseIncL);
        float voiceR = processVoiceSample(v, 1, phaseIncR);

        float gain = v.envValue * v.velocity;
        voiceSumL += voiceL * gain;
        voiceSumR += voiceR * gain;

        // 5. 位相インクリメント
        v.phaseL = std::fmod(v.phaseL + phaseIncL, 1.0f);
        v.phaseR = std::fmod(v.phaseR + phaseIncR, 1.0f);
    }

    // 6. ノイズブレンド (有声/無声比率、有声音へのホワイトノイズ混入)
    // -1.0 〜 +1.0 のホワイトノイズ
    float noiseSampleL = mRng.nextFloat() * 2.0f - 1.0f;
    float noiseSampleR = mRng.nextFloat() * 2.0f - 1.0f;

    // ポリフォニック合算音がクリップしないようにスケーリング
    float voiceScale = (activeVoiceCount > 1) ? (1.0f / std::sqrt((float)activeVoiceCount)) : 1.0f;
    voiceSumL *= voiceScale;
    voiceSumR *= voiceScale;

    outL = (1.0f - mNoiseMix) * voiceSumL + mNoiseMix * noiseSampleL;
    outR = (1.0f - mNoiseMix) * voiceSumR + mNoiseMix * noiseSampleR;

    // MIDIモード時、鍵盤を弾いていない（アクティブなボイスが0）ときはキャリアを完全ミュート（常時ノイズ出力を防止）
    if (isMidiMode && activeVoiceCount == 0)
    {
        outL = 0.0f;
        outR = 0.0f;
        return;
    }

    // 7. LoFiポストエフェクト適用
    applyLoFi(outL, outR);
}
