"""Cut the ChatGPT sprite sheets (docs/icon_prompts.md) into the stage icons the DLL loads.

    python tools/slice_sheets.py      assets/sheets/sheet_*.png -> assets/Interface/VitalsHUD/<icon>.png

Each sheet is a 3 x 2 grid read left to right, top row first. Magenta (#FF00FF) backgrounds are keyed out
when the sheet has no transparency. Each icon is trimmed to its pixels, centred on a square with a small
margin and downscaled to SIZE px. A sheet that is missing is skipped (its old icons stay).
"""
import os

import numpy as np
from PIL import Image
from scipy import ndimage

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHEETS = os.path.join(ROOT, "assets", "sheets")
OUT = os.path.join(ROOT, "assets", "Interface", "VitalsHUD")
PREVIEW = os.path.join(ROOT, "build", "sliced_sheet.png")
SIZE = 64
MARGIN = 0.06

LAYOUT = {
    "sheet_wetness": ["wet_dry", "wet_damp", "wet_sweaty", "wet_soaked"],
    "sheet_arousal": ["aro_calm", "aro_interested", "aro_aroused", "aro_lustful", "aro_desperate"],
    "sheet_dirt": ["dirt_clean", "dirt_light", "dirt_slight", "dirt_quite", "dirt_filthy"],
    "sheet_warmth": ["warm_warm", "warm_comfortable", "warm_chilly", "warm_verycold", "warm_freezing", "warm_numb"],
    "sheet_hunger": ["food_wellfed", "food_satisfied", "food_peckish", "food_hungry", "food_starving", "food_ravenous"],
    "sheet_fatigue": ["sleep_wellrested", "sleep_rested", "sleep_slightly", "sleep_tired", "sleep_weary", "sleep_debilitated"],
    "sheet_cycle_a": ["bf_follicular", "bf_ovulation", "bf_luteal", "bf_menstruation", "bf_recovery"],
    "sheet_cycle_b": ["bf_preg1", "bf_preg2", "bf_preg3", "bf_labor", "bf_eggs"],
    "sheet_craving": ["crave_satisfied", "crave_peckish", "crave_hungry", "crave_starving", "crave_ravenous"],
    # licences v2 (big symbol + wax seal); the v1 scroll licences in sheet_sls_a / the top row of sheet_sls_b are
    # used only while the v2 sheets are missing
    "sheet_sls_a": ["lic_magic", "lic_weapon", "lic_armor", "lic_bikini", "lic_clothes", "lic_curfew"],
    "sheet_sls_b": ["lic_whore", "lic_freedom", "lic_property", "toll_paid", "toll_unpaid", "sls_grounded"],
    "sheet_sls_lic_a": ["lic_magic", "lic_weapon", "lic_armor", "lic_bikini", "lic_clothes", "lic_curfew"],
    "sheet_sls_lic_b": ["lic_whore", "lic_freedom", "lic_property"],
    "sheet_sls_c": ["curfew_hours", "curfew_on", "curfew_breaking", "curse_bikini", "curse_magic"],
    "sheet_sls_d": ["ofat_1", "ofat_2", "ofat_3"],
}


def key_magenta(img):
    """Alpha from the distance to #FF00FF, with the magenta spill removed from the soft edge."""
    px = img.load()
    w, h = img.size
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            # magenta has high red and blue, low green; how far green sits below them is how magenta it is
            m = min(r, b) - g
            if m > 60:
                k = min(1.0, (m - 60) / 120.0)
                alpha = int(a * (1.0 - k))
                # remove the spill: pull red and blue down towards green
                r = int(r - (r - g) * k)
                b = int(b - (b - g) * k)
                px[x, y] = (r, g, b, alpha)
    return img


def has_alpha(img):
    return img.getextrema()[3][0] < 250


