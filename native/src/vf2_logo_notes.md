# Startup logo skip evidence

Target: retained decoded Xbox 360 image and generated PPC source for VF2.
This investigation used local generated code; REA reports no configured
native disassembler provider. No software installation was attempted.

## Static observations

- `0x82051D08` contains `DATA INITIALIZE`; the following vtable at
  `0x82051D1C` (preceded by its RTTI pointer at `0x82051D18`) contains initializer `0x82198B68`, update `0x82198EB8`, exit
  `0x82198BC8`, and draw `0x82198C20`.
- Constructor `0x82497520` writes `0x82051D1C` into the static task object
  at `0x83465878`, confirming the actual vtable begins after the RTTI pointer.
- Initializer zeros object fields `+76` (state), `+80` (counter), and `+84`
  (animation handle). Update is a switch over states 0 through 22.
- The update initializes sprite animation `+84` and asynchronously loads
  resource groups before reaching state 22. It also references
  `rom/sound/adx/sega441.adx` at `0x82051D34`.
- State 22 returns completion when the animation reports complete through
  `sub_820F3720`. Earlier states must run unchanged.
- Draw uses that same animation handle; exit frees it through
  `sub_820F7018`, unloads resource groups 8 and 9, and performs the original
  exit routine.
- The task scheduler `sub_82198188` moves a completed update into its normal
  task exit and cleanup path. Returning completion here does not bypass
  resource cleanup.

## Implementation

`vf2_logo.cpp` hooks only these four task functions and validates the exact
vtable and active object. A new controller button press, Enter, or Space
latches a request. Startup branding draw is suppressed after the request;
resource initialization continues. Only state 22's final animation wait may
return completion early. Exit resets the latch. Keyboard input is accepted
only while this process has the foreground window.

Build integration: add `src/vf2_logo.cpp` to the existing `vf2` CMake source
list. Existing `user32` and `xinput9_1_0` link dependencies are sufficient.
Settings exposes the default-enabled `vf2_skip_logos_with_buttons` option
under Input, and `vf2_no_intro` (No Intro) under Game. No Intro automatically
latches the same safe skip request and takes effect on the next launch.
The normal build now explicitly includes this translation unit (15 October
2026); the previously missing CMake source entry disabled these hooks.

## Verification boundary

16 October correction: the startup logo is also drawn by the global animation
registry (native virtual draw `0x820F4C50`), independently of DATA INITIALIZE's
draw callback. The registry at `0x824CBAC4` stores a handle at node +12 and its
object at node +16. Match exactly the animation handle owned by boot +84 and
suppress only its draw; native updates, palette/resource setup and cleanup run.
The task's original draw callback now runs unchanged. The original SEGA voice
callsite `0x821992F8` is omitted only while that boot skip request is active.
GPU captures demonstrate SEGA/CRIWARE absent with the option enabled and Main
Menu reached. The previous implementation set the skip latch but still drew
the registered animation.

The relation to startup branding is a static inference from the sprite
animation and Sega sound in the boot task. Runtime verification must press
a button during visible branding and confirm both log messages:
`skip requested; resource initialization continues` and
`final animation wait skipped after initialization`, followed by a working
title/menu. No assertion that all logos are covered has been made before
this runtime check. Input hooks do not target menu, character selection,
attract mode, or battle code.
