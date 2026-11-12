"""Checks the watch's moon (src/c/moon.c, built for the host) against PyEphem.

Usage: python3 test/moon_check.py path/to/moon_host
Fails if the moon's altitude is off by more than ALT_TOL degrees, its illuminated fraction by more
than FRAC_TOL, or waxing/waning is wrong away from new and full moon.
"""
import datetime as dt
import math
import subprocess
import sys

import ephem

ALT_TOL = 0.6
FRAC_TOL = 0.02
PLACES = [("london", 51.51, -0.13), ("seattle", 47.61, -122.33), ("sydney", -33.87, 151.21),
          ("quito", -0.18, -78.47), ("tromso", 69.65, 18.96)]
START = dt.datetime(2026, 11, 1, tzinfo=dt.timezone.utc)
TIMES = [START + dt.timedelta(hours=h) for h in range(0, 24 * 30, 37)]  # a month, every 37 h


def main():
    cases = [(f"{n}@{t:%m%dT%H}", la, lo, t) for t in TIMES for n, la, lo in PLACES]
    stdin = "".join(f"{n} {la} {lo} {int(t.timestamp())}\n" for n, la, lo, t in cases)
    out = subprocess.run([sys.argv[1]], input=stdin, capture_output=True, text=True, check=True).stdout
    worst_alt = worst_frac = 0.0
    failures = 0
    for (name, la, lo, t), line in zip(cases, out.strip().splitlines()):
        _, alt, frac, waxing = line.split()
        alt, frac, waxing = float(alt), float(frac), waxing == "1"
        obs = ephem.Observer()
        obs.lat, obs.lon, obs.elevation, obs.pressure = str(la), str(lo), 0, 0
        obs.date = ephem.Date(t.replace(tzinfo=None))
        m = ephem.Moon(obs)
        e_alt = math.degrees(float(m.alt))
        e_frac = m.moon_phase
        e_waxing = float(m.elong) > 0
        d_alt, d_frac = alt - e_alt, frac - e_frac
        worst_alt, worst_frac = max(worst_alt, abs(d_alt)), max(worst_frac, abs(d_frac))
        bad = abs(d_alt) > ALT_TOL or abs(d_frac) > FRAC_TOL or (0.03 < e_frac < 0.97 and waxing != e_waxing)
        if bad:
            failures += 1
            print(f"FAIL {name}: alt {alt:.2f} vs {e_alt:.2f}, lit {frac:.3f} vs {e_frac:.3f}, waxing {waxing} vs {e_waxing}")
    print(f"{len(cases)} cases; worst altitude error {worst_alt:.2f} deg, worst illumination error {worst_frac:.3f}")
    print("ok" if not failures else f"{failures} case(s) outside tolerance")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
