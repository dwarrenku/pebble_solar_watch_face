// Sunpath, phone side. Supplies the watch with:
//   - the location the sun is drawn for (phone GPS, the Pebble app's weather location, or typed in),
//   - the weather, from the Pebble app's weather plugin when plugins are on, else Open-Meteo,
//   - text for complication slots filled from other plugins (phone battery, calendar, Pebble picks).
// The watch draws the sun itself, so everything here is optional to the face working.

'use strict';

var Clay = require('@rebble/clay');
var clayConfig = require('./config');
var keys = require('message_keys');
var wx = require('./weather');
var slots = require('./slots');

// Hide the coordinate fields unless the wearer chose to type a location in.
function clayCustom() {
  var clay = this;
  clay.on(clay.EVENTS.AFTER_BUILD, function () {
    var mode = clay.getItemByMessageKey('LOC_MODE');
    var fields = [clay.getItemByMessageKey('LAT_MANUAL'), clay.getItemByMessageKey('LON_MANUAL'),
      clay.getItemById('coordsHint')];
    function update() {
      var manual = mode.get() === 'manual';
      fields.forEach(function (f) { if (!f) return; if (manual) f.show(); else f.hide(); });
    }
    update();
    mode.on('change', update);

    // The glow style only applies to the Solar glow themes.
    var theme = clay.getItemByMessageKey('THEME'), glowStyle = clay.getItemByMessageKey('GLOW_STYLE');
    function updateStyle() {
      if (!theme || !glowStyle) return;
      if (/^glow/.test(theme.get())) glowStyle.show(); else glowStyle.hide();
    }
    updateStyle();
    if (theme) theme.on('change', updateStyle);

    // A live preview of the face, pinned to the top of the page. It is a sketch with sample data
    // (2:30 in the afternoon, partly cloudy): close to the watch, not pixel-exact.
    var box = document.createElement('div');
    box.style.cssText = 'position:sticky;top:0;z-index:50;background:#1e1e1e;padding:10px 0 6px;text-align:center;border-bottom:1px solid #444;';
    var cv = document.createElement('canvas');
    cv.width = 400; cv.height = 456;
    cv.style.cssText = 'width:180px;height:205px;border-radius:10px;border:2px solid #555;';
    var cap = document.createElement('div');
    cap.textContent = 'Preview, with sample data';
    cap.style.cssText = 'font-size:11px;color:#999;margin-top:2px;';
    box.appendChild(cv); box.appendChild(cap);
    var host = document.getElementById('main-form') || document.body;
    host.insertBefore(box, host.firstChild);

    var SAMPLE = { next_sun: ['4:17', 'SUNSET'], daylight: ['1h 47m', 'DAYLIGHT'], golden: ['3:32', 'GOLDEN HR'],
      sun_alt: ['14°', 'SUN'], moon: ['62%', 'MOON'], battery: ['80%', 'BATTERY'], steps: ['6,214', 'STEPS'],
      distance: ['4.2 km', 'DISTANCE'], heart: ['68', 'HEART'], wx_hilo: ['54° 45°', 'HI / LO'], wx_precip: ['30%', 'RAIN'],
      wx_feels: ['49°', 'FEELS'], wx_humidity: ['71%', 'HUMIDITY'], wx_wind: ['9 SW', 'WIND'], wx_uv: ['2', 'UV'],
      phone_battery: ['64%', 'PHONE'], next_event: ['3:00', 'STANDUP'], pebble_1: ['--', 'PEBBLE'], pebble_2: ['--', 'PEBBLE'],
      pebble_3: ['--', 'PEBBLE'] };
    var KEYS = ['THEME', 'GLOW_STYLE', 'HORIZON_LABELS', 'TIME_STYLE', 'TIME_FORMAT', 'DATE_FORMAT', 'SLOT[0]', 'SLOT[1]',
      'SLOT[2]', 'WEATHER_ON'];
    function val(k, d) { var it = clay.getItemByMessageKey(k); var v = it ? it.get() : d; return v === undefined ? d : v; }

    function draw() {
      var c = cv.getContext('2d');
      c.setTransform(2, 0, 0, 2, 0, 0);
      var th = val('THEME', 'glow_cartoon'), light = th === 'cartoon_light', cartoon = /cartoon/.test(th), glow = /^glow/.test(th);
      var W = light ? 186 : 200, HZ = 130, ink = light ? '#000' : '#fff', soft = light ? '#555' : '#aaa', paper = light ? '#fff' : '#000';
      var h24 = val('TIME_FORMAT', 'auto') === '24';
      var clock = function (m) { var h = Math.floor(m / 60), mm = ('0' + (m % 60)).slice(-2); return (h24 ? ('0' + h).slice(-2) : (h % 12 || 12)) + ':' + mm; };
      var NOW = 870, RISE = 432, SET = 977;  // 2:30 pm; sunrise 7:12, sunset 4:17
      var altAt = function (m) { return 21 * Math.sin(Math.PI * (m - RISE) / (SET - RISE)) * (m > RISE && m < SET ? 1 : 2.4); };
      var yOf = function (a) { return HZ - Math.sin(a * Math.PI / 180) * (a >= 0 ? 56 : 42); };
      var sx = NOW / 1440 * W, alt = altAt(NOW), sy = yOf(alt);
      var line = function (x0, y0, x1, y1, w, col) { c.beginPath(); c.moveTo(x0, y0); c.lineTo(x1, y1); c.lineWidth = w; c.strokeStyle = col; c.lineCap = 'round'; c.stroke(); };
      var txt = function (s, x, y, font, col, align) { c.font = font; c.fillStyle = col; c.textAlign = align || 'left'; c.textBaseline = 'top'; c.fillText(s, x, y); };
      var NARROW = '"Arial Narrow", "Roboto Condensed", "Helvetica Neue", Arial, sans-serif';

      c.fillStyle = paper; c.fillRect(0, 0, 200, 228);
      if (glow) {
        c.save(); c.beginPath(); c.rect(0, 0, 200, HZ); c.clip();
        var g = c.createRadialGradient(sx, sy, 0, sx, sy, 88);
        var stops = [[0, '#fff'], [0.07, '#ffffaa'], [0.18, '#aaffdd'], [0.34, '#55dcff'], [0.54, '#6496ff'], [0.76, '#6e46c8'], [1, '#000']];
        var flat = val('GLOW_STYLE', 'soft') !== 'soft';
        for (var i = 0; i < stops.length; i++) {
          g.addColorStop(stops[i][0], stops[i][1]);
          if (flat && i < stops.length - 1) g.addColorStop(stops[i + 1][0] - 0.001, stops[i][1]);
        }
        c.fillStyle = g; c.fillRect(0, 0, 200, HZ); c.restore();
      }
      if (light) {
        c.fillStyle = '#ffaa00'; c.fillRect(W + 2, 0, 200 - W - 2, 228);
        c.beginPath(); c.moveTo(W + 2, sy - 7); c.lineTo(W - 4, sy); c.lineTo(W + 2, sy + 7); c.fill();
      }

      // Header: the time, the date, the weather.
      var ts = val('TIME_STYLE', 'large_bold'), right = W - 6 + (light ? 2 : 0);
      var px = /^large/.test(ts) ? 41 : /^medium/.test(ts) ? 29 : 27;
      var face = /digital/.test(ts) ? 'bold ' + px + 'px "Courier New", monospace'
        : (ts === 'large_light' ? '300 ' : 'bold ') + px + 'px ' + (ts === 'small' ? NARROW : '"Helvetica Neue", Helvetica, Arial, sans-serif');
      txt(clock(NOW), right, 1, face, ink, 'right');
      var df = val('DATE_FORMAT', 'dow_day');
      var date = { dow_day: 'WED 11', mon_day: 'NOV 11', day_mon: '11 NOV', m_d: '11/11', d_m: '11/11', none: '' }[df] || '';
      txt(date, 7, 2, 'bold 17px ' + NARROW, th === 'classic' ? '#ffaa00' : ink);
      if (val('WEATHER_ON', true) !== false) {
        c.beginPath(); c.arc(14, 34, 5, 0, 7); c.lineWidth = 2; c.strokeStyle = cartoon ? ink : '#ffaa00'; c.stroke();
        txt('51°', 29, 23, 'bold 22px ' + NARROW, ink);
        txt('54° 45°', 7, 47, 'bold 17px ' + NARROW, soft);
      }

      // The path and the horizon.
      var pathCol = th === 'classic' ? '#ffaa00' : light ? '#000' : '#aaa';
      for (var x = 1; x <= W; x++) {
        var a0 = altAt((x - 1) / W * 1440), a1 = altAt(x / W * 1440);
        if (cartoon && a1 < 0) { if (x % 5 === 0) { c.fillStyle = soft; c.fillRect(x - 1, yOf(a1) - 1, 2, 2); } continue; }
        line(x - 1, yOf(a0), x, yOf(a1), 2, a1 >= 0 ? pathCol : '#aaa');
      }
      line(cartoon ? 2 : 0, HZ, W - (cartoon ? 2 : 0), HZ, cartoon ? 3 : 2, cartoon ? ink : '#aaa');
      if (val('HORIZON_LABELS', true) !== false) {
        txt(clock(RISE), RISE / 1440 * W + 4, HZ + 4, 'bold 13px ' + NARROW, soft);
        if (!cartoon) txt(clock(SET), SET / 1440 * W - 4, HZ + 4, 'bold 13px ' + NARROW, soft, 'right');  // the cartoon sun covers it at this hour
      }

      // The sun.
      if (cartoon) {
        var R = 15, warm = light ? '#000' : '#ff5500', k, a;
        for (k = 0; k < 8; k++) { a = (-90 + k * 45) * Math.PI / 180; line(sx + Math.cos(a) * 19, sy + Math.sin(a) * 19, sx + Math.cos(a) * 23, sy + Math.sin(a) * 23, 2, warm); }
        c.beginPath();
        for (k = 0; k < 16; k++) { a = (-90 + k * 22.5) * Math.PI / 180; var r = k % 2 ? R * 0.87 : R; c[k ? 'lineTo' : 'moveTo'](sx + Math.cos(a) * r, sy + Math.sin(a) * r); }
        c.closePath(); c.fillStyle = '#ffaa00'; c.fill(); c.lineWidth = 2.5; c.strokeStyle = warm; c.lineJoin = 'round'; c.stroke();
        line(sx - 5, sy - 4.5, sx - 5, sy + 0.5, 2, '#000'); line(sx + 4.5, sy - 4.5, sx + 4.5, sy + 0.5, 2, '#000');
        c.beginPath(); c.moveTo(sx - 5.5, sy + 4); c.lineTo(sx - 0.5, sy + 6.5); c.lineTo(sx + 4.5, sy + 4); c.lineWidth = 2; c.strokeStyle = '#000'; c.stroke();
      } else if (th === 'classic') {
        c.beginPath(); c.arc(sx, sy, 9, 0, 7); c.lineWidth = 2; c.strokeStyle = '#ffaa00'; c.stroke();
        c.beginPath(); c.arc(sx, sy, 6, 0, 7); c.fillStyle = '#ffff00'; c.fill();
      } else {
        c.beginPath(); c.arc(sx, sy, 7, 0, 7); c.fillStyle = '#fff'; c.fill();
      }

      // Complications.
      for (var s = 0; s < 3; s++) {
        var d = SAMPLE[val('SLOT[' + s + ']', 'none')];
        if (!d) continue;
        var cx = [34, 100, 166][s] * W / 200, v = d[0];
        if (h24 && /^\d:\d\d$/.test(v)) v = clock((+v[0] + 12) * 60 + +v.slice(2));
        var size = 25;
        c.font = 'bold ' + size + 'px ' + NARROW;
        while (size > 12 && c.measureText(v).width > W / 3 - 8) { size--; c.font = 'bold ' + size + 'px ' + NARROW; }
        txt(v, cx, 183 + (25 - size) / 2, 'bold ' + size + 'px ' + NARROW, ink, 'center');
        txt(d[1], cx, 209, 'bold 13px ' + NARROW, soft, 'center');
      }
    }
    KEYS.forEach(function (k) { var it = clay.getItemByMessageKey(k); if (it) it.on('change', draw); });
    draw();
  });
}
var clay = new Clay(clayConfig, clayCustom, { autoHandleEvents: false });

