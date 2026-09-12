// ==========================================
// File: FactoryPresets.h
// SPECTRA8 - 音楽的かつ独創的な 110 個の FACTORY プリセットデータ
// ==========================================
#pragma once

#include <JuceHeader.h>
#include <vector>
#include <map>

struct FactoryPresetData
{
    juce::String name;
    juce::String category;
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
        list.reserve(110);

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
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.85f }, { "tracking", 100.0f }, { "bandCount", 4.0f }, { "formantShift", 2.0f }, { "resonance", 1.1f }, { "stereoWidth", 0.8f }, { "fx1Type", 4.0f }, { "fx1Amount", 0.35f }, { "choRate", 1.2f }, { "choDepth", 4.0f }
        });
        addP("FilterBank (Auto)", "03. Deep Male Resonator", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.3f }, { "tracking", 70.0f }, { "formantShift", -5.0f }, { "resonance", 1.4f }, { "basePitch", 90.0f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.5f }, { "resDecay", 1.8f }, { "resDamp", 40.0f }
        });
        addP("FilterBank (Auto)", "04. Crystal Clean Bank", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.95f }, { "tracking", 90.0f }, { "bandCount", 5.0f }, { "resonance", 0.75f }, { "stereoWidth", 0.9f }, { "noiseColor", 4500.0f }, { "noise", 15.0f }
        });
        addP("FilterBank (Auto)", "05. Warm Analog Tube", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.4f }, { "tracking", 60.0f }, { "bandCount", 2.0f }, { "resonance", 1.6f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.45f }, { "drvDrive", 0.35f }, { "drvLow", 0.6f }
        });
        addP("FilterBank (Auto)", "06. Wide Stereo Ensemble", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.65f }, { "tracking", 85.0f }, { "bandCount", 4.0f }, { "stereoWidth", 1.0f }, { "fx1Type", 4.0f }, { "fx1Amount", 0.6f }, { "choRate", 0.8f }, { "choDepth", 8.0f }, { "choWidth", 1.0f }
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
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.7f }, { "tracking", 90.0f }, { "formantShift", 2.0f }, { "fx1Type", 4.0f }, { "fx1Amount", 0.45f }, { "choRate", 0.6f }, { "choDepth", 6.0f }, { "fx2Type", 5.0f }, { "fx2Amount", 0.45f }, { "revSize", 0.75f }
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
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.6f }, { "tracking", 70.0f }, { "resonance", 1.2f }, { "fx1Type", 5.0f }, { "fx1Amount", 0.75f }, { "revSize", 0.9f }, { "revDamp", 50.0f }
        });
        addP("FilterBank (Auto)", "16. Pitch Quantized Auto-Tune", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.8f }, { "tracking", 100.0f }, { "trackResponse", 0.0f }, { "pitchQuantize", 100.0f }, { "pitchQKey", 0.0f }, { "pitchQScale", 1.0f }
        });
        addP("FilterBank (Auto)", "17. Random Panning Bands", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.6f }, { "tracking", 75.0f }, { "stereoWidth", 1.0f }, { "slot0src", 1.0f }, { "slot0dst", 28.0f }, { "slot0amt", 0.7f }, { "lfo0wave", 4.0f }, { "lfo0rate", 2.0f }
        });
        addP("FilterBank (Auto)", "18. Overdriven Formant Lead", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.65f }, { "tracking", 90.0f }, { "formantShift", 3.0f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.65f }, { "drvDrive", 0.6f }, { "fx2Type", 5.0f }, { "fx2Amount", 0.35f }, { "revSize", 0.6f }
        });
        addP("FilterBank (Auto)", "19. Dark Tube Sub", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.35f }, { "tracking", 65.0f }, { "formantShift", -6.0f }, { "basePitch", 85.0f }, { "resonance", 1.6f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.4f }
        });
        addP("FilterBank (Auto)", "20. Ethereal Reso Shimmer", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.6f }, { "tracking", 80.0f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.7f }, { "resDecay", 2.8f }, { "resShimmer", 55.0f }
        });

        // ====================================================================
        // 2. LPC (Auto) - 20個 (レトロ〜ビットスピーク〜高精細)
        // ====================================================================
        addP("LPC (Auto)", "01. Vintage 80s Speech Chip", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 1.0f }, { "frameRate", 1.0f }, { "lpcQuantBits", 3.0f }, { "interpolationMode", 0.0f }, { "tracking", 80.0f }
        });
        addP("LPC (Auto)", "02. BitSpeek Style Talker", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 0.0f }, { "frameRate", 0.0f }, { "lpcQuantBits", 4.0f }, { "interpolationMode", 0.0f }, { "tracking", 70.0f }, { "formantShift", 2.0f }
        });
        addP("LPC (Auto)", "03. Order 16 Modern Clarity", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 3.0f }, { "frameRate", 4.0f }, { "lpcQuantBits", 0.0f }, { "interpolationMode", 1.0f }, { "tracking", 100.0f }
        });
        addP("LPC (Auto)", "04. 8-Bit Crushed Throat", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 0.0f }, { "frameRate", 2.0f }, { "lpcQuantBits", 4.0f }, { "lofi", 0.7f }, { "tracking", 60.0f }
        });
        addP("LPC (Auto)", "05. Glitch Step 8Hz", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 2.0f }, { "frameRate", 0.0f }, { "interpolationMode", 0.0f }, { "lpcQuantBits", 2.0f }, { "tracking", 90.0f }
        });
        addP("LPC (Auto)", "06. Frozen Vowel Ambient Drone", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 3.0f }, { "formantFreeze", 1.0f }, { "tracking", 0.0f }, { "basePitch", 110.0f }, { "fx1Type", 5.0f }, { "fx1Amount", 0.65f }, { "revSize", 0.9f }
        });
        addP("LPC (Auto)", "07. Heavy Quantized Robot", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 1.0f }, { "frameRate", 3.0f }, { "pitchQuantize", 100.0f }, { "pitchQKey", 0.0f }, { "pitchQScale", 1.0f }, { "tracking", 100.0f }
        });
        addP("LPC (Auto)", "08. LSP Smooth Vocal Glide", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 3.0f }, { "interpolationMode", 1.0f }, { "frameRate", 4.0f }, { "lpcQuantBits", 0.0f }, { "tracking", 95.0f }
        });
        addP("LPC (Auto)", "09. LAR Tube Modeling", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 2.0f }, { "interpolationMode", 2.0f }, { "frameRate", 3.0f }, { "lpcQuantBits", 1.0f }, { "tracking", 85.0f }
        });
        addP("LPC (Auto)", "10. Low Rate Stutter Gate", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 1.0f }, { "frameRate", 0.0f }, { "interpolationMode", 0.0f }, { "fx1Type", 3.0f }, { "fx1Amount", 0.6f }, { "gateRate", 4.0f }, { "gatePattern", 1.0f }
        });
        addP("LPC (Auto)", "11. Crisp Order 12 Voice", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 2.0f }, { "frameRate", 4.0f }, { "lpcQuantBits", 0.0f }, { "interpolationMode", 1.0f }, { "formantShift", 1.0f }, { "tracking", 90.0f }
        });
        addP("LPC (Auto)", "12. Blackman Soft Speech", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 3.0f }, { "windowType", 2.0f }, { "frameRate", 3.0f }, { "tracking", 80.0f }
        });
        addP("LPC (Auto)", "13. Hamming Sharp Formant", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 2.0f }, { "windowType", 1.0f }, { "frameRate", 4.0f }, { "formantShift", 3.0f }, { "tracking", 85.0f }
        });
        addP("LPC (Auto)", "14. Crunchy 4-Bit Classic", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 1.0f }, { "frameRate", 2.0f }, { "lpcQuantBits", 3.0f }, { "interpolationMode", 0.0f }, { "tracking", 75.0f }
        });
        addP("LPC (Auto)", "15. Extreme 3-Bit Lo-Fi", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 0.0f }, { "frameRate", 1.0f }, { "lpcQuantBits", 4.0f }, { "lofi", 0.8f }, { "tracking", 50.0f }
        });
        addP("LPC (Auto)", "16. Whispering LPC Tube", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 3.0f }, { "tracking", 0.0f }, { "noiseColor", 5000.0f }, { "noise", 60.0f }, { "fx1Type", 5.0f }, { "fx1Amount", 0.5f }, { "revSize", 0.8f }
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
        addP("LPC (Auto)", "20. Cybernetic Echo Drift", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 1.0f }, { "frameRate", 1.0f }, { "lpcQuantBits", 3.0f }, { "fx1Type", 5.0f }, { "fx1Amount", 0.6f }, { "revSize", 0.85f }, { "revPredelay", 80.0f }
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
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 30.0f }, { "fx1Type", 3.0f }, { "fx1Amount", 0.85f }, { "gateRate", 4.0f }, { "gatePattern", 29.0f }, { "gateDepth", 100.0f }
        });
        addP("FilterBank (MIDI)", "05. Shimmer Resonator Pad", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 20.0f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.6f }, { "resDecay", 3.0f }, { "resShimmer", 50.0f }, { "fx2Type", 5.0f }, { "fx2Amount", 0.5f }, { "revSize", 0.8f }
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
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 10.0f }, { "attack", 0.001f }, { "decay", 0.25f }, { "sustain", 0.0f }, { "release", 0.2f }, { "fx1Type", 5.0f }, { "fx1Amount", 0.4f }, { "revSize", 0.6f }
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
        addP("FilterBank (MIDI)", "17. Cosmic Reverb Sweeps", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "fx1Type", 5.0f }, { "fx1Amount", 0.6f }, { "revSize", 0.85f }, { "revDamp", 30.0f }, { "slot0src", 1.0f }, { "slot0dst", 4.0f }, { "slot0amt", 0.5f }
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
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "frameRate", 4.0f }, { "lpcQuantBits", 0.0f }, { "waveform", 0.0f }, { "detune", 20.0f }, { "attack", 0.02f }, { "release", 0.4f }
        });
        addP("LPC (MIDI)", "02. Bit-Quantized TalkBox", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 1.0f }, { "frameRate", 2.0f }, { "lpcQuantBits", 3.0f }, { "waveform", 0.0f }, { "detune", 15.0f }, { "interpolationMode", 0.0f }
        });
        addP("LPC (MIDI)", "03. Order 16 HD Lead Vox", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "frameRate", 4.0f }, { "interpolationMode", 1.0f }, { "porta", 0.06f }, { "formantShift", 2.0f }
        });
        addP("LPC (MIDI)", "04. 8-Bit Arcade Vocoder", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 0.0f }, { "frameRate", 1.0f }, { "lpcQuantBits", 4.0f }, { "lofi", 0.8f }, { "waveform", 1.0f }
        });
        addP("LPC (MIDI)", "05. Trance Gated LPC Voice", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 2.0f }, { "frameRate", 3.0f }, { "fx1Type", 3.0f }, { "fx1Amount", 0.8f }, { "gateRate", 4.0f }, { "gatePattern", 29.0f }
        });
        addP("LPC (MIDI)", "06. Frozen Vowel Keyboard", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "formantFreeze", 1.0f }, { "waveform", 0.0f }, { "detune", 25.0f }, { "attack", 0.05f }, { "release", 0.6f }
        });
        addP("LPC (MIDI)", "07. Shimmering Vocal Tube", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.6f }, { "resDecay", 2.5f }, { "resShimmer", 60.0f }, { "fx2Type", 5.0f }, { "fx2Amount", 0.5f }, { "revSize", 0.8f }
        });
        addP("LPC (MIDI)", "08. LSP Smooth Poly Pad", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "interpolationMode", 1.0f }, { "frameRate", 4.0f }, { "attack", 0.3f }, { "release", 0.8f }
        });
        addP("LPC (MIDI)", "09. Rhythmic Stutter Vox", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 1.0f }, { "frameRate", 0.0f }, { "interpolationMode", 0.0f }, { "fx1Type", 3.0f }, { "fx1Amount", 0.7f }, { "gateRate", 5.0f }, { "gatePattern", 6.0f }
        });
        addP("LPC (MIDI)", "10. Lo-Fi Speech Synth Key", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 1.0f }, { "frameRate", 2.0f }, { "lpcQuantBits", 3.0f }, { "lofi", 0.6f }, { "waveform", 0.0f }
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
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 2.0f }, { "fx1Type", 5.0f }, { "fx1Amount", 0.6f }, { "revSize", 0.8f }, { "revPredelay", 60.0f }
        });
        addP("LPC (MIDI)", "15. ModWheel Reso Shimmer", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.6f }, { "slot0src", 8.0f }, { "slot0dst", 36.0f }, { "slot0amt", 0.8f }
        });
        addP("LPC (MIDI)", "16. Extreme 3-Bit Talker", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 0.0f }, { "frameRate", 1.0f }, { "lpcQuantBits", 4.0f }, { "waveform", 0.0f }
        });
        addP("LPC (MIDI)", "17. Resonator Tuned LPC", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 2.0f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.7f }, { "resDecay", 2.0f }
        });
        addP("LPC (MIDI)", "18. Formant Stretch Chord", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "formantShift", 3.0f }, { "formantStretch", 1.5f }, { "waveform", 0.0f }, { "detune", 20.0f }
        });
        addP("LPC (MIDI)", "19. Mod Wheel Morphing LPC", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "slot0src", 8.0f }, { "slot0dst", 4.0f }, { "slot0amt", 1.0f }
        });
        addP("LPC (MIDI)", "20. Futuristic AI Vocal Synth", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "frameRate", 4.0f }, { "interpolationMode", 1.0f }, { "lpcQuantBits", 0.0f }, { "fx1Type", 4.0f }, { "fx1Amount", 0.4f }, { "fx2Type", 5.0f }, { "fx2Amount", 0.4f }, { "revSize", 0.7f }
        });

        // ====================================================================
        // 5. M.Pitch Modulations - 10個 (S&H・テンポ同期・スケールスナップ移調)
        // ====================================================================
        // 01. Slow Drift Cassette: 極低速S&Hによるアナログテープのワウ・フラッター
        addP("M.Pitch Modulations", "01. Slow Drift Cassette", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.6f }, { "tracking", 85.0f },
            { "slot0src", 1.0f }, { "slot0dst", 27.0f }, { "slot0amt", 0.08f }, // LFO1 -> M.Pitch (±1.9st微細揺らぎ)
            { "lfo0wave", 4.0f }, { "lfo0rate", 0.25f },                         // S&H, 0.25Hz
            { "lofi", 0.35f }, { "noiseColor", 3500.0f }, { "noise", 10.0f },
            { "fx1Type", 4.0f }, { "fx1Amount", 0.4f }, { "choRate", 0.5f }, { "choDepth", 3.0f }
        });

        // 02. Tempo Synced Arp Walk: 1/8T同期S&H + Pentatonicスナップによる階段状アルペジオ
        addP("M.Pitch Modulations", "02. Tempo Synced Arp Walk", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.8f }, { "tracking", 90.0f },
            { "pitchQuantize", 100.0f }, { "pitchQKey", 0.0f }, { "pitchQScale", 4.0f }, // Major Pentatonic
            { "slot0src", 1.0f }, { "slot0dst", 27.0f }, { "slot0amt", 0.5f },          // LFO1 -> M.Pitch (±12st)
            { "lfo0sync", 1.0f }, { "lfo0rateSync", 2.0f }, { "lfo0wave", 4.0f },        // Sync 1/2T, S&H
            { "resonance", 1.3f }, { "stereoWidth", 0.85f }
        });

        // 03. Cosmic Starchild 1/4: 1/4 Sync Sawによる1オクターブ上昇リピート
        addP("M.Pitch Modulations", "03. Cosmic Starchild 1/4", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 20.0f },
            { "slot0src", 1.0f }, { "slot0dst", 27.0f }, { "slot0amt", 0.5f }, { "slot0uni", 1.0f }, // 0..+12st
            { "lfo0sync", 1.0f }, { "lfo0rateSync", 3.0f }, { "lfo0wave", 2.0f },                     // Sync 1/4, Saw
            { "fx1Type", 5.0f }, { "fx1Amount", 0.65f }, { "revSize", 0.85f }, { "revDamp", 40.0f }
        });

        // 04. Ambient Floating Octaves: Sine 0.15Hz によるゆったりとしたオクターブ揺らぎ
        addP("M.Pitch Modulations", "04. Ambient Floating Octaves", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.5f }, { "tracking", 80.0f },
            { "slot0src", 1.0f }, { "slot0dst", 27.0f }, { "slot0amt", 0.5f }, // ±12st
            { "lfo0wave", 0.0f }, { "lfo0rate", 0.15f },                        // Sine, 0.15Hz
            { "resonance", 1.4f }, { "stereoWidth", 1.0f },
            { "fx1Type", 5.0f }, { "fx1Amount", 0.7f }, { "revSize", 0.95f }, { "revPredelay", 70.0f }
        });

        // 05. 16th Stepped Chiptune: 1/16 Sync S&H + Major Scale によるキラキラ高速メロディ
        addP("M.Pitch Modulations", "05. 16th Stepped Chiptune", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 1.0f }, { "lpcQuantBits", 3.0f },
            { "pitchQuantize", 100.0f }, { "pitchQKey", 0.0f }, { "pitchQScale", 1.0f }, // Major Scale
            { "slot0src", 1.0f }, { "slot0dst", 27.0f }, { "slot0amt", 0.5f },          // ±12st
            { "lfo0sync", 1.0f }, { "lfo0rateSync", 9.0f }, { "lfo0wave", 4.0f },        // Sync 1/16, S&H
            { "tracking", 95.0f }
        });

        // 06. Sub Octave Pitch Drop: 打鍵時に-24半音へダイブするヘヴィサブ
        addP("M.Pitch Modulations", "06. Sub Octave Pitch Drop", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 1.0f }, { "pulseWidth", 45.0f },
            { "slot0src", 4.0f }, { "slot0dst", 27.0f }, { "slot0amt", -1.0f }, { "slot0uni", 1.0f }, // ENV1 -> -24st
            { "env0attack", 0.005f }, { "env0decay", 0.35f }, { "env0sustain", 0.0f },
            { "fx1Type", 2.0f }, { "fx1Amount", 0.7f }, { "drvDrive", 0.8f }, { "drvLow", 0.9f }
        });

        // 07. ModWheel Pitch Bend Lead: モジュレーションホイール(CC1)で+12stベンド
        addP("M.Pitch Modulations", "07. ModWheel Pitch Bend Lead", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 20.0f },
            { "slot0src", 8.0f }, { "slot0dst", 27.0f }, { "slot0amt", 0.5f }, { "slot0uni", 1.0f }, // ModWheel -> +12st
            { "porta", 0.05f }, { "resonance", 1.3f },
            { "fx1Type", 4.0f }, { "fx1Amount", 0.5f }, { "choRate", 1.2f }, { "choDepth", 4.0f }
        });

        // 08. Chaos Harmonic Stepper: Chaos LFO による有機的・非周期的な移調
        addP("M.Pitch Modulations", "08. Chaos Harmonic Stepper", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.7f }, { "tracking", 85.0f },
            { "pitchQuantize", 100.0f }, { "pitchQKey", 9.0f }, { "pitchQScale", 3.0f }, // A Minor Pentatonic
            { "slot0src", 1.0f }, { "slot0dst", 27.0f }, { "slot0amt", 0.5f },          // ±12st
            { "lfo0wave", 5.0f }, { "lfo0rate", 1.2f },                                 // Chaos
            { "stereoWidth", 0.9f },
            { "fx1Type", 5.0f }, { "fx1Amount", 0.5f }, { "revSize", 0.8f }
        });

        // 09. Detuned Dual Vibrato: 5.2Hz ビブラートとデチューンの融合
        addP("M.Pitch Modulations", "09. Detuned Dual Vibrato", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 30.0f },
            { "slot0src", 1.0f }, { "slot0dst", 27.0f }, { "slot0amt", 0.04f }, // ±1st ビブラート
            { "lfo0wave", 0.0f }, { "lfo0rate", 5.2f },                         // Sine 5.2Hz
            { "resonance", 1.2f }, { "attack", 0.05f }, { "release", 0.4f },
            { "fx1Type", 4.0f }, { "fx1Amount", 0.6f }, { "choWidth", 1.0f }
        });

        // 10. Formant & Pitch Cross: ピッチ上昇とフォルマント下降の交差モーフ
        addP("M.Pitch Modulations", "10. Formant & Pitch Cross", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.75f }, { "tracking", 85.0f },
            { "slot0src", 1.0f }, { "slot0dst", 27.0f }, { "slot0amt", 0.4f }, // LFO1 -> M.Pitch 上昇
            { "slot1src", 1.0f }, { "slot1dst", 4.0f },  { "slot1amt", -0.5f }, // LFO1 -> Formant Shift 下降
            { "lfo0wave", 1.0f }, { "lfo0rate", 0.4f },                         // Triangle 0.4Hz
            { "resonance", 1.5f }, { "stereoWidth", 0.9f }
        });

        // ====================================================================
        // 6. Rhythmic Formant Gate - 10個 (50パターン・母音変調・トランジェント)
        // ====================================================================
        // 01. Straight 16th Trance Gate: 王道トランスビートゲート
        addP("Rhythmic Formant Gate", "01. Straight 16th Trance Gate", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 35.0f },
            { "fx1Type", 3.0f }, { "fx1Amount", 0.9f }, // Gate
            { "gateRate", 4.0f }, { "gatePattern", 0.0f }, { "gateDepth", 100.0f }, { "gateDecay", 25.0f },
            { "fx2Type", 5.0f }, { "fx2Amount", 0.45f }, { "revSize", 0.75f }
        });

        // 02. Triplet Glitch Bounce: 3連符跳躍と母音モジュレーション
        addP("Rhythmic Formant Gate", "02. Triplet Glitch Bounce", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 25.0f },
            { "fx1Type", 3.0f }, { "fx1Amount", 0.85f },
            { "gateRate", 5.0f }, { "gatePattern", 12.0f }, { "gateDepth", 95.0f }, { "gateVowel", 65.0f },
            { "fx2Type", 4.0f }, { "fx2Amount", 0.5f }, { "choRate", 1.2f }
        });

        // 03. ColorBass Step Pluck: 超鋭角ディケイ(12ms) + Resonator
        addP("Rhythmic Formant Gate", "03. ColorBass Step Pluck", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 20.0f },
            { "fx1Type", 3.0f }, { "fx1Amount", 0.95f },
            { "gateRate", 4.0f }, { "gatePattern", 26.0f }, { "gateDepth", 100.0f }, { "gateDecay", 12.0f },
            { "fx2Type", 1.0f }, { "fx2Amount", 0.7f }, { "resDecay", 0.8f }, { "resShimmer", 40.0f }
        });

        // 04. Dotted Eighth Poly Groove: 付点8分ポリリズムビート
        addP("Rhythmic Formant Gate", "04. Dotted Eighth Poly Groove", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.7f }, { "tracking", 90.0f },
            { "fx1Type", 3.0f }, { "fx1Amount", 0.85f },
            { "gateRate", 3.0f }, { "gatePattern", 18.0f }, { "gateDepth", 90.0f }, { "gateDecay", 40.0f }
        });

        // 05. Vowel Morph Rhythm Talk: 母音(A-I-U-E-O)が激しく切り替わるトーキングゲート
        addP("Rhythmic Formant Gate", "05. Vowel Morph Rhythm Talk", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.8f }, { "tracking", 85.0f },
            { "fx1Type", 3.0f }, { "fx1Amount", 0.9f },
            { "gateRate", 4.0f }, { "gatePattern", 29.0f }, { "gateDepth", 80.0f }, { "gateVowel", 90.0f }
        });

        // 06. Micro Glitch Stutter 1/32: 1/32音符の超高速IDMスタッター
        addP("Rhythmic Formant Gate", "06. Micro Glitch Stutter 1/32", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 2.0f }, { "waveform", 0.0f },
            { "fx1Type", 3.0f }, { "fx1Amount", 0.85f },
            { "gateRate", 7.0f }, { "gatePattern", 42.0f }, { "gateDepth", 100.0f }, { "gateDecay", 15.0f }
        });

        // 07. Half-Time Swung Chords: ネオソウル・フューチャーベース風スイング
        addP("Rhythmic Formant Gate", "07. Half-Time Swung Chords", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 18.0f },
            { "fx1Type", 3.0f }, { "fx1Amount", 0.8f },
            { "gateRate", 4.0f }, { "gatePattern", 10.0f }, { "gateDepth", 85.0f }, { "gateDecay", 60.0f },
            { "fx2Type", 5.0f }, { "fx2Amount", 0.4f }, { "revSize", 0.7f }
        });

        // 08. Complex Polyrhythm Vox: 4 over 3 ポリリズム
        addP("Rhythmic Formant Gate", "08. Complex Polyrhythm Vox", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.75f }, { "tracking", 95.0f },
            { "fx1Type", 3.0f }, { "fx1Amount", 0.85f },
            { "gateRate", 4.0f }, { "gatePattern", 22.0f }, { "gateDepth", 90.0f }, { "gateVowel", 45.0f }
        });

        // 09. Dynamic Gate Decay Mod: ENV1でゲートの余韻長を変調
        addP("Rhythmic Formant Gate", "09. Dynamic Gate Decay Mod", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 25.0f },
            { "fx1Type", 3.0f }, { "fx1Amount", 0.9f },
            { "gateRate", 4.0f }, { "gatePattern", 0.0f }, { "gateDepth", 100.0f }, { "gateDecay", 20.0f },
            { "slot0src", 4.0f }, { "slot0dst", 53.0f }, { "slot0amt", 0.7f }, // ENV1 -> Gate Decay
            { "env0attack", 0.01f }, { "env0decay", 0.6f }, { "env0sustain", 0.2f }
        });

        // 10. Industrial Strobe Drive: カオスパターン + ディストーション直列
        addP("Rhythmic Formant Gate", "10. Industrial Strobe Drive", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 1.0f }, { "lpcQuantBits", 3.0f },
            { "fx1Type", 3.0f }, { "fx1Amount", 0.9f },
            { "gateRate", 6.0f }, { "gatePattern", 48.0f }, { "gateDepth", 100.0f },
            { "fx2Type", 2.0f }, { "fx2Amount", 0.85f }, { "drvDrive", 0.8f }, { "drvHigh", 0.75f }
        });

        // ====================================================================
        // 7. Spectral Resonator Lab - 10個 (Shift変調・金属倍音・シマー拡散)
        // ====================================================================
        // 01. Reso Shift Octave Glitch: S&HでResonator Shift(半音)をオクターブ跳躍
        addP("Spectral Resonator Lab", "01. Reso Shift Octave Glitch", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 20.0f },
            { "fx1Type", 1.0f }, { "fx1Amount", 0.8f }, // Resonator
            { "resDecay", 1.2f }, { "resDamp", 30.0f }, { "resSpread", 85.0f },
            { "slot0src", 1.0f }, { "slot0dst", 54.0f }, { "slot0amt", 0.5f }, // LFO1 -> resShift (±12st)
            { "lfo0sync", 1.0f }, { "lfo0rateSync", 6.0f }, { "lfo0wave", 4.0f } // Sync 1/8, S&H
        });

        // 02. Metallic Inharmonic Drone: 非調和倍音85%によるインダストリアル金属共振
        addP("Spectral Resonator Lab", "02. Metallic Inharmonic Drone", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.4f }, { "tracking", 70.0f },
            { "fx1Type", 1.0f }, { "fx1Amount", 0.75f },
            { "resDecay", 2.4f }, { "resInharm", 85.0f }, { "resDamp", 20.0f }, { "resSpread", 70.0f },
            { "fx2Type", 2.0f }, { "fx2Amount", 0.35f }, { "drvDrive", 0.4f }
        });

        // 03. ColorBass Pitch Snap: 押鍵ノートに共鳴がジャスト吸着するモダンColorBass
        addP("Spectral Resonator Lab", "03. ColorBass Pitch Snap", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 15.0f },
            { "fx1Type", 1.0f }, { "fx1Amount", 0.85f },
            { "resDecay", 0.75f }, { "resShimmer", 40.0f }, { "resSpread", 95.0f }, { "resOutGain", 2.0f },
            { "attack", 0.005f }, { "decay", 0.4f }, { "sustain", 0.2f }
        });

        // 04. Laser Pitch Sweep: Sine LFOによるShiftノブの±12stダイナミックスウィープ
        addP("Spectral Resonator Lab", "04. Laser Pitch Sweep", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.65f }, { "tracking", 85.0f },
            { "fx1Type", 1.0f }, { "fx1Amount", 0.8f },
            { "resDecay", 1.6f }, { "resDamp", 35.0f },
            { "slot0src", 1.0f }, { "slot0dst", 54.0f }, { "slot0amt", 0.5f }, // LFO1 -> resShift
            { "lfo0wave", 0.0f }, { "lfo0rate", 0.25f }                         // Sine 0.25Hz
        });

        // 05. Shimmer Diffusion Dream: 85%オールパス拡散による持続残響雲
        addP("Spectral Resonator Lab", "05. Shimmer Diffusion Dream", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 20.0f },
            { "fx1Type", 1.0f }, { "fx1Amount", 0.7f },
            { "resDecay", 3.0f }, { "resShimmer", 85.0f }, { "resSpread", 100.0f }, { "resDamp", 25.0f },
            { "fx2Type", 5.0f }, { "fx2Amount", 0.5f }, { "revSize", 0.85f }
        });

        // 06. Transient Click Snap: 5ms超短ディケイでアタックに硬質クリックを付加
        addP("Spectral Resonator Lab", "06. Transient Click Snap", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 1.0f }, { "pulseWidth", 35.0f },
            { "fx1Type", 1.0f }, { "fx1Amount", 0.8f },
            { "resShift", 12.0f }, { "resDecay", 0.005f }, { "resInharm", 40.0f }, { "resOutGain", 4.0f }
        });

        // 07. Detuned Spread Chorus Tube: ワイドステレオ拡散(100%) + コーラス
        addP("Spectral Resonator Lab", "07. Detuned Spread Chorus Tube", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.7f }, { "tracking", 80.0f },
            { "fx1Type", 1.0f }, { "fx1Amount", 0.65f },
            { "resDecay", 1.8f }, { "resSpread", 100.0f }, { "resInharm", 25.0f },
            { "fx2Type", 4.0f }, { "fx2Amount", 0.6f }, { "choRate", 1.0f }, { "choDepth", 5.0f }, { "choWidth", 1.0f }
        });

        // 08. ModWheel Shift Wobble: ModWheel(CC1)で共鳴ピッチを+24半音ベンド
        addP("Spectral Resonator Lab", "08. ModWheel Shift Wobble", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 20.0f },
            { "fx1Type", 1.0f }, { "fx1Amount", 0.8f },
            { "resDecay", 1.5f }, { "resDamp", 40.0f }, { "resSpread", 80.0f },
            { "slot0src", 8.0f }, { "slot0dst", 54.0f }, { "slot0amt", 1.0f }, { "slot0uni", 1.0f } // ModWheel -> resShift (0..+24st)
        });

        // 09. Cyber Ring Modulator: 非調和倍音100% + LoFi による強烈なロボット音
        addP("Spectral Resonator Lab", "09. Cyber Ring Modulator", {
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 1.0f }, { "lofi", 0.6f },
            { "fx1Type", 1.0f }, { "fx1Amount", 0.85f },
            { "resDecay", 1.2f }, { "resInharm", 100.0f }, { "resDamp", 15.0f },
            { "fx2Type", 2.0f }, { "fx2Amount", 0.5f }, { "drvDrive", 0.6f }
        });

        // 10. Ethereal Chord Resonator: 和音倍音がいつまでも伸びるアンビエントパッド
        addP("Spectral Resonator Lab", "10. Ethereal Chord Resonator", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 30.0f },
            { "attack", 0.15f }, { "release", 0.8f },
            { "fx1Type", 1.0f }, { "fx1Amount", 0.75f },
            { "resDecay", 2.8f }, { "resDamp", 45.0f }, { "resShimmer", 30.0f }, { "resSpread", 90.0f },
            { "fx2Type", 5.0f }, { "fx2Amount", 0.6f }, { "revSize", 0.9f }
        });

        return list;
    }
};
