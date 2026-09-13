#!/usr/bin/env python3
"""App Store screenshot generator for Faceoff.

Settings are injected by generating src/c/shot_config.h and rebuilding, not by
driving the Clay config page, so every shot is reproducible and pixel-identical
across runs. The generated header also pins the clock and every reading the
face draws -- the emulator has no step history, no pulse, and a weather reading
that depends on where the machine taking the shot happens to be standing.

A shot build asks the phone for nothing and ignores what it says unprompted,
which matters here: the JS fetches the weather on `ready` whether the watch
asked or not, and that reply would otherwise land on top of the pinned one a
second or two after the face came up.

Usage:
    scripts/shots.py                        # every platform x its presets
    scripts/shots.py --platform emery       # one platform
    scripts/shots.py --preset steps         # one preset, on every platform
    scripts/shots.py --list                 # show what would be produced
"""

import argparse
import datetime as dt
import json
import os
import subprocess
import sys
import time
from pathlib import Path

# --------------------------------------------------------------------------
# CONFIGURATION - edit this section
# --------------------------------------------------------------------------

# The clock behind every shot unless a preset overrides it. Pinned so that the
# weekday, the month and the day of the month all come out the same however
# long from now the shots are retaken. A Saturday, and clear of the spring and
# autumn clock changes.
DEFAULT_DATE = "2026-03-14"
DEFAULT_TIME = "14:35"

# Preset keys, all optional:
#
# time:         "HH:MM" on a 24h clock, what the watch shows.
# date:         "YYYY-MM-DD" override for DEFAULT_DATE. Drives the weekday, the
#               month and the day of the month the date column renders.
# format:       how the time is set, from TIME_FORMATS: "system" follows the
#               watch, "12h" and "24h" override it. Defaults to "system", which
#               is what the face ships with.
# hour24:       what the watch's own 12/24h setting says, default True. Only
#               reaches the screen while format is "system".
#
# top,          what each of the two corners beside the time shows, named from
# bottom:       COMPLICATIONS below. Top is the one beside the hour, bottom the
#               one beside the minute.
#
# colors:       what each element is drawn in, as {element: color}; elements
#               come from ELEMENTS below and colors from COLORS, or any
#               "0xRRGGBB". Anything left out keeps the app default. A no-op on
#               aplite, diorite and flint, which are black & white.
# bw_style:     which end of the face carries the dark stripe on a one bit
#               screen, from BW_STYLES. A no-op on the color platforms.
# fill_corners: True squares off the uncovered corners, so the stripes cover
#               the whole screen. Nothing to see on chalk or gabbro, which are
#               round and have no corners to fill.
#
# temp:         the temperature the weather complication renders, in whole
#               degrees Celsius whatever the unit. None renders the face as it
#               looks before the phone has reported: "--" under the no-data
#               icon.
# cond:         the sky it reports, from WEATHER_CONDS, which picks the icon.
# unit:         the scale the reading is drawn in, "c" or "f". The value is
#               always given in Celsius; only the rendering changes.
#
# steps:        step count the steps complication renders. None renders the
#               dashes a watch with no step history shows. Worth overriding to
#               check the widths the column has to hold: under a thousand it is
#               set in full, then "8.2K", then "12K".
# heart:        beats per minute the heart complication renders. None renders
#               the dashes the face shows before the monitor has sampled.
#
# Health readings need a HealthService, which aplite has none of: a preset that
# asks for one renders the dashes there however it is pinned.

