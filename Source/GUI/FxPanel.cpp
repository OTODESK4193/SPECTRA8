// ==========================================
// File: FxPanel.cpp
// ==========================================
#include "FxPanel.h"
#include "../PluginProcessor.h"

namespace
{
    // カード高さの内訳: 上下パディング8+8 / 番号行12 / TYPEコンボ22 / 余白6
    //                   / AMTノブ44 / AMTラベル12 = 112
    // ※旧92pxではノブ枠(44)を置く高さが足りず、AMTラベルが値表示と重なっていた。
    constexpr int kCardH = 112;
    constexpr int kDetailTop = kCardH + 16;
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
    typeBox.setTooltip("SLOT " + juce::String(slot + 1)
                       + " TYPE - which effect sits in this slot. Slots run left to right; "
                         "drag a card onto another to change the order.");
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
    amountKnob.setTooltip("SLOT " + juce::String(slot + 1)
                          + " AMOUNT - dry/wet balance for this effect only. "
                            "At 0 the effect is bypassed but keeps running, so its tail does "
                            "not cut off when you bring it back.");
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
    r.removeFromTop(6);

    const int kw = 44;
    // ノブ枠(kw)を確保してから、その下にラベルを置く。
    // removeFromTopで先に枠を取ることで、カードが短くても重なりが起きない。
    auto knobRow = r.removeFromTop(kw);
    amountKnob.setBounds(knobRow.getCentreX() - kw / 2, knobRow.getY(), kw, kw);
    amountLabel.setBounds(r.getX(), r.getY(), r.getWidth(), 12);
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

    // dec = 小数桁数。既定のままだと "5.0000..." のように桁があふれて省略表示になる。
    // tip = 下部ステータス行に出す英語の説明文。
    struct Def { const char* id; const char* label; int dec; const char* tip; };
    std::vector<Def> knobDefs;
    std::vector<Def> comboDefs;
    juce::String title;

