# Clean-room distribution 1.2.0 (2026-10-17)

## What ships

`dist/Virtua-Fighter-2-1.2.0-cleanroom.zip` (30 MB) contains:

| Path | Contents |
| --- | --- |
| `INSTALL.cmd` | Single install command (double-click, drag and drop, or `INSTALL.cmd [source] [destination]`) |
| `installer/install.ps1` | Installer (Windows PowerShell 5.1, no third-party tools) |
| `installer/game_files.json` | Expected source SHA-256, size, title ID; path/size/SHA-256 of the 37 original files (hashes only) |
| `payload/Virtua Fighter 2.exe` | Launcher (Release build) |
| `payload/data/engine/` | `vf2.exe` (Release build), `rexruntime.dll`, `rexgpu-xenos.dll`, default `vf2.toml` |
| `payload/data/textures/replacements/arenas/` | 11 upscaled arena backgrounds + README |
| `payload/licenses/`, `README.txt` | Third-party licences, game readme |
| `payload/MANIFEST.json` | SHA-256 of every file of the reference installation |

## Source analysis

* The user source is either `Virtua Fighter 2.rar` (RAR containing
  `Virtua Fighter 2/5841129C/000D0000/65D8FAC10B21FA185AAB93607663594CB568C78658`)
  or that content file itself: an Xbox 360 `LIVE` STFS package, title ID
  5841129C, 66,928,640 bytes, SHA-256 `2e4700ca…c74099`.
* The 37 files in `data/game` of the working build are byte-identical to the
  STFS contents (`analysis/package_inventory.json`). No original file is
  patched; every modification is in `vf2.exe` and the launcher.
* `vf2.exe` does not embed game data. The guest image is loaded at runtime
  from `data/game/default.xex`. The generated code contains no data arrays.

## Installer

1. It finds the source: the argument, else next to `INSTALL.cmd` or one folder above.
2. RAR: it lists the archive with Windows `tar.exe` (bsdtar/libarchive, Windows 10 1803+) and
   extracts only the content file, falling back to 7-Zip if installed. STFS: it uses the file directly.
3. It checks the STFS structure, title ID, root hash, every hash table and the
   SHA-1 of every data block. The reader is a port of the existing
   `tools/inspect_package.py`, which follows Xenia's `stfs_*` code.
4. It requires the source SHA-256 to match the tested revision, then extracts
   the 37 files and checks each one's size and SHA-256.
5. It copies the payload into a staging folder next to the destination and
   verifies every file against `MANIFEST.json` (missing, different or unexpected files).
6. It moves the staging folder to the destination. On reinstall it keeps
   `data/userdata` and `data/engine/vf2.toml` and replaces everything else.
   A failure never leaves a partial installation behind.

## Verification

### File-level comparison with the reference release

`tools/test_installer.ps1` extracts the ZIP into empty folders and installs
from each source format. Every entry of each installation (75 files and
directories) was compared by SHA-256 with `dist/Virtua-Fighter-2-1.2.0`:
RAR and STFS both came out **identical**, with no missing, extra or different entries.

### Error handling

All 15 tests pass. Results are in `cleanroom-release/installer_test_results.json`.

| Case | Result |
| --- | --- |
| No source found | Usage message, exit 1 |
| Source path missing | "Source file not found" |
| Not a game file | "Unrecognised source file" |
| Truncated package | "The package is truncated." |
| One flipped byte | "data block 9695 fails its SHA-1 check" |
| Other title ID | "Wrong game: package title ID 5841129D" |
| CON/PIRS package | Named as such |
| Damaged RAR | tar and 7-Zip errors are reported |
| Non-empty foreign destination | Refused; nothing touched |
| Payload missing a DLL | "missing: data/engine/rexgpu-xenos.dll" |
| Reinstall | Saves and settings kept, damaged `vf2.exe` repaired, no leftovers |
| Auto-detect | STFS file next to `INSTALL.cmd`, no arguments |