var OPEN_METEO_EVERY_MS = 30 * 60 * 1000;
var LOCATION_KEY = 'sunpath-location';

function log(msg) { console.log('[Sunpath] ' + msg); }

// ---------------------------------------------------------------------------------- settings

function settings() {
  var s = {};
  try { s = JSON.parse(localStorage.getItem('clay-settings')) || {}; } catch (e) { s = {}; }
  var pick = function (k, d) { return s[k] === undefined || s[k] === null || s[k] === '' ? d : s[k]; };
  return {
    slots: [pick('SLOT[0]', 'next_sun'), pick('SLOT[1]', 'battery'), pick('SLOT[2]', 'steps')],
    weatherOn: pick('WEATHER_ON', true) !== false,
    source: pick('WX_SOURCE', 'auto'),
    units: pick('UNITS', 'auto'),
    locMode: pick('LOC_MODE', 'auto'),
    lat: parseFloat(pick('LAT_MANUAL', '')),
    lon: parseFloat(pick('LON_MANUAL', '')),
  };
}

// ------------------------------------------------------------------------- message queue
// AppMessage takes one message at a time; send in order and retry a failed one a few times.

var queue = [];
var sending = false;

function send(msg) {
  queue.push({ msg: msg, tries: 0 });
  pump();
}

