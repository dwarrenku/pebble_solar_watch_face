#pragma once
// The glows, after the Solar face: radial gradients clipped at the horizon. The sun's colours and
// shape follow its altitude (pink and orange flat along the horizon at sunrise and dusk, a yellow
// core in cyan and violet by day, a magenta afterglow in twilight, nothing at night). The moon's is
// small and soft, amber when low and silver-violet when high, and only shows in a dark sky.
// The design simulator (design/sunpath-simulator.html, GLOW and MOON_GLOW) uses the same tables.

#include <pebble.h>

typedef struct {
  float x, y;        // centre in screen pixels; may be below the horizon
  double alt;        // the body's altitude, which picks the colours
  float strength;    // 0..1, scales the whole glow
  bool moon;         // use the moon's table rather than the sun's
} GlowSpot;

// Paints the glows straight into the frame buffer for rows above `horizon_y`, adding them and
// ordered-dithering once into the 64 colours. Rows above `fade_y` are dimmed toward the top so text
// there stays legible.
void glow_draw(GContext *ctx, const GlowSpot *spots, int count, int horizon_y, int fade_y);

// Glow styles. Soft is the dithered gradient above; the others are flat, in Pebble's design language:
// a few flat palette colours with crisp edges, which also hold up best with the backlight off.
typedef enum {
  GLOW_SOFT = 0,
  GLOW_BANDS,     // flat concentric rings
  GLOW_RAYS,      // a sunburst of alternating rays, turning a degree a minute
  GLOW_RIPPLES,   // a few thick rings with black between
  GLOW_PIXEL,     // 4 x 4 blocks with checkerboard transitions
  GLOW_SCAN,      // horizontal stripes, thinner toward the edge
  GLOW_STYLE_COUNT,
} GlowStyle;

// Paints a flat-style glow for the sun (and, if `moon` is not NULL, a smaller one for the moon)
// into the frame buffer above `horizon_y`. The sun's colours step through the day by altitude.
void glow_draw_flat(GContext *ctx, GlowStyle style, const GlowSpot *sun, const GlowSpot *moon,
                    int horizon_y, int minute_of_day);
