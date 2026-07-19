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
        k.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 15);
        k.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k.setColour(juce::Slider::textBoxTextColourId, SpectraColors::textDim);
        k.setColour(juce::Slider::rotarySliderFillColourId, SpectraColors::accentExcitation);
        k.setColour(juce::Slider::rotarySliderOutlineColourId, SpectraColors::knobTrack);
        k.setTextValueSuffix(suffix);
        k.setLookAndFeel(&mArcLookAndFeel);
        addAndMakeVisible(k);

        l.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        l.setJustificationType(juce::Justification::centred);
        l.setColour(juce::Label::textColourId, SpectraColors::textDim);
        addAndMakeVisible(l);
    };

    setupKnob(mKnobWtPos, mLblWtPos);
    setupKnob(mKnobPulseWidth, mLblPulseWidth, "%");
    setupKnob(mKnobDetune, mLblDetune);   // 表示はパラメータ側の "N ct (P5)" 書式
    setupKnob(mKnobPorta, mLblPorta, "s");
    setupKnob(mKnobBendAmt, mLblBendAmt);
    setupKnob(mKnobBendShift, mLblBendShift);
    setupKnob(mKnobSyncAmt, mLblSyncAmt);
    setupKnob(mKnobSyncShift, mLblSyncShift);
    setupKnob(mKnobVocAmt, mLblVocAmt);
    setupKnob(mKnobVocShift, mLblVocShift);
    mKnobDetune.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 96, 15); // "1200 ct (P8)" 用

    // MORPHセクション見出し
    mLblMorphHdr.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    mLblMorphHdr.setJustificationType(juce::Justification::centredLeft);
    mLblMorphHdr.setColour(juce::Label::textColourId, SpectraColors::accentExcitation.withAlpha(0.8f));
    addAndMakeVisible(mLblMorphHdr);

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

    // 2ペインのリスト (左=サブカテゴリ / 右=波形ファイル)
    mCatList.setModel(&mCatModel);
    mWtList.setModel(&mFileModel);
    for (auto* lb : { &mCatList, &mWtList })
    {
        lb->setRowHeight(20);
        lb->setColour(juce::ListBox::backgroundColourId, SpectraColors::bg);
        lb->setColour(juce::ListBox::outlineColourId, SpectraColors::panelLine);
        addChildComponent(*lb);
    }

    // ブラウザ操作ボタン
    for (auto* b : { &mBtnBrowserClose, &mBtnFactory, &mBtnRandom })
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
        mWtList.repaint();
        refreshWaveformDisplay();
    };
    mBtnRandom.onClick = [this] { loadRandomWavetable(); };

    // アタッチメント作成
    mAttachmentWtPos      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "wavetablePosition", mKnobWtPos);
    mAttachmentPulseWidth = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "pulseWidth", mKnobPulseWidth);
    mAttachmentDetune     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "detune", mKnobDetune);
    mAttachmentPorta      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "porta", mKnobPorta);

    mAttachmentBendAmt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "bendAmt", mKnobBendAmt);
    mAttachmentBendShift  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "bendShift", mKnobBendShift);
    mAttachmentSyncAmt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "syncAmt", mKnobSyncAmt);
    mAttachmentSyncShift  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "syncShift", mKnobSyncShift);
    mAttachmentVocAmt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "vocAmt", mKnobVocAmt);
    mAttachmentVocShift   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "vocShift", mKnobVocShift);

    mAttachmentWaveform   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "waveform", mComboWaveform);
    mAttachmentDetuneMode = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "detuneMode", mComboDetuneMode);
    mAttachmentDetuneSnap = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "detuneSnap", mBtnDetuneSnap);

    // 波形表示・BROWSE表示の追従
    mComboWaveform.onChange       = [this] { refreshWaveformDisplay(); updateBrowseVisibility(); };
    mKnobPulseWidth.onValueChange = [this] { refreshWaveformDisplay(); };
    mKnobWtPos.onValueChange      = [this] { refreshWaveformDisplay(); };

    // Morphノブも波形表示に即反映
    for (auto* k : { &mKnobBendAmt, &mKnobBendShift, &mKnobSyncAmt,
                     &mKnobSyncShift, &mKnobVocAmt, &mKnobVocShift })
        k->onValueChange = [this] { refreshWaveformDisplay(); };

    // SNAP: ON中はDetuneを100ct(度数)単位に丸める
    mKnobDetune.onValueChange = [this] { applyDetuneSnap(); };
    mBtnDetuneSnap.onClick    = [this] { applyDetuneSnap(); };

    refreshWaveformDisplay();
    updateBrowseVisibility();

    startTimerHz(30);   // MODレンジ帯の更新
}