PRESETS = {
    # What the face ships with: the weather beside the hour, the date beside
    # the minute.
    "default":      dict(),

    # The three-line column, which is the tightest thing either corner holds.
    "weekday-date": dict(bottom="weekday-date"),

    # The health readings, each beside the date.
    "steps":        dict(top="heart", bottom="steps", steps=8432),
    "heart":        dict(top="heart", bottom="weekday-date"),
    # A five-figure count, the widest the steps column ever has to hold.
    "walked":       dict(top="steps", bottom="date", steps=12040),

    # The 12h clock, at an hour where it differs from the 24h one.
    "12h":          dict(format="12h", time="21:05"),

    # A sub-zero reading, the widest the weather column ever has to hold, and
    # the other scale, at a temperature no Celsius reading could be mistaken for.
    "cold":         dict(temp=-22, cond="snow", bottom="weekday-date"),
    "fahrenheit":   dict(temp=32, cond="clear-day", unit="f"),

    # Nothing heard from the phone yet, which is what a first launch looks like.
    "no-weather":   dict(temp=None, cond="unknown"),

    # The stripes squared off to the edges, which only rect screens show.
    "fill-corners": dict(fill_corners=True, bottom="weekday-date"),

    # The palette moved off the default, to check the ink follows the stripes.
    "recolored":    dict(bottom="weekday-date",
                         colors=dict(top_stripe="dark-green",
                                     bottom_stripe="chrome-yellow",
                                     background="oxford-blue",
                                     hour="mint-green",
                                     minute="black",
                                     top_text="mint-green",
                                     bottom_text="black")),

    # The face with both corners cleared: the time and nothing else.
    "bare":         dict(top="none", bottom="none"),

    # The one bit face the other way up. A no-op anywhere but aplite, diorite
    # and flint.
    "light-top":    dict(bw_style="light-top", bottom="weekday-date"),

    # Store shots
    "red-black":    dict(top="heart", bottom="steps", steps=8432,
                         colors=dict(top_stripe="red",
                                     bottom_stripe="black",
                                     background="dark-gray")),
    "black-white":  dict(top="weather", bottom="steps", steps=8432,
                         colors=dict(top_stripe="black",
                                     bottom_stripe="white",
                                     minute="black",
                                     bottom_text="black",
                                     background="dark-gray")),
    "white-blue":   dict(top="weather", bottom="weekday-date",
                         fill_corners=True,
                         colors=dict(top_stripe="white",
                                     bottom_stripe="duke-blue",
                                     hour="duke-blue",
                                     top_text="duke-blue")),
    "brass-purple": dict(top="weekday", bottom="date",
                         colors=dict(top_stripe="imperial-purple",
                                     bottom_stripe="brass",
                                     hour="brass",
                                     top_text="brass",
                                     minute="imperial-purple",
                                     bottom_text="imperial-purple",
                                     background="midnight-green")),
}

# Which presets each platform is shot with. "*" is the fallback for any
# platform without its own entry. The one bit platforms are shot both ways up
# rather than recolored, and aplite -- which has neither a health service nor
# the room for the icon font -- gets the readings it can actually draw.
PLATFORM_SHOTS = {
    "*":       ["default", "red-black", "black-white", "white-blue", "brass-purple"],
    "aplite":  ["default", "bare", "light-top"],
    "diorite": ["default", "steps", "light-top"],
    "flint":   ["default", "steps", "light-top"],
}

# What a shot draws unless a preset says otherwise. The settings here are the
# ones default_settings() ships; the readings stand in for what the emulator
# cannot provide.
SHOT_TEMP_C = 18
SHOT_COND = "partly-day"
SHOT_STEPS = 8432
SHOT_HEART = 64

# Mirrors Complication in src/c/settings.h and COMPLICATIONS in
# src/pkjs/config.js; keep in step with both.
COMPLICATIONS = {
    "none": 0, "weather": 1, "weekday": 2, "date": 3, "weekday-date": 4,
    "steps": 5, "heart": 6,
}
# The ones a HealthService backs, which aplite has none of.
HEALTH_COMPLICATIONS = ("steps", "heart")

# Mirrors WeatherCondition in src/c/weather.h and CONDITIONS in
# src/pkjs/weather.js. Only clear and partly cloudy have a night form; the
# cloud-, rain- and snow-based icons are the same either way.
WEATHER_CONDS = {
    "clear-day": 0, "clear-night": 1, "partly-day": 2, "partly-night": 3,
    "cloudy": 4, "rain": 5, "snow": 6, "fog": 7, "thunder": 8, "unknown": 9,
}

