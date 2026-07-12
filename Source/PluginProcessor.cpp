#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DSP/LspUtils.h"
#include "DSP/PitchDetector.h"
#include <cmath>
#include <cstring>
#include <algorithm>

SPECTRA8AudioProcessor::SPECTRA8AudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
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
    
    for (int i = 0; i < 48; ++i)
    {
        mBandGains[i].store(1.0f);
        mBandLevelsForUi[i].store(0.0f);
    }
    mLpcAnalysisBuffer.assign(512, 0.0f);
    mCurrentLpcCoeffs.assign(16, 0.0f);
    
    mCurrentLsp.assign(16, 0.0f);
    mCurrentLspSmoothed.assign(16, 0.0f);
    mLspStep.assign(16, 0.0f);
    mFrozenLsp.assign(16, 0.0f);
    mCurrentLar.assign(16, 0.0f);
    mCurrentLarSmoothed.assign(16, 0.0f);
    mLarStep.assign(16, 0.0f);
    mFrozenLar.assign(16, 0.0f);
    mPitchHistory.assign(5, 130.0f);
    mLpcResidualBuffer.assign(512, 0.0f);
    mMsDelayBuffer.assign(128, 0.0f);
    mMsDelayWritePtr = 0;
    mApfBufferL.assign(64, 0.0f);
    mApfBufferR.assign(64, 0.0f);
    mApfWritePtrL = 0;
    mApfWritePtrR = 0;
}

SPECTRA8AudioProcessor::~SPECTRA8AudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SPECTRA8AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("character", 1), "Character", 0.0f, 1.0f, 1.0f));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("vocoderMode", 1), "Vocoder Mode", juce::StringArray{ "Filterbank", "LPC Mode" }, 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("limiterEnable", 1), "Limiter", juce::StringArray{ "Off", "On" }, 1));

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
        juce::ParameterID("outputLevel", 1), "Output Level", -60.0f, 12.0f, -24.0f));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("mode", 1), "Mode", juce::StringArray{ "Auto", "MIDI" }, 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("formantFreeze", 1), "Formant Freeze", juce::StringArray{ "Off", "On" }, 0));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("stereoWidth", 1), "Stereo Width", 0.0f, 100.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("windowType", 1), "Window Type", juce::StringArray{ "Hann", "Hamming", "Blackman" }, 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("interpolationMode", 1), "Interpolation Mode", juce::StringArray{ "LSP", "LAR" }, 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("filterbankType", 1), "Filterbank Type", juce::StringArray{ "Bandpass", "Subtractive" }, 0));

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

    mFormantShiftSmoother.reset(16000);

    int safeAllocationSize = std::max(samplesPerBlock * 3, 4096);
    mDownsampledBuffer.assign(static_cast<size_t>(safeAllocationSize), 0.0f);
    m16kWetBufferL.assign(static_cast<size_t>(safeAllocationSize), 0.0f);
    m16kWetBufferR.assign(static_cast<size_t>(safeAllocationSize), 0.0f);
    mWetFsBufferL.assign(static_cast<size_t>(safeAllocationSize), 0.0f);
    mWetFsBufferR.assign(static_cast<size_t>(safeAllocationSize), 0.0f);
    mDryLBuffer.assign(static_cast<size_t>(safeAllocationSize), 0.0f);

    mControlRateCounter = 0;
    mCurrentUnvoicedRatio = 0.0f;
    mTargetUnvoicedRatio = 0.0f;

    mLpcAnalysisBuffer.assign(512, 0.0f);
    mCurrentLpcCoeffs.assign(16, 0.0f);
    
    mCurrentLsp.assign(16, 0.0f);
    mCurrentLspSmoothed.assign(16, 0.0f);
    mLspStep.assign(16, 0.0f);
    mFrozenLsp.assign(16, 0.0f);
    mCurrentLar.assign(16, 0.0f);
    mCurrentLarSmoothed.assign(16, 0.0f);
    mLarStep.assign(16, 0.0f);
    mFrozenLar.assign(16, 0.0f);
    mFormantFreezeActive = false;
    
    mCurrentPitchHz = 130.0f;
    mTargetPitchHz = 130.0f;
    mPitchSmoothed = 130.0f;
    mIsVoiced = false;
    mVoicedDebounceCounter = 0;
    mPitchHistory.assign(5, 130.0f);
    mLpcResidualBuffer.assign(512, 0.0f);
    
    mModeCrossfade.reset(sampleRate, 0.030);
    mPrevVocoderMode = -1;
    mMode0WetBufferL.assign(static_cast<size_t>(safeAllocationSize), 0.0f);
    mMode0WetBufferR.assign(static_cast<size_t>(safeAllocationSize), 0.0f);
    mMode1WetBufferL.assign(static_cast<size_t>(safeAllocationSize), 0.0f);
    mMode1WetBufferR.assign(static_cast<size_t>(safeAllocationSize), 0.0f);
    
    mSmoothedGain = 1.0f;
    mLimiterGain = 1.0f;

    mMsFilterState = 0.0f;
    mMsPrevInput = 0.0f;
    mMsDelayBuffer.assign(128, 0.0f);
    mMsDelayWritePtr = 0;
    mApfBufferL.assign(64, 0.0f);
    mApfBufferR.assign(64, 0.0f);
    mApfWritePtrL = 0;
    mApfWritePtrR = 0;
}

