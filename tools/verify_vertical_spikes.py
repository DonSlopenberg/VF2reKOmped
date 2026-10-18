"""Count short vertical panorama spikes in a VF2_PANORAMA_TRACE sync file.

A spike is a jump of at least half a source texel undone (opposite sign)
within three updates. Real camera cuts jump once and stay. The legacy
stale-pitch pairing produced a +/-1 texel spike at each 16-unit pitch boundary.
Usage: verify_vertical_spikes.py <trace folder> [--max-spikes N]
"""
import csv,json,sys
from pathlib import Path

folder=Path(sys.argv[1])
limit=int(sys.argv[sys.argv.index('--max-spikes')+1]) if '--max-spikes' in sys.argv else 0
rows=[r for r in csv.reader((folder/'phase.csv.sync.csv').open()) if len(r)>=11]
per_tick={}
for r in rows:per_tick.setdefault(int(r[1]),float(r[7]))
ticks=sorted(per_tick)
step={b:per_tick[b]-per_tick[a] for a,b in zip(ticks,ticks[1:]) if b==a+1}
spikes=[]
for t0,d0 in sorted(step.items()):
    if not 0.5<=abs(d0)<64:continue
    for t1 in range(t0+1,t0+4):
        d1=step.get(t1)
        if d1 is not None and abs(d1)>=0.5 and d0*d1<0:
            spikes.append({'tick':t0,'delta':d0,'undo_tick':t1,'undo':d1});break
fractional=sum(1 for t in ticks if not per_tick[t].is_integer())
report={'ticks':len(ticks),'fractional_ticks':fractional,'spikes':len(spikes),'examples':spikes[:8]}
print(json.dumps(report,indent=2))
sys.exit(0 if len(spikes)<=limit else 1)
