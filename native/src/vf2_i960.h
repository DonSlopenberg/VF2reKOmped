#pragma once

#include "generated/default/vf2_pch.h"
#include <cstdint>
#include <span>

namespace vf2::i960 {
// Guest-thread only, between complete i960 instructions. Uses the shipped
// interpreter's CALL/RET implementation and leaves game data writes intact.
bool Call(PPCContext& context, uint8_t* base, uint32_t rom_pc,
          uint32_t max_steps = 2000000);
bool CallWithGlobals(PPCContext& context, uint8_t* base, uint32_t rom_pc,
                     std::span<const uint32_t> globals,
                     uint32_t max_steps = 2000000);
}
