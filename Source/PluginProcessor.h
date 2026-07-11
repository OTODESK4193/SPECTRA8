#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <memory>

// DSPとボイスのヘッダー
#include "VoiceState.h"
#include "MidiQueue.h"
#include "PitchDetector.h"
#include "LPCAnalyzer.h"
#include "TrueEnvelope.h"
#include "BarkFilterBank.h"
#include "TELPCIntegrator.h"
#include "LPCtoLSP.h"
#include "LSPtoLPC.h"
#include "FormantShifter.h"
#include "MultiRateMapper.h"
#include "OscillatorBank.h"
#include "NoiseGenerator.h"
#include "CharacterProcessor.h"
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

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // DSPモジュール群
    DSP::MidiQueue mMidiQueue;
    DSP::VoiceManager mVoiceManager;
    DSP::OscillatorBank mOscillatorBank;
    DSP::NoiseGenerator mNoiseGenerator;
    DSP::CharacterProcessor mCharacterProcessor;
    
    // 分析モジュール (16kHzで動作)
    DSP::MultiRateMapper mMultiRateMapper;
    DSP::PitchDetector mPitchDetector;
    DSP::LPCAnalyzer mLpcAnalyzer;
    DSP::TrueEnvelope mTrueEnvelope;
    DSP::BarkFilterBank mBarkFilterBank;
    DSP::TELPCIntegrator mTelpcIntegrator;
    
    // 変換・フォルマント処理モジュール
    DSP::LPCtoLSP mLpcToLsp;
    DSP::LSPtoLPC mLspToLpc;
    DSP::FormantShifter mFormantShifter;

    // 8ボイス並列DSP状態
    alignas(32) DSP::PolyphonicVoiceSoA mDspState;

    // 分析データバッファ (16kHz領域)
    std::vector<float> mAnalysisInputBuffer; // ダウンサンプルされた16kHzの継続サンプル
    std::vector<float> mAnalysisFrame;       // 400サンプルの分析用フレーム
    
    // 中間データバッファ
    std::vector<float> mTeEnvelope;
    std::vector<float> mBarkEnergies;
    std::vector<float> m16kLpc;
    std::vector<float> m16kLsp;
    
    // LSP 補間バッファ (ホストサンプリングレート Fs の 24次)
    std::vector<float> mLspCurrent;
    std::vector<float> mLspTarget;
    std::vector<float> mLspInterpolated;
    std::vector<float> mLspShifted;
    std::vector<float> mFsLpc;

    // 分析タイミング制御 (16kHz で 100サンプルホップ = 6.25ms毎)
    int mAnalysisHopSize;
    int mAnalysisWindowSize;
    
    // コントロール・レート制御 (ホストSRで 32サンプル毎にLSP補間)
    int mControlRateBlockSize;
    int mControlRateCounter;
    float mInterpolationBeta; // 0.0f 〜 1.0f
    
    // 前回のLPCゲイン（スカラー）
    float mCurrentGain;
    float mTargetGain;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SPECTRA8AudioProcessor)
};
