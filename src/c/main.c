// Sunpath: a solar-graph watchface for Pebble Time 2.
// The watch computes the sun itself from a saved location, so the face keeps working offline.
// The phone (src/pkjs) supplies the location, the weather and any plugin-sourced complications.

#include <pebble.h>
#include "config.h"
#include "face.h"
#include "solar.h"
#include "cartoon.h"

#define SETTINGS_VERSION 6   // 2: themes are Solar glow and Classic; 3: the moon; 4: glow style; 5: cartoon themes; 6: time and date formats
#define LOCATION_VERSION 1
#define WEATHER_VERSION 1
#define SLOT_TEXT_VERSION 1

#define FLICK_MS 5000
#define ANIM_FRAME_MS 33
#ifndef ANIM_SLOW
#define ANIM_SLOW 1   // >1 slows the wake-up down, for checking it frame by frame
#endif
#define WEATHER_REFRESH_S (60 * 60)   // ask the phone when the reading is older than this
#define REQUEST_BACKOFF_S (30 * 60)   // and at most this often

static Window *s_window;
static Layer *s_canvas;

static Settings s_settings;
static Location s_location;
static Weather s_weather;
static SlotText s_slot_text;
static DayModel s_day;

static bool s_flick;
static AppTimer *s_flick_timer;
static AppTimer *s_anim_timer;
static uint64_t s_anim_start;   // ms; the wake-up runs while s_anim_timer is set
static bool s_tap_subscribed;
static time_t s_last_request;

// ------------------------------------------------------------------------------- persistence

static void defaults(void) {
  s_settings = (Settings) {
    .version = SETTINGS_VERSION,
    .theme = THEME_GLOW_CARTOON,
    .slot = { COMP_NEXT_SUN, COMP_BATTERY, COMP_STEPS },
    .flick = true,
    .horizon_labels = true,
    .weather_on = true,
    .moon = true,
    .glow_style = 0,  // GLOW_SOFT
  };
  s_location = (Location) { .version = LOCATION_VERSION };
  s_weather = (Weather) {
    .version = WEATHER_VERSION, .temp = WX_NONE, .hi = WX_NONE, .lo = WX_NONE, .feels = WX_NONE,
    .precip = WX_NONE, .humidity = WX_NONE, .uv = WX_NONE, .code = WX_UNKNOWN, .is_day = -1,
  };
  s_slot_text = (SlotText) { .version = SLOT_TEXT_VERSION };
}

// Reads a blob only if it exists, is the right size, and carries the expected version.
static void load_blob(uint32_t key, void *dst, size_t size, uint8_t version) {
  if (!persist_exists(key) || persist_get_size(key) != (int)size) return;
  uint8_t buf[sizeof(SlotText) > sizeof(Weather) ? sizeof(SlotText) : sizeof(Weather)];
  if (size > sizeof(buf)) return;
  persist_read_data(key, buf, size);
  if (buf[0] == version) memcpy(dst, buf, size);
}

static void load_all(void) {
  defaults();
  load_blob(PERSIST_SETTINGS, &s_settings, sizeof(s_settings), SETTINGS_VERSION);
  load_blob(PERSIST_LOCATION, &s_location, sizeof(s_location), LOCATION_VERSION);
  load_blob(PERSIST_WEATHER, &s_weather, sizeof(s_weather), WEATHER_VERSION);
  load_blob(PERSIST_SLOT_TEXT, &s_slot_text, sizeof(s_slot_text), SLOT_TEXT_VERSION);
  for (int i = 0; i < SLOT_COUNT; i++) {
    if (s_settings.slot[i] >= COMP_COUNT) s_settings.slot[i] = COMP_NONE;
  }
}

// ------------------------------------------------------------------------------------- drawing

static uint64_t now_ms(void) {
  time_t s;
  uint16_t ms;
  time_ms(&s, &ms);
  return (uint64_t)s * 1000 + ms;
}