def drop_specks(cell):
    """Remove stray specks (tiny isolated shapes far smaller than any real droplet or symbol)."""
    a = np.array(cell)
    mask = a[:, :, 3] > 16
    labels, count = ndimage.label(mask, structure=np.ones((3, 3)))
    if count < 2:
        return cell
    sizes = ndimage.sum(mask, labels, range(1, count + 1))
    peaks = ndimage.maximum(a[:, :, 3], labels, range(1, count + 1))
    limit = 0.0018 * mask.size
    for i, (size, peak) in enumerate(zip(sizes, peaks), start=1):
        # tiny bits, or loose ghosts with no solid pixel (stray soft shadows under the icon)
        if size < limit or peak < 170:
            a[labels == i] = 0
    return Image.fromarray(a)


def tidy_final(icon, dark_shadows):
    """After downscaling: crumbs that only separate at 64 px, and (craving sheet) the dark soft shadow drawn
    under the lips. The largest shape always stays."""
    a = np.array(icon)
    mask = a[:, :, 3] > 16
    labels, count = ndimage.label(mask, structure=np.ones((3, 3)))
    if count < 2:
        return icon
    idx = range(1, count + 1)
    sizes = ndimage.sum(mask, labels, idx)
    lum = ndimage.mean(a[:, :, :3].mean(axis=2), labels, idx)
    keep = int(np.argmax(sizes)) + 1
    for i, size, l in zip(idx, sizes, lum):
        if i != keep and (size < 0.012 * mask.size or (dark_shadows and l < 60)):
            a[labels == i] = 0
    return Image.fromarray(a)


