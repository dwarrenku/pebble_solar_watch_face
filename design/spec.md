## Handoff Spec: Sunpath watchface (Pebble Time 2)

### Overview
Sunpath is a watchface for the Pebble Time 2 (`emery`: 200 × 228 px, 64 colours, reflective panel, always on).
It draws the sun's real elevation for the wearer's place and date as a curve across the day (local midnight at
the left edge, the next midnight at the right), with a fixed horizon and the sun marking *now*. Around it are the time,
the date, the current weather and three complications chosen on a settings page.

Who and when: someone glancing at their wrist, often outdoors and often with the backlight off. Every reading has
to work in about one second. The face has no buttons or touch (watchfaces get neither), so the only interaction is
a wrist flick, which shows a sun-details card for five seconds.

Source of truth for visuals: `design/sunpath-simulator.html` (`drawFinal()`; layout constants `L`). Rationale:
`design/critique.md`.

### Layout
Fixed canvas, one platform. All numbers are watch pixels from the top-left of the screen.

| Region | Rect (x, y, w, h) | Contents |
|---|---|---|
| Header, left | 0, 0, 74, 68 | Date at top (y 3); weather icon and temperature (y 24); high and low (y 49) |
| Header, right | 74, 0, 126, 50 | Time, right-aligned to x = 194, as on the Solar face |
| Glow | 0, 0, 200, 130 | The sun's radial glow (Solar glow theme), everywhere above the horizon, under the text |
| Graph | 0, 70, 200, 112 | Horizon, sun path, sun, horizon labels, flick card |
| Complications | 0, 184, 200, 44 | Three slots centred on x = 34, 100, 166, 66 px wide each |

Graph geometry:
- `HORIZON_Y = 130`. `AMP_UP = 56`, `AMP_DOWN = 42`.
- For a sun altitude `a`, `y = HORIZON_Y − sin(a) × (a ≥ 0 ? AMP_UP : AMP_DOWN)`. Daytime gets more height than
  night because that is where the interest is, and the scale is never normalised per day, so a November arc is
  visibly low and a June arc tall.
- Column `x` (0…199) samples the minute `(x + 0.5) × 1440 / 200`. The sun sits at `x = minuteOfDay × 200 / 1440`.

Unobstructed area (Timeline Quick View): when the unobstructed height drops below 228, hide the complication row.
Header and graph never move. When it returns to 228, show the row again.

### Design tokens used
Colours are SDK `GColor` constants; all are in the 64-colour set.

| Token | Value | Usage |
|---|---|---|
| `color-ground` | `GColorBlack` | Background, header, ground below the horizon |
| `color-text` | `GColorWhite` | Time, temperature, complication values, flick values |
| `color-label` | `GColorLightGray` | Complication labels, high/low, horizon labels, flick labels, night sun ring |
| `color-rule` | `GColorDarkGray` | Horizon line (Classic), flick card top rule, "--°" placeholder. Never for text that must be read |
| `color-accent` | `GColorChromeYellow` | Date; past sun path by day (Classic); sun halo and core between 4° and 12° |
| `color-sun-high` | `GColorYellow` | Sun core above 12° |
| `color-sun-low` | `GColorOrange` | Sun core 0–4°, halo 0–12° |
| `color-sun-twilight` | `GColorRajah` | Sun ring when −6° < altitude < 0° |
| `color-path-ahead-day` | `GColorPastelYellow` | Dotted path ahead, above the horizon (Classic) |
| `color-path-night` | `GColorLiberty` | Path below the horizon, past (solid) and ahead (dotted) |
| `color-rain` | `GColorPictonBlue` | Rain in icons; rain streaks on a dark sky |
| `color-rain-day` | `GColorCobaltBlue` | Rain streaks on a light sky |
| Glow stops | see below | Solar glow theme |

Solar glow (the default theme), after the Apple Watch Solar face, in Pebble's saturated colours: a radial gradient centred on the sun and
clipped at the horizon. Its colour stops, radius, vertical squash and gain are keyed to the sun's altitude and
interpolated between keys; distances are elliptical (`dx² + (dy / squash)²`), so near the horizon the glow hugs it
as a band. Each pixel is ordered-dithered per channel with a 4 × 4 Bayer matrix into the 64-colour set. Above
y = 70 the glow is dimmed (to 40 % at the top) so the time and date stay legible. The table lives in
`src/c/glow.c` and, identically, in the simulator's `GLOW`.

