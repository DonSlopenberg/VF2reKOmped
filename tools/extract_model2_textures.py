"""Extract pointer-referenced, uncompressed VF2 Model 2 luminance textures.

This handles encoding 1 only. Encoding 0 is recorded, never guessed/decoded.
No game process is required. Source paths are preserved under the output root.
"""
from pathlib import Path
import argparse, hashlib, json, struct
import numpy as np
from PIL import Image

BASE = 0x02000000

def u32(data, at):
    return struct.unpack_from('<I', data, at)[0]

def rom_offset(address, data):
    at = address - BASE
    return at if 0 <= at < len(data) else None

def pointer_run(data, start):
    result = []
    for at in range(start, min(start + 4096, len(data) - 3), 4):
        address = u32(data, at)
        offset = rom_offset(address, data)
        if offset is None:
            break
        result.append(offset)
    return result

def decode_native_words(data, width, height):
    """16-bit words contain a 2x2 block: high nibble is its upper left."""
    words = np.frombuffer(data, dtype='<u2').reshape(height // 2, width // 2)
    pixels = np.empty((height, width), dtype=np.uint8)
    for yy, xx, shift in ((0, 0, 12), (0, 1, 8), (1, 0, 4), (1, 1, 0)):
        pixels[yy::2, xx::2] = ((words >> shift) & 15) * 17
    # Every source nibble must survive export losslessly.
    reconstructed = ((pixels[0::2, 0::2].astype(np.uint16) // 17) << 12)
    reconstructed |= (pixels[0::2, 1::2].astype(np.uint16) // 17) << 8
    reconstructed |= (pixels[1::2, 0::2].astype(np.uint16) // 17) << 4
    reconstructed |= pixels[1::2, 1::2].astype(np.uint16) // 17
    if not np.array_equal(reconstructed, words):
        raise ValueError('Luminance extraction did not round-trip')
    return pixels

def extract(data, output):
    destination = output / 'media/rom/vf2_rom/rom_data.bin/model2_luminance'
    destination.mkdir(parents=True, exist_ok=True)
    # Version 2.2 TextureData directory, addressed by original i960 program.
    if abs(struct.unpack_from('<f', data, 0x300000)[0] - 2.2) > 0.001:
        raise ValueError('Unsupported TextureData version')
    directory = rom_offset(u32(data, 0x30000C), data)
    if directory is None:
        raise ValueError('Invalid TextureData directory')
    groups = pointer_run(data, directory)
    candidates = {}
    for group in groups:
        for texture in pointer_run(data, group + 4):
            candidates.setdefault(texture, []).append(group)
    records = []
    for at, owners in sorted(candidates.items()):
        encoding, header = struct.unpack_from('<II', data, at)
        width, height = (header & 255) + 1, ((header >> 8) & 255) + 1
        record = dict(rom_offset=at, encoding=encoding, header=f'{header:08x}',
                      width=width, height=height, group_offsets=owners)
        if encoding != 1:
            record['status'] = 'compressed_encoding_not_exported' if encoding == 0 else 'unknown_encoding_not_exported'
            records.append(record)
            continue
        size = width * height // 2
        if width % 2 or height % 2 or at + 16 + size > len(data):
            record['status'] = 'invalid_dimensions_not_exported'
            records.append(record)
            continue
        source = data[at + 16:at + 16 + size]
        pixels = decode_native_words(source, width, height)
        filename = f'texture_{at:06x}_{width}x{height}_luma.png'
        Image.fromarray(pixels, 'L').save(destination / filename)
        record.update(file=filename, status='exported', mip_level=0,
                      packed_sha256=hashlib.sha256(source).hexdigest(),
                      pixel_sha256=hashlib.sha256(pixels.tobytes()).hexdigest())
        records.append(record)
    manifest = dict(schema=1, source='media/rom/vf2_rom/rom_data.bin',
                    decoded_sha256=hashlib.sha256(data).hexdigest(),
                    kind='Model2_4bit_luminance_texture',
                    byte_order='little-endian 16-bit; 2x2 nibbles 12,8,4,0',
                    directory_offset=directory, group_count=len(groups), images=records,
                    coverage='Only pointer-referenced encoding 1 base mip levels; encoding 0 and material colors remain unresolved.')
    (destination / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    (destination / 'README.md').write_text(
        '# Model 2 luminance textures\n\n'
        'These PNG files contain the original 4-bit luminance masks expanded losslessly to 8-bit grayscale. '
        'Model 2 supplies polygon material colors separately; these are not missing RGB image channels.\n\n'
        'Only verified pointer-referenced, uncompressed encoding 1 blocks and their base mip level are exported. '
        'Compressed encoding 0 blocks are listed in manifest.json, without speculative image output. '
        'The native 2x2 word layout is decoded from little-endian 16-bit words.\n\n'
        'Keep the original copies. Upscaled replacements are not loaded automatically by the game yet.\n', encoding='utf-8')
    return manifest

if __name__ == '__main__':
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, default=root/'analysis/bitmap-captures/rom_data.bin.raw')
    parser.add_argument('--output', type=Path, default=root/'data/textures/original')
    args = parser.parse_args()
    manifest = extract(args.source.read_bytes(), args.output)
    print(json.dumps({'groups':manifest['group_count'], 'candidates':len(manifest['images']),
                      'exported':sum(i['status']=='exported' for i in manifest['images'])}))
