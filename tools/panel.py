#!/usr/bin/env python3
"""LXRXXL ZYNTH's four pages: where every control of the artwork sits on MPC's plugin screen.

    python3 tools/panel.py --write     (writes layout.conf from this file)
    python3 tools/panel.py --check     (fails when layout.conf is stale)

The pages are made from the artwork in art/panel/ by Suns Kiefer (1000 x 625 each), filling MPC's whole 1280 x 628
plugin area:
    PANEL      front_bg.png stretched to the full screen (x 1.28, y 1.0048); the knobs stay round
    VOICE      (0.2.0) modules_bg.png's fader row (y 0..242) and below it the same faders again (its rows 32..245, at
               y 242..455), at 1.28: the voice's twenty controls and the amp and filter ADSRs; the centre column the
               ACCENT knobs above, the SOUND selector and the pitch envelope below; a readout at the bottom
    FX MOD     (0.2.0) modules_bg.png's bottom (the delay and the gate/duck, y 395..625) over its sound modules (DRIVE,
               FILTER, SWARM, MODULATOR: y 248..418), at 1.28, and a readout
    MIXER      (0.2.0) the vein texture and eight strips (the track, PAN, DLY SEND, LEVEL, MUTE and SOLO the size of the
               step keys) and MASTER
The VOICE and FX MOD pictures are put together by tools/panel_art.py (compose()); positions on them are given here in
their own pixels: fx(y) and mods(y) move the delay's (modules_bg.png y 395..) and the modules' (y 248..) there.
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

TABS = ["PANEL", "VOICE", "FX MOD", "MIXER"]


def fx(y):
    """modules_bg.png's delay / duck (y 395..625) -> the FX MOD picture's top"""
    return y - 395


def mods(y):
    """modules_bg.png's sound modules (y 248..418) -> the FX MOD picture's bottom (from y 230)"""
    return y - 248 + 230


ROW2 = 242 - 32     # the VOICE page's second fader row: modules_bg.png's rows 32.. copied 210 px lower

# ---------------------------------------------------------------- each page: which artwork, what part, at what scale
# tab -> (picture, crop (x0, y0, x1, y1) in artwork pixels, sx, sy)
PAGES = {
    0: ("front_bg.png", (0, 0, 1000, 625), W / ART_W, H / ART_H),
    1: ("voice", (0, 0, 1000, 456), 1.28, 1.28),     # composed (panel_art.compose)
    2: ("fxmod", (0, 0, 1000, 400), 1.28, 1.28),     # composed
    3: ("mixer_bg.png", (0, 0, 1000, 491), 1.28, 1.28),
}
# the parts of modules_bg.png that are wiped before the pages are put together (artwork pixels)
MASKS = {
    1: [],
    2: [(383, 395, 94, 20)],
}
VOICE_CENTRE = (380, 242, 168, 214)   # VOICE: the second row's centre column (its copy of the ACCENT knobs), cleared
READOUT = {1: (0, 586, 1280, 42), 2: (0, 516, 1280, 108), 3: (0, 586, 1280, 42)}   # page px: the readout strip
MIXER_Y = 4                                                        # page px: the mixer (MIXER)


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
    # 0.2.0, the performance front: the voice's TIMBRE and MORPH where DETUNE and VCA ENV were, the amp envelope
    # (ATTACK, DECAY, RELEASE) where FM, SUB and NOISE were, MOD RATE / MOD DEPTH and SUB on the right
    (0, "timbre", 244, 256.5, 98, "big", 1), (0, "morph", 373, 256, 86, "big", 1),
    (0, "vcf_env", 625, 257, 88, "big", 1), (0, "resonance", 753, 257, 98, "big", 1),
    (0, "vca_atk", 52, 388, 76, "big", 1), (0, "vca_env", 138, 388, 76, "big", 1),
    (0, "sustain", 224, 388, 76, "big", 1), (0, "vca_rel", 310, 388, 76, "big", 1),
    (0, "lfo_rate", 682, 385, 98, "big", 1),
    (0, "lfo_lvl", 802, 383, 98, "big", 1), (0, "sub_lvl", 931, 385, 98, "big", 1),
    (0, "sel", 432, 424, 106, "encoder", 1), (0, "note", 563, 425, 106, "encoder", 1),
    (0, "dly_wet", 85, 100, 64, "big", 1), (0, "dly_time", 242, 120, 128, "big", 0),
    (0, "gd_rel", 712, 142, 72, "big", 1),
    # LXRXXL's randomizer amounts (their own knobs; the scale is printed around them: MOD_ART)
    (0, "srnd_amt", 370.3, 151.3, 50, "mod_yellow", 1), (0, "rnd_amt", 637.5, 151.3, 50, "mod_green", 1),
    # VOICE: the ACCENT column (top), FM RATIO and FM LVL under SLIDE TIME; the pitch envelope under the SOUND selector
    (1, "fm_ratio", 432.0, 218.75, 40, "big", 1), (1, "fm_lvl", 497.7, 218.75, 40, "big", 1),
    (1, "acc_amt", 419, 88, 52, "big", 1), (1, "acc_sweep", 419, 161, 52, "big", 1),
    (1, "slide_time", 490, 125.5, 50, "big", 1),
    (1, "env_cv", 425, 375, 46, "big", 1), (1, "pitch_dec", 504, 375, 46, "big", 1),
    # FX MOD: the sound modules' knobs (NOISE beside KEY TRACK) and the delay's and the duck's
    (2, "noise_lvl", 412, mods(374), 42, "big", 1), (2, "keytrack", 341, mods(382), 50, "big", 1),
    (2, "sw_rate", 494, mods(354), 48, "big", 1), (2, "detune", 577, mods(371), 44, "big", 1),
    (2, "sw_mix", 657, mods(356), 50, "big", 1),
    (2, "dly_time", 145.5, fx(519.5), 130, "big", 0), (2, "dly_in", 272, fx(467), 50, "big", 1),
    (2, "dly_fb", 344, fx(468), 50, "big", 1), (2, "dly_wet", 429, fx(452), 64, "big", 1),
    (2, "gd_depth", 621, fx(518), 72, "big", 1), (2, "gd_hold", 722, fx(517), 72, "big", 1),
    (2, "gd_atk", 818, fx(517), 72, "big", 1), (2, "gd_rel", 918, fx(517), 72, "big", 1),
]
KNOB_MAX = 126      # page px: a knob's 128-frame filmstrip stays under MPC's 16384 px (docs/NOTES.md, the framework's rule)

# ---------------------------------------------------------------- sliders (MODULES): key -> channel (x, y0, y1)
# the dark channel printed on the panel; the thumb runs along it, the top is the maximum (an option list: the first
# option at the top)
LEFT_X = [60, 118, 175, 246, 305, 360]                       # the six faders on the left
RIGHT_X = [566, 624, 681, 738, 797, 854, 912, 970]           # the eight on the right
VSLIDERS = ([("v%d" % (i + 1), x, 48, 197) for i, x in enumerate(LEFT_X)] +              # V1 .. V6
            # the amp's and the filter's ADSR (the same on every voice)
            [(k, x, 44, 193) for k, x in zip(["vca_atk", "vca_env", "sustain", "vca_rel",
                                              "vcf_atk", "vcf_dec", "vcf_sus", "vcf_rel"], RIGHT_X)] +
            # the second row: V7 .. V12 on the left, V13 .. V20 on the right
            [("v%d" % (i + 7), x, 48 + ROW2, 197 + ROW2) for i, x in enumerate(LEFT_X)] +
            [("v%d" % (i + 13), x, 44 + ROW2, 193 + ROW2) for i, x in enumerate(RIGHT_X)])
# the voice's faders' printed labels are under the label windows: nothing to reprint
RELABEL_WIPE = []
RELABEL_WORDS = []
# VOICE: the label windows over the voice's twenty faders (V1..V20 in order), page pixels; each shows the voice's names
LABEL_WINDOWS = ([(round(x * 1.28 - 33), 270, 66, 40) for x in LEFT_X] +
                 [(round(x * 1.28 - 33), round(270 + ROW2 * 1.28), 66, 40) for x in LEFT_X] +
                 [(round(x * 1.28 - 33), round(270 + ROW2 * 1.28), 66, 40) for x in RIGHT_X])
# MODULES: what is wiped from the artwork (artwork pixels): the title MODULES (the VOICE selector's place) and the
# OSC MODE slider's VCO / FM / WAVE legends
MODULES_WIPE = [(30, 4, 150, 30), (0, 30, 48, 26), (0, 100, 40, 24), (0, 172, 46, 24)]
# the option sliders under the selector bars: key -> channel (x0, y, x1); the first option at the left
HSLIDERS = [("drive_pos", 86, mods(324.5), 151), ("curve", 74, mods(384), 169), ("fmodel", 310, mods(334.5), 375),
            ("sw_mode", 546, mods(333.5), 611), ("mod_wave", 749, mods(321), 910), ("mod_dest", 754, mods(392), 915)]
THUMB_V = (9, 17)       # the thumb on every slider (as in the mockups)
THUMB_SW = (8, 16)      # the toggle switches' thumb

# ---------------------------------------------------------------- selector bars (MODULES): key -> (x, y, w, h, n)
BARS = [("drive_pos", 25, mods(290), 182, 18, 2), ("curve", 28, mods(345), 183, 19, 3),
        ("fmodel", 249, mods(290), 182, 18, 2), ("sw_mode", 485, mods(289), 182, 18, 2),
        ("mod_wave", 721, mods(281), 213, 21, 5), ("mod_dest", 723, mods(354), 214, 21, 4)]
LABEL_OPTS = [("mod_sync", [(941, mods(292), 39, 14), (938, mods(391), 47, 12)])]
SYNC_SLOT = (961, mods(317), mods(381))       # x, y0, y1: the FREE / TEMPO switch
SYNC_ZONES = [(938, mods(285), 54, 64), (938, mods(349), 54, 60)]
SYNC_CELLS = [(936, mods(290), 50, 18), (934, mods(388), 54, 16)]
THUMB_SYNC = (17, 9)

# ---------------------------------------------------------------- two- and three-way switches
# (tab, key, slot thumb positions [(x, y)], tap zones [(x, y, w, h)] per option, orientation)
SWITCHES = [
    (0, "wave", [(442.5, 238), (442.5, 253), (442.5, 268)],
     [(430, 231, 66, 15), (430, 246, 66, 15), (430, 261, 66, 15)], "v"),
    (0, "fmode", [(526, 245), (526, 261)], [(514, 231, 52, 22), (514, 253, 52, 22)], "v"),
    (0, "gd_mode", [(849.5, 133.5), (903.5, 133.5)], [(764, 118, 112, 28), (876, 118, 112, 28)], "h"),
    (2, "gd_mode", [(842.5, fx(450.5)), (896.5, fx(450.5))], [(756, fx(436), 113, 26), (869, fx(436), 113, 26)], "h"),
    (2, "dly_mode", [(292.5, fx(605.5)), (346.5, fx(605.5))], [(218, fx(592), 102, 22), (320, fx(592), 122, 22)], "h"),
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
    KEYS[k] = (2, x, fx(y), 36)   # the coloured keys of the delay and the duck: printed lit, unlit made from them

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
VOICE_SEL = (1, 98, 1, 168, 31)                          # MODULES: the VOICE selector, the voice's name (art rect)
VOICE_LABEL = (24, 2, 76, 38)                            # page rect: its legend "VOICE"
VOICE_KEYS = [("v_prev", 368, 21, 30), ("v_next", 422, 21, 30)]   # page: centre, size: VOICE < and VOICE >
VOICE_KEY_TOUCH = (50, 42)                               # their touch area (page px), larger than the key
# VOICE (0.2.0): the SOUND selector in the second row's centre: its legend, the name (MPC's text: the "snd_name"
# parameter), < and > (page pixels)
SOUND_LABEL = (497, 330, 194, 16)
SOUND_NAME = (497, 348, 194, 40)
SOUND_KEYS = [("snd_prev", 545, 414, 30), ("snd_next", 643, 414, 30)]
# VOICE: the pitch envelope's legends under its knobs (page rect, text, cap)
PITCH_LABELS = [((505, 510, 80, 14), "PITCH|AMT", 11), ((605, 510, 80, 14), "PITCH|DEC", 11)]
# MODULES (0.2.0): the artwork's small legends reprinted bigger: (the printed word's ink box, artwork px), the word,
# its new centre (artwork px; None: where it was), its cap height (page px). Their boxes are wiped (clean circuit).
# The ink boxes are modules_bg.png's (wiped there); the centres are the page's own picture (VOICE: the same; FX MOD:
# mods()). (tab, ink box, word, centre or None, cap)
MODULES_REPRINT = [
    (2, (90, 278, 144, 283), "POSITION", None, 12), (2, (35, 334, 65, 339), "CURVE", None, 12),
    (2, (310, 278, 349, 283), "MODEL", None, 12), (2, (483, 278, 516, 283), "MODE", None, 12),
    (2, (719, 267, 758, 273), "WAVE", None, 12), (2, (725, 341, 755, 346), "DEST", None, 12),
    (1, (392, 119, 448, 125), "AMOUNT", None, 12), (1, (399, 191, 445, 197), "SWEEP", None, 12),
    (2, (480, 387, 509, 392), "RATE", (494, 389), 12), (2, (645, 391, 666, 396), "MIX", (657, 391), 12),
    (2, (556, 410, 601, 415), "SPREAD", (577, 402), 12),
    # the section headings, bigger again
    (2, (91, 258, 138, 265), "DRIVE", None, 14), (2, (311, 258, 364, 265), "FILTER", None, 14),
    (2, (484, 257, 547, 264), "SWARM", None, 14), (2, (720, 252, 840, 260), "MODULATOR", (774, 256), 14),
    (1, (414, 41, 492, 49), "ACCENT", None, 14), (1, (878, 13, 991, 22), "AMP ENV", (652, 18), 14),
    (1, (883, 18, 883, 18), "FILTER ENV", None, 14),
]
# two-line legends reprinted the same way: (tab, ink box, (line 1, line 2), centre, cap)
MODULES_REPRINT2 = [(2, (270, 372, 307, 387), ("KEY", "TRACK"), (281, 380), 12),
                    (1, (471, 155, 508, 169), ("SLIDE", "TIME"), (490, 162), 12)]
# the selector bars' and FREE / TEMPO's words: drawn in label-window style (BAR_TEXT px tall at most)
BAR_TEXT = 14
# MODULES: printed legends (page rect, text, px) of the controls ZYNTH adds
MODULES_LABELS = [(1, (513, 309, 80, 14), "FM RATIO", 11), (1, (597, 309, 80, 14), "FM LVL", 11),
                  (2, (487, round(mods(511 / 1.28) * 1.28), 80, 14), "NOISE", 12)]
# PANEL: LXRXXL's randomizers beside the panic key (page pixels): each key's picture, centre and size; the scale
# printed round each amount knob; the legend ("?" for STEP RANDOM, the warning sign for SOUND RANDOM)
MOD_KEYS = [("srnd", "mod_key_yellow.png", 549, 176, 40), ("rnd", "mod_key_green.png", 744, 176, 40)]
MOD_ART = [("mod_label_question.png", 549, 140, 14), ("mod_label_warning.png", 744, 138, 30)]   # (the amount knobs: the
# white segmented ring of every other knob, not LXRXXL's coloured scales)
# MODULES: the other sliders' legends in label windows like the voice's (fixed text, page pixels)
FIXED_WINDOWS = [((round(x * 1.28 - 33), 270, 66, 40), t) for x, t in
                 zip(RIGHT_X, ["ATTACK", "DECAY", "SUSTAIN", "RELEASE", "ATTACK", "DECAY", "SUSTAIN", "RELEASE"])]
CH_SEL = (2, 661, fx(585), 40, 34)                         # FX MOD: the duck's MIDI channel

# ---------------------------------------------------------------- presets (FX MIXER, page pixels)
# the slot number (the display's font) with its label, and the four keys: glass centre, label centre
PRESET_SLOT = (622, 26, 86, 42)
PRESET_SLOT_LABEL = (622, 6, 86, 16)
PRESET_KEYS = [("pr_sound", 753, 47, "SOUND", 753, 17), ("pr_perf", 817, 70, "PERF", 817, 40),
               ("pr_save", 873, 47, "SAVE", 873, 17), ("pr_load", 928, 70, "LOAD", 928, 40)]
PRESET_GLASS = 34

# ---------------------------------------------------------------- the mixer (MIXER, page pixels)
STRIP_W = 141
MIX_STRIPS = [6 + i * STRIP_W for i in range(8)]          # each strip's left edge
MASTER_X = 6 + 8 * STRIP_W                                # the MASTER strip: to the right edge
MIX = {   # inside a strip (x from its left edge, y from MIXER_Y)
    "num": (8, 6, 44, 38),            # the track's number, in the display's font
    "pan": (44, 128, 72),             # knob: cx, cy, size
    "snd": (44, 296, 72),
    "pan_label": (0, 66, 88, 18), "snd_label": (0, 234, 88, 18),
    "fader": (90, 40, 44, 424),       # the slider box (the artwork's LEVEL box): x, y, w, h
    "lvl_label": (78, 12, 64, 18),
    "mute": (4, 474, 66, 58), "solo": (72, 474, 66, 58),          # the step keys' size
    "mute_label": (4, 536, 66, 18), "solo_label": (72, 536, 66, 18),
}
MIX_MASTER = {"label": (MASTER_X, 10, 1280 - MASTER_X - 8, 26), "fader": (MASTER_X + 40, 44, 56, 510)}


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
        t, x, y, w, h = TRACK_SEL
        out.append(("track",) + page(tab, x + w / 2, y + h / 2))
        t, x, y, w, h = VOICE_SEL
        out.append(("voice",) + page(tab, x + w / 2, y + h / 2))
    if tab == 2:
        for key, x, y, w, h, n in BARS:
            out.append((key,) + page(tab, x + w / 2, y + h / 2))
        out.append(("mod_sync",) + page(tab, SYNC_SLOT[0], (SYNC_SLOT[1] + SYNC_SLOT[2]) / 2))
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


ROW_GAP = {0: 48, 1: 110, 2: 48, 3: 48}   # page px: how far below a row's top a control still belongs to that row


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
PAGE_IDS = ["panel", "voice", "fxmod", "mixer"]


def mapped_keys(tab):
    """the controls a tab can put on a knob"""
    keys = [c[0] for c in controls(tab)]
    if tab == 3:
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
    base = ["PANEL", "VOICE", "FX MOD", "MIXER"][tab]
    return ["%s %d" % (base, i + 1) for i in range(len(qlink_pages(tab)))]


def layout_lines():
    """layout.conf: each tab with a knob per Q-Link control (panel_art.py replaces every control) and its Q-Links"""
    L = ["# LXRXXL ZYNTH -- page layout (mpc-vst-plugins layout.conf syntax: tools/shadow_skin.py).",
         "# GENERATED by tools/panel.py: edit that, then run `python3 tools/panel.py --write`. tools/panel_art.py then",
         "# builds every control of the four pages from the artwork (art/panel/).",
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
