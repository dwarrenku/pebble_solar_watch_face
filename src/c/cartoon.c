#include "cartoon.h"
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEG ((float)M_PI / 180.0f)
#define RIGHT_LIMIT 184  // keep the sleeping sun's z clear of the light theme's sidebar

static inline int rnd(float v) { return (int)lroundf(v); }

static float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

static void seg(GContext *ctx, float x0, float y0, float x1, float y1, int width, GColor c) {
  graphics_context_set_stroke_color(ctx, c);
  graphics_context_set_stroke_width(ctx, width);
  graphics_draw_line(ctx, GPoint(rnd(x0), rnd(y0)), GPoint(rnd(x1), rnd(y1)));
}

// A two-segment stroke: eyes, smiles, the moon's ^ eyes.
static void vee(GContext *ctx, float x0, float y0, float x1, float y1, float x2, float y2, int width, GColor c) {
  seg(ctx, x0, y0, x1, y1, width, c);
  seg(ctx, x1, y1, x2, y2, width, c);
}

static void fill_poly(GContext *ctx, GPoint *pts, int n, GColor c) {
  GPathInfo info = { .num_points = n, .points = pts };
  GPath *p = gpath_create(&info);
  if (!p) return;
  graphics_context_set_fill_color(ctx, c);
  gpath_draw_filled(ctx, p);
  gpath_destroy(p);
}

static void stroke_poly(GContext *ctx, GPoint *pts, int n, int width, GColor c) {
  GPathInfo info = { .num_points = n, .points = pts };
  GPath *p = gpath_create(&info);
  if (!p) return;
  graphics_context_set_stroke_color(ctx, c);
  graphics_context_set_stroke_width(ctx, width);
  gpath_draw_outline(ctx, p);
  gpath_destroy(p);
}

// n points around (cx, cy), odd ones pulled in to `inner` of the radius: the day separator's wobbly
// sun is 16 of them at 0.87, an octagon is 8 at 1.
static void facets(GPoint *out, float cx, float cy, float r, int n, float inner, float rot_deg) {
  for (int i = 0; i < n; i++) {
    const float a = (rot_deg + i * 360.0f / n) * DEG, rr = (i % 2) ? r * inner : r;
    out[i] = GPoint(rnd(cx + cosf(a) * rr), rnd(cy + sinf(a) * rr));
  }
}

float cartoon_rise(int anim_ms) {
  if (anim_ms < 0) return 1;
  const float p = clamp01(anim_ms / 420.0f), c1 = 2.2f, q = p - 1;
  return 1 + (c1 + 1) * q * q * q + c1 * q * q;  // ease-out-back
}

static GColor sun_fill(double alt) {
  return alt > 20 ? GColorYellow : alt > 6 ? GColorChromeYellow : GColorOrange;
}

void cartoon_sun(GContext *ctx, int cx, int cy, double alt, GColor outline, GColor rays, int anim_ms) {
  const float r = CARTOON_SUN_R;
  const float len = 2 + 4 * clamp01((float)alt / 30.0f);
  for (int k = 0; k < 8; k++) {
    const float g = anim_ms < 0 ? 1 : clamp01((anim_ms - 330 - k * 40) / 110.0f);
    if (g <= 0) continue;
    const float a = (-90 + k * 45) * DEG, r0 = r + 4 - 3 * (1 - g);
    seg(ctx, cx + cosf(a) * r0, cy + sinf(a) * r0, cx + cosf(a) * (r0 + len * g), cy + sinf(a) * (r0 + len * g), 2, rays);
  }
  GPoint body[16];
  facets(body, cx, cy, r, 16, 0.87f, -90);
  fill_poly(ctx, body, 16, sun_fill(alt));
  stroke_poly(ctx, body, 16, 3, outline);

  const GColor face = GColorBlack;
  const float ex1 = cx - 0.34f * r, ex2 = cx + 0.3f * r, ey = cy - 0.14f * r;
  const bool shut = alt < 6 || (anim_ms >= 0 && (anim_ms < 760 || (anim_ms > 1120 && anim_ms < 1200)));
  if (shut) {
    vee(ctx, ex1 - 2.5f, ey, ex1, ey + 1.5f, ex1 + 2.5f, ey, 2, face);
    vee(ctx, ex2 - 2.5f, ey, ex2, ey + 1.5f, ex2 + 2.5f, ey, 2, face);
  } else {
    seg(ctx, ex1, ey - 2.5f, ex1, ey + 2.5f, 2, face);
    seg(ctx, ex2, ey - 2.5f, ex2, ey + 2.5f, 2, face);
  }
  vee(ctx, cx - 0.36f * r, cy + 0.28f * r, cx - 0.03f * r, cy + (alt > 30 ? 0.5f : 0.42f) * r,
      cx + 0.3f * r, cy + 0.28f * r, 2, face);
}

