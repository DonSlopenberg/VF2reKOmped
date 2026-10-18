#include "generated/default/vf2_pch.h"
#include "generated/default/vf2_funcs.h"
#include <rex/hook.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include "vf2_panorama.h"

extern "C" REX_FUNC(__imp__sub_82145A90);
REXCVAR_DEFINE_BOOL(vf2_background_smooth_scroll, true, "Video",
                   "Subpixel arena background scrolling (synchronized to the game camera)");

namespace {
struct LayerScope { LayerScope(int,int) {} };
double SmoothAxis(int, double target, uint32_t) {
  // An independently evolving low-pass filter drifts relative to the camera.
  return target;
}
}

REX_HOOK_RAW(sub_82145A90) {
  vf2::panorama::Begin(ctx, base);
  __imp__sub_82145A90(ctx, base);
  vf2::panorama::End(ctx, base);
}

// Retain the original scroll builders as the fallback for non-panorama screens.
// vf2_panorama.cpp extends arena tile windows and bitmap viewport composition.
#include "vf2_arena_scroll.inc"
