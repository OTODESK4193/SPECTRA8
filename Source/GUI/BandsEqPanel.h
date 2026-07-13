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
    void mouseDoubleClick(const juce::MouseEvent& e) override;

private:
    void handleMouse(const juce::MouseEvent& e);
    void confirmResetAllBands(); // 右クリック→確認ダイアログ→全バンド0dB
    int  bandIndexAt(const juce::MouseEvent& e) const; // 座標→バンド番号 (-1 = 領域外)

    juce::AudioProcessorValueTreeState& apvts;
    std::array<std::atomic<float>, kMaxBands>& mBandGains;
    const std::array<std::atomic<float>, kMaxBands>& mBandLevelsForUi;

    // メーター表示用の平滑化値（描画スレッドのみが触る。ガタつき低減）
    std::array<float, kMaxBands> mMeterSmooth {};

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
