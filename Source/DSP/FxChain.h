// ==========================================
// File: FxChain.h
// 4スロット直列FXチェーン (Granular FxPanel と同じくGUIからD&Dで並べ替え)
//
// FX 5種 — EDMのColor Bassサウンドを主眼に選定:
//  - Spectral Resonator : 8本のチューンド・コムフィルタ。
//                         CHORD = ROOT+コード種でピッチ配置 (音が和音で鳴る)
//                         FREE  = ディレイ時間をms直指定 (超ショートDelay+高FB)
//                         ※この2つは同じDSP。1/f秒のディレイ+高FB = チューンドコム。
//  - Multiband Drive    : 3バンド分割 → 各帯域に歪み → 再合成
//  - Formant Gate       : 16ステップのリズムゲート + ステップ毎に母音フォルマント
//  - Chorus             : 4ボイス・アンサンブル (幅出し)
//  - Reverb             : FDN風の簡易リバーブ
//
// 設計方針: 全FXが「ホストレート・ステレオ・サンプル単位」で動作する。
// ボコーダー本体(16kHz内部処理)とは独立した後段なので、帯域制限を受けない。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include <cmath>
#include <cstdint>   // uint16_t (パターンマスク)。JuceHeader経由の間接includeに頼らない

// ------------------------------------------
// 共通: 1次/2次フィルタ部品
// ------------------------------------------
namespace fxutil
{
    // 状態変数フィルタ (LP/BP/HP同時出力)
    struct Svf
    {
        float ic1 = 0.0f, ic2 = 0.0f;
        void reset() noexcept { ic1 = ic2 = 0.0f; }

        // fcNorm = fc / sampleRate (0..0.49), Q >= 0.3
        void process(float in, float fcNorm, float Q, float& lp, float& bp, float& hp) noexcept
        {
            const float g = std::tan(juce::MathConstants<float>::pi
                                   * juce::jlimit(0.0005f, 0.49f, fcNorm));
            const float k = 1.0f / juce::jmax(0.3f, Q);
            const float a1 = 1.0f / (1.0f + g * (g + k));
            const float a2 = g * a1;
            const float a3 = g * a2;

            const float v3 = in - ic2;
            const float v1 = a1 * ic1 + a2 * v3;
            const float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0f * v1 - ic1;
            ic2 = 2.0f * v2 - ic2;

            lp = v2;
            bp = v1;
            hp = in - k * v1 - v2;
        }
    };

    // 1次ローパス (ダンピング用)
    struct OnePole
    {
        float z = 0.0f;
        void reset() noexcept { z = 0.0f; }
        float lp(float in, float coeff) noexcept   // coeff: 0(素通り)..0.99(強く鈍る)
        {
            z += (1.0f - coeff) * (in - z);
            return z;
        }
    };

    // 線形補間つきディレイライン
    struct DelayLine
    {
        std::vector<float> buf;
        int writePos = 0;

        void prepare(int maxSamples)
        {
            buf.assign((size_t)juce::jmax(4, maxSamples), 0.0f);
            writePos = 0;
        }
        void reset() noexcept { std::fill(buf.begin(), buf.end(), 0.0f); writePos = 0; }

        void write(float v) noexcept
        {
            buf[(size_t)writePos] = v;
            if (++writePos >= (int)buf.size()) writePos = 0;
        }
        // delaySamples は 1.0 以上・バッファ長未満
        float read(float delaySamples) const noexcept
        {
            const int n = (int)buf.size();
            float d = juce::jlimit(1.0f, (float)(n - 2), delaySamples);
            float rp = (float)writePos - d;
            while (rp < 0.0f) rp += (float)n;

            const int i0 = (int)rp;
            const int i1 = (i0 + 1 >= n) ? 0 : i0 + 1;
            const float fr = rp - (float)i0;
            return buf[(size_t)i0] * (1.0f - fr) + buf[(size_t)i1] * fr;
        }
    };

    inline float softClip(float x) noexcept
    {
        // tanh近似 (発散防止の最終段として全FX出力に噛ませる)
        if (x < -3.0f) return -1.0f;
        if (x >  3.0f) return  1.0f;
        const float x2 = x * x;
        return x * (27.0f + x2) / (27.0f + 9.0f * x2);
    }
}

// ==========================================
// 1. Spectral Resonator
//   8本のコムフィルタ。CHORDモードではROOT+コード種から各コムの周波数を決め、
//   FREEモードでは全コムをms指定の周りに広げる。
//   コムは「フィードバック付きディレイ + ループ内ダンピング」= Karplus-Strong構造。
// ==========================================
class SpectralResonator
{
public:
    static constexpr int kNumVoices = 8;
    // コード種ごとの半音オフセット (8音ぶん。足りない分はオクターブ上へ積む)
    enum Chord { Octaves = 0, Power, Major, Minor, Sus4, Min7, Maj9, Dim, NumChords };

    static juce::StringArray getChordNames()
    {
        return { "Octaves", "Power 5", "Major", "Minor", "Sus4", "Min 7", "Maj 9", "Dim" };
    }
    // Chord = ROOTノート + コード種で固定配置
    // Free  = ディレイ時間をms直指定 (チューニングされない金属的な響き)
    // MIDI  = 押さえている鍵盤に共鳴ピッチが追従する
    static juce::StringArray getModeNames() { return { "Chord", "Free", "MIDI" }; }

    void prepare(double sr)
    {
        sampleRate = sr;
        // 最低20Hz (= sr/20 サンプル) まで対応できる長さを確保
        const int maxLen = (int)(sr / 20.0) + 8;
        for (auto& v : voicesL) v.dl.prepare(maxLen);
        for (auto& v : voicesR) v.dl.prepare(maxLen);
        reset();
    }

