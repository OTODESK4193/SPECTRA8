#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <memory>
#include <atomic>

// DSPとボイスのヘッダー
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

    // パラメータアクセス用の APVTS
    juce::AudioProcessorValueTreeState apvts;

    juce::String getDebugMessage() const
    {
        int state = mErrorState.load();
        if (state == 1) return "ERR: NaN/Inf detected!";
        return "No errors. Running fine.";
    }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // DSPモジュール群
    DSP::MidiQueue mMidiQueue;
    DSP::VoiceManager mVoiceManager;
    DSP::OscillatorBank mOscillatorBank;
    DSP::NoiseGenerator mNoiseGenerator;

    // 8ボイス並列DSP状態
    alignas(32) DSP::PolyphonicVoiceSoA mDspState;

    // 48バンド ZDF SVF 状態変数 (16kHz動作)
    std::vector<float> mBandEnvelopes;       // 48バンドの現在のエンベロープ
    std::vector<float> mTargetBandEnvelopes; // 48バンドの目標エンベロープ

    // 分析側のZDF SVF状態変数（48バンド * 各2セクション分）
    std::vector<float> mAnalFilterS1;
    std::vector<float> mAnalFilterS2;

    // フィルタバンク中心周波数
    std::vector<float> mBandF0;

    // ZDF SVF フィルタ係数配列 (最大サイズ48)
    std::vector<float> mBandCoeffsG;
    std::vector<float> mBandCoeffsK;
    std::vector<float> mBandCoeffsA1;
    std::vector<float> mBandCoeffsA2;

    juce::LinearSmoothedValue<float> mFormantShiftSmoother;

    // リアルタイム安全な事前確保バッファ
    std::vector<float> mDownsampledBuffer;
    std::vector<float> m16kWetBuffer;
    std::vector<float> mWetFsBuffer;
    std::vector<float> mDryLBuffer;

    // 分析タイミング制御 (16kHz領域)
    int mAnalysisHopSize;
    int mAnalysisWindowSize;

    // コントロール・レート制御 (16kHz領域で 32サンプル毎にエンベロープ更新)
    int mControlRateBlockSize;
    int mControlRateCounter;

    // 有声/無声 (Voiced/Unvoiced) 動的ブレンド比率
    float mCurrentUnvoicedRatio;
    float mTargetUnvoicedRatio;

    bool mMidiActiveMode = false;
    float mInputEnvelope = 0.0f;
    double mDownsampleTimeAccum = 0.0;
    double mStoredSampleRate = 0.0;

    // ★フェイルセーフ：Autoモード時のボイスONエッジ検出用フラグ
    bool mWasAutoVoiceActive = false;

    std::atomic<int> mErrorState{ 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SPECTRA8AudioProcessor)
};