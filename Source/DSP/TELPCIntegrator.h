#pragma once
#include <vector>
#include <memory>
#include <juce_dsp/juce_dsp.h>
#include "BarkFilterBank.h"

namespace DSP {

class TELPCIntegrator {
public:
    TELPCIntegrator();
    ~TELPCIntegrator() = default;

    // 初期化
    // fftOrder: FFTサイズ決定用（例: 1024の場合は10）
    void setup(int fftOrder);

    // True Envelope 振幅包絡と Bark フィルタバンク下限包絡を結合し、
    // 最小位相を保証したLPC係数を抽出する。
    // 入力:
    //   teEnvelope: TrueEnvelopeクラスで抽出したリニア振幅包絡 (サイズ N_FFT / 2 + 1)
    //   barkEnergies: BarkFilterBankクラスで得られた各バンドエネルギー (サイズ 24)
    //   gamma: 結合係数（通常 0.85f）
    //   order: 求めるLPCの次数（例: 12〜18）
    // 出力:
    //   lpcCoeffs: 算出された最小位相LPC係数 (サイズ order + 1、coeffs[0] = 1.0)
    //   gain: フィルタのゲインG
    bool integrate(const std::vector<float>& teEnvelope,
                   const std::vector<float>& barkEnergies,
                   float gamma,
                   int order,
                   std::vector<float>& lpcCoeffs,
                   float& gain);

private:
    int mFftOrder;
    int mFftSize;
    int mNumBins;

    std::unique_ptr<juce::dsp::FFT> mFft;
    
    std::vector<float> mPowerSpectrum;
    std::vector<float> mBarkLowerBound;
    std::vector<float> mCombinedSpectrum;
    std::vector<float> mAutocorrBuffer;
    
    BarkFilterBank mBarkFilter;
};

} // namespace DSP
