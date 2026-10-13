#!/usr/bin/env python3
"""The single source of LXRXXL ZYNTH's voices: their names and their twenty controls. Writes src/voice_defs.h (the
engine's ENG_DEFS) and is read by tools/gen_params.py (the VOICE selector's options) and tools/panel_art.py (the
VOICE page's label windows).

Each voice has twenty controls, the twenty faders of the VOICE page that are not the envelopes:
  V1 .. V6    the top row's left half: the voice's main controls (0.1.0's six sliders, unchanged)
  V7 .. V12   the bottom row's left half: the voice's deeper controls
  V13 .. V20  the bottom row's right half: OCTAVE, FINE, VIBRATO, then a modulation envelope (ENV ATTACK, ENV DECAY)
              to two of the voice's controls (ENV > A, ENV > B) and the velocity to one (VEL > A); A and B are the
              voice's own (FM6: MOD INDEX and FEEDBACK), so the labels change with the voice.

A control: (the readout's name, the label window's text ("A|B": two lines), default, steps (0: continuous)).
"up" (optional): the value a 0.1.0 sound gets for a control it did not have, when not the default (a number, or
"=k": the value of control k, 0-based) so that it sounds as it did.

    python3 tools/voices.py          (writes src/voice_defs.h)"""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def C(name, label=None, d=0.0, steps=0, up=None):
    return {"name": name, "label": label or name, "def": d, "steps": steps, "up": up}