static void canvas_update(Layer *layer, GContext *ctx) {
  const time_t now = time(NULL);
  struct tm local = *localtime(&now);
  day_model_update(&s_day, &s_location, &local, now);

  FaceState st = {
    .settings = &s_settings,
    .weather = &s_weather,
    .slot_text = &s_slot_text,
    .day = &s_day,
    .now = local,
    .utc = now,
    .minute = local.tm_hour * 60 + local.tm_min + local.tm_sec / 60.0,
    .h24 = s_settings.time_format == 1 ? false : s_settings.time_format == 2 ? true : clock_is_24h_style(),
    .flick = s_flick,
    .anim_ms = s_anim_timer ? (int)((now_ms() - s_anim_start) / ANIM_SLOW) : -1,
    .has_location = s_location.valid,
  };
  if (s_day.valid) {
    SolarDay sd;
    solar_at((double)now, &sd);
    st.alt_now = solar_altitude(st.minute, s_day.lat, s_day.lon, local.tm_gmtoff / 60, &sd);
    moon_at((double)now, s_day.lat, s_day.lon, &st.moon);
  }
  face_draw(ctx, layer_get_bounds(layer), layer_get_unobstructed_bounds(layer), &st);
}

static void redraw(void) {
  if (s_canvas) layer_mark_dirty(s_canvas);
}

// ------------------------------------------------------------------------------------- phone

static void request_data(void) {
  DictionaryIterator *it;
  if (app_message_outbox_begin(&it) != APP_MSG_OK) return;
  dict_write_uint8(it, MESSAGE_KEY_REQUEST, 1);
  if (app_message_outbox_send() == APP_MSG_OK) s_last_request = time(NULL);
}

static int32_t tuple_int(const Tuple *t) {
  switch (t->type) {
    case TUPLE_INT:
      return t->length == 1 ? t->value->int8 : t->length == 2 ? t->value->int16 : t->value->int32;
    case TUPLE_UINT:
      return t->length == 1 ? t->value->uint8 : t->length == 2 ? t->value->uint16 : (int32_t)t->value->uint32;
    case TUPLE_CSTRING:
      return atoi(t->value->cstring);
    default:
      return 0;
  }
}

static void tap_handler(AccelAxisType axis, int32_t direction);

static void apply_tap_subscription(void) {
  if (s_settings.flick && !s_tap_subscribed) {
    accel_tap_service_subscribe(tap_handler);
    s_tap_subscribed = true;
  } else if (!s_settings.flick && s_tap_subscribed) {
    accel_tap_service_unsubscribe();
    s_tap_subscribed = false;
    s_flick = false;
  }
}

