#!/usr/bin/env python3
"""
Build the Vestergaard-style ramp-ops UI textures (Phase 7 art pass, offline).

Generates placeholder PNGs into Content/Holdover/UI/ so the Unreal editor
auto-imports them on next open. Everything is procedural and deterministic â€”
re-run after tweaking. Usage:

    python Tools/build_ui_textures.py                # write textures
    python Tools/build_ui_textures.py --preview out.png  # + contact sheet

Textures
    T_Panel_BG.png           512x512   dark brushed panel background (RGB)
    T_Hazard_Stripe.png      256x256   tileable yellow/black hazard stripes (RGB)
    T_Frost_Vignette.png    1024x576   frost creeping from screen edges (RGBA)
    T_Scan_Grid.png          256x128   faint radar grid for the scan panel (RGBA)
    T_Plane_Top.png          256x256   airliner plan-view silhouette (RGBA)
    T_SplitFlap_Digits.png   704x96    0-9 + ':' flap cells, amber on dark (RGB)
"""

import argparse
import math
import os
import random
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

SEED = 20261030
PANEL = (14, 18, 22)
AMBER = (255, 168, 0)
HIVIS = (255, 212, 0)
INK = (14, 14, 14)

# Windows font candidates for the FIDS look (first found wins).
FONT_CANDIDATES = [
    r"C:\Windows\Fonts\arialn.ttf",   # Arial Narrow
    r"C:\Windows\Fonts\impact.ttf",
    r"C:\Windows\Fonts\arialbd.ttf",
    r"C:\Windows\Fonts\courbd.ttf",
]


def pick_font(size):
    for path in FONT_CANDIDATES:
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def smoothstep(x, a, b):
    t = max(0.0, min(1.0, (x - a) / (b - a)))
    return t * t * (3.0 - 2.0 * t)


def build_panel_bg(rng):
    """Dark brushed-metal panel: vertical streaks + fine noise + scanlines."""
    w = h = 512
    img = Image.new("RGB", (w, h), PANEL)
    px = img.load()
    streaks = [rng.randint(-5, 5) for _ in range(w)]
    for x in range(w):
        s = streaks[x]
        for y in range(h):
            j = rng.randint(-3, 3)
            v = s + j - (2 if y % 3 == 0 else 0)
            r, g, b = PANEL
            px[x, y] = (max(0, min(255, r + v)), max(0, min(255, g + v)), max(0, min(255, b + v)))
    return img


