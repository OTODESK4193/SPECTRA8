// ==========================================
// File: ExcitationPanel.cpp
// 「EXCITATION」タブ・パネル (Granular 準拠)
// ==========================================
#include "ExcitationPanel.h"
#include "../PluginProcessor.h"
#include <algorithm>

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
    setupKnob(mKnobMorphAmt, mLblMorphAmt);
    setupKnob(mKnobMorphShift, mLblMorphShift);
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
    setupCombo(mComboMorphMode, { "Morph: None", "Morph: Bend +/-", "Morph: Sync", "Morph: Vocode" });

    addAndMakeVisible(mWaveDisplay);
    addAndMakeVisible(mBtnDetuneSnap);

    // BROWSE / ADD DIR ボタン (Wavetable選択時のみ表示)
    for (auto* b : { &mBtnBrowse, &mBtnAddDir })
    {
        b->setColour(juce::TextButton::buttonColourId, SpectraColors::knobTrack);
        b->setColour(juce::TextButton::textColourOffId, SpectraColors::text);
        addChildComponent(*b);
    }
    mBtnBrowse.onClick = [this] { if (mBrowserOpen) hideBrowser(); else showBrowser(); };
    mBtnAddDir.onClick = [this] { chooseFolder(); };

    // カスタムWT名ラベル
    mLblCustomName.setFont(juce::Font(juce::FontOptions(10.0f)));
    mLblCustomName.setColour(juce::Label::textColourId, SpectraColors::textDim);
    mLblCustomName.setJustificationType(juce::Justification::centredLeft);
    addChildComponent(mLblCustomName);

    // Wavetableリスト
    mWtList.setModel(this);
    mWtList.setRowHeight(22);
    mWtList.setColour(juce::ListBox::backgroundColourId, SpectraColors::bg);
    mWtList.setColour(juce::ListBox::outlineColourId, SpectraColors::panelLine);
    addChildComponent(mWtList);

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
        mWtList.deselectAllRows();
        refreshWaveformDisplay();
    };

    // アタッチメント作成
    mAttachmentWtPos      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "wavetablePosition", mKnobWtPos);
    mAttachmentPulseWidth = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "pulseWidth", mKnobPulseWidth);
    mAttachmentDetune     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "detune", mKnobDetune);
    mAttachmentPorta      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "porta", mKnobPorta);

    mAttachmentMorphAmt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "morphAmt", mKnobMorphAmt);
    mAttachmentMorphShift = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "morphShift", mKnobMorphShift);

    mAttachmentWaveform   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "waveform", mComboWaveform);
    mAttachmentDetuneMode = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "detuneMode", mComboDetuneMode);
    mAttachmentMorphMode  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "morphMode", mComboMorphMode);
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
    mWtList.setModel(nullptr);   // ListBox破棄時のダングリングモデル参照防止
    mKnobWtPos.setLookAndFeel(nullptr);
    mKnobPulseWidth.setLookAndFeel(nullptr);
    mKnobDetune.setLookAndFeel(nullptr);
    mKnobPorta.setLookAndFeel(nullptr);
    mKnobMorphAmt.setLookAndFeel(nullptr);
    mKnobMorphShift.setLookAndFeel(nullptr);
}

juce::File ExcitationPanel::getWtDir() const
{
    const juce::String p = apvts.state.getProperty("customWavetableDir", juce::String()).toString();
    return p.isNotEmpty() ? juce::File(p) : juce::File();
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
    mBtnAddDir.setVisible(wtMode);
    mLblCustomName.setVisible(wtMode);
    if (!wtMode)
        hideBrowser();
}

int ExcitationPanel::getNumRows()
{
    return (int)mWtEntries.size();
}

