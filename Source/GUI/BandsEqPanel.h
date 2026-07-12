// ==========================================
// File: BandsEqPanel.h
// 「BANDS EQ」タブ・パネル (Granular 準拠)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include "ColorPalette.h"
#include <array>
#include <atomic>

class BandsEqPanel : public juce::Component
{
public:
    static constexpr int kMaxBands = 48;

    BandsEqPanel(juce::AudioProcessorValueTreeState& state,
                 std::array<std::atomic<float>, kMaxBands>& bandGains,
                 const std::array<std::atomic<float>, kMaxBands>& bandLevelsForUi);
    ~BandsEqPanel();

    void paint(juce::Graphics& g) override;
    void resized() override;

    // レベルメーター更新用のタイマー
    void paintLevelsOnly();

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    void handleMouse(const juce::MouseEvent& e);

    juce::AudioProcessorValueTreeState& apvts;
    std::array<std::atomic<float>, kMaxBands>& mBandGains;
    const std::array<std::atomic<float>, kMaxBands>& mBandLevelsForUi;

    // メーター再描画のためのタイマー
    class MeterTimer : public juce::Timer
    {
    public:
        MeterTimer(BandsEqPanel& p) : owner(p) { startTimerHz(30); } // 30fps
        void timerCallback() override { owner.repaint(); }
    private:
        BandsEqPanel& owner;
    };
    MeterTimer mTimer;
};
