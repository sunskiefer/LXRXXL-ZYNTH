# LXRXXL ZYNTH v0.2.0

![LXRXXL ZYNTH, the front panel](docs/img/panel.png)

**An experimental synth for Force and MPC: 19 coded voices on unconventional maths.** A Lorenz-attractor CHAOS, a
Hilbert-transform frequency SHIFTer, Euclidean rhythms and sine-wave melodies, and voices coded after the Yamaha DX7
(FM6), the Casio CZ (PHASE), the Minimoog (TRIO), Hammond tonewheels (WHEEL), NES and console chips (CHIP), Mutable
Instruments Braids (MACRO) and Plaits (WAVE, PHYS), the LXR-02's drum voices (LXR TONE, LXR METAL) and the Erica
Synths DB-01 bass via LXRXXL BAZZLINE (BAZZ). Plus ANALOG, FM4, VOICE, GRAIN, NOISE, DRONE, TEXTURE and FX.

A native VST2 instrument for the Akai Force and MPC standalone (MPC OS 3.x, through
[mpc-vst-plugins](https://github.com/sd88me/mpc-vst-plugins)), with four full-screen pages of artwork by Suns Kiefer.

**→ The full manual: [docs/USER_GUIDE.md](docs/USER_GUIDE.md)** (every page and control, the 19 voices and their 152
starter sounds, the sequencer, the randomizers, CHAOS and SHIFT, presets, Q-Links, recipes, every key combination).

## What it is

- **Eight tracks, each a monophonic synth with its own step sequence** (up to 64 steps, 64 patterns), the LXR-02 /
  DB-01 style sequencer of [LXRXXL BAZZLINE](https://github.com/sunskiefer/LXRXXL-BAZZLINE), on MPC's tempo and
  transport.
- **Each track picks one of 19 voices**, each with **20 controls of its own** in two rows of faders on the VOICE page,
  and **8 starter sounds**. Each track remembers every voice's whole sound; a voice first picked starts from its home
  sound, true to the machine it was coded after.
- **The shared chain:** FM on any voice, SUB, NOISE, the swarm (BBD or UNISON), DRIVE, the Polivoks or diode-ladder
  filter with its ADSR, the amp ADSR, the pitch envelope, a modulator, accent and slide.
- **A performance front page:** TIMBRE and MORPH (the voice's two most expressive controls), the filter, the ADSR,
  the modulator, CHAOS and SHIFT, the randomizers.
- **STEP RANDOM writes music:** chords, arpeggios, leads, bass lines and hits over a harmony each pattern keeps
  (10 080 harmonies), by formulas chosen from each track's voice, with step velocities and a setup on the step keys.
- **SOUND RANDOM** makes a new sound that suits the voice (PAD, LEAD, PLUCK, KEYS, RING, BELL, ACID...).
- **CHAOS:** a Lorenz attractor moving every track's pitch, cutoff, timbre or pan, each from its own angle: it never
  repeats. **SHIFT:** a frequency shifter / ring modulator on a send bus: every partial moved by the same number of Hz,
  with a barber-pole feedback.
- **A tape / digital delay, an 8-track mixer** (pan, delay send, CHAOS and SHIFT per track, mute, solo), **sound and
  performance presets** on the device.

**The four pages:**

| PANEL | VOICE | FX MOD | MIXER |
|---|---|---|---|
| ![PANEL](docs/img/panel.png) | ![VOICE](docs/img/voice.png) | ![FX MOD](docs/img/fx_mod.png) | ![MIXER](docs/img/mixer.png) |

No code from the FM-1 firmwares (Felucca, Sloop, Baud Girl: GPL-3, which can't be combined with the LXR02 license) or
from the Dirtywave M8 (closed) is used, and no factory sounds of the machines named: the voices are written here, or
come from Mutable Instruments' MIT-licensed Braids and Plaits.

**Tested on an Akai Force:** 0.1.0 on MPC OS 3.9.1 (`tested.json`). Every build is tested offline (see
[Testing](#testing)). Downloads: [Releases](https://github.com/sunskiefer/LXRXXL-ZYNTH/releases). Report anything odd
under Issues.

## Quick start

1. Install it (see [Installing](#installing)) and load **LXRXXL ZYNTH** (L'Cronx) on a plugin track.
2. Press play: the factory project plays. TRACK 1-8 pick the track every knob shows.
3. Turn **TIMBRE** and **MORPH**. On the **VOICE** page, **◄ ►** change the voice and **SOUND ◄ ►** its starter sounds.
4. Tap **STEP RANDOM** (the yellow "?" key) on a few tracks: new parts over the pattern's harmony.
5. Turn **CHAOS** and **SHIFT** (top right of the PANEL) up a little.
6. Save it: FX MOD, **PERF**, a **SLOT**, **SAVE**, **SAVE**.

The [user guide](docs/USER_GUIDE.md) explains the rest.

## How the voices were made

The full map is in **[docs/VOICE_MAP.md](docs/VOICE_MAP.md)**. In short:

| Where from | Voices |
|---|---|
| Written here, for LXRXXL ZYNTH | ANALOG, TRIO, FM6 (operators; the DX7's algorithm table from Google's music-synthesizer-for-android, Apache 2.0), FM4, PHASE, CHIP, WHEEL, VOICE, GRAIN, NOISE, DRONE, TEXTURE, FX; CHAOS and SHIFT |
| The LXR (LXR02Open's tables) | LXR TONE, LXR METAL, BAZZ (LXRXXL BAZZLINE's oscillator) |
| Mutable Instruments (MIT, `third_party/mutable`) | MACRO (Braids), WAVE (Plaits' wavetable engine), PHYS (Plaits' string and modal voices) |

## The licenses

LXRXXL ZYNTH, like LXRXXL and LXRXXL BAZZLINE, is under the **LXR02 Open-Source license** (`LICENSE`): **not for sale
or commercial use**, and every binary comes with its complete source (each release has a `-source.zip`). The parts
under `third_party/` keep their licenses: Braids, Plaits and stmlib (Emilie Gillet, Mutable Instruments) are MIT
(`third_party/mutable/LICENSE`), mpc-vst-plugins is MIT. The DX7 algorithm table in `src/eng_fm.c` is Apache 2.0
(Copyright 2012 Google Inc.). Artwork by Suns Kiefer: ask before reusing it. The fonts are SIL OFL 1.1.

*(This is a plain-language summary, not legal advice. The `LICENSE` files are what count.)*

## Installing

Needs root SSH access to the device (for example MockbaMod) and MPC OS 3.x. Unzip the release, then:

```
scp -r LXRXXL-ZYNTH-<version> root@<device-ip>:/tmp/
ssh -t root@<device-ip> sh /tmp/LXRXXL-ZYNTH-<version>/install.sh
```

The installer:
1. stops MPC (save first);
2. copies the plugin to `/sdcard/Synths/L'Cronx - VST - LXRXXL ZYNTH/`;
3. adds it to MPC's plugin list;
4. starts MPC again.

Load **LXRXXL ZYNTH** (L'Cronx) as a plugin instrument. `uninstall.sh` removes it. INSTALL.md in the zip has
the manual steps.

## Building

Python 3 with Pillow, numpy and scipy, a host gcc, clang and clang++, Zig (`pip install ziglang`); valgrind and
qemu-user for the tests. No Docker.

```
git clone https://github.com/sunskiefer/LXRXXL-ZYNTH
cd LXRXXL-ZYNTH
./build.sh             # build/arm/lxrxxl_zynth.so + the skin -> build/package/
test/run_tests.sh      # every voice on its own, the wrapper's host test, the engine tests and the MPC simulation (valgrind)
test/arm_sim.sh        # the MPC simulation built for ARM, run under qemu-arm
```

The tools:
- `tools/gen_params.py`: the parameter list. From now on parameters are only ever appended, because MPC stores
  automation by index.
- `tools/panel.py`: where every control of the artwork sits, and the Q-Link pages from `qlinks.json`. It writes `layout.conf`.
- `tools/zynth_art.py`: ZYNTH's artwork from BAZZLINE's (`art/bazzline/`): the vein texture (`art/texture/`), the ice
  accent, the ZYNTH logo (writes `art/panel/`).
- `tools/voices.py`: the voices, their twenty controls and their starter sounds (writes `src/voice_defs.h` and
  `src/starter_defs.h`).
- `tools/panel_art.py`: builds the four pages from the artwork (`art/panel/`), with the voice selector and the label
  windows (each voice's labels, from `tools/voices.py`).
- `tools/preview.py`: draws a page with chosen values.

## Testing

Offline, every build (`test/run_tests.sh`, and in CI):
- **Every voice on its own** (`test/eng_test.c`): each of the nineteen at its defaults plays in tune (its partials on
  the note), at a sensible level, with no NaN; each deeper control changes the sound; 400 random settings and notes
  never blow up. Also built for ARM and run under qemu: the same results.
- **The framework's host test:** two instances, names, set/get, option stepping, notes into audio, the chunk.
- **The engine tests:** the six demo patterns play on MPC's transport; 20 000 random edits to every key and parameter;
  the state round-trip; a 0.1.0 project loaded (sounds, sliders, SUSTAIN from VCA ENV, the duck off); every voice
  played by MIDI; all 152 starter sounds play at a sensible level; VOICE, each voice's whole sound and its home
  sound, VOICE RESET; TIMBRE and MORPH; VOICE < >; SOUND RANDOM (one track, every track, its limits at a low amount)
  and STEP RANDOM (one track, the setup's keys, every track, every note in the key, the velocities, 50 presses giving
  50 different lines); CHAOS (each track its own motion, every destination, the per-track amount); SHIFT (a 1 kHz tone
  set to +250 Hz comes out at 1250 Hz, the other side about 56 dB down; RING with feedback stays bounded); MIDI notes;
  the delay; the mixer; presets.
- **The MPC simulation** (through the VST2 wrapper only, also built for ARM and run under qemu): MPC's play and the
  panel's PLAY, the display, TRACK, the VOICE selector (its keys, the data wheel and a Q-Link), CHAOS DEST stepped,
  SHIFT MIX, FM6 played, the readout, the encoders, SHIFT functions, the project saved and loaded, RESET.

On a device: 0.1.0 on an Akai Force, MPC OS 3.9.1 (`tested.json`); 0.2.0 not yet.

Worst case offline (all eight tracks on heavy voices (MACRO, PHYS, FM6, GRAIN, DRONE, WAVE, TEXTURE, TRIO) with
their deeper controls up, UNISON, FM, sub and noise, plus the delay, CHAOS and SHIFT): about 150-220 µs per 128-frame
block on an x86 core (0.1.0: about 150 µs). The device's budget is 2902 µs per block; the device check
(`mpc-vst-plugins tools/bench.sh`) is still to do, so on the Force start a heavy project with a few tracks and add
from there. The audio runs with flush-to-zero on (denormals made the physical models five times slower).

## Credits and license

- **Developer:** L'Cronx.
- **Artwork by Suns Kiefer**
- **The LXR's oscillator tables:** Julian Schmidt (Sonic Potions LXR) and Brendan Clarke (LXR02Open), LXR02
  Open-Source license.
- **Braids, Plaits, stmlib:** Emilie Gillet (Mutable Instruments), MIT.
- **The DX7 algorithm table:** music-synthesizer-for-android, Copyright 2012 Google Inc., Apache 2.0.
- **The sequencer, the voice chain, the delay, the mixer, the presets:** from LXRXXL BAZZLINE and LXRXXL.
- **SHIFT's Hilbert transformer:** Olli Niemitalo's all-pass coefficients (public).
- **VST2 wrapper, skin builder and installer:** sd88me, [mpc-vst-plugins](https://github.com/sd88me/mpc-vst-plugins) (MIT).
- **Fonts:** DSEG14 Classic by keshikan; Michroma by Vernon Adams (SIL Open Font License 1.1).

LXRXXL ZYNTH is not affiliated with or endorsed by Erica Synths, Sonic Potions, Mutable Instruments, M-VAVE, Dirtywave,
Yamaha, Casio, Moog, Hammond, Nintendo or Akai Professional. Their names only say which instruments a voice was coded
after; all trademarks are their owners'.
