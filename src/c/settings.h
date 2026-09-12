#ifndef FACEOFF_SETTINGS_H
#define FACEOFF_SETTINGS_H

#include <pebble.h>

#define SETTINGS_KEY 5

typedef enum {
  TIME_FORMAT_SYSTEM = 0,
  TIME_FORMAT_12H = 1,
  TIME_FORMAT_24H = 2,
} TimeFormat;

// Black and white watches get no palette at all; the only choice is which end
// of the face carries the dark stripe. The other stripe is light, and the text
// on each stripe is the inverse of it.
typedef enum {
  BW_STRIPES_DARK_TOP = 0,
  BW_STRIPES_LIGHT_TOP = 1,
} BWStripeStyle;

typedef struct Settings {
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
  BWStripeStyle bw_stripe_style;
} Settings;

extern Settings g_settings;

void default_settings();
bool update_settings(DictionaryIterator *iterator, void *context);
bool migrate_settings();

#endif
