"""Reconstruct unscrolled Model 2 bitmap planes from opt-in CPU captures."""
from pathlib import Path
import argparse, hashlib, json
import numpy as np
from PIL import Image, ImageDraw

def decode(path, endian='<', mask=0x3fff):
    raw=path.read_bytes()
    if len(raw)!=0x118c00: raise ValueError('Incomplete capture')
    chars=np.frombuffer(raw[:0x100000],dtype=np.uint8).reshape(1024,1024)
    palette=np.frombuffer(raw[0x108000:0x10c000],dtype=np.uint8).reshape(4096,4)
    maps=np.frombuffer(raw[0x100000:0x108000],dtype=endian+'u2').reshape(128,128)
    images=[]
    for row,col in [(0,0),(0,64),(64,0),(64,64)]:
        tile=maps[row:row+64,col:col+64].astype(np.uint32)
        yy,xx=np.indices((512,512)); attr=tile[yy//8,xx//8]
        code=attr&mask
        pen=chars[(code//128)*8+yy%8,(code%128)*8+xx%8]
        # Palette conversion in sub_82143798 writes B,G,R,A bytes.
        rgba=palette[((attr>>7)&255)*16+pen][:,:,[2,1,0,3]].copy()
        rgba[:,:,3]=np.where(pen==0,0,255)
        images.append(Image.fromarray(rgba))
    return images

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('input',type=Path);ap.add_argument('output',type=Path);ap.add_argument('--endian',default='<');args=ap.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    files=sorted(args.input.glob('*.bin'),key=lambda p:p.stat().st_mtime)
    thumbs=[]
    for p in files:
        for i,img in enumerate(decode(p,args.endian)):
            name=f'{p.stem}_page{i}.png';img.save(args.output/name)
            thumb=Image.new('RGB',(256,280),(48,48,48));thumb.paste(img.resize((256,256)),(0,24),img.resize((256,256)))
            ImageDraw.Draw(thumb).text((3,3),f'{p.stem[:8]} page{i}',fill='white');thumbs.append(thumb)
    sheet=Image.new('RGB',(1024,280*((len(thumbs)+3)//4)),(20,20,20))
    for i,img in enumerate(thumbs):sheet.paste(img,((i%4)*256,(i//4)*280))
    if thumbs:sheet.save(args.output/'contact.png')
    print(f'Decoded {len(files)} captures / {len(thumbs)} pages')
