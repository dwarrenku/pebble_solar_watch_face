// Draws the face. Geometry and colours follow design/spec.md; the simulator's drawFinal()
// (design/sunpath-simulator.html) is the visual reference.

#include "face.h"
#include "glow.h"
#include "cartoon.h"
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEG (M_PI / 180.0)

// Layout (watch pixels).
#define SCREEN_W     200
#define EDGE         6
#define GRAPH_TOP    70
#define GRAPH_H      112
#define HORIZON_Y    130
#define AMP_UP       56
#define AMP_DOWN     42
#define FOOT_Y       186
#define FOOT_LABEL_Y 210
#define SLOT_W       66
static const int SLOT_X[SLOT_COUNT] = { 34, 100, 166 };

// System fonts draw their glyphs a few pixels below the top of the text box; these offsets
// move the box up so the design's y values are where the glyphs' tops land.
#define OFF_BITHAM42 9
#define OFF_G28      7
#define OFF_G24      6
#define OFF_G18      5
#define OFF_G14      3

static const char *DOW[] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
static const char *MON[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };

// The time's font for each TimeStyle: the font, how far its glyphs sit below the top of the text
// box, and where their tops should land.
static const struct { const char *key; int8_t offset, y; } TIME_FONTS[TIME_STYLE_COUNT] = {
  { FONT_KEY_BITHAM_42_BOLD, 9, 2 },
  { FONT_KEY_BITHAM_42_LIGHT, 9, 2 },
  { FONT_KEY_LECO_42_NUMBERS, 9, 3 },
  { FONT_KEY_BITHAM_30_BLACK, 7, 3 },
  { FONT_KEY_LECO_32_BOLD_NUMBERS, 7, 3 },
  { FONT_KEY_GOTHIC_28_BOLD, 7, 1 },
};

// The theme's colours and width, set at the top of face_draw: ink for text and lines, soft for
// secondary text, paper for the background. The light cartoon theme gives the right edge to a
// time-of-day sidebar, so its graph is narrower.
#define SIDEBAR_GRAPH_W 186
static GColor s_ink, s_soft, s_paper;
static int s_graph_w = SCREEN_W;

static bool theme_cartoon(uint8_t t) {
  return t == THEME_CARTOON_LIGHT || t == THEME_CARTOON_DARK || t == THEME_GLOW_CARTOON;
}
static bool theme_glow(uint8_t t) { return t == THEME_GLOW || t == THEME_GLOW_CARTOON; }

// ---------------------------------------------------------------------------------------- text

static GSize text_size(const char *s, GFont font) {
  return graphics_text_layout_get_content_size(s, font, GRect(0, 0, SCREEN_W, 60),
                                               GTextOverflowModeFill, GTextAlignmentLeft);
}

// While set, text gets a 1 px black outline, so it stays crisp over a flat-coloured glow.
static bool s_outline;

static void text(GContext *ctx, const char *s, GFont font, int offset, int x, int y, int w,
                 GTextAlignment align, GColor color) {
  if (s_outline) {
    graphics_context_set_text_color(ctx, GColorBlack);
    for (int dy = -1; dy <= 1; dy++) {
      for (int dx = -1; dx <= 1; dx++) {
        if (dx || dy) {
          graphics_draw_text(ctx, s, font, GRect(x + dx, y - offset + dy, w, 60), GTextOverflowModeFill, align, NULL);
        }
      }
    }
  }
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, s, font, GRect(x, y - offset, w, 60), GTextOverflowModeFill, align, NULL);
}

// Shortens `s` in place until it fits `max_w`.
static void fit(char *s, GFont font, int max_w) {
  size_t n = strlen(s);
  while (n > 1 && text_size(s, font).w > max_w) {
    s[--n] = '\0';
  }
}

// ------------------------------------------------------------------------------------- shapes

static void disc(GContext *ctx, int x, int y, int r, GColor c) {
  graphics_context_set_fill_color(ctx, c);
  graphics_fill_circle(ctx, GPoint(x, y), r);
}

static void ring(GContext *ctx, int x, int y, int r, int width, GColor c) {
  graphics_context_set_stroke_color(ctx, c);
  graphics_context_set_stroke_width(ctx, width);
  graphics_draw_circle(ctx, GPoint(x, y), r);
}

