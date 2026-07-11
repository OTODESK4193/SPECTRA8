#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <cstring>

SPECTRA8AudioProcessor::SPECTRA8AudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::mono(), true)
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
}

SPECTRA8AudioProcessor::~SPECTRA8AudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SPECTRA8AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // キャラクター (0.0 = Lo-Fi, 1.0 = Hi-Fi)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("character", 1), "Character", 0.0f, 1.0f, 1.0f));

    // 分析フレームレート (0.0 = Freeze, 100.0 = Full Speed)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("frameRate", 1), "Frame Rate", 0.0f, 100.0f, 100.0f));

    // LPC 分析次数 (12 〜 18次)
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("lpcOrder", 1), "LPC Order", 12, 18, 16));

    // ピッチトランスポーズ (±36半音)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("pitch", 1), "Pitch", -36.0f, 36.0f, 0.0f));

    // ピッチトラッキング量 (0 〜 100%)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("tracking", 1), "Tracking", 0.0f, 100.0f, 100.0f));

    // フォルマントシフト (半音単位, 範囲 -24.0 〜 24.0 半音)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("formantShift", 1), "Formant Shift", -24.0f, 24.0f, 0.0f));

    // フォルマントストレッチ/スクィーズ (アフィン変換 of シグマ値、範囲 0.5 〜 2.0)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("formantStretch", 1), "Formant Stretch", 0.5f, 2.0f, 1.0f));

    // デチューン幅 (0 〜 1200 cents)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("detune", 1), "Detune", 0.0f, 1200.0f, 5.0f));

    // 有声/無声バランス（ノイズ量 0 〜 100%）
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("noise", 1), "Noise", 0.0f, 100.0f, 0.0f));

    // 励振波形 (Saw, Pulse, Wavetable)
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("waveform", 1), "Waveform", juce::StringArray{ "Saw", "Pulse", "Wavetable" }, 0));

    // ウェーブテーブルスキャン位置
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("wavetablePosition", 1), "Wavetable Position", 0.0f, 1.0f, 0.0f));

    // パルス幅 (5% 〜 95%)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("pulseWidth", 1), "Pulse Width", 5.0f, 95.0f, 50.0f));

    // ADSR
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("attack", 1), "Attack", 0.001f, 5.0f, 0.01f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("decay", 1), "Decay", 0.001f, 5.0f, 0.1f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("sustain", 1), "Sustain", 0.0f, 1.0f, 0.8f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("release", 1), "Release", 0.001f, 5.0f, 0.2f));

    // ミックス & アウトレベル
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("mix", 1), "Mix", 0.0f, 100.0f, 100.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("outputLevel", 1), "Output Level", -60.0f, 12.0f, 0.0f));

    // 動作モード (Auto = 0, MIDI = 1)
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("mode", 1), "Mode", juce::StringArray{ "Auto", "MIDI" }, 0));

    return layout;
}

