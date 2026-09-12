#include "weather.h"
#include "message_keys.auto.h"
#include "settings.h"
#include "icons.h"

#define WEATHER_KEY 100

// How long a reading stays good for, and how long to wait before asking again
// after one that did not arrive. The phone only answers while the watchface is
// on screen, so a request that goes unanswered is normal rather than an error.
#define WEATHER_REFRESH_SECONDS (30 * 60)
#define WEATHER_RETRY_SECONDS (5 * 60)
// Past this the reading is no longer worth showing at all.
#define WEATHER_STALE_SECONDS (4 * 60 * 60)

typedef struct Weather {
  // Tenths of a degree Celsius. The phone always sends Celsius; converting for
  // display here means switching units does not need another fetch.
  int16_t temperature_dc;
  uint8_t condition;
  time_t updated_at;
} Weather;

static Weather s_weather;
static time_t s_requested_at;

static const char *s_icons[] = {
    [WEATHER_CLEAR_DAY] = ICON_DAY_SUNNY,
    [WEATHER_CLEAR_NIGHT] = ICON_NIGHT_CLEAR,
    [WEATHER_PARTLY_CLOUDY_DAY] = ICON_DAY_CLOUDY,
    [WEATHER_PARTLY_CLOUDY_NIGHT] = ICON_NIGHT_ALT_CLOUDY,
    [WEATHER_CLOUDY] = ICON_CLOUD,
    [WEATHER_RAIN] = ICON_RAIN,
    [WEATHER_SNOW] = ICON_SNOW,
    [WEATHER_FOG] = ICON_FOG,
    [WEATHER_THUNDERSTORM] = ICON_THUNDERSTORM,
    [WEATHER_UNKNOWN] = ICON_NA,
};

void weather_init() {
  s_weather = (Weather){.condition = WEATHER_UNKNOWN};
  persist_read_data(WEATHER_KEY, &s_weather, sizeof(s_weather));
}

static bool prv_have_reading() {
  return s_weather.updated_at != 0 &&
         time(NULL) - s_weather.updated_at < WEATHER_STALE_SECONDS;
}

bool weather_update(DictionaryIterator *iterator) {
  Tuple *condition_tuple = dict_find(iterator, MESSAGE_KEY_WEATHER_CONDITION);
  Tuple *temperature_tuple =
      dict_find(iterator, MESSAGE_KEY_WEATHER_TEMPERATURE);
  if (!condition_tuple || !temperature_tuple) {
    return false;
  }

  int32_t condition = condition_tuple->value->int32;
  if (condition < 0 || condition > WEATHER_UNKNOWN) {
    condition = WEATHER_UNKNOWN;
  }

  s_weather.condition = condition;
  s_weather.temperature_dc = temperature_tuple->value->int32;
  s_weather.updated_at = time(NULL);
  persist_write_data(WEATHER_KEY, &s_weather, sizeof(s_weather));

  return true;
}

void weather_refresh() {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) {
    return;
  }
  dict_write_uint8(out, MESSAGE_KEY_WEATHER_REQUEST, 1);
  if (app_message_outbox_send() == APP_MSG_OK) {
    s_requested_at = time(NULL);
  }
}

void weather_refresh_if_due() {
  time_t now = time(NULL);
  time_t age = now - s_weather.updated_at;
  if (s_weather.updated_at != 0 && age < WEATHER_REFRESH_SECONDS) {
    return;
  }
  // Do not pile requests up on a phone that is not answering.
  if (s_requested_at != 0 && now - s_requested_at < WEATHER_RETRY_SECONDS) {
    return;
  }
  weather_refresh();
}

const char *weather_icon() {
  if (!prv_have_reading()) {
    return ICON_NA;
  }
  return s_icons[s_weather.condition];
}

void weather_temperature_string(char *buffer, size_t size) {
  if (!prv_have_reading()) {
    snprintf(buffer, size, "--°");
    return;
  }

  int32_t tenths = s_weather.temperature_dc;
  if (g_settings.temperature_unit == TEMPERATURE_UNIT_FAHRENHEIT) {
    tenths = tenths * 9 / 5 + 320;
  }

  // Round to whole degrees, away from zero so -0.6 reads as -1 and not 0.
  int32_t degrees = (tenths >= 0 ? tenths + 5 : tenths - 5) / 10;
  snprintf(buffer, size, "%d°", (int)degrees);
}
