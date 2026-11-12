// Host build of src/c/solar.c. Reads "name lat lon tz_minutes yday" lines on stdin and prints
// sunrise, sunset, solar noon (local minutes, -1 when none) and the noon altitude.
// Input lines: name lat lon tz_minutes local_midnight_unix_seconds
#include <stdio.h>
#include "../src/c/solar.h"

int main(void) {
  char name[64]; double lat, lon, midnight; int tz;
  while (scanf("%63s %lf %lf %d %lf", name, &lat, &lon, &tz, &midnight) == 5) {
    SolarDay sd; solar_at(midnight + 12 * 3600, &sd);
    double rise = -1, set = -1;
    bool ok = solar_crossing(ZENITH_SUNRISE, true, lat, lon, tz, midnight, &rise) &&
              solar_crossing(ZENITH_SUNRISE, false, lat, lon, tz, midnight, &set);
    double noon = solar_noon(lon, tz, &sd);
    printf("%s %.2f %.2f %.2f %.2f\n", name, ok ? rise : -1, ok ? set : -1, noon,
           solar_altitude(noon, lat, lon, tz, &sd));
  }
  return 0;
}
