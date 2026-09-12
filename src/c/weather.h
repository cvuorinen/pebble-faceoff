#ifndef FACEOFF_WEATHER_H
#define FACEOFF_WEATHER_H

#include <pebble.h>
#include <time.h>

// Kept in step with CONDITIONS in src/pkjs/weather.js, which is what turns the
// WMO code the forecast comes back with into one of these. Only the ones there
// is an icon for; everything else maps onto its nearest neighbour.
typedef enum {
  WEATHER_CLEAR_DAY = 0,
  WEATHER_CLEAR_NIGHT = 1,
  WEATHER_PARTLY_CLOUDY_DAY = 2,
  WEATHER_PARTLY_CLOUDY_NIGHT = 3,
  WEATHER_CLOUDY = 4,
  WEATHER_RAIN = 5,
  WEATHER_SNOW = 6,
  WEATHER_FOG = 7,
  WEATHER_THUNDERSTORM = 8,
  WEATHER_UNKNOWN = 9,
} WeatherCondition;

void weather_init();

// True when the reading changed and the face needs redrawing.
bool weather_update(DictionaryIterator *iterator);

// Asks the phone for a reading if the one we have has aged out. Called once a
// minute off the tick; it decides for itself whether that means anything.
void weather_refresh_if_due();

// Asks unconditionally. For a fresh launch, or after a settings change that
// needs a reading sooner than the next refresh would bring one.
void weather_refresh();

// One character of RESOURCE_ID_WEATHERFONT; see src/c/weather_icons.h.
const char *weather_icon();

// "21°", or "--°" when there is nothing worth showing yet.
void weather_temperature_string(char *buffer, size_t size);

#endif
