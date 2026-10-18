# Development journal

A chronological account of how Virtua Fighter 2 reKOmped was built, from the
first look at the Xbox 360 package to the current release. Each entry says
what the problem was, how it was approached and what came out of it, and
links to the detailed note in this folder. Version numbers refer to
[CHANGELOG.md](../CHANGELOG.md).

Background: the 2012 Xbox Live Arcade release is itself an emulator. A
PowerPC program (`default.xex`) runs the original Sega Model 2 arcade game
(an Intel i960 CPU program plus graphics data in `media/rom/vf2_rom`) and
draws it through Direct3D 9 on the Xbox 360 GPU. reKOmped translates the
PowerPC program into native x64 code with the ReXGlue SDK, and changes the
behaviour mostly by hooking PowerPC functions and individual i960
instructions.

---

## 14 October 2026 — from a package to a running game

### Feasibility
[FEASIBILITY_20261014.md](FEASIBILITY_20261014.md)

The STFS package (title 5841129C, 66.9 MB) was unpacked with our own
scripts, every SHA-1 block hash checked, and the XEX headers decoded:
encrypted, LZX-compressed, 166 `xam` and 332 `xboxkrnl` import references.
The `vf2_rom` folder revealed the arcade-emulator structure. ReXGlue was
picked as the most promising route because it bundles code generation and
an Xbox runtime.

### Stage 2 — C++ generation
[STAGE2_CODEGEN_20261014.md](STAGE2_CODEGEN_20261014.md)

ReXGlue 0.10.0 decrypted the XEX and translated it. Six unresolved jumps
were fixed by declaring their entry points instead of forcing the build:
13,198 functions, no `REX_FATAL`. XInput was made the default input.

### Stage 3 — the first Windows application
[STAGE3_FIRST_APP_20261014.md](STAGE3_FIRST_APP_20261014.md)

Build Tools and Clang were installed and the first `vf2.exe` and launcher
were built (English UI, 720p–UHD, fullscreen, controller count). Indirect
call targets found at run time and 46 pointer-table entries brought the
count to 13,249 functions. The title screen, two-pad Versus, saves and an
achievement unlock worked; `license_mask=1` avoided the store stub. The
Achievements menu hung on an unimplemented system UI call.

### 16:9
[WIDESCREEN_DIAGNOSTICS_20261014.md](WIDESCREEN_DIAGNOSTICS_20261014.md),
[WIDESCREEN_STAGE_20261014.md](WIDESCREEN_STAGE_20261014.md)

The fight was rendered in a 1395×1080 inset: the Model 2 screen is
496×384. Widening only the viewport stretched the models; scaling the
first row of the perspective matrix (`sub_821538A8`) by 0.7265625 gave
correct 16:9 proportions for 3D and HUD. True UHD was reached by drawing a
1920×1080 guest frame at 2× GPU draw scale; forcing a 4K console buffer
corrupted the image. Repeating the background bitmap duplicated HUD
elements too and was rolled back.

### Arena layers, rollback, smooth scrolling
[ARENA_LAYERS_20261014.md](ARENA_LAYERS_20261014.md),
[CRASH_ROLLBACK_20261014.md](CRASH_ROLLBACK_20261014.md),
[SMOOTH_SCROLL_ONLY_20261014.md](SMOOTH_SCROLL_ONLY_20261014.md)

Both hardware layers turned out to feed both final bitmaps, so the
background could not be isolated by layer or caller. Separate render
targets did not help either. An experimental renderer shipped by mistake
caused crashes and was rolled back to the last stable engine. Smooth
background scrolling then came back on its own: a filter on the layer-1
scroll coordinates only, with the correct 512/1024 wrap periods.

## 15 October 2026 — panoramic backgrounds (1.0.0)

### Startup
[STARTUP_FIXES_20261015.md](STARTUP_FIXES_20261015.md),
[STARTUP_LOGO_NOTES.md](STARTUP_LOGO_NOTES.md)

Fullscreen was requested before the window existed; it is now entered once
the presenter is attached. The logo-skip code had never been compiled (it
was missing from CMake). The DATA INITIALIZE task was decoded, and only its
final animation wait is shortened, so resource loading and cleanup still
run. "No Intro" was added.

### Panoramic arena renderer
[PANORAMA_RENDERING_20261015.md](PANORAMA_RENDERING_20261015.md),
[ARENA_ASSET_EXTRACTION_20261015.md](ARENA_ASSET_EXTRACTION_20261015.md),
[GRAPHICS_EXPORT_20261015.md](GRAPHICS_EXPORT_20261015.md)

