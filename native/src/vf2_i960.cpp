#include "vf2_i960.h"
#include "generated/default/vf2_funcs.h"
#include <rex/hook.h>
#include <rex/logging.h>
#include <array>
#include <cstring>

extern "C" REX_FUNC(__imp__sub_8212F838);

namespace vf2::i960 {
namespace {
// Verified in shipped PPC routines 8212F630/8212F838/8212F940/8212FDB0.
constexpr uint32_t kInterpreter = 0x82AB6718;
constexpr uint32_t kPc = kInterpreter + 56;
constexpr uint32_t kCpu = kInterpreter + 60;
constexpr uint32_t kCpuBytes = 300;  // r0..r31, FP state, AC, PC, IP.
thread_local bool active = false;
}

bool Call(PPCContext& context, uint8_t* base, uint32_t rom_pc,
          uint32_t max_steps) {
  return CallWithGlobals(context, base, rom_pc, {}, max_steps);
}

bool CallWithGlobals(PPCContext& context, uint8_t* base, uint32_t rom_pc,
                     std::span<const uint32_t> globals, uint32_t max_steps) {
  if (active || !base || globals.size() > 15 || !max_steps ||
      (rom_pc & 3) || rom_pc >= 0x80000) return false;
  const uint32_t cpu = REX_LOAD_U32(kCpu);
  const uint32_t program = REX_LOAD_U32(kInterpreter);
  const uint32_t return_pc = REX_LOAD_U32(kPc);
  if (!cpu || !program || return_pc < program ||
      return_pc - program >= 0x80000) return false;

  const uint32_t original_fp = REX_LOAD_U32(cpu + 124);
  const uint32_t original_sp = REX_LOAD_U32(cpu + 4);
  const uint32_t next_frame = (original_sp + 63u) & ~63u;
  // Native CALL spills 64 bytes at current FP and creates its next frame at
  // aligned SP. Both must be valid original Model2 work RAM, not ROM or I/O.
  if ((original_fp & ~63u) < 0x500000 ||
      (original_fp & ~63u) > 0x5FFFC0 ||
      original_sp < 0x500000 || original_sp > 0x5FFFC0 ||
      next_frame > 0x5FFFC0) {
    REXLOG_WARN("VF2 i960 call {:06X} rejected: FP={:08X}, SP={:08X}",
                rom_pc, original_fp, original_sp);
    return false;
  }

  std::array<uint8_t, kCpuBytes> saved_cpu;
  std::array<uint8_t, 64> saved_interpreter;
  std::array<uint8_t, 64> saved_caller_frame;
  std::memcpy(saved_cpu.data(), base + cpu, saved_cpu.size());
  std::memcpy(saved_interpreter.data(), base + kInterpreter,
              saved_interpreter.size());
  rex::CallFrame call(context);
  call.ctx.r3.u64 = original_fp & ~63u;
  sub_8212F3B8(call.ctx, base);
  const uint32_t frame = call.ctx.r3.u32;
  if (!frame) return false;
  std::memcpy(saved_caller_frame.data(), base + frame,
              saved_caller_frame.size());
  active = true;
  for (size_t index = 0; index < globals.size(); ++index)
    REX_STORE_U32(cpu + 64 + uint32_t(index) * 4, globals[index]);
  // Original CALL helper uses current PC as the saved return address. There
  // is no synthetic instruction and the sentinel is never executed.
  call.ctx.r3.u64 = rom_pc;
  sub_8212F940(call.ctx, base);
  REX_STORE_U32(cpu + 296, rom_pc);
  uint32_t steps = 0;
  bool returned = false;
  for (; steps < max_steps; ++steps) {
    const uint32_t pc = REX_LOAD_U32(kPc);
    if (pc == return_pc && REX_LOAD_U32(cpu + 124) == original_fp) {
      returned = true;
      break;
    }
    if (pc < program || pc - program >= 0x80000 || (pc & 3)) break;
    const uint32_t fp = REX_LOAD_U32(cpu + 124) & ~63u;
    const uint32_t sp = REX_LOAD_U32(cpu + 4);
    if (fp < 0x500000 || fp > 0x5FFFC0 ||
        sp < 0x500000 || sp > 0x600000) break;
    // The shipped stack starts near 0x5FF800, not below 0x5F0000.
    // Check every nested CALL before it can spill a new 64-byte frame.
    const uint8_t opcode = base[pc + 3];
    if ((opcode == 0x09 || opcode == 0x86) &&
        ((sp + 63u) & ~63u) > 0x5FFFC0) break;
    // Bypass the launch-request instruction hook while this synchronous
    // native call runs; only the outer original mode dispatcher consumes it.
    __imp__sub_8212F838(call.ctx, base);
  }
  const uint32_t stopped_pc = REX_LOAD_U32(kPc) - program;
  std::memcpy(base + frame, saved_caller_frame.data(),
              saved_caller_frame.size());
  std::memcpy(base + cpu, saved_cpu.data(), saved_cpu.size());
  std::memcpy(base + kInterpreter, saved_interpreter.data(),
              saved_interpreter.size());
  active = false;
  if (returned) {
    REXLOG_INFO("VF2 i960 native call {:06X} returned after {} instructions",
                rom_pc, steps);
  } else {
    REXLOG_ERROR("VF2 i960 native call {:06X} failed at {:06X} after {} instructions; CPU context restored",
                 rom_pc, stopped_pc, steps);
  }
  return returned;
}
}
