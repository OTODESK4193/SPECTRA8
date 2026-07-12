// ==========================================
// File: ColorPalette.h
// SPECTRA8 パステル・カラーパレット（Granular 準拠 / Midnight テーマ固定）
// 表示のみのグローバル設定。オーディオ処理には一切関与しない。
// ==========================================
#pragma once

#include <JuceHeader.h>

namespace SpectraColors
{
    // 背景・基本色
    inline const juce::Colour bg        { 0xff17141f };
    inline const juce::Colour panel     { 0xff201c2b };
    inline const juce::Colour panelLine { 0x22ffffff };
    inline const juce::Colour grid      { 0x14ffffff };
    inline const juce::Colour text      { 0xffe9e3f2 };
    inline const juce::Colour textDim   { 0xff8d86a0 };
    inline const juce::Colour knobTrack { 0xff2a2536 };

    // パステルパレット
    inline const juce::Colour mint      { 0xffb5ead7 };
    inline const juce::Colour pink      { 0xffffb7c5 };
    inline const juce::Colour lavender  { 0xffc7ceea };
    inline const juce::Colour peach     { 0xffffdac1 };
    inline const juce::Colour babyBlue  { 0xffaed9f7 };
    inline const juce::Colour sage      { 0xffe2f0cb };
    inline const juce::Colour rose      { 0xffffb7b2 };
    inline const juce::Colour lilac     { 0xffe0c3fc };

    // セクションアクセント
    inline const juce::Colour accentVocoder    = lavender;  // VOCODER タブ
    inline const juce::Colour accentExcitation = pink;      // EXCITATION タブ
    inline const juce::Colour accentMod        = lilac;     // MOD MATRIX タブ
    inline const juce::Colour accentBands      = mint;      // BANDS EQ タブ
    inline const juce::Colour accentEnv        = peach;     // ADSR
}
