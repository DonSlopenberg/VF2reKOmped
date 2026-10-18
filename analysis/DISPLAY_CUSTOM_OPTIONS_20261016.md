# Display and Custom Fight update — 2026-10-16

Installed the rebuilt engine at `data/engine/vf2.exe` and launcher at `Virtua Fighter 2.exe`.
Backup: `backups/before-display-options-20261016-085854/`.
Engine SHA256: `52B81CFA26B3EB38FB1C1A29B40C6485C4E20761858C0B3CAE94CB0F0FD9B94D`.

## Changes

- `native/src/vf2_video.cpp`, `vf2_panorama.cpp`: extend the actual final Model 2 viewport and bitmap composition to the whole output in ordinary demo/gameplay as well as Custom Fight. Keep auxiliary square/internal targets and the existing perspective correction.
- `native/src/vf2_display.h` (new), `vf2_app.h`, `launcher.cpp`: one authoritative output resolution. UHD selects 3840×2160 GPU output, with the existing 1920×1080 guest coordinate system and 2× draw scaling. Migrate old saved output resolutions without interpreting divided guest dimensions as the output choice.
- `native/src/vf2_settings.h`: F6 > VF2 > Display contains Resolution and Panoramic Background controls. Technical settings are under Advanced. Resolution changes explicitly require restarting. Save only the authoritative resolution, not competing derived dimensions/scales.
- `native/src/vf2_fight.h`, `vf2_fight.cpp`: shared native difficulty Easy/Normal/Hard/Hardest, rounds to win 2–5, and round time 15/30/45/60 seconds; every option also has Use Game Settings. Difficulty uses the original adaptive table and tuning data. Timer is set once after native match initialization and before scheduled FIGHT_INT converts it to the countdown. No per-frame timer overwrite or gameplay acceleration.
- `native/src/vf2_fight_config.h` (new), `vf2_settings.h`: atomic separate `custom-fight.cfg` persistence. Start applies edited choices; Restart repeats the resolved last configuration. Restore prior runtime values after the custom match; do not persist custom choices into native Arcade/SRAM settings.
- `native/src/vf2_fight_host.cpp`: Main Menu invokes the native Select_48k_loop music command when the previous default selected the stop entry. Preserve other explicitly selected tracks.

## Validation

- Successful release build: `analysis/build-display-options-6.log` (existing compiler deprecation warnings).
- Viewport regression fails on the original inset rectangle `(367,81,1186,918)` and passes after the change: `tools/verify_display_fit.py`.
- `analysis/panorama-test-uhd-display-final/`: 43-second demo/navigation run, alive at completion; final viewport `(0,0,1920,1080)`, GPU captures 3840×2160. Inspected the final demo image: full width and uncropped centered text. Square 1024×1024 auxiliary targets remain unchanged.
- `analysis/fight-options-test-final-3/`: 115-second integration run. Native resolved tuples `(difficulty,wins,time)` were `(0,2,15)`, `(1,3,30)`, `(2,4,45)`, `(3,5,60)`, Restart `(3,5,60)`, defaults `(1,2,15)`, Dural mirror on boss arena `(3,2,15)`. Values remain present during actual native updates. Complete Dural mirror match reaches the original full-match result continuation. Process alive at completion; GPU 3840×2160. Inspected a fight capture showing the actual 45-second countdown. `tools/verify_fight_options.py` passes.
- `tools/test_fight_config.cmd`: 125 persisted combinations pass round-trip validation; invalid values rejected.
- `analysis/display-settings-test-final/`: direct fullscreen startup using only `--vf2_output_resolution 3840x2160`; GPU output 3840×2160 without F4. F6 draws both panorama sliders and Custom Fight controls. Persisted profile loads characters 9/10, arena 10, difficulty 3, wins 5, time 60. Saved settings contain the authoritative UHD choice without derived video dimensions/scales. Process alive at completion.
- Runtime confirms the native Main Menu Select_48k_loop dispatch. Audio was not independently listened to or recorded; this verifies the playback path, not subjective audibility.

## Test scope

This update tests the reported UHD sizing regression and new Custom options. It does not constitute a new exhaustive visual pass of every arena, refresh rate, physical controller, or every F6 mouse interaction. GPU captures exclude the host ImGui overlay; F6 layout execution was checked through its draw logs. Existing panorama behavior beyond the final viewport change is retained.