void SPECTRA8AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    // 分析制御用パラメータの初期化 (未初期化によるLPC停止バグの解消)
    mAnalysisWindowSize = 1024;
    mAnalysisHopSize = 100;
    mControlRateBlockSize = 32;
    mDownsampleTimeAccum = 0.0;

    // 各モジュールの初期化 (ボイス合成・フィルタリングは16kHz固定)
    mVoiceManager.setup(16000.0);
    mOscillatorBank.setup(16000.0);
    
    mMidiQueue.clear();
    mMidiActiveMode = false;
    mInputEnvelope = 0.0f;
    mCurrentF0 = 150.0f;

    // 状態構造体のクリア
    std::memset(&mDspState, 0, sizeof(mDspState));

    // 分析バッファの初期化
    mAnalysisInputBuffer.clear();
    
    // 20バンド・バンドパス・フィルタバンク用バッファ・係数の初期化
    int numBands = 20;
    mBandEnvelopes.assign(numBands, 0.0f);
    mTargetBandEnvelopes.assign(numBands, 0.0f);

    mAnalFilterX1.assign(numBands, 0.0f);
    mAnalFilterX2.assign(numBands, 0.0f);
    mAnalFilterY1.assign(numBands, 0.0f);
    mAnalFilterY2.assign(numBands, 0.0f);

    mBandF0.resize(numBands);
    mBandCoeffsB0.resize(numBands);
    mBandCoeffsB2.resize(numBands);
    mBandCoeffsA1.resize(numBands);
    mBandCoeffsA2.resize(numBands);

    float fMin = 80.0f;
    float fMax = 7000.0f; // 16kHzのナイキスト周波数8000Hz以下に収める
    float Q = 8.0f;       // 音量が滑らかにつながるように設定

    for (int i = 0; i < numBands; ++i)
    {
        float freq = fMin * std::pow(fMax / fMin, static_cast<float>(i) / (numBands - 1));
        mBandF0[i] = freq;

        float omega = 2.0f * 3.14159265f * freq / 16000.0f;
        float sinW = std::sin(omega);
        float cosW = std::cos(omega);
        float alpha = sinW / (2.0f * Q);

        float b0 = sinW / 2.0f;
        float b2 = -b0;
        float a0 = 1.0f + alpha;
        float a1 = -2.0f * cosW;
        float a2 = 1.0f - alpha;

        mBandCoeffsB0[i] = b0 / a0;
        mBandCoeffsB2[i] = b2 / a0;
        mBandCoeffsA1[i] = a1 / a0;
        mBandCoeffsA2[i] = a2 / a0;
    }

    m16kWetBuffer.assign(samplesPerBlock, 0.0f);

    mControlRateCounter = 0;
    mCurrentUnvoicedRatio = 0.0f;
    mTargetUnvoicedRatio = 0.0f;
    mF0History.assign(5, 150.0f);
}

void SPECTRA8AudioProcessor::releaseResources()
{
}

