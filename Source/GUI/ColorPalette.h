#pragma once
#include <juce_graphics/juce_graphics.h>

namespace GUI {

namespace ColorPalette {
    // 背景・基本色 (Midnight風パステルダーク)
    static inline const juce::Colour background      = juce::Colour(0xff17141f);
    static inline const juce::Colour panelBg         = juce::Colour(0xff201c2b);
    static inline const juce::Colour panelBorder     = juce::Colour(0x22ffffff);
    static inline const juce::Colour grid            = juce::Colour(0x14ffffff);
    static inline const juce::Colour textHeader      = juce::Colour(0xffe9e3f2);
    static inline const juce::Colour textBody        = juce::Colour(0xff8d86a0);
    static inline const juce::Colour textMuted       = juce::Colour(0xff605870);

    // パステルアクセント (Granular準拠)
    static inline const juce::Colour mint            = juce::Colour(0xffb5ead7); // BANDS/EQ
    static inline const juce::Colour pink            = juce::Colour(0xffffb7c5); // EXCITATION
    static inline const juce::Colour lavender        = juce::Colour(0xffc7ceea); // Main / Pitch
    static inline const juce::Colour peach           = juce::Colour(0xffffdac1); // Envelope
    static inline const juce::Colour babyBlue        = juce::Colour(0xffaed9f7); // Tracking
    static inline const juce::Colour sage            = juce::Colour(0xffe2f0cb);
    static inline const juce::Colour rose            = juce::Colour(0xffffb7b2);
    static inline const juce::Colour lilac           = juce::Colour(0xffe0c3fc);

    // フェーダー・ノブ用スライダーカラー
    static inline const juce::Colour sliderTrack     = juce::Colour(0xff2a2536);
    static inline const juce::Colour sliderThumb     = juce::Colour(0xffe9e3f2);
}

} // namespace GUI
