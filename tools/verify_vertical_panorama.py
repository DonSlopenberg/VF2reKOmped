"""Verify captured native pitch fractions reach the actual GPU quad unchanged."""
import csv,json,math,sys
from pathlib import Path

folder=Path(sys.argv[1])
result=json.loads((folder/'result.json').read_text(encoding='utf-8-sig'))
smooth=result.get('smooth_scroll',True)
def records(file):
    return [r for r in csv.DictReader((folder/file).open()) if None not in r.values()]
phase=records('phase.csv')
assert phase,'No rendered arena recorded'
fractional=[r for r in phase if not float(r['phase_y']).is_integer()]
assert bool(fractional)==smooth,'Vertical phase does not match the smooth-scroll setting'
geometry=list(csv.reader((folder/'phase.csv.geometry.csv').open()))
geometry=[r for r in geometry if len(r)==5]
by_frame={int(r['frame']):r for r in phase}
fractional_gpu=0
for frame,draw_y,top,out_y,v in geometry:
    row=by_frame.get(int(frame))
    if not row:continue
    dy,t,y=map(float,(draw_y,top,out_y))
    assert abs(dy+float(row['phase_y']))<1e-8
    assert abs(t-dy)<1e-5,('Fraction lost before quad transform',frame,t,dy)
    scale=float(row['extent'])/(2048*float(row['fov'])/360)
    # All covered arenas in these runs use 2048px panoramas. Only un-clipped
    # tops are compared; clipped edges are intentional, not camera snapping.
    expected=192+(t-192)*scale-float(row['horizon'])
    assert abs(y-expected)<1e-3,(frame,y,expected)
    if not dy.is_integer():fractional_gpu+=1
assert bool(fractional_gpu)==smooth
for a,b in zip(phase,phase[1:]):
    if (a['tick'],a['panorama'])==(b['tick'],b['panorama']):
        assert (a['phase_x'],a['phase_y'])==(b['phase_x'],b['phase_y'])
status=records('phase.csv.status.csv')
scrolling=[r for r in status if r['mode'] in ('1','2')]
assert scrolling and all(r['active']=='1' for r in scrolling)
assert result['alive_at_end']
report={**result,'phase_records':len(phase),'fractional_vertical_records':len(fractional),
        'fractional_gpu_quads':fractional_gpu,'scrolling_frames':len(scrolling),
        'fallback_frames':0,'panoramas':sorted({int(r['panorama']) for r in phase}),
        'fov':sorted({float(r['fov']) for r in phase}),
        'horizon':sorted({float(r['horizon']) for r in phase})}
(folder/'vertical-verification.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
