// ==========================================
// File: BandsEqPanel.cpp
// 「BANDS EQ」タブ・パネル (Granular 準拠)
// ==========================================
#include "BandsEqPanel.h"
#include <cmath>

BandsEqPanel::BandsEqPanel(juce::AudioProcessorValueTreeState& state,
                           std::array<std::atomic<float>, kMaxBands>& bandGains,
                           const std::array<std::atomic<float>, kMaxBands>& bandLevelsForUi)
    : apvts(state),
      mBandGains(bandGains),
      mBandLevelsForUi(bandLevelsForUi),
      mTimer(*this)
{
}

BandsEqPanel::~BandsEqPanel()
{
}

void BandsEqPanel::paint(juce::Graphics& g)
{
    g.fillAll(SpectraColors::bg);

    auto r = getLocalBounds().reduced(16);
    const float w = (float)r.getWidth();
    const float h = (float)r.getHeight();

    // パネルの背景
    g.setColour(SpectraColors::panel);
    g.fillRoundedRectangle(r.toFloat(), 8.0f);
    g.setColour(SpectraColors::panelLine);
    g.drawRoundedRectangle(r.toFloat(), 8.0f, 1.0f);

    // 現在の有効バンド数
    const int activeBands = juce::jlimit(8, kMaxBands, 
        static_cast<int>(apvts.getRawParameterValue("bandCount")->load()));

    // ----------------------------------------------------
    // グリッド線描画
    // ----------------------------------------------------
    g.setColour(SpectraColors::grid);
    // 横線 (dB単位: +12, 0, -12, -24)
    std::array<float, 4> dbLines = { 12.0f, 0.0f, -12.0f, -24.0f };
    for (float db : dbLines)
    {
        // Yマッピング: +12dB = r.getY() + 10, -24dB = r.getBottom() - 20
        float pct = (db - (-24.0f)) / (12.0f - (-24.0f));
        float y = (float)r.getBottom() - 20.0f - pct * (h - 30.0f);
        g.drawHorizontalLine((int)y, (float)r.getX(), (float)r.getRight());

        g.setColour(SpectraColors::textDim.withAlpha(0.5f));
        g.setFont(9.0f);
        g.drawText(juce::String((int)db) + " dB", r.getX() + 6, (int)y - 12, 48, 12, juce::Justification::left);
        g.setColour(SpectraColors::grid);
    }

    // 縦境界線
    const float bandW = w / (float)activeBands;
    for (int i = 1; i < activeBands; ++i)
    {
        float x = (float)r.getX() + (float)i * bandW;
        g.drawVerticalLine((int)x, (float)r.getY(), (float)r.getBottom());
    }

    // ----------------------------------------------------
    // レベルメーターの描画 (背景のアナライザー / 上向きのバー)
    //   ※ EQカーブ(緑=mint)と誤認されないよう、暗い別色(青系)＋低不透明度で
    //     「あくまで背景の入力レベル表示」であることを視覚的に明確化する。
    //     さらに表示値を時間平滑化してガタつきを抑える。
    // ----------------------------------------------------
    g.setColour(SpectraColors::babyBlue.withAlpha(0.14f));
    for (int i = 0; i < activeBands; ++i)
    {
        const float level = mBandLevelsForUi[(size_t)i].load(); // リニア振幅 (通常 0.0〜1.0)

        // 表示平滑化 (立ち上がりは速め・減衰は緩やか)
        float& sm = mMeterSmooth[(size_t)i];
        const float rate = (level > sm) ? 0.5f : 0.2f;
        sm += rate * (level - sm);

        // デシベル変換 (メーター用)
        float db = (sm > 1e-5f) ? (20.0f * std::log10(sm)) : -60.0f;
        db = juce::jlimit(-48.0f, 12.0f, db);

        // Y座標算出 (下限を -48dB に設定)
        float pct = (db - (-48.0f)) / (12.0f - (-48.0f));
        float y = (float)r.getBottom() - 10.0f - pct * (h - 20.0f);

        float x = (float)r.getX() + (float)i * bandW + 2.0f;
        float barW = bandW - 4.0f;
        if (barW < 1.0f) barW = 1.0f;

        g.fillRect(x, y, barW, (float)r.getBottom() - 10.0f - y);
    }

    // ----------------------------------------------------
    // EQゲインカーブ（操作点 & 接続線）描画
    // ----------------------------------------------------
    juce::Path path;
    std::vector<juce::Point<float>> points;

    for (int i = 0; i < activeBands; ++i)
    {
        const float gain = mBandGains[(size_t)i].load();
        float db = (gain > 1e-5f) ? (20.0f * std::log10(gain)) : -24.0f;
        db = juce::jlimit(-24.0f, 12.0f, db);

        float pct = (db - (-24.0f)) / (12.0f - (-24.0f));
        float y = (float)r.getBottom() - 20.0f - pct * (h - 30.0f);
        float x = (float)r.getX() + ((float)i + 0.5f) * bandW;

        points.push_back({ x, y });
    }

    // 線で結ぶ (EQカーブ＝操作対象。メーターより手前・太めで明確に主役化)
    g.setColour(SpectraColors::accentBands);
    path.startNewSubPath(points[0]);
    for (size_t i = 1; i < points.size(); ++i)
    {
        path.lineTo(points[i]);
    }
    g.strokePath(path, juce::PathStrokeType(2.6f));

    // ドットを描画
    g.setColour(SpectraColors::text);
    for (const auto& p : points)
    {
        g.fillEllipse(p.x - 3.5f, p.y - 3.5f, 7.0f, 7.0f);
        g.setColour(SpectraColors::accentBands.withAlpha(0.4f));
        g.drawEllipse(p.x - 5.5f, p.y - 5.5f, 11.0f, 11.0f, 1.0f);
        g.setColour(SpectraColors::text);
    }
}

