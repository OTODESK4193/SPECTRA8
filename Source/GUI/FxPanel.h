// ==========================================
// File: FxPanel.h
// 「FX」タブ (Granular FxPanel 準拠)
//  - 上部: 4スロットのカード。D&Dで適用順を並べ替え、クリックで選択。
//  - 下部: 選択中スロットのFX詳細パラメータを動的生成する Detail エリア。
//
//  並べ替えの実体は「2スロット間で Type / Amount のパラメータ値を交換する」こと。
//  DSP側(FxChain)はスロット順に直列適用するだけなので、これで適用順が入れ替わる。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <functional>
#include <memory>
#include <vector>

#include "../DSP/FxChain.h"
#include "ColorPalette.h"
#include "ValueKnob.h"
#include "HelpComboBox.h"
#include "ArcDial.h"
#include "ModRing.h"
#include "GlowToggle.h"

class SPECTRA8AudioProcessor;

// ------------------------------------------
// 1スロット分のカード
// ------------------------------------------
class FxSlotCard : public juce::Component,
                   public juce::DragAndDropTarget
{
public:
    FxSlotCard(SPECTRA8AudioProcessor& processor, int slotIndex,
               std::function<void(int, int)> onSwapCallback,
               std::function<void(int)> onSelectCallback,
               std::function<void()> onTypeChangedCallback);
    ~FxSlotCard() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setSelected(bool shouldBeSelected)
    {
        if (selected != shouldBeSelected) { selected = shouldBeSelected; repaint(); }
    }

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    // --- DragAndDropTarget ---
    bool isInterestedInDragSource(const SourceDetails& details) override;
    void itemDragEnter(const SourceDetails&) override { dragOver = true; repaint(); }
    void itemDragExit(const SourceDetails&) override { dragOver = false; repaint(); }
    void itemDropped(const SourceDetails& details) override;

private:
    SPECTRA8AudioProcessor& proc;
    const int slot;                       // 0-based
    std::function<void(int, int)> onSwap;
    std::function<void(int)> onSelect;
    std::function<void()> onTypeChanged;

    HelpComboBox typeBox;
    ValueKnob amountKnob;
    juce::Label amountLabel { {}, "AMT" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   amountAttach;

    bool dragOver = false;
    bool selected = false;

    ArcDialLookAndFeel lookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxSlotCard)
};

// ------------------------------------------
// FXタブ本体
// ------------------------------------------
class FxPanel : public juce::Component,
                public juce::DragAndDropContainer,
                private juce::Timer
{
public:
    explicit FxPanel(SPECTRA8AudioProcessor& p);
    ~FxPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    int  getSlotType(int slot) const;
    void selectSlot(int slot);
    void swapSlots(int a, int b);
    void rebuildDetails();      // 選択中FXに応じて下部のノブ/コンボを作り直す

    SPECTRA8AudioProcessor& proc;
    std::array<std::unique_ptr<FxSlotCard>, FxChain::kNumSlots> cards;
    int selectedSlot = 0;

    juce::Label detailTitle;
    juce::Label detailHint;

    // 詳細エリアは選択FXごとに作り直すため動的保持
    std::vector<std::unique_ptr<ValueKnob>>      detailKnobs;
    std::vector<std::unique_ptr<juce::Label>>    detailKnobLabels;
    std::vector<std::unique_ptr<juce::ComboBox>> detailCombos;
    std::vector<std::unique_ptr<juce::Label>>    detailComboLabels;
    std::vector<std::unique_ptr<GlowToggle>>     detailToggles;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>>   detailKnobAttach;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> detailComboAttach;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>>   detailToggleAttach;

    ArcDialLookAndFeel lookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FxPanel)
};