Every failure case leaves no destination folder and no staging folder.

### Functional comparison

`tools/test_installed_game.ps1` runs the clean-room install and the reference
with identical arguments: hidden window, upscaled backgrounds, CPU-vs-CPU custom fight.

* The runtime event sequences are identical: 55 unique VF2 events,
  `cleanroom-release/functional_events.txt`. They include gameplay routing,
  `arena_02` replacement loaded at 4096x576, and panorama active.
* Runtime warnings are the same set in both runs: `XFileXctdCompressionInformation unimplemented`
  and the duplicate `bind_*` cvar registration, both also present in development builds.
* The GPU frame captures were byte-identical in all 5 samples. In that run the
  captures happened to show the logo screen. An earlier run showed the
  upscaled arena rendered by the clean-room install.

### Clean-room scan

`tools/package_release.py` refuses to build the ZIP if any shipped file:

* has the SHA-256 of an original file or of the source package;
* is in an original container or executable format (`LIVE`, `CON`, `PIRS`, `XEX2`)
  or carries an original asset extension;
* contains any of 1,692 high-entropy 64-byte samples taken across all 37 original files.

Result: 24 files, 54 MB scanned, **0 findings** (`cleanroom-release/CLEANROOM_VERIFICATION.json`).

## Caveats

* The clean-room package contains no original files, but two shipped parts are
  derived from the game:
  * `vf2.exe` is a machine translation of the game's PowerPC code into x64.
    This is the same model as other static-recompilation releases that require
    the user's own game data.
  * The upscaled backgrounds are upscales of SEGA artwork.
* Only the tested package revision (SHA-256 above) is accepted. Another
  revision is rejected with a clear message rather than installed untested.
* Gameplay was not tested by hand on the clean-room install. Equivalence rests
  on byte-identical files plus the automated runs above.

## Reproduce

```
cmd /c native\build_release.cmd
python tools\package_release.py
powershell -ExecutionPolicy Bypass -File tools\test_installer.ps1 -Work <empty folder>
powershell -ExecutionPolicy Bypass -File tools\test_installed_game.ps1 -Install <installation> -Out <folder>
```

## 1.2.1 (2026-10-17)

### Output resolution fix

* **Symptom:** QHD, and also HD, showed only the top-left two thirds of the frame, enlarged 1.5x.
* **Cause:** the game composes on a 1080-line canvas. With a 1280x720 guest
  backbuffer (HD, and QHD rendered as 1280x720 at 2x), that canvas overflows the surface and is cropped.
* **Fix:** every output uses a 1080-line guest (`vf2_display.h`).
  * HD and Full HD render 1920x1080 at 1x.
  * QHD and UHD render 1920x1080 at 2x, so QHD is downsampled from 3840x2160.

### Save Data prompt

* A fresh profile asks "No Save Data found. Do you want to create new Save Data?".
* With `vf2_auto_create_save` (on by default), the hook on `sub_821A8A60`, the
  XamShowMessageBoxUI wrapper, answers "Yes":
  * it writes the button index;
  * it completes the event-less XOVERLAPPED;
  * it returns ERROR_IO_PENDING, as the real dialog does.
* The hook on `sub_8218BEA0` then clears the "system UI open" flag 0x82AA6EF0,
  because no XN_SYS_UI close notification arrives.
* **Verified:**
  * `SYSTEM.DAT` is created and the Main Menu is reached.
  * A second start goes straight to the menu.
  * Other message boxes are unchanged.

### Ultrawide (tested, not supported natively)

* **1080 lines, wider than 1920 px:** 2560x1080 and 2400x1080 overflow the 10 MB
  EDRAM, because the game keeps both colour and depth there. The result is corrupted, wrapped stripes.
