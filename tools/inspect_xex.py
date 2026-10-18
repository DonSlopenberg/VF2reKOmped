"""Read XEX2 headers without decoding or executing the PPC image.
Reference: Xenia kernel/util/xex2_info.h.
"""
from pathlib import Path
import hashlib
import json
import struct

ROOT = Path(__file__).resolve().parents[1]
def main():
    path = ROOT / 'extracted/default.xex'
    b = path.read_bytes()
    def raw(p,n):
        if p < 0 or p+n > len(b):
            raise ValueError('Out of bounds')
        return b[p:p+n]
    def num(p,n=4):
        return int.from_bytes(raw(p,n),'big')
    if raw(0,4) != b'XEX2':
        raise ValueError('Expected XEX2')
    count = num(20)
    if count > 256:
        raise ValueError('Excessive header count')
    opts = dict(struct.iter_unpack('>II',raw(24,8*count)))
    sec = num(16)
    f = opts[0x3ff]
    imports = opts[0x103ff]
    string_size = num(imports+4)
    library_count = num(imports+8)
    strings = raw(imports+12,string_size)
    names = [s.decode('ascii') for s in strings.split(b'\0') if s]
    pos = imports+12+string_size
    libraries = []
    end = imports + num(imports)
    for i in range(library_count):
        size = num(pos)
        name_index = num(pos+36,2)
        references = num(pos+38,2)
        if size != 40+4*references or pos+size > end:
            raise ValueError('Invalid import library length')
        libraries.append(dict(name=names[name_index],reference_count=references,
            version_hex=f'{num(pos+28):08X}',minimum_version_hex=f'{num(pos+32):08X}',
            reference_addresses=[f'0x{num(pos+40+j*4):08X}' for j in range(references)],
            ordinals='unknown until image is decrypted/decompressed'))
        pos += size
    if pos != end:
        raise ValueError('Import header length mismatch')
    static = []
    if 0x200ff in opts:
        start = opts[0x200ff]
        size = num(start)
        if (size-4)%16:
            raise ValueError('Invalid static library header')
        for pos in range(start+4,start+size,16):
            static.append(dict(name=raw(pos,8).split(b'\0')[0].decode('ascii'),
                major=num(pos+8,2),minor=num(pos+10,2),build=num(pos+12,2),qfe=num(pos+15,1)))
    execution = opts[0x40006]
    original = opts[0x183ff]
    result = dict(path=str(path),sha256=hashlib.sha256(b).hexdigest(),file_size=len(b),
        format='XEX2',image_offset=num(8),image_size=num(sec+4),
        load_address=f'0x{num(sec+0x110):08X}',entry_point=f'0x{opts[0x10100]:08X}',
        original_pe_name=raw(original+4,num(original)-4).split(b'\0')[0].decode('ascii'),
        title_id=f'{num(execution+12):08X}',version_hex=f'{num(execution+4):08X}',
        base_version_hex=f'{num(execution+8):08X}',
        encryption_type=num(f+4,2),compression_type=num(f+6,2),
        lzx_window_size=num(f+8),first_compressed_block_size=num(f+12),
        import_libraries=libraries,static_libraries=static,
        options=[dict(key=f'0x{k:08X}',value=f'0x{v:08X}') for k,v in opts.items()],
        limitations=['Header-only inspection using custom Python parser, not REA Evidence.',
            'Image not decrypted or decompressed; imports counted as references, not unique functions.',
            'No PPC disassembly, function analysis, shader analysis or runtime testing.',
            'REA open_binary rejected both STFS and XEX as unsupported binary format.'])
    (ROOT/'analysis/xex_headers.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    compact = {k:v for k,v in result.items() if k not in ('options','import_libraries')}
    compact['import_libraries'] = [{k:v for k,v in lib.items() if k != 'reference_addresses'} for lib in libraries]
    print(json.dumps(compact,indent=2))

if __name__ == '__main__':
    main()
