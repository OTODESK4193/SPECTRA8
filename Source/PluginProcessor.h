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
#include "DSP/AirBand.h"
#include "DSP/FxChain.h"
#include "DSP/AnalyzerDSP.h"
#include "DSP/ResampleFilter.h"

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

    // 下部ステータス行の既定表示。
    //  ノブ/コンボにマウスを乗せている間は、エディター側がここを
    //  そのコントロールの英語説明文で上書きする (PluginEditor::timerCallback)。
    //  ※ InLvl 表示は廃止 (2026-08-01)。説明文の表示スペースに充てる。
    juce::String getDebugMessage() const
    {
        const int vMode = static_cast<int>(apvts.getRawParameterValue("vocoderMode")->load());
        const int voicing = static_cast<int>(apvts.getRawParameterValue("mode")->load());
        return juce::String(vMode == 0 ? "Filterbank" : "LPC")
             + " / " + juce::String(voicing == 0 ? "Auto" : "MIDI")
             + "   -   hover a control for help";
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

    // ---- GUI サイズ管理 (Ambience準拠) ----
    int getSavedEditorWidth() const noexcept { return savedEditorWidth; }
    int getSavedEditorHeight() const noexcept { return savedEditorHeight; }
    void setSavedEditorSize(int w, int h) noexcept
    {
        savedEditorWidth = w;
        savedEditorHeight = h;
    }

    // ---- モジュレーション ----
    // GUI(アークの変調レンジ帯表示)から参照する。
    const ModMatrix& getModMatrix() const noexcept { return mModMatrix; }

    // 宛先IDを渡すだけで「変調適用済みの実パラメータ値」が返る。
    //  パラメータID・スケール・掛かり方(線形/オクターブ)はすべてModMatrix側の
    //  定義に従うため、DSPとGUI表示でスケールがズレる余地が無い。
    //  【RT安全性】getRawParameterValue() は文字列キーのハッシュ検索なので、
    //  オーディオスレッドから毎回呼ぶと重い(制御ティック毎に約20回=1万回/秒)。
    //  prepareToPlay で宛先IDごとにポインタを引いてキャッシュしておく。
    float moddedParam(int dst) const noexcept
    {
        const int d = juce::jlimit(0, (int)ModMatrix::NumDsts - 1, dst);
        auto* p = mDestPtrs[(size_t)d];
        if (p == nullptr)
            return 0.0f;
        return ModMatrix::applyMod(d, p->load(), mModMatrix.get(d));
    }

    // 押鍵リストの維持 (FIFO先入れ先出し・最大8音、9音目は最古消去)
    void addHeldNote(int note) noexcept
    {
        for (int i = 0; i < mNumHeldNotes; ++i)
            if (mHeldNotes[(size_t)i] == note) return;      // 既に押されている

        if (mNumHeldNotes < (int)mHeldNotes.size())
        {
            mHeldNotes[(size_t)mNumHeldNotes] = note;
            ++mNumHeldNotes;
        }
        else
        {
            // 8音満杯時に9音目が来たら最初の音(0番目)を消して9音目を末尾(8音目)にする
            for (size_t i = 0; i < mHeldNotes.size() - 1; ++i)
                mHeldNotes[i] = mHeldNotes[i + 1];
            mHeldNotes[mHeldNotes.size() - 1] = note;
        }
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

    // GUI インフォバー用: 現在保持されている受信ノートの音名一覧文字列を取得
    juce::String getHeldNotesText() const
    {
        if (mNumHeldNotes <= 0)
            return "MIDI: --";

        juce::String text = "MIDI: ";
        static const char* kNoteNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        for (int i = 0; i < mNumHeldNotes; ++i)
        {
            const int n = mHeldNotes[(size_t)i];
            if (n >= 0 && n <= 127)
            {
                if (i > 0) text += " ";
                text += kNoteNames[n % 12];
            }
        }
        return text;
    }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ---- パラメータポインタのキャッシュ (オーディオスレッドからの文字列検索を排除) ----
    //  旧実装は制御ティック毎に "slot0src" のような juce::String を組み立てており、
    //  毎秒 5,500 回のヒープ確保がオーディオスレッドで走っていた (RT安全性違反)。
    void cacheParamPointers();
    // APVTS から ModMatrix::Params を充填する (制御ティック毎 / アイドル時の両方から呼ぶ)
    void loadModParams(double bpm) noexcept;

    struct SlotPtrs { std::atomic<float>* src = nullptr; std::atomic<float>* dst = nullptr;
                      std::atomic<float>* amt = nullptr; std::atomic<float>* uni = nullptr; };
    struct LfoPtrs  { std::atomic<float>* rate = nullptr; std::atomic<float>* sync = nullptr;
                      std::atomic<float>* rateSync = nullptr; std::atomic<float>* wave = nullptr; };
    struct EnvPtrs  { std::atomic<float>* attack = nullptr; std::atomic<float>* decay = nullptr;
                      std::atomic<float>* sustain = nullptr; std::atomic<float>* release = nullptr;
                      std::atomic<float>* loop = nullptr; };
    struct FxSlotPtrs { std::atomic<float>* type = nullptr; std::atomic<float>* amount = nullptr; };

    // ---- 全変調宛先の1極平滑 (ジッパーノイズ対策) ----------------------
    //  制御ティックは2ms毎なので、平滑しないと RESONANCE / CHARACTER / M.PITCH /
    //  NOISE COLOR などは 2ms 刻みの階段状に動き、フィルタ係数やオシレータ周波数が
    //  跳んで「ジリジリ」「プチプチ」が乗る。16kHzループの毎サンプルで進める。
    //  τ≈5ms (係数 = 1-exp(-1/(0.005*16000)))。宛先28個 × 16kHz = 448k演算/秒で軽い。
    static constexpr float kParamSmCoef = 0.0124f;
    std::array<float, ModMatrix::NumDsts> mParamSm {};
    bool mParamSmPrimed = false;   // 初回は平滑せず即値で埋める

    void advanceParamSmoothers() noexcept
    {
        if (!mParamSmPrimed)
        {
            for (int d = 0; d < (int)ModMatrix::NumDsts; ++d)
                mParamSm[(size_t)d] = moddedParam(d);
            mParamSmPrimed = true;
            return;
        }
        for (int d = 1; d < (int)ModMatrix::NumDsts; ++d)   // 0 = DstNone
            mParamSm[(size_t)d] += kParamSmCoef * (moddedParam(d) - mParamSm[(size_t)d]);
    }

    // 平滑済みの実効パラメータ値
    float smoothedParam(int dst) const noexcept
    {
        return mParamSm[(size_t)juce::jlimit(0, (int)ModMatrix::NumDsts - 1, dst)];
    }

    std::array<std::atomic<float>*, ModMatrix::NumDsts> mDestPtrs {};
    std::array<SlotPtrs, ModMatrix::kNumSlots> mSlotPtrs {};
    std::array<LfoPtrs,  ModMatrix::kNumLfos>  mLfoPtrs {};
    std::array<EnvPtrs,  ModMatrix::kNumEnvs>  mEnvPtrs {};
    std::array<FxSlotPtrs, FxChain::kNumSlots> mFxPtrs {};

    // モジュールインスタンス
    FilterbankVocoder mFilterbankVocoder;
    LpcVocoder mLpcVocoder;               // フェーズ2: LPCモード
    PostBandEq mPostEq;                   // フェーズ2 M3: LPC出力へBANDS EQをポスト適用
    ExcitationEngine mExcitationEngine;
    ModMatrix mModMatrix;
    AirBand mAirBand;   // 8kHz以上のエアバンド合成 (内部16kHzで失われる帯域の補完)
    float mAirSm = -1.0f;      // AIR量のサンプル単位平滑 (-1 = 未初期化)
    float mAirSmCoef = 0.0f;   // τ=20ms 相当。prepareToPlay で算出
    FxChain mFxChain;                     // 後段FX (5スロット直列)
    AnalyzerDSP mAnalyzer;                // 表示専用 (バックグラウンドスレッド)
    std::vector<float> mAnalyzerMono;     // 解析へ渡すモノラル和 (事前確保・RT安全)

    // MIXブレンド用の原音退避。
    //  MIX=0 で「FXも含めて完全バイパスした素のDry」を出すために、
    //  FX適用前のドライ信号を別に持っておく必要がある。
    std::vector<float> mDryL, mDryR;

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

    // ダウンサンプリング用ガードサンプル: 前ブロック末尾のAA済みサンプル。
    //  ブロック末尾でidx0+1がブロック外になる場合の補間に使用する。
    float mDownGuard = 0.0f;

    std::vector<float> mDownsampledBuffer;
    std::vector<float> m16kWetL;
    std::vector<float> m16kWetR;

    // 16kHz 内部処理のための帯域制限フィルタ (ResampleFilter.h の説明を参照)。
    //  mAaIn   : デシメーション前の入力に掛けるアンチエイリアス (モノ=解析用ch0のみ)
    //  mAiOutL/R: 補間後のウェット出力に掛けるアンチイメージング
    ResampleFilter mAaIn;
    ResampleFilter mAiOutL, mAiOutR;
    std::vector<float> mAaInBuf;   // フィルタ済み入力 (原音を壊さないよう別バッファ)

    int mControlRateCounter = 0;

    // ---- 入力ゲート (Autoモードで無入力時の残留ノイズを切る) ----
    //  【重要】旧実装は入力エンベロープもゲートゲインも「ブロックに1回」しか
    //  更新せず、その値をブロック全体に一律で掛けていた。
    //  入力レベルがソフトゲートの傾斜部にあると gateGain がブロック毎に階段状に変わり、
    //  ウェット信号がバッファレート(44.1kHz/1024smp なら 43Hz)で振幅変調される。
    //  その結果キャリアの全倍音の両脇に ±43Hz のサイドバンドが立ち、
    //  「ジリジリ」という常時ノイズになっていた (実測で確認)。
    //  → エンベロープもゲートもサンプル単位で更新・平滑する。
    //  さらに、しきい値の傾斜(ソフトニー)でゲインを連続的に動かす方式そのものが、
    //  入力レベルがしきい値付近にあると数秒周期のポンピングを生む。
    //  → 開/閉で別のしきい値を持つ「ヒステリシス付きのON/OFF」に変更し、
    //    通常の演奏レベルでは常に全開のまま一切動かないようにする。
    float mInEnvSm = 0.0f;        // 入力レベルの追従値 (ホストレート)
    float mGateSm  = 0.0f;        // 平滑後のゲートゲイン (0..1)
    bool  mGateOpen = false;      // ヒステリシスの状態
    float mEnvAttCoef = 0.0f;     // 入力エンベロープ アタック (τ=20ms)
    float mEnvRelCoef = 0.0f;     // 同 リリース (τ=300ms)
    float mGateOpenCoef  = 0.0f;  // 開くときのランプ (τ=15ms)
    float mGateCloseCoef = 0.0f;  // 閉じるときのランプ (τ=150ms・余韻を切らない)

    // ゲートのしきい値 (mean|x| 相当)。通常の演奏は -40dBFS 以上あるので、
    // -58dBFS で開き -68dBFS で閉じる設定なら演奏中は絶対に動かない。
    static constexpr float kGateOpenTh  = 0.0012f;   // ≈ -58 dBFS
    static constexpr float kGateCloseTh = 0.0004f;   // ≈ -68 dBFS
    // 入力が無い/ブロックが極小で16kサンプルが生成されないときに、
    // それでもLFO/ENVを進めるための16kHz換算の経過サンプル数アキュムレータ。
    double mIdleModAccum = 0.0;
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

    // GUI サイズ管理 (Ambience準拠)
    int savedEditorWidth{ 780 };
    int savedEditorHeight{ 417 };

    JUCE_DECLARE_WEAK_REFERENCEABLE(SPECTRA8AudioProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SPECTRA8AudioProcessor)
};