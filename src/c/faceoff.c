#include "settings.h"
#include "weather.h"
#include <pebble-fctx/fctx.h>
#include <pebble-fctx/ffont.h>
#include <pebble.h>
#include <time.h>

#define TEXT_ANGLE_DEGREES (-7)
#define TEXT_ANGLE (TEXT_ANGLE_DEGREES * TRIG_MAX_ANGLE / 360)

static Window *s_window;
static FFont *s_font;
static FFont *s_weather_font;
static Layer *s_time_layer;
static Layer *s_background_layer;

static const char *s_wdays[] = {"SUN", "MON", "TUE", "WED",
                                "THU", "FRI", "SAT"};

static const char *s_months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                 "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

// On a one bit screen there is no palette to configure: one stripe is black,
// the other white, and everything drawn on a stripe is the inverse of it.
static GColor prv_stripe_color(bool top) {
#ifdef PBL_BW
  bool dark = (g_settings.bw_stripe_style == BW_STRIPES_DARK_TOP) == top;
  return dark ? GColorBlack : GColorWhite;
#else
  return top ? g_settings.top_stripe_color : g_settings.bottom_stripe_color;
#endif
}

// The uncovered corners are a dithered gray on BW, so the window colour
// underneath them only ever shows through as the tone the dither starts from.
static GColor prv_background_color() {
  return PBL_IF_BW_ELSE(GColorBlack, g_settings.background_color);
}

static GColor prv_ink_color(bool top, GColor configured) {
#ifdef PBL_BW
  return gcolor_equal(prv_stripe_color(top), GColorBlack) ? GColorWhite
                                                          : GColorBlack;
#else
  (void)top;
  return configured;
#endif
}

static int32_t prv_f_time_font_height(GRect bounds) {
  return INT_TO_FIXED(bounds.size.h / 3);
}

// A one bit screen has no antialiasing to hold a condensed face together, so
// the date is set taller there -- at a tenth of the screen its counters close
// up into slits and "09" stops reading as a number.
static int32_t prv_f_date_font_height(GRect bounds) {
  return INT_TO_FIXED(bounds.size.h / PBL_IF_BW_ELSE(9, 10));
}

static int32_t prv_f_time_font_radius(GRect bounds) {
  return INT_TO_FIXED(bounds.size.h) / 3 - INT_TO_FIXED(bounds.size.h) / 9;
}

// Keeps the date and weekday clear of the line where the stripes meet.
static int32_t prv_f_date_font_padding(GRect bounds) {
  return prv_f_date_font_height(bounds) / 6;
}

static int32_t prv_f_date_font_radius(GRect bounds) {
  return prv_f_time_font_radius(bounds) - prv_f_time_font_height(bounds) / 2 +
         prv_f_date_font_height(bounds) / 2 + prv_f_date_font_padding(bounds);
}

// How far the hour and minute are nudged apart along the slant.
static int32_t prv_f_time_font_offset(GRect bounds) {
  return prv_f_time_font_height(bounds) / 6;
}

// Icons carry less detail than a glyph of the same height, so they are set a
// little larger than the text they sit under.
static int32_t prv_f_weather_icon_height(GRect bounds) {
  return prv_f_date_font_height(bounds) * 4 / 3;
}

static int32_t prv_f_date_line_height(GRect bounds) {
  return prv_f_date_font_height(bounds) * 4 / 3;
}

// Tightened on BW to pay for the taller date, which would otherwise push the
// weekday off the right edge.
static int32_t prv_f_date_font_gap(GRect bounds) {
  return prv_f_date_font_height(bounds) / PBL_IF_BW_ELSE(3, 2);
}

// How far a slanted line drops over a horizontal run of f_run.
static int32_t prv_f_slant_rise(int32_t f_run) {
  return -f_run * sin_lookup(TEXT_ANGLE) / cos_lookup(TEXT_ANGLE);
}

// Thickness of a stripe, measured perpendicular to the slant, so that it
// always covers the time digits regardless of the slant angle.
static int32_t prv_f_stripe_thickness(GRect bounds) {
  return prv_f_time_font_radius(bounds) +
         prv_f_time_font_height(bounds) * 3 / 4;
}

