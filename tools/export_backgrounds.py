"""Export unique full background planes from Model 2 capture files."""
from pathlib import Path
import hashlib, json
from PIL import Image, ImageDraw
from decode_bitmap_capture import decode

ROOT=Path(__file__).resolve().parents[1]
source=ROOT/'analysis/bitmap-captures'
dest=ROOT/'data/textures/original/backgrounds'
dest.mkdir(parents=True,exist_ok=True)
records={}
for p in sorted(source.glob('*.bin'),key=lambda p:p.stat().st_mtime):
    meta=dict(line.split('=',1) for line in p.with_suffix('.txt').read_text().splitlines())
    mode=int(meta['layer1_mode'])
    if mode not in (1,2):continue
    pages=decode(p)
    size=(512,1024) if mode==1 else (1024,512)
    plane=Image.new('RGBA',size)
    plane.paste(pages[2],(0,0));plane.paste(pages[3],(0,512) if mode==1 else (512,0))
    digest=hashlib.sha256(plane.tobytes()).hexdigest()
    name='bg_'+digest[:16]+'.png'
    if name not in records:
        plane.save(dest/name)
        records[name]={'file':name,'width':size[0],'height':size[1], 'rgba_sha256':digest,
                       'layout_mode':mode,'layer':1,'capture_files':[]}
    records[name]['capture_files'].append(p.name)
(dest/'manifest.json').write_text(json.dumps({'schema':1,'kind':'runtime_background_planes','images':list(records.values())},indent=2))
items=list(records.values()); sheet=Image.new('RGB',(1024,300*((len(items)+3)//4)),(38,38,38));d=ImageDraw.Draw(sheet)
for i,item in enumerate(items):
    im=Image.open(dest/item['file']); im.thumbnail((250,270))
    x=(i%4)*256;y=(i//4)*300
    sheet.paste(im,(x,y+24),im); d.text((x+2,y+2),item['file'][:19],fill='white')
if items:sheet.save(dest/'index.png')
print(f'{len(items)} unique full planes exported to {dest}')
