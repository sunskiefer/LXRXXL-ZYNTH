# Changelog

## 0.2.1
- Packed under **L'Cronx**: the plugin folder is now `L'Cronx - VST - LXRXXL ZYNTH` and MPC lists the maker as L'Cronx. Nothing else
  changed from 0.2.0. If you installed 0.2.0, run its `uninstall.sh` first (copy out anything you saved inside the plugin
  folder), then install 0.2.1. The installer handles the ' in the folder name.

## 0.2.0
- **An experimental synth:** 19 coded voices on unconventional maths. **DRONE**, **TEXTURE** and **FX** added; every
  voice has **20 controls of its own** (two rows of faders on the VOICE page: V7-V12 deeper controls per voice,
  V13-V20 OCTAVE, FINE, VIBRATO and a modulation envelope and velocity aimed at the voice's own controls) and **8
  starter sounds** (SOUND ◄ ► on the VOICE page, 152 in all).
- **CHAOS:** a Lorenz attractor moving every track (PITCH, CUTOFF, TIMBRE, PAN or ALL), each from its own angle;
  RATE, WILD, DEST on FX MOD, its depth on the PANEL, an amount per track on the MIXER.
- **SHIFT:** a frequency shifter (Hilbert transformer) or ring modulator on a send bus with feedback (the barber pole);
  FREQ, FEEDBACK, SHIFT / RING on FX MOD, its mix on the PANEL, a send per track on the MIXER.
- **The gate/duck is gone** (CHAOS and SHIFT took its place). 0.1.0 projects load with it off; every MIDI note now
  plays (none is taken as a duck trigger).
- **Four pages:** PANEL, VOICE (the voice's 20 faders, the amp and filter ADSRs, accent, FM, starter sounds, the pitch
  envelope), FX MOD (the delay, the presets, CHAOS, SHIFT, drive, filter, swarm, modulator), MIXER (8 strips with PAN,
  DLY SEND, CHAOS, SHIFT, LEVEL, MUTE, SOLO, and MASTER).
- The filter envelope is a full ADSR (FILTER SUSTAIN, FILTER RELEASE added).
- **docs/USER_GUIDE.md:** the full manual.
- Each voice keeps its character: a voice first picked on a track starts from its **home sound** (the chain set so
  the voice sounds as itself: held voices held, plucked and struck ones ringing, BAZZ as BAZZLINE's bass), and each
  track remembers **every voice's whole sound** (the sliders and the chain), not only its six sliders. VOICE RESET:
  the voice's home sound (the level, the pan and the other voices kept).
- The amp envelope is a full **ADSR**: SUSTAIN added (a new parameter, after the others). 0.1.0 projects and sounds
  play as before (VCA ENV fully down = SUSTAIN 100 %, anything else = SUSTAIN 0).
- **The front page, a performance page:** TIMBRE and MORPH (the voice's two most expressive controls, new parameters),
  ATTACK, DECAY, SUSTAIN, RELEASE, MOD RATE / MOD DEPTH (the LFO), SUB LVL, CHAOS and SHIFT; a SUB legend on the SQR /
  SAW / TRI switch. DETUNE, FM LVL, NOISE LVL and ENV/CV LVL moved to the other pages.
- **STEP RANDOM** is melodic: a harmony per pattern (key, mode, one of 24 progressions, chord colour), a style per
  track from its voice (BASS, CHORD, ARP, LEAD, HITS) with its own formula, step velocities (FLAT, GROOVE, HUMAN,
  SWELL), the length it writes. A tap: the selected track; SHIFT + it: the setup on the step keys (user guide). SHIFT +
  RND PATT does the same as a tap.
- **SOUND RANDOM** is voice-aware: a character for the voice's family (PAD, LEAD, PLUCK, KEYS / RING, BLIP, BELL /
  ACID, SUB, PUNCH / HOME), the voice's own sliders, the harsh controls held low at low amounts. A tap: the selected
  track; SHIFT + it: every track.
- Every step has a velocity (recorded from MIDI, written by STEP RANDOM; COPY / PASTE / CLEAR carry it).
- Bigger legends: the front's new and small legends, the VOICE and FX MOD legends, section titles and selector words (in the
  label windows' style), set in the printed legends' font.
- The display shows only the live characters (the artwork's baked "120" removed, as on BAZZLINE).
- Projects: state version 2; version 1 (0.1.0) converted on load.

## 0.1.0
- Tested on an Akai Force (MPC OS 3.9.1).
- MODULES: the envelope, LEVEL and PAN legends in ice label windows like the voice's; FM RATIO and RELEASE lower
  and centred, a little bigger; the VOICE selector and its keys a notch up, the
  keys further apart with a larger touch area. PANEL: the randomizer amount knobs with the white segmented ring of the other knobs.
- First release of LXRXXL ZYNTH, a variation of LXRXXL BAZZLINE 0.2.8 (its sequencer, panel, voice chain, delay,
  gate/duck, mixer and presets).
- Sixteen voices, one per track (the VOICE selector on MODULES: < >, the data wheel, a Q-Link): ANALOG, TRIO, FM6,
  FM4, PHASE, WAVE, CHIP, MACRO, WHEEL, VOICE, PHYS, GRAIN, NOISE, LXR TONE, LXR METAL, BAZZ. The six sliders on the left
  of MODULES are the voice's controls, with label windows naming them; each track remembers every voice's sliders.
- MACRO, WAVE and PHYS: Mutable Instruments' Braids and Plaits (MIT). The other voices written here (FM6 with the DX7's
  32 algorithms from music-synthesizer-for-android, Apache 2.0).
- FM LVL and FM RATIO on every voice; UNISON plays a detuned second copy of any voice; the SQR / SAW / TRI switch is
  BAZZ's VCO wave and every other voice's sub wave.
- MODULES: RELEASE (the amp envelope's release, 4 ms .. 4 s) and FM RATIO knobs under SLIDE TIME. VCA ENV fully down
  holds the note while its gate is, then RELEASE.
- PANEL: LXRXXL's randomizers beside the panic key: STEP RANDOM (yellow, "?") and SOUND RANDOM (green, the warning
  sign), each with its amount knob; SHIFT + a randomizer: the selected track only. VOICE RESET keeps the track's voice.
- Artwork (Suns Kiefer's choice, option B): BAZZLINE's panel with a vein texture tinted ice blue in place of the
  circuit, the ice-blue accent instead of the orange, the ZYNTH logo.
- Presets: `.zysound` / `.zyperf` in `/sdcard/LXRXXL ZYNTH/` (a sound keeps its voice and every voice's sliders).
- The audio runs with flush-to-zero on (denormals made the physical models five times slower).
