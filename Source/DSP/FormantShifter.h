#pragma once
#include <vector>

namespace DSP {

class FormantShifter {
public:
    FormantShifter() = default;
    ~FormantShifter() = default;

    // スペクトル領域でのフォルマント変調（ホストSRへのマッピング込み）
    // srcEnvelope16k: 16kHz領域のスペクトル包絡（513点、0Hz〜8000Hzに対応）
    // destEnvelopeFs: 変調後のホストSR用スペクトル包絡（1025点、0Hz〜Fs/2に対応）
    // shiftSemitones: フォルマントシフト（半音、例: -12.0f 〜 12.0f）
    // stretch: フォルマント伸縮（倍率、0.5f 〜 2.0f）
    // nativeSampleRate: ホストサンプリングレート（例: 44100.0）
    void process(const std::vector<float>& srcEnvelope16k, 
                 std::vector<float>& destEnvelopeFs, 
                 float shiftSemitones, 
                 float stretch, 
                 double nativeSampleRate);
};

} // namespace DSP