# Mirrors TimeFormat, BWStripeStyle and TemperatureUnit in src/c/settings.h.
TIME_FORMATS = {"system": 0, "12h": 1, "24h": 2}
BW_STYLES = {"dark-top": 0, "light-top": 1}
UNITS = {"c": 0, "f": 1}

# The colourable elements, and the SHOT_* define each one writes. "top_text"
# and "bottom_text" are the two complication columns, which the C still calls
# wday and mday after the only two readings they used to hold.
ELEMENTS = {
    "top_stripe": "TOP_STRIPE_COLOR",
    "bottom_stripe": "BOTTOM_STRIPE_COLOR",
    "background": "BACKGROUND_COLOR",
    "hour": "HOUR_COLOR",
    "minute": "MINUTE_COLOR",
    "top_text": "TOP_TEXT_COLOR",
    "bottom_text": "BOTTOM_TEXT_COLOR",
}
# What default_settings() ships, so a preset that names no colour renders the
# face as installed.
DEFAULT_COLORS = {
    "top_stripe": "jazzberry-jam",
    "bottom_stripe": "very-light-blue",
    "background": "black",
    "hour": "white",
    "minute": "white",
    "top_text": "white",
    "bottom_text": "white",
}

# A slice of the Pebble 64, enough to recolour the face without reaching for
# hex. Anything else can be given as "0xRRGGBB" instead of a name.
COLORS = {
    "black": 0x000000, "white": 0xFFFFFF, "light-gray": 0xAAAAAA,
    "dark-gray": 0x555555,
    "red": 0xFF0000, "folly": 0xFF0055, "jazzberry-jam": 0xAA0055,
    "dark-candy-apple-red": 0xAA0000, "orange": 0xFF5500,
    "chrome-yellow": 0xFFAA00, "yellow": 0xFFFF00, "icterine": 0xFFFF55,
    "green": 0x00FF00, "islamic-green": 0x00AA00, "dark-green": 0x005500,
    "malachite": 0x00FF55, "mint-green": 0xAAFFAA, "spring-bud": 0xAAFF00,
    "cyan": 0x00FFFF, "celeste": 0xAAFFFF, "picton-blue": 0x00AAFF,
    "blue": 0x0000FF, "very-light-blue": 0x5555FF, "oxford-blue": 0x000055,
    "duke-blue": 0x0000AA, "vivid-violet": 0xAA00FF, "purple": 0xAA00AA,
    "magenta": 0xFF00FF, "shocking-pink": 0xFF55FF, "brilliant-rose": 0xFF55AA,
    "melon": 0xFFAAAA, "rajah": 0xFFAA55, "windsor-tan": 0xAA5500,
    "brass": 0xAAAA55, "imperial-purple": 0x550055, "midnight-green": 0x005555,
}

# --------------------------------------------------------------------------
# End of configuration
# --------------------------------------------------------------------------

ROOT = Path(__file__).resolve().parent.parent
SHOT_CONFIG_H = ROOT / "src" / "c" / "shot_config.h"
DEFAULT_OUT = ROOT / "shots"

# `pebble kill` returns before the emulator is gone, and a freshly wiped
# emulator sometimes cold-boots slower than `pebble install` will wait, failing
# with "Connection refused". Pause after the kill, and relaunch on failure.
KILL_PAUSE_SECONDS = 2.0
INSTALL_ATTEMPTS = 3
RETRY_PAUSE_SECONDS = 3.0
# Long enough for the face to launch and lay itself out. Nothing here waits on
# the phone-side JS -- a shot build never asks it for anything.
DEFAULT_SETTLE = 3.0

PRESET_KEYS = {
    "time", "date", "format", "hour24", "top", "bottom", "colors", "bw_style",
    "fill_corners", "temp", "cond", "unit", "steps", "heart",
}


