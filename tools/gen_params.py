#!/usr/bin/env python3
"""The single source of LXRXXL ZYNTH's plugin parameters: writes params.json (mpc-vst-plugins), src/param_ids.h and
src/param_map.h (what each parameter is inside the engine).

MPC stores a project's values by parameter index, so parameters are only ever appended to this list.

Kinds (param_map.h):
  SOUND  a sound parameter of the selected track (engine: Sound.v[SP_*]); the front panel and the MODULES page show
         the selected track, as the DB-01's knobs do
  GLOBAL a parameter of the whole instance (engine: Globals.v[G_*]): the delay and the gate/duck
  UI     the two encoders SELECT/GATE and BACK/NOTE, the selected track
  BUTTON a key: a text parameter whose "<key>_on" is its LED (the wrapper polls it); any value the host sends is a press
  SHOW   a read-only picture parameter the engine moves (the display's characters and dots)"""
import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# the display's characters, in the order of their pictures (art: tools/panel_art.py draws one picture per character)
GLYPHS = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-+/"

# the voices (tools/voices.py, src/voice_defs.h: the same order; new voices only ever appended)
import voices as _voices  # noqa: E402
VOICES = _voices.NAMES

# ---- the sound of a track: (SP id, VST key, name, default, options or None). SP ids are the engine's (bl.h).
SOUND = [
    ("DRIVE", "drive", "DRIVE", 0.1, None),
    ("SPREAD", "detune", "DETUNE", 0.0, None),          # the swarm's spread: the DB-01's DETUNE
    ("VCA_DEC", "vca_env", "DECAY", 0.0, None),         # the amp envelope's decay; fully down: held while the gate is
    ("VCF_ENV", "vcf_env", "VCF ENV", 0.35, None),      # the filter envelope's amount
    ("RESO", "resonance", "RESONANCE", 0.3, None),
    ("CUTOFF", "cutoff", "CUTOFF", 0.6, None),
    ("FM", "fm_lvl", "FM LVL", 0.0, None),
    ("SUB", "sub_lvl", "SUB LVL", 0.0, None),
    ("NOISE", "noise_lvl", "NOISE LVL", 0.0, None),
    ("LFO_RATE", "lfo_rate", "MOD RATE", 0.4, None),    # the modulator's (MODULES: MODULATOR)
    ("LFO_LVL", "lfo_lvl", "MOD DEPTH", 0.0, None),
    ("PITCH_AMT", "env_cv", "ENV/CV LVL", 0.0, None),   # the pitch envelope's amount (MODULES: PITCH AMT)
    ("WAVE", "wave", "WAVE", 1, ["SQR", "SAW", "TRI"]), # BAZZ's VCO wave; the other voices' SUB wave
    ("FMODE", "fmode", "FILTER", 1, ["BP", "LP"]),
    # the MODULES page: the voice and its six sliders (their meaning is the voice's: src/engines.c)
    ("VOICE", "voice", "VOICE", 0, VOICES),
    ("V1", "v1", "VOICE CTRL 1", 0.0, None),
    ("V2", "v2", "VOICE CTRL 2", 0.0, None),
    ("V3", "v3", "VOICE CTRL 3", 0.0, None),
    ("V4", "v4", "VOICE CTRL 4", 0.0, None),
    ("V5", "v5", "VOICE CTRL 5", 0.0, None),
    ("V6", "v6", "VOICE CTRL 6", 0.0, None),
    ("FM_RATIO", "fm_ratio", "FM RATIO", 0.25, None),
    ("DRIVE_POS", "drive_pos", "DRIVE POS", 0, ["PRE", "POST"]),
    ("CURVE", "curve", "CURVE", 0, ["SOFT", "HARD", "FOLD"]),
    ("ACC_AMT", "acc_amt", "ACCENT", 0.5, None),
    ("ACC_SWEEP", "acc_sweep", "SWEEP", 0.35, None),
    ("SLIDE", "slide_time", "SLIDE TIME", 0.3, None),
    ("VCF_ATK", "vcf_atk", "VCF ATK", 0.0, None),
    ("VCF_DEC", "vcf_dec", "VCF DEC", 0.45, None),
    ("VCA_ATK", "vca_atk", "ATTACK", 0.0, None),
    ("PITCH_DEC", "pitch_dec", "PITCH DEC", 0.3, None),
    ("LEVEL", "level", "LEVEL", 0.8, None),
    ("PAN", "pan", "PAN", 0.5, None),
    ("FMODEL", "fmodel", "FILTER MODEL", 0, ["POLIVOKS", "DIODE"]),
    ("KEYTRACK", "keytrack", "KEY TRACK", 0.5, None),
    ("SW_MODE", "sw_mode", "SWARM MODE", 0, ["BBD", "UNISON"]),
    ("SW_RATE", "sw_rate", "SWARM RATE", 0.3, None),
    ("SW_MIX", "sw_mix", "SWARM MIX", 0.6, None),
    ("MOD_WAVE", "mod_wave", "MOD WAVE", 0, ["TRI", "SAW", "SQR", "S&H", "NOISE"]),
    ("MOD_DEST", "mod_dest", "MOD DEST", 1, ["PITCH", "CUTOFF", "DRIVE", "WAVE"]),
    ("MOD_SYNC", "mod_sync", "MOD SYNC", 0, ["FREE", "TEMPO"]),
    ("VCA_REL", "vca_rel", "RELEASE", 0.2, None),       # the amp envelope's release (after the gate closes)
]