static int32_t prv_f_stripe_vertical_thickness(GRect bounds) {
  return prv_f_stripe_thickness(bounds) * TRIG_MAX_RATIO /
         cos_lookup(TEXT_ANGLE);
}

// Positions a point f_radius perpendicular to the slant through the center of
// the screen, then f_offset along it.
static FPoint prv_f_slant_point(GRect bounds, int32_t f_radius,
                                int32_t f_offset) {
  FPoint f_center =
      FPoint(INT_TO_FIXED(bounds.size.w / 2), INT_TO_FIXED(bounds.size.h / 2));

  return FPoint(f_center.x + (sin_lookup(TEXT_ANGLE) * f_radius +
                              cos_lookup(TEXT_ANGLE) * f_offset) /
                                 TRIG_MAX_RATIO,
                f_center.y + (sin_lookup(TEXT_ANGLE) * f_offset -
                              cos_lookup(TEXT_ANGLE) * f_radius) /
                                 TRIG_MAX_RATIO);
}

static void prv_f_draw_text(FContext *fctx, FPoint f_center, const char *text,
                            FFont *font, GColor color,
                            GTextAlignment alignment) {
  fctx_set_rotation(fctx, TEXT_ANGLE);

  fctx_begin_fill(fctx);
  fctx_set_offset(fctx, f_center);
  fctx_set_fill_color(fctx, color);
  fctx_draw_string(fctx, text, font, alignment, FTextAnchorCapMiddle);
  fctx_end_fill(fctx);
}

static void prv_draw_time(Layer *layer, GContext *ctx, tm *time) {
  FContext fctx;
  GRect bounds = layer_get_unobstructed_bounds(layer);
  fctx_init_context(&fctx, ctx);

  fctx_set_text_cap_height(&fctx, s_font,
                           FIXED_TO_INT(prv_f_time_font_height(bounds)));

  int32_t f_radius = prv_f_time_font_radius(bounds);
  int32_t f_offset = prv_f_time_font_offset(bounds);

  FPoint f_hour_center = prv_f_slant_point(bounds, f_radius, -f_offset);
  FPoint f_min_center = prv_f_slant_point(bounds, -f_radius, f_offset);

  static char s_hour_buffer[3];
  static char s_min_buffer[3];
  bool use_24h =
      g_settings.time_format == TIME_FORMAT_24H ||
      (g_settings.time_format == TIME_FORMAT_SYSTEM && clock_is_24h_style());
  strftime(s_hour_buffer, sizeof(s_hour_buffer), use_24h ? "%H" : "%I", time);
  strftime(s_min_buffer, sizeof(s_min_buffer), "%M", time);

  prv_f_draw_text(&fctx, f_hour_center, s_hour_buffer,
                  s_font, prv_ink_color(true, g_settings.hour_color),
                  GTextAlignmentCenter);
  prv_f_draw_text(&fctx, f_min_center, s_min_buffer,
                  s_font, prv_ink_color(false, g_settings.minute_color),
                  GTextAlignmentCenter);

  fctx_deinit_context(&fctx);
}

// How far the side columns sit off the centre: clear of the time digits, which
// are the widest thing they have to stay out of the way of.
static int32_t prv_f_side_column_offset(FContext *fctx, GRect bounds) {
  fctx_set_text_cap_height(fctx, s_font,
                           FIXED_TO_INT(prv_f_time_font_height(bounds)));
  int32_t f_time_half_width = fctx_string_width(fctx, "00", s_font) / 2;

  return f_time_half_width + prv_f_date_font_gap(bounds) -
         prv_f_time_font_offset(bounds);
}

#define COMPLICATION_MAX_LINES 3