// 変調レンジ帯 / ライブ位置ドットの更新
void ExcitationPanel::timerCallback()
{
    using M = ModMatrix;
    const auto& mm = processor.getModMatrix();

    const std::pair<ValueKnob*, int> map[] = {
        { &mKnobWtPos,      M::DstWtPos },
        { &mKnobPulseWidth, M::DstPulseWidth },
        { &mKnobPorta,      M::DstPorta },
        { &mKnobDetune,     M::DstDetune },
        { &mKnobBendAmt,    M::DstBendAmt },
        { &mKnobBendShift,  M::DstBendShift },
        { &mKnobSyncAmt,    M::DstSyncAmt },
        { &mKnobSyncShift,  M::DstSyncShift },
        { &mKnobVocAmt,     M::DstVocAmt },
        { &mKnobVocShift,   M::DstVocShift },
    };

    for (const auto& e : map)
        ModRing::apply(*e.first, mm, e.second);
}

ExcitationPanel::~ExcitationPanel()
{
    stopTimer();
    // ListBox破棄時のダングリングモデル参照防止
    mCatList.setModel(nullptr);
    mWtList.setModel(nullptr);
    for (auto* k : { &mKnobWtPos, &mKnobPulseWidth, &mKnobDetune, &mKnobPorta,
                     &mKnobBendAmt, &mKnobBendShift, &mKnobSyncAmt,
                     &mKnobSyncShift, &mKnobVocAmt, &mKnobVocShift })
        k->setLookAndFeel(nullptr);
}

