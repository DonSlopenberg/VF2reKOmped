"""Compress completed GPU readbacks into PNG; preserve dimensions and pixels."""
from pathlib import Path
import argparse,json
from PIL import Image

def collect(folder):
    count=0
    for meta in sorted(folder.rglob('*.txt')):
        raw=meta.with_suffix('.rgba')
        if not raw.is_file():continue
        try:
            width,height,stride=map(int,meta.read_text().split())
            data=raw.read_bytes()
            if len(data)<stride*(height-1)+width*4:continue
            image=Image.frombytes('RGBX',(width,height),data,'raw','RGBX',stride).convert('RGB')
            png=meta.with_suffix('.png');image.save(png,compress_level=1)
            with Image.open(png) as check:
                assert check.size==image.size and check.tobytes()==image.tobytes()
            raw.unlink();count+=1
        except (OSError,ValueError):continue # Capture may still be writing.
    return count

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('folder',type=Path)
    args=ap.parse_args();print(json.dumps({'captures_compressed':collect(args.folder.resolve())}))