// The first line sits innermost, tucked against the time digits, and the rest
// stack outwards from it. So a column reads downwards in the bottom half and
// upwards in the top one, which is what puts the narrowest line -- the day of
// the month, or the weather icon -- furthest from the centre. That is where a
// round screen has the least width to give, and where a wide reading like
// "-22°" would otherwise run off the edge.
static FPoint prv_f_complication_point(GRect bounds, int32_t f_offset, bool top,
                                       int line, int32_t f_line_height) {
  int32_t f_radius = prv_f_date_font_radius(bounds) + line * f_line_height;

  return prv_f_slant_point(bounds, top ? f_radius : -f_radius,
                           top ? f_offset : -f_offset);
}

// Three lines will not fit the corner at the size one or two do -- the outer
// one would climb past the top of the hour and out of the stripe. Most of what
// has to be given back is taken out of the leading rather than the type, since
// a one bit screen has no antialiasing and the glyphs are the part that stops
// reading first: an eighth off the cap height and a sixth off the line lands
// the top line level with the top of the hour on every platform.
static int32_t prv_f_complication_text_height(GRect bounds, int count) {
  int32_t f_height = prv_f_date_font_height(bounds);

  return count < COMPLICATION_MAX_LINES ? f_height : f_height * 7 / 8;
}

static int32_t prv_f_complication_line_height(GRect bounds, int count) {
  if (count < COMPLICATION_MAX_LINES) {
    return prv_f_date_line_height(bounds);
  }

  return prv_f_complication_text_height(bounds, count) * 7 / 6;
}

static void prv_draw_complication(Layer *layer, GContext *ctx, tm *time,
                                  bool top) {
  Complication complication =
      top ? g_settings.top_complication : g_settings.bottom_complication;
  if (complication == COMPLICATION_NONE) {
    return;
  }

  FContext fctx;
  GRect bounds = layer_get_unobstructed_bounds(layer);
  fctx_init_context(&fctx, ctx);

  static char s_mday_buffer[3];
  static char s_temperature_buffer[8];

  const char *lines[COMPLICATION_MAX_LINES] = {NULL};
  int count = 0;
  // Only ever the last line, and only for the weather.
  bool ends_with_icon = false;

  switch (complication) {
  case COMPLICATION_WEATHER:
    weather_temperature_string(s_temperature_buffer,
                               sizeof(s_temperature_buffer));
    lines[count++] = s_temperature_buffer;
    lines[count++] = weather_icon();
    ends_with_icon = true;
    break;
  case COMPLICATION_WEEKDAY_DATE:
    lines[count++] = s_wdays[time->tm_wday];
    // fall through
  case COMPLICATION_DATE:
    strftime(s_mday_buffer, sizeof(s_mday_buffer), "%d", time);
    lines[count++] = s_months[time->tm_mon];
    lines[count++] = s_mday_buffer;
    break;
  case COMPLICATION_WEEKDAY:
    lines[count++] = s_wdays[time->tm_wday];
    break;
  case COMPLICATION_NONE:
    break;
  }

  int32_t f_offset = prv_f_side_column_offset(&fctx, bounds);
  int32_t f_line_height = prv_f_complication_line_height(bounds, count);
  GColor color =
      prv_ink_color(top, top ? g_settings.wday_color : g_settings.mday_color);
  GTextAlignment alignment = top ? GTextAlignmentLeft : GTextAlignmentRight;

  fctx_set_text_cap_height(
      &fctx, s_font,
      FIXED_TO_INT(prv_f_complication_text_height(bounds, count)));

  for (int line = 0; line < count; line++) {
    FPoint f_point =
        prv_f_complication_point(bounds, f_offset, top, line, f_line_height);
    bool is_icon = ends_with_icon && line == count - 1;
    if (!is_icon) {
      prv_f_draw_text(&fctx, f_point, lines[line], s_font, color, alignment);
    } else if (s_weather_font) {
      fctx_set_text_cap_height(&fctx, s_weather_font,
                               FIXED_TO_INT(prv_f_weather_icon_height(bounds)));
      prv_f_draw_text(&fctx, f_point, lines[line], s_weather_font, color,
                      alignment);
    }
  }

  fctx_deinit_context(&fctx);
}

