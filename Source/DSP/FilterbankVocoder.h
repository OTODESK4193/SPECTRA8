// ==========================================
// File: FilterbankVocoder.h
// アナログ風フィルターバンク・ボコーダー (スカラー設計)
//
//  - BPF Bank (2次SVFカスケード、バンド間隔連動Q + RESONANCEスケール)
//  - 8〜48バンドの動的追従
//  - フォルマント・シフト/ストレッチによる合成側フィルタ周波数の制御
//  - 帯域交互ステレオ・パンニング
//  ※ Subtractive LR4 タイプは廃止済み (2026-07-18)
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include <cmath>

class FilterbankVocoder
{
public:
    static constexpr int kMaxBands = 48;
    static constexpr double kInternalSampleRate = 16000.0;

    FilterbankVocoder();
    ~FilterbankVocoder() = default;

    void prepare(double sampleRate);
    void reset();

    // バンドレイアウト構築 (bands本で80-7500Hz全域をmel分割)。
    // バンド数が変わったときのみ再計算+状態リセット。同数なら何もしない。
    void rebuildLayout(int bands) noexcept;

    // 1サンプル処理
    // modulator: 分析側に入力する音声サンプル
    // carrierL/R: 合成側のステレオキャリア入力サンプル
    // outL/R: ボコーディング後のステレオ出力サンプル (書き戻し)
    // resonance: バンド幅スケール (0.3=太い/緩い 〜 1.0=標準 〜 3.0=鋭い)。
    //            バンド毎の基準Q(バンド間隔連動)に乗算される。
    void processSample(float modulator, float carrierL, float carrierR,
                       float& outL, float& outR,
                       int bandCount, float character, float resonance,
                       float formantShift, float formantStretch,
                       float stereoWidth,
                       const std::array<std::atomic<float>, kMaxBands>& bandGains,
                       std::array<std::atomic<float>, kMaxBands>& bandLevelsForUi) noexcept;

    // 分析専用パス (合成は行わない)。
    // LPCモードのようにフィルターバンク合成を実行しないときでも、
    // BANDS EQ のアナライザー(入力音声の帯域レベルメーター)を動かし続けるために使う。
    // 内部の分析フィルタ状態と mEnvValues、そして bandLevelsForUi のみを更新する。
    void analyzeForMeter(float modulator, int bandCount, float character, float resonance,
                         std::array<std::atomic<float>, kMaxBands>& bandLevelsForUi) noexcept
    {
        const int wantBands = juce::jlimit(8, kMaxBands, bandCount);
        // フェード中は合成側がレイアウトを握っているので触らない (メーターだけの用途)
        if (mFadeDir == 0)
            rebuildLayout(wantBands);
        const int activeBands = juce::jmax(8, mCurBands);
        updateDetectorCoeffs(character);
        for (int i = 0; i < activeBands; ++i)
        {
            updateAnalysisBand(i, modulator, character, resonance);
            bandLevelsForUi[(size_t)i].store(mEnvValues[(size_t)i] * kMeterGain);
        }
    }

private:
    // CHARACTER に依存する検波係数の再計算。
    //  exp() を毎サンプル×バンド数ぶん回すと重いので、CHARACTER が実際に
    //  動いたときだけ 2回だけ計算してキャッシュする。
    inline void updateDetectorCoeffs(float character) noexcept
    {
        const float c = juce::jlimit(0.0f, 1.0f, character);
        if (std::abs(c - mCharCached) < 1.0e-4f)
            return;
        mCharCached = c;

        // CHARACTER=1.0 → アタック1.5ms/リリース8ms (子音が立つ鋭い設定)
        // CHARACTER=0.0 → アタック13.5ms/リリース68ms (もやっと滑らかな設定)
        //
        //  アタックを 0.5ms まで速くすると、声門パルス1発ごとに検波が跳ね上がって
        //  かえってビビり音が増える (実測 0.0305 と旧実装 0.0295 より悪化)。
        //  1.5ms なら立ち上がりの鈍りは +1.1ms に収まり、ビビりは 0.0223 まで下がる
        //  (Docs/sim の掃引結果より、明瞭度とのバランスが最も良い点)。
        const float attT = 0.0015f + (1.0f - c) * 0.0120f;
        const float relT = 0.0080f + (1.0f - c) * 0.0600f;
        mAttCoef     = 1.0f - std::exp(-1.0f / (attT * (float)kInternalSampleRate));
        mRelCoefBase = 1.0f - std::exp(-1.0f / (relT * (float)kInternalSampleRate));
    }

