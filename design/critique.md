# Design Critique: Sunpath, round 1 (three directions)

Stage: exploration. Rendered at 200x228 and snapped to the 64-colour palette; Seattle, 11 Nov, light rain.
Contact sheet: `design/shots/sheet-v1.png`.

## Overall impression

A (Classic graph) has the clearest silhouette and is the most faithful to the Solar Graph face; it should be the base.
B (Live sky) is the most delightful and the only one where the weather is *shown* rather than stated, but it is busy
and has a text overflow. C (Solar dial) is tidy but drops the sun's height, which is the point of the face.
Recommendation: **A's layout, with B's sky offered as a theme inside A's graph area.** Drop C.

## Usability

| Finding | Severity | Recommendation |
|---|---|---|
| A: at sunrise/sunset the sun glyph sits on top of the "7:07" / "4:40" horizon labels (07:30 and 16:40 frames). | 🔴 Critical | Move the labels below the horizon, inside the day span, where the curve never goes. Hide a label while the sun is within 16 px of it. |
| B: the info line "WED NOV 11 · PARTLY CLOUDY 54°/45°" is wider than 200 px and clips on the left. | 🔴 Critical | Never put free-length text in a fixed row. Keep the date alone; the condition already shows in the sky. |
| All: after sunset, "next sun" shows *today's* sunrise (7:07 at 22:00). | 🟡 Moderate | Compute tomorrow's sunrise once today's sunset has passed. |
| C: "DAYLIGHT 0h 00m" after sunset reads like an error. | 🟡 Moderate | Show day length when the sun is down ("9h 33m day"), or switch to "night left". |
| All: Timeline Quick View covers the bottom ~50 px, i.e. all three complications in A. | 🟡 Moderate | When obstructed, drop the complication row and keep header and graph. Nothing else moves. |
| Polar cases (Tromsø in late Nov): no sunrise or sunset to label. | 🟡 Moderate | Write "POLAR NIGHT" / "MIDNIGHT SUN" centred on the horizon instead of times. |
| A watchface has no buttons or touch, so Apple's scrub-the-day interaction is lost. | 🟢 Minor | Use a wrist flick: for 5 s, show dawn, sunrise, solar noon (with elevation), sunset, dusk and day length in the graph area. |

## Visual hierarchy

- **What draws the eye first**: in A the time, then the sun; correct. In B the sky colour, then the time; acceptable for a
  theme but it makes the time the second read. In C the ring.
- **Reading flow (A)**: time → date → curve → complications, top to bottom. The weather corner is read after the time,
  which matches its importance.
- **Emphasis**: A's complication values (Gothic 24 bold, white) are as loud as the temperature. Keep them at 24 for
  glanceability, but keep labels in light grey so the row reads as secondary.

## Consistency

| Element | Issue | Recommendation |
|---|---|---|
| A header | The weather stack has three rows (icon, temperature, high/low) against two on the left (time, date), so nothing shares a baseline. | Two rows on both sides: icon + temperature level with the time, high/low on the date's baseline. |
| Path styling | B uses white for the past path, A uses amber. | One rule per theme: Classic is amber by day and Liberty by night; Live sky is white by day (it has to read on blue and orange) and Liberty by night. |
| Future path | Dark grey dots at night are close to invisible on black (about 2.9:1), and dotted paths vanish into B's dither. | Night future dots in Liberty; on Live sky, 2 px dots every 4 px. |
| Sky colour (B) | It jumps between altitude buckets (for example at 4° and −1°). | Interpolate the gradient stops continuously by altitude. |

## Accessibility

- **Colour contrast**: white on black 21:1; light grey on black 9.7:1; Chrome Yellow on black ≈ 11:1, all fine for text.
  Dark grey (85,85,85) on black ≈ 2.9:1 fails for text, so use it only for the horizon rule. On B's day sky, white on
  Picton Blue ≈ 2.4:1 fails, so sky text switches to black above 4° of altitude (≈ 8:1 on Vivid Cerulean).
- **Touch targets**: not applicable; a watchface takes no input.
- **Text readability**: nothing smaller than Gothic 14, and Gothic 14 only for labels. At 202 ppi Gothic 24 bold is about
  3 mm tall, good for a glance. Reflective e-paper with the light off favours light, saturated colours, so the Classic
  theme stays the default.

## What works well

- A's fixed horizon makes the season visible: in November the day arc barely clears the line, and in June it towers.
- B's dithered sky looks native to a 64-colour panel rather than like a phone gradient squeezed onto one.
- Solid past and dotted future is a clear, wordless way to encode "now".
- Three equal complication slots in A read cleanly and survive any value up to about six characters.

## Priority recommendations

1. **Adopt A as the layout and fix the label collisions.** Labels go below the horizon, inside the day span, and hide when
   the sun is near or the span is under 64 px.
2. **Offer B's sky as a "Live sky" theme within A's graph area**, interpolated continuously, with weather drawn sparingly
   (at most two clouds, sparse rain or snow) so the curve stays readable.