| Altitude | Radius | Squash | Gain | Colours, centre → edge |
|---|---|---|---|---|
| ≤ −16° | — | — | 0 | none (night) |
| −10° | 96 | 0.45 | 0.6 | purple → indigo → black |
| −4° | 108 | 0.5 | 1 | pink → hot pink → magenta → purple → black (civil twilight) |
| 0° | 106 | 0.6 | 1 | pale yellow → gold → orange → folly pink → purple → black (sunrise, sunset) |
| 8° | 94 | 0.82 | 1 | cream → yellow → amber → rose → violet → black |
| ≥ 22° | 86 | 1 | 1 | white → yellow → mint → cyan → ultramarine → violet → black (day) |

The moon's glow uses the same machinery with its own table, smaller and softer: radius 40–44, gain 0.55,
amber (≤ 6°) to silver-violet (≥ 18°). Its strength is `illuminated^0.8 × darkness`, where darkness runs from 0
with the sun at the horizon to 1 with the sun 8° down, so it never competes with daylight. Both glows are added
per pixel and dithered once.

**Glow styles** (`GLOW_STYLE`, Solar glow theme only): `soft` (default, the gradient above), and five flat styles
that use a few palette colours with crisp edges and no dither, after Pebble's guidelines (few colours, high contrast,
character). They share one colour run per stretch of the day, from the sun outward:

| From | Colours |
|---|---|
| −12° | LavenderIndigo, Purple, Indigo |
| −6° | RichBrilliantLavender, ShockingPink, Purple, Indigo |
| −1° | PastelYellow, ChromeYellow, Orange, Folly, Purple |
| 5° | PastelYellow, Icterine, ChromeYellow, Melon, BabyBlueEyes |
| 15° | PastelYellow, ElectricBlue, PictonBlue, VeryLightBlue |

Shape is the soft glow's radius × 0.86 and its squash. `bands`: equal flat rings. `rays`: 16 alternating wedges
(long ones banded, short ones to 0.45 of the radius) around a core to 0.2, turned 1° per minute. `ripples`: one 3 px
ring per colour at R·(k + 1)/(n + 0.4) plus a core. `pixel`: bands sampled on 4 × 4 blocks, with a checkerboard into
the next colour over the outer 38 % of each band. `scan`: bands drawn as horizontal stripes every 4 px, 3 px thick near
the sun, 2 px, then 1 px. The moon gets one or two small rings (amber when low, silver-violet when high). Over a flat
glow, the header text is drawn with a 1 px black outline.

**Moon** (shown when `MOON` is on and the moon is above the horizon, and more than 8 % lit at night or 25 % by day): a pixel disc r 6 at
(now, its altitude), lit in PastelYellow on the sun's side (mirrored south of the equator), with the unlit part in
DarkGray once the sun is 4° down (earthshine). Its path for the day is drawn as 2 × 2 LightGray dots every 6 px, above the horizon only. The sun and moon share the "now" column: when their glyphs would touch (within 16 px), the
moon is not drawn by day, and once the sun has set the moon sits 16 px above it.

Typography (system fonts only):

| Token | Font key | Usage |
|---|---|---|
| `font-time` | `FONT_KEY_BITHAM_42_BOLD` | Time, right-aligned to x = 194. 12 h: no leading zero ("7:30"); 24 h: leading zero ("07:30"). Follows `clock_is_24h_style()`. Bold so it reads with the backlight off |
| `font-temp` | `FONT_KEY_GOTHIC_24_BOLD` | Current temperature, from x = 29, after a 22 px icon |
| `font-date` | `FONT_KEY_GOTHIC_18_BOLD` | "WED 11", from x = 7; white in Solar glow, chrome yellow in Classic |
| `font-hilo` | `FONT_KEY_GOTHIC_18_BOLD` | "54° 45°", from x = 7, in light grey |
| `font-value` | `FONT_KEY_GOTHIC_28_BOLD` | Complication value; steps down to `GOTHIC_24_BOLD` past 64 px and `GOTHIC_18_BOLD` past 62 px |
| `font-label` | `FONT_KEY_GOTHIC_14_BOLD` | Complication labels (truncated at the 66 px slot width), horizon times, flick labels |
| `font-label-strong` | `FONT_KEY_GOTHIC_14_BOLD` | "POLAR NIGHT" / "MIDNIGHT SUN" |
| `font-flick-value` | `FONT_KEY_GOTHIC_18_BOLD` | Values on the flick card |