    void reset()
    {
        for (auto& v : voicesL) { v.dl.reset(); v.damp.reset(); v.shimHp.reset(); v.delaySamples = v.targetDelay; }
        for (auto& v : voicesR) { v.dl.reset(); v.damp.reset(); v.shimHp.reset(); v.delaySamples = v.targetDelay; }
        mLfoPhase = 0.0f; mLfoPhase2 = 0.33f;
    }

    // mode      : 0=Chord, 1=Free, 2=MIDI
    // rootHz    : Chord時の基準周波数
    // chord     : コード種
    // freeMs    : Free時のディレイ時間(ms)
    // spreadAmt : Free時のボイス間隔の広がり / Chord時のステレオ広がり
    // decaySec  : 余韻(T60)の長さ[秒]。ピッチに依らず一定になるようボイス毎に帰還量を算出する。
    //             ※旧実装は生の帰還係数だったため、同じ設定でも 55Hz=1.63秒 / 880Hz=0.12秒 と
    //               13倍も差が出ていた(高い音ほど1周期が短く、同じ係数でも早く減衰するため)。
    // damp      : 0..1 (帰還ループ内LPFの強さ。高域だけ先に減衰させる音色調整)
    // midiHz/numMidi : MIDIモード時に押鍵中の音の周波数 (0本なら直前の配置を維持)
    // shimmer   : 0..1 オクターブ上のタップを出力へ加算 (きらめき)
    // inharm    : 0..1 部分音を非調和に伸ばす (ベル/金属的な倍音)
    void setParams(int mode, float rootHz, int chord, float freeMs,
                   float spreadAmt, float decaySec, float damp,
                   float shimmer, float inharm,
                   const float* midiHz = nullptr, int numMidi = 0) noexcept
    {
        mShimmer = juce::jlimit(0.0f, 1.0f, shimmer);
        mInharm  = juce::jlimit(0.0f, 1.0f, inharm);
        // 補間損失のぶん実測が短めに出るので、表示値と体感を合わせる較正係数を掛ける
        // (無補正だと 2.0s設定→実測1.42s、15s設定→7.7s と乖離していた)
        mDecaySec = juce::jlimit(0.02f, 30.0f, decaySec * 1.55f);
        static const int kChordSemis[NumChords][8] = {
            {  0, 12, 24, 36, 48, 60, 72, 84 },   // Octaves
            {  0,  7, 12, 19, 24, 31, 36, 43 },   // Power 5
            {  0,  4,  7, 12, 16, 19, 24, 28 },   // Major
            {  0,  3,  7, 12, 15, 19, 24, 27 },   // Minor
            {  0,  5,  7, 12, 17, 19, 24, 29 },   // Sus4
            {  0,  3,  7, 10, 12, 15, 19, 22 },   // Min 7
            {  0,  4,  7, 11, 14, 16, 19, 23 },   // Maj 9
            {  0,  3,  6,  9, 12, 15, 18, 21 },   // Dim
        };

        mDamp = juce::jlimit(0.0f, 0.95f, damp);

        const int ch = juce::jlimit(0, (int)NumChords - 1, chord);
        const float sp = juce::jlimit(0.0f, 1.0f, spreadAmt);

        // MIDIモードで押鍵が無い間は直前の配置を保持する (音が急に消えないように)
        if (mode == 2 && numMidi <= 0)
            return;

        for (int i = 0; i < kNumVoices; ++i)
        {
            float delaySamples;
            if (mode == 0)   // Chord: 周波数 → 周期
            {
                const float f = juce::jlimit(20.0f, 5000.0f,
                                             rootHz * std::pow(2.0f, kChordSemis[ch][i] / 12.0f));
                delaySamples = (float)sampleRate / f;
            }
            else if (mode == 2)   // MIDI: 押鍵中の音へ追従
            {
                // 鍵盤が8音に満たない場合は上のオクターブへ積んで8ボイスを埋める
                const int n = juce::jlimit(1, kNumVoices, numMidi);
                const float baseF = midiHz[i % n];
                const int oct = i / n;
                const float f = juce::jlimit(20.0f, 5000.0f, baseF * (float)(1 << oct));
                delaySamples = (float)sampleRate / f;
            }
            else             // Free: 指定msを中心に、ボイス毎に少しずつずらす
            {
                const float ms = juce::jlimit(0.2f, 50.0f, freeMs)
                               * (1.0f + sp * 0.35f * (float)i);
                delaySamples = (float)(ms * 0.001 * sampleRate);
            }

            // 非調和性: 実弦/ベルのように高次部分音ほど上へずれる (f_n *= sqrt(1+B*n²))。
            // 低次はほぼ動かず高次ほど大きくずれるので、基音は保ったまま
            // 上の倍音だけが「うなる」= ベル/金属的な響きになる。
            // ※旧値 0.0012 では最上位ボイスでも +3.8% しか動かず体感できなかった。
            //   0.005 で最上位 +15% 程度になり、はっきりうなりが出る。
            if (mInharm > 0.001f)
            {
                const float B = mInharm * 0.005f;
                const float n = (float)(i + 1);
                delaySamples /= std::sqrt(1.0f + B * n * n);
            }

            // L/Rでわずかに長さを変えてステレオ感を出す
            const float det = sp * 0.02f * ((i % 2 == 0) ? 1.0f : -1.0f);
            voicesL[(size_t)i].targetDelay = delaySamples * (1.0f + det);
            voicesR[(size_t)i].targetDelay = delaySamples * (1.0f - det);

            // --- ボイス毎の帰還量を「余韻の長さ」から逆算する ---
            //  遅延長 D サンプルのコムは1周につき g 倍になるので、
            //  -60dB に達するまでの時間は  T60 = -3*D / (log10(g)*sr)
            //  これを g について解くと   g = 10^(-3*D / (T60*sr))
            //  こうするとピッチが変わっても余韻の長さが一定になる。
            for (auto* vv : { &voicesL[(size_t)i], &voicesR[(size_t)i] })
            {
                const float D = juce::jmax(2.0f, vv->targetDelay);
                const float g = std::pow(10.0f, -3.0f * D / (mDecaySec * (float)sampleRate));

                // ※小数遅延の線形補間による損失を打ち消す補正も試したが、
                //   短い遅延(高い部分音)では補正後の帰還量が1を超えてクランプされ、
                //   低域だけが減衰せずに残って「ボワつき」になった。
                //   補間損失は物理的な減衰として受け入れ、素の値をそのまま使う。
                //   結果として長い設定では実測がやや短めに出るが、
                //   ノブは単調で、ピッチ依存も旧実装の13倍→1.7倍まで縮んでいる。
                vv->feedback = juce::jlimit(0.0f, 0.9995f, g);   // 1.0未満を厳守(発散防止)
            }
        }
    }

