#include "settings.h"
#include "gcolor_definitions.h"
#include "message_keys.auto.h"
#include "pebble.h"

#define SETTINGS_KEY_V4 4
#define SETTINGS_KEY_V3 3
#define SETTINGS_KEY_V2 2

typedef struct SettingsV4 {
  GColor top_stripe_color;
  GColor bottom_stripe_color;
  GColor background_color;
  bool fill_corners;
  GColor hour_color;
  GColor minute_color;
  TimeFormat time_format;
  bool show_date;
  GColor wday_color;
  GColor mday_color;
} SettingsV4;

typedef struct SettingsV3 {
  GColor top_stripe_color;
  GColor bottom_stripe_color;
  GColor background_color;
  bool fill_corners;
  GColor hour_color;
  GColor minute_color;
  bool show_24h_time;
  bool show_date;
  GColor wday_color;
  GColor mday_color;
} SettingsV3;

typedef struct SettingsV2 {
  GColor top_stripe_color;
  GColor bottom_stripe_color;
  GColor background_color;
  bool fill_corners;
  GColor hour_color;
  GColor minute_color;
  bool show_date;
  GColor wday_color;
  GColor mday_color;
} SettingsV2;

Settings g_settings;

void default_settings() {
  g_settings.top_stripe_color = GColorJazzberryJam;
  g_settings.bottom_stripe_color = GColorVeryLightBlue;
  g_settings.background_color = GColorBlack;
  g_settings.fill_corners = false;
  g_settings.hour_color = GColorWhite;
  g_settings.minute_color = GColorWhite;
  g_settings.time_format = TIME_FORMAT_SYSTEM;
  g_settings.show_date = true;
  g_settings.wday_color = GColorWhite;
  g_settings.mday_color = GColorWhite;
  g_settings.bw_stripe_style = BW_STRIPES_DARK_TOP;
}

bool update_settings(DictionaryIterator *iterator, void *context) {
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

  Tuple *show_date_tuple = dict_find(iterator, MESSAGE_KEY_SHOW_DATE);
  if (show_date_tuple) {
    g_settings.show_date = show_date_tuple->value->int32 == 1;
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
}

static void prv_from_v4_settings(SettingsV4 v4) {
  g_settings.top_stripe_color = v4.top_stripe_color;
  g_settings.bottom_stripe_color = v4.bottom_stripe_color;
  g_settings.background_color = v4.background_color;
  g_settings.fill_corners = v4.fill_corners;
  g_settings.hour_color = v4.hour_color;
  g_settings.minute_color = v4.minute_color;
  g_settings.time_format = v4.time_format;
  g_settings.show_date = v4.show_date;
  g_settings.wday_color = v4.wday_color;
  g_settings.mday_color = v4.mday_color;
  g_settings.bw_stripe_style = BW_STRIPES_DARK_TOP;
}

static void prv_from_v2_settings(SettingsV2 v2) {
  g_settings.top_stripe_color = v2.top_stripe_color;
  g_settings.bottom_stripe_color = v2.bottom_stripe_color;
  g_settings.background_color = v2.background_color;
  g_settings.fill_corners = v2.fill_corners;
  g_settings.hour_color = v2.hour_color;
  g_settings.minute_color = v2.minute_color;
  g_settings.time_format = TIME_FORMAT_SYSTEM;
  g_settings.show_date = v2.show_date;
  g_settings.wday_color = v2.wday_color;
  g_settings.mday_color = v2.mday_color;
  g_settings.bw_stripe_style = BW_STRIPES_DARK_TOP;
}

static void prv_from_v3_settings(SettingsV3 v3) {
  g_settings.top_stripe_color = v3.top_stripe_color;
  g_settings.bottom_stripe_color = v3.bottom_stripe_color;
  g_settings.background_color = v3.background_color;
  g_settings.fill_corners = v3.fill_corners;
  g_settings.hour_color = v3.hour_color;
  g_settings.minute_color = v3.minute_color;
  g_settings.time_format =
      v3.show_24h_time ? TIME_FORMAT_24H : TIME_FORMAT_SYSTEM;
  g_settings.show_date = v3.show_date;
  g_settings.wday_color = v3.wday_color;
  g_settings.mday_color = v3.mday_color;
  g_settings.bw_stripe_style = BW_STRIPES_DARK_TOP;
}

bool migrate_settings() {
  SettingsV4 settings_v4;
  if (persist_read_data(SETTINGS_KEY_V4, &settings_v4, sizeof(settings_v4)) !=
      E_DOES_NOT_EXIST) {
    prv_from_v4_settings(settings_v4);
    persist_write_data(SETTINGS_KEY, &g_settings, sizeof(g_settings));
    persist_delete(SETTINGS_KEY_V4);
    persist_delete(SETTINGS_KEY_V3);
    persist_delete(SETTINGS_KEY_V2);
    return true;
  }

  SettingsV3 settings_v3;
  if (persist_read_data(SETTINGS_KEY_V3, &settings_v3, sizeof(settings_v3)) !=
      E_DOES_NOT_EXIST) {
    prv_from_v3_settings(settings_v3);
    persist_write_data(SETTINGS_KEY, &g_settings, sizeof(g_settings));
    persist_delete(SETTINGS_KEY_V3);
    persist_delete(SETTINGS_KEY_V2);
    return true;
  }

  SettingsV2 settings_v2;
  if (persist_read_data(SETTINGS_KEY_V2, &settings_v2, sizeof(settings_v2)) !=
      E_DOES_NOT_EXIST) {
    prv_from_v2_settings(settings_v2);
    persist_write_data(SETTINGS_KEY, &g_settings, sizeof(g_settings));
    persist_delete(SETTINGS_KEY_V2);
    return true;
  }

  return false;
}