// Wavetableフォルダの登録パス。
//  優先度: グローバル設定 (DAW再起動をまたいで保持) → 旧セッション保存値 (後方互換)。
//  旧セッションにしか無い場合はこの場でグローバル側へ移行する。
juce::File ExcitationPanel::getWtDir() const
{
    juce::String p = SPECTRA8AudioProcessor::getGlobalWavetableDir();

    if (p.isEmpty())
    {
        // v0.2.0以前のセッションからの移行
        const juce::String legacy = apvts.state.getProperty("customWavetableDir", juce::String()).toString();
        if (legacy.isNotEmpty() && juce::File(legacy).isDirectory())
        {
            SPECTRA8AudioProcessor::setGlobalWavetableDir(legacy);
            p = legacy;
        }
    }

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
    mWaveDisplay.setMorph(apvts.getRawParameterValue("bendAmt")->load(),
                          apvts.getRawParameterValue("bendShift")->load(),
                          apvts.getRawParameterValue("syncAmt")->load(),
                          apvts.getRawParameterValue("syncShift")->load(),
                          apvts.getRawParameterValue("vocAmt")->load(),
                          apvts.getRawParameterValue("vocShift")->load());
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

// ---- Wavetableフォルダのスキャン (再帰。子フォルダ = サブカテゴリ) ----
void ExcitationPanel::rescanFolder()
{
    mCategories.clear();
    const juce::File dir = getWtDir();
    if (dir.isDirectory())
    {
        auto catFor = [this](const juce::String& key) -> WtCategory&
        {
            for (auto& c : mCategories)
                if (c.name == key)
                    return c;
            mCategories.push_back({ key, {} });
            return mCategories.back();
        };

        for (auto& f : dir.findChildFiles(juce::File::findFiles, true, "*.wav;*.aif;*.aiff"))
        {
            juce::String rel = f.getParentDirectory().getRelativePathFrom(dir);
            if (rel == ".")
                rel.clear();
            catFor(rel.isEmpty() ? dir.getFileName() : rel.replaceCharacter('\\', '/')).files.push_back(f);
        }

        std::sort(mCategories.begin(), mCategories.end(),
                  [](const WtCategory& a, const WtCategory& b)
                  { return a.name.compareIgnoreCase(b.name) < 0; });
        for (auto& c : mCategories)
            std::sort(c.files.begin(), c.files.end(),
                      [](const juce::File& a, const juce::File& b)
                      { return a.getFileName().compareIgnoreCase(b.getFileName()) < 0; });
    }

    // ロード中ファイルが属するカテゴリを選択状態にする (なければ先頭)
    mSelectedCat = 0;
    const juce::String cur = processor.getCustomWavetablePath();
    for (int c = 0; c < (int)mCategories.size(); ++c)
        for (auto& f : mCategories[(size_t)c].files)
            if (f.getFullPathName() == cur)
            {
                mSelectedCat = c;
                c = (int)mCategories.size();   // 外側ループも終了
                break;
            }

    mCatList.updateContent();
    mWtList.updateContent();
    if (!mCategories.empty())
        mCatList.selectRow(mSelectedCat);

    // 右ペイン: ロード中ファイルを選択
    if (mSelectedCat < (int)mCategories.size())
    {
        const auto& files = mCategories[(size_t)mSelectedCat].files;
        for (int i = 0; i < (int)files.size(); ++i)
            if (files[(size_t)i].getFullPathName() == cur)
            {
                mWtList.selectRow(i);
                break;
            }
    }
}

void ExcitationPanel::loadFileAt(int catIdx, int fileIdx)
{
    if (catIdx < 0 || catIdx >= (int)mCategories.size())
        return;
    const auto& files = mCategories[(size_t)catIdx].files;
    if (fileIdx < 0 || fileIdx >= (int)files.size())
        return;

    const juce::File f = files[(size_t)fileIdx];
    if (processor.loadCustomWavetable(f))
    {
        refreshWaveformDisplay();
        mWtList.repaint();
    }
    else
    {
        mLblCustomName.setText("WT: load failed - " + f.getFileNameWithoutExtension(),
                               juce::dontSendNotification);
    }
}

void ExcitationPanel::loadRandomWavetable()
{
    if (mCategories.empty())
        rescanFolder();

    // 全カテゴリの総ファイル数から一様に1つ選ぶ
    int total = 0;
    for (auto& c : mCategories)
        total += (int)c.files.size();
    if (total <= 0)
        return;

    int pick = mRng.nextInt(total);
    for (int c = 0; c < (int)mCategories.size(); ++c)
    {
        const int n = (int)mCategories[(size_t)c].files.size();
        if (pick < n)
        {
            mSelectedCat = c;
            mCatList.selectRow(c);
            mWtList.updateContent();
            mWtList.selectRow(pick);
            loadFileAt(c, pick);
            return;
        }
        pick -= n;
    }
}

void ExcitationPanel::setKnobsVisible(bool v)
{
    for (auto* c : { (juce::Component*)&mKnobWtPos, (juce::Component*)&mKnobPulseWidth,
                     (juce::Component*)&mKnobDetune, (juce::Component*)&mKnobPorta,
                     (juce::Component*)&mKnobBendAmt, (juce::Component*)&mKnobBendShift,
                     (juce::Component*)&mKnobSyncAmt, (juce::Component*)&mKnobSyncShift,
                     (juce::Component*)&mKnobVocAmt, (juce::Component*)&mKnobVocShift,
                     (juce::Component*)&mLblWtPos, (juce::Component*)&mLblPulseWidth,
                     (juce::Component*)&mLblDetune, (juce::Component*)&mLblPorta,
                     (juce::Component*)&mLblBendAmt, (juce::Component*)&mLblBendShift,
                     (juce::Component*)&mLblSyncAmt, (juce::Component*)&mLblSyncShift,
                     (juce::Component*)&mLblVocAmt, (juce::Component*)&mLblVocShift,
                     (juce::Component*)&mLblMorphHdr,
                     (juce::Component*)&mBtnDetuneSnap })
        c->setVisible(v);
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
    mCatList.setVisible(true);
    mWtList.setVisible(true);
    mBtnBrowserClose.setVisible(true);
    mBtnFactory.setVisible(true);
    mBtnRandom.setVisible(true);
    setKnobsVisible(false);
    resized();
}

void ExcitationPanel::hideBrowser()
{
    if (!mBrowserOpen)
        return;
    mBrowserOpen = false;
    mCatList.setVisible(false);
    mWtList.setVisible(false);
    mBtnBrowserClose.setVisible(false);
    mBtnFactory.setVisible(false);
    mBtnRandom.setVisible(false);
    setKnobsVisible(true);
    resized();
}

void ExcitationPanel::chooseFolder()
{
    juce::File initial = getWtDir();
    if (!initial.isDirectory())
        initial = juce::File::getSpecialLocation(juce::File::userHomeDirectory);

    // SafePointerで生存確認 (ダイアログ表示中にエディタが閉じられた場合のダングリング防止)
    juce::Component::SafePointer<ExcitationPanel> sp(this);
    mChooser = std::make_unique<juce::FileChooser>("Select Wavetable Folder", initial);
    mChooser->launchAsync(juce::FileBrowserComponent::openMode
                        | juce::FileBrowserComponent::canSelectDirectories,
        [sp](const juce::FileChooser& fc)
        {
            if (sp == nullptr)
                return;
            const juce::File dir = fc.getResult();
            if (dir.isDirectory())
            {
                // グローバル設定へ保存 = DAWを再起動しても、別プロジェクトでも残る
                SPECTRA8AudioProcessor::setGlobalWavetableDir(dir.getFullPathName());
                sp->showBrowser();   // 登録後すぐ一覧表示
            }
        });
}

// ---- ListBoxModel 転送 ----
int ExcitationPanel::ListProxy::getNumRows()
{
    if (isCat)
        return (int)owner.mCategories.size();
    if (owner.mSelectedCat < 0 || owner.mSelectedCat >= (int)owner.mCategories.size())
        return 0;
    return (int)owner.mCategories[(size_t)owner.mSelectedCat].files.size();
}

void ExcitationPanel::ListProxy::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= getNumRows())
        return;

    if (selected)
    {
        g.setColour(SpectraColors::accentExcitation.withAlpha(0.20f));
        g.fillRect(0, 0, w, h);
    }

    if (isCat)
    {
        g.setColour(selected ? SpectraColors::accentExcitation : SpectraColors::text);
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText(owner.mCategories[(size_t)row].name, 8, 0, w - 12, h,
                   juce::Justification::centredLeft);
        return;
    }

    const auto& f = owner.mCategories[(size_t)owner.mSelectedCat].files[(size_t)row];
    const bool isLoaded = (f.getFullPathName() == owner.processor.getCustomWavetablePath());
    g.setColour(isLoaded ? SpectraColors::accentExcitation : SpectraColors::text);
    g.setFont(juce::Font(juce::FontOptions(11.5f)));
    g.drawText(f.getFileNameWithoutExtension(), 8, 0, w - 12, h, juce::Justification::centredLeft);
}

