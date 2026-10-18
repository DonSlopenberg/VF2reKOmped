"""Export original graphics offline, retaining paths relative to data/game.

Does not launch the game or upscale assets. Requires bundled Pillow/NumPy.
Existing export directories are protected; select a fresh output to re-export.
"""
from pathlib import Path
import argparse, hashlib, json, shutil
from extract_farc import extract_recursive,export_sprite_rectangles

ROOT=Path(__file__).resolve().parents[1]

def main(out,banks):
    game=ROOT/'data/game';out.mkdir(parents=True,exist_ok=True)
    images=[];archives=[];sprites=[]
    # Preserve original loose image paths as well as archive paths.
    for source in sorted(game.rglob('*.png')):
        relative=source.relative_to(game);target=out/relative
        target.parent.mkdir(parents=True,exist_ok=True)
        if target.exists():raise FileExistsError(target)
        shutil.copy2(source,target)
        images.append({'source':relative.as_posix(),'file':relative.as_posix(),
            'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'kind':'original_png'})
    for source in sorted(game.rglob('*.farc')):
        relative=source.relative_to(game);folder=out/relative
        if folder.exists():raise FileExistsError(folder)
        exported=extract_recursive(source,relative,out)
        images.extend(exported)
        if relative.parts[:3]==('media','rom','sprite'):
            sprites.extend(export_sprite_rectangles(folder,relative))
        archives.append({'source':relative.as_posix(),'sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
            'images':len(exported)})
    from extract_model2_bitmaps import export as export_banks
    from extract_model2_textures import extract as export_textures
    from extract_arena_bitmaps import extract as export_arenas
    bitmap_banks=export_banks(ROOT,banks,out/'media/rom/vf2_rom/rom_data.bin')
    texture_records=export_textures((banks/'rom_data.bin.raw').read_bytes(),out)
    arena_records=export_arenas((banks/'ic12_15.bin.raw').read_bytes(),(banks/'rom_data.bin.raw').read_bytes(),out/'arenas')
    manifest={'schema':1,'original_game_root':'data/game','export_root':str(out.relative_to(ROOT)),
        'archives':archives,'images':images,'sprites':sprites,
        'rom_banks':{'directory':str(banks),'origin':'Previously decoded original ROM banks; no game execution',
            'bitmap_sets':bitmap_banks['sets'],'bitmap_sheets':len(bitmap_banks['images']),
            'arena_panoramas':len(arena_records),
            'texture_blocks':len(texture_records['images']),
            'texture_blocks_exported':sum(r['status']=='exported' for r in texture_records['images'])},
        'note':'DDS preserved without PNG duplicates; sprite crops and ROM bitmaps exported as PNG. No upscaling. Runtime replacements are not loaded yet.'}
    (out/'archive_graphics_manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print(json.dumps({'archives':len(archives),'loose_or_atlas_pngs':len(images),'individual_sprites':len(sprites),
        **manifest['rom_banks']}))

if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--output',type=Path,default=ROOT/'data/textures/original')
    ap.add_argument('--banks',type=Path,default=ROOT/'analysis/bitmap-captures')
    args=ap.parse_args();main(args.output.resolve(),args.banks.resolve())
