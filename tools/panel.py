#!/usr/bin/env python3
"""LXRXXL ZYNTH's three pages: where every control of the artwork sits on MPC's plugin screen.

    python3 tools/panel.py --write     (writes layout.conf from this file)
    python3 tools/panel.py --check     (fails when layout.conf is stale)

The pages are made from the artwork in art/panel/ by Suns Kiefer (1000 x 625 each), filling MPC's whole 1280 x 628
plugin area:
    PANEL      front_bg.png stretched to the full screen (x 1.28, y 1.0048); the knobs stay round
    MODULES    modules_bg.png's top (the sound modules, y 0..418) at 1.28, with a big readout under it: the control
               last touched, its track and its value
    FX MIXER   modules_bg.png's bottom (the delay and the gate/duck, y 395..625) at 1.28, a readout, and the mixer: eight
               strips (PAN, DLY SEND, LEVEL, MUTE, SOLO) and MASTER, made from the artwork's slider boxes and caps
Other pictures used: knob_big.png / knob_encoder.png (the knobs), btn_lit_*.png (a lit key), thumb_v.png / thumb_h.png
(the slider and switch caps). The positions below are in the artwork's pixels; page() puts them on the screen.

layout.conf only carries the three tabs and their Q-Links for mpc-vst-plugins; tools/panel_art.py then builds every
control from the artwork. Q-Links: every parameter of a page, in reading order (top-left first, row by row), 16 per
Q-Link sub-page. Coordinates in layout.conf are Force Shadow pixels (y 86..714)."""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ART = os.path.join(ROOT, "art", "panel")
Y0 = 86
W, H = 1280, 628
ART_W, ART_H = 1000, 625

TABS = ["PANEL", "MODULES", "FX MIXER"]

# ---------------------------------------------------------------- each page: which artwork, what part, at what scale
# tab -> (picture, crop (x0, y0, x1, y1) in artwork pixels, sx, sy)
PAGES = {
    0: ("front_bg.png", (0, 0, 1000, 625), W / ART_W, H / ART_H),
    1: ("modules_bg.png", (0, 0, 1000, 418), 1.28, 1.28),
    2: ("modules_bg.png", (0, 395, 1000, 625), 1.28, 1.28),
}
# the parts of a crop that belong to the other page: their printing wiped (the panel's finish filled in)
MASKS = {
    1: [(383, 395, 94, 20), (0, 414, 1000, 4)],                     # DRY/WET (the delay's), the delay's labels' tops
    2: [(0, 395, 385, 24), (475, 395, 525, 24)],                    # the modules' last labels and slots above the delay
}
READOUT = {1: (0, 535, 1280, 93), 2: (0, 294, 1280, 44)}          # page px: the big readout strip
MIXER_Y = 338                                                      # page px: the mixer (FX MIXER), 290 px tall


def scale(tab):
    return PAGES[tab][2], PAGES[tab][3]


def page(tab, x, y):
    """artwork pixels -> page pixels (0..1280, 0..628)"""
    _, (cx0, cy0, _, _), sx, sy = PAGES[tab]
    return (x - cx0) * sx, (y - cy0) * sy


