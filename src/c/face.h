#pragma once
// Everything the face draws from, and the drawing entry point.

#include <pebble.h>
#include "config.h"
#include "solar.h"
#include "moon.h"

#define GRAPH_COLUMNS 200

// Facts about today's sun, recomputed only when the date, time zone or place changes.
typedef struct {
  bool valid;
  int year, yday, tz;            // cache key, with lat/lon below
  int32_t lat_e4, lon_e4;
  double lat, lon;
  float alt[GRAPH_COLUMNS];      // sun altitude (degrees) at each column's minute
  float moon_alt[GRAPH_COLUMNS]; // moon altitude at each column's minute
  double noon, noon_alt;
  bool has_rise; double rise, set;
  bool has_dawn; double dawn, dusk;
  bool has_gold_am; double gold_am;
  bool has_gold_pm; double gold_pm;
  bool has_rise_tomorrow; double rise_tomorrow;
  bool has_len_yesterday; double len_yesterday;
} DayModel;

typedef struct {
  const Settings *settings;
  const Weather *weather;
  const SlotText *slot_text;
  const DayModel *day;
  struct tm now;        // local time
  time_t utc;
  double minute;        // minutes since local midnight, with seconds
  double alt_now;       // sun altitude now
  MoonState moon;       // the moon now
  bool h24;
  bool flick;           // the wrist-flick card is showing
  int anim_ms;          // time into the cartoon sun's wake-up, or -1
  bool has_location;
} FaceState;

// Rebuilds `day` if its cache key no longer matches. Returns true if it changed.
bool day_model_update(DayModel *day, const Location *loc, const struct tm *now, time_t utc);

void face_draw(GContext *ctx, GRect bounds, GRect unobstructed, const FaceState *st);

// Complication text for one slot (complications.c).
void comp_text(CompType type, int slot, const FaceState *st, char *value, size_t value_len,
               char *label, size_t label_len);

// Formats a minute-of-day as a clock time, honouring 12/24 h (no leading zero in 12 h).
void format_clock(double minute, bool h24, char *out, size_t len);