class ShotError(Exception):
    pass


def log(msg):
    print(f"[shots] {msg}", flush=True)


def run(cmd, verbose, env=None, cwd=ROOT):
    """Run a command, raising ShotError with its output on failure."""
    if verbose:
        log("$ " + " ".join(cmd))
    proc = subprocess.run(
        cmd, cwd=cwd, text=True, env=env,
        stdout=None if verbose else subprocess.PIPE,
        stderr=None if verbose else subprocess.STDOUT,
    )
    if proc.returncode != 0:
        out = (proc.stdout or "").strip()
        raise ShotError(
            f"`{' '.join(cmd)}` exited {proc.returncode}"
            + (f"\n--- output ---\n{out}" if out else "")
        )
    return proc.stdout or ""


def target_platforms():
    pkg = json.loads((ROOT / "package.json").read_text())
    return pkg["pebble"]["targetPlatforms"]


def presets_for(platform):
    return PLATFORM_SHOTS.get(platform, PLATFORM_SHOTS.get("*", []))


def color_hex(name, where):
    """A palette name or a "0xRRGGBB" / "#RRGGBB" literal, as an int."""
    if isinstance(name, int):
        return name
    if name in COLORS:
        return COLORS[name]
    try:
        return int(str(name).lstrip("#").replace("0x", ""), 16)
    except ValueError:
        raise ShotError(f"{where}: unknown color {name!r}; known: "
                        f"{', '.join(sorted(COLORS))}, or 0xRRGGBB")


def resolve_preset(name):
    if name not in PRESETS:
        raise ShotError(f"unknown preset {name!r}; known: {', '.join(sorted(PRESETS))}")
    return normalize_preset(name, PRESETS[name])


def normalize_preset(name, preset):
    """Validate a preset and fill in every default write_shot_config expects."""
    p = dict(preset)

    unknown = set(p) - PRESET_KEYS
    if unknown:
        raise ShotError(f"{name}: unknown preset keys {sorted(unknown)}")

    when = f"{p.pop('date', DEFAULT_DATE)} {p.pop('time', DEFAULT_TIME)}"
    try:
        p["when"] = dt.datetime.strptime(when, "%Y-%m-%d %H:%M")
    except ValueError as e:
        raise ShotError(f"{name}: bad date/time {when!r} ({e})")

    p.setdefault("format", "system")
    if p["format"] not in TIME_FORMATS:
        raise ShotError(f"{name}: unknown format {p['format']!r}; "
                        f"known: {', '.join(TIME_FORMATS)}")
    p.setdefault("hour24", True)

    for slot, default in (("top", "weather"), ("bottom", "date")):
        value = p.get(slot, default)
        value = "none" if value is None else value
        if value not in COMPLICATIONS:
            raise ShotError(f"{name}: unknown complication {value!r} in {slot}; "
                            f"known: {', '.join(sorted(COMPLICATIONS))}")
        p[slot] = value

    colors = dict(DEFAULT_COLORS)
    for element, color in (p.get("colors") or {}).items():
        if element not in ELEMENTS:
            raise ShotError(f"{name}: unknown element {element!r}; "
                            f"known: {', '.join(sorted(ELEMENTS))}")
        colors[element] = color
    p["colors"] = {e: color_hex(c, name) for e, c in colors.items()}

    p.setdefault("bw_style", "dark-top")
    if p["bw_style"] not in BW_STYLES:
        raise ShotError(f"{name}: unknown bw_style {p['bw_style']!r}; "
                        f"known: {', '.join(BW_STYLES)}")
    p.setdefault("fill_corners", False)

    if "temp" not in p:
        p["temp"] = SHOT_TEMP_C
    p.setdefault("cond", SHOT_COND)
    if p["cond"] not in WEATHER_CONDS:
        raise ShotError(f"{name}: unknown cond {p['cond']!r}; "
                        f"known: {', '.join(sorted(WEATHER_CONDS))}")
    p.setdefault("unit", "c")
    if p["unit"] not in UNITS:
        raise ShotError(f"{name}: unknown unit {p['unit']!r}; "
                        f"known: {', '.join(UNITS)}")

    if "steps" not in p:
        p["steps"] = SHOT_STEPS
    if "heart" not in p:
        p["heart"] = SHOT_HEART
    return p


