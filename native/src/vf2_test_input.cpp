#include "generated/default/vf2_pch.h"
#include <rex/hook.h>
#include <rex/logging.h>
#include <windows.h>
#include <cstdlib>
#include <array>
#include <fstream>
#include <atomic>

extern "C" REX_FUNC(__imp__sub_821A9CB0);

// Explicit local test mode only. Physical XInput input is always read first.
namespace vf2::fight { bool InjectStart(); }
namespace vf2::arcade { bool DuralConfirm(unsigned user, bool start_held); void CostumeInput(unsigned user, uint16_t buttons); }
namespace vf2::input {
std::atomic<uint16_t> last_buttons[2];
bool StartHeld(unsigned user) { return user < 2 && (last_buttons[user].load() & 0x10); }
}
// Applied after every input source (pad, keyboard, test script).
static void InjectStartupStart(uint8_t* base, uint32_t user, uint32_t state) {
  if (user < 2 && state) {
    const uint16_t buttons = REX_LOAD_U16(state + 4);
    vf2::input::last_buttons[user].store(buttons);
    vf2::arcade::CostumeInput(user, buttons);
    if (vf2::arcade::DuralConfirm(user, buttons & 0x10)) {
      REX_STORE_U16(state + 4, uint16_t(buttons | 0x1000));
      REX_STORE_U32(state, REX_LOAD_U32(state) + 1);
    }
  }
  if (user != 0 || !state || !vf2::fight::InjectStart()) return;
  REX_STORE_U16(state + 4, REX_LOAD_U16(state + 4) | 0x10);
  static uint32_t packet = 0x30000000;
  REX_STORE_U32(state, ++packet);
}
REX_HOOK_RAW(sub_821A9CB0) {
  const uint32_t user = ctx.r3.u32, state = ctx.r4.u32;
  struct Inject { uint8_t* base; uint32_t user, state; PPCContext& ctx;
    ~Inject(){ if(ctx.r3.u32==0) InjectStartupStart(base,user,state); } } inject{base,user,state,ctx};
  __imp__sub_821A9CB0(ctx, base);
  if(std::getenv("VF2_DEMO_COMMAND") && user<2 && state) {
    std::memset(base+state,0,16);ctx.r3.u64=0;return;
  }
  // Explicit test-only input script. Allows reproducible renderer scenarios
  // without requiring a foreground desktop or changing controller bindings.
  if (const char* script=std::getenv("VF2_AUTOMATED_INPUT"); script && user<2 && state) {
    unsigned buttons=0,second=0;
    std::ifstream file(script); file >> std::hex >> buttons;
    if(user==1) {if(!(file>>std::hex>>second))return;buttons=second;}
    static unsigned previous[2]={~0u,~0u};
    if(previous[user]!=buttons) {
      previous[user]=buttons;
      REXLOG_INFO("VF2 synthetic test controller {} buttons={:04X}",user,buttons);
    }
    std::memset(base+state,0,16);
    static uint32_t serial[2]={0x20000000,0x20000000};
    REX_STORE_U32(state,++serial[user]);
    REX_STORE_U16(state+4,uint16_t(buttons));
    ctx.r3.u64=0;
    return;
  }
  static bool enabled = std::getenv("VF2_TEST_KEYS") != nullptr;
  if (!enabled || user != 0 || !state) return;
  DWORD foreground = 0;
  GetWindowThreadProcessId(GetForegroundWindow(), &foreground);
  if (foreground != GetCurrentProcessId()) return;
  struct Key { int vk; uint16_t button; ULONGLONG until; };
  static std::array<Key, 7> keys{{
      {VK_RETURN, 0x0010, 0}, {VK_SPACE, 0x1000, 0}, {VK_BACK, 0x2000, 0},
      {VK_LEFT, 0x0004, 0}, {VK_RIGHT, 0x0008, 0},
      {VK_UP, 0x0001, 0}, {VK_DOWN, 0x0002, 0}}};
  const auto now = GetTickCount64();
  uint16_t buttons = 0;
  for (auto& key : keys) {
    const SHORT input = GetAsyncKeyState(key.vk);
    if ((input & 0x8001) != 0) key.until = now + 120;
    if (now < key.until) buttons |= key.button;
  }
  if (!buttons) return;
  if (ctx.r3.u32 != 0) std::memset(base + state, 0, 16);
  REX_STORE_U16(state + 4, REX_LOAD_U16(state + 4) | buttons);
  static uint32_t packet = 0x10000000;
  REX_STORE_U32(state, ++packet);
  ctx.r3.u64 = 0;
}
