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
#include "DSP/LpcVocoder.h"
#include "DSP/PostBandEq.h"
#include "DSP/ExcitationEngine.h"
#include "DSP/ModMatrix.h"
#include "DSP/PitchTracker.h"
#include "DSP/Limiter.h"
#include "DSP/FxChain.h"
#include "DSP/AnalyzerDSP.h"

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

    bool mIsInitialized = false;
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
        msg += "[" + juce::String(vMode == 0 ? "Filterbank" : "LPC") + "]";

        return msg;
    }

    // Band EQ API (UIとの橋渡し)
    std::array<std::atomic<float>, 48>& getBandGains() { return mBandGains; }
    const std::array<std::atomic<float>, 48>& getBandLevelsForUi() const { return mBandLevelsForUi; }

    // 高精度アナライザー (BANDS EQ の背景表示用)。
    // プラグイン最終出力を投入しているので「実際に聞こえている音」のスペクトルになる。
    const AnalyzerDSP& getAnalyzer() const noexcept { return mAnalyzer; }

    // ---- カスタムWavetable (メッセージスレッド専用) ----
    // wav/aiff を読み込み 2048smp/フレームのウェーブテーブルとしてエンジンへ適用。
    // 成功時はパスを apvts.state に保存 (セッション復元用)。
    bool loadCustomWavetable(const juce::File& file);
    void clearCustomWavetable();
    juce::String getCustomWavetablePath() const
    {
        return apvts.state.getProperty("customWavetablePath", juce::String()).toString();
    }
    bool hasCustomWavetable() const { return mExcitationEngine.getWavetable().hasCustom(); }
    const ExcitationEngine& getExcitationEngine() const { return mExcitationEngine; }

    // ---- グローバル設定 (プラグイン全体で共有。DAW再起動・新規インスタンスでも保持) ----
    //  セッション/プリセット (apvts.state) とは独立した設定ファイルに保存される。
    //   Windows: %APPDATA%/SPECTRA8/SPECTRA8.settings
    //   macOS  : ~/Library/Application Support/SPECTRA8/SPECTRA8.settings
    //  Wavetableフォルダの登録パスはここに置く (毎回ADD DIRし直さなくて済むように)。
    static juce::PropertiesFile& getGlobalSettings();
    static juce::String getGlobalWavetableDir();
    static void setGlobalWavetableDir(const juce::String& path);

    // ---- モジュレーション ----
    // GUI(アークの変調レンジ帯表示)から参照する。
    const ModMatrix& getModMatrix() const noexcept { return mModMatrix; }

    // 宛先IDを渡すだけで「変調適用済みの実パラメータ値」が返る。
    //  パラメータID・スケール・掛かり方(線形/オクターブ)はすべてModMatrix側の
    //  定義に従うため、DSPとGUI表示でスケールがズレる余地が無い。
    float moddedParam(int dst) const noexcept
    {
        const float base = apvts.getRawParameterValue(ModMatrix::destParamId(dst))->load();
        return ModMatrix::applyMod(dst, base, mModMatrix.get(dst));
    }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // 押鍵リストの維持 (低い順・重複なし・最大8音)
    void addHeldNote(int note) noexcept
    {
        for (int i = 0; i < mNumHeldNotes; ++i)
            if (mHeldNotes[(size_t)i] == note) return;      // 既に押されている
        if (mNumHeldNotes >= (int)mHeldNotes.size()) return; // 満杯なら無視
        int pos = mNumHeldNotes;
        while (pos > 0 && mHeldNotes[(size_t)pos - 1] > note)
        {
            mHeldNotes[(size_t)pos] = mHeldNotes[(size_t)pos - 1];
            --pos;
        }
        mHeldNotes[(size_t)pos] = note;
        ++mNumHeldNotes;
    }
    void removeHeldNote(int note) noexcept
    {
        for (int i = 0; i < mNumHeldNotes; ++i)
            if (mHeldNotes[(size_t)i] == note)
            {
                for (int j = i; j < mNumHeldNotes - 1; ++j)
                    mHeldNotes[(size_t)j] = mHeldNotes[(size_t)j + 1];
                --mNumHeldNotes;
                return;
            }
    }

    // モジュールインスタンス
    FilterbankVocoder mFilterbankVocoder;
    LpcVocoder mLpcVocoder;               // フェーズ2: LPCモード
    PostBandEq mPostEq;                   // フェーズ2 M3: LPC出力へBANDS EQをポスト適用
    ExcitationEngine mExcitationEngine;
    ModMatrix mModMatrix;
    FxChain mFxChain;                     // 後段FX (5スロット直列)
    AnalyzerDSP mAnalyzer;                // 表示専用 (バックグラウンドスレッド)
    std::vector<float> mAnalyzerMono;     // 解析へ渡すモノラル和 (事前確保・RT安全)

    // FX Resonator の MIDI モード用。押鍵中のノート番号を低い順に保持する。
    // (ExcitationEngineのボイスはスチール式で消えることがあるため、FX用に別管理)
    std::array<int, 8> mHeldNotes {};
    int mNumHeldNotes = 0;
    PitchTracker mPitchTracker;
    BrickLimiter mLimiter;

    // ボコーダーモード切替 (30ms等パワークロスフェード @16kHz)
    static constexpr int kVocXfadeLen = 480;
    int mCurVocoderMode = -1;
    int mVocXfadeRemaining = 0;

    // バンドEQデータ (UIおよびDSP共有)
    std::array<std::atomic<float>, 48> mBandGains;
    std::array<std::atomic<float>, 48> mBandLevelsForUi;

    // パラメータ同期用のモジュレーションマトリクス値保持バッファ
    ModMatrix::Params mModParams;

    // カスタムWavetableファイル読み込み用
    juce::AudioFormatManager mFormatManager;

    // 16kHzダウンサンプリング/アップサンプリング用状態
    double mStoredSampleRate = 44100.0;
    double mDownsampleTimeAccum = 0.0;
    double mUpsampleTimeAccum = 0.0;

    std::vector<float> mDownsampledBuffer;
    std::vector<float> m16kWetL;
    std::vector<float> m16kWetR;

    int mControlRateCounter = 0;
    alignas(8) std::atomic<float> mInputEnvelope { 0.0f };
    int mPrevMode = -1;
    float mLastVoicedPitch = 130.0f;
    float mVoicedSmooth = 0.0f;   // 平滑化した有声度(0..1)。LPCのNOISE± 自動V/UV用

    // Tracking用ピッチ平滑 (log2領域1次平滑、Responseパラメータでτ切替)
    float mPitchLogSmooth = -1.0f;   // <0 = 未初期化
    float mPitchSmoothCoef = 0.02f;  // ブロック毎にτから再計算

    // PITCH Q ヒステリシス用: 現在保持中のスナップ先ノート (-1=未保持)
    int mQuantNoteHeld = -1;

    // パラメータ・スムージング (ジッパーノイズ対策)
    juce::LinearSmoothedValue<float> mMixSm;      // MIX (ホストレート, 20ms)
    juce::LinearSmoothedValue<float> mOutGainSm;  // OUT LEVEL リニアゲイン (同上)
    float mFmtShiftSm = 0.0f;     // FMT SHIFT (16k一次平滑 τ≈5ms)
    float mFmtStretchSm = 1.0f;   // FMT STRETCH (同上)

    JUCE_DECLARE_WEAK_REFERENCEABLE(SPECTRA8AudioProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SPECTRA8AudioProcessor)
};