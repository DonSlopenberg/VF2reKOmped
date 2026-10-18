#pragma once
#include <cstdint>
#include <span>
struct PPCContext;
namespace vf2::demo {
bool Enabled();
bool Before();
bool AllowUpdate(uint8_t*);
bool Host(PPCContext&,uint8_t*,bool boot_complete,std::span<const uint32_t> menus);
void NativeUpdate(uint8_t*);
}
