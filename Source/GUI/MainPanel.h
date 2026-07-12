#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <memory>
#include "ColorPalette.h"
#include "ValueKnob.h"
#include "ArcDial.h"

namespace GUI {

class VocoderTabPanel : public juce::Component
{
public:
    VocoderTabPanel(juce::AudioProcessorValueTreeState& state);
    ~VocoderTabPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct DialInfo {
        juce::String paramID;
        juce::String label;
        std::unique_ptr<ValueKnob> slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void addDial(const juce::String& paramID, const juce::String& label, juce::Colour fillColour);

    juce::AudioProcessorValueTreeState& mState;
    std::vector<DialInfo> mDials;

    juce::ComboBox mVocoderModeCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mVocoderModeAttachment;
    juce::Label mVocoderModeLabel;

    juce::ComboBox mModeCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mModeAttachment;
    juce::Label mModeLabel;

    juce::ComboBox mLimiterCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mLimiterAttachment;
    juce::Label mLimiterLabel;

    juce::ComboBox mWindowTypeCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mWindowTypeAttachment;
    juce::Label mWindowTypeLabel;

    juce::ComboBox mInterpolationModeCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mInterpolationModeAttachment;
    juce::Label mInterpolationModeLabel;

    juce::ComboBox mFilterbankTypeCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mFilterbankTypeAttachment;
    juce::Label mFilterbankTypeLabel;

    ArcDialLookAndFeel mArcLookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocoderTabPanel)
};

class ExcitationTabPanel : public juce::Component
{
public:
    ExcitationTabPanel(juce::AudioProcessorValueTreeState& state);
    ~ExcitationTabPanel() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct DialInfo {
        juce::String paramID;
        juce::String label;
        std::unique_ptr<ValueKnob> slider;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void addDial(const juce::String& paramID, const juce::String& label, juce::Colour fillColour);

    juce::AudioProcessorValueTreeState& mState;
    std::vector<DialInfo> mDials;

    juce::ComboBox mWaveformCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mWaveformAttachment;
    juce::Label mWaveformLabel;

    ArcDialLookAndFeel mArcLookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExcitationTabPanel)
};

} // namespace GUI