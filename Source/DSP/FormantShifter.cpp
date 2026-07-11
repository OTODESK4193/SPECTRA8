#include "FormantShifter.h"
#include <cmath>
#include <algorithm>

namespace DSP {

void FormantShifter::process(const std::vector<float>& srcEnvelope16k, 
                             std::vector<float>& destEnvelope16k, 
                             float shiftSemitones, 
                             float stretch)
{
    int destSize = 513;
    destEnvelope16k.resize(destSize);

    float shiftRatio = std::pow(2.0f, shiftSemitones / 12.0f);
    float scaleFactor = shiftRatio * stretch;
    if (scaleFactor < 0.01f) scaleFactor = 0.01f;

    // 16kHz領域（0Hz〜8000Hz）のグリッド
    float binWidth16k = 8000.0f / 512.0f; // 15.625 Hz
    float invBinWidth16k = 1.0f / binWidth16k;

    for (int i = 0; i < destSize; ++i)
    {
        // 現在のビンに対応する物理周波数 f (0Hz〜8000Hz)
        double f = static_cast<double>(i) * binWidth16k;

        // 逆写像による元の周波数 f_orig
        double f_orig = f / scaleFactor;

        if (f_orig < 0.0)
        {
            f_orig = 0.0;
        }

        if (f_orig <= 8000.0)
        {
            float idx = static_cast<float>(f_orig) * invBinWidth16k;
            int idx0 = static_cast<int>(idx);
            int idx1 = std::min(512, idx0 + 1);
            float frac = idx - static_cast<float>(idx0);

            destEnvelope16k[i] = srcEnvelope16k[idx0] * (1.0f - frac) + srcEnvelope16k[idx1] * frac;
        }
        else
        {
            // 8000Hz以上（折り返し境界外）は、急激にロールオフ減衰させる
            double excess = f_orig - 8000.0;
            float rollOff = std::exp(static_cast<float>(-excess / 1000.0));
            destEnvelope16k[i] = std::max(1e-5f, srcEnvelope16k[512] * rollOff);
        }
    }
}

} // namespace DSP