    // 分析側(モジュレーター)の1バンド分の処理。mEnvValues[i] を更新する。
    // processSample と analyzeForMeter の両方から呼ばれる共通ロジック。
    // mEnvValues は「正規化された帯域振幅」(中心利得0dBのフィルタ出力包絡) を保持する。
    // ※ 呼び出し前に updateDetectorCoeffs(character) を1回通しておくこと。
    inline void updateAnalysisBand(int i, float modulator,
                                   float character, float resonance) noexcept
    {
        juce::ignoreUnused(character);   // 係数は updateDetectorCoeffs 側で確定済み
        const float f0 = mBandF0[(size_t)i];
        // バンド間隔連動Q (定オーバーラップ設計) × Resonanceスケール。
        // melスケール配置では低域の間隔が狭く高域は広いため、固定Qだと
        // 低域に感度の谷・高域に過剰オーバーラップが生じる。バンド幅を
        // 隣接エッジ幅に連動させ、全帯域で均一なカバレッジにする。
        const float Q = juce::jlimit(1.0f, 40.0f, mBandQ[(size_t)i] * resonance);

        float g_anal, k_anal, a1_anal;
        computeFilterCoeffs(f0, Q, g_anal, k_anal, a1_anal);

        // ZDF SVF 1段目 (BPF)
        auto& s1 = mAnalSvf[(size_t)i][0];
        float v1 = a1_anal * (s1.s1 + g_anal * (modulator - s1.s2));
        float y_bp = v1;
        float y_lp = s1.s2 + g_anal * v1;
        s1.s1 = 2.0f * y_bp - s1.s1;
        s1.s2 = 2.0f * y_lp - s1.s2;

        // ZDF SVF 2段目 (BPF)
        auto& s2 = mAnalSvf[(size_t)i][1];
        float v1_2 = a1_anal * (s2.s1 + g_anal * (y_bp - s2.s2));
        float y_bp_2 = v1_2;
        float y_lp_2 = s2.s2 + g_anal * v1_2;
        s2.s1 = 2.0f * y_bp_2 - s2.s1;
        s2.s2 = 2.0f * y_lp_2 - s2.s2;

        // 中心利得正規化: SVF-BPカスケードの中心利得はQ²。1/Q²を掛けて
        // 中心0dBに揃える(Q可変化してもバンド間・Resonance変更時のレベルが暴れない)。
        const float analOut = y_bp_2 / (Q * Q);

        // エンベロープ追従。
        //  リリースは「CHARACTER で決まる値」と「バンド毎の下限」の遅い方を採る。
        //  検波リリースが声の基音周期より短いと、エンベロープが1周期ごとに
        //  落ちきって振幅変調 = 耳障りな「ビビり音」になるため、
        //  低域ほど長いリリース下限を課す (mRelCoefFloor は rebuildLayout で算出)。
        //  高域は下限が緩いので、子音のキレ (CHARACTER の効き) はそのまま残る。
        const float env = std::abs(analOut);
        const float coeff = (env > mEnvValues[(size_t)i])
                              ? mAttCoef
                              : juce::jmin(mRelCoefBase, mRelCoefFloor[(size_t)i]);
        mEnvValues[(size_t)i] += coeff * (env - mEnvValues[(size_t)i]);
    }

    struct SVFState
    {
        float s1 = 0.0f;
        float s2 = 0.0f;
        void reset() { s1 = 0.0f; s2 = 0.0f; }
    };

    // 出力メイクアップ。
    //  旧値 32.36 は「旧実装と同じ音量」を狙った較正値だったが、その旧実装自体が
    //  過大だった。数値シミュレーション(Docs/sim/)で自己ボコード(キャリア=モジュレーター)
    //  を測ると +11.1dB の過剰ゲインがあり、既定設定で出力ピークが +6.15dBFS
    //  (全サンプルの約30%が0dBFS超) に達していた。
    //  約 -12dB してユニティへ揃える。既定設定のピークは約 -4.8dBFS になる。
    static constexpr float kBpfMakeup = 8.0f;

    // BANDS EQ メーター表示用ゲイン。正規化後の帯域振幅は旧実装のQ²(=100)倍
    // 小さくなったため、表示スケールを旧実装と揃える。
    static constexpr float kMeterGain = 100.0f;

    void computeFilterCoeffs(float fc, float Q, float& g, float& k, float& a1) noexcept
    {
        g = std::tan(3.14159265f * fc / (float)kInternalSampleRate);
        k = 1.0f / Q;
        a1 = 1.0f / (1.0f + g * (g + k));
    }

    double mSampleRate = 44100.0;
    int mCurBands = 0;   // 現在構築済みのバンド数 (rebuildLayout用)

    // 分析側バンド周波数
    std::array<float, kMaxBands> mBandF0 {};

    // バンド端周波数 (kMaxBands+1 エッジ)。バンド間隔連動Qの導出に使用。
    std::array<float, kMaxBands + 1> mBandEdges {};
    std::array<float, kMaxBands> mBandQ {};    // バンド間隔連動の基準Q (f0/エッジ幅)

    // ---- BANDS 切替のフェード ----------------------------------------
    //  rebuildLayout() は全フィルタ状態を reset() するため、バンド数が1つ変わるだけで
    //  必ず「プツッ」と鳴っていた。切替を検知したら
    //   ①10ms かけて出力をフェードアウト → ②レイアウト再構築 → ③10ms でフェードイン
    //  の順に行い、無音の瞬間に切り替える。
    static constexpr int kBandFadeLen = 160;   // 10ms @16kHz
    int mPendingBands = -1;
    int mFadeDir = 0;        // 0=通常 / -1=フェードアウト中 / +1=フェードイン中
    int mFadeCounter = 0;

    // BANDS EQ 帯域ゲインの平滑 (τ≈10ms)。EQをドラッグ中のジッパーノイズ対策。
    static constexpr float kGainSmCoef = 0.0062f;   // 1-exp(-1/(0.010*16000))
    std::array<float, kMaxBands> mGainSm {};
    bool mGainSmPrimed = false;

    // 検波(エンベロープ追従)の係数。
    //  mAttCoef / mRelCoefBase は CHARACTER 依存 (updateDetectorCoeffs でキャッシュ)。
    //  mRelCoefFloor はバンド中心周波数から決まるリリース下限 (rebuildLayout で算出)。
    float mCharCached = -1.0f;      // <0 = 未計算。次回必ず再計算させる
    float mAttCoef = 0.05f;
    float mRelCoefBase = 0.01f;
    std::array<float, kMaxBands> mRelCoefFloor {};

    // フィルタ状態変数
    std::array<std::array<SVFState, 2>, kMaxBands> mAnalSvf {}; // 2段カスケード用
    std::array<std::array<SVFState, 2>, kMaxBands> mSynthSvfL {};
    std::array<std::array<SVFState, 2>, kMaxBands> mSynthSvfR {};

    // 包絡線追従
    std::array<float, kMaxBands> mEnvValues {};
};
