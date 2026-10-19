#include "generated/default/vf2_pch.h"
#include "generated/default/vf2_funcs.h"
#include "vf2_menu.h"
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <atomic>
#include <chrono>

extern "C" REX_FUNC(__imp__sub_82182000);
extern "C" REX_FUNC(__imp__sub_82115E38);
extern "C" REX_FUNC(__imp__sub_82125988);

// Help & Options > Settings (Main Menu and the pause menu share the task):
// the Screen Size row has no effect in this port, so it becomes "Advanced
// Settings" (strings renamed in BrandStrings) and A on it opens the F6
// settings. Draw sub_82115E38 lists fixed rows (451 Music, 429 Sound
// Effects, 476 Screen Size, 480 Brightness, 473 Autosave); task +76 is the
// selected row, +452 + row*4 its value, whose string is label + value + 1.
namespace vf2::menu {
namespace {
constexpr uint32_t kAdvancedRow = 2;
std::atomic<uint32_t> settings_task{0};
std::atomic<int64_t> settings_seen_ms{-1000};
std::atomic<bool> open_request{false};
uint32_t empty_text = 0;

int64_t NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}
bool AdvancedRowSelected(uint8_t* base) {
  if (NowMs() - settings_seen_ms.load() > 100) return false;  // menu not drawn
  const uint32_t task = settings_task.load();
  return task && REX_LOAD_U32(task + 76) == kAdvancedRow;
}
}  // namespace

void FilterInput(uint8_t* base, unsigned user, uint32_t state) {
  if (user > 1 || !state) return;
  static uint16_t previous[2] = {};
  const uint16_t buttons = REX_LOAD_U16(state + 4);
  const uint16_t before = previous[user];
  previous[user] = buttons;
  if (!AdvancedRowSelected(base)) return;
  if ((buttons & 0x1000) && !(before & 0x1000) && !open_request.exchange(true))
    REXLOG_INFO("VF2 menu: Advanced Settings opens the F6 settings");
  // The native row would change its (hidden) value: no A, Left/Right or
  // horizontal left stick reach it.
  REX_STORE_U16(state + 4, uint16_t(buttons & ~(0x1000 | 0x0004 | 0x0008)));
  REX_STORE_U16(state + 8, 0);
}

bool TakeOpenSettingsRequest() { return open_request.exchange(false); }
}  // namespace vf2::menu

REX_HOOK_RAW(sub_82115E38) {
  vf2::menu::settings_task.store(ctx.r3.u32);
  vf2::menu::settings_seen_ms.store(vf2::menu::NowMs());
  __imp__sub_82115E38(ctx, base);
}

// String lookup: the Screen Size values (477..479, drawn at 0x821160FC) are
// shown empty.
REX_HOOK_RAW(sub_82182000) {
  const uint32_t id = ctx.r3.u32, caller = uint32_t(ctx.lr);
  __imp__sub_82182000(ctx, base);
  if (caller != 0x821160FC || id < 477 || id > 479) return;
  if (!vf2::menu::empty_text) {
    vf2::menu::empty_text = REX_KERNEL_MEMORY()->SystemHeapAlloc(4);
    if (!vf2::menu::empty_text) return;
    REX_STORE_U32(vf2::menu::empty_text, 0);
  }
  ctx.r3.u64 = vf2::menu::empty_text;
}

// Value arrows around the selected row (task +624, shown from 0x82116250):
// hidden on Advanced Settings, as the draw does for rows without a value.
REX_HOOK_RAW(sub_82125988) {
  if (uint32_t(ctx.lr) == 0x82116250 && vf2::menu::AdvancedRowSelected(base)) {
    sub_82124938(ctx, base);
    return;
  }
  __imp__sub_82125988(ctx, base);
}
