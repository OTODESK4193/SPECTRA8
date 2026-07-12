// ==========================================
// File: PluginProcessor.h
// SPECTRA8 プロセッサー層 (フェーズ1軽量化設計)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <atomic>
#include <array>

// 新モジュール
#include "DSP/FilterbankVocoder.h"
#include "DSP/ExcitationEngine.h"
#include "DSP/ModMatrix.h"
#include "DSP/PitchTracker.h"
#include "DSP/Limiter.h"

class SPECTRA8AudioProcessor : public juce::AudioProcessor 
{
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
        juce::String msg = "Status: ";
        const float inEnv = mInputEnvelope.load();
        if (inEnv < 0.0001f)
            msg += "Idle | ";
        else
            msg += "InLvl: " + juce::String(inEnv * 100.0f, 1) + "% | ";

        int vMode = static_cast<int>(apvts.getRawParameterValue("vocoderMode")->load());
        msg += "[" + juce::String(vMode == 0 ? "Filterbank" : "LPC (Phase2)") + "]";

        return msg;
    }

    // Band EQ API (UIとの橋渡し)
    std::array<std::atomic<float>, 48>& getBandGains() { return mBandGains; }
    const std::array<std::atomic<float>, 48>& getBandLevelsForUi() const { return mBandLevelsForUi; }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // モジュールインスタンス
    FilterbankVocoder mFilterbankVocoder;
    ExcitationEngine mExcitationEngine;
    ModMatrix mModMatrix;
    PitchTracker mPitchTracker;
    BrickLimiter mLimiter;

    // バンドEQデータ (UIおよびDSP共有)
    std::array<std::atomic<float>, 48> mBandGains;
    std::array<std::atomic<float>, 48> mBandLevelsForUi;

    // パラメータ同期用のモジュレーションマトリクス値保持バッファ
    ModMatrix::Params mModParams;

    // 16kHzダウンサンプリング/アップサンプリング用状態
    double mStoredSampleRate = 44100.0;
    double mDownsampleTimeAccum = 0.0;
    double mUpsampleTimeAccum = 0.0;

    std::vector<float> mDownsampledBuffer;
    std::vector<float> m16kWetL;
    std::vector<float> m16kWetR;

    int mControlRateCounter = 0;
    std::atomic<float> mInputEnvelope { 0.0f };
    int mPrevMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SPECTRA8AudioProcessor)
};