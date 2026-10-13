#!/usr/bin/env python3
"""LXRXXL ZYNTH's artwork, made from LXRXXL BAZZLINE's (Suns Kiefer, art/bazzline/) as Suns Kiefer chose (option B):

  - the cybernetic circuit under the printing is replaced by the vein texture (art/texture/veins.png), toned to the
    panel's dark finish, its veins tinted ice blue; the printing, the keys, the slider boxes, the switches and the
    machine bezel are kept;
  - every orange of the artwork becomes the ice blue of the accent;
  - the front panel's logo reads ZYNTH, its legend "8-TRACK SYNTH VOICE";
  - LXRXXL's randomizer keys, knobs, scales and legends (mod_*.png) are copied as they are.

    python3 tools/zynth_art.py          (writes art/panel/ from art/bazzline/ and art/texture/)

tools/panel_art.py then builds the skin's pages from art/panel/."""
import os
import shutil

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont
from scipy import ndimage

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "art", "bazzline")
OUT = os.path.join(ROOT, "art", "panel")
TEX = os.path.join(ROOT, "art", "texture", "veins.png")
FONT14 = os.path.join(ROOT, "art", "fonts", "DSEG14Classic-Italic.ttf")
MICH = os.path.join(ROOT, "art", "fonts", "Michroma-Regular.ttf")

# 0.2.0, the performance front: printed legends replaced (BAZZLINE's ink box, the new word); the new words are set in
# Michroma (the printed legends' face) at the measured cap height, width and letter spacing (legend())
RELABEL = [
    ((211, 311, 278, 319), "TIMBRE"),        # was DETUNE
    ((339, 304, 412, 312), "MORPH"),         # was VCA ENV
    ((37, 440, 88, 448), None),              # FM LVL, SUB LVL, NOISE LVL: the ADSR's four legends instead (ADSR)
    ((161, 437, 222, 445), None),
    ((276, 441, 351, 449), None),
    ((651, 439, 720, 447), "MOD RATE"),      # was LFO RATE
    ((774, 438, 831, 446), "MOD DEPTH"),     # was LFO LVL
    ((890, 441, 975, 449), "SUB LVL"),       # was ENV/CV LVL
    ((666, 184, 759, 193), "CHAOS"),         # was RELEASE (the gate/duck's): CHAOS's depth
    ((770, 184, 820, 193), "SHIFT"),         # under SHIFT's mix knob (beside CHAOS)
]
SUB_LEGEND = (443, 218, "SUB")
ADSR = [(52, "ATTACK"), (138, "DECAY"), (224, "SUSTAIN"), (310, "RELEASE")]   # under the four knobs (x centre)
ADSR_TOP, ADSR_CAP, ADSR_W = 438, 8, 76
# the small legends reprinted bigger: (ink box), word, cap (artwork px), the widest it may be (artwork px)
STEP_WORDS = ["ACC", "SLIDE", "MOD", "PITCH ENV", "LAST STEP", "PLAY MODE", "TRANSP", "SCALE", "ARPEGG", "LFO",
              "RND PATT", "CLEAR", "COPY", "PASTE", "BANK", "PATTERN"]
STEP_X = [39, 101, 164, 225, 287, 350, 411, 472, 534, 595, 658, 719, 781, 843, 905, 968]
SMALL = [((x - 30, 581, x + 30, 592), w, 9, 48) for x, w in zip(STEP_X, STEP_WORDS)] + [
    ((462, 235, 488, 242), "SQR", 8, 40), ((462, 250, 492, 256), "SAW", 8, 40), ((463, 264, 481, 271), "TRI", 8, 40),
    ((543, 239, 559, 246), "BP", 8, 30), ((543, 262, 557, 268), "LP", 8, 30),
    ((819, 497, 850, 506), "-1 OCT", 8, 56), ((880, 497, 915, 506), "+1 OCT", 8, 56),
    ((306, 47, 353, 55), "TRACK", 9, 60),
    ((379, 477, 481, 486), "SELECT/GATE", 10, 120), ((521, 478, 607, 486), "BACK/NOTE", 10, 110),
]              # over the SQR/SAW/TRI switch: the sub oscillator's wave (BAZZ: its own)

