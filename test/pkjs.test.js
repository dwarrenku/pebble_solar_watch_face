// Unit tests for the phone-side mapping (src/pkjs/weather.js, src/pkjs/slots.js).
// Run: node test/pkjs.test.js
'use strict';
const assert = require('node:assert/strict');
const path = require('node:path');
const wx = require(path.join(__dirname, '../src/pkjs/weather.js'));
const slots = require(path.join(__dirname, '../src/pkjs/slots.js'));

let passed = 0;
function test(name, fn) { fn(); passed++; console.log('ok - ' + name); }

// A weather/location envelope shaped like the guide's example (built-in plugin, °C).
const NOW = 1794412800; // 2026-11-11 16:00 UTC
const envelope = { pluginUuid: 'builtin', instances: [{ instanceId: 'current', properties: {
  name: { shortText: { text: 'Seattle' } },
  temperature: { numericValue: { value: 10.6, unit: '°C' }, shortText: { text: '11°C' } },
  high: { numericValue: { value: 12.2, unit: '°C' } },
  low: { numericValue: { value: 7.2, unit: '°C' } },
  feels_like: { numericValue: { value: 32767, unit: '°C' } },     // the built-in's "no value"
  condition_code: { longText: { text: 'light_rain' } },
  precipitation: { numericValue: { value: 60, unit: '%', min: 0, max: 100 } },
  humidity: { numericValue: { value: 87, unit: '%' } },
  uv_index: { numericValue: { value: 1, min: 0, max: 11 } },
  wind_speed: { numericValue: { value: 4, unit: 'm/s' } },
  wind_direction: { numericValue: { value: 225 } },
  sunrise: { timestamp: { value: NOW - 6 * 3600 } },
  sunset: { timestamp: { value: NOW + 3 * 3600 } },
} }] };

test('plugin weather converts °C to °F and rounds', () => {
  const p = wx.fromPlugin(envelope, true, NOW);
  assert.equal(p.WX_TEMP, 51);
  assert.equal(p.WX_HI, 54);
  assert.equal(p.WX_LO, 45);
});
test('plugin weather keeps °C when the face shows °C', () => {
  assert.equal(wx.fromPlugin(envelope, false, NOW).WX_TEMP, 11);
});
test("plugin's 32767 'no value' becomes NONE, not 199", () => {
  assert.equal(wx.fromPlugin(envelope, true, NOW).WX_FEELS, wx.NONE);
});
test('condition code, extras, wind and day flag map across', () => {
  const p = wx.fromPlugin(envelope, true, NOW);
  assert.equal(wx.CODES[p.WX_CODE], 'light_rain');
  assert.equal(p.WX_PRECIP, 60);
  assert.equal(p.WX_HUMIDITY, 87);
  assert.equal(p.WX_UV, 1);
  assert.equal(p.WX_WIND, '9 SW');   // 4 m/s = 14.4 km/h = 8.9 mph, from 225°
  assert.equal(p.WX_IS_DAY, 1);
  assert.equal(wx.fromPlugin(envelope, false, NOW).WX_WIND, '14 SW');
});
test('a plugin update with no temperature is ignored', () => {
  assert.equal(wx.fromPlugin({ instances: [{ properties: { condition_code: { longText: { text: 'sun' } } } }] }, true, NOW), null);
  assert.equal(wx.fromPlugin({ instances: [] }, true, NOW), null);
});

test('WMO codes map onto the eight plugin kinds', () => {
  assert.equal(wx.wmoToCode(0), 'sun');
  assert.equal(wx.wmoToCode(2), 'partly_cloudy');
  assert.equal(wx.wmoToCode(45), 'cloudy');
  assert.equal(wx.wmoToCode(61), 'light_rain');
  assert.equal(wx.wmoToCode(95), 'heavy_rain');
  assert.equal(wx.wmoToCode(67), 'rain_and_snow');
  assert.equal(wx.wmoToCode(73), 'light_snow');
  assert.equal(wx.wmoToCode(75), 'heavy_snow');
  assert.equal(wx.wmoToCode(42), 'unknown');
});
test('Open-Meteo response maps to the same payload shape', () => {
  const p = wx.fromOpenMeteo({
    current: { temperature_2m: 50.6, apparent_temperature: 46.8, relative_humidity_2m: 88, weather_code: 61,
      wind_speed_10m: 8.6, wind_direction_10m: 200, uv_index: 0.9, is_day: 1 },
    daily: { temperature_2m_max: [54.1], temperature_2m_min: [44.6], precipitation_probability_max: [70] },
  }, true, NOW);
  assert.deepEqual(p, { WX_TEMP: 51, WX_HI: 54, WX_LO: 45, WX_FEELS: 47, WX_CODE: 4, WX_IS_DAY: 1, WX_PRECIP: 70,
    WX_HUMIDITY: 88, WX_UV: 1, WX_WIND: '9 S', WX_TIME: NOW });
});
test('Open-Meteo URL asks for the display units', () => {
  const u = wx.openMeteoUrl(47.61, -122.33, true);
  assert.match(u, /^https:\/\/api\.open-meteo\.com\/v1\/forecast\?latitude=47\.6100&longitude=-122\.3300/);
  assert.match(u, /temperature_unit=fahrenheit/);
  assert.match(u, /wind_speed_unit=mph/);
  assert.match(wx.openMeteoUrl(1, 2, false), /temperature_unit=celsius.*wind_speed_unit=kmh/);
});
test('compass points', () => {
  assert.equal(wx.compass(0), 'N');
  assert.equal(wx.compass(359), 'N');
  assert.equal(wx.compass(-90), 'W');
  assert.equal(wx.compass(NaN), '');
});

