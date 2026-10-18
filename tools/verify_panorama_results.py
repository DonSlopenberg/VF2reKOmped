"""Validate recorded GPU dimensions, full-wrap coverage and image continuity."""
from pathlib import Path
from collections import defaultdict
import csv,hashlib,json
import numpy as np
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    suites=[]
    for name,size in [('4x3-wrap',(1440,1080)),('uhd-wrap',(3840,2160))]:
        folder=ROOT/'analysis'/('panorama-test-'+name)
        result=json.loads((folder/'result.json').read_text(encoding='utf-8-sig'))
        assert result['alive_at_end'],name
        images=list((folder/'captures').glob('*.png'))
        assert len(images)==80,(name,len(images))
        for path in images:
            with Image.open(path) as image:
                assert image.size==size,(path,image.size)
                rgb=np.asarray(image.convert('RGB'))
                # A full-height vertical gap/border must also be black at all
                # these scanlines. Test every output column, not just its edges.
                dark_columns=np.max(rgb[::16],axis=(0,2))<=2
                assert not dark_columns.any(),(path,np.where(dark_columns)[0].tolist())
        groups=defaultdict(list)
        for row in csv.DictReader((folder/'phase.csv').open()):
            groups[row['panorama']].append(float(row['phase_x']))
        phase=groups['0']
        bins=len({int(value//8)%256 for value in phase})
        assert bins==256,(name,bins)
        suites.append({'suite':name,'gpu_size':size,'png_frames_checked':len(images),
            'full_panorama_phase_bins':bins,'phase_bin_count':256,
            'observed_wraps':sum(abs(a-b)>1024 for a,b in zip(phase,phase[1:])),
            'vertical_black_gaps':0,'alive_at_end':True,
            'kind':'Explicit GPU source-angle sweep; gameplay camera not modified'})
    originals=[]
    with (ROOT/'data/textures/asset_inventory.csv').open(encoding='utf-8-sig',newline='') as file:
        for record in csv.DictReader(file):
            path=ROOT/'data/game'/record['path']
            assert sha(path)==record['sha256'].lower(),path
            originals.append(record['path'])
    build=ROOT/'native/out/build/win-amd64-relwithdebinfo/vf2.exe'
    candidate=ROOT/'data/engine/vf2_panorama_candidate.exe'
    assert sha(build)==sha(candidate)
    normal=ROOT/'analysis/panorama-test-final-hud-clip/result.json'
    normal_result=json.loads(normal.read_text(encoding='utf-8-sig')) if normal.exists() else None
    if normal_result:assert normal_result['alive_at_end']
    report={'suites':suites,'normal_camera_run':normal_result,'normal_camera_result_path':str(normal.relative_to(ROOT)),
            'original_game_files_unchanged':len(originals),'build_sha256':sha(build),
            'installed_sha256':sha(ROOT/'data/engine/vf2.exe'),
            'limitations':['Live tests cover the introductory arena and Lau; remaining arena tilemaps verified offline.',
                'Full 360-degree GPU coverage uses an explicit diagnostic source-angle sweep, not a forced gameplay camera rotation.',
                'No exhaustive live enumeration of game quality-menu combinations. Original shaders and filtering paths are retained.']}
    (ROOT/'analysis/panorama-render-verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
