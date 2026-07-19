// ==========================================
// File: ModRing.h
// ノブへ「変調レンジ帯 + ライブ位置ドット」を反映する共有ヘルパ (Granular MainPanel 準拠)
//
//  ArcDialLookAndFeel は mod_active / mod_min / mod_max / mod_live の4プロパティを
//  読んで描画する。それを毎フレーム書き込むのがこのヘルパの役目。
//
//  【要点】レンジの実値換算に ModMatrix::applyMod() をそのまま使っている。
//  DSP側の変調適用と完全に同じ関数を通るので、「表示された帯と実際の効きが違う」
//  という定番のズレが構造的に発生しない。
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <cmath>
#include "../DSP/ModMatrix.h"

namespace ModRing
{
    // s   : 対象ノブ (ArcDialLookAndFeel が付いていること)
    // mm  : 変調量とレンジの供給元
    // dst : ModMatrix::Dst
    inline void apply(juce::Slider& s, const ModMatrix& mm, int dst)
    {
        auto& props = s.getProperties();

        const float rMin = mm.getRangeMin(dst);
        const float rMax = mm.getRangeMax(dst);

        // この宛先へ向いているスロットが1つも無ければ帯を消す
        if (std::abs(rMax - rMin) <= 1.0e-4f)
        {
            if ((bool)props.getWithDefault("mod_active", false))
            {
                props.set("mod_active", false);
                s.repaint();
            }
            return;
        }

        const auto range = s.getNormalisableRange();
        const float base = (float)s.getValue();

        // mod単位 → 実パラメータ値 → ノブ正規化位置(0..1)
        auto toNorm = [&](float modUnit)
        {
            float real = ModMatrix::applyMod(dst, base, modUnit);
            real = juce::jlimit((float)range.start, (float)range.end, real);
            return (float)range.convertTo0to1(real);
        };

        const float nMin  = toNorm(rMin);
        const float nMax  = toNorm(rMax);
        const float nLive = toNorm(mm.get(dst));

        // 変化があった時だけ repaint (30Hz×27ノブを毎回再描画すると重いため)
        const float oldMin  = (float)props.getWithDefault("mod_min", -1.0f);
        const float oldMax  = (float)props.getWithDefault("mod_max", -1.0f);
        const float oldLive = (float)props.getWithDefault("mod_live", -1.0f);

        if (!(bool)props.getWithDefault("mod_active", false)
            || std::abs(nMin - oldMin) > 0.002f
            || std::abs(nMax - oldMax) > 0.002f
            || std::abs(nLive - oldLive) > 0.004f)
        {
            props.set("mod_active", true);
            props.set("mod_min", nMin);
            props.set("mod_max", nMax);
            props.set("mod_live", nLive);
            s.repaint();
        }
    }
}