The real fix for 16:9 backgrounds: the arena records in the ROM point to
complete panoramas (11 maps, usually eight 256-pixel strips). The renderer
identifies the visible tiles in the full map and fills the game's own two
background banks with a wider window, wrapping horizontally, while all
original shaders, palettes and layers stay in use. The panoramas and other
graphics were also exported offline (tile banks, palettes, sprites, DDS),
verified pixel for pixel against runtime samples.

### Demo and gameplay fixes, timing
[DEMO_PANORAMA_FIXES_20261015.md](DEMO_PANORAMA_FIXES_20261015.md),
[TIMING_PROJECTION_20261015.md](TIMING_PROJECTION_20261015.md)

Camera tilts in the demo made the matcher fall back to the old renderer
(303 of 5,991 frames); the retained mapping is now validated instead. Sky
colours are sampled per arena and drawn through the original palette
shader. A Background FOV slider and a Horizon Height slider were added to
F6. The background pose is taken once per 60 Hz simulation update, and a
timing audit confirmed the guest stays at 60 Hz on a 144 Hz monitor
(59.99–60.00 updates/s); no game clock was changed. Settings now save as
valid TOML.

## 16 October 2026 — Custom Fight, launcher and precision (1.1.0)

### Custom Fight Launcher
[CUSTOM_FIGHT_CHECKPOINT_20261016.md](CUSTOM_FIGHT_CHECKPOINT_20261016.md),
[CUSTOM_FIGHT_RELEASE_20261016.md](CUSTOM_FIGHT_RELEASE_20261016.md),
[FIGHT_FLOW_FIXES_20261016.md](FIGHT_FLOW_FIXES_20261016.md)

An F6 window starts any fight: both characters (Dural included), Human or
CPU per side, all 16 native arena records. It enters the original i960 mode
dispatcher through a small CALL/RET bridge, so the game's own selection,
stage, HUD and camera initialisation run. Follow-ups: the native gameplay
scene owns pause and cleanup, Start opens the pause menu, a full match
returns to the Main Menu, and the game starts directly at the Main Menu
after skipping only the save-icon explanation.

### Display and options
[DISPLAY_CUSTOM_OPTIONS_20261016.md](DISPLAY_CUSTOM_OPTIONS_20261016.md)

The final viewport now covers the whole output in normal play too. A single
authoritative output resolution replaced competing settings. Custom Fight
gained native difficulty, rounds to win and round time, saved separately
from the Arcade settings.

### Launcher style, monitors, colours
[LAUNCHER_STYLE_20261016.md](LAUNCHER_STYLE_20261016.md),
[STARTUP_MONITOR_COLORS_20261016.md](STARTUP_MONITOR_COLORS_20261016.md)

Native Win32 launcher with light/dark theme, accent colour, Per-Monitor V2
DPI, logo and icon; monitor selection by model name. The SEGA/CRIWARE logos
were drawn by a global animation registry as well, so only that draw is
suppressed now. A report of wrong character colours could not be
reproduced at the time (it came back in 1.3.2, see below).

### Vertical background precision
[VERTICAL_SUBPIXEL_FIX_20261016.md](VERTICAL_SUBPIXEL_FIX_20261016.md),
[BACKGROUND_VERTICAL_SYNC_20261016.md](BACKGROUND_VERTICAL_SYNC_20261016.md)

The i960 code drops four bits of the camera pitch, so the background moved
in whole source pixels (about 5.6 UHD pixels). The fraction is now
recovered, but the first version paired a fresh integer with a stale
fraction. A runtime trace showed the port follows the current pitch, and
selecting the sample that reproduces the native port cut spikes from 110 to
0 in a 40 s demo. Colour decoding before filtering made the fraction
visible.

### Demo Lab
[DEMO_LAB_CHECKPOINT_20261016.md](DEMO_LAB_CHECKPOINT_20261016.md),
[DEMO_LAB_SECOND_STAGE_20261016.md](DEMO_LAB_SECOND_STAGE_20261016.md)

A diagnostic tool that runs the attract demo repeatably. It showed that the
background atlas holds packed palette indices, so filtering must happen
after palette decoding. That was proven by a red/green capture test. Full
scene interpolation to 120/144 Hz was ruled out for now: the geometry has
no stable identities across frames.

### More background work
[BACKGROUND_VERTICAL_SYNC_20261016.md](BACKGROUND_VERTICAL_SYNC_20261016.md)

- The interlaced water wave on line-scroll arenas was smoothed.
- F6 now keeps the scene visible while it freezes the game.
- Pause-menu Achievements work.
- Keyboard menus got their own routing.
- The background is filtered at output resolution, so the HUD stays sharp.
- Static bitmap screens are smoothed.
- A bank guard covers animated arenas.
- Line-scroll arenas (Jacky, Akira, Dural) no longer apply the camera
  scroll twice.
- An optional background blur was added.
- Startup goes through the native Push Start flow (storage device and save
  load).

