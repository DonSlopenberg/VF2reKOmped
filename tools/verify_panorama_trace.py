"""Compare captured panorama phase with independently read native scroll ports."""
import argparse
import csv
import json
import math
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("folder", type=Path)
args = parser.parse_args()
rows = list(csv.reader((args.folder / "phase.csv.native.csv").open()))
phase = {int(r['frame']):float(r['phase_y'])
         for r in csv.DictReader((args.folder/'phase.csv').open()) if None not in r.values()}
smooth=json.loads((args.folder/'result.json').read_text(encoding='utf-8-sig')).get('smooth_scroll',True)
failures = []
for row in rows:
    frame, arena, sx, sy, full_x, y, age, *_ = map(float, row)
    # Early trace writers used the default six significant digits. Validate
    # the full-precision rendering trace at that same frame, not rounded text.
    if int(frame) in phase:y=phase[int(frame)]
    # The native scalar port covers a 512px window; absolute phase also
    # includes its streamed panorama column. Compare their shared low bits.
    dx = (full_x + sx + 256) % 512 - 256
    # The hardware port is integral. New traces also retain the low four bits
    # of its delayed camera pitch; compare the integer port independently and
    # verify that the exact discarded fraction was restored, with no new lag.
    expected_fraction = ((int(row[12]) - 736) & 15) / 16 if len(row)>12 and smooth else 0
    dy = (y + expected_fraction + sy + 512) % 1024 - 512
    fraction_ok = abs((math.ceil(y)-y)-expected_fraction)<1e-9
    if dx or dy or age > 2 or not fraction_ok:
        failures.append(dict(frame=frame, arena=arena, dx=dx, dy=dy, age=age))
result = dict(samples=len(rows), arenas=sorted({int(r[1]) for r in rows}),
              mismatches=failures, passed=bool(rows) and not failures)
print(json.dumps(result, indent=2))
raise SystemExit(0 if result["passed"] else 1)
