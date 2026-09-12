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
