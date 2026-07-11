#include "FormantShifter.h"
#include <cmath>
#include <algorithm>

namespace DSP {

void FormantShifter::process(const std::vector<float>& srcEnvelope16k, 
                             std::vector<float>& destEnvelopeFs, 
                             float shiftSemitones, 
                             float stretch, 
                             double nativeSampleRate)
{
    // ホストSRのFFTサイズ2048に対応するスペクトル包絡（1025点、DCからNyquistまで）
    int destSize = 1025;
    destEnvelopeFs.resize(destSize);

    float shiftRatio = std::pow(2.0f, shiftSemitones / 12.0f);
    float scaleFactor = shiftRatio * stretch;
    if (scaleFactor < 0.01f) scaleFactor = 0.01f;

    // 16kHz領域の513点スペクトルのグリッド幅 (0Hz〜8000Hzを512等分)
    float binWidth16k = 8000.0f / 512.0f;
    float invBinWidth16k = 1.0f / binWidth16k;

    double nyquistFs = nativeSampleRate * 0.5;

    for (int i = 0; i < destSize; ++i)
    {
        // 1. 現在のホストSR上の物理周波数 f を算出
        double f = (static_cast<double>(i) / 1024.0) * nyquistFs;

        // 2. 変調（シフト＆ストレッチ）の逆写像により、元の16kHz領域での周波数 f_orig を得る
        double f_orig = f / scaleFactor;

        if (f_orig < 0.0)
        {
            f_orig = 0.0;
        }

        // 3. 16kHz領域（0Hz〜8000Hz）のエンベロープから線形補間
        if (f_orig <= 8000.0)
        {
            float idx = static_cast<float>(f_orig) * invBinWidth16k;
            int idx0 = static_cast<int>(idx);
            int idx1 = std::min(512, idx0 + 1);
            float frac = idx - static_cast<float>(idx0);

            destEnvelopeFs[i] = srcEnvelope16k[idx0] * (1.0f - frac) + srcEnvelope16k[idx1] * frac;
        }
        else
        {
            // 8000Hz以上の高域は、声のエネルギーがないため、緩やかにロールオフ（1オクターブあたり約-12dB）を適用
            // これにより高域の極によるフィルタ不安定化を完全に防ぎます
            double excess = f_orig - 8000.0;
            float rollOff = std::exp(static_cast<float>(-excess / 1500.0)); // 1500Hzごとに約 -8.6dB 減衰
            
            // 最小値（ノイズフロア）として 1e-5f (-100dB) を保証
            destEnvelopeFs[i] = std::max(1e-5f, srcEnvelope16k[512] * rollOff);
        }
    }
}

} // namespace DSP