    void process(float& l, float& r) noexcept
    {
        // ディレイ長のサンプル毎スムージング。
        //  ブロック毎に直接書き換えると読み出し位置が飛び、実測で入力の15倍の
        //  不連続(プチッ)が出ていた。1次平滑で読み出し位置を連続にする。
        //  ついでにテープ的なピッチグライドにもなる。
        constexpr float kGlide = 0.0004f;   // τ≈50ms @48k

        // SHIMMER用のゆっくりした揺らぎ (2本の非整数比LFOでうねりを作る)
        float modL = 0.0f, modR = 0.0f;
        if (mShimmer > 0.001f)
        {
            mLfoPhase  += 0.31f / (float)sampleRate;  if (mLfoPhase  >= 1.0f) mLfoPhase  -= 1.0f;
            mLfoPhase2 += 0.47f / (float)sampleRate;  if (mLfoPhase2 >= 1.0f) mLfoPhase2 -= 1.0f;
            const float a = std::sin(mLfoPhase  * juce::MathConstants<float>::twoPi);
            const float b = std::sin(mLfoPhase2 * juce::MathConstants<float>::twoPi);
            modL = mShimmer * 0.0025f * a;
            modR = mShimmer * 0.0025f * b;
        }

        float sumL = 0.0f, sumR = 0.0f;
        for (int i = 0; i < kNumVoices; ++i)
        {
            auto& vL = voicesL[(size_t)i];
            auto& vR = voicesR[(size_t)i];
            vL.delaySamples += kGlide * (vL.targetDelay - vL.delaySamples);
            vR.delaySamples += kGlide * (vR.targetDelay - vR.delaySamples);

            sumL += tick(vL, l, modL);
            sumR += tick(vR, r, modR);
        }
        // ボイス数で正規化 (8本足しても音量が跳ねないように)
        const float norm = 1.0f / (float)kNumVoices;
        l = fxutil::softClip(sumL * norm * 2.0f);
        r = fxutil::softClip(sumR * norm * 2.0f);
    }

private:
    struct Voice
    {
        fxutil::DelayLine dl;
        fxutil::OnePole damp;
        fxutil::OnePole shimHp;      // オクターブ上タップの低域を落とす
        float delaySamples = 100.0f; // 実際に読み出している長さ (平滑後)
        float targetDelay = 100.0f;  // setParams が書き込む目標長
        float feedback = 0.9f;       // このボイスの帰還量 (DECAYから逆算)
    };

    float tick(Voice& v, float in, float mod) noexcept
    {
        const float d = v.delaySamples * (1.0f + mod);
        const float delayed = v.dl.read(d);
        const float fb = v.damp.lp(delayed, mDamp);

        // 帰還ループは常に素のまま。SHIMMERを帰還へ入れると
        // オクターブ上の成分が再循環して増殖し、基音の共鳴を食い潰してしまう
        // (「効果が大きすぎて全てをかき消す」状態になっていた原因)。
        v.dl.write(fxutil::softClip(in + fb * v.feedback));

        float out = delayed;

        // SHIMMER: 1/2 と 1/4 の長さ = 1・2オクターブ上のタップを
        //  「出力へ並列に加算するだけ」。帰還には一切戻さない。
        //  基音の共鳴はそのまま残り、その上に倍音の煌めき(Sparkle)だけが乗る。
        if (mShimmer > 0.001f)
        {
            const float oct1 = v.dl.read(d * 0.5f);    // +1oct
            const float oct2 = v.dl.read(d * 0.25f);   // +2oct
            float sp = oct1 * 0.55f + oct2 * 0.45f;
            sp -= v.shimHp.lp(sp, 0.75f);              // 低域を落として「空気感」だけ残す
            out += sp * mShimmer * 0.75f;
        }

        return out;
    }

    double sampleRate = 44100.0;
    std::array<Voice, kNumVoices> voicesL, voicesR;
    float mDamp = 0.3f;
    float mDecaySec = 2.0f;
    float mShimmer = 0.0f;
    float mInharm = 0.0f;
    float mLfoPhase = 0.0f, mLfoPhase2 = 0.33f;
};

// ==========================================
// 2. Multiband Drive
//   3バンド(Low/Mid/High)に分けて各帯域を個別に歪ませる。
//   全帯域を一括で歪ませると低域が濁って潰れるのを避けるのが目的。
// ==========================================
class MultibandDrive
{
public:
    enum Shape { Tanh = 0, Fold, Bitcrush, NumShapes };
    static juce::StringArray getShapeNames() { return { "Tanh", "Fold", "Crush" }; }