# 0.2.0: sound parameters added later (appended to the parameters' end; their SP ids follow SOUND's)
SOUND2 = [
    ("SUSTAIN", "sustain", "SUSTAIN", 1.0, None),       # the amp envelope's sustain level (ADSR)
] + [("V%d" % i, "v%d" % i, "VOICE CTRL %d" % i, 0.0, None) for i in range(7, 21)] + [   # the VOICE page's second row
    ("VCF_SUS", "vcf_sus", "VCF SUSTAIN", 0.0, None),   # the filter envelope's sustain and release (its ADSR)
    ("VCF_REL", "vcf_rel", "VCF RELEASE", 0.3, None),
]


def sp_order():
    """the engine's order of the sound parameters (Sound.v[]): V1 .. V20 together; the parameters' order is MPC's"""
    rows = SOUND + SOUND2
    v = [r for r in rows if r[0] in ["V%d" % i for i in range(1, 21)]]
    rest = [r for r in rows if r not in v]
    i = [r[0] for r in rest].index("FM_RATIO")
    return rest[:i] + v + rest[i:]

# 0.2.0: CHAOS (a Lorenz attractor modulating every track) and SHIFT (a frequency shifter / ring modulator on a send
# bus), in place of the gate/duck on the pages (the gate/duck's parameters stay, unused, so MPC's indices stay)
GLOBAL2 = [
    ("CHAOS_RATE", "chaos_rate", "CHAOS RATE", 0.4, None),
    ("CHAOS_WILD", "chaos_wild", "CHAOS WILD", 0.6, None),
    ("CHAOS_DEST", "chaos_dest", "CHAOS DEST", 1, ["PITCH", "CUTOFF", "TIMBRE", "PAN", "ALL"]),
    ("CHAOS_DEPTH", "chaos_depth", "CHAOS", 0.0, None),
    ("SHIFT_FREQ", "shift_freq", "SHIFT FREQ", 0.62, None),
    ("SHIFT_FB", "shift_fb", "SHIFT FEEDBACK", 0.0, None),
    ("SHIFT_MODE", "shift_mode", "SHIFT MODE", 0, ["SHIFT", "RING"]),
    ("SHIFT_MIX", "shift_mix", "SHIFT MIX", 0.0, None),
]
# 0.2.0: each track's CHAOS amount and SHIFT send (the MIXER), after MIX's three rows (ids 32.., 40..)
MIX2 = [("chs", "CHAOS", 1.0), ("shs", "SHIFT SEND", 1.0)]

# ---- the instance: the delay and the gate/duck
GLOBAL = [
    ("DLY_TIME", "dly_time", "DELAY TIME", 0.5, None),
    ("DLY_IN", "dly_in", "DELAY IN LVL", 0.7, None),
    ("DLY_FB", "dly_fb", "FEEDBACK", 0.35, None),
    ("DLY_WET", "dly_wet", "DRY/WET", 0.0, None),
    ("DLY_MODE", "dly_mode", "DELAY MODE", 0, ["TAPE", "DIGITAL"]),
    ("GD_MODE", "gd_mode", "GATE/DUCK", 1, ["GATE", "DUCK"]),
    ("GD_DEPTH", "gd_depth", "DEPTH", 0.7, None),
    ("GD_HOLD", "gd_hold", "HOLD", 0.15, None),
    ("GD_ATK", "gd_atk", "ATTACK", 0.1, None),
    ("GD_REL", "gd_rel", "RELEASE", 0.35, None),
    ("GD_CH", "gd_ch", "DUCK MIDI CH", 9, [str(i + 1) for i in range(16)]),
]

