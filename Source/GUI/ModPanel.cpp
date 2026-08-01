// ==========================================
// File: ModPanel.cpp
// 「MOD MATRIX」タブ・パネル (Granular 準拠)
// ==========================================
#include "ModPanel.h"

// ------------------------------------------
// 共通セットアップ
// ------------------------------------------
void ModPanel::setupSmallLabel(juce::Label& l, bool bold)
{
    l.setFont(juce::Font(juce::FontOptions(bold ? 10.5f : 9.5f,
                                           bold ? juce::Font::bold : juce::Font::plain)));
    l.setJustificationType(juce::Justification::centred);
    l.setColour(juce::Label::textColourId,
                bold ? SpectraColors::accentMod : SpectraColors::textDim);
    addAndMakeVisible(l);
}

void ModPanel::setupKnob(ValueKnob& k, const juce::String& paramID)
{
    k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 52, 14);
    k.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.setColour(juce::Slider::textBoxTextColourId, SpectraColors::textDim);
    k.setColour(juce::Slider::rotarySliderFillColourId, SpectraColors::accentMod);
    k.setColour(juce::Slider::rotarySliderOutlineColourId, SpectraColors::knobTrack);
    k.setLookAndFeel(&mArcLookAndFeel);
    addAndMakeVisible(k);
    mSliderAttach.push_back(
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID, k));
}

void ModPanel::setupCombo(HelpComboBox& c, const juce::StringArray& items, const juce::String& paramID)
{
    c.setColour(juce::ComboBox::backgroundColourId, SpectraColors::knobTrack);
    c.setColour(juce::ComboBox::textColourId, SpectraColors::text);
    c.setColour(juce::ComboBox::outlineColourId, SpectraColors::panelLine);
    c.setColour(juce::ComboBox::arrowColourId, SpectraColors::textDim);
    c.setJustificationType(juce::Justification::centredLeft);
    for (int i = 0; i < items.size(); ++i)
        c.addItem(items[i], i + 1);
    addAndMakeVisible(c);
    mComboAttach.push_back(
        std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, paramID, c));
}

void ModPanel::setupToggle(std::unique_ptr<GlowToggle>& b, const juce::String& text,
                           juce::Colour accent, const juce::String& paramID)
{
    b = std::make_unique<GlowToggle>(text, accent);
    addAndMakeVisible(*b);
    mButtonAttach.push_back(
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, paramID, *b));
}

void ModPanel::styleTab(juce::TextButton& b, bool active)
{
    b.setColour(juce::TextButton::buttonColourId,
                active ? SpectraColors::accentMod.withAlpha(0.22f) : SpectraColors::knobTrack);
    b.setColour(juce::TextButton::textColourOffId,
                active ? SpectraColors::text : SpectraColors::textDim);
    b.repaint();
}

