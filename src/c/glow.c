#include "glow.h"
#include <math.h>

#define LUT_SIZE 64
#define MAX_STOPS 8
#define MAX_SPOTS 2

typedef struct {
  float u;               // distance from the centre as a fraction of the radius
  uint8_t r, g, b;
} GlowStop;

typedef struct {
  int8_t alt;            // altitude this key is for, degrees
  uint8_t radius;        // horizontal radius, px
  float squash;          // vertical radius / horizontal radius (< 1 hugs the horizon)
  float gain;            // overall brightness
  uint8_t count;
  GlowStop stops[MAX_STOPS];
} GlowKey;

// Lighter tints than a backlit screen would need: with the backlight off the panel only shows
// bright colours well, and dark blues and purples read as murk. The fall to black is kept short.
static const GlowKey SUN_KEYS[] = {
  { -16, 80, 0.5f, 0.0f, 2, { { 0, 0, 0, 0 }, { 1, 0, 0, 0 } } },
  { -10, 96, 0.45f, 0.7f, 5, { { 0, 255, 85, 255 }, { 0.3f, 170, 85, 255 }, { 0.62f, 85, 0, 170 },
                               { 0.82f, 40, 0, 85 }, { 1, 0, 0, 0 } } },
  { -4, 108, 0.5f, 1.0f, 7, { { 0, 255, 220, 230 }, { 0.12f, 255, 140, 200 }, { 0.32f, 255, 60, 170 },
                              { 0.55f, 190, 90, 255 }, { 0.78f, 85, 40, 170 }, { 0.9f, 30, 10, 60 }, { 1, 0, 0, 0 } } },
  { 0, 106, 0.6f, 1.0f, 7, { { 0, 255, 255, 210 }, { 0.08f, 255, 230, 100 }, { 0.22f, 255, 170, 40 },
                             { 0.4f, 255, 90, 120 }, { 0.62f, 200, 90, 230 }, { 0.82f, 80, 40, 150 }, { 1, 0, 0, 0 } } },
  { 8, 94, 0.82f, 1.0f, 7, { { 0, 255, 255, 230 }, { 0.07f, 255, 250, 140 }, { 0.2f, 255, 200, 90 },
                             { 0.38f, 255, 130, 170 }, { 0.6f, 150, 130, 255 }, { 0.82f, 50, 40, 130 }, { 1, 0, 0, 0 } } },
  { 22, 86, 1.0f, 1.0f, 8, { { 0, 255, 255, 255 }, { 0.06f, 255, 255, 170 }, { 0.16f, 170, 255, 220 },
                             { 0.32f, 85, 220, 255 }, { 0.52f, 100, 150, 255 }, { 0.75f, 110, 70, 200 },
                             { 0.9f, 40, 20, 80 }, { 1, 0, 0, 0 } } },
};

static const GlowKey MOON_KEYS[] = {
  { 6, 40, 0.8f, 0.6f, 5, { { 0, 255, 230, 170 }, { 0.15f, 255, 190, 110 }, { 0.42f, 200, 110, 110 },
                            { 0.72f, 70, 30, 70 }, { 1, 0, 0, 0 } } },
  { 18, 44, 1.0f, 0.6f, 5, { { 0, 240, 245, 255 }, { 0.15f, 190, 200, 255 }, { 0.42f, 120, 120, 230 },
                             { 0.72f, 40, 40, 110 }, { 1, 0, 0, 0 } } },
};

static const uint8_t BAYER4[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };

static void sample(const GlowKey *key, float u, float out[3]) {
  int j = 0;
  while (j < key->count - 2 && u > key->stops[j + 1].u) j++;
  const GlowStop *a = &key->stops[j], *b = &key->stops[j + 1];
  float v = (u - a->u) / (b->u - a->u);
  if (v < 0) v = 0;
  if (v > 1) v = 1;
  out[0] = (a->r + (b->r - a->r) * v) * key->gain;
  out[1] = (a->g + (b->g - a->g) * v) * key->gain;
  out[2] = (a->b + (b->b - a->b) * v) * key->gain;
}

static inline uint8_t level(float v, float threshold) {
  const int l = (int)(v / 85.0f + threshold);
  return l > 3 ? 3 : (l < 0 ? 0 : l);
}

typedef struct {
  float x, y, r2, k_y, ry, radius;
  uint8_t lut[LUT_SIZE][3];
} Prepared;

