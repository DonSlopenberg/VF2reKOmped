#include "generated/default/vf2_pch.h"
#include "generated/default/vf2_funcs.h"
#include <rex/hook.h>
#include <rex/logging.h>
#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <set>
#include <string>

extern "C" REX_FUNC(__imp__sub_82144618);
// Opt-in CPU-side asset capture. Never redirects render targets or GPU draws.
REX_HOOK_RAW(sub_82144618) {
  __imp__sub_82144618(ctx, base);
  static const char* output = std::getenv("VF2_EXPORT_BITMAPS");
  if (!output) return;
  static unsigned frame = 0;
  if (++frame % 120) return;
  const uint32_t engine = REX_LOAD_U32(0x82AB6CF8);
  if (!engine) return;
  static bool rom_saved = false;
  if (!rom_saved) {
    const uint32_t rom=REX_LOAD_U32(0x82AB6354);
    if (rom) {
      std::error_code ec; std::filesystem::create_directories(output,ec);
      rom_saved=true;
      for (unsigned i=0; i<7; ++i) {
        const uint32_t ptr=REX_LOAD_U32(0x82AB6348+i*4);
        const uint32_t name=REX_LOAD_U32(0x82051C5C+i*8);
        const uint32_t size=REX_LOAD_U32(0x82051C60+i*8);
        if (!ptr || !name || !size || size>0x1000000) continue;
        const auto filename=std::filesystem::path(reinterpret_cast<const char*>(base+name)).filename();
        std::ofstream raw(std::filesystem::path(output)/(filename.string()+".raw"),std::ios::binary);
        raw.write(reinterpret_cast<const char*>(base+ptr),size);
        rom_saved=rom_saved && raw.good();
      }
      REXLOG_INFO("VF2 data ROM export: address={:08X}, success={}",rom,rom_saved);
    }
  }
  uint64_t hash = 14695981039346656037ull;
  // Include pixels, tilemaps and palette; exclude per-frame scroll registers.
  for (uint32_t i=0; i<0x10C000; ++i) { hash ^= base[engine+i]; hash *= 1099511628211ull; }
  static std::set<uint64_t> captured;
  if (captured.size() >= 100 || !captured.insert(hash).second) return;
  std::error_code error;
  std::filesystem::create_directories(output, error);
  if (error) return;
  const auto stem = std::filesystem::path(output) / std::to_string(hash);
  std::ofstream file(stem.string()+".bin", std::ios::binary);
  file.write(reinterpret_cast<const char*>(base+engine), 0x118C00);
  std::ofstream meta(stem.string()+".txt");
  meta << "engine=" << std::hex << engine << "\nframe=" << std::dec << frame << "\n";
  for (unsigned layer=0; layer<4; ++layer) {
    rex::CallFrame call(ctx); call.ctx.r3.u64=layer; sub_82188848(call.ctx, base);
    meta << "layer" << layer << "_mode=" << call.ctx.r3.u32 << "\n";
  }
  REXLOG_INFO("VF2 asset snapshot: {}", stem.string());
}