// ------------------------------------------
ModPanel::ModPanel(juce::AudioProcessorValueTreeState& state)
    : apvts(state)
{
    // --- サブタブ ---
    for (auto* b : { &mLfoTabBtn, &mEnvTabBtn })
    {
        b->setConnectedEdges(juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
        addAndMakeVisible(*b);
    }
    mLfoTabBtn.onClick = [this] { setSourceTab(0); };
    mEnvTabBtn.onClick = [this] { setSourceTab(1); };
    mLfoTabBtn.setTooltip("LFO - show the three free-running or tempo-synced oscillators.");
    mEnvTabBtn.setTooltip("ENV - show the two envelopes. They are triggered by MIDI notes "
                          "and can be set to loop.");

    // --- LFO ×3 ---
    for (int i = 0; i < ModMatrix::kNumLfos; ++i)
    {
        auto& L = mLfos[(size_t)i];
        const juce::String prefix = "lfo" + juce::String(i);

        L.label.setText("LFO " + juce::String(i + 1), juce::dontSendNotification);
        setupSmallLabel(L.label, true);
        setupSmallLabel(L.rateLbl, false);

        setupCombo(L.waveBox, ModMatrix::getWaveNames(), prefix + "wave");
        setupCombo(L.syncRateBox, ModMatrix::getSyncRateNames(), prefix + "rateSync");
        setupKnob(L.rateKnob, prefix + "rate");
        setupToggle(L.syncBtn, "SYNC", SpectraColors::accentMod, prefix + "sync");

        const juce::String n = juce::String(i + 1);
        L.waveBox.setTooltip("LFO " + n + " WAVE - Sine and Triangle are smooth, Saw and Square "
                             "step abruptly, S&H jumps to a new random value each cycle, "
                             "Chaos layers two out-of-tune sines for a drifting, never-repeating shape.");
        L.waveBox.setItemHelp(
            { "SINE - the smoothest shape. Best for gentle vibrato, tremolo and slow formant sweeps.",
              "TRIANGLE - linear rise and fall. Similar to Sine but with a sharper turn at the peaks.",
              "SAW - ramps up then drops instantly. Good for repeating sweeps that always restart "
              "from the same place.",
              "SQUARE - jumps between two values with nothing in between. Use it to flip a knob "
              "between two settings in time.",
              "S&H - holds a new random value for each cycle. Stepped randomness, great on "
              "PITCH Q, WT POSITION or FMT SHIFT.",
              "CHAOS - two sines at an irrational ratio. Drifts and never repeats, so it sounds "
              "organic rather than looped." });
        L.rateKnob.setTooltip("LFO " + n + " RATE - free-running speed in Hz. "
                              "Ignored while SYNC is lit.");
        L.syncRateBox.setTooltip("LFO " + n + " SYNC RATE - note division locked to the host tempo. "
                                 "Only used while SYNC is lit.");
        L.syncBtn->setTooltip("SYNC - lock LFO " + n + " to the host tempo instead of a free Hz rate.");
    }

    // --- ENV ×2 ---
    for (int i = 0; i < ModMatrix::kNumEnvs; ++i)
    {
        auto& E = mEnvs[(size_t)i];
        const juce::String prefix = "env" + juce::String(i);

        E.label.setText("ENV " + juce::String(i + 1), juce::dontSendNotification);
        setupSmallLabel(E.label, true);
        for (auto* l : { &E.la, &E.ld, &E.ls, &E.lr })
            setupSmallLabel(*l, false);

        setupKnob(E.a, prefix + "attack");
        setupKnob(E.d, prefix + "decay");
        setupKnob(E.s, prefix + "sustain");
        setupKnob(E.r, prefix + "release");
        setupToggle(E.loopBtn, "LOOP", SpectraColors::accentMod, prefix + "loop");

        const juce::String n = juce::String(i + 1);
        E.a.setTooltip("ENV " + n + " ATTACK - time to rise to full after a note is pressed.");
        E.d.setTooltip("ENV " + n + " DECAY - time to fall from full down to the sustain level. "
                       "With LOOP on this is the fall time of the repeating cycle.");
        E.s.setTooltip("ENV " + n + " SUSTAIN - level held while a key stays pressed.");
        E.r.setTooltip("ENV " + n + " RELEASE - time to fall back to zero after the key is let go.");
        E.loopBtn->setTooltip("LOOP - ENV " + n + " repeats attack and decay continuously, "
                              "turning it into a tempo-free extra LFO with its own shape.");
    }

    // --- スロット ×6 ---
    setupSmallLabel(mSlotHdr, false);
    mSlotHdr.setJustificationType(juce::Justification::centredLeft);

    for (int i = 0; i < ModMatrix::kNumSlots; ++i)
    {
        auto& S = mSlots[(size_t)i];
        const juce::String prefix = "slot" + juce::String(i);

        S.rowLabel.setText(juce::String(i + 1), juce::dontSendNotification);
        setupSmallLabel(S.rowLabel, false);

        setupCombo(S.srcBox, ModMatrix::getSourceNames(), prefix + "src");
        setupCombo(S.dstBox, ModMatrix::getDestNames(), prefix + "dst");
        setupToggle(S.uniBtn, "UNI", SpectraColors::accentMod, prefix + "uni");

        const juce::String n = juce::String(i + 1);
        S.srcBox.setTooltip("SLOT " + n + " SOURCE - what does the modulating. LFO and ENV come "
                            "from this tab; Velocity, Note, Mod Wheel and Random come from MIDI.");
        S.dstBox.setTooltip("SLOT " + n + " DESTINATION - which knob gets modulated. The affected "
                            "range is drawn as a pink band around that knob.");
        S.uniBtn->setTooltip("UNI - unipolar. The source only pushes the knob in one direction "
                             "(0 to +1) instead of swinging both ways (-1 to +1).");
        S.amtKnob.setTooltip("SLOT " + n + " AMOUNT - depth and direction of the modulation. "
                             "Negative values invert the source.");

        // AMTは横スライダー (行が細いのでロータリーだと潰れる)
        S.amtKnob.setSliderStyle(juce::Slider::LinearHorizontal);
        S.amtKnob.setTextBoxStyle(juce::Slider::TextBoxRight, false, 44, 18);
        S.amtKnob.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        S.amtKnob.setColour(juce::Slider::textBoxTextColourId, SpectraColors::textDim);
        S.amtKnob.setColour(juce::Slider::trackColourId, SpectraColors::accentMod);
        S.amtKnob.setColour(juce::Slider::backgroundColourId, SpectraColors::knobTrack);
        S.amtKnob.setColour(juce::Slider::thumbColourId, SpectraColors::text);
        addAndMakeVisible(S.amtKnob);
        mSliderAttach.push_back(
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                apvts, prefix + "amt", S.amtKnob));
    }

    setSourceTab(0);
}

ModPanel::~ModPanel()
{
    for (auto& L : mLfos)
        L.rateKnob.setLookAndFeel(nullptr);
    for (auto& E : mEnvs)
        for (auto* k : { &E.a, &E.d, &E.s, &E.r })
            k->setLookAndFeel(nullptr);
}

