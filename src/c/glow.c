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

// The glow changes shape as the sun comes up and over, as on the Solar face. While the sun is under
// the horizon it is a low, wide swell lying along the horizon, like a bell curve: a red band on the
// ground, blue above, peaking gently over where the sun will rise. As the sun climbs the swell
// draws in and stands up, until it is a round halo about the sun. (radius is the half-width; squash
// is height / half-width.) Colours are bright ones the reflective panel shows well, with the fall
// to black kept short, since dark tints read as murk.
static const GlowKey SUN_KEYS[] = {
  { -16, 90, 0.2f, 0.0f, 2, { { 0, 0, 0, 0 }, { 1, 0, 0, 0 } } },
  { -10, 96, 0.24f, 0.75f, 5, { { 0, 255, 120, 150 }, { 0.3f, 220, 90, 190 }, { 0.6f, 130, 90, 230 },
                                { 0.85f, 70, 60, 170 }, { 1, 0, 0, 0 } } },
  { -4, 106, 0.32f, 1.0f, 6, { { 0, 255, 170, 140 }, { 0.2f, 255, 100, 110 }, { 0.42f, 230, 90, 170 },
                               { 0.66f, 140, 110, 240 }, { 0.87f, 80, 80, 190 }, { 1, 0, 0, 0 } } },
  { 0, 102, 0.42f, 1.0f, 7, { { 0, 255, 255, 210 }, { 0.1f, 255, 225, 110 }, { 0.26f, 255, 150, 70 },
                              { 0.46f, 255, 90, 120 }, { 0.68f, 190, 100, 235 }, { 0.87f, 100, 90, 210 }, { 1, 0, 0, 0 } } },
  { 8, 84, 0.72f, 1.0f, 7, { { 0, 255, 255, 230 }, { 0.1f, 255, 250, 140 }, { 0.28f, 255, 200, 90 },
                             { 0.48f, 255, 140, 150 }, { 0.68f, 200, 140, 255 }, { 0.86f, 120, 110, 240 }, { 1, 0, 0, 0 } } },
  { 22, 64, 1.0f, 1.0f, 7, { { 0, 255, 255, 255 }, { 0.1f, 255, 255, 170 }, { 0.26f, 255, 225, 110 },
                             { 0.44f, 255, 190, 150 }, { 0.62f, 190, 200, 255 }, { 0.84f, 110, 150, 255 }, { 1, 0, 0, 0 } } },
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
  float bell;             // 0 a plain ellipse .. 1 a bell curve lying on the horizon
  float col_k[200];       // per column: 1 / (the glow's height there)^2
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
  // The flatter the glow, the more it is a bell curve: full height over the sun, tapering away to
  // thin tails along the horizon, rather than an ellipse with blunt ends.
  float bell = spot->moon ? 0 : (0.8f - squash) / 0.45f;
  p->bell = bell < 0 ? 0 : bell > 1 ? 1 : bell;
  const float two_sigma2 = 2 * (p->radius * 0.5f) * (p->radius * 0.5f);
  for (int x = 0; x < 200; x++) {
    const float dx = x - p->x;
    const float h = p->ry * (p->bell * expf(-dx * dx / two_sigma2) + (1 - p->bell));
    p->col_k[x] = h > 0.5f ? 1.0f / (h * h) : 4.0f;
  }
  return true;
}

// The glows, added over an optional sky gradient (top to bottom colours, or NULL for none), then
// ordered-dithered once into the 64 colours.
static void draw(GContext *ctx, const GlowSpot *spots, int count, int horizon_y, int fade_y,
                 const uint8_t *sky_top, const uint8_t *sky_bottom) {
  static Prepared prepared[MAX_SPOTS];
  int n = 0;
  int y0 = horizon_y, y1 = 0, x0 = 10000, x1 = -10000;
  for (int i = 0; i < count && n < MAX_SPOTS; i++) {
    if (!prepare(&spots[i], &prepared[n])) continue;
    Prepared *p = &prepared[n++];
    // A sun under the horizon lights the horizon itself: its swell is centred on the line.
    if (!spots[i].moon && p->y > horizon_y) p->y = horizon_y;
    if ((int)(p->y - p->ry) < y0) y0 = (int)(p->y - p->ry);
    if ((int)(p->y + p->ry) + 1 > y1) y1 = (int)(p->y + p->ry) + 1;
    const float reach = p->radius * (1 + p->bell);  // a bell's tails run past the ellipse's ends
    if ((int)(p->x - reach) < x0) x0 = (int)(p->x - reach);
    if ((int)(p->x + reach) + 1 > x1) x1 = (int)(p->x + reach) + 1;
  }
  const bool sky = sky_top && sky_bottom;
  if (n == 0 && !sky) return;
  if (sky) { y0 = 0; y1 = horizon_y; x0 = 0; x1 = 200; }
  if (y0 < 0) y0 = 0;
  if (y1 > horizon_y) y1 = horizon_y;

  GBitmap *fb = graphics_capture_frame_buffer(ctx);
  if (!fb) return;
  for (int y = y0; y < y1; y++) {
    // Above fade_y the glow dies away quickly, so the time and weather stay on near-black.
    const float fy = (float)y / fade_y, att = y >= fade_y ? 1.0f : fy * fy * fy;
    float base[3] = { 0, 0, 0 };
    if (sky) {  // darkest at the top, deepening toward the horizon
      const float t = (float)y / horizon_y, k = t * t;
      for (int c = 0; c < 3; c++) base[c] = sky_top[c] + (sky_bottom[c] - sky_top[c]) * k;
    }
    float dy2[MAX_SPOTS];
    bool any = sky;
    for (int i = 0; i < n; i++) {
      const float dy = y - prepared[i].y;
      dy2[i] = dy * dy;
      any |= dy2[i] < prepared[i].ry * prepared[i].ry;
    }
    if (!any) continue;
    GBitmapDataRowInfo row = gbitmap_get_data_row_info(fb, y);
    int xs = x0 < row.min_x ? row.min_x : x0;
    int xe = x1 > row.max_x + 1 ? row.max_x + 1 : x1;
    if (xs < 0) xs = 0;
    if (xe > 200) xe = 200;
    for (int x = xs; x < xe; x++) {
      int r = 0, g = 0, b = 0;
      bool hit = false;
      for (int i = 0; i < n; i++) {
        // Normalised squared distance: 0 at the centre, 1 at the glow's edge.
        const Prepared *p = &prepared[i];
        const float dx = x - p->x;
        const float u2 = (1 - p->bell) * dx * dx / p->r2 + dy2[i] * p->col_k[x];
        if (u2 >= 1) continue;
        const uint8_t *c = p->lut[(int)(u2 * (LUT_SIZE - 1))];
        r += c[0]; g += c[1]; b += c[2];
        hit = true;
      }
      if (!hit && !sky) continue;
      const float th = (BAYER4[(y & 3) * 4 + (x & 3)] + 0.5f) / 16.0f;
      row.data[x] = 0xC0 | level(base[0] + r * att, th) << 4 | level(base[1] + g * att, th) << 2 |
                    level(base[2] + b * att, th);
    }
  }
  graphics_release_frame_buffer(ctx, fb);
}

void glow_draw(GContext *ctx, const GlowSpot *spots, int count, int horizon_y, int fade_y) {
  draw(ctx, spots, count, horizon_y, fade_y, NULL, NULL);
}

void glow_draw_sky(GContext *ctx, const GlowSpot *spots, int count, int horizon_y, int fade_y,
                   const uint8_t top[3], const uint8_t bottom[3]) {
  draw(ctx, spots, count, horizon_y, fade_y, top, bottom);
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
