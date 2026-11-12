#pragma once
// The cartoon sun and moon, after PebbleOS's own illustrations (the timeline's day-separator sun):
// one thick stroke with round ends, faceted shapes instead of curves, faces, short dash rays, flat
// fills. The design simulator's drawPebble() (design/sunpath-simulator.html) is the visual reference.

#include <pebble.h>
#include "config.h"
#include "moon.h"

#define CARTOON_SUN_R 15      // the sun's radius
#define CARTOON_ANIM_MS 1400  // how long the wake-up runs

// The wake-up, after PebbleOS's animations: the body springs up from behind the horizon with a
// little overshoot, the rays pop out one by one, then the eyes open and blink. `anim_ms` is the time
// since it started, or -1 when it is not running. Returns how far the body has risen (0 to 1, a
// little over 1 while it overshoots).
float cartoon_rise(int anim_ms);

// The sun with its face: eyes shut near the horizon (and while waking), open once it is up, and a
// wider smile once it is high. Its rays lengthen as it climbs.
void cartoon_sun(GContext *ctx, int cx, int cy, double alt, GColor outline, GColor rays, int anim_ms);

// Below the horizon: the sun asleep, in outline, with a small z.
void cartoon_sun_asleep(GContext *ctx, int cx, int cy, GColor ink);

// The faceted moon, lit with its real phase (mirrored south of the equator); a face once it is
// mostly lit.
void cartoon_moon(GContext *ctx, int cx, int cy, const MoonState *m, double lat, GColor outline, GColor dark);

// Pebble's chamfered clouds over a body at (sx, sy), with rain dashes or snow pluses under them,
// kept above the horizon.
void cartoon_weather(GContext *ctx, WxCode code, int sx, int sy, int r, int horizon_y, GColor ink, GColor paper);

// A small line-drawn weather icon for the header, in a 22 px box at (x, y).
void cartoon_wx_icon(GContext *ctx, WxCode code, int x, int y, GColor ink, GColor paper, bool is_day);

// A night-sky sparkle.
void cartoon_star(GContext *ctx, int x, int y, GColor ink, bool big);
