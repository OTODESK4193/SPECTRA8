// ==========================================
// File: PluginProcessor.cpp
// SPECTRA8 プロセッサー層 (フェーズ1軽量化設計)
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

// ---- グローバル設定ファイル ----
//  ホスト側のセッション保存とは無関係に、ユーザー設定フォルダへ永続化する。
//  静的ローカルのコンストラクタで初期化することで初期化の競合を避ける。
namespace
{
    struct GlobalSettingsHolder
    {
        juce::ApplicationProperties props;
        GlobalSettingsHolder()
        {
            juce::PropertiesFile::Options o;
            o.applicationName     = "SPECTRA8";
            o.filenameSuffix      = "settings";
            o.folderName          = "SPECTRA8";
            o.osxLibrarySubFolder = "Application Support";
            o.storageFormat       = juce::PropertiesFile::storeAsXML;
            props.setStorageParameters(o);
        }
    };
}

juce::PropertiesFile& SPECTRA8AudioProcessor::getGlobalSettings()
{
    static GlobalSettingsHolder holder;
    return *holder.props.getUserSettings();
}

juce::String SPECTRA8AudioProcessor::getGlobalWavetableDir()
{
    return getGlobalSettings().getValue("customWavetableDir", juce::String());
}

void SPECTRA8AudioProcessor::setGlobalWavetableDir(const juce::String& path)
{
    auto& s = getGlobalSettings();
    s.setValue("customWavetableDir", path);
    s.saveIfNeeded();   // 即時ディスク書き込み (DAWが異常終了しても残るように)
}

SPECTRA8AudioProcessor::SPECTRA8AudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    // EQゲインの初期値 1.0 (0dB) に設定
    for (int i = 0; i < 48; ++i)
    {
        mBandGains[(size_t)i].store(1.0f);
        mBandLevelsForUi[(size_t)i].store(0.0f);
    }
    mFormatManager.registerBasicFormats();   // wav/aiff リーダー
    mIsInitialized = true;
}

// ---- カスタムWavetable ----
bool SPECTRA8AudioProcessor::loadCustomWavetable(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader(mFormatManager.createReaderFor(file));
    if (reader == nullptr)
        return false;

    // 最大 64フレーム(2048smp/フレーム) まで読み込み・モノラルミックス
    const int maxLen = MorphWavetable::kMaxCustomFrames * MorphWavetable::kTableSize;
    const int len = (int)juce::jmin<juce::int64>(reader->lengthInSamples, (juce::int64)maxLen);
    if (len < 16)
        return false;

    juce::AudioBuffer<float> buf((int)reader->numChannels, len);
    if (!reader->read(&buf, 0, len, 0, true, true))
        return false;

    std::vector<float> mono((size_t)len, 0.0f);
    const float chScale = 1.0f / (float)juce::jmax(1u, (unsigned)reader->numChannels);
    for (int ch = 0; ch < (int)reader->numChannels; ++ch)
    {
        const float* src = buf.getReadPointer(ch);
        for (int n = 0; n < len; ++n)
            mono[(size_t)n] += src[n] * chScale;
    }

    if (!mExcitationEngine.getWavetable().loadCustomFromBuffer(mono.data(), len))
        return false;

    apvts.state.setProperty("customWavetablePath", file.getFullPathName(), nullptr);
    return true;
}

void SPECTRA8AudioProcessor::clearCustomWavetable()
{
    mExcitationEngine.getWavetable().clearCustom();
    apvts.state.removeProperty("customWavetablePath", nullptr);
}

SPECTRA8AudioProcessor::~SPECTRA8AudioProcessor()
{
    mIsInitialized = false;
}

