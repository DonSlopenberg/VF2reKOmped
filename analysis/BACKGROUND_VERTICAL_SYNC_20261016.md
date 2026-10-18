# Vertical panorama sync fix — 2026-10-16 (Claude)

## Symptom

Arena background "rides over uneven ground" vertically, most visible in the
Arcade attract demo with its many scripted camera tilts.

## Root cause (measured)

Two independent defects:

1. **Stale fraction paired with a fresh integer.** `CaptureComputedNativeScroll`
   combined the native integer vertical port (g4 at i960 PC 0x24664) with the
   four discarded bits of the *entry-time* `0x503010` pitch sample. A trace of
   every invocation (`analysis/claude-pitch-trace.csv`, env `VF2_PITCH_TRACE`)
   shows the integer port changes in exactly the update where the **current**
   pitch at `0x50300C` (read at 0x24664) crosses a 16-unit boundary; the entry
   sample is one to two updates older. Result: a whole source texel spike
   (~5.6 UHD px at FOV 120) at every boundary, e.g. phase
   125.875 → 125.9375 → **126.9375** → 127 → 126.0625.
   Codex's static reading (native uses the delayed sample) does not match the
   runtime values; the runtime trace is authoritative.
2. **Fraction discarded in normal gameplay.** The atlas holds palette indices,
   so linear filtering of it had no effect (Codex's Demo Lab: zero changed
   pixels). The post-palette color-decode path existed only in Demo Lab.

## Changes

- `vf2_panorama_math.h`: `NativeVerticalPort` (exact replay of 0x245BC..0x2465C
  including the bank byte at `0x50303E`) and `SelectNativePitch`.
- `vf2_panorama.cpp`: at 0x24664 the fraction comes from the first candidate
  that reproduces the native port: current `0x50300C`, entry `0x503010`,
  previous post-advance value. No match → integer only (counted). Coverage log
  reports `pitch_match / pitch_mismatch / entry_missed`.
- Color decode before filtering is now the default outside Demo Lab.
- New cvars, all with F6 → VF2 → Display → *Background diagnostics* toggles:
  `vf2_background_color_decode` (default on), `vf2_background_pitch_fix`
  (default on; off = previous pairing). `vf2_background_smooth_scroll` also
  has a toggle now.
- `native/build.cmd`: `--parallel 16`. The old build directory was generated
  for the original folder path; it is kept as
  `native/out/build/win-amd64-relwithdebinfo.original-folder`.

## Validation

| Check | Result |
| --- | --- |
| `analysis/test_panorama_math.cmd` | PASS, 7,742,128 checks; new tests replay the port for all 65,536 pitches × 4 banks and reproduce the measured spike with a stale sample |
| `tools/verify_vertical_spikes.py`, Arcade attract demo, 40 s | legacy pairing: **110** spikes; fix: **3**, all three a fast pan ending in a camera cut (−216 px), i.e. not spikes |
| Pitch selection, demo | 1532/1532 matched, 0 mismatches |
| Normal custom fight + native pause, arena 3, UHD | 0 spikes, 413/413 matched; background outside the pause menu: 0 changed pixels across 20 captures 17 ms apart |
| Visual | Demo and pause GPU captures inspected: HUD, panorama, clouds, character colors correct |

## Not done / open

- Native-pause background flicker is **not reproduced** in guest GPU output
  (installed 71bf build and this build, custom fight arena 3). Only the native
  menu highlight animates. Needs a reproduction description.
- No temporal interpolation yet: the scene still updates at 60 Hz and the
  presenter repeats frames 3:2 at 144 Hz.

Backup of previous engine and sources: `backups/before-claude-background-20261016/`.
Installed SHA-256: `268897206fe1a2af4d5964447e3a8776f55ba47642b5be3262dc2d89c43e8fce`.

## Follow-up fixes (same day)