* **Fewer lines** (`VF2_TEST_GUEST_LINES=810` with `VF2_TEST_PHYSICAL_CANVAS=1`,
  giving 1920x810): 3D, the fight HUD and the widened backgrounds all adapt correctly.
  The frontend menus, however, keep a fixed 1920x1080 pixel layout and get cropped at the bottom.
* **1.2.1 behaviour:** the guest is capped at 1920x1080, so ultrawide outputs get a
  clean 16:9 frame, which the presenter (`present_letterbox`) should show with side bars.
  Not verified on a real 21:9 window.
* **For real 21:9:** scale the frontend's 2D layer by canvas height/1080 and centre it
  horizontally, then use a guest such as 1920x810 at 2x.

## 1.2.2: native 21:9 ultrawide (2026-10-17)

### Render path findings

* **Display block** `0x824A0498`, filled by `sub_821067A0` from resolution mode 13:
  * `+0..31` = "screen", the space the sprite/AET layer is projected onto;
  * `+32..63` = "logical" canvas.
* **Resolution table** at `0x82002430`: 16 entries of {w, h, wide}; mode 13 = 1920x1080, mode 16 = "display".
* **Sprite/AET projection** (`sub_82108960`, `sub_82108A68`): a frustum of the screen size plus
  a lookAt (`sub_82103A38`) at the screen centre. The menu layout is in mode-13 pixels.
* **Render pass setup** `sub_82157398(screen, canvas)`: it sizes the canvas target (viewport
  `0x8215734C`) and the backbuffer rectangle (`0x82157864`).
  A canvas wider than 1920 at 1080 lines overflows the EDRAM.
* **Cause of the HD/QHD bug fixed in 1.2.1:** the screen fields were overwritten with the physical
  backbuffer size, so the 1080-line layout was projected 1:1 onto a 720-line buffer.

### Implementation (`vf2_display.h`, `vf2_video.cpp`, `vf2_panorama.cpp`)

1. **Guest frame:** 16:9 stays at 1920x1080. Wider outputs render 1920 x 1920/aspect at 2x:
   2560x1080 renders 1920x810, 3440x1440 renders 1920x804, both drawn at 3840 px wide and downsampled.
2. **Screen and logical:** both are a virtual 1080-line screen with the output aspect (2560x1080 at 21:9).
   `sub_82157398` receives the physical guest size, so its targets fit the EDRAM.
3. **Centring:** the sprite camera's lookAt targets the layout centre x=960, so menus and HUD
   stay centred and the extra width becomes margins (`vf2::display::UiLeft`).
4. **Background compositing:** the panorama composite, `FlushPending` and the blur blit cover
   the visible UI rectangle (UiLeft .. UiLeft+width).
5. **3D viewport:** a viewport at `0x82151B10` of the virtual screen size is mapped to the physical size.
6. **Presets:** 2560x1080 and 3440x1440 are in the launcher and in F6 > Display.

At 16:9 every value is identical to 1.2.1.

### Ultrawide menu margins

The frontend layouts are 1920 wide and would leave dark bars at 21:9.

* **Sprite drawing is deferred.** The AET draw (`sub_820F4C50`) does not draw anything itself;
  sprites are drawn from 32 priority lists per set by `sub_82181D78(set)`. The lists are at
  `0x82E7C9B0 + set*512 + i*16` = {begin, end}, and `entity->vtable[1]` draws each one.
* **Viewports do not clip on Xenos.** The window scissor comes from `SetScissorRect`
  (`sub_821B8BD0`), using the device at `*0x824CBD3C`: enable flag at +12288, RECT at +13052.
* **Fix:** after each list flush, the lists are drawn once more per margin:
  * the sprite camera is recomputed with `sub_82108B80(cam, cam+648)`, with the frustum
    (`sub_820FBD08`) and lookAt narrowed to the layout's outermost 2 columns;
  * the scissor is limited to that margin.
* **Result:** the margins continue the layout's edge (menu gradient, bottom bar, the pause
  bottom bar); everything else is clipped. The centre stays pixel-identical.
