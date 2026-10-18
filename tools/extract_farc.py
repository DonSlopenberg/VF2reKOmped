"""Offline XMem TD/FARC extraction using the already bundled ReX LZX decoder.

Original archive names and directory structure are retained. No game execution.
"""
from pathlib import Path
import argparse
import ctypes
import gzip
import hashlib
import io
import json
import os
import struct

ROOT = Path(__file__).resolve().parents[1]

def u32(data, offset):
    return struct.unpack_from('>I', data, offset)[0]

def decode_xmem(data):
    if data[:4] != b'\x0f\xf5\x12\xed':
        return data
    flags = u32(data, 12)
    count = (flags >> 6) & 65535
    bits = 32 if flags & 0x00c00000 else 20
    pitch = 0x8000 << ((flags >> 4) & 3)
    window = 1 << ((flags & 15) + 15)
    table_end = 16 + ((count * bits + 31) // 32) * 4
    table = int.from_bytes(data[16:table_end], 'big')
    table_bits = (table_end - 16) * 8
    sizes = [(table >> (table_bits-(i+1)*bits)) & ((1 << bits)-1) for i in range(count)]
    if not count or sum(sizes) > 512*1024*1024 or table_end > len(data):
        raise ValueError('Invalid XMem segment table')
    dll_dir = ROOT/'data/engine'
    with os.add_dll_directory(str(dll_dir)):
        dll = ctypes.CDLL(str(dll_dir/'rexruntimerd.dll'))
        decode = getattr(dll, '?lzx_decompress@@YAHPEBX_KPEAX1I21@Z')
        decode.argtypes = [ctypes.c_void_p,ctypes.c_size_t,ctypes.c_void_p,ctypes.c_size_t,ctypes.c_uint32,ctypes.c_void_p,ctypes.c_size_t]
        decode.restype = ctypes.c_int
        result = bytearray()
        for i, size in enumerate(sizes):
            pos = table_end if i == 0 else i*pitch
            end = min((i+1)*pitch,len(data))
            raw = bytearray()
            output_count = 0
            while output_count < size:
                if pos+2 > end: raise ValueError('Truncated XMem chunk')
                if data[pos] == 255:
                    if pos+5 > end: raise ValueError('Truncated extended XMem chunk')
                    outsize = int.from_bytes(data[pos+1:pos+3],'big')
                    compressed = int.from_bytes(data[pos+3:pos+5],'big')
                    pos += 5
                else:
                    outsize = 32768
                    compressed = int.from_bytes(data[pos:pos+2],'big')
                    pos += 2
                if not compressed or pos+compressed > end: raise ValueError('Invalid XMem chunk size')
                raw += data[pos:pos+compressed]
                pos += compressed
                output_count += min(outsize,size-output_count)
            source = ctypes.create_string_buffer(bytes(raw))
            dest = ctypes.create_string_buffer(size)
            status = decode(source,len(raw),dest,size,window,None,0)
            if status: raise ValueError(f'LZX decoder failed: segment {i}, status {status}')
            result += dest.raw
    return bytes(result)

def extract(data, folder):
    data = decode_xmem(data)
    signature = data[:4]
    if signature not in (b'FArc',b'FArC'):
        raise ValueError(f'Unsupported FARC signature: {signature!r}')
    header_end = u32(data,4)+8
    pos = 12
    exported = []
    while pos < header_end:
        name_end = data.index(0,pos,header_end)
        name = data[pos:name_end].decode('utf-8')
        pos = name_end+1
        offset, size = struct.unpack_from('>II',data,pos)
        pos += 8
        unpacked = 0
        if signature == b'FArC':
            unpacked = u32(data,pos)
            pos += 4
        target = (folder/name).resolve()
        if not target.is_relative_to(folder.resolve()): raise ValueError('Unsafe archive member path')
        if offset+size > len(data): raise ValueError('Member outside archive')
        content = data[offset:offset+size]
        if unpacked:
            content = gzip.decompress(content)
            if len(content) != unpacked: raise ValueError('FARC decompression length mismatch')
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_bytes(content)
        exported.append((name,len(content),content[:12].hex()))
    return exported

def export_archive_tree(data, folder, manifest=None, source=''):
    """Extract nested archives, retaining original DDS atlases.

    Archives become directories with their exact original names. Original
    DDS images are inspected in memory without creating PNG duplicates.
    """
    from PIL import Image
    folder=folder.resolve()
    if manifest is None: manifest=[]
    members=extract(data,folder)
    for name,size,magic in members:
        path=folder/name
        content=path.read_bytes()
        member_source=source+'/'+name
        if content[:4] in (b'FArc',b'FArC',b'\x0f\xf5\x12\xed'):
            # Retain raw data, and use the exact archive member name for its
            # directory so the exported path remains unambiguous.
            rawpath=(ROOT/'analysis/unpacked-archives'/member_source).resolve()
            if not rawpath.is_relative_to((ROOT/'analysis/unpacked-archives').resolve()):
                raise ValueError('Unsafe source archive path')
            rawpath.parent.mkdir(parents=True,exist_ok=True)
            rawpath.write_bytes(content)
            path.unlink()
            export_archive_tree(content,path,manifest,member_source)
        elif content[:4] == b'DDS ':
            with Image.open(io.BytesIO(content)) as image:
                image.load()
                rgba=image.convert('RGBA')
                manifest.append(dict(source=member_source,dds=str(path.relative_to(ROOT)),kind='original_dds',
                    width=rgba.width,height=rgba.height,source_sha256=hashlib.sha256(content).hexdigest(),
                    rgba_sha256=hashlib.sha256(rgba.tobytes()).hexdigest()))
        elif content[:8] == b'\x89PNG\r\n\x1a\n':
            with Image.open(path) as image:
                image.load()
                manifest.append(dict(source=member_source,png=str(path.relative_to(ROOT)),
                    width=image.width,height=image.height,source_sha256=hashlib.sha256(content).hexdigest(),
                    png_sha256=hashlib.sha256(content).hexdigest(),png_bytes=len(content)))
    return manifest

def extract_recursive(source,relative,out):
    """Export one original archive while retaining its path relative to game data."""
    source=Path(source)
    relative=Path(relative)
    out=Path(out).resolve()
    folder=(out/relative).resolve()
    if not folder.is_relative_to(out): raise ValueError('Unsafe relative source path')
    return export_archive_tree(source.read_bytes(),folder,source=str(relative))

def export_sprite_rectangles(folder, relative):
    """Export exact integer rectangles from VF2's 48-byte sprite records.

    The file stores indices, not names. UVs independently validate every
    rectangle against the selected atlas dimensions before any crop is saved.
    """
    from PIL import Image
    folder=Path(folder).resolve()
    relative=Path(relative)
    data=(folder/'sprite.bin').read_bytes()
    atlas_count,count,table=struct.unpack_from('>III',data)
    archive=(ROOT/'analysis/unpacked-archives'/relative/'texture.farc').read_bytes()
    pos=12
    names=[]
    while pos<u32(archive,4)+8:
        end=archive.index(0,pos)
        names.append(archive[pos:end].decode('utf-8'))
        pos=end+1+8
    if atlas_count!=len(names): raise ValueError('Sprite atlas count mismatch')
    images=[Image.open(folder/'texture.farc'/name).convert('RGBA') for name in names]
    records=[]
    for i in range(count):
        pointer=u32(data,table+4*i)
        atlas,flags,a,b,*values=struct.unpack_from('>4I8f',data,pointer)
        if atlas>=len(images) or a or b: raise ValueError('Unrecognized sprite record')
        u0,v0,u1,v1,x,y,w,h=values
        image=images[atlas]
        if any(abs(v-round(v))>0.001 for v in (x,y,w,h)):
            raise ValueError('Sprite rectangle is not an exact integer rectangle')
        x,y,w,h=map(round,(x,y,w,h))
        if x<0 or y<0 or w<1 or h<1 or x+w>image.width or y+h>image.height:
            raise ValueError('Sprite rectangle outside atlas')
        expected=(x/image.width,y/image.height,(x+w)/image.width,(y+h)/image.height)
        if any(abs(a-b)>0.00001 for a,b in zip(values[:4],expected)):
            raise ValueError('Sprite UV and rectangle mismatch')
        records.append((i,atlas,flags,x,y,w,h))
    output=folder/'sprites'
    output.mkdir(exist_ok=True)
    manifest=[]
    for i,atlas,flags,x,y,w,h in records:
        target=output/f'sprite_{i:05d}.png'
        images[atlas].crop((x,y,x+w,y+h)).save(target)
        manifest.append(dict(index=i,atlas=names[atlas],flags=flags,x=x,y=y,width=w,height=h,
            png=str(target.relative_to(ROOT)),png_sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
    (output/'sprite_manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    return manifest

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archives',nargs='+',type=Path)
    parser.add_argument('--output',type=Path,default=ROOT/'data/textures/original')
    parser.add_argument('--recursive',action='store_true',help='Extract nested FARC archives, preserving DDS images')
    args=parser.parse_args()
    args.output=args.output.resolve()
    manifest=[]
    for archive in args.archives:
        archive=archive.resolve()
        rel=archive.relative_to(ROOT/'data/game')
        folder=args.output/rel
        if args.recursive:
            if folder.is_file(): raise ValueError(f'Output path is already a file: {folder}')
            # Re-running safely restores source files before recursively
            # replacing them by directories.
            if folder.exists() and any(p.is_dir() for p in folder.iterdir()):
                raise ValueError(f'Use a fresh output folder for recursive export: {folder}')
            before=len(manifest)
            export_archive_tree(archive.read_bytes(),folder,manifest,str(rel))
            print(str(rel),f'{len(manifest)-before} images')
        else:
            members=extract(archive.read_bytes(),folder)
            print(str(rel),members)
    if args.recursive:
        args.output.mkdir(parents=True,exist_ok=True)
        (args.output/'farc_image_manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