# ---------------------------------------------------------------- knobs: (tab, key, cx, cy, size, picture, ring)
# size: the knob picture's diameter in artwork pixels (as placed in the mockups); ring: the white segmented ring
# around it (the delay's TIME has its scale printed on the panel instead)
KNOBS = [
    (0, "drive", 98.5, 226.5, 130, "big", 1), (0, "cutoff", 896, 226, 130, "big", 1),
    (0, "detune", 244, 256.5, 98, "big", 1), (0, "vca_env", 373, 256, 86, "big", 1),
    (0, "vcf_env", 625, 257, 88, "big", 1), (0, "resonance", 753, 257, 98, "big", 1),
    (0, "fm_lvl", 62, 388, 98, "big", 1), (0, "sub_lvl", 191, 385, 98, "big", 1),
    (0, "noise_lvl", 311, 389, 98, "big", 1), (0, "lfo_rate", 682, 385, 98, "big", 1),
    (0, "lfo_lvl", 802, 383, 98, "big", 1), (0, "env_cv", 931, 385, 98, "big", 1),
    (0, "sel", 432, 424, 106, "encoder", 1), (0, "note", 563, 425, 106, "encoder", 1),
    (0, "dly_wet", 85, 100, 64, "big", 1), (0, "dly_time", 242, 120, 128, "big", 0),
    (0, "gd_rel", 712, 142, 72, "big", 1),
    # LXRXXL's randomizer amounts (their own knobs; the scale is printed around them: MOD_ART)
    (0, "srnd_amt", 370.3, 151.3, 50, "mod_yellow", 1), (0, "rnd_amt", 637.5, 151.3, 50, "mod_green", 1),
    # MODULES: FM RATIO (FM LVL's ratio, any voice) and RELEASE (the amp envelope's), under SLIDE TIME
    (1, "fm_ratio", 451.6, 217.2, 34, "big", 1), (1, "vca_rel", 506.3, 217.2, 34, "big", 1),
    (1, "acc_amt", 419, 88, 52, "big", 1), (1, "acc_sweep", 419, 161, 52, "big", 1),
    (1, "slide_time", 490, 125.5, 50, "big", 1), (1, "keytrack", 341, 382, 50, "big", 1),
    (1, "sw_rate", 494, 354, 48, "big", 1), (1, "detune", 577, 380, 50, "big", 1),
    (1, "sw_mix", 657, 356, 50, "big", 1),
    (2, "dly_time", 145.5, 519.5, 130, "big", 0), (2, "dly_in", 272, 467, 50, "big", 1),
    (2, "dly_fb", 344, 468, 50, "big", 1), (2, "dly_wet", 429, 452, 64, "big", 1),
    (2, "gd_depth", 621, 518, 72, "big", 1), (2, "gd_hold", 722, 517, 72, "big", 1),
    (2, "gd_atk", 818, 517, 72, "big", 1), (2, "gd_rel", 918, 517, 72, "big", 1),
]
KNOB_MAX = 126      # page px: a knob's 128-frame filmstrip stays under MPC's 16384 px (docs/NOTES.md, the framework's rule)

# ---------------------------------------------------------------- sliders (MODULES): key -> channel (x, y0, y1)
# the dark channel printed on the panel; the thumb runs along it, the top is the maximum (an option list: the first
# option at the top)
VSLIDERS = [("v1", 60, 48, 197), ("v2", 118, 48, 197), ("v3", 175, 48, 197),          # the voice's six sliders
            ("v4", 246, 48, 197), ("v5", 305, 48, 197), ("v6", 360, 48, 197),
            ("vcf_atk", 566, 44, 193), ("vcf_dec", 624, 44, 193), ("vca_atk", 681, 44, 193),
            ("vca_env", 738, 44, 193), ("env_cv", 797, 44, 193), ("pitch_dec", 854, 44, 193),
            ("level", 912, 44, 193), ("pan", 970, 44, 193)]
# the six voice sliders' printed labels are under the label windows (LABEL_WINDOWS): nothing to reprint
RELABEL_WIPE = []
RELABEL_WORDS = []
# MODULES: the label windows over the six voice sliders' legends, page pixels; each shows the voice's names
LABEL_WINDOWS = [(round(x * 1.28 - 33), 270, 66, 40) for _, x, _, _ in VSLIDERS[:6]]
# MODULES: what is wiped from the artwork (artwork pixels): the title MODULES (the VOICE selector's place) and the
# OSC MODE slider's VCO / FM / WAVE legends
MODULES_WIPE = [(30, 4, 150, 30), (0, 30, 48, 26), (0, 100, 40, 24), (0, 172, 46, 24)]
# the option sliders under the selector bars: key -> channel (x0, y, x1); the first option at the left
HSLIDERS = [("drive_pos", 86, 324.5, 151), ("curve", 74, 384, 169), ("fmodel", 310, 334.5, 375),
            ("sw_mode", 546, 333.5, 611), ("mod_wave", 749, 321, 910), ("mod_dest", 754, 392, 915)]
