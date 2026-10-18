# Vertical panorama precision fix — 2026-10-16

## Cause and implementation

The native i960 scroll calculation reads the delayed signed camera pitch at
`0x503010`, subtracts 736, and shifts right four bits at ROM PC `0x245C0`.
Its vertical hardware port therefore contains whole source pixels. Previously
`CaptureComputedNativeScroll` converted that already quantized port to double;
the lost four bits never reached the GPU. At UHD and background FOV 120, a
whole source pixel spans approximately 5.625 output pixels.

The renderer now captures those four bits from the exact delayed sample before
the native routine advances its delay registers. It restores their negative
fraction to the interpreter's authoritative integer result, retaining all bank
offsets, signed-angle behavior, original timing and double-buffer selection.
Pitch resolution is now 1/16 source pixel (about 0.352 UHD pixels at FOV 120).
No independent low-pass filter, wall-clock integration, extrapolation, camera
offset or gameplay-speed change was introduced.

The game's atlas preparation (`82143DF0` / `82143F20`) already decodes tile IDs
and palette indices into an RGBA atlas. The scroll drawing path originally
selected point minification/magnification. The panorama draw now temporarily
sets both to LINEAR through the original sampler API (`82103890`, native enum
428), then restores POINT (427) before foreground/HUD rendering. Indexed tile
and palette textures retain their original filtering. Two overlapping guard
tiles keep bilinear taps inside each bank's panorama data at the shared seam.

FOV, Horizon Height, upper/lower sampled color extensions and original palette/
quality decoding remain in place. Disabling Smooth Scroll intentionally uses
the original integer phase and point sampling.

## Modified files

- `native/src/vf2_panorama.cpp`: capture/latch the pitch fraction; scoped RGBA
  filtering; overlapping bank guards; opt-in full-precision trace metadata.
- `native/src/vf2_panorama_math.h`: native fixed-point reconstruction and bank constants.
- `native/tests/panorama_math_test.cpp`: exhaustive pitch/offset regression,
  slow fractional pitch under 60/90/120/144/240/360 Hz render schedules, guards.
- `tools/verify_panorama_trace.py`: compare integral ports and exact delayed
  fractions with the full-precision rendered phase (legacy CSV rounded to six digits).
- `tools/test_vertical_panorama.ps1`, `tools/verify_vertical_panorama.py`:
  isolated UHD native-demo/gameplay regression, GPU quad and phase checks.
- `tools/test_refresh.cpp`: temporary physical-refresh test utility; no registry persistence.

`vf2_fight_host.cpp` is byte-identical to its pre-task backup; a temporary
attract test was discarded. No game assets or user save/config were edited.

## Validation

The regression command `cmd /c analysis\test_panorama_math.cmd` first failed
with `FAIL: native pitch retains fractional texels including negative angles
and bank offsets` against the original integer calculation. After correction:
**7,470,222 checks passed**, including all 65,536 signed pitch encodings and
four native bank adjustments, negative/wrapped positions and repeated renders.

| Runtime suite | Settings | Result |
| --- | --- | --- |
| `vertical-linear-144` | UHD, CPU fight, FOV 120, horizon 0 | 890 phase records, 58 fractional records, 116 fractional GPU quads; alive; zero panorama fallbacks |
| `vertical-native-demo-144` | UHD, native Arcade attract/demo, FOV 90, horizon 32.25 | 3,051 phase records, 2,743 fractional records, 5,430 fractional GPU quads; 3,054 scrolling frames, zero fallbacks |
| `vertical-guarded-demo-144` | Final guarded renderer; UHD native demo, FOV 140, horizon -32.25 | Same deterministic 3,051 phase records / 2,743 fractional records / 5,430 fractional GPU quads; 3,054 scrolling frames, zero fallbacks; 102 native samples match exactly |
| `vertical-no-smooth-144` | Final guarded renderer; UHD fight, Smooth Scroll off | 890 phase records; zero fractional phases/quads; 30 native samples match; no crash or fallbacks |
| `tools/test_background_settings.ps1` | Save/load separate FOV 90 and horizon 64 | Passed in two separate processes |

GPU readbacks are 3840×2160. Captures inspected include normal fighting and
the scripted Shun-stage demo; shader colors, models, foreground and HUD remain
present. Full-precision GPU vertex traces confirm that fractional vertical
positions survive quad construction and the FOV/horizon transform. Snapshot
and trace checks do not prove that every frame of every arena is artifact-free.

**Refresh limitation:** the actual monitor was UHD at 144 Hz. Windows rejected
both temporary 60 and 120 Hz mode changes with `DISP_CHANGE_FAILED` (-1), and
the monitor remained at 144 Hz. Those are not physical 60/120 Hz test passes.
Unit tests cover those render schedules; native gameplay/simulation remains
fixed at 60 Hz, with repeated poses rather than invented 120-fps camera poses.
No visual verification on a physical 60/120 Hz output is claimed.

## Build, installation and recovery

`cmd /c native\build.cmd` succeeded with the existing toolchain. No tools
were downloaded or installed. The built engine was installed at
`data/engine/vf2.exe`; root launcher behavior is unchanged.

Previous engine and source snapshots:
`backups/before-vertical-subpixel-20261016-151441/`.

Installed SHA-256:
`71bf0221620dfc751da0d6c94935c50102a14e177392fe586b0fb7e7b36f41c0`.

The earlier ZIP in `dist/` was not regenerated by this task.