void ExcitationPanel::rescanFolder()
{
    mWtEntries.clear();
    const juce::File dir = getWtDir();
    if (dir.isDirectory())
    {
        // 再帰スキャンして「サブフォルダ相対パス → ファイル群」でグループ化
        std::vector<std::pair<juce::String, std::vector<juce::File>>> groups;
        auto groupFor = [&groups](const juce::String& key) -> std::vector<juce::File>&
        {
            for (auto& g : groups)
                if (g.first == key)
                    return g.second;
            groups.push_back({ key, {} });
            return groups.back().second;
        };

        for (auto& f : dir.findChildFiles(juce::File::findFiles, true, "*.wav;*.aif;*.aiff"))
        {
            juce::String rel = f.getParentDirectory().getRelativePathFrom(dir);
            if (rel == ".")
                rel.clear();
            groupFor(rel).push_back(f);
        }

        // グループをパス名順 (ルート先頭)、グループ内をファイル名順にソート
        std::sort(groups.begin(), groups.end(),
                  [](const auto& a, const auto& b)
                  { return a.first.compareIgnoreCase(b.first) < 0; });
        for (auto& g : groups)
            std::sort(g.second.begin(), g.second.end(),
                      [](const juce::File& a, const juce::File& b)
                      { return a.getFileName().compareIgnoreCase(b.getFileName()) < 0; });

        for (auto& g : groups)
        {
            // サブカテゴリヘッダ (ルート直下は親フォルダ名)
            WtEntry header;
            header.isHeader = true;
            header.label = g.first.isEmpty() ? dir.getFileName()
                                             : g.first.replaceCharacter('\\', '/');
            mWtEntries.push_back(header);

            for (auto& f : g.second)
            {
                WtEntry e;
                e.label = f.getFileNameWithoutExtension();
                e.file = f;
                mWtEntries.push_back(e);
            }
        }
    }
    mWtList.updateContent();

    // 現在ロード中のファイルがあれば選択状態にする
    const juce::String cur = processor.getCustomWavetablePath();
    for (int i = 0; i < (int)mWtEntries.size(); ++i)
        if (!mWtEntries[(size_t)i].isHeader
            && mWtEntries[(size_t)i].file.getFullPathName() == cur)
        {
            mWtList.selectRow(i);
            break;
        }
}

void ExcitationPanel::showBrowser()
{
    if (!getWtDir().isDirectory())
    {
        chooseFolder();   // フォルダ未登録ならまず登録から
        return;
    }

    rescanFolder();
    mBrowserOpen = true;
    mWtList.setVisible(true);
    mBtnBrowserClose.setVisible(true);
    mBtnFactory.setVisible(true);

    // ノブ側を隠す
    for (auto* c : { (juce::Component*)&mKnobWtPos, (juce::Component*)&mKnobPulseWidth,
                     (juce::Component*)&mKnobDetune, (juce::Component*)&mKnobPorta,
                     (juce::Component*)&mKnobMorphAmt, (juce::Component*)&mKnobMorphShift,
                     (juce::Component*)&mLblWtPos, (juce::Component*)&mLblPulseWidth,
                     (juce::Component*)&mLblDetune, (juce::Component*)&mLblPorta,
                     (juce::Component*)&mLblMorphAmt, (juce::Component*)&mLblMorphShift,
                     (juce::Component*)&mBtnDetuneSnap })
        c->setVisible(false);

    resized();
}

void ExcitationPanel::hideBrowser()
{
    if (!mBrowserOpen)
        return;
    mBrowserOpen = false;
    mWtList.setVisible(false);
    mBtnBrowserClose.setVisible(false);
    mBtnFactory.setVisible(false);

    for (auto* c : { (juce::Component*)&mKnobWtPos, (juce::Component*)&mKnobPulseWidth,
                     (juce::Component*)&mKnobDetune, (juce::Component*)&mKnobPorta,
                     (juce::Component*)&mKnobMorphAmt, (juce::Component*)&mKnobMorphShift,
                     (juce::Component*)&mLblWtPos, (juce::Component*)&mLblPulseWidth,
                     (juce::Component*)&mLblDetune, (juce::Component*)&mLblPorta,
                     (juce::Component*)&mLblMorphAmt, (juce::Component*)&mLblMorphShift,
                     (juce::Component*)&mBtnDetuneSnap })
        c->setVisible(true);

    resized();
}

void ExcitationPanel::chooseFolder()
{
    juce::File initial = getWtDir();
    if (!initial.isDirectory())
        initial = juce::File::getSpecialLocation(juce::File::userHomeDirectory);

    // SafePointerで生存確認 (ダイアログ表示中にエディタが閉じられた場合のダングリング防止)
    juce::Component::SafePointer<ExcitationPanel> sp(this);
    mChooser = std::make_unique<juce::FileChooser>("Wavetableフォルダを選択", initial);
    mChooser->launchAsync(juce::FileBrowserComponent::openMode
                        | juce::FileBrowserComponent::canSelectDirectories,
        [sp](const juce::FileChooser& fc)
        {
            if (sp == nullptr)
                return;
            const juce::File dir = fc.getResult();
            if (dir.isDirectory())
            {
                sp->apvts.state.setProperty("customWavetableDir", dir.getFullPathName(), nullptr);
                sp->showBrowser();   // 登録後すぐ一覧表示
            }
        });
}

