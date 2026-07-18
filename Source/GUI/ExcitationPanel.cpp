// ==========================================
// File: ExcitationPanel.cpp
// 「EXCITATION」タブ・パネル (Granular 準拠)
// ==========================================
#include "ExcitationPanel.h"
#include "../PluginProcessor.h"

ExcitationPanel::ExcitationPanel(SPECTRA8AudioProcessor& proc)
    : processor(proc),
      apvts(proc.apvts),
      mBtnDetuneSnap("SNAP", SpectraColors::accentExcitation)
{
    auto setupKnob = [this](ValueKnob& k, juce::Label& l, const juce::String& suffix = "")
    {
        k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
        k.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k.setColour(juce::Slider::textBoxTextColourId, SpectraColors::textDim);
        k.setColour(juce::Slider::rotarySliderFillColourId, SpectraColors::accentExcitation);
        k.setColour(juce::Slider::rotarySliderOutlineColourId, SpectraColors::knobTrack);
        k.setTextValueSuffix(suffix);
        k.setLookAndFeel(&mArcLookAndFeel);
        addAndMakeVisible(k);

        l.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        l.setJustificationType(juce::Justification::centred);
        l.setColour(juce::Label::textColourId, SpectraColors::textDim);
        addAndMakeVisible(l);
    };

    setupKnob(mKnobWtPos, mLblWtPos);
    setupKnob(mKnobPulseWidth, mLblPulseWidth, "%");
    setupKnob(mKnobDetune, mLblDetune);   // 表示はパラメータ側の "N ct (度数)" 書式
    setupKnob(mKnobPorta, mLblPorta, "s");
    mKnobDetune.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 110, 16); // "1200 ct (8度)" が収まる幅

    auto setupCombo = [this](juce::ComboBox& c, const juce::StringArray& items)
    {
        c.setColour(juce::ComboBox::backgroundColourId, SpectraColors::knobTrack);
        c.setColour(juce::ComboBox::textColourId, SpectraColors::text);
        c.setColour(juce::ComboBox::outlineColourId, SpectraColors::panelLine);
        c.setColour(juce::ComboBox::arrowColourId, SpectraColors::textDim);
        c.setJustificationType(juce::Justification::centred);
        for (int i = 0; i < items.size(); ++i)
            c.addItem(items[i], i + 1);
        addAndMakeVisible(c);
    };

    setupCombo(mComboWaveform, { "Sawtooth", "Pulse", "Wavetable" });
    setupCombo(mComboDetuneMode, { "Dtn: Classic", "Dtn: Linear", "Dtn: Exp", "Dtn: Drift", "Dtn: Chorus" });

    addAndMakeVisible(mWaveDisplay);
    addAndMakeVisible(mBtnDetuneSnap);

    // BROWSE ボタン (Wavetable選択時のみ表示)
    mBtnBrowse.setColour(juce::TextButton::buttonColourId, SpectraColors::knobTrack);
    mBtnBrowse.setColour(juce::TextButton::textColourOffId, SpectraColors::text);
    mBtnBrowse.onClick = [this] { showBrowser(); };
    addChildComponent(mBtnBrowse);

    // カスタムWT名ラベル
    mLblCustomName.setFont(juce::Font(juce::FontOptions(10.0f)));
    mLblCustomName.setColour(juce::Label::textColourId, SpectraColors::textDim);
    mLblCustomName.setJustificationType(juce::Justification::centredLeft);
    addChildComponent(mLblCustomName);

    // ブラウザ操作ボタン
    for (auto* b : { &mBtnBrowserClose, &mBtnFactory })
    {
        b->setColour(juce::TextButton::buttonColourId, SpectraColors::knobTrack);
        b->setColour(juce::TextButton::textColourOffId, SpectraColors::text);
        addChildComponent(*b);
    }
    mBtnBrowserClose.onClick = [this] { hideBrowser(); };
    mBtnFactory.onClick = [this]
    {
        processor.clearCustomWavetable();
        refreshWaveformDisplay();
    };

    // アタッチメント作成
    mAttachmentWtPos      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "wavetablePosition", mKnobWtPos);
    mAttachmentPulseWidth = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "pulseWidth", mKnobPulseWidth);
    mAttachmentDetune     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "detune", mKnobDetune);
    mAttachmentPorta      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "porta", mKnobPorta);

    mAttachmentWaveform   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "waveform", mComboWaveform);
    mAttachmentDetuneMode = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "detuneMode", mComboDetuneMode);
    mAttachmentDetuneSnap = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "detuneSnap", mBtnDetuneSnap);

    // 波形表示・BROWSE表示の追従
    mComboWaveform.onChange       = [this] { refreshWaveformDisplay(); updateBrowseVisibility(); };
    mKnobPulseWidth.onValueChange = [this] { refreshWaveformDisplay(); };
    mKnobWtPos.onValueChange      = [this] { refreshWaveformDisplay(); };

    // SNAP: ON中はDetuneを100ct(度数)単位に丸める
    mKnobDetune.onValueChange = [this] { applyDetuneSnap(); };
    mBtnDetuneSnap.onClick    = [this] { applyDetuneSnap(); };

    refreshWaveformDisplay();
    updateBrowseVisibility();
}