# ---- the keys: (key, name) -- the front panel, then the MODULES page's
BUTTONS = [("trk%d" % (i + 1), "TRACK %d" % (i + 1)) for i in range(8)] + \
          [("link", "LINK"), ("mute", "MUTE"), ("shift", "SHIFT"), ("play", "PLAY"), ("stop", "STOP"), ("rec", "REC"),
           ("barl", "BAR <"), ("barr", "BAR >"), ("octd", "-1 OCT"), ("octu", "+1 OCT"), ("tap", "TAP")] + \
          [("s%d" % (i + 1), n) for i, n in enumerate(["ACC", "SLIDE", "MOD", "PITCH ENV", "LAST STEP", "PLAY MODE",
                                                       "TRANSP", "SCALE", "ARPEGG", "LFO", "RND PATT", "CLEAR",
                                                       "COPY", "PASTE", "BANK", "PATTERN"])] + \
          [("d_add", "ADD"), ("d_hold", "HOLD"), ("d_rev", "REVERSE"), ("d_tap", "TAP SYNC"), ("gd_on", "DUCK ON/OFF"),
           ("trk_next", "NEXT TRACK"), ("ch_next", "NEXT DUCK CH")] + \
          [("mute%d" % (i + 1), "MUTE %d" % (i + 1)) for i in range(8)] + \
          [("solo%d" % (i + 1), "SOLO %d" % (i + 1)) for i in range(8)]   # 0.2.0: the MIXER's keys

# ---- the MIXER (0.2.0): per track level, pan and delay send (any track, not only the selected one), the master
MIX = [("lvl", "LEVEL", 0.8), ("pan", "PAN", 0.5), ("snd", "DLY SEND", 1.0)]


def glyph_names():
    return ["_" if c == " " else c for c in GLYPHS]


# appended after the mixer (MPC keeps automation by index): the preset keys
BUTTONS2 = [("pr_sound", "SOUND"), ("pr_perf", "PERF"), ("pr_save", "SAVE"), ("pr_load", "LOAD"),
            ("v_reset", "VOICE RESET"),   # the selected track's sound back to the default; SHIFT: mute all
            ("v_prev", "VOICE <"), ("v_next", "VOICE >"),          # MODULES: the selected track's voice
            ("rnd", "SOUND RANDOM"), ("srnd", "STEP RANDOM")]       # PANEL: the randomizers (from LXRXXL)
BUTTONS3 = [("snd_prev", "SOUND <"), ("snd_next", "SOUND >")]   # 0.2.0, the VOICE page: the voice's starter sounds
AMOUNTS = ["%d%%" % (i * 10) for i in range(11)]
NUM_SLOTS = 64