3. **Regrid the header** into two rows that share baselines across the time and weather sides.
4. **Handle the edge states**: tomorrow's sunrise after sunset, polar day and night, Quick View, and no weather yet
   (a dimmed "--°" with no icon until data arrives; reusing the degree sign for the sun's elevation would read as a
   temperature).
5. **Add the wrist-flick detail view** so the face stays sparse at rest but answers "when is sunset?" on demand.

## Round 3: matching the Solar face's glow

Feedback: the sun should radiate light like the Apple Watch Solar face, a radial gradient expanding from the sun,
not a gradient over the whole sky. References: the Solar face photos in Six Colors' review of the astronomical faces
(8:23 and 1:15 by day, and a twilight scrub at 8:46 PM).

What the references show, and what changed:

| Reference | Before | Now |
|---|---|---|
| Pure black screen; light only around the sun | A linear gradient filled the whole sky band | A radial glow centred on the sun, black everywhere else |
| The glow is cut off at the horizon | n/a | Clipped at the horizon line |
| Day: small white sun in a soft light-blue bloom fading to black | Amber sun with a halo ring | White disc r 5 in a blue bloom (radius ≈ 82 px) |
| Twilight: coral band along the horizon, lavender above | Violet linear gradient | Elliptical glow (squashed to half height) in coral and lavender, centred on the set sun |
| Set sun: black disc ringed in white | Grey ring | White ring on a black disc |
| Thin grey path and horizon | Two-colour path, solid and dotted | One 1 px light-grey line (the amber path stays as the Classic theme) |
| Date top-left, large thin time top-right | Time top-left | Date and weather top-left, time top-right |

The Live sky theme (weather drawn into the sky) is gone; Solar glow is the default and Classic remains.

## Round 4: Pebble colour, and the moon

Feedback: keep the black ground and the focus on the sun, but make it more colourful and playful, like Pebble,
not just bright blue; and give the moon a similar, subtler treatment.

- The sun's glow now runs through Pebble's saturated colours: pink and orange at sunrise, a yellow core inside cyan,
  ultramarine and violet by day, magenta and purple at dusk. Black stays the ground everywhere the glow is not.
- The moon is drawn at its real altitude and phase (checked against PyEphem), with a faint dotted path while it is
  up and a small glow at night scaled by how full it is: amber when low, like a harvest moon, silver-violet when
  high. By day it has no glow, so it never competes with the sun.
- Because the graph's x axis is time, the sun and moon share the "now" column. By day a moon touching the sun is
  outshone and not drawn (the "snowman" in the first try read badly); at dusk the moon sits just above the set sun,
  which is exactly the full-moonrise moment worth showing.

## Round 5: readable with the backlight off

Feedback, with photos of the watch on a wrist: make the fonts and strokes bolder; it should be easier to read with
the backlight off.

What the photos show: black reads as dark grey (about 50), white tops out around 200, colour mostly washes out, and
anything a pixel wide blurs into the black. The thin Bitham Light time, the 1 px path and horizon, the regular-weight
labels and the glow's dark blues and violets all suffered.

| Element | Before | Now |
|---|---|---|
| Time | Bitham 42 Light | Bitham 42 Bold |
| Complication values | Gothic 24 bold | Gothic 28 bold, stepping down only for long text |
| Labels, high / low, horizon times | Gothic 14 and 14 regular | Gothic 14 bold; high / low Gothic 18 bold |
| Path and horizon | 1 px | 2 px; ticks 2 px |
| Sun | r 5; eclipse ring 2 px | r 7; eclipse ring 3 px |
| Moon | r 5; path in faint 1 px dots | r 6; path in 2 × 2 dots |
| Glow | Saturated mid-tones fading slowly through dark blue and violet | Lighter tints of the same hues, a short fall to black |
| Classic night path | Dark blue (invisible without the backlight) | Light grey |

Labels that no longer fit in bold were shortened: HIGH / LOW is HI / LO, and DAY LENGTH is DAYLIGHT. Thin crescents
are now hidden by day (under 25 % lit) and at night (under 8 %), since at this size they read as stray marks.
The simulator has a "Backlight off" preview, calibrated against the photo.

## Round 6: Pebble's design language

Prompt: other designs closer to Pebble's design language, especially for the gradient.

Pebble's guidelines (Core Experience Design, Recommended Guidelines): colours "small in number", high contrast that
reads in a wide variety of lighting, larger bolder fonts for what matters, and animation for character. Pebble Time's
look was flat saturated colour, crisp shapes and thick strokes. A smooth dithered gradient is the least Pebble thing
on the face.

Five flat alternatives were built (Bands, Sunburst, Ripples, Pixel, Scanlines) and run on the emulator alongside Soft,
now selectable as Glow style. Findings:

- Flat colour cannot fade under the time, and clipping it at the header left an ugly flat top at midday. The fix is
  Pebble's own: let the colour run to the top, and give the header text a 1 px black outline.
- With the backlight off, Bands, Sunburst and Ripples keep their shape best; Pixel holds; Soft and Scanlines go hazy.
- Bands is the most direct translation of Pebble's language and the clearest without the backlight; Sunburst has the
  most character, and its slow turn is the one bit of motion a once-a-minute watchface can afford.
