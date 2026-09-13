#!/usr/bin/env python3
"""Compile a handful of icons into a single ffont for pebble-fctx.

Two upstream sets feed it, because neither covers what the face needs on its
own. Weather Icons ships a whole SVG font of 200-odd glyphs -- far more than a
watchface wants, and far more than a Pebble's heap will hold, since ffont_create
loads the entire font into RAM. Material Symbols ships one SVG per icon, so only
the ones named get vendored at all. Both are the outline style, which is what
lets them sit next to each other on the same face.

fonts/icons.list decides what goes in: a bare name is a Weather Icons class,
and a "material:" prefix is a file under fonts/material-symbols.

Two things have to happen on the way, neither of which fctx-compiler does:

  * The icons are remapped onto ASCII letters. Upstream they live in the
    private use area, and whether fctx decodes UTF-8 is not something to bet a
    font on; letters also keep the glyph index small. Letters rather than all
    of printable ASCII because the character has to survive both an XML
    attribute and a C string literal unescaped. Which icon got which letter is
    written out as src/c/weather_icons.h, so nothing in the C has to know.

  * The glyphs are normalised into a common box. Upstream they are drawn at the
    size each icon wants -- a sun spans 2267 units and a moon 1510, sitting on
    different baselines -- so swapping one for another in a fixed slot would
    visibly jump. Each is scaled to fit ICON_BOX preserving aspect, then centred
    both ways, and the font's cap height is set to the box. That makes
    fctx_set_text_cap_height(ctx, font, n) mean "draw this icon n pixels tall",
    and FTextAnchorCapMiddle land on the icon's middle.

Everything upstream is vendored under fonts/, so this needs no network.
"""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WEATHER_SVG = ROOT / "fonts" / "weathericons-regular-webfont.svg"
WEATHER_CSS = ROOT / "fonts" / "weathericons.css"
MATERIAL_DIR = ROOT / "fonts" / "material-symbols"
MATERIAL_PREFIX = "material:"
LIST_IN = ROOT / "fonts" / "icons.list"
SVG_OUT = ROOT / "fonts" / "Icons.generated.svg"
FFONT_OUT = ROOT / "resources" / "Icons.ffont"
HEADER_OUT = ROOT / "src" / "c" / "icons.h"

UNITS_PER_EM = 2048
# The square every icon is fitted into, and the font's cap height. Three
# quarters of the em leaves the glyphs room to breathe against text set at the
# same nominal size.
ICON_BOX = 1536
# Icons are addressed by these characters, in list order. Letters only: the
# character has to sit unescaped inside both an XML attribute and a C string.
ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"

TOKEN = re.compile(r"([MmZzLlHhVvCcSsQqTtAa])|(-?\d*\.?\d+(?:[eE][-+]?\d+)?)")
ARITY = {"M": 2, "L": 2, "H": 1, "V": 1, "C": 6, "S": 4, "Q": 4, "T": 2, "A": 7, "Z": 0}


def die(msg):
    sys.exit(f"build-weather-font: {msg}")


def parse_path(d):
    """Split path data into (command, numbers), expanding implicit repeats."""
    tokens = [(m.group(1), m.group(2)) for m in TOKEN.finditer(d)]
    out, i, cmd = [], 0, None
    while i < len(tokens):
        if tokens[i][0]:
            cmd = tokens[i][0]
            i += 1
            if cmd in "Zz":
                out.append((cmd, []))
                continue
        if cmd is None:
            die("path data does not start with a command")
        want = ARITY[cmd.upper()]
        nums = []
        while len(nums) < want and i < len(tokens) and tokens[i][1] is not None:
            nums.append(float(tokens[i][1]))
            i += 1
        if len(nums) < want:
            break
        out.append((cmd, nums))
        # A repeated moveto is an implicit lineto.
        if cmd == "M":
            cmd = "L"
        elif cmd == "m":
            cmd = "l"
    return out