def params():
    out = []
    for sp, key, name, d, opts in SOUND:
        e = {"key": key, "name": name, "default": d, "kind": "SOUND", "id": "SP_" + sp}
        if opts:
            e["options"] = opts
        else:
            e.update({"min": 0, "max": 1})
        out.append(e)
    out.append({"key": "sel", "name": "SELECT/GATE", "min": 0, "max": 1, "default": 0, "kind": "UI", "id": "UI_SEL"})
    out.append({"key": "note", "name": "BACK/NOTE", "min": 0, "max": 1, "default": 0.5, "kind": "UI", "id": "UI_NOTE"})
    for g, key, name, d, opts in GLOBAL:
        e = {"key": key, "name": name, "default": d, "kind": "GLOBAL", "id": "G_" + g}
        if opts:
            e["options"] = opts
        else:
            e.update({"min": 0, "max": 1})
        out.append(e)
    out.append({"key": "track", "name": "TRACK", "options": [str(i + 1) for i in range(8)], "default": 0,
                "kind": "UI", "id": "UI_TRACK"})
    for i in range(3):
        out.append({"key": "disp%d" % (i + 1), "name": "DISPLAY %d" % (i + 1), "options": glyph_names(), "default": 0,
                    "kind": "SHOW", "id": "SH_DISP%d" % (i + 1)})
    for i in range(3):
        out.append({"key": "dot%d" % (i + 1), "name": "DOT %d" % (i + 1), "options": ["off", "on"], "default": 0,
                    "kind": "SHOW", "id": "SH_DOT%d" % (i + 1)})
    for k, n in BUTTONS:
        out.append({"key": k, "name": n, "display": "string", "min": 0, "max": 1, "default": 0, "kind": "BUTTON",
                    "id": "B_" + k.upper()})
    # 0.2.0
    for w, (k, n, d) in enumerate(MIX):
        for t in range(8):
            out.append({"key": "mix_%s%d" % (k, t + 1), "name": "%s %d" % (n, t + 1), "min": 0, "max": 1, "default": d,
                        "kind": "MIX", "id": str(w * 8 + t)})
    out.append({"key": "mix_master", "name": "MASTER", "min": 0, "max": 1, "default": 0.8, "kind": "MIX", "id": "24"})
    out.append({"key": "readout", "name": "READOUT", "display": "string", "min": 0, "max": 1, "default": 0,
                "kind": "SHOW", "id": "SH_READOUT"})
    # 0.2.0: presets (sounds and performances on the device's storage)
    out.append({"key": "pr_slot", "name": "PRESET SLOT", "options": ["%02d" % (i + 1) for i in range(NUM_SLOTS)],
                "default": 0, "kind": "UI", "id": "UI_SLOT"})
    for k, n in BUTTONS2:
        out.append({"key": k, "name": n, "display": "string", "min": 0, "max": 1, "default": 0, "kind": "BUTTON",
                    "id": "B_" + k.upper()})
    out.append({"key": "rnd_amt", "name": "SOUND RND AMT", "options": AMOUNTS, "default": 5, "kind": "UI",
                "id": "UI_RND_AMT"})
    out.append({"key": "srnd_amt", "name": "STEP RND AMT", "options": AMOUNTS, "default": 5, "kind": "UI",
                "id": "UI_SRND_AMT"})
    # 0.2.0: the front panel's TIMBRE and MORPH: the two sliders that change the selected track's voice the most
    # (src/engines.c ENG_MACROS); they move V1..V6, so MPC's automation of them follows the voice
    out.append({"key": "timbre", "name": "TIMBRE", "min": 0, "max": 1, "default": 0.5, "kind": "UI", "id": "UI_TIMBRE"})
    out.append({"key": "morph", "name": "MORPH", "min": 0, "max": 1, "default": 0.5, "kind": "UI", "id": "UI_MORPH"})
    # 0.2.0: SHOW the VOICE page's SOUND name (a starter sound of the voice)
    out.append({"key": "snd_name", "name": "SOUND NAME", "display": "string", "min": 0, "max": 1, "default": 0,
                "kind": "SHOW", "id": "SH_SNDNAME"})
    for k, n in BUTTONS3:
        out.append({"key": k, "name": n, "display": "string", "min": 0, "max": 1, "default": 0, "kind": "BUTTON",
                    "id": "B_" + k.upper()})
    for sp, key, name, d, opts in SOUND2:
        e = {"key": key, "name": name, "default": d, "kind": "SOUND", "id": "SP_" + sp}
        if opts:
            e["options"] = opts
        else:
            e.update({"min": 0, "max": 1})
        out.append(e)
    for g, key, name, d, opts in GLOBAL2:
        e = {"key": key, "name": name, "default": d, "kind": "GLOBAL", "id": "G_" + g}
        if opts:
            e["options"] = opts
        else:
            e.update({"min": 0, "max": 1})
        out.append(e)
    for w, (k, n, d) in enumerate(MIX2):
        for t in range(8):
            out.append({"key": "mix_%s%d" % (k, t + 1), "name": "%s %d" % (n, t + 1), "min": 0, "max": 1, "default": d,
                        "kind": "MIX", "id": str((4 + w) * 8 + t)})
    return out


