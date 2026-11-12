#pragma once
// The sun's position from date, place and clock, using the Astronomical Almanac's low-precision
// solar coordinates (good to about 0.01 degrees from 1950 to 2050). Times are continuous days since
// J2000, so leap years need no special handling. Checked against PyEphem by test/solar_check.py.

#include <pebble.h>

#define ZENITH_SUNRISE 90.833   // upper limb on the horizon, with refraction
#define ZENITH_CIVIL   96.0     // civil twilight: first and last light
#define ZENITH_GOLDEN  84.0     // sun at 6 degrees: the edge of golden hour

typedef struct {
  double eq_time;     // equation of time, minutes (apparent minus mean solar time)
  double declination; // radians
} SolarDay;

// The sun's declination and equation of time at a moment, given as Unix seconds (UTC).
void solar_at(double unix_seconds, SolarDay *out);

// Sun altitude in degrees at `minute` minutes after local midnight, with the sun as given by `sd`.
// `tz_minutes` is the local clock's offset east of UTC.
double solar_altitude(double minute, double lat, double lon, int tz_minutes, const SolarDay *sd);

// Local minute (after `local_midnight`, Unix seconds) at which the sun's centre reaches `zenith`,
// rising or setting. The sun is evaluated at the event itself (one refinement step). Returns
// false when it never happens that day (polar day or night).
bool solar_crossing(double zenith, bool rising, double lat, double lon, int tz_minutes,
                    double local_midnight, double *out_minute);

// Local minute of solar noon, with the sun as given by `sd`.
double solar_noon(double lon, int tz_minutes, const SolarDay *sd);