juce::AudioProcessorValueTreeState::ParameterLayout SPECTRA8AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // --- ボコーダー基本パラメータ ---
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

    // BPF Bank のバンド幅スケール (バンド間隔連動Qに乗算)。1.0=標準(中域で旧Q=10相当)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("resonance", 1), "Resonance",
        juce::NormalisableRange<float>(0.3f, 3.0f, 0.0f, 0.5f), 1.0f));

    // Filterbank の帯域交互パンニング幅。
    //  旧実装は 1.0 固定で、高域は偶数バンドが完全L・奇数バンドが完全Rへ振り切っており、
    //  L/R がほぼ無相関(実測 +0.017)になってモノ互換性が失われていた
    //  (モノ化するとピークが -4dB 落ち音色も変わる)。既定を 0.6 にしてノブ化する。
    //  0.0 = 完全センター(モノ) / 1.0 = 旧来の振り切り。
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("stereoWidth", 1), "Stereo Width", 0.0f, 1.0f, 0.6f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tracking", 1), "Tracking", 0.0f, 100.0f, 0.0f)); // レポート留意点5

    // Tracking応答速度: ピッチ追従のlog域平滑時定数 (Fast=2ms/Natural=6ms/Smooth=20ms)
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("trackResponse", 1), "Track Response",
        juce::StringArray{ "Fast", "Natural", "Smooth" }, 1));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("mix", 1), "Mix", 0.0f, 100.0f, 100.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("outputLevel", 1), "Output Level", -60.0f, 12.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("pitchQuantize", 1), "Pitch Quantize", 0.0f, 100.0f, 0.0f));

    // Master Pitch: キャリア全体の移調 (半音)。
    //  PITCH Q を効かせている状態で動かすと Key/Scale にスナップされるので、
    //  LFOなどで変調するとスケール上を音が跳ねる (ハーモナイザ的な使い方)。
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("masterPitch", 1), "Master Pitch",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int)
            {
                return juce::String(v, 2) + " st";
            })));

    // PITCH Q のスナップ先: キー(ルート音)とスケール
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("pitchQKey", 1), "PitchQ Key",
        juce::StringArray{ "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("pitchQScale", 1), "PitchQ Scale",   // 一覧は ScaleSnap::getScaleNames()
        ScaleSnap::getScaleNames(), 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("mode", 1), "Mode", juce::StringArray{ "Auto", "MIDI" }, 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("formantFreeze", 1), "Formant Freeze", juce::StringArray{ "Off", "On" }, 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("windowType", 1), "Window Type", juce::StringArray{ "Hann", "Hamming", "Blackman" }, 0));

    // M4: フレーム間フルホップ補間ドメイン。
    //  Step=従来のブロックランプ(低フレームレートでカクつくトイ感を保持)
    //  LSP/LAR=ホップ全長を掛けて滑らかにモーフ(Hi-Fi向き)
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("interpolationMode", 1), "Interpolation Mode", juce::StringArray{ "Step", "LSP", "LAR" }, 0));

    // ※ filterbankType (Bandpass/Subtractive) は Subtractive LR4 廃止に伴い削除 (2026-07-18)

    // フェーズ2: LPC次数 (BitSpeekレトロ=10 / 高品位=16)
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("lpcOrder", 1), "LPC Order", juce::StringArray{ "8", "10", "12", "16" }, 3));

    // フェーズ2 M5: BitSpeekレトロ層
    //  FRAME RATE: 分析フレーム更新レート。低いほど声が「カクつく」トイ感。
    //  ※フリーズ(更新停止)は FREEZE ボタン(formantFreeze)に一本化した。
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("frameRate", 1), "Frame Rate",
        juce::StringArray{ "8 Hz", "15 Hz", "25 Hz", "50 Hz", "80 Hz" }, 3)); // 既定50Hz
    //  K QUANT: 反射係数のビット量子化。少ないほど声道が粗くBitSpeek的レトロ音に。
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("lpcQuantBits", 1), "K Quant",
        juce::StringArray{ "Off", "6 bit", "5 bit", "4 bit", "3 bit" }, 0));

    // --- キャリアパラメータ ---
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("waveform", 1), "Waveform", juce::StringArray{ "Saw", "Pulse", "Wavetable" }, 0));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("wavetablePosition", 1), "Wavetable Position", 0.0f, 1.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("pulseWidth", 1), "Pulse Width", 5.0f, 95.0f, 50.0f));

    // Detune: cent値と度数(音程名)の併記表示
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("detune", 1), "Detune",
        juce::NormalisableRange<float>(0.0f, 1200.0f), 5.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int)
            {
                static const char* names[13] = {
                    // 音程名は英語略記で統一 (ホスト/フォント差による文字化けを避けるためASCIIのみ)
                    //  P=Perfect, M=Major, m=minor, TT=Tritone
                    "P1", "m2", "M2", "m3", "M3",
                    "P4", "TT", "P5", "m6",
                    "M6", "m7", "M7", "P8" };
                const int st = juce::jlimit(0, 12, (int)std::lround(v / 100.0f));
                return juce::String((int)std::lround(v)) + " ct (" + juce::String(names[st]) + ")";
            })));

    // Detune SNAP: ON時はDetuneノブが100セント(半音=度数)単位でスナップ (GUI挙動用)
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("detuneSnap", 1), "Detune Snap", false));

    // Detune Mode: ユニゾンデチューンの分散アルゴリズム
    //  Classic=従来 / Linear=均等拡散 / Exp=中心密・外側疎(スーパーソウ的) /
    //  Drift=ボイス毎ランダムウォーク(アナログ揺らぎ) / Chorus=低速LFO変調
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("detuneMode", 1), "Detune Mode",
        juce::StringArray{ "Classic", "Linear", "Exp", "Drift", "Chorus" }, 0));

    // Morph (BassSynthより移植、3種併存可能):
    //  Bend +/- = 位相ベンド (Amt=曲げ量, Shift=対称点)
    //  Sync     = ハードシンク風の位相繰り返し (Amt=シンク比 1〜8x, Shift=位相オフセット)
    //  Vocode   = A-I-U-E-O フォルマントフィルタ (Amt=効き, Shift=母音モーフ位置)
    //  Amt=0 でそのMorphは無効。全て同時に掛けられる (Bend→Sync→Vocodeの順に適用)。
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("bendAmt", 1), "Bend Amount", -1.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("bendShift", 1), "Bend Shift", -1.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("syncAmt", 1), "Sync Amount", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("syncShift", 1), "Sync Shift", -1.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("vocAmt", 1), "Vocode Amount", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("vocShift", 1), "Vocode Vowel", -1.0f, 1.0f, 0.0f));

    // Noise: BitSpeek式の双方向コントロール。
    //  Filterbank : 0〜100% がキャリアへのノイズ混入（負値は0扱い＝従来と同一）
    //  LPC        : -100%=ノイズ除去(純トーン) / 0%=自動V/UV追従 / +100%=全ノイズ(ウィスパー)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("noise", 1), "Noise", -100.0f, 100.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("lofi", 1), "LoFi", 0.0f, 1.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("porta", 1), "Portamento", 0.0f, 2.0f, 0.1f)); // ポルタメント

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("basePitch", 1), "Base Pitch", 50.0f, 500.0f, 130.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("noiseColor", 1), "Noise Color", 
        juce::NormalisableRange<float>(100.0f, 10000.0f, 0.0f, 0.25f), 1000.0f));

    // MIDI用ADSR
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("attack", 1), "Attack", 0.001f, 5.0f, 0.01f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("decay", 1), "Decay", 0.001f, 5.0f, 0.1f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("sustain", 1), "Sustain", 0.0f, 1.0f, 0.8f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("release", 1), "Release", 0.001f, 5.0f, 0.2f));

    // --- モジュレーションマトリクスパラメータ (6スロット = ModMatrix::kNumSlots) ---
    for (int i = 0; i < ModMatrix::kNumSlots; ++i)
    {
        const juce::String prefix = "slot" + juce::String(i);
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "src", 1), prefix + " Source", ModMatrix::getSourceNames(), 0));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "dst", 1), prefix + " Dest", ModMatrix::getDestNames(), 0));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "amt", 1), prefix + " Amount", -1.0f, 1.0f, 0.0f));
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(prefix + "uni", 1), prefix + " Uni", false));
    }

    // LFO (3基)
    for (int i = 0; i < ModMatrix::kNumLfos; ++i)
    {
        const juce::String prefix = "lfo" + juce::String(i);
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "rate", 1), prefix + " Rate", 0.01f, 50.0f, 1.0f));
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(prefix + "sync", 1), prefix + " Sync", false));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "rateSync", 1), prefix + " Rate Sync", ModMatrix::getSyncRateNames(), 6));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(prefix + "wave", 1), prefix + " Wave", ModMatrix::getWaveNames(), 0));
    }

    // --- FX (4スロット直列。GUIからD&Dで並べ替え = Type/Amountの値を入れ替える) ---
    for (int i = 0; i < FxChain::kNumSlots; ++i)
    {
        const juce::String pre = "fx" + juce::String(i + 1);
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(pre + "Type", 1), pre + " Type", FxChain::getTypeNames(), 0));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(pre + "Amount", 1), pre + " Amount", 0.0f, 1.0f, 1.0f));
    }

    // Spectral Resonator
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("resMode", 1), "Res Mode", SpectralResonator::getModeNames(), 0));
    // ROOTはHz直値ではなくMIDIノート番号。音楽的に合わせやすく、表示も音名になる。
    //  24 = C1 … 96 = C7 / 既定45 = A2 (110Hz)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("resRoot", 1), "Res Root",
        juce::NormalisableRange<float>(24.0f, 96.0f, 1.0f), 45.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int)
            {
                static const char* kNames[12] = { "C", "C#", "D", "D#", "E", "F",
                                                  "F#", "G", "G#", "A", "A#", "B" };
                const int n = juce::jlimit(0, 127, (int)std::lround(v));
                return juce::String(kNames[n % 12]) + juce::String(n / 12 - 1);
            })));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("resChord", 1), "Res Chord", SpectralResonator::getChordNames(), 2));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("resFreeMs", 1), "Res Time",
        juce::NormalisableRange<float>(0.2f, 50.0f, 0.0f, 0.4f), 5.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("resSpread", 1), "Res Spread", 0.0f, 1.0f, 0.4f));
    // DECAY: 余韻(T60)の長さ[秒]。ピッチに依らず一定になるよう内部で帰還量へ変換する。
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("resDecay", 1), "Res Decay",
        juce::NormalisableRange<float>(0.05f, 20.0f, 0.0f, 0.35f), 2.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction(
            [](float v, int) { return juce::String(v, v < 1.0f ? 2 : 1) + " s"; })));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("resDamp", 1), "Res Damp", 0.0f, 1.0f, 0.35f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("resShimmer", 1), "Res Shimmer", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("resInharm", 1), "Res Inharm", 0.0f, 1.0f, 0.0f));

    // Multiband Drive
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("drvShape", 1), "Drive Shape", MultibandDrive::getShapeNames(), 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("drvDrive", 1), "Drive Amount",
        juce::NormalisableRange<float>(1.0f, 40.0f, 0.0f, 0.4f), 4.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("drvLow", 1), "Drive Low", 0.0f, 1.0f, 0.4f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("drvMid", 1), "Drive Mid", 0.0f, 1.0f, 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("drvHigh", 1), "Drive High", 0.0f, 1.0f, 0.7f));

    // Formant Gate
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("gateRate", 1), "Gate Rate", FormantGate::getRateNames(), 4));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("gatePattern", 1), "Gate Pattern", FormantGate::getPatternNames(), 1));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("gateDepth", 1), "Gate Depth", 0.0f, 1.0f, 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("gateVowel", 1), "Gate Vowel", 0.0f, 1.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("gateSmooth", 1), "Gate Smooth", 0.0f, 1.0f, 0.2f));
    // SHAPE: 0=ステップ保持 / 1=各ステップ頭で鋭く減衰する打点 (連打が作れる)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("gateShape", 1), "Gate Shape", 0.0f, 1.0f, 0.0f));

    // Chorus
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("choRate", 1), "Chorus Rate",
        juce::NormalisableRange<float>(0.02f, 8.0f, 0.0f, 0.4f), 0.6f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("choDepth", 1), "Chorus Depth", 0.1f, 12.0f, 4.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("choWidth", 1), "Chorus Width", 0.0f, 1.0f, 0.7f));

    // Reverb
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("revSize", 1), "Reverb Size", 0.0f, 1.0f, 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("revDamp", 1), "Reverb Damp", 0.0f, 0.95f, 0.4f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("revPredelay", 1), "Reverb Predelay",
        juce::NormalisableRange<float>(0.0f, 200.0f, 0.0f, 0.5f), 20.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("revWidth", 1), "Reverb Width", 0.0f, 1.0f, 0.6f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("revLowCut", 1), "Reverb Low Cut",
        juce::NormalisableRange<float>(20.0f, 1000.0f, 0.0f, 0.35f), 200.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("revMod", 1), "Reverb Mod", 0.0f, 1.0f, 0.3f));

    // ENV (2基)
    for (int i = 0; i < ModMatrix::kNumEnvs; ++i)
    {
        const juce::String prefix = "env" + juce::String(i);
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "attack", 1), prefix + " Attack", 0.001f, 5.0f, 0.1f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "decay", 1), prefix + " Decay", 0.001f, 5.0f, 0.3f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "sustain", 1), prefix + " Sustain", 0.0f, 1.0f, 1.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(prefix + "release", 1), prefix + " Release", 0.001f, 5.0f, 0.5f));
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(prefix + "loop", 1), prefix + " Loop", false));
    }

    return layout;
}

