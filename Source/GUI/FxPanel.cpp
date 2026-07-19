// ==========================================
// File: FxPanel.cpp
// ==========================================
#include "FxPanel.h"
#include "../PluginProcessor.h"

namespace
{
    constexpr int kCardH = 92;    // スロットカードの高さ
    constexpr int kDetailTop = 108;
}

// ==========================================
// FxSlotCard
// ==========================================
FxSlotCard::FxSlotCard(SPECTRA8AudioProcessor& processor, int slotIndex,
                       std::function<void(int, int)> onSwapCallback,
                       std::function<void(int)> onSelectCallback,
                       std::function<void()> onTypeChangedCallback)
    : proc(processor), slot(slotIndex),
      onSwap(std::move(onSwapCallback)),
      onSelect(std::move(onSelectCallback)),
      onTypeChanged(std::move(onTypeChangedCallback))
{
    const juce::String pre = "fx" + juce::String(slot + 1);

    typeBox.setColour(juce::ComboBox::backgroundColourId, SpectraColors::knobTrack);
    typeBox.setColour(juce::ComboBox::textColourId, SpectraColors::text);
    typeBox.setColour(juce::ComboBox::outlineColourId, SpectraColors::panelLine);
    typeBox.setColour(juce::ComboBox::arrowColourId, SpectraColors::textDim);
    typeBox.setJustificationType(juce::Justification::centred);
    typeBox.addItemList(FxChain::getTypeNames(), 1);
    addAndMakeVisible(typeBox);
    typeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        proc.apvts, pre + "Type", typeBox);

    // 種類を変えたら詳細エリアも作り直す
    typeBox.onChange = [this]
    {
        if (onSelect) onSelect(slot);
        if (onTypeChanged) onTypeChanged();
        repaint();
    };

    amountKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    amountKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 14);
    amountKnob.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    amountKnob.setColour(juce::Slider::textBoxTextColourId, SpectraColors::textDim);
    amountKnob.setColour(juce::Slider::rotarySliderFillColourId, SpectraColors::accentFx);
    amountKnob.setColour(juce::Slider::rotarySliderOutlineColourId, SpectraColors::knobTrack);
    amountKnob.setLookAndFeel(&lookAndFeel);
    addAndMakeVisible(amountKnob);
    amountAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, pre + "Amount", amountKnob);

    amountLabel.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    amountLabel.setJustificationType(juce::Justification::centred);
    amountLabel.setColour(juce::Label::textColourId, SpectraColors::textDim);
    addAndMakeVisible(amountLabel);
}

FxSlotCard::~FxSlotCard()
{
    amountKnob.setLookAndFeel(nullptr);
}

void FxSlotCard::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(SpectraColors::panel.brighter(selected ? 0.10f : 0.03f));
    g.fillRoundedRectangle(bounds.reduced(0.75f), 10.0f);

    juce::Colour border = SpectraColors::panelLine;
    float borderW = 1.0f;
    if (dragOver)      { border = SpectraColors::mint.withAlpha(0.9f);       borderW = 2.0f; }
    else if (selected) { border = SpectraColors::accentFx.withAlpha(0.85f);  borderW = 1.6f; }
    g.setColour(border);
    g.drawRoundedRectangle(bounds.reduced(0.75f), 10.0f, borderW);

    // スロット番号 (= 適用順)
    g.setColour(selected ? SpectraColors::accentFx : SpectraColors::textDim);
    g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    g.drawText(juce::String(slot + 1), 8, 4, 20, 12, juce::Justification::centredLeft);

    // ドラッグ用グリップ (見た目のヒント)
    g.setColour(SpectraColors::textDim.withAlpha(0.4f));
    for (int i = 0; i < 3; ++i)
        g.fillRect(getWidth() - 16, 7 + i * 4, 8, 2);
}

void FxSlotCard::resized()
{
    auto r = getLocalBounds().reduced(8);
    r.removeFromTop(12);                       // 番号行
    typeBox.setBounds(r.removeFromTop(22));
    r.removeFromTop(4);

    const int kw = 44;
    auto knobArea = r.removeFromTop(kw + 14);
    amountKnob.setBounds(knobArea.getCentreX() - kw / 2, knobArea.getY(), kw, kw);
    amountLabel.setBounds(knobArea.getX(), knobArea.getBottom() - 11, knobArea.getWidth(), 10);
}

void FxSlotCard::mouseDown(const juce::MouseEvent&)
{
    if (onSelect) onSelect(slot);
}

void FxSlotCard::mouseDrag(const juce::MouseEvent& e)
{
    if (e.getDistanceFromDragStart() > 6)
        if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
            if (!container->isDragAndDropActive())
                container->startDragging(juce::var(slot), this);
}

bool FxSlotCard::isInterestedInDragSource(const SourceDetails& details)
{
    return details.description.isInt() && (int)details.description != slot;
}

void FxSlotCard::itemDropped(const SourceDetails& details)
{
    dragOver = false;
    repaint();
    if (onSwap != nullptr)
        onSwap((int)details.description, slot);
}