def main():
    ps = params()
    json.dump({"params": [{k: v for k, v in p.items() if k not in ("kind", "id")} for p in ps]},
              open(os.path.join(ROOT, "params.json"), "w"), indent=1)
    L = ["/* GENERATED by tools/gen_params.py: do not edit. The plugin's parameters, in VST index order. */",
         "#pragma once", "enum {"]
    for i, p in enumerate(ps):
        L.append("    P_%s = %d," % (p["key"].upper(), i))
    L += ["    P_COUNT = %d" % len(ps), "};",
          "static const char *const PARAM_KEYS[P_COUNT] __attribute__((unused)) = {" + ", ".join('"%s"' % p["key"] for p in ps) + "};"]
    open(os.path.join(ROOT, "src/param_ids.h"), "w").write("\n".join(L) + "\n")

    M = ["/* GENERATED by tools/gen_params.py: do not edit. What each plugin parameter is inside the engine. */",
         "#pragma once", "#include \"bl.h\"", "#include \"param_ids.h\"", "#define UNUSED __attribute__((unused))", "",
         "enum { PK_SOUND, PK_GLOBAL, PK_UI, PK_BUTTON, PK_SHOW, PK_MIX };",
         "enum { UI_SEL, UI_NOTE, UI_TRACK, UI_SLOT, UI_RND_AMT, UI_SRND_AMT, UI_TIMBRE, UI_MORPH };",
         "enum { SH_DISP1, SH_DISP2, SH_DISP3, SH_DOT1, SH_DOT2, SH_DOT3, SH_READOUT, SH_SNDNAME };", ""]
    M += ["typedef struct { uint8_t kind, id, nopts; } ParamMap;",
          "static const ParamMap PARAM_MAP[P_COUNT] UNUSED = {"]
    for p in ps:
        M.append("    {PK_%s, %s, %d},   /* %s */" % (p["kind"], p["id"], len(p.get("options") or []), p["key"]))
    M += ["};"]
    # the sound and global tables: VST key, name and option labels per id (display, serialisation)
    M += ["", "typedef struct { const char *key, *name; float def; uint8_t nopts; const char *const *opts; } SoundDef;"]
    for tab, rows, pfx in (("SOUND_DEFS", sp_order(), "SP_"), ("GLOBAL_DEFS", GLOBAL + GLOBAL2, "G_")):
        for sp, key, name, d, opts in rows:
            if opts:
                M.append("static const char *const OPTS_%s%s[] UNUSED = {%s};" % (pfx, sp, ", ".join('"%s"' % o for o in opts)))
        M.append("static const SoundDef %s[] UNUSED = {" % tab)
        for sp, key, name, d, opts in rows:
            M.append('    {"%s", "%s", %sf, %d, %s},' % (key, name, float(d), len(opts or []),
                                                       ("OPTS_%s%s" % (pfx, sp)) if opts else "0"))
        M.append("};")
    open(os.path.join(ROOT, "src/param_map.h"), "w").write("\n".join(M) + "\n")

    # the engine's id lists (bl.h includes these)
    E = ["/* GENERATED by tools/gen_params.py: do not edit. */", "#pragma once", "enum {"]
    E += ["    SP_%s," % sp for sp, *_ in sp_order()] + ["    SP_COUNT", "};", "#define SP_COUNT_V1 %d   /* 0.1.0's */" % len(SOUND)]
    E += ["/* 0.1.0's order of the sound parameters (its Sound.v[]): each one's SP id now */",
          "#define SP_FROM_V1 {" + ", ".join("SP_" + r[0] for r in SOUND) + "}", "enum {"]
    E += ["    G_%s," % g for g, *_ in GLOBAL + GLOBAL2] + ["    G_COUNT", "};", "#define G_COUNT_V1 %d   /* 0.1.0's */" % len(GLOBAL), "enum {"]
    E += ["    B_%s = %d," % (k.upper(), i) for i, (k, _) in enumerate(BUTTONS + BUTTONS2 + BUTTONS3)]
    E += ["    NUM_BUTTONS = %d" % len(BUTTONS + BUTTONS2 + BUTTONS3), "};", "#define NUM_SLOTS %d" % NUM_SLOTS, "#define GLYPHS \"%s\"" % GLYPHS, "#define NUM_GLYPHS %d" % len(GLYPHS)]
    open(os.path.join(ROOT, "src/ids.h"), "w").write("\n".join(E) + "\n")
    print("params: %d (%d sound + %d, %d global, %d keys)" % (len(ps), len(SOUND), len(SOUND2), len(GLOBAL), len(BUTTONS + BUTTONS2)))


if __name__ == "__main__":
    main()
