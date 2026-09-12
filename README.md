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

The weather icons are a subset of [Weather Icons](https://github.com/erikflowers/weather-icons), compiled by `just compile_weather_font`. Which icons go in is the list in `fonts/weather-icons.list`; adding one there and rerunning the recipe regenerates both `resources/WeatherIcons.ffont` and the `WI_*` defines in `src/c/weather_icons.h`. To see how candidates hold up at the sizes the face draws them, `scripts/preview-weather-icons.py` renders a sheet without going near an emulator.