- **Launcher**: window title "Virtua Fighter 2 Launcher"; closes after the game
  process starts (mouse and controller Play). Footer shows launcher version
  and the game version read from `data/engine/vf2.exe` VERSIONINFO. Versions
  live in `native/CMakeLists.txt` (`VF2_GAME_VERSION`, `VF2_LAUNCHER_VERSION`,
  both 1.1.0); the shared block is `resources/version_block.rc2` (rc.exe
  ignores non-#define content in `.h` includes).
- **F6 shows the game**: suspending TaskM2E also stopped the whole Model 2
  screen (only the frontend gradient remained). F6 now skips emulated-system
  updates (`sub_821830D8`) instead; the frame keeps rendering, tick frozen.
  Legacy: `--vf2_f6_suspend_m2e=true`. Verified: GPU capture shows the scene.
- **Pause → Achievements**: native system-UI dispatcher `sub_8218CB00` bails
  while the active user `0x824A04E8` is -1. Push Start (skipped at startup)
  normally sets it via `sub_8218B848`; the host now assigns user 0 through
  that setter. Verified: request 2 reaches `XamShowAchievementsUI`, panel stays open.
- **Dural Boss arena (10)**: half-speed round clock is native: ROM 0x1FD44
  boss branch writes `0x50a004`=2.0 (normal 1.0) and low gravity at `0x50a000`.
  Direct custom start (30 captures from SET 1/READY) shows no HUD garbage and
  no black top; the user's report came via Arcade → pause → F6 → Custom Fight,
  not yet reproduced.

## Interlaced water wave (Dural arena and other line-scroll arenas)

- Source art is clean (`data/textures/original/arenas/arena_10_*.png`). The
  wave is the native per-line X scroll: `sub_82144618` calls
  `sub_82188AB0(layer, src, dst, 384)` (`dst[i]=src[(i+vscroll)&511]`, signed
  16-bit pixels) and uploads it for the line-scroll tile shaders (8190…, 2AAE…,
  0847… fetch a per-line table before the tile fetch).
- Measured table (`VF2_LINE_TRACE`): two interleaved opposite-phase waves on
  even/odd lines (−3..+5 px) — a CRT blended them; magnified it is a comb.
- Fix: background layers 1/3 get a [1,2,1]/4 filter across lines before upload
  (`vf2_background_wave_smooth`, default on, F6 toggle). The wave remains,
  the comb is gone (UHD captures `fight-claude-dural-wave` vs `-nowave`).
- Bank seam from the user's screenshot not reproduced over a 30-angle
  `VF2_PANORAMA_SWEEP` rotation at arena 10.
- Background magnification remains point-sampled (pixel blocks ~5.6 px at UHD).

## Open: main-menu navy frame

The logo and its white panel are frontend sprites from `n_vf2.farc` drawn by
the frontend layout/animation system (TaskMenuMain draw `0x82148D00`, layout at
task+508, registry draw `0x820F4C50`); the navy gradient is a full-screen
textured quad from `0x82181450`. Not changed yet.

## Keyboard menus and fullscreen cursor

- Keyboard routing (log: "VF2 keyboard routing"): frontend scenes send the
  whole keyboard (WASD/arrows, Enter = A, Esc = B) to player 1, the active
  user; Enter is also player 2's Start when P2 has no pad (Offline Versus
  join screen). Native pause: the same keys go to both players. Gameplay:
  unchanged per-player fallback (P1 WASD, P2 arrows + Enter Start).
  First version sent arrows to P2 and Enter/Esc to P1, which broke menu
  navigation with a pad on P1 and blocked the Versus join (user report).
- Fullscreen: SDK `Window::SetCursorVisibility(kAutoHidden)`, 1 s delay; the
  cursor reappears on mouse movement (F6 sliders stay usable). Windowed: visible.
- Not verifiable in hidden automated runs (needs a foreground window and real
  keys); build and pause/achievements smoke test passed.

## Background free-look (diagnostic)

F6 → VF2 → Display: "Background free-look" + yaw/pitch sliders; in game
Numpad 4/6 yaw, 8/2 pitch, 5 reset (applies with F6 closed; F6 freezes the
game). Turns only the distant 2D layer and its sky/ground fills, to inspect
every arena's panorama edges. The 3D view is unchanged: camera +0x24/+0x26
alter shading rather than the view, and the orthonormal matrix at
[0x500814]+0xB54 is a fighter-object field (cleared at i960 0x141C8). A real
3D free-look needs the view matrix in the geometrizer command stream.
Test hook: VF2_FREELOOK_TEST="yaw pitch".

## Output-resolution background filtering (2026-10-17)