static void seg(GContext *ctx, int x0, int y0, int x1, int y1, int width, GColor c) {
  graphics_context_set_stroke_color(ctx, c);
  graphics_context_set_stroke_width(ctx, width);
  graphics_draw_line(ctx, GPoint(x0, y0), GPoint(x1, y1));
}

static void cloud(GContext *ctx, int x, int y, int w, GColor c) {
  const int r = w / 4;
  graphics_context_set_fill_color(ctx, c);
  graphics_fill_circle(ctx, GPoint(x + r, y + r * 13 / 10), r);
  graphics_fill_circle(ctx, GPoint(x + w * 45 / 100, y + r * 8 / 10), r * 5 / 4);
  graphics_fill_circle(ctx, GPoint(x + w - r, y + r * 13 / 10), r * 19 / 20);
  graphics_fill_rect(ctx, GRect(x + r, y + r * 13 / 10, w - 2 * r, r * 19 / 20 + 1), 0, GCornerNone);
}

static void sun_icon(GContext *ctx, int cx, int cy, int r, bool is_day) {
  if (is_day) {
    for (int i = 0; i < 8; i++) {
      const int32_t a = TRIG_MAX_ANGLE * i / 8;
      const int s = sin_lookup(a), c = cos_lookup(a);
      seg(ctx, cx + (r + 2) * c / TRIG_MAX_RATIO, cy + (r + 2) * s / TRIG_MAX_RATIO,
          cx + (r + 4) * c / TRIG_MAX_RATIO, cy + (r + 4) * s / TRIG_MAX_RATIO, 2, GColorChromeYellow);
    }
    disc(ctx, cx, cy, r, GColorChromeYellow);
  } else {
    disc(ctx, cx, cy, r + 1, GColorLightGray);
    disc(ctx, cx + r * 7 / 10, cy - r / 2, r, GColorBlack);
  }
}

// Weather icon in an s x s box at (x, y).
static void weather_icon(GContext *ctx, WxCode code, int x, int y, int s, bool is_day) {
  switch (code) {
    case WX_SUN:
      sun_icon(ctx, x + s / 2, y + s / 2, s * 22 / 100, is_day);
      return;
    case WX_PARTLY_CLOUDY:
      sun_icon(ctx, x + s * 36 / 100, y + s * 34 / 100, s * 17 / 100, is_day);
      cloud(ctx, x + s / 5, y + s * 38 / 100, s * 78 / 100, GColorWhite);
      return;
    case WX_UNKNOWN:
      cloud(ctx, x + s * 6 / 100, y + s * 14 / 100, s * 88 / 100, GColorLightGray);
      return;
    default:
      break;
  }
  cloud(ctx, x + s * 6 / 100, y + s * 14 / 100, s * 88 / 100,
        code == WX_HEAVY_RAIN ? GColorLightGray : GColorWhite);
  const bool rain = code == WX_LIGHT_RAIN || code == WX_HEAVY_RAIN || code == WX_RAIN_AND_SNOW;
  const bool snow = code == WX_LIGHT_SNOW || code == WX_HEAVY_SNOW || code == WX_RAIN_AND_SNOW;
  const int n = (code == WX_HEAVY_RAIN || code == WX_HEAVY_SNOW) ? 4 : 3;
  for (int i = 0; i < n; i++) {
    const int px = x + s * (25 + i * 17) / 100;
    if (rain && !(snow && i % 2)) {
      seg(ctx, px, y + s * 72 / 100, px - s / 20, y + s * 92 / 100, 2, GColorPictonBlue);
    } else if (snow) {
      disc(ctx, px, y + s * (78 + (i % 2) * 10) / 100, 1, GColorWhite);
    }
  }
}

// ---------------------------------------------------------------------------------- the graph

static float y_of_f(float alt) {
  return HORIZON_Y - sinf(alt * (float)DEG) * (alt >= 0 ? AMP_UP : AMP_DOWN);
}

static int y_of(float alt) {
  return (int)lroundf(y_of_f(alt));
}

// Solar glow theme: a white sun by day (pale yellow when low); once it has set, a black disc
// ringed in white, like an eclipse.
static void sun_glow_glyph(GContext *ctx, int x, int y, double alt) {
  if (alt >= -0.833) {
    disc(ctx, x, y, 7, alt > 12 ? GColorWhite : GColorPastelYellow);
  } else {
    disc(ctx, x, y, 8, GColorBlack);
    ring(ctx, x, y, 7, 3, GColorWhite);
  }
}

