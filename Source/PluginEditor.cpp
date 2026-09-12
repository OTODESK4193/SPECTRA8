// ==========================================
// File: PluginEditor.cpp
// SPECTRA8 エディター層 (4タブ + HUD / Granular 準拠)
// ==========================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

SPECTRA8AudioProcessorEditor::SPECTRA8AudioProcessorEditor(SPECTRA8AudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      mVocoderPanel(p),
      mExcitationPanel(p),
      mModPanel(p.apvts),
      mFxPanel(p),
      mBandsEqPanel(p.apvts, p.getBandGains(), p.getBandLevelsForUi(), p.getAnalyzer()),
      mPresetPanel(p.apvts)
{
    // Content Component の設定 (Ambience準拠: 75%縮小〜175%拡大)
    addAndMakeVisible(content);
    content.onPaint = [this](juce::Graphics& g) { paintContent(g); };
    content.onLayout = [this] { layoutContent(); };

    constrainer.setFixedAspectRatio((double)kBaseW / (double)kBaseH);
    constrainer.setSizeLimits(
        static_cast<int>(kBaseW * 0.75), static_cast<int>(kBaseH * 0.75),
        static_cast<int>(kBaseW * 1.75), static_cast<int>(kBaseH * 1.75));
    setConstrainer(&constrainer);
    setResizable(true, true);

    const int savedW = audioProcessor.getSavedEditorWidth();
    const int savedH = audioProcessor.getSavedEditorHeight();
    setSize(juce::jlimit(static_cast<int>(kBaseW * 0.75), static_cast<int>(kBaseW * 1.75), savedW),
            juce::jlimit(static_cast<int>(kBaseH * 0.75), static_cast<int>(kBaseH * 1.75), savedH));

    // ボタンのスタイルとリスナー初期化
    auto setupTabButton = [this](juce::TextButton& btn, int tabIdx)
    {
        btn.setButtonText(btn.getButtonText());
        btn.setColour(juce::TextButton::buttonColourId, SpectraColors::knobTrack);
        btn.setColour(juce::TextButton::textColourOffId, SpectraColors::textDim);
        btn.setColour(juce::TextButton::textColourOnId, SpectraColors::text);
        btn.setClickingTogglesState(true);
        btn.setRadioGroupId(1001); // 同一グループで排他トグル
        btn.onClick = [this, tabIdx] { selectTab(tabIdx); };
        content.addAndMakeVisible(btn);
    };

    setupTabButton(mTabVocoderBtn, 0);
    setupTabButton(mTabExcitationBtn, 1);
    setupTabButton(mTabModBtn, 2);
    setupTabButton(mTabFxBtn, 3);
    setupTabButton(mTabEqBtn, 4);
    setupTabButton(mTabPresetBtn, 5);

    // タブパネルを追加
    content.addChildComponent(mVocoderPanel);
    content.addChildComponent(mExcitationPanel);
    content.addChildComponent(mModPanel);
    content.addChildComponent(mFxPanel);
    content.addChildComponent(mBandsEqPanel);
    content.addChildComponent(mPresetPanel);

    // インフォバーの初期化。
    //  juce::Label は drawFittedText で「高さ / 行の高さ」ぶんの行数まで自動折り返しするので、
    //  高さを 3 行ぶん確保するだけで説明文が3行で回り込む。
    //  複数行を読ませるので中央揃えではなく左揃えにする。
    mDebugLabel.setColour(juce::Label::backgroundColourId, SpectraColors::panel);
    mDebugLabel.setColour(juce::Label::textColourId, SpectraColors::textDim);
    // インフォバーの本文は「左端から kInfoTextInset px」の位置から書き始める。
    //  コンボボックスは左端の細い列に並んでいるので、ポップアップは必ず左側に出る。
    //  その幅ぶんを最初から余白にしておけば、メニューを開いても本文は絶対に隠れない。
    mDebugLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
    mDebugLabel.setJustificationType(juce::Justification::topLeft);
    mDebugLabel.setBorderSize(juce::BorderSize<int>(5, kInfoTextInset, 4, 12));
    mDebugLabel.setMinimumHorizontalScale(1.0f);   // 縮小せず必ず折り返す
    content.addAndMakeVisible(mDebugLabel);

    // インフォバー左側余白の受信ノート名ラベル初期化
    mMidiNotesLabel.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    mMidiNotesLabel.setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    mMidiNotesLabel.setColour(juce::Label::textColourId, SpectraColors::accentVocoder);
    mMidiNotesLabel.setJustificationType(juce::Justification::centredLeft);
    content.addAndMakeVisible(mMidiNotesLabel);

    // 初期タブの選択
    mTabVocoderBtn.setToggleState(true, juce::sendNotification);

    // タブボタンにも説明を付ける
    mTabVocoderBtn.setTooltip("VOCODER - core vocoder settings: engine type, voicing mode, formants and level.");
    mTabExcitationBtn.setTooltip("EXCITATION - the carrier oscillator: waveform, wavetable, detune and shaping.");
    mTabModBtn.setTooltip("MOD MATRIX - route 3 LFOs, 2 envelopes and MIDI sources to any knob.");
    mTabFxBtn.setTooltip("FX - five serial effect slots applied after the vocoder. Drag to reorder.");
    mTabEqBtn.setTooltip("EQ - per-band gain for the vocoder bands, with a live spectrum display.");
    mTabPresetBtn.setTooltip("PRESET - factory and user preset browser with search and favorites.");

    startTimer(100); // 10Hzでステータス行/ヘルプ表示を更新
}

SPECTRA8AudioProcessorEditor::~SPECTRA8AudioProcessorEditor()
{
    stopTimer();
}

void SPECTRA8AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(SpectraColors::bg);
}

