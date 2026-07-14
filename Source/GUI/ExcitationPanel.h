// ==========================================
// File: ExcitationPanel.h
// 「EXCITATION」タブ・パネル (Granular 準拠)
//  - 左: 波形選択コンボ + 2D波形表示エリア
//  - 右: キャリア関連ノブ (WT POS / PULSE WIDTH / DETUNE / PORTA)
//  ※ LOFI / BASE PITCH / NOISE COLOR / NOISE MIX は VOCODER タブへ移設。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <cmath>
#include "ColorPalette.h"
#include "ValueKnob.h"
#include "ArcDial.h"

// キャリア波形の概形を描く2D表示エリア（波形タイプ / パルス幅 / WT位置で形が変化）
class WaveformDisplay : public juce::Component
{
public:
    void setParams(int waveformType, float pulseWidth01, float wtPos01)
    {
        if (waveformType != mType || std::abs(pulseWidth01 - mPulse) > 1e-4f
            || std::abs(wtPos01 - mWt) > 1e-4f)
        {
            mType = waveformType;
            mPulse = pulseWidth01;
            mWt = wtPos01;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(2.0f);
        g.setColour(SpectraColors::bg);
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(SpectraColors::panelLine);
        g.drawRoundedRectangle(r, 6.0f, 1.0f);

        // 中央のゼロ線
        const float midY = r.getCentreY();
        g.setColour(SpectraColors::grid);
        g.drawHorizontalLine((int)midY, r.getX(), r.getRight());

        // 波形（2周期）を描く
        const int N = 256;
        const float amp = r.getHeight() * 0.38f;
        juce::Path p;
        for (int i = 0; i < N; ++i)
        {
            const float u = (float)i / (float)(N - 1);   // 0..1（画面幅）
            const float ph = std::fmod(u * 2.0f, 1.0f);  // 2周期の位相 0..1
            float y = waveValue(ph);
            const float px = r.getX() + u * r.getWidth();
            const float py = midY - y * amp;
            if (i == 0) p.startNewSubPath(px, py);
            else        p.lineTo(px, py);
        }
        g.setColour(SpectraColors::accentExcitation);
        g.strokePath(p, juce::PathStrokeType(1.8f));
    }

private:
    // 位相 ph(0..1) に対する波形値 -1..1
    float waveValue(float ph) const
    {
        switch (mType)
        {
            case 1: // Pulse: デューティ = mPulse
                return (ph < mPulse) ? 1.0f : -1.0f;
            case 2: // Wavetable: 正弦 → 倍音付き へモーフ（概形）
                return std::sin(ph * juce::MathConstants<float>::twoPi)
                     + mWt * 0.4f * std::sin(ph * juce::MathConstants<float>::twoPi * 3.0f);
            default: // Saw: -1→+1 ランプ
                return 2.0f * ph - 1.0f;
        }
    }

    int mType = 0;
    float mPulse = 0.5f;
    float mWt = 0.0f;
};

class ExcitationPanel : public juce::Component
{
public:
    ExcitationPanel(juce::AudioProcessorValueTreeState& state);
    ~ExcitationPanel();

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::AudioProcessorValueTreeState& apvts;

    void refreshWaveformDisplay();

    // ノブ 4基（残置）
    ValueKnob mKnobWtPos;
    ValueKnob mKnobPulseWidth;
    ValueKnob mKnobDetune;
    ValueKnob mKnobPorta;

    // コンボ 1種 + 2D波形表示
    juce::ComboBox mComboWaveform;
    WaveformDisplay mWaveDisplay;

    // ラベル
    juce::Label mLblWtPos { {}, "WT POS" };
    juce::Label mLblPulseWidth { {}, "PULSE WIDTH" };
    juce::Label mLblDetune { {}, "DETUNE" };
    juce::Label mLblPorta { {}, "PORTA" };

    // アタッチメント
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentWtPos;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentPulseWidth;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentDetune;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentPorta;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentWaveform;

    ArcDialLookAndFeel mArcLookAndFeel;
};