static void prv_draw_background_stripe(Layer *layer, GContext *ctx,
                                       GColor color, bool flip) {
  FContext fctx;
  GRect bounds = layer_get_unobstructed_bounds(layer);
  fctx_init_context(&fctx, ctx);

  FPoint f_center =
      FPoint(INT_TO_FIXED(bounds.size.w / 2), INT_TO_FIXED(bounds.size.h / 2));

  FPoint f_bounds =
      FPoint(INT_TO_FIXED(bounds.size.w), INT_TO_FIXED(bounds.size.h));

  int32_t f_stripe_rise = prv_f_slant_rise(f_bounds.x);

  int32_t f_height_offset = -prv_f_stripe_vertical_thickness(bounds);
  // Hackfix: there is some minor blending that causes a black stripe between
  // two adjacent stripes I've "found that 1/4 of a pixel is enough to hide
  // this.
  int32_t f_aa_hackfix_offset = FIXED_POINT_SCALE / 4;
  if (flip) {
    f_height_offset *= -1;
    f_aa_hackfix_offset *= -1;
  }

  FPoint lower_left_point = FPoint(
      0, f_center.y + prv_f_slant_rise(f_center.x) + f_aa_hackfix_offset);

  FPoint upper_left_point =
      FPoint(0, lower_left_point.y + f_height_offset + f_aa_hackfix_offset);

  FPoint lower_right_point =
      FPoint(f_bounds.x, lower_left_point.y - f_stripe_rise);

  FPoint upper_right_point =
      FPoint(f_bounds.x, lower_right_point.y + f_height_offset);

  if (g_settings.fill_corners) {
    if (flip) {
      upper_right_point =
          FPoint(f_bounds.x, f_bounds.y + abs(upper_right_point.y));
    } else {
      upper_left_point = FPoint(0, upper_right_point.y);
    }
  }

  FPoint points[] = {upper_left_point, lower_left_point, lower_right_point,
                     upper_right_point};

  fctx_begin_fill(&fctx);
  fctx_set_fill_color(&fctx, color);
  fctx_draw_path(&fctx, points, sizeof(points) / sizeof(FPoint));
  fctx_end_fill(&fctx);

  fctx_deinit_context(&fctx);
}

#ifdef PBL_BW
// The uncovered corners have to sit apart from both stripes, and a one bit
// screen has no third tone -- so a checkerboard stands in for one. Written
// straight into the frame buffer because fctx has no pattern fill.
static void prv_fill_dithered_gray(GContext *ctx) {
  GBitmap *frame_buffer =
      graphics_capture_frame_buffer_format(ctx, GBitmapFormat1Bit);
  if (!frame_buffer) {
    return;
  }

  GRect fb_bounds = gbitmap_get_bounds(frame_buffer);
  for (int y = fb_bounds.origin.y; y < fb_bounds.origin.y + fb_bounds.size.h;
       y++) {
    GBitmapDataRowInfo row = gbitmap_get_data_row_info(frame_buffer, y);
    for (int x = row.min_x; x <= row.max_x; x++) {
      uint8_t mask = 1 << (x % 8);
      if ((x + y) % 2 == 0) {
        row.data[x / 8] |= mask;
      } else {
        row.data[x / 8] &= ~mask;
      }
    }
  }

  graphics_release_frame_buffer(ctx, frame_buffer);
}
#endif

static void prv_draw_background_layer(Layer *layer, GContext *ctx) {
#ifdef PBL_BW
  prv_fill_dithered_gray(ctx);
#endif
  prv_draw_background_stripe(layer, ctx, prv_stripe_color(true), false);
  prv_draw_background_stripe(layer, ctx, prv_stripe_color(false), true);
}

static void prv_draw_time_layer(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *time = localtime(&now);

  prv_draw_time(layer, ctx, time);
  prv_draw_complication(layer, ctx, time, true);
  prv_draw_complication(layer, ctx, time, false);
}

static void prv_tick_handler(tm *_tick_time, TimeUnits _units_changed) {
  if (settings_want_weather()) {
    weather_refresh_if_due();
  }
  layer_mark_dirty(s_time_layer);
}