Spacing: outer margin `space-edge = 6` px; date indent 8 px; icon-to-temperature gap 2 px; the horizon labels sit
3 px inside the sunrise and sunset ticks and 4 px below the line.

### Components

| Component | Variant | Props | Notes |
|---|---|---|---|
| Clock | — | time, 24 h flag | Redrawn every minute |
| Date | — | weekday, month, day | Uppercase three-letter names, English (system fonts have no localisation guarantees) |
| Weather readout | fresh / stale / none | temp, code, high, low, isDay, age | Icon 22 × 22 to the left of the temperature, under the date |
| Weather icon | sun, partly_cloudy, cloudy, light_rain, heavy_rain, light_snow, heavy_snow, rain_and_snow, unknown | isDay | Drawn with paths, no bitmaps. At night "sun" becomes a crescent moon in light grey |
| Glow | by altitude | sun x, y, altitude | See "Solar glow" above. Not drawn under the flick card or without a location |
| Horizon | Solar glow / Classic | — | 2 px full width, LightGray |
| Sun path | Solar glow / Classic | altitudes[200], nowX | Solar glow: the whole day as one 2 px LightGray line. Classic: past 2 px solid, ahead 2 px dots every 4 px; ChromeYellow by day, LightGray by night |
| Sun | day / set | x, y, altitude | Solar glow: white disc r 7 while the sun is up (altitude ≥ −0.833°; pale yellow below 12°); once set, a black disc r 8 ringed in white (3 px), like an eclipse. Classic: disc r 6 plus a halo ring at r 9; below the horizon a black disc with a ring |
| Horizon labels | shown / hidden | sunrise, sunset | Sunrise is left-aligned 3 px right of its tick; sunset is right-aligned 3 px left of its tick. Both sit 4 px under the line |
| Polar label | polar night / midnight sun | — | Polar night: above the line (the path is all below). Midnight sun: below the line |
| Complication slot | value + label | value ≤ 6 chars ideal, label ≤ 66 px | Value (Gothic 28 bold) centred at y = 182, label at y = 210 |
| Flick card | — | dawn, dusk, rise, set, noon, noon altitude, day-length change | Black panel over the graph (y 70–182) with a DarkGray top rule; 3 rows × 2 columns at x = 8 and x = 106 |

### States and interactions

| Element | State | Behaviour |
|---|---|---|
| Whole face | Minute tick | Redraw. Recompute the 200 altitudes only when the date or location changes |
| Weather readout | Fresh (< 3 h old) | White temperature, icon, high and low |
| Weather readout | Stale (3–12 h) | Temperature in LightGray; icon and high/low unchanged |
| Weather readout | None / > 12 h | "--°" in DarkGray, no icon, no high/low. The face never shows an old reading as current |
| Horizon labels | Sun within 10 px of a label box | That label is hidden; its tick stays |
| Horizon labels | Day span narrower than both labels + 12 px | Both labels hidden (short winter days at high latitude) |
| Horizon labels | Setting off | Ticks and labels hidden |
| Flick card | Accel tap | Show for 5 s, then return. Another tap while it shows restarts the 5 s |
| Flick card | Setting off | Taps ignored; tap service not subscribed (saves power) |
| Complication | Weather value missing | Value "--", label as normal |
| Complication | Plugin value missing or permission denied | Value "--"; label "PERMISSION" when the phone reports `PERMISSION_DENIED`, else the normal label |
| Complication | Health unavailable | Value "--" |
| Location | Unknown (first run, no phone yet) | Graph uses the last saved location; if none, draws only the horizon and the label "WAITING FOR LOCATION" under it |

### Responsive behaviour
There is one screen size. The only adaptive behaviour is the unobstructed area:

| Condition | Changes |
|---|---|
| Unobstructed height = 228 | Default layout |
| Unobstructed height < 228 (Quick View) | Complication row hidden; nothing else moves |

