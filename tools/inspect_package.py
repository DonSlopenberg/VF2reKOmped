"""Bounded STFS extraction for this read-only LIVE package; Python stdlib only.

Format references: Xenia stfs_xbox.h and stfs_container_device.cc (BSD).
Does not execute game code or modify the source. Verifies the SHA-1 tree,
not the Microsoft signature or the user's entitlement.
"""
from pathlib import Path
import hashlib
import json
import math

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / '5841129C/000D0000/65D8FAC10B21FA185AAB93607663594CB568C78658'
OUT = ROOT / 'extracted'
REPORT = ROOT / 'analysis'

def main():
    data = SOURCE.read_bytes()
    def read(pos, size):
        if pos < 0 or pos + size > len(data):
            raise ValueError(f'Out-of-bounds read: {pos:#x}+{size:#x}')
        return data[pos:pos + size]
    def num(pos, size, endian='big'):
        return int.from_bytes(read(pos, size), endian)
    if read(0, 4) != b'LIVE' or num(0x3A9, 4) != 0:
        raise ValueError('Only single-file LIVE/STFS supported')
    if num(0x379, 1) != 0x24 or not (num(0x37B, 1) & 1):
        raise ValueError('Only read-only hash layout supported')
    total = num(0x395, 4)
    if not 170 < total <= 170**2:
        raise ValueError('Only two-level package supported')
    base = (num(0x340, 4) + 4095) & ~4095
    def offset(block):
        if not 0 <= block < total:
            raise ValueError(f'Invalid block {block}')
        physical = block + block // 170 + 1
        if block >= 170:
            physical += block // (170**2) + 1
        return base + physical * 4096
    def hash_table(group):
        physical = 0 if group == 0 else group * 171 + 1
        return read(base + physical * 4096, 4096)
    def block_record(block):
        table = hash_table(block // 170)
        index = (block % 170) * 24
        return table[index:index+24]
    def next_block(block):
        return int.from_bytes(block_record(block)[21:24], 'big')
    top = read(base + 171 * 4096, 4096)
    if hashlib.sha1(top).digest() != read(0x381, 20):
        raise ValueError('Root hash mismatch')
    groups = math.ceil(total / 170)
    for group in range(groups):
        if hashlib.sha1(hash_table(group)).digest() != top[group*24:group*24+20]:
            raise ValueError(f'Hash table mismatch {group}')
    for block in range(total):
        if hashlib.sha1(read(offset(block),4096)).digest() != block_record(block)[:20]:
            raise ValueError(f'Data hash mismatch {block}')
    table_count = num(0x37C,2,'little')
    table_block = num(0x37E,3,'little')
    entries = []
    visited = set()
    for _ in range(table_count):
        if table_block in visited:
            raise ValueError('Directory chain cycle')
        visited.add(table_block)
        table = read(offset(table_block),4096)
        for index in range(64):
            row = table[index*64:(index+1)*64]
            if row[0] == 0:
                break
            length = row[40] & 63
            if not 0 < length <= 40:
                raise ValueError('Invalid filename length')
            name = row[:length].decode('ascii')
            if name in ('.','..') or any(c in name for c in '/\\:'):
                raise ValueError('Unsafe name')
            parent = int.from_bytes(row[50:52],'big')
            if parent != 65535 and (parent >= len(entries) or not entries[parent]['directory']):
                raise ValueError('Invalid parent directory')
            path = name if parent == 65535 else entries[parent]['path'] + '/' + name
            entries.append(dict(path=path, directory=bool(row[40]&128),
                contiguous=bool(row[40]&64), size=int.from_bytes(row[52:56],'big'),
                start_block=int.from_bytes(row[47:50],'little'),
                allocated_blocks=int.from_bytes(row[44:47],'little')))
        table_block = next_block(table_block)
    if table_block != 0xFFFFFF:
        raise ValueError('Directory chain longer than metadata')
    OUT.mkdir(exist_ok=True)
    for entry in entries:
        target = OUT / entry['path']
        if not target.resolve().is_relative_to(OUT.resolve()):
            raise ValueError('Path escapes output')
        if entry['directory']:
            target.mkdir(parents=True,exist_ok=True)
            continue
        chunks = []
        block = entry['start_block']
        remaining = entry['size']
        seen = set()
        while remaining:
            if block in seen or block == 0xFFFFFF:
                raise ValueError('Invalid file chain')
            seen.add(block)
            chunks.append(read(offset(block),min(remaining,4096)))
            remaining -= len(chunks[-1])
            block = next_block(block)
        if len(seen) != entry['allocated_blocks'] or (seen and block != 0xFFFFFF):
            raise ValueError('File block count/end mismatch')
        payload = b''.join(chunks)
        target.parent.mkdir(parents=True,exist_ok=True)
        if target.exists() and target.read_bytes() != payload:
            raise ValueError(f'Refusing to overwrite different file {target}')
        target.write_bytes(payload)
        entry['sha256'] = hashlib.sha256(payload).hexdigest()
        entry['magic_hex'] = payload[:16].hex()
    result = dict(source=str(SOURCE), source_sha256=hashlib.sha256(data).hexdigest(),
        source_size=len(data), title=read(0x411,256).decode('utf-16-be').rstrip('\0'),
        title_id=f'{num(0x360,4):08X}', total_data_blocks=total,
        verification=dict(root_sha1=True, hash_tables=groups, data_sha1_blocks=total,
            signature='not_verified', game_execution='not_performed'), entries=entries)
    REPORT.mkdir(exist_ok=True)
    (REPORT/'package_inventory.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k != 'entries'},indent=2))
    for e in entries:
        print(f"{e['size']:>10} {'DIR' if e['directory'] else 'FILE'} {e['path']}")

if __name__ == '__main__':
    main()
