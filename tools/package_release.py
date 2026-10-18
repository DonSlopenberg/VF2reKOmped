"""Create the Windows release packages from the Release build.

Usage: python tools/package_release.py   (after native/build_release.cmd)

dist/VF2-reKOmped-<v>/                     private reference installation
                                           (includes the original game files)
dist/VF2-reKOmped-<v>-cleanroom.zip        distributable package: modified
                                           binaries, upscaled backgrounds and
                                           the installer; no original files.
                                           The user runs INSTALL.cmd with
                                           their own Virtua Fighter 2.rar or
                                           STFS content file.
"""
from pathlib import Path
import hashlib, json, re, shutil, zipfile

root=Path(__file__).resolve().parents[1]
build=root/'native/out/build/win-amd64-release'
version=re.search(r'set\(VF2_GAME_VERSION\s+"?([\d.]+)',(root/'native/CMakeLists.txt').read_text()).group(1)
dist=root/'dist'
stage=dist/f'VF2-reKOmped-{version}'
clean=dist/f'VF2-reKOmped-{version}-cleanroom'
for d in (stage,clean):
    if d.exists():shutil.rmtree(d)
stage.mkdir(parents=True)
sha=lambda b:hashlib.sha256(b).hexdigest()

def copy(source,relative):
    target=stage/relative
    target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(source,target)

# ---- reference installation ------------------------------------------------
copy(build/'vf2_launcher.exe','Virtua Fighter 2.exe')
copy(build/'vf2.exe','data/engine/vf2.exe')
for dll in sorted(build.glob('*.dll')):copy(dll,'data/engine/'+dll.name)
for p in sorted((root/'data/game').rglob('*')):
    if p.is_file():copy(p,p.relative_to(root))
for p in sorted((root/'data/textures/replacements/arenas').glob('*')):
    if p.suffix.lower() in ('.png','.md'):copy(p,p.relative_to(root))
for name in ['data/userdata','data/logs']:
    (stage/name).mkdir(parents=True,exist_ok=True)
(stage/'data/engine/vf2.toml').write_text('''# Virtua Fighter 2 reKOmped settings. Paths are supplied by the launcher.
input_backend = "xinput"
license_mask = 1
user_language = 1
bind_settings = "F6"
fullscreen = true
vf2_output_resolution = "1920x1080"
vf2_no_intro = true
vf2_background_replacements = false
vf2_arcade_tourist = false
''',encoding='utf-8')
(stage/'README.txt').write_text(f'''VIRTUA FIGHTER 2 REKOMPED - {version}
2012 Xbox360 Live Arcade Edition for Windows

Start Virtua Fighter 2.exe. The launcher selects the monitor, resolution
(16:9 up to UHD, or 21:9 ultrawide 2560x1080 / 3440x1440), fullscreen,
Skip Startup Logos, Upscaled 2D Backgrounds and the Arcade options Tourist,
Random Opponents and Random Arenas. Settings are shared with
data/engine/vf2.toml. Saves are created in data/userdata. The game can also be
started directly with data/engine/vf2.exe (same settings and saves).

Xbox controllers use Windows XInput. F4 toggles fullscreen; F6 opens Settings
(Display, Arcade & Cheats) and the Custom Fight Launcher; F7 shows
achievements. Enter/Esc work in the menus. At character select, Up/Down
switches to the alternate costume.

Keyboard Player 1: WASD movement, V guard, B punch, C kick, Z Start.
Keyboard Player 2: arrow keys, Delete guard, End punch, Page Down kick, Enter Start.

data/game holds the original game files from your own Xbox 360 XBLA package.
Upscaled arena backgrounds are read from data/textures/replacements/arenas.

Created by Sol, Opus and Don Slopenberg. Virtua Fighter 2 and its original
logo are trademarks of SEGA; this project is not affiliated with SEGA.
Runtime: ReXGlue (Xenia-derived), licenses in the licenses folder.
''',encoding='utf-8')
licenses=root/'tools/rexglue-sdk-0.10.0/win-amd64/licenses'
if licenses.exists():shutil.copytree(licenses,stage/'licenses')

manifest=[]
for p in sorted(stage.rglob('*')):
    if p.is_file():
        manifest.append({'path':p.relative_to(stage).as_posix(),'bytes':p.stat().st_size,'sha256':sha(p.read_bytes())})
(stage/'MANIFEST.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')

# ---- original file list (hashes only) --------------------------------------
inventory=json.loads((root/'analysis/package_inventory.json').read_text(encoding='utf-8'))
originals={e['path']:e for e in inventory['entries'] if not e['directory']}
for path,e in originals.items():
    local=root/'data/game'/path
    assert local.stat().st_size==e['size'] and sha(local.read_bytes())==e['sha256'],path
game_files={'title':inventory['title'],'title_id':inventory['title_id'],
    'content_name':'65D8FAC10B21FA185AAB93607663594CB568C78658',
    'source_sha256':inventory['source_sha256'],'source_size':inventory['source_size'],
    'files':[{'path':p,'size':e['size'],'sha256':e['sha256']} for p,e in sorted(originals.items())]}