THUMB_V = (9, 17)       # the thumb on every slider (as in the mockups)
THUMB_SW = (8, 16)      # the toggle switches' thumb

# ---------------------------------------------------------------- selector bars (MODULES): key -> (x, y, w, h, n)
BARS = [("drive_pos", 25, 290, 182, 18, 2), ("curve", 28, 345, 183, 19, 3), ("fmodel", 249, 290, 182, 18, 2),
        ("sw_mode", 485, 289, 182, 18, 2), ("mod_wave", 721, 281, 213, 21, 5), ("mod_dest", 723, 354, 214, 21, 4)]
LABEL_OPTS = [("mod_sync", [(941, 292, 39, 14), (938, 391, 47, 12)])]
SYNC_SLOT = (961, 317, 381)       # x, y0, y1: the FREE / TEMPO switch
THUMB_SYNC = (17, 9)

# ---------------------------------------------------------------- two- and three-way switches
# (tab, key, slot thumb positions [(x, y)], tap zones [(x, y, w, h)] per option, orientation)
SWITCHES = [
    (0, "wave", [(442.5, 238), (442.5, 253), (442.5, 268)],
     [(430, 231, 66, 15), (430, 246, 66, 15), (430, 261, 66, 15)], "v"),
    (0, "fmode", [(526, 245), (526, 261)], [(514, 231, 52, 22), (514, 253, 52, 22)], "v"),
    (0, "gd_mode", [(849.5, 133.5), (903.5, 133.5)], [(764, 118, 112, 28), (876, 118, 112, 28)], "h"),
    (2, "gd_mode", [(842.5, 450.5), (896.5, 450.5)], [(756, 436, 113, 26), (869, 436, 113, 26)], "h"),
    (2, "dly_mode", [(292.5, 605.5), (346.5, 605.5)], [(218, 592, 102, 22), (320, 592, 122, 22)], "h"),
]

# ---------------------------------------------------------------- keys: key -> (tab, cx, cy, diameter of the glass)
KEYS = {}
for i, x in enumerate([385, 447, 507, 567, 630, 691, 752, 813]):
    KEYS["trk%d" % (i + 1)] = (0, x, 52, 45)
for k, x, y in [("link", 899, 51), ("mute", 960, 51), ("shift", 42, 471), ("play", 103, 471), ("stop", 167, 472),
                ("rec", 226, 472), ("barl", 712, 474), ("barr", 774, 474), ("octd", 835, 475), ("octu", 897, 475),
                ("tap", 960, 476)]:
    KEYS[k] = (0, x, y, 45)
for i, x in enumerate([39, 101, 164, 225, 287, 350, 411, 472, 534, 595, 658, 719, 781, 843, 905, 968]):
    KEYS["s%d" % (i + 1)] = (0, x, 545, 57)
for k, x, y in [("d_add", 270.5, 539.5), ("d_hold", 336.5, 540), ("d_rev", 410.5, 540), ("d_tap", 490.5, 539.5),
                ("gd_on", 886.5, 600.5)]:
    KEYS[k] = (2, x, y, 36)   # the coloured keys of the delay and the duck: printed lit, unlit made from them

# VOICE RESET (SHIFT: MUTE ALL): Suns Kiefer's button on its bezel, PANEL, artwork pixels (centre)
RESET_KEY = ("v_reset", 505, 175)                      # key centre (its touch area)
RESET_ART = {"btn_reset_bezel.png": (453, 123),          # top-left of each picture, as in Suns Kiefer's composite
             "btn_reset.png": (454, 130), "btn_reset_on.png": (454, 130)}

