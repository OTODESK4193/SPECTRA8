// ==========================================
// File: FactoryPresets.h
// SPECTRA8 - 音楽的かつ独創的な 80 個の FACTORY プリセットデータ
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <map>

struct FactoryPresetData
{
    juce::String name;
    juce::String category; // "FilterBank (Auto)", "LPC (Auto)", "FilterBank (MIDI)", "LPC (MIDI)"
    std::map<juce::String, float> params;
};

class FactoryPresets
{
public:
    static const std::vector<FactoryPresetData>& getPresets()
    {
        static const std::vector<FactoryPresetData> presets = createPresets();
        return presets;
    }

private:
    static std::vector<FactoryPresetData> createPresets()
    {
        std::vector<FactoryPresetData> list;
        list.reserve(80);

        auto addP = [&list](const juce::String& cat, const juce::String& nm, const std::map<juce::String, float>& p)
        {
            list.push_back({ nm, cat, p });
        };

        // ====================================================================
        // 1. FilterBank (Auto) - 20個 (自然な美しさ〜音楽的エフェクティブ)
        // ====================================================================
        addP("FilterBank (Auto)", "01. Standard Vocal Formant", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.5f }, { "tracking", 80.0f }, { "bandCount", 3.0f }, { "resonance", 1.0f }, { "stereoWidth", 0.6f }, { "mix", 100.0f }, { "outputLevel", 0.0f }
        });
        addP("FilterBank (Auto)", "02. Bright Pop Harmony", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.85f }, { "tracking", 100.0f }, { "bandCount", 4.0f }, { "formantShift", 2.0f }, { "resonance", 1.1f }, { "stereoWidth", 0.8f }, { "fx1Type", 4.0f }, { "fx1Amount", 0.35f }, { "choRate", 1.2f }, { "choDepth", 4.0f }, { "choMix", 0.4f }
        });
        addP("FilterBank (Auto)", "03. Deep Male Resonator", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.3f }, { "tracking", 70.0f }, { "formantShift", -5.0f }, { "resonance", 1.4f }, { "basePitch", 90.0f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.5f }, { "resMode", 0.0f }, { "resChord", 0.0f }, { "resDecay", 1.8f }
        });
        addP("FilterBank (Auto)", "04. Crystal Clean Bank", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.95f }, { "tracking", 90.0f }, { "bandCount", 5.0f }, { "resonance", 0.75f }, { "stereoWidth", 0.9f }, { "noiseColor", 4500.0f }, { "noise", 15.0f }
        });
        addP("FilterBank (Auto)", "05. Warm Analog Tube", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.4f }, { "tracking", 60.0f }, { "bandCount", 2.0f }, { "resonance", 1.6f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.45f }, { "drvDrive", 0.35f }, { "drvLow", 0.6f }
        });
        addP("FilterBank (Auto)", "06. Wide Stereo Ensemble", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.65f }, { "tracking", 85.0f }, { "bandCount", 4.0f }, { "stereoWidth", 1.0f }, { "fx1Type", 4.0f }, { "fx1Amount", 0.6f }, { "choRate", 0.8f }, { "choDepth", 8.0f }, { "choWidth", 1.0f }, { "choMix", 0.5f }
        });
        addP("FilterBank (Auto)", "07. Sub Bass Tracker", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.2f }, { "tracking", 100.0f }, { "formantShift", -12.0f }, { "basePitch", 65.0f }, { "resonance", 2.0f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.6f }, { "drvLow", 0.85f }
        });
        addP("FilterBank (Auto)", "08. Modulated Formant Sweep", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.7f }, { "tracking", 85.0f }, { "formantShift", 0.0f }, { "resonance", 1.3f }, { "slot0src", 1.0f }, { "slot0dst", 4.0f }, { "slot0amt", 0.6f }, { "lfo0rate", 0.8f }, { "lfo0wave", 0.0f }
        });
        addP("FilterBank (Auto)", "09. Telephone Radio Vox", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.5f }, { "tracking", 60.0f }, { "bandCount", 1.0f }, { "resonance", 2.2f }, { "lofi", 0.6f }, { "noiseColor", 1800.0f }, { "noise", 25.0f }
        });
        addP("FilterBank (Auto)", "10. Space Echo Vocoder", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.7f }, { "tracking", 90.0f }, { "formantShift", 2.0f }, { "fx1Type", 5.0f }, { "fx1Amount", 0.5f }, { "dlyTime", 0.375f }, { "dlyFeedback", 0.55f }, { "fx2Type", 6.0f }, { "fx2Amount", 0.4f }
        });
        addP("FilterBank (Auto)", "11. Saturated Beast", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.6f }, { "tracking", 80.0f }, { "resonance", 1.4f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.8f }, { "drvDrive", 0.75f }, { "drvMid", 0.85f }, { "drvHigh", 0.6f }
        });
        addP("FilterBank (Auto)", "12. Dynamic Reso Flutter", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.5f }, { "tracking", 90.0f }, { "resonance", 1.8f }, { "slot0src", 1.0f }, { "slot0dst", 10.0f }, { "slot0amt", 0.4f }, { "lfo0rate", 3.5f }
        });
        addP("FilterBank (Auto)", "13. Smooth Air Follower", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.85f }, { "tracking", 100.0f }, { "trackResponse", 2.0f }, { "bandCount", 5.0f }, { "resonance", 0.7f }, { "noiseColor", 6500.0f }, { "noise", 30.0f }
        });
        addP("FilterBank (Auto)", "14. Crispy Top Air", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.9f }, { "tracking", 85.0f }, { "formantShift", 4.0f }, { "noiseColor", 8500.0f }, { "noise", 35.0f }, { "stereoWidth", 0.95f }
        });
        addP("FilterBank (Auto)", "15. Ambient Cathedral Vox", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.6f }, { "tracking", 70.0f }, { "resonance", 1.2f }, { "fx1Type", 6.0f }, { "fx1Amount", 0.75f }, { "revSize", 0.9f }, { "revDecay", 5.0f }, { "revMix", 0.5f }
        });
        addP("FilterBank (Auto)", "16. Pitch Quantized Auto-Tune", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.8f }, { "tracking", 100.0f }, { "trackResponse", 0.0f }, { "pitchQuantize", 100.0f }, { "pitchQKey", 0.0f }, { "pitchQScale", 1.0f }
        });
        addP("FilterBank (Auto)", "17. Random Panning Bands", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.6f }, { "tracking", 75.0f }, { "stereoWidth", 1.0f }, { "slot0src", 1.0f }, { "slot0dst", 28.0f }, { "slot0amt", 0.7f }, { "lfo0wave", 4.0f }, { "lfo0rate", 2.0f }
        });
        addP("FilterBank (Auto)", "18. Overdriven Formant Lead", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.65f }, { "tracking", 90.0f }, { "formantShift", 3.0f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.65f }, { "drvDrive", 0.6f }, { "fx2Type", 5.0f }, { "fx2Amount", 0.35f }
        });
        addP("FilterBank (Auto)", "19. Dark Tube Sub", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.35f }, { "tracking", 65.0f }, { "formantShift", -6.0f }, { "basePitch", 85.0f }, { "resonance", 1.6f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.4f }
        });
        addP("FilterBank (Auto)", "20. Ethereal Reso Shimmer", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.6f }, { "tracking", 80.0f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.7f }, { "resMode", 1.0f }, { "resFreeMs", 18.0f }, { "resDecay", 2.8f }, { "resShimmer", 0.55f }
        });

        // ====================================================================
        // 2. LPC (Auto) - 20個 (レトロ〜ビットスピーク〜高精細)
        // ====================================================================
        addP("LPC (Auto)", "01. Vintage 80s Speech Chip", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 1.0f }, { "frameRate", 1.0f }, { "quantBits", 3.0f }, { "interpMode", 0.0f }, { "tracking", 80.0f }
        });
        addP("LPC (Auto)", "02. BitSpeek Style Talker", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 0.0f }, { "frameRate", 0.0f }, { "quantBits", 4.0f }, { "interpMode", 0.0f }, { "tracking", 70.0f }, { "formantShift", 2.0f }
        });
        addP("LPC (Auto)", "03. Order 16 Modern Clarity", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 3.0f }, { "frameRate", 4.0f }, { "quantBits", 0.0f }, { "interpMode", 1.0f }, { "tracking", 100.0f }
        });
        addP("LPC (Auto)", "04. 8-Bit Crushed Throat", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 0.0f }, { "frameRate", 2.0f }, { "quantBits", 4.0f }, { "lofi", 0.7f }, { "tracking", 60.0f }
        });
        addP("LPC (Auto)", "05. Glitch Step 8Hz", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 2.0f }, { "frameRate", 0.0f }, { "interpMode", 0.0f }, { "quantBits", 2.0f }, { "tracking", 90.0f }
        });
        addP("LPC (Auto)", "06. Frozen Vowel Ambient Drone", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 3.0f }, { "formantFreeze", 1.0f }, { "tracking", 0.0f }, { "basePitch", 110.0f }, { "fx1Type", 6.0f }, { "fx1Amount", 0.65f }, { "revDecay", 4.5f }
        });
        addP("LPC (Auto)", "07. Heavy Quantized Robot", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 1.0f }, { "frameRate", 3.0f }, { "pitchQuantize", 100.0f }, { "pitchQKey", 0.0f }, { "pitchQScale", 1.0f }, { "tracking", 100.0f }
        });
        addP("LPC (Auto)", "08. LSP Smooth Vocal Glide", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 3.0f }, { "interpMode", 1.0f }, { "frameRate", 4.0f }, { "quantBits", 0.0f }, { "tracking", 95.0f }
        });
        addP("LPC (Auto)", "09. LAR Tube Modeling", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 2.0f }, { "interpMode", 2.0f }, { "frameRate", 3.0f }, { "quantBits", 1.0f }, { "tracking", 85.0f }
        });
        addP("LPC (Auto)", "10. Low Rate Stutter Gate", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 1.0f }, { "frameRate", 0.0f }, { "interpMode", 0.0f }, { "fx1Type", 3.0f }, { "fx1Amount", 0.6f }, { "gateRate", 4.0f }, { "gatePattern", 1.0f }
        });
        addP("LPC (Auto)", "11. Crisp Order 12 Voice", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 2.0f }, { "frameRate", 4.0f }, { "quantBits", 0.0f }, { "interpMode", 1.0f }, { "formantShift", 1.0f }, { "tracking", 90.0f }
        });
        addP("LPC (Auto)", "12. Blackman Soft Speech", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 3.0f }, { "analysisWindow", 2.0f }, { "frameRate", 3.0f }, { "tracking", 80.0f }
        });
        addP("LPC (Auto)", "13. Hamming Sharp Formant", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 2.0f }, { "analysisWindow", 1.0f }, { "frameRate", 4.0f }, { "formantShift", 3.0f }, { "tracking", 85.0f }
        });
        addP("LPC (Auto)", "14. Crunchy 4-Bit Classic", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 1.0f }, { "frameRate", 2.0f }, { "quantBits", 3.0f }, { "interpMode", 0.0f }, { "tracking", 75.0f }
        });
        addP("LPC (Auto)", "15. Extreme 3-Bit Lo-Fi", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 0.0f }, { "frameRate", 1.0f }, { "quantBits", 4.0f }, { "lofi", 0.8f }, { "tracking", 50.0f }
        });
        addP("LPC (Auto)", "16. Whispering LPC Tube", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 3.0f }, { "tracking", 0.0f }, { "noiseColor", 5000.0f }, { "noise", 60.0f }, { "fx1Type", 6.0f }, { "fx1Amount", 0.5f }
        });
        addP("LPC (Auto)", "17. LFO Vibrato LPC", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 3.0f }, { "tracking", 90.0f }, { "slot0src", 1.0f }, { "slot0dst", 7.0f }, { "slot0amt", 0.2f }, { "lfo0rate", 5.5f }
        });
        addP("LPC (Auto)", "18. Alien Synthesizer Voice", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 3.0f }, { "formantShift", 12.0f }, { "formantStretch", 1.8f }, { "tracking", 90.0f }, { "fx1Type", 4.0f }, { "fx1Amount", 0.5f }
        });
        addP("LPC (Auto)", "19. Overdriven LPC Drive", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 2.0f }, { "frameRate", 3.0f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.7f }, { "drvDrive", 0.6f }
        });
        addP("LPC (Auto)", "20. Cybernetic Delay Echo", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 1.0f }, { "frameRate", 1.0f }, { "quantBits", 3.0f }, { "fx1Type", 5.0f }, { "fx1Amount", 0.5f }, { "dlyTime", 0.25f }, { "dlyFeedback", 0.6f }
        });

        // ====================================================================
        // 3. FilterBank (MIDI) - 20個 (鍵盤演奏用コード・シンセ・トランスゲート)
        // ====================================================================
        addP("FilterBank (MIDI)", "01. Classic MIDI Choir", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 15.0f }, { "bandCount", 4.0f }, { "resonance", 1.0f }, { "stereoWidth", 0.8f }, { "attack", 0.08f }, { "release", 0.5f }
        });
        addP("FilterBank (MIDI)", "02. Saw Synth Vocoder Chord", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 25.0f }, { "bandCount", 5.0f }, { "resonance", 1.2f }, { "stereoWidth", 0.9f }, { "attack", 0.01f }, { "sustain", 1.0f }
        });
        addP("FilterBank (MIDI)", "03. Pulse Width Morph Keys", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 1.0f }, { "pulseWidth", 30.0f }, { "bandCount", 4.0f }, { "slot0src", 1.0f }, { "slot0dst", 18.0f }, { "slot0amt", 0.5f }, { "lfo0rate", 1.0f }
        });
        addP("FilterBank (MIDI)", "04. Trance Gate Vocoder", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 30.0f }, { "fx1Type", 3.0f }, { "fx1Amount", 0.85f }, { "gateRate", 4.0f }, { "gatePattern", 5.0f }, { "gateDepth", 1.0f }
        });
        addP("FilterBank (MIDI)", "05. Shimmer Resonator Pad", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 20.0f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.6f }, { "resMode", 2.0f }, { "resDecay", 3.0f }, { "resShimmer", 0.5f }, { "fx2Type", 6.0f }, { "fx2Amount", 0.5f }
        });
        addP("FilterBank (MIDI)", "06. Detuned SuperSaw Vox", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 55.0f }, { "detuneMode", 1.0f }, { "bandCount", 5.0f }, { "resonance", 1.15f }, { "stereoWidth", 1.0f }
        });
        addP("FilterBank (MIDI)", "07. Funk Talking Bass", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 1.0f }, { "pulseWidth", 40.0f }, { "masterPitch", -12.0f }, { "resonance", 2.2f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.5f }
        });
        addP("FilterBank (MIDI)", "08. Sync Sweep Chords", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "syncAmt", 0.8f }, { "slot0src", 4.0f }, { "slot0dst", 23.0f }, { "slot0amt", 0.7f }, { "attack", 0.15f }, { "decay", 0.8f }
        });
        addP("FilterBank (MIDI)", "09. Plucked Vocoder Arp", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 10.0f }, { "attack", 0.001f }, { "decay", 0.25f }, { "sustain", 0.0f }, { "release", 0.2f }, { "fx1Type", 5.0f }, { "fx1Amount", 0.4f }, { "dlyTime", 0.375f }
        });
        addP("FilterBank (MIDI)", "10. Mod Wheel Drive Express", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 20.0f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.5f }, { "slot0src", 8.0f }, { "slot0dst", 29.0f }, { "slot0amt", 0.8f }
        });
        addP("FilterBank (MIDI)", "11. Octave Pitch Stacker", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "masterPitch", 12.0f }, { "detune", 20.0f }, { "stereoWidth", 1.0f }
        });
        addP("FilterBank (MIDI)", "12. Lush Stereo Ensemble", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 35.0f }, { "stereoWidth", 1.0f }, { "fx1Type", 4.0f }, { "fx1Amount", 0.7f }, { "choRate", 1.5f }, { "choDepth", 6.0f }, { "choWidth", 1.0f }
        });
        addP("FilterBank (MIDI)", "13. Retro 80s Vocal Lead", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "porta", 0.08f }, { "detune", 12.0f }, { "formantShift", 3.0f }, { "resonance", 1.5f }
        });
        addP("FilterBank (MIDI)", "14. ModWheel Formant Shift", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "slot0src", 8.0f }, { "slot0dst", 4.0f }, { "slot0amt", 1.0f }
        });
        addP("FilterBank (MIDI)", "15. Fast Attack Staccato", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "attack", 0.001f }, { "decay", 0.15f }, { "sustain", 0.1f }, { "release", 0.1f }
        });
        addP("FilterBank (MIDI)", "16. Sub Harmonics Layer", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "masterPitch", -12.0f }, { "formantShift", -4.0f }, { "resonance", 1.8f }
        });
        addP("FilterBank (MIDI)", "17. Cosmic Delay Sweeps", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "fx1Type", 5.0f }, { "fx1Amount", 0.6f }, { "dlyTime", 0.5f }, { "dlyFeedback", 0.7f }, { "slot0src", 1.0f }, { "slot0dst", 4.0f }, { "slot0amt", 0.5f }
        });
        addP("FilterBank (MIDI)", "18. Velocity Sensitive Filter", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "slot0src", 6.0f }, { "slot0dst", 10.0f }, { "slot0amt", 0.6f }, { "slot1src", 6.0f }, { "slot1dst", 4.0f }, { "slot1amt", 0.4f }
        });
        addP("FilterBank (MIDI)", "19. S&H Modulated Formants", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "slot0src", 1.0f }, { "slot0dst", 4.0f }, { "slot0amt", 0.8f }, { "lfo0wave", 4.0f }, { "lfo0rate", 4.0f }
        });
        addP("FilterBank (MIDI)", "20. Hyper Drive Vox Lead", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 20.0f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.8f }, { "drvDrive", 0.8f }, { "drvHigh", 0.7f }
        });

        // ====================================================================
        // 4. LPC (MIDI) - 20個 (トークボックス・鍵盤歌唱・サイバーシンセ)
        // ====================================================================
        addP("LPC (MIDI)", "01. LPC Cyber Poly Synth", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "frameRate", 4.0f }, { "quantBits", 0.0f }, { "waveform", 0.0f }, { "detune", 20.0f }, { "attack", 0.02f }, { "release", 0.4f }
        });
        addP("LPC (MIDI)", "02. Bit-Quantized TalkBox", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 1.0f }, { "frameRate", 2.0f }, { "quantBits", 3.0f }, { "waveform", 0.0f }, { "detune", 15.0f }, { "interpMode", 0.0f }
        });
        addP("LPC (MIDI)", "03. Order 16 HD Lead Vox", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "frameRate", 4.0f }, { "interpMode", 1.0f }, { "porta", 0.06f }, { "formantShift", 2.0f }
        });
        addP("LPC (MIDI)", "04. 8-Bit Arcade Vocoder", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 0.0f }, { "frameRate", 1.0f }, { "quantBits", 4.0f }, { "lofi", 0.8f }, { "waveform", 1.0f }
        });
        addP("LPC (MIDI)", "05. Trance Gated LPC Voice", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 2.0f }, { "frameRate", 3.0f }, { "fx1Type", 3.0f }, { "fx1Amount", 0.8f }, { "gateRate", 4.0f }, { "gatePattern", 5.0f }
        });
        addP("LPC (MIDI)", "06. Frozen Vowel Keyboard", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "formantFreeze", 1.0f }, { "waveform", 0.0f }, { "detune", 25.0f }, { "attack", 0.05f }, { "release", 0.6f }
        });
        addP("LPC (MIDI)", "07. Shimmering Vocal Tube", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.6f }, { "resMode", 2.0f }, { "resDecay", 2.5f }, { "resShimmer", 0.6f }, { "fx2Type", 6.0f }, { "fx2Amount", 0.5f }
        });
        addP("LPC (MIDI)", "08. LSP Smooth Poly Pad", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "interpMode", 1.0f }, { "frameRate", 4.0f }, { "attack", 0.3f }, { "release", 0.8f }
        });
        addP("LPC (MIDI)", "09. Rhythmic Stutter Vox", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 1.0f }, { "frameRate", 0.0f }, { "interpMode", 0.0f }, { "fx1Type", 3.0f }, { "fx1Amount", 0.7f }, { "gateRate", 5.0f }, { "gatePattern", 6.0f }
        });
        addP("LPC (MIDI)", "10. Lo-Fi Speech Synth Key", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 1.0f }, { "frameRate", 2.0f }, { "quantBits", 3.0f }, { "lofi", 0.6f }, { "waveform", 0.0f }
        });
        addP("LPC (MIDI)", "11. Overdriven LPC Lead", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 2.0f }, { "frameRate", 3.0f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.8f }, { "drvDrive", 0.7f }, { "porta", 0.05f }
        });
        addP("LPC (MIDI)", "12. Sub Bass LPC Synthesizer", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 2.0f }, { "masterPitch", -12.0f }, { "waveform", 0.0f }, { "detune", 10.0f }
        });
        addP("LPC (MIDI)", "13. Chorus Ensemble LPC", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "frameRate", 4.0f }, { "fx1Type", 4.0f }, { "fx1Amount", 0.6f }, { "choRate", 1.5f }, { "choDepth", 5.0f }
        });
        addP("LPC (MIDI)", "14. Space Echo LPC Drift", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 2.0f }, { "fx1Type", 5.0f }, { "fx1Amount", 0.6f }, { "dlyTime", 0.4f }, { "dlyFeedback", 0.6f }
        });
        addP("LPC (MIDI)", "15. ModWheel Reso Shimmer", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.6f }, { "resMode", 2.0f }, { "slot0src", 8.0f }, { "slot0dst", 36.0f }, { "slot0amt", 0.8f }
        });
        addP("LPC (MIDI)", "16. Extreme 3-Bit Talker", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 0.0f }, { "frameRate", 1.0f }, { "quantBits", 4.0f }, { "waveform", 0.0f }
        });
        addP("LPC (MIDI)", "17. Resonator Tuned LPC", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 2.0f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.7f }, { "resMode", 2.0f }, { "resDecay", 2.0f }
        });
        addP("LPC (MIDI)", "18. Formant Stretch Chord", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "formantShift", 3.0f }, { "formantStretch", 1.5f }, { "waveform", 0.0f }, { "detune", 20.0f }
        });
        addP("LPC (MIDI)", "19. Mod Wheel Morphing LPC", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "slot0src", 8.0f }, { "slot0dst", 4.0f }, { "slot0amt", 1.0f }
        });
        addP("LPC (MIDI)", "20. Futuristic AI Vocal Synth", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "frameRate", 4.0f }, { "interpMode", 1.0f }, { "quantBits", 0.0f }, { "fx1Type", 4.0f }, { "fx1Amount", 0.4f }, { "fx2Type", 6.0f }, { "fx2Amount", 0.4f }
        });

        return list;
    }
};
