// ==========================================
// File: ExcitationPanel.h
// 「EXCITATION」タブ・パネル (Granular 準拠)
//  - 左: 波形選択コンボ + BROWSE/ADD DIR + 2D波形表示 + DETUNE MODEコンボ
//  - 右: キャリア関連ノブ 10基 (4列×3行)
//        1行目: WT POS / PULSE WIDTH / PORTA / DETUNE(+SNAP)
//        2行目: BEND / BEND SYM / SYNC / SYNC PH
//        3行目: VOCODE / VOWEL
//        ※Morphはコンボによる排他選択を廃止し、3種を同時併用できる。
//          各Amtノブが0のときその段は自動バイパス。
//  - カスタムWavetable:
//      ADD DIR でWavetableフォルダを登録 (パスはセッション保存)
//      BROWSE で右側が2ペインのブラウザに切替わる
//        左ペイン = サブカテゴリ (子フォルダ)、右ペイン = 波形ファイル
//      クリックで即ロード&波形/音へ反映 (試聴しながら選べる)
//      RANDOM で全カテゴリからランダムに1つロード
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

// キャリア波形の概形を描く2D表示エリア。
// 波形タイプ / パルス幅 / WT位置 に加え、Morph (Bend/Sync/Vocode) も反映する。
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

    // Morphパラメータ (ExcitationEngine::syncParameters と同一の式で描画へ反映)
    void setMorph(float bendAmt, float bendShift, float syncAmt, float syncShift,
                  float vocAmt, float vocShift)
    {
        auto ch = [](float a, float b) { return std::abs(a - b) > 1e-4f; };
        if (!ch(bendAmt, mBendAmt) && !ch(bendShift, mBendShift) && !ch(syncAmt, mSyncAmt)
            && !ch(syncShift, mSyncShift) && !ch(vocAmt, mVocAmt) && !ch(vocShift, mVocShift))
            return;

        mBendAmt = bendAmt; mBendShift = bendShift;
        mSyncAmt = syncAmt; mSyncShift = syncShift;
        mVocAmt  = vocAmt;  mVocShift  = vocShift;

        // ---- ExcitationEngine と同じ事前計算 ----
        mBendOn = std::abs(mBendAmt) > 0.001f;
        mBendSym = juce::jlimit(0.01f, 0.99f, 0.5f + juce::jlimit(-1.0f, 1.0f, mBendShift) * 0.49f);
        mBendB = std::exp(-juce::jlimit(-0.99f, 0.99f, mBendAmt) * 2.0f);

        mSyncOn = mSyncAmt > 0.001f;
        mSyncSt = 1.0f + juce::jlimit(0.0f, 1.0f, mSyncAmt) * 7.0f;
        mSyncShiftHalf = juce::jlimit(-1.0f, 1.0f, mSyncShift) * 0.5f;

        mVocOn = mVocAmt > 0.001f;
        if (mVocOn)
        {
            static const float kFmts[5][3] = {
                { 32.0f, 55.0f, 120.0f },   // A
                { 14.0f, 102.0f, 139.0f },  // I
                { 14.0f, 41.0f, 116.0f },   // U
                { 18.0f, 74.0f, 111.0f },   // E
                { 18.0f, 37.0f, 120.0f },   // O
            };
            float s = juce::jlimit(-1.0f, 1.0f, mVocShift);
            int seq[5];
            if (s >= 0.0f) { seq[0]=0; seq[1]=1; seq[2]=2; seq[3]=3; seq[4]=4; }
            else           { seq[0]=0; seq[1]=3; seq[2]=1; seq[3]=4; seq[4]=2; s = -s; }
            const float pos = s * 4.0f;
            const int i0 = juce::jlimit(0, 3, (int)pos);
            const float frac = pos - (float)i0;
            for (int j = 0; j < 3; ++j)
                mVocHarm[j] = kFmts[seq[i0]][j] * (1.0f - frac) + kFmts[seq[i0 + 1]][j] * frac;
        }
        repaint();
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

        // ---- 波形を生成 (2周期を表示) ----
        // Vocodeフィルタは共振のため立ち上がりに時間がかかる。
        // ウォームアップとして余分な周期を先に流し、末尾2周期だけを描画する。
        static constexpr int kP = 512;                    // 1周期あたりのサンプル数
        const int warmPeriods = mVocOn ? 10 : 0;
        const int totalPeriods = warmPeriods + 2;
        std::vector<float> buf((size_t)(totalPeriods * kP), 0.0f);

        const float phaseInc = 1.0f / (float)kP;
        for (int i = 0; i < totalPeriods * kP; ++i)
        {
            float ph = (float)(i % kP) * phaseInc;
            float sMul = 1.0f;
            ph = applyMorphPhase(ph, phaseInc, sMul);
            buf[(size_t)i] = waveValue(ph) * sMul;
        }

        if (mVocOn)
        {
            // 中心周波数 = 倍音番号 × 基音。1周期=kPサンプルなので fc/sr = 倍音番号/kP。
            DispSvf f[3];
            const float amt = juce::jlimit(0.0f, 1.0f, mVocAmt);
            for (int i = 0; i < totalPeriods * kP; ++i)
            {
                const float dry = buf[(size_t)i];
                float wet = 0.0f;
                for (int j = 0; j < 3; ++j)
                {
                    const float h = juce::jlimit(1.0f, (float)kP * 0.45f, mVocHarm[j]);
                    const float Q = juce::jlimit(1.0f, 30.0f, h / (0.15f * h + 4.0f));
                    wet += f[j].bpf(dry, h / (float)kP, Q);
                }
                buf[(size_t)i] = dry * (1.0f - amt) + wet * 4.0f * amt;
            }
        }

        // 末尾2周期を取り出し、はみ出さないよう正規化
        const int start = warmPeriods * kP;
        float peak = 1.0f;
        for (int i = start; i < totalPeriods * kP; ++i)
            peak = juce::jmax(peak, std::abs(buf[(size_t)i]));

        const int N = 512;
        const float amp = r.getHeight() * 0.38f / peak;
        juce::Path p;
        for (int i = 0; i < N; ++i)
        {
            const float u = (float)i / (float)(N - 1);
            const int si = juce::jlimit(0, 2 * kP - 1, (int)(u * (float)(2 * kP - 1)));
            const float px = r.getX() + u * r.getWidth();
            const float py = midY - buf[(size_t)(start + si)] * amp;
            if (i == 0) p.startNewSubPath(px, py);
            else        p.lineTo(px, py);
        }
        g.setColour(SpectraColors::accentExcitation);
        g.strokePath(p, juce::PathStrokeType(1.8f));
    }

