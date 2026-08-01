// ==========================================
// File: PresetPanel.h
// SPECTRA8 - プリセットブラウザパネル (PicoSampler 準拠)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "../Presets/PresetManager.h"
#include "ColorPalette.h"

class PresetPanel : public juce::Component
{
public:
    explicit PresetPanel(juce::AudioProcessorValueTreeState& apvts);
    ~PresetPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void refreshAll();

private:
    void refreshPresetList();
    void categoryRowClicked(int row);
    void presetRowClicked(int row);
    void presetRowRightClicked(int row);
    void toggleFavoriteClicked(int row);
    void showSaveDialog();
    void showInitDialog();
    void confirmDelete(const PresetItem& item);

    struct SimpleListModel : public juce::ListBoxModel
    {
        PresetPanel* owner = nullptr;
        bool isCategoryList = false;

        int getNumRows() override;
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override;
        void listBoxItemClicked(int row, const juce::MouseEvent& e) override;
        void listBoxItemDoubleClicked(int row, const juce::MouseEvent& e) override;
        void returnKeyPressed(int row) override;
    };

    PresetManager manager;

    juce::TextEditor txtSearch;
    juce::TextButton btnSave { "SAVE" };
    juce::TextButton btnInit { "INIT" };
    juce::ListBox lstCategories;
    juce::ListBox lstPresets;

    SimpleListModel categoryListModel;
    SimpleListModel presetListModel;

    juce::StringArray categories;
    juce::Array<PresetItem> allPresets;
    juce::Array<PresetItem> currentPresets;
    int selectedCategoryRow = 0;
    juce::String lastSavedName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetPanel)
};