// Builds one glow's colour table (by squared distance) and shape; false if it would be black.
static bool prepare(const GlowSpot *spot, Prepared *p) {
  const GlowKey *keys = spot->moon ? MOON_KEYS : SUN_KEYS;
  const int n = spot->moon ? (int)ARRAY_LENGTH(MOON_KEYS) : (int)ARRAY_LENGTH(SUN_KEYS);
  int i = 0;
  if (spot->alt >= keys[n - 1].alt) i = n - 2;
  else while (i < n - 2 && spot->alt > keys[i + 1].alt) i++;
  const GlowKey *a = &keys[i], *b = &keys[i + 1];
  float t = (float)((spot->alt - a->alt) / (b->alt - a->alt));
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  for (int k = 0; k < LUT_SIZE; k++) {
    const float u = sqrtf((float)k / (LUT_SIZE - 1));
    float ca[3], cb[3];
    sample(a, u, ca);
    sample(b, u, cb);
    for (int c = 0; c < 3; c++) {
      const float v = (ca[c] + (cb[c] - ca[c]) * t) * spot->strength;
      p->lut[k][c] = v > 255 ? 255 : (v < 0 ? 0 : (uint8_t)v);
    }
  }
  if (p->lut[0][0] + p->lut[0][1] + p->lut[0][2] == 0) return false;
  p->radius = a->radius + (b->radius - a->radius) * t;
  const float squash = a->squash + (b->squash - a->squash) * t;
  p->r2 = p->radius * p->radius;
  p->ry = p->radius * squash;
  p->k_y = 1.0f / (squash * squash);
  p->x = spot->x;
  p->y = spot->y;
  return true;
}

void glow_draw(GContext *ctx, const GlowSpot *spots, int count, int horizon_y, int fade_y) {
  static Prepared prepared[MAX_SPOTS];
  int n = 0;
  int y0 = horizon_y, y1 = 0, x0 = 10000, x1 = -10000;
  for (int i = 0; i < count && n < MAX_SPOTS; i++) {
    if (!prepare(&spots[i], &prepared[n])) continue;
    const Prepared *p = &prepared[n++];
    if ((int)(p->y - p->ry) < y0) y0 = (int)(p->y - p->ry);
    if ((int)(p->y + p->ry) + 1 > y1) y1 = (int)(p->y + p->ry) + 1;
    if ((int)(p->x - p->radius) < x0) x0 = (int)(p->x - p->radius);
    if ((int)(p->x + p->radius) + 1 > x1) x1 = (int)(p->x + p->radius) + 1;
  }
  if (n == 0) return;
  if (y0 < 0) y0 = 0;
  if (y1 > horizon_y) y1 = horizon_y;

  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) return;
  for (int y = y0; y < y1; y++) {
    const float att = y >= fade_y ? 1.0f : 0.4f + 0.6f * y / fade_y;
    float dy2[MAX_SPOTS];
    bool any = false;
    for (int i = 0; i < n; i++) {
      const float dy = y - prepared[i].y;
      dy2[i] = dy * dy * prepared[i].k_y;
      any |= dy2[i] < prepared[i].r2;
    }
    if (!any) continue;
    GBitmapDataRowInfo row = gbitmap_get_data_row_info(fb, y);
    const int xs = x0 < row.min_x ? row.min_x : x0;
    const int xe = x1 > row.max_x + 1 ? row.max_x + 1 : x1;
    for (int x = xs; x < xe; x++) {
      int r = 0, g = 0, b = 0;
      bool hit = false;
      for (int i = 0; i < n; i++) {
        const float dx = x - prepared[i].x, d2 = dx * dx + dy2[i];
        if (d2 >= prepared[i].r2) continue;
        const uint8_t *c = prepared[i].lut[(int)(d2 * (LUT_SIZE - 1) / prepared[i].r2)];
        r += c[0]; g += c[1]; b += c[2];
        hit = true;
      }
      if (!hit) continue;
      const float th = (BAYER4[(y & 3) * 4 + (x & 3)] + 0.5f) / 16.0f;
      row.data[x] = 0xC0 | level(r * att, th) << 4 | level(g * att, th) << 2 | level(b * att, th);
    }
  }
  graphics_release_frame_buffer(ctx, fb);
}

// ------------------------------------------------------------------------------- flat styles

#define FLAT_SCALE 0.86f   // flat glows are a little smaller than the soft one: hard edges read larger

typedef struct {
  int8_t from;           // from this sun altitude up
  uint8_t count;
  uint8_t colors[5];     // GColor8 values, from the sun outward
} Bands;

static const Bands BANDS[] = {
  { -12, 3, { GColorLavenderIndigoARGB8, GColorPurpleARGB8, GColorIndigoARGB8 } },
  { -6, 4, { GColorRichBrilliantLavenderARGB8, GColorShockingPinkARGB8, GColorPurpleARGB8, GColorIndigoARGB8 } },
  { -1, 5, { GColorPastelYellowARGB8, GColorChromeYellowARGB8, GColorOrangeARGB8, GColorFollyARGB8, GColorPurpleARGB8 } },
  { 5, 5, { GColorPastelYellowARGB8, GColorIcterineARGB8, GColorChromeYellowARGB8, GColorMelonARGB8, GColorBabyBlueEyesARGB8 } },
  { 15, 4, { GColorPastelYellowARGB8, GColorElectricBlueARGB8, GColorPictonBlueARGB8, GColorVeryLightBlueARGB8 } },
};