static void inbox_received(DictionaryIterator *it, void *context) {
  bool loc = false, wx = false, slots = false, settings = false;
  Tuple *t;

  Tuple *lat = dict_find(it, MESSAGE_KEY_LAT_E4), *lon = dict_find(it, MESSAGE_KEY_LON_E4);
  if (lat && lon) {
    s_location.lat_e4 = tuple_int(lat);
    s_location.lon_e4 = tuple_int(lon);
    s_location.valid = true;
    loc = true;
  }

  #define WX_INT(key, field) if ((t = dict_find(it, MESSAGE_KEY_##key))) { s_weather.field = tuple_int(t); wx = true; }
  WX_INT(WX_TEMP, temp)
  WX_INT(WX_HI, hi)
  WX_INT(WX_LO, lo)
  WX_INT(WX_FEELS, feels)
  WX_INT(WX_CODE, code)
  WX_INT(WX_IS_DAY, is_day)
  WX_INT(WX_PRECIP, precip)
  WX_INT(WX_HUMIDITY, humidity)
  WX_INT(WX_UV, uv)
  WX_INT(WX_TIME, time)
  #undef WX_INT
  if ((t = dict_find(it, MESSAGE_KEY_WX_WIND))) {
    snprintf(s_weather.wind, sizeof(s_weather.wind), "%s", t->type == TUPLE_CSTRING ? t->value->cstring : "");
    wx = true;
  }

  for (int i = 0; i < SLOT_COUNT; i++) {
    if ((t = dict_find(it, MESSAGE_KEY_SLOT_VALUE + i)) && t->type == TUPLE_CSTRING) {
      snprintf(s_slot_text.value[i], sizeof(s_slot_text.value[i]), "%s", t->value->cstring);
      slots = true;
    }
    if ((t = dict_find(it, MESSAGE_KEY_SLOT_LABEL + i)) && t->type == TUPLE_CSTRING) {
      snprintf(s_slot_text.label[i], sizeof(s_slot_text.label[i]), "%s", t->value->cstring);
      slots = true;
    }
  }

  // Settings from the Clay page.
  if ((t = dict_find(it, MESSAGE_KEY_THEME)) && t->type == TUPLE_CSTRING) {
    static const char *THEMES[] = { "glow", "classic", "cartoon_light", "cartoon_dark", "glow_cartoon" };
    s_settings.theme = THEME_GLOW_CARTOON;
    for (int i = 0; i < (int)ARRAY_LENGTH(THEMES); i++) {
      if (strcmp(t->value->cstring, THEMES[i]) == 0) s_settings.theme = i;
    }
    settings = true;
  }
  for (int i = 0; i < SLOT_COUNT; i++) {
    if ((t = dict_find(it, MESSAGE_KEY_SLOT + i)) && t->type == TUPLE_CSTRING) {
      s_settings.slot[i] = comp_from_id(t->value->cstring);
      settings = true;
    }
  }
  if ((t = dict_find(it, MESSAGE_KEY_FLICK))) { s_settings.flick = tuple_int(t) != 0; settings = true; }
  if ((t = dict_find(it, MESSAGE_KEY_HORIZON_LABELS))) { s_settings.horizon_labels = tuple_int(t) != 0; settings = true; }
  if ((t = dict_find(it, MESSAGE_KEY_WEATHER_ON))) { s_settings.weather_on = tuple_int(t) != 0; settings = true; }
  if ((t = dict_find(it, MESSAGE_KEY_MOON))) { s_settings.moon = tuple_int(t) != 0; settings = true; }
  if ((t = dict_find(it, MESSAGE_KEY_GLOW_STYLE)) && t->type == TUPLE_CSTRING) {
    static const char *STYLES[] = { "soft", "bands", "rays", "ripples", "pixel", "scan" };
    s_settings.glow_style = 0;
    for (int i = 0; i < (int)ARRAY_LENGTH(STYLES); i++) {
      if (strcmp(t->value->cstring, STYLES[i]) == 0) s_settings.glow_style = i;
    }
    settings = true;
  }

  #define PICK(key, field, ...) if ((t = dict_find(it, MESSAGE_KEY_##key)) && t->type == TUPLE_CSTRING) { \
    static const char *IDS[] = { __VA_ARGS__ }; \
    s_settings.field = 0; \
    for (int i = 0; i < (int)ARRAY_LENGTH(IDS); i++) if (strcmp(t->value->cstring, IDS[i]) == 0) s_settings.field = i; \
    settings = true; }
  PICK(TIME_STYLE, time_style, "large_bold", "large_light", "large_digital", "medium_bold", "medium_digital", "small")
  PICK(TIME_FORMAT, time_format, "auto", "12", "24")
  PICK(DATE_FORMAT, date_format, "dow_day", "mon_day", "day_mon", "m_d", "d_m", "none")
  #undef PICK

  if (loc) persist_write_data(PERSIST_LOCATION, &s_location, sizeof(s_location));
  if (wx) persist_write_data(PERSIST_WEATHER, &s_weather, sizeof(s_weather));
  if (slots) persist_write_data(PERSIST_SLOT_TEXT, &s_slot_text, sizeof(s_slot_text));
  if (settings) {
    persist_write_data(PERSIST_SETTINGS, &s_settings, sizeof(s_settings));
    apply_tap_subscription();
  }
  if (loc || wx || slots || settings) redraw();
}

