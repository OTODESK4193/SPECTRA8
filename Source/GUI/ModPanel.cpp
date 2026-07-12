// ==========================================
// File: ModPanel.cpp
// 「MOD MATRIX」タブ・パネル (Granular 準拠)
// ==========================================
#include "ModPanel.h"
#include "ColorPalette.h"
#include "../DSP/ModMatrix.h"

ModPanel::ModPanel(juce::AudioProcessorValueTreeState& state)
    : apvts(state)
{
    // ----------------------------------------------------
    // LFO GUI初期化
    // ----------------------------------------------------
    auto lfoWaves = ModMatrix::getWaveNames();
    auto lfoRates = ModMatrix::getSyncRateNames();

    for (int i = 0; i < 4; ++i)
    {
        auto& lfo = mLfoGuis[(size_t)i];

        lfo.label.setText("LFO " + juce::String(i + 1), juce::dontSendNotification);
        lfo.label.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        lfo.label.setColour(juce::Label::textColourId, SpectraColors::text);
        lfo.label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(lfo.label);

        lfo.rateSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        lfo.rateSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 14);
        lfo.rateSlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        lfo.rateSlider.setColour(juce::Slider::textBoxTextColourId, SpectraColors::textDim);
        lfo.rateSlider.setColour(juce::Slider::rotarySliderFillColourId, SpectraColors::accentMod);
        lfo.rateSlider.setColour(juce::Slider::rotarySliderOutlineColourId, SpectraColors::knobTrack);
        lfo.rateSlider.setTextValueSuffix("Hz");
        addAndMakeVisible(lfo.rateSlider);

        lfo.syncButton.setColour(juce::ToggleButton::textColourId, SpectraColors::textDim);
        lfo.syncButton.setColour(juce::ToggleButton::tickColourId, SpectraColors::accentMod);
        addAndMakeVisible(lfo.syncButton);

        lfo.rateSyncCombo.setColour(juce::ComboBox::backgroundColourId, SpectraColors::knobTrack);
        lfo.rateSyncCombo.setColour(juce::ComboBox::textColourId, SpectraColors::text);
        lfo.rateSyncCombo.setColour(juce::ComboBox::outlineColourId, SpectraColors::panelLine);
        lfo.rateSyncCombo.setJustificationType(juce::Justification::centred);
        for (int k = 0; k < lfoRates.size(); ++k)
            lfo.rateSyncCombo.addItem(lfoRates[k], k + 1);
        addAndMakeVisible(lfo.rateSyncCombo);

        lfo.waveCombo.setColour(juce::ComboBox::backgroundColourId, SpectraColors::knobTrack);
        lfo.waveCombo.setColour(juce::ComboBox::textColourId, SpectraColors::text);
        lfo.waveCombo.setColour(juce::ComboBox::outlineColourId, SpectraColors::panelLine);
        lfo.waveCombo.setJustificationType(juce::Justification::centred);
        for (int k = 0; k < lfoWaves.size(); ++k)
            lfo.waveCombo.addItem(lfoWaves[k], k + 1);
        addAndMakeVisible(lfo.waveCombo);

        // アタッチメント
        const juce::String prefix = "lfo" + juce::String(i);
        lfo.rateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, prefix + "rate", lfo.rateSlider);
        lfo.syncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, prefix + "sync", lfo.syncButton);
        lfo.rateSyncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, prefix + "rateSync", lfo.rateSyncCombo);
        lfo.waveAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, prefix + "wave", lfo.waveCombo);

        // Sync有効時にSliderを無効化するリスナーなどのバインドは簡略化
        lfo.syncButton.onClick = [this, i] { resized(); };
    }

    // ----------------------------------------------------
    // ENV GUI初期化
    // ----------------------------------------------------
    for (int i = 0; i < 3; ++i)
    {
        auto& env = mEnvGuis[(size_t)i];

        env.label.setText("ENV " + juce::String(i + 1), juce::dontSendNotification);
        env.label.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        env.label.setColour(juce::Label::textColourId, SpectraColors::text);
        env.label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(env.label);

        auto setupAdsrKnob = [this](juce::Slider& k, const juce::String& suffix)
        {
            k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            k.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 40, 14);
            k.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            k.setColour(juce::Slider::textBoxTextColourId, SpectraColors::textDim);
            k.setColour(juce::Slider::rotarySliderFillColourId, SpectraColors::accentEnv);
            k.setColour(juce::Slider::rotarySliderOutlineColourId, SpectraColors::knobTrack);
            k.setTextValueSuffix(suffix);
            addAndMakeVisible(k);
        };

        setupAdsrKnob(env.attackSlider, "s");
        setupAdsrKnob(env.decaySlider, "s");
        setupAdsrKnob(env.sustainSlider, "");
        setupAdsrKnob(env.releaseSlider, "s");

        env.loopButton.setColour(juce::ToggleButton::textColourId, SpectraColors::textDim);
        env.loopButton.setColour(juce::ToggleButton::tickColourId, SpectraColors::accentEnv);
        addAndMakeVisible(env.loopButton);

        // アタッチメント
        const juce::String prefix = "env" + juce::String(i);
        env.aAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, prefix + "attack", env.attackSlider);
        env.dAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, prefix + "decay", env.decaySlider);
        env.sAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, prefix + "sustain", env.sustainSlider);
        env.rAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, prefix + "release", env.releaseSlider);
        env.loopAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, prefix + "loop", env.loopButton);
    }

    // ----------------------------------------------------
    // Modulation Slots GUI初期化
    // ----------------------------------------------------
    auto srcNames = ModMatrix::getSourceNames();
    auto dstNames = ModMatrix::getDestNames();

    addAndMakeVisible(mViewport);
    mViewport.setViewedComponent(&mSlotContainer, false);
    mViewport.setScrollBarsShown(true, false);

    for (int i = 0; i < 16; ++i)
    {
        auto& slot = mSlotGuis[(size_t)i];

        slot.srcCombo.setColour(juce::ComboBox::backgroundColourId, SpectraColors::knobTrack);
        slot.srcCombo.setColour(juce::ComboBox::textColourId, SpectraColors::text);
        slot.srcCombo.setColour(juce::ComboBox::outlineColourId, SpectraColors::panelLine);
        for (int k = 0; k < srcNames.size(); ++k)
            slot.srcCombo.addItem(srcNames[k], k + 1);
        mSlotContainer.addAndMakeVisible(slot.srcCombo);

        slot.dstCombo.setColour(juce::ComboBox::backgroundColourId, SpectraColors::knobTrack);
        slot.dstCombo.setColour(juce::ComboBox::textColourId, SpectraColors::text);
        slot.dstCombo.setColour(juce::ComboBox::outlineColourId, SpectraColors::panelLine);
        for (int k = 0; k < dstNames.size(); ++k)
            slot.dstCombo.addItem(dstNames[k], k + 1);
        mSlotContainer.addAndMakeVisible(slot.dstCombo);

        slot.amtSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        slot.amtSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 16);
        slot.amtSlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slot.amtSlider.setColour(juce::Slider::textBoxTextColourId, SpectraColors::textDim);
        slot.amtSlider.setColour(juce::Slider::trackColourId, SpectraColors::accentMod);
        slot.amtSlider.setColour(juce::Slider::backgroundColourId, SpectraColors::knobTrack);
        mSlotContainer.addAndMakeVisible(slot.amtSlider);

        slot.uniButton.setColour(juce::ToggleButton::textColourId, SpectraColors::textDim);
        slot.uniButton.setColour(juce::ToggleButton::tickColourId, SpectraColors::accentMod);
        mSlotContainer.addAndMakeVisible(slot.uniButton);

        // アタッチメント
        const juce::String prefix = "slot" + juce::String(i);
        slot.srcAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, prefix + "src", slot.srcCombo);
        slot.dstAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, prefix + "dst", slot.dstCombo);
        slot.amtAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, prefix + "amt", slot.amtSlider);
        slot.uniAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, prefix + "uni", slot.uniButton);
    }
}

