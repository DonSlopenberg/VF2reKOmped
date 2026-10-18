#include "generated/default/vf2_pch.h"
#include "generated/default/vf2_funcs.h"
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/cvar.h>
#include <cstdlib>
#include <array>
#include <set>
#include <cmath>
#include <chrono>
#include <cstring>

extern "C" REX_FUNC(__imp__sub_82145610);
extern "C" REX_FUNC(__imp__sub_821441D8);
extern "C" REX_FUNC(__imp__sub_821448F0);
extern "C" REX_FUNC(__imp__sub_82144050);
extern "C" REX_FUNC(__imp__sub_82145A90);
extern "C" REX_FUNC(__imp__sub_82188848);
extern "C" REX_FUNC(__imp__sub_82144548);
extern "C" REX_FUNC(__imp__sub_821445B0);
extern "C" REX_FUNC(__imp__sub_820FE650);
extern "C" REX_FUNC(__imp__sub_821038F8);
extern "C" REX_FUNC(__imp__sub_820F7D90);
extern "C" REX_FUNC(__imp__sub_820F7E00);
extern "C" REX_FUNC(__imp__sub_820F8508);

REXCVAR_DEFINE_BOOL(vf2_arena_mirror_horizon, false, "Video", "Mirror arena horizons (restart required)");
REXCVAR_DEFINE_BOOL(vf2_arena_smooth_scroll, true, "Video", "Smooth scrolling arena horizons");