def square_icon(cell):
    cell = drop_specks(cell)
    box = cell.getchannel("A").point(lambda v: 255 if v > 16 else 0).getbbox()
    if not box:
        return None
    icon = cell.crop(box)
    side = int(max(icon.size) * (1.0 + 2 * MARGIN))
    canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    canvas.alpha_composite(icon, ((side - icon.width) // 2, (side - icon.height) // 2))
    return canvas.resize((SIZE, SIZE), Image.LANCZOS)


def split_figure(cell):
    """Beeing Female cells: (figure, symbols above it, everything else) as separate RGBA layers.
    The figure is the largest connected shape; symbols are the small shapes in the top 40% of the cell."""
    a = np.array(cell)
    mask = a[:, :, 3] > 16
    labels, count = ndimage.label(mask, structure=np.ones((3, 3)))
    if count == 0:
        return None
    sizes = ndimage.sum(mask, labels, range(1, count + 1))
    figure_id = int(np.argmax(sizes)) + 1
    top = np.zeros_like(mask)
    rest = np.zeros_like(mask)
    for i, sl in enumerate(ndimage.find_objects(labels), start=1):
        if i == figure_id or sl is None:
            continue
        cy = (sl[0].start + sl[0].stop) / 2
        (top if cy < 0.4 * mask.shape[0] else rest)[labels == i] = True
    def layer(m):
        out = a.copy()
        out[~m] = 0
        return Image.fromarray(out)
    return layer(labels == figure_id), layer(top), layer(rest)


def cycle_phase(cell):
    """Phases differ only by their symbol: shrink the woman, make the symbol big enough to read at HUD size.
    The symbol floats at the upper right: everything in the top band right of the first empty column gap after
    the head (opaque pixels only, a glow may bridge the gap)."""
    a = np.array(cell)
    h, w = a.shape[:2]
    band = int(h * 0.42)
    solid = (a[:band, :, 3] > 140).any(axis=0)
    cols = np.nonzero(solid)[0]
    split = None
    started = False
    for x in range(cols.min(), w):
        if solid[x]:
            started = True
        elif started and x > w * 0.45:
            split = x
            break
    if split is None:
        return cell
    sym_a = a.copy()
    sym_a[band:, :] = 0
    sym_a[:, :split] = 0
    fig_a = a.copy()
    fig_a[:band, split:] = 0
    figure, symbol = Image.fromarray(fig_a), Image.fromarray(sym_a)
    fbox, sbox = figure.getbbox(), symbol.getbbox()
    if not sbox:
        return cell
    fig = figure.crop(fbox)
    sym = symbol.crop(sbox)
    side = max(cell.size)
    canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    fh = int(side * 0.92)
    fig = fig.resize((int(fig.width * fh / fig.height), fh), Image.LANCZOS)
    canvas.alpha_composite(fig, (int(side * 0.04), side - fh))
    k = side * 0.5 / max(sym.size)
    sym = sym.resize((max(1, int(sym.width * k)), max(1, int(sym.height * k))), Image.LANCZOS)
    canvas.alpha_composite(sym, (side - sym.width - int(side * 0.02), int(side * 0.04)))
    return canvas


def pregnancy(cell):
    """ChatGPT carried the phase symbols over into the pregnancy sheet: keep the figure and the low shapes
    (labour dashes), drop the symbols floating above."""
    figure, _, rest = split_figure(cell)
    figure.alpha_composite(rest)
    return figure


def licence(cell):
    """Licences are nine identical scrolls at HUD size (18 px): zoom into the paper so the symbol is what reads,
    the cream paper and a bit of the wax seal still say "licence"."""
    box = cell.getchannel("A").point(lambda v: 255 if v > 140 else 0).getbbox()
    if not box:
        return cell
    x0, y0, x1, y1 = box
    w, h = x1 - x0, y1 - y0
    inner = cell.crop((int(x0 + w * 0.14), int(y0 + h * 0.16), int(x0 + w * 0.92), int(y0 + h * 0.86)))
    side = max(inner.size)
    canvas = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    canvas.alpha_composite(inner, ((side - inner.width) // 2, (side - inner.height) // 2))
    return canvas


FIX = {"sheet_cycle_a": cycle_phase, "sheet_cycle_b": pregnancy}
CELL_FIX = {("sheet_sls_a", i): licence for i in range(6)} | {("sheet_sls_b", i): licence for i in range(3)}


def main():
    os.makedirs(OUT, exist_ok=True)
    done = []
    for sheet, names in LAYOUT.items():
        path = os.path.join(SHEETS, sheet + ".png")
        if not os.path.exists(path):
            print(f"skip {sheet}: no {path}")
            continue
        img = Image.open(path).convert("RGBA")
        if not has_alpha(img):
            img = key_magenta(img)
        cw, ch = img.width / 3, img.height / 2
        for i, name in enumerate(names):
            col, row = i % 3, i // 3
            cell = img.crop((int(col * cw), int(row * ch), int((col + 1) * cw), int((row + 1) * ch)))
            if sheet in FIX:
                cell = FIX[sheet](cell)
            if (sheet, i) in CELL_FIX:
                cell = CELL_FIX[(sheet, i)](cell)
            icon = square_icon(cell)
            if icon is None:
                raise SystemExit(f"{sheet}: cell {i + 1} ({name}) is empty")
            icon = tidy_final(icon, sheet == "sheet_craving")
            icon.save(os.path.join(OUT, name + ".png"))
            done.append((name, icon))
        print(f"{sheet}: {len(names)} icons")
    if done:
        cols = 8
        rows = (len(done) + cols - 1) // cols
        sheet = Image.new("RGBA", (cols * 80, rows * 80), (58, 62, 70, 255))
        for i, (_, icon) in enumerate(done):
            small = icon.resize((32, 32), Image.BILINEAR)
            sheet.alpha_composite(icon, ((i % cols) * 80 + 2, (i // cols) * 80 + 2))
            sheet.alpha_composite(small, ((i % cols) * 80 + 46, (i // cols) * 80 + 40))
        os.makedirs(os.path.dirname(PREVIEW), exist_ok=True)
        sheet.save(PREVIEW)
        print(f"{len(done)} icons written, preview {PREVIEW}")


if __name__ == "__main__":
    main()