def to_absolute(d):
    """Flatten an outline to absolute M / L / Q / C / Z.

    Arcs stay a hard error rather than something to silently mangle; neither
    source uses them.
    """
    x = y = start_x = start_y = 0.0
    ctrl_x = ctrl_y = 0.0
    out = []
    for cmd, a in parse_path(d):
        rel, up = cmd.islower(), cmd.upper()
        if up == "M":
            x, y = (x + a[0], y + a[1]) if rel else (a[0], a[1])
            start_x, start_y = x, y
            out.append(("M", [x, y]))
        elif up == "L":
            x, y = (x + a[0], y + a[1]) if rel else (a[0], a[1])
            out.append(("L", [x, y]))
        elif up == "H":
            x = x + a[0] if rel else a[0]
            out.append(("L", [x, y]))
        elif up == "V":
            y = y + a[0] if rel else a[0]
            out.append(("L", [x, y]))
        elif up == "C":
            c1x, c1y = (x + a[0], y + a[1]) if rel else (a[0], a[1])
            c2x, c2y = (x + a[2], y + a[3]) if rel else (a[2], a[3])
            nx, ny = (x + a[4], y + a[5]) if rel else (a[4], a[5])
            out.append(("C", [c1x, c1y, c2x, c2y, nx, ny]))
            ctrl_x, ctrl_y, x, y = c2x, c2y, nx, ny
        elif up == "S":
            # Smooth cubic: the first control point reflects the previous one.
            c1x, c1y = 2 * x - ctrl_x, 2 * y - ctrl_y
            c2x, c2y = (x + a[0], y + a[1]) if rel else (a[0], a[1])
            nx, ny = (x + a[2], y + a[3]) if rel else (a[2], a[3])
            out.append(("C", [c1x, c1y, c2x, c2y, nx, ny]))
            ctrl_x, ctrl_y, x, y = c2x, c2y, nx, ny
        elif up == "Q":
            cx, cy = (x + a[0], y + a[1]) if rel else (a[0], a[1])
            nx, ny = (x + a[2], y + a[3]) if rel else (a[2], a[3])
            out.append(("Q", [cx, cy, nx, ny]))
            ctrl_x, ctrl_y, x, y = cx, cy, nx, ny
        elif up == "T":
            # Smooth quadratic: reflect the previous control point.
            cx, cy = 2 * x - ctrl_x, 2 * y - ctrl_y
            nx, ny = (x + a[0], y + a[1]) if rel else (a[0], a[1])
            out.append(("Q", [cx, cy, nx, ny]))
            ctrl_x, ctrl_y, x, y = cx, cy, nx, ny
        elif up == "Z":
            out.append(("Z", []))
            x, y = start_x, start_y
        else:
            die(f"unsupported path command {cmd!r}; no source should need arcs")
        if up not in "QTCS":
            ctrl_x, ctrl_y = x, y
    return out


def bounds(segments):
    xs, ys = [], []
    for _cmd, a in segments:
        for i in range(0, len(a), 2):
            xs.append(a[i])
            ys.append(a[i + 1])
    if not xs:
        return None
    return min(xs), min(ys), max(xs), max(ys)


def normalise(segments):
    """Scale to fit ICON_BOX preserving aspect, then centre in the box."""
    box = bounds(segments)
    if box is None:
        return segments, 0.0
    x0, y0, x1, y1 = box
    width, height = x1 - x0, y1 - y0
    scale = ICON_BOX / max(width, height) if max(width, height) else 1.0
    dx = (ICON_BOX - width * scale) / 2 - x0 * scale
    dy = (ICON_BOX - height * scale) / 2 - y0 * scale
    moved = []
    for cmd, a in segments:
        moved.append((cmd, [a[i] * scale + (dx if i % 2 == 0 else dy)
                            for i in range(len(a))]))
    return moved, scale


def render_path(segments):
    def num(v):
        return f"{v:.0f}" if abs(v - round(v)) < 0.05 else f"{v:.1f}"
    return "".join(cmd + " ".join(num(v) for v in a) for cmd, a in segments)


def load_names():
    """Weather Icons ships no glyph-name attributes, so its CSS is the name map."""
    css = WEATHER_CSS.read_text(encoding="utf-8")
    pairs = re.findall(r"\.(wi-[a-z0-9-]+):before\s*\{\s*content:\s*\"\\([0-9a-fA-F]+)\"",
                       css)
    if not pairs:
        die(f"no icon names found in {WEATHER_CSS}")
    return {name: int(code, 16) for name, code in pairs}


def load_glyphs():
    svg = WEATHER_SVG.read_text(encoding="utf-8")
    out = {}
    for m in re.finditer(r'<glyph unicode="&#x([0-9a-f]+);"'
                         r'(?:\s+horiz-adv-x="(\d+)")?\s*d="([^"]*)"', svg):
        out[int(m.group(1), 16)] = m.group(3)
    if not out:
        die(f"no glyphs found in {WEATHER_SVG}")
    return out