    void prepare(double sr) { sampleRate = sr; reset(); }
    void reset()
    {
        for (auto& f : splitL) f.reset();
        for (auto& f : splitR) f.reset();
    }

    void setParams(int shape, float drive, float loMix, float midMix, float hiMix) noexcept
    {
        mShape = juce::jlimit(0, (int)NumShapes - 1, shape);
        mDrive = juce::jlimit(1.0f, 40.0f, drive);
        mLo = juce::jlimit(0.0f, 1.0f, loMix);
        mMid = juce::jlimit(0.0f, 1.0f, midMix);
        mHi = juce::jlimit(0.0f, 1.0f, hiMix);
    }

    void process(float& l, float& r) noexcept
    {
        l = processOne(l, splitL);
        r = processOne(r, splitR);
    }

private:
    float shapeOne(float x) const noexcept
    {
        switch (mShape)
        {
        case Fold:
        {
            // 三角波状に折り返す (倍音が派手に増える)。
            // ※反復で折り返す実装はDriveが大きいと回数が足りず折り切れずに
            //   +18dBの暴走を起こす。周期4の三角波として一発で畳む。
            float y = x + 1.0f;
            y -= 4.0f * std::floor(y * 0.25f);      // [0,4) へ巻き取る
            y = (y < 2.0f) ? y : (4.0f - y);        // 0→2→0 の三角
            return y - 1.0f;                        // [-1,+1]
        }
        case Bitcrush:
        {
            const float levels = 8.0f;
            return std::round(juce::jlimit(-1.0f, 1.0f, x) * levels) / levels;
        }
        default:
            return fxutil::softClip(x);
        }
    }

    float processOne(float in, std::array<fxutil::Svf, 2>& f) noexcept
    {
        // 300Hz / 2500Hz の2点で3分割
        float lp1, bp1, hp1, lp2, bp2, hp2;
        f[0].process(in, 300.0f / (float)sampleRate, 0.707f, lp1, bp1, hp1);
        f[1].process(hp1, 2500.0f / (float)sampleRate, 0.707f, lp2, bp2, hp2);

        const float low = lp1, mid = lp2, high = hp2;

        // 帯域毎に歪ませてからミックス量で戻す。1/driveで音量を揃える。
        const float g = 1.0f / std::sqrt(mDrive);
        const float dLow  = shapeOne(low  * mDrive) * g;
        const float dMid  = shapeOne(mid  * mDrive) * g;
        const float dHigh = shapeOne(high * mDrive) * g;

        const float sum = low  * (1.0f - mLo)  + dLow  * mLo
                        + mid  * (1.0f - mMid) + dMid  * mMid
                        + high * (1.0f - mHi)  + dHigh * mHi;
        // 3帯域が同位相で重なった時の突出を抑える最終段
        return fxutil::softClip(sum);
    }

    double sampleRate = 44100.0;
    std::array<fxutil::Svf, 2> splitL, splitR;
    int mShape = 0;
    float mDrive = 4.0f, mLo = 0.5f, mMid = 1.0f, mHi = 0.7f;
};