game_paths={'data/game/'+p for p in originals}
assert game_paths=={m['path'] for m in manifest if m['path'].startswith('data/game/')}

# ---- clean-room package ----------------------------------------------------
for m in manifest:
    if m['path'] in game_paths:continue
    target=clean/'payload'/m['path']
    target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copy2(stage/m['path'],target)
shutil.copy2(stage/'MANIFEST.json',clean/'payload/MANIFEST.json')
(clean/'installer').mkdir(parents=True)
shutil.copy2(root/'installer/install.ps1',clean/'installer/install.ps1')
(clean/'installer/game_files.json').write_text(json.dumps(game_files,indent=2),encoding='utf-8')
shutil.copy2(root/'installer/INSTALL.cmd',clean/'INSTALL.cmd')
(clean/'README.txt').write_text(f'''VIRTUA FIGHTER 2 REKOMPED - {version} - INSTALLER

This package contains the modified Windows executables, runtime DLLs,
configuration, upscaled arena backgrounds and the installer. It contains no
original Virtua Fighter 2 game files: you supply your own copy of the
Xbox 360 XBLA package, which is processed locally on your PC.

Install
  1. Extract this ZIP into a writable folder.
  2. Put "Virtua Fighter 2.rar" or the file
     65D8FAC10B21FA185AAB93607663594CB568C78658 next to INSTALL.cmd
     (or drop either file onto INSTALL.cmd), then run INSTALL.cmd.
     Command line:  INSTALL.cmd [source] [destination]
  3. Start "Virtua Fighter 2\\Virtua Fighter 2.exe".

The installer reads the RAR with Windows' built-in tar.exe (or 7-Zip),
checks the package's SHA-1 hash tree and the SHA-256 of every original file,
and stops with a clear message for a missing, damaged or different package.
Running it again over an installation keeps your saves (data/userdata) and
settings (data/engine/vf2.toml).

Requirements: Windows 10 1803 or newer (x64), a Direct3D 12 GPU,
about 400 MB of free space.

Expected package: SHA-256 {inventory['source_sha256']}
({inventory['source_size']} bytes, title ID {inventory['title_id']}).
''',encoding='utf-8')

# ---- clean-room verification -----------------------------------------------
original_bytes={p:(root/'data/game'/p).read_bytes() for p in originals}
source_names={'65D8FAC10B21FA185AAB93607663594CB568C78658','Virtua Fighter 2.rar'}
forbidden_hashes={e['sha256'] for e in originals.values()}|{inventory['source_sha256']}
def samples(data):
    # high-entropy 64-byte probes spread over the file
    out=[]
    for i in range(48):
        at=(len(data)-64)*i//47 if len(data)>64 else 0
        piece=data[at:at+64]
        if len(set(piece))>=40:out.append(piece)
    return out
probes={p:samples(b) for p,b in original_bytes.items()}
report={'files':0,'bytes':0,'probes':sum(len(v) for v in probes.values()),'findings':[]}
for p in sorted(clean.rglob('*')):
    if not p.is_file():continue
    rel=p.relative_to(clean).as_posix();data=p.read_bytes()
    report['files']+=1;report['bytes']+=len(data)
    if sha(data) in forbidden_hashes:report['findings'].append(f'{rel}: identical to an original file')
    if p.name in source_names or data[:4] in (b'LIVE',b'CON ',b'PIRS',b'XEX2'):
        report['findings'].append(f'{rel}: original package/executable format')
    if p.suffix.lower() in ('.xex','.farc','.acb','.awb','.acf','.adx','.bin'):
        report['findings'].append(f'{rel}: original asset extension')
    for src,pieces in probes.items():
        hits=sum(1 for piece in pieces if piece in data)
        if hits:report['findings'].append(f'{rel}: contains {hits} byte sequences of data/game/{src}')
assert not report['findings'],report['findings']

archive=dist/(clean.name+'.zip')
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for p in sorted(clean.rglob('*')):
        z.write(p,p.relative_to(clean).as_posix())
with zipfile.ZipFile(archive) as z:
    assert z.testzip() is None
    names=z.namelist()
    assert all(not n.endswith(('.pdb','.cpp','.obj','.log','.ilk')) for n in names)
    assert not any('data/game/' in n for n in names)
digest=sha(archive.read_bytes())
(dist/(archive.name+'.sha256')).write_text(digest+'  '+archive.name+'\n',encoding='ascii')
(dist/'CLEANROOM_VERIFICATION.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
old=dist/f'VF2-reKOmped-{version}.zip'
for f in (old,dist/(old.name+'.sha256')):
    if f.exists():f.unlink()
print(json.dumps({'reference':str(stage),'reference_files':len(manifest)+1,
 'cleanroom_zip':str(archive),'bytes':archive.stat().st_size,'files':len(names),'sha256':digest,
 'verification':{k:v for k,v in report.items() if k!='findings'}},indent=2))