### Content
- **Complication catalogue** (setting values in brackets):
  - Watch: Empty (`none`), Next sunrise / sunset (`next_sun`), Daylight left (`daylight`), Golden hour (`golden`),
    Sun elevation (`sun_alt`), Moon (`moon`), Watch battery (`battery`), Steps (`steps`), Distance (`distance`),
    Heart rate (`heart`).
  - Weather: High / low (`wx_hilo`), Chance of rain (`wx_precip`), Feels like (`wx_feels`),
    Humidity (`wx_humidity`), Wind (`wx_wind`), UV index (`wx_uv`).
  - Plugins: Phone battery (`phone_battery`, `phone/phone_state`), Next event (`next_event`, `calendar/event`),
    Pebble app pick 1–3 (`pebble_1`–`pebble_3`, `preferences/complications`).
  - Defaults: `next_sun`, `battery`, `steps`. All three work with no phone, no plugins and no network.
- **Labels** (uppercase, ≤ 64 px in Gothic 14, else truncated a character at a time): SUNRISE, SUNSET, DAY LEFT,
  DAY LENGTH, GOLDEN HR, SUN, NEW / WAXING / FULL / WANING, BATTERY, STEPS, KM or MI, BPM, HIGH / LOW, RAIN, FEELS,
  HUMIDITY, WIND, UV, PHONE. Next event and the Pebble picks use the label the phone sends (event title, place name).
- **Value formats**: times follow the 12/24 h setting; durations `9h33`; steps with a thousands comma up to 99,999,
  then `102k`; distance one decimal; temperatures with a degree sign and no unit letter.
- **Next sunrise / sunset** after today's sunset shows *tomorrow's* sunrise.
- **Daylight**: while the sun is up, time until sunset (DAY LEFT); otherwise today's length (DAY LENGTH).
  Polar night `0h00`, midnight sun `24h00`.
- **Golden hour**: the next time today that the sun's centre crosses 6° (zenith 84°), rising or setting; "—" when none is left.
- **Flick card**: FIRST LIGHT / LAST LIGHT (civil twilight, zenith 96°), SUNRISE / SUNSET (zenith 90.833°),
  SOLAR NOON with its elevation, VS YESTERDAY (signed change in day length, "−2m 50s").

### Data and protocol