    switch (getSlotType(selectedSlot))
    {
    case FxChain::Resonator:
        title = "SPECTRAL RESONATOR";
        comboDefs = { { "resMode", "MODE", 0,
                        "MODE - Chord tunes the resonators to the ROOT and CHORD below, "
                        "Free spaces them by TIME regardless of key, MIDI tunes them to the "
                        "notes you hold on the keyboard." },
                      { "resChord", "CHORD", 0,
                        "CHORD - which chord the resonators are tuned to in Chord mode." } };
        // ROOTはMIDIノート番号パラメータ。表示はパラメータ側の音名書式 (例 "A2")。
        knobDefs  = { { "resRoot", "ROOT", 0,
                        "ROOT - base note of the resonator bank, shown as a note name." },
                      { "resFreeMs", "TIME ms", 1,
                        "TIME - delay length in Free mode. Short times ring at a pitch, "
                        "long times turn into discrete echoes." },
                      { "resDecay", "DECAY", 2,
                        "DECAY - how long the resonance rings out, in seconds. "
                        "Kept constant across pitches." },
                      { "resDamp", "DAMP", 2,
                        "DAMP - rolls off the highs as the resonance decays, "
                        "so the tail gets darker and softer." },
                      { "resSpread", "SPREAD", 2,
                        "SPREAD - stereo spread of the resonator voices." },
                      { "resShimmer", "SHIMMER", 2,
                        "SHIMMER - feeds an octave-up copy back in, giving an ethereal "
                        "rising sheen." },
                      { "resInharm", "INHARM", 2,
                        "INHARM - detunes the partials away from a perfect harmonic series "
                        "for a bell-like, metallic character." } };
        break;

    case FxChain::Drive:
        title = "MULTIBAND DRIVE";
        comboDefs = { { "drvShape", "SHAPE", 0,
                        "SHAPE - the distortion curve. Each one has a different harmonic "
                        "flavour, from soft warmth to hard digital edge." } };
        knobDefs  = { { "drvDrive", "DRIVE", 1,
                        "DRIVE - how hard the signal is pushed into the distortion." },
                      { "drvLow", "LOW", 2,
                        "LOW - how much of the low band gets driven. Keep this down to "
                        "protect the bottom end from mud." },
                      { "drvMid", "MID", 2,
                        "MID - drive amount for the midrange, where most vocal presence sits." },
                      { "drvHigh", "HIGH", 2,
                        "HIGH - drive amount for the top end. Adds air and bite, but too much "
                        "gets harsh." } };
        break;

    case FxChain::Gate:
        title = "FORMANT GATE";
        comboDefs = { { "gateRate", "RATE", 0,
                        "RATE - step length, locked to the host tempo." },
                      { "gatePattern", "PATTERN", 0,
                        "PATTERN - the on/off rhythm the gate plays." } };
        knobDefs  = { { "gateDepth", "DEPTH", 2,
                        "DEPTH - how far the gate closes. At 1.0 the off steps are silent." },
                      { "gateShape", "SHAPE", 2,
                        "SHAPE - 0 holds each step flat, higher values give a sharp decay at "
                        "the start of every step so it plays like a stutter." },
                      { "gateVowel", "VOWEL", 2,
                        "VOWEL - vowel filter that moves with the gate, turning the rhythm "
                        "into a talking pattern." },
                      { "gateSmooth", "SMOOTH", 2,
                        "SMOOTH - rounds the gate edges. Low is clicky and percussive, "
                        "high fades gently between steps." } };
        break;

    case FxChain::Chorus:
        title = "ENSEMBLE CHORUS";
        knobDefs = { { "choRate", "RATE Hz", 2,
                       "RATE - speed of the chorus movement." },
                     { "choDepth", "DEPTH ms", 1,
                       "DEPTH - how far the delay time sweeps. More depth means more "
                       "pitch wobble and thickness." },
                     { "choWidth", "WIDTH", 2,
                       "WIDTH - stereo spread of the chorus voices." } };
        break;

    case FxChain::Reverb:
        title = "REVERB";
        knobDefs = { { "revSize", "SIZE", 2,
                       "SIZE - how large the simulated space is, and therefore how long "
                       "the tail lasts." },
                     { "revDamp", "DAMP", 2,
                       "DAMP - how quickly the highs disappear from the tail. "
                       "High values sound like a soft, carpeted room." },
                     { "revPredelay", "PRE-DLY ms", 0,
                       "PRE-DELAY - gap before the reverb starts. Keeps the dry voice clear "
                       "in front of the tail." },
                     { "revWidth", "WIDTH", 2,
                       "WIDTH - stereo spread of the reverb tail." },
                     { "revLowCut", "LOW CUT Hz", 0,
                       "LOW CUT - removes low frequencies from the tail so the reverb "
                       "does not muddy the bass." },
                     { "revMod", "MOD", 2,
                       "MOD - slow movement inside the tail. Stops the reverb sounding "
                       "static and metallic." } };
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
        c->setTooltip(d.tip);
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
        k->setTooltip(d.tip);
        k->setLookAndFeel(&lookAndFeel);
        addAndMakeVisible(*k);
        detailKnobAttach.push_back(
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                proc.apvts, d.id, *k));
        // Attachment生成後に設定する (生成時に既定桁数へ戻されるため)
        k->setNumDecimalPlacesToDisplay(d.dec);

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

    // 直列の流れを示す矢印 (カード間の隙間に描く)
    g.setColour(SpectraColors::textDim.withAlpha(0.5f));
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    const int gap = 8;
    const int cardW = (inner.getWidth() - (FxChain::kNumSlots - 1) * gap) / FxChain::kNumSlots;
    for (int i = 0; i < FxChain::kNumSlots - 1; ++i)
    {
        const int x = inner.getX() + (i + 1) * cardW + i * gap;
        g.drawText(">", x, inner.getY() + kCardH / 2 - 8, gap, 16, juce::Justification::centred);
    }
}

void FxPanel::resized()
{
    auto r = getLocalBounds().reduced(16);

    // --- スロットカード (横5枚・均等幅) ---
    const int gap = 8;
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

    // コンボは左から順に (Resonatorは2コンボ+7ノブが最大構成。幅はそこに合わせる)
    for (size_t i = 0; i < detailCombos.size(); ++i)
    {
        const int w = 96;
        detailComboLabels[i]->setBounds(x, rowY, w, 11);
        detailCombos[i]->setBounds(x, rowY + 13, w, 22);
        x += w + 10;
    }

    // ノブはその右へ
    const int kw = 52;
    const int step = kw + 20;
    int kx = x + 8;
    const int ky = rowY - 4;
    for (size_t i = 0; i < detailKnobs.size(); ++i)
    {
        detailKnobs[i]->setBounds(kx, ky, kw, kw);
        detailKnobLabels[i]->setBounds(kx - 10, ky + kw + 13, kw + 20, 11);
        kx += step;
    }
}
