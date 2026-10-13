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
    m[118:148, 828:922] = True                          # the GATE/DUCK switch's slot
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
    new = Image.fromarray(comp(front_tex, fb, mask).astype(np.uint8), "RGBA")
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
