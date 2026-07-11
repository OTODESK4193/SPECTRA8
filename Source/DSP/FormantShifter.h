#pragma once
#include <vector>

namespace DSP {

class FormantShifter {
public:
    FormantShifter() = default;
    ~FormantShifter() = default;

    // LSPドメインでのフォルマント変調処理を実行する
    // lspCoeffs: 入力LSP根（サイズ order、降順。余弦値）
    // shiftedLsp: 変調後のLSP根（サイズ order、降順。余弦値）
    // shift: シフト量（双一次写像のalphaパラメータ、範囲 -0.5f 〜 0.5f。正で声道伸長(太声)、負で声道短縮(細声)）
    // stretch: ストレッチ量（アフィン変換のsigmaパラメータ、範囲 0.5f 〜 2.0f。1.0で変化なし、大で共鳴鋭化、小で平坦化）
    void process(const std::vector<float>& lspCoeffs, 
                 std::vector<float>& shiftedLsp, 
                 float shift, 
                 float stretch, 
                 int order);

private:
    // ガードバンド処理 (角度ドメイン)
    void applyGuardBand(std::vector<float>& lsfs, float minDistance, int order);
};

} // namespace DSP
