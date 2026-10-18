#include "generated/default/vf2_pch.h"
#include "generated/default/vf2_funcs.h"
#include <rex/hook.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <windows.h>
#include <Xinput.h>
#include <cstdlib>
#include <chrono>
#include <fstream>

REXCVAR_DEFINE_BOOL(vf2_skip_logos_with_buttons, true, "Input",
                   "Skip startup logos with controller buttons or keyboard");
REXCVAR_DEFINE_BOOL(vf2_no_intro, false, "Game",
                   "Skip Startup Logos: automatically omit startup branding while loading resources")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

extern "C" REX_FUNC(__imp__sub_82198B68);
extern "C" REX_FUNC(__imp__sub_82198EB8);
extern "C" REX_FUNC(__imp__sub_82198C20);
extern "C" REX_FUNC(__imp__sub_82198BC8);
extern "C" REX_FUNC(__imp__sub_820F4C50);
extern "C" REX_FUNC(__imp__sub_82179B50);

namespace {
// This is the DATA INITIALIZE task, not the attract sequence or menu.
// Its animation owns the startup branding. State 22 is its final animation
// wait, after the original asynchronous resource/profile initialization.
// 0x82051D18 is the RTTI pointer preceding the actual virtual function table.
constexpr uint32_t kBootVtable = 0x82051D1C;
uint32_t boot_object = 0;
bool skip_requested = false;
uint32_t previous_buttons = 0;
std::chrono::steady_clock::time_point boot_started;
const bool input_test_hold = std::getenv("VF2_LOGO_INPUT_TEST") != nullptr;

uint32_t Buttons() {
  constexpr WORD mask = XINPUT_GAMEPAD_A | XINPUT_GAMEPAD_B |
      XINPUT_GAMEPAD_X | XINPUT_GAMEPAD_Y | XINPUT_GAMEPAD_START |
      XINPUT_GAMEPAD_BACK | XINPUT_GAMEPAD_LEFT_SHOULDER |
      XINPUT_GAMEPAD_RIGHT_SHOULDER;
  uint32_t result = 0;
  // Reuse the existing opt-in controller test input; inactive in normal play.
  if (const char* path=std::getenv("VF2_AUTOMATED_INPUT")) {
    std::ifstream input(path);
    uint32_t buttons=0;
    if (input >> std::hex >> buttons) result |= buttons & mask;
  }
  for (DWORD user = 0; user < XUSER_MAX_COUNT; ++user) {
    XINPUT_STATE state{};
    if (XInputGetState(user, &state) == ERROR_SUCCESS)
      result |= uint32_t(state.Gamepad.wButtons & mask);
  }
  DWORD foreground_pid = 0;
  GetWindowThreadProcessId(GetForegroundWindow(), &foreground_pid);
  if (foreground_pid == GetCurrentProcessId()) {
    if (GetAsyncKeyState(VK_RETURN) & 0x8000) result |= 0x10000;
    if (GetAsyncKeyState(VK_SPACE) & 0x8000) result |= 0x20000;
    for (int key='A'; key<='Z'; ++key)
      if (GetAsyncKeyState(key) & 0x8000) result |= 0x40000;
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) result |= 0x80000;
  }
  return result;
}
}

REX_HOOK_RAW(sub_82198B68) {
  const uint32_t object = ctx.r3.u32;
  __imp__sub_82198B68(ctx, base);
  REXLOG_INFO("VF2 startup task initialized: object={:08X}, vtable={:08X}",
              object, REX_LOAD_U32(object));
  if (REX_LOAD_U32(object) == kBootVtable) {
    boot_object = object;
    skip_requested = REXCVAR_GET(vf2_no_intro);
    previous_buttons = Buttons();
    boot_started = std::chrono::steady_clock::now();
    REXLOG_INFO("VF2 startup logos: Skip Startup Logos={}; press a controller button or keyboard key to skip", skip_requested);
  }
}

REX_HOOK_RAW(sub_82198EB8) {
  const uint32_t object = ctx.r3.u32;
  if (object == boot_object && REX_LOAD_U32(object) == kBootVtable) {
    const uint32_t buttons = Buttons();
    if ((buttons & ~previous_buttons) && !skip_requested &&
        REXCVAR_GET(vf2_skip_logos_with_buttons)) {
      skip_requested = true;
      REXLOG_INFO("VF2 startup logos: skip requested; resource initialization continues");
    }
    previous_buttons = buttons;
    // Opt-in local diagnostic: provide time for a keyboard input test before
    // initialization begins. Normal launches never enter this branch.
    if (input_test_hold && !skip_requested && REX_LOAD_U32(object + 76) == 0 &&
        std::chrono::steady_clock::now() - boot_started < std::chrono::seconds(20)) {
      ctx.r3.u64 = 0;
      return;
    }
  }
  __imp__sub_82198EB8(ctx, base);
  if (object == boot_object && skip_requested &&
      REX_LOAD_U32(object + 76) == 22 && ctx.r3.u32 == 0) {
    // Return through the task scheduler's existing exit and cleanup path.
    ctx.r3.u64 = 1;
    REXLOG_INFO("VF2 startup logos: final animation wait skipped after initialization");
  }
}

REX_HOOK_RAW(sub_82198C20) {
  // Retain the native loading UI and graphics setup. The logo animation is
  // drawn independently by the animation registry, below.
  __imp__sub_82198C20(ctx, base);
}

REX_HOOK_RAW(sub_820F4C50) {
  // The native animation registry stores {handle, object} at node +12/+16.
  // Match only DATA INITIALIZE's animation; never suppress shared UI draws,
  // animation updates, resource loading, palette setup or cleanup.
  if(boot_object && skip_requested) {
    const uint32_t handle=REX_LOAD_U32(boot_object+84);
    const uint32_t head=REX_LOAD_U32(0x824CBAC8);
    if(handle && head>=0x80000000 && head<0xf0000000) {
      uint32_t node=REX_LOAD_U32(head);
      for(unsigned n=0;node!=head && n<1024;++n) {
        if(node<0x80000000 || node>0xefffffe0)break;
        if(REX_LOAD_U32(node+12)==handle && REX_LOAD_U32(node+16)==ctx.r3.u32) {
          static bool reported=false;
          if(!reported){reported=true;REXLOG_INFO("VF2 startup logos: suppressed registered branding animation; native initialization retained");}
          return;
        }
        node=REX_LOAD_U32(node);
      }
    }
  }
  __imp__sub_820F4C50(ctx,base);
}

REX_HOOK_RAW(sub_82179B50) {
  // Suppress only the boot animation's SEGA voice, not sound-bank loading or
  // any gameplay/menu audio that uses the same native playback entry.
  if(boot_object && skip_requested && uint32_t(ctx.lr)==0x821992F8) {ctx.r3.u64=0;return;}
  __imp__sub_82179B50(ctx,base);
}

REX_HOOK_RAW(sub_82198BC8) {
  const uint32_t object = ctx.r3.u32;
  __imp__sub_82198BC8(ctx, base);
  if (object == boot_object) {
    boot_object = 0;
    skip_requested = false;
    previous_buttons = 0;
  }
}

// TaskWarning is exclusively the startup save-icon explanation (string 747:
// "When this icon is displayed, the console is saving content..."). Save
// creation, profile selection and errors use separate native dialog tasks.
// Leave this task unregistered; its original wait/cleanup states complete
// normally without touching any actual save operation.
REX_HOOK_RAW(sub_821995B8) {
  REXLOG_INFO("VF2 startup: skipped save-icon information (native string 747)");
  ctx.r3.u64=1;
}
