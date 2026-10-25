"""Vitals HUD stage icons: SVG sources generated here, rendered with resvg to PNG.

    python tools/icons.py            -> assets/Interface/VitalsHUD/*.png (64 px) + build/icon_sheet.png (preview)

Every icon is drawn on a 100x100 canvas with a dark outline so it reads on any background, and each stage of a
meter gets its own picture (the drop fills up, the mud splat grows, the belly grows by trimester).
"""
import math
import os
import random

import resvg_py
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets", "Interface", "VitalsHUD")
SHEET = os.path.join(ROOT, "build", "icon_sheet.png")
SIZE = 64

O = "#14161c"  # outline
SW = 4.2       # outline width


def svg(body, defs=""):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="100" height="100">'
            f'<defs>{defs}</defs>{body}</svg>')


def lin(id_, top, bottom, x1=0, y1=0, x2=0, y2=1):
    return (f'<linearGradient id="{id_}" x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}">'
            f'<stop offset="0" stop-color="{top}"/><stop offset="1" stop-color="{bottom}"/></linearGradient>')


def rad(id_, inner, outer, cx=0.4, cy=0.35, r=0.75):
    return (f'<radialGradient id="{id_}" cx="{cx}" cy="{cy}" r="{r}">'
            f'<stop offset="0" stop-color="{inner}"/><stop offset="1" stop-color="{outer}"/></radialGradient>')


def shape(d, fill, stroke=O, sw=SW, extra=""):
    return f'<path d="{d}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}" stroke-linejoin="round" stroke-linecap="round" {extra}/>'


def shine(d, opacity=0.55):
    return f'<path d="{d}" fill="none" stroke="#ffffff" stroke-opacity="{opacity}" stroke-width="3.2" stroke-linecap="round"/>'


ICONS = {}


def icon(name):
    def wrap(fn):
        ICONS[name] = fn
        return fn
    return wrap


# ---- wetness: a drop that fills up --------------------------------------------------------------------------

DROP = "M50 7 C50 7 21 43 21 62 A29 29 0 0 0 79 62 C79 43 50 7 50 7 Z"


def small_drop(cx, cy, s):
    return (f"M{cx} {cy - 10 * s} C{cx} {cy - 10 * s} {cx - 6 * s} {cy - 2 * s} {cx - 6 * s} {cy + 2 * s} "
            f"A{6 * s} {6 * s} 0 0 0 {cx + 6 * s} {cy + 2 * s} C{cx + 6 * s} {cy - 2 * s} {cx} {cy - 10 * s} {cx} {cy - 10 * s} Z")


def wet(level, extras=0):
    defs = lin("w", "#9be0ff", "#2a78e0") + f'<clipPath id="c"><rect x="0" y="{91 - 84 * level}" width="100" height="100"/></clipPath>'
    body = shape(DROP, "#1f2a38", sw=0)
    if level > 0:
        body += f'<path d="{DROP}" fill="url(#w)" clip-path="url(#c)"/>'
        surface = 91 - 84 * level
        if 0 < level < 1:
            body += f'<path d="M14 {surface} Q32 {surface - 3} 50 {surface} T86 {surface}" fill="none" stroke="#d8f3ff" stroke-width="2.4" clip-path="url(#d)"/>'
            defs += f'<clipPath id="d"><path d="{DROP}"/></clipPath>'
    body += shape(DROP, "none")
    if level >= 0.5:
        body += shine("M33 52 Q31 62 36 70")
    for i in range(extras):
        cx, cy, s = [(84, 30, 0.9), (16, 34, 0.75), (88, 74, 0.6)][i]
        body += shape(small_drop(cx, cy, s), "url(#w)", sw=3)
    return svg(body, defs)


icon("wet_dry")(lambda: wet(0.0))
icon("wet_damp")(lambda: wet(0.35))
icon("wet_sweaty")(lambda: wet(0.72, 1))
icon("wet_soaked")(lambda: wet(1.0, 3))

# ---- arousal: a heart that fills, glows and burns ------------------------------------------------------------

HEART = "M50 88 C20 66 7 50 7 33 C7 18 18 9 31 9 C40 9 46 14 50 21 C54 14 60 9 69 9 C82 9 93 18 93 33 C93 50 80 66 50 88 Z"


