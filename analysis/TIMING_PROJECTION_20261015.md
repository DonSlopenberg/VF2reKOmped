# Panorama projection, camera synchronization and horizon controls

## Delivered changes

The renderer no longer applies tangent/cylindrical projection to the game's already projected 2D panorama. It uses a uniform affine scale, horizontal source wrapping and the original absolute camera scroll. FOV scales both axes equally; scenery is not stretched differently across the screen or between axes. The entire short vertical panorama is cached, so zoomed-out views and camera tilts can expose rows beyond the previous vertical window without losing them.

Background pose is captured once per original simulation update, using the original counter at `0x82E7E38C`. Repeated render passes reuse the same immutable yaw/pitch sample. The separate wall-time 12 ms smoothing filter was removed from both active panorama and fallback paths: it lagged behind the original camera and could move the background while the scene repeated a pose. The existing smooth-scroll option now selects subpixel positioning rather than a separately animated low-pass filter. No yaw accumulator, extrapolation or independent background clock is used.

F6 contains two independent controls:

- **Background FOV:** 40–140 degrees of panorama coverage, default 120. Uniform zoom around the view center, independent of arena geometry/camera FOV.
- **Horizon Height / Vertical Offset:** −192 to +192 logical pixels, default 0. Positive values raise the distant layer. The offset is applied after zoom, so a 64-pixel adjustment remains 64 logical pixels at every FOV. Each control has its own reset.

Both update immediately and persist through **Save to config**. A fresh-process save/load test preserved FOV 90 and horizon +64 independently. The application now escapes strings correctly in TOML and atomically replaces the config file; the bundled SDK writer emitted unescaped Windows paths, which caused its saved config to fail parsing. The original SDK settings categories and Save button remain in use.

Arena sky-color sampling, original palette/quality shaders, HUD, character geometry, gameplay camera, controller/keyboard input, No Intro and fullscreen hotkeys are retained. Original game assets are not modified.

## Timing audit and implementation

The emulated Xbox video mode must remain at **60 Hz**, while the physical monitor/presenter may run faster. `video_mode_refresh_rate` is explicitly a guest-mode flag in SDK `include/rex/ui/flags.h`. VF2 sets it to 60 before GPU setup and makes that title-specific setting read-only in the runtime editor. It does not change the Windows monitor mode or the global guest clock scalar.

Relevant original code paths were inspected rather than rewriting every occurrence of 30/60/120:

| Path | Role / decision |
| --- | --- |
| `sub_82146690` → `sub_82146168` | Original outer frame body, task update scheduler then conditional render scheduler and present. Retained. |
| `sub_82197A38` / `sub_82197B18` | Separate update/render task traversal, virtual slots +16/+20. Retained. |
| `sub_82199488` → `sub_821830D8` | Original emulated-system update. Gated counter `0x82E7E38C` advances before original system/input work. Used to identify a camera update. |
| `sub_821994B8` → `sub_821831D0` → `sub_82145A90` | Separate bitmap rendering path. Render count is not used to advance camera movement. |
| `sub_821CD3E0`, generated `.0.cpp:8021` | Original swap pacing uses vblank counters and a refresh divisor defaulting to 60. Retained. |
| `vf2_video_mode.inc` | Original D3D backbuffer refresh field remains 60. |
| `sub_82198EB8`, generated `.57.cpp:6304` | Boot/logo state/counters advance through original task updates; final state waits animation completion. Retained. |
| `sub_8216F300` / `sub_8216F398` | Profiling timestamps, not animation deltas. Retained. |
| `sub_821965B8` | Modulo-120 profiler reset, not a 120 Hz animation assumption. Retained. |
| `sub_8212FF38` | Original 12-slice emulated-system timing units, not monitor frames. Retained. |

No additional character-pose interpolation is introduced. Original 60 Hz scheduling and repeated/gated original poses are preserved on faster displays, rather than accelerating gameplay to match monitor refresh.

## Modified files

- `native/src/vf2_panorama.cpp`: affine/axis-uniform projection, full vertical cache, immutable camera snapshot integration, horizon offset and timing telemetry.
- `native/src/vf2_panorama_math.h`: projection/inverse, common scale, horizon transform and per-tick pose cache.
- `native/src/vf2_background_scroll.cpp`: remove independent wall-clock filtering from the fallback path.
- `native/src/vf2_arena_scroll.inc`: update fallback description.
- `native/src/vf2_app.h`: fixed guest-mode setup, physical-monitor logging, F6/config test integration.
- `native/src/vf2_settings.h`: horizon slider, independent reset and correct atomic TOML serialization.
- `native/tests/panorama_math_test.cpp`: projection isotropy, inverse/monotonicity, wraps, immutable repeated poses and presentation-schedule tests.
- `tools/test_panorama.ps1`, `tools/test_background_settings.ps1`, `tools/verify_timing_projection.py`: reproducible runtime/settings checks and report.

Legacy `vf2_arena.cpp` and `vf2_scroll_filter.h` still contain old experimental filtering code; they are not used by the default installed renderer.

## Validation

Build succeeded with the existing toolchain; no programs installed.

- **7,197,545 mathematical checks passed**, covering uniform horizontal/vertical scale, FOV endpoints, horizon/FOV independence, full horizontal wraps, negative source coordinates, duplicate renders and presentation schedules at 60/90/120/144/240/360 Hz.
- Runtime monitor queries reported **3840×2160 at 144 Hz**. Windows display settings were not changed.
- Two 125-second runs requested guest modes 60 and 120; both were normalized to 60 before setup. Arcade update rates were **59.990 and 60.003 updates/s**. Demo counters advanced at approximately 57.2–57.5 updates/s while bitmap scheduling stayed approximately 59.6–59.9 frames/s: the original demo repeats some poses. Those repeated poses remained unchanged; the renderer does not manufacture motion during them.
- The final 125-second default-projection run covered intro, Arcade, KO/replay, continue/game over and return to intro. Recorded scrolling frames had **zero fallback transitions**; duplicate-tick renders reused identical phase/sample counts. 100 GPU readbacks were retained.
- Final **UHD 3840×2160**, FOV 90, horizon +64: 65-second live run, both F6 sliders rendered, process alive, zero background fallbacks, 12 GPU readbacks.
- Final **4:3 1440×1080**, FOV 140, horizon −96: 65-second live run, process alive, zero background fallbacks, 12 GPU readbacks.
- Corrected config parsed with a standard TOML parser and reloaded in a new game process with FOV 90 / horizon +64 intact.

Selected GPU captures were inspected for intro deformation, HUD duplication, missing vertical background coverage and horizon fill. Machine-readable counts, measured rates and installed/build hash comparison are in `analysis/timing-projection-verification.json`.

## Backup and limits

Pre-change executable, sources and CMake configuration: `backups/before-timing-projection-20261015-160014/`.

Live arena coverage is the Shun intro and Lau Arcade arena; not every arena or quality-menu combination was exercised. Source-map handling remains generic for the previously identified 11 panoramas. Actual hardware validation used a 144 Hz display; 120 Hz was covered by guest-mode normalization and deterministic presentation-schedule tests, not by switching the physical monitor to 120 Hz. This does not add native 120fps character-animation interpolation or guarantee artifact-free output at every possible scripted camera pose.
