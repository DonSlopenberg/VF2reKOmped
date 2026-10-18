# Custom Fight Launcher and panorama update

Built and installed on 2026-10-16. Original executable is preserved in
`backups/before-launcher-install-20261016/vf2.exe`; the earlier source snapshot
remains in `analysis/checkpoint-custom-fight-20261016-011056/`.

## Use

Open F6, choose **Custom Fight Launcher...**, select both characters and
Human/CPU controls and an arena, then **Start Fight**. F6 resumes gameplay.
Reopen F6 to change the match or use **Restart Fight**. Difficulty remains the
original shared game setting, as requested. Dural is available on either side,
including mirror matches and the original boss arena.

## Implementation

- `native/src/vf2_fight.h`, `vf2_fight.cpp`: queued launcher requests, native
  selection resources, player control modes, HUD, stage and match initialization,
  restart configuration, validation and opt-in test telemetry.
- `native/src/vf2_i960.h`, `vf2_i960.cpp`: bounded native CALL/RET bridge that
  preserves interpreter state and permits original task initialization.
- `native/src/vf2_fight_host.cpp`: original frontend task exit/resume callbacks
  and boot-resource readiness, applied only when a custom start is pending.
- `native/src/vf2_settings.h`: English F6 launcher, independent panorama FOV
  and horizon sliders; original settings retained.
- `native/src/vf2_panorama.h`, `vf2_panorama.cpp`: separate palette-aware top
  and bottom edge fills. Native computed scroll registers are captured at i960
  PC 0x24664, published after completion at 0x2467C and associated with the
  original register buffer at 0x82188C08. No guessed camera phase or added lag.
- `native/src/vf2_video.cpp`: custom-start final viewport expansion without
  changing arena or model projection behavior.
- `native/src/vf2_test_input.cpp`: opt-in synthetic XInput users for offline
  input verification. Inactive during normal launches.
- `native/CMakeLists.txt`: new source registration.
- `tools/verify_panorama_trace.py`: independent comparison with native scalar
  scroll ports, respecting their 512-pixel window.

## Results

- Final build: `analysis/build-resumed-final.log`, successful.
- Panorama math: 7,197,545 checks passed, including full texture rotations,
  negative scroll, tile boundaries and 4:3/16:9 extents.
- Earlier native matrix: 22 configurations, all 16 native stage records and all
  11 characters across the cases. Dural P1/P2/both, Human/CPU permutations and
  restart survived with requested native state. This is not all 121 pairings.
- Final UHD matrix: `analysis/fight-test-resumed-final-uhd/`, six configurations
  including Dural/Dural, boss stage, CPU/CPU, Human/Human and restart; alive after
  50 seconds. GPU readbacks are 3840x2160; inspected models and HUD are correct.
- Final dynamic demo/Arcade: `analysis/panorama-test-resumed-final-uhd/`, alive
  after 45 seconds. All 30 recorded scroll samples on arenas 0/3 match the native
  horizontal phase modulo its 512px cache window and vertical phase; no stale
  buffer samples. Earlier 4:3 run `pant-corama-tesomputed-scroll` also passed
  30/30 samples. These sampled traces do not prove every frame of every demo.
- Native input: `analysis/fight-input-test/state.txt.controls.csv`; both user
  masks reach the ROM immediately before its input stores (e.g. P1/P2 64/128
  and 4/2), physical ports respond, and both character positions change. No
  unsupported binding reset was added. This uses synthetic XInput, not a human
  physical-controller session. State logs contain appended prior runs.
- F6 UI smoke: `analysis/launcher-ui-resumed-final/`, alive after eight seconds;
  logs confirm both sliders and launcher widgets draw. Interactive navigation
  of every widget has not been independently exercised.
- Requested guest rate 120 was clamped to native 60 Hz as intended. Actual
  monitor was 3840x2160 at 144 Hz. Dedicated physical 60/120 Hz comparisons were
  not performed. Host refresh does not multiply native gameplay updates.

## Limits

The extended panorama path still does not reproduce the original per-scanline
water/wave modulation. Absolute camera scroll synchronization is verified as
above; complete absence of visual flicker across every arena/demo is not
claimed. Native stage 15 is a debug-grid variant; stage 13/14 retain generic
labels because no authored arena names were verified. Dural's alternate native
costume points to the same model. No upscale or bitmap replacement loader was
added in this stage. Normal Arcade navigation was exercised in demo tests;
long Arcade campaigns and online functionality were not tested.

Installed SHA-256:
`434270F6EE1F90A80D5CAC60C819D608E917D52AEFB49020BABC89A871D31B26`