bool SPECTRA8AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::mono() &&
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

    // デバッグ用NaN/Inf検出マクロ
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

    #define CHECK_NAN_VEC(vec, msg) CHECK_NAN_ARRAY((vec).data(), static_cast<int>((vec).size()), msg)

    if (numInputs > 0 && numSamples > 0)
    {
        CHECK_NAN_ARRAY(buffer.getReadPointer(0), numSamples, "DAW Input Buffer");
    }

    // 1. MIDIメッセージをスレッド安全なキューにロード
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
    float noiseParam = apvts.getRawParameterValue("noise")->load() * 0.01f; // 0.0f 〜 1.0f

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

    // 3. MIDIキューまたはオートモードの同期、および入力音量追従エンベロープ
    if (numInputs > 0 && numSamples > 0)
    {
        const float* inputL = buffer.getReadPointer(0);
        float sumAbs = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            sumAbs += std::abs(inputL[i]);
        }
        float avgAbs = sumAbs / static_cast<float>(numSamples);
        float coeff = (avgAbs > mInputEnvelope) ? 0.05f : 0.005f; // 非対称アタック/リリース平滑化
        mInputEnvelope = mInputEnvelope * (1.0f - coeff) + avgAbs * coeff;
    }

    if (isMidiMode)
    {
        mVoiceManager.processMidiEvents(mMidiQueue, mDspState);
        mVoiceManager.updateVoices(attack, decay, sustain, release);
    }
    else
    {
        // オート・ピッチトラッキングモード (声の入力がある時だけボイス0を発音)
        bool hasInput = (mInputEnvelope > 0.0005f);
        mVoiceManager.setVoiceActive(0, hasInput);
        for (int i = 1; i < 8; ++i)
        {
            mVoiceManager.setVoiceActive(i, false);
        }
        
        // 音量を入力エンベロープに追従させる (感度調整用に 4.0f 倍)
        float envVolume = std::clamp(mInputEnvelope * 4.0f, 0.0f, 1.0f);
        mVoiceManager.setVoiceEnvelope(0, envVolume);
        
        float baseF0 = 150.0f; // C3ベースのデフォルトピッチ
        float freq0 = baseF0 + (mCurrentF0 - baseF0) * tracking;
        float finalF0 = freq0 * std::pow(2.0f, pitchTranspose / 12.0f);

        mVoiceManager.setVoiceFrequency(0, finalF0);
    }

    // 4. 入力音声を 16kHz にダウンサンプリング (ホストサンプリングレートから 16kHz への直接線形リサンプラー)
    std::vector<float> downsampled;
    double srcSampleRate = getSampleRate();
    if (numInputs > 0 && numSamples > 0 && srcSampleRate > 0.0)
    {
        const float* inputL = buffer.getReadPointer(0);
        double timeAccum = mDownsampleTimeAccum;
        double step = srcSampleRate / 16000.0;
        
        while (timeAccum < static_cast<double>(numSamples))
        {
            int idx0 = static_cast<int>(timeAccum);
            int idx1 = std::min(numSamples - 1, idx0 + 1);
            float frac = static_cast<float>(timeAccum - idx0);
            
            float val = inputL[idx0] * (1.0f - frac) + inputL[idx1] * frac;
            downsampled.push_back(val);
            
            timeAccum += step;
        }
        mDownsampleTimeAccum = timeAccum - static_cast<double>(numSamples);
    }
    
    mAnalysisInputBuffer.insert(mAnalysisInputBuffer.end(), downsampled.begin(), downsampled.end());

    // 5. 16kHz領域でのリアルタイム・チャネルボコーディング処理 (サンプル同期駆動)
    int num16kSamples = static_cast<int>(downsampled.size());
    m16kWetBuffer.resize(num16kSamples);

    int numBands = 20;

    for (int sample16k = 0; sample16k < num16kSamples; ++sample16k)
    {
        float inSample = downsampled[sample16k];

        // 5-A. 分析側（モジュレーター）：20バンドの Biquad BPF を通してエンベロープを検出
        for (int i = 0; i < numBands; ++i)
        {
            float x = inSample;
            float x1 = mAnalFilterX1[i];
            float x2 = mAnalFilterX2[i];
            float y1 = mAnalFilterY1[i];
            float y2 = mAnalFilterY2[i];

            // Biquad フィルタ実行
            float y = mBandCoeffsB0[i] * x + mBandCoeffsB2[i] * x2 - mBandCoeffsA1[i] * y1 - mBandCoeffsA2[i] * y2;
            if (std::isnan(y) || std::isinf(y)) y = 0.0f;

            mAnalFilterX2[i] = x1;
            mAnalFilterX1[i] = x;
            mAnalFilterY2[i] = y1;
            mAnalFilterY1[i] = y;

            // 整流エンベロープフォロワーの平滑化更新
            float envCoeff = 0.008f; // 平滑化の応答時定数 (カットオフ約25Hz)
            mTargetBandEnvelopes[i] = mTargetBandEnvelopes[i] * (1.0f - envCoeff) + std::abs(y) * envCoeff;
        }

        // 5-B. コントロールレート（32サンプル毎）でのエンベロープ更新
        if (mControlRateCounter >= mControlRateBlockSize || mControlRateCounter == 0)
        {
            mControlRateCounter = 0;
            mBandEnvelopes = mTargetBandEnvelopes;
        }
        mControlRateCounter++;

        // 5-C. ボイス状態の更新 (16kHz基準)
        if (isMidiMode)
        {
            mVoiceManager.updateVoices(attack, decay, sustain, release);
        }
        else
        {
            // オート・ピッチトラッキングモード (声の入力がある時だけボイス0を発音)
            bool hasInput = (mInputEnvelope > 0.0005f);
            mVoiceManager.setVoiceActive(0, hasInput);
            for (int i = 1; i < 8; ++i)
            {
                mVoiceManager.setVoiceActive(i, false);
            }
            float envVolume = std::clamp(mInputEnvelope * 4.0f, 0.0f, 1.0f);
            mVoiceManager.setVoiceEnvelope(0, envVolume);
            
            // ピッチ検出は使用せず、C3 (130Hz) 固定ピッチで完璧なロボットボイスを生成
            float finalF0 = 130.0f * std::pow(2.0f, pitchTranspose / 12.0f);
            mVoiceManager.setVoiceFrequency(0, finalF0);
        }

        // ボコーダー合成用にフラットなピッチで SoA 状態を同期
        mVoiceManager.syncToDspState(mDspState, detuneWidth, pitchTranspose, tracking, 130.0f);

        // 5-D. 8ボイス並列キャリア用白色ノイズの生成
        __m256 noiseBuffer = mNoiseGenerator.nextBlockAVX2();
        __m256 activeMask = mVoiceManager.getActiveVoicesMask();
        __m256 mixEnvelopes = mVoiceManager.getVoiceEnvelopes();

        // ノイズの比率は UI の noiseParam をそのまま使用
        __m256 noiseMixVec = _mm256_set1_ps(noiseParam);

        float sampleL = 0.0f;
        float sampleR = 0.0f;

        // 5-E. 20バンド・バンドパス・フィルタバンクによるキャリア変調処理を実行
        mOscillatorBank.processSampleAVX2(
            mDspState,
            activeMask,
            mixEnvelopes,
            noiseMixVec,
            noiseBuffer,
            mBandEnvelopes.data(),
            formantShift,
            mBandCoeffsB0.data(),
            mBandCoeffsB2.data(),
            mBandCoeffsA1.data(),
            mBandCoeffsA2.data(),
            sampleL,
            sampleR
        );

        // Wet 信号バッファに書き戻す (ゲインは後段で適用)
        m16kWetBuffer[sample16k] = 0.5f * (sampleL + sampleR);
    }
    mAnalysisInputBuffer.clear(); // リアルタイム処理につき毎回クリア

    // 7. 16kHz Wet信号をホストサンプリングレートへアップサンプリング (直書き線形リサンプラー)
    std::vector<float> wetFs(numSamples, 0.0f);
    if (numSamples > 0 && !m16kWetBuffer.empty())
    {
        double upRatio = static_cast<double>(m16kWetBuffer.size()) / static_cast<double>(numSamples);
        for (int i = 0; i < numSamples; ++i)
        {
            double pos = i * upRatio;
            int idx0 = static_cast<int>(pos);
            int idx1 = std::min(static_cast<int>(m16kWetBuffer.size() - 1), idx0 + 1);
            float frac = static_cast<float>(pos - idx0);
            wetFs[i] = m16kWetBuffer[idx0] * (1.0f - frac) + m16kWetBuffer[idx1] * frac;
        }
    }

    // 8. 最終ミックスと出力 (ホストサンプリングレート処理)
    auto* writePointerL = (numOutputs > 0) ? buffer.getWritePointer(0) : nullptr;
    auto* writePointerR = (numOutputs > 1) ? buffer.getWritePointer(1) : nullptr;
    
    // 入力のコピー (Dry用)
    std::vector<float> dryL(numSamples, 0.0f);
    if (numInputs > 0 && buffer.getNumChannels() > 0)
    {
        std::copy(buffer.getReadPointer(0), buffer.getReadPointer(0) + numSamples, dryL.begin());
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        float drySampleL = dryL[sample];
        float drySampleR = (numInputs > 1 && buffer.getNumChannels() > 1) ? buffer.getReadPointer(1)[sample] : drySampleL;

        float wetVal = wetFs[sample];

        // 入力エネルギーが極めて低ければ、Wet信号を完全にミュート（ゲート）してノイズ漏れを防止
        if (mInputEnvelope < 0.001f)
        {
            wetVal = 0.0f;
        }

        float wetSampleL = wetVal;
        float wetSampleR = wetVal;

        // 直書き Lo-Fi ビットクラッシャー ＋ サンプルレートリダクション
        if (character < 0.98f)
        {
            float bits = 4.0f + 20.0f * character;
            float steps = std::pow(2.0f, bits);
            
            int holdSamples = static_cast<int>(1.0f + 31.0f * (1.0f - character));
            int holdStartIdx = std::clamp((sample / holdSamples) * holdSamples, 0, numSamples - 1);
            
            wetSampleL = wetFs[holdStartIdx];
            wetSampleR = wetFs[holdStartIdx];
            
            wetSampleL = std::round(wetSampleL * steps) / steps;
            wetSampleR = std::round(wetSampleR * steps) / steps;
        }

        float outValL = (1.0f - mix) * drySampleL + mix * wetSampleL;
        float outValR = (1.0f - mix) * drySampleR + mix * wetSampleR;

        CHECK_NAN(outValL, "outValL");
        CHECK_NAN(outValR, "outValR");

        if (writePointerL != nullptr)
        {
            writePointerL[sample] = outValL * outputGain;
        }
        if (writePointerR != nullptr)
        {
            writePointerR[sample] = outValR * outputGain;
        }
    }

    #undef CHECK_NAN
    #undef CHECK_NAN_ARRAY
    #undef CHECK_NAN_VEC
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

// JUCEにこのプラグインを認識させるためのエントリポイント
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SPECTRA8AudioProcessor();
}