// ==========================================
// FxPanel
// ==========================================
FxPanel::FxPanel(SPECTRA8AudioProcessor& p) : proc(p)
{
    for (int i = 0; i < FxChain::kNumSlots; ++i)
    {
        cards[(size_t)i] = std::make_unique<FxSlotCard>(proc, i,
            [this](int a, int b) { swapSlots(a, b); },
            [this](int s) { selectSlot(s); },
            [this] { rebuildDetails(); });
        addAndMakeVisible(*cards[(size_t)i]);
    }

    detailTitle.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    detailTitle.setColour(juce::Label::textColourId, SpectraColors::accentFx);
    detailTitle.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(detailTitle);

    detailHint.setFont(juce::Font(juce::FontOptions(9.5f)));
    detailHint.setColour(juce::Label::textColourId, SpectraColors::textDim);
    detailHint.setJustificationType(juce::Justification::centredLeft);
    detailHint.setText("Drag a card to reorder the chain", juce::dontSendNotification);
    addAndMakeVisible(detailHint);

    selectSlot(0);
}

FxPanel::~FxPanel()
{
    for (auto& k : detailKnobs)
        if (k) k->setLookAndFeel(nullptr);
}

int FxPanel::getSlotType(int slot) const
{
    return (int)proc.apvts.getRawParameterValue("fx" + juce::String(slot + 1) + "Type")->load();
}

void FxPanel::selectSlot(int slot)
{
    selectedSlot = juce::jlimit(0, FxChain::kNumSlots - 1, slot);
    for (int i = 0; i < FxChain::kNumSlots; ++i)
        if (cards[(size_t)i] != nullptr)
            cards[(size_t)i]->setSelected(i == selectedSlot);
    rebuildDetails();
    repaint();
}

// 並べ替え = 2スロットの Type / Amount を交換する。
// DSPはスロット順に直列適用するだけなので、これで適用順が入れ替わる。
void FxPanel::swapSlots(int a, int b)
{
    if (a == b || a < 0 || b < 0 || a >= FxChain::kNumSlots || b >= FxChain::kNumSlots)
        return;

    for (const char* suffix : { "Type", "Amount" })
    {
        auto* pa = proc.apvts.getParameter("fx" + juce::String(a + 1) + suffix);
        auto* pb = proc.apvts.getParameter("fx" + juce::String(b + 1) + suffix);
        if (pa == nullptr || pb == nullptr) continue;

        const float va = pa->getValue();
        const float vb = pb->getValue();

        pa->beginChangeGesture(); pa->setValueNotifyingHost(vb); pa->endChangeGesture();
        pb->beginChangeGesture(); pb->setValueNotifyingHost(va); pb->endChangeGesture();
    }

    selectSlot(b);   // 移動先を選択状態に
}