// The moon's strength: its glow follows how full it is, and only shows once the sky is dark.
static float moon_strength(const MoonState *m, double sun_alt) {
  float dark = (float)(-sun_alt / 8.0);
  if (dark < 0) dark = 0;
  if (dark > 1) dark = 1;
  return powf(m->illuminated, 0.8f) * dark;
}

// Whether the moon is worth drawing: on and up; at night once it is more than a sliver (thinner
// crescents are a pixel or two wide at this size and read as stray marks), by day only from about
// first quarter (a thin daytime crescent is invisible to the eye anyway).
static bool moon_shown(const FaceState *st) {
  const float min_lit = st->alt_now >= -0.833 ? 0.25f : 0.08f;
  return st->settings->moon && st->moon.altitude >= 0 && st->moon.illuminated > min_lit;
}

// The sun and the moon share the "now" column, so their glyphs can touch. By day the sun outshines
// the moon and it is not drawn; once the sun has set, the moon sits just above it, as when a full
// moon rises while the sun sets.
static bool moon_touches_sun(int moon_y, int sun_y) {
  return abs(moon_y - sun_y) < 16;
}

// A small pixel moon with its real phase: lit on the sun's side (mirrored south of the equator).
// At night the unlit part shows faintly, like earthshine.
static void moon_glyph(GContext *ctx, int x0, int y0, const MoonState *m, double lat, bool night) {
  const int r = 6;
  const float a = 1.0f - 2.0f * m->illuminated;
  const bool lit_right = m->waxing != (lat < 0);
  for (int py = -r; py <= r; py++) {
    const float ny = (float)py / r, edge = sqrtf(1.0f - ny * ny > 0 ? 1.0f - ny * ny : 0);
    for (int px = -r; px <= r; px++) {
      if (px * px + py * py > r * r + r * 4 / 5) continue;
      const float nx = (float)px / r;
      const bool lit = lit_right ? nx > a * edge : nx < -a * edge;
      if (!lit && !night) continue;
      graphics_context_set_stroke_color(ctx, lit ? GColorPastelYellow : GColorDarkGray);
      graphics_draw_pixel(ctx, GPoint(x0 + px, y0 + py));
    }
  }
}

// The moon's path while it is up, as small dots: quieter than the sun's line, but they must still
// show on the reflective screen with the backlight off.
static void moon_path(GContext *ctx, const DayModel *d) {
  graphics_context_set_fill_color(ctx, GColorLightGray);
  for (int x = 3; x < GRAPH_COLUMNS; x += 6) {
    if (d->moon_alt[x] >= 0) graphics_fill_rect(ctx, GRect(x - 1, y_of(d->moon_alt[x]) - 1, 2, 2), 0, GCornerNone);
  }
}

// Solar glow theme: the whole day's path as one grey line, 2 px so it holds up with the backlight off.
static void sun_path_line(GContext *ctx, const DayModel *d) {
  int prev = y_of(d->alt[0]);
  for (int x = 1; x < GRAPH_COLUMNS; x++) {
    const int y = y_of(d->alt[x]);
    seg(ctx, x - 1, prev, x, y, 2, GColorLightGray);
    prev = y;
  }
}

static void sun_glyph(GContext *ctx, int x, int y, double alt) {
  if (alt >= 0) {
    ring(ctx, x, y, 9, 2, alt > 12 ? GColorChromeYellow : GColorOrange);
    disc(ctx, x, y, 6, alt > 12 ? GColorYellow : (alt > 4 ? GColorChromeYellow : GColorOrange));
  } else {
    disc(ctx, x, y, 6, GColorBlack);
    ring(ctx, x, y, 5, 2, alt > -6 ? GColorRajah : GColorLightGray);
  }
}

// Classic theme: amber by day, solid behind the sun and dotted ahead of it.
static void sun_path(GContext *ctx, const DayModel *d, int now_x) {
  const GColor past_day = GColorChromeYellow;
  const GColor ahead_day = GColorPastelYellow;
  int prev = y_of(d->alt[0]);
  for (int x = 0; x < GRAPH_COLUMNS; x++) {
    const int y = y_of(d->alt[x]);
    const bool up = d->alt[x] >= 0;
    if (x <= now_x) {
      if (x > 0) seg(ctx, x - 1, prev, x, y, 2, up ? past_day : GColorLightGray);
    } else if (x % 4 == 0) {
      graphics_context_set_fill_color(ctx, up ? ahead_day : GColorLightGray);
      graphics_fill_rect(ctx, GRect(x, y - 1, 2, 2), 0, GCornerNone);
    }
    prev = y;
  }
}

