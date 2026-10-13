#!/usr/bin/env python3
"""A picture of a built page with chosen values, as MPC would draw it (offline check, README pictures).

    python3 tools/preview.py <Plugin Skins dir> <out.png> <tab 0|1|2>[.<Q-Link page 1..>] [key=value ...]

value: a number 0..1, or an option's index; keys not given take params.json's default. A key (button) given as
1 is drawn lit; disp1..3 take a character ("disp1=1"), dots 0/1."""
import json
import os
import sys

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_params  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main(skin, out, tab, sets):
    tui = json.load(open(os.path.join(skin, "TUI.json")))["pageData"]
    params = json.load(open(os.path.join(ROOT, "params.json")))["params"]
    vals = {}
    texts = {}
    for i, p in enumerate(params):
        n = len(p.get("options") or [])
        vals[i] = (float(p.get("default", 0)), n)
    for s in sets:
        k, v = s.split("=", 1)
        i = [p["key"] for p in params].index(k)
        if k.startswith("disp"):
            v = str(gen_params.GLYPHS.index(v if v != "_" else " "))
        if k == "readout":
            texts[i] = v
            continue
        vals[i] = (float(v), vals[i][1])
    defs = {d["key"]: d["value"] for d in tui["componentDefinitions"]["localComponentDefinitions"]}
    tab, _, subp = str(tab).partition(".")   # "1" or "1.3": tab 1's third Q-Link page (its Q-Link numbers)
    page = next(defs[t["componentName"]] for t in tui["tabs"]
                if t["fnKeyIndex"] == int(tab) and t["fnKeySubIndex"] == (int(subp) - 1 if subp else 0))
    img = Image.new("RGBA", (1280, 628), (0, 0, 0, 255))

    def opt(i):
        v, n = vals[i]
        return int(round(v)) if n else v

    for c in page["componentsData"]:
        x, y, w, h = map(int, c["bounds"]["bounds"].split())
        hs = c["bounds"].get("additionalInvalidatingHandles") or []
        if hs:
            _, o, n, par = hs[0].split("/")
            if opt(int(par.split()[1])) != int(o):
                continue
        typ = c["componentData"]["type"]
        if typ == "Image":
            im = Image.open(os.path.join(skin, c["componentData"]["data"]["image"])).convert("RGBA")
            img.alpha_composite(im.resize((w, h)), (x, y))
            continue
        d = defs.get(typ)
        if not d:
            continue
        pi = int(c["handle remapping"]["map"][0]["value"].split()[1])
        v, n = vals[pi]
        for part in d["componentsData"]:
            sd = part["componentData"]
            px, py, pw, ph = map(int, part["bounds"]["bounds"].split())
            if sd["type"] == "Knob":
                st = Image.open(os.path.join(skin, sd["data"]["filmStrip"])).convert("RGBA")
                nf = sd["data"]["numFrames"]
                frames = st.size[1] // ph
                norm = v / (n - 1) if n > 1 else v
                k = min(frames - 1, int(round(norm * (frames - 1))))
                img.alpha_composite(st.crop((0, k * ph, pw, (k + 1) * ph)), (x + px, y + py))
            elif sd["type"] == "Button":
                dd = sd["data"]
                on = (int(round(v)) == dd["buttonId"]) if dd["numButtonsInGroup"] > 1 else v > 0.5
                im = Image.open(os.path.join(skin, dd["onImage"] if on else dd["offImage"])).convert("RGBA")
                img.alpha_composite(im, (x + px, y + py))
            elif sd["type"] == "Label" and pi in texts:   # MPC's own text (Titillium Web): a stand-in font here
                st = sd["data"]["textStyle"]
                col = st["colour"]
                f = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf", int(st["font"]["height"] * 0.8))
                ImageDraw.Draw(img).text((x + px + pw / 2, y + py + ph / 2), texts[pi], font=f, anchor="mm",
                                         fill="#" + col[2:])
    img.convert("RGB").save(out)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4:])
