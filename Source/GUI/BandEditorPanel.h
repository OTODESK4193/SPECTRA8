#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "ColorPalette.h"

namespace GUI {

class BandEditorPanel : public juce::Component, private juce::Timer
{
public:
    BandEditorPanel(SPECTRA8AudioProcessor& processor);
    ~BandEditorPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    void timerCallback() override;
    void handleMouse(const juce::MouseEvent& e);

    SPECTRA8AudioProcessor& mProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandEditorPanel)
};

} // namespace GUI