function pump() {
  if (sending || !queue.length) return;
  sending = true;
  var head = queue[0];
  Pebble.sendAppMessage(head.msg, function () {
    queue.shift();
    sending = false;
    pump();
  }, function () {
    head.tries++;
    if (head.tries >= 3) { log('dropping a message after 3 tries'); queue.shift(); }
    sending = false;
    setTimeout(pump, 1000);
  });
}

// ------------------------------------------------------------------------------ plugin API

var subscriptions = [];

function pluginApi() {
  return typeof Pebble.subscribeToSource === 'function';
}

function installedPlugins() {
  if (typeof Pebble.enumeratePlugins !== 'function') return [];
  try {
    var list = Pebble.enumeratePlugins();
    return Array.isArray(list) ? list : [];
  } catch (e) {
    return [];
  }
}

function serves(category, item) {
  return installedPlugins().some(function (plugin) {
    return (plugin.sources || []).some(function (src) {
      return src && src.category === category && (src.items || []).indexOf(item) >= 0;
    });
  });
}

// Subscribes and keeps the handle so a settings change can end it. Returns null if the phone
// has no plugin support or the call throws.
function subscribe(request) {
  if (!pluginApi()) return null;
  try {
    var handle = Pebble.subscribeToSource(request);
    if (handle && typeof handle.unsubscribe === 'function') subscriptions.push(handle);
    return handle;
  } catch (e) {
    log('subscribe ' + request.category + '/' + request.item + ' threw: ' + e);
    return null;
  }
}

