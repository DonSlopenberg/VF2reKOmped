"""Shadow-edge motion per simulation tick from a tools/test_step_capture.ps1 run.

python tools/measure_shadow_steps.py <run folder> [row] [x0] [x1]

Keeps one capture per stepped tick (deduplicated by content, after the
"step test: armed" log time), finds the strongest dark->light edge on a
screen row (default 1060, the bridge shadow on Shun's raft at 1920x1080) and
prints its per-tick movement. A shadow updated on alternate ticks shows a
sawtooth: the sign of the movement flips on most ticks.
"""
import datetime, glob, hashlib, os, re, sys

run = sys.argv[1]
row = int(sys.argv[2]) if len(sys.argv) > 2 else 1060
x0 = int(sys.argv[3]) if len(sys.argv) > 3 else 300
x1 = int(sys.argv[4]) if len(sys.argv) > 4 else 1700
win = int(sys.argv[5]) if len(sys.argv) > 5 else 32   # wide: ignores narrow board seams
log = open(os.path.join(run, 'game.log'), encoding='utf-8', errors='replace').read()
m = re.search(r'\[(\d+-\d+-\d+ \d+:\d+:\d+\.\d+)\][^\n]*step test: armed', log)
armed = datetime.datetime.strptime(m.group(1), '%Y-%m-%d %H:%M:%S.%f').timestamp()
steps = int(open(os.path.join(run, 'step.txt')).read().strip() or 0)

files = sorted(glob.glob(os.path.join(run, 'captures', '*.rgba')), key=lambda p: int(os.path.basename(p)[:-5]))
frames, previous = [], None
end = os.path.getmtime(os.path.join(run, 'step.txt')) + 0.3
for path in files:
    t = os.path.getmtime(path)
    if t < armed or t > end:
        continue
    digest = hashlib.md5(open(path, 'rb').read()).hexdigest()
    if digest != previous:
        frames.append(path)
    previous = digest

def edge(path):
    w, h, pitch = map(int, open(path[:-5] + '.txt').read().split())
    data = open(path, 'rb').read()
    lum = lambda x, y: 0.3 * data[y * pitch + x * 4] + 0.59 * data[y * pitch + x * 4 + 1] + 0.11 * data[y * pitch + x * 4 + 2]
    values = [sum(lum(x, y) for y in range(row - 4, row + 5)) / 9 for x in range(x0, x1)]
    best = (-1e9, 0)
    for i in range(win, len(values) - win):
        step = sum(values[i:i + win]) / win - sum(values[i - win:i]) / win
        best = max(best, (step, x0 + i))
    return best[1]

positions = [edge(p) for p in frames]
moves = [b - a for a, b in zip(positions, positions[1:])]
small = [m for m in moves if abs(m) < 60]          # ignore fighters crossing the row
flips = sum(1 for a, b in zip(small, small[1:]) if a * b < 0)
print(f'{len(frames)} ticks captured ({steps} steps); edge row {row}')
print('moves:', ' '.join(f'{m:+d}' for m in moves))
print(f'sign flips between consecutive moves: {flips} of {max(0, len(small) - 1)}'
      f' ({flips / max(1, len(small) - 1):.0%}; sawtooth ~100%, smooth motion ~0%)')