static void prv_invalidate_layers() {
  window_set_background_color(s_window, prv_background_color());
  layer_mark_dirty(s_time_layer);
  layer_mark_dirty(s_background_layer);
}

static void prv_window_load(Window *window) {
  layer_set_update_proc(s_time_layer, prv_draw_time_layer);
  layer_set_update_proc(s_background_layer, prv_draw_background_layer);
}

static void prv_window_unload(Window *window) {
  layer_destroy(s_time_layer);
  layer_destroy(s_background_layer);
  ffont_destroy(s_font);
  if (s_weather_font) {
    ffont_destroy(s_weather_font);
  }
}

// The icon font is a third of aplite's free heap, so it is only resident while
// a complication is actually drawing icons out of it.
static void prv_sync_weather_font() {
  bool wanted = settings_want_weather();
  if (wanted && !s_weather_font) {
    s_weather_font = ffont_create_from_resource(RESOURCE_ID_WEATHERFONT);
  } else if (!wanted && s_weather_font) {
    ffont_destroy(s_weather_font);
    s_weather_font = NULL;
  }
}

static void prv_save_settings() {
  persist_write_data(SETTINGS_KEY, &g_settings, sizeof(g_settings));
}

static void prv_load_settings() {
  default_settings();
  if (!migrate_settings()) {
    persist_read_data(SETTINGS_KEY, &g_settings, sizeof(g_settings));
  }
}

static void prv_inbox_received_callback(DictionaryIterator *iterator,
                                        void *context) {
  bool weather_dirty = weather_update(iterator);

  bool wanted_weather = settings_want_weather();
  bool settings_dirty = update_settings(iterator, context);
  if (settings_dirty) {
    prv_save_settings();
    prv_sync_weather_font();
    // Turning a weather complication on should not leave it blank until the
    // next refresh comes round.
    if (!wanted_weather && settings_want_weather()) {
      weather_refresh();
    }
  }

  if (settings_dirty || weather_dirty) {
    prv_invalidate_layers();
  }
}

static void
prv_unobstructed_will_change_callback(GRect final_unobstructed_screen_area,
                                      void *context) {}

static void prv_unobstructed_change_callback(AnimationProgress progress,
                                             void *context) {
  prv_invalidate_layers();
}

static void prv_unobstructed_did_change_callback(void *context) {
  prv_invalidate_layers();
}

static void prv_init(void) {
  prv_load_settings();
  weather_init();

  app_message_register_inbox_received(prv_inbox_received_callback);

  app_message_open(256, 64);

  s_window = window_create();
  window_set_window_handlers(s_window, (WindowHandlers){
                                           .load = prv_window_load,
                                           .unload = prv_window_unload,
                                       });

  s_font = ffont_create_from_resource(RESOURCE_ID_TIMEFONT);
  prv_sync_weather_font();

  Layer *window_layer = window_get_root_layer(s_window);
  GRect bounds = layer_get_bounds(window_layer);

  s_background_layer = layer_create(bounds);
  layer_add_child(window_layer, s_background_layer);

  s_time_layer = layer_create(bounds);
  layer_add_child(window_layer, s_time_layer);

  window_set_background_color(s_window, prv_background_color());

  // Aplite compiles the subscription away, which leaves this unreferenced.
  UnobstructedAreaHandlers handlers __attribute__((unused)) = {
      .will_change = prv_unobstructed_will_change_callback,
      .change = prv_unobstructed_change_callback,
      .did_change = prv_unobstructed_did_change_callback};
  unobstructed_area_service_subscribe(handlers, NULL);

  tick_timer_service_subscribe(MINUTE_UNIT, prv_tick_handler);

  if (settings_want_weather()) {
    weather_refresh();
  }

  const bool animated = true;
  window_stack_push(s_window, animated);
}

static void prv_deinit(void) { window_destroy(s_window); }

int main(void) {
  prv_init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p",
          s_window);

  app_event_loop();
  prv_deinit();
}