// ---- ListBoxModel ----
void ExcitationPanel::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int)mWtEntries.size())
        return;

    const auto& e = mWtEntries[(size_t)row];

    if (e.isHeader)
    {
        // サブカテゴリヘッダ (子フォルダ名)
        g.setColour(SpectraColors::knobTrack);
        g.fillRect(0, 0, w, h);
        g.setColour(SpectraColors::accentExcitation.withAlpha(0.85f));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText(e.label, 6, 0, w - 10, h, juce::Justification::centredLeft);
        g.setColour(SpectraColors::panelLine);
        g.drawHorizontalLine(h - 1, 0.0f, (float)w);
        return;
    }

    if (selected)
    {
        g.setColour(SpectraColors::accentExcitation.withAlpha(0.20f));
        g.fillRect(0, 0, w, h);
    }
    const bool isLoaded = (e.file.getFullPathName() == processor.getCustomWavetablePath());
    g.setColour(isLoaded ? SpectraColors::accentExcitation : SpectraColors::text);
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawText(e.label, 16, 0, w - 20, h, juce::Justification::centredLeft);
}

void ExcitationPanel::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= (int)mWtEntries.size())
        return;
    const auto& e = mWtEntries[(size_t)row];
    if (e.isHeader)
    {
        mWtList.deselectAllRows();   // ヘッダは選択対象外
        return;
    }

    // クリックで即ロード&反映 (リストは開いたまま=試聴しながら選べる)
    if (processor.loadCustomWavetable(e.file))
    {
        refreshWaveformDisplay();
        mWtList.repaint();
    }
    else
    {
        mLblCustomName.setText("WT: load failed - " + e.label, juce::dontSendNotification);
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

    // --- 左: 波形コンボ + BROWSE/ADD DIR + 2D波形表示 + DETUNE MODE ---
    const int comboH = 26;
    const int comboW = 160;
    const int leftX = r.getX() + 16;
    const int dispW = 288;
    mComboWaveform.setBounds(leftX, r.getY() + 20, comboW, comboH);
    mBtnBrowse.setBounds(leftX + comboW + 8, r.getY() + 20, 70, comboH);
    mBtnAddDir.setBounds(leftX + comboW + 8 + 70 + 6, r.getY() + 20, 70, comboH);
    mLblCustomName.setBounds(leftX, r.getY() + 20 + comboH + 2, dispW, 12);

    const int dispH = 150;
    const int dispY = r.getY() + 20 + comboH + 16;
    mWaveDisplay.setBounds(leftX, dispY, dispW, dispH);

    mComboDetuneMode.setBounds(leftX, dispY + dispH + 10, comboW, 22);
    mComboMorphMode.setBounds(leftX + comboW + 8, dispY + dispH + 10, comboW, 22);

    // --- 右: ノブ 6基 (2×3 グリッド) or Wavetableリスト ---
    const int knobSize = 60;
    const int labelH = 14;
    const int rightX = r.getX() + 360;
    const int rightW = r.getRight() - rightX;
    const int stepX = rightW / 2;
    const int rowY1 = r.getY() + 16;
    const int rowY2 = r.getY() + 110;
    const int rowY3 = r.getY() + 204;

    if (mBrowserOpen)
    {
        auto area = juce::Rectangle<int>(rightX, r.getY() + 8, rightW - 8, r.getHeight() - 16);
        auto btnRow = area.removeFromBottom(28);
        mWtList.setBounds(area);
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
    place(mKnobMorphAmt,   mLblMorphAmt,   0, rowY3);
    place(mKnobMorphShift, mLblMorphShift, 1, rowY3);

    // DETUNE: 長い値表示のため境界を左右へ拡張 + 右にSNAPボタン
    {
        const int kx = rightX + (stepX - knobSize) / 2;
        mKnobDetune.setBounds(kx - 28, rowY2, knobSize + 56, knobSize + 4);
        mLblDetune.setBounds(kx - 10, rowY2 + knobSize + 4, knobSize + 20, labelH);
        mBtnDetuneSnap.setBounds(kx + knobSize + 34, rowY2 + (knobSize - 24) / 2, 58, 24);
    }
}
