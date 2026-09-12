# SPECTRA 8 — Quick Manual (EN)

A hybrid vocoder with two switchable engines: a classic **Filterbank** vocoder and an **LPC** vocal-tract model.

---

## 1. Getting Started

1. Insert SPECTRA 8 on the track carrying your **voice** (the modulator).
2. Choose the carrier source:
   * **Auto mode** — the plugin generates its own carrier and follows your voice's pitch.
   * **MIDI mode** — you play the carrier pitch from a MIDI keyboard.
3. Turn **MIX** up. At 0 % you hear the untouched dry signal (the vocoder *and* the whole FX chain are bypassed); at 100 % you hear the fully processed sound.

> **No sound?** In Auto mode the plugin gates itself when no input is detected. Check that audio is actually reaching the track, and that MIX is above 0 %.

---

## 2. The Five Tabs & UI Resizing

| Tab | What it does |
|---|---|
| **VOCODER** | Engine choice and the main voice controls |
| **EXCITATION** | The carrier oscillator (what the voice is imposed onto) |
| **MOD MATRIX** | LFOs and envelopes routed to any knob (56 destinations) |
| **FX** | Five reorderable effect slots ported from COLORS |
| **BANDS EQ** | Draggable band EQ with a spectrum analyzer |

> **Window Resizing:** Drag from any corner or edge to freely scale the interface from 25% up to 200% with a fixed aspect ratio. Your window size is automatically remembered across DAW sessions.

---

## 3. VOCODER Tab

### Choosing an engine

* **Filterbank** — Splits the voice into 8–48 bands and imposes their levels onto the carrier. Crisp and articulate. This is the classic vocoder sound.
* **LPC Mode** — Models the vocal tract as a filter and rebuilds it. More natural formants, and the LPC-only controls let you go all the way to 8-bit robot speech.

Switching between them uses a 30 ms equal-power crossfade, so it is safe to do while audio is playing.

### Core controls

| Knob | What to use it for |
|---|---|
| **CHARACTER** | Intelligibility. Higher = sharper, more articulate consonants |
| **TRACKING** | 0 % = carrier stays at BASE PITCH. 100 % = carrier follows your voice's pitch |
| **PITCH Q** | Auto-tune amount. Set Key and Scale in the combo boxes on the left |
| **FMT SHIFT** | Moves formants without changing pitch. Negative = larger/deeper, positive = smaller/brighter |
| **FMT STRETCH** | Spreads or compresses formant spacing. Subtler than SHIFT, useful for gender morphing |
| **NOISE MIX** | Bipolar. Negative removes breath/consonant noise, 0 follows the voice automatically, positive forces noise |
| **M.PITCH** | Transposes the whole carrier ±24 semitones, *before* scale snapping |
| **MIX** | Dry ⇔ processed. 0 % is a true bypass |

### LPC-only controls

| Control | Effect |
|---|---|
| **ORDER** | Vocal tract detail. 16 = natural, 8 = coarse and noticeably louder |
| **FRAME RATE** | How often the tract is re-analyzed. 50 Hz = smooth, 8 Hz = choppy and toy-like |
| **K QUANT** | Bit-crushes the tract model itself. 3-bit is full BitSpeek territory |
| **INTERP** | How frames are blended. Step = abrupt (retro), LSP/LAR = smooth |
| **FREEZE** | Holds the current vowel shape indefinitely |

---

## 4. EXCITATION Tab

This is the carrier — the raw tone the voice is printed onto. A brighter, harmonically rich carrier gives a more intelligible vocoder.

