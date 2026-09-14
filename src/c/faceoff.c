#include "health.h"
#include "icons.h"
#include "settings.h"
#include "weather.h"
#include <pebble-fctx/fctx.h>
#include <pebble-fctx/ffont.h>
#include <pebble.h>
#include <time.h>

#ifdef SHOT_CONFIG
#include "shot_config.h"
#endif

#define TEXT_ANGLE_DEGREES (-7)
#define TEXT_ANGLE (TEXT_ANGLE_DEGREES * TRIG_MAX_ANGLE / 360)

// The face arrives rather than appearing, the way the two banners of a fighting
// game's versus screen do: the top half slides in from the left, a beat at a
// time -- the complication's rows from the screen edge inwards, then the two
// digits behind them in quick succession -- and once it has landed the bottom
// half comes in the same way from the right.
//
// The same beats turn the face over when the time moves on: the reading being
// left behind flies out the way its half was already travelling, and once it is
// gone the new one flies in from where the intro brings it.
#define ANIM_ROW_STAGGER_MS 110
#define ANIM_DIGIT_STAGGER_MS 70
// How long one element takes to fly in. Longer than the gap between beats, so
// the elements of a half overlap in the air rather than queueing.
#define ANIM_TRAVEL_MS 300
// The pause between the top half landing and the bottom half setting off: the
// beat that makes it two entrances rather than one gesture. Negative overlaps
// them instead.
#define ANIM_HALF_HOLD_MS (-150)
// The pause between the outgoing reading having left and the incoming one
// setting off. Never negative: both passes are the same elements carrying
// different readings, so they must not be on screen together.
#define ANIM_CHANGE_HOLD_MS 80
// The unit an eased beat is measured in. A power of two, since the easing
// squares and cubes it.
#define ANIM_SCALE 1024
// strftime gives %H, %I and %M two characters whatever the hour.
#define TIME_DIGIT_COUNT 2

// Whether an animation is the face arriving or the face turning over. A change
// has an outgoing reading to clear away first; an intro finds the screen empty.
typedef enum {
  ANIM_KIND_INTRO,
  ANIM_KIND_CHANGE,
} AnimKind;

static Window *s_window;
static FFont *s_font;
static FFont *s_icon_font;
static Layer *s_time_layer;
static Layer *s_background_layer;
static Animation *s_animation;
static AnimationProgress s_anim_progress;
static uint32_t s_anim_duration_ms;
static bool s_anim_running;
static AnimKind s_anim_kind;
// Which halves the running animation moves. An intro brings the whole face in;
// a minute turns over the bottom half alone, the hour being where it was.
static bool s_anim_top;
static bool s_anim_bottom;
// What the face was showing before a change animation set off, drawn until the
// last of it has left the screen.
static tm s_outgoing_time;

static const char *s_wdays[] = {"SUN", "MON", "TUE", "WED",
                                "THU", "FRI", "SAT"};

