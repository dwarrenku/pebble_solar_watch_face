#include "face.h"

static void set(char *value, size_t vlen, const char *v, char *label, size_t llen, const char *l) {
  snprintf(value, vlen, "%s", v);
  snprintf(label, llen, "%s", l);
}

static void duration(double minutes, char *out, size_t len) {
  int m = minutes < 0 ? 0 : (int)(minutes + 0.5);
  snprintf(out, len, "%dh%02d", m / 60, m % 60);
}

static void degrees(int v, char *out, size_t len) {
  if (v == WX_NONE) snprintf(out, len, "--");
  else snprintf(out, len, "%d°", v);
}

// A weather number is only shown while the reading is less than 12 hours old.
static bool weather_fresh(const FaceState *st) {
  return st->weather->time > 0 && st->utc - st->weather->time < 12 * 60 * 60;
}

static void health_number(HealthMetric metric, char *out, size_t len) {
#if defined(PBL_HEALTH)
  const time_t start = time_start_of_today(), end = time(NULL);
  HealthServiceAccessibilityMask mask = health_service_metric_accessible(metric, start, end);
  if (!(mask & HealthServiceAccessibilityMaskAvailable)) {
    snprintf(out, len, "--");
    return;
  }
  const int v = (int)health_service_sum_today(metric);
  if (metric == HealthMetricWalkedDistanceMeters) {
    const bool miles = health_service_get_measurement_system_for_display(metric) == MeasurementSystemImperial;
    const int tenths = miles ? (v * 10 + 804) / 1609 : (v + 50) / 100;
    snprintf(out, len, "%d.%d", tenths / 10, tenths % 10);
  } else if (v >= 100000) {
    snprintf(out, len, "%dk", v / 1000);
  } else if (v >= 1000) {
    snprintf(out, len, "%d,%03d", v / 1000, v % 1000);
  } else {
    snprintf(out, len, "%d", v);
  }
#else
  snprintf(out, len, "--");
#endif
}

