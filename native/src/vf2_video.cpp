#include "generated/default/vf2_pch.h"
#include "generated/default/vf2_funcs.h"
#include <rex/hook.h>
#include <rex/logging.h>
#include <mutex>
#include <set>
#include <unordered_map>
#include <array>
#include <rex/cvar.h>
#include <rex/memory.h>
#include <cmath>
#include <cstdlib>
#include "vf2_panorama.h"
#include "vf2_fight.h"
#include "vf2_display.h"

REXCVAR_DEFINE_STRING(vf2_output_resolution,"1920x1080","VF2",
    "Output and GPU rendering resolution; select in VF2 > Display")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);
REXCVAR_DEFINE_STRING(vf2_display_device,"","VF2","Windows display selected in the launcher")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);




#include "vf2_video_mode.inc"

extern "C" REX_FUNC(__imp__sub_820FDC50);
extern "C" REX_FUNC(__imp__sub_820FBDA0);
extern "C" REX_FUNC(__imp__sub_821538A8);

extern "C" REX_FUNC(__imp__sub_821067A0);


namespace {
std::mutex trace_mutex;
std::set<std::array<int32_t, 5>> viewport_seen;
std::set<std::array<float, 4>> projection_seen;
}

REX_HOOK_RAW(sub_821067A0) {
  __imp__sub_821067A0(ctx, base);
  // The display block holds the "screen" (+0..31) the sprite/AET layer is
  // projected onto and the "logical" canvas (+32..63) that 2D layout uses.
  // Natively both are the 1080-line mode 13 whatever the backbuffer. Keep
  // them as a virtual 1080-line screen with the output aspect (1920x1080 at
  // 16:9, 2560x1080 at 21:9); only the render targets (sub_82157398) get the
  // physical guest size. Writing the physical size here made a 720- or
  // 810-line backbuffer show the 1080-line layout 1:1 (cropped/zoomed).
  constexpr uint32_t display = 0x824A0498;
  const uint32_t width = std::stoul(rex::cvar::GetFlagByName("video_mode_width"));
  const uint32_t height = std::stoul(rex::cvar::GetFlagByName("video_mode_height"));
  PPCRegister aspect{};
  aspect.f64 = double(width) / height;
  const uint32_t virtual_width=uint32_t(std::lround(1080*aspect.f64));
  REX_STORE_U32(display+4,virtual_width);REX_STORE_U32(display+8,1080);
  REX_STORE_U64(display+16,aspect.u64);
  REX_STORE_U32(display+36,virtual_width);REX_STORE_U32(display+40,1080);
  REX_STORE_U64(display+48,aspect.u64);
  REXLOG_INFO("VF2 physical display {}x{}, virtual screen {}x1080", width, height, virtual_width);
}

REX_HOOK_RAW(sub_821538A8) {
  const uint32_t output = ctx.r3.u32;
  const uint32_t camera = ctx.r4.u32;
  const bool perspective = REX_LOAD_U8(camera + 189) != 0;
  PPCRegister width{}, height{};
  width.u32 = REX_LOAD_U32(camera + 260);
  height.u32 = REX_LOAD_U32(camera + 264);
  const auto caller = uint32_t(ctx.lr);
  __imp__sub_821538A8(ctx, base);
  if (perspective && std::abs(width.f32 / height.f32 - 496.0f / 384.0f) < 0.01f) {
    // The builder transposes its matrix before returning: its first row
    // produces clip X. Scale that row while keeping clip Y/Z/W intact.
    const double aspect = std::stod(rex::cvar::GetFlagByName("video_mode_width")) /
                          std::stod(rex::cvar::GetFlagByName("video_mode_height"));
    const float correction = float((width.f32 / height.f32) / aspect);
    PPCRegister original_scale{};
    original_scale.u32 = REX_LOAD_U32(output);
    for (uint32_t offset : {0u, 4u, 8u, 12u}) {
      PPCRegister value{};
      value.u32 = REX_LOAD_U32(output + offset);
      value.f32 *= correction;
      REX_STORE_U32(output + offset, value.u32);
    }
#ifdef VF2_PANORAMA_RENDERER
    vf2::panorama::SetProjection(original_scale.f32, original_scale.f32 * correction);
#endif
    static bool logged = false;
    if (!logged) {
      logged = true;
      REXLOG_INFO("VF2 perspective corrected: camera {}x{}, X factor={}, caller={:08X}",
                  width.f32, height.f32, correction, caller);
    }
  } else {
    static int logged = 0;
    if (logged++ < 5) REXLOG_INFO("VF2 camera: perspective={}, size={}x{}, caller={:08X}",
                                perspective, width.f32, height.f32, caller);
  }
}

