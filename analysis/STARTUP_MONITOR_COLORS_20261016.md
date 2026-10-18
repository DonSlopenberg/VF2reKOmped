# Startup branding, display selection and character-color investigation

Installed both rebuilt EXEs. Backup: `backups/before-logos-monitor-20261016-101455/`.

## Startup fix

The previous boot-task draw hook did not suppress the actual branding animation:
it is registered and drawn independently by the native global animation registry.
`native/src/vf2_logo.cpp` now matches the boot task's exact animation handle and
object in that registry and suppresses only its native virtual draw
(`0x820F4C50`). Creation, animation updates, resource loading, palette setup and
cleanup remain native. The complete original boot draw callback is restored.
The specific SEGA voice playback callsite is also omitted while skipping; other
audio and bank initialization remain native. The option is named
**Skip Startup Logos**; the existing `vf2_no_intro` config key remains compatible.

`tools/verify_startup_logos.py` catches actual visible logo pixels. The old
implementation fails with 108452 original SEGA-ink pixels in each of five early
frames. The corrected run has zero in all six early frames and reaches Main Menu.
Contact sheets confirm both SEGA and CRIWARE absent. Loading still completes.

- Enabled: `analysis/startup-logos-registry-on/`.
- Disabled: `analysis/startup-logos-release-off-complete/`, original branding and
  normal Main Menu transition retained.
- UHD selected-monitor smoke: `analysis/startup-logos-release-uhd/`.

## Monitor selection

`native/src/vf2_monitors.h` enumerates active Windows displays and resolves the
reported friendly model name using QueryDisplayConfig. `launcher.cpp` shows the
name for one display or a pad/keyboard-accessible dropdown for multiple displays.
Numbered labels distinguish identical monitor models. The selected GDI display
device is saved with TOML escaping alongside existing launcher settings, and is
passed to the engine. `vf2_app.h` positions the window on that display before
the existing deferred fullscreen transition. A disconnected selection falls
back to the primary active display.

Tests: config round-trip for all 16 combinations including escaped display ID;
active monitor detected by its model name, physical 3840×2160; runtime selected it
and entered fullscreen at 3840×2160, DPI 192, host 144 Hz. Multiple-display UI
was inspected using an explicitly gated test fixture because only one physical
display was active. Physical switching between two monitors remains untested.
Preview: `analysis/launcher-monitor-preview/`.

## Character colors — verification and unresolved report

All 37 files in `data/game` match SHA-256 against
`backups/before-root-layout-20261014/extracted/`. No textures were replaced.
The installed engine before this investigation also had an identical `.text`
section to the engine immediately before the launcher artwork change.

Original boot graphics setup is now preserved instead of bypassing its entire
draw callback. Fresh native Arcade tests reached real fights (not Custom Fight)
with Kage and Sarah wearing their original blue/navy costumes:

- `analysis/arcade-colors-sarah-original/last.png` is the Kage test (native ID 3).
- `analysis/arcade-colors-sarah-check/last.png` is Sarah (native ID 2).
- `analysis/arcade-colors-palette-baseline/last.png` also shows blue Kage on the
  previously installed engine under the same test inputs.

The reported red costumes and white lips were **not reproduced**. Therefore this
does not establish their root cause or prove that every occurrence is resolved.
If the symptom persists, a screenshot and the current match configuration are
needed to distinguish a native alternate costume from a rendering/palette error.
No speculative recoloring or forced costume replacement was applied.

Final build: `analysis/build-startup-monitor-installed.log`. Seven-size app icons
verified in both installed EXEs. No software installed; user saves untouched.
