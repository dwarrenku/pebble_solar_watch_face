// The settings page, built by Clay. Option values are the ids the watch understands (src/c/model.c).

var COMPLICATIONS = [
  { label: 'Empty', value: 'none' },
  { label: 'On the watch', value: [
    { label: 'Next sunrise / sunset', value: 'next_sun' },
    { label: 'Daylight left', value: 'daylight' },
    { label: 'Golden hour', value: 'golden' },
    { label: 'Sun elevation', value: 'sun_alt' },
    { label: 'Moon', value: 'moon' },
    { label: 'Watch battery', value: 'battery' },
    { label: 'Steps', value: 'steps' },
    { label: 'Distance', value: 'distance' },
    { label: 'Heart rate', value: 'heart' },
  ] },
  { label: 'Weather', value: [
    { label: 'High / low', value: 'wx_hilo' },
    { label: 'Chance of rain', value: 'wx_precip' },
    { label: 'Feels like', value: 'wx_feels' },
    { label: 'Humidity', value: 'wx_humidity' },
    { label: 'Wind', value: 'wx_wind' },
    { label: 'UV index', value: 'wx_uv' },
  ] },
  { label: 'Pebble app plugins', value: [
    { label: 'Phone battery', value: 'phone_battery' },
    { label: 'Next event', value: 'next_event' },
    { label: 'Pebble app pick 1', value: 'pebble_1' },
    { label: 'Pebble app pick 2', value: 'pebble_2' },
    { label: 'Pebble app pick 3', value: 'pebble_3' },
  ] },
];

module.exports = [
  { type: 'heading', defaultValue: 'Sunpath' },
  { type: 'text', defaultValue: 'The sun\'s path for where you are, with the weather and three complications of your choice.' },

  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Look' },
      {
        type: 'select', messageKey: 'THEME', label: 'Theme', defaultValue: 'glow_cartoon',
        options: [
          { label: 'Solar glow, cartoon sun', value: 'glow_cartoon' },
          { label: 'Cartoon sun on white', value: 'cartoon_light' },
          { label: 'Cartoon sun on black', value: 'cartoon_dark' },
          { label: 'Solar glow', value: 'glow' },
          { label: 'Classic', value: 'classic' },
        ],
        description: 'The cartoon sun is drawn like the PebbleOS timeline sun: it peeks over the horizon with its eyes shut at sunrise, wakes up as it climbs, and sleeps under the horizon at night. On white, a strip down the right edge takes the colour of the time of day. Solar glow lights the sky around the sun; Classic draws the path in amber on black.',
      },
      {
        type: 'select', messageKey: 'GLOW_STYLE', label: 'Glow style', defaultValue: 'soft',
        options: [
          { label: 'Soft', value: 'soft' },
          { label: 'Bands', value: 'bands' },
          { label: 'Sunburst', value: 'rays' },
          { label: 'Ripples', value: 'ripples' },
          { label: 'Pixel', value: 'pixel' },
          { label: 'Scanlines', value: 'scan' },
        ],
        description: 'How the sun lights the sky in the Solar glow themes. Soft is a smooth gradient; the others use a few flat colours with crisp edges, which read best with the backlight off.',
      },
      { type: 'toggle', messageKey: 'HORIZON_LABELS', label: 'Sunrise and sunset times on the horizon', defaultValue: true },
      {
        type: 'toggle', messageKey: 'MOON', label: 'Show the moon', defaultValue: true,
        description: 'Its phase and its path while it is up, with a soft glow once the sky is dark.',
      },
      {
        type: 'toggle', messageKey: 'FLICK', label: 'Flick your wrist for sun details', defaultValue: true,
        description: 'For five seconds: first light, sunrise, solar noon and how high the sun gets, sunset, last light, and how much longer or shorter the day is than yesterday.',
      },
    ],
  },

  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Time and date' },
      {
        type: 'select', messageKey: 'TIME_STYLE', label: 'Time size and style', defaultValue: 'large_bold',
        options: [
          { label: 'Large, bold', value: 'large_bold' },
          { label: 'Large, light', value: 'large_light' },
          { label: 'Large, digital', value: 'large_digital' },
          { label: 'Medium, bold', value: 'medium_bold' },
          { label: 'Medium, digital', value: 'medium_digital' },
          { label: 'Small', value: 'small' },
        ],
      },
      {
        type: 'select', messageKey: 'TIME_FORMAT', label: 'Clock', defaultValue: 'auto',
        options: [
          { label: 'Same as the watch', value: 'auto' },
          { label: '12-hour', value: '12' },
          { label: '24-hour', value: '24' },
        ],
        description: 'Also used for sunrise, sunset and the other times on the face.',
      },
      {
        type: 'select', messageKey: 'DATE_FORMAT', label: 'Date', defaultValue: 'dow_day',
        options: [
          { label: 'WED 11', value: 'dow_day' },
          { label: 'NOV 11', value: 'mon_day' },
          { label: '11 NOV', value: 'day_mon' },
          { label: '11/11 (month/day)', value: 'm_d' },
          { label: '11/11 (day/month)', value: 'd_m' },
          { label: 'Hidden', value: 'none' },
        ],
      },
    ],
  },

  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Complications' },
      { type: 'select', messageKey: 'SLOT[0]', label: 'Left', defaultValue: 'next_sun', options: COMPLICATIONS },
      { type: 'select', messageKey: 'SLOT[1]', label: 'Middle', defaultValue: 'battery', options: COMPLICATIONS },
      { type: 'select', messageKey: 'SLOT[2]', label: 'Right', defaultValue: 'steps', options: COMPLICATIONS },
      {
        type: 'text',
        defaultValue: 'Plugin readings need plugins turned on in the Pebble app: Settings, Plugins, Plugins / permissions preview. "Pebble app pick" shows what you chose under Settings, Plugins, Complications.',
      },
    ],
  },

  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Weather' },
      { type: 'toggle', messageKey: 'WEATHER_ON', label: 'Show the weather', defaultValue: true },
      {
        type: 'select', messageKey: 'WX_SOURCE', label: 'Source', defaultValue: 'auto',
        options: [
          { label: 'Automatic', value: 'auto' },
          { label: 'Pebble app weather', value: 'plugin' },
          { label: 'Open-Meteo', value: 'openmeteo' },
        ],
        description: 'Automatic uses the Pebble app\'s weather plugin when plugins are on, and Open-Meteo otherwise.',
      },
      {
        type: 'select', messageKey: 'UNITS', label: 'Temperature', defaultValue: 'auto',
        options: [
          { label: 'Same as the Pebble app', value: 'auto' },
          { label: '°F', value: 'f' },
          { label: '°C', value: 'c' },
        ],
      },
    ],
  },

  {
    type: 'section',
    items: [
      { type: 'heading', defaultValue: 'Location' },
      {
        type: 'select', messageKey: 'LOC_MODE', label: 'Place', defaultValue: 'auto',
        options: [
          { label: 'My phone\'s location', value: 'auto' },
          { label: 'Coordinates I enter', value: 'manual' },
        ],
      },
      { type: 'input', messageKey: 'LAT_MANUAL', label: 'Latitude', defaultValue: '', attributes: { placeholder: '47.61', inputmode: 'decimal' } },
      { type: 'input', messageKey: 'LON_MANUAL', label: 'Longitude', defaultValue: '', attributes: { placeholder: '-122.33', inputmode: 'decimal' } },
      { type: 'text', id: 'coordsHint', defaultValue: 'Decimal degrees; south and west are negative.' },
    ],
  },

  { type: 'submit', defaultValue: 'Save' },
];
