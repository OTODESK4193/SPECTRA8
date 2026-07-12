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
        int errState = mErrorState.load();
        if (errState == 1) return "ERR: NaN/Inf detected! (Muted)";

        float inEnv = mInputEnvelope;
        int activeVoices = mVoiceManager.getNumActiveVoices();

        juce::String msg = "Status: ";
        if (inEnv < 0.0001f)
        {
            msg += "No Input (Dry Only) | ";
        }
        else
        {
            msg += "InLvl: " + juce::String(inEnv * 100.0f, 2) + "% | ";
        }

        if (activeVoices == 0)
        {
            msg += "NO ACTIVE VOICES";
        }
        else
        {
            msg += "Voices Active: " + juce::String(activeVoices);
        }

        int vMode = static_cast<int>(apvts.getRawParameterValue("vocoderMode")->load());
        msg += " [" + juce::String(vMode == 0 ? "Filterbank" : "LPC") + "]";

        return msg;
    }

    // Band EQ およびアナライザー用メソッド
    void setBandGain(int bandIdx, float gain) { mBandGains[bandIdx].store(gain); }
    float getBandGain(int bandIdx) const { return mBandGains[bandIdx].load(); }
    float getBandLevel(int bandIdx) const { return mBandLevelsForUi[bandIdx].load(); }

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
    std::vector<float> m16kWetBufferL;
    std::vector<float> m16kWetBufferR;
    std::vector<float> mWetFsBufferL;
    std::vector<float> mWetFsBufferR;
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

    // ★フェイルセーフ：Autoモード時のボイスONエッジ検出用フラグ
    bool mWasAutoVoiceActive = false;

    // LPC / LSP 分析・合成用
    std::vector<float> mLpcAnalysisBuffer;
    std::vector<float> mCurrentLpcCoeffs;
    std::vector<float> mCurrentLsp;
    std::vector<float> mCurrentLspSmoothed;
    std::vector<float> mLspStep;
    std::vector<float> mFrozenLsp;
    std::vector<float> mCurrentLar;
    std::vector<float> mCurrentLarSmoothed;
    std::vector<float> mLarStep;
    std::vector<float> mFrozenLar;
    bool mFormantFreezeActive = false;

    // ピッチ検出 & V/UV判定用
    float mCurrentPitchHz = 130.0f;
    float mTargetPitchHz = 130.0f;
    float mPitchSmoothed = 130.0f;
    bool mIsVoiced = false;
    int mVoicedDebounceCounter = 0;
    std::vector<float> mPitchHistory;

    // 残差信号 (Residual) バッファ
    std::vector<float> mLpcResidualBuffer;

    // アルゴリズム切り替えクロスフェード
    juce::LinearSmoothedValue<float> mModeCrossfade;
    int mPrevVocoderMode = -1;
    std::vector<float> mMode0WetBufferL;
    std::vector<float> mMode0WetBufferR;
    std::vector<float> mMode1WetBufferL;
    std::vector<float> mMode1WetBufferR;

    // Band EQ およびアナライザーレベル
    std::array<std::atomic<float>, 48> mBandGains;
    std::array<std::atomic<float>, 48> mBandLevelsForUi;

    // 最終段リミッター用状態
    float mSmoothedGain = 1.0f;
    float mLimiterGain = 1.0f;

    // MSクロスマトリクスおよびデコレレーター用状態
    float mMsFilterState = 0.0f;
    float mMsPrevInput = 0.0f;
    std::vector<float> mMsDelayBuffer;
    int mMsDelayWritePtr = 0;

    std::vector<float> mApfBufferL;
    std::vector<float> mApfBufferR;
    int mApfWritePtrL = 0;
    int mApfWritePtrR = 0;

    std::atomic<int> mErrorState{ 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SPECTRA8AudioProcessor)
};