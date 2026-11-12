// Weather mapping for Sunpath. Pure functions, no Pebble APIs, so test/weather.test.js can run
// them under node. Both sources produce the same payload of AppMessage keys (design/spec.md).

'use strict';

var NONE = -1000;

// Condition codes, as the weather plugin names them; the numbers are the watch's WxCode.
var CODES = ['unknown', 'sun', 'partly_cloudy', 'cloudy', 'light_rain', 'heavy_rain',
  'light_snow', 'heavy_snow', 'rain_and_snow'];

function codeNumber(name) {
  var i = CODES.indexOf(name);
  return i < 0 ? 0 : i;
}

// WMO weather interpretation codes (Open-Meteo) onto the plugin's eight kinds.
function wmoToCode(wmo) {
  if (wmo === 0) return 'sun';
  if (wmo === 1 || wmo === 2) return 'partly_cloudy';
  if (wmo === 3 || wmo === 45 || wmo === 48) return 'cloudy';
  if ([51, 53, 55, 56, 57, 61, 80].indexOf(wmo) >= 0) return 'light_rain';
  if ([63, 65, 81, 82, 95, 96, 99].indexOf(wmo) >= 0) return 'heavy_rain';
  if (wmo === 66 || wmo === 67) return 'rain_and_snow';
  if ([71, 73, 77, 85].indexOf(wmo) >= 0) return 'light_snow';
  if (wmo === 75 || wmo === 86) return 'heavy_snow';
  return 'unknown';
}

function compass(deg) {
  if (typeof deg !== 'number' || !isFinite(deg)) return '';
  var names = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'];
  return names[Math.round((((deg % 360) + 360) % 360) / 45) % 8];
}

function round(v) {
  return typeof v === 'number' && isFinite(v) ? Math.round(v) : NONE;
}

function within(v, lo, hi) {
  return typeof v === 'number' && isFinite(v) && v >= lo && v <= hi ? v : undefined;
}

// A temperature reading { value, unit } in the display unit ('F' or 'C').
// Values outside -99..199 are a plugin's "no value" (the built-in uses 32767).
function temperature(reading, fahrenheit) {
  if (!reading || typeof reading.value !== 'number') return NONE;
  var v = reading.value;
  var unitF = /F/i.test(reading.unit || '');
  var unitC = /C/i.test(reading.unit || '');
  if (fahrenheit && unitC) v = v * 9 / 5 + 32;
  if (!fahrenheit && unitF) v = (v - 32) * 5 / 9;
  v = Math.round(v);
  return v >= -99 && v <= 199 ? v : NONE;
}

var KMH_PER = { 'km/h': 1, kmh: 1, kph: 1, mph: 1.609344, 'm/s': 3.6, mps: 3.6, kn: 1.852, kt: 1.852, kts: 1.852, knots: 1.852 };

function wind(speedReading, directionDeg, fahrenheit) {
  if (!speedReading || typeof speedReading.value !== 'number') return '';
  var per = KMH_PER[String(speedReading.unit || 'km/h').toLowerCase().replace(/\s/g, '')];
  if (!per) return '';
  var kmh = speedReading.value * per;
  if (!(kmh >= 0 && kmh < 400)) return '';
  var shown = Math.round(fahrenheit ? kmh / 1.609344 : kmh);
  var dir = compass(directionDeg);
  return dir ? shown + ' ' + dir : String(shown);
}

function numeric(prop) {
  return prop && prop.numericValue && typeof prop.numericValue.value === 'number' ? prop.numericValue : null;
}

function textOf(prop) {
  if (!prop) return '';
  if (prop.longText && prop.longText.text) return prop.longText.text;
  if (prop.shortText && prop.shortText.text) return prop.shortText.text;
  return '';
}

function stampOf(prop) {
  return prop && prop.timestamp && typeof prop.timestamp.value === 'number' ? prop.timestamp.value : null;
}

/**
 * The weather/location plugin envelope (its first instance) as a watch payload.
 * @param {object} envelope - { instances: [{ properties }] }
 * @param {boolean} fahrenheit
 * @param {number} nowSec - epoch seconds
 * @return {object|null} payload, or null when there is no usable temperature
 */
