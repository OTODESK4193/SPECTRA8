// ==========================================
// File: ExcitationPanel.h
// 「EXCITATION」タブ・パネル (Granular 準拠)
//  - 左: 波形選択コンボ + BROWSE/ADD DIR + 2D波形表示 + DETUNE MODEコンボ
//  - 右: キャリア関連ノブ (WT POS / PULSE WIDTH / DETUNE+SNAP / PORTA)
//  - カスタムWavetable:
//      ADD DIR でWavetableフォルダを登録 (パスはセッション保存)
//      BROWSE で右側がフォルダ内 wav/aiff のリストに切替わり、
//      クリックで即ロード&波形/音へ反映 (試聴しながら選べる)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <vector>
#include "ColorPalette.h"
#include "ValueKnob.h"
#include "ArcDial.h"
#include "GlowToggle.h"

class SPECTRA8AudioProcessor;

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

    // カスタムWavetableの実波形 (n点) を表示に使う。nullptrで解除。
    void setCustomWave(const float* data, int n)
    {
        if (data != nullptr && n > 1)
            mCustom.assign(data, data + n);
        else
            mCustom.clear();
        repaint();
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
            case 2: // Wavetable
                if (!mCustom.empty())
                {
                    const float idx = ph * (float)(mCustom.size() - 1);
                    const int i0 = juce::jlimit(0, (int)mCustom.size() - 2, (int)idx);
                    const float fr = idx - (float)i0;
                    return mCustom[(size_t)i0] * (1.0f - fr) + mCustom[(size_t)i0 + 1] * fr;
                }
                return std::sin(ph * juce::MathConstants<float>::twoPi)
                     + mWt * 0.4f * std::sin(ph * juce::MathConstants<float>::twoPi * 3.0f);
            default: // Saw: -1→+1 ランプ
                return 2.0f * ph - 1.0f;
        }
    }

    int mType = 0;
    float mPulse = 0.5f;
    float mWt = 0.0f;
    std::vector<float> mCustom;   // カスタムWT実波形 (空=概形描画)
};

class ExcitationPanel : public juce::Component,
                        public juce::ListBoxModel
{
public:
    explicit ExcitationPanel(SPECTRA8AudioProcessor& proc);
    ~ExcitationPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // ListBoxModel (Wavetableファイル一覧)
    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;

private:
    SPECTRA8AudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    void refreshWaveformDisplay();
    void updateBrowseVisibility();   // Wavetable選択時のみBROWSE/ADD DIR表示
    void showBrowser();              // 登録フォルダのリストを表示 (未登録ならADD DIRへ)
    void hideBrowser();
    void chooseFolder();             // ADD DIR: フォルダ選択ダイアログ
    void rescanFolder();             // 登録フォルダから wav/aiff 一覧を再取得
    void applyDetuneSnap();          // SNAP時に現在値を100ct単位へ丸める

    juce::File getWtDir() const;

    // ノブ 6基
    ValueKnob mKnobWtPos;
    ValueKnob mKnobPulseWidth;
    ValueKnob mKnobDetune;
    ValueKnob mKnobPorta;
    ValueKnob mKnobMorphAmt;    // Morph Amount (BassSynth移植)
    ValueKnob mKnobMorphShift;  // Morph Shift

    // コンボ + トグル + ボタン
    juce::ComboBox mComboWaveform;
    juce::ComboBox mComboDetuneMode;
    juce::ComboBox mComboMorphMode;   // None / Bend +/- / Sync / Vocode
    GlowToggle mBtnDetuneSnap;
    juce::TextButton mBtnBrowse { "BROWSE" };
    juce::TextButton mBtnAddDir { "ADD DIR" };
    WaveformDisplay mWaveDisplay;

    // カスタムWTリスト (BROWSE押下で右側に表示)
    // 入れ子フォルダ対応: 子フォルダ名をサブカテゴリのヘッダ行として表示する
    struct WtEntry
    {
        bool isHeader = false;
        juce::String label;   // ヘッダ: サブフォルダ名 / ファイル行: 表示名
        juce::File file;      // ファイル行のみ有効
    };
    bool mBrowserOpen = false;
    juce::ListBox mWtList;
    std::vector<WtEntry> mWtEntries;
    juce::TextButton mBtnBrowserClose { "CLOSE" };
    juce::TextButton mBtnFactory { "FACTORY" };
    std::unique_ptr<juce::FileChooser> mChooser;

    // ラベル
    juce::Label mLblWtPos { {}, "WT POS" };
    juce::Label mLblPulseWidth { {}, "PULSE WIDTH" };
    juce::Label mLblDetune { {}, "DETUNE" };
    juce::Label mLblPorta { {}, "PORTA" };
    juce::Label mLblMorphAmt { {}, "MORPH AMT" };
    juce::Label mLblMorphShift { {}, "MORPH SHIFT" };
    juce::Label mLblCustomName;   // ロード中のカスタムWT名 / フォルダ状態

    // アタッチメント
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentWtPos;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentPulseWidth;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentDetune;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentPorta;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentMorphAmt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentMorphShift;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentWaveform;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentDetuneMode;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentMorphMode;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mAttachmentDetuneSnap;

    ArcDialLookAndFeel mArcLookAndFeel;
};