void comp_text(CompType type, int slot, const FaceState *st, char *value, size_t vlen,
               char *label, size_t llen) {
  const DayModel *d = st->day;
  const Weather *w = st->weather;
  const bool sun_known = st->has_location && d->valid;
  char buf[16];
  value[0] = label[0] = '\0';

  switch (type) {
    case COMP_NONE:
    case COMP_COUNT:
      return;

    case COMP_NEXT_SUN:
      if (!sun_known) { set(value, vlen, "--", label, llen, "SUNSET"); return; }
      if (!d->has_rise) {
        set(value, vlen, "--", label, llen, d->noon_alt < 0 ? "NO SUNRISE" : "NO SUNSET");
      } else if (st->minute < d->rise) {
        format_clock(d->rise, st->h24, value, vlen); snprintf(label, llen, "SUNRISE");
      } else if (st->minute < d->set) {
        format_clock(d->set, st->h24, value, vlen); snprintf(label, llen, "SUNSET");
      } else if (d->has_rise_tomorrow) {
        format_clock(d->rise_tomorrow, st->h24, value, vlen); snprintf(label, llen, "SUNRISE");
      } else {
        set(value, vlen, "--", label, llen, "NO SUNRISE");
      }
      return;

    case COMP_DAYLIGHT:
      if (!sun_known) { set(value, vlen, "--", label, llen, "DAYLIGHT"); return; }
      if (!d->has_rise) {
        set(value, vlen, d->noon_alt < 0 ? "0h00" : "24h00", label, llen, "DAYLIGHT");
      } else if (st->minute >= d->rise && st->minute < d->set) {
        duration(d->set - st->minute, value, vlen); snprintf(label, llen, "DAY LEFT");
      } else {
        duration(d->set - d->rise, value, vlen); snprintf(label, llen, "DAYLIGHT");
      }
      return;

    case COMP_GOLDEN: {
      snprintf(label, llen, "GOLDEN HR");
      double next = -1;
      if (sun_known && d->has_gold_am && d->gold_am > st->minute) next = d->gold_am;
      else if (sun_known && d->has_gold_pm && d->gold_pm > st->minute) next = d->gold_pm;
      if (next < 0) snprintf(value, vlen, "--");
      else format_clock(next, st->h24, value, vlen);
      return;
    }

    case COMP_SUN_ALT:
      snprintf(label, llen, "SUN");
      if (sun_known) degrees((int)(st->alt_now < 0 ? st->alt_now - 0.5 : st->alt_now + 0.5), value, vlen);
      else snprintf(value, vlen, "--");
      return;

    case COMP_MOON: {
      // The phase does not depend on where you are, so this works before the phone sends a place.
      MoonState m = st->moon;
      if (!sun_known) moon_at((double)st->utc, 0, 0, &m);
      const float lit = m.illuminated;
      snprintf(value, vlen, "%d%%", (int)(lit * 100 + 0.5f));
      snprintf(label, llen, "%s", lit < 0.03f ? "NEW" : lit > 0.97f ? "FULL" : m.waxing ? "WAXING" : "WANING");
      return;
    }

    case COMP_BATTERY:
      snprintf(value, vlen, "%d%%", battery_state_service_peek().charge_percent);
      snprintf(label, llen, battery_state_service_peek().is_charging ? "CHARGING" : "BATTERY");
      return;

    case COMP_STEPS:
      health_number(HealthMetricStepCount, value, vlen);
      snprintf(label, llen, "STEPS");
      return;

    case COMP_DISTANCE: {
      health_number(HealthMetricWalkedDistanceMeters, value, vlen);
#if defined(PBL_HEALTH)
      const bool miles = health_service_get_measurement_system_for_display(HealthMetricWalkedDistanceMeters)
                         == MeasurementSystemImperial;
      snprintf(label, llen, miles ? "MI" : "KM");
#else
      snprintf(label, llen, "KM");
#endif
      return;
    }

    case COMP_HEART: {
      snprintf(label, llen, "BPM");
#if defined(PBL_HEALTH)
      const HealthValue bpm = health_service_peek_current_value(HealthMetricHeartRateBPM);
      if (bpm > 0) { snprintf(value, vlen, "%d", (int)bpm); return; }
#endif
      snprintf(value, vlen, "--");
      return;
    }

    case COMP_WX_HILO:
      snprintf(label, llen, "HI / LO");
      if (weather_fresh(st) && w->hi != WX_NONE && w->lo != WX_NONE) snprintf(value, vlen, "%d/%d", w->hi, w->lo);
      else snprintf(value, vlen, "--");
      return;

    case COMP_WX_PRECIP:
      snprintf(label, llen, "RAIN");
      if (weather_fresh(st) && w->precip != WX_NONE) snprintf(value, vlen, "%d%%", w->precip);
      else snprintf(value, vlen, "--");
      return;

    case COMP_WX_FEELS:
      snprintf(label, llen, "FEELS");
      degrees(weather_fresh(st) ? w->feels : WX_NONE, value, vlen);
      return;

    case COMP_WX_HUMIDITY:
      snprintf(label, llen, "HUMIDITY");
      if (weather_fresh(st) && w->humidity != WX_NONE) snprintf(value, vlen, "%d%%", w->humidity);
      else snprintf(value, vlen, "--");
      return;

    case COMP_WX_WIND:
      snprintf(label, llen, "WIND");
      snprintf(value, vlen, "%s", weather_fresh(st) && w->wind[0] ? w->wind : "--");
      return;

    case COMP_WX_UV:
      snprintf(label, llen, "UV");
      if (weather_fresh(st) && w->uv != WX_NONE) snprintf(value, vlen, "%d", w->uv);
      else snprintf(value, vlen, "--");
      return;

    case COMP_PHONE_BATTERY:
    case COMP_NEXT_EVENT:
    case COMP_PEBBLE_1:
    case COMP_PEBBLE_2:
    case COMP_PEBBLE_3: {
      // The phone reads the plugin and sends finished text for this slot.
      static const char *DEFAULT_LABEL[] = { "PHONE", "NEXT EVENT", "PEBBLE 1", "PEBBLE 2", "PEBBLE 3" };
      const SlotText *t = st->slot_text;
      snprintf(value, vlen, "%s", t->value[slot][0] ? t->value[slot] : "--");
      snprintf(buf, sizeof(buf), "%s", DEFAULT_LABEL[type - COMP_PHONE_BATTERY]);
      snprintf(label, llen, "%s", t->label[slot][0] ? t->label[slot] : buf);
      return;
    }
  }
}