## 17 October 2026 — Arcade extras and distribution (1.2.x)

### Arcade extras (1.2.0)
[ARCADE_EXTRAS_20261016.md](ARCADE_EXTRAS_20261016.md)

The extras rely on facts decoded from the ROM: the opponent and arena
table, the Ranking Mode flag, fighter health and the CPU level formula.

- **Random Opponents and Random Arenas:** a permutation is written into the
  in-memory ROM table and locked for the whole run.
- **Tourist:** constant easiest CPU, using Lau's AI tables. The CPU level is
  a handicap, so "most merciful" means +16, not −32.
- **Infinite Health:** cheats for either player.
- **Hidden Dural:** hold Start at character select.
- **Arcade end:** a finished Arcade run returns to the Main Menu.
- **Fixes:** the achievement gate kept the save indicator spinning; the
  "No storage device" message came from skipping Push Start; the Expert
  Mode arrows were missing glyphs.
- **Upscaled panorama replacements** became optional.

### Clean-room distribution and 21:9 (1.2.0–1.2.2)
[CLEANROOM_RELEASE_20261017.md](CLEANROOM_RELEASE_20261017.md)

The release contains no game files. `INSTALL.cmd` takes the user's RAR or
STFS file, checks every block hash and the exact revision, extracts and
verifies the 37 files, and installs atomically (a reinstall keeps saves and
settings). The packager refuses to build if any shipped file matches
original data: hashes, formats or 1,692 entropy samples. 15 installer tests
cover the error paths. Next came three fixes:

- **1.2.1:** a 1080-line guest frame fixed cropped HD/QHD output.
- **1.2.1:** the "create Save Data" question is answered automatically.
- **1.2.2:** native 21:9. The guest is 1920×810/804 at 2×; menus and HUD
  stay centred, and menu margins are filled by redrawing the sprite lists
  per margin with their own scissor.

## 18 October 2026 — reKOmped (1.3.x)

### 1.3.0 and 1.3.1
[CHANGELOG.md](../CHANGELOG.md)

The project was renamed reKOmped, with a new logo and icon. The launcher
gained an Arcade section (Tourist, Random Opponents, Random Arenas).

- **Alternate costumes:** Up/Down at character select, shown by a frame in
  negative colours. The select screen always previews the normal costume
  and applies the choice when selection ends, because previewing the
  alternate broke the preview heads and palettes.
- **Custom Fight:** an alternate-costume option, Tourist difficulty, full
  matches, and leaving a running game cleanly first.
- **Esc:** pauses any match for the side that's playing.
- **Dural arenas:** the original wave table produced seams and jumps once
  widened, so it was replaced by a synthetic seamless sine wave (amplitude
  3.5 px, 112 lines, 128-tick loop).
- **FXAA:** in the launcher and in F6.
- **F6:** scales with small windows, opens top centre and stays reachable.
- **Launcher:** GitHub update notice and per-monitor DPI resizing.
- **Clean-room package:** renamed `VF2reKOmped-<version>-windows-x64.zip`;
  `vf2.exe` can also run without the launcher.

### 1.3.2 — the colour glitch
[CHANGELOG.md](../CHANGELOG.md#how-the-colour-glitch-was-tracked-down)

The intermittent wrong colours (a green beard, black fighters, missing
textures) were reproduced in about one fight start in four. Dumps showed
the colour data in memory was correct. Then logging every texture upload
made the bug vanish, which pointed to a race between the CPU writing
textures and the emulated GPU caching them. The fix invalidated GPU memory
after each burst of texture uploads (20/20 correct vs 7/30 bad).

### 1.3.3 — the flicker regression and the real fix
The 1.3.2 fix invalidated all physical memory, including render targets.
Reloading textures that are in use costs a frame without them, so the HUD
and the SET/READY/WINNER messages flickered. Several narrower variants were
measured:

| Variant | Colour test | Flicker |
| --- | --- | --- |
| All memory after each burst (1.3.2) | 20/20 correct | HUD flicker |
| Each texture right after its upload | 6/20 bad | none |
| Each texture's range, delayed after the burst | 1/20 bad | none |
| All load-time memory below 0xBE000000, after every burst | 20/20 correct | one dark frame at fight start |
| Load-time memory after real loads only, texture ranges in matches | 20/20 correct | none |

The texture addresses come from the game's device table: the wrapper index
points to a 1,264-byte entry, with the mip count at +0x1C and mip addresses
from +0x84. A probe of the GPU's watched pages separated load-time data
(from 0xB0000000) from per-frame data (0xBE4… and above). Immediate
invalidation failed because the upload call only queues the copy; the game
writes the memory later. The launcher footer now also reads the version and
build date from the engine executable.
