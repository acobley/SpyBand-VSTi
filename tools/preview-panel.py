#!/usr/bin/env python3
"""Render the editor panel and look at it.

Nothing about the layout is placed by eye: every position below is the
DIALOG UNITS from SpyBand.rc, converted by the same two factors the editor
uses, and every colour and offset is from SlideSpin::PaintBk,
PatchBoard::PaintBk and DrawArea::PaintBk. What this checks is that the
result is a panel and not a heap - that nothing lands on top of anything
else, that the labels fit their controls, and that the new Output Trim sits
where the eye expects it.

    python3 tools/preview-panel.py [out.png]

Writes docs/panel-preview.png by default.
"""

import os
import sys

from PIL import Image, ImageDraw, ImageFont

X_SCALE = 1.5
Y_SCALE = 1.625
Y_ORIGIN_DLU = 1

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# SlideSpin::PaintBk, DrawArea::PaintBk, PatchBoard::PaintBk
BAR_LIGHT = (200, 200, 200)
BAR_HIGH = (255, 255, 255)
BAR_FILL = (100, 100, 100)
LABEL = (50, 255, 50)
VALUE = (192, 50, 50)
LAMP_ON = (255, 0, 0)
LAMP_OFF = (0, 0, 0)
LAMP_FRAME = (100, 100, 100)
GRID = (200, 200, 200)
GRID_BORDER = (100, 255, 100)
OUTER = (100, 100, 100)
PIN = (255, 0, 0)
TRACE = (127, 200, 255)

BAR_BOTTOM_INSET = 3
BAR_HEIGHT = 12
LABEL_TOP = 17
LABEL_BOTTOM = 6
LAMP = 10


def rect(x, y, w, h):
    """Dialog units -> a pixel box, artwork-relative."""
    px = x * X_SCALE
    py = (y - Y_ORIGIN_DLU) * Y_SCALE
    return [px, py, px + w * X_SCALE, py + h * Y_SCALE]