REX_HOOK_RAW(sub_820FDC50) {
  // The native screen-size rectangle also applies to demo/intro and ordinary
  // Arcade. Its final 3D viewport must match the final bitmap composition.
  // Keep the auxiliary square/496x384 targets intact.
  if(uint32_t(ctx.lr)==0x82151B10 && ctx.r6.s32>=480 &&
      std::abs(double(ctx.r5.s32)/ctx.r6.s32-496.0/384.0)<0.01) {
    ctx.r3.u64=0;ctx.r4.u64=0;
    ctx.r5.u64=std::stoul(rex::cvar::GetFlagByName("video_mode_width"));
    ctx.r6.u64=std::stoul(rex::cvar::GetFlagByName("video_mode_height"));
  }
  // Passes that size their viewport from the virtual screen draw into the
  // physical backbuffer.
  if(uint32_t(ctx.lr)==0x82151B10 && ctx.r5.u32==REX_LOAD_U32(0x824A0498+4) && ctx.r6.u32==1080) {
    ctx.r5.u64=std::stoul(rex::cvar::GetFlagByName("video_mode_width"));
    ctx.r6.u64=std::stoul(rex::cvar::GetFlagByName("video_mode_height"));
  }
  std::array<int32_t, 5> viewport{ctx.r3.s32, ctx.r4.s32, ctx.r5.s32, ctx.r6.s32, int32_t(ctx.lr)};
  {
    std::lock_guard lock(trace_mutex);
    if (viewport_seen.size() < 40 && viewport_seen.insert(viewport).second)
      REXLOG_INFO("VF2 video viewport: x={}, y={}, width={}, height={} caller={:08X}",
                  viewport[0], viewport[1], viewport[2], viewport[3], uint32_t(viewport[4]));
  }
  __imp__sub_820FDC50(ctx, base);
}

REX_HOOK_RAW(sub_820FBDA0) {
  std::array<float, 4> projection{float(ctx.f1.f64), float(ctx.f2.f64), float(ctx.f3.f64), float(ctx.f4.f64)};
  {
    std::lock_guard lock(trace_mutex);
    if (projection_seen.size() < 40 && projection_seen.insert(projection).second)
      REXLOG_INFO("VF2 video ortho: left={}, right={}, bottom={}, top={} caller={:08X}",
                  projection[0], projection[1], projection[2], projection[3], uint32_t(ctx.lr));
  }
  __imp__sub_820FBDA0(ctx, base);
}


extern "C" REX_FUNC(__imp__sub_82157398);
REX_HOOK_RAW(sub_82157398) {
  // Render-pass setup (screen r4/r5, canvas r6/r7, from the display block):
  // its targets and viewports are physical, the guest backbuffer size. A
  // virtual 2560x1080 canvas would overflow the EDRAM.
  ctx.r4.u64=ctx.r6.u64=std::stoul(rex::cvar::GetFlagByName("video_mode_width"));
  ctx.r5.u64=ctx.r7.u64=std::stoul(rex::cvar::GetFlagByName("video_mode_height"));
  __imp__sub_82157398(ctx,base);
}



