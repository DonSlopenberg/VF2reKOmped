"""Build the launcher logo and the Windows icon from the reKOmped logo.

python tools/prepare_launcher_art.py [source.png]

Source: native/resources/logo_master.png (the full-size reKOmped logo).
Writes native/resources/logo.png (trimmed, 1000 px wide, embedded in the
launcher) and native/resources/vf2.ico (the numeral 2, 16..256 px, PNG
entries). Standard library only; see tools/optimize_png.py.
"""
from pathlib import Path
import io, struct, sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from optimize_png import read_png, write_png, downsize

ROOT = Path(__file__).resolve().parents[1]
RESOURCES = ROOT / 'native/resources'


def trim(width, height, rows):
    visible = lambda line: any(line[x] for x in range(3, len(line), 4))
    top = next(y for y in range(height) if visible(rows[y]))
    bottom = next(y for y in range(height - 1, -1, -1) if visible(rows[y])) + 1
    columns = [x for x in range(width) if any(rows[y][x * 4 + 3] for y in range(top, bottom))]
    left, right = columns[0], columns[-1] + 1
    return right - left, bottom - top, [bytearray(line[left * 4:right * 4]) for line in rows[top:bottom]]


def largest_component(width, height, rows):
    """Keep only the biggest opaque shape (drops the tip of the adjacent 'r')."""
    seen = bytearray(width * height)
    best = []
    for start in range(width * height):
        if seen[start] or not rows[start // width][(start % width) * 4 + 3]:
            continue
        seen[start] = 1
        stack, component = [start], []
        while stack:
            p = stack.pop(); component.append(p)
            x, y = p % width, p // width
            for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                q = ny * width + nx
                if 0 <= nx < width and 0 <= ny < height and not seen[q] and rows[ny][nx * 4 + 3]:
                    seen[q] = 1; stack.append(q)
        if len(component) > len(best):
            best = component
    keep = bytearray(width * height)
    for p in best:
        keep[p] = 1
    for y in range(height):
        for x in range(width):
            if not keep[y * width + x]:
                rows[y][x * 4:x * 4 + 4] = b'\0\0\0\0'
    return rows


def square(width, height, rows, margin=0.06):
    side = int(max(width, height) * (1 + 2 * margin))
    ox, oy = (side - width) // 2, (side - height) // 2
    out = [bytearray(side * 4) for _ in range(side)]
    for y in range(height):
        out[oy + y][ox * 4:(ox + width) * 4] = rows[y]
    return side, out


def main():
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else RESOURCES / 'logo_master.png'
    width, height, rows = read_png(source)
    for line in rows:
        for x in range(3, len(line), 4):
            if line[x] == 0:
                line[x - 3:x] = b'\0\0\0'
    w, h, logo = trim(width, height, [bytearray(r) for r in rows])
    write_png(RESOURCES / 'logo.png', *downsize(w, h, logo, 1000))
    # The numeral 2: right part of the "Fighter 2" line.
    x0, y0, x1, y1 = int(width * 0.764), int(height * 0.259), width, int(height * 0.711)
    two = [bytearray(r[x0 * 4:x1 * 4]) for r in rows[y0:y1]]
    two = largest_component(x1 - x0, y1 - y0, two)
    tw, th, two = trim(x1 - x0, y1 - y0, two)
    side, canvas = square(tw, th, two)
    entries = []
    for size in (16, 24, 32, 48, 64, 128, 256):
        buffer = io.BytesIO()
        _, _, scaled = downsize(side, side, canvas, size)
        temp = RESOURCES / f'.icon{size}.png'
        write_png(temp, size, size, scaled)
        entries.append((size, temp.read_bytes()))
        temp.unlink()
    offset = 6 + 16 * len(entries)
    header = struct.pack('<HHH', 0, 1, len(entries))
    directory, images = b'', b''
    for size, data in entries:
        directory += struct.pack('<BBBBHHII', size % 256, size % 256, 0, 0, 1, 32, len(data), offset + len(images))
        images += data
    (RESOURCES / 'vf2.ico').write_bytes(header + directory + images)
    print('logo.png', (RESOURCES / 'logo.png').stat().st_size, 'bytes; vf2.ico', (RESOURCES / 'vf2.ico').stat().st_size, 'bytes')


if __name__ == '__main__':
    main()
