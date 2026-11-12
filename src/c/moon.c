#include "moon.h"
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define DEG (M_PI / 180.0)
#define UNIX_J2000 946728000.0   // 2000-01-01 12:00 UTC

static double s(double deg) { return sin(deg * DEG); }

void moon_at(double unix_seconds, double lat, double lon, MoonState *out) {
  const double d = (unix_seconds - UNIX_J2000) / 86400.0;
  // Mean elements, degrees.
  const double L = 218.316 + 13.176396 * d;      // moon's mean longitude
  const double M = 134.963 + 13.064993 * d;      // moon's mean anomaly
  const double F = 93.272 + 13.229350 * d;       // argument of latitude
  const double D = 297.850 + 12.190749 * d;      // mean elongation from the sun
  const double Ms = 357.529 + 0.98560028 * d;    // sun's mean anomaly

  // Ecliptic longitude and latitude with the largest perturbations.
  const double lambda = L + 6.289 * s(M) + 0.214 * s(2 * M)
      - 1.274 * s(M - 2 * D) + 0.658 * s(2 * D) - 0.186 * s(Ms)
      - 0.059 * s(2 * M - 2 * D) - 0.057 * s(M - 2 * D + Ms) + 0.053 * s(M + 2 * D)
      + 0.046 * s(2 * D - Ms) + 0.041 * s(M - Ms) - 0.035 * s(D) - 0.031 * s(M + Ms)
      - 0.015 * s(2 * F - 2 * D) + 0.011 * s(M - 4 * D);
  const double beta = 5.128 * s(F) - 0.173 * s(F - 2 * D) - 0.055 * s(M - F - 2 * D)
      - 0.046 * s(M + F - 2 * D) + 0.033 * s(F + 2 * D) + 0.017 * s(2 * M + F);

  // To right ascension and declination.
  const double eps = (23.439 - 0.0000004 * d) * DEG;
  const double lr = lambda * DEG, br = beta * DEG;
  const double ra = atan2(sin(lr) * cos(eps) - tan(br) * sin(eps), cos(lr));
  const double dec = asin(sin(br) * cos(eps) + cos(br) * sin(eps) * sin(lr));

  // Local hour angle and altitude, then the moon's parallax (about a degree).
  const double gmst = fmod(280.46061837 + 360.98564736629 * d, 360.0);
  const double ha = (gmst + lon) * DEG - ra;
  const double phi = lat * DEG;
  double sa = sin(phi) * sin(dec) + cos(phi) * cos(dec) * cos(ha);
  if (sa > 1) sa = 1;
  if (sa < -1) sa = -1;
  const double alt = asin(sa) / DEG;
  const double parallax = 0.9508 + 0.0518 * cos(M * DEG);
  out->altitude = alt - parallax * cos(alt * DEG);

  // Phase, from the moon's elongation from the sun.
  const double g = Ms * DEG;
  const double sun_lambda = 280.460 + 0.9856474 * d + 1.915 * sin(g) + 0.020 * sin(2 * g);
  const double delta = (lambda - sun_lambda) * DEG;
  const double cos_elong = cos(br) * cos(delta);
  out->illuminated = (float)((1.0 - cos_elong) / 2.0);
  out->waxing = sin(delta) > 0;
}