// ==========================================
// 3. Formant Gate
//   16ステップのリズムゲート。各ステップで母音(A-I-U-E-O)のフォルマントを切り替える。
//   ボコーダー出力に掛けると「喋るチョップ」になる。
// ==========================================
class FormantGate
{
public:
    static constexpr int kNumSteps = 16;
    static juce::StringArray getRateNames()
    {
        return { "1/2", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
    }
    static juce::StringArray getPatternNames()
    {
        return { "All On", "Off Beat", "Gallop", "Syncopate 1", "Syncopate 2",
                 "Trance Gate", "Stutter 4", "Straight 4", "Random 1", "Random 2",
                 "Build", "Sparse" };
    }

    void prepare(double sr) { sampleRate = sr; reset(); }
    void reset()
    {
        phase = 0.0;
        stepIdx = 0;
        env = 0.0f;
        for (auto& f : fmtL) f.reset();
        for (auto& f : fmtR) f.reset();
    }

    // rate: getRateNames のインデックス / pattern: getPatternNames のインデックス
    // depth: 0..1 ゲートの深さ / vowelAmt: 0..1 フォルマントの効き / smooth: 0..1 立ち上がり鈍化
    // shape: 0..1 ステップ内の減衰。0=そのステップ全体を保持(従来) /
    //        1=各ステップ頭で鋭く減衰する打点になる。
    void setParams(int rate, int pattern, float depth, float vowelAmt, float smooth,
                   float shape, double bpm) noexcept
    {
        mShape = juce::jlimit(0.0f, 1.0f, shape);
        static const double kBeats[7] = { 2.0, 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        mRate = juce::jlimit(0, 6, rate);
        mPattern = juce::jlimit(0, 11, pattern);
        mDepth = juce::jlimit(0.0f, 1.0f, depth);
        mVowel = juce::jlimit(0.0f, 1.0f, vowelAmt);
        mSmooth = juce::jlimit(0.0f, 1.0f, smooth);

        const double b = (bpm > 1.0) ? bpm : 120.0;
        stepHz = b / (60.0 * kBeats[mRate]);
    }

    void process(float& l, float& r) noexcept
    {
        // --- ステップ進行 ---
        phase += stepHz / sampleRate;
        while (phase >= 1.0)
        {
            phase -= 1.0;
            stepIdx = (stepIdx + 1) % kNumSteps;
            if (mShape > 0.001f && stepOn(stepIdx))
                env = 0.0f;
        }

        // --- ゲートエンベロープ (smoothで立ち上がり/下がりを鈍らせクリック防止) ---
        float target = stepOn(stepIdx) ? 1.0f : 0.0f;
        if (mShape > 0.001f && target > 0.5f)
        {
            const float decayPos = (float)phase / juce::jmax(0.05f, 1.0f - mShape * 0.92f);
            target = juce::jlimit(0.0f, 1.0f, 1.0f - decayPos);
        }

        const float tauMs = 1.0f + mSmooth * 60.0f;
        const float coeff = 1.0f - std::exp(-1.0f / (tauMs * 0.001f * (float)sampleRate));
        env += coeff * (target - env);

        const float gain = 1.0f - mDepth * (1.0f - env);

        // --- 母音フォルマント (ステップ毎に切替) ---
        if (mVowel > 0.001f)
        {
            static const float kVowelHz[5][3] = {
                {  730.0f, 1090.0f, 2440.0f },   // A
                {  270.0f, 2290.0f, 3010.0f },   // I
                {  300.0f,  870.0f, 2240.0f },   // U
                {  530.0f, 1840.0f, 2480.0f },   // E
                {  570.0f,  840.0f, 2410.0f },   // O
            };
            const int v = stepIdx % 5;
            float wetL = 0.0f, wetR = 0.0f;
            for (int j = 0; j < 3; ++j)
            {
                const float fc = kVowelHz[v][j] / (float)sampleRate;
                float lp, bp, hp;
                fmtL[(size_t)j].process(l, fc, 6.0f, lp, bp, hp);
                wetL += bp;
                fmtR[(size_t)j].process(r, fc, 6.0f, lp, bp, hp);
                wetR += bp;
            }
            l = l * (1.0f - mVowel) + wetL * 2.0f * mVowel;
            r = r * (1.0f - mVowel) + wetR * 2.0f * mVowel;
        }

        l *= gain;
        r *= gain;
    }

private:
    bool stepOn(int i) const noexcept
    {
        // 16bitのパターンマスク (全12種類)
        static const uint16_t kMasks[12] = {
            0xFFFF,             // All On
            0b1010101010101010, // Off Beat
            0b1101110111011101, // Gallop
            0b1001001001001001, // Syncopate 1
            0b0100100100100100, // Syncopate 2
            0b1100110011001100, // Trance Gate
            0b1111000011110000, // Stutter 4
            0b1000100010001000, // Straight 4
            0b1011010110110101, // Random 1
            0b1110010110100111, // Random 2
            0b1000100010101111, // Build
            0b1000000110000001, // Sparse
        };
        return (kMasks[mPattern] >> i) & 1;
    }

    double sampleRate = 44100.0;
    double phase = 0.0, stepHz = 8.0;
    int stepIdx = 0;
    float env = 0.0f;
    int mRate = 4, mPattern = 0;
    float mDepth = 1.0f, mVowel = 0.0f, mSmooth = 0.2f, mShape = 0.0f;
    std::array<fxutil::Svf, 3> fmtL, fmtR;
};

// ==========================================
// 4. Chorus (4ボイス・アンサンブル)
// ==========================================
class EnsembleChorus
{
public:
    void prepare(double sr)
    {
        sampleRate = sr;
        dlL.prepare((int)(sr * 0.05) + 8);
        dlR.prepare((int)(sr * 0.05) + 8);
        reset();
    }
    void reset() { dlL.reset(); dlR.reset(); for (auto& p : lfoPhase) p = 0.0f; }

    void setParams(float rateHz, float depthMs, float width, float mix) noexcept
    {
        mRate = juce::jlimit(0.02f, 8.0f, rateHz);
        mDepth = juce::jlimit(0.1f, 12.0f, depthMs);
        mWidth = juce::jlimit(0.0f, 1.0f, width);
        mMix = juce::jlimit(0.0f, 1.0f, mix);
    }

    void process(float& l, float& r) noexcept
    {
        dlL.write(l);
        dlR.write(r);

        float wetL = 0.0f, wetR = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            // ボイス毎にレートを少しずらす (完全同期だとフランジャーになる)
            lfoPhase[(size_t)i] += mRate * (1.0f + 0.17f * (float)i) / (float)sampleRate;
            if (lfoPhase[(size_t)i] >= 1.0f) lfoPhase[(size_t)i] -= 1.0f;

            const float mod = std::sin(lfoPhase[(size_t)i] * juce::MathConstants<float>::twoPi);
            const float baseMs = 8.0f + 3.0f * (float)i;
            const float dMs = baseMs + mod * mDepth;
            const float dSamp = dMs * 0.001f * (float)sampleRate;

            // 偶数ボイスをL寄り、奇数をR寄りに振る
            const float panL = (i % 2 == 0) ? 1.0f : (1.0f - mWidth);
            const float panR = (i % 2 == 0) ? (1.0f - mWidth) : 1.0f;
            wetL += dlL.read(dSamp) * panL;
            wetR += dlR.read(dSamp) * panR;
        }
        wetL *= 0.35f;
        wetR *= 0.35f;

        l = l * (1.0f - mMix) + wetL * mMix;
        r = r * (1.0f - mMix) + wetR * mMix;
    }

private:
    double sampleRate = 44100.0;
    fxutil::DelayLine dlL, dlR;
    std::array<float, 4> lfoPhase {};
    float mRate = 0.6f, mDepth = 4.0f, mWidth = 0.7f, mMix = 0.5f;
};

// ==========================================
// 5. Reverb (4本コムディフューザ + 2段オールパス、L/R非対称)
//   SIZE / DAMP に加えて PREDELAY / WIDTH / LOW CUT / MOD を持つ。
//   MOD はコム長を微揺らしして、固定長コム特有の金属的な付帯音を散らす。
// ==========================================
class SimpleReverb
{
public:
    void prepare(double sr)
    {
        sampleRate = sr;
        preL.prepare((int)(sr * 0.25) + 8);   // プリディレイ最大200ms
        preR.prepare((int)(sr * 0.25) + 8);
        // 互いに素に近い長さ(ms)にして金属的な癖を避ける
        static const float combMs[4] = { 29.7f, 37.1f, 41.1f, 43.7f };
        static const float apMs[2]   = {  5.0f,  1.7f };
        for (int i = 0; i < 4; ++i)
        {
            combL[(size_t)i].prepare((int)(combMs[i] * 0.001f * sr) + 4);
            combR[(size_t)i].prepare((int)((combMs[i] + 1.3f) * 0.001f * sr) + 4);
            combLenL[(size_t)i] = combMs[i] * 0.001f * (float)sr;
            combLenR[(size_t)i] = (combMs[i] + 1.3f) * 0.001f * (float)sr;
        }
        for (int i = 0; i < 2; ++i)
        {
            apL[(size_t)i].prepare((int)(apMs[i] * 0.001f * sr) + 4);
            apR[(size_t)i].prepare((int)(apMs[i] * 0.001f * sr) + 4);
            apLen[(size_t)i] = apMs[i] * 0.001f * (float)sr;
        }
        reset();
    }
    void reset()
    {
        preL.reset(); preR.reset();
        for (auto& d : combL) d.reset();
        for (auto& d : combR) d.reset();
        for (auto& d : apL) d.reset();
        for (auto& d : apR) d.reset();
        for (auto& f : dampL) f.reset();
        for (auto& f : dampR) f.reset();
        hpL.reset(); hpR.reset();
        modPhase = 0.0f;
    }

