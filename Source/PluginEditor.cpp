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
      mBandsEqPanel(p.apvts, p.getBandGains(), p.getBandLevelsForUi(), p.getAnalyzer())
{
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
        addAndMakeVisible(btn);
    };

    setupTabButton(mTabVocoderBtn, 0);
    setupTabButton(mTabExcitationBtn, 1);
    setupTabButton(mTabModBtn, 2);
    setupTabButton(mTabFxBtn, 3);
    setupTabButton(mTabBandsEqBtn, 4);

    // タブパネルを追加
    addChildComponent(mVocoderPanel);
    addChildComponent(mExcitationPanel);
    addChildComponent(mModPanel);
    addChildComponent(mFxPanel);
    addChildComponent(mBandsEqPanel);

    // HUD の初期化
    mDebugLabel.setColour(juce::Label::backgroundColourId, SpectraColors::panel);
    mDebugLabel.setColour(juce::Label::textColourId, SpectraColors::textDim);
    mDebugLabel.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    mDebugLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(mDebugLabel);

    // 初期タブの選択
    mTabVocoderBtn.setToggleState(true, juce::sendNotification);

    // ウィンドウサイズ設定 (Granular準拠のワイド表示)
    setSize(780, 380);

    // タブボタンにも説明を付ける
    mTabVocoderBtn.setTooltip("VOCODER - core vocoder settings: engine type, voicing mode, "
                              "formants, band count and output level.");
    mTabExcitationBtn.setTooltip("EXCITATION - the carrier oscillator that the voice is imprinted on: "
                                 "waveform, wavetable, detune and waveshaping.");
    mTabModBtn.setTooltip("MOD MATRIX - route 3 LFOs, 2 envelopes and MIDI sources to any knob.");
    mTabFxBtn.setTooltip("FX - five serial effect slots applied after the vocoder. "
                         "Drag the slots to reorder them.");
    mTabBandsEqBtn.setTooltip("BANDS EQ - per-band gain for the vocoder bands, with a live spectrum "
                              "of the plugin output behind it.");

    startTimer(100); // 10Hzでステータス行/ヘルプ表示を更新
}

SPECTRA8AudioProcessorEditor::~SPECTRA8AudioProcessorEditor()
{
    stopTimer();
}

void SPECTRA8AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(SpectraColors::bg);

    // ヘッダー背景
    auto headerRect = getLocalBounds().removeFromTop(36);
    g.setColour(SpectraColors::panel);
    g.fillRect(headerRect);
    g.setColour(SpectraColors::panelLine);
    g.drawHorizontalLine(headerRect.getBottom() - 1, 0.0f, (float)getWidth());

    // プラグインタイトル
    g.setColour(SpectraColors::text);
    g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    g.drawText("SPECTRA 8", 16, 0, 120, headerRect.getHeight(), juce::Justification::centredLeft);

    g.setColour(SpectraColors::textDim);
    g.setFont(10.0f);
    g.drawText("v0.2.0 Hybrid Vocoder", 124, 2, 120, headerRect.getHeight(), juce::Justification::centredLeft);
}

void SPECTRA8AudioProcessorEditor::resized()
{
    auto r = getLocalBounds();

    // 1. ヘッダー部のレイアウト
    auto headerArea = r.removeFromTop(36);
    
    // タブ選択ボタンの配置 (ヘッダーの右半分に並べる)
    // タブは5つ。96pxのままだとヘッダー左のタイトルに重なるため88pxへ
    const int tabW = 88;
    const int tabH = 24;
    int tabX = getWidth() - (tabW * 5) - 16;
    const int tabY = (headerArea.getHeight() - tabH) / 2;

    mTabVocoderBtn.setBounds(tabX, tabY, tabW, tabH);
    mTabExcitationBtn.setBounds(tabX + tabW, tabY, tabW, tabH);
    mTabModBtn.setBounds(tabX + tabW * 2, tabY, tabW, tabH);
    mTabFxBtn.setBounds(tabX + tabW * 3, tabY, tabW, tabH);
    mTabBandsEqBtn.setBounds(tabX + tabW * 4, tabY, tabW, tabH);

    // 2. HUD（下部）のレイアウト
    auto hudArea = r.removeFromBottom(20);
    mDebugLabel.setBounds(hudArea);

    // 3. メインパネル（中央）のレイアウト
    mVocoderPanel.setBounds(r);
    mExcitationPanel.setBounds(r);
    mModPanel.setBounds(r);
    mFxPanel.setBounds(r);
    mBandsEqPanel.setBounds(r);
}

// 下部ステータス行の更新。
//  マウスの下にあるコンポーネントを辿って TooltipClient (Slider / ComboBox / Button は
//  すべてこれを実装している) を探し、setTooltip() で登録した英語説明文を表示する。
//  何も指していないときはプラグインの状態表示に戻す。
//  この方式なら各パネル側は setTooltip("...") を呼ぶだけで済み、
//  ポップアップが隣のノブを隠すこともない。
void SPECTRA8AudioProcessorEditor::timerCallback()
{
    juce::String help;

    auto& mouse = juce::Desktop::getInstance().getMainMouseSource();
    if (auto* under = mouse.getComponentUnderMouse())
    {
        // 自分のエディター内のコンポーネントだけを対象にする
        if (under == this || isParentOf(under))
        {
            for (auto* c = under; c != nullptr && c != this; c = c->getParentComponent())
            {
                if (auto* tc = dynamic_cast<juce::TooltipClient*>(c))
                {
                    help = tc->getTooltip();
                    if (help.isNotEmpty())
                        break;
                }
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

    // タブに合わせたボタンのトグル状態の再設定
    mTabVocoderBtn.setToggleState(mActiveTab == 0, juce::dontSendNotification);
    mTabExcitationBtn.setToggleState(mActiveTab == 1, juce::dontSendNotification);
    mTabModBtn.setToggleState(mActiveTab == 2, juce::dontSendNotification);
    mTabFxBtn.setToggleState(mActiveTab == 3, juce::dontSendNotification);
    mTabBandsEqBtn.setToggleState(mActiveTab == 4, juce::dontSendNotification);

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
    setBtnHighlight(mTabBandsEqBtn, mActiveTab == 4, SpectraColors::accentBands);

    repaint();
}