ExcitationPanel::~ExcitationPanel()
{
    mKnobWtPos.setLookAndFeel(nullptr);
    mKnobPulseWidth.setLookAndFeel(nullptr);
    mKnobDetune.setLookAndFeel(nullptr);
    mKnobPorta.setLookAndFeel(nullptr);
}

void ExcitationPanel::applyDetuneSnap()
{
    const bool snap = mBtnDetuneSnap.getToggleState();
    if (!snap)
        return;
    const double v = mKnobDetune.getValue();
    const double q = std::round(v / 100.0) * 100.0;
    if (std::abs(v - q) > 0.5)
        mKnobDetune.setValue(q, juce::sendNotificationSync);
}

void ExcitationPanel::refreshWaveformDisplay()
{
    const int type = (int)apvts.getRawParameterValue("waveform")->load();
    const float pw  = apvts.getRawParameterValue("pulseWidth")->load() * 0.01f;   // 5..95% → 0.05..0.95
    const float wt  = apvts.getRawParameterValue("wavetablePosition")->load();     // 0..1

    // カスタムWT使用中は実波形を表示へ反映
    if (type == 2 && processor.hasCustomWavetable())
    {
        float buf[256];
        processor.getExcitationEngine().getWavetable().getDisplayWave(wt, buf, 256);
        mWaveDisplay.setCustomWave(buf, 256);
        mLblCustomName.setText("WT: " + juce::File(processor.getCustomWavetablePath()).getFileName(),
                               juce::dontSendNotification);
    }
    else
    {
        mWaveDisplay.setCustomWave(nullptr, 0);
        mLblCustomName.setText("WT: Factory", juce::dontSendNotification);
    }

    mWaveDisplay.setParams(type, pw, wt);
}

void ExcitationPanel::updateBrowseVisibility()
{
    const bool wtMode = (mComboWaveform.getSelectedItemIndex() == 2);
    mBtnBrowse.setVisible(wtMode);
    mLblCustomName.setVisible(wtMode);
    if (!wtMode)
        hideBrowser();
}

void ExcitationPanel::showBrowser()
{
    if (mBrowser != nullptr)
        return;

    // 初期ディレクトリ: 前回のカスタムフォルダ (state保存) → 無ければユーザーフォルダ
    juce::File initialDir(apvts.state.getProperty("customWavetableDir",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory).getFullPathName()).toString());
    if (!initialDir.isDirectory())
        initialDir = juce::File::getSpecialLocation(juce::File::userHomeDirectory);

    mBrowser = std::make_unique<juce::FileBrowserComponent>(
        juce::FileBrowserComponent::openMode
        | juce::FileBrowserComponent::canSelectFiles
        | juce::FileBrowserComponent::filenameBoxIsReadOnly,
        initialDir, &mFileFilter, nullptr);
    mBrowser->addListener(this);
    mBrowser->setColour(juce::ListBox::backgroundColourId, SpectraColors::bg);
    addAndMakeVisible(*mBrowser);

    mBtnBrowserClose.setVisible(true);
    mBtnFactory.setVisible(true);

    // ノブ側を隠す
    for (auto* c : { (juce::Component*)&mKnobWtPos, (juce::Component*)&mKnobPulseWidth,
                     (juce::Component*)&mKnobDetune, (juce::Component*)&mKnobPorta,
                     (juce::Component*)&mLblWtPos, (juce::Component*)&mLblPulseWidth,
                     (juce::Component*)&mLblDetune, (juce::Component*)&mLblPorta,
                     (juce::Component*)&mBtnDetuneSnap })
        c->setVisible(false);

    resized();
}