    // size/damp/mix に加えて predelayMs / width / lowCutHz / modAmt
    void setParams(float size, float damp, float mix,
                   float predelayMs, float width, float lowCutHz, float modAmt) noexcept
    {
        mSize = juce::jlimit(0.0f, 1.0f, size);
        mDamp = juce::jlimit(0.0f, 0.95f, damp);
        mMix = juce::jlimit(0.0f, 1.0f, mix);
        mFeedback = 0.70f + mSize * 0.28f;   // 最大0.98 (1未満を厳守)

        mPredelay = juce::jlimit(0.0f, 200.0f, predelayMs) * 0.001f * (float)sampleRate;
        mWidth = juce::jlimit(0.0f, 1.0f, width);
        mLowCut = juce::jlimit(20.0f, 1000.0f, lowCutHz) / (float)sampleRate;
        mModAmt = juce::jlimit(0.0f, 1.0f, modAmt);
    }

    void process(float& l, float& r) noexcept
    {
        // --- プリディレイ (初期反射までの間合い) ---
        preL.write(l);
        preR.write(r);
        const float inL = (mPredelay > 1.0f) ? preL.read(mPredelay) : l;
        const float inR = (mPredelay > 1.0f) ? preR.read(mPredelay) : r;

        // --- コム長の微揺らし (固定長コムの金属的な癖を散らす) ---
        modPhase += 0.35f / (float)sampleRate;
        if (modPhase >= 1.0f) modPhase -= 1.0f;
        const float lfo = std::sin(modPhase * juce::MathConstants<float>::twoPi);

        float wetL = 0.0f, wetR = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            // ボイス毎に位相をずらした微小変調 (最大±0.3%)
            const float ph = lfo * std::cos((float)i * 1.1f);
            const float mL = combLenL[(size_t)i] * (1.0f + mModAmt * 0.003f * ph);
            const float mR = combRenR(i) * (1.0f - mModAmt * 0.003f * ph);

            const float dL = combL[(size_t)i].read(mL);
            const float dR = combR[(size_t)i].read(mR);
            combL[(size_t)i].write(inL + dampL[(size_t)i].lp(dL, mDamp) * mFeedback);
            combR[(size_t)i].write(inR + dampR[(size_t)i].lp(dR, mDamp) * mFeedback);
            wetL += dL;
            wetR += dR;
        }
        wetL *= 0.25f;
        wetR *= 0.25f;

        // オールパスで拡散
        for (int i = 0; i < 2; ++i)
        {
            wetL = allpass(apL[(size_t)i], apLen[(size_t)i], wetL);
            wetR = allpass(apR[(size_t)i], apLen[(size_t)i], wetR);
        }

        // --- ローカット (残響で低域が濁るのを防ぐ。1次HPF = 原音 - LPF) ---
        wetL -= hpL.lp(wetL, 1.0f - juce::jlimit(0.0f, 0.9f, mLowCut * 6.2831853f));
        wetR -= hpR.lp(wetR, 1.0f - juce::jlimit(0.0f, 0.9f, mLowCut * 6.2831853f));

        // --- WIDTH (M/Sでサイド成分を伸縮) ---
        const float mid = (wetL + wetR) * 0.5f;
        const float side = (wetL - wetR) * 0.5f * (mWidth * 2.0f);
        wetL = mid + side;
        wetR = mid - side;

        l = l * (1.0f - mMix) + wetL * mMix;
        r = r * (1.0f - mMix) + wetR * mMix;
    }

private:
    static float allpass(fxutil::DelayLine& d, float len, float in) noexcept
    {
        const float g = 0.5f;
        const float delayed = d.read(len);
        const float v = in + delayed * g;
        d.write(v);
        return delayed - v * g;
    }

