// ==========================================
// File: BandsEqPanel.cpp
// 「BANDS EQ」タブ・パネル (Granular 準拠)
// ==========================================
#include "BandsEqPanel.h"
#include "../DSP/AnalyzerDSP.h"
#include <cmath>

BandsEqPanel::BandsEqPanel(juce::AudioProcessorValueTreeState& state,
                           std::array<std::atomic<float>, kMaxBands>& bandGains,
                           const std::array<std::atomic<float>, kMaxBands>& bandLevelsForUi,
                           const AnalyzerDSP& analyzer)
    : apvts(state),
      mBandGains(bandGains),
      mBandLevelsForUi(bandLevelsForUi),
      mAnalyzer(analyzer),
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

    mBtnResetYes.setTooltip("RESET ALL - set every band back to 0 dB.");
    mBtnResetNo.setTooltip("CANCEL - keep the current band gains.");

    // グラフ本体の説明 (パネル全体に付けておけば、バー上のどこでも表示される)
    setTooltip("BANDS EQ - drag a bar to boost or cut that vocoder band. The moving bars show "
               "the live level of each band and the curve behind them is the spectrum of the "
               "plugin output. The number of bars follows the BANDS knob. "
               "Double-click a bar to return it to 0 dB.");
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
    // スペクトラムアナライザー (背景) — 高精度定Qフィルタバンクの出力を
    // 画面X座標に対して連続曲線として描く。
    //   X軸はEQバンドと同じ mel 配置。各描画点の周波数を mel 逆変換で求め、
    //   アナライザーから対数補間で dB を取り出すので、バンド数に依存せず滑らか。
    //   色は低域→高域でグラデーション (帯域が一目で分かるように)。
    // ----------------------------------------------------
    {
        // EQバンドと同じ mel 軸: 画面比 t → 周波数
        const float fMin = 80.0f, fMax = 7500.0f;
        const float mMin = 2595.0f * std::log10(1.0f + fMin / 700.0f);
        const float mMax = 2595.0f * std::log10(1.0f + fMax / 700.0f);
        auto xToFreq = [&](float t)
        {
            const float mv = mMin + (mMax - mMin) * t;
            return 700.0f * (std::pow(10.0f, mv / 2595.0f) - 1.0f);
        };

        const float botY = (float)r.getBottom() - 10.0f;

        juce::Path fill, line;
        fill.startNewSubPath((float)r.getX(), botY);

        for (int i = 0; i < kCurvePoints; ++i)
        {
            const float t = (float)i / (float)(kCurvePoints - 1);
            // 表示レンジ。
            //  旧値 -48〜+12dB は「0dBFS を上端に置く」設計だったが、
            //  ボコーダー出力を48バンドに分けた1本あたりのレベルは -30dBFS 前後なので、
            //  実際には常にパネル下端 1/4 にへばりついた描画になっていた。
            //  実測に合わせて窓を下へずらし、通常の音量でパネルの 7 割を使うようにする。
            const float db = juce::jlimit(kAnaDbMin, kAnaDbMax, mAnalyzer.getDbAtFreq(xToFreq(t)));

            // 描画側でも軽く平滑化してフレーム間のちらつきを抑える
            float& sm = mCurveSmooth[(size_t)i];
            if (!mCurveInit) sm = db;
            else             sm += ((db > sm) ? 0.55f : 0.25f) * (db - sm);

            const float pct = (sm - kAnaDbMin) / (kAnaDbMax - kAnaDbMin);
            const float y = botY - pct * (h - 20.0f);
            const float x = (float)r.getX() + t * w;

            if (i == 0) line.startNewSubPath(x, y);
            else        line.lineTo(x, y);
            fill.lineTo(x, y);
        }
        mCurveInit = true;

        fill.lineTo((float)r.getRight(), botY);
        fill.closeSubPath();

        // 低域→高域のグラデーション。
        //  「今どのあたりの帯域を見ているか」を色だけで掴めるように、
        //  紫(低) → 青 → シアン(中) → 緑 → 桃(高) と5段で振る。
        //  塗りは濃いめ、線は不透明・太めにして背景から確実に浮かせる。
        auto makeGrad = [&r](float a)
        {
            juce::ColourGradient gr(SpectraColors::lilac.withAlpha(a), (float)r.getX(), 0.0f,
                                    SpectraColors::pink.withAlpha(a), (float)r.getRight(), 0.0f,
                                    false);
            gr.addColour(0.28, SpectraColors::babyBlue.withAlpha(a));
            gr.addColour(0.52, SpectraColors::mint.withAlpha(a));
            gr.addColour(0.78, SpectraColors::sage.withAlpha(a));
            return gr;
        };

        g.setGradientFill(makeGrad(0.42f));   // 塗り
        g.fillPath(fill);

        g.setGradientFill(makeGrad(1.0f));    // 輪郭線は不透明・太めで確実に浮かせる
        g.strokePath(line, juce::PathStrokeType(1.9f, juce::PathStrokeType::curved));

        // ------------------------------------------------
        // 周波数目盛り (X軸)
        //  X軸は EQ バンドと同じ mel 配置なので、対数目盛りとも等間隔目盛りとも
        //  一致しない。目当ての周波数がどこかを目視で掴めるよう、代表的な
        //  周波数に細い縦線とラベルを入れる。
        //  ※ mel 逆変換ではなく順変換 (freq → t) を使う。
        // ------------------------------------------------
        auto freqToT = [&](float f)
        {
            const float mv = 2595.0f * std::log10(1.0f + f / 700.0f);
            return (mv - mMin) / (mMax - mMin);
        };

        struct Tick { float hz; const char* label; };
        static const Tick kTicks[] = {
            { 100.0f,  "100" },  { 200.0f,  "200" },  { 500.0f,  "500" },
            { 1000.0f, "1k"  },  { 2000.0f, "2k"  },  { 3000.0f, "3k"  },
            { 5000.0f, "5k"  },  { 7000.0f, "7k"  },
        };

        const float labelY = (float)r.getBottom() - 13.0f;
        g.setFont(9.0f);
        for (const auto& t : kTicks)
        {
            const float tt = freqToT(t.hz);
            if (tt < 0.005f || tt > 0.995f)
                continue;
            const float x = (float)r.getX() + tt * w;

            // 目盛り線 (EQのバンド境界線より少し明るく、ただし主役を邪魔しない濃さ)
            g.setColour(SpectraColors::textDim.withAlpha(0.22f));
            g.drawVerticalLine((int)x, (float)r.getY() + 4.0f, labelY - 1.0f);

            // ラベル (背景を少し敷いてスペクトラム上でも読めるようにする)
            const juce::Rectangle<int> lb((int)x - 16, (int)labelY, 32, 12);
            g.setColour(SpectraColors::panel.withAlpha(0.75f));
            g.fillRoundedRectangle(lb.toFloat().reduced(1.0f, 0.0f), 2.0f);
            g.setColour(SpectraColors::textDim.withAlpha(0.9f));
            g.drawText(t.label, lb, juce::Justification::centred);
        }
        // 右端に単位を出しておく
        g.setColour(SpectraColors::textDim.withAlpha(0.55f));
        g.drawText("Hz", r.getRight() - 26, (int)labelY, 22, 12, juce::Justification::right);
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
        // 下端は周波数目盛りに使うようになったので、凡例は上端へ移した。
        // 全リセット確認バーも上端に出るので、そのときは凡例を伏せる。
        if (!mConfirmVisible)
        {
            const int ly = r.getY() + 6;
            g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
            g.setColour(SpectraColors::accentBands);
            g.drawText("EQ (drag to edit)", r.getRight() - 240, ly, 118, 12,
                       juce::Justification::centredRight);
            g.setColour(SpectraColors::babyBlue.withAlpha(0.65f));
            g.drawText("ANALYZER (output)", r.getRight() - 118, ly, 112, 12,
                       juce::Justification::centredRight);
        }
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
