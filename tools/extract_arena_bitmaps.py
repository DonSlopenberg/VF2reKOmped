"""Offline extraction of VF2 Model 2 arena panoramas from decoded ROM banks.

Input banks are byte-exact copies of the decompressed original game files.
No game process, GPU capture, scaling or image generation is used.
"""
from pathlib import Path
import argparse, hashlib, json, struct
import numpy as np
from PIL import Image, ImageDraw

ROM_BASE=0x02000000
def u32(data,offset): return struct.unpack_from('<I',data,offset)[0]
def offset(address,data):
    result=address-ROM_BASE
    if not 0<=result<len(data):raise ValueError(f'Invalid ROM address {address:08x}')
    return result

def load_tiles(rom,address,tiles):
    cursor=offset(address,rom); sources=[]
    while (source:=u32(rom,cursor)):
        destination=u32(rom,cursor+4);start=(destination-0x01080000)//32
        pos=offset(source,rom); count=u32(rom,pos)
        if not 0<count<=16384 or not 0<=start<16384 or start+count>16384:
            raise ValueError('Invalid character bank')
        words=np.frombuffer(rom,dtype='<u2',count=count*16,offset=pos+4).astype(np.uint16)
        pixels=np.stack([(words>>s)&15 for s in (12,8,4,0)],axis=1).reshape(count,8,8)
        tiles[start:start+count]=pixels
        sources.append({'rom_offset':pos,'first_tile':start,'tile_count':count})
        cursor+=8
        if len(sources)>256:raise ValueError('Unterminated character list')
    return sources

def load_palette(rom,address,palette):
    cursor=offset(address,rom); sources=[]
    while (destination:=u32(rom,cursor)):
        count=u32(rom,cursor+4);start=(destination-0x01800000)//2
        if not 0<count<=4096 or not 0<=start<4096 or start+count>4096:
            raise ValueError('Invalid palette bank')
        value=np.frombuffer(rom,dtype='<u2',count=count,offset=cursor+8).astype(np.uint32)
        rgb=np.stack([(value>>s)&31 for s in (0,5,10)],axis=1)*255//31
        rgb=np.where((value[:,None]&0x8000)!=0,rgb,rgb//2)
        palette[start:start+count,:3]=rgb
        palette[start:start+count,3]=255
        sources.append({'rom_offset':cursor+8,'first_color':start,'color_count':count})
        cursor+=8+count*2
        if len(sources)>256:raise ValueError('Unterminated palette list')
    return sources

def extract(program,rom,dest):
    dest.mkdir(parents=True,exist_ok=True);records=[]; previews=[];unique={};bindings=[]
    for arena in range(16):
        record=0x2502c+arena*32
        tile_list=u32(program,record+8);palette_list=u32(program,record+12);descriptor=u32(program,record+16)
        if not 0x02800000<=tile_list<0x03000000 or not 0x02800000<=palette_list<0x03000000:break
        if not 0<descriptor<len(program)-76:raise ValueError('Invalid panorama descriptor')
        rows,vertical_parameter=struct.unpack_from('<HH',program,descriptor)
        if not 1<=rows<=128:raise ValueError('Invalid panorama height')
        pointers=[u32(program,descriptor+4+i*4) for i in range(18)]
        period=next(n for n in range(1,19) if all(v==pointers[i%n] for i,v in enumerate(pointers)))
        tiles=np.zeros((16384,8,8),dtype=np.uint8);palette=np.zeros((4096,4),dtype=np.uint8)
        common_tiles=load_tiles(rom,0x0280001c,tiles)
        common_palette=load_palette(rom,0x028003d4,palette)
        banks=load_tiles(rom,tile_list,tiles);colors=load_palette(rom,palette_list,palette)
        panels=[];tilemaps=[]
        for pointer in pointers[:period]:
            pos=offset(pointer,rom)
            tilemap=np.frombuffer(rom,dtype='<u2',count=rows*32,offset=pos).reshape(rows,32).astype(np.uint32)
            yy,xx=np.indices((rows*8,256));attribute=tilemap[yy//8,xx//8]
            code=attribute&0x3fff;pen=tiles[code,yy%8,xx%8]
            rgba=palette[((attribute>>7)&255)*16+pen].copy();rgba[:,:,3]=np.where(pen==0,0,255)
            panels.append(rgba);tilemaps.append(pos)
        pixels=np.concatenate(panels,axis=1)
        digest=hashlib.sha256(pixels.tobytes()).hexdigest()
        if digest in unique:
            bindings.append({'arena_index':arena,'file':unique[digest], 'program_record_offset':record})
            continue
        filename=f'arena_{arena:02d}_rom_{descriptor:06x}.png'
        unique[digest]=filename
        bindings.append({'arena_index':arena,'file':filename,'program_record_offset':record})
        image=Image.fromarray(pixels);image.save(dest/filename)
        records.append({'file':filename,'arena_index':arena,'program_record_offset':record,
            'descriptor_offset':descriptor,'width':image.width,'height':image.height,
            'vertical_parameter':vertical_parameter,'panel_width':256,'panel_count':period,
            'tilemap_rom_offsets':tilemaps,'tile_banks':banks,'palette_banks':colors,
            'rgba_sha256':digest})
        preview=Image.new('RGB',(1024,200),(40,40,40));thumb=image.copy();thumb.thumbnail((1024,170))
        preview.paste(thumb,(0,25),thumb);ImageDraw.Draw(preview).text((5,5),f'{filename}  {image.width} x {image.height}',fill='white');previews.append(preview)
    manifest={'schema':1,'kind':'original_arena_panoramas','source':'decoded_original_ROM_banks',
        'program_sha256':hashlib.sha256(program).hexdigest(),'data_sha256':hashlib.sha256(rom).hexdigest(),
        'images':records,'arena_bindings':bindings}
    (dest/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    if previews:
        sheet=Image.new('RGB',(1024,len(previews)*200))
        for i,p in enumerate(previews):sheet.paste(p,(0,i*200))
        sheet.save(dest/'index.png')
    return records

if __name__=='__main__':
    root=Path(__file__).resolve().parents[1]
    ap=argparse.ArgumentParser();ap.add_argument('--banks',type=Path,default=root/'analysis/bitmap-captures')
    ap.add_argument('--output',type=Path,default=root/'data/textures/original/arenas');args=ap.parse_args()
    records=extract((args.banks/'ic12_15.bin.raw').read_bytes(),(args.banks/'rom_data.bin.raw').read_bytes(),args.output)
    print(json.dumps([{'file':r['file'],'size':[r['width'],r['height']]} for r in records],indent=2))
