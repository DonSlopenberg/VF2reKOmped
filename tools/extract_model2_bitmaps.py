"""Offline export of every listed Model 2 character bank and its palette.

Tile sheets preserve original tile order. They are not invented sprite crops.
"""
from pathlib import Path
import argparse, hashlib, json, shutil, struct
import numpy as np
from PIL import Image, ImageDraw
from extract_arena_bitmaps import load_tiles,load_palette,u32

def lists(rom,start,end,palettes=False):
    result=[];cursor=start
    while cursor<end:
        result.append(cursor)
        while u32(rom,cursor):
            cursor+=8+(u32(rom,cursor+4)*2 if palettes else 0)
            if cursor>end:raise ValueError('Table exceeds known boundary')
        cursor+=4
    if cursor!=end:raise ValueError('Table boundary mismatch')
    return result

def export(root,banks,dest):
    rom=(banks/'rom_data.bin.raw').read_bytes()
    tile_lists=lists(rom,0x80001c,0x8003d4)
    palette_lists=lists(rom,0x8003d4,0x805cb4,True)
    if len(tile_lists)!=len(palette_lists):raise ValueError('Palette/tile group count mismatch')
    records=[];previews=[]
    for index,(tile_list,palette_list) in enumerate(zip(tile_lists,palette_lists)):
        tiles=np.zeros((16384,8,8),dtype=np.uint8);palette=np.zeros((4096,4),dtype=np.uint8)
        load_tiles(rom,0x0280001c,tiles);load_palette(rom,0x028003d4,palette)
        sources=load_tiles(rom,0x02000000+tile_list,tiles)
        colors=load_palette(rom,0x02000000+palette_list,palette)
        directory=dest/f'set_{index:02d}_rom_{tile_list:06x}'
        directory.mkdir(parents=True,exist_ok=True)
        group_records=[]
        for source in sources:
            first=source['first_tile'];count=source['tile_count'];columns=min(32,count);rows=(count+columns-1)//columns
            pixels=np.zeros((rows*8,columns*8,4),dtype=np.uint8)
            for n in range(count):
                code=first+n;pen=tiles[code].astype(np.uint16);rgba=palette[((code>>7)&255)*16+pen].copy()
                rgba[:,:,3]=np.where(pen==0,0,255)
                y,x=divmod(n,columns);pixels[y*8:y*8+8,x*8:x*8+8]=rgba
            name=f'bank_{source["rom_offset"]:08x}_tile_{first:04x}.png'
            Image.fromarray(pixels).save(directory/name)
            # Exact 0..15 pen values remain available even when a caller uses
            # a palette override rather than the default tile-address palette.
            indices=np.zeros((rows*8,columns*8),dtype=np.uint8)
            for n in range(count):
                y,x=divmod(n,columns);indices[y*8:y*8+8,x*8:x*8+8]=tiles[first+n]
            Image.fromarray(indices).save(directory/(Path(name).stem+'.indices.png'))
            item={'file':str((directory/name).relative_to(dest)).replace('\\','/'),'kind':'indexed_tile_sheet',
                'width':pixels.shape[1],'height':pixels.shape[0],'tile_width':8,'tile_height':8,
                'tiles_per_row':columns,'last_row_padding_tiles':rows*columns-count,
                'indices_file':str((directory/(Path(name).stem+'.indices.png')).relative_to(dest)).replace('\\','/'),
                **source,'rgba_sha256':hashlib.sha256(pixels.tobytes()).hexdigest()}
            group_records.append(item);records.append(item)
        # Full VRAM layout for selecting/locating tiles. Pixel file names remain stable.
        tiled=tiles.reshape(128,128,8,8).transpose(0,2,1,3).reshape(1024,1024)
        codes=np.indices((1024,1024));code=(codes[0]//8)*128+codes[1]//8
        atlas=palette[((code>>7)&255)*16+tiled].copy();atlas[:,:,3]=np.where(tiled==0,0,255)
        Image.fromarray(atlas).save(directory/'vram_layout.png')
        swatch=palette.reshape(256,16,4).copy();swatch[:,:,3]=255
        Image.fromarray(swatch).save(directory/'palette.png')
        metadata={'set_index':index,'tile_list_offset':tile_list,'palette_list_offset':palette_list,
            'palette_banks':colors,'images':group_records,'palette_lookup':'((absolute_tile_index >> 7) & 255) * 16 + pen',
            'zero_pen_transparent':True,'vram_layout':'1024x1024; 128 tiles per row',
            'note':'Source tile sheets, not reconstructed sprite rectangles. Do not crop or rearrange tiles.'}
        (directory/'manifest.json').write_text(json.dumps(metadata,indent=2),encoding='utf-8')
        preview=Image.new('RGB',(300,330),(36,36,36));im=Image.fromarray(atlas);im.thumbnail((300,300))
        preview.paste(im,(0,30),im);ImageDraw.Draw(preview).text((4,5),directory.name,fill='white');previews.append(preview)
    manifest={'schema':1,'kind':'model2_original_2d_tile_banks','source_path':'media/rom/vf2_rom/rom_data.bin',
        'source_decoded_sha256':hashlib.sha256(rom).hexdigest(),'sets':len(tile_lists),'images':records}
    (dest/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    sheet=Image.new('RGB',(900,330*((len(previews)+2)//3)))
    for n,im in enumerate(previews):sheet.paste(im,((n%3)*300,(n//3)*330))
    sheet.save(dest/'index.png')
    return manifest

if __name__=='__main__':
    root=Path(__file__).resolve().parents[1]
    ap=argparse.ArgumentParser();ap.add_argument('--banks',type=Path,default=root/'analysis/bitmap-captures')
    ap.add_argument('--output',type=Path,default=root/'data/textures/original/media/rom/vf2_rom/rom_data.bin')
    args=ap.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    result=export(root,args.banks,args.output)
    print(json.dumps({'sets':result['sets'],'tile_bank_pngs':len(result['images']),'output':str(args.output)}))
