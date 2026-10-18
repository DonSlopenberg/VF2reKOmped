# Settings and Custom Fight lifecycle fixes

## Agreed behavior

- Settings F6 has a title-bar X. Closing it also hides its Custom Fight child.
- Start Fight and Restart Fight close both overlays and begin gameplay.
- F6 suspends the native M2E task only while the settings window is open. It
  resumes only a task that it suspended. The native pause timescale remains
  untouched, so an already-paused fight stays paused after F6 closes.
- Start on either player/controller (including keyboard Start bindings) opens
  the original pause menu during Custom Fight, including CPU/CPU matches.
- A full custom match returns to Main Menu after the winning animation.
  Per-round transitions continue normally. Confirmed Exit Game does the same.
- Startup opens the original Arcade / Offline Versus Main Menu automatically.
  Only the save-icon explanation is skipped; actual save operations, errors
  and save-creation questions are not intercepted.

## Root causes and changes

`native/src/vf2_settings.h` keeps dialog ownership with Vf2App while hiding
closed windows, adds the close button and reports its visibility to the host.
Both Start and Restart close the overlays. `native/src/vf2_app.h` reopens a
previously closed settings object correctly on the next F6 press.

`native/src/vf2_fight_host.cpp` enters native gameplay scene 1/child 6 before
consuming a direct-start request. This scene owns TaskPause and normal game
cleanup; direct ROM initialization had bypassed it. The native logical-player
enable API is called for both users. The generic native pause query originally
polls Back (translated mask 0x100); custom fights additionally accept Start
(0x200). Start is consumed by this frontend instead of also reaching Model 2's
join/new-challenger switches. Normal Arcade input behavior is unchanged.

Start/Restart while paused closes the original pause task recursively through
its exit API, restores native audio/timing through its Resume routine, then
registers a fresh original pause task before starting the requested match.

`native/src/vf2_fight.cpp` sets the verified native gameplay flag (bit 1),
alongside the two entered-player flags. Without it the match-result dispatcher
took the attract/demo continuation. A hook at original i960 PC 0xF208 detects
the full-match continuation after MATCHEND_DSP (0xF85C) has completed the winner
animation. Checking only the outer mode byte missed Versus starting its next
pairing while remaining in main mode 9. The host requests native reset and
Main Menu scene 2/child 8 instead. Last launcher configuration stays available.

`native/src/vf2_logo.cpp` skips registration of TaskWarning, verified against
native string 747 in the decoded English string table: the explanatory text
about the save icon. Separate save/profile/error dialog tasks remain intact.
Main Menu is requested after the original boot task finishes resource/profile
initialization. No save files are deleted or fabricated.

## Verification

- Final build: `analysis/build-fight-flow-6.log`, successful.
- `analysis/startup-menu-test-normal/`: no button input, no No Intro override;
  native Main Menu opens automatically. Screenshot inspected.
- `analysis/fight-flow-test-final-gameplay/`: P1 and P2 Start open the native
  pause menu, Resume works, F6 freezes a running fight, and F6 opened over a
  native pause preserves that pause on close. CPU/CPU pause was also exercised.
  Four sampled F6-over-pause rows retain exactly the same guest tick (713).
- `analysis/fight-test-full-match-return/`: final candidate, Dural versus Akira,
  Human/CPU, full match reaches native result after winner animation and returns
  to Main Menu. Process survives 100 seconds; Main Menu remains stable.
- `analysis/fight-flow-test-exit-confirm-yes/`: final candidate, restart from
  pause, native Exit Game with Yes confirmation, Main Menu, then another custom
  fight. P2 input and source indices 0/1 remain valid after reentry. Process
  survives 48 seconds. The earlier exit test accepted the default No, correctly
  returning to the pause menu instead of exiting.
- Machine-readable assertions: `analysis/FIGHT_FLOW_TEST_RESULTS_20261016.json`.
  Input uses the opt-in synthetic XInput harness. GPU captures show game-owned
  menus; they do not include the host ImGui overlay. The title-bar X and the
  actual mouse click on Start were code-reviewed/built, not separately clicked
  by UI automation.

Backup before this change: `backups/before-fight-flow-20261016-013139/`.
Only native sources, developer test tools/docs and the engine executable were
updated. Original game assets and installed user saves were not replaced.

Installed executable SHA-256: 270291A2B3CD39883299DCF826C68E5ABADD85A2B5F5FB0630503514FCE26799
