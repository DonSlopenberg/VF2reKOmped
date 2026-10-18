# Fullscreen startup and logo skipping

Installed executable SHA-256: `D55E870F1484BB5A0037FAC2EE61AF4AB1419797138FFEB7D7880D41D64B3A9E`.

Changes:

- `native/src/vf2_app.h`: defer initial fullscreen until the window is open and the presenter is attached. Previously the SDK requested fullscreen before opening the window and attaching the presenter. Entering fullscreen after attachment follows the successful later F4 transition without requiring the user to toggle it.
- `native/CMakeLists.txt`: actually include `vf2_logo.cpp` in the normal build. Its previous omission explains why neither the startup hooks nor their setting were active.
- `native/src/vf2_logo.cpp`: expose `vf2_no_intro` under the Game category in F6 settings, described as “No Intro”. It automatically requests startup-logo skipping while allowing resource initialization and original task cleanup to finish. Save settings to retain the selection; it applies on subsequent launches. This skips startup branding, not attract-mode demonstrations after the title screen. Manual skipping accepts controller buttons, Enter, Space, Escape and A–Z while the game has focus. The existing manual-skip option remains enabled by default.

Validation:

- Build succeeded with the installed toolchain.
- Fresh fullscreen UHD launch reported a 3840×2160 surface at DPI 192 (200%). Twelve direct GPU readbacks were collected; inspected title/logo output was complete and correctly framed. No F4 toggle was sent. These GPU readbacks do not capture the final Windows desktop composition.
- `vf2_no_intro=true` completed the startup animation wait through the original cleanup path after resource loading; the log confirms completion.
- With No Intro disabled, a scripted Start press latched a manual skip and completed the animation wait. The process remained alive. Physical keyboard presses were not injected during the automated test; their foreground-scoped polling is implemented alongside controller polling.
- Installed executable hash matches the build output.

Evidence: `analysis/startup-uhd-no-intro/`, `analysis/startup-buttons/`, `analysis/build-startup-fixes.log`.

Backup: `backups/before-startup-fixes-20261015-145916/` contains the previous EXE, CMake configuration and native sources. Arena renderer and assets were not changed in this update.