// ---- パラメータポインタのキャッシュ ------------------------------------
//  ここで一度だけ juce::String を組み立てて生ポインタを引いておく。
//  以降オーディオスレッドは配列参照だけで済み、文字列確保もハッシュ検索も起きない。
void SPECTRA8AudioProcessor::cacheParamPointers()
{
    for (int d = 0; d < (int)ModMatrix::NumDsts; ++d)
    {
        const char* id = ModMatrix::destParamId(d);
        mDestPtrs[(size_t)d] = (id != nullptr && id[0] != '\0')
                                 ? apvts.getRawParameterValue(id) : nullptr;
    }

    for (int i = 0; i < ModMatrix::kNumSlots; ++i)
    {
        const juce::String p = "slot" + juce::String(i);
        auto& s = mSlotPtrs[(size_t)i];
        s.src = apvts.getRawParameterValue(p + "src");
        s.dst = apvts.getRawParameterValue(p + "dst");
        s.amt = apvts.getRawParameterValue(p + "amt");
        s.uni = apvts.getRawParameterValue(p + "uni");
    }

    for (int i = 0; i < ModMatrix::kNumLfos; ++i)
    {
        const juce::String p = "lfo" + juce::String(i);
        auto& l = mLfoPtrs[(size_t)i];
        l.rate     = apvts.getRawParameterValue(p + "rate");
        l.sync     = apvts.getRawParameterValue(p + "sync");
        l.rateSync = apvts.getRawParameterValue(p + "rateSync");
        l.wave     = apvts.getRawParameterValue(p + "wave");
    }

    for (int i = 0; i < ModMatrix::kNumEnvs; ++i)
    {
        const juce::String p = "env" + juce::String(i);
        auto& e = mEnvPtrs[(size_t)i];
        e.attack  = apvts.getRawParameterValue(p + "attack");
        e.decay   = apvts.getRawParameterValue(p + "decay");
        e.sustain = apvts.getRawParameterValue(p + "sustain");
        e.release = apvts.getRawParameterValue(p + "release");
        e.loop    = apvts.getRawParameterValue(p + "loop");
    }

    for (int i = 0; i < FxChain::kNumSlots; ++i)
    {
        const juce::String p = "fx" + juce::String(i + 1);
        auto& f = mFxPtrs[(size_t)i];
        f.type   = apvts.getRawParameterValue(p + "Type");
        f.amount = apvts.getRawParameterValue(p + "Amount");
    }
}

// APVTS の現在値を ModMatrix::Params へ充填する (RTセーフ: 配列参照のみ)
void SPECTRA8AudioProcessor::loadModParams(double bpm) noexcept
{
    mModParams.bpm = bpm;

    for (int i = 0; i < ModMatrix::kNumSlots; ++i)
    {
        const auto& s = mSlotPtrs[(size_t)i];
        auto& d = mModParams.slot[(size_t)i];
        if (s.src == nullptr) continue;
        d.src = (int)s.src->load();
        d.dst = (int)s.dst->load();
        d.amt = s.amt->load();
        d.uni = (s.uni->load() >= 0.5f);
    }

    for (int i = 0; i < ModMatrix::kNumLfos; ++i)
    {
        const auto& l = mLfoPtrs[(size_t)i];
        auto& d = mModParams.lfo[(size_t)i];
        if (l.rate == nullptr) continue;
        d.rateHz   = l.rate->load();
        d.sync     = (l.sync->load() >= 0.5f);
        d.rateSync = (int)l.rateSync->load();
        d.wave     = (int)l.wave->load();
    }

    for (int i = 0; i < ModMatrix::kNumEnvs; ++i)
    {
        const auto& e = mEnvPtrs[(size_t)i];
        auto& d = mModParams.env[(size_t)i];
        if (e.attack == nullptr) continue;
        d.attack  = e.attack->load();
        d.decay   = e.decay->load();
        d.sustain = e.sustain->load();
        d.release = e.release->load();
        d.loop    = (e.loop->load() >= 0.5f);
    }
}