function unsubscribeAll() {
  subscriptions.forEach(function (h) { try { h.unsubscribe(); } catch (e) { /* already ended */ } });
  subscriptions = [];
}

// --------------------------------------------------------------------------------- units

var appUnits = { fahrenheit: null, h24: null };  // from preferences/units, when plugins are on

function localeFahrenheit() {
  var lang = (typeof navigator !== 'undefined' && navigator.language) || 'en-US';
  return /^en-(US|LR)|^my\b/i.test(lang);
}

function fahrenheit() {
  var s = settings();
  if (s.units === 'f') return true;
  if (s.units === 'c') return false;
  return appUnits.fahrenheit !== null ? appUnits.fahrenheit : localeFahrenheit();
}

function h24() {
  return appUnits.h24 !== null ? appUnits.h24 : !localeFahrenheit();
}

function watchUnits(onChange) {
  subscribe({
    category: 'preferences', item: 'units',
    onData: function (env) {
      var p = env && env.instances && env.instances[0] && env.instances[0].properties;
      if (!p) return;
      var before = appUnits.fahrenheit;
      var t = p.temperature && p.temperature.shortText && p.temperature.shortText.text;
      var c = p.clock && p.clock.shortText && p.clock.shortText.text;
      if (t) appUnits.fahrenheit = /F/.test(t);
      if (c) appUnits.h24 = c === '24h';
      if (before !== appUnits.fahrenheit) onChange();
    },
    onError: function (err) { log('preferences/units: ' + err.code); },
  });
}

// ------------------------------------------------------------------------------- location

var place = null;

function loadCachedLocation() {
  try { return JSON.parse(localStorage.getItem(LOCATION_KEY)); } catch (e) { return null; }
}

function setLocation(lat, lon, why) {
  if (!isFinite(lat) || !isFinite(lon) || Math.abs(lat) > 90 || Math.abs(lon) > 180) return false;
  place = { lat: lat, lon: lon };
  localStorage.setItem(LOCATION_KEY, JSON.stringify(place));
  log('location from ' + why);
  send({ LAT_E4: Math.round(lat * 1e4), LON_E4: Math.round(lon * 1e4) });
  return true;
}

