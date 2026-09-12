// ==========================================
// File: FactoryPresets.h
// SPECTRA8 - 音楽的かつ独創的な 150 個の FACTORY プリセットデータ
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
        list.reserve(150);

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
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.85f }, { "tracking", 100.0f }, { "bandCount", 4.0f }, { "formantShift", 2.0f }, { "resonance", 1.1f }, { "stereoWidth", 0.8f }, { "fx1Type", 4.0f }, { "fx1Amount", 0.35f }, { "choRate", 1.2f }, { "choDepth", 30.0f }
        });
        addP("FilterBank (Auto)", "03. Deep Male Resonator", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.3f }, { "tracking", 70.0f }, { "formantShift", -5.0f }, { "resonance", 1.4f }, { "basePitch", 90.0f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.5f }, { "resDecay", 1.8f }, { "resDamp", 40.0f }
        });
        addP("FilterBank (Auto)", "04. Crystal Clean Bank", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.95f }, { "tracking", 90.0f }, { "bandCount", 5.0f }, { "resonance", 0.75f }, { "stereoWidth", 0.9f }, { "noiseColor", 4500.0f }, { "noise", 15.0f }
        });
        addP("FilterBank (Auto)", "05. Warm Analog Tube", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.4f }, { "tracking", 60.0f }, { "bandCount", 2.0f }, { "resonance", 1.6f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.45f }, { "drvDrive", 2.5f }, { "drvLow", 0.6f }
        });
        addP("FilterBank (Auto)", "06. Wide Stereo Ensemble", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.65f }, { "tracking", 85.0f }, { "bandCount", 4.0f }, { "stereoWidth", 1.0f }, { "fx1Type", 4.0f }, { "fx1Amount", 0.6f }, { "choRate", 0.8f }, { "choDepth", 60.0f }, { "choWidth", 100.0f }
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
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.7f }, { "tracking", 90.0f }, { "formantShift", 2.0f }, { "fx1Type", 4.0f }, { "fx1Amount", 0.45f }, { "choRate", 0.6f }, { "choDepth", 45.0f }, { "fx2Type", 5.0f }, { "fx2Amount", 0.45f }, { "revSize", 0.75f }
        });
        addP("FilterBank (Auto)", "11. Saturated Beast", {
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.6f }, { "tracking", 80.0f }, { "resonance", 1.4f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.8f }, { "drvDrive", 7.0f }, { "drvMid", 0.85f }, { "drvHigh", 0.6f }
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
            { "vocoderMode", 0.0f }, { "mode", 0.0f }, { "character", 0.65f }, { "tracking", 90.0f }, { "formantShift", 3.0f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.65f }, { "drvDrive", 5.0f }, { "fx2Type", 5.0f }, { "fx2Amount", 0.35f }, { "revSize", 0.6f }
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
            { "vocoderMode", 1.0f }, { "mode", 0.0f }, { "lpcOrder", 2.0f }, { "frameRate", 3.0f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.7f }, { "drvDrive", 5.0f }
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
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 35.0f }, { "stereoWidth", 1.0f }, { "fx1Type", 4.0f }, { "fx1Amount", 0.7f }, { "choRate", 1.5f }, { "choDepth", 45.0f }, { "choWidth", 100.0f }
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
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 20.0f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.8f }, { "drvDrive", 8.0f }, { "drvHigh", 0.7f }
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
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 2.0f }, { "frameRate", 3.0f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.8f }, { "drvDrive", 6.0f }, { "porta", 0.05f }
        });
        addP("LPC (MIDI)", "12. Sub Bass LPC Synthesizer", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 2.0f }, { "masterPitch", -12.0f }, { "waveform", 0.0f }, { "detune", 10.0f }
        });
        addP("LPC (MIDI)", "13. Chorus Ensemble LPC", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "frameRate", 4.0f }, { "fx1Type", 4.0f }, { "fx1Amount", 0.6f }, { "choRate", 1.5f }, { "choDepth", 38.0f }
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
            { "fx1Type", 4.0f }, { "fx1Amount", 0.4f }, { "choRate", 0.5f }, { "choDepth", 22.0f }
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
            { "fx1Type", 2.0f }, { "fx1Amount", 0.7f }, { "drvDrive", 8.0f }, { "drvLow", 0.9f }
        });

        // 07. ModWheel Pitch Bend Lead: モジュレーションホイール(CC1)で+12stベンド
        addP("M.Pitch Modulations", "07. ModWheel Pitch Bend Lead", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 20.0f },
            { "slot0src", 8.0f }, { "slot0dst", 27.0f }, { "slot0amt", 0.5f }, { "slot0uni", 1.0f }, // ModWheel -> +12st
            { "porta", 0.05f }, { "resonance", 1.3f },
            { "fx1Type", 4.0f }, { "fx1Amount", 0.5f }, { "choRate", 1.2f }, { "choDepth", 30.0f }
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
            { "fx1Type", 4.0f }, { "fx1Amount", 0.6f }, { "choWidth", 100.0f }
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
            { "fx2Type", 2.0f }, { "fx2Amount", 0.85f }, { "drvDrive", 8.0f }, { "drvHigh", 0.75f }
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
            { "fx2Type", 2.0f }, { "fx2Amount", 0.35f }, { "drvDrive", 3.0f }
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
            { "fx2Type", 4.0f }, { "fx2Amount", 0.6f }, { "choRate", 1.0f }, { "choDepth", 38.0f }, { "choWidth", 100.0f }
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
            { "fx2Type", 2.0f }, { "fx2Amount", 0.5f }, { "drvDrive", 5.0f }
        });

        // 10. Ethereal Chord Resonator: 和音倍音がいつまでも伸びるアンビエントパッド
        addP("Spectral Resonator Lab", "10. Ethereal Chord Resonator", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "waveform", 0.0f }, { "detune", 30.0f },
            { "attack", 0.15f }, { "release", 0.8f },
            { "fx1Type", 1.0f }, { "fx1Amount", 0.75f },
            { "resDecay", 2.8f }, { "resDamp", 45.0f }, { "resShimmer", 30.0f }, { "resSpread", 90.0f },
            { "fx2Type", 5.0f }, { "fx2Amount", 0.6f }, { "revSize", 0.9f }
        });

        // ====================================================================
        // 8. SpecialFX - 40個 (独創的FX / 全MIDIモード / 6 Modスロット全使用)
        // ====================================================================
        addP("SpecialFX", "SFX01 Cybernetic Uplink", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "bandCount", 32.0f }, { "resonance", 2.4f }, { "stereoWidth", 1.0f }, { "waveform", 0.0f }, { "detune", 20.0f }, { "character", 0.8f }, { "mix", 100.0f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.65f }, { "resDecay", 1.5f }, { "resShift", 0.0f }, { "resSpread", 0.8f }, { "fx2Type", 3.0f }, { "fx2Amount", 0.8f }, { "gatePattern", 1.0f }, { "gateDecay", 0.03f }, { "gateVowel", 0.5f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.5f }, { "revSize", 0.8f }, { "revDamp", 30.0f }, { "lfo0sync", 1.0f }, { "lfo0rateSync", 9.0f }, { "lfo0wave", 4.0f }, { "lfo1sync", 1.0f }, { "lfo1rateSync", 6.0f }, { "lfo1wave", 0.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 12.0f }, { "lfo2wave", 1.0f }, { "env0attack", 0.01f }, { "env0decay", 0.25f }, { "env0sustain", 0.0f }, { "env0release", 0.1f }, { "env0loop", 1.0f }, { "slot0src", 1.0f }, { "slot0dst", 27.0f }, { "slot0amt", 0.5f }, { "slot1src", 2.0f }, { "slot1dst", 4.0f }, { "slot1amt", 0.33f }, { "slot2src", 3.0f }, { "slot2dst", 54.0f }, { "slot2amt", 0.5f }, { "slot3src", 4.0f }, { "slot3dst", 10.0f }, { "slot3amt", 0.4f }, { "slot4src", 9.0f }, { "slot4dst", 28.0f }, { "slot4amt", 0.8f }, { "slot5src", 8.0f }, { "slot5dst", 29.0f }, { "slot5amt", 0.8f }
        });
        addP("SpecialFX", "SFX02 Quantum Singularity", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "bandCount", 48.0f }, { "resonance", 1.8f }, { "stereoWidth", 0.9f }, { "waveform", 0.0f }, { "detune", 60.0f }, { "detuneMode", 2.0f }, { "character", 0.7f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.75f }, { "resDecay", 2.8f }, { "resInharm", 70.0f }, { "resShimmer", 80.0f }, { "fx2Type", 2.0f }, { "fx2Amount", 0.6f }, { "drvDrive", 4.5f }, { "drvLow", 0.8f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.7f }, { "revSize", 0.95f }, { "revDamp", 50.0f }, { "env0attack", 0.05f }, { "env0decay", 3.0f }, { "env0sustain", 0.0f }, { "env0release", 1.0f }, { "lfo0sync", 0.0f }, { "lfo0rate", 0.4f }, { "lfo0wave", 5.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 0.1f }, { "lfo1wave", 0.0f }, { "slot0src", 4.0f }, { "slot0dst", 27.0f }, { "slot0amt", -1.0f }, { "slot0uni", 1.0f }, { "slot1src", 1.0f }, { "slot1dst", 38.0f }, { "slot1amt", 0.7f }, { "slot2src", 2.0f }, { "slot2dst", 5.0f }, { "slot2amt", 0.5f }, { "slot3src", 7.0f }, { "slot3dst", 4.0f }, { "slot3amt", 0.4f }, { "slot4src", 6.0f }, { "slot4dst", 35.0f }, { "slot4amt", 0.6f }, { "slot5src", 8.0f }, { "slot5dst", 47.0f }, { "slot5amt", 0.5f }
        });
        addP("SpecialFX", "SFX03 Hologram Glitcher", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "bandCount", 16.0f }, { "resonance", 2.0f }, { "character", 0.9f }, { "waveform", 0.0f }, { "bendAmt", 0.5f }, { "fx1Type", 3.0f }, { "fx1Amount", 0.9f }, { "gatePattern", 25.0f }, { "gateDecay", 0.02f }, { "fx2Type", 2.0f }, { "fx2Amount", 0.5f }, { "drvDrive", 3.5f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.4f }, { "revSize", 0.6f }, { "lfo0sync", 1.0f }, { "lfo0rateSync", 12.0f }, { "lfo0wave", 3.0f }, { "lfo1sync", 1.0f }, { "lfo1rateSync", 3.0f }, { "lfo1wave", 2.0f }, { "lfo2sync", 1.0f }, { "lfo2rateSync", 8.0f }, { "lfo2wave", 4.0f }, { "env1attack", 0.01f }, { "env1decay", 0.15f }, { "env1sustain", 0.0f }, { "env1release", 0.05f }, { "slot0src", 1.0f }, { "slot0dst", 31.0f }, { "slot0amt", 0.9f }, { "slot1src", 2.0f }, { "slot1dst", 21.0f }, { "slot1amt", 0.8f }, { "slot2src", 3.0f }, { "slot2dst", 4.0f }, { "slot2amt", 0.6f }, { "slot3src", 5.0f }, { "slot3dst", 29.0f }, { "slot3amt", 0.7f }, { "slot4src", 6.0f }, { "slot4dst", 18.0f }, { "slot4amt", 0.5f }, { "slot5src", 9.0f }, { "slot5dst", 28.0f }, { "slot5amt", 0.75f }
        });
        addP("SpecialFX", "SFX04 Alien Bio-Scanner", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "bandCount", 24.0f }, { "resonance", 1.5f }, { "character", 0.75f }, { "waveform", 2.0f }, { "wavetablePosition", 0.4f }, { "vocAmt", 0.8f }, { "vocShift", 0.2f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.7f }, { "resDecay", 2.0f }, { "resSpread", 0.5f }, { "resShimmer", 50.0f }, { "fx2Type", 4.0f }, { "fx2Amount", 0.6f }, { "choRate", 1.5f }, { "choDepth", 45.0f }, { "choWidth", 100.0f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.65f }, { "revSize", 0.85f }, { "lfo0sync", 1.0f }, { "lfo0rateSync", 1.0f }, { "lfo0wave", 1.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 4.5f }, { "lfo1wave", 0.0f }, { "lfo2sync", 1.0f }, { "lfo2rateSync", 6.0f }, { "lfo2wave", 2.0f }, { "env0attack", 0.3f }, { "env0decay", 0.8f }, { "env0sustain", 0.1f }, { "env0release", 0.5f }, { "env0loop", 1.0f }, { "slot0src", 1.0f }, { "slot0dst", 26.0f }, { "slot0amt", 0.8f }, { "slot1src", 2.0f }, { "slot1dst", 55.0f }, { "slot1amt", 0.7f }, { "slot2src", 3.0f }, { "slot2dst", 4.0f }, { "slot2amt", 0.5f }, { "slot3src", 4.0f }, { "slot3dst", 35.0f }, { "slot3amt", 0.6f }, { "slot4src", 8.0f }, { "slot4dst", 36.0f }, { "slot4amt", 0.85f }, { "slot5src", 7.0f }, { "slot5dst", 8.0f }, { "slot5amt", 0.6f }
        });
        addP("SpecialFX", "SFX05 Sub-Atomic Collider", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "bandCount", 32.0f }, { "resonance", 1.8f }, { "detune", 40.0f }, { "waveform", 0.0f }, { "syncAmt", 0.2f }, { "basePitch", 60.0f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.7f }, { "drvDrive", 5.0f }, { "drvLow", 0.75f }, { "fx2Type", 3.0f }, { "fx2Amount", 0.85f }, { "gatePattern", 7.0f }, { "gateDecay", 0.04f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.45f }, { "revSize", 0.7f }, { "env0attack", 0.05f }, { "env0decay", 1.5f }, { "env0sustain", 0.2f }, { "env0release", 0.4f }, { "lfo0sync", 1.0f }, { "lfo0rateSync", 7.0f }, { "lfo0wave", 4.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 18.0f }, { "lfo1wave", 3.0f }, { "lfo2sync", 1.0f }, { "lfo2rateSync", 3.0f }, { "lfo2wave", 1.0f }, { "slot0src", 4.0f }, { "slot0dst", 23.0f }, { "slot0amt", 0.85f }, { "slot1src", 1.0f }, { "slot1dst", 27.0f }, { "slot1amt", 0.3f }, { "slot2src", 2.0f }, { "slot2dst", 53.0f }, { "slot2amt", 0.7f }, { "slot3src", 3.0f }, { "slot3dst", 4.0f }, { "slot3amt", 0.4f }, { "slot4src", 6.0f }, { "slot4dst", 29.0f }, { "slot4amt", 0.65f }, { "slot5src", 8.0f }, { "slot5dst", 47.0f }, { "slot5amt", 0.7f }
        });
        addP("SpecialFX", "SFX06 Android Tears", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "bandCount", 40.0f }, { "resonance", 0.9f }, { "stereoWidth", 0.85f }, { "waveform", 0.0f }, { "detune", 25.0f }, { "detuneMode", 3.0f }, { "character", 0.65f }, { "fx1Type", 4.0f }, { "fx1Amount", 0.5f }, { "choRate", 0.6f }, { "choDepth", 60.0f }, { "choWidth", 100.0f }, { "fx2Type", 1.0f }, { "fx2Amount", 0.6f }, { "resDecay", 2.2f }, { "resShimmer", 75.0f }, { "resDamp", 30.0f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.65f }, { "revSize", 0.9f }, { "revPredelay", 40.0f }, { "lfo0sync", 0.0f }, { "lfo0rate", 0.15f }, { "lfo0wave", 0.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 0.22f }, { "lfo1wave", 1.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 0.08f }, { "lfo2wave", 0.0f }, { "env0attack", 1.5f }, { "env0decay", 2.0f }, { "env0sustain", 0.6f }, { "env0release", 2.5f }, { "slot0src", 1.0f }, { "slot0dst", 20.0f }, { "slot0amt", 0.4f }, { "slot1src", 2.0f }, { "slot1dst", 4.0f }, { "slot1amt", 0.15f }, { "slot2src", 3.0f }, { "slot2dst", 40.0f }, { "slot2amt", 0.5f }, { "slot3src", 4.0f }, { "slot3dst", 47.0f }, { "slot3amt", 0.4f }, { "slot4src", 8.0f }, { "slot4dst", 36.0f }, { "slot4amt", 0.85f }, { "slot5src", 6.0f }, { "slot5dst", 1.0f }, { "slot5amt", 0.35f }
        });
        addP("SpecialFX", "SFX07 Neuro Warp Monster", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "bandCount", 24.0f }, { "resonance", 2.2f }, { "waveform", 0.0f }, { "bendAmt", 0.6f }, { "character", 0.8f }, { "basePitch", 50.0f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.85f }, { "drvDrive", 6.0f }, { "drvLow", 0.9f }, { "fx2Type", 3.0f }, { "fx2Amount", 0.75f }, { "gatePattern", 3.0f }, { "gateDecay", 0.05f }, { "gateVowel", 0.4f }, { "fx3Type", 1.0f }, { "fx3Amount", 0.5f }, { "resDecay", 1.0f }, { "resShift", -7.0f }, { "lfo0sync", 1.0f }, { "lfo0rateSync", 3.0f }, { "lfo0wave", 2.0f }, { "lfo1sync", 1.0f }, { "lfo1rateSync", 6.0f }, { "lfo1wave", 1.0f }, { "lfo2sync", 1.0f }, { "lfo2rateSync", 1.0f }, { "lfo2wave", 0.0f }, { "env0attack", 0.02f }, { "env0decay", 0.4f }, { "env0sustain", 0.2f }, { "env0release", 0.2f }, { "slot0src", 1.0f }, { "slot0dst", 21.0f }, { "slot0amt", 0.95f }, { "slot1src", 2.0f }, { "slot1dst", 4.0f }, { "slot1amt", 0.65f }, { "slot2src", 3.0f }, { "slot2dst", 32.0f }, { "slot2amt", 0.8f }, { "slot3src", 4.0f }, { "slot3dst", 29.0f }, { "slot3amt", 0.75f }, { "slot4src", 8.0f }, { "slot4dst", 10.0f }, { "slot4amt", 0.6f }, { "slot5src", 6.0f }, { "slot5dst", 27.0f }, { "slot5amt", -0.2f }
        });
        addP("SpecialFX", "SFX08 Deep Sea Sonar", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "bandCount", 16.0f }, { "resonance", 2.8f }, { "character", 0.4f }, { "waveform", 1.0f }, { "pulseWidth", 10.0f }, { "attack", 0.005f }, { "decay", 1.8f }, { "sustain", 0.0f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.8f }, { "resDecay", 2.5f }, { "resDamp", 60.0f }, { "fx2Type", 4.0f }, { "fx2Amount", 0.5f }, { "choRate", 0.3f }, { "choDepth", 90.0f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.7f }, { "revSize", 0.95f }, { "revPredelay", 50.0f }, { "env0attack", 0.005f }, { "env0decay", 2.5f }, { "env0sustain", 0.0f }, { "env0release", 1.0f }, { "lfo0sync", 0.0f }, { "lfo0rate", 0.05f }, { "lfo0wave", 0.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 0.3f }, { "lfo1wave", 1.0f }, { "slot0src", 4.0f }, { "slot0dst", 35.0f }, { "slot0amt", 0.85f }, { "slot1src", 1.0f }, { "slot1dst", 4.0f }, { "slot1amt", 0.25f }, { "slot2src", 2.0f }, { "slot2dst", 50.0f }, { "slot2amt", 0.5f }, { "slot3src", 7.0f }, { "slot3dst", 54.0f }, { "slot3amt", 0.5f }, { "slot4src", 6.0f }, { "slot4dst", 36.0f }, { "slot4amt", 0.6f }, { "slot5src", 8.0f }, { "slot5dst", 47.0f }, { "slot5amt", 0.8f }
        });
        addP("SpecialFX", "SFX09 Mecha Transform", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "bandCount", 32.0f }, { "resonance", 1.6f }, { "character", 0.85f }, { "waveform", 1.0f }, { "pulseWidth", 50.0f }, { "syncAmt", 0.3f }, { "fx1Type", 3.0f }, { "fx1Amount", 0.85f }, { "gatePattern", 11.0f }, { "gateDecay", 0.035f }, { "fx2Type", 2.0f }, { "fx2Amount", 0.7f }, { "drvDrive", 4.0f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.4f }, { "revSize", 0.65f }, { "env0attack", 0.05f }, { "env0decay", 0.8f }, { "env0sustain", 0.1f }, { "env0release", 0.2f }, { "env0loop", 1.0f }, { "lfo0sync", 1.0f }, { "lfo0rateSync", 11.0f }, { "lfo0wave", 2.0f }, { "lfo1sync", 1.0f }, { "lfo1rateSync", 3.0f }, { "lfo1wave", 4.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 8.0f }, { "lfo2wave", 1.0f }, { "slot0src", 4.0f }, { "slot0dst", 23.0f }, { "slot0amt", 0.8f }, { "slot1src", 1.0f }, { "slot1dst", 18.0f }, { "slot1amt", 0.45f }, { "slot2src", 2.0f }, { "slot2dst", 27.0f }, { "slot2amt", 0.5f }, { "slot3src", 3.0f }, { "slot3dst", 29.0f }, { "slot3amt", 0.4f }, { "slot4src", 6.0f }, { "slot4dst", 30.0f }, { "slot4amt", 0.5f }, { "slot5src", 8.0f }, { "slot5dst", 4.0f }, { "slot5amt", 0.75f }
        });
        addP("SpecialFX", "SFX10 Cyber Shaman", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "bandCount", 48.0f }, { "resonance", 1.4f }, { "stereoWidth", 0.9f }, { "waveform", 0.0f }, { "detune", 35.0f }, { "pitchQuantize", 100.0f }, { "pitchQKey", 0.0f }, { "pitchQScale", 1.0f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.8f }, { "resDecay", 2.4f }, { "resShimmer", 80.0f }, { "resInharm", 50.0f }, { "fx2Type", 4.0f }, { "fx2Amount", 0.5f }, { "choRate", 1.0f }, { "choDepth", 60.0f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.65f }, { "revSize", 0.9f }, { "lfo0sync", 1.0f }, { "lfo0rateSync", 6.0f }, { "lfo0wave", 4.0f }, { "lfo1sync", 1.0f }, { "lfo1rateSync", 1.0f }, { "lfo1wave", 0.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 0.2f }, { "lfo2wave", 5.0f }, { "env1attack", 0.8f }, { "env1decay", 1.5f }, { "env1sustain", 0.5f }, { "env1release", 1.2f }, { "slot0src", 1.0f }, { "slot0dst", 27.0f }, { "slot0amt", 0.5f }, { "slot1src", 2.0f }, { "slot1dst", 36.0f }, { "slot1amt", 0.75f }, { "slot2src", 3.0f }, { "slot2dst", 38.0f }, { "slot2amt", 0.6f }, { "slot3src", 5.0f }, { "slot3dst", 4.0f }, { "slot3amt", 0.3f }, { "slot4src", 8.0f }, { "slot4dst", 40.0f }, { "slot4amt", 0.7f }, { "slot5src", 6.0f }, { "slot5dst", 10.0f }, { "slot5amt", 0.5f }
        });
        addP("SpecialFX", "SFX11 Temporal Distortion", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "bandCount", 32.0f }, { "resonance", 1.2f }, { "waveform", 0.0f }, { "formantStretch", 1.2f }, { "stereoWidth", 0.8f }, { "fx1Type", 3.0f }, { "fx1Amount", 0.8f }, { "gatePattern", 13.0f }, { "gateDecay", 0.06f }, { "fx2Type", 4.0f }, { "fx2Amount", 0.6f }, { "choRate", 0.4f }, { "choDepth", 100.0f }, { "choWidth", 100.0f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.6f }, { "revSize", 0.85f }, { "lfo0sync", 0.0f }, { "lfo0rate", 0.4f }, { "lfo0wave", 0.0f }, { "lfo1sync", 1.0f }, { "lfo1rateSync", 3.0f }, { "lfo1wave", 2.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 0.1f }, { "lfo2wave", 1.0f }, { "env0attack", 1.0f }, { "env0decay", 1.0f }, { "env0sustain", 0.0f }, { "env0release", 0.5f }, { "slot0src", 1.0f }, { "slot0dst", 40.0f }, { "slot0amt", 0.75f }, { "slot1src", 2.0f }, { "slot1dst", 32.0f }, { "slot1amt", 0.7f }, { "slot2src", 3.0f }, { "slot2dst", 5.0f }, { "slot2amt", 0.5f }, { "slot3src", 4.0f }, { "slot3dst", 15.0f }, { "slot3amt", 0.4f }, { "slot4src", 8.0f }, { "slot4dst", 41.0f }, { "slot4amt", 0.6f }, { "slot5src", 6.0f }, { "slot5dst", 27.0f }, { "slot5amt", -0.3f }
        });
        addP("SpecialFX", "SFX12 Plasma Storm", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "bandCount", 48.0f }, { "resonance", 2.0f }, { "noise", 45.0f }, { "air", 80.0f }, { "noiseColor", 4000.0f }, { "waveform", 0.0f }, { "detune", 40.0f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.75f }, { "drvDrive", 5.0f }, { "drvHigh", 0.8f }, { "fx2Type", 4.0f }, { "fx2Amount", 0.6f }, { "choRate", 2.2f }, { "choDepth", 45.0f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.7f }, { "revSize", 0.9f }, { "lfo0sync", 0.0f }, { "lfo0rate", 35.0f }, { "lfo0wave", 5.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 0.3f }, { "lfo1wave", 0.0f }, { "lfo2sync", 1.0f }, { "lfo2rateSync", 6.0f }, { "lfo2wave", 0.0f }, { "env0attack", 0.05f }, { "env0decay", 0.2f }, { "env0sustain", 0.1f }, { "env0release", 0.1f }, { "env0loop", 1.0f }, { "slot0src", 1.0f }, { "slot0dst", 8.0f }, { "slot0amt", 0.6f }, { "slot1src", 2.0f }, { "slot1dst", 52.0f }, { "slot1amt", 0.5f }, { "slot2src", 3.0f }, { "slot2dst", 28.0f }, { "slot2amt", 0.5f }, { "slot3src", 4.0f }, { "slot3dst", 29.0f }, { "slot3amt", 0.5f }, { "slot4src", 8.0f }, { "slot4dst", 9.0f }, { "slot4amt", 0.6f }, { "slot5src", 7.0f }, { "slot5dst", 4.0f }, { "slot5amt", 0.4f }
        });
        addP("SpecialFX", "SFX13 Gravitational Waves", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "bandCount", 40.0f }, { "resonance", 1.5f }, { "basePitch", 55.0f }, { "waveform", 0.0f }, { "detune", 30.0f }, { "character", 0.5f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.7f }, { "resDecay", 3.0f }, { "resShimmer", 60.0f }, { "resDamp", 40.0f }, { "fx2Type", 2.0f }, { "fx2Amount", 0.5f }, { "drvDrive", 3.0f }, { "drvLow", 0.9f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.8f }, { "revSize", 0.98f }, { "revDamp", 60.0f }, { "lfo0sync", 0.0f }, { "lfo0rate", 0.08f }, { "lfo0wave", 0.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 0.12f }, { "lfo1wave", 1.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 0.2f }, { "lfo2wave", 0.0f }, { "env0attack", 3.0f }, { "env0decay", 3.0f }, { "env0sustain", 0.7f }, { "env0release", 4.0f }, { "slot0src", 1.0f }, { "slot0dst", 7.0f }, { "slot0amt", 0.4f }, { "slot1src", 2.0f }, { "slot1dst", 28.0f }, { "slot1amt", 0.6f }, { "slot2src", 3.0f }, { "slot2dst", 47.0f }, { "slot2amt", 0.4f }, { "slot3src", 4.0f }, { "slot3dst", 36.0f }, { "slot3amt", 0.65f }, { "slot4src", 8.0f }, { "slot4dst", 4.0f }, { "slot4amt", -0.5f }, { "slot5src", 6.0f }, { "slot5dst", 37.0f }, { "slot5amt", -0.4f }
        });
        addP("SpecialFX", "SFX14 Android Morse Code", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "bandCount", 16.0f }, { "resonance", 2.5f }, { "character", 0.9f }, { "waveform", 1.0f }, { "pulseWidth", 15.0f }, { "attack", 0.005f }, { "decay", 0.1f }, { "fx1Type", 3.0f }, { "fx1Amount", 0.95f }, { "gatePattern", 25.0f }, { "gateDecay", 0.015f }, { "fx2Type", 2.0f }, { "fx2Amount", 0.6f }, { "drvDrive", 4.0f }, { "drvHigh", 0.7f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.4f }, { "revSize", 0.6f }, { "lfo0sync", 1.0f }, { "lfo0rateSync", 12.0f }, { "lfo0wave", 4.0f }, { "lfo1sync", 1.0f }, { "lfo1rateSync", 7.0f }, { "lfo1wave", 1.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 6.0f }, { "lfo2wave", 3.0f }, { "env1attack", 0.005f }, { "env1decay", 0.03f }, { "env1sustain", 0.0f }, { "env1release", 0.02f }, { "slot0src", 1.0f }, { "slot0dst", 31.0f }, { "slot0amt", 1.0f }, { "slot1src", 2.0f }, { "slot1dst", 18.0f }, { "slot1amt", 0.5f }, { "slot2src", 3.0f }, { "slot2dst", 27.0f }, { "slot2amt", 0.3f }, { "slot3src", 5.0f }, { "slot3dst", 29.0f }, { "slot3amt", 0.7f }, { "slot4src", 8.0f }, { "slot4dst", 30.0f }, { "slot4amt", 0.8f }, { "slot5src", 6.0f }, { "slot5dst", 4.0f }, { "slot5amt", 0.5f }
        });
        addP("SpecialFX", "SFX15 Liquid Metal Morph", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "bandCount", 32.0f }, { "resonance", 2.0f }, { "character", 0.8f }, { "waveform", 2.0f }, { "wavetablePosition", 0.6f }, { "vocAmt", 0.7f }, { "vocShift", 0.3f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.8f }, { "resDecay", 1.8f }, { "resInharm", 40.0f }, { "resSpread", 0.7f }, { "fx2Type", 4.0f }, { "fx2Amount", 0.55f }, { "choRate", 1.2f }, { "choDepth", 60.0f }, { "fx3Type", 2.0f }, { "fx3Amount", 0.5f }, { "drvDrive", 3.0f }, { "lfo0sync", 0.0f }, { "lfo0rate", 0.8f }, { "lfo0wave", 0.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 0.5f }, { "lfo1wave", 1.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 1.4f }, { "lfo2wave", 2.0f }, { "env0attack", 0.1f }, { "env0decay", 0.5f }, { "env0sustain", 0.2f }, { "env0release", 0.3f }, { "env0loop", 1.0f }, { "slot0src", 1.0f }, { "slot0dst", 54.0f }, { "slot0amt", 0.5f }, { "slot1src", 2.0f }, { "slot1dst", 26.0f }, { "slot1amt", 0.7f }, { "slot2src", 3.0f }, { "slot2dst", 55.0f }, { "slot2amt", 0.6f }, { "slot3src", 4.0f }, { "slot3dst", 38.0f }, { "slot3amt", 0.6f }, { "slot4src", 8.0f }, { "slot4dst", 21.0f }, { "slot4amt", 0.8f }, { "slot5src", 6.0f }, { "slot5dst", 56.0f }, { "slot5amt", 0.5f }
        });
        addP("SpecialFX", "SFX16 Dimension Rift", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "bandCount", 48.0f }, { "resonance", 1.6f }, { "detune", 700.0f }, { "detuneMode", 2.0f }, { "stereoWidth", 1.0f }, { "waveform", 0.0f }, { "fx1Type", 4.0f }, { "fx1Amount", 0.65f }, { "choRate", 0.7f }, { "choDepth", 75.0f }, { "choWidth", 100.0f }, { "fx2Type", 5.0f }, { "fx2Amount", 0.75f }, { "revSize", 0.95f }, { "revPredelay", 60.0f }, { "fx3Type", 2.0f }, { "fx3Amount", 0.45f }, { "drvDrive", 3.0f }, { "lfo0sync", 0.0f }, { "lfo0rate", 0.1f }, { "lfo0wave", 5.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 0.35f }, { "lfo1wave", 0.0f }, { "lfo2sync", 1.0f }, { "lfo2rateSync", 3.0f }, { "lfo2wave", 1.0f }, { "env0attack", 2.0f }, { "env0decay", 2.0f }, { "env0sustain", 0.5f }, { "env0release", 2.0f }, { "slot0src", 1.0f }, { "slot0dst", 20.0f }, { "slot0amt", 0.4f }, { "slot1src", 2.0f }, { "slot1dst", 40.0f }, { "slot1amt", 0.6f }, { "slot2src", 3.0f }, { "slot2dst", 4.0f }, { "slot2amt", 0.4f }, { "slot3src", 4.0f }, { "slot3dst", 49.0f }, { "slot3amt", 0.5f }, { "slot4src", 8.0f }, { "slot4dst", 47.0f }, { "slot4amt", 0.6f }, { "slot5src", 9.0f }, { "slot5dst", 28.0f }, { "slot5amt", 0.6f }
        });
        addP("SpecialFX", "SFX17 Cyber Bird Chirp", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "bandCount", 16.0f }, { "resonance", 2.7f }, { "character", 0.95f }, { "waveform", 0.0f }, { "attack", 0.005f }, { "decay", 0.2f }, { "sustain", 0.0f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.75f }, { "resDecay", 1.2f }, { "resShift", 12.0f }, { "fx2Type", 5.0f }, { "fx2Amount", 0.55f }, { "revSize", 0.7f }, { "fx3Type", 4.0f }, { "fx3Amount", 0.4f }, { "choRate", 2.0f }, { "choDepth", 30.0f }, { "env0attack", 0.005f }, { "env0decay", 0.08f }, { "env0sustain", 0.0f }, { "env0release", 0.05f }, { "env0loop", 1.0f }, { "lfo0sync", 0.0f }, { "lfo0rate", 14.0f }, { "lfo0wave", 0.0f }, { "lfo1sync", 1.0f }, { "lfo1rateSync", 6.0f }, { "lfo1wave", 4.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 0.6f }, { "lfo2wave", 1.0f }, { "slot0src", 4.0f }, { "slot0dst", 27.0f }, { "slot0amt", 1.0f }, { "slot1src", 1.0f }, { "slot1dst", 4.0f }, { "slot1amt", 0.25f }, { "slot2src", 2.0f }, { "slot2dst", 54.0f }, { "slot2amt", 0.5f }, { "slot3src", 3.0f }, { "slot3dst", 28.0f }, { "slot3amt", 0.7f }, { "slot4src", 6.0f }, { "slot4dst", 35.0f }, { "slot4amt", 0.5f }, { "slot5src", 8.0f }, { "slot5dst", 47.0f }, { "slot5amt", 0.6f }
        });
        addP("SpecialFX", "SFX18 Industrial Strobe", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "bandCount", 24.0f }, { "resonance", 1.8f }, { "waveform", 1.0f }, { "pulseWidth", 60.0f }, { "character", 0.85f }, { "fx1Type", 3.0f }, { "fx1Amount", 0.9f }, { "gatePattern", 0.0f }, { "gateDecay", 0.025f }, { "fx2Type", 2.0f }, { "fx2Amount", 0.8f }, { "drvDrive", 5.5f }, { "drvLow", 0.8f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.5f }, { "revSize", 0.75f }, { "lfo0sync", 1.0f }, { "lfo0rateSync", 9.0f }, { "lfo0wave", 3.0f }, { "lfo1sync", 1.0f }, { "lfo1rateSync", 3.0f }, { "lfo1wave", 2.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 2.5f }, { "lfo2wave", 1.0f }, { "env0attack", 0.01f }, { "env0decay", 0.15f }, { "env0sustain", 0.0f }, { "env0release", 0.05f }, { "slot0src", 1.0f }, { "slot0dst", 31.0f }, { "slot0amt", 1.0f }, { "slot1src", 2.0f }, { "slot1dst", 29.0f }, { "slot1amt", 0.6f }, { "slot2src", 3.0f }, { "slot2dst", 18.0f }, { "slot2amt", 0.4f }, { "slot3src", 4.0f }, { "slot3dst", 27.0f }, { "slot3amt", -0.5f }, { "slot4src", 8.0f }, { "slot4dst", 53.0f }, { "slot4amt", 0.7f }, { "slot5src", 6.0f }, { "slot5dst", 10.0f }, { "slot5amt", 0.6f }
        });
        addP("SpecialFX", "SFX19 Radio Galaxy Pulse", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "bandCount", 32.0f }, { "resonance", 1.5f }, { "detune", 30.0f }, { "waveform", 0.0f }, { "syncAmt", 0.4f }, { "character", 0.75f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.75f }, { "resDecay", 2.0f }, { "resShift", 7.0f }, { "resShimmer", 70.0f }, { "fx2Type", 4.0f }, { "fx2Amount", 0.5f }, { "choRate", 0.8f }, { "choDepth", 45.0f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.7f }, { "revSize", 0.92f }, { "revPredelay", 50.0f }, { "lfo0sync", 1.0f }, { "lfo0rateSync", 6.0f }, { "lfo0wave", 4.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 0.4f }, { "lfo1wave", 0.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 0.15f }, { "lfo2wave", 1.0f }, { "env1attack", 0.05f }, { "env1decay", 0.5f }, { "env1sustain", 0.1f }, { "env1release", 0.3f }, { "env1loop", 1.0f }, { "slot0src", 1.0f }, { "slot0dst", 23.0f }, { "slot0amt", 0.7f }, { "slot1src", 2.0f }, { "slot1dst", 4.0f }, { "slot1amt", 0.35f }, { "slot2src", 3.0f }, { "slot2dst", 49.0f }, { "slot2amt", 0.5f }, { "slot3src", 5.0f }, { "slot3dst", 27.0f }, { "slot3amt", 0.5f }, { "slot4src", 8.0f }, { "slot4dst", 47.0f }, { "slot4amt", 0.35f }, { "slot5src", 7.0f }, { "slot5dst", 54.0f }, { "slot5amt", 0.5f }
        });
        addP("SpecialFX", "SFX20 Alien Queen Whispers", {
            { "vocoderMode", 0.0f }, { "mode", 1.0f }, { "bandCount", 48.0f }, { "resonance", 1.6f }, { "noise", 60.0f }, { "air", 75.0f }, { "waveform", 0.0f }, { "character", 0.9f }, { "formantStretch", 1.3f }, { "fx1Type", 3.0f }, { "fx1Amount", 0.8f }, { "gatePattern", 3.0f }, { "gateDecay", 0.06f }, { "gateVowel", 0.5f }, { "fx2Type", 1.0f }, { "fx2Amount", 0.7f }, { "resDecay", 2.2f }, { "resShimmer", 80.0f }, { "resSpread", 0.9f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.75f }, { "revSize", 0.95f }, { "lfo0sync", 1.0f }, { "lfo0rateSync", 3.0f }, { "lfo0wave", 0.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 0.25f }, { "lfo1wave", 1.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 8.0f }, { "lfo2wave", 5.0f }, { "env0attack", 1.2f }, { "env0decay", 2.0f }, { "env0sustain", 0.4f }, { "env0release", 2.0f }, { "slot0src", 1.0f }, { "slot0dst", 32.0f }, { "slot0amt", 0.75f }, { "slot1src", 2.0f }, { "slot1dst", 5.0f }, { "slot1amt", 0.5f }, { "slot2src", 3.0f }, { "slot2dst", 8.0f }, { "slot2amt", 0.5f }, { "slot3src", 4.0f }, { "slot3dst", 36.0f }, { "slot3amt", 0.7f }, { "slot4src", 8.0f }, { "slot4dst", 35.0f }, { "slot4amt", 0.8f }, { "slot5src", 6.0f }, { "slot5dst", 15.0f }, { "slot5amt", 0.3f }
        });
        addP("SpecialFX", "SFX21 8-Bit Robot Overlord", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 1.0f }, { "frameRate", 0.0f }, { "lpcQuantBits", 3.0f }, { "interpolationMode", 0.0f }, { "waveform", 1.0f }, { "pulseWidth", 50.0f }, { "character", 1.0f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.7f }, { "drvDrive", 4.5f }, { "fx2Type", 1.0f }, { "fx2Amount", 0.6f }, { "resDecay", 0.8f }, { "resShift", 12.0f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.4f }, { "revSize", 0.5f }, { "lfo0sync", 1.0f }, { "lfo0rateSync", 9.0f }, { "lfo0wave", 4.0f }, { "lfo1sync", 1.0f }, { "lfo1rateSync", 6.0f }, { "lfo1wave", 3.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 15.0f }, { "lfo2wave", 1.0f }, { "env0attack", 0.005f }, { "env0decay", 0.08f }, { "env0sustain", 0.0f }, { "env0release", 0.05f }, { "slot0src", 1.0f }, { "slot0dst", 27.0f }, { "slot0amt", 0.5f }, { "slot1src", 2.0f }, { "slot1dst", 4.0f }, { "slot1amt", 0.3f }, { "slot2src", 3.0f }, { "slot2dst", 18.0f }, { "slot2amt", 0.4f }, { "slot3src", 4.0f }, { "slot3dst", 29.0f }, { "slot3amt", 0.5f }, { "slot4src", 8.0f }, { "slot4dst", 27.0f }, { "slot4amt", 0.5f }, { "slot4uni", 1.0f }, { "slot5src", 6.0f }, { "slot5dst", 54.0f }, { "slot5amt", 0.5f }
        });
        addP("SpecialFX", "SFX22 Alien Vocalizer", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "interpolationMode", 2.0f }, { "waveform", 2.0f }, { "wavetablePosition", 0.5f }, { "character", 0.85f }, { "formantStretch", 1.2f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.75f }, { "resDecay", 2.0f }, { "resInharm", 70.0f }, { "resSpread", 0.8f }, { "fx2Type", 4.0f }, { "fx2Amount", 0.5f }, { "choRate", 1.1f }, { "choDepth", 45.0f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.65f }, { "revSize", 0.85f }, { "lfo0sync", 0.0f }, { "lfo0rate", 0.6f }, { "lfo0wave", 5.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 0.3f }, { "lfo1wave", 0.0f }, { "lfo2sync", 1.0f }, { "lfo2rateSync", 6.0f }, { "lfo2wave", 1.0f }, { "env0attack", 0.2f }, { "env0decay", 0.8f }, { "env0sustain", 0.3f }, { "env0release", 0.4f }, { "env0loop", 1.0f }, { "slot0src", 1.0f }, { "slot0dst", 4.0f }, { "slot0amt", 0.6f }, { "slot1src", 2.0f }, { "slot1dst", 5.0f }, { "slot1amt", 0.5f }, { "slot2src", 3.0f }, { "slot2dst", 54.0f }, { "slot2amt", 0.4f }, { "slot3src", 4.0f }, { "slot3dst", 38.0f }, { "slot3amt", 0.75f }, { "slot4src", 8.0f }, { "slot4dst", 36.0f }, { "slot4amt", 0.8f }, { "slot5src", 6.0f }, { "slot5dst", 1.0f }, { "slot5amt", 0.4f }
        });
        addP("SpecialFX", "SFX23 Cyber Swarm Hive", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 2.0f }, { "detune", 120.0f }, { "detuneMode", 3.0f }, { "waveform", 0.0f }, { "character", 0.8f }, { "stereoWidth", 0.9f }, { "fx1Type", 3.0f }, { "fx1Amount", 0.75f }, { "gatePattern", 14.0f }, { "gateDecay", 0.04f }, { "fx2Type", 4.0f }, { "fx2Amount", 0.6f }, { "choRate", 2.5f }, { "choDepth", 60.0f }, { "choWidth", 100.0f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.6f }, { "revSize", 0.8f }, { "lfo0sync", 0.0f }, { "lfo0rate", 4.5f }, { "lfo0wave", 4.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 0.8f }, { "lfo1wave", 0.0f }, { "lfo2sync", 1.0f }, { "lfo2rateSync", 7.0f }, { "lfo2wave", 1.0f }, { "env0attack", 0.01f }, { "env0decay", 0.2f }, { "env0sustain", 0.0f }, { "env0release", 0.1f }, { "slot0src", 1.0f }, { "slot0dst", 20.0f }, { "slot0amt", 0.6f }, { "slot1src", 2.0f }, { "slot1dst", 28.0f }, { "slot1amt", 0.6f }, { "slot2src", 3.0f }, { "slot2dst", 27.0f }, { "slot2amt", 0.2f }, { "slot3src", 4.0f }, { "slot3dst", 31.0f }, { "slot3amt", 0.8f }, { "slot4src", 8.0f }, { "slot4dst", 9.0f }, { "slot4amt", 0.5f }, { "slot5src", 7.0f }, { "slot5dst", 4.0f }, { "slot5amt", 0.4f }
        });
        addP("SpecialFX", "SFX24 Quantum Freeze Matrix", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "formantFreeze", 1.0f }, { "waveform", 0.0f }, { "detune", 20.0f }, { "character", 0.7f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.7f }, { "resDecay", 2.5f }, { "resShimmer", 75.0f }, { "resSpread", 0.7f }, { "fx2Type", 5.0f }, { "fx2Amount", 0.75f }, { "revSize", 0.95f }, { "revPredelay", 40.0f }, { "fx3Type", 4.0f }, { "fx3Amount", 0.5f }, { "choRate", 0.5f }, { "choDepth", 45.0f }, { "lfo0sync", 0.0f }, { "lfo0rate", 0.2f }, { "lfo0wave", 0.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 0.1f }, { "lfo1wave", 1.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 3.2f }, { "lfo2wave", 5.0f }, { "env0attack", 2.0f }, { "env0decay", 2.0f }, { "env0sustain", 0.6f }, { "env0release", 2.5f }, { "slot0src", 1.0f }, { "slot0dst", 4.0f }, { "slot0amt", 0.2f }, { "slot1src", 2.0f }, { "slot1dst", 47.0f }, { "slot1amt", 0.4f }, { "slot2src", 3.0f }, { "slot2dst", 55.0f }, { "slot2amt", 0.6f }, { "slot3src", 4.0f }, { "slot3dst", 36.0f }, { "slot3amt", 0.75f }, { "slot4src", 8.0f }, { "slot4dst", 50.0f }, { "slot4amt", 0.6f }, { "slot5src", 6.0f }, { "slot5dst", 15.0f }, { "slot5amt", 0.3f }
        });
        addP("SpecialFX", "SFX25 Talking Bass Chopper", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 1.0f }, { "frameRate", 3.0f }, { "waveform", 0.0f }, { "bendAmt", 0.4f }, { "basePitch", 55.0f }, { "character", 0.85f }, { "fx1Type", 3.0f }, { "fx1Amount", 0.9f }, { "gatePattern", 1.0f }, { "gateDecay", 0.03f }, { "gateVowel", 0.6f }, { "fx2Type", 2.0f }, { "fx2Amount", 0.8f }, { "drvDrive", 5.5f }, { "drvLow", 0.85f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.4f }, { "revSize", 0.6f }, { "lfo0sync", 1.0f }, { "lfo0rateSync", 6.0f }, { "lfo0wave", 2.0f }, { "lfo1sync", 1.0f }, { "lfo1rateSync", 9.0f }, { "lfo1wave", 3.0f }, { "lfo2sync", 1.0f }, { "lfo2rateSync", 3.0f }, { "lfo2wave", 1.0f }, { "env0attack", 0.01f }, { "env0decay", 0.12f }, { "env0sustain", 0.0f }, { "env0release", 0.05f }, { "slot0src", 1.0f }, { "slot0dst", 32.0f }, { "slot0amt", 0.8f }, { "slot1src", 2.0f }, { "slot1dst", 31.0f }, { "slot1amt", 0.9f }, { "slot2src", 3.0f }, { "slot2dst", 21.0f }, { "slot2amt", 0.7f }, { "slot3src", 4.0f }, { "slot3dst", 29.0f }, { "slot3amt", 0.7f }, { "slot4src", 8.0f }, { "slot4dst", 4.0f }, { "slot4amt", 0.5f }, { "slot5src", 6.0f }, { "slot5dst", 18.0f }, { "slot5amt", 0.4f }
        });
        addP("SpecialFX", "SFX26 Dark Matter Drone", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "basePitch", 40.0f }, { "waveform", 0.0f }, { "detune", 45.0f }, { "character", 0.6f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.75f }, { "resDecay", 3.5f }, { "resDamp", 50.0f }, { "resInharm", 60.0f }, { "fx2Type", 5.0f }, { "fx2Amount", 0.8f }, { "revSize", 0.98f }, { "revDamp", 60.0f }, { "fx3Type", 2.0f }, { "fx3Amount", 0.55f }, { "drvDrive", 3.5f }, { "drvLow", 0.9f }, { "lfo0sync", 0.0f }, { "lfo0rate", 0.06f }, { "lfo0wave", 0.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 0.14f }, { "lfo1wave", 1.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 0.3f }, { "lfo2wave", 5.0f }, { "env0attack", 4.0f }, { "env0decay", 3.0f }, { "env0sustain", 0.8f }, { "env0release", 4.0f }, { "slot0src", 1.0f }, { "slot0dst", 27.0f }, { "slot0amt", 0.15f }, { "slot1src", 2.0f }, { "slot1dst", 37.0f }, { "slot1amt", 0.4f }, { "slot2src", 3.0f }, { "slot2dst", 50.0f }, { "slot2amt", 0.4f }, { "slot3src", 4.0f }, { "slot3dst", 47.0f }, { "slot3amt", 0.5f }, { "slot4src", 8.0f }, { "slot4dst", 38.0f }, { "slot4amt", 0.85f }, { "slot5src", 6.0f }, { "slot5dst", 1.0f }, { "slot5amt", 0.4f }
        });
        addP("SpecialFX", "SFX27 Glitchy Speak & Spell", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 0.0f }, { "frameRate", 1.0f }, { "lpcQuantBits", 4.0f }, { "waveform", 1.0f }, { "pulseWidth", 30.0f }, { "character", 0.95f }, { "fx1Type", 3.0f }, { "fx1Amount", 0.85f }, { "gatePattern", 21.0f }, { "gateDecay", 0.02f }, { "fx2Type", 2.0f }, { "fx2Amount", 0.7f }, { "drvDrive", 4.0f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.45f }, { "revSize", 0.6f }, { "lfo0sync", 1.0f }, { "lfo0rateSync", 7.0f }, { "lfo0wave", 4.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 12.0f }, { "lfo1wave", 3.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 0.8f }, { "lfo2wave", 5.0f }, { "env1attack", 0.005f }, { "env1decay", 0.03f }, { "env1sustain", 0.0f }, { "env1release", 0.02f }, { "env1loop", 1.0f }, { "slot0src", 1.0f }, { "slot0dst", 27.0f }, { "slot0amt", 0.65f }, { "slot1src", 2.0f }, { "slot1dst", 31.0f }, { "slot1amt", 1.0f }, { "slot2src", 3.0f }, { "slot2dst", 4.0f }, { "slot2amt", 0.5f }, { "slot3src", 5.0f }, { "slot3dst", 29.0f }, { "slot3amt", 0.6f }, { "slot4src", 8.0f }, { "slot4dst", 9.0f }, { "slot4amt", 0.6f }, { "slot5src", 6.0f }, { "slot5dst", 18.0f }, { "slot5amt", 0.5f }
        });
        addP("SpecialFX", "SFX28 Cyber Shifter Siren", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 2.0f }, { "waveform", 0.0f }, { "detune", 25.0f }, { "character", 0.85f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.75f }, { "resDecay", 1.5f }, { "resShift", 0.0f }, { "resSpread", 0.7f }, { "fx2Type", 5.0f }, { "fx2Amount", 0.6f }, { "revSize", 0.8f }, { "fx3Type", 2.0f }, { "fx3Amount", 0.65f }, { "drvDrive", 4.0f }, { "lfo0sync", 1.0f }, { "lfo0rateSync", 1.0f }, { "lfo0wave", 2.0f }, { "lfo1sync", 1.0f }, { "lfo1rateSync", 6.0f }, { "lfo1wave", 1.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 8.0f }, { "lfo2wave", 0.0f }, { "env0attack", 0.05f }, { "env0decay", 0.5f }, { "env0sustain", 0.2f }, { "env0release", 0.2f }, { "env0loop", 1.0f }, { "slot0src", 1.0f }, { "slot0dst", 27.0f }, { "slot0amt", 1.0f }, { "slot1src", 2.0f }, { "slot1dst", 54.0f }, { "slot1amt", 0.5f }, { "slot2src", 3.0f }, { "slot2dst", 28.0f }, { "slot2amt", 0.6f }, { "slot3src", 4.0f }, { "slot3dst", 29.0f }, { "slot3amt", 0.5f }, { "slot4src", 8.0f }, { "slot4dst", 47.0f }, { "slot4amt", 0.6f }, { "slot5src", 6.0f }, { "slot5dst", 10.0f }, { "slot5amt", 0.5f }
        });
        addP("SpecialFX", "SFX29 Neural Net Hallucination", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "interpolationMode", 1.0f }, { "waveform", 2.0f }, { "wavetablePosition", 0.3f }, { "character", 0.8f }, { "air", 60.0f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.7f }, { "resDecay", 2.2f }, { "resShimmer", 80.0f }, { "fx2Type", 4.0f }, { "fx2Amount", 0.6f }, { "choRate", 0.8f }, { "choDepth", 60.0f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.75f }, { "revSize", 0.95f }, { "lfo0sync", 0.0f }, { "lfo0rate", 0.18f }, { "lfo0wave", 0.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 0.32f }, { "lfo1wave", 1.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 0.09f }, { "lfo2wave", 5.0f }, { "env0attack", 1.5f }, { "env0decay", 2.0f }, { "env0sustain", 0.4f }, { "env0release", 2.0f }, { "slot0src", 1.0f }, { "slot0dst", 4.0f }, { "slot0amt", 0.35f }, { "slot1src", 2.0f }, { "slot1dst", 5.0f }, { "slot1amt", 0.45f }, { "slot2src", 3.0f }, { "slot2dst", 36.0f }, { "slot2amt", 0.7f }, { "slot3src", 4.0f }, { "slot3dst", 40.0f }, { "slot3amt", 0.6f }, { "slot4src", 8.0f }, { "slot4dst", 47.0f }, { "slot4amt", 0.7f }, { "slot5src", 6.0f }, { "slot5dst", 52.0f }, { "slot5amt", 0.4f }
        });
        addP("SpecialFX", "SFX30 Warp Drive Engagement", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 2.0f }, { "waveform", 1.0f }, { "pulseWidth", 50.0f }, { "syncAmt", 0.2f }, { "character", 0.9f }, { "fx1Type", 3.0f }, { "fx1Amount", 0.85f }, { "gatePattern", 7.0f }, { "gateDecay", 0.04f }, { "fx2Type", 2.0f }, { "fx2Amount", 0.75f }, { "drvDrive", 5.0f }, { "drvLow", 0.85f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.5f }, { "revSize", 0.7f }, { "env0attack", 0.1f }, { "env0decay", 1.8f }, { "env0sustain", 0.2f }, { "env0release", 0.5f }, { "lfo0sync", 1.0f }, { "lfo0rateSync", 9.0f }, { "lfo0wave", 2.0f }, { "lfo1sync", 1.0f }, { "lfo1rateSync", 12.0f }, { "lfo1wave", 4.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 6.0f }, { "lfo2wave", 1.0f }, { "slot0src", 4.0f }, { "slot0dst", 23.0f }, { "slot0amt", 0.95f }, { "slot1src", 1.0f }, { "slot1dst", 18.0f }, { "slot1amt", 0.5f }, { "slot2src", 2.0f }, { "slot2dst", 27.0f }, { "slot2amt", 0.3f }, { "slot3src", 3.0f }, { "slot3dst", 53.0f }, { "slot3amt", 0.6f }, { "slot4src", 8.0f }, { "slot4dst", 29.0f }, { "slot4amt", 0.7f }, { "slot5src", 6.0f }, { "slot5dst", 28.0f }, { "slot5amt", 0.8f }
        });
        addP("SpecialFX", "SFX31 Holographic Choir", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "detune", 45.0f }, { "stereoWidth", 0.95f }, { "waveform", 0.0f }, { "character", 0.7f }, { "fx1Type", 4.0f }, { "fx1Amount", 0.7f }, { "choRate", 0.9f }, { "choDepth", 75.0f }, { "choWidth", 100.0f }, { "fx2Type", 5.0f }, { "fx2Amount", 0.8f }, { "revSize", 0.95f }, { "revPredelay", 50.0f }, { "fx3Type", 1.0f }, { "fx3Amount", 0.5f }, { "resDecay", 1.8f }, { "resShimmer", 60.0f }, { "lfo0sync", 0.0f }, { "lfo0rate", 0.25f }, { "lfo0wave", 0.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 0.4f }, { "lfo1wave", 1.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 0.15f }, { "lfo2wave", 0.0f }, { "env0attack", 1.0f }, { "env0decay", 2.0f }, { "env0sustain", 0.6f }, { "env0release", 2.0f }, { "slot0src", 1.0f }, { "slot0dst", 4.0f }, { "slot0amt", 0.18f }, { "slot1src", 2.0f }, { "slot1dst", 20.0f }, { "slot1amt", 0.35f }, { "slot2src", 3.0f }, { "slot2dst", 39.0f }, { "slot2amt", 0.4f }, { "slot3src", 4.0f }, { "slot3dst", 49.0f }, { "slot3amt", 0.5f }, { "slot4src", 8.0f }, { "slot4dst", 40.0f }, { "slot4amt", 0.6f }, { "slot5src", 6.0f }, { "slot5dst", 47.0f }, { "slot5amt", 0.35f }
        });
        addP("SpecialFX", "SFX32 Subterranean Echoes", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "frameRate", 2.0f }, { "waveform", 0.0f }, { "basePitch", 48.0f }, { "character", 0.65f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.8f }, { "resDecay", 3.2f }, { "resDamp", 45.0f }, { "resSpread", 0.8f }, { "fx2Type", 4.0f }, { "fx2Amount", 0.6f }, { "choRate", 0.3f }, { "choDepth", 90.0f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.75f }, { "revSize", 0.95f }, { "env0attack", 0.01f }, { "env0decay", 3.0f }, { "env0sustain", 0.1f }, { "env0release", 2.0f }, { "lfo0sync", 0.0f }, { "lfo0rate", 0.1f }, { "lfo0wave", 0.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 0.25f }, { "lfo1wave", 1.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 0.05f }, { "lfo2wave", 5.0f }, { "slot0src", 4.0f }, { "slot0dst", 35.0f }, { "slot0amt", 0.9f }, { "slot1src", 1.0f }, { "slot1dst", 40.0f }, { "slot1amt", 0.6f }, { "slot2src", 2.0f }, { "slot2dst", 4.0f }, { "slot2amt", 0.25f }, { "slot3src", 3.0f }, { "slot3dst", 37.0f }, { "slot3amt", 0.45f }, { "slot4src", 8.0f }, { "slot4dst", 47.0f }, { "slot4amt", 0.7f }, { "slot5src", 7.0f }, { "slot5dst", 54.0f }, { "slot5amt", 0.5f }
        });
        addP("SpecialFX", "SFX33 Cybernetic Insect", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 0.0f }, { "lpcQuantBits", 3.0f }, { "waveform", 1.0f }, { "pulseWidth", 50.0f }, { "character", 0.9f }, { "fx1Type", 2.0f }, { "fx1Amount", 0.75f }, { "drvDrive", 5.0f }, { "fx2Type", 3.0f }, { "fx2Amount", 0.85f }, { "gatePattern", 31.0f }, { "gateDecay", 0.03f }, { "fx3Type", 4.0f }, { "fx3Amount", 0.5f }, { "choRate", 3.0f }, { "choDepth", 45.0f }, { "lfo0sync", 0.0f }, { "lfo0rate", 42.0f }, { "lfo0wave", 0.0f }, { "lfo1sync", 1.0f }, { "lfo1rateSync", 6.0f }, { "lfo1wave", 2.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 1.5f }, { "lfo2wave", 5.0f }, { "env0attack", 0.01f }, { "env0decay", 0.08f }, { "env0sustain", 0.0f }, { "env0release", 0.05f }, { "env0loop", 1.0f }, { "slot0src", 1.0f }, { "slot0dst", 18.0f }, { "slot0amt", 0.65f }, { "slot1src", 2.0f }, { "slot1dst", 27.0f }, { "slot1amt", 0.38f }, { "slot2src", 3.0f }, { "slot2dst", 28.0f }, { "slot2amt", 0.7f }, { "slot3src", 4.0f }, { "slot3dst", 29.0f }, { "slot3amt", 0.6f }, { "slot4src", 8.0f }, { "slot4dst", 9.0f }, { "slot4amt", 0.55f }, { "slot5src", 6.0f }, { "slot5dst", 4.0f }, { "slot5amt", 0.5f }
        });
        addP("SpecialFX", "SFX34 Android Ghost Whispers", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "noise", 75.0f }, { "air", 90.0f }, { "noiseColor", 4500.0f }, { "waveform", 0.0f }, { "character", 0.8f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.75f }, { "resDecay", 2.8f }, { "resShimmer", 85.0f }, { "resSpread", 0.9f }, { "fx2Type", 5.0f }, { "fx2Amount", 0.8f }, { "revSize", 0.98f }, { "revPredelay", 40.0f }, { "fx3Type", 4.0f }, { "fx3Amount", 0.5f }, { "choRate", 0.6f }, { "choDepth", 60.0f }, { "lfo0sync", 0.0f }, { "lfo0rate", 0.2f }, { "lfo0wave", 0.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 0.12f }, { "lfo1wave", 1.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 0.08f }, { "lfo2wave", 5.0f }, { "env0attack", 2.5f }, { "env0decay", 2.5f }, { "env0sustain", 0.6f }, { "env0release", 3.0f }, { "slot0src", 1.0f }, { "slot0dst", 4.0f }, { "slot0amt", 0.3f }, { "slot1src", 2.0f }, { "slot1dst", 8.0f }, { "slot1amt", 0.55f }, { "slot2src", 3.0f }, { "slot2dst", 36.0f }, { "slot2amt", 0.75f }, { "slot3src", 4.0f }, { "slot3dst", 47.0f }, { "slot3amt", 0.6f }, { "slot4src", 8.0f }, { "slot4dst", 35.0f }, { "slot4amt", 0.8f }, { "slot5src", 6.0f }, { "slot5dst", 52.0f }, { "slot5amt", 0.35f }
        });
        addP("SpecialFX", "SFX35 Laser Harpsichord", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 2.0f }, { "waveform", 0.0f }, { "character", 0.95f }, { "attack", 0.001f }, { "decay", 0.3f }, { "sustain", 0.0f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.8f }, { "resDecay", 1.4f }, { "resInharm", 45.0f }, { "resSpread", 0.7f }, { "fx2Type", 4.0f }, { "fx2Amount", 0.55f }, { "choRate", 1.5f }, { "choDepth", 45.0f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.55f }, { "revSize", 0.75f }, { "env0attack", 0.001f }, { "env0decay", 0.15f }, { "env0sustain", 0.0f }, { "env0release", 0.05f }, { "lfo0sync", 1.0f }, { "lfo0rateSync", 9.0f }, { "lfo0wave", 4.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 0.5f }, { "lfo1wave", 1.0f }, { "lfo2sync", 1.0f }, { "lfo2rateSync", 7.0f }, { "lfo2wave", 0.0f }, { "slot0src", 4.0f }, { "slot0dst", 27.0f }, { "slot0amt", 0.8f }, { "slot0uni", 1.0f }, { "slot1src", 1.0f }, { "slot1dst", 54.0f }, { "slot1amt", 0.5f }, { "slot2src", 2.0f }, { "slot2dst", 38.0f }, { "slot2amt", 0.6f }, { "slot3src", 3.0f }, { "slot3dst", 40.0f }, { "slot3amt", 0.4f }, { "slot4src", 6.0f }, { "slot4dst", 35.0f }, { "slot4amt", 0.55f }, { "slot5src", 8.0f }, { "slot5dst", 40.0f }, { "slot5amt", 0.6f }
        });
        addP("SpecialFX", "SFX36 Bionic Heartbeat", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 1.0f }, { "basePitch", 45.0f }, { "waveform", 1.0f }, { "pulseWidth", 20.0f }, { "character", 0.7f }, { "fx1Type", 3.0f }, { "fx1Amount", 0.95f }, { "gatePattern", 35.0f }, { "gateDecay", 0.05f }, { "fx2Type", 2.0f }, { "fx2Amount", 0.6f }, { "drvDrive", 3.5f }, { "drvLow", 0.9f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.6f }, { "revSize", 0.8f }, { "lfo0sync", 1.0f }, { "lfo0rateSync", 3.0f }, { "lfo0wave", 3.0f }, { "env0attack", 0.005f }, { "env0decay", 0.3f }, { "env0sustain", 0.0f }, { "env0release", 0.1f }, { "env0loop", 1.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 0.3f }, { "lfo1wave", 1.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 1.2f }, { "lfo2wave", 0.0f }, { "slot0src", 1.0f }, { "slot0dst", 31.0f }, { "slot0amt", 1.0f }, { "slot1src", 4.0f }, { "slot1dst", 27.0f }, { "slot1amt", 0.5f }, { "slot2src", 2.0f }, { "slot2dst", 53.0f }, { "slot2amt", 0.6f }, { "slot3src", 3.0f }, { "slot3dst", 29.0f }, { "slot3amt", 0.4f }, { "slot4src", 8.0f }, { "slot4dst", 47.0f }, { "slot4amt", 0.4f }, { "slot5src", 6.0f }, { "slot5dst", 35.0f }, { "slot5amt", 0.6f }
        });
        addP("SpecialFX", "SFX37 Solar Flare Radiance", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "air", 85.0f }, { "noise", 50.0f }, { "noiseColor", 5000.0f }, { "waveform", 0.0f }, { "character", 0.8f }, { "fx1Type", 4.0f }, { "fx1Amount", 0.6f }, { "choRate", 0.7f }, { "choDepth", 60.0f }, { "choWidth", 100.0f }, { "fx2Type", 5.0f }, { "fx2Amount", 0.75f }, { "revSize", 0.95f }, { "fx3Type", 2.0f }, { "fx3Amount", 0.4f }, { "drvDrive", 2.5f }, { "lfo0sync", 0.0f }, { "lfo0rate", 0.35f }, { "lfo0wave", 0.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 0.15f }, { "lfo1wave", 5.0f }, { "lfo2sync", 1.0f }, { "lfo2rateSync", 3.0f }, { "lfo2wave", 1.0f }, { "env0attack", 1.8f }, { "env0decay", 2.0f }, { "env0sustain", 0.5f }, { "env0release", 2.0f }, { "env0loop", 1.0f }, { "slot0src", 1.0f }, { "slot0dst", 52.0f }, { "slot0amt", 0.5f }, { "slot1src", 2.0f }, { "slot1dst", 8.0f }, { "slot1amt", 0.6f }, { "slot2src", 3.0f }, { "slot2dst", 41.0f }, { "slot2amt", 0.5f }, { "slot3src", 4.0f }, { "slot3dst", 5.0f }, { "slot3amt", 0.45f }, { "slot4src", 8.0f }, { "slot4dst", 47.0f }, { "slot4amt", 0.7f }, { "slot5src", 7.0f }, { "slot5dst", 4.0f }, { "slot5amt", 0.4f }
        });
        addP("SpecialFX", "SFX38 Cyberpunk Bass Chug", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 1.0f }, { "waveform", 0.0f }, { "bendAmt", 0.5f }, { "basePitch", 50.0f }, { "character", 0.9f }, { "fx1Type", 3.0f }, { "fx1Amount", 0.85f }, { "gatePattern", 1.0f }, { "gateDecay", 0.035f }, { "fx2Type", 2.0f }, { "fx2Amount", 0.8f }, { "drvDrive", 5.5f }, { "drvLow", 0.85f }, { "fx3Type", 4.0f }, { "fx3Amount", 0.4f }, { "choRate", 1.5f }, { "choDepth", 30.0f }, { "lfo0sync", 1.0f }, { "lfo0rateSync", 9.0f }, { "lfo0wave", 2.0f }, { "lfo1sync", 1.0f }, { "lfo1rateSync", 9.0f }, { "lfo1wave", 3.0f }, { "lfo2sync", 1.0f }, { "lfo2rateSync", 6.0f }, { "lfo2wave", 1.0f }, { "env0attack", 0.01f }, { "env0decay", 0.08f }, { "env0sustain", 0.0f }, { "env0release", 0.05f }, { "slot0src", 1.0f }, { "slot0dst", 21.0f }, { "slot0amt", 0.85f }, { "slot1src", 2.0f }, { "slot1dst", 31.0f }, { "slot1amt", 0.85f }, { "slot2src", 3.0f }, { "slot2dst", 4.0f }, { "slot2amt", 0.35f }, { "slot3src", 4.0f }, { "slot3dst", 29.0f }, { "slot3amt", 0.75f }, { "slot4src", 8.0f }, { "slot4dst", 18.0f }, { "slot4amt", 0.5f }, { "slot5src", 6.0f }, { "slot5dst", 27.0f }, { "slot5amt", -0.3f }
        });
        addP("SpecialFX", "SFX39 Astral Projection", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 3.0f }, { "interpolationMode", 2.0f }, { "waveform", 0.0f }, { "stereoWidth", 1.0f }, { "character", 0.75f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.8f }, { "resDecay", 2.6f }, { "resShimmer", 85.0f }, { "resSpread", 0.9f }, { "fx2Type", 4.0f }, { "fx2Amount", 0.6f }, { "choRate", 0.5f }, { "choDepth", 75.0f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.8f }, { "revSize", 0.96f }, { "lfo0sync", 0.0f }, { "lfo0rate", 0.12f }, { "lfo0wave", 0.0f }, { "lfo1sync", 0.0f }, { "lfo1rate", 0.08f }, { "lfo1wave", 1.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 0.2f }, { "lfo2wave", 0.0f }, { "env0attack", 3.0f }, { "env0decay", 3.0f }, { "env0sustain", 0.6f }, { "env0release", 3.0f }, { "slot0src", 1.0f }, { "slot0dst", 4.0f }, { "slot0amt", 0.42f }, { "slot1src", 2.0f }, { "slot1dst", 54.0f }, { "slot1amt", 0.3f }, { "slot2src", 3.0f }, { "slot2dst", 40.0f }, { "slot2amt", 0.6f }, { "slot3src", 4.0f }, { "slot3dst", 36.0f }, { "slot3amt", 0.85f }, { "slot4src", 8.0f }, { "slot4dst", 47.0f }, { "slot4amt", 0.4f }, { "slot5src", 6.0f }, { "slot5dst", 50.0f }, { "slot5amt", 0.4f }
        });
        addP("SpecialFX", "SFX40 Tachyon Particle Beam", {
            { "vocoderMode", 1.0f }, { "mode", 1.0f }, { "lpcOrder", 2.0f }, { "waveform", 1.0f }, { "pulseWidth", 40.0f }, { "character", 0.9f }, { "fx1Type", 1.0f }, { "fx1Amount", 0.85f }, { "resDecay", 1.2f }, { "resInharm", 70.0f }, { "resShift", 12.0f }, { "fx2Type", 2.0f }, { "fx2Amount", 0.7f }, { "drvDrive", 5.0f }, { "fx3Type", 5.0f }, { "fx3Amount", 0.45f }, { "revSize", 0.65f }, { "lfo0sync", 1.0f }, { "lfo0rateSync", 6.0f }, { "lfo0wave", 2.0f }, { "lfo1sync", 1.0f }, { "lfo1rateSync", 9.0f }, { "lfo1wave", 4.0f }, { "lfo2sync", 0.0f }, { "lfo2rate", 16.0f }, { "lfo2wave", 0.0f }, { "env0attack", 0.005f }, { "env0decay", 0.04f }, { "env0sustain", 0.0f }, { "env0release", 0.02f }, { "env0loop", 1.0f }, { "slot0src", 1.0f }, { "slot0dst", 27.0f }, { "slot0amt", 0.75f }, { "slot1src", 2.0f }, { "slot1dst", 54.0f }, { "slot1amt", 0.6f }, { "slot2src", 3.0f }, { "slot2dst", 18.0f }, { "slot2amt", 0.5f }, { "slot3src", 4.0f }, { "slot3dst", 29.0f }, { "slot3amt", 0.6f }, { "slot4src", 8.0f }, { "slot4dst", 56.0f }, { "slot4amt", 0.7f }, { "slot5src", 6.0f }, { "slot5dst", 38.0f }, { "slot5amt", 0.7f }
        });

        return list;
    }
};