void SPECTRA8AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    mStoredSampleRate = sampleRate;

    cacheParamPointers();

    mFilterbankVocoder.prepare(sampleRate);
    mLpcVocoder.prepare(sampleRate);
    mPostEq.prepare(LpcVocoder::kInternalSampleRate);   // ポストEQは16kHz内部レートで動作
    mExcitationEngine.prepare(sampleRate);
    // 重要: ModMatrix::processBlock(32, ...) は 16kHz ループの中から呼ばれる
    // (= 制御ティックは 16000/32 = 500回/秒)。ここにホストレートを渡すと
    // LFO の位相進みが freq*32/hostSR となり、実レートが 16000/hostSR 倍
    // (48kHz で 1/3) に落ちる。テンポ同期も ENV の A/D/R も同じ倍率でズレるため、
    // 必ず内部レート(16kHz)を渡すこと。
    mModMatrix.prepare(LpcVocoder::kInternalSampleRate);
    // 重要: PitchTracker へは 16kHz ダウンサンプル後のサンプル (processBlock の
    // 16kループ内 inSample) を供給しているため、prepare も 16kHz を渡す。
    // ホストレートを渡すと内部でさらに 1/3 デシメーションされ、検出ピッチが
    // ホスト48kHz時に常に3倍になる (Tracking使用時に音程が3倍になるバグの原因)。
    mPitchTracker.prepare(LpcVocoder::kInternalSampleRate);
    mLimiter.prepare(sampleRate);
    mFxChain.prepare(sampleRate);
    mAnalyzer.prepare(sampleRate);
    // 解析用モノラルバッファはここで確保しておく (processBlock内でのアロケーションを避ける)
    mAnalyzerMono.assign((size_t)juce::jmax(64, samplesPerBlock), 0.0f);
    mDryL.assign((size_t)juce::jmax(64, samplesPerBlock), 0.0f);
    mDryR.assign((size_t)juce::jmax(64, samplesPerBlock), 0.0f);

    // ボコーダーモード切替状態の初期化 + PDC報告
    // (LPCモードは分析窓の群遅延 kLatency16k = 窓長/2 @16kHz)
    mCurVocoderMode = -1;
    mVocXfadeRemaining = 0;
    mVoicedSmooth = 0.0f;
    mPitchLogSmooth = -1.0f;   // Trackingピッチ平滑の再初期化
    mQuantNoteHeld = -1;       // PITCH Q保持音の再初期化

    // パラメータ・スムージング初期化 (20msランプ)
    mMixSm.reset(sampleRate, 0.02);
    mOutGainSm.reset(sampleRate, 0.02);
    mMixSm.setCurrentAndTargetValue(apvts.getRawParameterValue("mix")->load() * 0.01f);
    mOutGainSm.setCurrentAndTargetValue(
        std::pow(10.0f, apvts.getRawParameterValue("outputLevel")->load() / 20.0f));
    mFmtShiftSm = apvts.getRawParameterValue("formantShift")->load();
    mFmtStretchSm = apvts.getRawParameterValue("formantStretch")->load();
    mParamSmPrimed = false;   // 全宛先スムーザを次サンプルで即値初期化させる
    mIdleModAccum = 0.0;
    {
        const double lpcLatencySec = (double)LpcVocoder::kLatency16k / LpcVocoder::kInternalSampleRate;
        const int vm = (int)apvts.getRawParameterValue("vocoderMode")->load();
        setLatencySamples(vm == 1 ? (int)std::round(lpcLatencySec * sampleRate) : 0);
    }

    mDownsampleTimeAccum = 0.0;
    mUpsampleTimeAccum = 0.0;
    mControlRateCounter = 0;
    mInputEnvelope.store(0.0f);

    // バッファ確保
    int maxSafeSize = std::max(samplesPerBlock * 3, 4096);
    mDownsampledBuffer.assign((size_t)maxSafeSize, 0.0f);
    m16kWetL.assign((size_t)maxSafeSize, 0.0f);
    m16kWetR.assign((size_t)maxSafeSize, 0.0f);
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
    if (!mIsInitialized)
    {
        buffer.clear();
        return;
    }

    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numInputs = getTotalNumInputChannels();
    
    if (numSamples <= 0) return;

    // 1. MIDIイベントのExcitationEngineへの供給
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            mExcitationEngine.noteOn(msg.getNoteNumber(), msg.getFloatVelocity());
            addHeldNote(msg.getNoteNumber());
        }
        else if (msg.isNoteOff())
        {
            mExcitationEngine.noteOff(msg.getNoteNumber());
            removeHeldNote(msg.getNoteNumber());
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            mExcitationEngine.allNotesOff();
            mNumHeldNotes = 0;
        }
    }
    mModMatrix.handleMidi(midiMessages); // モジュレーションマトリクス用
    midiMessages.clear();

    // 2. 入力音声の包絡線（エンベロープ）算出
    float inputAvgAbs = 0.0f;
    if (numInputs > 0)
    {
        const float* readPtr = buffer.getReadPointer(0);
        float sumAbs = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            sumAbs += std::abs(readPtr[i]);
        inputAvgAbs = sumAbs / (float)numSamples;
    }
    // 指数平滑化
    float coeff = (inputAvgAbs > mInputEnvelope.load()) ? 0.05f : 0.005f;
    mInputEnvelope.store(mInputEnvelope.load() * (1.0f - coeff) + inputAvgAbs * coeff);

    // CHARACTER は 16kHz ループ内で毎サンプル smoothedParam() から取り直す。
    // (FMT SHIFT / STRETCH も同様に mFmtShiftSm / mFmtStretchSm へ直接入る)
    float effectiveCharacter = apvts.getRawParameterValue("character")->load();

    // 3. ダウンサンプリング処理 (16kHzへ)
    int num16kSamples = 0;
    if (numInputs > 0 && mStoredSampleRate > 0.0)
    {
        const float* inputL = buffer.getReadPointer(0);
        double step = mStoredSampleRate / 16000.0;
        int maxSafeSize = (int)mDownsampledBuffer.size();

        while (mDownsampleTimeAccum < (double)numSamples)
        {
            int idx0 = (int)mDownsampleTimeAccum;
            int idx1 = std::min(numSamples - 1, idx0 + 1);
            float frac = (float)(mDownsampleTimeAccum - idx0);

            float val = inputL[idx0] * (1.0f - frac) + inputL[idx1] * frac;

            if (num16kSamples < maxSafeSize)
            {
                mDownsampledBuffer[(size_t)num16kSamples] = val;
                num16kSamples++;
            }
            mDownsampleTimeAccum += step;
        }
        mDownsampleTimeAccum -= (double)numSamples;
    }

    // 4. 16kHz領域でのDSP処理ループ
    const int mode = static_cast<int>(apvts.getRawParameterValue("mode")->load());
    const bool isMidiMode = (mode == 1);

    if (mPrevMode != mode)
    {
        mExcitationEngine.reset();
        mFilterbankVocoder.reset();
        mLpcVocoder.reset();
        mPrevMode = mode;
    }

    // ボコーダーモード切替の検出 (30ms等パワークロスフェード + PDC更新)
    const int vocoderMode = (int)apvts.getRawParameterValue("vocoderMode")->load();
    if (mCurVocoderMode < 0)
    {
        mCurVocoderMode = vocoderMode;   // 初回は即時適用
    }
    else if (vocoderMode != mCurVocoderMode)
    {
        mCurVocoderMode = vocoderMode;
        mVocXfadeRemaining = kVocXfadeLen;
        // 切り替わり先モジュールの状態をクリアしてから立ち上げる
        if (vocoderMode == 1) mLpcVocoder.reset(); else mFilterbankVocoder.reset();
        const double lpcLatencySec = (double)LpcVocoder::kLatency16k / LpcVocoder::kInternalSampleRate;
        setLatencySamples(vocoderMode == 1 ? (int)std::round(lpcLatencySec * mStoredSampleRate) : 0);
    }

    // LPCモード用パラメータ (ブロック毎に1回読む)
    static constexpr int kLpcOrderMap[4] = { 8, 10, 12, 16 };
    const int lpcOrderIdx = juce::jlimit(0, 3, (int)apvts.getRawParameterValue("lpcOrder")->load());
    const int lpcOrder = kLpcOrderMap[lpcOrderIdx];
    const bool lpcFreeze = (apvts.getRawParameterValue("formantFreeze")->load() >= 0.5f);
    mLpcVocoder.setWindowType((int)apvts.getRawParameterValue("windowType")->load());

    // M5: FRAME RATE / k量子化（ブロック毎に設定）
    static constexpr float kFrameRateMap[5] = { 8.0f, 15.0f, 25.0f, 50.0f, 80.0f };
    const int frIdx = juce::jlimit(0, 4, (int)apvts.getRawParameterValue("frameRate")->load());
    mLpcVocoder.setFrameRate(kFrameRateMap[frIdx]);
    static constexpr int kQuantBitsMap[5] = { 0, 6, 5, 4, 3 };
    const int qbIdx = juce::jlimit(0, 4, (int)apvts.getRawParameterValue("lpcQuantBits")->load());
    mLpcVocoder.setQuantBits(kQuantBitsMap[qbIdx]);

    // M4: フルホップ補間ドメイン (0=Step/1=LSP/2=LAR)。次の分析フレームから適用。
    mLpcVocoder.setInterpolationMode((int)apvts.getRawParameterValue("interpolationMode")->load());

    // Tracking応答: log2領域1次平滑の係数をブロック毎に算出 (Fast=2ms/Natural=6ms/Smooth=20ms)
    {
        static constexpr float kRespTau[3] = { 0.002f, 0.006f, 0.020f };
        const int respIdx = juce::jlimit(0, 2, (int)apvts.getRawParameterValue("trackResponse")->load());
        mPitchSmoothCoef = 1.0f - std::exp(-1.0f / (kRespTau[respIdx] * 16000.0f));
    }

    // ポストEQ(LPC出力用)の係数をブロック毎に更新。BANDS EQの帯域ゲインを反映する。
    //  帯域ゲインはブロック毎に階段状に変わるため、τ=20ms の1極平滑を掛けて
    //  EQドラッグ中のジッパーノイズを防ぐ。係数はブロック長から算出する。
    {
        const double blockSec = (double)numSamples / juce::jmax(1.0, mStoredSampleRate);
        const float eqSmCoef = (float)(1.0 - std::exp(-blockSec / 0.020));
        mPostEq.updateCoeffs((int)apvts.getRawParameterValue("bandCount")->load(),
                             mBandGains, eqSmCoef);
    }

    for (int s = 0; s < num16kSamples; ++s)
    {
        const float inSample = mDownsampledBuffer[(size_t)s];

        // ピッチ検出器にサンプル供給
        mPitchTracker.pushSample(inSample);

        // 全変調宛先の実効値を1サンプルぶん平滑して進める (2ms階段の除去)。
        // 以降このサンプルでは smoothedParam() を使い、moddedParam() の生値は使わない。
        advanceParamSmoothers();

        // コントロールレート同期 (32サンプルごと)
        if (mControlRateCounter >= 32 || mControlRateCounter == 0)
        {
            mControlRateCounter = 0;

            // DAWのテンポ情報を取得
            double bpm = 120.0;
            if (auto* pH = getPlayHead())
            {
                if (auto info = pH->getPosition())
                {
                    if (info->getBpm().hasValue())
                        bpm = *(info->getBpm());
                }
            }

            // APVTSからパラメータ値をモジュレーションマトリクス用構造体にロード
            // (ポインタキャッシュ経由。文字列生成もハッシュ検索も起きない)
            loadModParams(bpm);
            mModMatrix.processBlock(32, mModParams);

            // 変調適用済み＋平滑済みの実効値。宛先IDだけで
            // パラメータID・スケール・掛かり方が決まる (PluginProcessor.h 参照)。
            auto modded = [this](int dst) { return smoothedParam(dst); };

            const float wtPos = juce::jlimit(0.0f, 1.0f, modded(ModMatrix::DstWtPos));
            const float pulseWidth = juce::jlimit(5.0f, 95.0f, modded(ModMatrix::DstPulseWidth)) * 0.01f;
            const float detune = juce::jlimit(0.0f, 1200.0f, modded(ModMatrix::DstDetune));
            const float lofi = juce::jlimit(0.0f, 1.0f, modded(ModMatrix::DstLofi));

            // --- NOISE± (BitSpeek式) → ExcitationEngineへ渡す実効ノイズmix(0..1)を算出 ---
            const float noiseSigned = juce::jlimit(-100.0f, 100.0f, modded(ModMatrix::DstNoise)) * 0.01f; // -1..+1
            // 有声度の平滑化(入力音声のV/UV)。無声ほど自動でノイズ励起へ。
            const float voicedNow = mPitchTracker.isVoiced() ? 1.0f : 0.0f;
            mVoicedSmooth += 0.25f * (voicedNow - mVoicedSmooth);
            float noise; // ExcitationEngine::syncParameters が受け取る 0..1 のmix
            if (mCurVocoderMode == 1) // LPCモード: -1=除去 / 0=自動V/UV / +1=全ノイズ
            {
                const float autoMix = 1.0f - mVoicedSmooth;            // 無声=1.0, 有声=0.0
                noise = (noiseSigned >= 0.0f)
                          ? autoMix + noiseSigned * (1.0f - autoMix)   // 0→auto, +1→1.0
                          : autoMix * (1.0f + noiseSigned);            // 0→auto, -1→0.0
            }
            else // Filterbankモード: 従来通り(負値は0)
            {
                noise = juce::jmax(0.0f, noiseSigned);
            }

            const float porta = juce::jlimit(0.0f, 2.0f, modded(ModMatrix::DstPorta));
            const int waveform = (int)(apvts.getRawParameterValue("waveform")->load());

            // ADSRはオクターブ倍率変調 (0.001〜5sを対数的に動かす)
            const float attack  = juce::jlimit(0.001f, 5.0f, modded(ModMatrix::DstAttack));
            const float decay   = juce::jlimit(0.001f, 5.0f, modded(ModMatrix::DstDecay));
            const float sustain = juce::jlimit(0.0f, 1.0f, modded(ModMatrix::DstSustain));
            const float release = juce::jlimit(0.001f, 5.0f, modded(ModMatrix::DstRelease));

            const float noiseColor = juce::jlimit(100.0f, 10000.0f, modded(ModMatrix::DstNoiseColor));

            const int detuneMode = (int)(apvts.getRawParameterValue("detuneMode")->load());
            const float bendAmt   = juce::jlimit(-1.0f, 1.0f, modded(ModMatrix::DstBendAmt));
            const float bendShift = juce::jlimit(-1.0f, 1.0f, modded(ModMatrix::DstBendShift));
            const float syncAmt   = juce::jlimit(0.0f, 1.0f, modded(ModMatrix::DstSyncAmt));
            const float syncShift = juce::jlimit(-1.0f, 1.0f, modded(ModMatrix::DstSyncShift));
            const float vocAmt    = juce::jlimit(0.0f, 1.0f, modded(ModMatrix::DstVocAmt));
            const float vocShift  = juce::jlimit(-1.0f, 1.0f, modded(ModMatrix::DstVocShift));

            // モジュール側の同期
            mExcitationEngine.syncParameters(waveform, wtPos, pulseWidth, detune, noise, lofi, porta,
                                              attack, decay, sustain, release, noiseColor, detuneMode,
                                              bendAmt, bendShift, syncAmt, syncShift, vocAmt, vocShift);
        }
        mControlRateCounter++;

        // 励起信号（キャリア）の生成
        float carrierL = 0.0f;
        float carrierR = 0.0f;
        
        // 有声音ピッチの取得。
        //  旧実装の「±20セントデッドバンド保持→超えたら一括ジャンプ」は、自然な
        //  イントネーションを階段状にしてうねり/ポルタメント風アーティファクトの
        //  原因になっていたため廃止。代わりに生ピッチを log2 領域の1次平滑
        //  (Responseパラメータ: Fast/Natural/Smooth) に通す。
        //  微小ジッタは平滑が吸収し、原音の抑揚(ピッチ感)はそのまま保たれる。
        float targetHz = mLastVoicedPitch;
        if (mPitchTracker.isVoiced())
        {
            targetHz = mPitchTracker.getRawPitchHz();
            mLastVoicedPitch = targetHz;
        }
        targetHz = juce::jlimit(30.0f, 4000.0f, targetHz);

        if (mPitchLogSmooth < 0.0f)
            mPitchLogSmooth = std::log2(targetHz);   // 初回は即値
        mPitchLogSmooth += mPitchSmoothCoef * (std::log2(targetHz) - mPitchLogSmooth);
        const float pitchHz = std::exp2(mPitchLogSmooth);

        // Tracking パラメータの適用 (Autoモードでも0%のときは基準ピッチに固定しうねりを防止)
        float basePitch = juce::jlimit(50.0f, 500.0f, smoothedParam(ModMatrix::DstBasePitch));
        float tracking = juce::jlimit(0.0f, 100.0f, smoothedParam(ModMatrix::DstTracking)) * 0.01f;
        float activePitch = basePitch + (pitchHz - basePitch) * tracking;

        // 安全対策: activePitch が異常値のときは basePitch に戻す
        if (std::isnan(activePitch) || activePitch <= 20.0f || activePitch > 8000.0f)
            activePitch = basePitch;

        // M.PITCH と PITCH Q は ScaleSnap に集約 (MIDIモードのExcitationEngineと同一実装)

        // M.PITCH(移調) → PITCH Q(Key/Scale吸着) を ScaleSnap で一括適用。
        //  移調を吸着より前に行うため、PITCH Q=100%ならM.PITCHをLFOで振ると
        //  そのKey/Scale上を音が渡り歩く。ヒステリシス付きでワブルも出ない。
        const float qAmt = juce::jlimit(0.0f, 100.0f, smoothedParam(ModMatrix::DstPitchQuantize)) * 0.01f;
        const float mPitchSt = juce::jlimit(-24.0f, 24.0f, smoothedParam(ModMatrix::DstMasterPitch));
        const int pqKey   = juce::jlimit(0, 11, (int)apvts.getRawParameterValue("pitchQKey")->load());
        const int pqScale = juce::jlimit(0, ScaleSnap::kNumScales - 1,
                                         (int)apvts.getRawParameterValue("pitchQScale")->load());

        // ※ qAmt=0 でも M.PITCH は効かせる (transposeAndSnap が内部で分岐)
        if (activePitch > 20.0f && !std::isnan(activePitch))
        {
            activePitch = ScaleSnap::transposeAndSnap(activePitch, mPitchSt, qAmt,
                                                      pqKey, pqScale, mQuantNoteHeld);
        }
        if (qAmt <= 0.001f)
            mQuantNoteHeld = -1; // PITCH Q無効時は保持解除

        // FMT SHIFT/STRETCH と CHARACTER は advanceParamSmoothers() で
        // 既にサンプル単位の1極平滑(τ≈5ms)を通っているので、そのまま受け取る。
        // (旧実装はここで二重に平滑していた)
        mFmtShiftSm   = juce::jlimit(-24.0f, 24.0f, smoothedParam(ModMatrix::DstFormantShift));
        mFmtStretchSm = juce::jlimit(0.5f, 2.0f, smoothedParam(ModMatrix::DstFormantStretch));
        effectiveCharacter = juce::jlimit(0.0f, 1.0f, smoothedParam(ModMatrix::DstCharacter));

        // MIDIモードでは activePitch(=Autoの追従ピッチ) は使われずノート番号で発音するため、
        // M.PITCH / PITCH Q を別途エンジン側へ渡してボイス毎に適用させる。
        mExcitationEngine.setPitchShaping(mPitchSt, qAmt, pqKey, pqScale);
        mExcitationEngine.processSample(carrierL, carrierR, activePitch, isMidiMode);

        // ボコーディング処理 (vocoderMode: 0=Filterbank / 1=LPC)
        float wetL = 0.0f;
        float wetR = 0.0f;

        const int bandCount = (int)apvts.getRawParameterValue("bandCount")->load();
        const float resonance = juce::jlimit(0.3f, 3.0f, smoothedParam(ModMatrix::DstResonance));
        const float stereoWidth = juce::jlimit(0.0f, 1.0f, smoothedParam(ModMatrix::DstStereoWidth));

        auto renderFilterbank = [&](float& l, float& r)
        {
            mFilterbankVocoder.processSample(inSample, carrierL, carrierR, l, r,
                                             bandCount, effectiveCharacter, resonance,
                                             mFmtShiftSm, mFmtStretchSm,
                                             stereoWidth, mBandGains, mBandLevelsForUi);
        };
        // character(0..1) → 帯域拡張γ(0.97=ぼやけ 〜 0.998=シャープ) へマッピング (M3)
        const float lpcGamma = 0.970f + 0.028f * juce::jlimit(0.0f, 1.0f, effectiveCharacter);
        auto renderLpc = [&](float& l, float& r)
        {
            // FMT SHIFT(リサンプル比) / FMT STRETCH(LSP領域の間隔伸縮)をLPCへ渡す
            mLpcVocoder.processSample(inSample, carrierL, carrierR, l, r, lpcOrder, lpcFreeze,
                                      lpcGamma, mFmtShiftSm, mFmtStretchSm);
            // BANDS EQ をポストEQとしてLPC出力へ適用 (クロスフェード時もLPC側のみに掛かる)
            mPostEq.process(l, r);
        };

        if (mVocXfadeRemaining > 0)
        {
            // 30ms等パワークロスフェード (旧→新)
            float oldL = 0.0f, oldR = 0.0f, newL = 0.0f, newR = 0.0f;
            if (mCurVocoderMode == 1) { renderFilterbank(oldL, oldR); renderLpc(newL, newR); }
            else                      { renderLpc(oldL, oldR); renderFilterbank(newL, newR); }

            const float t = 1.0f - (float)mVocXfadeRemaining / (float)kVocXfadeLen;
            const float gOld = std::cos(t * juce::MathConstants<float>::halfPi);
            const float gNew = std::sin(t * juce::MathConstants<float>::halfPi);
            wetL = oldL * gOld + newL * gNew;
            wetR = oldR * gOld + newR * gNew;
            --mVocXfadeRemaining;
        }
        else if (mCurVocoderMode == 1)
        {
            renderLpc(wetL, wetR);
            // LPCモードはフィルターバンク合成を行わないため、そのままだと
            // BANDS EQ のアナライザー(帯域レベルメーター)が更新されず止まってしまう。
            // 分析専用パスを呼び、入力音声の帯域レベルだけをメーターへ反映する。
            // (クロスフェード中は renderFilterbank 側で分析が走るので、ここでは呼ばない=二重処理を防止)
            mFilterbankVocoder.analyzeForMeter(inSample, bandCount, effectiveCharacter, resonance,
                                               mBandLevelsForUi);
        }
        else
        {
            renderFilterbank(wetL, wetR);
        }

        m16kWetL[(size_t)s] = wetL;
        m16kWetR[(size_t)s] = wetR;
    }

    // 4b. 入力が繋がっていない / ブロックが極小で16kサンプルが生成されなかったときも
    //     LFO・ENV を止めない。上のループの中でしか ModMatrix を回していないため、
    //     MIDIモードでサイドチェイン入力を繋がずに使うと変調が完全に固まっていた。
    //     経過時間を16kHz換算して、足りないぶんの制御ティックをここで進める。
    if (num16kSamples <= 0 && numSamples > 0 && mStoredSampleRate > 0.0)
    {
        mIdleModAccum += (double)numSamples * LpcVocoder::kInternalSampleRate / mStoredSampleRate;
        int guard = 0;
        while (mIdleModAccum >= 32.0 && guard++ < 64)   // 暴走防止の上限
        {
            mIdleModAccum -= 32.0;
            double bpm = 120.0;
            if (auto* pH = getPlayHead())
                if (auto info = pH->getPosition())
                    if (info->getBpm().hasValue())
                        bpm = *(info->getBpm());
            loadModParams(bpm);
            mModMatrix.processBlock(32, mModParams);
        }
        if (guard >= 64) mIdleModAccum = 0.0;
    }
    else
    {
        mIdleModAccum = 0.0;
    }

    // 5. アップサンプリング (ブレンドは FX 通過後の 5.9 で行う)
    //    MIX/OUT LEVEL は20msランプでサンプル毎に平滑 (ジッパーノイズ対策)
    mMixSm.setTargetValue(juce::jlimit(0.0f, 100.0f, smoothedParam(ModMatrix::DstMix)) * 0.01f);
    mOutGainSm.setTargetValue(
        std::pow(10.0f, juce::jlimit(-60.0f, 12.0f, smoothedParam(ModMatrix::DstOutLevel)) / 20.0f));

    // ゲートゲインの算出 (リニア入力エンベロープに基づくソフトゲート)
    float envVal = mInputEnvelope.load();
    float threshold = 0.005f; // ナレーション等の合間の極小ノイズを遮断するしきい値
    float gateGain = 1.0f;
    if (!isMidiMode)
    {
        if (envVal < threshold)
            gateGain = 0.0f;
        else if (envVal < threshold + 0.005f)
            gateGain = (envVal - threshold) / 0.005f; // 0.005〜0.01の間で滑らかにフェード
    }

    float* writeL = buffer.getWritePointer(0);
    float* writeR = (numInputs > 1) ? buffer.getWritePointer(1) : writeL;

    // MIXは「原音」と「ボコーダー+FXを通した音」のクロスフェード。
    //  そのため、ここではまだブレンドせずウェットのみをバッファへ書き、
    //  原音は mDryL/mDryR へ退避しておく。FX通過後の最終段でブレンドする。
    //  (旧実装はブレンド後にFXを掛けていたため、MIX=0でもFXが原音に掛かっていた)
    // num16kSamples が 0 のときに補間へ入ると idx1 = num16kSamples-1 = -1 となり、
    // (size_t)(-1) で巨大インデックスになって範囲外アクセス→クラッシュする。
    // ホストブロックが hostSR/16000 サンプル(48kHzで3)より小さいと実際に 0 になり得るため
    // (サンプル精度オートメーションでブロックを刻むホスト等)、条件に必ず含める。
    const bool didProcess = (numInputs > 0 && mStoredSampleRate > 0.0
                             && num16kSamples > 0
                             && (int)mDryL.size() >= numSamples);
    if (didProcess)
    {
        double step = 16000.0 / mStoredSampleRate;
        const bool stereoIn = (numInputs > 1);
        for (int i = 0; i < numSamples; ++i)
        {
            // 原音の退避 (writeR が writeL を指す場合があるので先に読む)
            mDryL[(size_t)i] = writeL[i];
            mDryR[(size_t)i] = stereoIn ? writeR[i] : writeL[i];

            // 線形補間アップサンプリング。
            //
            //  【重要】mUpsampleTimeAccum は「ブロック境界をまたいで連続する読み出し位相」であり、
            //  ブロック末尾で num16kSamples を引いた結果はわずかに負になることが普通にある
            //  (ダウンサンプラーが出す整数個数と、numSamples×step の実数値がぴったり一致しないため)。
            //  この負の端数は次ブロックで正しく吸収されるので、決して 0 に丸めてはいけない。
            //  丸めるとブロック毎に読み出し位相が最大1サンプルぶん飛び、
            //  ブロックレート(48kHz/64smp なら 750Hz)のクリック列 = 常時「ジリジリ」になる。
            //
            //  frac も同様に負を許す (元実装どおりの後方外挿)。クランプすると段差が出る。
            //  範囲外アクセスの防止は「添字だけ」を丸めることで行う。
            //  num16kSamples > 0 は didProcess 側で保証済みなので idx1 は必ず有効。
            const int rawIdx = (int)mUpsampleTimeAccum;
            const float frac = (float)(mUpsampleTimeAccum - (double)rawIdx);
            const int idx0 = juce::jlimit(0, num16kSamples - 1, rawIdx);
            const int idx1 = juce::jlimit(0, num16kSamples - 1, rawIdx + 1);

            writeL[i] = (m16kWetL[(size_t)idx0] * (1.0f - frac) + m16kWetL[(size_t)idx1] * frac) * gateGain;
            writeR[i] = (m16kWetR[(size_t)idx0] * (1.0f - frac) + m16kWetR[(size_t)idx1] * frac) * gateGain;

            mUpsampleTimeAccum += step;
        }
        mUpsampleTimeAccum -= (double)num16kSamples;
        // ここで範囲を丸めてはいけない (上のコメント参照)。
        // 壊れた値(NaN/Inf や桁あふれ)のときだけリセットする。
        if (!std::isfinite(mUpsampleTimeAccum) || std::abs(mUpsampleTimeAccum) > 1.0e6)
            mUpsampleTimeAccum = 0.0;
    }
    else
    {
        // 今ブロックはボコーダーを通していない → 位相アキュムレータを初期化して
        // 次のブロックで古い端数から読み始めないようにする
        mUpsampleTimeAccum = 0.0;
    }

    // 5.5 FXチェーン — ウェット(ボコーダー出力)に対してのみ適用する。
    //     MIXノブは「原音 ⇔ ボコーダー+FX」のクロスフェードなので、
    //     FXはウェット側に属する。これにより MIX=0 で FX も完全にバイパスされる。
    //     なお MIX=0 でもFX自体は動かし続ける (残響やレゾネーターの尾が
    //     MIXを戻した瞬間に不自然に途切れないようにするため)。
    {
        FxChain::Params fp;
        for (int s = 0; s < FxChain::kNumSlots; ++s)
        {
            const auto& f = mFxPtrs[(size_t)s];
            if (f.type == nullptr) continue;
            fp.slot[(size_t)s].type   = (int)f.type->load();
            fp.slot[(size_t)s].amount = f.amount->load();
        }
        fp.resMode     = (int)apvts.getRawParameterValue("resMode")->load();
        // ROOTはMIDIノート番号なのでHzへ変換
        fp.resRootHz   = 440.0f * std::pow(2.0f,
                            (apvts.getRawParameterValue("resRoot")->load() - 69.0f) / 12.0f);
        fp.resChord    = (int)apvts.getRawParameterValue("resChord")->load();
        fp.resFreeMs   = apvts.getRawParameterValue("resFreeMs")->load();
        fp.resSpread   = apvts.getRawParameterValue("resSpread")->load();
        fp.resDecay    = apvts.getRawParameterValue("resDecay")->load();
        fp.resDamp     = apvts.getRawParameterValue("resDamp")->load();
        fp.resShimmer  = apvts.getRawParameterValue("resShimmer")->load();
        fp.resInharm   = apvts.getRawParameterValue("resInharm")->load();

        fp.drvShape = (int)apvts.getRawParameterValue("drvShape")->load();
        fp.drvDrive = apvts.getRawParameterValue("drvDrive")->load();
        fp.drvLow   = apvts.getRawParameterValue("drvLow")->load();
        fp.drvMid   = apvts.getRawParameterValue("drvMid")->load();
        fp.drvHigh  = apvts.getRawParameterValue("drvHigh")->load();

        fp.gateRate    = (int)apvts.getRawParameterValue("gateRate")->load();
        fp.gatePattern = (int)apvts.getRawParameterValue("gatePattern")->load();
        fp.gateDepth   = apvts.getRawParameterValue("gateDepth")->load();
        fp.gateVowel   = apvts.getRawParameterValue("gateVowel")->load();
        fp.gateSmooth  = apvts.getRawParameterValue("gateSmooth")->load();
        fp.gateShape   = apvts.getRawParameterValue("gateShape")->load();

        fp.choRate  = apvts.getRawParameterValue("choRate")->load();
        fp.choDepth = apvts.getRawParameterValue("choDepth")->load();
        fp.choWidth = apvts.getRawParameterValue("choWidth")->load();

        fp.revSize     = apvts.getRawParameterValue("revSize")->load();
        fp.revDamp     = apvts.getRawParameterValue("revDamp")->load();
        fp.revPredelay = apvts.getRawParameterValue("revPredelay")->load();
        fp.revWidth    = apvts.getRawParameterValue("revWidth")->load();
        fp.revLowCut   = apvts.getRawParameterValue("revLowCut")->load();
        fp.revMod      = apvts.getRawParameterValue("revMod")->load();

        // Resonator MIDIモード用: 押鍵中のノートを周波数へ
        fp.numMidiHz = juce::jmin(mNumHeldNotes, (int)fp.midiHz.size());
        for (int i = 0; i < fp.numMidiHz; ++i)
            fp.midiHz[(size_t)i] = 440.0f * std::pow(2.0f, (mHeldNotes[(size_t)i] - 69) / 12.0f);

        // Gateのテンポ同期用BPM
        fp.bpm = 120.0;
        if (auto* pH = getPlayHead())
            if (auto info = pH->getPosition())
                if (info->getBpm().hasValue())
                    fp.bpm = *(info->getBpm());

        // FXの連続パラメータはブロック毎の階段になるので τ=30ms で均す
        {
            const double blockSec = (double)numSamples / juce::jmax(1.0, mStoredSampleRate);
            mFxChain.syncParameters(fp, (float)(1.0 - std::exp(-blockSec / 0.030)));
        }

        if (mFxChain.isActive())
            for (int i = 0; i < numSamples; ++i)
                mFxChain.processSample(writeL[i], writeR[i]);
    }

    // 5.9 DRY/WET ブレンド & 出力ゲイン。
    //     MIX=0 → 退避しておいた原音そのまま (ボコーダーもFXも一切通らない)
    //     MIX=1 → ボコーダー+FX の全処理音
    //     MIX/OUT LEVEL は20msランプでサンプル毎に平滑 (ジッパーノイズ対策)
    if (didProcess)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float mix = mMixSm.getNextValue();
            const float outGain = mOutGainSm.getNextValue();
            // モノラル時は writeR が writeL を指すため、先に両方読んでから書く
            // (先に書くと2行目が「ブレンド済みの値」を再ブレンドしてしまう)
            const float wl = writeL[i];
            const float wr = writeR[i];
            writeL[i] = (mDryL[(size_t)i] * (1.0f - mix) + wl * mix) * outGain;
            writeR[i] = (mDryR[(size_t)i] * (1.0f - mix) + wr * mix) * outGain;
        }
    }

    // 5.95 NaN / Inf の水際チェック。
    //  LPCのラティスやResonatorのフィードバックで一度でも非有限値が生まれると、
    //  そのまま状態変数に居座り「リセットするまで永久に無音 or 轟音」になる。
    //  ブロック末尾で1回だけ検査し、見つかったら全DSPの状態を捨てて復帰する。
    //  (検査はブロックあたり numSamples×2 回の比較のみで、実質ゼロコスト)
    {
        bool bad = false;
        for (int i = 0; i < numSamples && !bad; ++i)
            bad = !std::isfinite(writeL[i]) || !std::isfinite(writeR[i]);

        if (bad)
        {
            buffer.clear();
            mFilterbankVocoder.reset();
            mLpcVocoder.reset();
            mExcitationEngine.reset();
            mFxChain.reset();
            mLimiter.reset();
            mModMatrix.reset();
            mPostEq.reset();
            mMixSm.setCurrentAndTargetValue(mMixSm.getTargetValue());
            mOutGainSm.setCurrentAndTargetValue(mOutGainSm.getTargetValue());
            mFmtShiftSm   = apvts.getRawParameterValue("formantShift")->load();
            mFmtStretchSm = apvts.getRawParameterValue("formantStretch")->load();
            mPitchLogSmooth = -1.0f;
            mInputEnvelope.store(0.0f);
            return;   // このブロックは無音で返す
        }
    }

    // 6. 最終段リミッター。
    //    天井は内部固定 -0.1 dBFS (BrickLimiter::kCeiling)。突発ピークも天井へ抑える。
    //    ON/OFF は 20ms クロスフェードで切り替える (旧実装は瞬時分岐でクリックが出ていた)。
    {
        const bool limiter = (static_cast<int>(apvts.getRawParameterValue("limiterEnable")->load()) == 1);
        mLimiter.processBlended(writeL, writeR, numSamples, limiter);
    }

    // 7. アナライザーへ最終出力を投入 (表示専用・ロックフリー)。
    //    ホストが prepareToPlay より大きいブロックを渡してくる場合に備えて容量を確認する
    //    (RTスレッドなのでリサイズはせず、入る分だけ渡す)。
    {
        const int n = juce::jmin(numSamples, (int)mAnalyzerMono.size());
        if (n > 0)
        {
            for (int i = 0; i < n; ++i)
                mAnalyzerMono[(size_t)i] = 0.5f * (writeL[i] + writeR[i]);
            mAnalyzer.pushAudio(mAnalyzerMono.data(), n);
        }
    }
}