* **Waveform** — Sawtooth (richest, best default), Pulse (thinner, adjustable width), Wavetable.
* **Custom wavetables** — Press **ADD DIR** once to register a folder. **BROWSE** then shows sub-folders on the left and waveforms on the right; click any file to load it instantly. **RANDOM** picks one at random. The folder is remembered permanently, including across DAW restarts.
* **DETUNE** — Unison spread, shown in cents with the interval name (e.g. `700 ct (P5)`). **SNAP** locks dragging to whole semitones.
* **Morph** — Three shapers that can all run at once:
  * **BEND / BEND SYM** — bends the waveform's phase. Adds odd harmonics.
  * **SYNC / SYNC PH** — hard-sync-style repetition. Aggressive and metallic.
  * **VOCODE / VOWEL** — imposes a fixed A-I-U-E-O formant on the carrier.
  * Any Amount knob at 0 bypasses that stage. The waveform display shows all three combined.
* **LOFI** — Bit reduction plus pitch-tracked sample & hold. Gets grainy without dropping in pitch.

---

## 5. MOD MATRIX Tab

Three LFOs and two loopable envelopes, routed through six slots.

1. Pick a **Source** (LFO 1–3, ENV 1–2, Velocity, Note, Mod Wheel, Random).
2. Pick a **Destination** — **56 destinations** across the VOCODER, EXCITATION, and FX tabs.
3. Set **AMT** (bipolar) and choose **UNI** for a one-directional sweep.

Modulated knobs persistently draw a **pink arc band** showing the reachable range, plus a bright dot for the instantaneous modulated value — continuously animated in real time even when idle. LFOs can be free-running or tempo-synced across 13 note divisions.

> **Try this:**
> - LFO 1 (Saw) → **Master Pitch**, AMT around 0.5, with PITCH Q at 100 % and a Key/Scale chosen. The sound walks up the scale in time with the track.
> - LFO 2 (S&H) → **Resonator Shift**, AMT 0.5. The resonator hops across pitch intervals in stepped arpeggios.
> - ENV 1 → **Gate Decay**, AMT 0.7. Dynamically expands and contracts the gate release with key velocity.

---

## 6. FX Tab

Five slots in series ported directly from **COLORS**. **Drag any card** to change the processing order; **click** a card to edit it below.

### Spectral Resonator

Eight tuned comb resonators that ring in response to the input, featuring complete MIDI tracking.

* **MODE**
  * **Chord** — Fixed pitches from **ROOT** (a note name) and **CHORD** type.
  * **MIDI** — Resonator pitches follow the notes you hold. Fewer than 8 notes are stacked into upper octaves. Releasing keys holds the last voicing without cutting out.
  * **Free** — Delay time in milliseconds instead of a pitch. Metallic flanging and comb tones.
* **SHIFT** — **Semitone transposition (±24 st)**. Can be modulated via the ModMatrix for arpeggios and pitch bends.
* **SPREAD** — Stereo pan distribution across the 8 resonator lines (0–100%).
* **OUT GAIN** — Output volume trim (-12 to +12 dB).
* **DECAY** — How long the resonance rings, in seconds. Pitch-compensated for uniform decay from low to high pitches.
* **DAMP** — Rolls off the highs as it decays. Higher = darker, more muted tail.
* **SHIMMER** — Adds octave-up and two-octave-up sparkle exclusively in parallel to the output without feedback instability.
* **INHARM** — Detunes the upper partials the way a real bell does (`f_n /= √(1+B·n²)`). Adds beating and metallic chime.

### Multiband Drive

Splits at 300 Hz and 2.5 kHz and distorts each band separately, so the low end stays solid while the mids get dirty. Shapes: **Tanh** (smooth), **Fold** (aggressive), **Crush** (digital).

### Formant Gate

16-step tempo-synced rhythm slicer ported from COLORS with per-step vowel modulations.

* **RATE** — Step length, from 1/2 down to 1/32.
* **PATTERN** — **50 rhythm patterns** (classic trance, triplets, dotted polyrhythms, stutter glitches, swing cuts, and chiseled riffs).
* **DECAY** — **Gate release envelope (5–1000 ms)**. Short values yield tight ColorBass plucks; longer values give breathing vocal swells.
* **VOWEL** — How strongly the per-step vowel formant filter is applied.
* **SMOOTH** — Softens the gate edges.

