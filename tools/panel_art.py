#!/usr/bin/env python3
"""Build the four pages of LXRXXL ZYNTH's skin from the artwork (art/panel/), into the skin mpc-vst-plugins made
from layout.conf (which only gave the tabs and their Q-Links).

    python3 tools/panel_art.py "<skin dir>/Plugin Skins"

Geometry: tools/panel.py. For each page:
  - the background: the artwork filling the whole screen (PANEL stretched; MODULES and FX MIXER at 1.28), a white
    segmented ring around each knob (drawn on the page, so it stays round), the display windows, the readout strip,
    and on FX MIXER the mixer's strips (labels in Michroma, OFL; the track numbers in DSEG14);
  - knobs: filmstrips of the knob picture turning (-150..+150 degrees), 128 square frames with numFrames 127 and at
    most 126 px (16128 px of strip), the framework's rule for knobs that turn in place on MPC (docs/NOTES.md); one
    transparent strip per size, over the background;
  - sliders (MODULES) and the mixer's faders: filmstrips of the cap along the printed channel (frames of the slider's
    own size, numFrames = their count, strips <= 12288 px); the faders are the artwork's slider box and cap;
  - option controls (selector bars, OSC MODE, FREE/TEMPO, the toggle switches): a radio group of pictures;
  - keys: two pictures each, unlit and lit (the lit glass in the display's orange);
  - the PANEL display: one picture per character (MPC's IndexedEnabling), in DSEG14 Classic (OFL) in the display's
    orange with its unlit segments and a glow; TRACK (MODULES) and the duck's MIDI CH (FX MIXER) are filmstrips of
    that look, so the wheel, a drag and a Q-Link step them both ways;
  - the readout: MPC's own text (Titillium Web) showing the "readout" parameter: the control last touched.
Every control's definition in TUI.json is written here; the generated ones are removed."""
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont
from scipy import ndimage

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import panel as P  # noqa: E402
import gen_params  # noqa: E402
import voices  # noqa: E402

FONT = os.path.join(P.ROOT, "art", "fonts", "DSEG14Classic-Italic.ttf")
LABEL_FONT = os.path.join(P.ROOT, "art", "fonts", "Michroma-Regular.ttf")
LIT = (125, 205, 245)           # the accent: ice blue (art/panel, tools/zynth_art.py)
GHOST = (10, 28, 38)            # its unlit segments
WINDOW = (4, 7, 9)              # the display window
INK = (232, 232, 228)           # the panel's printed white
FINISH = (24, 24, 24)           # the panel's dark finish
KNOB_SPAN = 150                 # degrees each side of 12 o'clock
KNOB_FRAMES = 128               # the framework's knob strips: 128 frames, numFrames 127 (turn in place on MPC)
MAX_STRIP = 12288               # px: slider strips (frames of their own size, numFrames = count)
SS = 3


def art(name):
    return Image.open(os.path.join(P.ART, name)).convert("RGBA")


def R(v):
    return int(round(v))


def prect(tab, x, y, w, h):
    """artwork rect -> page rect (integers)"""
    x0, y0 = P.page(tab, x, y)
    x1, y1 = P.page(tab, x + w, y + h)
    return R(x0), R(y0), R(x1) - R(x0), R(y1) - R(y0)


def knob_px(tab, size):
    """a knob's page size: round (the page's vertical scale), even, at most KNOB_MAX"""
    return min(P.KNOB_MAX, 2 * R(size * P.scale(tab)[1] / 2))