// ------------------------------------------
void ModPanel::setSourceTab(int t)
{
    mActiveSrcTab = juce::jlimit(0, 1, t);
    const bool lfo = (mActiveSrcTab == 0);

    for (auto& L : mLfos)
    {
        L.label.setVisible(lfo);
        L.waveBox.setVisible(lfo);
        L.syncRateBox.setVisible(lfo);
        L.rateKnob.setVisible(lfo);
        L.rateLbl.setVisible(lfo);
        if (L.syncBtn) L.syncBtn->setVisible(lfo);
    }
    for (auto& E : mEnvs)
    {
        E.label.setVisible(!lfo);
        for (auto* k : { &E.a, &E.d, &E.s, &E.r })
            k->setVisible(!lfo);
        for (auto* l : { &E.la, &E.ld, &E.ls, &E.lr })
            l->setVisible(!lfo);
        if (E.loopBtn) E.loopBtn->setVisible(!lfo);
    }

    styleTab(mLfoTabBtn, lfo);
    styleTab(mEnvTabBtn, !lfo);
    resized();
}

void ModPanel::paint(juce::Graphics& g)
{
    g.fillAll(SpectraColors::bg);

    auto r = getLocalBounds().toFloat().reduced(12.0f);
    g.setColour(SpectraColors::panel);
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(SpectraColors::panelLine);
    g.drawRoundedRectangle(r, 8.0f, 1.0f);

    // ソース段とスロット段の区切り線
    auto inner = getLocalBounds().reduced(16);
    const int sepY = inner.getY() + 24 + 104;
    g.setColour(SpectraColors::panelLine);
    g.drawHorizontalLine(sepY, (float)inner.getX() + 4.0f, (float)inner.getRight() - 4.0f);
}

void ModPanel::resized()
{
    auto r = getLocalBounds().reduced(16);

    // --- サブタブ ---
    const int tabW = 72, tabH = 20;
    mLfoTabBtn.setBounds(r.getX(), r.getY(), tabW, tabH);
    mEnvTabBtn.setBounds(r.getX() + tabW, r.getY(), tabW, tabH);

    // --- ソース段 (y: r.getY()+24 から高さ104) ---
    const int srcY = r.getY() + 26;
    const int knobSz = 44;

    if (mActiveSrcTab == 0)
    {
        // LFO ×3 を横並び
        const int colW = r.getWidth() / ModMatrix::kNumLfos;
        for (int i = 0; i < ModMatrix::kNumLfos; ++i)
        {
            auto& L = mLfos[(size_t)i];
            const int x = r.getX() + i * colW;

            L.label.setBounds(x, srcY, 60, 14);
            L.waveBox.setBounds(x, srcY + 18, 104, 22);
            if (L.syncBtn) L.syncBtn->setBounds(x + 110, srcY + 18, 66, 22);
            L.rateKnob.setBounds(x + 4, srcY + 46, knobSz, knobSz);
            L.rateLbl.setBounds(x - 4, srcY + 46 + knobSz, knobSz + 16, 12);
            L.syncRateBox.setBounds(x + 60, srcY + 56, 80, 22);
        }
    }
    else
    {
        // ENV ×2 を横並び (A D S R + LOOP)
        const int colW = r.getWidth() / ModMatrix::kNumEnvs;
        for (int i = 0; i < ModMatrix::kNumEnvs; ++i)
        {
            auto& E = mEnvs[(size_t)i];
            const int x = r.getX() + i * colW;

            E.label.setBounds(x, srcY, 60, 14);
            if (E.loopBtn) E.loopBtn->setBounds(x + 64, srcY - 2, 66, 20);

            ValueKnob* ks[4] = { &E.a, &E.d, &E.s, &E.r };
            juce::Label* ls[4] = { &E.la, &E.ld, &E.ls, &E.lr };
            for (int j = 0; j < 4; ++j)
            {
                const int kx = x + 4 + j * (knobSz + 26);
                ks[j]->setBounds(kx, srcY + 20, knobSz, knobSz);
                ls[j]->setBounds(kx, srcY + 20 + knobSz + 14, knobSz, 12);
            }
        }
    }

    // --- スロット段 ---
    const int slotTop = srcY + 104;
    mSlotHdr.setBounds(r.getX() + 22, slotTop - 2, r.getWidth() - 22, 12);

    const int rowH = 25;
    const int numW = 16;
    const int srcW = 108;
    const int dstW = 128;
    const int uniW = 58;

    for (int i = 0; i < ModMatrix::kNumSlots; ++i)
    {
        auto& S = mSlots[(size_t)i];
        const int y = slotTop + 12 + i * rowH;
        int x = r.getX();

        S.rowLabel.setBounds(x, y, numW, rowH - 4);
        x += numW + 4;
        S.srcBox.setBounds(x, y, srcW, rowH - 4);
        x += srcW + 6;
        S.dstBox.setBounds(x, y, dstW, rowH - 4);
        x += dstW + 8;

        // AMTスライダーは残り幅から UNI ボタン分を引いた領域いっぱい
        const int amtW = juce::jmax(80, r.getRight() - x - uniW - 8);
        S.amtKnob.setBounds(x, y, amtW, rowH - 4);
        x += amtW + 6;
        if (S.uniBtn) S.uniBtn->setBounds(x, y, uniW, rowH - 4);
    }
}
