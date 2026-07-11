#pragma once
#include <juce_graphics/juce_graphics.h>

namespace GUI {

struct ColorPalette {
    static inline const juce::Colour background      = juce::Colour::fromString("FF121214"); // 超深ダーク
    static inline const juce::Colour panelBg         = juce::Colour::fromString("FF1A1A1E"); // パネル背景
    static inline const juce::Colour panelBorder     = juce::Colour::fromString("FF2E2E36"); // パネル境界
    
    // ネオンアクセント
    static inline const juce::Colour neonCyan        = juce::Colour::fromString("FF00F0FF"); // シアン (LPC/フォルマント)
    static inline const juce::Colour neonPink        = juce::Colour::fromString("FFFF007F"); // ピンク (オシレーター)
    static inline const juce::Colour neonPurple      = juce::Colour::fromString("FFB000FF"); // パープル (変調)
    static inline const juce::Colour neonGreen       = juce::Colour::fromString("FF39FF14"); // ライムグリーン (レベル)

    // テキスト
    static inline const juce::Colour textHeader      = juce::Colour::fromString("FFFFFFFF");
    static inline const juce::Colour textBody        = juce::Colour::fromString("FFB0B0B8");
    static inline const juce::Colour textMuted       = juce::Colour::fromString("FF606068");

    // フェーダー・ノブ用スライダーカラー
    static inline const juce::Colour sliderTrack     = juce::Colour::fromString("FF242428");
    static inline const juce::Colour sliderThumb     = juce::Colour::fromString("FF00F0FF");
};

} // namespace GUI
