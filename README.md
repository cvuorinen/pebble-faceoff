# Pebble Faceoff Watchface

A Pebble watchface inspired by "versus" screens.

[Download it on the rePebble App Store](https://apps.repebble.com/836fe574006048dd84dacb5d)

## Screenshots

![Screenshot of Faceoff on Emery](./img/emery.png)
![Screenshot of Faceoff on Gabbro](./img/gabbro.png)

## Build

You must have [`just`](https://github.com/casey/just) and the [Pebble SDK](https://developer.repebble.com/sdk/) installed. This was built on version 4.9.148 of the SDK, but newer versions are likely still supported.

Once installed, you can run `just build` to build the pbw file.

Compiled versions of the fonts for fctx are included, but if you wish to recompile them, you can use `just compile_font` for the time and date font.

The complication icons come from two sets: [Weather Icons](https://github.com/erikflowers/weather-icons) for the conditions, and the outlined variant of [Material Symbols](https://github.com/google/material-design-icons) for everything else. Both are compiled into one font by `just compile_icon_font`. Which icons go in is the list in `fonts/icons.list` -- a bare name is a Weather Icons class, a `material:` prefix is a file under `fonts/material-symbols` -- and rerunning the recipe regenerates both `resources/Icons.ffont` and the `ICON_*` defines in `src/c/icons.h`. To see how candidates hold up at the sizes the face draws them, `scripts/preview-icons.py` renders a sheet without going near an emulator.

## Store screenshots

`scripts/shots.py` renders reproducible screenshots, one per platform per preset:

```
just shots --list                        # show what would be produced
just shots                               # every platform x its presets
just shots --platform emery              # one platform
just shots --preset steps                # one preset, on every platform
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
`PLATFORM_SHOTS` which ones each platform is shot with -- the one bit platforms
are shot both ways up rather than recoloured, and aplite, which has neither a
health service nor the room for the icon font, gets the readings it can actually
draw. Shots land in `shots/`.

A run leaves `build/` holding the screenshot build, so run `just build` before
installing to a watch you mean to wear.