def build_hazard(rng):
    """Tileable 45-degree hazard stripes, drawn 2x then downsampled."""
    big = 512
    img = Image.new("RGB", (big, big), HIVIS)
    d = ImageDraw.Draw(img)
    step = 96  # 48px stripes after downscale -> divides 256, tiles cleanly
    for o in range(-big, big * 2, step):
        d.polygon([(o, 0), (o + step // 2, 0),
                   (o + step // 2 + big, big), (o + big, big)], fill=INK)
    return img.resize((256, 256), Image.LANCZOS)


def build_frost(rng):
    """RGBA frost vignette: radial edge falloff + dendrite strokes + speckle."""
    w, h = 1024, 576
    cx, cy = w / 2.0, h / 2.0
    maxd = math.hypot(cx, cy)
    alpha = Image.new("L", (w, h), 0)
    ap = alpha.load()
    for y in range(h):
        for x in range(w):
            d = max(abs(x - cx) / cx, abs(y - cy) / cy)
            edge = smoothstep(d, 0.42, 1.0)
            ap[x, y] = int(200 * (edge ** 1.7))

    # Dendritic ice crystals reaching in from the edges.
    strokes = Image.new("L", (w, h), 0)
    sd = ImageDraw.Draw(strokes)

    def branch(x, y, ang, steps, width, bright):
        for _ in range(steps):
            seg = rng.uniform(8, 22)
            nx, ny = x + math.cos(ang) * seg, y + math.sin(ang) * seg
            # Keep frost in the outer band: never creep past the clear centre.
            if max(abs(nx - cx) / cx, abs(ny - cy) / cy) < 0.50:
                return
            sd.line([(x, y), (nx, ny)], fill=bright, width=width)
            x, y = nx, ny
            ang += rng.uniform(-0.30, 0.30)
            if rng.random() < 0.22 and width > 1:
                branch(x, y, ang + rng.uniform(-0.9, 0.9), steps // 2, width - 1, max(40, bright - 40))
            if not (0 <= x < w and 0 <= y < h):
                return

    for _ in range(90):
        side = rng.randint(0, 3)
        if side == 0:   x, y = rng.uniform(0, w), 0
        elif side == 1: x, y = rng.uniform(0, w), h
        elif side == 2: x, y = 0, rng.uniform(0, h)
        else:           x, y = w, rng.uniform(0, h)
        ang = math.atan2(cy - y, cx - x) + rng.uniform(-0.8, 0.8)
        branch(x, y, ang, rng.randint(14, 26), rng.choice([2, 3, 3]), rng.randint(120, 200))

    # Frost speckle near the edges.
    spk = Image.new("L", (w, h), 0)
    sp = ImageDraw.Draw(spk)
    for _ in range(900):
        x, y = rng.uniform(0, w), rng.uniform(0, h)
        d = max(abs(x - cx) / cx, abs(y - cy) / cy)
        if rng.random() < smoothstep(d, 0.5, 1.0):
            r = rng.uniform(1, 4)
            sp.ellipse([x - r, y - r, x + r, y + r], fill=rng.randint(60, 170))

    from PIL import ImageChops
    alpha = ImageChops.add(alpha, strokes)
    alpha = ImageChops.add(alpha, spk)
    alpha = alpha.filter(ImageFilter.GaussianBlur(1.6))

    out = Image.new("RGBA", (w, h), (225, 242, 255, 0))
    out.putalpha(alpha)
    return out


def build_scan_grid(rng):
    """Faint radar grid + centre crosshair for the SURFACE SCAN panel."""
    w, h = 256, 128
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for x in range(0, w + 1, 16):
        d.line([(x, 0), (x, h)], fill=(60, 110, 120, 60), width=1)
    for y in range(0, h + 1, 16):
        d.line([(0, y), (w, y)], fill=(60, 110, 120, 60), width=1)
    d.rectangle([0, 0, w - 1, h - 1], outline=(80, 150, 160, 130), width=2)
    d.line([(w // 2, h // 2 - 14), (w // 2, h // 2 + 14)], fill=(90, 170, 180, 120), width=1)
    d.line([(w // 2 - 14, h // 2), (w // 2 + 14, h // 2)], fill=(90, 170, 180, 120), width=1)
    for (tx, ty) in [(4, 4), (w - 5, 4), (4, h - 5), (w - 5, h - 5)]:
        d.rectangle([tx, ty, tx + 4, ty + 4], fill=(90, 170, 180, 160))
    return img


def build_plane_top(rng):
    """Airliner plan-view silhouette (nose up), drawn 2x then downsampled."""
    S = 2
    w = h = 256 * S
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    col = (215, 222, 230, 235)

    def mirror(poly):
        return [(w - x, y) for (x, y) in poly]

    fuselage = [(128 * S, 12 * S), (138 * S, 36 * S), (138 * S, 228 * S),
                (134 * S, 242 * S), (122 * S, 242 * S), (118 * S, 228 * S),
                (118 * S, 36 * S)]
    wing_l = [(121 * S, 110 * S), (26 * S, 180 * S), (26 * S, 200 * S), (121 * S, 158 * S)]
    stab_l = [(123 * S, 212 * S), (74 * S, 236 * S), (74 * S, 246 * S), (123 * S, 232 * S)]

    d.polygon(fuselage, fill=col)
    for poly in (wing_l, stab_l):
        d.polygon(poly, fill=col)
        d.polygon(mirror(poly), fill=col)

    # Engines under the wings.
    for ex in (84, 154):
        d.rounded_rectangle([ex * S, 146 * S, (ex + 18) * S, 176 * S], radius=8 * S, fill=col)

    return img.resize((256, 256), Image.LANCZOS)


def build_splitflap(rng):
    """11 flap cells: 0-9 and ':'. Amber digits, seam line, drawn x2."""
    cw, ch, n = 64, 96, 11
    S = 2
    img = Image.new("RGB", (cw * n * S, ch * S), INK)
    d = ImageDraw.Draw(img)
    font = pick_font(140 * S // 2)
    chars = list("0123456789:")
    for i, c in enumerate(chars):
        x0 = i * cw * S
        d.rectangle([x0, 0, x0 + cw * S - 1, ch * S - 1], fill=(16, 20, 24))
        d.rectangle([x0, 0, x0 + cw * S - 1, 6 * S], fill=(26, 31, 36))          # top lip
        d.rectangle([x0, ch * S // 2 - 2 * S, x0 + cw * S - 1, ch * S // 2 + 2 * S],
                    fill=(6, 8, 10))                                              # flap seam
        d.rectangle([x0, 0, x0 + cw * S - 1, ch * S - 1], outline=(40, 46, 52), width=2)
        bbox = d.textbbox((0, 0), c, font=font)
        tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
        d.text((x0 + (cw * S - tw) // 2 - bbox[0], (ch * S - th) // 2 - bbox[1]),
               c, font=font, fill=AMBER)
    return img.resize((cw * n, ch), Image.LANCZOS)


def contact_sheet(textures, out_path):
    """Checkerboard preview so alpha reads clearly; one image to eyeball all."""
    pad, label_h = 16, 22
    cw = 1080
    panels = []
    for name, im in textures:
        prev = im.copy()
        if prev.width > 480:
            prev = prev.resize((480, int(prev.height * 480 / prev.width)), Image.LANCZOS)
        panels.append((name, prev))

    rows = [[panels[0], panels[1]], [panels[2]], [panels[3], panels[4]], [panels[5]]]
    row_h = [label_h + max(p[1].height for p in r) + pad for r in rows]
    sheet = Image.new("RGB", (cw, sum(row_h) + pad), (60, 60, 60))
    dr = ImageDraw.Draw(sheet)
    y = pad // 2
    for r, h in zip(rows, row_h):
        x = pad
        for name, im in r:
            bg = Image.new("RGB", im.size, (60, 60, 60))
            chk = ImageDraw.Draw(bg)
            for cy in range(0, im.height, 16):
                for cx in range(0, im.width, 16):
                    if (cx // 16 + cy // 16) % 2 == 0:
                        chk.rectangle([cx, cy, cx + 15, cy + 15], fill=(90, 90, 90))
            if im.mode == "RGBA":
                bg.paste(im, (0, 0), im)
            else:
                bg.paste(im, (0, 0))
            sheet.paste(bg, (x, y + label_h))
            dr.text((x, y + 4), name, fill=(255, 255, 255))
            x += bg.width + pad
        y += h
    sheet.save(out_path)


def main():
    ap = argparse.ArgumentParser()
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap.add_argument("--out", default=os.path.join(root, "Content", "Holdover", "UI"))
    ap.add_argument("--preview", default=None, help="write a contact sheet PNG here")
    args = ap.parse_args()

    rng = random.Random(SEED)
    os.makedirs(args.out, exist_ok=True)

    textures = [
        ("T_Panel_BG.png", build_panel_bg(rng), "RGB", (512, 512)),
        ("T_Hazard_Stripe.png", build_hazard(rng), "RGB", (256, 256)),
        ("T_Frost_Vignette.png", build_frost(rng), "RGBA", (1024, 576)),
        ("T_Scan_Grid.png", build_scan_grid(rng), "RGBA", (256, 128)),
        ("T_Plane_Top.png", build_plane_top(rng), "RGBA", (256, 256)),
        ("T_SplitFlap_Digits.png", build_splitflap(rng), "RGB", (704, 96)),
    ]

    failed = False
    for name, img, mode, size in textures:
        ok = img.mode == mode and img.size == size
        if mode == "RGBA":
            ok = ok and img.getextrema()[3][0] < 255  # real transparency present
        if not ok:
            print(f"FAIL  {name}: got {img.mode} {img.size}, want {mode} {size}")
            failed = True
            continue
        path = os.path.join(args.out, name)
        img.save(path)
        kb = os.path.getsize(path) // 1024
        print(f"OK    {name:<24} {img.mode:<5} {img.size[0]}x{img.size[1]}  {kb} KB")

    if args.preview:
        contact_sheet([(n, i) for n, i, _, _ in textures], args.preview)
        print(f"preview -> {args.preview}")

    if failed:
        sys.exit(1)
    print("all textures OK")


if __name__ == "__main__":
    main()