static void horizon_labels(GContext *ctx, const FaceState *st, int now_x, int sun_y, int sun_r, bool ticks) {
  const DayModel *d = st->day;
  GFont f14 = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  if (!d->has_rise) {
    // The whole path is on one side of the line, so the label goes on the empty side.
    const bool night = d->noon_alt < 0;
    text(ctx, night ? "POLAR NIGHT" : "MIDNIGHT SUN", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
         OFF_G14, 0, night ? HORIZON_Y - 15 : HORIZON_Y + 4, s_graph_w, GTextAlignmentCenter, s_soft);
    return;
  }
  if (!st->settings->horizon_labels) return;
  char rt[8], stt[8];
  format_clock(d->rise, st->h24, rt, sizeof(rt));
  format_clock(d->set, st->h24, stt, sizeof(stt));
  const int rx = (int)(d->rise * s_graph_w / 1440), sx = (int)(d->set * s_graph_w / 1440);
  const int rw = text_size(rt, f14).w, sw = text_size(stt, f14).w;
  if (sx - rx < rw + sw + 12) return;  // short day: no room for both
  if (ticks) {
    seg(ctx, rx, HORIZON_Y - 3, rx, HORIZON_Y + 4, 2, s_soft);
    seg(ctx, sx, HORIZON_Y - 3, sx, HORIZON_Y + 4, 2, s_soft);
  }
  // Hide a label while the sun overlaps its box.
  const int ly0 = HORIZON_Y + 2, ly1 = HORIZON_Y + 18, r = sun_r;
  const bool sun_in_band = sun_y + r > ly0 && sun_y - r < ly1;
  if (!(sun_in_band && now_x + r > rx + 3 && now_x - r < rx + 3 + rw)) {
    text(ctx, rt, f14, OFF_G14, rx + 3, HORIZON_Y + 4, rw + 4, GTextAlignmentLeft, s_soft);
  }
  if (!(sun_in_band && now_x + r > sx - 3 - sw && now_x - r < sx - 3)) {
    text(ctx, stt, f14, OFF_G14, sx - 3 - sw - 4, HORIZON_Y + 4, sw + 4, GTextAlignmentRight, s_soft);
  }
}

static void flick_card(GContext *ctx, const FaceState *st) {
  const DayModel *d = st->day;
  graphics_context_set_fill_color(ctx, s_paper);
  graphics_fill_rect(ctx, GRect(0, GRAPH_TOP, s_graph_w, GRAPH_H), 0, GCornerNone);
  seg(ctx, 0, GRAPH_TOP, s_graph_w, GRAPH_TOP, 1, s_soft);

  GFont f14 = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  GFont f18b = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  char v[6][20];
  const bool ok = st->has_location && d->valid;
  #define T(i, has, m) do { if (ok && (has)) format_clock((m), st->h24, v[i], sizeof(v[i])); \
                            else snprintf(v[i], sizeof(v[i]), "--"); } while (0)
  T(0, d->has_dawn, d->dawn);
  T(1, d->has_dawn, d->dusk);
  T(2, d->has_rise, d->rise);
  T(3, d->has_rise, d->set);
  #undef T
  if (ok) {
    char noon[8];
    format_clock(d->noon, st->h24, noon, sizeof(noon));
    snprintf(v[4], sizeof(v[4]), "%s  %d°", noon, (int)lround(d->noon_alt));
  } else {
    snprintf(v[4], sizeof(v[4]), "--");
  }
  if (ok && d->has_rise && d->has_len_yesterday) {
    const double delta = (d->set - d->rise) - d->len_yesterday;  // minutes
    const int secs = (int)lround(fabs(delta) * 60);
    snprintf(v[5], sizeof(v[5]), "%s%dm %02ds", delta < 0 ? "-" : "+", secs / 60, secs % 60);
  } else {
    snprintf(v[5], sizeof(v[5]), "--");
  }
  static const char *LABELS[] = { "FIRST LIGHT", "LAST LIGHT", "SUNRISE", "SUNSET", "SOLAR NOON", "VS YESTERDAY" };
  for (int i = 0; i < 6; i++) {
    const int x = (i % 2) ? s_graph_w * 53 / 100 : 8, y = GRAPH_TOP + 6 + (i / 2) * 34;
    text(ctx, LABELS[i], f14, OFF_G14, x, y, 92, GTextAlignmentLeft, s_soft);
    text(ctx, v[i], f18b, OFF_G18, x, y + 14, 92, GTextAlignmentLeft, s_ink);
  }
}