void SPECTRA8AudioProcessor::releaseResources()
{
}

bool SPECTRA8AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo() &&
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

    // 追加でロードするパラメータ
    int waveform = static_cast<int>(apvts.getRawParameterValue("waveform")->load());
    float pulseWidth = apvts.getRawParameterValue("pulseWidth")->load() * 0.01f;
    float wavetablePosition = apvts.getRawParameterValue("wavetablePosition")->load();
    int vocoderMode = static_cast<int>(apvts.getRawParameterValue("vocoderMode")->load());
    bool limiterEnable = static_cast<int>(apvts.getRawParameterValue("limiterEnable")->load()) == 1;

    bool freezeParam = (static_cast<int>(apvts.getRawParameterValue("formantFreeze")->load()) == 1);
    float stereoWidth = apvts.getRawParameterValue("stereoWidth")->load() * 0.01f;

    int windowType = static_cast<int>(apvts.getRawParameterValue("windowType")->load());
    int interpolationMode = static_cast<int>(apvts.getRawParameterValue("interpolationMode")->load());
    int filterbankType = static_cast<int>(apvts.getRawParameterValue("filterbankType")->load());

    // アルゴリズム切り替え時のクロスフェード開始判定
    if (mPrevVocoderMode != vocoderMode)
    {
        if (mPrevVocoderMode != -1)
        {
            mModeCrossfade.setCurrentAndTargetValue(0.0f);
            mModeCrossfade.setTargetValue(1.0f);
        }
        else
        {
            mModeCrossfade.setCurrentAndTargetValue(1.0f);
        }
        mPrevVocoderMode = vocoderMode;
    }
    bool isCrossfading = mModeCrossfade.isSmoothing();

    // 入力ステレオイメージの解析 (M-S デコレレーターおよびパン復元用)
    float inputPan = 0.5f;
    float inputCorrelation = 1.0f;
    if (numInputs > 0 && numSamples > 0)
    {
        float sumSqL = 0.0f;
        float sumSqR = 0.0f;
        float sumCross = 0.0f;
        const float* readL = buffer.getReadPointer(0);
        const float* readR = (numInputs > 1) ? buffer.getReadPointer(1) : readL;
        for (int s = 0; s < numSamples; ++s)
        {
            float l = readL[s];
            float r = readR[s];
            sumSqL += l * l;
            sumSqR += r * r;
            sumCross += l * r;
        }
        float E_L = sumSqL;
        float E_R = sumSqR;
        float denom = std::sqrt(E_L * E_R);
        if (denom > 1e-9f)
        {
            inputCorrelation = sumCross / denom;
        }
        inputCorrelation = std::clamp(inputCorrelation, -1.0f, 1.0f);

        float sqrtEL = std::sqrt(E_L);
        float sqrtER = std::sqrt(E_R);
        float panDenom = sqrtEL + sqrtER;
        if (panDenom > 1e-9f)
        {
            inputPan = sqrtER / panDenom;
        }
        inputPan = std::clamp(inputPan, 0.0f, 1.0f);
    }

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
        mWasAutoVoiceActive = false;
    }
    else
    {
        bool hasInput = (mInputEnvelope > 0.0001f);

        if (hasInput && !mWasAutoVoiceActive)
        {
            for (int v = 0; v < 3; ++v)
            {
                mDspState.phaseL[v] = static_cast<float>(v) * 500.0f;
                mDspState.phaseR[v] = static_cast<float>(v) * 500.0f + 250.0f;

                for (int b = 0; b < DSP::PolyphonicVoiceSoA::kNumBands; ++b)
                {
                    mDspState.filterS1_S1_L[b][v] = 0.0f;
                    mDspState.filterS1_S2_L[b][v] = 0.0f;
                    mDspState.filterS1_S1_R[b][v] = 0.0f;
                    mDspState.filterS1_S2_R[b][v] = 0.0f;
                    mDspState.filterS2_S1_L[b][v] = 0.0f;
                    mDspState.filterS2_S2_L[b][v] = 0.0f;
                    mDspState.filterS2_S1_R[b][v] = 0.0f;
                    mDspState.filterS2_S2_R[b][v] = 0.0f;
                }
            }
        }
        mWasAutoVoiceActive = hasInput;

        mVoiceManager.setVoiceActive(0, hasInput);
        mVoiceManager.setVoiceActive(1, hasInput);
        mVoiceManager.setVoiceActive(2, hasInput);
        for (int i = 3; i < 8; ++i)
        {
            mVoiceManager.setVoiceActive(i, false);
        }

        float envVolume = std::clamp(mInputEnvelope * 6.0f, 0.0f, 1.0f);
        mVoiceManager.setVoiceEnvelope(0, envVolume);
        mVoiceManager.setVoiceEnvelope(1, envVolume * 0.8f);
        mVoiceManager.setVoiceEnvelope(2, envVolume * 0.8f);

    }

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

    int numNew = num16kSamples;
    if (numNew > 0)
    {
        if (numNew >= 512)
        {
            std::memcpy(mLpcAnalysisBuffer.data(), mDownsampledBuffer.data() + numNew - 512, 512 * sizeof(float));
        }
        else
        {
            std::memmove(mLpcAnalysisBuffer.data(), mLpcAnalysisBuffer.data() + numNew, (512 - numNew) * sizeof(float));
            std::memcpy(mLpcAnalysisBuffer.data() + 512 - numNew, mDownsampledBuffer.data(), numNew * sizeof(float));
        }
    }

    // 5. 16kHz領域でのリアルタイム・チャネルボコーディング処理
    int maxBands = DSP::PolyphonicVoiceSoA::kNumBands;
    float mPitchStep = 0.0f;

    for (int sample16k = 0; sample16k < num16kSamples; ++sample16k)
    {
        if (isMidiMode)
        {
            mVoiceManager.updateVoices(attack, decay, sustain, release);
        }

        float inSample = mDownsampledBuffer[sample16k];

        // 5-A. 分析側（モジュレーター）
        if (filterbankType == 0) // Bandpass
        {
            for (int i = 0; i < currentNumBands; ++i)
            {
                float s1_s1 = mAnalFilterS1[i];
                float s2_s1 = mAnalFilterS2[i];
                float v1_s1 = mBandCoeffsA1[i] * (s1_s1 + mBandCoeffsG[i] * (inSample - s2_s1));
                float y_bp_s1 = v1_s1;
                float y_lp_s1 = s2_s1 + mBandCoeffsG[i] * v1_s1;

                mAnalFilterS1[i] = 2.0f * y_bp_s1 - s1_s1;
                mAnalFilterS2[i] = 2.0f * y_lp_s1 - s2_s1;

                int idx_s2 = i + maxBands;
                float s1_s2 = mAnalFilterS1[idx_s2];
                float s2_s2 = mAnalFilterS2[idx_s2];
                float v1_s2 = mBandCoeffsA1[i] * (s1_s2 + mBandCoeffsG[i] * (y_bp_s1 - s2_s2));
                float y_bp_s2 = v1_s2;
                float y_lp_s2 = s2_s2 + mBandCoeffsG[i] * v1_s2;

                mAnalFilterS1[idx_s2] = 2.0f * y_bp_s2 - s1_s2;
                mAnalFilterS2[idx_s2] = 2.0f * y_lp_s2 - s2_s2;

                float env = std::abs(y_bp_s2);
                float envCoeff = (env > mTargetBandEnvelopes[i]) ? 0.015f : 0.003f;
                mTargetBandEnvelopes[i] = mTargetBandEnvelopes[i] * (1.0f - envCoeff) + env * envCoeff;
            }
        }
        else // Subtractive / LR4
        {
            const float k_lr4 = 1.41421356f; // Q = 0.707 (damping k = 1/Q)
            for (int i = 0; i < currentNumBands; ++i)
            {
                float g = mBandCoeffsG[i];
                float a1_lr4 = 1.0f / (1.0f + g * (g + k_lr4));

                // セクション 1 (LPF)
                float s1_s1 = mAnalFilterS1[i];
                float s2_s1 = mAnalFilterS2[i];
                float v1_s1 = a1_lr4 * (s1_s1 + g * (inSample - s2_s1));
                float y_lp_s1 = s2_s1 + g * v1_s1;

                mAnalFilterS1[i] = 2.0f * v1_s1 - s1_s1;
                mAnalFilterS2[i] = 2.0f * y_lp_s1 - s2_s1;

                // セクション 2 (HPF)
                int idx_s2 = i + maxBands;
                float s1_s2 = mAnalFilterS1[idx_s2];
                float s2_s2 = mAnalFilterS2[idx_s2];
                float v1_s2 = a1_lr4 * (s1_s2 + g * (y_lp_s1 - s2_s2));
                float y_lp_s2 = s2_s2 + g * v1_s2;
                float y_hp_s2 = y_lp_s1 - k_lr4 * v1_s2 - y_lp_s2; // HPF 出力

                mAnalFilterS1[idx_s2] = 2.0f * v1_s2 - s1_s2;
                mAnalFilterS2[idx_s2] = 2.0f * y_lp_s2 - s2_s2;

                float env = std::abs(y_hp_s2);
                float envCoeff = (env > mTargetBandEnvelopes[i]) ? 0.015f : 0.003f;
                mTargetBandEnvelopes[i] = mTargetBandEnvelopes[i] * (1.0f - envCoeff) + env * envCoeff;
            }
        }

        if (mControlRateCounter >= mControlRateBlockSize || mControlRateCounter == 0)
        {
            mControlRateCounter = 0;
            for (int i = 0; i < maxBands; ++i)
            {
                float gain = mBandGains[i].load();
                mBandEnvelopes[i] = mTargetBandEnvelopes[i] * gain;
                mBandLevelsForUi[i].store(mBandEnvelopes[i]);
            }
            
            // LPC/LSP分析の実行
            if (vocoderMode == 1 || isCrossfading)
            {
                float r[17] = { 0.0f };
                float windowed[512];
                for (int j = 0; j < 512; ++j)
                {
                    float w = 0.0f;
                    if (windowType == 0) // Hann
                    {
                        w = 0.5f * (1.0f - std::cos(2.0f * 3.14159265f * j / 511.0f));
                    }
                    else if (windowType == 1) // Hamming
                    {
                        w = 0.54f - 0.46f * std::cos(2.0f * 3.14159265f * j / 511.0f);
                    }
                    else // Blackman
                    {
                        w = 0.42f - 0.5f * std::cos(2.0f * 3.14159265f * j / 511.0f) + 0.08f * std::cos(4.0f * 3.14159265f * j / 511.0f);
                    }
                    windowed[j] = mLpcAnalysisBuffer[j] * w;
                }

                for (int k = 0; k <= 16; ++k)
                {
                    float sum = 0.0f;
                    for (int j = 0; j < 512 - k; ++j)
                    {
                        sum += windowed[j] * windowed[j + k];
                    }
                    r[k] = sum;
                }

                // 正確な Levinson-Durbin 再帰
                float a[17] = { 0.0f };
                float E = r[0];
                a[0] = 1.0f;

                bool lpcSuccess = false;
                if (E > 1e-9f)
                {
                    lpcSuccess = true;
                    for (int i = 1; i <= 16; ++i)
                    {
                        float sum = 0.0f;
                        for (int j = 1; j < i; ++j)
                        {
                            sum += a[j] * r[i - j];
                        }
                        
                        float lambda = - (r[i] + sum) / E;

                        // 反射係数を絶対的に安定な 0.98 に制限して発振を完全防止
                        lambda = std::clamp(lambda, -0.98f, 0.98f);

                        float next_a[17];
                        next_a[0] = 1.0f;
                        for (int j = 1; j < i; ++j)
                        {
                            next_a[j] = a[j] + lambda * a[i - j];
                        }
                        next_a[i] = lambda;

                        std::copy(next_a, next_a + i + 1, a);

                        E *= (1.0f - lambda * lambda);
                        if (E <= 1e-9f) {
                            lpcSuccess = false;
                            break;
                        }
                    }
                }

                // 帯域拡張 (gamma=0.96f により極を内側に引き込み、LSPの消失を防止)
                for (int i = 1; i <= 16; ++i)
                {
                    a[i] *= std::pow(0.96f, static_cast<float>(i));
                }

                // LSP/LAR変換とフリーズ
                if (interpolationMode == 0) // LSP Mode
                {
                    if (freezeParam)
                    {
                        if (!mFormantFreezeActive)
                        {
                            mFrozenLsp = mCurrentLsp;
                            mFormantFreezeActive = true;
                        }
                        mCurrentLsp = mFrozenLsp;
                    }
                    else
                    {
                        mFormantFreezeActive = false;
                        if (lpcSuccess)
                        {
                            float lpc_for_lsp[16];
                            for (int i = 1; i <= 16; ++i)
                            {
                                lpc_for_lsp[i - 1] = -a[i]; // 極性反転の整合
                            }
                            bool lspSuccess = DSP::LspUtils::lpcToLsp(lpc_for_lsp, mCurrentLsp.data(), 16);
                            if (lspSuccess)
                            {
                                DSP::LspUtils::enforceLspClearance(mCurrentLsp.data(), 16);
                            }
                        }
                    }
                }
                else // LAR Mode
                {
                    if (freezeParam)
                    {
                        if (!mFormantFreezeActive)
                        {
                            mFrozenLar = mCurrentLar;
                            mFormantFreezeActive = true;
                        }
                        mCurrentLar = mFrozenLar;
                    }
                    else
                    {
                        mFormantFreezeActive = false;
                        if (lpcSuccess)
                        {
                            float lpc_for_lar[16];
                            for (int i = 1; i <= 16; ++i)
                            {
                                lpc_for_lar[i - 1] = -a[i];
                            }
                            float parcor[16] = {0.0f};
                            DSP::LspUtils::lpcToParcor(lpc_for_lar, parcor, 16);
                            DSP::LspUtils::parcorToLar(parcor, mCurrentLar.data(), 16);
                        }
                    }
                }

                // ピッチ検出 (MPM) と V/UV 判定
                float pitchHz = 0.0f;
                float clarity = 0.0f;
                bool pitchFound = DSP::PitchDetector::detectPitchMPM(windowed, 512, 16000.0, pitchHz, clarity);
                
                float zcr = DSP::PitchDetector::computeZcr(windowed, 512);
                float lber = DSP::PitchDetector::computeLber(windowed, 512, 16000.0);
                float lpcGain = (r[0] > 1e-9f) ? (10.0f * std::log10(r[0] / (E + 1e-9f))) : 0.0f;

                // 有声判定条件
                bool isVoicedFrame = (pitchFound && clarity >= 0.60f && lpcGain >= 6.0f && zcr < 0.20f && lber >= 0.50f);
                if (isVoicedFrame)
                {
                    mIsVoiced = true;
                    mVoicedDebounceCounter = 0;
                }
                else
                {
                    mVoicedDebounceCounter++;
                    if (mVoicedDebounceCounter >= 3)
                    {
                        mIsVoiced = false;
                    }
                }

                if (mIsVoiced && pitchHz >= 50.0f && pitchHz <= 500.0f)
                {
                    mPitchHistory.erase(mPitchHistory.begin());
                    mPitchHistory.push_back(pitchHz);
                    
                    std::vector<float> sortedHistory = mPitchHistory;
                    std::sort(sortedHistory.begin(), sortedHistory.end());
                    mTargetPitchHz = sortedHistory[2]; // 中央値
                }
                else if (!mIsVoiced)
                {
                    mTargetPitchHz = 130.0f;
                }
            }

            // 線形補間ステップの計算
            if (interpolationMode == 0) // LSP
            {
                for (int i = 0; i < 16; ++i)
                {
                    mLspStep[i] = (mCurrentLsp[i] - mCurrentLspSmoothed[i]) / static_cast<float>(mControlRateBlockSize);
                    mLarStep[i] = 0.0f;
                }
            }
            else // LAR
            {
                for (int i = 0; i < 16; ++i)
                {
                    mLarStep[i] = (mCurrentLar[i] - mCurrentLarSmoothed[i]) / static_cast<float>(mControlRateBlockSize);
                    mLspStep[i] = 0.0f;
                }
            }
            mPitchStep = (mTargetPitchHz - mCurrentPitchHz) / static_cast<float>(mControlRateBlockSize);

            // --- キャリアボイスのパラメータ同期 (コントロールレート) ---
            float detuneCents = detuneWidth;
            if (isMidiMode)
            {
                mVoiceManager.syncToDspState(mDspState, detuneCents, pitchTranspose, tracking, mCurrentPitchHz, noiseParam);
            }
            else
            {
                float activePitch = mIsVoiced ? mCurrentPitchHz : 130.0f;
                mVoiceManager.setVoiceFrequency(0, activePitch);
                mVoiceManager.setVoiceFrequency(1, activePitch);
                mVoiceManager.setVoiceFrequency(2, activePitch);
                mVoiceManager.syncToDspState(mDspState, detuneCents, pitchTranspose, 0.0f, activePitch, noiseParam);
            }
        }
        mControlRateCounter++;

        // --- 1サンプルごとの LSP/LAR & ピッチ 補間 ---
        if (vocoderMode == 1 || isCrossfading)
        {
            mCurrentPitchHz += mPitchStep;

            std::vector<float> interpLpc(16, 0.0f);

            if (interpolationMode == 0) // LSP Mode
            {
                for (int i = 0; i < 16; ++i)
                {
                    mCurrentLspSmoothed[i] += mLspStep[i];
                }

                // LSPの双一次周波数ワーピング (BFW) をサンプル精度で適用
                std::vector<float> warpedLsp(16, 0.0f);
                float alpha = std::tanh(currentFormantShift / 24.0f * 0.45f);

                for (int i = 0; i < 16; ++i)
                {
                    float w = std::acos(std::clamp(mCurrentLspSmoothed[i], -0.9999f, 0.9999f));
                    float sin_w = std::sin(w);
                    float cos_w = std::cos(w);
                    
                    float warped_w = w + 2.0f * std::atan((alpha * sin_w) / (1.0f - alpha * cos_w + 1e-9f));
                    warpedLsp[i] = std::cos(std::clamp(warped_w, 0.001f, 3.1415f));
                }

                // ワーピング後のLSPの安定化
                DSP::LspUtils::enforceLspClearance(warpedLsp.data(), 16);

                // LSP から LPC 係数に逆変換
                DSP::LspUtils::lspToLpc(warpedLsp.data(), interpLpc.data(), 16);
            }
            else // LAR Mode
            {
                for (int i = 0; i < 16; ++i)
                {
                    mCurrentLarSmoothed[i] += mLarStep[i];
                }

                // LAR空間での双一次周波数ワーピング
                float alpha = std::tanh(currentFormantShift / 24.0f * 0.45f);
                float warpedLar[16] = {0.0f};

                float parcor[16] = {0.0f};
                DSP::LspUtils::larToParcor(mCurrentLarSmoothed.data(), parcor, 16);

                for (int i = 0; i < 16; ++i)
                {
                    float w = std::acos(std::clamp(parcor[i], -0.99f, 0.99f));
                    float sin_w = std::sin(w);
                    float cos_w = std::cos(w);
                    float warped_w = w + 2.0f * std::atan((alpha * sin_w) / (1.0f - alpha * cos_w + 1e-9f));
                    parcor[i] = std::cos(std::clamp(warped_w, 0.001f, 3.1415f));
                }

                DSP::LspUtils::parcorToLar(parcor, warpedLar, 16);
                
                // ワーピング後の LAR から LPC 係数に逆変換
                float warpedParcor[16] = {0.0f};
                DSP::LspUtils::larToParcor(warpedLar, warpedParcor, 16);
                DSP::LspUtils::parcorToLpc(warpedParcor, interpLpc.data(), 16);
            }

            // DspState の lpcCoeffs にロード
            for (int i = 0; i < 16; ++i)
            {
                float val = interpLpc[i];
                for (int v = 0; v < 8; ++v)
                {
                    mDspState.lpcCoeffs[i][v] = val;
                }
            }

            // LPC逆フィルタを回して残差 (Residual) 信号を抽出
            if (mLpcResidualBuffer.size() == 512 && mLpcAnalysisBuffer.size() == 512)
            {
                float residual = inSample;
                for (int i = 1; i <= 16; ++i)
                {
                    float prevInput = mLpcAnalysisBuffer[512 - i];
                    // 符号の整合性を確保： 逆フィルタ A(z) = 1 + sum(a_i z^-i) なので加算(+)とする
                    residual += interpLpc[i - 1] * prevInput;
                }
                
                std::memmove(mLpcResidualBuffer.data(), mLpcResidualBuffer.data() + 1, 511 * sizeof(float));
                mLpcResidualBuffer[511] = residual;
            }
        }

        // --- 有声/無声比率の計算 ---
        float targetUv = mIsVoiced ? 0.0f : 1.0f;
        mCurrentUnvoicedRatio = mCurrentUnvoicedRatio * 0.95f + targetUv * 0.05f;

        __m256 noiseBuffer = mNoiseGenerator.nextBlockAVX2();
        __m256 activeMask = mVoiceManager.getActiveVoicesMask();
        __m256 mixEnvelopes = mVoiceManager.getVoiceEnvelopes();

        float dynNoiseMix = std::clamp(noiseParam + (1.0f - noiseParam) * mCurrentUnvoicedRatio * 1.5f, 0.0f, 1.0f);
        __m256 noiseMixVec = _mm256_set1_ps(dynNoiseMix);

        // --- レンダリング処理 ---
        float mode0_L = 0.0f, mode0_R = 0.0f;
        float mode1_L = 0.0f, mode1_R = 0.0f;

        // Mode 0（Filterbank）の計算
        if (vocoderMode == 0 || isCrossfading)
        {
            mOscillatorBank.processSampleAVX2(
                mDspState, activeMask, mixEnvelopes, noiseMixVec, noiseBuffer,
                mBandEnvelopes.data(), currentFormantShift, stereoWidth, character,
                0, // vocoderMode = 0
                currentNumBands, waveform, pulseWidth, wavetablePosition,
                mBandCoeffsG.data(), mBandCoeffsK.data(), mBandCoeffsA1.data(), mBandCoeffsA1.data(),
                mode0_L, mode0_R
            );
        }

        // Mode 1（LPC）の計算
        if (vocoderMode == 1 || isCrossfading)
        {
            // SoA 状態へ残差信号を書き込み
            float resVal = mLpcResidualBuffer[511];
            for (int v = 0; v < 8; ++v)
            {
                mDspState.lpcResidual[v] = resVal;
            }

            mOscillatorBank.processSampleAVX2(
                mDspState, activeMask, mixEnvelopes, noiseMixVec, noiseBuffer,
                mBandEnvelopes.data(), currentFormantShift, stereoWidth, character,
                1, // vocoderMode = 1
                currentNumBands, waveform, pulseWidth, wavetablePosition,
                mBandCoeffsG.data(), mBandCoeffsK.data(), mBandCoeffsA1.data(), mBandCoeffsA1.data(),
                mode1_L, mode1_R
            );
        }

        if (sample16k < static_cast<int>(m16kWetBufferL.size()))
        {
            mMode0WetBufferL[sample16k] = mode0_L;
            mMode0WetBufferR[sample16k] = mode0_R;
            mMode1WetBufferL[sample16k] = mode1_L;
            mMode1WetBufferR[sample16k] = mode1_R;
        }
    }

    // 6. Wet信号のクロスフェード & RMSゲインマッチング
    if (num16kSamples > 0)
    {
        // 6-A. クロスフェードの適用 (ステレオL/R独立)
        for (int i = 0; i < num16kSamples; ++i)
        {
            float w0_L = mMode0WetBufferL[i];
            float w0_R = mMode0WetBufferR[i];
            float w1_L = mMode1WetBufferL[i];
            float w1_R = mMode1WetBufferR[i];
            
            float blend = mModeCrossfade.getNextValue();
            float targetCross = static_cast<float>(vocoderMode);
            mModeCrossfade.setTargetValue(targetCross);

            m16kWetBufferL[i] = (1.0f - blend) * w0_L + blend * w1_L;
            m16kWetBufferR[i] = (1.0f - blend) * w0_R + blend * w1_R;
        }

        // 6-B. RMS ゲインマッチング
        float sumSqIn = 0.0f;
        float sumSqWet = 0.0f;
        for (int i = 0; i < num16kSamples; ++i)
        {
            sumSqIn += mDownsampledBuffer[i] * mDownsampledBuffer[i];
            sumSqWet += (m16kWetBufferL[i] * m16kWetBufferL[i] + m16kWetBufferR[i] * m16kWetBufferR[i]) * 0.5f;
        }
        float rmsIn = std::sqrt(sumSqIn / num16kSamples);
        float rmsWet = std::sqrt(sumSqWet / num16kSamples);
        
        float targetGain = 1.0f;
        if (rmsWet > 1e-6f)
        {
            targetGain = rmsIn / rmsWet;
        }
        targetGain = std::clamp(targetGain, 0.05f, 15.0f); // ゲイン幅制限

        // アタック5ms(0.0124f)、リリース30ms(0.0021f)
        for (int i = 0; i < num16kSamples; ++i)
        {
            float diff = targetGain - mSmoothedGain;
            float coeff = (diff > 0.0f) ? 0.0124f : 0.0021f;
            mSmoothedGain += diff * coeff;

            // LPCモードのブレンド比率(0.0〜1.0)に応じてゲインマッチングを適用
            float blend = mModeCrossfade.getCurrentValue();
            float finalGain = (1.0f - blend) + blend * mSmoothedGain;

            m16kWetBufferL[i] *= finalGain;
            m16kWetBufferR[i] *= finalGain;
        }
    }

    // 7. 16kHz Wet信号をホストサンプリングレートへアップサンプリング
    juce::FloatVectorOperations::clear(mWetFsBufferL.data(), numSamples);
    juce::FloatVectorOperations::clear(mWetFsBufferR.data(), numSamples);
    if (numSamples > 0 && num16kSamples > 0)
    {
        double upRatio = static_cast<double>(num16kSamples) / static_cast<double>(numSamples);
        int maxWetFsSize = static_cast<int>(mWetFsBufferL.size());

        for (int i = 0; i < numSamples; ++i)
        {
            double pos = i * upRatio;
            int idx0 = static_cast<int>(pos);
            int idx1 = std::min(num16kSamples - 1, idx0 + 1);
            float frac = static_cast<float>(pos - idx0);

            if (i < maxWetFsSize)
            {
                mWetFsBufferL[i] = m16kWetBufferL[idx0] * (1.0f - frac) + m16kWetBufferL[idx1] * frac;
                mWetFsBufferR[i] = m16kWetBufferR[idx0] * (1.0f - frac) + m16kWetBufferR[idx1] * frac;
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

        float wetValL = mWetFsBufferL[sample];
        float wetValR = mWetFsBufferR[sample];

        if (mInputEnvelope < 0.00005f)
        {
            wetValL = 0.0f;
            wetValR = 0.0f;
        }

        float wetSampleL = wetValL;
        float wetSampleR = wetValR;

        // LPC Mode のステレオ全通 (APF) デコレレーター
        if (vocoderMode == 1)
        {
            float g_L = std::sqrt(1.0f - inputPan);
            float g_R = std::sqrt(inputPan);

            // 左側 APF (delay = 13)
            int rPtrL = (mApfWritePtrL - 13 + 64) % 64;
            float delayed_xL = mApfBufferL[rPtrL];
            float apfOutL = -0.6f * wetValL + delayed_xL;
            mApfBufferL[mApfWritePtrL] = wetValL + 0.6f * apfOutL;
            mApfWritePtrL = (mApfWritePtrL + 1) % 64;

            // 右側 APF (delay = 17)
            int rPtrR = (mApfWritePtrR - 17 + 64) % 64;
            float delayed_xR = mApfBufferR[rPtrR];
            float apfOutR = -0.6f * wetValR + delayed_xR;
            mApfBufferR[mApfWritePtrR] = wetValR + 0.6f * apfOutR;
            mApfWritePtrR = (mApfWritePtrR + 1) % 64;

            // 相関度合い C_LR に基づく非相関ステレオ化
            float beta = stereoWidth * std::sqrt(1.0f - inputCorrelation * inputCorrelation);
            wetSampleL = g_L * wetValL + beta * apfOutL;
            wetSampleR = g_R * wetValR - beta * apfOutR;
        }

        // Lo-Fi プロセッシング (左右独立して適用)
        if (character < 0.98f)
        {
            float bits = 4.0f + 20.0f * character;
            float steps = std::pow(2.0f, bits);

            int holdSamples = static_cast<int>(1.0f + 31.0f * (1.0f - character));
            int holdStartIdx = std::clamp((sample / holdSamples) * holdSamples, 0, numSamples - 1);

            // サンプルホールド (L/R 独立)
            wetSampleL = mWetFsBufferL[holdStartIdx];
            wetSampleR = mWetFsBufferR[holdStartIdx];

            // 量子化 (L/R 独立)
            wetSampleL = std::round(wetSampleL * steps) / steps;
            wetSampleR = std::round(wetSampleR * steps) / steps;
        }

        // MSダイナミック・クロス・マトリクス (全モード適用)
        {
            float mid = 0.5f * (wetSampleL + wetSampleR);
            float side = 0.5f * (wetSampleL - wetSampleR);

            // 80Hz 1次 HPF (Side成分のみ)
            float a1_hp = std::exp(-2.0f * 3.14159265f * 80.0f / static_cast<float>(srcSampleRate));
            float sideFiltered = side - mMsPrevInput + a1_hp * mMsFilterState;
            mMsPrevInput = side;
            mMsFilterState = sideFiltered;

            // 1.5ms 遅延 (Side成分のみ)
            int delaySamples = std::clamp(static_cast<int>(0.0015 * srcSampleRate), 1, 120);
            mMsDelayBuffer[mMsDelayWritePtr] = sideFiltered;
            int readPtr = (mMsDelayWritePtr - delaySamples + 128) % 128;
            float sideProcessed = mMsDelayBuffer[readPtr] * stereoWidth;
            mMsDelayWritePtr = (mMsDelayWritePtr + 1) % 128;

            // MSデコードして L/R に戻す
            wetSampleL = mid + sideProcessed;
            wetSampleR = mid - sideProcessed;
        }

        if (!std::isfinite(wetSampleL) || !std::isfinite(wetSampleR))
        {
            wetSampleL = 0.0f;
            wetSampleR = 0.0f;
        }

        float outValL = ((1.0f - mix) * drySampleL + mix * wetSampleL) * outputGain;
        float outValR = ((1.0f - mix) * drySampleR + mix * wetSampleR) * outputGain;

        if (limiterEnable)
        {
            float peak = std::max(std::abs(outValL), std::abs(outValR));
            float targetGain = 1.0f;
            if (peak > 0.99f)
            {
                targetGain = 0.99f / peak;
            }
            
            if (targetGain < mLimiterGain)
            {
                mLimiterGain = targetGain; // 即座にアタック
            }
            else
            {
                // ゆっくりとリリース (約150ms)
                mLimiterGain = mLimiterGain + (1.0f - mLimiterGain) * 0.00015f;
            }
            
            outValL *= mLimiterGain;
            outValR *= mLimiterGain;

            // 絶対的な安全クリッピング
            outValL = std::clamp(outValL, -0.99f, 0.99f);
            outValR = std::clamp(outValR, -0.99f, 0.99f);
        }

        CHECK_NAN(outValL, "outValL");
        CHECK_NAN(outValR, "outValR");

        if (writePointerL != nullptr) writePointerL[sample] = outValL;
        if (writePointerR != nullptr) writePointerR[sample] = outValR;
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