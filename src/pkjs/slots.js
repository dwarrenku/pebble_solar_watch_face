// Text for complication slots that come from Pebble app plugins. Pure functions, tested under
// node (test/slots.test.js). The watch draws the value and label exactly as sent.

'use strict';

var wx = require('./weather');

var PLUGIN_SLOTS = {
  phone_battery: { category: 'phone', item: 'phone_state', properties: ['name', 'battery_level'] },
  next_event: { category: 'calendar', item: 'event', properties: ['name', 'starts_at', 'ends_at', 'all_day'] },
  pebble_1: { prefsIndex: 0 },
  pebble_2: { prefsIndex: 1 },
  pebble_3: { prefsIndex: 2 },
};

var DAYS = ['SUN', 'MON', 'TUE', 'WED', 'THU', 'FRI', 'SAT'];

function upper(s, max) {
  s = String(s || '').toUpperCase().replace(/\s+/g, ' ').trim();
  return s.length > max ? s.slice(0, max) : s;
}

function clock(date, h24) {
  var h = date.getHours(), m = ('0' + date.getMinutes()).slice(-2);
  if (h24) return ('0' + h).slice(-2) + ':' + m;
  h = h % 12;
  return (h === 0 ? 12 : h) + ':' + m;
}

/** Phone battery from phone/phone_state. */
function phoneBattery(envelope) {
  var inst = envelope && envelope.instances && envelope.instances[0];
  var n = inst && wx.numeric(inst.properties.battery_level);
  if (!n) return { value: '--', label: 'PHONE' };
  return { value: Math.round(n.value) + '%', label: 'PHONE' };
}

/**
 * The next calendar event that has not started yet (an event under way counts until it ends,
 * so a meeting you are in still shows). All-day events are skipped: they have no time to show.
 */
function nextEvent(envelope, nowMs, h24) {
  var best = null;
  ((envelope && envelope.instances) || []).forEach(function (inst) {
    var p = inst.properties || {};
    if (p.all_day && p.all_day.boolean && p.all_day.boolean.value) return;
    var start = wx.stampOf(p.starts_at);
    if (start === null) return;
    var end = wx.stampOf(p.ends_at);
    var stillOn = end !== null ? end * 1000 > nowMs : start * 1000 > nowMs - 15 * 60 * 1000;
    if (!stillOn) return;
    if (!best || start < best.start) best = { start: start, name: wx.textOf(p.name) };
  });
  if (!best) return { value: '--', label: 'NO EVENTS' };
  var at = new Date(best.start * 1000);
  var soon = best.start * 1000 - nowMs < 20 * 60 * 60 * 1000;
  return { value: soon ? clock(at, h24) : DAYS[at.getDay()], label: upper(best.name, 18) || 'EVENT' };
}

/** Any reading named by preferences/complications: shortText first, else a number with its unit. */
function anyReading(envelope, index, property) {
  var inst = envelope && envelope.instances && envelope.instances[index];
  var p = inst && inst.properties && inst.properties[property];
  var label = upper((inst && wx.textOf(inst.properties.name)) || property.replace(/_/g, ' '), 18);
  if (!p) return { value: '--', label: label };
  if (p.shortText && p.shortText.text) return { value: p.shortText.text.slice(0, 12), label: label };
  var n = wx.numeric(p);
  if (n) return { value: Math.round(n.value) + (n.unit && n.unit.length <= 2 ? n.unit : ''), label: label };
  if (p.boolean) return { value: p.boolean.value ? 'ON' : 'OFF', label: label };
  return { value: '--', label: label };
}

/** The reading a preferences/complications instance points at. */
function prefsReading(envelope, index) {
  var inst = envelope && envelope.instances && envelope.instances[index];
  if (!inst) return null;
  var p = inst.properties || {};
  var t = function (k) { return p[k] && p[k].shortText ? p[k].shortText.text : ''; };
  var category = t('category'), item = t('item'), property = t('property');
  if (!category || !item || !property) return null;
  return {
    category: category, item: item, property: property,
    index: p.index && p.index.numericValue ? p.index.numericValue.value : 0,
  };
}

function errorText(err, label) {
  return { value: '--', label: err && err.code === 'PERMISSION_DENIED' ? 'PERMISSION' : label };
}

module.exports = {
  PLUGIN_SLOTS: PLUGIN_SLOTS, phoneBattery: phoneBattery, nextEvent: nextEvent, anyReading: anyReading,
  prefsReading: prefsReading, errorText: errorText, clock: clock,
};