def write_shot_config(preset):
    """Generate src/c/shot_config.h for one variant."""
    w = preset["when"]
    tm = ", ".join([
        ".tm_sec = 0",
        f".tm_min = {w.minute}",
        f".tm_hour = {w.hour}",
        f".tm_mday = {w.day}",
        f".tm_mon = {w.month - 1}",
        f".tm_year = {w.year - 1900}",
        f".tm_wday = {(w.weekday() + 1) % 7}",  # C: 0 = Sunday
        f".tm_yday = {w.timetuple().tm_yday - 1}",
        ".tm_isdst = 0",
    ])
    SHOT_CONFIG_H.write_text(
        "// Generated by scripts/shots.py - do not edit, do not commit.\n"
        "#pragma once\n\n"
        f"#define SHOT_TM {{ {tm} }}\n"
        f"#define SHOT_24H {1 if preset['hour24'] else 0}\n"
        f"#define SHOT_TIME_FORMAT {TIME_FORMATS[preset['format']]}\n"
        f"#define SHOT_TOP_COMPLICATION {COMPLICATIONS[preset['top']]}\n"
        f"#define SHOT_BOTTOM_COMPLICATION {COMPLICATIONS[preset['bottom']]}\n"
        f"#define SHOT_FILL_CORNERS {1 if preset['fill_corners'] else 0}\n"
        f"#define SHOT_BW_STRIPE_STYLE {BW_STYLES[preset['bw_style']]}\n"
        + "".join(f"#define SHOT_{define} 0x{preset['colors'][element]:06X}\n"
                  for element, define in ELEMENTS.items())
        + f"#define SHOT_TEMPERATURE_UNIT {UNITS[preset['unit']]}\n"
        f"#define SHOT_WEATHER_HAVE {0 if preset['temp'] is None else 1}\n"
        f"#define SHOT_WEATHER_DC {0 if preset['temp'] is None else preset['temp'] * 10}\n"
        f"#define SHOT_WEATHER_COND {WEATHER_CONDS[preset['cond']]}\n"
        f"#define SHOT_STEPS {-1 if preset['steps'] is None else preset['steps']}\n"
        f"#define SHOT_HEART {0 if preset['heart'] is None else preset['heart']}\n"
    )


def rel(path):
    """`path` relative to the repo, or as given when --out points outside it."""
    try:
        return str(path.relative_to(ROOT))
    except ValueError:
        return str(path)


def describe(preset):
    slots = f"top:{preset['top']} bottom:{preset['bottom']}"
    recolored = "".join(
        f" {element}:0x{preset['colors'][element]:06X}"
        for element in ELEMENTS
        if preset["colors"][element] != COLORS[DEFAULT_COLORS[element]])
    extras = "".join(
        flag for flag, on in (
            (" fill-corners", preset["fill_corners"]),
            (" light-top", preset["bw_style"] == "light-top"),
        ) if on)
    return f"{slots}{recolored}{extras}"


def build(verbose):
    """Rebuild every target platform against the current shot_config.h."""
    run(["pebble", "build"], verbose, env=dict(os.environ, SHOT_CONFIG="1"))


def pebble(args, verbose):
    run(["pebble"] + args, verbose)


