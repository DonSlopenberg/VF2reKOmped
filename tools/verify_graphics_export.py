"""Validate exported PNGs, source preservation and exact sprite crops offline."""
from pathlib import Path
import csv,hashlib,json
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'data/textures/original'
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
    pngs=list(OUT.rglob('*.png'))
    for p in pngs:
        with Image.open(p) as image:image.verify()
    archives=json.loads((OUT/'archive_graphics_manifest.json').read_text())
    for record in archives['images']:
        if 'png' in record or 'dds' in record:
            p=ROOT/record.get('dds',record.get('png'))
            if 'dds' in record:assert sha(p)==record['source_sha256'],p
            if 'png_sha256' in record:assert sha(p)==record['png_sha256'],p
            if 'rgba_sha256' in record:
                with Image.open(p) as im:assert hashlib.sha256(im.convert('RGBA').tobytes()).hexdigest()==record['rgba_sha256'],p
        else:
            assert sha(OUT/record['file'])==record['source_sha256']
    crop_count=0
    for m in OUT.glob('media/rom/sprite/*.farc/sprites/sprite_manifest.json'):
        for record in json.loads(m.read_text()):
            sprite=ROOT/record['png'];atlas=m.parent.parent/'texture.farc'/record['atlas']
            with Image.open(atlas) as im,Image.open(sprite) as cut:
                x,y=record['x'],record['y'];w,h=record['width'],record['height']
                assert cut.size==(w,h)
                assert cut.convert('RGBA').tobytes()==im.convert('RGBA').crop((x,y,x+w,y+h)).tobytes()
            crop_count+=1
    tile_manifest=json.loads((OUT/'media/rom/vf2_rom/rom_data.bin/manifest.json').read_text())
    for record in tile_manifest['images']:
        path=OUT/'media/rom/vf2_rom/rom_data.bin'/record['file']
        with Image.open(path) as image:
            assert image.size==(record['width'],record['height'])
            assert hashlib.sha256(image.convert('RGBA').tobytes()).hexdigest()==record['rgba_sha256']
        with Image.open(path.parent/Path(record['indices_file']).name) as image:
            assert image.getextrema()[1]<=15
    source_checks=[]
    with (ROOT/'data/textures/asset_inventory.csv').open(newline='',encoding='utf-8-sig') as f:
        for record in csv.DictReader(f):
            source=ROOT/'data/game'/record['path']
            assert sha(source)==record['sha256'].lower(),source
            source_checks.append(record['path'])
    # Independent fresh export must regenerate every image byte-for-byte.
    fresh=ROOT/'analysis/graphics-export-verification'
    rebuilt=list(fresh.rglob('*.png'))
    for path in rebuilt:
        relative=path.relative_to(fresh)
        assert (OUT/relative).is_file(),relative
        assert sha(path)==sha(OUT/relative),relative
    result={'png_files_checked':len(pngs),'atlas_or_loose_sources':len(archives['images']),
        'exact_sprite_crops_checked':crop_count,'tile_bank_sheets_checked':len(tile_manifest['images']),
        'original_game_files_unchanged':len(source_checks),'fresh_export_identical_pngs':len(rebuilt),
        'installed_engine_sha256':sha(ROOT/'data/engine/vf2.exe')}
    (ROOT/'analysis/graphics-export-verification.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(result,indent=2))
if __name__=='__main__':main()