ICE = np.array([125, 205, 245], np.float32)      # the accent: the ice orb's glow
GHOST = (10, 28, 38)                              # unlit segments
ORANGE_LUM = 0.30 * 251 + 0.59 * 144 + 0.11 * 19
PAGE_SCALE = 1.02                                  # texture px -> page px: the same vein size on every page


def lum(a):
    return a[..., :3] @ np.array([0.30, 0.59, 0.11], np.float32)


# ---------------------------------------------------------------- the texture
def texture_tone(t):
    """the veins in the panel's dark finish (ground ~17, veins ~44, as the circuit's traces were), tinted ice"""
    L = lum(t)
    g = np.clip((L - 22.0) * 0.62 + 16.0, 6, 255)
    g = ndimage.gaussian_filter(g, 0.6)                       # the picture's crunchy edges softened a touch
    h, w = g.shape
    yy, xx = np.mgrid[0:h, 0:w]
    g = g * (1 - 0.09 * (((xx - w / 2) / (w / 2)) ** 2 + ((yy - h / 2) / (h / 2)) ** 2))   # a soft vignette
    rgb = np.repeat(g[..., None], 3, -1)
    v = np.clip((L - 32.0) / 38.0, 0, 1)[..., None]           # how much of a vein
    tint = np.array([0.70, 1.12, 1.42], np.float32)            # ice: the same brightness, a blue cast
    rgb = rgb * (1 - v * 0.85) + rgb * tint * v * 0.85
    return np.clip(rgb, 0, 255)


def tex_page(t):
    im = Image.fromarray(t.astype(np.uint8), "RGB")
    return im.resize((round(im.width * PAGE_SCALE), round(im.height * PAGE_SCALE)), Image.LANCZOS)


# ---------------------------------------------------------------- orange -> ice
def ice_recolour(img):
    a = np.asarray(img.convert("RGBA")).astype(np.float32)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    w = np.clip((r - b - 30) / 60, 0, 1) * (r >= g) * np.clip((r - 40) / 40, 0, 1)
    k = lum(a) / ORANGE_LUM
    new = ICE[None, None, :] * k[..., None]
    over = np.clip((new.max(-1, keepdims=True) - 255) / 120, 0, 1)
    new = np.clip(new * (1 - over) + 255 * over, 0, 255)
    a[..., :3] = a[..., :3] * (1 - w[..., None]) + new * w[..., None]
    return Image.fromarray(a.astype(np.uint8), "RGBA")


# ---------------------------------------------------------------- what is printed (kept) and what is circuit (replaced)
def soft(mask, grow=1, feather=0.8):
    m = ndimage.binary_dilation(mask, iterations=grow) if grow else mask
    return np.clip(ndimage.gaussian_filter(m.astype(np.float32), feather) * 1.6, 0, 1)


def front_printing(a):
    L = lum(a)
    sat = a[..., :3].max(-1) - a[..., :3].min(-1)
    m = (L > 62) | (sat > 45)
    m = ndimage.binary_fill_holes(ndimage.binary_closing(m, iterations=2))
    m = ndimage.binary_opening(m, iterations=1) | (L > 90)
    m[:30, :] = True                                    # the machine bezel along the top
    m[289:359, 420:580] = True                          # the display window
    m[226:280, 428:458] = True                          # SQR/SAW/TRI's slot
    m[228:276, 512:540] = True                          # BP/LP's slot
    m[-3:, :] = True
    return soft(m, 1)


def modules_printing(a, clean, cover):
    d = np.abs(a[..., :3] - clean[..., :3]).max(-1)
    m = (d > 14) & cover
    L = lum(a)
    m |= (~cover) & (L > 62)
    m[22:212, 940:1000] = True                          # PAN's slider box (past the clean circuit's edge)
    m = ndimage.binary_closing(m, iterations=1)
    m = ndimage.binary_fill_holes(m)
    m[:22, :] |= L[:22, :] > 40                         # the top bezel strip
    return soft(m, 1)


def comp(base, src, mask):
    out = base.copy()
    out[..., :3] = base[..., :3] * (1 - mask[..., None]) + src[..., :3] * mask[..., None]
    return out