namespace {
thread_local int hardware_layer = -1;
thread_local int scroll_mode = 0;
thread_local bool clearing_hud = false;
constexpr std::array<uint32_t, 2> canvases{0x82BB6D60, 0x82BB6DB8};
constexpr uint32_t descriptor_size = 88;
uint32_t background_canvases = 0;
bool horizon_active = false;
int horizon_mode = 0;
std::array<double, 2> horizon_scroll{};
thread_local bool composite_source = false;
thread_local std::array<float, 4> composite_uv{};
struct RedirectState {
  bool active = false;
  uint32_t priority = 0;
  std::array<uint8_t, descriptor_size> original{};
} redirect;

bool WideEnabled() {
  static bool trial = std::getenv("VF2_ARENA_TRIAL") != nullptr;
  return trial || REXCVAR_GET(vf2_arena_mirror_horizon);
}
float LoadFloat(uint8_t* base, uint32_t address) {
  PPCRegister r{}; r.u32 = REX_LOAD_U32(address); return r.f32;
}
void StoreFloat(uint8_t* base, uint32_t address, float value) {
  PPCRegister r{}; r.f32 = value; REX_STORE_U32(address, r.u32);
}
void RestoreCanvas(uint8_t* base) {
  if (!redirect.active) return;
  std::memcpy(base + background_canvases + redirect.priority * 128,
              base + canvases[redirect.priority], descriptor_size);
  std::memcpy(base + canvases[redirect.priority], redirect.original.data(), descriptor_size);
  redirect.active = false;
}
void CreateBackgroundCanvases(PPCContext& ctx, uint8_t* base) {
  if (background_canvases) return;
  background_canvases = REX_KERNEL_MEMORY()->SystemHeapAlloc(256);
  if (!background_canvases) { REXLOG_ERROR("VF2 horizon canvas allocation failed"); return; }
  std::memset(base + background_canvases, 0, 256);
  for (uint32_t priority = 0; priority < 2; ++priority) {
    rex::CallFrame call(ctx);
    call.ctx.r3.u64 = background_canvases + priority * 128;
    call.ctx.r4.u64 = 1024; call.ctx.r5.u64 = 1024;
    __imp__sub_821448F0(call.ctx, base);
  }
  REXLOG_INFO("VF2 unscrolled source canvases created: 1024x1024, address={:08X}", background_canvases);
}
double SmoothAxis(int axis, double target, uint32_t layer) {
  if (WideEnabled() && layer == 1 && redirect.active) {
    horizon_scroll[axis] = target;
    return 0;
  }
  if (!WideEnabled() || layer != 1 || !REXCVAR_GET(vf2_arena_smooth_scroll)) return target;
  const double period = ((scroll_mode == 1 && axis == 1) || (scroll_mode == 2 && axis == 0)) ? 1024 : 512;
  struct State { double position{}, result{}, period{}; bool initialized{};
    std::chrono::steady_clock::time_point time{}; };
  static std::array<State, 2> states;
  auto& state = states[axis];
  const auto now = std::chrono::steady_clock::now();
  const double dt = std::chrono::duration<double>(now - state.time).count();
  if (state.initialized && state.period == period && dt < 0.004)
    return target + std::remainder(state.result - target, period);
  const double delta = std::remainder(target - state.position, period);
  if (!state.initialized || state.period != period || dt > 0.15 || std::abs(delta) > 48)
    state.position = target;
  else state.position += delta * (1.0 - std::exp(-dt / 0.012));
  state.result = target + std::remainder(state.position - target, period);
  state.time = now; state.period = period; state.initialized = true;
  return state.result;
}
struct LayerScope {
  int previous, previous_mode;
  explicit LayerScope(int layer, int mode) : previous(hardware_layer), previous_mode(scroll_mode) {
    hardware_layer = layer; scroll_mode = mode;
    static std::set<std::array<int, 2>> logged;
    if (logged.insert({layer, mode}).second)
      REXLOG_INFO("VF2 hardware bitmap source: layer={}, scroll mode={}", layer, mode);
  }
  ~LayerScope() { hardware_layer = previous; scroll_mode = previous_mode; }
};
int ProbeLayer() {
  static int layer = [] { const char* value = std::getenv("VF2_LAYER_PROBE");
    return value ? std::atoi(value) : -1; }();
  return layer;
}
using CompositeFunction = void (*)(PPCContext&, uint8_t*);
void CompositeHorizon(PPCContext& ctx, uint8_t* base, uint32_t priority, CompositeFunction original) {
  const uint32_t uv = ctx.r3.u32, first = ctx.r4.u32, last = ctx.r5.u32;
  const float left = LoadFloat(base, first), right = LoadFloat(base, last);
  const float height = std::stof(rex::cvar::GetFlagByName("video_mode_height"));
  const bool composite = WideEnabled() && horizon_active && background_canvases &&
      LoadFloat(base, uv) == 496 && LoadFloat(base, uv + 4) == 384 && right > left &&
      std::abs(std::abs(LoadFloat(base, last + 4) - LoadFloat(base, first + 4)) - height) < 2;
  if (composite) {
    std::array<uint8_t, descriptor_size> hud;
    std::memcpy(hud.data(), base + canvases[priority], descriptor_size);
    std::memcpy(base + canvases[priority], base + background_canvases + priority * 128, descriptor_size);
    // Sample the complete, unscrolled source. Every segment shares the same
    // camera phase; splits occur at source mirror folds, never viewport edges.
    const double period_x = horizon_mode == 2 ? 1024.0 : 512.0;
    const double period_y = horizon_mode == 1 ? 1024.0 : 512.0;
    const double logical_width = 384.0 * 16.0 / 9.0;
    const double start_x = -horizon_scroll[0] - (logical_width - 496.0) / 2;
    const double start_y = -horizon_scroll[1];
    const float top = LoadFloat(base, first + 4), bottom = LoadFloat(base, last + 4);
    auto mirror = [](double value, double period) {
      double phase = std::fmod(value, 2 * period);
      if (phase < 0) phase += 2 * period;
      return float(phase <= period ? phase : 2 * period - phase);
    };
    for (double y = 0; y < 384.0 - 0.001;) {
      const double source_y = start_y + y;
      const double row = std::floor(source_y / period_y);
      const double dy = std::min(384.0 - y, (row + 1) * period_y - source_y);
      for (double x = 0; x < logical_width - 0.001;) {
        const double source_x = start_x + x;
        const double fold = std::floor(source_x / period_x);
        const double dx = std::min(logical_width - x, (fold + 1) * period_x - source_x);
        composite_uv = {mirror(source_x, period_x), float(source_y - row * period_y),
                        mirror(source_x + dx, period_x), float(source_y + dy - row * period_y)};
        StoreFloat(base, first, float(x / logical_width * height * 16.0 / 9.0));
        StoreFloat(base, last, float((x + dx) / logical_width * height * 16.0 / 9.0));
        StoreFloat(base, first + 4, top + float(y / 384.0) * (bottom - top));
        StoreFloat(base, last + 4, top + float((y + dy) / 384.0) * (bottom - top));
        composite_source = true;
        rex::CallFrame call(ctx);
        call.ctx.r3.u64 = uv; call.ctx.r4.u64 = first; call.ctx.r5.u64 = last;
        original(call.ctx, base);
        composite_source = false;
        x += dx;
      }
      y += dy;
    }
    StoreFloat(base, first + 4, top); StoreFloat(base, last + 4, bottom);
    StoreFloat(base, first, left); StoreFloat(base, last, right);
    std::memcpy(base + canvases[priority], hud.data(), descriptor_size);
    static bool logged = false;
    if (!logged) { logged = true; REXLOG_INFO("VF2 continuous source-space mirror tiling; common camera phase, separate HUD"); }
  }
  original(ctx, base);
}
}

