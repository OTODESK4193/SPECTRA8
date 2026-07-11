#pragma once
#include <vector>

namespace DSP {

class FormantShifter {
public:
    FormantShifter() = default;
    ~FormantShifter() = default;

    // 16kHz領域（0Hz〜8000Hz）でのフォルマント変調処理
    // srcEnvelope16k: 16kHz領域の入力スペクトル包絡（513点、0Hz〜8000Hzに対応）
    // destEnvelope16k: 16kHz領域の変調後スペクトル包絡（513点、0Hz〜8000Hzに対応）
    // shiftSemitones: フォルマントシフト（半音、例: -24.0f 〜 24.0f）
    // stretch: フォルマント伸縮（倍率、0.5f 〜 2.0f）
    void process(const std::vector<float>& srcEnvelope16k, 
                 std::vector<float>& destEnvelope16k, 
                 float shiftSemitones, 
                 float stretch);
};

} // namespace DSP
