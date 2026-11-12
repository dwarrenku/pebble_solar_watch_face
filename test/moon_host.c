// Host build of src/c/moon.c. Reads "name lat lon unix_seconds" lines on stdin and prints the
// moon's altitude, illuminated fraction and whether it is waxing.
#include <stdio.h>
#include "../src/c/moon.h"

int main(void) {
  char name[64]; double lat, lon, t;
  while (scanf("%63s %lf %lf %lf", name, &lat, &lon, &t) == 4) {
    MoonState m;
    moon_at(t, lat, lon, &m);
    printf("%s %.3f %.4f %d\n", name, m.altitude, m.illuminated, m.waxing ? 1 : 0);
  }
  return 0;
}