def flame(cx, base, w, h, outer="#ffb030", inner="#fff0a0"):
    d = (f"M{cx} {base - h} C{cx + w * 0.2} {base - h * 0.6} {cx + w} {base - h * 0.45} {cx + w * 0.6} {base} "
         f"L{cx - w * 0.6} {base} C{cx - w} {base - h * 0.45} {cx - w * 0.15} {base - h * 0.55} {cx} {base - h} Z")
    di = (f"M{cx} {base - h * 0.55} C{cx + w * 0.15} {base - h * 0.35} {cx + w * 0.45} {base - h * 0.25} {cx + w * 0.3} {base} "
          f"L{cx - w * 0.3} {base} C{cx - w * 0.45} {base - h * 0.25} {cx - w * 0.1} {base - h * 0.3} {cx} {base - h * 0.55} Z")
    return shape(d, outer, sw=3.2) + f'<path d="{di}" fill="{inner}"/>'


def arousal(level, glow=False, fire=False):
    top, bottom = ("#ff8ab8", "#e0306e") if not fire else ("#ff6a6a", "#b0102e")
    defs = lin("h", top, bottom) + f'<clipPath id="c"><rect x="0" y="{89 - 80 * level}" width="100" height="100"/></clipPath>'
    body = ""
    if glow or fire:
        body += f'<path d="{HEART}" fill="none" stroke="#ff5c96" stroke-opacity="0.35" stroke-width="12" transform="translate(50 50) scale(1.02) translate(-50 -50)"/>'
    if fire:
        body += flame(30, 30, 11, 28) + flame(50, 24, 13, 30) + flame(70, 30, 11, 28)
    body += shape(HEART, "#2a1e26", sw=0)
    if level > 0:
        body += f'<path d="{HEART}" fill="url(#h)" clip-path="url(#c)"/>'
    body += shape(HEART, "none")
    if level >= 0.9:
        body += shine("M22 26 Q24 18 32 17")
    if glow and not fire:
        for x1, y1, x2, y2 in [(4, 52, 13, 52), (87, 52, 96, 52), (10, 70, 17, 64), (90, 70, 83, 64)]:
            body += f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="#ff6aa0" stroke-width="4" stroke-linecap="round"/>'
    return svg(body, defs)


icon("aro_calm")(lambda: arousal(0.0))
icon("aro_interested")(lambda: arousal(0.4))
icon("aro_aroused")(lambda: arousal(1.0))
icon("aro_lustful")(lambda: arousal(1.0, glow=True))
icon("aro_desperate")(lambda: arousal(1.0, fire=True))

# ---- dirt: soap bubbles when clean, a growing mud splat ------------------------------------------------------


def splat(cx, cy, r, lobes, seed):
    rng = random.Random(seed)
    pts = []
    n = lobes * 2
    for i in range(n):
        a = 2 * math.pi * (i + rng.uniform(-0.15, 0.15)) / n
        rr = r * rng.uniform(1.02, 1.3) if i % 2 == 0 else r * rng.uniform(0.7, 0.8)
        pts.append((cx + rr * math.cos(a), cy + rr * math.sin(a)))
    # smooth closed curve through the points (Catmull-Rom -> Bezier)
    d = f"M{pts[0][0]:.1f} {pts[0][1]:.1f} "
    for i in range(n):
        p0, p1, p2, p3 = pts[i - 1], pts[i], pts[(i + 1) % n], pts[(i + 2) % n]
        c1 = (p1[0] + (p2[0] - p0[0]) / 6, p1[1] + (p2[1] - p0[1]) / 6)
        c2 = (p2[0] - (p3[0] - p1[0]) / 6, p2[1] - (p3[1] - p1[1]) / 6)
        d += f"C{c1[0]:.1f} {c1[1]:.1f} {c2[0]:.1f} {c2[1]:.1f} {p2[0]:.1f} {p2[1]:.1f} "
    return d + "Z"