static const char *s_months[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                 "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

// The clock the face draws. A screenshot build draws the one scripts/shots.py
// pinned instead of the watch's own, so the same shot taken a year from now
// comes out identical.
static tm *prv_now() {
#ifdef SHOT_CONFIG
  static tm s_shot_time = SHOT_TM;
  return &s_shot_time;
#else
  time_t now = time(NULL);
  return localtime(&now);
#endif
}

// What the watch's own 12/24h setting says, which only matters while the face
// is set to follow it.
static bool prv_is_24h_style() {
#ifdef SHOT_CONFIG
  return SHOT_24H;
#else
  return clock_is_24h_style();
#endif
}

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

// Being the taller of the two, an icon all but fills the line it is given, and
// lands against the reading above it. This pushes it further out to open a gap.
static int32_t prv_f_icon_gap(GRect bounds) {
  return prv_f_date_font_height(bounds) / 3;
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

// A point f_offset further along the slant. Moving along the text's own
// baseline rather than straight across the screen is what keeps an element
// parallel to the stripe it belongs to as it slides.
static FPoint prv_f_slide(FPoint f_point, int32_t f_offset) {
  return FPoint(f_point.x + cos_lookup(TEXT_ANGLE) * f_offset / TRIG_MAX_RATIO,
                f_point.y + sin_lookup(TEXT_ANGLE) * f_offset / TRIG_MAX_RATIO);
}

// How many lines a complication puts on screen, and so how many beats its
// column takes to arrive. Not the same as the number of lines it lays out: the
// icon line is only drawn where there is a font for it, and a beat spent on a
// line nobody sees would read as a stumble in the sequence.
static int prv_complication_entry_count(Complication complication) {
  switch (complication) {
  case COMPLICATION_NONE:
    return 0;
  case COMPLICATION_WEEKDAY:
    return 1;
  case COMPLICATION_DATE:
    return 2;
  case COMPLICATION_WEEKDAY_DATE:
    return 3;
  case COMPLICATION_WEATHER:
  case COMPLICATION_STEPS:
  case COMPLICATION_HEART_RATE:
    return s_icon_font ? 2 : 1;
  }

  return 0;
}

static bool prv_anim_moves_half(bool top) {
  return top ? s_anim_top : s_anim_bottom;
}

static int prv_anim_half_rows(bool top) {
  return prv_complication_entry_count(top ? g_settings.top_complication
                                          : g_settings.bottom_complication);
}

// When a beat starts, in milliseconds from the top of its own pass. The
// complication's rows come first at their own pace, the digits behind them at
// a quicker one.
static uint32_t prv_anim_beat_start_ms(int rows, int beat) {
  if (beat < rows) {
    return beat * ANIM_ROW_STAGGER_MS;
  }

  return rows * ANIM_ROW_STAGGER_MS + (beat - rows) * ANIM_DIGIT_STAGGER_MS;
}

// How long a half takes from its own first beat to its last one landing.
static uint32_t prv_anim_half_length_ms(int rows) {
  return prv_anim_beat_start_ms(rows, rows + TIME_DIGIT_COUNT - 1) +
         ANIM_TRAVEL_MS;
}

// When a half sets off. The top goes first; the bottom waits for it to land,
// which is what makes the two read as fighter and challenger rather than as
// one face sliding apart. A half with nothing to wait for does not wait, which
// is how a minute change -- the bottom on its own -- starts straight away.
static uint32_t prv_anim_half_start_ms(bool top) {
  if (top || !prv_anim_moves_half(true)) {
    return 0;
  }

  int32_t start = (int32_t)prv_anim_half_length_ms(prv_anim_half_rows(true)) +
                  ANIM_HALF_HOLD_MS;

  return start < 0 ? 0 : (uint32_t)start;
}

// How long one pass runs: until the later of the two halves has landed. The
// bottom is normally that, but a hold short enough to overlap the halves can
// leave a long top column still arriving after a bare bottom one has finished.
static uint32_t prv_anim_pass_length_ms() {
  uint32_t length = 0;

  for (int half = 0; half < 2; half++) {
    bool top = half == 0;
    if (!prv_anim_moves_half(top)) {
      continue;
    }

    uint32_t end = prv_anim_half_start_ms(top) +
                   prv_anim_half_length_ms(prv_anim_half_rows(top));
    if (end > length) {
      length = end;
    }
  }

  return length;
}

// When the incoming face sets off: after the outgoing one has been cleared
// away, or at once when there was nothing to clear.
static uint32_t prv_anim_entry_start_ms() {
  return s_anim_kind == ANIM_KIND_CHANGE
             ? prv_anim_pass_length_ms() + ANIM_CHANGE_HOLD_MS
             : 0;
}

#ifndef SHOT_CONFIG
static uint32_t prv_anim_duration_ms() {
  return prv_anim_entry_start_ms() + prv_anim_pass_length_ms();
}
#endif

// Where the running animation has got to, from its own top. Safe in 32 bits for
// anything under about half a minute, which is a good deal longer than an
// animation worth watching.
static int32_t prv_anim_elapsed_ms() {
  return (int32_t)s_anim_progress * (int32_t)s_anim_duration_ms /
         ANIMATION_NORMALIZED_MAX;
}

// How far along a travel that set off at start_ms an element is: 0 before it
// starts, ANIM_SCALE once it has arrived. Eased so an element decelerates into
// place rather than stopping dead against it.
static int32_t prv_anim_travel_progress(int32_t elapsed_ms, int32_t start_ms) {
  int32_t travelled_ms = elapsed_ms - start_ms;

  if (travelled_ms <= 0) {
    return 0;
  }
  if (travelled_ms >= ANIM_TRAVEL_MS) {
    return ANIM_SCALE;
  }

  // Cubic ease out: away quickly, settling in slowly.
  int32_t left = ANIM_SCALE - travelled_ms * ANIM_SCALE / ANIM_TRAVEL_MS;
  return ANIM_SCALE - left * left / ANIM_SCALE * left / ANIM_SCALE;
}

// How far out along the slant an element still is, as an offset to add to
// wherever it finally sits. A whole screen width, so it is clear of the edge
// whatever its own width.
//
// The sign is what makes the two halves mirror each other: an arrival comes in
// from the left up top and from the right below. A departure carries on the way
// its half was already travelling rather than backing out of the way it came,
// so a change reads as a reel turning over rather than as a face that thought
// better of itself. Between the two passes an element waits a screen away on
// the far side, which is where the jump from one sign to the other hides.
static int32_t prv_f_anim_offset(GRect bounds, bool top, int rows, int beat) {
  if (!s_anim_running || !prv_anim_moves_half(top)) {
    return 0;
  }

  int32_t f_width = INT_TO_FIXED(bounds.size.w);
  int32_t f_arrives_from = top ? -f_width : f_width;
  int32_t elapsed_ms = prv_anim_elapsed_ms();
  int32_t beat_ms = (int32_t)prv_anim_half_start_ms(top) +
                    (int32_t)prv_anim_beat_start_ms(rows, beat);

  if (s_anim_kind == ANIM_KIND_CHANGE) {
    int32_t leaving = prv_anim_travel_progress(elapsed_ms, beat_ms);
    if (leaving < ANIM_SCALE) {
      return -f_arrives_from * leaving / ANIM_SCALE;
    }
  }

  int32_t arriving = prv_anim_travel_progress(
      elapsed_ms, (int32_t)prv_anim_entry_start_ms() + beat_ms);

  return f_arrives_from * (ANIM_SCALE - arriving) / ANIM_SCALE;
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

// The digits of one number, drawn a glyph at a time so each can arrive on its
// own beat. They are laid out from the left edge of the whole string by each
// glyph's own advance, which is where fctx would have put them had it drawn the
// string in one go -- so a settled face is the same face it always was.
//
// The digit that breaks the edge of the screen first enters first, which is the
// trailing one: sliding in from the left, the right of the two is nearest the
// screen and arrives ahead of the one behind it, so the hour fills in ones then
// tens and the minute, coming the other way, tens then ones.
//
// That order also keeps the pair apart. Both digits cover the same ground on
// the same curve, so the one still to arrive is always further out than the one
// ahead of it; entering them the other way round would have the number closing
// up on itself in the air and springing open as it landed.
static void prv_draw_time_digits(FContext *fctx, GRect bounds,
                                 const char *digits, FPoint f_center,
                                 GColor color, bool top, int rows) {
  int count = strlen(digits);
  int32_t f_pen = -fctx_string_width(fctx, digits, s_font) / 2;

  for (int digit = 0; digit < count; digit++) {
    char glyph[2] = {digits[digit], '\0'};
    int beat = rows + (top ? count - 1 - digit : digit);
    int32_t f_anim = prv_f_anim_offset(bounds, top, rows, beat);

    prv_f_draw_text(fctx, prv_f_slide(f_center, f_pen + f_anim), glyph, s_font,
                    color, GTextAlignmentLeft);
    f_pen += fctx_string_width(fctx, glyph, s_font);
  }
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
      (g_settings.time_format == TIME_FORMAT_SYSTEM && prv_is_24h_style());
  strftime(s_hour_buffer, sizeof(s_hour_buffer), use_24h ? "%H" : "%I", time);
  strftime(s_min_buffer, sizeof(s_min_buffer), "%M", time);

  prv_draw_time_digits(&fctx, bounds, s_hour_buffer, f_hour_center,
                       prv_ink_color(true, g_settings.hour_color), true,
                       prv_complication_entry_count(g_settings.top_complication));
  prv_draw_time_digits(&fctx, bounds, s_min_buffer, f_min_center,
                       prv_ink_color(false, g_settings.minute_color), false,
                       prv_complication_entry_count(g_settings.bottom_complication));

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
                                       int line, int32_t f_line_height,
                                       int32_t f_extra, int32_t f_anim) {
  int32_t f_radius =
      prv_f_date_font_radius(bounds) + line * f_line_height + f_extra;

  return prv_f_slant_point(bounds, top ? f_radius : -f_radius,
                           (top ? f_offset : -f_offset) + f_anim);
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
  static char s_reading_buffer[8];

  const char *lines[COMPLICATION_MAX_LINES] = {NULL};
  int count = 0;
  // Only ever the last line, and only where a complication has one.
  bool ends_with_icon = false;

  switch (complication) {
  case COMPLICATION_WEATHER:
    weather_temperature_string(s_reading_buffer, sizeof(s_reading_buffer));
    lines[count++] = s_reading_buffer;
    lines[count++] = weather_icon();
    ends_with_icon = true;
    break;
  case COMPLICATION_STEPS:
    health_steps_string(s_reading_buffer, sizeof(s_reading_buffer));
    lines[count++] = s_reading_buffer;
    lines[count++] = ICON_STEPS;
    ends_with_icon = true;
    break;
  case COMPLICATION_HEART_RATE:
    health_heart_rate_string(s_reading_buffer, sizeof(s_reading_buffer));
    lines[count++] = s_reading_buffer;
    lines[count++] = ICON_FAVORITE;
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

  // The outermost line enters first and the sequence works inwards, so a
  // column arrives from the edge of the screen towards the time rather than
  // growing out of it. Both halves read the same way round, which is what makes
  // them mirror each other.
  int rows = prv_complication_entry_count(complication);

  for (int line = 0; line < count; line++) {
    bool is_icon = ends_with_icon && line == count - 1;
    if (is_icon && !s_icon_font) {
      continue;
    }

    FPoint f_point = prv_f_complication_point(
        bounds, f_offset, top, line, f_line_height,
        is_icon ? prv_f_icon_gap(bounds) : 0,
        prv_f_anim_offset(bounds, top, rows, rows - 1 - line));
    if (!is_icon) {
      prv_f_draw_text(&fctx, f_point, lines[line], s_font, color, alignment);
    } else {
      fctx_set_text_cap_height(&fctx, s_icon_font,
                               FIXED_TO_INT(prv_f_weather_icon_height(bounds)));
      prv_f_draw_text(&fctx, f_point, lines[line], s_icon_font, color,
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

// A change animation draws the reading the face is leaving behind until the
// last of it is off the screen. A half that is not moving carries a reading
// that has not changed anyway.
static tm *prv_drawn_time() {
  if (s_anim_running && s_anim_kind == ANIM_KIND_CHANGE &&
      prv_anim_elapsed_ms() < (int32_t)prv_anim_entry_start_ms()) {
    return &s_outgoing_time;
  }

  return prv_now();
}

static void prv_draw_time_layer(Layer *layer, GContext *ctx) {
  tm *time = prv_drawn_time();

  prv_draw_time(layer, ctx, time);
  prv_draw_complication(layer, ctx, time, true);
  prv_draw_complication(layer, ctx, time, false);
}

#ifdef SHOT_CONFIG
// A screenshot should not have to race a timer to catch the settled face, so a
// shot build never starts one: s_anim_running stays false, every beat reads as
// landed, and the face draws where it comes to rest.
static void prv_play_intro() {}
static void prv_play_time_change(tm *tick_time) { (void)tick_time; }
#else
static void prv_anim_update(Animation *animation,
                            const AnimationProgress progress) {
  s_anim_progress = progress;
  // Only the text moves; the stripes underneath it are already where they
  // belong.
  layer_mark_dirty(s_time_layer);
}

static void prv_anim_stopped(Animation *animation, bool finished,
                             void *context) {
  s_anim_running = false;
  // The animation destroys itself from here, so nothing outside may hold on
  // to it past this point.
  s_animation = NULL;
  layer_mark_dirty(s_time_layer);
}

static const AnimationImplementation s_anim_implementation = {
    .update = prv_anim_update,
};

static void prv_play(AnimKind kind, bool top, bool bottom) {
  s_anim_kind = kind;
  s_anim_top = top;
  s_anim_bottom = bottom;
  s_anim_duration_ms = prv_anim_duration_ms();
  s_anim_progress = 0;
  s_anim_running = true;

  s_animation = animation_create();
  animation_set_implementation(s_animation, &s_anim_implementation);
  animation_set_duration(s_animation, s_anim_duration_ms);
  // Each beat eases itself; easing the whole run as well would bunch them up.
  animation_set_curve(s_animation, AnimationCurveLinear);
  animation_set_handlers(
      s_animation, (AnimationHandlers){.stopped = prv_anim_stopped}, NULL);
  animation_schedule(s_animation);
}

static void prv_play_intro() {
  if (!g_settings.intro_animation) {
    return;
  }

  prv_play(ANIM_KIND_INTRO, true, true);
}

// The bottom half turns over every minute, that being the half the minute is
// in; on the hour the top goes with it. A face that is still arriving is left
// to arrive.
static void prv_play_time_change(tm *tick_time) {
  if (!g_settings.tick_animation || s_anim_running) {
    return;
  }

  time_t outgoing = time(NULL) - SECONDS_PER_MINUTE;
  s_outgoing_time = *localtime(&outgoing);

  prv_play(ANIM_KIND_CHANGE, tick_time->tm_min == 0, true);
}
#endif

static void prv_tick_handler(tm *tick_time, TimeUnits _units_changed) {
  if (settings_want_weather()) {
    weather_refresh_if_due();
  }

  prv_play_time_change(tick_time);
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
  prv_play_intro();
}

static void prv_window_unload(Window *window) {
  if (s_animation) {
    animation_unschedule(s_animation);
  }
  layer_destroy(s_time_layer);
  layer_destroy(s_background_layer);
  ffont_destroy(s_font);
  if (s_icon_font) {
    ffont_destroy(s_icon_font);
  }
}

// Aplite never gets the icon font. It has 24K of app RAM, of which the time
// font takes 4.4K and fctx's rasterisation buffers about 3K, leaving too little
// for a 7K icon font -- fctx fails to allocate its flag buffer and the app
// faults. So the font is left out of aplite's bundle entirely and its
// complications draw their reading without an icon above it. Everywhere else it
// is loaded only while a complication is actually drawing icons out of it,
// which is still worth doing on the 64K platforms.
static void prv_sync_icon_font() {
#if defined(PBL_PLATFORM_APLITE)
  return;
#else
  bool wanted = settings_want_icons();
  if (wanted && !s_icon_font) {
    s_icon_font = ffont_create_from_resource(RESOURCE_ID_ICONFONT);
  } else if (!wanted && s_icon_font) {
    ffont_destroy(s_icon_font);
    s_icon_font = NULL;
  }
#endif
}

static void prv_save_settings() {
  persist_write_data(SETTINGS_KEY, &g_settings, sizeof(g_settings));
}

static void prv_load_settings() {
  default_settings();
#ifndef SHOT_CONFIG
  // Leaves the defaults standing until something has been saved.
  persist_read_data(SETTINGS_KEY, &g_settings, sizeof(g_settings));
#endif
}

static void prv_inbox_received_callback(DictionaryIterator *iterator,
                                        void *context) {
  bool weather_dirty = weather_update(iterator);

  bool wanted_weather = settings_want_weather();
  bool settings_dirty = update_settings(iterator, context);
  if (settings_dirty) {
    prv_save_settings();
    prv_sync_icon_font();
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
  prv_sync_icon_font();

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