// --------------------------------------------------------------------------------- the header

// As on the Solar face: the date top-left and a large thin time top-right, with the weather under
// the date.
static void header(GContext *ctx, const FaceState *st) {
  const uint8_t theme = st->settings->theme;
  const bool cartoon = theme_cartoon(theme);
  char buf[24];
  format_clock(st->now.tm_hour * 60 + st->now.tm_min, st->h24, buf, sizeof(buf));
  const int right = s_graph_w - EDGE + (s_graph_w < SCREEN_W ? 2 : 0);
  const uint8_t ts = st->settings->time_style < TIME_STYLE_COUNT ? st->settings->time_style : 0;
  text(ctx, buf, fonts_get_system_font(TIME_FONTS[ts].key), TIME_FONTS[ts].offset, 30, TIME_FONTS[ts].y, right - 30,
       GTextAlignmentRight, s_ink);
  const int day = st->now.tm_mday, mon = st->now.tm_mon;
  switch (st->settings->date_format) {
    case DATE_MON_DAY: snprintf(buf, sizeof(buf), "%s %d", MON[mon], day); break;
    case DATE_DAY_MON: snprintf(buf, sizeof(buf), "%d %s", day, MON[mon]); break;
    case DATE_M_D: snprintf(buf, sizeof(buf), "%d/%d", mon + 1, day); break;
    case DATE_D_M: snprintf(buf, sizeof(buf), "%d/%d", day, mon + 1); break;
    case DATE_NONE: buf[0] = '\0'; break;
    default: snprintf(buf, sizeof(buf), "%s %d", DOW[st->now.tm_wday], day); break;
  }
  text(ctx, buf, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), OFF_G18, EDGE + 1, 3, 80,
       GTextAlignmentLeft, theme == THEME_CLASSIC ? GColorChromeYellow : s_ink);

  if (!st->settings->weather_on) return;
  const Weather *w = st->weather;
  GFont f24b = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  const int age = st->utc - w->time;
  if (w->time == 0 || age >= 12 * 60 * 60 || w->temp == WX_NONE) {
    text(ctx, "--\u00b0", f24b, OFF_G24, EDGE + 1, 24, 70, GTextAlignmentLeft, theme == THEME_CARTOON_LIGHT ? s_soft : GColorDarkGray);
    return;
  }
  // The watch knows where the sun is this minute; the reading's own day flag can be an hour old.
  const bool located = st->has_location && st->day->valid;
  const bool is_day = located ? st->alt_now > -0.833 : (w->is_day >= 0 ? w->is_day == 1 : true);
  if (cartoon) {
    cartoon_wx_icon(ctx, (WxCode)w->code, EDGE - 1, 24, s_ink, s_paper, is_day);
  } else {
    weather_icon(ctx, (WxCode)w->code, EDGE - 1, 25, 22, is_day);
  }
  snprintf(buf, sizeof(buf), "%d\u00b0", w->temp);
  text(ctx, buf, f24b, OFF_G24, EDGE + 23, 24, 60, GTextAlignmentLeft, age < 3 * 60 * 60 ? s_ink : s_soft);
  if (w->hi != WX_NONE && w->lo != WX_NONE) {
    snprintf(buf, sizeof(buf), "%d\u00b0 %d\u00b0", w->hi, w->lo);
    text(ctx, buf, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), OFF_G18, EDGE + 1, 48, 70,
         GTextAlignmentLeft, s_soft);
  }
}

// ---------------------------------------------------------------------------- complications