// The radius and squash the soft glow would have at this altitude.
static void soft_shape(const GlowSpot *spot, float *radius, float *squash) {
  const GlowKey *keys = spot->moon ? MOON_KEYS : SUN_KEYS;
  const int n = spot->moon ? (int)ARRAY_LENGTH(MOON_KEYS) : (int)ARRAY_LENGTH(SUN_KEYS);
  int i = 0;
  if (spot->alt >= keys[n - 1].alt) i = n - 2;
  else while (i < n - 2 && spot->alt > keys[i + 1].alt) i++;
  const GlowKey *a = &keys[i], *b = &keys[i + 1];
  float t = (float)((spot->alt - a->alt) / (b->alt - a->alt));
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  *radius = a->radius + (b->radius - a->radius) * t;
  *squash = a->squash + (b->squash - a->squash) * t;
}

static void paint_flat(GBitmap *fb, GlowStyle style, float sx, float sy, float R, float squash,
                       const uint8_t *colors, int n, int horizon_y, int minute_of_day) {
  const float rot = (minute_of_day % 360) * 3.14159265f / 180.0f;
  const float wedge_w = 2.0f * 3.14159265f / 16.0f;
  int y0 = (int)floorf(sy - R * squash), y1 = (int)ceilf(sy + R * squash);
  if (y0 < 0) y0 = 0;
  if (y1 > horizon_y) y1 = horizon_y;
  for (int y = y0; y < y1; y++) {
    GBitmapDataRowInfo row = gbitmap_get_data_row_info(fb, y);
    int x0 = (int)floorf(sx - R), x1 = (int)ceilf(sx + R);
    if (x0 < row.min_x) x0 = row.min_x;
    if (x1 > row.max_x + 1) x1 = row.max_x + 1;
    for (int x = x0; x < x1; x++) {
      const float px = style == GLOW_PIXEL ? (x & ~3) + 2 : x;
      const float py = style == GLOW_PIXEL ? (y & ~3) + 2 : y;
      const float dx = px - sx, dy = (py - sy) / squash;
      const float u = sqrtf(dx * dx + dy * dy) / R;
      if (u >= 1) continue;
      const int band = (int)(u * n);
      switch (style) {
        case GLOW_BANDS:
          row.data[x] = colors[band];
          break;
        case GLOW_RIPPLES: {
          const float r = u * R;
          for (int k = 0; k < n; k++) {
            if (fabsf(r - R * (k + 1) / (n + 0.4f)) <= 1.6f) row.data[x] = colors[k];
          }
          if (r < R * 0.16f) row.data[x] = colors[0];
          break;
        }
        case GLOW_RAYS: {
          const float a = atan2f((y - sy) / squash, x - sx) + rot;
          const int wedge = (int)((fmodf(a, 2 * 3.14159265f) + 2 * 3.14159265f) / wedge_w) % 2;
          if (u < 0.2f) row.data[x] = colors[0];
          else if (wedge == 0) row.data[x] = colors[band < n - 1 ? band : n - 1];
          else if (u < 0.45f) row.data[x] = colors[n > 1 ? 1 : 0];
          break;
        }
        case GLOW_PIXEL: {
          const float frac = u * n - band;
          const bool checker = (((x >> 1) + (y >> 1)) & 1) == 1;
          if (frac > 0.62f && checker) row.data[x] = band + 1 < n ? colors[band + 1] : GColorBlackARGB8;
          else row.data[x] = colors[band];
          break;
        }
        case GLOW_SCAN: {
          const int thick = u < 0.4f ? 3 : (u < 0.7f ? 2 : 1);
          const int phase = (((y - (int)lroundf(sy)) % 4) + 4) % 4;
          if (phase < thick) row.data[x] = colors[band];
          break;
        }
        default:
          break;
      }
    }
  }
}

void glow_draw_flat(GContext *ctx, GlowStyle style, const GlowSpot *sun, const GlowSpot *moon,
                    int horizon_y, int minute_of_day) {
  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) return;
  if (sun->alt >= BANDS[0].from) {
    const Bands *b = &BANDS[0];
    for (size_t i = 0; i < ARRAY_LENGTH(BANDS); i++) {
      if (sun->alt >= BANDS[i].from) b = &BANDS[i];
    }
    float radius, squash;
    soft_shape(sun, &radius, &squash);
    paint_flat(fb, style, sun->x, sun->y, radius * FLAT_SCALE, squash, b->colors, b->count, horizon_y, minute_of_day);
  }
  // The moon gets one or two small rings, by how full and how dark it is; no rays.
  if (moon && moon->strength > 0.25f) {
    static const uint8_t LOW[] = { GColorRajahARGB8, GColorJazzberryJamARGB8 };
    static const uint8_t HIGH[] = { GColorBabyBlueEyesARGB8, GColorLibertyARGB8 };
    const uint8_t *mc = moon->alt < 6 ? LOW : HIGH;
    const bool bright = moon->strength > 0.6f;
    paint_flat(fb, style == GLOW_RAYS ? GLOW_BANDS : style, moon->x, moon->y, bright ? 34 : 24, 1.0f,
               bright ? mc : mc + 1, bright ? 2 : 1, horizon_y, minute_of_day);
  }
  graphics_release_frame_buffer(ctx, fb);
}
