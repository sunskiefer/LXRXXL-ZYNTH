# The voice map: from LXRXXL BAZZLINE to LXRXXL ZYNTH

LXRXXL ZYNTH keeps LXRXXL BAZZLINE's voice and puts one of sixteen voice engines where BAZZLINE's oscillator was. The
code of the M-VAVE FM-1 firmwares (Felucca, Sloop, Baud Girl: GPL-3, which can't be combined with the LXR02 license)
and of the Dirtywave M8 (closed) is **not** used: they only inspired the choice of voices.

## 1. One track's voice

```mermaid
flowchart LR
  subgraph ENG["the VOICE (one of sixteen, src/eng*.c, src/mi_bridge.cc)"]
    direction TB
    V["V1 .. V6<br/>(the MODULES sliders)"] --> E["engine"]
    U["UNISON: a second engine,<br/>detuned by SPREAD"]
  end
  E --> PM["FM LVL x FM RATIO<br/>(a delay swung by a sine:<br/>phase modulation of any tone)"]
  U --> PM
  PM --> MIX(("mix"))
  SUB["SUB<br/>-1 oct (BAZZ: -1 / -2)<br/>SQR / SAW / TRI"] -- "SUB LVL" --> MIX
  NOISE["NOISE"] -- "NOISE LVL" --> MIX
  MIX --> SW["SWARM<br/>BBD"] --> PRE["DRIVE (PRE)"] --> FLT["FILTER<br/>Polivoks / Diode, LP / BP"] --> POST["DRIVE (POST)"] --> TONE["TONE (BAZZ)"] --> VCA["VCA<br/>ADSR"] --> OUT["LEVEL, PAN"]
```

Everything after the engine is LXRXXL BAZZLINE's (`src/voice.c`): the envelopes, accent and sweep, slide, the pitch
envelope and the modulator. The modulator's WAVE destination moves the voice's main slider (`ENG_DEFS[].timbre`).
The front's TIMBRE and MORPH move the two sliders in `ENG_MACROS[]` (`src/engines.c`). Each voice's home sound (the
chain it starts with on a track, and VOICE RESET's) is in `src/sound.c` (`HOME[]`).

## 2. The nineteen voices

| Voice | Where from | How it works |
|---|---|---|
| ANALOG | written here | two polyBLEP oscillators (saw to square, pulse width), interval and detune, or hard sync at 2x oversampling |
| TRIO | written here | three polyBLEP oscillators at their own intervals, a detune spread |
| FM6 | written here; the DX7's 32 algorithms from music-synthesizer-for-android (Apache 2.0) | six sine operators on msfa's bus model (op 6 .. op 1, two buses, feedback in and out), 16 ratio sets, a modulator envelope (BRIGHT) |
| FM4 | written here | four operators on 12 algorithms, op 4's feedback, the modulators' envelope |
| PHASE | written here | Casio CZ style phase distortion of a cosine (eight waves, three of them windowed resonances), 2x oversampled, a DCW envelope, a detuned second line |
| WAVE | Mutable Instruments Plaits (MIT) | Plaits' wavetable engine (integrated waves, differentiated: no aliasing), then a warp written here (fold, shape, crush, ring) and a scan envelope |
| CHIP | written here | pulses, the NES's 4-bit triangle, a stepped saw, 15-bit LFSR noise; bits, sample-and-hold rate, chord arpeggio, pitch sweep |
| MACRO | Mutable Instruments Braids (MIT) | Braids' macro oscillator (44 shapes), run at 88.2 kHz and brought to 44.1 kHz by a half-band filter, pitched for its 96 kHz tables; timbre and color envelopes written here |
| WHEEL | written here | nine sine tonewheels (foldback above 5.9 kHz), percussion on the 2nd or 3rd harmonic, key click, a rotary speaker (horn: Doppler and loudness; drum: loudness, each rotor with its own inertia) |
| VOICE | written here | a band-limited glottal pulse with breath through three formant band-passes (A E I O U), delayed vibrato |
| PHYS | Mutable Instruments Plaits (MIT) | Plaits' string voice (Karplus-Strong with dispersion and a curved bridge) and modal voice (resonators), with Plaits' limiter |
| GRAIN | written here | up to 20 Hann-windowed grains reading 16 single-cycle waves (three band limits by pitch), spray, pitch spread |
| NOISE | written here | white, pink (Paul Kellet's filter), crackle (a chaotic map), digital (LFSR), dust; tilt; resonators on the note and its 2nd and 3rd harmonics; bit and rate reduction |
| LXR TONE | the LXR's tables (LXR02Open) | the LXR drum voice's oscillator and FM modulator on the LXR's band-limited tables, an FM envelope, decimation |
| LXR METAL | written here, after the LXR's hi-hat | six square oscillators at metallic ratios (808 hats, cymbal, bell, gong), FM, ring modulation, noise, a band-pass above the note |
| BAZZ | LXRXXL BAZZLINE | BAZZLINE's VCO / FM / WAVE oscillator on the LXR's tables, with an octave range, the sub's octave, drift and its tone filter |
| DRONE | written here, after BAZZ | two to seven oscillators (sine to square) on an interval stack (unison, octaves, fifths, chords, cluster), detuned and drifting, their waves moving slowly, beating, the upper ones blooming in, a sub and a shimmer |
| TEXTURE | written here, after the LXR's noise and metal | noise, metallic partials and dust in one SOURCE, resonators tuned to the note, slow motion, wind, sparkles, a rumble and a short space |
| FX | written here, after the LXR voice | eight sweeping effects (laser, zap, riser, fall, siren, alarm, glitch, bomb) with repeats, chaos, wobble, crush, noise, ring and a feedback comb |

Every voice has **20 controls** (`tools/voices.py`): V1-V6 its main ones, V7-V12 its deeper ones (read by the
engine itself), V13-V20 the same eight kinds on every voice (OCTAVE, FINE, VIBRATO, a modulation envelope's ATTACK
and DECAY, the envelope to two of the voice's controls, velocity to one), applied in `src/voice.c`.

## 2b. CHAOS and SHIFT (`src/alien.c`)

- **CHAOS:** a Lorenz attractor (σ 10, ρ 28, β 8/3) integrated per block (Euler, steps of at most 0.004), blended
  (WILD) with a calm orbit of two sines; each track reads it from its own angle, smoothed over 30 ms, times the
  PANEL's depth and its MIXER amount, into PITCH, CUTOFF, TIMBRE, PAN or ALL (`voice.c`).
- **SHIFT:** a send bus; a Hilbert transformer (two chains of four first-order all-passes, Olli Niemitalo's
  coefficients, chain A one sample later) times a quadrature oscillator (a frequency shift, the other side about
  56 dB down), or a ring modulator; feedback through a soft clip; its level into the mix after the delay.

## 3. What came from LXRXXL BAZZLINE (unchanged)

The sequencer (`seq.c`), the front panel's logic (`ui.c`), the delay (`fx.c`; its gate/duck is no longer used), the mixer, the
presets (`preset.c`), the state chunk (`store.c`) and the Force integration (`engine.c`, the VST2 wrapper). The
randomizers began as LXRXXL's; in 0.2.0 SOUND RANDOM (`random.c`) is voice-aware and STEP RANDOM (`seqgen.c`) a
melodic generator (the user guide explains both).
