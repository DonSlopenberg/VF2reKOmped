"""Reject inset final Model 2 viewports in a real renderer diagnostic log."""
import argparse, json, re
from pathlib import Path
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('log', type=Path)
args = parser.parse_args()
rects = [tuple(map(int, m)) for m in re.findall(
    r'VF2 video viewport: x=(-?\d+), y=(-?\d+), width=(\d+), height=(\d+)',
    args.log.read_text(errors='replace'))]
inset = [r for r in rects if r[3] >= 720 and abs(r[2]/r[3]-496/384) < .01
         and (r[0] > 0 or r[1] > 0)]
result = {'observed_rectangles':len(rects), 'inset_final_rectangles':inset,
          'passed':bool(rects) and not inset}
print(json.dumps(result, indent=2))
raise SystemExit(0 if result['passed'] else 1)