private:
    // 表示用の軽量SVFバンドパス (中心利得0dB正規化。fcNorm = fc/サンプルレート)
    struct DispSvf
    {
        float s1 = 0.0f, s2 = 0.0f;
        float bpf(float in, float fcNorm, float Q) noexcept
        {
            const float g = std::tan(juce::MathConstants<float>::pi
                                   * juce::jlimit(0.0005f, 0.45f, fcNorm));
            const float k = 1.0f / juce::jmax(0.3f, Q);
            const float h = 1.0f / (1.0f + g * (g + k));
            const float v1 = (s1 + g * (in - s2)) * h;
            const float v2 = s2 + g * v1;
            s1 = 2.0f * v1 - s1;
            s2 = 2.0f * v2 - s2;
            return v1 * k;
        }
    };

    // ExcitationEngine::applyMorphPhase と同一 (Bend → Sync の直列適用 + 周期端フェード)
    float applyMorphPhase(float phase, float phaseInc, float& sMul) const
    {
        if (!mBendOn && !mSyncOn)
            return phase;

        const float orig = phase;
        float warped = phase;

        if (mBendOn)
        {
            if (warped < mBendSym)
                warped = mBendSym * std::pow(warped / mBendSym, mBendB);
            else
                warped = mBendSym + (1.0f - mBendSym)
                       * (1.0f - std::pow((1.0f - warped) / (1.0f - mBendSym), mBendB));
        }

        if (mSyncOn)
        {
            float res = (warped + mSyncShiftHalf) * mSyncSt;
            res -= std::floor(res);
            if (res > 0.985f)      sMul *= (1.0f - res) / 0.015f;
            else if (res < 0.015f) sMul *= res / 0.015f;
            warped = res - mSyncShiftHalf;
            if (warped >= 1.0f) warped -= 1.0f;
            else if (warped < 0.0f) warped += 1.0f;
        }

        const float fadeWidth = juce::jlimit(0.001f, 0.03f, phaseInc * 0.16f);
        if (orig < fadeWidth)
        {
            const float mix = orig / fadeWidth;
            return warped * mix + orig * (1.0f - mix);
        }
        if (orig > 1.0f - fadeWidth)
        {
            const float mix = (1.0f - orig) / fadeWidth;
            return warped * mix + orig * (1.0f - mix);
        }
        return warped;
    }

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

    // Morph生パラメータ (変化検出用)
    float mBendAmt = 0.0f, mBendShift = 0.0f;
    float mSyncAmt = 0.0f, mSyncShift = 0.0f;
    float mVocAmt = 0.0f,  mVocShift = 0.0f;
    // 事前計算値
    bool  mBendOn = false, mSyncOn = false, mVocOn = false;
    float mBendSym = 0.5f, mBendB = 1.0f;
    float mSyncSt = 1.0f, mSyncShiftHalf = 0.0f;
    float mVocHarm[3] = { 32.0f, 55.0f, 120.0f };
};

