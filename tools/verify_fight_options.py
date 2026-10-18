"""Check resolved native options from the real Custom Fight integration run."""
import json, re, sys
from pathlib import Path
p = Path(sys.argv[1])
log = (p / 'runtime.log').read_text(errors='replace')
actual = [tuple(map(int, x)) for x in re.findall(
    r'match options: difficulty=(\d+) rounds_to_win=(\d+) round_time=(\d+)', log)]
expected = [(0,2,15),(1,3,30),(2,4,45),(3,5,60),(3,5,60),(1,2,15),(3,2,15)]
assert actual == expected, actual
state = (p / 'state.txt').read_text(errors='replace')
for level, wins, time in expected:
    assert re.search(fr'^OPTIONS .*base={level} wins={wins} time={time}\b', state, re.M)
assert 'native full-match result reached after victory animation' in log
assert json.loads((p / 'result.json').read_text())['alive_at_end']
assert (p / 'captures/0.txt').read_text().split()[:2] == ['3840','2160']
print('PASS: all native options, restart, defaults, full match completion, UHD and process stability')