ModPanel::~ModPanel()
{
}

void ModPanel::paint(juce::Graphics& g)
{
    g.fillAll(SpectraColors::bg);

    auto r = getLocalBounds().toFloat().reduced(12.0f);
    g.setColour(SpectraColors::panel);
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(SpectraColors::panelLine);
    g.drawRoundedRectangle(r, 8.0f, 1.0f);

    // LFO/ENVセクションとスロットセクションの区切り線
    g.drawHorizontalLine(r.getY() + 110.0f, r.getX() + 10.0f, r.getRight() - 10.0f);
}

void ModPanel::resized()
{
    auto r = getLocalBounds().reduced(16);
    const int w = r.getWidth();
    
    // ----------------------------------------------------
    // 上段: LFO (4基) と ENV (3基) (Y: 0〜100)
    // ----------------------------------------------------
    auto sourceArea = r.removeFromTop(100);
    const int itemW = w / 7;

    // LFO 4基配置
    for (int i = 0; i < 4; ++i)
    {
        auto& lfo = mLfoGuis[(size_t)i];
        auto area = sourceArea.removeFromLeft(itemW).reduced(4);

        lfo.label.setBounds(area.getX(), area.getY(), area.getWidth(), 14);
        lfo.waveCombo.setBounds(area.getX(), area.getY() + 18, area.getWidth(), 20);

        const bool sync = lfo.syncButton.getToggleState();
        lfo.syncButton.setBounds(area.getX(), area.getY() + 40, area.getWidth(), 16);

        if (sync)
        {
            lfo.rateSyncCombo.setVisible(true);
            lfo.rateSlider.setVisible(false);
            lfo.rateSyncCombo.setBounds(area.getX(), area.getY() + 58, area.getWidth(), 20);
        }
        else
        {
            lfo.rateSyncCombo.setVisible(false);
            lfo.rateSlider.setVisible(true);
            lfo.rateSlider.setBounds(area.getX() + (area.getWidth() - 36) / 2, area.getY() + 58, 36, 36);
        }
    }

    // ENV 3基配置
    for (int i = 0; i < 3; ++i)
    {
        auto& env = mEnvGuis[(size_t)i];
        auto area = sourceArea.removeFromLeft(itemW).reduced(4);

        env.label.setBounds(area.getX(), area.getY(), area.getWidth(), 14);
        
        // A, D, S, R を小さく配置
        const int knobSize = 24;
        const int kY = area.getY() + 18;
        const int kXStep = area.getWidth() / 4;
        env.attackSlider.setBounds(area.getX() + kXStep * 0, kY, knobSize, knobSize + 14);
        env.decaySlider.setBounds(area.getX() + kXStep * 1, kY, knobSize, knobSize + 14);
        env.sustainSlider.setBounds(area.getX() + kXStep * 2, kY, knobSize, knobSize + 14);
        env.releaseSlider.setBounds(area.getX() + kXStep * 3, kY, knobSize, knobSize + 14);

        env.loopButton.setBounds(area.getX(), area.getY() + 60, area.getWidth(), 16);
    }

    // ----------------------------------------------------
    // 下段: モジュレーションスロット (Y: 110〜)
    // ----------------------------------------------------
    r.removeFromTop(12); // 余白
    mViewport.setBounds(r);

    // スロットコンテナのサイズ (2列に配置するため、高さは 8スロット分)
    const int slotH = 28;
    const int containerH = slotH * 8 + 16;
    mSlotContainer.setSize(r.getWidth() - 16, containerH);

    const int containerW = mSlotContainer.getWidth();
    const int colW = containerW / 2 - 8;

    for (int i = 0; i < 16; ++i)
    {
        auto& slot = mSlotGuis[(size_t)i];
        const int col = i / 8;
        const int row = i % 8;

        const int x = (col == 0) ? 4 : (containerW / 2 + 4);
        const int y = row * slotH + 8;

        // スロット内レイアウト: Src (30%) | Dst (30%) | Amt (30%) | Uni (10%)
        const int srcW = (int)(colW * 0.28f);
        const int dstW = (int)(colW * 0.28f);
        const int amtW = (int)(colW * 0.32f);
        const int uniW = colW - srcW - dstW - amtW - 8;

        slot.srcCombo.setBounds(x, y, srcW, 20);
        slot.dstCombo.setBounds(x + srcW + 2, y, dstW, 20);
        slot.amtSlider.setBounds(x + srcW + dstW + 4, y, amtW, 20);
        slot.uniButton.setBounds(x + srcW + dstW + amtW + 6, y, uniW, 20);
    }
}
