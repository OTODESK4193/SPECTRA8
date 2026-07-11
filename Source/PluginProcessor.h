#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <memory>
#include <atomic>

#include "VoiceState.h"
#include "MidiQueue.h"
#include "OscillatorBank.h"
#include "NoiseGenerator.h"
#include "VoiceManager.h"

class SPECTRA8AudioProcessor : public juce::AudioProcessor {
public:
    SPECTRA8AudioProcessor();
    ~SPECTRA8AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SPECTRA8"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    juce::String getDebugMessage() const
    {
        int state = mErrorState.load();
        if (state == 1) return "ERR: NaN/Inf detected!";
        return "No errors. Running fine.";
    }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    DSP::MidiQueue mMidiQueue;
    DSP::VoiceManager mVoiceManager;
    DSP::OscillatorBank mOscillatorBank;
    DSP::NoiseGenerator mNoiseGenerator;

    alignas(32) DSP::PolyphonicVoiceSoA mDspState;

    std::vector<float> mBandEnvelopes;
    std::vector<float> mTargetBandEnvelopes;

    std::vector<float> mAnalFilterS1;
    std::vector<float> mAnalFilterS2;

    std::vector<float> mBandF0;

    std::vector<float> mBandCoeffsG;
    std::vector<float> mBandCoeffsK;
    std::vector<float> mBandCoeffsA1;

    juce::LinearSmoothedValue<float> mFormantShiftSmoother;

    // リアルタイム安全な事前確保バッファ
    std::vector<float> mDownsampledBuffer;
    std::vector<float> m16kWetBuffer;
    std::vector<float> mWetFsBuffer;
    std::vector<float> mDryLBuffer;

    int mAnalysisHopSize;
    int mAnalysisWindowSize;

    int mControlRateBlockSize;
    int mControlRateCounter;

    float mCurrentUnvoicedRatio;
    float mTargetUnvoicedRatio;

    bool mMidiActiveMode = false;
    float mInputEnvelope = 0.0f;
    double mDownsampleTimeAccum = 0.0;
    double mStoredSampleRate = 0.0;

    bool mWasAutoVoiceActive = false;

    std::atomic<int> mErrorState{ 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SPECTRA8AudioProcessor)
};