# ---------------------------------------------------------------- displays
DISPLAY_CELLS = [(427, 297), (480, 297), (533, 297)]   # PANEL: the three characters' top-left (40 x 54, dot included)
CELL_W, CELL_H = 40, 54
DISPLAY_BOX = (420, 289, 160, 70)                       # the window repainted under them
# the selectors shown in the display's font: a filmstrip each (turn, drag or the data wheel, both ways; a Q-Link)
TRACK_SEL = (1, 388, 3, 160, 31)                         # MODULES: "TRACK n" (art rect)
VOICE_SEL = (1, 98, 4, 168, 31)                          # MODULES: the VOICE selector, the voice's name (art rect)
VOICE_LABEL = (24, 6, 76, 38)                            # page rect: its legend "VOICE"
VOICE_KEYS = [("v_prev", 362, 24, 30), ("v_next", 398, 24, 30)]   # page: centre, size: VOICE < and VOICE >
# MODULES: printed legends (page rect, text, px) of the controls ZYNTH adds
MODULES_LABELS = [((546, 304, 64, 13), "FM RATIO", 8), ((616, 304, 64, 13), "RELEASE", 8)]
# PANEL: LXRXXL's randomizers beside the panic key (page pixels): each key's picture, centre and size; the scale
# printed round each amount knob; the legend ("?" for STEP RANDOM, the warning sign for SOUND RANDOM)
MOD_KEYS = [("srnd", "mod_key_yellow.png", 549, 176, 40), ("rnd", "mod_key_green.png", 744, 176, 40)]
MOD_ART = [("mod_label_question.png", 549, 140, 14), ("mod_label_warning.png", 744, 138, 30)]   # (the amount knobs: the
# white segmented ring of every other knob, not LXRXXL's coloured scales)
# MODULES: the other sliders' legends in label windows like the voice's (fixed text, page pixels)
FIXED_WINDOWS = [((round(x * 1.28 - 33), 270, 66, 40), t) for (_, x, _, _), t in
                 zip(VSLIDERS[6:], ["VCF|ATK", "VCF|DEC", "VCA|ATK", "VCA|DEC", "PITCH|AMT", "PITCH|DEC", "LEVEL", "PAN"])]
CH_SEL = (2, 661, 585, 40, 34)                             # FX MIXER: the duck's MIDI channel

# ---------------------------------------------------------------- presets (FX MIXER, page pixels)
# the slot number (the display's font) with its label, and the four keys: glass centre, label centre
PRESET_SLOT = (622, 26, 86, 42)
PRESET_SLOT_LABEL = (622, 6, 86, 16)
PRESET_KEYS = [("pr_sound", 753, 47, "SOUND", 753, 17), ("pr_perf", 817, 70, "PERF", 817, 40),
               ("pr_save", 873, 47, "SAVE", 873, 17), ("pr_load", 928, 70, "LOAD", 928, 40)]
PRESET_GLASS = 34

# ---------------------------------------------------------------- the mixer (FX MIXER, page pixels)
STRIP_W = 134
MIX_STRIPS = [8 + i * STRIP_W for i in range(8)]          # each strip's left edge
MASTER_X = 8 + 8 * STRIP_W                                # the MASTER strip: to the right edge
MIX = {   # inside a strip (x from its left edge, y from MIXER_Y)
    "num": (6, 4, 40, 34),            # the track's number, in the display's font
    "pan": (38, 88, 54),              # knob: cx, cy, size
    "snd": (38, 172, 54),
    "pan_label": (0, 42, 76, 14), "snd_label": (0, 126, 76, 14),
    "fader": (80, 22, 44, 206),       # the slider box (the artwork's LEVEL box): x, y, w, h
    "lvl_label": (70, 4, 64, 14),
    "mute": (4, 232, 62, 54), "solo": (68, 232, 62, 54),
}
MIX_MASTER = {"label": (MASTER_X, 4, 1280 - MASTER_X - 8, 18), "fader": (MASTER_X + 68, 28, 56, 254)}


