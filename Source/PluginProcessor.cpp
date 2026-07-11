#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

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
      mControlRateCounter(0),
      mInterpolationBeta(0.0f),
      mCurrentGain(0.0f),
      mTargetGain(0.0f)
{
    mAnalysisFft = std::make_unique<juce::dsp::FFT>(10);
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

    // フォルマントシフト (双一次写像のアルファ値、範囲 -0.5 〜 0.5)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("formantShift", 1), "Formant Shift", -0.5f, 0.5f, 0.0f));

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

    return layout;
}

void SPECTRA8AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);
    // 各モジュールの初期化
    mVoiceManager.setup(sampleRate);
    mOscillatorBank.setup(sampleRate);
    mCharacterProcessor.setup(sampleRate);
    mMultiRateMapper.setup(sampleRate);
    
    mLpcAnalyzer.setup(24);
    mLpcToLsp.setup(24);
    mLspToLpc.setup(24);
    mTelpcIntegrator.setup(10); // fftOrder=10 -> FFTSize=1024

    mMidiQueue.clear();
    mMidiActiveMode = false;
    mInputEnvelope = 0.0f;
    mCurrentF0 = 150.0f;

    // 状態構造体のクリア
    mDspState = DSP::PolyphonicVoiceSoA();

    // 分析バッファの初期化
    mAnalysisInputBuffer.clear();
    mAnalysisFrame.assign(mAnalysisWindowSize, 0.0f);
    
    mTeEnvelope.resize(513, 0.0f);
    mBarkEnergies.resize(24, 0.0f);
    m16kLpc.resize(25, 0.0f);
    m16kLsp.resize(24, 0.0f);

    // LSP 初期目標値の設定（中立フィルタ、根を等間隔で配置）
    mLspCurrent.resize(24);
    for (int i = 0; i < 24; ++i)
    {
        float angle = static_cast<float>(i + 1) * 3.14159265f / 25.0f;
        mLspCurrent[i] = std::cos(angle);
    }
    mLspTarget = mLspCurrent;
    mLspInterpolated = mLspCurrent;
    mLspShifted = mLspCurrent;
    mFsLpc.resize(25, 0.0f);

    mControlRateCounter = 0;
    mInterpolationBeta = 0.0f;
    mCurrentGain = 0.0f;
    mTargetGain = 0.0f;
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

    // 1. MIDIメッセージをスレッド安全なキューにロード
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        DSP::NoteEvent event;
        event.sampleOffset = static_cast<uint32_t>(metadata.samplePosition);
        if (msg.isNoteOn())
        {
            mMidiActiveMode = true;
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
    float frameRate = apvts.getRawParameterValue("frameRate")->load();
    int lpcOrder = static_cast<int>(apvts.getRawParameterValue("lpcOrder")->load());
    float formantShift = apvts.getRawParameterValue("formantShift")->load();
    float formantStretch = apvts.getRawParameterValue("formantStretch")->load();
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

    if (mMidiActiveMode)
    {
        mVoiceManager.processMidiEvents(mMidiQueue, mDspState);
        mVoiceManager.updateVoices(attack, decay, sustain, release);
    }
    else
    {
        // オート・ピッチトラッキングモード (ボイス0を常時発音、入力ピッチと音量を追従)
        mVoiceManager.setVoiceActive(0, true);
        for (int i = 1; i < 8; ++i)
        {
            mVoiceManager.setVoiceActive(i, false);
        }
        
        float baseF0 = 150.0f; // C3ベースのデフォルトピッチ
        float freq0 = baseF0 + (mCurrentF0 - baseF0) * tracking;
        float finalF0 = freq0 * std::pow(2.0f, pitchTranspose / 12.0f);

        mVoiceManager.setVoiceFrequency(0, finalF0);
        mVoiceManager.setVoiceEnvelope(0, 1.0f);
    }

    // 4. 入力音声を 16kHz にダウンサンプリング (分析パス)
    std::vector<float> downsampled;
    if (numInputs > 0)
    {
        const float* inputL = buffer.getReadPointer(0);
        mMultiRateMapper.downsample(inputL, numSamples, downsampled);
    }
    mAnalysisInputBuffer.insert(mAnalysisInputBuffer.end(), downsampled.begin(), downsampled.end());

    // 5. 16kHz領域でのホップごとのLPC/TrueEnvelope分析
    // frameRateが0%の場合は前回のフレームを「フリーズ」する
    if (frameRate > 0.0f && mAnalysisInputBuffer.size() >= static_cast<size_t>(mAnalysisWindowSize))
    {
        // 簡易ホップレート制御 (frameRateが低い場合は一部の分析ホップをスキップして負荷を軽減可能)
        while (mAnalysisInputBuffer.size() >= static_cast<size_t>(mAnalysisWindowSize))
        {
            // 分析フレームの構築
            std::fill(mAnalysisFrame.begin(), mAnalysisFrame.end(), 0.0f);
            int copySize = std::min(mAnalysisWindowSize, static_cast<int>(mAnalysisInputBuffer.size()));
            std::copy(mAnalysisInputBuffer.begin(), mAnalysisInputBuffer.begin() + copySize, mAnalysisFrame.begin());

            // ピッチ検出
            float f0 = mPitchDetector.detectPitch(mAnalysisFrame.data(), static_cast<int>(mAnalysisFrame.size()), 16000.0f);
            if (f0 > 50.0f && f0 < 800.0f)
            {
                mCurrentF0 = f0;
            }

            // 振幅包絡抽出 (True Envelope)
            mTrueEnvelope.estimate(mAnalysisFrame.data(), static_cast<int>(mAnalysisFrame.size()), mCurrentF0, mTeEnvelope, 16000.0f);

            // Barkフィルタバンクによる低域補償
            // 自己相関のために実数FFTを準備
            std::vector<float> fftBuffer(2048, 0.0f);
            std::copy(mAnalysisFrame.begin(), mAnalysisFrame.end(), fftBuffer.begin());
            
            // FFT実行 (コールバック内でのインスタンス生成を排除)
            mAnalysisFft->performRealOnlyForwardTransform(fftBuffer.data());
            
            std::vector<float> powerSpectrum(513, 0.0f);
            powerSpectrum[0] = fftBuffer[0] * fftBuffer[0];
            powerSpectrum[512] = fftBuffer[1] * fftBuffer[1];
            for (int k = 1; k < 512; ++k)
            {
                powerSpectrum[k] = fftBuffer[2 * k] * fftBuffer[2 * k] + fftBuffer[2 * k + 1] * fftBuffer[2 * k + 1];
            }

            mBarkFilterBank.process(powerSpectrum.data(), mBarkEnergies);

            // True Envelope と Bark下限拘束の統合による安定LPC導出
            // 結合係数 gamma=0.85
            mTelpcIntegrator.integrate(mTeEnvelope, mBarkEnergies, 0.85f, lpcOrder, m16kLpc, mTargetGain);

            // 16kHz LPC から 16kHz LSP への変換
            mLpcToLsp.convert(m16kLpc, m16kLsp, lpcOrder);

            // ホストSR用の 24次 LSP 空間へ再射影 (高域ダミー極追加)
            mMultiRateMapper.mapLSF(m16kLsp, lpcOrder, mLspTarget, 24);

            // 処理したホップ分を削除
            mAnalysisInputBuffer.erase(mAnalysisInputBuffer.begin(), mAnalysisInputBuffer.begin() + mAnalysisHopSize);
        }
    }

    // 6. 出力バッファの構築 (ホストサンプリングレート処理)
    auto* writePointerL = buffer.getWritePointer(0);
    auto* writePointerR = buffer.getWritePointer(1);
    
    // 入力のコピー (Dry用)
    std::vector<float> dryL(numSamples, 0.0f);
    if (numInputs > 0)
    {
        std::copy(buffer.getReadPointer(0), buffer.getReadPointer(0) + numSamples, dryL.begin());
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // 6-A. コントロール・レート (32サンプル毎にLSP補間＋LPC逆変換を実行)
        if (mControlRateCounter >= mControlRateBlockSize || mControlRateCounter == 0)
        {
            mControlRateCounter = 0;
            
            // LSPおよびゲインの線形補間
            mInterpolationBeta = 0.0f; // ブロック頭
            
            for (int i = 0; i < 24; ++i)
            {
                mLspInterpolated[i] = mLspCurrent[i];
            }
            mCurrentGain = mTargetGain; // Phase 1では即座に更新するか、緩やかに追従

            // 6-B. LSPドメインでのフォルマント変調 (Shifter)
            mFormantShifter.process(mLspInterpolated, mLspShifted, formantShift, formantStretch, 24);

            // 6-C. LSP から LPC への逆変換
            mLspToLpc.convert(mLspShifted, mFsLpc, 24);

            // 算出された LPC 係数を 8ボイス並列状態にコピー
            for (int i = 0; i < 24; ++i)
            {
                float val = mFsLpc[i + 1]; // a_1 〜 a_24 (coeffs[0] = 1.0 なのでインデックス+1)
                for (int v = 0; v < 8; ++v)
                {
                    mDspState.filterCoeffsL[i][v] = val;
                    mDspState.filterCoeffsR[i][v] = val;
                }
            }

            // 次のブロックに向けて値を更新
            mLspCurrent = mLspTarget;
        }

        mControlRateCounter++;

        // 6-D. ボイス状態の更新 (ADSRエンベロープなど)
        if (mMidiActiveMode)
        {
            mVoiceManager.updateVoices(attack, decay, sustain, release);
        }
        mVoiceManager.syncToDspState(mDspState, detuneWidth, pitchTranspose, tracking, mCurrentF0);

        // 6-E. 8ボイス並列用白色ノイズの生成
        __m256 noiseBuffer = mNoiseGenerator.nextBlockAVX2();

        // 6-F. 8ボイス並列オシレーター＆フィルター処理実行 (AVX2 SIMD)
        float sampleL = 0.0f;
        float sampleR = 0.0f;
        
        __m256 activeMask = mVoiceManager.getActiveVoicesMask();
        __m256 mixEnvelopes = mVoiceManager.getVoiceEnvelopes(); // 各ボイスのエンベロープ
        __m256 noiseMixVec = _mm256_set1_ps(noiseParam); // 有声無声パラメータ

        mOscillatorBank.processSampleAVX2(mDspState, activeMask, mixEnvelopes, noiseMixVec, noiseBuffer, sampleL, sampleR);

        // 各ボイスの音量およびグローバルゲイン、LPC合成後のスケーリングを適用
        // LPC残差エネルギーに基づき、ゲインを調整
        sampleL *= mCurrentGain;
        sampleR *= mCurrentGain;

        // 6-G. Characterノブによる Lo-Fi/Hi-Fi 連続処理の適用
        mCharacterProcessor.processSample(sampleL, sampleR, character);

        // 6-H. Dry / Wet ミックス & アウトプットレベルの適用
        float drySample = dryL[sample];
        float outValL = (1.0f - mix) * drySample + mix * sampleL;
        float outValR = (1.0f - mix) * drySample + mix * sampleR;

        writePointerL[sample] = outValL * outputGain;
        writePointerR[sample] = outValR * outputGain;
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