static void inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "inbox dropped: %d", (int)reason);
}

static void outbox_failed(DictionaryIterator *it, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "outbox failed: %d", (int)reason);
}

// ---------------------------------------------------------------------------------- services

static void tick_handler(struct tm *tick_time, TimeUnits changed) {
  const time_t now = time(NULL);
  const bool stale = s_weather.time == 0 || now - s_weather.time > WEATHER_REFRESH_S;
  if (s_settings.weather_on && stale && now - s_last_request > REQUEST_BACKOFF_S) {
    request_data();
  }
  redraw();
}

static bool cartoon_theme(void) {
  return s_settings.theme == THEME_CARTOON_LIGHT || s_settings.theme == THEME_CARTOON_DARK ||
         s_settings.theme == THEME_GLOW_CARTOON;
}

// The cartoon sun's wake-up: redraw every frame until it has run its course. Frames are timed
// from the start, so a slow frame skips ahead rather than stretching the animation.
static void anim_frame(void *data) {
  if (now_ms() - s_anim_start >= (uint64_t)CARTOON_ANIM_MS * ANIM_SLOW) {
    s_anim_timer = NULL;
  } else {
    s_anim_timer = app_timer_register(ANIM_FRAME_MS, anim_frame, NULL);
  }
  redraw();
}

static void start_wake_up(void) {
  if (!cartoon_theme()) return;
  s_anim_start = now_ms();
  if (s_anim_timer) app_timer_cancel(s_anim_timer);
  s_anim_timer = app_timer_register(ANIM_FRAME_MS, anim_frame, NULL);
  redraw();
}

static void flick_done(void *data) {
  s_flick_timer = NULL;
  s_flick = false;
  start_wake_up();  // the sun pops back up as the details card goes
  redraw();
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  s_flick = true;
  if (s_flick_timer) app_timer_reschedule(s_flick_timer, FLICK_MS);
  else s_flick_timer = app_timer_register(FLICK_MS, flick_done, NULL);
  redraw();
}

static void battery_handler(BatteryChargeState state) {
  redraw();
}

static bool slot_uses(CompType a, CompType b) {
  for (int i = 0; i < SLOT_COUNT; i++) {
    if (s_settings.slot[i] == a || s_settings.slot[i] == b) return true;
  }
  return false;
}

#if defined(PBL_HEALTH)
static void health_handler(HealthEventType event, void *context) {
  if ((event == HealthEventMovementUpdate && slot_uses(COMP_STEPS, COMP_DISTANCE)) ||
      (event == HealthEventHeartRateUpdate && slot_uses(COMP_HEART, COMP_HEART))) {
    redraw();
  }
}
#endif

static void unobstructed_change(AnimationProgress progress, void *context) {
  redraw();
}

// ------------------------------------------------------------------------------------ window

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);
}

static void window_appear(Window *window) {
  start_wake_up();
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
  s_canvas = NULL;
}

static void init(void) {
  load_all();

  s_window = window_create();
  window_set_background_color(s_window, GColorBlack);
  window_set_window_handlers(s_window, (WindowHandlers) { .load = window_load, .appear = window_appear, .unload = window_unload });
  window_stack_push(s_window, false);

  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_handler);
  unobstructed_area_service_subscribe((UnobstructedAreaHandlers) { .change = unobstructed_change }, NULL);
#if defined(PBL_HEALTH)
  health_service_events_subscribe(health_handler, NULL);
#endif
  apply_tap_subscription();

  app_message_register_inbox_received(inbox_received);
  app_message_register_inbox_dropped(inbox_dropped);
  app_message_register_outbox_failed(outbox_failed);
  app_message_open(2048, 128);
  request_data();
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  unobstructed_area_service_unsubscribe();
#if defined(PBL_HEALTH)
  health_service_events_unsubscribe();
#endif
  if (s_tap_subscribed) accel_tap_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
