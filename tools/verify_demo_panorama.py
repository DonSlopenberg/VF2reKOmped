"""Summarize the recorded renderer regressions without launching the game."""
import csv,json,hashlib
from pathlib import Path
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
def rows(name,file):
    return list(csv.DictReader((ROOT/'analysis'/name/file).open()))
def counts(name):
    data=rows(name,'phase.csv.status.csv')
    scrolling=[r for r in data if r['mode'] in ('1','2')]
    return {'bitmap_frames':len(data),'scrolling_frames':len(scrolling),
            'fallback_frames':sum(r['active']=='0' for r in scrolling)}
def main():
    baseline=counts('panorama-test-demo-baseline-navigation')
    suites=[]
    for name,size,fov in [('demo-swatch-final',(1920,1080),120),
                           ('4x3-fov140',(1440,1080),140),
                           ('uhd-fov40-final',(3840,2160),40)]:
        folder=ROOT/'analysis'/('panorama-test-'+name)
        result=json.loads((folder/'result.json').read_text(encoding='utf-8-sig'))
        assert result['alive_at_end'],name
        statistics=counts('panorama-test-'+name)
        assert statistics['fallback_frames']==0,(name,statistics)
        phase=rows('panorama-test-'+name,'phase.csv')
        assert {float(r['fov']) for r in phase}=={fov}
        assert all(0<=float(r[k])<=1 for r in phase for k in ('sky_r','sky_g','sky_b'))
        images=list((folder/'captures').glob('*.png'))
        assert images,name
        for path in images:
            with Image.open(path) as image: assert image.size==size,(path,image.size)
        suites.append({'suite':name,'output_size':size,'fov':fov,'captures':len(images),
                       **statistics,'alive_at_end':True})
    sky=json.loads((ROOT/'analysis/arena-sky-colors.json').read_text())
    assert len(sky)==11 and all(r['top'] for r in sky)
    # Both live arenas must sample the same modal top-row colors as the ROM
    # export. GPU fill uses those pixels through the original palette shader.
    phase=rows('panorama-test-demo-swatch-final','phase.csv')
    for arena in (0,3):
        expected=sky[arena]['top'][0][0]
        observed=[r for r in phase if int(r['panorama'])==arena]
        assert observed
        for r in observed:
            actual=[round(float(r[k])*255) for k in ('sky_r','sky_g','sky_b')]
            assert actual==expected,(arena,actual,expected)
    ui=(ROOT/'analysis/panorama-test-uhd-fov40-final/runtime.log').read_text(encoding='utf-8')
    assert 'F6 background FOV slider drawn: value=40 range=40..140' in ui
    build=ROOT/'native/out/build/win-amd64-relwithdebinfo/vf2.exe'
    installed=ROOT/'data/engine/vf2.exe'
    digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    report={'baseline':baseline,'suites':suites,'arena_top_rows_checked':len(sky),
            'live_sky_palette_matches':[0,3],'f6_slider_rendered':True,
            'build_sha256':digest(build),'installed_sha256':digest(installed),
            'installed_matches_build':digest(build)==digest(installed),
            'limitations':['Live coverage is the introductory Shun arena and Arcade Lau arena, not every arena/quality-menu combination.',
              '4:3/UHD FOV extreme runs precede the final palette-swatch fill adjustment; final 1080p run includes it.',
              'Automated fallback counts and mathematical continuity checks do not prove every frame is visually artifact-free.']}
    (ROOT/'analysis/demo-panorama-verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report,indent=2))
if __name__=='__main__':main()