# ------------------------------------------------------------------------------------------------ drawing helpers
def ring_layer(size_px):
    """the white segmented ring of the mockups: 10 segments over 300 degrees, at 0.505..0.57 of the knob's size"""
    S = size_px * 1.25
    n = int(math.ceil(S)) * 4
    im = Image.new("L", (n, n), 0)
    d = ImageDraw.Draw(im)
    c = n / 2
    r0, r1 = 0.505 * size_px * 4, 0.57 * size_px * 4
    gap = 6.0
    seg = (300.0 - 9 * gap) / 10
    for i in range(10):
        a0 = -150 + i * (seg + gap)
        a1 = a0 + seg
        # PIL angles: 0 = 3 o'clock, clockwise; ours: 0 = 12 o'clock, clockwise
        d.arc([c - (r0 + r1) / 2, c - (r0 + r1) / 2, c + (r0 + r1) / 2, c + (r0 + r1) / 2], a0 - 90, a1 - 90,
              fill=255, width=max(1, int(round(r1 - r0))))
    im = im.resize((n // 4, n // 4), Image.LANCZOS)
    rng = np.random.default_rng(3)
    a = np.asarray(im, np.float32) / 255.0
    tex = 215 + rng.normal(0, 10, a.shape)          # the printed white's grain
    rgb = np.clip(np.stack([tex, tex, tex - 4], -1), 0, 255)
    out = np.concatenate([rgb, (a * 255)[..., None]], -1).astype(np.uint8)
    return Image.fromarray(out, "RGBA")


def wipe(img, rect):
    """wipe the printing (labels, slots) inside rect: every bright pixel takes the dark finish around it"""
    a = np.asarray(img).astype(np.float32)
    x, y, w, h = [R(v) for v in rect]
    sub = a[y:y + h, x:x + w, :3]
    lum = sub @ np.array([0.30, 0.59, 0.11], np.float32)
    keep = lum < np.percentile(lum, 35) + 6
    keep = ndimage.binary_erosion(keep, iterations=2)
    k = keep.astype(np.float32)
    fill = np.stack([ndimage.uniform_filter(sub[..., c] * k, 15) for c in range(3)], -1)
    den = ndimage.uniform_filter(k, 15)[..., None]
    fill = fill / np.maximum(den, 1e-3)
    fill[den[..., 0] < 1e-3] = FINISH
    rng = np.random.default_rng(5)
    fill += rng.normal(0, 2.0, fill.shape[:2])[..., None]
    a[y:y + h, x:x + w, :3] = np.clip(fill, 0, 255)
    return Image.fromarray(a.astype(np.uint8), "RGBA")


def label_font(px):
    return ImageFont.truetype(LABEL_FONT, px)


def label(d, rect, text, px, fill=INK, anchor="mm"):
    """printed label (Michroma), centred in rect"""
    x, y, w, h = rect
    d.text((x + w / 2, y + h / 2), text, font=label_font(px), fill=fill, anchor=anchor)


def mixer_finish(w, h):
    """the mixer's ground: the panel's dark finish with its grain and a soft top shade"""
    rng = np.random.default_rng(17)
    base = np.full((h, w, 3), FINISH, np.float32)
    base += rng.normal(0, 3.0, (h, w))[..., None]
    shade = np.linspace(-6, 4, h)[:, None, None]
    return Image.fromarray(np.clip(base + shade, 0, 255).astype(np.uint8), "RGB").convert("RGBA")


def printed_label(page, rect, text, cap=8):
    """a legend like the MODULES page's printed ones (KEY TRACK, RATE): Michroma, pale, an ice glow round it"""
    import zynth_art
    x, y, w, h = rect
    core = zynth_art.legend(text, cap, ratio=1.0, track=1.6 if cap < 13 else 3.0, sw=2, colour=(226, 233, 228))
    pad = 6
    glow = Image.new("RGBA", (core.width + 2 * pad, core.height + 2 * pad), LIT + (0,))
    a = np.zeros((glow.height, glow.width), np.uint8)
    a[pad:pad + core.height, pad:pad + core.width] = np.asarray(core)[..., 3]
    glow.putalpha(Image.fromarray(a).filter(ImageFilter.GaussianBlur(2.2)).point(lambda v: int(v * 0.9)))
    ox, oy = R(x + w / 2 - glow.width / 2), R(y + h / 2 - glow.height / 2)
    page.alpha_composite(glow, (ox, oy))
    page.alpha_composite(core, (ox + pad, oy + pad))


def window(d, rect, radius=6):
    x, y, w, h = rect
    d.rounded_rectangle([x, y, x + w - 1, y + h - 1], radius=radius, fill=WINDOW + (255,), outline=(40, 40, 40, 255), width=2)


# circuit_clean.png (Suns Kiefer): the MODULES panel's circuit without its printing, 37 px right and 3 px up of it
CLEAN_DX, CLEAN_DY = 37, -3
_clean = {}


def clean_modules():
    """the clean circuit registered on modules_bg.png (1000 x 625) and where it covers it (a mask)"""
    if not _clean:
        src = art("circuit_clean.png")
        out = Image.new("RGBA", (P.ART_W, P.ART_H), (0, 0, 0, 0))
        out.paste(src.crop((CLEAN_DX, 0, P.ART_W, P.ART_H)), (0, -CLEAN_DY))
        cover = np.zeros((P.ART_H, P.ART_W), bool)
        cover[-CLEAN_DY:, :P.ART_W - CLEAN_DX] = True
        _clean["img"], _clean["cover"] = out, cover
    return _clean["img"], _clean["cover"]


def clean_patch(img, rect):
    """put the clean circuit back inside rect (artwork pixels of modules_bg.png); outside its cover: wipe()"""
    x, y, w, h = [R(v) for v in rect]
    c, cover = clean_modules()
    if cover[y:y + h, x:x + w].all():
        img.paste(c.crop((x, y, x + w, y + h)), (x, y))
        return img
    return wipe(img, rect)


def circuit_texture(w, h, region=(0, 100, 963, 318)):
    """a stretch of the clean circuit as a ground (the mixer, the readout strips)"""
    src = art("circuit_clean.png")
    x0, y0, x1, y1 = region
    return src.crop((CLEAN_DX + x0, y0, CLEAN_DX + x1, y1)).resize((w, h), Image.LANCZOS)


def relabel(img):
    """MODULES: the sliders printed DRIVE POS and CURVE are DRIVE and SUB LVL (both selectors stay on their bars): POS
    and CURVE wiped, SUB / LVL taken from the front panel's SUB LVL legend and set in two lines like DRIVE POS"""
    for r in P.RELABEL_WIPE:
        img = clean_patch(img, r)
    front = np.asarray(art("front_bg.png").convert("L")).astype(np.float32)
    for (sx0, sy0, sx1, sy1), (cx, top, hgt) in P.RELABEL_WORDS:
        a = np.clip((front[sy0:sy1, sx0:sx1] - 60) / 160, 0, 1)
        w = R((sx1 - sx0) * hgt / (sy1 - sy0))
        word = np.zeros((sy1 - sy0, sx1 - sx0, 4), np.uint8)
        word[..., :3] = INK
        word[..., 3] = (a * 255).astype(np.uint8)
        pic = Image.fromarray(word, "RGBA").resize((w, hgt), Image.LANCZOS)
        img.alpha_composite(pic, (R(cx - w / 2), top))
    return img


def reset_key_art(img, lit):
    """the VOICE RESET button (btn_reset*.png) on its bezel (btn_reset_bezel.png), centred at P.RESET_KEY, artwork scale"""
    img = img.copy()
    for name in ("btn_reset_bezel.png", "btn_reset_on.png" if lit else "btn_reset.png"):
        img.alpha_composite(art(name), P.RESET_ART[name])
    return img


def modules_clean(masks):
    """modules_bg.png with what is reprinted or moved wiped (artwork pixels), and masks"""
    img = art("modules_bg.png")
    img = relabel(img)
    for _, (x0, y0, x1, y1), *_ in P.MODULES_REPRINT + P.MODULES_REPRINT2:   # reprinted bigger on the page
        img = clean_patch(img, (x0 - 3, y0 - 3, x1 - x0 + 6, y1 - y0 + 6))
    for r in P.MODULES_WIPE:   # the title (the VOICE selector's place) and OSC MODE's legends
        img = clean_patch(img, r)
    for m in masks:
        img = clean_patch(img, m)
    return img


def compose(tab):
    """the VOICE and FX MOD pictures, put together from modules_bg.png (panel.py)"""
    if tab == 1:
        mb = modules_clean([(383, 395, 94, 20), (0, 414, 1000, 4)])
        out = Image.new("RGBA", (P.ART_W, 456), FINISH + (255,))
        out.paste(mb.crop((0, 0, P.ART_W, 242)), (0, 0))
        out.paste(mb.crop((0, 32, P.ART_W, 245)), (0, 242))   # the faders again, a row lower
        c, _ = clean_modules()
        x, y, w, h = P.VOICE_CENTRE   # the copied ACCENT column: the clean circuit instead
        out.paste(c.crop((x, 40, x + w, 40 + h)), (x, y))
        return out
    top = modules_clean([(0, 395, 385, 24), (475, 395, 525, 24)])   # the delay
    c, _ = clean_modules()
    for x, y, w, h in P.ALIEN_WIPE:   # the gate/duck's printing: the vein texture instead (CHAOS and SHIFT go there)
        sx = min(x, 955 - w)   # the clean texture ends at x 963
        sy = min(y, c.size[1] - h - 1)
        top.paste(c.crop((sx, sy, sx + w, sy + h)), (x, y))
    low = modules_clean([(383, 395, 94, 20), (0, 414, 1000, 4)])    # the modules: the delay's first labels wiped
    out = Image.new("RGBA", (P.ART_W, 400), FINISH + (255,))
    out.paste(top.crop((0, 395, P.ART_W, 625)), (0, 0))
    out.paste(low.crop((0, 248, P.ART_W, 418)), (0, 230))
    return out


def make_background(tab):
    pic, (cx0, cy0, cx1, cy1), sx, sy = P.PAGES[tab]
    bg_art = compose(tab) if pic in ("voice", "fxmod") else art(pic)
    if tab == 0:
        bg_art = reset_key_art(bg_art, False)   # the VOICE RESET button on its bezel, unlit
        # the display window: cleared (the live characters are pictures over it)
        x, y, w, h = P.DISPLAY_BOX
        a = np.asarray(bg_art).copy()
        sub_ = a[y:y + h, x:x + w, :3].astype(np.int32)
        # the printed segments (lit and ghost, orange or recoloured ice): anything coloured or lit in the window
        seg = ((sub_.max(-1) - sub_.min(-1)) > 14) | (sub_.max(-1) > 42)
        seg[:4, :] = seg[-4:, :] = False                 # the window's edge stays
        seg[:, :4] = seg[:, -4:] = False
        seg = np.asarray(Image.fromarray((seg * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(5))) > 0
        sub_[seg] = WINDOW
        a[y:y + h, x:x + w, :3] = sub_
        bg_art = Image.fromarray(a, "RGBA")
        for cx, cy in P.DISPLAY_CELLS:   # the unlit segments under each character
            bg_art.alpha_composite(glyph_cell("~", ghost_only=True), (cx - 3, cy - 4))
    part = bg_art.crop((cx0, cy0, cx1, cy1))
    pw, ph = R((cx1 - cx0) * sx), R((cy1 - cy0) * sy)
    page = Image.new("RGBA", (P.W, P.H), FINISH + (255,))
    page.alpha_composite(part.resize((pw, ph), Image.LANCZOS), (0, 0))
    d = ImageDraw.Draw(page)
    # the rings, on the page (round whatever the page's stretch)
    for t, key, cx, cy, size, img, ring in P.KNOBS:
        if t == tab and ring:
            rl = ring_layer(knob_px(tab, size))
            px, py = P.page(tab, cx, cy)
            page.alpha_composite(rl, (R(px - rl.size[0] / 2), R(py - rl.size[1] / 2)))
    if tab in P.READOUT:
        x, y, w, h = P.READOUT[tab]
        page.alpha_composite(circuit_texture(w, h, (0, 430, 963, 470)), (x, y))
        window(d, (x + 10, y + 6, w - 20, h - 12), 8)
    if tab == 0:   # LXRXXL's randomizers: the scales round the amount knobs, the legends over the keys
        for name, cx, cy, w in P.MOD_ART:
            pic = art(name)
            pic = pic.resize((w, R(pic.size[1] * w / pic.size[0])), Image.LANCZOS)
            page.alpha_composite(pic, (R(cx - pic.size[0] / 2), R(cy - pic.size[1] / 2)))
    if tab in (1, 2):
        mv = (lambda y: y) if tab == 1 else P.mods
        for t_, rect, txt, px in P.MODULES_LABELS:   # as the page's printed legends: pale letters with an ice glow
            if t_ == tab:
                printed_label(page, rect, txt, px)
        if tab == 2:   # 0.2.0: CHAOS and SHIFT's titles and legends
            for cx, cy, txt in P.ALIEN_TITLES:
                px_, py_ = P.page(tab, cx, cy)
                printed_label(page, (px_ - 80, py_ - 10, 160, 20), txt, 15)
            for cx, cy, txt in P.ALIEN_LABELS:
                px_, py_ = P.page(tab, cx, cy)
                printed_label(page, (px_ - 60, py_ - 9, 120, 18), txt, 12)
        for t_, (x0, y0, x1, y1), txt, c, cap in P.MODULES_REPRINT:
            if t_ != tab:
                continue
            cx, cy = c if c else ((x0 + x1) / 2, (y0 + y1) / 2)
            px_, py_ = P.page(tab, cx, mv(cy))
            printed_label(page, (px_ - 80, py_ - 9, 160, 18), txt, cap)
        for t_, (x0, y0, x1, y1), lines, (cx, cy), cap in P.MODULES_REPRINT2:
            if t_ != tab:
                continue
            px_, py_ = P.page(tab, cx, mv(cy))
            for i, txt in enumerate(lines):
                printed_label(page, (px_ - 60, py_ - 8 + (i - 0.5) * (cap + 5), 120, 16), txt, cap)
    if tab == 1:
        label(d, P.VOICE_LABEL, "VOICE", 17)
        for (x, y, w, h), txt, px in P.PITCH_LABELS:
            for i, line in enumerate(txt.split("|")):
                printed_label(page, (x, y + i * (px + 5), w, h), line, px)
        printed_label(page, P.SOUND_LABEL, "SOUND", 12)
        window(d, P.SOUND_NAME, 6)
        for rect in P.LABEL_WINDOWS:   # under the voice's labels (pictures, one per voice)
            window(d, rect, 5)
        for rect, txt in P.FIXED_WINDOWS:   # the envelope's, LEVEL's and PAN's legends in the same windows
            window(d, rect, 5)
            x, y, w, h = rect
            im = page.crop((x, y, x + w, y + h))
            window_label(im, txt)
            page.paste(im, (x, y))
    if tab == 3:
        build_mixer_bg(page)
    if tab == 2:
        # the presets' legends, in the display's orange (as drawn by Suns Kiefer's mockup)
        for key, gx, gy, txt, lx, ly in P.PRESET_KEYS:
            label(d, (lx - 40, ly - 9, 80, 18), txt, 15, fill=LIT)
        x, y, w, h = P.PRESET_SLOT_LABEL
        label(d, (x, y, w, h), "SLOT", 13, fill=LIT)
    return page


def build_mixer_bg(page):
    y0 = P.MIXER_Y
    d = ImageDraw.Draw(page)
    bottom = P.READOUT[3][1] - 6
    for i, x0 in enumerate(P.MIX_STRIPS + [P.MASTER_X]):
        if i:
            d.line([(x0 - 2, y0 + 8), (x0 - 2, bottom)], fill=(60, 66, 70, 255), width=2)
    for i, x0 in enumerate(P.MIX_STRIPS):
        M = P.MIX
        nx, ny, nw, nh = M["num"]
        page.alpha_composite(lit_text(str(i + 1), nw, nh, fit_font("8", nw - 6, nh - 6)), (x0 + nx, y0 + ny))
        for k, txt in (("pan_label", "PAN"), ("snd_label", "DLY SEND"), ("chs_label", "CHAOS"), ("shs_label", "SHIFT"),
                       ("lvl_label", "LEVEL"), ("mute_label", "MUTE"), ("solo_label", "SOLO")):
            x, y, w, h = M[k]
            printed_label(page, (x0 + x, y0 + y, w, h), txt, 11)
        for k in ("pan", "snd", "chs", "shs"):
            cx, cy, s = M[k]
            rl = ring_layer(s)
            page.alpha_composite(rl, (R(x0 + cx - rl.size[0] / 2), R(y0 + cy - rl.size[1] / 2)))
        fx, fy, fw, fh = M["fader"]
        page.alpha_composite(fader_box(fw, fh), (x0 + fx, y0 + fy))
    x, y, w, h = P.MIX_MASTER["label"]
    printed_label(page, (x, y0 + y, w, h), "MASTER", 14)
    fx, fy, fw, fh = P.MIX_MASTER["fader"]
    page.alpha_composite(fader_box(fw, fh), (fx, y0 + fy))


# the artwork's LEVEL slider box (modules_bg.png) and its channel inside it, artwork pixels
FADER_BOX = (893, 28, 40, 182)
FADER_CHANNEL = (13, 16, 12, 151)     # x, y, w, h inside the box


def fader_box(w, h):
    """the artwork's slider box at w x h: its ends kept, its middle stretched (it does not look pulled)"""
    x, y, bw, bh = FADER_BOX
    src = art("modules_bg.png").crop((x, y, x + bw, y + bh))
    k = w / bw
    end = 18
    top = src.crop((0, 0, bw, end)).resize((w, R(end * k)), Image.LANCZOS)
    bot = src.crop((0, bh - end, bw, bh)).resize((w, R(end * k)), Image.LANCZOS)
    mid = src.crop((0, end, bw, bh - end)).resize((w, max(1, h - top.height - bot.height)), Image.LANCZOS)
    out = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    out.paste(top, (0, 0))
    out.paste(mid, (0, top.height))
    out.paste(bot, (0, h - bot.height))
    return out


def fader_travel(w, h):
    """the cap's centre's y range inside a fader box of w x h"""
    _, _, bw, bh = FADER_BOX
    cx, cy, cw, ch = FADER_CHANNEL
    k = w / bw
    return cy * k, h - (bh - cy - ch) * k


# ------------------------------------------------------------------------------------------------ the display font
_font_cache = {}


def font_px(px):
    if px not in _font_cache:
        _font_cache[px] = ImageFont.truetype(FONT, px)
    return _font_cache[px]


def fit_font(text, w, h):
    """the largest size whose '~' (all segments) fits h and text fits w"""
    px = 10
    for p in range(8, 200):
        f = font_px(p)
        b = f.getbbox("~" * max(1, len(text)))
        if b[3] - b[1] > h or b[2] - b[0] > w:
            break
        px = p
    return font_px(px)


def lit_text(text, w, h, f, ghost=True, glow=True, win=True):
    """text in the display's look, w x h: the window, its unlit segments, the lit ones and their glow"""
    layer = Image.new("RGBA", (w * SS, h * SS), WINDOW + (255,) if win else (0, 0, 0, 0))
    big = font_px(round(f.size * SS))
    gb = big.getbbox("~" * len(text))
    ox = (w * SS - (gb[2] - gb[0])) / 2 - gb[0]
    oy = (h * SS - (gb[3] - gb[1])) / 2 - gb[1]
    if ghost:
        ImageDraw.Draw(layer).text((ox, oy), "~" * len(text), font=big, fill=GHOST + (255,))
    lit = Image.new("RGBA", layer.size, (0, 0, 0, 0))
    ImageDraw.Draw(lit).text((ox, oy), text, font=big, fill=LIT + (255,))
    if glow:
        g = np.asarray(lit.filter(ImageFilter.GaussianBlur(3 * SS))).astype(np.float32)
        g[..., 3] *= 0.75
        layer.alpha_composite(Image.fromarray(g.astype(np.uint8), "RGBA"))
    layer.alpha_composite(lit)
    return layer.resize((w, h), Image.LANCZOS)


def cell_font():
    """the front display's size: the largest whose all-segments character with its dot fits a cell"""
    px = 10
    for p in range(8, 200):
        b = font_px(p).getbbox("~.")
        if b[3] - b[1] > P.CELL_H or b[2] - b[0] > P.CELL_W:
            break
        px = p
    return font_px(px)


def cell_origin(big):
    gb = big.getbbox("~.")
    return 3 * SS - gb[0], 4 * SS - gb[1] + (P.CELL_H * SS - (gb[3] - gb[1])) / 2


def glyph_cell(ch, ghost_only=False, ghost=True):
    """one character of the front display, artwork scale: (CELL_W + 6) x (CELL_H + 8), the cell at (3, 4)"""
    w, h = P.CELL_W + 6, P.CELL_H + 8
    layer = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
    big = font_px(round(cell_font().size * SS))
    ox, oy = cell_origin(big)
    if ghost:
        ImageDraw.Draw(layer).text((ox, oy), "~.", font=big, fill=GHOST + (255,))
    if not ghost_only and ch != " ":
        lit = Image.new("RGBA", layer.size, (0, 0, 0, 0))
        ImageDraw.Draw(lit).text((ox, oy), ch, font=big, fill=LIT + (255,))
        g = np.asarray(lit.filter(ImageFilter.GaussianBlur(3.5 * SS))).astype(np.float32)
        g[..., 3] *= 0.8
        layer.alpha_composite(Image.fromarray(g.astype(np.uint8), "RGBA"))
        layer.alpha_composite(lit)
    return layer.resize((w, h), Image.LANCZOS)


def dot_cell(on):
    """a character's dot alone (lit or unlit), with the rect it sits in (artwork scale, relative to the cell layer)"""
    w, h = P.CELL_W + 6, P.CELL_H + 8
    layer = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
    big = font_px(round(cell_font().size * SS))
    ox, oy = cell_origin(big)
    tmp = Image.new("RGBA", layer.size, (0, 0, 0, 0))
    ImageDraw.Draw(tmp).text((ox, oy), "8.", font=big, fill=(255, 255, 255, 255))
    only8 = Image.new("RGBA", layer.size, (0, 0, 0, 0))
    ImageDraw.Draw(only8).text((ox, oy), "8", font=big, fill=(255, 255, 255, 255))
    dmask = np.clip(np.asarray(tmp)[..., 3].astype(np.int32) - np.asarray(only8)[..., 3], 0, 255).astype(np.uint8)
    ys, xs = np.nonzero(dmask > 20)
    col = LIT if on else GHOST
    dot = np.zeros(layer.size[::-1] + (4,), np.uint8)
    dot[..., :3] = col
    dot[..., 3] = dmask
    dl = Image.fromarray(dot, "RGBA")
    if on:
        g = np.asarray(dl.filter(ImageFilter.GaussianBlur(3 * SS))).astype(np.float32)
        g[..., 3] *= 0.8
        layer.alpha_composite(Image.fromarray(g.astype(np.uint8), "RGBA"))
    layer.alpha_composite(dl)
    pad = 5 * SS
    box = (max(0, xs.min() - pad), max(0, ys.min() - pad), min(layer.size[0], xs.max() + pad), min(layer.size[1], ys.max() + pad))
    box = tuple(int(round(v / SS)) * SS for v in box)
    piece = layer.crop(box).resize(((box[2] - box[0]) // SS, (box[3] - box[1]) // SS), Image.LANCZOS)
    return piece, (box[0] // SS, box[1] // SS)


# ------------------------------------------------------------------------------------------------ keys
def recolour(img, rgb):
    """a lit key's glass in another colour: its shading kept, the hue the display's"""
    a = np.asarray(img).astype(np.float32)
    lum = a[..., :3] @ np.array([0.30, 0.59, 0.11], np.float32)
    ref = np.percentile(lum[a[..., 3] > 200], 60) if (a[..., 3] > 200).any() else 128
    k = lum / max(ref, 1)
    out = np.clip(np.array(rgb, np.float32)[None, None, :] * k[..., None], 0, 255)
    hi = np.clip((k - 1.15) / 0.6, 0, 1)[..., None]       # the glass's highlight stays near white
    out = out * (1 - hi) + 255 * hi
    return Image.fromarray(np.concatenate([out, a[..., 3:]], -1).astype(np.uint8), "RGBA")


def grey_glass(img_rect_crop, cx, cy, r):
    """a coloured lit key as unlit grey glass (inside its circle)"""
    a = np.asarray(img_rect_crop).astype(np.float32)
    h, w = a.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w]
    m = np.clip(r + 0.5 - np.hypot(xx - cx, yy - cy), 0, 1)[..., None]
    lum = a[..., :3] @ np.array([0.30, 0.59, 0.11], np.float32)
    inside = m[..., 0] > 0.5
    top = np.percentile(lum[inside], 92) if inside.any() else 200.0
    grey = np.clip(lum / max(top, 1) * 205, 0, 255)[..., None].repeat(3, -1)   # every colour to the same grey glass
    a[..., :3] = a[..., :3] * (1 - m) + grey * m
    return Image.fromarray(a.astype(np.uint8), "RGBA")


def round_alpha(img):
    """a key picture cut to its circle (the front panel's unlit glass, lifted off the panel)"""
    a = np.asarray(img).copy()
    h, w = a.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w]
    r = min(w, h) / 2 - 1
    m = np.clip(r + 0.5 - np.hypot(xx - (w - 1) / 2, yy - (h - 1) / 2), 0, 1)
    a[..., 3] = (a[..., 3] * m).astype(np.uint8)
    return Image.fromarray(a, "RGBA")


# ------------------------------------------------------------------------------------------------ the voices' labels
def voice_labels():
    """each voice's twenty label-window texts (tools/voices.py, "A|B" = two lines)"""
    return voices.labels()


def window_label(im, text):
    """a label window's text: ice letters with a glow, one line or two (A|B), fitted to the window"""
    w, h = im.size
    lines = text.split("|")
    px = 13 if len(lines) == 1 else 11
    while px > 11:
        f = label_font(px * SS)
        if max(f.getlength(t) for t in lines) <= (w - 8) * SS:
            break
        px -= 1
    f = label_font(px * SS)
    tw = int(max(f.getlength(t) for t in lines)) + 4 * SS
    big_ = Image.new("RGBA", (max(tw, w * SS), h * SS), (0, 0, 0, 0))
    dd = ImageDraw.Draw(big_)
    gap = (px + 3) * SS
    for i, t in enumerate(lines):
        dd.text((big_.width / 2, h * SS / 2 + (i - (len(lines) - 1) / 2) * gap), t, font=f, anchor="mm", fill=LIT + (255,))
    room = (w - 6) * SS
    if tw > room:   # a long word: condensed into the window, not made smaller
        inner = big_.crop(((big_.width - tw) // 2, 0, (big_.width + tw) // 2, h * SS)).resize((room, h * SS), Image.LANCZOS)
        big_ = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
        big_.alpha_composite(inner, ((w * SS - room) // 2, 0))
    elif big_.width != w * SS:
        big_ = big_.crop(((big_.width - w * SS) // 2, 0, (big_.width + w * SS) // 2, h * SS))
    small = big_.resize((w, h), Image.LANCZOS)
    im.alpha_composite(small.filter(ImageFilter.GaussianBlur(2.5)))
    im.alpha_composite(small)


# ------------------------------------------------------------------------------------------------ TUI.json pieces
def bounds_s(x, y, w, h):
    return "%d %d %d %d" % (R(x), R(y), R(w), R(h))


def sub(name, typ, data, b, visible="Always"):
    return {"version": 2, "componentData": {"version": 1, "name": name, "type": typ, "data": data},
            "handle remapping": {"version": 1, "map": []},
            "bounds": {"version": 2, "acceptsHWFocus": "No", "showWhenDataModelInvalid": "Show", "whenVisible": visible,
                       "boundsType": "Absolute", "bounds": b, "additionalInvalidatingHandles": []}}


def focus(w, h):
    return sub("Focus", "Focus", {"version": 1, "backgroundColour": "00000000", "outlineColour": "00000000",
                                  "backgroundInset": 2.0, "outlineThickness": 0.0}, bounds_s(0, 0, w, h), "WhenFocussed")


def action(on, handler, extra=""):
    return {"version": 2, "onAction": on, "handler": handler, "handleName": "" if handler == "Show Overlay" else "Data",
            "additionalData": extra, "handle remapping": {"version": 1, "map": []}}


KNOB_ACTIONS = [action("Mouse Down", "Q-Link"), action("Double Click", "Show Overlay", "knob overlay"),
                action("Enter Pressed", "Show Overlay", "knob overlay")]
KEY_ACTIONS = [action("Mouse Down", "Q-Link"), action("Enter Pressed", "Toggle Switch")]
RADIO_ACTIONS = [action("Mouse Down", "Q-Link")]


def definition(key, actions, parts, ignore=False):
    return {"key": key, "value": {
        "version": 4, "actions": actions,
        "backgroundData": {"version": 1, "focussed": {"version": 1, "colour": "0", "image": ""},
                           "unfocussed": {"version": 1, "colour": "0", "image": ""}},
        "ignoreMousePresses": ignore, "disableCoarseDataWheel": False, "repeats": 1, "hideQLinkBounds": True,
        "componentsData": parts}}


def filmstrip(name, img, frames, w, h, drag="Vertical", x=0, y=0):
    return sub(name, "Knob", {"version": 5, "knobType": "FilmStrip", "filmStrip": img, "numFrames": frames,
                              "invert": False, "dragOrientation": drag, "handleName": "Data"}, bounds_s(x, y, w, h))


def button(on_img, off_img, bid, n, w, h):
    return sub("Button", "Button", {"version": 2, "onImage": on_img, "offImage": off_img, "buttonId": bid,
                                    "numButtonsInGroup": n, "handleName": "Data", "gestureBehaviour": "Instant"},
               bounds_s(0, 0, w, h))


def value_label(w, h, px, colour="7dcdf5"):
    """MPC's own text: the parameter's value (Titillium Web, the font MPC renders)"""
    return sub("Value", "Label", {"version": 1, "textStyle": {"version": 1, "font": {"version": 1, "name": "Titillium Web",
                                                                                  "style": "SemiBold", "height": px},
                                                            "colour": "ff" + colour,
                                                            "justification": "horizontallyCentred verticallyCentred",
                                                            "case": "Original"},
                                  "type": "Value", "handleName": "Data"}, bounds_s(0, 0, w, h))


def placed(dkey, name, index, rect, focus_on=True):
    x, y, w, h = rect
    return {"version": 2,
            "componentData": {"version": 1, "name": name, "type": dkey, "data": {"version": 1, "handleName": "Data"}},
            "handle remapping": {"version": 1, "map": [{"key": "Data", "value": "Parameter %d" % index}]},
            "bounds": {"version": 2, "acceptsHWFocus": "Yes" if focus_on else "No", "showWhenDataModelInvalid": "Hide",
                       "whenVisible": "Always", "boundsType": "Absolute", "bounds": bounds_s(x, y, w, h),
                       "additionalInvalidatingHandles": []}}


def picture(img, rect, index, option, count):
    """an image shown while parameter index is at option (of count): MPC's IndexedEnabling"""
    c = sub("Picture", "Image", {"version": 2, "imageType": "Regular", "colour": "0", "image": img}, bounds_s(*rect))
    c["bounds"]["additionalInvalidatingHandles"] = ["IndexedEnabling/%d/%d/Parameter %d" % (option, count, index)]
    return c


# ------------------------------------------------------------------------------------------------ the builder
class Skin:
    def __init__(self, skin, params):
        self.skin = skin
        self.index = {p["key"]: i for i, p in enumerate(params)}
        self.params = params
        self.defs = []
        self.used = set()
        self.saved = {}

    def save(self, img, name, rgb=True):
        """write a picture once (identical pictures share one file)"""
        data = img.convert("RGB" if rgb else "RGBA")
        h = hash(data.tobytes()) ^ hash(data.size) ^ hash(rgb)
        if h in self.saved:
            return self.saved[h]
        base, n = name[:-4], 2
        while name in self.used:   # a name already taken by another picture: a new one
            name = "%s_%d.png" % (base, n)
            n += 1
        data.save(os.path.join(self.skin, name))
        self.used.add(name)
        self.saved[h] = name
        return name

    def opts(self, key):
        return len(self.params[self.index[key]].get("options") or [])

    def opt_names(self, key):
        return list(self.params[self.index[key]].get("options") or [])

    def has_def(self, key):
        return any(d["key"] == key for d in self.defs)


def knob_strip(sk, img, s):
    """the transparent filmstrip of a knob picture at s px: 128 frames turning -150..+150 degrees"""
    name = "bz_knob_%s_%d.png" % (img, s)
    if name not in sk.used:
        pic = art({"big": "knob_big.png", "encoder": "knob_encoder.png", "mod_green": "mod_knob_green.png",
                   "mod_yellow": "mod_knob_yellow.png"}[img])
        big = pic.resize((s * SS, s * SS), Image.LANCZOS)
        strip = Image.new("RGBA", (s, s * KNOB_FRAMES), (0, 0, 0, 0))
        for k in range(KNOB_FRAMES):
            ang = -KNOB_SPAN + 2 * KNOB_SPAN * k / (KNOB_FRAMES - 1)
            strip.paste(big.rotate(-ang, resample=Image.BICUBIC).resize((s, s), Image.LANCZOS), (0, k * s))
        assert strip.size[1] <= 16128
        strip.save(os.path.join(sk.skin, name))
        sk.used.add(name)
    return name


def slider_strip(sk, bg, rect, cap, y_top, y_bot, name):
    """a slider's filmstrip: frames of the slider's own size (the page under it, the cap from the bottom (0) to the
    top (1)); y_top / y_bot: the cap's centre's range inside the frame. -> (file, frame count)"""
    x, y, w, h = rect
    under = bg.crop((x, y, x + w, y + h)).convert("RGBA")
    frames = min(101, MAX_STRIP // h)
    strip = Image.new("RGB", (w, h * frames))
    for k in range(frames):
        v = k / (frames - 1)
        fr = under.copy()
        cy = y_bot + (y_top - y_bot) * v
        fr.alpha_composite(cap, ((w - cap.size[0]) // 2, R(cy - cap.size[1] / 2)))
        strip.paste(fr.convert("RGB"), (0, k * h))
    return sk.save(strip, name), frames


def build_page(sk, tab):
    """(the page's background, its components)"""
    bg = make_background(tab)
    kids = []
    t = "fmxz"[tab]
    sx, sy = P.scale(tab)
    seen_focus = set()

    def place(dkey, name, key, rect):
        f = key not in seen_focus
        seen_focus.add(key)
        kids.append(placed(dkey, name, sk.index[key], rect, f))

    def knob(key, pcx, pcy, s, img):
        name = knob_strip(sk, img, s)
        tb = max(s, 60)                       # a finger-sized touch box around small knobs
        dkey = "bzKnob_%s_%d_%d" % (img, s, tb)
        if not sk.has_def(dkey):
            sk.defs.append(definition(dkey, KNOB_ACTIONS, [focus(tb, tb), filmstrip("Knob", name, KNOB_FRAMES - 1, s, s,
                                                                                     "Vertical", (tb - s) // 2, (tb - s) // 2)]))
        place(dkey, key, key, (R(pcx - tb / 2), R(pcy - tb / 2), tb, tb))

    # ---- knobs
    for kt, key, cx, cy, size, img, ring in P.KNOBS:
        if kt == tab:
            knob(key, *P.page(tab, cx, cy), knob_px(tab, size), img)

    thumb = art("thumb_v.png")
    # ---- sliders (MODULES): the cap along the channel; the top is the maximum
    if tab == 1:
        tw, th = R(P.THUMB_V[0] * sx), R(P.THUMB_V[1] * sy)
        tpic = thumb.resize((tw, th), Image.LANCZOS)
        for key, x, y0, y1 in P.VSLIDERS:
            if sk.opts(key):
                continue   # option sliders are radio groups (below)
            px, py0 = P.page(tab, x, y0)
            _, py1 = P.page(tab, x, y1)
            w = tw + 18
            rect = (R(px - w / 2), R(py0) - 2, w, R(py1 - py0) + 4)
            name, frames = slider_strip(sk, bg, rect, tpic, 2 + th / 2, rect[3] - 2 - th / 2, "bz_m_slider_%s.png" % key)
            tb = 56
            dkey = "bzSlider_%s" % key
            sk.defs.append(definition(dkey, KNOB_ACTIONS, [focus(tb, rect[3]), filmstrip("Slider", name, frames, w, rect[3],
                                                                                          "Vertical", (tb - w) // 2, 0)]))
            place(dkey, key, key, (rect[0] - (tb - w) // 2, rect[1], tb, rect[3]))

    # ---- option controls: radio groups of pictures
    def radio(key, zones, draw_on, draw_off=None, lab=None):
        """zones: page rects per option; draw_on(i, img, rect) draws option i selected onto a copy of its zone"""
        n = len(zones)
        for i, rect in enumerate(zones):
            off = bg.crop((rect[0], rect[1], rect[0] + rect[2], rect[1] + rect[3])).convert("RGBA")
            if draw_off:
                draw_off(i, off, rect)
            on = off.copy()
            draw_on(i, on, rect)
            on_n = sk.save(on, "bz_%s_opt_%s_%s_%d_on.png" % (t, key, lab, i))
            off_n = sk.save(off, "bz_%s_opt_%s_%s_%d_off.png" % (t, key, lab, i))
            dkey = "bzOpt_%s_%s_%d_%s" % (t, key, i, lab or "")
            sk.defs.append(definition(dkey, RADIO_ACTIONS, [button(on_n, off_n, i, n, rect[2], rect[3])]))
            place(dkey, "%s %d" % (key, i), key, rect)

    def thumb_at(img, rect, pos, pic):
        px, py = P.page(tab, *pos)
        img.alpha_composite(pic, (R(px - pic.size[0] / 2) - rect[0], R(py - pic.size[1] / 2) - rect[1]))

    def paste_look(img, zone_rect, art_rect, look):
        """an artwork-scale look (H x W x 3 array) at its place inside a zone picture"""
        x, y, w, h = [R(v) for v in art_rect]
        pic = Image.fromarray(np.asarray(look).astype(np.uint8), "RGB").convert("RGBA")
        pr = prect(tab, x, y, w, h)
        pic = pic.resize((pr[2], pr[3]), Image.LANCZOS)
        img.alpha_composite(pic, (pr[0] - zone_rect[0], pr[1] - zone_rect[1]))

    sw_pic = thumb.resize((R(P.THUMB_SW[0] * sx), R(P.THUMB_SW[1] * sy)), Image.LANCZOS)
    sl_pic = thumb.resize((R(P.THUMB_V[0] * sx), R(P.THUMB_V[1] * sy)), Image.LANCZOS)
    sync_pic = art("thumb_h.png").resize((R(P.THUMB_SYNC[0] * sx), R(P.THUMB_SYNC[1] * sy)), Image.LANCZOS)

    for st_tab, key, thumbs, zones, orient in P.SWITCHES:
        if st_tab == tab:
            radio(key, [prect(tab, *z) for z in zones], lambda i, img, rect, th=thumbs: thumb_at(img, rect, th[i], sw_pic), lab="sw")

    if tab == 2:
        src = art("modules_bg.png")
        hs = {h[0]: h for h in P.HSLIDERS}
        for key, bx, by, bw, bh, n in P.BARS:
            hx0, hy, hx1 = hs[key][1:]
            col_w = bw / n
            thumbs = [(hx0 + 4.5 + i * (hx1 - hx0 - 9) / (n - 1), hy) for i in range(n)]
            seg_rects = [(bx + i * col_w, by, col_w, bh) for i in range(n)]
            words = sk.opt_names(key)
            zr = [prect(tab, bx + i * col_w, by - 2, col_w, bh + 4) for i in range(n)]
            wpx = min(P.BAR_TEXT, zr[0][3] - 5)
            radio(key, zr,
                  lambda i, img, rect, words=words, wpx=wpx: word_cell(img, rect, rect, words[i], True, wpx),
                  lambda i, img, rect, words=words, wpx=wpx: word_cell(img, rect, rect, words[i], False, wpx),
                  lab="bar")
            sw_ = (hx1 - hx0 + 8) / n
            radio(key, [prect(tab, hx0 - 4 + i * sw_, hy - 10, sw_, 20) for i in range(n)],
                  lambda i, img, rect, thumbs=thumbs: thumb_at(img, rect, thumbs[i], sl_pic), lab="hs")
        # CHAOS DEST, SHIFT MODE (0.2.0)
        for key, bx, by, bw, bh, n in P.BARS2:
            col_w = bw / n
            words = sk.opt_names(key)
            zr = [prect(tab, bx + i * col_w, by, col_w, bh) for i in range(n)]
            wpx = min(P.BAR_TEXT, zr[0][3] - 5)
            radio(key, zr,
                  lambda i, img, rect, words=words, wpx=wpx: word_cell(img, rect, rect, words[i], True, wpx),
                  lambda i, img, rect, words=words, wpx=wpx: word_cell(img, rect, rect, words[i], False, wpx),
                  lab="bar")
        # FREE / TEMPO
        sxx, sy0, sy1 = P.SYNC_SLOT
        zones = [prect(tab, *z) for z in P.SYNC_ZONES]
        thumbs = [(sxx, sy0 + 6), (sxx, sy1 - 6)]
        sync_words = sk.opt_names("mod_sync")
        sync_cells = [prect(tab, *z) for z in P.SYNC_CELLS]
        radio("mod_sync", zones,
              lambda i, img, rect: (word_cell(img, rect, sync_cells[i], sync_words[i], True), thumb_at(img, rect, thumbs[i], sync_pic)),
              lambda i, img, rect: word_cell(img, rect, sync_cells[i], sync_words[i], False), lab="sync")

    # ---- keys
    lit_big = recolour(art("btn_lit_big.png"), LIT)
    lit_small = recolour(art("btn_lit_small.png"), LIT)
    for key, (ktab, cx, cy, dia) in P.KEYS.items():
        if ktab != tab:
            continue
        pcx, pcy = P.page(tab, cx, cy)
        if tab == 0:   # the printed keys, stretched with the page: the lit glass stretched the same
            bw_, bh_ = R(min(max(dia + 10, 52), 59) * sx), R(max(dia + 10, 52) * sy)   # 59: the step keys' pitch
            rect = (R(pcx - bw_ / 2), R(pcy - bh_ / 2), bw_, bh_)
            off = bg.crop((rect[0], rect[1], rect[0] + bw_, rect[1] + bh_)).convert("RGBA")
            src_ = lit_big if dia > 50 else lit_small
            g = src_.resize((R(src_.size[0] * sx), R(src_.size[1] * sy)), Image.LANCZOS)
            on = off.copy()
            on.alpha_composite(g, (R(pcx - g.size[0] / 2) - rect[0], R(pcy - g.size[1] / 2) - rect[1]))
        else:
            box = R(46 * sx)
            rect = (R(pcx - box / 2), R(pcy - box / 2), box, box)
            on = bg.crop((rect[0], rect[1], rect[0] + box, rect[1] + box)).convert("RGBA")
            off = grey_glass(on, pcx - rect[0], pcy - rect[1], dia / 2 * sx + 1)
        on_n = sk.save(on, "bz_%s_key_%s_on.png" % (t, key))
        off_n = sk.save(off, "bz_%s_key_%s_off.png" % (t, key))
        dkey = "bzKey_%s" % key
        sk.defs.append(definition(dkey, KEY_ACTIONS, [focus(rect[2], rect[3]), button(on_n, off_n, 1, 1, rect[2], rect[3])]))
        place(dkey, key, key, rect)

    # ---- VOICE RESET (PANEL): the unlit button is in the background; the lit one is the same spot of the artwork with it
    if tab == 0:
        key, cx, cy = P.RESET_KEY
        rect = prect(tab, cx - 54, cy - 54, 108, 108)
        off = bg.crop((rect[0], rect[1], rect[0] + rect[2], rect[1] + rect[3])).convert("RGBA")
        lit_art = reset_key_art(art("front_bg.png"), True).crop((R(cx - 54), R(cy - 54), R(cx + 54), R(cy + 54)))
        on = lit_art.resize((rect[2], rect[3]), Image.LANCZOS)
        on_n = sk.save(on, "bz_f_key_v_reset_on.png")
        off_n = sk.save(off, "bz_f_key_v_reset_off.png")
        sk.defs.append(definition("bzKey_v_reset", KEY_ACTIONS, [focus(rect[2], rect[3]), button(on_n, off_n, 1, 1, rect[2], rect[3])]))
        place("bzKey_v_reset", key, key, rect)

    # ---- LXRXXL's randomizer keys (PANEL): lit, their own colour; unlit, grey glass
    if tab == 0:
        for key, pic, cx, cy, size in P.MOD_KEYS:
            box = size + 14
            rect = (R(cx - box / 2), R(cy - box / 2), box, box)
            glass = art(pic).resize((size, size), Image.LANCZOS)
            off = bg.crop((rect[0], rect[1], rect[0] + box, rect[1] + box)).convert("RGBA")
            on = off.copy()
            on.alpha_composite(glass, (7, 7))
            g = Image.new("RGBA", (box, box), (0, 0, 0, 0))
            g.alpha_composite(glass, (7, 7))
            off.alpha_composite(grey_glass(g, box / 2, box / 2, size / 2))
            on_n = sk.save(on, "zy_f_key_%s_on.png" % key, rgb=False)
            off_n = sk.save(off, "zy_f_key_%s_off.png" % key, rgb=False)
            dkey = "bzKey_%s" % key
            sk.defs.append(definition(dkey, KEY_ACTIONS, [focus(box, box), button(on_n, off_n, 1, 1, box, box)]))
            place(dkey, key, key, rect)

    # ---- the VOICE keys (MODULES): < and >, round grey keys with an ice arrow (lit while pressed)
    if tab == 1:
        for key, cx, cy, size in P.VOICE_KEYS + P.SOUND_KEYS:
            box = size + 12
            tw, th = P.VOICE_KEY_TOUCH   # the touch area: wider than the key's picture
            rect = (R(cx - tw / 2), R(cy - th / 2), tw, th)
            imgs = []
            for lit_ in (False, True):
                im = bg.crop((rect[0], rect[1], rect[0] + tw, rect[1] + th)).convert("RGBA")
                big_ = Image.new("RGBA", (box * SS, box * SS), (0, 0, 0, 0))
                dd = ImageDraw.Draw(big_)
                c, r = box * SS / 2, size * SS / 2
                dd.ellipse([c - r, c - r, c + r, c + r], fill=(38, 42, 46, 255) if not lit_ else (30, 52, 64, 255),
                           outline=(110, 118, 124, 255), width=2 * SS)
                a = size * SS * 0.24
                pts = [(c - a, c), (c + a * 0.8, c - a), (c + a * 0.8, c + a)] if key.endswith("prev") else \
                      [(c + a, c), (c - a * 0.8, c - a), (c - a * 0.8, c + a)]
                dd.polygon(pts, fill=LIT + (255,) if lit_ else (150, 175, 188, 255))
                small = big_.resize((box, box), Image.LANCZOS)
                at = ((tw - box) // 2, (th - box) // 2)
                if lit_:
                    im.alpha_composite(small.filter(ImageFilter.GaussianBlur(3)), at)
                im.alpha_composite(small, at)
                imgs.append(im)
            off_n = sk.save(imgs[0], "zy_m_key_%s_off.png" % key)
            on_n = sk.save(imgs[1], "zy_m_key_%s_on.png" % key)
            dkey = "bzKey_%s" % key
            sk.defs.append(definition(dkey, KEY_ACTIONS, [focus(tw, th), button(on_n, off_n, 1, 1, tw, th)]))
            place(dkey, key, key, rect)
        # the label windows: one picture per voice over each of the six sliders' legends
        names = gen_params.VOICES
        defs_ = voice_labels()
        for si, rect in enumerate(P.LABEL_WINDOWS):
            for vi in range(len(names)):
                im = bg.crop((rect[0], rect[1], rect[0] + rect[2], rect[1] + rect[3])).convert("RGBA")
                window_label(im, defs_[vi][si])
                name = sk.save(im, "zy_m_lbl_%d_%d.png" % (si, vi))
                kids.append(picture(name, rect, sk.index["voice"], vi, len(names)))

    # ---- the selectors in the display's font: TRACK (MODULES), the duck's MIDI CH (FX MIXER)
    def selector(key, sel, texts, prefix_w=0):
        st, x, y, w, h = sel
        rect = prect(tab, x, y, w, h)
        f = fit_font(max(texts, key=len), rect[2] - 10, rect[3] - 8)
        strip = Image.new("RGB", (rect[2], rect[3] * len(texts)))
        for i, txt in enumerate(texts):
            strip.paste(lit_text(txt, rect[2], rect[3], f).convert("RGB"), (0, i * rect[3]))
        name = sk.save(strip, "bz_%s_sel_%s.png" % (t, key))
        dkey = "bzSel_%s" % key
        sk.defs.append(definition(dkey, KNOB_ACTIONS, [focus(rect[2], rect[3]),
                                                       filmstrip("Selector", name, len(texts), rect[2], rect[3])]))
        place(dkey, key, key, rect)

    if tab == 1:
        selector("track", P.TRACK_SEL, ["TRACK %d" % (i + 1) for i in range(8)])
        selector("voice", P.VOICE_SEL, gen_params.VOICES)
    if tab == 2:
        # the preset slot: the same look, page pixels
        rect = P.PRESET_SLOT
        texts = ["%02d" % (i + 1) for i in range(gen_params.NUM_SLOTS)]
        f = fit_font("88", rect[2] - 14, rect[3] - 10)
        strip = Image.new("RGB", (rect[2], rect[3] * len(texts)))
        for i, txt in enumerate(texts):
            strip.paste(lit_text(txt, rect[2], rect[3], f).convert("RGB"), (0, i * rect[3]))
        name = sk.save(strip, "bz_x_sel_pr_slot.png")
        sk.defs.append(definition("bzSel_pr_slot", KNOB_ACTIONS, [focus(rect[2], rect[3]),
                                                                  filmstrip("Selector", name, len(texts), rect[2], rect[3])]))
        place("bzSel_pr_slot", "pr_slot", "pr_slot", rect)
        # the four keys: unlit glass (the front panel's), lit in the display's orange
        unlit = round_alpha(art("front_bg.png").crop((899 - 24, 51 - 24, 899 + 24, 51 + 24)))
        litg = recolour(art("btn_lit_small.png"), LIT)
        dg = P.PRESET_GLASS
        for key, gx, gy, txt, lx, ly in P.PRESET_KEYS:
            box = dg + 10
            rect = (gx - box // 2, gy - box // 2, box, box)
            off = bg.crop((rect[0], rect[1], rect[0] + box, rect[1] + box)).convert("RGBA")
            on = off.copy()
            off.alpha_composite(unlit.resize((dg, dg), Image.LANCZOS), (5, 5))
            on.alpha_composite(litg.resize((dg + 2, dg + 2), Image.LANCZOS), (4, 4))
            on_n = sk.save(on, "bz_x_key_%s_on.png" % key)
            off_n = sk.save(off, "bz_x_key_%s_off.png" % key)
            dkey = "bzKey_%s" % key
            sk.defs.append(definition(dkey, KEY_ACTIONS, [focus(box, box), button(on_n, off_n, 1, 1, box, box)]))
            place(dkey, key, key, rect)

    # ---- the readout: MPC's text, the control last touched
    if tab in P.READOUT:
        x, y, w, h = P.READOUT[tab]
        rect = (x + 20, y + 8, w - 40, h - 16)
        px = 46 if h > 60 else 30
        dkey = "bzReadout_%d" % h
        sk.defs.append(definition(dkey, [], [value_label(rect[2], rect[3], px)], ignore=True))
        kids.append(placed(dkey, "readout", sk.index["readout"], rect, False))
    # ---- VOICE: the SOUND's name (MPC's text)
    if tab == 1:
        x, y, w, h = P.SOUND_NAME
        rect = (x + 6, y + 4, w - 12, h - 8)
        sk.defs.append(definition("zySoundName", [], [value_label(rect[2], rect[3], 22)], ignore=True))
        kids.append(placed("zySoundName", "snd_name", sk.index["snd_name"], rect, False))

    # ---- the mixer (MIXER)
    if tab == 3:
        build_mixer(sk, bg, kids, place, knob, thumb)

    # ---- the PANEL's characters
    if tab == 0:
        glyphs = gen_params.GLYPHS
        for ci, (cx, cy) in enumerate(P.DISPLAY_CELLS):
            rect = prect(tab, cx - 3, cy - 4, P.CELL_W + 6, P.CELL_H + 8)
            under = bg.crop((rect[0], rect[1], rect[0] + rect[2], rect[1] + rect[3])).convert("RGBA")
            for gi, ch in enumerate(glyphs):
                cell = glyph_cell(ch, ghost=False).resize((rect[2], rect[3]), Image.LANCZOS)
                im = under.copy()
                im.alpha_composite(cell)
                name = sk.save(im, "bz_f_glyph_%d.png" % gi)
                kids.append(picture(name, rect, sk.index["disp%d" % (ci + 1)], gi, len(glyphs)))
        for ci, (cx, cy) in enumerate(P.DISPLAY_CELLS):
            piece, (ox, oy) = dot_cell(True)
            r = prect(tab, cx - 3 + ox, cy - 4 + oy, piece.size[0], piece.size[1])
            im = bg.crop((r[0], r[1], r[0] + r[2], r[1] + r[3])).convert("RGBA")
            im.alpha_composite(piece.resize((r[2], r[3]), Image.LANCZOS))
            name = sk.save(im, "bz_f_dot_on.png")
            kids.append(picture(name, r, sk.index["dot%d" % (ci + 1)], 1, 2))
    return bg, kids


def build_mixer(sk, bg, kids, place, knob, thumb):
    y0 = P.MIXER_Y
    M = P.MIX
    # MUTE and SOLO: the step keys' look and size (the front panel's first step key, unlit and lit)
    fb = art("front_bg.png")
    step_off = fb.crop((39 - 27, 545 - 30, 39 + 27, 545 + 30))
    step_lit = step_off.copy()
    lb = recolour(art("btn_lit_big.png"), LIT)
    step_lit.alpha_composite(lb, ((step_off.width - lb.width) // 2, (step_off.height - lb.height) // 2))

    def fader(key, rect):
        x, y, w, h = rect
        t0, t1 = fader_travel(w, h)
        cap = thumb.resize((R(w * 0.36), R(w * 0.36 * 25 / 14)), Image.LANCZOS)   # the artwork's cap, its shape kept
        name, frames = slider_strip(sk, bg, rect, cap, t0, t1, "bz_x_fader_%s.png" % key)
        dkey = "bzFader_%s" % key
        sk.defs.append(definition(dkey, KNOB_ACTIONS, [focus(w + 16, h), filmstrip("Fader", name, frames, w, h, "Vertical", 8, 0)]))
        place(dkey, key, key, (x - 8, y, w + 16, h))

    def mkey(key, rect, txt):
        x, y, w, h = rect
        off = bg.crop((x, y, x + w, y + h)).convert("RGBA")
        on = off.copy()
        off.alpha_composite(step_off.resize((w, h), Image.LANCZOS))
        on.alpha_composite(step_lit.resize((w, h), Image.LANCZOS))
        on_n = sk.save(on, "zy_z_key_%s_on.png" % key)
        off_n = sk.save(off, "zy_z_key_%s_off.png" % key)
        dkey = "bzKey_%s" % key
        sk.defs.append(definition(dkey, KEY_ACTIONS, [focus(w, h), button(on_n, off_n, 1, 1, w, h)]))
        place(dkey, key, key, rect)

    for i, x0 in enumerate(P.MIX_STRIPS):
        n = i + 1
        for k, key in (("pan", "mix_pan%d" % n), ("snd", "mix_snd%d" % n), ("chs", "mix_chs%d" % n), ("shs", "mix_shs%d" % n)):
            cx, cy, s = M[k]
            knob(key, x0 + cx, y0 + cy, s, "big")
        fx, fy, fw, fh = M["fader"]
        fader("mix_lvl%d" % n, (x0 + fx, y0 + fy, fw, fh))
        for k, txt in (("mute", "MUTE"), ("solo", "SOLO")):
            x, y, w, h = M[k]
            mkey("%s%d" % (k, n), (x0 + x, y0 + y, w, h), txt)
    fx, fy, fw, fh = P.MIX_MASTER["fader"]
    fader("mix_master", (fx, y0 + fy, fw, fh))


# ------------------------------------------------------------------------------------------------ selector looks
def _levels(a, rect):
    x, y, w, h = [R(v) for v in rect]
    sub_ = a[y:y + h, x:x + w, :3].astype(np.float32)
    lum = sub_ @ np.array([0.30, 0.59, 0.11], np.float32)
    return lum, np.percentile(lum, 5), np.percentile(lum, 97)


def word_cell(img, zone_rect, cell, text, selected, px=None):
    """a selector's option in label-window style, over its printed one (page px): unselected, ice letters with a glow on
    the window's dark; selected, a lit ice box with dark letters"""
    x, y, w, h = cell
    big = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(big)
    if selected:
        d.rounded_rectangle([0, 0, w * SS - 1, h * SS - 1], radius=3 * SS, fill=(200, 232, 248, 255),
                            outline=(240, 250, 255, 255), width=SS)
    else:
        d.rounded_rectangle([0, 0, w * SS - 1, h * SS - 1], radius=3 * SS, fill=WINDOW + (255,),
                            outline=(46, 52, 56, 255), width=SS)
    if px is None:
        px = min(P.BAR_TEXT, h - 5)
    f = label_font(px * SS)
    tw = int(f.getlength(text)) + 4 * SS
    word = Image.new("RGBA", (tw, h * SS), (0, 0, 0, 0))
    ImageDraw.Draw(word).text((tw / 2, h * SS / 2 + SS), text, font=f, anchor="mm",
                              fill=(8, 22, 30, 255) if selected else LIT + (255,))
    room = (w - 5) * SS
    if tw > room:   # a long word: condensed to its cell, the same height as the others
        word = word.resize((int(room), h * SS), Image.LANCZOS)
    txt = Image.new("RGBA", big.size, (0, 0, 0, 0))
    txt.alpha_composite(word, ((w * SS - word.width) // 2, 0))
    cell_im = big.resize((w, h), Image.LANCZOS)
    t = txt.resize((w, h), Image.LANCZOS)
    if not selected:
        cell_im.alpha_composite(t.filter(ImageFilter.GaussianBlur(2.0)))
    cell_im.alpha_composite(t)
    img.alpha_composite(cell_im, (x - zone_rect[0], y - zone_rect[1]))


def word_px(words, w, h):
    """the size every word of a selector fits its cell at (the same for all, so the bar reads evenly)"""
    px = min(P.BAR_TEXT, h - 5)
    while px > 7 and max(label_font(px * SS).getlength(t) for t in words) > (w - 6) * SS:
        px -= 1
    return px


def bar_looks(src, rects):
    """(unselected, selected) pictures (artwork scale, H x W x 3) of each option of a printed selector, whose first option
    is printed selected (a white box, dark text) and the others not (light text on the dark): each missing look is the
    printed one with its two levels swapped"""
    a = np.asarray(src.convert("RGB"))
    lum0, s_text, s_box = _levels(a, rects[0])     # selected: box bright, text dark
    _, u_dark, u_text = _levels(a, rects[1])       # unselected: dark ground, text light
    looks = []
    for i, r in enumerate(rects):
        lum, lo, hi = _levels(a, r)
        x, y, w, h = [R(v) for v in r]
        printed = a[y:y + h, x:x + w, :3].astype(np.float32)
        if i == 0:
            t = np.clip((s_box - lum) / max(s_box - s_text, 1), 0, 1)          # 0 box, 1 text
            unsel = u_dark + t * (u_text - u_dark)
            looks.append((np.repeat(unsel[..., None], 3, -1), printed))
        else:
            t = np.clip((lum - lo) / max(hi - lo, 1), 0, 1)                    # 0 ground, 1 text
            sel = s_box + t * (s_text - s_box)
            looks.append((printed, np.repeat(sel[..., None], 3, -1)))
    return looks


BADGE = 24


def badge(n):
    """a Q-Link number: the display's orange disc, the number dark on it"""
    big = BADGE * SS
    im = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.ellipse([SS, SS, big - SS - 1, big - SS - 1], fill=LIT + (255,), outline=(4, 12, 20, 255), width=SS)
    d.text((big / 2, big / 2 + SS), str(n), font=label_font(int(15 * SS)), fill=(20, 12, 4, 255), anchor="mm")
    return im.resize((BADGE, BADGE), Image.LANCZOS)


# ------------------------------------------------------------------------------------------------ main
def main(skin):
    tui_path = os.path.join(skin, "TUI.json")
    tui = json.load(open(tui_path))
    cdefs = tui["pageData"]["componentDefinitions"]
    defs = cdefs["localComponentDefinitions"]
    params = json.load(open(os.path.join(P.ROOT, "params.json")))["params"]
    sk = Skin(skin, params)
    pages = {}
    for tab, name in enumerate(P.TABS):
        bg, kids = build_page(sk, tab)
        bgname = "bz_bg_%d.png" % tab
        bg.convert("RGB").save(os.path.join(skin, bgname))
        sk.used.add(bgname)
        pages[name] = [sub("Background", "Image", {"version": 2, "imageType": "Regular", "colour": "0", "image": bgname},
                           bounds_s(0, 0, P.W, P.H))] + kids
    # each Q-Link page: an orange number on each of its controls = the Force knob (1-8) that turns it now
    badge_names = ["" for n in range(1, 9)]   # the pages stay clean: the numbers only check that every key has a control
    sub_kids = {}
    for tab, name in enumerate(P.TABS):
        for sname, keys in zip(P.qlink_names(tab), P.qlink_pages(tab)):
            extra = []
            for n, key in enumerate(keys):
                if key == "-":   # an empty knob
                    continue
                idx = "Parameter %d" % sk.index[key]
                for c in pages[name]:
                    m = c.get("handle remapping", {}).get("map") or []
                    if c["componentData"]["type"] != "Image" and m and m[0]["value"] == idx:
                        x, y, w, h = map(int, c["bounds"]["bounds"].split())
                        bx, by = max(0, min(P.W - BADGE, x - 6)), max(0, min(P.H - BADGE, y - 6))
                        extra.append(sub("Q-Link %d" % (n + 1), "Image", {"version": 2, "imageType": "Regular", "colour": "0",
                                                                          "image": badge_names[n]}, bounds_s(bx, by, BADGE, BADGE)))
                        break
                else:
                    raise SystemExit("panel_art: %s (Q-Link page %s) has no control on the page" % (key, sname))
            sub_kids["%s|%s" % (name, sname)] = extra
    keep = []
    for e in defs:
        tabname = e["key"].split("|")[0]
        if "|" in e["key"] and tabname in pages:
            e["value"]["componentsData"] = pages[tabname]   # no knob numbers on the screen (Suns Kiefer's choice)
            e["value"]["backgroundData"] = {"version": 1, "focussed": {"version": 1, "colour": "ff0e0e0e", "image": ""},
                                            "unfocussed": {"version": 1, "colour": "ff0e0e0e", "image": ""}}
            keep.append(e)
    cdefs["localComponentDefinitions"] = keep + sk.defs
    json.dump(tui, open(tui_path, "w"), indent=1)
    for f in os.listdir(skin):
        if f.endswith(".png") and f not in sk.used:
            os.remove(os.path.join(skin, f))
    print("panel_art: %d definitions, %d pictures" % (len(sk.defs), len(sk.used)))


if __name__ == "__main__":
    main(sys.argv[1])