void BandsEqPanel::resized()
{
}

void BandsEqPanel::mouseDown(const juce::MouseEvent& e)
{
    // 右クリック → 全バンドリセットの確認ダイアログ (誤操作でカーブを消さないための Y/N 確認)
    if (e.mods.isPopupMenu())
    {
        confirmResetAllBands();
        return;
    }

    // ダブルクリックの2回目のクリックではカーブを描き込まない
    // (直後の mouseDoubleClick によるリセットを上書きしないため)
    if (e.getNumberOfClicks() > 1)
        return;

    handleMouse(e);
}

void BandsEqPanel::mouseDrag(const juce::MouseEvent& e)
{
    // 右ドラッグ、およびダブルクリック中の微小ドラッグではカーブを描き込まない
    if (e.mods.isPopupMenu() || e.getNumberOfClicks() > 1)
        return;

    handleMouse(e);
}

void BandsEqPanel::mouseDoubleClick(const juce::MouseEvent& e)
{
    // 左ダブルクリック → そのバンドのみ 0dB にリセット
    if (e.mods.isPopupMenu()) return;

    const int bandIdx = bandIndexAt(e);
    if (bandIdx < 0) return;

    mBandGains[(size_t)bandIdx].store(1.0f); // リニア 1.0 = 0dB
    repaint();
}

int BandsEqPanel::bandIndexAt(const juce::MouseEvent& e) const
{
    auto r = getLocalBounds().reduced(16);
    if (!r.contains(e.getPosition())) return -1;

    const int activeBands = juce::jlimit(8, kMaxBands,
        static_cast<int>(apvts.getRawParameterValue("bandCount")->load()));

    const float bandW = (float)r.getWidth() / (float)activeBands;
    int bandIdx = (int)(((float)(e.x - r.getX())) / bandW);
    return juce::jlimit(0, activeBands - 1, bandIdx);
}

void BandsEqPanel::confirmResetAllBands()
{
    // プラグインウィンドウでは JUCE製 AlertWindow が表示されない・背面に隠れる事例があるため、
    // OSネイティブのメッセージボックス (Windows: Win32 MessageBox) を使用する
    auto options = juce::MessageBoxOptions::makeOptionsYesNo(
        juce::MessageBoxIconType::QuestionIcon,
        "BANDS EQ Reset",
        "Reset all band gains to 0 dB?",
        "Yes", "No", this);

    juce::NativeMessageBox::showAsync(options,
        [safeThis = juce::Component::SafePointer<BandsEqPanel>(this)](int result)
        {
            // result はボタンの登録順インデックス (0始まり): 0 = Yes, 1 = No
            // (JUCE 8 Windowsネイティブ実装 TaskDialogIndirect はボタンIDに0始まりの連番を使用)
            if (result == 0 && safeThis != nullptr)
            {
                for (auto& g : safeThis->mBandGains)
                    g.store(1.0f);
                safeThis->repaint();
            }
        });
}

void BandsEqPanel::handleMouse(const juce::MouseEvent& e)
{
    auto r = getLocalBounds().reduced(16);
    if (!r.contains(e.getPosition())) return;

    const float w = (float)r.getWidth();

    const int activeBands = juce::jlimit(8, kMaxBands,
        static_cast<int>(apvts.getRawParameterValue("bandCount")->load()));

    const float bandW = w / (float)activeBands;

    // X座標からバンドを特定
    int bandIdx = (int)(((float)(e.x - r.getX())) / bandW);
    bandIdx = juce::jlimit(0, activeBands - 1, bandIdx);

    // Y座標からゲイン(dB)を特定
    // +12dB = r.getY() + 10, -24dB = r.getBottom() - 20
    float topY = (float)r.getY() + 10.0f;
    float bottomY = (float)r.getBottom() - 20.0f;
    float pct = 1.0f - ((float)e.y - topY) / (bottomY - topY);
    pct = juce::jlimit(0.0f, 1.0f, pct);

    float db = -24.0f + pct * (12.0f - (-24.0f));
    db = juce::jlimit(-24.0f, 12.0f, db);

    // リニアゲインに変換してストア
    float gain = std::pow(10.0f, db / 20.0f);
    mBandGains[(size_t)bandIdx].store(gain);

    repaint();
}
