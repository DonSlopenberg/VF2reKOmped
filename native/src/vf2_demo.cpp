#include "generated/default/vf2_pch.h"
#include "generated/default/vf2_funcs.h"
#include "vf2_demo.h"
#include <rex/logging.h>
#include <rex/hook.h>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <atomic>

namespace vf2::demo {
namespace {
bool ready=false,requested=true;
bool have_index=false;
uint8_t original_index=0;
unsigned restarts=0;
std::atomic_bool& LegacyMode(){static std::atomic_bool mode{std::getenv("VF2_DEMO_BEFORE")!=nullptr};return mode;}
}
bool Enabled(){return std::getenv("VF2_DEMO_COMMAND")!=nullptr;}
bool Before(){return Enabled() && LegacyMode().load();}
bool AllowUpdate(uint8_t* base) {
  if(!Enabled())return true;
  if(const char* stop=std::getenv("VF2_DEMO_STOP_TICK"))
    return REX_LOAD_U32(0x82E7E38C)<unsigned(std::atoi(stop));
  return true;
}
bool Host(PPCContext& ctx,uint8_t* base,bool boot_complete,std::span<const uint32_t> menus) {
  const char* path=std::getenv("VF2_DEMO_COMMAND");
  if(!path)return false;
  static std::filesystem::file_time_type last{};
  std::error_code error;
  const auto time=std::filesystem::last_write_time(path,error);
  if(!error && time!=last) {
    std::ifstream input(path);std::string command;input>>command;
    if(command=="restart" || command=="run")requested=true;
    if(command=="before" || command=="after") {
      LegacyMode().store(command=="before");requested=true;
      REXLOG_INFO("VF2 demo test: live comparison variant={}",command);
    }
    last=time;
  }
  if(!boot_complete)return true;
  if(!menus.empty()) {
    for(auto menu:menus){rex::CallFrame call(ctx);call.ctx.r3.u64=menu;sub_82197D10(call.ctx,base);}
    return true;
  }
  constexpr uint32_t scene=0x82BB7BF8;
  if(REX_LOAD_U32(scene)!=1) {
    rex::CallFrame call(ctx);call.ctx.r3.u64=1;sub_8214B9D8(call.ctx,base);
    call.ctx.r3.u64=6;sub_8214B9F8(call.ctx,base);return true;
  }
  if(REX_LOAD_U32(scene+12)!=1 || REX_LOAD_U32(scene+28)!=6)return true;
  constexpr uint32_t m2e=0x834658F8;
  if(REX_LOAD_U32(m2e+16)==3 && REX_LOAD_U32(m2e+20)!=5) {
    rex::CallFrame call(ctx);call.ctx.r3.u64=0;sub_82199420(call.ctx,base);
  }
  ready=true;
  return true;
}
void NativeUpdate(uint8_t* base) {
  if(!Enabled() || !ready || !requested)return;
  const uint32_t memory=REX_LOAD_U32(0x833BD428);
  if(!memory)return;
  auto* ram=base+memory;
  if(!have_index){original_index=ram[0x50008f];have_index=true;}
  // Verified ROM dispatch table 0xA6F8: mode 2 -> original demo initializer
  // 0xAB0C. It sets up the original 0x43FD0 scripted title/demo and schedules
  // camera, stage, actors and HUD itself. No custom fight or input playback.
  ram[0x50008f]=original_index;
  ram[0x50002a]=2;
  requested=false;
  REXLOG_INFO("VF2 demo test: original title/demo initialization {} at native dispatcher; index={}",++restarts,original_index);
}
}