juce::AudioProcessorEditor* SPECTRA8AudioProcessor::createEditor()
{
    return new SPECTRA8AudioProcessorEditor(*this);
}

void SPECTRA8AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    // 【重要】setStateInformation の replaceState で BAND_EQ_GAINS ごと apvts.state に
    // 取り込まれるため、copyState() には既に前回のノードが含まれている。
    // そのまま createNewChildElement すると保存の度にノードが増殖し、
    // 復元時の getChildByName が「一番古いノード」を拾って EQ が過去の値に戻る
    // (勝手に値が変わる/リセットが効かない、の原因)。必ず既存分を消してから追加する。
    xml->deleteAllChildElementsWithTagName("BAND_EQ_GAINS");

    auto* eqNode = xml->createNewChildElement("BAND_EQ_GAINS");
    for (int i = 0; i < 48; ++i)
    {
        eqNode->setAttribute("gain" + juce::String(i), (double)mBandGains[(size_t)i].load());
    }

    copyXmlToBinary(*xml, destData);
}

void SPECTRA8AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
    {
        // Band EQ ゲインの復元 (replaceState より先に読む)
        if (auto* eqNode = xmlState->getChildByName("BAND_EQ_GAINS"))
        {
            for (int i = 0; i < 48; ++i)
            {
                const float g = (float)eqNode->getDoubleAttribute("gain" + juce::String(i), 1.0);
                // 壊れた値(NaN/0/極端値)で無音化しないよう常識的な範囲に丸める
                mBandGains[(size_t)i].store(
                    (std::isfinite(g) && g > 0.0f) ? juce::jlimit(0.0631f, 3.98f, g) : 1.0f);
            }
        }

        if (xmlState->hasTagName(apvts.state.getType()))
        {
            // BAND_EQ_GAINS は apvts の管理外。取り込むと保存の度に増殖するので
            // ValueTree へ移す前に取り除く。
            xmlState->deleteAllChildElementsWithTagName("BAND_EQ_GAINS");
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
        }

        // カスタムWavetableの復元 (パスが保存されていればロード)。
        // ファイルIO/FFTを伴うためメッセージスレッドで実行する。
        // 非同期実行時はWeakReferenceで生存確認 (プロセッサ破棄後のダングリング防止)。
        const juce::String wtPath = getCustomWavetablePath();
        if (wtPath.isNotEmpty())
        {
            juce::WeakReference<SPECTRA8AudioProcessor> wp(this);
            auto doLoad = [wp, wtPath]
            {
                if (auto* p = wp.get())
                {
                    const juce::File f(wtPath);
                    if (f.existsAsFile())
                        p->loadCustomWavetable(f);
                }
            };
            if (juce::MessageManager::getInstance()->isThisTheMessageThread())
                doLoad();
            else
                juce::MessageManager::callAsync(doLoad);
        }
    }
}

// JUCEにこのプラグインを生成するコールバックを教える
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SPECTRA8AudioProcessor();
}