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

    juce::String getDebugMessage() const { return mDebugMessage; }

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
    
    // ホストSR用のFFT/IFFTオブジェクト (サイズ2048 = 11次)
    std::unique_ptr<juce::dsp::FFT> mFsFft;
    std::vector<float> mFsEnvelope;       // ホストSR用スペクトル包絡 (1025点)
    std::vector<float> mAutocorrBuffer;   // 自己相関用IFFTバッファ (2048点)
    std::vector<float> mFsLpc;            // ホストSRのLPC係数 (25点)
    std::vector<float> mFsLpcTarget;      // 補間ターゲットLPC係数 (25点)

    // 分析タイミング制御 (16kHz で 100サンプルホップ = 6.25ms毎)
    int mAnalysisHopSize;
    int mAnalysisWindowSize;
    
    // コントロール・レート制御 (ホストSRで 32サンプル毎にLPC更新)
    int mControlRateBlockSize;
    int mControlRateCounter;
    
    // 前回のLPCゲイン（スカラー）
    float mCurrentGain;
    float mTargetGain;

    bool mMidiActiveMode = false;
    float mInputEnvelope = 0.0f;
    float mCurrentF0 = 150.0f;

    std::unique_ptr<juce::dsp::FFT> mAnalysisFft; // オーディオコールバック内でのメモリ確保を防ぐためメンバ化

    juce::String mDebugMessage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SPECTRA8AudioProcessor)
};
