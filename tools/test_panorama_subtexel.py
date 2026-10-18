"""Exercise the actual demo renderer's captured response to a 1/8-texel pan.
Capture the same frozen native tick with VF2_DEMO_SAMPLE_Y=0 and 0.125.
The old point-filtered path fails this test; the post-palette path must pass.
"""
from pathlib import Path
import argparse,json
import numpy as np
from PIL import Image

def pixels(folder):
    p=folder/'captures/0.png'
    if p.exists():return np.asarray(Image.open(p).convert('RGB')).astype(np.int16)
    p=folder/'captures/0'
    w,h,stride=map(int,p.with_suffix('.txt').read_text().split())
    return np.frombuffer(p.with_suffix('.rgba').read_bytes(),np.uint8).reshape(h,stride)[:,:w*4].reshape(h,w,4)[:,:,:3].astype(np.int16)

def last_pose(folder):
    import csv
    rows=list(csv.DictReader((folder/'phase.csv').open()))
    return next(row for row in reversed(rows) if row.get('horizon') is not None)

ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('zero',type=Path);ap.add_argument('fraction',type=Path)
args=ap.parse_args();a=pixels(args.zero);b=pixels(args.fraction)
p,q=last_pose(args.zero),last_pose(args.fraction)
assert a.shape==b.shape
assert p['tick']==q['tick']=='500', 'Comparison must freeze at native tick 500'
assert p['phase_x']==q['phase_x'] and abs(float(q['phase_y'])-float(p['phase_y'])-.125)<1e-9
assert p['fov']==q['fov'] and p['horizon']==q['horizon']
d=np.abs(a-b);changed=np.any(d,2)
# This demo pose puts the panorama above the foreground/character/HUD strip.
foreground_changes=int(changed[int(a.shape[0]*.55):].sum())
result={'size':[a.shape[1],a.shape[0]],'native_tick':500,'fractional_texels':.125,
        'changed_background_pixels':int(changed[:int(a.shape[0]*.55)].sum()),
        'changed_foreground_pixels':foreground_changes,'max_channel_change':int(d.max())}
print(json.dumps(result,indent=2))
assert result['changed_background_pixels']>1000, 'FAIL: fractional motion was lost before color sampling'
assert foreground_changes==0, 'FAIL: background adjustment changed foreground/characters/HUD'
print('PASS: fractional panorama displacement affects colors and leaves foreground unchanged.')