void cartoon_sun_asleep(GContext *ctx, int cx, int cy, GColor ink) {
  const float r = 10;
  GPoint body[16];
  facets(body, cx, cy, r, 16, 0.87f, -90);
  stroke_poly(ctx, body, 16, 2, ink);
  const float ey = cy - 0.1f * r;
  vee(ctx, cx - 0.34f * r - 2, ey, cx - 0.34f * r, ey + 1.2f, cx - 0.34f * r + 2, ey, 1, ink);
  vee(ctx, cx + 0.3f * r - 2, ey, cx + 0.3f * r, ey + 1.2f, cx + 0.3f * r + 2, ey, 1, ink);
  const int zx = cx + r + 8 > RIGHT_LIMIT ? cx - r - 8 : cx + r + 2, zy = cy - r;
  seg(ctx, zx, zy, zx + 5, zy, 1, ink);
  seg(ctx, zx + 5, zy, zx, zy + 5, 1, ink);
  seg(ctx, zx, zy + 5, zx + 5, zy + 5, 1, ink);
}

void cartoon_moon(GContext *ctx, int cx, int cy, const MoonState *m, double lat, GColor outline, GColor dark) {
  const int r = 10;
  const float ri = r * 0.924f;  // the octagon's inradius
  const float a = 1.0f - 2.0f * m->illuminated;
  const bool lit_right = m->waxing != (lat < 0);
  for (int py = -r; py <= r; py++) {
    const float ny = (float)py / r, e2 = 1.0f - ny * ny, edge = sqrtf(e2 > 0 ? e2 : 0);
    for (int px = -r; px <= r; px++) {
      if (abs(px) > ri || abs(py) > ri || (abs(px) + abs(py)) > ri * 1.4142f) continue;
      const float nx = (float)px / r;
      const bool lit = lit_right ? nx > a * edge : nx < -a * edge;
      graphics_context_set_stroke_color(ctx, lit ? GColorPastelYellow : dark);
      graphics_draw_pixel(ctx, GPoint(cx + px, cy + py));
    }
  }
  GPoint oct[8];
  facets(oct, cx, cy, r, 8, 1, 22.5f);
  stroke_poly(ctx, oct, 8, 2, outline);
  if (m->illuminated > 0.6f) {  // a ^ eye each side and a little smile, as on the PebbleOS moon
    vee(ctx, cx - 5, cy - 1, cx - 3, cy - 3.5f, cx - 1, cy - 1, 2, GColorBlack);
    vee(ctx, cx + 1.5f, cy - 1, cx + 3.5f, cy - 3.5f, cx + 5.5f, cy - 1, 2, GColorBlack);
    vee(ctx, cx - 3, cy + 3, cx + 0.5f, cy + 5, cx + 4, cy + 3, 2, GColorBlack);
  }
}

// PebbleOS's chamfered cloud, w x h at (x, y), with its little fold line.
static void cloud(GContext *ctx, float x, float y, float w, float h, int width, GColor ink, GColor paper) {
  static const int8_t K[12][2] = { { 2, 12 }, { 0, 10 }, { 0, 6 }, { 3, 3 }, { 8, 3 }, { 11, 0 },
                                   { 16, 0 }, { 19, 3 }, { 21, 3 }, { 24, 6 }, { 24, 10 }, { 22, 12 } };
  GPoint pts[12];
  for (int i = 0; i < 12; i++) pts[i] = GPoint(rnd(x + K[i][0] * w / 24), rnd(y + K[i][1] * h / 12));
  fill_poly(ctx, pts, 12, paper);
  stroke_poly(ctx, pts, 12, width, ink);
  seg(ctx, x + 8 * w / 24, y + 3 * h / 12, x + 10.5f * w / 24, y + 5.5f * h / 12, width, ink);
}