# each voice: name, three-character name, the slider the modulator's WAVE destination moves, the front's TIMBRE and
# MORPH (control indices), the modulation envelope's targets A and B and the velocity's (indices, short names), and
# its twelve own controls
VOICES = [
    dict(name="ANALOG", short="ANL", timbre=1, macros=(1, 4), env=((1, "PW"), (4, "SYNC")), vel=(1, "PW"), ctrls=[
        C("WAVE"), C("PULSE WIDTH", "PULSE|WIDTH"), C("DETUNE", d=.12), C("INTERVAL", "INTER|VAL", .5 / 9, 9),
        C("SYNC"), C("MIX", d=.5),
        C("WAVE 2", "WAVE|2", up="=0"), C("PWM"), C("PWM RATE", "PWM|RATE", .3), C("RING"),
        C("CROSS FM", "CROSS|FM"), C("DRIFT")]),
    dict(name="TRIO", short="TRI", timbre=0, macros=(3, 4), env=((3, "DETUNE"), (5, "LVL 3")), vel=(0, "WAVE"), ctrls=[
        C("WAVE"), C("OSC 2 INTERVAL", "OSC 2|INT", 19.5 / 25, 25), C("OSC 3 INTERVAL", "OSC 3|INT", 12.5 / 49, 49),
        C("DETUNE", d=.25), C("LEVEL 2", "LEVEL|2", .75), C("LEVEL 3", "LEVEL|3", .6),
        C("WAVE 2", "WAVE|2", up="=0"), C("WAVE 3", "WAVE|3", up="=0"), C("OSC 3 TO PITCH", "O3>|PITCH"),
        C("OSC 3 LOW", "O3|LOW"), C("FEEDBACK", "FEED|BACK"), C("DRIFT")]),
    dict(name="FM6", short="FM6", timbre=2, macros=(2, 3), env=((2, "INDEX"), (3, "FBACK")), vel=(2, "INDEX"), ctrls=[
        C("ALGO", d=4.5 / 32, steps=32), C("RATIO", d=1.5 / 16, steps=16), C("MOD INDEX", "MOD|INDEX", .35),
        C("FEEDBACK", "FEED|BACK", .2), C("DETUNE", d=.15), C("BRIGHT", d=.4),
        C("MOD ATTACK", "MOD|ATTACK"), C("DECAY TILT", "DECAY|TILT", .5), C("INHARMONIC", "INHAR|MONIC"),
        C("OP 6 FIXED", "OP 6|FIXED"), C("CARRIER SPREAD", "CAR|SPREAD"), C("KEY SCALE", "KEY|SCALE")]),
    dict(name="FM4", short="FM4", timbre=3, macros=(3, 4), env=((3, "INDEX"), (4, "FBACK")), vel=(3, "INDEX"), ctrls=[
        C("ALGO", d=.5 / 12, steps=12), C("RATIO B", "RATIO|B", 3.5 / 20, 20), C("RATIO C", "RATIO|C", 5.5 / 20, 20),
        C("MOD INDEX", "MOD|INDEX", .4), C("FEEDBACK", "FEED|BACK", .1), C("DECAY", d=.5),
        C("RATIO D", "RATIO|D", 3.5 / 20, 20, up="=1"), C("MOD ATTACK", "MOD|ATTACK"), C("OP 4 FIXED", "OP 4|FIXED"),
        C("INHARMONIC", "INHAR|MONIC"), C("CARRIER SPREAD", "CAR|SPREAD"), C("KEY SCALE", "KEY|SCALE")]),
    dict(name="PHASE", short="PHS", timbre=1, macros=(1, 2), env=((1, "AMOUNT"), (2, "ENV AMT")), vel=(1, "AMOUNT"), ctrls=[
        C("WAVE", d=.5 / 8, steps=8), C("AMOUNT", d=.45), C("ENV AMOUNT", "ENV|AMT", .4),
        C("ENV DECAY", "ENV|DECAY", .35), C("DETUNE", d=.1), C("LINE 2", "LINE|2"),
        C("WAVE 2", "WAVE|2", .5 / 8, 8, up="=0"), C("ENV ATTACK", "DCW|ATTACK"), C("ENV SUSTAIN", "DCW|SUSTAIN"),
        C("LINE 2 OCTAVE", "LINE 2|OCT", 1.5 / 3, 3), C("RING"), C("NOISE MOD", "NOISE|MOD")]),
    dict(name="WAVE", short="WAV", timbre=1, macros=(1, 4), env=((1, "POS X"), (2, "POS Y")), vel=(1, "POS X"), ctrls=[
        C("BANK"), C("POS X", "POS|X", .3), C("POS Y", "POS|Y", .5), C("WARP TYPE", "WARP|TYPE", .5 / 4, 4),
        C("WARP AMOUNT", "WARP|AMT"), C("SCAN", d=.3),
        C("SCAN DECAY", "SCAN|DECAY", .435), C("SCAN Y", "SCAN|Y"), C("DRIFT"), C("RING RATIO", "RING|RATIO", .2667),
        C("WARP ENV", "WARP|ENV"), C("BRIGHT", d=.5)]),
    dict(name="CHIP", short="CHP", timbre=2, macros=(1, 5), env=((1, "BITS"), (2, "RATE")), vel=(1, "BITS"), ctrls=[
        C("WAVE", d=1.5 / 7, steps=7), C("BITS", d=.5), C("RATE", d=.75), C("ARP", d=.5 / 8, steps=8),
        C("SPEED", d=.5), C("SWEEP", d=.5),
        C("PULSE MOD", "PULSE|MOD"), C("ARP OCTAVES", "ARP|OCT", .5 / 3, 3), C("ARP MODE", "ARP|MODE", .5 / 4, 4),
        C("NOISE MIX", "NOISE|MIX"), C("DUAL", "DUAL"), C("BLIP")]),
    dict(name="MACRO", short="MAC", timbre=1, macros=(1, 2), env=((1, "TIMBRE"), (2, "COLOR")), vel=(1, "TIMBRE"), ctrls=[
        C("SHAPE", d=.5 / 44, steps=44), C("TIMBRE", d=.4), C("COLOR", d=.5), C("TIMBRE ENV", "TIMBRE|ENV", .2),
        C("COLOR ENV", "COLOR|ENV"), C("ENV DECAY", "ENV|DECAY", .4),
        C("SHAPE ATTACK", "SHAPE|ATTACK"), C("CRUSH"), C("DOWNSAMPLE", "DOWN|SAMPLE"), C("DRIFT"),
        C("SIGNATURE", "SIGNA|TURE"), C("BRIGHT", d=.5)]),
    dict(name="WHEEL", short="WHL", timbre=2, macros=(2, 5), env=((2, "BRIGHT"), (1, "BODY")), vel=(4, "CLICK"), ctrls=[
        C("BASS", d=.8), C("BODY", d=.8), C("BRIGHT", d=.3), C("PERC", d=2.5 / 5, steps=5), C("CLICK", d=.3),
        C("LESLIE", d=.3),
        C("5 1/3"), C("2 2/3"), C("1 3/5"), C("1'"), C("SCANNER", "SCAN|VIBRATO"), C("LEAKAGE", "LEAK|AGE")]),
    dict(name="VOICE", short="VOX", timbre=0, macros=(0, 1), env=((0, "VOWEL"), (1, "FORMNT")), vel=(2, "BREATH"), ctrls=[
        C("VOWEL"), C("FORMANT", d=.5), C("BREATH", d=.15), C("VIBRATO", d=.3), C("RESO", d=.5), C("TONE", d=.5),
        C("SINGER", "SINGER|FORMANT"), C("NASAL"), C("GLOTTAL PULSE", "GLOTTAL|PULSE"), C("CHOIR"), C("JITTER"),
        C("BREATH ATTACK", "BREATH|ATTACK")]),
    dict(name="PHYS", short="PHY", timbre=1, macros=(1, 2), env=((1, "STRUCT"), (2, "BRIGHT")), vel=(2, "BRIGHT"), ctrls=[
        C("MODEL", d=.25, steps=2), C("STRUCTURE", "STRUC|TURE", .25), C("BRIGHT", d=.5), C("DAMPING", d=.55),
        C("EXCITER"), C("STRENGTH", d=.6),
        C("BODY"), C("STRIKE NOISE", "STRIKE|NOISE"), C("PITCH STRIKE", "PITCH|STRIKE"), C("MUTE"),
        C("SYMPATHETIC", "SYMPA|THETIC"), C("DRIVE")]),
    dict(name="GRAIN", short="GRN", timbre=2, macros=(1, 2), env=((2, "POSITN"), (1, "DENSTY")), vel=(0, "SIZE"), ctrls=[
        C("SIZE", d=.5), C("DENSITY", d=.5), C("POSITION", d=.3), C("SPRAY", d=.2), C("PITCH SPREAD", "PITCH|SPREAD", .1),
        C("TONE", d=.7),
        C("REVERSE", "REV|ERSE"), C("WINDOW"), C("SCAN SPEED", "SCAN|SPEED", .5), C("DETUNE"),
        C("OCTAVE UP", "OCTAVE|UP"), C("JITTER", d=.3)]),
    dict(name="NOISE", short="NOI", timbre=2, macros=(1, 2), env=((1, "COLOR"), (2, "RESO")), vel=(1, "COLOR"), ctrls=[
        C("TYPE", d=.5 / 5, steps=5), C("COLOR", d=.5), C("RESO", d=.6), C("HARMONICS", "HARMO|NICS", .3),
        C("DENSITY", d=.5), C("GRIT"),
        C("SWEEP", d=.5), C("PULSE"), C("PULSE RATE", "PULSE|RATE", .4), C("TONE"), C("INHARMONIC", "INHAR|MONIC"),
        C("LOW CUT", "LOW|CUT")]),
    dict(name="LXR TONE", short="LXT", timbre=1, macros=(1, 11), env=((1, "FM AMT"), (11, "FOLD")), vel=(1, "FM AMT"), ctrls=[
        C("WAVE", d=.5 / 4, steps=4), C("FM AMOUNT", "FM|AMT", .3), C("FM FREQ", "FM|FREQ", 5.5 / 16, 16),
        C("FM WAVE", "FM|WAVE", .5 / 4, 4), C("FM ENV", "FM|ENV", .4), C("DECIMATE", "DECI|MATE"),
        C("PITCH ENV", "PITCH|ENV"), C("PITCH DECAY", "PITCH|DECAY", .3), C("CLICK"), C("SINE BODY", "SINE|BODY"),
        C("SYNC"), C("FOLD")]),
    dict(name="LXR METAL", short="LXM", timbre=4, macros=(1, 2), env=((1, "SPREAD"), (4, "BAND")), vel=(2, "FM AMT"), ctrls=[
        C("CHARACTER", "CHAR|ACTER"), C("SPREAD", d=.5), C("FM AMOUNT", "FM|AMT", .2), C("NOISE", d=.1),
        C("BAND", d=.5), C("RING", d=.3),
        C("DETUNE"), C("TONE"), C("BAND Q", "BAND|Q", .4), C("LOW CUT", "LOW|CUT"), C("SHIMMER", "SHIM|MER"),
        C("CRUSH")]),
    dict(name="BAZZ", short="BAZ", timbre=1, macros=(1, 5), env=((1, "W POS"), (5, "TONE")), vel=(5, "TONE"), ctrls=[
        C("OSC MODE", "OSC|MODE", .5 / 3, 3), C("WAVE POS", "WAVE|POS"), C("OCTAVE", d=2.5 / 5, steps=5),
        C("SUB OCT", "SUB|OCT", .5 / 2, 2), C("DRIFT", d=.1), C("TONE", d=.75),
        C("OSC 2", "OSC 2|LEVEL"), C("OSC 2 DETUNE", "OSC 2|DETUNE", .2), C("OSC 2 OCTAVE", "OSC 2|OCT", 2.5 / 5, 5),
        C("FOLD"), C("CRUSH"), C("SATURATE", "SATU|RATE")]),
    # 0.2.0: the three new voices
    dict(name="DRONE", short="DRN", timbre=0, macros=(0, 2), env=((0, "WAVE"), (2, "DETUNE")), vel=(11, "TONE"), ctrls=[
        C("WAVE", d=.3), C("VOICES", "VOI|CES", 3.5 / 6, 6), C("DETUNE", d=.3), C("STACK", d=2.5 / 8, steps=8),
        C("DRIFT", d=.3), C("MOTION", "MOT|ION", .3),
        C("MOTION RATE", "MOTION|RATE", .3), C("BEAT", d=.2), C("SUB", d=.3), C("SHIMMER", "SHIM|MER"),
        C("BLOOM", d=.3), C("TONE", d=.7)]),
    dict(name="TEXTURE", short="TXT", timbre=0, macros=(0, 3), env=((3, "COLOR"), (4, "RESO")), vel=(3, "COLOR"), ctrls=[
        C("SOURCE", "SOUR|CE", .2), C("DENSITY", d=.5), C("PITCH TRACK", "PITCH|TRACK", .5), C("COLOR", d=.5),
        C("RESO", d=.3), C("MOTION", "MOT|ION", .4),
        C("MOTION RATE", "MOTION|RATE", .3), C("GRIT"), C("WIND", d=.3), C("SPARKLE", "SPAR|KLE", .2),
        C("RUMBLE", "RUM|BLE", .2), C("SPACE", d=.3)]),
    dict(name="FX", short="FX", timbre=2, macros=(2, 5), env=((2, "DEPTH"), (4, "CHAOS")), vel=(2, "DEPTH"), ctrls=[
        C("TYPE", d=.5 / 8, steps=8), C("SWEEP TIME", "SWEEP|TIME", .4), C("SWEEP DEPTH", "SWEEP|DEPTH", .6),
        C("REPEAT", "RE|PEAT"), C("CHAOS", d=.2), C("WOBBLE", "WOB|BLE"),
        C("WOBBLE RATE", "WOBBLE|RATE", .4), C("CRUSH"), C("NOISE", d=.1), C("RING"), C("FEEDBACK", "FEED|BACK", .2),
        C("TONE", d=.7)]),
]