REX_HOOK_RAW(sub_82145610) {
  LayerScope scope(ctx.r3.u32, 0);
  __imp__sub_82145610(ctx, base);
}
#include "vf2_arena_scroll.inc"
REX_HOOK_RAW(sub_821441D8) {
  if (hardware_layer >= 0 && ProbeLayer() >= 0 && hardware_layer != ProbeLayer()) return;
  if (hardware_layer == 1 && redirect.active) {
    const uint32_t vertices = ctx.r3.u32;
    rex::CallFrame call(ctx);
    call.ctx.r3.u64=3; call.ctx.r4.u64=353; call.ctx.r5.u64=0; call.ctx.r6.u64=vertices;
    __imp__sub_820F7D90(call.ctx, base);
    call.ctx.r3.u64=2; call.ctx.r4.u64=353; call.ctx.r5.u64=0; call.ctx.r6.u64=vertices+48;
    __imp__sub_820F7E00(call.ctx, base);
    call.ctx.r3.u64=9; call.ctx.r4.u64=0; call.ctx.r5.u64=4;
    __imp__sub_820F8508(call.ctx, base);
    return;
  }
  __imp__sub_821441D8(ctx, base);
}
REX_HOOK_RAW(sub_821448F0) {
  const uint32_t canvas = ctx.r3.u32;
  __imp__sub_821448F0(ctx, base);
  // Allocate during startup, before the atlas sampler bindings are set for
  // a frame. Creating textures during the first horizon pass disturbed them.
  if (canvas == canvases[0] && WideEnabled()) CreateBackgroundCanvases(ctx, base);
}
REX_HOOK_RAW(sub_82145A90) {
  RestoreCanvas(base);
  horizon_active = false;
  __imp__sub_82145A90(ctx, base);
  RestoreCanvas(base);
}
REX_HOOK_RAW(sub_82144050) {
  RestoreCanvas(base);
  const uint32_t priority = ctx.r3.u32, layer = ctx.r5.u32;
  if (WideEnabled() && layer == 1 && priority < 2) {
    rex::CallFrame mode(ctx); mode.ctx.r3.u64 = 1;
    __imp__sub_82188848(mode.ctx, base);
    if (mode.ctx.r3.u32 == 1 || mode.ctx.r3.u32 == 2) {
      horizon_mode = mode.ctx.r3.u32;
      CreateBackgroundCanvases(ctx, base);
      if (background_canvases) {
        horizon_active = true;
        redirect.priority = priority; redirect.active = true;
        std::memcpy(redirect.original.data(), base + canvases[priority], descriptor_size);
        std::memcpy(base + canvases[priority], base + background_canvases + priority * 128, descriptor_size);
      }
    }
  }
  const bool hud = WideEnabled() && horizon_active && layer == 0;
  if (hud) ctx.r4.u64 = 0;
  clearing_hud = hud;
  __imp__sub_82144050(ctx, base);
  clearing_hud = false;
}
REX_HOOK_RAW(sub_820FE650) {
  if (clearing_hud) ctx.f4.f64 = 0;
  __imp__sub_820FE650(ctx, base);
}
REX_HOOK_RAW(sub_821038F8) {
  if (redirect.active) { ctx.f2.f64 = 1024; ctx.f3.f64 = 1024; }
  __imp__sub_821038F8(ctx, base);
}
REX_HOOK_RAW(sub_820F7E00) {
  if (composite_source && ctx.r3.u32 == 2 && uint32_t(ctx.lr) == 0x821444DC) {
    const uint32_t p = ctx.r6.u32;
    for (uint32_t v = 0; v < 4; ++v) {
      StoreFloat(base, p + v * 8, composite_uv[v < 2 ? 0 : 2]);
      StoreFloat(base, p + v * 8 + 4, composite_uv[v % 2 ? 3 : 1]);
    }
  }
  __imp__sub_820F7E00(ctx, base);
}
REX_HOOK_RAW(sub_82144548) {
  CompositeHorizon(ctx, base, 1, __imp__sub_82144548);
}
REX_HOOK_RAW(sub_821445B0) {
  CompositeHorizon(ctx, base, 0, __imp__sub_821445B0);
}