def legend(word, cap, ratio=0.95, track=2.5, sw=1, S=8, colour=(236, 236, 236)):
    """a printed legend: Michroma, cap px high, squeezed by ratio (the artwork is drawn 1/1.28 wide), letter-spaced"""
    f = ImageFont.truetype(MICH, cap * S)
    xs, x = [], 0.0
    for ch in word:
        xs.append(x)
        x += f.getlength(ch) + track * S / ratio
    _, t, _, b = f.getbbox("H", stroke_width=sw)
    big = Image.new("L", (int(x + 20), int(b - t + 2 * S)), 0)
    d = ImageDraw.Draw(big)
    for ch, px in zip(word, xs):
        d.text((4 + px, S - t), ch, font=f, fill=255, stroke_width=sw, stroke_fill=255)
    bb = big.getbbox()
    big = big.crop((bb[0], bb[1], bb[2], bb[3]))
    m = big.resize((max(1, round(big.width / S * ratio)), max(1, round(big.height / S))), Image.LANCZOS)
    out = Image.new("RGBA", m.size, colour + (0,))
    out.putalpha(m)
    return out


def seg_text(text, cells, cell_w, h):
    """14-segment text in ice with its unlit segments and a glow (as the BAZZLINE logo was)"""
    f = ImageFont.truetype(FONT14, h)
    layer = Image.new("RGBA", (cells * cell_w + 12, h + 16), (0, 0, 0, 0))
    lit = Image.new("RGBA", layer.size, (0, 0, 0, 0))
    d, dl = ImageDraw.Draw(layer), ImageDraw.Draw(lit)
    for i, ch in enumerate(text.ljust(cells)):
        d.text((6 + i * cell_w, 6), "~", font=f, fill=GHOST + (255,))
        if ch != " ":
            dl.text((6 + i * cell_w, 6), ch, font=f, fill=tuple(int(c) for c in ICE) + (255,))
    ga = np.asarray(lit.filter(ImageFilter.GaussianBlur(4))).copy()
    ga[..., 3] = (ga[..., 3] * 0.7).astype(np.uint8)
    layer.alpha_composite(Image.fromarray(ga, "RGBA"))
    layer.alpha_composite(lit)
    return layer


