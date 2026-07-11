#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <memory>
#include "ColorPalette.h"

namespace GUI {

class MainPanel : public juce::Component {
public:
    MainPanel(juce::AudioProcessorValueTreeState& state);
    ~MainPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct DialInfo {
        juce::String paramID;
        juce::String label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void addDial(const juce::String& paramID, const juce::String& label);

    juce::AudioProcessorValueTreeState& mState;
    std::vector<DialInfo> mDials;

    // 選択波形用コンボボックス
    juce::ComboBox mWaveformCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mWaveformAttachment;
    juce::Label mWaveformLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainPanel)
};

} // namespace GUI