### Ensemble Chorus / Reverb

Standard wideners. The reverb adds **PRE-DLY**, **WIDTH**, **LOW CUT** (keeps the tail from muddying the low end) and **MOD** (breaks up metallic ringing).

---

## 7. BANDS EQ Tab

* **Drag on the graph** to shape the response band by band.
* **Double-click** a band to reset it to 0 dB. **Right-click** for a reset-all confirmation.
* The coloured curve behind the EQ is the **analyzer**, showing the plugin's final output. It is a constant-Q filterbank rather than an FFT, which is why the low end reads smoothly while the highs still respond quickly.
* Band centre frequencies follow the **BANDS** count set on the VOCODER tab, so the graph always matches what the filters are actually doing.

---

## 8. Starting Points

| Sound | Settings |
|---|---|
| **Classic vocoder** | Filterbank, BANDS 32–48, TRACKING 0 %, Saw carrier, BASE PITCH set to your track's key |
| **Robot / BitSpeek** | LPC Mode, ORDER 8–10, FRAME RATE 8–15 Hz, K QUANT 3–4 bit, INTERP Step |
| **Auto-tuned vocal** | TRACKING 100 %, PITCH Q 100 %, pick Key and Scale |
| **Scale-locked melody** | Above, plus LFO → Master Pitch in the MOD MATRIX |
| **Color Bass** | FX 1 = Resonator (Chord, Minor, DECAY ~1.5 s, SHIMMER 0.4, INHARM 0.3), FX 2 = Drive |
| **EDM vocal chop** | FX Gate, RATE 1/16, DECAY 35 ms, VOWEL 0.6 |
| **Bell / metallic pad** | Resonator in MIDI mode, DECAY 6–10 s, INHARM 0.6, SHIMMER 0.5 |
| **Tricky Resonator Arp** | MOD MATRIX: LFO (S&H) → Resonator Shift with Resonator in MIDI mode |

---

## 9. 150 Factory Presets

Click the preset name in the header to browse all 150 factory presets across 8 distinct categories. Star (★) any preset to add it to your favorites.

1. **FilterBank (Auto)** — 20 presets: Natural vocal articulation, pop vocal harmonies, deep vocoded textures, sub tracking.
2. **LPC (Auto)** — 20 presets: 8-bit retro gaming robots, educational speech toys, speech IC chips, synthetic whispers.
3. **FilterBank (MIDI)** — 20 presets: Playable polyphonic chords, vocal leads, sharp synth plucks.
4. **LPC (MIDI)** — 20 presets: Expressive talking basslines, playable vocal-tract solo leads.
5. **M.Pitch Modulations** — 10 presets: Slow analog flutter, 16th sync arpeggios, pentatonic scale snapping.
6. **Rhythmic Formant Gate** — 10 presets: 50-pattern trance gates, 32nd stutter glitches, breathing envelope releases.
7. **Spectral Resonator Lab** — 10 presets: Semitone shift modulation, 85% inharmonic metallic partials, shimmer clouds.
8. **SpecialFX** — 40 presets: Extreme 6-slot modulation matrix patches across FilterBank (20) and LPC (20) engines in complete MIDI mode.

---

## 10. Troubleshooting

| Symptom | Cause |
|---|---|
| No sound in Auto mode | Input gate is closed — no signal is reaching the plugin. Check routing |
| Vocoder sounds muffled | Raise CHARACTER, or use a brighter carrier (Saw with some DETUNE) |
| Words are unintelligible | Increase BANDS, raise CHARACTER, or in LPC mode raise ORDER to 16 |
| Level jumps when changing ORDER | Expected. Lower orders let more carrier through — trim OUT LEVEL to compensate |
| Resonator rings forever | Lower **DECAY**. It sets the tail length directly |
| Gate does not sound rhythmic | Shorten **DECAY** (around 30 ms), or pick another pattern from the 50 choices |