def main():
    os.makedirs(OUT, exist_ok=True)
    for f in os.listdir(SRC):
        if f.endswith(".png"):
            shutil.copy(os.path.join(SRC, f), os.path.join(OUT, f))
    tex = np.asarray(Image.open(TEX).convert("RGB")).astype(np.float32)
    T = tex_page(texture_tone(tex))
    # ---- PANEL: the page is the artwork stretched 1.28 x 1.0048: a 1280 x 628 page of texture squeezed to the artwork
    ox, oy = (T.width - 1280) // 2, (T.height - 628) // 2
    front_tex = np.asarray(T.crop((ox, oy, ox + 1280, oy + 628)).resize((1000, 625), Image.LANCZOS).convert("RGBA")).astype(np.float32)
    fb = np.asarray(Image.open(os.path.join(SRC, "front_bg.png")).convert("RGBA")).astype(np.float32)
    mask = front_printing(fb)
    # the BAZZLINE logo goes (its orange cells, and everything under the step keys' legends); the legends stay
    logo = fb[584:625, 100:342]
    keep = ~((logo[..., 0] - logo[..., 2] > 25) | (np.arange(41)[:, None] >= 9))
    mask[584:625, 100:342] *= keep
    mask[584:625, 100:342] = ndimage.binary_erosion(mask[584:625, 100:342] > 0.5, iterations=1) * mask[584:625, 100:342]
    mask[458:492, 254:362] = 0                          # "8-TRACK BASS VOICE": reprinted below
    mask[104:153, 760:993] = 0                          # GATE, MODE, DUCK and the switch: SHIFT's knob there (0.2.0)
    for (x0, y0, x1, y1), _ in RELABEL:                 # the replaced legends: the texture shows through
        mask[y0 - 2:y1 + 2, x0 - 3:x1 + 3] = 0
    for (x0, y0, x1, y1), *_ in SMALL:                  # the small ones, reprinted bigger
        mask[y0 - 1:y1 + 1, x0 - 1:x1 + 1] = 0
    new = Image.fromarray(comp(front_tex, fb, mask).astype(np.uint8), "RGBA")
    for cx, word in ADSR:
        lg = legend(word, ADSR_CAP, track=2.0)
        if lg.width > ADSR_W:
            lg = lg.resize((ADSR_W, lg.height), Image.LANCZOS)
        new.alpha_composite(lg, (round(cx - lg.width / 2), ADSR_TOP))
    for (x0, y0, x1, y1), word in RELABEL:
        if not word:
            continue
        lg = legend(word, y1 - y0 + 1)
        new.alpha_composite(lg, (round((x0 + x1) / 2 - lg.width / 2), y0))
    for (x0, y0, x1, y1), word, cap, wmax in SMALL:
        lg = legend(word, cap, track=1.6)
        if lg.width > wmax:
            lg = lg.resize((wmax, lg.height), Image.LANCZOS)
        cy = (y0 + y1) / 2
        new.alpha_composite(lg, (round((x0 + x1) / 2 - lg.width / 2), round(cy - lg.height / 2)))
    sx, sy, word = SUB_LEGEND
    lg = legend(word, 6, track=1.5, sw=0, colour=(200, 200, 200))
    new.alpha_composite(lg, (round(sx - lg.width / 2), sy))
    zy = seg_text("ZYNTH", 5, 40, 30)
    new.alpha_composite(zy, (338 - zy.width + 4, 587))
    tag = Image.new("RGBA", (150, 34), (0, 0, 0, 0))
    td = ImageDraw.Draw(tag)
    fnt = ImageFont.truetype(MICH, 11)
    td.text((2, 2), "8-TRACK", font=fnt, fill=(150, 150, 150, 255))
    td.text((2, 17), "SYNTH VOICE", font=fnt, fill=(150, 150, 150, 255))
    new.alpha_composite(tag.resize((round(150 / 1.28), 34), Image.LANCZOS), (258, 461))
    new.save(os.path.join(OUT, "front_bg.png"))
    # ---- MODULES and FX MIXER (the artwork at 1.28): the texture at PAGE_SCALE / 1.28
    k = 1 / 1.28
    Tm = T.resize((round(T.width * k), round(T.height * k)), Image.LANCZOS)
    big = np.asarray(Tm.convert("RGBA")).astype(np.float32)
    mx, my = (Tm.width - 1000) // 2, (Tm.height - 625) // 2
    mb = np.asarray(Image.open(os.path.join(SRC, "modules_bg.png")).convert("RGBA")).astype(np.float32)
    cl = np.asarray(Image.open(os.path.join(SRC, "circuit_clean.png")).convert("RGBA")).astype(np.float32)
    reg = np.zeros_like(mb)
    reg[3:, :963] = cl[:622, 37:1000]                   # the clean circuit where panel_art registers it (37 right, 3 up)
    cover = np.zeros(mb.shape[:2], bool)
    cover[3:, :963] = True
    Image.fromarray(comp(big[my:my + 625, mx:mx + 1000], mb, modules_printing(mb, reg, cover)).astype(np.uint8),
                    "RGBA").save(os.path.join(OUT, "modules_bg.png"))
    # ---- MIXER (0.2.0): the texture alone, at the same scale
    Image.fromarray(big[my:my + 491, mx:mx + 1000].astype(np.uint8), "RGBA").save(os.path.join(OUT, "mixer_bg.png"))
    cw, ch = cl.shape[1], cl.shape[0]
    Image.fromarray(big[my - 3:my - 3 + ch, mx - 37:mx - 37 + cw].astype(np.uint8), "RGBA").save(
        os.path.join(OUT, "circuit_clean.png"))
    # ---- every orange left (the track numbers, the panic key's glow, the lit glass) -> ice; LXRXXL's keys keep theirs
    for f in os.listdir(OUT):
        if f.endswith(".png") and not f.startswith("mod_"):
            p = os.path.join(OUT, f)
            ice_recolour(Image.open(p)).save(p)
    print("zynth_art: art/panel written")


if __name__ == "__main__":
    main()