static bool is_rain(WxCode c) { return c == WX_LIGHT_RAIN || c == WX_HEAVY_RAIN || c == WX_RAIN_AND_SNOW; }
static bool is_snow(WxCode c) { return c == WX_LIGHT_SNOW || c == WX_HEAVY_SNOW || c == WX_RAIN_AND_SNOW; }

void cartoon_weather(GContext *ctx, WxCode code, int sx, int sy, int r, int horizon_y, GColor ink, GColor paper) {
  if (code == WX_SUN || code == WX_UNKNOWN) return;
  const bool full = code != WX_PARTLY_CLOUDY, heavy = code == WX_HEAVY_RAIN || code == WX_HEAVY_SNOW;
  const bool rain = is_rain(code), snow = is_snow(code);
  const float cw = full ? 34 : 28, ch = full ? 16 : 14, x = sx - cw * 0.12f;
  float y = sy + r * 0.05f;
  const float lowest = horizon_y - ch - ((rain || snow) ? 14 : 3);
  if (y > lowest) y = lowest;
  if (code == WX_CLOUDY || heavy) cloud(ctx, sx - cw * 0.95f, sy - r - 2, cw * 0.65f, ch * 0.7f, 2, ink, paper);
  cloud(ctx, x, y, cw, ch, 2, ink, paper);
  const float below = y + ch + 3;
  const int n = heavy ? 4 : 3;
  for (int i = 0; i < n; i++) {
    const float px = x + 6 + i * 7, py = below + (i % 2) * 3;
    if (rain && !(snow && i % 2)) {
      seg(ctx, px, py, px - 1, py + 4, 2, ink);
    } else if (snow) {
      seg(ctx, px - 2, py + 2, px + 2, py + 2, 2, ink);
      seg(ctx, px, py, px, py + 4, 2, ink);
    }
  }
}

void cartoon_wx_icon(GContext *ctx, WxCode code, int x, int y, GColor ink, GColor paper, bool is_day) {
  if (code == WX_SUN || code == WX_PARTLY_CLOUDY) {
    GPoint oct[8];
    if (is_day) {
      const float cx = x + 9, cy = y + 9;
      for (int k = 0; k < 8; k++) {
        const float a = k * 45 * DEG;
        seg(ctx, cx + cosf(a) * 7, cy + sinf(a) * 7, cx + cosf(a) * 9, cy + sinf(a) * 9, 2, ink);
      }
      facets(oct, cx, cy, 4.5f, 8, 1, 22.5f);
    } else {
      facets(oct, x + 9, y + 8, 5, 8, 1, 22.5f);
    }
    fill_poly(ctx, oct, 8, paper);
    stroke_poly(ctx, oct, 8, 2, ink);
    if (code == WX_PARTLY_CLOUDY) cloud(ctx, x + 6, y + 9, 15, 8, 2, ink, paper);
    return;
  }
  cloud(ctx, x, y + 3, 19, 10, 2, ink, paper);
  const bool rain = is_rain(code), snow = is_snow(code);
  for (int i = 0; i < 3; i++) {
    const float px = x + 4 + i * 5;
    if (rain && !(snow && i % 2)) {
      seg(ctx, px, y + 16, px, y + 19, 2, ink);
    } else if (snow) {
      seg(ctx, px - 1.5f, y + 17.5f, px + 1.5f, y + 17.5f, 1, ink);
      seg(ctx, px, y + 16, px, y + 19, 1, ink);
    }
  }
}

void cartoon_star(GContext *ctx, int x, int y, GColor ink, bool big) {
  const int r = big ? 3 : 2;
  seg(ctx, x - r, y, x + r, y, 1, ink);
  seg(ctx, x, y - r, x, y + r, 1, ink);
}
