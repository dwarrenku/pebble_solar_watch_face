#include "solar.h"
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEG (M_PI / 180.0)
#define UNIX_J2000 946728000.0   // 2000-01-01 12:00 UTC

static double wrap180(double deg) {
  deg = fmod(deg + 180.0, 360.0);
  if (deg < 0) deg += 360.0;
  return deg - 180.0;
}

void solar_at(double unix_seconds, SolarDay *out) {
  const double n = (unix_seconds - UNIX_J2000) / 86400.0;          // days since J2000
  const double L = fmod(280.460 + 0.9856474 * n, 360.0);            // mean longitude
  const double g = fmod(357.528 + 0.9856003 * n, 360.0) * DEG;      // mean anomaly
  const double lambda = (L + 1.915 * sin(g) + 0.020 * sin(2 * g)) * DEG;  // ecliptic longitude
  const double eps = (23.439 - 0.0000004 * n) * DEG;                // obliquity
  const double ra = atan2(cos(eps) * sin(lambda), cos(lambda)) / DEG;
  out->declination = asin(sin(eps) * sin(lambda));
  out->eq_time = 4.0 * wrap180(L - ra);
}

double solar_altitude(double minute, double lat, double lon, int tz_minutes, const SolarDay *sd) {
  const double true_solar = minute + sd->eq_time + 4.0 * lon - tz_minutes;
  const double hour_angle = (true_solar / 4.0 - 180.0) * DEG;
  const double phi = lat * DEG;
  double s = sin(phi) * sin(sd->declination) + cos(phi) * cos(sd->declination) * cos(hour_angle);
  if (s > 1.0) s = 1.0;
  if (s < -1.0) s = -1.0;
  return asin(s) / DEG;
}

static bool crossing_once(double zenith, bool rising, double lat, double lon, int tz_minutes,
                          const SolarDay *sd, double *out_minute) {
  const double phi = lat * DEG;
  const double c = cos(zenith * DEG) / (cos(phi) * cos(sd->declination))
                   - tan(phi) * tan(sd->declination);
  if (c > 1.0 || c < -1.0) {
    return false;
  }
  const double ha = acos(c) / DEG;
  *out_minute = 720.0 - 4.0 * (lon + (rising ? ha : -ha)) - sd->eq_time + tz_minutes;
  return true;
}

bool solar_crossing(double zenith, bool rising, double lat, double lon, int tz_minutes,
                    double local_midnight, double *out_minute) {
  SolarDay sd;
  solar_at(local_midnight + 12 * 3600, &sd);
  double t;
  if (!crossing_once(zenith, rising, lat, lon, tz_minutes, &sd, &t)) {
    return false;
  }
  // Again with the sun as it is at the moment of the event.
  solar_at(local_midnight + t * 60.0, &sd);
  if (!crossing_once(zenith, rising, lat, lon, tz_minutes, &sd, &t)) {
    return false;
  }
  *out_minute = t;
  return true;
}

double solar_noon(double lon, int tz_minutes, const SolarDay *sd) {
  return 720.0 - 4.0 * lon - sd->eq_time + tz_minutes;
}