    float combRenR(int i) const noexcept { return combLenR[(size_t)i]; }

    double sampleRate = 44100.0;
    fxutil::DelayLine preL, preR;
    std::array<fxutil::DelayLine, 4> combL, combR;
    std::array<fxutil::DelayLine, 2> apL, apR;
    std::array<fxutil::OnePole, 4> dampL, dampR;
    fxutil::OnePole hpL, hpR;
    std::array<float, 4> combLenL {}, combLenR {};
    std::array<float, 2> apLen {};
    float mSize = 0.5f, mDamp = 0.4f, mMix = 0.3f, mFeedback = 0.84f;
    float mPredelay = 0.0f, mWidth = 0.5f, mLowCut = 0.005f, mModAmt = 0.3f;
    float modPhase = 0.0f;
};

// ==========================================
// FxChain — 4スロット直列
// ==========================================
class FxChain
{
public:
    // FX5種を全部同時に挿せるようスロットも5本
    static constexpr int kNumSlots = 5;

    enum Type { None = 0, Resonator, Drive, Gate, Chorus, Reverb, NumTypes };

    static juce::StringArray getTypeNames()
    {
        return { "---", "Resonator", "Drive", "Gate", "Chorus", "Reverb" };
    }

    struct SlotParams
    {
        int type = None;
        float amount = 1.0f;   // このスロットのDry/Wet
    };

    struct Params
    {
        std::array<SlotParams, kNumSlots> slot;

        // Resonator
        int   resMode = 0;      // 0=Chord 1=Free 2=MIDI
        float resShimmer = 0.0f, resInharm = 0.0f;
        float resRootHz = 110.0f;
        std::array<float, 8> midiHz {};   // MIDIモード時の押鍵周波数 (低い順)
        int   numMidiHz = 0;
        int   resChord = 2;
        float resFreeMs = 5.0f;
        float resSpread = 0.4f;
        float resDecay = 2.0f;   // 余韻の長さ[秒]
        float resDamp = 0.35f;

        // Drive
        int   drvShape = 0;
        float drvDrive = 4.0f;
        float drvLow = 0.4f, drvMid = 1.0f, drvHigh = 0.7f;

        // Gate
        int   gateRate = 4, gatePattern = 1;
        float gateDepth = 1.0f, gateVowel = 0.0f, gateSmooth = 0.2f, gateShape = 0.0f;

        // Chorus
        float choRate = 0.6f, choDepth = 4.0f, choWidth = 0.7f;

        // Reverb
        float revSize = 0.5f, revDamp = 0.4f;
        float revPredelay = 20.0f, revWidth = 0.6f, revLowCut = 200.0f, revMod = 0.3f;

        double bpm = 120.0;
    };

    void prepare(double sr)
    {
        mResonator.prepare(sr);
        mDrive.prepare(sr);
        mGate.prepare(sr);
        mChorus.prepare(sr);
        mReverb.prepare(sr);

        // Amount / Type 切替の平滑係数。
        //  Amount はブロック毎の階段だとジッパーノイズになるので τ=15ms、
        //  Type 切替は一度 Amount を 0 まで落としてから差し替えるので τ=8ms。
        const double s = (sr > 1000.0) ? sr : 48000.0;
        mAmtCoef  = (float)(1.0 - std::exp(-1.0 / (0.015 * s)));
        mSwapCoef = (float)(1.0 - std::exp(-1.0 / (0.008 * s)));
        reset();
    }

    void reset()
    {
        mResonator.reset();
        mDrive.reset();
        mGate.reset();
        mChorus.reset();
        mReverb.reset();
        for (auto& s : mSlotRt)
            s = {};
    }

    // ブロック先頭で1回だけ呼ぶ。
    //  smoothCoef: 連続値パラメータの1極平滑係数。呼び出し側がブロック長から
    //  coef = 1 - exp(-blockSec/0.03) として渡す。1.0 で平滑なし。
    //  DECAY や SIZE のような連続パラメータはブロック毎の階段だと段差が聞こえるため、
    //  ここでいったん均してから各FXへ渡す (バッファ1024smpなら21msの段差が消える)。
    void syncParameters(const Params& p, float smoothCoef = 1.0f) noexcept
    {
        const float c = juce::jlimit(0.0f, 1.0f, smoothCoef);
        auto sm = [c](float& cur, float tgt) { cur += c * (tgt - cur); };

        if (!mParamsPrimed)
        {
            mParams = p;
            mParamsPrimed = true;
        }
        else
        {
            // 整数・離散値はそのまま反映 (平滑すると中間値が生まれて破綻する)
            mParams.slot     = p.slot;
            mParams.resMode  = p.resMode;
            mParams.resChord = p.resChord;
            mParams.drvShape = p.drvShape;
            mParams.gateRate = p.gateRate;
            mParams.gatePattern = p.gatePattern;
            mParams.midiHz   = p.midiHz;
            mParams.numMidiHz = p.numMidiHz;
            mParams.bpm      = p.bpm;

            // 連続値は1極で追従させる
            sm(mParams.resShimmer, p.resShimmer);  sm(mParams.resInharm, p.resInharm);
            sm(mParams.resRootHz,  p.resRootHz);   sm(mParams.resFreeMs, p.resFreeMs);
            sm(mParams.resSpread,  p.resSpread);   sm(mParams.resDecay,  p.resDecay);
            sm(mParams.resDamp,    p.resDamp);
            sm(mParams.drvDrive,   p.drvDrive);    sm(mParams.drvLow,    p.drvLow);
            sm(mParams.drvMid,     p.drvMid);      sm(mParams.drvHigh,   p.drvHigh);
            sm(mParams.gateDepth,  p.gateDepth);   sm(mParams.gateVowel, p.gateVowel);
            sm(mParams.gateSmooth, p.gateSmooth);  sm(mParams.gateShape, p.gateShape);
            sm(mParams.choRate,    p.choRate);     sm(mParams.choDepth,  p.choDepth);
            sm(mParams.choWidth,   p.choWidth);
            sm(mParams.revSize,    p.revSize);     sm(mParams.revDamp,   p.revDamp);
            sm(mParams.revPredelay,p.revPredelay); sm(mParams.revWidth,  p.revWidth);
            sm(mParams.revLowCut,  p.revLowCut);   sm(mParams.revMod,    p.revMod);
        }

        // 以降は平滑済みの mParams を各FXへ渡す (p ではないことに注意)
        const Params& q = mParams;
        mResonator.setParams(q.resMode, q.resRootHz, q.resChord, q.resFreeMs,
                             q.resSpread, q.resDecay, q.resDamp,
                             q.resShimmer, q.resInharm,
                             q.midiHz.data(), q.numMidiHz);
        mDrive.setParams(q.drvShape, q.drvDrive, q.drvLow, q.drvMid, q.drvHigh);
        mGate.setParams(q.gateRate, q.gatePattern, q.gateDepth, q.gateVowel, q.gateSmooth,
                        q.gateShape, q.bpm);
        // Mixは各slotのAmountで管理するのでFX内部のMixは常に1.0
        mChorus.setParams(q.choRate, q.choDepth, q.choWidth, 1.0f);
        mReverb.setParams(q.revSize, q.revDamp, 1.0f,
                          q.revPredelay, q.revWidth, q.revLowCut, q.revMod);
    }

