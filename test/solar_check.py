"""Checks the watch's solar model (src/c/solar.c, built for the host) against PyEphem.

Usage: python3 test/solar_check.py path/to/solar_host
Fails if any sunrise, sunset or solar noon is off by more than TOLERANCE_MIN minutes,
or the noon altitude by more than 0.5 degrees.
"""
import datetime as dt
import subprocess
import sys

import ephem

TOLERANCE_MIN = 3.0
CASES = [  # name, lat, lon, utc offset minutes, local date
    ("seattle-nov", 47.61, -122.33, -480, dt.date(2026, 11, 11)),
    ("seattle-jun", 47.61, -122.33, -420, dt.date(2026, 6, 21)),
    ("newyork-mar", 40.71, -74.01, -240, dt.date(2026, 3, 20)),
    ("quito-sep", -0.18, -78.47, -300, dt.date(2026, 9, 23)),
    ("sydney-dec", -33.87, 151.21, 660, dt.date(2026, 12, 21)),
    ("reykjavik-nov", 64.15, -21.94, 0, dt.date(2026, 11, 11)),
    ("tromso-dec", 69.65, 18.96, 60, dt.date(2026, 12, 10)),   # polar night
    ("tromso-jun", 69.65, 18.96, 120, dt.date(2026, 6, 21)),   # midnight sun
    ("capetown-jan", -33.92, 18.42, 120, dt.date(2026, 1, 15)),
    ("london-dec", 51.51, -0.13, 0, dt.date(2026, 12, 31)),
]


def ephem_minutes(lat, lon, tz, date):
    obs = ephem.Observer()
    obs.lat, obs.lon, obs.elevation = str(lat), str(lon), 0
    obs.pressure = 0
    obs.horizon = "-0:50"  # centre 50 arcmin down (refraction + semi-diameter): zenith 90.833
    local_midnight_utc = dt.datetime.combine(date, dt.time()) - dt.timedelta(minutes=tz)
    obs.date = ephem.Date(local_midnight_utc)
    sun = ephem.Sun()

    def to_local_min(d):
        return (ephem.Date(d).datetime() - local_midnight_utc).total_seconds() / 60.0

    try:
        rise = to_local_min(obs.next_rising(sun, use_center=True))
        sset = to_local_min(obs.next_setting(sun, use_center=True))
        if rise > 1440 or sset > 1440:
            rise = sset = None
    except (ephem.AlwaysUpError, ephem.NeverUpError):
        rise = sset = None
    noon = to_local_min(obs.next_transit(sun))
    obs.date = ephem.Date(local_midnight_utc + dt.timedelta(minutes=noon))
    sun.compute(obs)
    return rise, sset, noon, float(sun.alt) * 180 / 3.141592653589793


def main():
    def midnight(d, tz):
        return int((dt.datetime.combine(d, dt.time(), dt.timezone.utc) - dt.timedelta(minutes=tz)).timestamp())
    stdin = "".join(f"{n} {la} {lo} {tz} {midnight(d, tz)}\n" for n, la, lo, tz, d in CASES)
    out = subprocess.run([sys.argv[1]], input=stdin, capture_output=True, text=True, check=True).stdout
    failures = 0
    print(f"{'case':16} {'rise':>14} {'set':>14} {'noon':>14} {'noon alt':>14}")
    for (name, la, lo, tz, d), line in zip(CASES, out.strip().splitlines()):
        _, rise, sset, noon, alt = line.split()
        rise, sset, noon, alt = float(rise), float(sset), float(noon), float(alt)
        e_rise, e_set, e_noon, e_alt = ephem_minutes(la, lo, tz, d)
        cells, bad = [], False
        for ours, theirs in ((rise, e_rise), (sset, e_set), (noon, e_noon)):
            if theirs is None or ours < 0:
                ok = (theirs is None) == (ours < 0)
                cells.append(f"{'none' if ours < 0 else round(ours)}/{'none' if theirs is None else round(theirs)}")
            else:
                ok = abs(ours - theirs) <= TOLERANCE_MIN
                cells.append(f"{ours - theirs:+.1f} min")
            bad |= not ok
        alt_ok = abs(alt - e_alt) <= 0.5
        bad |= not alt_ok
        cells.append(f"{alt - e_alt:+.2f} deg")
        failures += bad
        print(f"{name:16} " + " ".join(f"{c:>14}" for c in cells) + ("   FAIL" if bad else ""))
    print("ok" if not failures else f"{failures} case(s) outside tolerance")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
