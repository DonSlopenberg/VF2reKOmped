# Virtua Fighter 2 reKOmped Changelog

## 1.3.2 (2026-10-18)

- Fixed characters and arena parts that occasionally showed wrong colours
  (for example a green beard, or a black fighter with green highlights) or
  missing/black textures from character select until the next load. The GPU
  now reloads texture memory once after each load.

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