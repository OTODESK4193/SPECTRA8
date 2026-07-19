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
    // --- 全リセットの確認バー (パネル内。ネイティブモーダルは使わない) ---
    mLblConfirm.setText("Reset all band gains to 0 dB?", juce::dontSendNotification);
    mLblConfirm.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    mLblConfirm.setColour(juce::Label::textColourId, SpectraColors::text);
    mLblConfirm.setJustificationType(juce::Justification::centredRight);
    addChildComponent(mLblConfirm);

    for (auto* b : { &mBtnResetYes, &mBtnResetNo })
    {
        b->setColour(juce::TextButton::buttonColourId, SpectraColors::knobTrack);
        b->setColour(juce::TextButton::textColourOffId, SpectraColors::text);
        addChildComponent(*b);
    }
    mBtnResetYes.setColour(juce::TextButton::buttonColourId,
                           SpectraColors::accentBands.withAlpha(0.28f));

    mBtnResetYes.onClick = [this]
    {
        for (auto& g : mBandGains)
            g.store(1.0f);      // リニア 1.0 = 0dB
        showResetConfirm(false);
    };
    mBtnResetNo.onClick = [this] { showResetConfirm(false); };
}

BandsEqPanel::~BandsEqPanel()
{
}

void BandsEqPanel::showResetConfirm(bool show)
{
    mConfirmVisible = show;
    mLblConfirm.setVisible(show);
    mBtnResetYes.setVisible(show);
    mBtnResetNo.setVisible(show);
    repaint();
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
    // 入力アナライザーの描画 (背景)
    //   ※ 以前は個別のバーとして描いていたため、バンド数が少ないと
    //     「太いバーの上端が音に合わせて上下する」= EQポイントが勝手に動いている
    //     ように見えてしまっていた。連続した塗り面 + 不透明度低下で、
    //     操作対象(EQカーブ)ではなく背景の可視化であることを明確にする。
    // ----------------------------------------------------
    {
        juce::Path fill;
        fill.startNewSubPath((float)r.getX(), (float)r.getBottom() - 10.0f);
        for (int i = 0; i < activeBands; ++i)
        {
            const float level = mBandLevelsForUi[(size_t)i].load(); // リニア振幅

            // 表示平滑化 (立ち上がりは速め・減衰は緩やか)
            float& sm = mMeterSmooth[(size_t)i];
            const float rate = (level > sm) ? 0.5f : 0.2f;
            sm += rate * (level - sm);

            float db = (sm > 1e-5f) ? (20.0f * std::log10(sm)) : -60.0f;
            db = juce::jlimit(-48.0f, 12.0f, db);

            const float pct = (db - (-48.0f)) / (12.0f - (-48.0f));
            const float y = (float)r.getBottom() - 10.0f - pct * (h - 20.0f);
            const float x = (float)r.getX() + ((float)i + 0.5f) * bandW;
            fill.lineTo(x, y);
        }
        fill.lineTo((float)r.getRight(), (float)r.getBottom() - 10.0f);
        fill.closeSubPath();

        g.setColour(SpectraColors::babyBlue.withAlpha(0.10f));
        g.fillPath(fill);
        g.setColour(SpectraColors::babyBlue.withAlpha(0.22f));
        g.strokePath(fill, juce::PathStrokeType(1.0f));
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

    // 線で結ぶ (EQカーブ＝操作対象。アナライザーより手前・太め・グロー付きで主役化)
    path.startNewSubPath(points[0]);
    for (size_t i = 1; i < points.size(); ++i)
    {
        path.lineTo(points[i]);
    }
    g.setColour(SpectraColors::accentBands.withAlpha(0.22f));
    g.strokePath(path, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
    g.setColour(SpectraColors::accentBands);
    g.strokePath(path, juce::PathStrokeType(2.6f));

    // ドットを描画
    for (const auto& p : points)
    {
        g.setColour(SpectraColors::accentBands.withAlpha(0.4f));
        g.drawEllipse(p.x - 5.5f, p.y - 5.5f, 11.0f, 11.0f, 1.0f);
        g.setColour(SpectraColors::text);
        g.fillEllipse(p.x - 3.5f, p.y - 3.5f, 7.0f, 7.0f);
    }

    // ----------------------------------------------------
    // 凡例 — どちらが操作対象なのかを明示する
    // ----------------------------------------------------
    {
        const int ly = r.getBottom() - 13;
        g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
        g.setColour(SpectraColors::accentBands);
        g.drawText("EQ (drag to edit)", r.getRight() - 240, ly, 118, 12,
                   juce::Justification::centredRight);
        g.setColour(SpectraColors::babyBlue.withAlpha(0.55f));
        g.drawText("ANALYZER (input)", r.getRight() - 118, ly, 112, 12,
                   juce::Justification::centredRight);
    }

    // 右クリック確認バーの背景 (ボタン類は子コンポーネント)
    if (mConfirmVisible)
    {
        auto bar = juce::Rectangle<int>(r.getX() + 1, r.getY() + 1, r.getWidth() - 2, 34).toFloat();
        g.setColour(SpectraColors::panel.brighter(0.16f));
        g.fillRoundedRectangle(bar, 6.0f);
        g.setColour(SpectraColors::accentBands.withAlpha(0.7f));
        g.drawRoundedRectangle(bar.reduced(0.5f), 6.0f, 1.2f);
    }
}

void BandsEqPanel::resized()
{
    // 確認バー: パネル上端に「メッセージ + RESET ALL + CANCEL」を右詰めで並べる
    auto r = getLocalBounds().reduced(16);
    auto bar = juce::Rectangle<int>(r.getX() + 1, r.getY() + 1, r.getWidth() - 2, 34).reduced(6, 5);

    mBtnResetNo.setBounds(bar.removeFromRight(80));
    bar.removeFromRight(6);
    mBtnResetYes.setBounds(bar.removeFromRight(96));
    bar.removeFromRight(10);
    mLblConfirm.setBounds(bar);
}

void BandsEqPanel::mouseDown(const juce::MouseEvent& e)
{
    // 右クリック → 全バンドリセットの確認バーを表示 (誤操作でカーブを消さないため)
    if (e.mods.isPopupMenu())
    {
        showResetConfirm(true);
        return;
    }

    // 確認バー表示中は、誤ってカーブを描かないよう左クリックを無視する
    if (mConfirmVisible)
        return;

    // ダブルクリックの2回目のクリックではカーブを描き込まない
    // (直後の mouseDoubleClick によるリセットを上書きしないため)
    if (e.getNumberOfClicks() > 1)
        return;

    handleMouse(e);
}

void BandsEqPanel::mouseDrag(const juce::MouseEvent& e)
{
    // 右ドラッグ、確認バー表示中、ダブルクリック中の微小ドラッグではカーブを描き込まない
    if (e.mods.isPopupMenu() || mConfirmVisible || e.getNumberOfClicks() > 1)
        return;

    handleMouse(e);
}

void BandsEqPanel::mouseDoubleClick(const juce::MouseEvent& e)
{
    // 左ダブルクリック → そのバンドのみ 0dB にリセット
    if (e.mods.isPopupMenu() || mConfirmVisible) return;

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