void ExcitationPanel::ListProxy::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= getNumRows())
        return;

    if (isCat)
    {
        // カテゴリ切替 = 右ペインの中身を差し替え (ロードはしない)
        owner.mSelectedCat = row;
        owner.mWtList.updateContent();
        owner.mWtList.deselectAllRows();
        owner.mWtList.repaint();
        return;
    }

    // ファイルクリックで即ロード&反映 (リストは開いたまま=試聴しながら選べる)
    owner.loadFileAt(owner.mSelectedCat, row);
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
    const int comboW = 140;
    const int btnW = 62;
    const int leftX = r.getX() + 12;
    const int dispW = 280;
    mComboWaveform.setBounds(leftX, r.getY() + 18, comboW, comboH);
    mBtnBrowse.setBounds(leftX + comboW + 6, r.getY() + 18, btnW, comboH);
    mBtnAddDir.setBounds(leftX + comboW + 6 + btnW + 6, r.getY() + 18, btnW, comboH);
    mLblCustomName.setBounds(leftX, r.getY() + 18 + comboH + 2, dispW, 12);

    const int dispH = 148;
    const int dispY = r.getY() + 18 + comboH + 16;
    mWaveDisplay.setBounds(leftX, dispY, dispW, dispH);

    mComboDetuneMode.setBounds(leftX, dispY + dispH + 10, comboW, 22);

    // --- 右: ノブ 10基 (4列×3行) or 2ペインWavetableブラウザ ---
    const int rightX = r.getX() + 316;
    const int rightW = r.getRight() - rightX;

    if (mBrowserOpen)
    {
        auto area = juce::Rectangle<int>(rightX, r.getY() + 8, rightW - 4, r.getHeight() - 16);
        auto btnRow = area.removeFromBottom(28);
        auto catArea = area.removeFromLeft(juce::jmax(110, area.getWidth() / 3));
        mCatList.setBounds(catArea.withTrimmedRight(4));
        mWtList.setBounds(area);
        mBtnBrowserClose.setBounds(btnRow.removeFromRight(72).reduced(2));
        mBtnFactory.setBounds(btnRow.removeFromRight(72).reduced(2));
        mBtnRandom.setBounds(btnRow.removeFromRight(72).reduced(2));
        return;
    }

    const int knobSize = 54;
    const int labelH = 13;
    const int stepX = rightW / 4;
    // 行送り: 1行目はSNAPボタン分だけ縦に余裕を取る (54+2+13+1+18 = 88)
    const int rowY1 = r.getY() + 6;
    const int rowY2 = r.getY() + 108;
    const int rowY3 = r.getY() + 198;

    auto place = [&](ValueKnob& k, juce::Label& l, int gx, int gy)
    {
        const int kx = rightX + gx * stepX + (stepX - knobSize) / 2;
        k.setBounds(kx, gy, knobSize, knobSize);
        l.setBounds(kx - 12, gy + knobSize, knobSize + 24, labelH);
    };

    // 1行目: キャリア基本
    place(mKnobWtPos,      mLblWtPos,      0, rowY1);
    place(mKnobPulseWidth, mLblPulseWidth, 1, rowY1);
    place(mKnobPorta,      mLblPorta,      2, rowY1);

    // DETUNE: 長い値表示のため境界を左右へ拡張 + SNAPをラベル下へ
    {
        const int kx = rightX + 3 * stepX + (stepX - knobSize) / 2;
        const int extra = juce::jmax(0, (96 - knobSize) / 2);
        mKnobDetune.setBounds(kx - extra, rowY1, knobSize + extra * 2, knobSize + 2);
        mLblDetune.setBounds(kx - 12, rowY1 + knobSize + 2, knobSize + 24, labelH);
        // SNAP: GlowToggleはLED分に約20px使うため、"SNAP"(11pt bold)が入る幅を確保する
        const int snapW = 66;
        mBtnDetuneSnap.setBounds(kx + (knobSize - snapW) / 2, rowY1 + knobSize + 2 + labelH + 1, snapW, 18);
    }

    // MORPHセクション見出し
    mLblMorphHdr.setBounds(rightX + 6, rowY2 - 14, rightW - 12, 14);

    // 2行目: Bend / Sync
    place(mKnobBendAmt,   mLblBendAmt,   0, rowY2);
    place(mKnobBendShift, mLblBendShift, 1, rowY2);
    place(mKnobSyncAmt,   mLblSyncAmt,   2, rowY2);
    place(mKnobSyncShift, mLblSyncShift, 3, rowY2);

    // 3行目: Vocode
    place(mKnobVocAmt,   mLblVocAmt,   0, rowY3);
    place(mKnobVocShift, mLblVocShift, 1, rowY3);
}
