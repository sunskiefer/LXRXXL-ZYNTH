# Changelog

## 0.1.0
- Tested on an Akai Force (MPC OS 3.9.1).
- MODULES: the envelope, LEVEL and PAN legends in ice label windows like the voice's; FM RATIO and RELEASE lower
  and centred, a little bigger; the VOICE selector and its keys a notch up. PANEL: the randomizer amount knobs with the white segmented ring of the other knobs.
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
