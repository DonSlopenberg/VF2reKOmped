# Virtua Fighter 2 reKOmped Changelog

## Unreleased

- Help & Options > Settings (main menu and pause menu): the Screen Size row,
  which had no effect due to widescreen optimizations, is now **Advanced
  Settings** and opens the F6 settings.
- Esc or B on the controller closes the Custom Fight window, then the F6
  settings; the same press does not reach the game.
- Fixed a stitching line in Kage's Arena background (quite embarassing
  as this is just a simple gradient).
- Dural arenas with upscaled backgrounds: the water wave moves by exact fractions of a pixel, so the temple columns no longer show jagged steps.
  Finally, I can rest.

## 1.3.3 (2026-10-18)

- Fixed the HUD flicker (timer, health bars, SET/READY/WINNER) that came
  with the 1.3.2 colour fix. **Why it appeared:** 1.3.2 made the GPU reload
  *all* of its memory after every texture upload, including the in-match
  HUD graphics. That memory includes render targets, and textures in use
  vanish for a frame while they reload. **How it was fixed:** after a real
  load the GPU reloads only load-time data (textures and geometry), and
  during a match only the textures that just changed. Render targets are
  never touched. Tested: 20 of 20 fights with correct colours, no dropped
  or dark frames over a whole round captured frame by frame.
- Launcher: the version and build date in the footer are read from the
  game executable; the update notice uses the regular font and sits evenly
  between Play and the credits.
- Analysis notes are all in English now, plus a development journal
  (`analysis/JOURNAL.md`).

## 1.3.2 (2026-10-18)

- Fixed characters and arena parts that occasionally showed wrong colours
  (for example a green beard, or a black fighter with green highlights) or
  missing/black textures from character select until the next load. The GPU
  now reloads texture memory once after each load.
- Launcher opens centred on the screen instead of a cascaded position that
  could be cut off by the taskbar.

### How the colour glitch was tracked down

The report: after a fresh install, Jeffry's beard was green at character
select and stayed that way in the fight; a restart made it go away. The log
of that session was gone, and the bug never came back on demand.

1. **Suspects ruled out by reading the code.** The random line-up is
   written only after the characters are chosen, so it could not affect
   character select. The costume hook clears a bit only where the i960
   register holds the controller word. The custom-fight costume bytes were
   valid (Pai = 6, no +13).
2. **A reproducible case.** An automated Lau vs Pai fight on the Great Wall
   with GPU frame captures showed Pai black with green highlights, plus a
   black floor and banner. With the sides swapped it looked fine, and
   turning Tourist off seemed to fix it, but a five-way bisect of the
   Tourist patches then came back clean every time. The bug was
   intermittent, not tied to a setting.
3. **Measuring the rate.** A loop of 10 fresh fight starts, each with
   captures, gave 2 bad runs; a later loop of 20 gave 5. That is roughly one
   fight start in four.
4. **The data is correct.** Every rebuild of the 3D colour table (1024
   RGB555 colours expanded into 16 shades) was dumped in good and bad runs.
   The colours were byte-for-byte identical, so the game computed the right
   values and the GPU showed something else.
5. **A wrong turn.** Forcing the colour table to rebuild every frame turned
   everything black and green: the source table is only valid on frames the
   game marks as changed. Re-uploading the finished texture a few frames
   later changed nothing (5 of 20 still bad). The bad frames also had
   missing textures (the gold coin) besides wrong colours, so the problem
   was wider than the colour table.
6. **A timing race.** Logging every texture upload with a data hash made the
   bug vanish (0 of 12): a classic sign of a race. The log showed about 150
   model textures streamed in over ~60 frames from one shared staging
   buffer, while the game kept rendering. The GPU emulation watches
   texture memory pages for CPU writes; when it read a page while the game
   was still writing it, it kept a half-old copy and missed the rest.
7. **The fix.** Three frames after a burst of model-texture or colour-table
   uploads ends, the physical memory views are invalidated once, so the GPU
   reloads them from the now-complete guest memory. Result: **20 of 20**
   fight starts correct, against 7 of 30 bad before (the chance of 20 clean
   runs in a row without a fix is about 0.5%).

## 1.3.1 (2026-10-18)

- Esc pauses any match (Arcade, Local Versus, Custom Fight) for the side that
  is playing, whatever mix of pads and keyboard is used.
- Alternate costumes: the select screen keeps showing the normal costume, so
  preview heads and portraits no longer flicker or lose colours when Up/Down
  is pressed quickly; the chosen costume is applied when the fight starts.
- Dural arenas: new seamless water wave on both arenas (smooth 128-tick
  loop, no flicker, jumps or smeared rows when the camera tilts).
- Antialiasing: Off / FXAA / FXAA Extreme, in the launcher and in F6.
- F6: the first tab is now **VF2 reKOmped**; the window scales with small
  game windows (Advanced tab included), opens at the top centre and always
  stays reachable when dragged; the background FOV and offset sliders moved
  down (**Background FOV & Offset**); internal background switches are no
  longer shown.
- Launcher: notice and link when a newer release is on GitHub; shows only
  the game version; correct size when moved to a screen with another DPI;
  clearer Random Arenas description.

## 1.3.0 (2026-10-18)

- Launcher: new **Arcade** section with Tourist, Random Opponents and Random
  Arenas; the help line names F6 (Settings & Custom Fight Launcher); the
  Display header was removed. 21:9 resolutions are now kept when saved.
- Alternate costumes: Up/Down at character select (Arcade and Versus); the
  cursor frame turns to its negative colour while the alternate costume is
  chosen. Custom Fight has an **Alternate costume** option per player.
- Custom Fight: **Tourist** in the difficulty list; full matches (a match no
  longer ends after the first round); starting during an Arcade or Versus
  game first leaves that game cleanly (no frozen fighters or leftover
  select-screen graphics).

## 1.2.2 (2026-10-17)

- Native 21:9 ultrawide (2560x1080, 3440x1440): the 3D scene and backgrounds
  fill the width, menus and HUD stay centred, menu side bars are filled.

## 1.2.1 (2026-10-17)

- QHD/UHD output no longer cropped in the window (1080-line guest frame
  drawn at 2x).
- The first-start "No Save Data found" question is answered automatically.

## 1.2.0 (2026-10-17)

- Upscaled arena backgrounds.
- Arcade extras: Tourist difficulty, Random Opponents, Random Arenas, hidden
  Dural (hold Start), Infinite Health cheats; an ended Arcade run returns to
  the Main Menu; save device and achievement fixes.

## 1.1.0 (2026-10-16)

- Windows launcher with resolution, fullscreen, monitor and startup options,
  light/dark theme and version footer.
- F6 settings keep the game visible while paused; pause menu Achievements.
- Custom Fight Launcher with Human/CPU sides, native arena variants, native
  difficulty, rounds and round time.

## 1.0.0 (2026-10-15)

- Initial stable version.