void SPECTRA8AudioProcessorEditor::paintContent(juce::Graphics& g)
{
    // ヘッダー背景
    auto headerRect = content.getLocalBounds().removeFromTop(36);
    g.setColour(SpectraColors::panel);
    g.fillRect(headerRect);
    g.setColour(SpectraColors::panelLine);
    g.drawHorizontalLine(headerRect.getBottom() - 1, 0.0f, (float)kBaseW);

    // プラグインタイトル
    g.setColour(SpectraColors::text);
    g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    g.drawText("SPECTRA 8", 16, 0, 100, headerRect.getHeight(), juce::Justification::centredLeft);

    g.setColour(SpectraColors::textDim);
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("v1.0.0 B003", 118, 2, 120, headerRect.getHeight(), juce::Justification::centredLeft);
}

void SPECTRA8AudioProcessorEditor::resized()
{
    const float scale = juce::jmax(0.25f, (float)getWidth() / (float)kBaseW);
    content.setTransform(juce::AffineTransform::scale(scale));
    content.setBounds(0, 0, kBaseW, kBaseH);
    audioProcessor.setSavedEditorSize(getWidth(), getHeight());
}

void SPECTRA8AudioProcessorEditor::layoutContent()
{
    auto r = content.getLocalBounds();

    // 1. ヘッダー部のレイアウト
    auto headerArea = r.removeFromTop(36);
    
    // タブ選択ボタンの配置 (ヘッダーの右側に6個並べる)
    const int tabW = 76;
    const int tabH = 24;
    int tabX = kBaseW - (tabW * 6) - 12;
    const int tabY = (headerArea.getHeight() - tabH) / 2;

    mTabVocoderBtn.setBounds(tabX, tabY, tabW, tabH);
    mTabExcitationBtn.setBounds(tabX + tabW, tabY, tabW, tabH);
    mTabModBtn.setBounds(tabX + tabW * 2, tabY, tabW, tabH);
    // ヘッダの並びは信号の流れ順にする: … MOD MATRIX → EQ → FX → PRESET。
    // (実際の処理順も ボコーダー → BANDS EQ → FX なのでタブ順と一致する)
    // タブID自体は変えず (FX=3 / EQ=4)、表示位置だけ入れ替える。
    mTabEqBtn.setBounds(tabX + tabW * 3, tabY, tabW, tabH);
    mTabFxBtn.setBounds(tabX + tabW * 4, tabY, tabW, tabH);
    mTabPresetBtn.setBounds(tabX + tabW * 5, tabY, tabW, tabH);

    // 2. インフォバー（下部・3行）のレイアウト
    //    13px フォント × 3行 + 上下の余白 = 57px
    auto hudArea = r.removeFromBottom(57);
    mDebugLabel.setBounds(hudArea);
    mMidiNotesLabel.setBounds(hudArea.getX() + 6, hudArea.getY() + 4, kInfoTextInset - 12, 18);

    // 3. メインパネル（中央）のレイアウト
    mVocoderPanel.setBounds(r);
    mExcitationPanel.setBounds(r);
    mModPanel.setBounds(r);
    mFxPanel.setBounds(r);
    mBandsEqPanel.setBounds(r);
    mPresetPanel.setBounds(r);
}