def S(i, n):
    """a stepped control's option i of n, as its value"""
    return (i + 0.5) / n


# ---------------------------------------------------------------- the starter sounds: eight per voice (the VOICE page's
# SOUND < >). The first is the voice's home sound (src/sound.c HOME); each other one is the home sound with these
# changes. Keys: "V1".."V20" (the voice's controls) or a sound parameter's key (tools/gen_params.py).
STARTERS = {
    "ANALOG": [
        ("ANALOG", {}),
        ("SAW LEAD", {"V1": 0, "V3": .15, "cutoff": .65, "resonance": .35, "vcf_env": .45, "vcf_dec": .35,
                      "sustain": .8, "vca_rel": .3, "V15": .25, "slide_time": .3}),
        ("SQUARE BASS", {"V1": 1, "V2": .2, "V7": 1, "V4": S(8, 9), "cutoff": .35, "resonance": .4, "vcf_env": .55,
                         "vcf_dec": .3, "vca_env": .4, "sustain": 0, "drive": .2}),
        ("SYNC LEAD", {"V5": .4, "V18": .85, "V17": .4, "cutoff": .8, "sustain": .9, "vca_rel": .25}),
        ("PWM STRINGS", {"V1": 1, "V7": 1, "V8": .6, "V9": .25, "vca_atk": .35, "vca_rel": .55, "cutoff": .6,
                         "detune": .3, "sw_mix": .6}),
        ("FIFTH STAB", {"V4": S(4, 9), "cutoff": .5, "vcf_env": .6, "vcf_dec": .3, "vca_env": .35, "sustain": 0,
                        "vca_rel": .3}),
        ("BRASS", {"V1": .2, "V3": .2, "vcf_atk": .35, "vcf_env": .5, "vcf_sus": .4, "cutoff": .45, "vca_atk": .12,
                   "sustain": .85, "vca_rel": .3}),
        ("CROSS BELL", {"V11": .6, "V4": S(5, 9), "vca_env": .6, "sustain": 0, "vca_rel": .5, "cutoff": .9}),
    ],
    "TRIO": [
        ("TRIO", {}),
        ("FAT BASS", {"V2": S(12, 25), "V3": S(12, 49), "V4": .2, "cutoff": .3, "resonance": .45, "vcf_env": .5,
                      "vcf_dec": .3, "sustain": 0, "vca_env": .4, "drive": .25, "fmodel": 1, "vca_atk": 0, "vca_rel": .2}),
        ("MOOG LEAD", {"V2": S(12, 25), "V3": S(31, 49), "V4": .25, "cutoff": .55, "resonance": .5, "vcf_env": .4,
                       "sustain": .8, "slide_time": .35, "V15": .2, "vca_atk": 0, "vca_rel": .3}),
        ("TRILL LEAD", {"V10": .45, "V9": .41, "cutoff": .7, "vca_atk": 0, "vca_rel": .3}),
        ("WIDE PAD", {"V4": .6, "vca_atk": .45, "vca_rel": .65, "cutoff": .55, "detune": .4, "sw_mix": .7, "lfo_lvl": .1}),
        ("OCTAVE STACK", {"V2": S(24, 25), "V3": S(48, 49), "V5": .6, "V6": .4, "cutoff": .75, "vca_atk": 0}),
        ("GROWL", {"V11": .7, "cutoff": .4, "resonance": .3, "vcf_env": .5, "sustain": .7, "drive": .2, "vca_atk": 0}),
        ("DRIFT CHOIR", {"V12": .6, "V4": .5, "vca_atk": .3, "vca_rel": .6, "cutoff": .6, "V1": 0}),
    ],
    "FM6": [
        ("FM6", {}),
        ("E.PIANO", {"V1": S(4, 32), "V2": S(3, 16), "V3": .3, "V6": .3, "vca_env": .62, "sustain": 0, "vca_rel": .4,
                     "V20": .8}),
        ("BELL", {"V1": S(4, 32), "V2": S(8, 16), "V3": .45, "V6": .55, "V9": .4, "vca_env": .75, "sustain": 0,
                  "vca_rel": .65}),
        ("FM BASS", {"V1": S(0, 32), "V2": S(11, 16), "V3": .5, "V4": .35, "V6": .2, "vca_env": .45, "sustain": .2,
                     "cutoff": .6}),
        ("BRASS", {"V1": S(21, 32), "V2": S(0, 16), "V3": .4, "V7": .35, "V6": .7, "vca_atk": .08, "sustain": .9,
                   "vca_rel": .3}),
        ("ORGAN", {"V1": S(31, 32), "V2": S(6, 16), "V3": .2, "sustain": 1, "vca_rel": .15}),
        ("MARIMBA", {"V1": S(4, 32), "V2": S(4, 16), "V3": .5, "V6": .15, "vca_env": .4, "sustain": 0, "V8": .8}),
        ("FB GROWL", {"V1": S(0, 32), "V4": .75, "V3": .5, "V19": .8, "V17": .35, "cutoff": .55, "resonance": .3,
                      "sustain": .7}),
    ],
    "FM4": [
        ("FM4", {}),
        ("DX BASS", {"V1": S(0, 12), "V2": S(2, 20), "V3": S(2, 20), "V7": S(2, 20), "V4": .5, "V5": .4, "V6": .35,
                     "sustain": .1, "vca_env": .45}),
        ("CHIME", {"V1": S(4, 12), "V2": S(13, 20), "V3": S(8, 20), "V7": S(8, 20), "V4": .35, "V6": .7, "sustain": 0,
                   "vca_env": .7, "vca_rel": .6}),
        ("WOOD", {"V1": S(9, 12), "V2": S(6, 20), "V3": S(9, 20), "V7": S(12, 20), "V4": .45, "V6": .15, "sustain": 0,
                  "vca_env": .35}),
        ("FM LEAD", {"V1": S(0, 12), "V2": S(2, 20), "V3": S(2, 20), "V7": S(2, 20), "V5": .6, "V4": .55,
                     "sustain": .8, "cutoff": .7}),
        ("FM PAD", {"V1": S(4, 12), "V2": S(4, 20), "V7": S(6, 20), "V4": .3, "V8": .5, "vca_atk": .4, "sustain": 1,
                    "vca_rel": .6, "detune": .3, "V11": .4}),
        ("PLUCK", {"V1": S(1, 12), "V2": S(4, 20), "V3": S(2, 20), "V4": .55, "V6": .2, "sustain": 0, "vca_env": .4}),
        ("METAL", {"V1": S(5, 12), "V2": S(19, 20), "V3": S(16, 20), "V7": S(15, 20), "V10": .5, "V4": .6,
                   "sustain": 0, "vca_env": .6}),
    ],
    "PHASE": [
        ("PHASE", {}),
        ("CZ BASS", {"V1": S(0, 8), "V2": .55, "V3": .35, "V4": .25, "sustain": .2, "vca_env": .4}),
        ("RESO LEAD", {"V1": S(5, 8), "V2": .4, "V3": .4, "V4": .45, "V9": .3, "sustain": .85, "V15": .2}),
        ("CZ STRINGS", {"V1": S(1, 8), "V6": .7, "V5": .3, "V7": S(0, 8), "V8": .5, "V9": .6, "vca_atk": .3,
                        "sustain": 1, "vca_rel": .55}),
        ("CZ BRASS", {"V1": S(0, 8), "V8": .3, "V9": .5, "V2": .25, "V3": .6, "sustain": .9}),
        ("RESO BELL", {"V1": S(6, 8), "V7": S(6, 8), "V11": .6, "V6": .6, "sustain": 0, "vca_env": .6}),
        ("PULSE KEYS", {"V1": S(2, 8), "V2": .4, "V3": .5, "V4": .3, "sustain": .3, "vca_env": .5}),
        ("NOISE SYNTH", {"V1": S(4, 8), "V12": .6, "V2": .5, "sustain": .8}),
    ],
    "WAVE": [
        ("WAVE", {}),
        ("SCAN PAD", {"V1": .33, "V2": .2, "V6": .7, "V7": .7, "vca_atk": .35, "vca_rel": .6, "lfo_lvl": .2,
                      "mod_dest": 3}),
        ("DIGI BELL", {"V1": .66, "V2": .6, "V3": .2, "V4": S(0, 4), "V5": .3, "sustain": 0, "vca_env": .6,
                       "vca_atk": 0}),
        ("CRUSH LEAD", {"V4": S(2, 4), "V5": .5, "V2": .5, "sustain": .9, "vca_atk": 0}),
        ("RING BELL", {"V4": S(3, 4), "V5": .7, "V10": .5, "sustain": 0, "vca_env": .55, "vca_atk": 0}),
        ("SHAPE BASS", {"V1": 0, "V2": .1, "V4": S(1, 4), "V5": .5, "cutoff": .5, "vcf_env": .4, "sustain": .3,
                        "vca_atk": 0, "vca_rel": .2}),
        ("WARP SWEEP", {"V11": .7, "V4": S(0, 4), "V5": .2, "V7": .6}),
        ("DRIFT KEYS", {"V9": .6, "V2": .45, "V3": .7, "sustain": .4, "vca_env": .55, "vca_atk": 0}),
    ],
    "CHIP": [
        ("CHIP", {}),
        ("NES LEAD", {"V1": S(1, 7), "V15": .25, "vca_rel": .15}),
        ("NES BASS", {"V1": S(3, 7), "vca_rel": .08}),
        ("MAJOR ARP", {"V1": S(2, 7), "V4": S(2, 8), "V5": .6, "V8": S(1, 3)}),
        ("MINOR ARP", {"V1": S(1, 7), "V4": S(3, 8), "V5": .7, "V9": S(2, 4)}),
        ("BLIP", {"V12": .7, "V1": S(0, 7), "sustain": 0, "vca_env": .3}),
        ("NOISE HIT", {"V1": S(5, 7), "sustain": 0, "vca_env": .3}),
        ("DUAL PULSE", {"V1": S(1, 7), "V11": .6, "V7": .5}),
    ],
    "MACRO": [
        ("MACRO", {}),
        ("SAW SWARM", {"V1": S(14, 44), "V2": .6, "vca_atk": .2, "vca_rel": .5}),
        ("PLUCKED", {"V1": S(28, 44), "V2": .5, "V3": .5, "sustain": 0, "vca_env": .6}),
        ("BOWED", {"V1": S(29, 44), "V2": .5, "V3": .4, "vca_atk": .25}),
        ("VOWEL", {"V1": S(22, 44), "V2": .3, "V4": .5, "V6": .5}),
        ("FM BELL", {"V1": S(25, 44), "V2": .5, "V3": .6, "sustain": 0, "vca_env": .6}),
        ("WAVE CHORD", {"V1": S(37, 44), "V2": .4, "V3": .3, "vca_atk": .2}),
        ("SQUARE SUB", {"V1": S(5, 44), "V2": .5, "cutoff": .5, "resonance": .4, "vcf_env": .5, "sustain": .5}),
    ],
    "WHEEL": [
        ("WHEEL", {}),
        ("JAZZ", {"V1": .9, "V2": .8, "V3": 0, "V4": S(2, 5), "V5": .4, "V6": 0}),
        ("GOSPEL", {"V1": .9, "V2": .9, "V3": .9, "V6": .8, "V5": .3}),
        ("ROCK", {"V1": .9, "V2": .9, "V3": .6, "drive": .45, "V6": .3, "V12": .2}),
        ("THEATRE", {"V11": .7, "V1": .8, "V2": .7, "V3": .5, "V7": .5}),
        ("FLUTE", {"V1": 0, "V2": .6, "V3": 0, "V4": S(0, 5), "V5": .1}),
        ("PERC ORGAN", {"V4": S(1, 5), "V5": .5, "V1": .7, "V2": .7, "V3": 0}),
        ("REED", {"V1": .4, "V2": .8, "V3": .8, "V7": .6, "V9": .5, "V10": .4}),
    ],
    "VOICE": [
        ("VOICE", {}),
        ("CHOIR AH", {"V1": 0, "V10": .7, "vca_atk": .3, "vca_rel": .5, "V4": .3}),
        ("OOH PAD", {"V1": .95, "V10": .5, "V3": .3, "vca_atk": .4, "vca_rel": .6}),
        ("SINGER", {"V7": .6, "V1": .4, "V4": .4}),
        ("WHISPER", {"V3": .9, "V12": .4}),
        ("ROBOT", {"V9": .8, "V4": 0, "V11": 0, "V1": .25}),
        ("NASAL", {"V8": .7, "V1": .2}),
        ("VOWEL SWEEP", {"V18": .9, "V17": .5, "V16": .3}),
    ],
    "PHYS": [
        ("PHYS", {}),
        ("NYLON", {"V1": S(0, 2), "V2": .3, "V3": .35, "V4": .6, "V7": .6}),
        ("STEEL", {"V2": .2, "V3": .7, "V4": .7, "V7": .3}),
        ("MARIMBA", {"V1": S(1, 2), "V2": .3, "V3": .4, "V4": .4}),
        ("PHYS BELL", {"V1": S(1, 2), "V2": .75, "V3": .6, "V4": .8}),
        ("BOWED", {"V5": .7, "V2": .3, "V3": .4, "V4": .8, "vca_atk": .2}),
        ("KALIMBA", {"V1": S(1, 2), "V2": .55, "V3": .5, "V4": .55, "V9": .4}),
        ("SITAR", {"V2": .2, "V3": .8, "V11": .7, "V12": .2}),
    ],
    "GRAIN": [
        ("GRAIN", {}),
        ("CLOUD", {"V1": .6, "V2": .7, "V4": .5, "V5": .2, "vca_atk": .45, "vca_rel": .7}),
        ("SHIMMER", {"V5": .8, "V11": .5, "V6": .9}),
        ("SCAN", {"V9": .7, "V2": .6}),
        ("REVERSE", {"V7": .7, "V1": .7, "V2": .5}),
        ("BUZZ", {"V1": .1, "V2": .95, "V3": .6, "vca_atk": 0}),
        ("GRAIN KEYS", {"V1": .3, "V2": .6, "sustain": 0, "vca_env": .5, "vca_atk": 0}),
        ("FROZEN", {"V1": .9, "V2": .4, "V4": 0, "V8": .6, "vca_atk": .5}),
    ],
    "NOISE": [
        ("NOISE", {}),
        ("WIND", {"V1": S(1, 5), "V2": .5, "V3": .3, "vca_atk": .5, "vca_rel": .7}),
        ("OCEAN", {"V1": S(1, 5), "V3": .2, "mod_dest": 1, "lfo_rate": .1, "lfo_lvl": .6, "vca_atk": .6, "vca_rel": .7}),
        ("VINYL", {"V1": S(4, 5), "V5": .3, "V2": .4, "V3": 0}),
        ("TUNED NOISE", {"V3": .85, "V4": .5}),
        ("STEAM", {"V1": S(0, 5), "V2": .8, "V3": 0, "V7": .7}),
        ("GEIGER", {"V1": S(3, 5), "V5": .2, "V8": .7, "V9": .6}),
        ("CRACKLE", {"V1": S(2, 5), "V5": .6}),
    ],
    "LXR TONE": [
        ("LXR TONE", {}),
        ("SINE BASS", {"V1": S(0, 4), "V2": .1, "V7": .25, "V8": .2, "sustain": .6, "vca_env": .5, "cutoff": .7}),
        ("FM KEYS", {"V1": S(0, 4), "V2": .45, "V3": S(3, 16), "V5": .6, "sustain": 0, "vca_env": .55}),
        ("SYNC LEAD", {"V11": .5, "V1": S(2, 4), "sustain": .9}),
        ("FOLD BASS", {"V12": .5, "V1": S(1, 4), "sustain": .5, "cutoff": .5}),
        ("LXR TOM", {"V7": .45, "V8": .35, "V10": .5, "sustain": 0, "vca_env": .45}),
        ("CLICK KEYS", {"V9": .5, "V2": .3, "sustain": 0, "vca_env": .4}),
        ("BITS LEAD", {"V6": .4, "V1": S(3, 4)}),
    ],
    "LXR METAL": [
        ("LXR METAL", {}),
        ("BELLS", {"V1": .7, "V3": .1, "V8": .5, "vca_env": .7}),
        ("GONG", {"V1": 1, "V3": .3, "vca_env": .85, "vca_rel": .7}),
        ("SHIMMER PAD", {"V11": .7, "V1": .4, "V8": .4, "vca_atk": .5, "sustain": 1, "vca_rel": .7}),
        ("HATS", {"V1": 0, "V5": .8, "vca_env": .2}),
        ("CYMBAL", {"V1": .35, "V5": .7, "vca_env": .7}),
        ("METAL BASS", {"V8": .8, "V5": .1, "cutoff": .4, "sustain": .5}),
        ("CRUSHED", {"V12": .6, "V1": .5}),
    ],
    "BAZZ": [
        ("BAZZLINE", {}),
        ("ACID", {"V1": S(0, 3), "fmodel": 1, "resonance": .8, "cutoff": .28, "vcf_env": .55, "vcf_dec": .36,
                  "vca_env": .55, "acc_amt": .7, "acc_sweep": .55, "slide_time": .33, "drive": .3, "keytrack": .3}),
        ("DB-01 SUB", {"V4": S(1, 2), "sub_lvl": .6, "cutoff": .35, "resonance": .3, "sustain": .5}),
        ("FM BASS", {"V1": S(1, 3), "fm_lvl": .5, "fm_ratio": 5 / 13, "cutoff": .5}),
        ("WAVE BASS", {"V1": S(2, 3), "V2": .6, "cutoff": .45}),
        ("DOUBLE", {"V7": .7, "V8": .3, "V9": S(3, 5)}),
        ("FOLD BASS", {"V10": .5, "V12": .3}),
        ("REESE", {"V7": .9, "V8": .25, "V9": S(2, 5), "detune": .3, "cutoff": .4, "sustain": .9}),
    ],
    "DRONE": [
        ("DRONE", {}),
        ("DARK DRONE", {"V1": .1, "V2": S(4, 6), "V4": S(1, 8), "V12": .4, "V9": .5}),
        ("FIFTHS", {"V4": S(2, 8), "V2": S(5, 6), "V11": .6}),
        ("MINOR CLOUD", {"V4": S(4, 8), "V6": .6, "V7": .2}),
        ("BEATING", {"V8": .7, "V4": S(0, 8), "V2": S(3, 6)}),
        ("SHIMMER", {"V10": .7, "V4": S(3, 8)}),
        ("CLUSTER", {"V4": S(7, 8), "V3": .5, "V12": .5}),
        ("SAW WALL", {"V1": .7, "V2": S(5, 6), "V3": .5, "V4": S(0, 8)}),
    ],
    "TEXTURE": [
        ("TEXTURE", {}),
        ("RAIN", {"V1": .9, "V2": .6, "V9": 0, "V11": .1}),
        ("WIND", {"V1": 0, "V9": .8, "V4": .4}),
        ("MACHINE", {"V1": .5, "V2": .7, "V6": .6, "V8": .4}),
        ("SPARKLES", {"V10": .8, "V1": .2}),
        ("UNDERWATER", {"V11": .7, "V4": .2, "V12": .7}),
        ("TUNED BED", {"V5": .8, "V3": 1}),
        ("STATIC", {"V8": .8, "V1": .1, "V4": .8}),
    ],
    "FX": [
        ("LASER", {}),
        ("ZAP", {"V1": S(1, 8)}),
        ("RISER", {"V1": S(2, 8), "V2": .8}),
        ("FALL", {"V1": S(3, 8), "V2": .7}),
        ("SIREN", {"V1": S(4, 8), "V2": .5}),
        ("ALARM", {"V1": S(5, 8), "V2": .4}),
        ("GLITCH", {"V1": S(6, 8), "V4": .5, "V8": .5}),
        ("BOMB", {"V1": S(7, 8), "V2": .7, "V9": .3}),
    ],
}
NUM_STARTERS = 8