test('phone battery slot', () => {
  assert.deepEqual(slots.phoneBattery({ instances: [{ properties: { battery_level: { numericValue: { value: 71, unit: '%' } } } }] }),
    { value: '71%', label: 'PHONE' });
  assert.deepEqual(slots.phoneBattery({ instances: [] }), { value: '--', label: 'PHONE' });
});
test('next event skips finished and all-day events and labels the title', () => {
  const nowMs = NOW * 1000;
  const ev = (name, startH, endH, allDay) => ({ properties: {
    name: { shortText: { text: name } },
    starts_at: { timestamp: { value: NOW + startH * 3600 } },
    ends_at: { timestamp: { value: NOW + endH * 3600 } },
    all_day: { boolean: { value: !!allDay } } } });
  const env = { instances: [ev('Done already', -3, -2), ev('Holiday', -8, 16, true), ev('Design review', 2, 3), ev('Standup', 0.5, 1)] };
  const out = slots.nextEvent(env, nowMs, false);
  assert.equal(out.label, 'STANDUP');
  assert.match(out.value, /^\d{1,2}:\d\d$/);
  assert.deepEqual(slots.nextEvent({ instances: [] }, nowMs, false), { value: '--', label: 'NO EVENTS' });
});
test('an event more than 20 h away shows its weekday', () => {
  const env = { instances: [{ properties: { name: { shortText: { text: 'Trip' } }, starts_at: { timestamp: { value: NOW + 3 * 86400 } } } }] };
  assert.match(slots.nextEvent(env, NOW * 1000, false).value, /^(SUN|MON|TUE|WED|THU|FRI|SAT)$/);
});
test('preferences/complications reading is followed by index and property', () => {
  const prefs = { instances: [
    { instanceId: '0', properties: { category: { shortText: { text: 'phone' } }, item: { shortText: { text: 'phone_state' } },
      property: { shortText: { text: 'battery_level' } }, index: { numericValue: { value: 0 } } } },
    { instanceId: '1', properties: { category: { shortText: { text: 'weather' } }, item: { shortText: { text: 'location' } },
      property: { shortText: { text: 'temperature' } }, index: { numericValue: { value: 1 } } } }] };
  assert.deepEqual(slots.prefsReading(prefs, 1), { category: 'weather', item: 'location', property: 'temperature', index: 1 });
  assert.equal(slots.prefsReading(prefs, 2), null);
  const weather = { instances: [{ properties: {} }, { properties: { name: { shortText: { text: 'London' } }, temperature: { shortText: { text: '9°C' } } } }] };
  assert.deepEqual(slots.anyReading(weather, 1, 'temperature'), { value: '9°C', label: 'LONDON' });
  assert.deepEqual(slots.anyReading({ instances: [{ properties: { battery_level: { numericValue: { value: 64.4, unit: '%' } } } }] }, 0, 'battery_level'),
    { value: '64%', label: 'BATTERY LEVEL' });
});
test('permission errors are named on the slot', () => {
  assert.deepEqual(slots.errorText({ code: 'PERMISSION_DENIED' }, 'PHONE'), { value: '--', label: 'PERMISSION' });
  assert.deepEqual(slots.errorText({ code: 'TIMEOUT' }, 'PHONE'), { value: '--', label: 'PHONE' });
});
test('12/24 h clock text', () => {
  const d = new Date(2026, 10, 11, 14, 5);
  assert.equal(slots.clock(d, false), '2:05');
  assert.equal(slots.clock(d, true), '14:05');
  assert.equal(slots.clock(new Date(2026, 10, 11, 0, 30), false), '12:30');
});

console.log(`\n${passed} tests passed`);
