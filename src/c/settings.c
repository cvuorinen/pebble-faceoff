#include "settings.h"
#include "gcolor_definitions.h"
#include "message_keys.auto.h"
#include "pebble.h"

#ifdef SHOT_CONFIG
#include "shot_config.h"
#endif

Settings g_settings;

void default_settings() {
#ifdef SHOT_CONFIG
  // A screenshot build draws the settings scripts/shots.py pinned, so a shot
  // comes out the same whatever the emulator was last left configured as --
  // and there is nothing saved to read, since prv_load_settings skips the
  // persisted copy entirely.
  g_settings.top_stripe_color = GColorFromHEX(SHOT_TOP_STRIPE_COLOR);
  g_settings.bottom_stripe_color = GColorFromHEX(SHOT_BOTTOM_STRIPE_COLOR);
  g_settings.background_color = GColorFromHEX(SHOT_BACKGROUND_COLOR);
  g_settings.fill_corners = SHOT_FILL_CORNERS;
  g_settings.hour_color = GColorFromHEX(SHOT_HOUR_COLOR);
  g_settings.minute_color = GColorFromHEX(SHOT_MINUTE_COLOR);
  g_settings.time_format = SHOT_TIME_FORMAT;
  g_settings.wday_color = GColorFromHEX(SHOT_TOP_TEXT_COLOR);
  g_settings.mday_color = GColorFromHEX(SHOT_BOTTOM_TEXT_COLOR);
  g_settings.bw_stripe_style = SHOT_BW_STRIPE_STYLE;
  g_settings.top_complication = SHOT_TOP_COMPLICATION;
  g_settings.bottom_complication = SHOT_BOTTOM_COMPLICATION;
  g_settings.temperature_unit = SHOT_TEMPERATURE_UNIT;
  // A shot never plays an animation anyway -- both play functions are no-ops
  // in a screenshot build -- but the fields are not left to whatever was on the
  // stack.
  g_settings.intro_animation = false;
  g_settings.tick_animation = false;
#else
  g_settings.top_stripe_color = GColorJazzberryJam;
  g_settings.bottom_stripe_color = GColorVeryLightBlue;
  g_settings.background_color = GColorBlack;
  g_settings.fill_corners = false;
  g_settings.hour_color = GColorWhite;
  g_settings.minute_color = GColorWhite;
  g_settings.time_format = TIME_FORMAT_SYSTEM;
  g_settings.wday_color = GColorWhite;
  g_settings.mday_color = GColorWhite;
  g_settings.bw_stripe_style = BW_STRIPES_DARK_TOP;
  g_settings.top_complication = COMPLICATION_WEATHER;
  g_settings.bottom_complication = COMPLICATION_DATE;
  g_settings.temperature_unit = TEMPERATURE_UNIT_CELSIUS;
  g_settings.intro_animation = true;
  g_settings.tick_animation = false;
#endif
}

static bool prv_draws_icons(Complication complication) {
  return complication == COMPLICATION_WEATHER ||
         complication == COMPLICATION_STEPS ||
         complication == COMPLICATION_HEART_RATE;
}

bool settings_want_weather() {
  return g_settings.top_complication == COMPLICATION_WEATHER ||
         g_settings.bottom_complication == COMPLICATION_WEATHER;
}

bool settings_want_icons() {
  return prv_draws_icons(g_settings.top_complication) ||
         prv_draws_icons(g_settings.bottom_complication);
}

