#!/usr/bin/env python3
"""Render build/WeatherIcons.svg to a PNG sheet at real watch pixel sizes.

Picking icons in the emulator costs a build, install and settle apiece. This
draws the same outlines the watch would, at the sizes the face actually uses,
so a dozen candidates can be compared at a glance and only the chosen ones need
confirming on a watch.

Fills are even-odd, which is what fctx does: many Weather Icons glyphs are
outlines built from an outer and an inner contour, and filling the contours
independently would show them as solid blobs that the watch never draws.

Usage: scripts/preview-weather-icons.py [out.png]
"""

import re
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
SVG = ROOT / "fonts" / "WeatherIcons.generated.svg"
# The cap heights the face asks for across the platform range, give or take.
SIZES = [16, 20, 24, 30]
SUPERSAMPLE = 8
FLATTEN_STEPS = 12
PAD = 5


def flatten(d):
    """Absolute M/L/Q/Z path data to a list of point lists, one per contour."""
    tokens = re.findall(r"([MLQZ])|(-?\d*\.?\d+)", d)
    contours, current, cmd, i = [], [], None, 0
    while i < len(tokens):
        letter, _number = tokens[i]
        if letter:
            cmd = letter
            i += 1
            if cmd == "Z":
                if current:
                    contours.append(current)
                current = []
                continue
        values = []
        want = {"M": 2, "L": 2, "Q": 4}[cmd]
        while len(values) < want and i < len(tokens) and tokens[i][1] is not None:
            values.append(float(tokens[i][1]))
            i += 1
        if cmd == "M":
            if current:
                contours.append(current)
            current = [(values[0], values[1])]
        elif cmd == "L":
            current.append((values[0], values[1]))
        elif cmd == "Q":
            x0, y0 = current[-1]
            cx, cy, x1, y1 = values
            for step in range(1, FLATTEN_STEPS + 1):
                t = step / FLATTEN_STEPS
                u = 1 - t
                current.append((u * u * x0 + 2 * u * t * cx + t * t * x1,
                                u * u * y0 + 2 * u * t * cy + t * t * y1))
    if current:
        contours.append(current)
    return contours


def render(contours, cap_height, pixels, cell):
    """One glyph at one cap height, even-odd filled."""
    scale = pixels / cap_height * SUPERSAMPLE
    size = cell * SUPERSAMPLE
    mask = Image.new("1", (size, size), 0)
    for contour in contours:
        if len(contour) < 3:
            continue
        layer = Image.new("1", (size, size), 0)
        ImageDraw.Draw(layer).polygon(
            [(x * scale + PAD * SUPERSAMPLE,
              (cap_height - y) * scale + PAD * SUPERSAMPLE) for x, y in contour],
            fill=1)
        mask = ImageChops.logical_xor(mask, layer)
    return Image.eval(mask.convert("L").resize((cell, cell), Image.BOX),
                      lambda v: 255 - v)


def main():
    if not SVG.exists():
        sys.exit(f"{SVG} not found; run scripts/build-weather-font.py first")
    svg = SVG.read_text(encoding="utf-8")
    cap_height = int(re.search(r'cap-height="(\d+)"', svg).group(1))
    glyphs = re.findall(r'<glyph unicode="(.)" glyph-name="([^"]+)"[^>]*?\bd="([^"]*)"',
                        svg)
    if not glyphs:
        sys.exit(f"no glyphs in {SVG}")

    cell = max(SIZES) + PAD * 2
    sheet = Image.new("L", (cell * len(SIZES), cell * len(glyphs)), 255)
    for row, (_code, _name, d) in enumerate(glyphs):
        contours = flatten(d)
        for col, pixels in enumerate(SIZES):
            sheet.paste(render(contours, cap_height, pixels, cell),
                        (col * cell, row * cell))

    out = (Path(sys.argv[1]) if len(sys.argv) > 1
           else ROOT / "fonts" / "weather-icons-preview.png")
    sheet.resize((sheet.width * 4, sheet.height * 4), Image.NEAREST).save(out)
    print(f"{out}  columns: {SIZES}px")
    for (_code, name, _d), row in zip(glyphs, range(len(glyphs))):
        print(f"  row {row + 1}: {name}")


main()