The panorama was decoded and blitted into the 384-line bitmap canvas, which
the canvas composite (sub_821445B0, POINT 427) magnified: hard ~5.6 px blocks
at UHD on every arena. Forcing that composite to LINEAR also blurred the HUD,
which shares the canvas (rejected). Now the sky swatch, ground swatch and
panorama blits are queued per priority, the canvas is cleared transparent
(clear at lr 0x8214419C with alpha 0), and the queue is drawn straight to the
output with LINEAR filtering in Composite, just before the canvas. HUD stays
point-filtered; layer order verified (panorama under 3D, HUD on top).
Toggle: `vf2_background_linear_composite` (F6). Test hook VF2_TEST_FILTER.
Sky/ground extensions checked on all 11 arenas with free-look ±30° (UHD);
arena 7's magenta top is its authentic sunset top row.
Free-look pitch sign flipped: + / Numpad 8 = look up.

## All bitmap backgrounds smoothed (2026-10-17, later)

- Panorama queue now persists until the next bitmap pass (cleared only by a
  pass without a panorama), so a composite without a fresh pass still draws
  the background instead of a transparent canvas.
- Bitmap screens without a panorama (title, high-score entry, static stage
  screens): canvas-0 composite forced to LINEAR (`vf2_background_static_smooth`).
  Verified on the SEGA title (UHD crop, smooth edges vs original blocks).
- F6: "Smooth background filtering (off = pixelated)" restores the original
  point-filtered look everywhere. Render targets have no mip chain, so
  bilinear is the effective form of trilinear filtering here.
- Arenas 7/9/10 in Arcade (VF2_ARCADE_FIRST test hook) and native pause
  (BACK): background outside the pause panel pixel-identical across 20
  consecutive frames, sky fill present; the user's seams/flicker were not
  reproduced in GPU readbacks (installed and new build).
- Akira difficulty: hard-coded per-character tables — base level −3 (with
  Dural the strongest) and handicap cap +1 (others up to 16). Tourist
  overrides it (logged: "CPU level -3 -> 16").

## Bank guard for animated arenas (2026-10-17)

sub_821831D0 runs sub_821881B0 (native map copy + upload via sub_82144618)
before the bitmap pass. Arenas with animated maps re-upload every frame; the
GPU consumes the previous pass's panorama draws asynchronously, so the native
4:3 map could replace background banks 1/3 (map rows 64..127) still in use:
a vertical seam with mismatched sky (user screenshots: Jacky ~30 %, Dural
~20 % of width). Hook sub_82199CB8: while a panorama is active, the map
texture upload carries the last pass's remapped rows 64..127; the CPU map is
restored right after. Toggle `vf2_background_bank_guard`. Not reproduced in
hidden UHD readbacks (timing-dependent), so unverified on the user's setup.

## Line-scroll arenas fixed at the root (2026-10-17, evening)

Arenas 7/9/10 (Jacky, Akira, Dural) render the distant layer through per-line
scroll tables (sub_82188AB0 → engine+0x10C000, read by the tile shaders).
Measured (`VF2_LINE_LOG`): every entry = (−phase_x) mod 512 + per-line wave,
and the native copy indexes it by screen line with the vertical register.
The widened renderer decodes the panorama at 1:1 (canvas line = map row,
camera X already in phase_x), so the native tables double-applied the camera
scroll (up to ~250 px on turns → left-hand seam) and misplaced rows by the
pitch. Now, for layers 1/3 while the decoded panorama is active:
`dst[y] = src[y + vscroll − phase_y]`, minus the modal entry (keeps only the
wave), entries 382/383 = 0. The sky/ground swatch texels moved to canvas row
382, so line offsets no longer push them off their pixel — that was the
missing sky/ground colour on these arenas. Toggle `vf2_background_line_align`.
Verified on KO/TIME OVER camera turns, arenas 7/9/10, UHD.

## Background blur (F6 slider)

`vf2_background_blur` 0..3: the decoded canvas is drawn LINEAR into a
1024>>n × 512>>n native canvas (viewport set explicitly; blitter uses the UI
projection) and magnified LINEAR by FlushPending. HUD unaffected.

## Startup: Push Start instead of a direct Main Menu jump

The direct jump (scene 0/2 or 2/7 → 2/8) skipped the native Push Start flow
(active user, storage device, save load). Now Start is pressed on the title
(`vf2_startup_press_start`, default on); log "native Push Start flow entered",
followed by the original save-icon task. Fallback to the direct jump after
4 s. The "No storage device has been selected" prompt was not reproduced in
hidden runs, so the fix is unverified for that message.