static void footer(GContext *ctx, const FaceState *st) {
  GFont f28b = fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD);
  GFont f24b = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  GFont f18b = fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD);
  GFont f14 = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  for (int i = 0; i < SLOT_COUNT; i++) {
    char value[20], label[24];
    comp_text((CompType)st->settings->slot[i], i, st, value, sizeof(value), label, sizeof(label));
    if (!value[0] && !label[0]) continue;
    // Three equal columns across the graph's width (narrower beside the light theme's sidebar).
    const int sw = s_graph_w / 3, x = SLOT_X[i] * s_graph_w / SCREEN_W - sw / 2;
    // Values are the strongest thing in a slot: as large as fits, stepping down for long text.
    if (text_size(value, f28b).w <= sw - 2) {
      text(ctx, value, f28b, OFF_G28, x, FOOT_Y - 4, sw, GTextAlignmentCenter, s_ink);
    } else if (text_size(value, f24b).w <= sw - 4) {
      text(ctx, value, f24b, OFF_G24, x, FOOT_Y, sw, GTextAlignmentCenter, s_ink);
    } else {
      fit(value, f18b, sw);
      text(ctx, value, f18b, OFF_G18, x, FOOT_Y + 4, sw, GTextAlignmentCenter, s_ink);
    }
    fit(label, f14, sw);
    text(ctx, label, f14, OFF_G14, x, FOOT_LABEL_Y, sw, GTextAlignmentCenter, s_soft);
  }
}

// ------------------------------------------------------------------------------ cartoon themes

// The light theme's sidebar: a flat colour for the time of day, with a tab at the sun's height.
static void sidebar(GContext *ctx, double alt, int sun_y) {
  const GColor c = alt >= 6 ? GColorChromeYellow : alt >= -1 ? GColorOrange : alt >= -6 ? GColorFolly
                 : alt >= -12 ? GColorPurple : GColorDukeBlue;
  graphics_context_set_fill_color(ctx, c);
  graphics_fill_rect(ctx, GRect(s_graph_w + 2, 0, SCREEN_W - s_graph_w - 2, 228), 0, GCornerNone);
  const int ty = sun_y < 76 ? 76 : sun_y > 176 ? 176 : sun_y;
  GPoint tab[3] = { GPoint(s_graph_w + 2, ty - 7), GPoint(s_graph_w - 4, ty), GPoint(s_graph_w + 2, ty + 7) };
  GPathInfo info = { .num_points = 3, .points = tab };
  GPath *p = gpath_create(&info);
  if (p) { gpath_draw_filled(ctx, p); gpath_destroy(p); }
}

static float col_alt(const float *alts, int x) {
  int i = (x * SCREEN_W + s_graph_w / 2) / s_graph_w;
  return alts[i < 0 ? 0 : i >= GRAPH_COLUMNS ? GRAPH_COLUMNS - 1 : i];
}

// Whether there is a fresh weather reading to draw over the sun.
static bool weather_fresh(const FaceState *st) {
  const Weather *w = st->weather;
  return st->settings->weather_on && w->time != 0 && st->utc - w->time < 12 * 60 * 60 && w->temp != WX_NONE;
}