# ---------------------------------------------------------------- Q-Links: every parameter of a page, in reading order
def controls(tab):
    """(key, page x, page y) of every parameter on a page (not the keys: they are pressed, not turned)"""
    out = []
    for t, key, cx, cy, *_ in KNOBS:
        if t == tab:
            out.append((key,) + page(tab, cx, cy))
    for t, key, th, zones, _ in SWITCHES:
        if t == tab:
            x, y, w, h = zones[0]
            out.append((key,) + page(tab, x + w / 2, y + h / 2))
    if tab == 1:
        for key, x, y0, y1 in VSLIDERS:
            out.append((key,) + page(tab, x, (y0 + y1) / 2))
        for key, x, y, w, h, n in BARS:
            out.append((key,) + page(tab, x + w / 2, y + h / 2))
        out.append(("mod_sync",) + page(tab, SYNC_SLOT[0], (SYNC_SLOT[1] + SYNC_SLOT[2]) / 2))
        t, x, y, w, h = TRACK_SEL
        out.append(("track",) + page(tab, x + w / 2, y + h / 2))
        t, x, y, w, h = VOICE_SEL
        out.append(("voice",) + page(tab, x + w / 2, y + h / 2))
    if tab == 2:
        t, x, y, w, h = CH_SEL
        out.append(("gd_ch",) + page(tab, x + w / 2, y + h / 2))
        x, y, w, h = PRESET_SLOT
        out.append(("pr_slot", x + w / 2, y + h / 2))
    seen, uniq = set(), []
    for c in out:
        if c[0] not in seen:
            seen.add(c[0])
            uniq.append(c)
    return uniq


ROW_GAP = {0: 48, 1: 110, 2: 48}   # page px: how far below a row's top a control still belongs to that row


def rows_of(items, row_gap):
    """rows from the top (a new row where the next control is row_gap below the row's first), each left to right"""
    items = sorted(items, key=lambda c: c[2])
    rows, cur, top = [], [], None
    for c in items:
        if cur and c[2] - top > row_gap:
            rows.append(cur)
            cur = []
        if not cur:
            top = c[2]
        cur.append(c)
    if cur:
        rows.append(cur)
    return [[c[0] for c in sorted(r, key=lambda c: (round(c[1]), c[2]))] for r in rows]


# MODULES, as the artwork reads (its tall sliders and the ACCENT column make one row, then the selectors under them)
MODULES_ROWS = [["voice", "track"],
                ["v1", "v2", "v3", "v4", "v5", "v6", "acc_amt", "acc_sweep", "slide_time", "fm_ratio", "vca_rel",
                 "vcf_atk", "vcf_dec", "vca_atk", "vca_env", "env_cv", "pitch_dec", "level", "pan"],
                ["drive_pos", "curve", "fmodel", "keytrack", "sw_rate", "sw_mode", "detune", "sw_mix", "mod_wave", "mod_dest", "mod_sync"]]

# FX MIXER's top: the delay's row (TIME's label is on it) then the duck's knobs, then the bottom switches
FX_ROWS = [["dly_time", "dly_in", "dly_fb", "dly_wet", "pr_slot", "gd_mode"], ["gd_depth", "gd_hold", "gd_atk", "gd_rel"],
           ["dly_mode", "gd_ch"]]


def pack(rows, n=16):
    """Q-Link sub-pages of up to n, in reading order; a row that fits on the next sub-page is not split"""
    pages = [[]]
    for r in rows:
        if len(pages[-1]) + len(r) > n and len(r) <= n and pages[-1]:
            pages.append([])
        for k in r:
            if len(pages[-1]) == n:
                pages.append([])
            pages[-1].append(k)
    return [p for p in pages if p]


