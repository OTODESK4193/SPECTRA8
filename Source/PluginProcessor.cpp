#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <cstring>
#include <algorithm>

SPECTRA8AudioProcessor::SPECTRA8AudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::mono(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "Parameters", createParameterLayout()),
    mMidiQueue(1024),
    mDspState(),
    mAnalysisHopSize(100),
    mAnalysisWindowSize(1024),
    mControlRateBlockSize(32),
    mControlRateCounter(0)
{
    std::memset(&mDspState, 0, sizeof(mDspState));
}

SPECTRA8AudioProcessor::~SPECTRA8AudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SPECTRA8AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("character", 1), "Character", 0.0f, 1.0f, 1.0f));

    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("bandCount", 1), "Band Count", 8, 48, 48));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("formantShift", 1), "Formant Shift", -24.0f, 24.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("formantStretch", 1), "Formant Stretch", 0.5f, 2.0f, 1.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("pitch", 1), "Pitch", -36.0f, 36.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tracking", 1), "Tracking", 0.0f, 100.0f, 100.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("detune", 1), "Detune", 0.0f, 1200.0f, 5.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("noise", 1), "Noise", 0.0f, 100.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("waveform", 1), "Waveform", juce::StringArray{ "Saw", "Pulse", "Wavetable" }, 0));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("wavetablePosition", 1), "Wavetable Position", 0.0f, 1.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("pulseWidth", 1), "Pulse Width", 5.0f, 95.0f, 50.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("attack", 1), "Attack", 0.001f, 5.0f, 0.01f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("decay", 1), "Decay", 0.001f, 5.0f, 0.1f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("sustain", 1), "Sustain", 0.0f, 1.0f, 0.8f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("release", 1), "Release", 0.001f, 5.0f, 0.2f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("mix", 1), "Mix", 0.0f, 100.0f, 100.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("outputLevel", 1), "Output Level", -60.0f, 12.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("mode", 1), "Mode", juce::StringArray{ "Auto", "MIDI" }, 0));

    return layout;
}

void SPECTRA8AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mStoredSampleRate = sampleRate;

    mAnalysisWindowSize = 1024;
    mAnalysisHopSize = 100;
    mControlRateBlockSize = 32;
    mDownsampleTimeAccum = 0.0;

    mVoiceManager.setup(16000.0);
    mOscillatorBank.setup(16000.0);

    mMidiQueue.clear();
    mMidiActiveMode = false;
    mInputEnvelope = 0.0f;
    mWasAutoVoiceActive = false;

    std::memset(&mDspState, 0, sizeof(mDspState));

    int maxBands = DSP::PolyphonicVoiceSoA::kNumBands;
    mBandEnvelopes.assign(maxBands, 0.0f);
    mTargetBandEnvelopes.assign(maxBands, 0.0f);

    mAnalFilterS1.assign(maxBands * 2, 0.0f);
    mAnalFilterS2.assign(maxBands * 2, 0.0f);

    mBandF0.resize(maxBands);
    mBandCoeffsG.resize(maxBands);
    mBandCoeffsK.resize(maxBands);
    mBandCoeffsA1.resize(maxBands);

    float fMin = 80.0f;
    float fMax = 7500.0f;
    float mMin = 2595.0f * std::log10(1.0f + fMin / 700.0f);
    float mMax = 2595.0f * std::log10(1.0f + fMax / 700.0f);
    float Q = 10.0f;

    for (int i = 0; i < maxBands; ++i)
    {
        float mVal = mMin + (mMax - mMin) * (static_cast<float>(i) / (maxBands - 1));
        float freq = 700.0f * (std::pow(10.0f, mVal / 2595.0f) - 1.0f);
        mBandF0[i] = freq;

        float g = std::tan(3.14159265f * freq / 16000.0f);
        float k = 1.0f / Q;
        float a1 = 1.0f / (1.0f + g * (g + k));

        mBandCoeffsG[i] = g;
        mBandCoeffsK[i] = k;
        mBandCoeffsA1[i] = a1;
    }

    mFormantShiftSmoother.reset(16000.0);

    int safeAllocationSize = std::max(samplesPerBlock * 3, 4096);
    mDownsampledBuffer.assign(static_cast<size_t>(safeAllocationSize), 0.0f);
    m16kWetBuffer.assign(static_cast<size_t>(safeAllocationSize), 0.0f);
    mWetFsBuffer.assign(static_cast<size_t>(safeAllocationSize), 0.0f);
    mDryLBuffer.assign(static_cast<size_t>(safeAllocationSize), 0.0f);

    mControlRateCounter = 0;
    mCurrentUnvoicedRatio = 0.0f;
    mTargetUnvoicedRatio = 0.0f;
}