def font(size=11):
    """Liberation Sans is metrically compatible with the Arial the DXi
    asked for, so a string that fits here fits there."""
    for path in ("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
                 "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


FONTS = [font(11), font(10), font(9)]


def draw3d(d, box, top_left, bottom_right):
    l, t, r, b = [round(v) for v in box]
    if r <= l or b <= t:
        return
    d.line([(l, t), (r - 1, t)], fill=top_left)
    d.line([(l, t), (l, b - 1)], fill=top_left)
    d.line([(l, b - 1), (r - 1, b - 1)], fill=bottom_right)
    d.line([(r - 1, t), (r - 1, b - 1)], fill=bottom_right)


def fitted(d, text, box, colour):
    """Centred horizontally at the TOP of the box, dropping a size rather
    than running past the edges.

    Windows drew both the label and the value with DT_CENTER and no
    DT_VCENTER, so each sat at the top of the rect it was given. Centring
    them vertically instead puts the red value straight through the green
    label, which is what the first render of this panel showed."""
    if not text:
        return
    l, t, r, b = box
    chosen = FONTS[-1]
    for f in FONTS:
        if d.textlength(text, font=f) <= (r - l):
            chosen = f
            break
    tw = d.textlength(text, font=chosen)
    d.text((l + (r - l - tw) / 2, t), text, font=chosen, fill=colour)


def bar(d, box, fraction, fill=True):
    l, t, r, b = box
    bar_box = [l, b - BAR_BOTTOM_INSET - BAR_HEIGHT,
               l + (r - l) * max(0.0, min(1.0, fraction)), b - BAR_BOTTOM_INSET]
    draw3d(d, bar_box, BAR_LIGHT, BAR_HIGH)
    if fill and bar_box[2] - bar_box[0] > 2:
        d.rectangle([bar_box[0] + 1, bar_box[1] + 1, bar_box[2] - 2, bar_box[3] - 2],
                    fill=BAR_FILL)


def label_band(box):
    l, t, r, b = box
    return [l, b - LABEL_TOP, r, b]


def lamp(d, box, on):
    l, t = box[0], box[1]
    draw3d(d, [l, t, l + LAMP, t + LAMP], LAMP_FRAME, LAMP_FRAME)
    d.rectangle([l + 2, t + 2, l + LAMP - 3, t + LAMP - 3],
                fill=LAMP_ON if on else LAMP_OFF)


def slider(d, geometry, text, value, fraction, indicator=None):
    box = rect(*geometry)
    bar(d, box, fraction)
    fitted(d, text, label_band(box), LABEL)
    if value:
        fitted(d, value, box, VALUE)
    if indicator is not None:
        lamp(d, box, indicator)


def toggle(d, geometry, name, on, indicator=None):
    box = rect(*geometry)
    bar(d, box, 1.0 if on else 0.0)
    fitted(d, name, label_band(box), LABEL)
    if indicator is not None:
        lamp(d, box, indicator)


def selector(d, geometry, name):
    box = rect(*geometry)
    draw3d(d, [box[0], box[1], box[2], box[3] - BAR_BOTTOM_INSET], BAR_LIGHT, BAR_HIGH)
    fitted(d, name, box, VALUE)


def file_button(d, geometry, name, on):
    box = rect(*geometry)
    fitted(d, name, label_band(box), LABEL)
    lamp(d, box, on)


LED_GREEN = (50, 255, 50)
LED_AMBER = (255, 190, 40)
LED_RED = (255, 0, 0)
LED_OFF = (26, 26, 26)
LED_SEGMENTS = 20


def led_column(d, box, level, peak, label):
    """A segmented input meter, in PIXELS - these are new and have no
    dialog units to convert from."""
    l, t, r, b = box
    label_h = 13
    body = [l, t, r, b - label_h]
    draw3d(d, body, LAMP_FRAME, LAMP_FRAME)

    inner = [body[0] + 2, body[1] + 2, body[2] - 2, body[3] - 2]
    pitch = (inner[3] - inner[1]) / LED_SEGMENTS
    lit_to = int(level * LED_SEGMENTS + 0.5)
    peak_at = int(peak * LED_SEGMENTS + 0.5)

    for i in range(LED_SEGMENTS):
        top = inner[3] - (i + 1) * pitch
        cell = [inner[0], top + 1, inner[2], top + pitch - 1]
        if cell[3] <= cell[1]:
            continue
        if i >= LED_SEGMENTS - 2:
            colour = LED_RED
        elif i >= LED_SEGMENTS - 5:
            colour = LED_AMBER
        else:
            colour = LED_GREEN
        lit = i < lit_to
        is_peak = peak_at > 0 and i == peak_at - 1
        if not lit and not is_peak:
            colour = LED_OFF
        elif not lit and is_peak:
            colour = tuple(int(c * 0.66) for c in colour)
        d.rectangle(cell, fill=colour)

    fitted(d, label, [l, b - label_h, r, b], LABEL)


def meter(d, geometry, values, stereo=False):
    box = rect(*geometry)
    l, t, r, b = box
    if stereo:
        d.line([((l + r) / 2, t), ((l + r) / 2, b)], fill=GRID)
    draw3d(d, box, OUTER, OUTER)
    n = len(values)
    previous = None
    for i, v in enumerate(values):
        x0 = l + (i / n) * (r - l)
        x1 = l + ((i + 1) / n) * (r - l)
        y = max(t, min(b, b - (b - t) * v))
        d.line([(x0, y), (x1, y)], fill=TRACE)
        if previous is not None:
            d.line([(x0, previous), (x0, y)], fill=TRACE)
        previous = y


def patchboard(d, geometry, bands, cells):
    box = rect(*geometry)
    l, t, r, b = box
    dimension = min(r - l, b - t)
    step = int(dimension // bands)
    border = int((dimension - step * bands) // 2)
    grid = [l + border, t + border, l + border + step * bands, t + border + step * bands]
    for i in range(1, bands + 1):
        d.line([(grid[0] + i * step, grid[1]), (grid[0] + i * step, grid[3])], fill=GRID)
        d.line([(grid[0], grid[1] + i * step), (grid[2], grid[1] + i * step)], fill=GRID)
    draw3d(d, grid, GRID_BORDER, GRID_BORDER)
    outside = [grid[0] - border, grid[1] - border, grid[2] + border, grid[3] + border]
    draw3d(d, outside, OUTER, OUTER)
    for row, column, value in cells:
        pin = [grid[0] + row * step + 1, grid[1] + column * step + 1,
               grid[0] + (row + 1) * step - 2, grid[1] + (column + 1) * step - 2]
        shade = int(max(0, min(255, 60 + 195 * value)))
        d.rectangle(pin, fill=(PIN[0], shade // 3, shade // 3))


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "docs", "panel-preview.png")

    background = os.path.join(ROOT, "resource", "background.png")
    image = Image.open(background).convert("RGB")
    d = ImageDraw.Draw(image)

    # ---- left column: the envelope, the levels, and the new trim
    slider(d, (12, 14, 46, 17), "Env  Release", "5", 0.05)
    slider(d, (12, 30, 46, 17), "Env  Attack", "5", 0.05)
    slider(d, (12, 47, 46, 17), "Env Level", "25", 0.25)
    slider(d, (12, 89, 46, 17), "R Wav Level", "20", 0.20)
    slider(d, (12, 106, 46, 17), "L+R Src Level", "20", 0.20)
    slider(d, (12, 122, 46, 17), "Through", "0", 0.0)
    slider(d, (12, 156, 46, 17), "Output Trim", "-20 dB", 40.0 / 60.0)

    # ---- middle column
    slider(d, (90, 13, 46, 17), "Bottom Freq", "27 Hz", 0.01)
    slider(d, (90, 30, 46, 17), "Top Freq", "52 Hz", 0.02)
    slider(d, (90, 47, 46, 17), "Resonance", "75", 0.75)
    toggle(d, (90, 89, 55, 17), "Voiced Det Off", False, indicator=False)
    slider(d, (90, 106, 55, 17), "Voiced Sensitivity", "20", 0.20)
    slider(d, (90, 123, 55, 17), "Unvoiced Noise Level", "20", 0.20)
    toggle(d, (90, 139, 55, 18), "Input Carrier", False)
    slider(d, (90, 156, 55, 17), "Noise HighPass", "110 Hz", 0.01)

    # ---- mode switches and the two selectors
    toggle(d, (186, 89, 55, 17), "Mono", False)
    toggle(d, (186, 106, 55, 17), "Use Sample", False)
    toggle(d, (186, 123, 55, 17), "Single Shot", False)
    selector(d, (186, 140, 55, 11), "9 Bands")
    selector(d, (186, 151, 55, 11), "Shallow Slope")

    # ---- the new input LED columns, in pixels
    led_column(d, [93, 143, 109, 300], 0.72, 0.80, "L")
    led_column(d, [115, 143, 131, 300], 0.44, 0.52, "R")

    # ---- displays
    meter(d, (171, 12, 111, 55),
          [0.15, 0.42, 0.66, 0.81, 0.55, 0.30, 0.47, 0.72, 0.22])
    patchboard(d, (257, 89, 106, 106 * 1.5 / 1.625), 9, [(i, i, 0.5) for i in range(9)])

    # ---- the five file slots and the version label
    for i, (y, h) in enumerate([(6, 14), (20, 17), (37, 17), (54, 17), (71, 17)]):
        file_button(d, (312, y, 50, h), "File %d" % (i + 1), False)

    box = rect(312, 190, 51, 17)
    fitted(d, "SpyBand  44.1 k", label_band(box), LABEL)

    os.makedirs(os.path.dirname(out), exist_ok=True)
    image.save(out)
    print("wrote %s  (%d x %d)" % (out, image.width, image.height))
    report_overlaps()


# Every control's box, by name, for the overlap check below. This is the
# same list the panel is drawn from; the point of checking it separately is
# that two controls can both be in the right place and still be on top of
# each other - which is how the version label spent 2004 hidden behind the
# patch board.
BOXES = [
    ("Env Release", (12, 14, 46, 17)),
    ("Env Attack", (12, 30, 46, 17)),
    ("Env Level", (12, 47, 46, 17)),
    ("Wav Level", (12, 89, 46, 17)),
    ("Src Level", (12, 106, 46, 17)),
    ("Through", (12, 122, 46, 17)),
    ("Output Trim", (12, 156, 46, 17)),
    ("Bottom Freq", (90, 13, 46, 17)),
    ("Top Freq", (90, 30, 46, 17)),
    ("Resonance", (90, 47, 46, 17)),
    ("Voiced Detect", (90, 89, 55, 17)),
    ("Voiced Sensitivity", (90, 106, 55, 17)),
    ("Noise Level", (90, 123, 55, 17)),
    ("Noise Override", (90, 139, 55, 18)),
    ("Noise HighPass", (90, 156, 55, 17)),
    ("Stereo", (186, 89, 55, 17)),
    ("Interlaced", (186, 106, 55, 17)),
    ("Repeat", (186, 123, 55, 17)),
    ("Bands", (186, 140, 55, 11)),
    ("Filter Slopes", (186, 151, 55, 11)),
    ("Band meter", (171, 12, 111, 55)),
    # the square the board actually draws, not the 106 x 106 DLU box:
    # 106 DLU is 159 px across and 172 down, and PaintBk took the smaller.
    ("Patch board", (257, 89, 106, 106 * 1.5 / 1.625)),
    ("File 1", (312, 6, 50, 14)),
    ("File 2", (312, 20, 50, 17)),
    ("File 3", (312, 37, 50, 17)),
    ("File 4", (312, 54, 50, 17)),
    ("File 5", (312, 71, 50, 17)),
    ("Version", (312, 190, 51, 17)),
]


# The resource editor left several neighbouring controls sharing one dialog
# unit of edge - 1.6 pixels - which is its own rounding and not a fault: a
# SlideSpin draws its value at the top and its label and bar at the bottom,
# so nothing of either is within 1.6 pixels of the boundary. Anything DEEPER
# than that is a real collision.
TOUCHING_PX = 2.0


# The LED columns are placed in pixels, so they go into the check as a
# pre-converted box rather than through rect().
PIXEL_BOXES = [
    ("Input LED L", [93, 143, 109, 300]),
    ("Input LED R", [115, 143, 131, 300]),
]


def report_overlaps():
    boxes = [(n, rect(*g)) for n, g in BOXES] + PIXEL_BOXES
    touching, colliding = [], []
    for i, (name_a, ra) in enumerate(boxes):
        for name_b, rb in boxes[i + 1:]:
            dx = min(ra[2], rb[2]) - max(ra[0], rb[0])
            dy = min(ra[3], rb[3]) - max(ra[1], rb[1])
            if dx <= 0 or dy <= 0:
                continue
            line = "%s and %s share %.1f x %.1f px" % (name_a, name_b, dx, dy)
            if min(dx, dy) <= TOUCHING_PX:
                touching.append(line)
            else:
                colliding.append(line)

    for line in touching:
        print("  touching (the .rc's own rounding): " + line)
    if colliding:
        print("COLLIDING CONTROLS:")
        for line in colliding:
            print("  " + line)
    else:
        print("no two controls collide")

    for name, r in boxes:
        if r[0] < 0 or r[1] < 0 or r[2] > 564 or r[3] > 353:
            print("  %s falls outside the panel: %s" % (name, r))


if __name__ == "__main__":
    main()