// The first of the wearer's Pebble app weather locations that has coordinates.
function weatherLocation(done) {
  var finished = false;
  var handle = subscribe({
    category: 'preferences', item: 'weather_location',
    onData: function (env) {
      if (finished) return;
      var found = ((env && env.instances) || []).map(function (i) { return i.properties || {}; })
        .filter(function (p) { return wx.numeric(p.latitude) && wx.numeric(p.longitude); })[0];
      finished = true;
      if (handle) handle.unsubscribe();
      done(found ? { lat: found.latitude.numericValue.value, lon: found.longitude.numericValue.value } : null);
    },
    onError: function () { if (!finished) { finished = true; done(null); } },
  });
  if (!handle) done(null);
}

function locate(done) {
  var s = settings();
  if (s.locMode === 'manual') {
    if (setLocation(s.lat, s.lon, 'settings')) return done();
    log('manual location is not valid; using the phone');
  }
  var fallback = function () {
    weatherLocation(function (loc) {
      if (loc && setLocation(loc.lat, loc.lon, 'Pebble app weather location')) return done();
      var cached = loadCachedLocation();
      if (cached && setLocation(cached.lat, cached.lon, 'cache')) return done();
      done();
    });
  };
  if (!navigator.geolocation) return fallback();
  navigator.geolocation.getCurrentPosition(function (pos) {
    setLocation(pos.coords.latitude, pos.coords.longitude, 'phone');
    done();
  }, function (err) {
    log('geolocation failed: ' + (err && err.message));
    fallback();
  }, { timeout: 20000, maximumAge: 30 * 60 * 1000, enableHighAccuracy: false });
}

// -------------------------------------------------------------------------------- weather

var latestWeather = null;
var meteoTimer = null;
var meteoFetchedAt = 0;

function sendWeather(payload) {
  latestWeather = payload;
  send(payload);
}

function fetchOpenMeteo() {
  if (!place) { log('no location yet for Open-Meteo'); return; }
  var f = fahrenheit();
  var req = new XMLHttpRequest();
  req.onload = function () {
    try {
      var payload = wx.fromOpenMeteo(JSON.parse(req.responseText), f, Date.now() / 1000);
      if (payload) { meteoFetchedAt = Date.now(); sendWeather(payload); }
    } catch (e) {
      log('Open-Meteo reply was not usable: ' + e);
    }
  };
  req.onerror = function () { log('Open-Meteo request failed'); };
  req.open('GET', wx.openMeteoUrl(place.lat, place.lon, f));
  req.send();
}

function startOpenMeteo() {
  if (meteoTimer) return;
  log('weather from Open-Meteo');
  fetchOpenMeteo();
  meteoTimer = setInterval(fetchOpenMeteo, OPEN_METEO_EVERY_MS);
}

function stopOpenMeteo() {
  if (meteoTimer) clearInterval(meteoTimer);
  meteoTimer = null;
}

var weatherEnvelope = null;

function startWeather() {
  var s = settings();
  if (!s.weatherOn) return;
  var usePlugin = s.source === 'plugin' || (s.source === 'auto' && pluginApi() && serves('weather', 'location'));
  if (!usePlugin) return startOpenMeteo();
  log('weather from the Pebble app plugin');
  var handle = subscribe({
    category: 'weather', item: 'location', properties: wx.PLUGIN_PROPERTIES,
    onData: function (env) {
      weatherEnvelope = env;
      var payload = wx.fromPlugin(env, fahrenheit(), Date.now() / 1000);
      if (payload) sendWeather(payload);
    },
    onError: function (err) {
      log('weather/location: ' + err.code + ' ' + (err.message || ''));
      if (s.source === 'auto' && err.code === 'PLUGIN_UNAVAILABLE') startOpenMeteo();
    },
  });
  if (!handle && s.source === 'auto') startOpenMeteo();
}

function weatherUnitsChanged() {
  if (meteoTimer) fetchOpenMeteo();
  else if (weatherEnvelope) {
    var payload = wx.fromPlugin(weatherEnvelope, fahrenheit(), Date.now() / 1000);
    if (payload) sendWeather(payload);
  }
}

// ---------------------------------------------------------------------- plugin complications

var slotText = [null, null, null];
var eventTimer = null;

function sendSlot(i, text) {
  var prev = slotText[i];
  if (prev && prev.value === text.value && prev.label === text.label) return;
  slotText[i] = text;
  var msg = {};
  msg[keys.SLOT_VALUE + i] = text.value;
  msg[keys.SLOT_LABEL + i] = text.label;
  send(msg);
}