def chunks(keys, n=8):
    return [keys[i:i + n] for i in range(0, len(keys), n)]


QLINK_FILE = os.path.join(ROOT, "qlinks.json")   # the Q-Link order (the Q-Link Map's format)
PAGE_IDS = ["panel", "modules", "fx"]


def mapped_keys(tab):
    """the controls a tab can put on a knob"""
    keys = [c[0] for c in controls(tab)]
    if tab == 2:
        keys += ["mix_%s%d" % (w, i) for w in ("lvl", "pan", "snd") for i in range(1, 9)] + ["mix_master"]
    return keys


def qlink_pages(tab):
    """the Q-Link pages of a tab: 8 slots each (one Force knob bank), from qlinks.json; "-" is an empty knob"""
    data = json.load(open(QLINK_FILE))
    knobs = data["pages"][PAGE_IDS[tab]]["knobs"]
    allowed = set(mapped_keys(tab))
    slots = []
    for k in sorted(knobs, key=lambda k: k["n"]):
        while len(slots) < k["n"] - 1:
            slots.append("-")
        key = k.get("key") or "-"
        if key != "-" and key not in allowed:
            raise SystemExit("qlinks.json: %s is not a control of %s" % (key, TABS[tab]))
        if key != "-" and key in slots:
            raise SystemExit("qlinks.json: %s is on two knobs of %s" % (key, TABS[tab]))
        slots.append(key)
    while slots and slots[-1] == "-":
        slots.pop()
    pages = [slots[i:i + 8] for i in range(0, len(slots), 8)]
    return [p for p in pages if any(k != "-" for k in p)]


def qlink_names(tab):
    """each Q-Link page's short name in MPC's page strip"""
    base = ["PANEL", "MODULES", "FX"][tab]
    return ["%s %d" % (base, i + 1) for i in range(len(qlink_pages(tab)))]


def layout_lines():
    """layout.conf: each tab with a knob per Q-Link control (panel_art.py replaces every control) and its Q-Links"""
    L = ["# LXRXXL ZYNTH -- page layout (mpc-vst-plugins layout.conf syntax: tools/shadow_skin.py).",
         "# GENERATED by tools/panel.py: edit that, then run `python3 tools/panel.py --write`. tools/panel_art.py then",
         "# builds every control of the three pages from the artwork (art/panel/).",
         "theme_bg=0e0e0e", "theme_panel=0e0e0e", "theme_ink=ececec", "theme_ink_dim=cfcfcf", "theme_accent=7dcdf5",
         "theme_knob_ring=7dcdf5", "theme_seg_active=7dcdf5", ""]
    for t, name in enumerate(TABS):
        L.append("[tab %s]" % name)
        qp = qlink_pages(t)
        for i, key in enumerate(k for p in qp for k in p if k != "-"):   # placeholders: a small grid (panel_art.py replaces them)
            L.append('knob cx=%d cy=%d r=12 label="" key=%s ns=0 bw=30' % (24 + 34 * (i % 36), Y0 + 24 + 40 * (i // 36), key))
        for i, p in enumerate(qp):   # each Q-Link page its own short name (MPC's page strip shows them)
            L.append('qlinks "%s" = %s' % (qlink_names(t)[i], ",".join(p)))
        L.append("")
    return L


if __name__ == "__main__":
    text = "\n".join(layout_lines()) + "\n"
    if "--write" in sys.argv:
        open(os.path.join(ROOT, "layout.conf"), "w").write(text)
        print("layout.conf written")
    elif "--check" in sys.argv:
        if open(os.path.join(ROOT, "layout.conf")).read() != text:
            raise SystemExit("layout.conf is stale: run python3 tools/panel.py --write")
    elif "--qlinks" in sys.argv:
        for t, name in enumerate(TABS):
            for i, p in enumerate(qlink_pages(t)):
                print("%s %d: %s" % (name, i + 1, " ".join(p)))
    else:
        sys.stdout.write(text)
