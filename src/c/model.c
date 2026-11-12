#include "face.h"

static const struct { const char *id; CompType type; } COMP_IDS[] = {
  { "none", COMP_NONE },
  { "next_sun", COMP_NEXT_SUN },
  { "daylight", COMP_DAYLIGHT },
  { "golden", COMP_GOLDEN },
  { "sun_alt", COMP_SUN_ALT },
  { "moon", COMP_MOON },
  { "battery", COMP_BATTERY },
  { "steps", COMP_STEPS },
  { "distance", COMP_DISTANCE },
  { "heart", COMP_HEART },
  { "wx_hilo", COMP_WX_HILO },
  { "wx_precip", COMP_WX_PRECIP },
  { "wx_feels", COMP_WX_FEELS },
  { "wx_humidity", COMP_WX_HUMIDITY },
  { "wx_wind", COMP_WX_WIND },
  { "wx_uv", COMP_WX_UV },
  { "phone_battery", COMP_PHONE_BATTERY },
  { "next_event", COMP_NEXT_EVENT },
  { "pebble_1", COMP_PEBBLE_1 },
  { "pebble_2", COMP_PEBBLE_2 },
  { "pebble_3", COMP_PEBBLE_3 },
};

CompType comp_from_id(const char *id) {
  for (size_t i = 0; i < ARRAY_LENGTH(COMP_IDS); i++) {
    if (strcmp(COMP_IDS[i].id, id) == 0) {
      return COMP_IDS[i].type;
    }
  }
  return COMP_NONE;
}

void format_clock(double minute, bool h24, char *out, size_t len) {
  int m = (int)(minute + 0.5);
  m = ((m % 1440) + 1440) % 1440;
  int h = m / 60;
  if (h24) {
    snprintf(out, len, "%02d:%02d", h, m % 60);
  } else {
    h %= 12;
    snprintf(out, len, "%d:%02d", h == 0 ? 12 : h, m % 60);
  }
}

bool day_model_update(DayModel *day, const Location *loc, const struct tm *now, time_t utc) {
  const int tz = now->tm_gmtoff / 60;
  if (!loc->valid) {
    bool changed = day->valid;
    day->valid = false;
    return changed;
  }
  if (day->valid && day->year == now->tm_year && day->yday == now->tm_yday && day->tz == tz &&
      day->lat_e4 == loc->lat_e4 && day->lon_e4 == loc->lon_e4) {
    return false;
  }

  day->year = now->tm_year;
  day->yday = now->tm_yday;
  day->tz = tz;
  day->lat_e4 = loc->lat_e4;
  day->lon_e4 = loc->lon_e4;
  day->lat = loc->lat_e4 / 10000.0;
  day->lon = loc->lon_e4 / 10000.0;
  const double lat = day->lat, lon = day->lon;

  // Unix time of local midnight today; days either side are +/- 86400 s.
  const double midnight = (double)utc - (now->tm_hour * 3600 + now->tm_min * 60 + now->tm_sec);
  SolarDay today;
  solar_at(midnight + 12 * 3600, &today);

  for (int x = 0; x < GRAPH_COLUMNS; x++) {
    const double minute = (x + 0.5) * 1440.0 / GRAPH_COLUMNS;
    day->alt[x] = (float)solar_altitude(minute, lat, lon, tz, &today);
    MoonState m;
    moon_at(midnight + minute * 60.0, lat, lon, &m);
    day->moon_alt[x] = (float)m.altitude;
  }
  day->noon = solar_noon(lon, tz, &today);
  day->noon_alt = solar_altitude(day->noon, lat, lon, tz, &today);

  day->has_rise = solar_crossing(ZENITH_SUNRISE, true, lat, lon, tz, midnight, &day->rise) &&
                  solar_crossing(ZENITH_SUNRISE, false, lat, lon, tz, midnight, &day->set);
  day->has_dawn = solar_crossing(ZENITH_CIVIL, true, lat, lon, tz, midnight, &day->dawn) &&
                  solar_crossing(ZENITH_CIVIL, false, lat, lon, tz, midnight, &day->dusk);
  day->has_gold_am = solar_crossing(ZENITH_GOLDEN, true, lat, lon, tz, midnight, &day->gold_am);
  day->has_gold_pm = solar_crossing(ZENITH_GOLDEN, false, lat, lon, tz, midnight, &day->gold_pm);
  day->has_rise_tomorrow = solar_crossing(ZENITH_SUNRISE, true, lat, lon, tz, midnight + 86400,
                                          &day->rise_tomorrow);

  double y_rise, y_set;
  day->has_len_yesterday = solar_crossing(ZENITH_SUNRISE, true, lat, lon, tz, midnight - 86400, &y_rise) &&
                           solar_crossing(ZENITH_SUNRISE, false, lat, lon, tz, midnight - 86400, &y_set);
  day->len_yesterday = day->has_len_yesterday ? y_set - y_rise : 0;

  day->valid = true;
  return true;
}