def load_material(name):
    """A Material Symbols icon is a standalone SVG drawing rather than a font
    glyph, so it arrives in screen coordinates: y grows downwards, out of a
    viewBox with a negative origin. Negating y puts it back in font space."""
    path = MATERIAL_DIR / f"{name}.svg"
    if not path.exists():
        die(f"{path} not found; fetch it from the Material Symbols repository")
    outlines = re.findall(r'\bd="([^"]*)"', path.read_text(encoding="utf-8"))
    if not outlines:
        die(f"no outline in {path}")

    segments = to_absolute(" ".join(outlines))

    return [(cmd, [v if i % 2 == 0 else -v for i, v in enumerate(a)])
            for cmd, a in segments]


def load_wanted():
    wanted = []
    for line in LIST_IN.read_text(encoding="utf-8").splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            wanted.append(line)
    if not wanted:
        die(f"{LIST_IN} lists no icons")
    if len(wanted) > len(ALPHABET):
        die(f"{len(wanted)} icons, but only {len(ALPHABET)} characters to map them onto")
    dupes = {n for n in wanted if wanted.count(n) > 1}
    if dupes:
        die(f"duplicate icons in the list: {', '.join(sorted(dupes))}")
    return wanted


def c_name(icon):
    if icon.startswith(MATERIAL_PREFIX):
        return "ICON_" + icon[len(MATERIAL_PREFIX):].replace("-", "_").upper()

    return "ICON_" + icon[len("wi-"):].replace("-", "_").upper()


def main():
    names, glyphs, wanted = load_names(), load_glyphs(), load_wanted()

    entries = []
    for index, icon in enumerate(wanted):
        if icon.startswith(MATERIAL_PREFIX):
            raw = load_material(icon[len(MATERIAL_PREFIX):])
            origin = "Material Symbols"
        else:
            if icon not in names:
                die(f"unknown icon {icon!r}; it is not in {WEATHER_CSS.name}")
            codepoint = names[icon]
            if codepoint not in glyphs:
                die(f"{icon} (U+{codepoint:04X}) has no outline in "
                    f"{WEATHER_SVG.name}")
            raw = to_absolute(glyphs[codepoint])
            origin = f"U+{codepoint:04X}"

        segments, scale = normalise(raw)
        entries.append({
            "icon": icon,
            "code": ALPHABET[index],
            "origin": origin,
            "d": render_path(segments),
            "scale": scale,
        })

    SVG_OUT.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        '<?xml version="1.0" standalone="no"?>',
        "<!-- Generated by scripts/build-icon-font.py. Do not edit. -->",
        '<svg xmlns="http://www.w3.org/2000/svg">',
        "<defs>",
        f'<font id="Icons" horiz-adv-x="{ICON_BOX}">',
        f'<font-face units-per-em="{UNITS_PER_EM}" ascent="{ICON_BOX}"'
        f' descent="0" cap-height="{ICON_BOX}" />',
        '<missing-glyph horiz-adv-x="0" />',
    ]
    for e in entries:
        lines.append(f'<glyph unicode="{e["code"]}" glyph-name="{e["icon"]}"'
                     f' horiz-adv-x="{ICON_BOX}" d="{e["d"]}" />')
    lines += ["</font>", "</defs>", "</svg>", ""]
    SVG_OUT.write_text("\n".join(lines), encoding="utf-8")

    compiler = ROOT / "node_modules" / ".bin" / "fctx-compiler"
    if not compiler.exists():
        die("fctx-compiler not found; run `npm install` first")
    result = subprocess.run([str(compiler), str(SVG_OUT)], cwd=ROOT)
    if result.returncode != 0:
        die("fctx-compiler failed")
    if not FFONT_OUT.exists():
        die(f"fctx-compiler wrote no {FFONT_OUT}")

    guard = "FACEOFF_ICONS_H"
    header = [
        "// Generated by scripts/build-icon-font.py. Do not edit.",
        "//",
        "// Each icon is one character of RESOURCE_ID_ICONFONT. Draw one with",
        "// fctx_draw_string(&fctx, ICON_DAY_SUNNY, s_icon_font, ...) -- the glyphs",
        f"// are normalised to a {ICON_BOX}/{UNITS_PER_EM} em square, so the cap height "
        "set on the",
        "// context is the height the icon comes out.",
        "",
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
    ]
    width = max(len(c_name(e["icon"])) for e in entries)
    for e in entries:
        header.append(f'#define {c_name(e["icon"]):<{width}} "{e["code"]}"'
                      f'  // {e["icon"]} ({e["origin"]})')
    header += ["", f"#endif", ""]
    HEADER_OUT.write_text("\n".join(header), encoding="utf-8")

    print(f"\n{len(entries)} icons -> {FFONT_OUT.relative_to(ROOT)}"
          f" ({FFONT_OUT.stat().st_size} bytes), {HEADER_OUT.relative_to(ROOT)}")


main()