**Solar model** (on the watch, so the face works offline): the Astronomical Almanac's low-precision solar
coordinates, from continuous days since J2000 (so leap years need no special case), give declination and the
equation of time; `hourAngle = (minute + eqTime + 4·lon − tzMinutes) / 4 − 180`; altitude from the standard
spherical formula; crossings from `cos(H0) = cos(zenith)/(cos φ cos δ) − tan φ tan δ`, evaluated once at local
noon and again at the moment found. No `cos(H0)` in [−1, 1] means no crossing.
Measured accuracy (`test/solar_check.py` against PyEphem, ten places from the equator to 70° N): sunrise, sunset
and solar noon within 0.1 minute, noon elevation within 0.02°.
(An earlier draft used NOAA's fractional-year series; it ran a day behind in 2026 and missed by up to 3.5 minutes.)

**Location**: the phone sends latitude and longitude; the watch saves them and redraws. The time-zone offset
comes from the watch's own clock.

**AppMessage keys** (phone → watch unless noted):

| Key | Type | Meaning |
|---|---|---|
| `REQUEST` | int (watch → phone) | Sent at launch and when weather is older than 1 h; the phone replies with everything it has |
| `LAT_E4`, `LON_E4` | int32 | Degrees × 10 000 |
| `WX_TEMP`, `WX_HI`, `WX_LO`, `WX_FEELS` | int32 | Whole degrees in the display unit; `−1000` = none |
| `WX_CODE` | int32 | 0 unknown, 1 sun, 2 partly_cloudy, 3 cloudy, 4 light_rain, 5 heavy_rain, 6 light_snow, 7 heavy_snow, 8 rain_and_snow |
| `WX_IS_DAY` | int32 | 1 when it is day at the weather location, from the source. The watch uses it only when it has no location; otherwise its own sun altitude decides (a reading can be an hour old) |
| `WX_PRECIP`, `WX_HUMIDITY`, `WX_UV` | int32 | Percent, percent, index; `−1000` = none |
| `WX_WIND` | cstring | e.g. "9 SW" in the user's units |
| `WX_TIME` | int32 | Epoch seconds of the reading |
| `SLOT_VALUE[3]`, `SLOT_LABEL[3]` | cstring | Text for slots filled by plugins, by slot index |
| `THEME`, `SLOT[3]`, `FLICK`, `HORIZON_LABELS`, `WEATHER_ON`, `MOON` | from Clay | Watch settings (strings for selects, ints for toggles). `THEME` is `glow` (default) or `classic` |
| `UNITS`, `WX_SOURCE`, `LOC_MODE`, `LAT_MANUAL`, `LON_MANUAL` | from Clay | Phone-side settings; the watch ignores them |

**Weather sources** (phone): setting `WX_SOURCE` = Automatic (default) / Pebble app plugin / Open-Meteo.
Automatic subscribes to `weather/location` (first instance) when `Pebble.enumeratePlugins()` lists a plugin serving
it, and otherwise, or on `PLUGIN_UNAVAILABLE`, fetches Open-Meteo every 30 minutes. A plugin only sends when the
weather changes, so when the watch asks (`REQUEST`, sent once its reading is an hour old) the phone re-stamps a
still-valid plugin reading (`validUntilMs` in the future) instead of letting the watch grey it out.
Units: Automatic follows `preferences/units` (plugin) or the phone locale; °F and °C force one.

**Permissions** (`usesPermissions`): `Location` (required: the sun needs it), `Internet` for `api.open-meteo.com`
(optional: only the fallback uses it), `Calendar` (optional: Next event), `Health` (optional: steps, distance,
heart rate). Capabilities: `configurable`, `health`.

**Persistence** (watch): settings, location, last weather and slot texts each under their own persist key, so the
face draws complete at launch before the phone answers.

### Edge cases
- **First launch with no phone**: horizon plus "WAITING FOR LOCATION"; time, date, battery and steps work.
- **Polar night / midnight sun**: no horizon times; polar label; Next sunrise/sunset shows "—" with NO SUNRISE or NO SUNSET.
- **Equator**: steep arc; the noon sun can reach y ≈ 74, under the header's bottom edge at 68. Nothing overlaps.
- **Time-zone or DST change**: the minute tick recomputes when the UTC offset changes.
- **Long plugin text**: values over 62 px drop to Gothic 18 bold; past 66 px they are truncated with no ellipsis
  (the slot is too narrow for one). Labels are truncated at 64 px.
- **Plugins preview off**: `enumeratePlugins` is missing or empty, so Automatic uses Open-Meteo and plugin slots show "--".
- **Permission prompt pending**: the plugin call fails with `PERMISSION_DENIED`; the phone retries on the next refresh.
- **Bluetooth disconnected**: everything keeps drawing from saved data; weather goes stale, then "--°", on the rules above.

### Animation / motion
| Element | Trigger | Animation | Duration | Easing |
|---|---|---|---|---|
| Flick card | Wrist flick | Appears in one frame, disappears in one frame | Visible 5000 ms | none |
| Sun | Minute tick | Moves one step; no tweening (battery, and a minute is about 0.14 px) | — | — |

No other motion. A watchface that animates costs battery every minute of every day.

### Accessibility notes
- Panel colours: the Time 2's reflective panel mutes saturated colours (chrome yellow shows as peach, orange as
  coral, yellow as cream; the SDK's emulator screenshots apply the same correction). Warm accents were checked on
  the panel, not by their nominal values.
- Contrast: text is white (21:1), light grey (9.7:1) or chrome yellow (≈ 11:1) on black. Dark grey is never used for
  text that must be read; only the "--°" placeholder uses it, and that placeholder signals that nothing is there.
- Meaning never relies on colour alone: past and future differ in stroke (solid vs dotted), night and day in
  position (below or above the line), and the sun's ring changes shape below the horizon.
- Minimum text size is Gothic 14 bold, used only for labels.
- Backlight off: the reflective panel lifts black to dark grey, tops white out at light grey, washes out colour, and
  blurs strokes finer than 2 px into the background (measured from a photo of the watch on a wrist). So every
  text style is bold, every line is at least 2 px, glyphs are at least r 6, and the glow is built from light tints
  with a short fall to black. The simulator's "Backlight off" toggle previews this.
- Settings page (Clay): every control has a visible label and, where needed, a one-line description; choices are
  named by what the user sees ("Next sunrise / sunset"), not by key.
