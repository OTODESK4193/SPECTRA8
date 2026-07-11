#pragma once
#include <vector>
#include "VoiceState.h"

namespace DSP {

class OscillatorBank {
public:
    OscillatorBank();
    ~OscillatorBank() = default;

    // 初期化とWavetableの生成
    void setup(double sampleRate);

    // 8ボイス並列のオシレーターおよびLPC合成フィルタを一括処理する (AVX2 SIMD)
    // state: PolyphonicVoiceSoA 状態構造体への参照
    // activeVoicesMask: 各ボイスのアクティブ状態フラグ (1.0f = ON, 0.0f = OFF) 8個パックされた __m256
    // noiseMix: 各ボイスのノイズのミックス量 (0.0f = 完全音源, 1.0f = 完全ノイズ) 8個パックされた __m256
    // noiseBuffer: 8ボイス分の独立したノイズサンプルが入った __m256 ベクトル
    // outL: 出力される左チャンネル混合結果 (スカラー)
    // outR: 出力される右チャンネル混合結果 (スカラー)
    void processSampleAVX2(PolyphonicVoiceSoA& state,
                            __m256 activeVoicesMask,
                            __m256 noiseMix,
                            __m256 noiseBuffer,
                            float& outL,
                            float& outR);

private:
    void generateWavetables();

    double mSampleRate;
    
    // アライメントされたウェーブテーブルバッファ (L1キャッシュ常駐用、サイズ2048)
    // メンバーとして持たせる
    alignas(32) std::vector<float> mWavetableSaw;
    alignas(32) std::vector<float> mWavetablePulse;
    alignas(32) std::vector<float> mWavetableTri;
};

} // namespace DSP