bool update_settings(DictionaryIterator *iterator, void *context) {
#ifdef SHOT_CONFIG
  // Nothing the phone says can move a pinned shot. Clay only pushes when the
  // config page is saved, but a shot should not depend on nobody having opened
  // it.
  (void)iterator;
  (void)context;
  return false;
#else
  bool dirty = false;

  Tuple *top_stripe_color_tuple =
      dict_find(iterator, MESSAGE_KEY_TOP_STRIPE_COLOR);

  if (top_stripe_color_tuple) {
    g_settings.top_stripe_color =
        GColorFromHEX(top_stripe_color_tuple->value->int32);
    dirty = true;
  }

  Tuple *bottom_stripe_color_tuple =
      dict_find(iterator, MESSAGE_KEY_BOTTOM_STRIPE_COLOR);
  if (bottom_stripe_color_tuple) {
    g_settings.bottom_stripe_color =
        GColorFromHEX(bottom_stripe_color_tuple->value->int32);
    dirty = true;
  }

  Tuple *background_color_tuple =
      dict_find(iterator, MESSAGE_KEY_BACKGROUND_COLOR);
  if (background_color_tuple) {
    g_settings.background_color =
        GColorFromHEX(background_color_tuple->value->int32);
    dirty = true;
  }

  Tuple *fill_corners_tuple = dict_find(iterator, MESSAGE_KEY_FILL_CORNERS);
  if (fill_corners_tuple) {
    g_settings.fill_corners = fill_corners_tuple->value->int32 == 1;
    dirty = true;
  }

  Tuple *hour_color_tuple = dict_find(iterator, MESSAGE_KEY_HOUR_COLOR);
  if (hour_color_tuple) {
    g_settings.hour_color = GColorFromHEX(hour_color_tuple->value->int32);
    dirty = true;
  }

  Tuple *minute_color_tuple = dict_find(iterator, MESSAGE_KEY_MINUTE_COLOR);
  if (minute_color_tuple) {
    g_settings.minute_color = GColorFromHEX(minute_color_tuple->value->int32);
    dirty = true;
  }

  Tuple *time_format_tuple = dict_find(iterator, MESSAGE_KEY_TIME_FORMAT);
  if (time_format_tuple) {
    // atoi returns 0 on error but that's fine because "0" is our default.
    g_settings.time_format =
        (TimeFormat)atoi(time_format_tuple->value->cstring);
    dirty = true;
  }

  Tuple *top_complication_tuple =
      dict_find(iterator, MESSAGE_KEY_TOP_COMPLICATION);
  if (top_complication_tuple) {
    // atoi returns 0 on error but that's fine because "0" is our default.
    g_settings.top_complication =
        (Complication)atoi(top_complication_tuple->value->cstring);
    dirty = true;
  }

  Tuple *bottom_complication_tuple =
      dict_find(iterator, MESSAGE_KEY_BOTTOM_COMPLICATION);
  if (bottom_complication_tuple) {
    g_settings.bottom_complication =
        (Complication)atoi(bottom_complication_tuple->value->cstring);
    dirty = true;
  }

  Tuple *temperature_unit_tuple =
      dict_find(iterator, MESSAGE_KEY_TEMPERATURE_UNIT);
  if (temperature_unit_tuple) {
    g_settings.temperature_unit =
        (TemperatureUnit)atoi(temperature_unit_tuple->value->cstring);
    dirty = true;
  }

  Tuple *intro_animation_tuple =
      dict_find(iterator, MESSAGE_KEY_INTRO_ANIMATION);
  if (intro_animation_tuple) {
    g_settings.intro_animation = intro_animation_tuple->value->int32 == 1;
    dirty = true;
  }

  Tuple *tick_animation_tuple = dict_find(iterator, MESSAGE_KEY_TICK_ANIMATION);
  if (tick_animation_tuple) {
    g_settings.tick_animation = tick_animation_tuple->value->int32 == 1;
    dirty = true;
  }

  Tuple *wday_color = dict_find(iterator, MESSAGE_KEY_WDAY_COLOR);
  if (wday_color) {
    g_settings.wday_color = GColorFromHEX(wday_color->value->int32);
    dirty = true;
  }

  Tuple *mday_color_tuple = dict_find(iterator, MESSAGE_KEY_MDAY_COLOR);
  if (mday_color_tuple) {
    g_settings.mday_color = GColorFromHEX(mday_color_tuple->value->int32);
    dirty = true;
  }

  Tuple *bw_stripe_style_tuple =
      dict_find(iterator, MESSAGE_KEY_BW_STRIPE_STYLE);
  if (bw_stripe_style_tuple) {
    // atoi returns 0 on error but that's fine because "0" is our default.
    g_settings.bw_stripe_style =
        (BWStripeStyle)atoi(bw_stripe_style_tuple->value->cstring);
    dirty = true;
  }

  return dirty;
#endif
}