extern "C" REX_FUNC(__imp__sub_82108B80);
extern "C" REX_FUNC(__imp__sub_820FBD08);
extern "C" REX_FUNC(__imp__sub_82181D78);
namespace {
// Ultrawide margins: the menus' full-screen background is 1920 wide. After the
// sprite lists are drawn, draw them once more per margin with the sprite
// camera narrowed to the layout's outermost 2 columns and the D3D scissor
// limited to that margin, so the margin continues the layout's edge
// (gradient, bottom bar) and everything else is clipped.
constexpr double kMarginEdge=2;
int margin_side=-1;            // camera override while drawing a margin
uint32_t sprite_camera=0;      // last sub_82108B80 camera (inputs at +648)
// Sprite lists drawn by sub_82181D78(set): set s, list i (32 per set) at
// kSpriteLists + s*512 + i*16 = {begin, end} of entities; vtable[1] draws.
constexpr uint32_t kSpriteLists=0x82E7C9B0;
void Scissor(PPCContext& ctx,uint8_t* base,uint32_t device,uint32_t enable,const uint32_t rect_values[4]) {
  // D3D device: scissor test enable +12288, RECT +13052; sub_821B8BD0
  // (SetScissorRect) programs the window scissor as viewport ∩ RECT.
  static uint32_t rect=0;
  if(!rect)rect=REX_KERNEL_MEMORY()->SystemHeapAlloc(32);
  REX_STORE_U32(device+12288,enable);
  for(int i=0;i<4;++i)REX_STORE_U32(rect+i*4,rect_values[i]);
  rex::CallFrame call(ctx);call.ctx.r3.u64=device;call.ctx.r4.u64=rect;sub_821B8BD0(call.ctx,base);
}
void DrawEntity(PPCContext& ctx,uint8_t* base,uint32_t entity) {
  ctx.r3.u64=entity;
  REX_CALL_INDIRECT_FUNC(REX_LOAD_U32(REX_LOAD_U32(entity)+4));
}
void DrawLists(PPCContext& ctx,uint8_t* base,uint32_t set) {
  for(uint32_t l=0;l<32;++l) {
    const uint32_t list=kSpriteLists+set*512+l*16;
    for(uint32_t p=REX_LOAD_U32(list);p!=REX_LOAD_U32(list+4);p+=4)DrawEntity(ctx,base,REX_LOAD_U32(p));
  }
}
void RecomputeCamera(PPCContext& ctx,uint8_t* base) {
  rex::CallFrame call(ctx);call.ctx.r3.u64=sprite_camera;call.ctx.r4.u64=sprite_camera+648;
  sub_82108B80(call.ctx,base);
}
}
REX_HOOK_RAW(sub_82108B80) {
  sprite_camera=ctx.r3.u32;
  __imp__sub_82108B80(ctx,base);
}
REX_HOOK_RAW(sub_820FBD08) {
  // Frustum(l f1, r f2, b f3, t f4, n f5, f f6) of the sprite cameras: a
  // margin camera spans only the edge columns.
  if(margin_side>=0) {
    const double k=kMarginEdge/REX_LOAD_U32(0x824A0498+4);
    ctx.f1.f64*=k;ctx.f2.f64*=k;
  }
  __imp__sub_820FBD08(ctx,base);
}
REX_HOOK_RAW(sub_82181D78) {
  const uint32_t set=ctx.r3.u32;
  __imp__sub_82181D78(ctx,base);
  const double screen=REX_LOAD_U32(0x824A0498+4);
  const uint32_t device=REX_LOAD_U32(0x824CBD3C);
  if(screen<=1920.5 || set>1 || !device || !sprite_camera)return;
  const double margin=-vf2::display::UiLeft(screen);
  const uint32_t guest_w=std::stoul(rex::cvar::GetFlagByName("video_mode_width"));
  const uint32_t guest_h=std::stoul(rex::cvar::GetFlagByName("video_mode_height"));
  const uint32_t margin_px=uint32_t(std::lround(margin*guest_w/screen));
  rex::CallFrame call(ctx);
  const uint32_t saved_enable=REX_LOAD_U32(device+12288);
  uint32_t saved_rect[4];for(int i=0;i<4;++i)saved_rect[i]=REX_LOAD_U32(device+13052+i*4);
  for(int side=0;side<2;++side) {
    margin_side=side;
    RecomputeCamera(call.ctx,base);
    const uint32_t r[4]={side==0?0:guest_w-margin_px,0,side==0?margin_px:guest_w,guest_h};
    Scissor(call.ctx,base,device,1,r);
    sub_821814F8(call.ctx,base);
    call.ctx.r3.u64=378;sub_820FB8E8(call.ctx,base);
    sub_820FB9A8(call.ctx,base);
    DrawLists(call.ctx,base,set);
  }
  margin_side=-1;
  RecomputeCamera(call.ctx,base);
  Scissor(call.ctx,base,device,saved_enable,saved_rect);
  sub_821814F8(call.ctx,base);
}

extern "C" REX_FUNC(__imp__sub_82103A38);
REX_HOOK_RAW(sub_82103A38) {
  // lookAt(eye f1-f3, target f4-f6, up f7-f9) of the sprite/AET layer camera
  // (sub_82108960/sub_82108A68, its only callers) looks at the screen centre.
  // Menus and HUD are laid out on the 1920-wide mode 13, so on a wider
  // virtual screen look at the layout centre: they stay centred and the
  // extra width becomes margins (vf2::display::UiLeft).
  const double half=REX_LOAD_U32(0x824A0498+4)*0.5;
  if(half>960.0 && std::abs(ctx.f1.f64-half)<0.5 && std::abs(ctx.f4.f64-half)<0.5)
    ctx.f1.f64=ctx.f4.f64=margin_side<0 ? 960.0 : margin_side==0 ? kMarginEdge/2 : 1920.0-kMarginEdge/2;
  __imp__sub_82103A38(ctx,base);
}