// After the PebbleOS day separator: the sun peeks over a thick horizon with its eyes shut, wakes as
// it climbs, and sleeps under the horizon at night while the faceted moon and a few sparkles come out.
static void cartoon_scene(GContext *ctx, const FaceState *st, int now_x, int sun_y) {
  const DayModel *d = st->day;
  const bool light = st->settings->theme == THEME_CARTOON_LIGHT;
  const bool up = st->alt_now >= -0.833;
  const int W = s_graph_w, anim = st->anim_ms;
  const float rise = cartoon_rise(anim);
  const WxCode code = weather_fresh(st) ? (WxCode)st->weather->code : WX_UNKNOWN;

  if (st->alt_now < -8) {
    static const int16_t STARS[6][3] = { { 16, 80, 1 }, { 58, 74, 0 }, { 104, 90, 1 }, { 146, 78, 0 }, { 36, 106, 0 }, { 128, 110, 0 } };
    for (int i = 0; i < 6; i++) cartoon_star(ctx, STARS[i][0] * W / SCREEN_W, STARS[i][1], s_soft, STARS[i][2]);
  }
  if (st->settings->moon) {
    graphics_context_set_fill_color(ctx, s_soft);
    for (int x = 3; x < W; x += 6) {
      const float ma = col_alt(d->moon_alt, x);
      if (ma >= 0) graphics_fill_rect(ctx, GRect(x - 1, y_of(ma) - 1, 2, 2), 0, GCornerNone);
    }
  }
  // The path, solid while the sun is up (its dotted night part goes in after the cover below).
  const GColor path = light ? s_ink : GColorLightGray;
  int prev = 0;
  bool have_prev = false;
  for (int x = 0; x <= W; x++) {
    const float al = col_alt(d->alt, x);
    const int y = y_of(al);
    if (al >= 0) {
      if (have_prev) seg(ctx, x - 1, prev, x, y, 2, path);
      prev = y;
      have_prev = true;
    } else {
      have_prev = false;
    }
  }

  // The moon: up and lit enough to matter. By day it gives way if it would touch the sun.
  int moon_y = y_of((float)st->moon.altitude);
  if (moon_y > HORIZON_Y - 4) moon_y = HORIZON_Y - 4;
  if (!up && anim >= 0) moon_y = (int)lroundf((HORIZON_Y + 12) + (moon_y - (HORIZON_Y + 12)) * rise);
  const bool touching = abs(moon_y - sun_y) < 30;
  if (moon_shown(st) && !(touching && up)) {
    cartoon_moon(ctx, now_x, moon_y, &st->moon, d->lat, s_ink, light ? GColorLightGray : GColorDarkGray);
    if (!up) cartoon_weather(ctx, code, now_x, moon_y, 10, HORIZON_Y, s_ink, s_paper);
  }
  if (up) {
    // On black the outline and rays are one step deeper than the body, so the edge still shows.
    const GColor warm = st->alt_now > 20 ? GColorChromeYellow : st->alt_now > 6 ? GColorOrange : GColorRed;
    cartoon_sun(ctx, now_x, sun_y, st->alt_now, light ? s_ink : warm, light ? s_ink : warm, anim);
    if (anim < 0 || anim > 700) cartoon_weather(ctx, code, now_x, sun_y, CARTOON_SUN_R, HORIZON_Y, s_ink, s_paper);
  }

  // Everything above was drawn whole; cut it off at the horizon, so the sun peeks over it.
  int cx0 = now_x - 32, cx1 = now_x + 32;
  if (cx0 < 0) cx0 = 0;
  if (cx1 > W) cx1 = W;
  graphics_context_set_fill_color(ctx, s_paper);
  graphics_fill_rect(ctx, GRect(cx0, HORIZON_Y + 1, cx1 - cx0, 34), 0, GCornerNone);

  graphics_context_set_fill_color(ctx, s_soft);
  for (int x = 0; x < W; x += 5) {
    const float al = col_alt(d->alt, x);
    if (al < 0) graphics_fill_rect(ctx, GRect(x - 1, y_of(al) - 1, 2, 2), 0, GCornerNone);
  }
  // Asleep, the sun lies wholly under the horizon (just after sunset its true place is on the line).
  const int sleep_y = sun_y < HORIZON_Y + 13 ? HORIZON_Y + 13 : sun_y > 174 ? 174 : sun_y;
  if (!up) cartoon_sun_asleep(ctx, now_x, sleep_y, s_soft);
  seg(ctx, 2, HORIZON_Y, W - 2, HORIZON_Y, 3, s_ink);
  horizon_labels(ctx, st, now_x, up ? sun_y : sleep_y, up ? CARTOON_SUN_R + 4 : 12, false);
}

// ---------------------------------------------------------------------------------- the face

