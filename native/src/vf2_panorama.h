#pragma once
#include <cstdint>
struct PPCContext;
namespace vf2::panorama {
void Begin(PPCContext&, uint8_t*);
void End(PPCContext&, uint8_t*);
bool Draw(PPCContext&, uint8_t*);
void SetProjection(double original_x_scale, double wide_x_scale);
void CaptureNativeScroll(PPCContext&, uint8_t*);
void CommitNativeScroll(PPCContext&, uint8_t*);
void CaptureComputedNativeScroll(PPCContext&, uint8_t*);
}