// 下部ステータス行の更新。
void SPECTRA8AudioProcessorEditor::timerCallback()
{
    // モジュレーションプレビュー (DAW停止時の自走LFOと静的レンジの即時更新)
    audioProcessor.updateModMatrixPreview(1.0 / 30.0);

    // 左端余白に受信MIDIノートを表示 (FIFO最大8音)
    mMidiNotesLabel.setText(audioProcessor.getHeldNotesText(), juce::dontSendNotification);

    juce::String help;

    const auto mousePos = content.getLocalPoint(this, getMouseXYRelative());
    if (auto* c = content.getComponentAt(mousePos))
    {
        for (auto* comp = c; comp != nullptr && comp != &content; comp = comp->getParentComponent())
        {
            if (auto* tc = dynamic_cast<juce::TooltipClient*>(comp))
            {
                help = tc->getTooltip();
                if (help.isNotEmpty())
                    break;
            }
        }
    }

    const bool showingHelp = help.isNotEmpty();
    if (showingHelp != mShowingHelp)
    {
        mShowingHelp = showingHelp;
        mDebugLabel.setColour(juce::Label::textColourId,
                              showingHelp ? SpectraColors::text : SpectraColors::textDim);
    }

    mDebugLabel.setText(showingHelp ? help : audioProcessor.getDebugMessage(),
                        juce::dontSendNotification);
}

void SPECTRA8AudioProcessorEditor::selectTab(int tabIndex)
{
    mActiveTab = tabIndex;

    mVocoderPanel.setVisible(mActiveTab == 0);
    mExcitationPanel.setVisible(mActiveTab == 1);
    mModPanel.setVisible(mActiveTab == 2);
    mFxPanel.setVisible(mActiveTab == 3);
    mBandsEqPanel.setVisible(mActiveTab == 4);
    mPresetPanel.setVisible(mActiveTab == 5);

    // タブに合わせたボタンのトグル状態の再設定
    mTabVocoderBtn.setToggleState(mActiveTab == 0, juce::dontSendNotification);
    mTabExcitationBtn.setToggleState(mActiveTab == 1, juce::dontSendNotification);
    mTabModBtn.setToggleState(mActiveTab == 2, juce::dontSendNotification);
    mTabFxBtn.setToggleState(mActiveTab == 3, juce::dontSendNotification);
    mTabEqBtn.setToggleState(mActiveTab == 4, juce::dontSendNotification);
    mTabPresetBtn.setToggleState(mActiveTab == 5, juce::dontSendNotification);

    // タブごとのカラーアクセントをボタンに反映して視覚的フィードバックを高める
    auto setBtnHighlight = [](juce::TextButton& btn, bool active, juce::Colour accent)
    {
        btn.setColour(juce::TextButton::buttonColourId, active ? accent.withAlpha(0.24f) : SpectraColors::knobTrack);
        btn.setColour(juce::TextButton::buttonOnColourId, accent.withAlpha(0.24f));
        btn.setColour(juce::TextButton::textColourOnId, active ? SpectraColors::text : SpectraColors::textDim);
    };

    setBtnHighlight(mTabVocoderBtn, mActiveTab == 0, SpectraColors::accentVocoder);
    setBtnHighlight(mTabExcitationBtn, mActiveTab == 1, SpectraColors::accentExcitation);
    setBtnHighlight(mTabModBtn, mActiveTab == 2, SpectraColors::accentMod);
    setBtnHighlight(mTabFxBtn, mActiveTab == 3, SpectraColors::accentFx);
    setBtnHighlight(mTabEqBtn, mActiveTab == 4, SpectraColors::accentBands);
    setBtnHighlight(mTabPresetBtn, mActiveTab == 5, SpectraColors::mint);

    if (mActiveTab == 5)
        mPresetPanel.refreshAll();

    repaint();
}