def bubble(cx, cy, r):
    return (f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="#bfe8ff" fill-opacity="0.35" stroke="{O}" stroke-width="3.4"/>'
            f'<circle cx="{cx}" cy="{cy}" r="{r - 2.2}" fill="none" stroke="#e6f7ff" stroke-width="2.2"/>'
            f'<path d="M{cx - r * 0.5} {cy - r * 0.15} A{r * 0.55} {r * 0.55} 0 0 1 {cx - r * 0.05} {cy - r * 0.55}" fill="none" stroke="#ffffff" stroke-width="2.6" stroke-linecap="round"/>')


def sparkle(cx, cy, s):
    d = f"M{cx} {cy - s} Q{cx + s * 0.18} {cy - s * 0.18} {cx + s} {cy} Q{cx + s * 0.18} {cy + s * 0.18} {cx} {cy + s} Q{cx - s * 0.18} {cy + s * 0.18} {cx - s} {cy} Q{cx - s * 0.18} {cy - s * 0.18} {cx} {cy - s} Z"
    return shape(d, "#ffffff", sw=2.6)


def dirt(stage):
    defs = rad("m", "#9a6a3a", "#4e3218")
    if stage == 0:
        body = bubble(40, 58, 22) + bubble(70, 34, 13) + bubble(72, 72, 9) + sparkle(24, 24, 11)
        return svg(body, defs)
    r = [0, 16, 21, 25, 27][stage]
    rng = random.Random(3 + stage * 5)
    blobs = [(50, 52, r)]
    lobes = 6 + stage
    for k in range(lobes):
        a = 2 * math.pi * (k + rng.uniform(-0.25, 0.25)) / lobes
        d = r * rng.uniform(0.62, 0.85)
        blobs.append((50 + d * math.cos(a), 52 + d * math.sin(a), r * rng.uniform(0.32, 0.5)))
    for k in range(stage + 1):  # flung drops, joined to the blot by a thin neck or loose
        a = rng.uniform(0, 2 * math.pi)
        d = r * rng.uniform(1.4, 1.7)
        blobs.append((50 + d * math.cos(a), 52 + d * math.sin(a), rng.uniform(3.0, 5.0)))
    # union of circles: all outlines first, then all fills on top hide the inner edges
    body = "".join(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{rr:.1f}" fill="{O}" stroke="{O}" stroke-width="{SW * 2}"/>' for x, y, rr in blobs)
    body += "".join(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{rr:.1f}" fill="url(#m)"/>' for x, y, rr in blobs)
    defs = ('<radialGradient id="m" gradientUnits="userSpaceOnUse" cx="42" cy="40" r="42">'
            '<stop offset="0" stop-color="#a87442"/><stop offset="1" stop-color="#4a2e14"/></radialGradient>')
    if stage >= 3:
        for x in (42, 60):
            body += shape(f"M{x - 4} 70 L{x - 4} {84 + (x % 7)} A4 4 0 0 0 {x + 4} {84 + (x % 7)} L{x + 4} 70 Z", "url(#m)", sw=3)
    body += f'<path d="M{50 - r * 0.45} {52 - r * 0.4} Q{50 - r * 0.1} {52 - r * 0.7} {50 + r * 0.25} {52 - r * 0.55}" fill="none" stroke="#c9a070" stroke-width="3" stroke-linecap="round" stroke-opacity="0.8"/>'
    if stage == 4:
        for fx, fy in [(20, 18), (80, 14), (86, 40)]:
            body += (f'<ellipse cx="{fx - 3}" cy="{fy - 4}" rx="4" ry="2.6" fill="#dde8f0" fill-opacity="0.8" stroke="{O}" stroke-width="1.4" transform="rotate(-25 {fx - 3} {fy - 4})"/>'
                     f'<ellipse cx="{fx + 3}" cy="{fy - 4}" rx="4" ry="2.6" fill="#dde8f0" fill-opacity="0.8" stroke="{O}" stroke-width="1.4" transform="rotate(25 {fx + 3} {fy - 4})"/>'
                     f'<circle cx="{fx}" cy="{fy}" r="3.4" fill="#20242c"/>')
    return svg(body, defs)


for i, n in enumerate(["dirt_clean", "dirt_light", "dirt_slight", "dirt_quite", "dirt_filthy"]):
    icon(n)(lambda i=i: dirt(i))

# ---- warmth: flame -> thermometer -> snowflake -> ice -------------------------------------------------------


def thermometer(level, colour):
    tube = "M42 14 A8 8 0 0 1 58 14 L58 58 A17 17 0 1 1 42 58 Z"
    defs = f'<clipPath id="t"><rect x="0" y="{74 - 62 * level}" width="100" height="100"/></clipPath>'
    body = shape(tube, "#e8eef4")
    body += f'<rect x="46" y="14" width="8" height="60" rx="4" fill="{colour}" clip-path="url(#t)"/>'
    body += f'<circle cx="50" cy="72" r="11" fill="{colour}"/>'
    body += shape(tube, "none")
    for y in (26, 36, 46):
        body += f'<line x1="58" y1="{y}" x2="66" y2="{y}" stroke="{O}" stroke-width="3" stroke-linecap="round"/>'
    return body, defs


def snowflake(cx, cy, r, colour="#bfe6ff", sw=5.5):
    body = ""
    for k in range(6):
        a = math.radians(90 + 60 * k)
        ex, ey = cx + r * math.cos(a), cy - r * math.sin(a)
        mx, my = cx + r * 0.58 * math.cos(a), cy - r * 0.58 * math.sin(a)
        b = r * 0.3
        segs = f"M{cx} {cy} L{ex:.1f} {ey:.1f} "
        for side in (-1, 1):
            ba = a + side * math.radians(42)
            segs += f"M{mx:.1f} {my:.1f} L{mx + b * math.cos(ba):.1f} {my - b * math.sin(ba):.1f} "
        body += f'<path d="{segs}" stroke="{O}" stroke-width="{sw + 3.6}" stroke-linecap="round" fill="none"/>'
        body += f'<path d="{segs}" stroke="{colour}" stroke-width="{sw}" stroke-linecap="round" fill="none"/>'
    return body + f'<circle cx="{cx}" cy="{cy}" r="{r * 0.16}" fill="{colour}" stroke="{O}" stroke-width="2.4"/>'


def warmth(stage):
    if stage == 0:  # warm
        defs = lin("f", "#ffe070", "#ff5a20")
        d = "M50 6 C56 22 76 34 76 60 A26 26 0 0 1 24 60 C24 44 34 36 38 24 C42 34 46 36 48 40 C52 30 52 18 50 6 Z"
        di = "M50 46 C54 54 64 60 64 70 A14 14 0 0 1 36 70 C36 62 42 58 44 52 C46 56 48 58 50 60 C51 56 51 50 50 46 Z"
        return svg(shape(d, "url(#f)") + f'<path d="{di}" fill="#fff2b0"/>', defs)
    if stage == 1:  # comfortable
        body, defs = thermometer(0.62, "#ff6a3a")
        return svg(body, defs)
    if stage == 2:  # chilly
        body, defs = thermometer(0.3, "#4aa8ff")
        return svg(body + snowflake(78, 26, 15, sw=4), defs)
    if stage == 3:  # very cold
        return svg(snowflake(50, 50, 40))
    if stage == 4:  # freezing
        body = snowflake(50, 46, 36, colour="#d8f2ff")
        for x, h in ((26, 14), (40, 20), (56, 12), (72, 18)):
            body += shape(f"M{x - 5} 84 L{x + 5} 84 L{x} {84 + h} Z", "#bfe6ff", sw=3)
        body += shape("M14 80 L86 80 L86 88 L14 88 Z", "#e6f6ff", sw=3.4)
        return svg(body)
    # numb: a frozen block
    defs = lin("i", "#e8f8ff", "#7cc4f0")
    body = shape("M16 30 L50 14 L84 30 L84 70 L50 88 L16 70 Z", "url(#i)")
    body += f'<path d="M16 30 L50 46 L84 30 M50 46 L50 88" stroke="{O}" stroke-width="3" fill="none" stroke-linejoin="round"/>'
    body += snowflake(50, 56, 16, colour="#ffffff", sw=3.4)
    body += shine("M24 38 L24 56", 0.7)
    return svg(body, defs)


for i, n in enumerate(["warm_warm", "warm_comfortable", "warm_chilly", "warm_verycold", "warm_freezing", "warm_numb"]):
    icon(n)(lambda i=i: warmth(i))

# ---- hunger: a drumstick eaten down to the bone ---------------------------------------------------------------


def bone(x1, y1, x2, y2):
    knob = 7.5
    ux, uy = (x2 - x1), (y2 - y1)
    L = math.hypot(ux, uy)
    nx, ny = -uy / L, ux / L
    body = ""
    for x, y in ((x1, y1), (x2, y2)):
        for s in (-1, 1):
            body += f'<circle cx="{x + nx * knob * 0.75 * s:.1f}" cy="{y + ny * knob * 0.75 * s:.1f}" r="{knob}" fill="#f4ecd8" stroke="{O}" stroke-width="{SW}"/>'
    body += f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{O}" stroke-width="{12 + SW * 2}" stroke-linecap="round"/>'
    body += f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="#f4ecd8" stroke-width="12" stroke-linecap="round"/>'
    for x, y in ((x1, y1), (x2, y2)):
        for s in (-1, 1):
            body += f'<circle cx="{x + nx * knob * 0.75 * s:.1f}" cy="{y + ny * knob * 0.75 * s:.1f}" r="{knob - 2.1}" fill="#f4ecd8"/>'
    return body


def hunger(stage):
    defs = rad("d", "#e8a050", "#9a4e1c", 0.35, 0.3, 0.8)
    if stage >= 4:  # starving / ravenous: bare bone
        body = bone(22, 78, 78, 22)
        if stage == 5:
            body += f'<path d="M44 44 L52 50 L47 55 L56 62" stroke="{O}" stroke-width="3" fill="none" stroke-linejoin="round"/>'
            for x1, y1, x2, y2 in [(12, 30, 22, 36), (30, 10, 34, 20), (70, 90, 66, 80), (90, 66, 80, 62)]:
                body += f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="#ff6a4c" stroke-width="4" stroke-linecap="round"/>'
        return svg(body, defs)
    body = bone(22, 82, 50, 54)
    meat = "M42 62 C30 52 30 30 44 18 C58 6 82 10 88 26 C94 42 82 62 62 66 C54 68 48 67 42 62 Z"
    defs += f'<clipPath id="b"><path d="M0 0 H100 V100 H0 Z {bites(stage)}" clip-rule="evenodd"/></clipPath>'
    body += f'<g clip-path="url(#b)">{shape(meat, "url(#d)")}</g>'
    if stage >= 2:
        defs += f'<clipPath id="mc"><path d="{meat}"/></clipPath>'
        rings = "".join(f'<circle cx="{x}" cy="{y}" r="{r}" fill="none" stroke="{O}" stroke-width="{SW * 2}"/>' for x, y, r in BITES[stage])
        body += f'<g clip-path="url(#mc)">{rings}</g>'
    body += shine("M50 26 Q58 17 70 18", 0.5)
    if stage == 0:
        body += sparkle(84, 74, 9)
    return svg(body, defs)


BITES = {2: [(84, 24, 11)], 3: [(84, 24, 13), (80, 46, 13), (66, 16, 10)]}


def bites(stage):
    return " ".join(f"M{x - r} {y} A{r} {r} 0 1 0 {x + r} {y} A{r} {r} 0 1 0 {x - r} {y} Z" for x, y, r in BITES.get(stage, []))


def bite_outline(stage):
    out = ""
    for x, y, r in BITES.get(stage, []):
        out += f"M{x - r * 0.95} {y + r * 0.3} A{r} {r} 0 0 0 {x + r * 0.3} {y + r * 0.95} "
    return out


for i, n in enumerate(["food_wellfed", "food_satisfied", "food_peckish", "food_hungry", "food_starving", "food_ravenous"]):
    icon(n)(lambda i=i: hunger(i))

# ---- fatigue: sun -> setting sun -> moon with more and more Z ------------------------------------------------


def zed(x, y, s, opacity=1.0):
    d = f"M{x} {y} L{x + s} {y} L{x} {y + s} L{x + s} {y + s}"
    return (f'<path d="{d}" stroke="{O}" stroke-width="{s * 0.34 + 3.4}" fill="none" stroke-linejoin="round" stroke-linecap="round" opacity="{opacity}"/>'
            f'<path d="{d}" stroke="#e8e4ff" stroke-width="{s * 0.34}" fill="none" stroke-linejoin="round" stroke-linecap="round" opacity="{opacity}"/>')


def fatigue(stage):
    if stage <= 1:
        defs = rad("s", "#fff6a8", "#ffb020")
        cx, cy, r = (50, 50, 20) if stage == 0 else (50, 64, 20)
        body = ""
        for k in range(8):
            a = math.radians(45 * k)
            x1, y1 = cx + (r + 7) * math.cos(a), cy + (r + 7) * math.sin(a)
            x2, y2 = cx + (r + 17) * math.cos(a), cy + (r + 17) * math.sin(a)
            if stage == 1 and y1 > cy + 2:
                continue
            body += f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{O}" stroke-width="10" stroke-linecap="round"/>'
            body += f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="#ffc838" stroke-width="5" stroke-linecap="round"/>'
        if stage == 1:
            defs += '<clipPath id="h"><rect x="0" y="0" width="100" height="66"/></clipPath>'
            body += f'<g clip-path="url(#h)"><circle cx="{cx}" cy="{cy}" r="{r}" fill="url(#s)" stroke="{O}" stroke-width="{SW}"/></g>'
            body += f'<line x1="10" y1="66" x2="90" y2="66" stroke="{O}" stroke-width="9" stroke-linecap="round"/><line x1="10" y1="66" x2="90" y2="66" stroke="#ffd870" stroke-width="4" stroke-linecap="round"/>'
        else:
            body += f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="url(#s)" stroke="{O}" stroke-width="{SW}"/>'
        return svg(body, defs)
    defs = rad("n", "#f4f0ff", "#9c92e8", 0.3, 0.3, 0.9)
    moon = "M58 12 A38 38 0 1 0 88 66 A30 30 0 1 1 58 12 Z"
    if stage == 5:
        defs = rad("n", "#c8c0e0", "#6a6290", 0.3, 0.3, 0.9)
    body = f'<g transform="translate(-10 6)">{shape(moon, "url(#n)")}</g>'
    zs = {2: [], 3: [(70, 18, 14)], 4: [(62, 30, 11), (76, 12, 15)], 5: [(58, 36, 9), (70, 22, 12), (82, 6, 15)]}[stage]
    for x, y, s in zs:
        body += zed(x, y, s)
    return svg(body, defs)


for i, n in enumerate(["sleep_wellrested", "sleep_rested", "sleep_slightly", "sleep_tired", "sleep_weary", "sleep_debilitated"]):
    icon(n)(lambda i=i: fatigue(i))

# ---- cycle (Beeing Female): a woman in profile, the belly grows by trimester ---------------------------------


def woman(belly):
    """Bust-to-thigh profile facing right; belly 0 (flat) .. 1 (term)."""
    bx = 62 + 22 * belly           # belly's front
    by = 66 + 2 * belly            # its lowest-forward point
    d = ("M45 31 "                                   # back of the neck
         "C41 35 37 38 37 44 "                       # shoulder
         "C37 52 41 57 40 63 "                       # back
         "C39 68 33 72 33 80 "                       # buttock
         "C33 88 37 93 38 99 "                       # back of the thigh
         "L60 99 "
         f"C60 94 61 90 {58 + 4 * belly:.1f} 86 "    # front of the thigh
         f"C{bx - 2:.1f} {by + 14:.1f} {bx:.1f} {by + 4:.1f} {bx:.1f} {by - 4:.1f} "
         f"C{bx:.1f} {by - 14 - 4 * belly:.1f} {62 + 6 * belly:.1f} 56 60 54 "   # under the breast
         "C68 52 72 47 70 42 "                       # breast
         "C68 37 61 36 57 36 "                       # upper chest
         "C55 35 54 33 54 30 Z")                     # front of the neck
    head = "M52 4 C61 4 66 10 66 16 L69 20 L66 22 C66 27 62 31 55 31 C47 31 41 25 41 17 C41 9 46 4 52 4 Z"
    hair = "M41 18 C39 8 47 2 55 3 C62 4 66 8 66 12 C60 10 55 11 53 16 C52 22 50 27 46 30 C42 27 41 23 41 18 Z"
    bun = "M37 10 A7 7 0 1 0 44 4"
    return d, head, hair, bun


def cycle(name):
    belly = {"bf_preg1": 0.35, "bf_preg2": 0.68, "bf_preg3": 1.0, "bf_labor": 1.0, "bf_eggs": 0.8}.get(name, 0.0)
    body_d, head, hair, bun = woman(belly)
    skin = ("#f6c8dc", "#d880ae") if name != "bf_eggs" else ("#d8f0c0", "#7ab060")
    defs = lin("sk", skin[0], skin[1], 0, 0, 1, 1)
    body = ""
    if name == "bf_labor":
        for k in range(7):
            a = math.radians(-60 + 20 * k)
            cx, cy = 80, 68
            body += f'<line x1="{cx + 14 * math.cos(a):.1f}" y1="{cy + 14 * math.sin(a):.1f}" x2="{cx + 22 * math.cos(a):.1f}" y2="{cy + 22 * math.sin(a):.1f}" stroke="#ff5a5a" stroke-width="4" stroke-linecap="round"/>'
    body += shape(body_d, "url(#sk)")
    body += f'<circle cx="35" cy="12" r="7" fill="#5a3048" stroke="{O}" stroke-width="3.4"/>'
    body += shape(head, "url(#sk)", sw=3.6)
    body += shape(hair, "#5a3048", sw=3.0)
    if belly > 0:
        bx = 62 + 22 * belly
        body += f'<path d="M{bx - 8:.1f} {60:.1f} Q{bx - 2:.1f} {64:.1f} {bx - 3:.1f} {72:.1f}" fill="none" stroke="#ffffff" stroke-opacity="0.6" stroke-width="3" stroke-linecap="round"/>'
    if name == "bf_ovulation":
        body += (f'<circle cx="84" cy="22" r="11" fill="#ffe08a" stroke="{O}" stroke-width="3.4"/>'
                 f'<circle cx="84" cy="22" r="5" fill="#fff8e0"/>')
        for k in range(8):
            a = math.radians(45 * k)
            body += f'<line x1="{84 + 14 * math.cos(a):.1f}" y1="{22 + 14 * math.sin(a):.1f}" x2="{84 + 18 * math.cos(a):.1f}" y2="{22 + 18 * math.sin(a):.1f}" stroke="#ffd040" stroke-width="2.6" stroke-linecap="round"/>'
    elif name == "bf_menstruation":
        body += shape(small_drop(86, 26, 1.15), "#e0303c", sw=3.2)
    elif name == "bf_recovery":
        body += shape("M86 34 C78 28 74 24 74 20 C74 16 77 14 80 14 C82 14 85 15 86 18 C87 15 90 14 92 14 C95 14 98 16 98 20 C98 24 94 28 86 34 Z", "#ff7aa8", sw=3)
    elif name == "bf_follicular":
        body += f'<circle cx="86" cy="22" r="7" fill="#f8f0ff" stroke="{O}" stroke-width="3"/>'
    elif name == "bf_luteal":
        body += f'<circle cx="86" cy="22" r="7" fill="#e8c860" stroke="{O}" stroke-width="3"/>'
    elif name == "bf_eggs":
        for ex, ey in ((70, 66), (78, 72), (72, 76)):
            body += f'<ellipse cx="{ex}" cy="{ey}" rx="4.4" ry="3.6" fill="#c8f070" stroke="{O}" stroke-width="2"/>'
    return svg(body, defs)


for n in ["bf_follicular", "bf_ovulation", "bf_luteal", "bf_menstruation", "bf_preg1", "bf_preg2", "bf_preg3", "bf_labor", "bf_recovery", "bf_eggs"]:
    icon(n)(lambda n=n: cycle(n))

# ---- craving (SL Survival): lips, then a drip, then a hungry open mouth ---------------------------------------

UPPER = "M10 50 C20 40 30 32 40 34 C45 35 48 38 50 40 C52 38 55 35 60 34 C70 32 80 40 90 50 C76 52 62 52 50 52 C38 52 24 52 10 50 Z"
LOWER = "M10 50 C24 52 38 52 50 52 C62 52 76 52 90 50 C82 64 68 72 50 72 C32 72 18 64 10 50 Z"


def craving(stage):
    defs = lin("l", "#ff8aa8", "#c02c5a")
    body = ""
    if stage >= 3:
        mouth = "M18 50 C30 48 40 48 50 48 C60 48 70 48 82 50 C76 62 64 66 50 66 C36 66 24 62 18 50 Z"
        body += shape("M10 46 C20 36 30 28 40 30 C45 31 48 34 50 36 C52 34 55 31 60 30 C70 28 80 36 90 46 C76 46 62 44 50 44 C38 44 24 46 10 46 Z", "url(#l)")
        body += shape(mouth, "#3a0c1c")
        body += f'<path d="M34 60 C40 54 60 54 66 60 C60 64 40 64 34 60 Z" fill="#ff7a9a"/>'
        body += shape("M10 46 C18 66 32 78 50 78 C68 78 82 66 90 46 C82 58 68 66 50 66 C32 66 18 58 10 46 Z", "url(#l)")
    elif stage == 2:
        body += shape(UPPER, "url(#l)")
        body += shape(LOWER.replace("C82 64 68 72 50 72 C32 72 18 64 10 50", "C82 66 68 76 50 76 C32 76 18 66 10 50"), "url(#l)")
        body += f'<path d="M24 52 C36 56 64 56 76 52" stroke="#3a0c1c" stroke-width="4" fill="none" stroke-linecap="round"/>'
    else:
        body += shape(UPPER, "url(#l)") + shape(LOWER, "url(#l)")
    body += shine("M30 62 Q40 67 50 67", 0.5) if stage < 3 else ""
    drips = {0: [], 1: [(58, 78, 0.65)], 2: [(60, 82, 0.8)], 3: [(42, 86, 0.75), (62, 90, 0.9)], 4: [(38, 88, 0.8), (58, 92, 0.95), (74, 84, 0.6)]}[stage]
    for x, y, s in drips:
        body += shape(small_drop(x, y, s), "#f6f4ea", sw=2.8)
    if stage == 4:
        for x1, y1, x2, y2 in [(6, 24, 14, 30), (94, 24, 86, 30), (50, 8, 50, 18)]:
            body += f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="#ff6a8a" stroke-width="4" stroke-linecap="round"/>'
    return svg(body, defs)


for i, n in enumerate(["crave_satisfied", "crave_peckish", "crave_hungry", "crave_starving", "crave_ravenous"]):
    icon(n)(lambda i=i: craving(i))


def main():
    os.makedirs(OUT, exist_ok=True)
    os.makedirs(os.path.dirname(SHEET), exist_ok=True)
    tiles = []
    for name, fn in ICONS.items():
        source = fn()
        png = resvg_py.svg_to_bytes(svg_string=source, width=SIZE, height=SIZE)
        with open(os.path.join(OUT, name + ".png"), "wb") as f:
            f.write(bytes(png))
        big = resvg_py.svg_to_bytes(svg_string=source, width=128, height=128)
        tiles.append((name, bytes(big), bytes(png)))
    # preview sheet: 128 px and the shipped 64 px shown at the HUD's ~30 px, on a mid-grey game-like background
    import io
    cols = 6
    cell_w, cell_h = 200, 170
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new("RGBA", (cols * cell_w, rows * cell_h), (58, 62, 70, 255))
    draw = ImageDraw.Draw(sheet)
    for i, (name, big, small) in enumerate(tiles):
        x, y = (i % cols) * cell_w, (i // cols) * cell_h
        img = Image.open(io.BytesIO(big)).convert("RGBA")
        sheet.alpha_composite(img, (x + 8, y + 8))
        hud = Image.open(io.BytesIO(small)).convert("RGBA").resize((30, 30), Image.BILINEAR)
        sheet.alpha_composite(hud, (x + 150, y + 60))
        draw.text((x + 8, y + 145), name, fill=(235, 235, 235, 255))
    sheet.save(SHEET)
    print(f"{len(tiles)} icons -> {OUT}\npreview -> {SHEET}")


if __name__ == "__main__":
    main()