def boot(platform, verbose, settle):
    """Kill every emulator, wipe persisted data, install and let it settle."""
    failure = None
    for attempt in range(1, INSTALL_ATTEMPTS + 1):
        pebble(["kill"], verbose)
        time.sleep(KILL_PAUSE_SECONDS)
        pebble(["wipe"], verbose)
        try:
            pebble(["install", "--emulator", platform], verbose)
            break
        except ShotError as e:
            failure = e
            log(f"install attempt {attempt}/{INSTALL_ATTEMPTS} failed "
                f"({str(e).splitlines()[-1]}), relaunching")
            time.sleep(RETRY_PAUSE_SECONDS)
    else:
        raise ShotError(
            f"{platform}: install failed {INSTALL_ATTEMPTS} times\n{failure}")
    time.sleep(settle)


def screenshot(platform, path, verbose):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        path.unlink()  # the pebble tool will not overwrite in place
    # Color correction stays on: store shots should match the real display.
    cmd = ["screenshot", "--emulator", platform, "--no-open", str(path)]
    try:
        pebble(cmd, verbose)
    except ShotError as e:
        log(f"screenshot failed ({str(e).splitlines()[-1]}), retrying once")
        time.sleep(RETRY_PAUSE_SECONDS)
        pebble(cmd, verbose)
    if not path.exists() or path.stat().st_size == 0:
        raise ShotError(f"pebble screenshot produced nothing at {path}")


def warn_unrenderable(platform, name, preset):
    """Say so when a platform cannot draw what a preset asked for."""
    if platform != "aplite":
        return
    asked = [slot for slot in ("top", "bottom")
             if preset[slot] in HEALTH_COMPLICATIONS]
    if asked:
        log(f"note: aplite has no health service, so {name} renders "
            f"{'/'.join(asked)} as --")


def do_shots(plan, out_dir, verbose, settle):
    made = []
    # One build per preset, then every platform that wants it, so a preset shot
    # on seven platforms rebuilds once rather than seven times.
    for name in dict.fromkeys(n for _, n in plan):
        preset = resolve_preset(name)
        log(f"{name}: {preset['when']:%Y-%m-%d %H:%M} {preset['format']} "
            f"{describe(preset)}")
        write_shot_config(preset)
        build(verbose)
        for platform in [p for p, n in plan if n == name]:
            warn_unrenderable(platform, name, preset)
            boot(platform, verbose, settle)
            path = out_dir / f"{platform}-{name}.png"
            screenshot(platform, path, verbose)
            log(f"wrote {rel(path)}")
            made.append(path)
    pebble(["kill"], verbose)
    return made


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--platform", action="append", metavar="NAME",
                    help="only this platform (repeatable); default all targets")
    ap.add_argument("--preset", action="append", metavar="NAME",
                    help="only this preset (repeatable); default per-platform set")
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT,
                    help=f"output directory (default {rel(DEFAULT_OUT)})")
    ap.add_argument("--settle", type=float, default=DEFAULT_SETTLE, metavar="SECONDS",
                    help="pause after install before capturing")
    ap.add_argument("--list", action="store_true",
                    help="print the plan and exit without building")
    ap.add_argument("--verbose", "-v", action="store_true",
                    help="stream pebble output")
    args = ap.parse_args()

    try:
        targets = target_platforms()
        platforms = args.platform or targets
        unknown = set(platforms) - set(targets)
        if unknown:
            raise ShotError(f"not a target platform: {', '.join(sorted(unknown))}; "
                            f"known: {', '.join(targets)}")

        plan = []
        for platform in platforms:
            for name in args.preset or presets_for(platform):
                resolve_preset(name)  # validate before anything is built
                plan.append((platform, name))
        if not plan:
            raise ShotError("nothing to shoot")

        if args.list:
            for platform, name in plan:
                print(f"{platform}-{name}.png  {describe(resolve_preset(name))}")
            return 0

        made = do_shots(plan, args.out, args.verbose, args.settle)
        log(f"{len(made)} shot(s) in {rel(args.out)}")
        log("build/ now holds a -DSHOT_CONFIG build; "
            "run `pebble build` before installing for real")
    except ShotError as e:
        print(f"[shots] error: {e}", file=sys.stderr)
        return 1
    finally:
        SHOT_CONFIG_H.unlink(missing_ok=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
