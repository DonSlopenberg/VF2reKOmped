# Windows launcher update

- Native Win32 launcher using Segoe UI, Windows app light/dark preference,
  system accent, dark title bar and Per-Monitor V2 DPI awareness.
- Embedded transparent user-supplied Wikimedia logo; original aspect preserved.
- Resolution, Fullscreen and Skip intro read from and atomically update the
  shared `data/engine/vf2.toml`. Unrelated settings and comments are retained.
  Legacy `resolution` is read when `vf2_output_resolution` is absent.
- Settings reload when the launcher regains focus, including after using F6
  in the game. Launch arguments match the selected resolution and intro setting.
- Footer shows build timestamp, Windows x64 and runtime version.
- Both EXEs contain a seven-size Windows app icon based on the numeral 2 from
  the supplied SVG's embedded original PNG. No new runtime dependencies.
- Controller navigation now includes Skip intro. Keyboard fallback is retained.

## Verification

- Successful full build: `analysis/build-launcher-style-7.log`.
- `tools/test_launcher_config.cmd`: 16 combinations passed; legacy values,
  unrelated path settings and license setting preserved.
- Actual launcher client rendering inspected in light/dark modes, at the host's
  200% DPI. Verified UHD selected from the original config and QHD with both
  options checked from an isolated test config. Captures are the launcher's
  own rendered surface, not desktop screenshots:
  `analysis/launcher-style-preview/`.
- `tools/verify_exe_icons.py`: both compiled EXEs contain all icon sizes
  16, 24, 32, 48, 64, 128 and 256 pixels.
- No arena, input, gameplay or save data changes in this update. Multi-monitor
  DPI transitions and physical controller clicks were not exercised in this pass.

Sources and repeatable icon packaging: `native/resources/SOURCES.md` and
`tools/prepare_launcher_art.py`. Backup: `backups/before-launcher-style-*`.