NAMES = [v["name"] for v in VOICES]
DEEP = 12          # the controls an engine reads itself
CTRLS = 20


def play_row(v):
    """V13 .. V20: the same kinds of control on every voice, aimed at the voice's own"""
    (a, an), (b, bn) = v["env"]
    vi, vn = v["vel"]
    return [C("OCTAVE", d=2.5 / 5, steps=5), C("FINE", d=.5), C("VIBRATO"),
            C("ENV ATTACK", "ENV|ATTACK"), C("ENV DECAY", "ENV|DECAY", .4),
            C("ENV > " + v["ctrls"][a]["name"], "ENV>|" + an, .5), C("ENV > " + v["ctrls"][b]["name"], "ENV>|" + bn, .5),
            C("VEL > " + v["ctrls"][vi]["name"], "VEL>|" + vn, .5)]


def all_ctrls(v):
    return v["ctrls"] + play_row(v)


def labels():
    """every voice's twenty label-window texts (tools/panel_art.py)"""
    return [[c["label"] for c in all_ctrls(v)] for v in VOICES]


def cstr(s):
    return '"%s"' % s.replace("\\", "\\\\").replace('"', '\\"')


def main():
    L = ["/* GENERATED by tools/voices.py: do not edit. The voices' names and their twenty controls (src/engines.c",
         " * includes it inside ENG_DEFS). */"]
    for i, v in enumerate(VOICES):
        cs = all_ctrls(v)
        assert len(v["ctrls"]) == DEEP, v["name"]
        ups, copies = [], []
        for c in cs:
            u = c["up"]
            if isinstance(u, str) and u.startswith("="):
                copies.append(int(u[1:]))
                ups.append(c["def"])
            else:
                copies.append(-1)
                ups.append(c["def"] if u is None else u)
        (a, _), (b, _) = v["env"]
        L.append("    [%d] = {%s, %s," % (i, cstr(v["name"]), cstr(v["short"])))
        L.append("        {" + ", ".join(cstr(c["name"]) for c in cs) + "},")
        L.append("        {" + ", ".join(cstr(c["label"]) for c in cs) + "},")
        L.append("        {" + ", ".join("%.6ff" % c["def"] for c in cs) + "},")
        L.append("        %d, {" % v["timbre"] + ", ".join(str(c["steps"]) for c in cs) + "},")
        L.append("        {%d, %d}, {%d, %d, %d}," % (v["macros"][0], v["macros"][1], a, b, v["vel"][0]))
        L.append("        {" + ", ".join("%.6ff" % u for u in ups) + "},")
        L.append("        {" + ", ".join(str(c) for c in copies) + "}},")
    open(os.path.join(ROOT, "src", "voice_defs.h"), "w").write("\n".join(L) + "\n")
    # the starter sounds
    import gen_params
    keys = {key: sp for sp, key, *_ in gen_params.SOUND + gen_params.SOUND2}
    names, sets, most = [], [], 0
    for v in VOICES:
        st = STARTERS[v["name"]]
        assert len(st) == NUM_STARTERS, v["name"]
        names.append(st)
        for _, d in st:
            most = max(most, len(d))
    T = ["/* GENERATED by tools/voices.py: do not edit. Each voice's eight starter sounds (src/sound.c): the name and",
         " * what changes from the voice's home sound. */",
         "#define NUM_STARTERS %d" % NUM_STARTERS, "#define STARTER_SETS %d" % (most + 1),
         "static const char *const STARTER_NAMES[NUM_ENGINES][NUM_STARTERS] = {"]
    for st in names:
        T.append("    {" + ", ".join(cstr(n) for n, _ in st) + "},")
    T += ["};", "static const StarterSet STARTERS[NUM_ENGINES][NUM_STARTERS][STARTER_SETS] = {"]
    for st in names:
        T.append("    {")
        for n, d in st:
            items = []
            for k, val in d.items():
                sp = "SP_" + k if k.startswith("V") else "SP_" + keys[k]
                items.append("{%s, %.6ff}" % (sp, float(val)))
            T.append("        {" + ", ".join(items + ["{255, 0.f}"]) + "},")
        T.append("    },")
    T.append("};")
    open(os.path.join(ROOT, "src", "starter_defs.h"), "w").write("\n".join(T) + "\n")
    print("voices: %d, %d controls each" % (len(VOICES), CTRLS))


if __name__ == "__main__":
    main()
