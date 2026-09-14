# Pebble Faceoff Remix Watchface

A Pebble watchface that is a "Remix" of a watchface called "Faceoff" (inspired by "versus" screens) made by ollien.
Credits to ollien for the original idea, and for releasing it as open source.
Original source code: https://github.com/ollien/pebble-faceoff

Changes I have made:

* Less slant and offset hour/minute numbers from each other.
* Switch "complications" to the other side.
* Add more complication options, including weather, step count and heart rate.
* Add support for black & white models.
* Add "intro animation" that slides in the values at startup, and an optional slide on every time change.

## Build

You must have the [Pebble SDK](https://developer.repebble.com/sdk/) installed.

Install dependencies with `npm ci`.

Compiled versions of the fonts for fctx are included, but if you wish to recompile them, you can use `npm run compile-font` for the time and date font.

The complication icons come from two sets: [Weather Icons](https://github.com/erikflowers/weather-icons) for the conditions, and the outlined variant of [Material Symbols](https://github.com/google/material-design-icons) for everything else. Both are compiled into one font by `npm run compile-icon-font`. Which icons go in is the list in `fonts/icons.list` -- a bare name is a Weather Icons class, a `material:` prefix is a file under `fonts/material-symbols` -- and rerunning the recipe regenerates both `resources/Icons.ffont` and the `ICON_*` defines in `src/c/icons.h`. To see how candidates hold up at the sizes the face draws them, `scripts/preview-icons.py` renders a sheet without going near an emulator.

## Store screenshots

`scripts/shots.py` renders reproducible screenshots, one per platform per preset:

```
scripts/shots.py --list                 # show what would be produced
scripts/shots.py                        # every platform x its presets
scripts/shots.py --platform emery       # one platform
scripts/shots.py --preset steps         # one preset, on every platform
```

Nothing in a shot comes from the machine it is taken on. Rather than driving the
Clay config page, the script generates `src/c/shot_config.h` and rebuilds with
`-DSHOT_CONFIG`, which pins the settings, the clock, the weather reading and the
health readings -- the emulator has no step history, no pulse, and a forecast
that depends on where the machine taking the shot happens to be standing. A shot
build also ignores the weather the phone-side JS reports unasked on launch, which
would otherwise land on top of the pinned reading a second after the face came
up.

`PRESETS` at the top of the script says what each named shot shows, and
`PLATFORM_SHOTS` which ones each platform is shot with. Shots land in `shots/`.

A run leaves `build/` holding the screenshot build, so re-run build before
installing to a watch you mean to wear.
