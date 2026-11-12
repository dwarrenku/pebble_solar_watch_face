#pragma once
// The moon's position and phase, from a low-precision lunar theory: the mean elements plus the
// largest periodic terms (Paul Schlyter's "How to compute planetary positions"), good to a few
// tenths of a degree. Checked against PyEphem by test/solar_check.py.

#include <pebble.h>

typedef struct {
  double altitude;    // degrees above the horizon, topocentric (parallax applied), no refraction
  float illuminated;  // 0 (new) .. 1 (full)
  bool waxing;        // the lit side is growing (the sun's side is west of the moon)
} MoonState;

// The moon as seen from (lat, lon) at `unix_seconds`.
void moon_at(double unix_seconds, double lat, double lon, MoonState *out);