// ------------------------------------------
// 詳細エリアの再構築
// ------------------------------------------
void FxPanel::rebuildDetails()
{
    // 破棄は Attachment → Component の順 (逆にするとダングリング参照になる)
    detailKnobAttach.clear();
    detailComboAttach.clear();
    for (auto& k : detailKnobs)
        if (k) k->setLookAndFeel(nullptr);
    detailKnobs.clear();
    detailKnobLabels.clear();
    detailCombos.clear();
    detailComboLabels.clear();

    struct Def { const char* id; const char* label; };
    std::vector<Def> knobDefs;
    std::vector<Def> comboDefs;
    juce::String title;

    switch (getSlotType(selectedSlot))
    {
    case FxChain::Resonator:
        title = "SPECTRAL RESONATOR";
        comboDefs = { { "resMode", "MODE" }, { "resChord", "CHORD" } };
        knobDefs  = { { "resRoot", "ROOT" }, { "resFreeMs", "TIME" }, { "resFeedback", "FEEDBACK" },
                      { "resDamp", "DAMP" }, { "resSpread", "SPREAD" } };
        break;

    case FxChain::Drive:
        title = "MULTIBAND DRIVE";
        comboDefs = { { "drvShape", "SHAPE" } };
        knobDefs  = { { "drvDrive", "DRIVE" }, { "drvLow", "LOW" },
                      { "drvMid", "MID" }, { "drvHigh", "HIGH" } };
        break;

    case FxChain::Gate:
        title = "FORMANT GATE";
        comboDefs = { { "gateRate", "RATE" }, { "gatePattern", "PATTERN" } };
        knobDefs  = { { "gateDepth", "DEPTH" }, { "gateVowel", "VOWEL" }, { "gateSmooth", "SMOOTH" } };
        break;

    case FxChain::Chorus:
        title = "ENSEMBLE CHORUS";
        knobDefs = { { "choRate", "RATE" }, { "choDepth", "DEPTH" }, { "choWidth", "WIDTH" } };
        break;

    case FxChain::Reverb:
        title = "REVERB";
        knobDefs = { { "revSize", "SIZE" }, { "revDamp", "DAMP" } };
        break;

    default:
        title = "SLOT " + juce::String(selectedSlot + 1) + "  -  empty";
        break;
    }

    detailTitle.setText(title, juce::dontSendNotification);

    for (const auto& d : comboDefs)
    {
        auto c = std::make_unique<juce::ComboBox>();
        c->setColour(juce::ComboBox::backgroundColourId, SpectraColors::knobTrack);
        c->setColour(juce::ComboBox::textColourId, SpectraColors::text);
        c->setColour(juce::ComboBox::outlineColourId, SpectraColors::panelLine);
        c->setColour(juce::ComboBox::arrowColourId, SpectraColors::textDim);
        c->setJustificationType(juce::Justification::centred);
        if (auto* cp = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter(d.id)))
            c->addItemList(cp->choices, 1);
        addAndMakeVisible(*c);
        detailComboAttach.push_back(
            std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
                proc.apvts, d.id, *c));

        auto l = std::make_unique<juce::Label>();
        l->setText(d.label, juce::dontSendNotification);
        l->setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        l->setJustificationType(juce::Justification::centred);
        l->setColour(juce::Label::textColourId, SpectraColors::textDim);
        addAndMakeVisible(*l);

        detailCombos.push_back(std::move(c));
        detailComboLabels.push_back(std::move(l));
    }

    for (const auto& d : knobDefs)
    {
        auto k = std::make_unique<ValueKnob>();
        k->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 14);
        k->setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        k->setColour(juce::Slider::textBoxTextColourId, SpectraColors::textDim);
        k->setColour(juce::Slider::rotarySliderFillColourId, SpectraColors::accentFx);
        k->setColour(juce::Slider::rotarySliderOutlineColourId, SpectraColors::knobTrack);
        k->setLookAndFeel(&lookAndFeel);
        addAndMakeVisible(*k);
        detailKnobAttach.push_back(
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                proc.apvts, d.id, *k));

        auto l = std::make_unique<juce::Label>();
        l->setText(d.label, juce::dontSendNotification);
        l->setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        l->setJustificationType(juce::Justification::centred);
        l->setColour(juce::Label::textColourId, SpectraColors::textDim);
        addAndMakeVisible(*l);

        detailKnobs.push_back(std::move(k));
        detailKnobLabels.push_back(std::move(l));
    }

    resized();
}

void FxPanel::paint(juce::Graphics& g)
{
    g.fillAll(SpectraColors::bg);

    auto r = getLocalBounds().toFloat().reduced(12.0f);
    g.setColour(SpectraColors::panel);
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(SpectraColors::panelLine);
    g.drawRoundedRectangle(r, 8.0f, 1.0f);

    // カード段と詳細段の区切り
    auto inner = getLocalBounds().reduced(16);
    g.setColour(SpectraColors::panelLine);
    g.drawHorizontalLine(inner.getY() + kDetailTop - 8,
                         (float)inner.getX() + 4.0f, (float)inner.getRight() - 4.0f);

    // 直列の流れを示す矢印
    g.setColour(SpectraColors::textDim.withAlpha(0.5f));
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    const int cardW = (inner.getWidth() - 3 * 10) / FxChain::kNumSlots;
    for (int i = 0; i < FxChain::kNumSlots - 1; ++i)
    {
        const int x = inner.getX() + (i + 1) * cardW + i * 10;
        g.drawText(">", x, inner.getY() + kCardH / 2 - 8, 10, 16, juce::Justification::centred);
    }
}

void FxPanel::resized()
{
    auto r = getLocalBounds().reduced(16);

    // --- スロットカード (横4枚) ---
    const int gap = 10;
    const int cardW = (r.getWidth() - (FxChain::kNumSlots - 1) * gap) / FxChain::kNumSlots;
    for (int i = 0; i < FxChain::kNumSlots; ++i)
        if (cards[(size_t)i] != nullptr)
            cards[(size_t)i]->setBounds(r.getX() + i * (cardW + gap), r.getY(), cardW, kCardH);

    // --- 詳細エリア ---
    const int dy = r.getY() + kDetailTop;
    detailTitle.setBounds(r.getX() + 2, dy, 260, 14);
    detailHint.setBounds(r.getRight() - 240, dy, 240, 14);

    int x = r.getX() + 2;
    const int rowY = dy + 22;

    // コンボは左から順に
    for (size_t i = 0; i < detailCombos.size(); ++i)
    {
        const int w = 108;
        detailComboLabels[i]->setBounds(x, rowY, w, 11);
        detailCombos[i]->setBounds(x, rowY + 13, w, 22);
        x += w + 12;
    }

    // ノブはその右へ
    const int kw = 52;
    const int step = kw + 22;
    int kx = x + 8;
    const int ky = rowY - 4;
    for (size_t i = 0; i < detailKnobs.size(); ++i)
    {
        detailKnobs[i]->setBounds(kx, ky, kw, kw);
        detailKnobLabels[i]->setBounds(kx - 10, ky + kw + 13, kw + 20, 11);
        kx += step;
    }
}