void ExcitationPanel::hideBrowser()
{
    if (mBrowser == nullptr)
        return;
    mBrowser->removeListener(this);
    removeChildComponent(mBrowser.get());
    mBrowser.reset();
    mBtnBrowserClose.setVisible(false);
    mBtnFactory.setVisible(false);

    for (auto* c : { (juce::Component*)&mKnobWtPos, (juce::Component*)&mKnobPulseWidth,
                     (juce::Component*)&mKnobDetune, (juce::Component*)&mKnobPorta,
                     (juce::Component*)&mLblWtPos, (juce::Component*)&mLblPulseWidth,
                     (juce::Component*)&mLblDetune, (juce::Component*)&mLblPorta,
                     (juce::Component*)&mBtnDetuneSnap })
        c->setVisible(true);

    resized();
}

void ExcitationPanel::fileDoubleClicked(const juce::File& file)
{
    if (file.isDirectory())
        return;
    loadWavetableFile(file);
}

void ExcitationPanel::loadWavetableFile(const juce::File& file)
{
    if (processor.loadCustomWavetable(file))
    {
        apvts.state.setProperty("customWavetableDir",
                                file.getParentDirectory().getFullPathName(), nullptr);
        hideBrowser();
        refreshWaveformDisplay();
    }
    else
    {
        mLblCustomName.setText("WT: load failed", juce::dontSendNotification);
    }
}

void ExcitationPanel::paint(juce::Graphics& g)
{
    g.fillAll(SpectraColors::bg);

    auto r = getLocalBounds().toFloat().reduced(12.0f);
    g.setColour(SpectraColors::panel);
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(SpectraColors::panelLine);
    g.drawRoundedRectangle(r, 8.0f, 1.0f);
}

void ExcitationPanel::resized()
{
    auto r = getLocalBounds().reduced(16);

    // --- 左: 波形コンボ + BROWSE + 2D波形表示 + DETUNE MODE ---
    const int comboH = 26;
    const int comboW = 160;
    const int leftX = r.getX() + 16;
    mComboWaveform.setBounds(leftX, r.getY() + 20, comboW, comboH);
    mBtnBrowse.setBounds(leftX + comboW + 8, r.getY() + 20, 74, comboH);
    mLblCustomName.setBounds(leftX + comboW + 8 + 74 + 6, r.getY() + 20, 160, comboH);

    const int dispW = 288;
    const int dispH = 160;
    const int dispY = r.getY() + 20 + comboH + 12;
    mWaveDisplay.setBounds(leftX, dispY, dispW, dispH);

    mComboDetuneMode.setBounds(leftX, dispY + dispH + 10, comboW, 22);

    // --- 右: ノブ 4基 (2×2 グリッド) or ファイルブラウザ ---
    const int knobSize = 64;
    const int labelH = 14;
    const int rightX = r.getX() + 360;
    const int rightW = r.getRight() - rightX;
    const int stepX = rightW / 2;
    const int rowY1 = r.getY() + 40;
    const int rowY2 = r.getY() + 160;

    if (mBrowser != nullptr)
    {
        // ブラウザ表示中: 右エリア全体をブラウザ + 下部ボタン列
        auto area = juce::Rectangle<int>(rightX, r.getY() + 8, rightW - 8, r.getHeight() - 16);
        auto btnRow = area.removeFromBottom(28);
        mBrowser->setBounds(area);
        mBtnBrowserClose.setBounds(btnRow.removeFromRight(80).reduced(2));
        mBtnFactory.setBounds(btnRow.removeFromRight(80).reduced(2));
        return;
    }

    auto place = [&](ValueKnob& k, juce::Label& l, int gx, int gy)
    {
        const int kx = rightX + gx * stepX + (stepX - knobSize) / 2;
        k.setBounds(kx, gy, knobSize, knobSize);
        l.setBounds(kx - 10, gy + knobSize, knobSize + 20, labelH);
    };

    place(mKnobWtPos,      mLblWtPos,      0, rowY1);
    place(mKnobPulseWidth, mLblPulseWidth, 1, rowY1);
    place(mKnobPorta,      mLblPorta,      1, rowY2);

    // DETUNE: 長い値表示のため境界を左右へ拡張 + 右にSNAPボタン
    {
        const int kx = rightX + (stepX - knobSize) / 2;
        mKnobDetune.setBounds(kx - 28, rowY2, knobSize + 56, knobSize + 4);
        mLblDetune.setBounds(kx - 10, rowY2 + knobSize + 4, knobSize + 20, labelH);
        mBtnDetuneSnap.setBounds(kx + knobSize + 34, rowY2 + (knobSize - 24) / 2, 58, 24);
    }
}