    // スロット順に直列適用。各スロットのAmountがそのFXのDry/Wet。
    //
    //  【改修 2026-08-01】
    //   1. Amount=0 でも FX 本体は動かし続ける (出力ミックスだけ 0 にする)。
    //      旧実装は丸ごと skip していたため、リバーブやレゾネーターの状態が凍結し、
    //      Amount を戻した瞬間に古い残響が復活してポップしていた。
    //   2. Amount をサンプル単位で平滑 (τ=15ms)。ブロック毎の階段を無くす。
    //   3. Type を変えるときは、いったん Amount を 0 まで落としてから差し替える
    //      (τ=8ms)。旧実装は瞬時に切り替わり必ずプツッと鳴っていた。
    void processSample(float& l, float& r) noexcept
    {
        for (int s = 0; s < kNumSlots; ++s)
        {
            const auto& sp = mParams.slot[(size_t)s];
            auto& rt = mSlotRt[(size_t)s];

            // --- Type 切替: 一度ミックスを 0 へ落としてから差し替える ---
            if (sp.type != rt.curType)
            {
                rt.swapping = true;
                rt.mix += mSwapCoef * (0.0f - rt.mix);
                if (rt.mix < 0.001f)
                {
                    rt.mix = 0.0f;
                    rt.curType = sp.type;
                    rt.swapping = false;
                    resetSlotEffect(sp.type);   // 新しいFXは綺麗な状態から始める
                }
            }
            else if (!rt.swapping)
            {
                const float target = (rt.curType == None)
                                       ? 0.0f : juce::jlimit(0.0f, 1.0f, sp.amount);
                rt.mix += mAmtCoef * (target - rt.mix);
            }

            if (rt.curType == None)
                continue;

            // Amount=0 でも本体は常に走らせる (残響の尾を保つため)
            float wl = l, wr = r;
            switch (rt.curType)
            {
            case Resonator: mResonator.process(wl, wr); break;
            case Drive:     mDrive.process(wl, wr);     break;
            case Gate:      mGate.process(wl, wr);      break;
            case Chorus:    mChorus.process(wl, wr);    break;
            case Reverb:    mReverb.process(wl, wr);    break;
            default: continue;
            }

            const float a = rt.mix;
            if (a > 0.0f)
            {
                l = l * (1.0f - a) + wl * a;
                r = r * (1.0f - a) + wr * a;
            }
        }
    }

    // 現在どれかのスロットが処理を必要としているか。
    //  Amount=0 でも状態を進め続けたいので、Type が入っていれば true を返す。
    bool isActive() const noexcept
    {
        for (int s = 0; s < kNumSlots; ++s)
            if (mParams.slot[(size_t)s].type != None || mSlotRt[(size_t)s].curType != None
                || mSlotRt[(size_t)s].mix > 0.0f)
                return true;
        return false;
    }

private:
    // スロット差し替え時に、そのFXの内部状態だけを初期化する
    void resetSlotEffect(int type) noexcept
    {
        switch (type)
        {
        case Resonator: mResonator.reset(); break;
        case Drive:     mDrive.reset();     break;
        case Gate:      mGate.reset();      break;
        case Chorus:    mChorus.reset();    break;
        case Reverb:    mReverb.reset();    break;
        default: break;
        }
    }

    // スロット毎のランタイム状態 (平滑済みミックスと現在有効なFX種別)
    struct SlotRt
    {
        int   curType  = None;
        float mix      = 0.0f;
        bool  swapping = false;
    };
    std::array<SlotRt, kNumSlots> mSlotRt {};
    float mAmtCoef  = 0.002f;
    float mSwapCoef = 0.004f;
    bool  mParamsPrimed = false;   // 初回は平滑せず即値で取り込む

    Params mParams;
    SpectralResonator mResonator;
    MultibandDrive mDrive;
    FormantGate mGate;
    EnsembleChorus mChorus;
    SimpleReverb mReverb;
};