function startSlot(i, id) {
  var spec = slots.PLUGIN_SLOTS[id];
  if (!spec) return;
  var fallbackLabel = { phone_battery: 'PHONE', next_event: 'NEXT EVENT' }[id] || ('PEBBLE ' + (spec.prefsIndex + 1));
  if (!pluginApi()) return sendSlot(i, { value: '--', label: 'NO PLUGINS' });
  sendSlot(i, { value: '--', label: fallbackLabel });

  if (id === 'phone_battery') {
    subscribe({
      category: spec.category, item: spec.item, properties: spec.properties,
      onData: function (env) { sendSlot(i, slots.phoneBattery(env)); },
      onError: function (err) { sendSlot(i, slots.errorText(err, fallbackLabel)); },
    });
  } else if (id === 'next_event') {
    var latest = null;
    var refresh = function () { if (latest) sendSlot(i, slots.nextEvent(latest, Date.now(), h24())); };
    subscribe({
      category: spec.category, item: spec.item, properties: spec.properties,
      onData: function (env) { latest = env; refresh(); },
      onError: function (err) { sendSlot(i, slots.errorText(err, fallbackLabel)); },
    });
    // Events pass while nothing changes in the calendar, so look again every few minutes.
    if (eventTimer) clearInterval(eventTimer);
    eventTimer = setInterval(refresh, 5 * 60 * 1000);
  } else {
    // preferences/complications names a reading; follow it, and follow changes to the choice.
    var inner = null;
    subscribe({
      category: 'preferences', item: 'complications',
      onData: function (env) {
        if (inner) { try { inner.unsubscribe(); } catch (e) { /* ended */ } inner = null; }
        var reading = slots.prefsReading(env, spec.prefsIndex);
        if (!reading) return sendSlot(i, { value: '--', label: 'NOT SET' });
        inner = subscribe({
          category: reading.category, item: reading.item, properties: ['name', reading.property],
          onData: function (env2) { sendSlot(i, slots.anyReading(env2, reading.index, reading.property)); },
          onError: function (err) { sendSlot(i, slots.errorText(err, fallbackLabel)); },
        });
      },
      onError: function (err) { sendSlot(i, slots.errorText(err, fallbackLabel)); },
    });
  }
}

// ------------------------------------------------------------------------------- lifecycle

function start() {
  unsubscribeAll();
  stopOpenMeteo();
  if (eventTimer) { clearInterval(eventTimer); eventTimer = null; }
  slotText = [null, null, null];
  var s = settings();
  log('starting; plugins ' + (pluginApi() ? 'available (' + installedPlugins().length + ' installed)' : 'off'));
  if (pluginApi()) watchUnits(weatherUnitsChanged);
  locate(function () {
    startWeather();
  });
  s.slots.forEach(function (id, i) { startSlot(i, id); });
}

function resendAll() {
  if (place) send({ LAT_E4: Math.round(place.lat * 1e4), LON_E4: Math.round(place.lon * 1e4) });
  // A plugin only sends when the weather changes, so an unchanged reading that is still valid is
  // re-stamped now; otherwise the watch would grey it out as stale after three quiet hours.
  if (weatherEnvelope && !meteoTimer) {
    var validUntil = weatherEnvelope.validUntilMs;
    if (!validUntil || validUntil > Date.now()) {
      var fresh = wx.fromPlugin(weatherEnvelope, fahrenheit(), Date.now() / 1000);
      if (fresh) latestWeather = fresh;
    }
  }
  if (latestWeather) send(latestWeather);
  slotText.forEach(function (t, i) {
    if (!t) return;
    var msg = {};
    msg[keys.SLOT_VALUE + i] = t.value;
    msg[keys.SLOT_LABEL + i] = t.label;
    send(msg);
  });
  if (meteoTimer && Date.now() - meteoFetchedAt > OPEN_METEO_EVERY_MS) fetchOpenMeteo();
}

Pebble.addEventListener('ready', function () {
  log('PebbleKit JS ready');
  start();
});

Pebble.addEventListener('appmessage', function (e) {
  if (e.payload && (e.payload.REQUEST || e.payload[keys.REQUEST])) resendAll();
});

Pebble.addEventListener('showConfiguration', function () {
  Pebble.openURL(clay.generateUrl());
});

Pebble.addEventListener('webviewclosed', function (e) {
  if (!e || !e.response) return;
  var dict;
  try { dict = clay.getSettings(e.response); } catch (err) { log('settings were not readable: ' + err); return; }
  send(dict);
  start();
});