function fromPlugin(envelope, fahrenheit, nowSec) {
  var inst = envelope && envelope.instances && envelope.instances[0];
  var p = inst && inst.properties;
  if (!p) return null;
  var temp = temperature(numeric(p.temperature), fahrenheit);
  if (temp === NONE) return null;
  var rise = stampOf(p.sunrise), set = stampOf(p.sunset);
  var dir = numeric(p.wind_direction);
  var precip = numeric(p.precipitation), hum = numeric(p.humidity), uv = numeric(p.uv_index);
  return {
    WX_TEMP: temp,
    WX_HI: temperature(numeric(p.high), fahrenheit),
    WX_LO: temperature(numeric(p.low), fahrenheit),
    WX_FEELS: temperature(numeric(p.feels_like), fahrenheit),
    WX_CODE: codeNumber(textOf(p.condition_code)),
    WX_IS_DAY: rise !== null && set !== null ? (nowSec >= rise && nowSec < set ? 1 : 0) : -1,
    WX_PRECIP: round(within(precip && precip.value, 0, 100)),
    WX_HUMIDITY: round(within(hum && hum.value, 0, 100)),
    WX_UV: round(within(uv && uv.value, 0, 20)),
    WX_WIND: wind(numeric(p.wind_speed), dir && dir.value, fahrenheit),
    WX_TIME: Math.round(nowSec),
  };
}

function openMeteoUrl(lat, lon, fahrenheit) {
  return 'https://api.open-meteo.com/v1/forecast?latitude=' + lat.toFixed(4) + '&longitude=' + lon.toFixed(4) +
    '&current=temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,wind_speed_10m,' +
    'wind_direction_10m,uv_index,is_day' +
    '&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max' +
    '&timezone=auto&forecast_days=1' +
    '&temperature_unit=' + (fahrenheit ? 'fahrenheit' : 'celsius') +
    '&wind_speed_unit=' + (fahrenheit ? 'mph' : 'kmh');
}

/**
 * An Open-Meteo forecast response as a watch payload (requested in the display units).
 */
function fromOpenMeteo(json, fahrenheit, nowSec) {
  var c = json && json.current;
  if (!c || typeof c.temperature_2m !== 'number') return null;
  var d = json.daily || {};
  var first = function (arr) { return arr && arr.length ? arr[0] : undefined; };
  var speed = typeof c.wind_speed_10m === 'number' ? Math.round(c.wind_speed_10m) : null;
  var dir = compass(c.wind_direction_10m);
  return {
    WX_TEMP: round(c.temperature_2m),
    WX_HI: round(first(d.temperature_2m_max)),
    WX_LO: round(first(d.temperature_2m_min)),
    WX_FEELS: round(c.apparent_temperature),
    WX_CODE: codeNumber(wmoToCode(c.weather_code)),
    WX_IS_DAY: c.is_day === 1 ? 1 : c.is_day === 0 ? 0 : -1,
    WX_PRECIP: round(within(first(d.precipitation_probability_max), 0, 100)),
    WX_HUMIDITY: round(within(c.relative_humidity_2m, 0, 100)),
    WX_UV: round(within(c.uv_index, 0, 20)),
    WX_WIND: speed === null ? '' : (dir ? speed + ' ' + dir : String(speed)),
    WX_TIME: Math.round(nowSec),
  };
}

// The properties Sunpath asks the weather plugin for (a hint; the plugin may send more).
var PLUGIN_PROPERTIES = ['name', 'temperature', 'high', 'low', 'feels_like', 'condition', 'condition_code',
  'precipitation', 'humidity', 'uv_index', 'wind_speed', 'wind_direction', 'sunrise', 'sunset'];

module.exports = {
  NONE: NONE, CODES: CODES, wmoToCode: wmoToCode, compass: compass, temperature: temperature, wind: wind,
  fromPlugin: fromPlugin, fromOpenMeteo: fromOpenMeteo, openMeteoUrl: openMeteoUrl,
  PLUGIN_PROPERTIES: PLUGIN_PROPERTIES, textOf: textOf, numeric: numeric, stampOf: stampOf,
};