void SPECTRA8AudioProcessor::releaseResources()
{
}

bool SPECTRA8AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void SPECTRA8AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numInputs = getTotalNumInputChannels();
    const int numOutputs = getTotalNumOutputChannels();
    double srcSampleRate = getSampleRate();

    if (srcSampleRate != mStoredSampleRate && srcSampleRate > 0.0)
    {
        prepareToPlay(srcSampleRate, numSamples);
    }

#define CHECK_NAN(val, msg) \
        if (std::isnan(val) || std::isinf(val)) { \
            mErrorState.store(1); \
        }

#define CHECK_NAN_ARRAY(ptr, size, msg) \
        for (int _i = 0; _i < (size); ++_i) { \
            if (std::isnan((ptr)[_i]) || std::isinf((ptr)[_i])) { \
                mErrorState.store(1); \
                break; \
            } \
        }

    if (numInputs > 0 && numSamples > 0)
    {
        CHECK_NAN_ARRAY(buffer.getReadPointer(0), numSamples, "DAW Input Buffer");
    }

    // 1. MIDIメッセージのロード
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        DSP::NoteEvent event;
        event.sampleOffset = static_cast<uint32_t>(metadata.samplePosition);
        if (msg.isNoteOn())
        {
            event.type = DSP::NoteEvent::NoteOn;
            event.note = static_cast<uint8_t>(msg.getNoteNumber());
            event.velocity = static_cast<uint8_t>(msg.getVelocity());
            mMidiQueue.tryPush(event);
        }
        else if (msg.isNoteOff())
        {
            event.type = DSP::NoteEvent::NoteOff;
            event.note = static_cast<uint8_t>(msg.getNoteNumber());
            event.velocity = 0;
            mMidiQueue.tryPush(event);
        }
    }
    midiMessages.clear();

    // 2. パラメータのロード
    float character = apvts.getRawParameterValue("character")->load();
    float formantShift = apvts.getRawParameterValue("formantShift")->load();
    float detuneWidth = apvts.getRawParameterValue("detune")->load();
    float noiseParam = apvts.getRawParameterValue("noise")->load() * 0.01f;
    int currentNumBands = static_cast<int>(std::clamp(apvts.getRawParameterValue("bandCount")->load(), 8.0f, 48.0f));

    mFormantShiftSmoother.setTargetValue(formantShift);

    float pitchTranspose = apvts.getRawParameterValue("pitch")->load();
    float tracking = apvts.getRawParameterValue("tracking")->load() * 0.01f;

    float attack = apvts.getRawParameterValue("attack")->load();
    float decay = apvts.getRawParameterValue("decay")->load();
    float sustain = apvts.getRawParameterValue("sustain")->load();
    float release = apvts.getRawParameterValue("release")->load();

    float mix = apvts.getRawParameterValue("mix")->load() * 0.01f;
    float outputDb = apvts.getRawParameterValue("outputLevel")->load();
    float outputGain = std::pow(10.0f, outputDb / 20.0f);

    int mode = static_cast<int>(apvts.getRawParameterValue("mode")->load());
    bool isMidiMode = (mode == 1);

    // 3. 入力音声の包絡線（エンベロープ）算出
    if (numInputs > 0 && numSamples > 0)
    {
        const float* inputL = buffer.getReadPointer(0);
        float sumAbs = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            sumAbs += std::abs(inputL[i]);
        }
        float avgAbs = sumAbs / static_cast<float>(numSamples);
        float coeff = (avgAbs > mInputEnvelope) ? 0.05f : 0.005f;
        mInputEnvelope = mInputEnvelope * (1.0f - coeff) + avgAbs * coeff;
    }

    if (isMidiMode)
    {
        mVoiceManager.processMidiEvents(mMidiQueue, mDspState);
        mVoiceManager.updateVoices(attack, decay, sustain, release);
        mVoiceManager.syncToDspState(mDspState, detuneWidth, pitchTranspose, tracking, 130.0f);
        mWasAutoVoiceActive = false;
    }
    else
    {
        bool hasInput = (mInputEnvelope > 0.0001f);

        if (hasInput && !mWasAutoVoiceActive)
        {
            mDspState.phaseL[0] = 0.0f;
            mDspState.phaseR[0] = 0.0f;

            for (int b = 0; b < DSP::PolyphonicVoiceSoA::kNumBands; ++b)
            {
                mDspState.filterS1_S1_L[b][0] = 0.0f;
                mDspState.filterS1_S2_L[b][0] = 0.0f;
                mDspState.filterS1_S1_R[b][0] = 0.0f;
                mDspState.filterS1_S2_R[b][0] = 0.0f;
                mDspState.filterS2_S1_L[b][0] = 0.0f;
                mDspState.filterS2_S2_L[b][0] = 0.0f;
                mDspState.filterS2_S1_R[b][0] = 0.0f;
                mDspState.filterS2_S2_R[b][0] = 0.0f;
            }
        }
        mWasAutoVoiceActive = hasInput;

        mVoiceManager.setVoiceActive(0, hasInput);
        for (int i = 1; i < 8; ++i)
        {
            mVoiceManager.setVoiceActive(i, false);
        }

        float envVolume = std::clamp(mInputEnvelope * 6.0f, 0.0f, 1.0f);
        mVoiceManager.setVoiceEnvelope(0, envVolume);
        mVoiceManager.setVoiceFrequency(0, 130.0f);
        mVoiceManager.syncToDspState(mDspState, detuneWidth, pitchTranspose, 0.0f, 130.0f);
    }

    // サンプル精度フォルマントシフターの取得
    mFormantShiftSmoother.skip(numSamples);
    float currentFormantShift = mFormantShiftSmoother.getCurrentValue();

    // 4. 入力音声を 16kHz にダウンサンプリング
    int num16kSamples = 0;
    if (numInputs > 0 && numSamples > 0 && srcSampleRate > 0.0)
    {
        const float* inputL = buffer.getReadPointer(0);
        double timeAccum = mDownsampleTimeAccum;
        double step = srcSampleRate / 16000.0;
        int maxSafeSize = static_cast<int>(mDownsampledBuffer.size());

        while (timeAccum < static_cast<double>(numSamples))
        {
            int idx0 = static_cast<int>(timeAccum);
            int idx1 = std::min(numSamples - 1, idx0 + 1);
            float frac = static_cast<float>(timeAccum - idx0);

            float val = inputL[idx0] * (1.0f - frac) + inputL[idx1] * frac;

            if (num16kSamples < maxSafeSize)
            {
                mDownsampledBuffer[num16kSamples] = val;
                num16kSamples++;
            }

            timeAccum += step;
        }
        mDownsampleTimeAccum = timeAccum - static_cast<double>(numSamples);
    }

    // 5. 16kHz領域でのリアルタイム・チャネルボコーディング処理
    int maxBands = DSP::PolyphonicVoiceSoA::kNumBands;

    for (int sample16k = 0; sample16k < num16kSamples; ++sample16k)
    {
        float inSample = mDownsampledBuffer[sample16k];

        // 5-A. 分析側（モジュレーター）：100%確実に音が鳴る実績のあるオリジナル差分式
        for (int i = 0; i < currentNumBands; ++i)
        {
            // --- セクション 1 ---
            float s1_s1 = mAnalFilterS1[i];
            float s2_s1 = mAnalFilterS2[i];
            float v1_s1 = mBandCoeffsA1[i] * (mBandCoeffsG[i] * (inSample - s2_s1) - s1_s1);
            float y_bp_s1 = v1_s1;
            float y_lp_s1 = mBandCoeffsG[i] * v1_s1 + s2_s1;

            mAnalFilterS1[i] = 2.0f * y_bp_s1 - s1_s1;
            mAnalFilterS2[i] = 2.0f * y_lp_s1 - s2_s1;

            // --- セクション 2 (直列接続 S1 -> S2) ---
            int idx_s2 = i + maxBands;
            float s1_s2 = mAnalFilterS1[idx_s2];
            float s2_s2 = mAnalFilterS2[idx_s2];
            float v1_s2 = mBandCoeffsA1[i] * (mBandCoeffsG[i] * (y_bp_s1 - s2_s2) - s1_s2);
            float y_bp_s2 = v1_s2;
            float y_lp_s2 = mBandCoeffsG[i] * v1_s2 + s2_s2;

            mAnalFilterS1[idx_s2] = 2.0f * y_bp_s2 - s1_s2;
            mAnalFilterS2[idx_s2] = 2.0f * y_lp_s2 - s2_s2;

            // 整流エンベロープフォロワーの更新
            float env = std::abs(y_bp_s2);
            float envCoeff = (env > mTargetBandEnvelopes[i]) ? 0.015f : 0.003f;
            mTargetBandEnvelopes[i] = mTargetBandEnvelopes[i] * (1.0f - envCoeff) + env * envCoeff;
        }

        if (mControlRateCounter >= mControlRateBlockSize || mControlRateCounter == 0)
        {
            mControlRateCounter = 0;
            std::copy(mTargetBandEnvelopes.begin(), mTargetBandEnvelopes.end(), mBandEnvelopes.begin());
        }
        mControlRateCounter++;

        float lowEnergy = 0.0f;
        float highEnergy = 0.0f;
        int midPoint = currentNumBands / 2;
        for (int i = 0; i < midPoint; ++i) lowEnergy += mBandEnvelopes[i];
        for (int i = midPoint; i < currentNumBands; ++i) highEnergy += mBandEnvelopes[i];

        float uvRatio = highEnergy / (lowEnergy + highEnergy + 1e-6f);
        float consonantFactor = std::clamp((uvRatio - 0.22f) * 3.0f, 0.0f, 1.0f);
        mCurrentUnvoicedRatio = mCurrentUnvoicedRatio * 0.9f + consonantFactor * 0.1f;

        __m256 noiseBuffer = mNoiseGenerator.nextBlockAVX2();
        __m256 activeMask = mVoiceManager.getActiveVoicesMask();
        __m256 mixEnvelopes = mVoiceManager.getVoiceEnvelopes();

        float dynNoiseMix = std::clamp(noiseParam + (1.0f - noiseParam) * mCurrentUnvoicedRatio * 1.5f, 0.0f, 1.0f);
        __m256 noiseMixVec = _mm256_set1_ps(dynNoiseMix);

        float sampleL = 0.0f;
        float sampleR = 0.0f;

        // ★WET音復活の核心：キャリアフィルターは固定係数のまま処理（発散・相殺を100%防止）
        // スムージングされた currentFormantShift スカラー値を直接 OscillatorBank へ転送
        mOscillatorBank.processSampleAVX2(
            mDspState,
            activeMask,
            mixEnvelopes,
            noiseMixVec,
            noiseBuffer,
            mBandEnvelopes.data(),
            currentFormantShift,
            currentNumBands,
            mBandCoeffsG.data(),
            mBandCoeffsK.data(),
            mBandCoeffsA1.data(),
            mBandCoeffsA1.data(),
            sampleL,
            sampleR
        );

        if (sample16k < static_cast<int>(m16kWetBuffer.size()))
        {
            m16kWetBuffer[sample16k] = 0.5f * (sampleL + sampleR);
        }
    }

    // 7. 16kHz Wet信号をホストサンプリングレートへアップサンプリング
    juce::FloatVectorOperations::clear(mWetFsBuffer.data(), numSamples);
    if (numSamples > 0 && num16kSamples > 0)
    {
        double upRatio = static_cast<double>(num16kSamples) / static_cast<double>(numSamples);
        int maxWetFsSize = static_cast<int>(mWetFsBuffer.size());

        for (int i = 0; i < numSamples; ++i)
        {
            double pos = i * upRatio;
            int idx0 = static_cast<int>(pos);
            int idx1 = std::min(num16kSamples - 1, idx0 + 1);
            float frac = static_cast<float>(pos - idx0);

            if (i < maxWetFsSize)
            {
                mWetFsBuffer[i] = m16kWetBuffer[idx0] * (1.0f - frac) + m16kWetBuffer[idx1] * frac;
            }
        }
    }

    // 8. 最終ミックスと出力
    auto* writePointerL = (numOutputs > 0) ? buffer.getWritePointer(0) : nullptr;
    auto* writePointerR = (numOutputs > 1) ? buffer.getWritePointer(1) : nullptr;

    juce::FloatVectorOperations::clear(mDryLBuffer.data(), numSamples);
    if (numInputs > 0 && buffer.getNumChannels() > 0)
    {
        std::memcpy(mDryLBuffer.data(), buffer.getReadPointer(0), static_cast<size_t>(numSamples) * sizeof(float));
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float drySampleL = mDryLBuffer[sample];
        float drySampleR = (numInputs > 1 && buffer.getNumChannels() > 1) ? buffer.getReadPointer(1)[sample] : drySampleL;

        float wetVal = mWetFsBuffer[sample];

        if (mInputEnvelope < 0.00005f)
        {
            wetVal = 0.0f;
        }

        float wetSampleL = wetVal;
        float wetSampleR = wetVal;

        if (character < 0.98f)
        {
            float bits = 4.0f + 20.0f * character;
            float steps = std::pow(2.0f, bits);

            int holdSamples = static_cast<int>(1.0f + 31.0f * (1.0f - character));
            int holdStartIdx = std::clamp((sample / holdSamples) * holdSamples, 0, numSamples - 1);

            wetSampleL = mWetFsBuffer[holdStartIdx];
            wetSampleR = mWetFsBuffer[holdStartIdx];

            wetSampleL = std::round(wetSampleL * steps) / steps;
            wetSampleR = std::round(wetSampleR * steps) / steps;
        }

        if (!std::isfinite(wetSampleL) || !std::isfinite(wetSampleR))
        {
            wetSampleL = 0.0f;
            wetSampleR = 0.0f;
        }

        float outValL = (1.0f - mix) * drySampleL + mix * wetSampleL;
        float outValR = (1.0f - mix) * drySampleR + mix * wetSampleR;

        CHECK_NAN(outValL, "outValL");
        CHECK_NAN(outValR, "outValR");

        if (writePointerL != nullptr) writePointerL[sample] = outValL * outputGain;
        if (writePointerR != nullptr) writePointerR[sample] = outValR * outputGain;
    }

#undef CHECK_NAN
#undef CHECK_NAN_ARRAY
}

juce::AudioProcessorEditor* SPECTRA8AudioProcessor::createEditor()
{
    return new SPECTRA8AudioProcessorEditor(*this);
}

void SPECTRA8AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SPECTRA8AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SPECTRA8AudioProcessor();
}