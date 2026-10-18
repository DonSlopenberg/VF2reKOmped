#pragma once
#include <cstdint>
// Diagnostic free-look for the distant 2D background: yaw/pitch offsets
// applied by the panorama renderer (3D view unchanged for now).
namespace vf2::freelook {
struct Offsets { bool enabled=false; float yaw=0, pitch=0; };  // degrees
Offsets Get();
void Set(const Offsets&);
void Apply(uint8_t* base);    // i960 PC 0x24534, before the scroll routine reads the camera
void Restore(uint8_t* base);  // i960 PC 0xA6C0, before the next update's camera work
}