void face_draw(GContext *ctx, GRect bounds, GRect unobstructed, const FaceState *st) {
  const Settings *s = st->settings;
  const DayModel *d = st->day;
  const bool glow = theme_glow(s->theme);
  const bool cartoon = theme_cartoon(s->theme);
  const bool light = s->theme == THEME_CARTOON_LIGHT;
  const bool located = st->has_location && d->valid;
  const bool sun_up = st->alt_now >= -0.833;

  s_graph_w = light ? SIDEBAR_GRAPH_W : SCREEN_W;
  s_ink = light ? GColorBlack : GColorWhite;
  s_soft = light ? GColorDarkGray : GColorLightGray;
  s_paper = light ? GColorWhite : GColorBlack;

  const float now_xf = (float)(st->minute * s_graph_w / 1440);
  const int now_x = (int)now_xf;
  int sun_y = y_of((float)st->alt_now);
  if (cartoon && sun_up) {
    // From sunrise to about 7 degrees the sun sits with its face just over the horizon, and while
    // it wakes it springs up from behind it.
    const int peek = HORIZON_Y - (CARTOON_SUN_R * 45 + 50) / 100;
    if (sun_y > peek) sun_y = peek;
    if (st->anim_ms >= 0) {
      const int from = HORIZON_Y + CARTOON_SUN_R + 2;
      sun_y = (int)lroundf(from + (sun_y - from) * cartoon_rise(st->anim_ms));
    }
  }

  graphics_context_set_antialiased(ctx, true);
  graphics_context_set_fill_color(ctx, s_paper);
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);

  const int moon_raw_y = y_of((float)st->moon.altitude);
  const bool touching = moon_touches_sun(moon_raw_y, sun_y);
  const int moon_y = touching ? sun_y - 16 : moon_raw_y;
  const bool moon = located && moon_shown(st) && !(touching && sun_up);

  // The glows go down first so the time, the paths, the sun and the moon draw over them.
  const GlowStyle style = s->glow_style < GLOW_STYLE_COUNT ? (GlowStyle)s->glow_style : GLOW_SOFT;
  const float glow_y = cartoon ? (float)sun_y : y_of_f((float)st->alt_now);
  if (glow && located && !st->flick && style != GLOW_SOFT) {
    const GlowSpot sun_spot = { .x = now_xf, .y = glow_y, .alt = st->alt_now, .strength = 1.0f, .moon = false };
    const GlowSpot moon_spot = { .x = now_xf, .y = (float)moon_y, .alt = st->moon.altitude,
                                 .strength = moon_strength(&st->moon, st->alt_now), .moon = true };
    glow_draw_flat(ctx, style, &sun_spot, moon ? &moon_spot : NULL, HORIZON_Y, st->now.tm_hour * 60 + st->now.tm_min);
  } else if (glow && located && !st->flick) {
    GlowSpot spots[2] = {
      { .x = now_xf, .y = glow_y, .alt = st->alt_now, .strength = 1.0f, .moon = false },
      { .x = now_xf, .y = (float)moon_y, .alt = st->moon.altitude,
        .strength = moon_strength(&st->moon, st->alt_now), .moon = true },
    };
    glow_draw(ctx, spots, moon ? 2 : 1, HORIZON_Y, GRAPH_TOP);
  }
  if (light) sidebar(ctx, located ? st->alt_now : 0, located ? sun_y : HORIZON_Y);

  // Over a flat glow the header text is outlined; the soft glow fades under it instead.
  s_outline = glow && located && !st->flick && style != GLOW_SOFT;
  header(ctx, st);
  s_outline = false;

  if (!located) {
    seg(ctx, 0, HORIZON_Y, s_graph_w, HORIZON_Y, cartoon ? 3 : 2, cartoon ? s_ink : GColorLightGray);
    text(ctx, "WAITING FOR LOCATION", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), OFF_G14, 0,
         HORIZON_Y + 4, s_graph_w, GTextAlignmentCenter, s_soft);
  } else if (st->flick) {
    flick_card(ctx, st);
  } else if (cartoon) {
    cartoon_scene(ctx, st, now_x, sun_y);
  } else {
    if (st->settings->moon) moon_path(ctx, d);
    if (moon) moon_glyph(ctx, now_x, moon_y, &st->moon, d->lat, st->alt_now < -4);
    // Ticks and times go under the sun, so at sunrise and sunset the sun covers its own tick.
    if (glow) {
      seg(ctx, 0, HORIZON_Y, SCREEN_W, HORIZON_Y, 2, GColorLightGray);
      sun_path_line(ctx, d);
      horizon_labels(ctx, st, now_x, sun_y, 10, true);
      sun_glow_glyph(ctx, now_x, sun_y, st->alt_now);
    } else {
      seg(ctx, 0, HORIZON_Y, SCREEN_W, HORIZON_Y, 2, GColorLightGray);
      sun_path(ctx, d, now_x);
      horizon_labels(ctx, st, now_x, sun_y, 10, true);
      sun_glyph(ctx, now_x, sun_y, st->alt_now);
    }
  }

  // Timeline Quick View covers the bottom of the screen: give way with the complication row only.
  if (unobstructed.size.h >= bounds.size.h) {
    footer(ctx, st);
  }
}
