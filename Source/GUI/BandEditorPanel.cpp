#include "BandEditorPanel.h"

namespace GUI {

BandEditorPanel::BandEditorPanel(SPECTRA8AudioProcessor& processor)
    : mProcessor(processor)
{
    startTimerHz(30);
}

BandEditorPanel::~BandEditorPanel()
{
    stopTimer();
}

void BandEditorPanel::paint(juce::Graphics& g)
{
    g.fillAll(ColorPalette::panelBg);

    // グリッド線
    g.setColour(ColorPalette::grid);
    int numGridLines = 4;
    for (int i = 1; i < numGridLines; ++i)
    {
        float y = getHeight() * (static_cast<float>(i) / numGridLines);
        g.drawHorizontalLine(static_cast<int>(y), 0.0f, static_cast<float>(getWidth()));
    }

    auto bounds = getLocalBounds().toFloat();
    float w = bounds.getWidth() / 48.0f;
    float h = bounds.getHeight();

    // 1. 各バンドのレベルメーター（ビジュアライザー）の描画
    for (int i = 0; i < 48; ++i)
    {
        float level = mProcessor.getBandLevel(i);
        // レベルは見やすさのために適度なスケールアップ (リニアなので 6.0f で乗算)
        float levelHeight = std::clamp(level * 6.0f, 0.0f, 1.0f) * h;
        
        g.setColour(ColorPalette::mint.withAlpha(0.2f));
        g.fillRect(i * w + 1.0f, h - levelHeight, w - 2.0f, levelHeight);
    }

    // 2. ゲイン値の折れ線（EQカーブ）と各バンドのポインターの描画
    juce::Path eqPath;
    g.setColour(ColorPalette::mint);

    for (int i = 0; i < 48; ++i)
    {
        float gain = mProcessor.getBandGain(i);
        float x = i * w + w * 0.5f;
        float y = h * (1.0f - gain);

        if (i == 0)
            eqPath.startNewSubPath(x, y);
        else
            eqPath.lineTo(x, y);

        // 各バンドのゲインポインター (ドラッグ可能位置)
        g.fillEllipse(x - 2.5f, y - 2.5f, 5.0f, 5.0f);
    }

    g.strokePath(eqPath, juce::PathStrokeType(1.5f));

    // 外枠
    g.setColour(ColorPalette::panelBorder);
    g.drawRect(getLocalBounds(), 1);
}

void BandEditorPanel::resized()
{
}

void BandEditorPanel::timerCallback()
{
    repaint();
}

void BandEditorPanel::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        // 右クリックでリセット
        float w = getWidth() / 48.0f;
        int bandIdx = std::clamp(static_cast<int>(e.x / w), 0, 47);
        mProcessor.setBandGain(bandIdx, 1.0f);
        repaint();
    }
    else
    {
        handleMouse(e);
    }
}

void BandEditorPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (!e.mods.isRightButtonDown())
    {
        handleMouse(e);
    }
}

void BandEditorPanel::handleMouse(const juce::MouseEvent& e)
{
    float w = getWidth() / 48.0f;
    int bandIdx = std::clamp(static_cast<int>(e.x / w), 0, 47);
    float gain = std::clamp(1.0f - (static_cast<float>(e.y) / getHeight()), 0.0f, 1.0f);
    mProcessor.setBandGain(bandIdx, gain);
    repaint();
}

} // namespace GUI