class ExcitationPanel : public juce::Component
{
public:
    explicit ExcitationPanel(SPECTRA8AudioProcessor& proc);
    ~ExcitationPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

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
    void loadRandomWavetable();      // RANDOM: 全カテゴリからランダムに1つロード
    void loadFileAt(int catIdx, int fileIdx);
    void setKnobsVisible(bool v);

    juce::File getWtDir() const;

    // ---- 2ペインブラウザ (BassSynth式: 左=サブカテゴリ / 右=波形ファイル) ----
    struct WtCategory
    {
        juce::String name;
        std::vector<juce::File> files;
    };
    std::vector<WtCategory> mCategories;
    int mSelectedCat = 0;

    // 2つのListBoxを1クラスで扱うための転送モデル
    class ListProxy : public juce::ListBoxModel
    {
    public:
        ListProxy(ExcitationPanel& o, bool categories) : owner(o), isCat(categories) {}
        int getNumRows() override;
        void paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected) override;
        void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    private:
        ExcitationPanel& owner;
        bool isCat;
    };

    ListProxy mCatModel { *this, true };
    ListProxy mFileModel { *this, false };

    // ノブ 10基
    ValueKnob mKnobWtPos;
    ValueKnob mKnobPulseWidth;
    ValueKnob mKnobDetune;
    ValueKnob mKnobPorta;
    ValueKnob mKnobBendAmt;     // Morph: Bend 曲げ量 (-1..+1)
    ValueKnob mKnobBendShift;   // Morph: Bend 対称点
    ValueKnob mKnobSyncAmt;     // Morph: Sync 比率 (0..1 → 1〜8x)
    ValueKnob mKnobSyncShift;   // Morph: Sync 位相オフセット
    ValueKnob mKnobVocAmt;      // Morph: Vocode 効き
    ValueKnob mKnobVocShift;    // Morph: 母音モーフ位置

    // コンボ + トグル + ボタン
    juce::ComboBox mComboWaveform;
    juce::ComboBox mComboDetuneMode;
    GlowToggle mBtnDetuneSnap;
    juce::TextButton mBtnBrowse { "BROWSE" };
    juce::TextButton mBtnAddDir { "ADD DIR" };
    WaveformDisplay mWaveDisplay;

    bool mBrowserOpen = false;
    juce::ListBox mCatList;
    juce::ListBox mWtList;
    juce::TextButton mBtnBrowserClose { "CLOSE" };
    juce::TextButton mBtnFactory { "FACTORY" };
    juce::TextButton mBtnRandom { "RANDOM" };
    std::unique_ptr<juce::FileChooser> mChooser;
    juce::Random mRng;

    // ラベル
    juce::Label mLblWtPos { {}, "WT POS" };
    juce::Label mLblPulseWidth { {}, "PULSE WIDTH" };
    juce::Label mLblDetune { {}, "DETUNE" };
    juce::Label mLblPorta { {}, "PORTA" };
    juce::Label mLblBendAmt { {}, "BEND" };
    juce::Label mLblBendShift { {}, "BEND SYM" };
    juce::Label mLblSyncAmt { {}, "SYNC" };
    juce::Label mLblSyncShift { {}, "SYNC PH" };
    juce::Label mLblVocAmt { {}, "VOCODE" };
    juce::Label mLblVocShift { {}, "VOWEL" };
    juce::Label mLblMorphHdr { {}, "MORPH (同時併用可 / Amt=0でバイパス)" };
    juce::Label mLblCustomName;   // ロード中のカスタムWT名 / フォルダ状態

    // アタッチメント
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentWtPos;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentPulseWidth;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentDetune;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentPorta;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentBendAmt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentBendShift;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentSyncAmt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentSyncShift;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentVocAmt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mAttachmentVocShift;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentWaveform;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mAttachmentDetuneMode;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mAttachmentDetuneSnap;

    ArcDialLookAndFeel mArcLookAndFeel;
};
