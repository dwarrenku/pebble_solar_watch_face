#pragma once
// Shared types for Sunpath: settings, saved data, and the complication catalogue.
// The ids here match the `value`s on the settings page (src/pkjs/config.js) and
// the design spec (design/spec.md).

#include <pebble.h>

#define SLOT_COUNT 3
#define WX_NONE (-1000)            // "no reading" for any weather number

// Persist keys. Each blob is versioned so a later release can grow it safely.
enum {
  PERSIST_SETTINGS = 1,
  PERSIST_LOCATION = 2,
  PERSIST_WEATHER = 3,
  PERSIST_SLOT_TEXT = 4,
};

typedef enum {
  THEME_GLOW = 0,          // the Solar face: black, a radial glow around the sun, thin grey lines
  THEME_CLASSIC = 1,       // black, the path in amber
  THEME_CARTOON_LIGHT = 2, // after the PebbleOS timeline: white, black linework, the cartoon sun
  THEME_CARTOON_DARK = 3,  // the same on black
  THEME_GLOW_CARTOON = 4,  // the Solar glow with the cartoon sun and moon
} Theme;

typedef enum {
  COMP_NONE = 0,
  COMP_NEXT_SUN,
  COMP_DAYLIGHT,
  COMP_GOLDEN,
  COMP_SUN_ALT,
  COMP_MOON,
  COMP_BATTERY,
  COMP_STEPS,
  COMP_DISTANCE,
  COMP_HEART,
  COMP_WX_HILO,
  COMP_WX_PRECIP,
  COMP_WX_FEELS,
  COMP_WX_HUMIDITY,
  COMP_WX_WIND,
  COMP_WX_UV,
  COMP_PHONE_BATTERY,  // plugin readings: the phone sends the text
  COMP_NEXT_EVENT,
  COMP_PEBBLE_1,
  COMP_PEBBLE_2,
  COMP_PEBBLE_3,
  COMP_COUNT,
} CompType;

// Condition codes, as the weather plugin names them (sent as these numbers).
typedef enum {
  WX_UNKNOWN = 0,
  WX_SUN,
  WX_PARTLY_CLOUDY,
  WX_CLOUDY,
  WX_LIGHT_RAIN,
  WX_HEAVY_RAIN,
  WX_LIGHT_SNOW,
  WX_HEAVY_SNOW,
  WX_RAIN_AND_SNOW,
} WxCode;

typedef struct {
  uint8_t version;
  uint8_t theme;
  uint8_t slot[SLOT_COUNT];
  bool flick;
  bool horizon_labels;
  bool weather_on;
  bool moon;
  uint8_t glow_style;   // GlowStyle (glow.h)
  uint8_t time_style;   // TimeStyle
  uint8_t time_format;  // 0 as the watch is set, 1 12-hour, 2 24-hour
  uint8_t date_format;  // DateFormat
} Settings;

typedef enum {
  TIME_LARGE_BOLD = 0, TIME_LARGE_LIGHT, TIME_LARGE_DIGITAL, TIME_MEDIUM_BOLD, TIME_MEDIUM_DIGITAL, TIME_SMALL,
  TIME_STYLE_COUNT,
} TimeStyle;

typedef enum {
  DATE_DOW_DAY = 0,  // WED 11
  DATE_MON_DAY,      // NOV 11
  DATE_DAY_MON,      // 11 NOV
  DATE_M_D,          // 11/11, month first
  DATE_D_M,          // 11/11, day first
  DATE_NONE,
  DATE_FORMAT_COUNT,
} DateFormat;

typedef struct {
  uint8_t version;
  bool valid;
  int32_t lat_e4;   // degrees x 10000
  int32_t lon_e4;
} Location;

typedef struct {
  uint8_t version;
  int16_t temp, hi, lo, feels;   // whole degrees in the display unit, or WX_NONE
  int16_t precip, humidity, uv;  // percent, percent, index, or WX_NONE
  int8_t code;                   // WxCode
  int8_t is_day;                 // 1, 0, or -1 when the source didn't say
  char wind[12];                 // e.g. "9 SW"
  int32_t time;                  // epoch seconds of the reading; 0 = never
} Weather;

typedef struct {
  uint8_t version;
  char value[SLOT_COUNT][16];
  char label[SLOT_COUNT][20];
} SlotText;

// Maps a settings-page id ("next_sun") to a CompType; unknown ids map to COMP_NONE.
CompType comp_from_id(const char *id);
