"""Smaller PNG without visible change (standard library only).

python tools/optimize_png.py <input.png> <output.png> [--trim] [--width N]

Decodes 8-bit RGB/RGBA non-interlaced PNGs, clears the colour of fully
transparent pixels, picks the best filter per row, recompresses at zlib
level 9 and keeps only IHDR/IDAT/IEND. --trim crops to the visible pixels;
--width downsizes with an area average (premultiplied alpha). The written
file is verified against the processed image.
"""
import struct, sys, zlib


def read_png(path):
    data = open(path, 'rb').read()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', 'not a PNG'
    pos, idat, header = 8, b'', None
    while pos < len(data):
        length, kind = struct.unpack('>I4s', data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        if kind == b'IHDR':
            header = struct.unpack('>IIBBBBB', body)
        elif kind == b'IDAT':
            idat += body
        pos += 12 + length
    width, height, depth, colour, _, _, interlace = header
    assert depth == 8 and colour in (2, 6) and not interlace, 'only 8-bit RGB/RGBA, non-interlaced'
    bpp = 4 if colour == 6 else 3
    raw, stride = zlib.decompress(idat), width * bpp
    rows, previous = [], bytearray(stride)
    for y in range(height):
        start = y * (stride + 1)
        kind, line = raw[start], bytearray(raw[start + 1:start + 1 + stride])
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0
            b = previous[x]
            c = previous[x - bpp] if x >= bpp else 0
            if kind == 1: line[x] = (line[x] + a) & 255
            elif kind == 2: line[x] = (line[x] + b) & 255
            elif kind == 3: line[x] = (line[x] + ((a + b) >> 1)) & 255
            elif kind == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line)
        previous = line
    if bpp == 3:  # promote to RGBA
        rows = [bytearray(b''.join(bytes(r[i:i + 3]) + b'\xff' for i in range(0, len(r), 3))) for r in rows]
    return width, height, rows


def downsize(width, height, rows, new_width):
    new_height = max(1, round(height * new_width / width))
    out = []
    for ny in range(new_height):
        y0, y1 = ny * height / new_height, (ny + 1) * height / new_height
        line = bytearray(new_width * 4)
        for nx in range(new_width):
            x0, x1 = nx * width / new_width, (nx + 1) * width / new_width
            acc = [0.0] * 4
            weight = 0.0
            for y in range(int(y0), min(height, int(y1 + 0.999999))):
                wy = min(y + 1, y1) - max(y, y0)
                row = rows[y]
                for x in range(int(x0), min(width, int(x1 + 0.999999))):
                    w = wy * (min(x + 1, x1) - max(x, x0))
                    r, g, b, a = row[x * 4:x * 4 + 4]
                    acc[0] += r * a * w; acc[1] += g * a * w; acc[2] += b * a * w; acc[3] += a * w
                    weight += w
            alpha = acc[3] / weight
            if acc[3] > 0:
                line[nx * 4:nx * 4 + 3] = bytes(min(255, round(c / acc[3])) for c in acc[:3])
            line[nx * 4 + 3] = min(255, round(alpha))
        out.append(line)
    return new_width, new_height, out


def write_png(path, width, height, rows):
    stride, packed, previous = width * 4, bytearray(), bytearray(width * 4)
    for line in rows:
        best = None
        for kind in range(5):
            f = bytearray(stride)
            for x in range(stride):
                a = line[x - 4] if x >= 4 else 0
                b = previous[x]
                c = previous[x - 4] if x >= 4 else 0
                if kind == 0: v = line[x]
                elif kind == 1: v = line[x] - a
                elif kind == 2: v = line[x] - b
                elif kind == 3: v = line[x] - ((a + b) >> 1)
                else:
                    p = a + b - c
                    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                    v = line[x] - (a if pa <= pb and pa <= pc else b if pb <= pc else c)
                f[x] = v & 255
            score = sum(v if v < 128 else 256 - v for v in f)
            if best is None or score < best[0]:
                best = (score, kind, f)
        packed.append(best[1]); packed += best[2]
        previous = line

    def chunk(kind, body):
        return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body) & 0xffffffff)
    with open(path, 'wb') as out:
        out.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)) +
                  chunk(b'IDAT', zlib.compress(bytes(packed), 9)) + chunk(b'IEND', b''))


def main():
    source, target = sys.argv[1], sys.argv[2]
    width, height, rows = read_png(source)
    for line in rows:  # colour under alpha 0 is invisible
        for x in range(3, len(line), 4):
            if line[x] == 0:
                line[x - 3:x] = b'\0\0\0'
    if '--trim' in sys.argv:
        visible = lambda line: any(line[x] for x in range(3, len(line), 4))
        top = next(y for y in range(height) if visible(rows[y]))
        bottom = next(y for y in range(height - 1, -1, -1) if visible(rows[y])) + 1
        columns = [x for x in range(width) if any(rows[y][x * 4 + 3] for y in range(top, bottom))]
        left, right = columns[0], columns[-1] + 1
        rows = [line[left * 4:right * 4] for line in rows[top:bottom]]
        width, height = right - left, bottom - top
    if '--width' in sys.argv:
        width, height, rows = downsize(width, height, rows, int(sys.argv[sys.argv.index('--width') + 1]))
    write_png(target, width, height, rows)
    check_width, check_height, check = read_png(target)
    assert (check_width, check_height) == (width, height) and check == rows, 'verification failed'
    import os
    print(f'{source}: {os.path.getsize(source)} -> {target}: {os.path.getsize(target)} bytes ({width}x{height})')


if __name__ == '__main__':
    main()
