#include "generated/default/vf2_pch.h"
#include "generated/default/vf2_funcs.h"
#include "vf2_fight.h"
#include "vf2_i960.h"
#include "vf2_panorama.h"
#include "vf2_demo.h"
#include "vf2_interpolation.h"
#include "vf2_freelook.h"
#include "vf2_arcade.h"
#include <rex/hook.h>
#include <rex/logging.h>
#include <mutex>
#include <optional>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <vector>
#include <array>
#include <cstring>

extern "C" REX_FUNC(__imp__sub_821830D8);
extern "C" REX_FUNC(__imp__sub_8212F838);

namespace vf2::fight {
namespace {
std::mutex mutex;
std::optional<Config> pending;
Config current;
bool active=false, starting=false;
bool configured=false;
bool pending_restart=false,restore_requested=false;
struct RuntimeOptions {
  std::array<uint8_t,12> tuning{};
  uint8_t difficulty=0,rounds=2,time=30;
  uint8_t base_difficulty=0;
};
std::optional<RuntimeOptions> original_options;
RuntimeOptions active_options;

std::string status="Ready. CPU difficulty uses the game's current native setting.";
uint32_t launch_tick=0;
uint8_t previous_control[2]{};
uint32_t Read(const uint8_t* memory,uint32_t address) {
  const auto* p=memory+address;
  return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;
}
void Write(uint8_t* memory,uint32_t address,uint32_t value) {
  for (int n=0;n<4;++n) memory[address+n]=uint8_t(value>>(n*8));
}
bool Object(uint32_t address) { return address>=0x500000 && address<0x580000; }
void SetStatus(std::string value) {
  std::lock_guard lock(mutex);status=std::move(value);
}
RuntimeOptions ReadOptions(uint8_t* memory) {
  RuntimeOptions result;
  std::memcpy(result.tuning.data(),memory+0x50a700,result.tuning.size());
  result.difficulty=memory[0x50d005];
  result.base_difficulty=memory[Read(memory,0x50016c)+0x3342];
  result.rounds=memory[0x500059];result.time=memory[0x500090];
  return result;
}
void WriteOptions(uint8_t* memory,const RuntimeOptions& options) {
  std::memcpy(memory+0x50a700,options.tuning.data(),options.tuning.size());
  memory[0x50d005]=options.difficulty;
  memory[0x500059]=options.rounds;memory[0x500090]=options.time;
}
void RestoreOptions(uint8_t* memory) {
  if(!original_options)return;
  WriteOptions(memory,*original_options);original_options.reset();
}
bool ApplyOptions(uint8_t* base,uint8_t* memory,const Config& config) {
  if(config.difficulty>=0) {
    const uint32_t program=REX_LOAD_U32(0x82AB6718);
    if(!program || program>0xfff00000)return false;
    const auto* rom=base+program;
    const unsigned difficulty=unsigned(config.difficulty);
    // Exact defaults used by native set_game_setting (0x113F4), and the
    // original difficulty assignment's reset tables (0x5B2CC). Apply only
    // derived runtime values; neither SRAM mirror nor frontend settings changes.
    const auto rom16=[&](unsigned offset) {
      return uint32_t(rom[offset])|uint32_t(rom[offset+1])<<8;
    };
    Write(memory,0x50a700,Read(rom,0x11530+difficulty*4));
    Write(memory,0x50a704,Read(rom,0x11540+difficulty*4));
    Write(memory,0x50a708,rom16(0x11550+difficulty*2)|
        rom16(0x11558+difficulty*2)<<16);
    memory[0x50d005]=uint8_t(difficulty);
  }
  if(config.rounds_to_win>=0)memory[0x500059]=uint8_t(config.rounds_to_win);
  return true;
}
bool Prepare(PPCContext& ctx,uint8_t* base,uint8_t* memory,const Config& config,
             const RuntimeOptions* restart_options) {
  // All addresses below belong to the original i960 engine. See the decoded
  // ROM and analysis/custom-fight-rom/match-flow.txt for the native flow.
  // SEL_INT establishes the same task activation and resources as ordinary
  // character selection; the launcher supplies its completed selection.
  if (!i960::Call(ctx,base,0xC474)) return false;
  // SEL_INT schedules these initializers rather than loading the selection
  // resources itself. Run each native entry once before completing selection;
  // a launch from the title otherwise lacks the HUD character/palette banks.
  for(uint32_t slot:{0x500838u,0x50083cu,0x500840u}) {
    const uint32_t object=Read(memory,slot);
    const uint32_t entry=Read(memory,object+12);
    std::array<uint32_t,14> globals;
    const uint32_t cpu=REX_LOAD_U32(0x82AB6754);
    for(unsigned i=0;i<globals.size();++i)globals[i]=REX_LOAD_U32(cpu+64+i*4);
    globals[13]=object; // g13 is the original task scheduler's current task.
    if(!i960::CallWithGlobals(ctx,base,entry,globals))return false;
  }
  // Selection rebuilds native difficulty/round defaults. Restart reuses the
  // previous match's resolved values even when its UI choice was Use Game Settings.
  if(restart_options)WriteOptions(memory,*restart_options);
  else if(!ApplyOptions(base,memory,config))return false;
  const uint32_t fighters[2]={Read(memory,0x500804),Read(memory,0x500808)};
  for (int player=0;player<2;++player) {
    memory[fighters[player]+0x1b0]=uint8_t(config.characters[player]);
    memory[fighters[player]+0x1b1]=uint8_t(config.characters[player]);
    // Exact native debug-control enumeration: CPU=1, MAN_PLAYER=5.
    memory[0x50a0b8+player]=config.cpu[player]?1:5;
  }
  // Alternate costumes use the native +13 index, as a mirror match does for
  // P2. A mirror match with equal choices still gives P2 the other costume.
  bool alternate[2]={config.alternate[0],config.alternate[1]};
  if(config.characters[0]==config.characters[1] && alternate[0]==alternate[1])alternate[1]=!alternate[0];
  for(int player=0;player<2;++player)
    if(alternate[player])memory[fighters[player]+0x1b0]+=13;
  // Disable the three selection-only tasks just created by SEL_INT.
  for (uint32_t slot:{0x500838u,0x50083cu,0x500840u}) {
    const uint32_t object=Read(memory,slot);
    Write(memory,object,Read(memory,object)&~((1u<<31)|1u));
  }
  // Native SEL_DSP completion (ROM 0xCDDC..0xCDF0) releases the HUD from
  // selection mode. Bit 26 suppresses its state callbacks, including the
  // SET_INT callback at 0x2CCD0 that rebuilds both character-name tilemaps.
  const uint32_t hud=Read(memory,0x500834);
  Write(memory,hud,(Read(memory,hud)&~((1u<<26)|(1u<<24)))|(1u<<31));
  // Original ADV_INT activation (0xABDC..0xABFC). Title launches bypass it.
  // Restart the real HUD initializer so its normal state callbacks run.
  Write(memory,hud+12,0x2B1BC);
  memory[0x500056]=2; // Native versus initialization branch in GAME_INT.
  uint32_t flags=Read(memory,0x500068);
  // Bit 1 selects actual gameplay rather than the endless attract/demo path
  // in native match-end dispatcher 0xF208. Bits 2/3 mark both slots entered.
  flags|=(1u<<1)|(1u<<2)|(1u<<3);
  flags&=~((1u<<15)|(1u<<20)|(1u<<21));
  Write(memory,0x500068,flags);
  memory[0x500064]=uint8_t(config.arena);
  memory[0x50004c]=config.arena==10?0:2;
  Write(memory,0x50a00c,Read(memory,0x50a700));
  // Native stage setup after its character-pair automatic-stage choice. This
  // preserves an explicitly selected arena for Sarah versus Jacky too.
  // Stage 10 must use the boss initialization branch rather than its native
  // versus substitute (11); GAME_INT restores versus semantics afterwards.
  if (!i960::Call(ctx,base,0x1FD14)) return false;
  // GAME_INT advances the main mode by one; it normally enters from 8.
  memory[0x50002a]=8;
  if (!i960::Call(ctx,base,0xD014)) return false;
  // GAME_INT restores the frontend timer through the shipped interpreter.
  // Apply the custom runtime value once after it returns; the scheduled
  // FIGHT_INT will convert this to the original countdown on its next update.
  if(restart_options)memory[0x500090]=restart_options->time;
  else if(config.round_time>=0)memory[0x500090]=uint8_t(config.round_time);
  // The frontend normally enables both logical users when leaving Versus
  // selection. ROM physical ports alone do not enable native pause-menu input.
  rex::CallFrame input(ctx);input.ctx.r3.u64=2;sub_821110F8(input.ctx,base);
  return true;
}
void TestCommand() {
  const char* path=std::getenv("VF2_CUSTOM_FIGHT_TEST");
  if (!path) return;
  static std::filesystem::file_time_type last{};
  std::error_code ec;const auto stamp=std::filesystem::last_write_time(path,ec);
  if (ec || stamp==last) return;
  std::ifstream input(path);Config c;int cpu0=0,cpu1=0,unused_difficulty0=0,unused_difficulty1=0;
  input>>std::ws;
  if(input.peek()=='s') {
    std::string command;input>>command;
    if(command=="settings-open" || command=="settings-close") {
      last=stamp;SetSettingsOpen(command=="settings-open");
    }
    return;
  }
  if (input.peek()=='r') {
    std::string command;input>>command;
    if (command=="restart") {last=stamp;RequestRestart();}
    return;
  }
  if (input.peek()=='m') {  // test only: the native Exit Game reset to the Main Menu
    std::string command;input>>command;
    if (command=="menu") {last=stamp;RequestMainMenu();}
    return;
  }
  if (input>>c.characters[0]>>c.characters[1]>>cpu0>>cpu1
      >>unused_difficulty0>>unused_difficulty1>>c.arena) {
    int difficulty=-1,wins=-1,time=-1;
    if(input>>difficulty>>wins>>time) {
      c.difficulty=difficulty;c.rounds_to_win=wins;c.round_time=time;
      int tourist=0,alt0=0,alt1=0;
      if(input>>tourist>>alt0>>alt1){c.tourist=tourist!=0;c.alternate[0]=alt0!=0;c.alternate[1]=alt1!=0;}
    }
    c.cpu[0]=cpu0!=0;c.cpu[1]=cpu1!=0;last=stamp;RequestStart(c);
  }
}
}
void RequestStart(const Config& config) {
  std::lock_guard lock(mutex);
  if (config.characters[0]<0 || config.characters[0]>10 ||
      config.characters[1]<0 || config.characters[1]>10 ||
      config.arena<0 || config.arena>15 ||
      config.difficulty < -1 || config.difficulty > 3 || (config.tourist && config.difficulty!=0) ||
      (config.rounds_to_win!=-1 && (config.rounds_to_win<2 || config.rounds_to_win>5)) ||
      (config.round_time!=-1 && config.round_time!=15 && config.round_time!=30 &&
       config.round_time!=45 && config.round_time!=60)) {
    status="Failed: custom fight values are outside the verified native options.";
    return;
  }
  pending=config;pending_restart=false;status="Queued: waiting for the original game update.";
}
void RequestRestart() {
  std::lock_guard lock(mutex);
  if (!configured) {status="Start a custom fight before restarting.";return;}
  pending=current;pending_restart=true;status="Queued: restarting the current custom fight.";
}
std::string Status() {std::lock_guard lock(mutex);return status;}
bool Active() {std::lock_guard lock(mutex);return active;}
bool CanRestart() {std::lock_guard lock(mutex);return configured;}
bool Pending() {std::lock_guard lock(mutex);return pending.has_value();}
int TouristOverride() {
  std::lock_guard lock(mutex);
  if(!active)return -1;
  return current.tourist?1:current.difficulty>=0?0:-1;  // Use Game Settings: F6 switch
}
void Finish() {
  std::lock_guard lock(mutex);restore_requested=restore_requested||active||starting;
  active=false;starting=false;
  status="Match finished. Ready to start another custom fight.";
}
void MatchEnded(uint8_t* base) {
  if(!Active())return;
  const uint32_t memory=REX_LOAD_U32(0x833BD428);
  // 0xF208 runs after every round; it continues to the next round unless
  // round judgement (0xE800) has set bit 1 of 0x500068: match decided.
  if(!memory || memory>=0xf0000000 || !((Read(base+memory,0x500068)>>1)&1))return;
  if(memory && memory<0xf0000000) {
    base[memory+0x50a0b8]=previous_control[0];
    base[memory+0x50a0b9]=previous_control[1];
    RestoreOptions(base+memory);
  }
  Finish();RequestMainMenu();
  REXLOG_INFO("VF2 custom fight: native full-match result reached after victory animation");
}
void NativeDifficulty(uint8_t* base) {
  if(!Active())return;
  const uint32_t cpu=REX_LOAD_U32(0x82AB6754);
  if(cpu)REX_STORE_U32(cpu+12,active_options.base_difficulty);
}
void PollTestCommand() {TestCommand();}
void Update(PPCContext& ctx,uint8_t* base,bool after) {
  if (!after && !HostReady()) return;
  const uint32_t pointer=REX_LOAD_U32(0x833BD428);
  const uint32_t tick=REX_LOAD_U32(0x82E7E38C);
  if (const char* report=std::getenv("VF2_CUSTOM_FIGHT_TEST_REPORT")) {
    static uint64_t entries=0;
    const uint64_t entry=++entries;
    if (entry<20 || tick%30==0 || Active()) {
      std::ofstream out(report,std::ios::app);
      out<<"UPDATE "<<entry<<" after="<<after<<" tick="<<tick
          <<" memory="<<std::hex<<pointer<<" enable="<<int(base[0x82e7e388])
          <<" pc="<<REX_LOAD_U32(0x82ab6750);
      if (pointer && pointer<0xf0000000) {
        auto* m=base+pointer;
        out<<" mode="<<std::dec<<int(m[0x50002a])<<" sub="<<int(m[0x500030])
            <<" p1="<<std::hex<<Read(m,0x500804)<<" p2="<<Read(m,0x500808);
      }
      out<<'\n';
    }
  }
  if (!pointer || pointer>0xF0000000) return;
  auto* memory=base+pointer;
  bool restore=false;
  {std::lock_guard lock(mutex);restore=restore_requested;restore_requested=false;}
  if(restore) {
    memory[0x50a0b8]=previous_control[0];memory[0x50a0b9]=previous_control[1];
    RestoreOptions(memory);
  }
  const uint32_t p1=Read(memory,0x500804),p2=Read(memory,0x500808);
  if (!Object(p1)||!Object(p2)) return;
  for (uint32_t slot:{0x500814u,0x50081cu,0x500824u,0x500828u,
       0x500834u,0x500838u,0x50083cu,0x500840u,0x500844u,
       0x500848u,0x50084cu,0x500850u,0x50085cu,0x500860u,
       0x500864u,0x500868u,0x50086cu})
    if (!Object(Read(memory,slot))) return;
  if (!after) {
    TestCommand();
    std::optional<Config> request;bool restart=false;
    {std::lock_guard lock(mutex);request=pending;restart=pending_restart;
      pending.reset();pending_restart=false;}
    if (request) {
      SetStatus("Starting: running the original stage and match initialization.");
      const std::vector<uint8_t> saved_ram(memory+0x500000,memory+0x600000);
      const RuntimeOptions baseline=original_options?*original_options:ReadOptions(memory);
      // Clear only previous custom runtime choices before a fresh configuration;
      // SEL_INT then loads normal native settings. Preserve SRAM at all times.
      if(original_options)WriteOptions(memory,*original_options);
      {std::lock_guard lock(mutex);
        if (!active) {
          previous_control[0]=memory[0x50a0b8];
          previous_control[1]=memory[0x50a0b9];
        }
      }
      const RuntimeOptions restart_values=active_options;
      if (!Prepare(ctx,base,memory,*request,restart?&restart_values:nullptr)) {
        std::memcpy(memory+0x500000,saved_ram.data(),saved_ram.size());
        SetStatus("Failed: native initialization did not complete. See the runtime log.");
        return;
      }
      if(!original_options)original_options=baseline;
      active_options=ReadOptions(memory);
      active_options.base_difficulty=restart?restart_values.base_difficulty:
          (request->difficulty>=0?uint8_t(request->difficulty):active_options.base_difficulty);
      {std::lock_guard lock(mutex);current=*request;active=true;starting=true;configured=true;}
      launch_tick=tick;
      REXLOG_INFO("VF2 custom fight initialized: chars={},{} CPU={},{} arena={}",
          request->characters[0],request->characters[1],request->cpu[0],request->cpu[1],request->arena);
      REXLOG_INFO("VF2 custom match options: difficulty={} rounds_to_win={} round_time={}",
          active_options.base_difficulty,memory[0x500052],memory[0x500090]);
      if (const char* report=std::getenv("VF2_CUSTOM_FIGHT_TEST_REPORT")) {
        std::ofstream out(report,std::ios::app);
        out<<"INITIALIZED mode="<<int(memory[0x50002a])<<" sub="<<int(memory[0x500030])
            <<" tick="<<tick<<" p1="<<std::hex<<Read(memory,0x500804)
            <<" p2="<<Read(memory,0x500808)<<" pc="<<REX_LOAD_U32(0x82ab6750)<<'\n';
      }
    }
  } else {
    bool launch=false;Config selected;
    {std::lock_guard lock(mutex);launch=starting;selected=current;}
    if (!launch && Active() && memory[0x50002a]!=8 && memory[0x50002a]!=9) {
      memory[0x50a0b8]=previous_control[0];
      memory[0x50a0b9]=previous_control[1];
      RestoreOptions(memory);
      {std::lock_guard lock(mutex);active=false;}
      RequestMainMenu();
      SetStatus("Returned to the original game. Custom control overrides restored.");
    }
    if (launch && tick-launch_tick>=90) {
      const bool matches=memory[0x50002a]==9 &&
          memory[p1+0x1b1]==selected.characters[0] &&
          memory[p2+0x1b1]==selected.characters[1] && memory[0x500064]==selected.arena;
      {std::lock_guard lock(mutex);starting=false;if (!matches) active=false;}
      if (!matches) {
        memory[0x50a0b8]=previous_control[0];
        memory[0x50a0b9]=previous_control[1];
        RestoreOptions(memory);
        REXLOG_ERROR("VF2 custom fight state check failed: mode={} sub={} chars={},{} arena={}",
            memory[0x50002a],memory[0x500030],memory[p1+0x1b1],
            memory[p2+0x1b1],memory[0x500064]);
      }
      SetStatus(matches?"Fight started. Custom native match options are active."
          :"Failed: native match state differs from the requested configuration.");
    }
  }
  if (const char* report=std::getenv("VF2_CUSTOM_FIGHT_TEST_REPORT")) {
    if (after && tick%30==0) {
      std::ofstream out(report,std::ios::app);
      out<<tick<<' '<<int(memory[0x50002a])<<' '<<int(memory[0x500030])<<' '
          <<int(memory[p1+0x1b1])<<' '<<int(memory[p2+0x1b1])<<' '
          <<int(memory[0x500064])<<' '<<int(memory[0x50a0b8])<<' '
          <<int(memory[0x50a0b9])<<' '<<Status()
          <<" hud="<<std::hex<<Read(memory,Read(memory,0x500834))
          <<" hudfn="<<Read(memory,Read(memory,0x500834)+12)
          <<" hudstate="<<int(memory[0x500048])
          <<" input1="<<Read(memory,p1+0x1200)<<" input2="<<Read(memory,p2+0x1200)
          <<" x1="<<Read(memory,p1+0x18)<<" x2="<<Read(memory,p2+0x18)
          <<" hostmode="<<REX_LOAD_U32(0x824A08CC)
          <<" map1="<<REX_LOAD_U32(0x82AAEB9C)<<" map2="<<REX_LOAD_U32(0x82AAEBA0)<<'\n';
      out<<"OPTIONS tick="<<std::dec<<tick<<" difficulty="<<int(memory[0x50d005])
          <<" base="<<int(active_options.base_difficulty)<<" wins="<<int(memory[0x500052])
          <<" time="<<int(memory[0x500090])<<" tuning="<<std::hex<<Read(memory,0x50a700)
          <<','<<Read(memory,0x50a708)<<'\n';
    }
  }
}
void TracePhysicalControl(uint8_t* base,unsigned player) {
  static const char* report=std::getenv("VF2_CUSTOM_FIGHT_TEST_REPORT");
  if(!report)return;
  const uint32_t tick=REX_LOAD_U32(0x82E7E38C);
  if(tick%10)return;
  const uint32_t cpu=REX_LOAD_U32(0x82AB6754),memory=REX_LOAD_U32(0x833BD428);
  if(!cpu||!memory)return;
  const uint32_t task=REX_LOAD_U32(cpu+116);
  if(!Object(task))return;
  static std::ofstream trace(std::string(report)+".controls.csv");
  trace<<tick<<','<<player<<','<<(REX_LOAD_U32(cpu+12)&255)<<','
       <<Read(base+memory,0x500700)<<','<<Read(base+memory,task)<<','
       <<int(REX_LOAD_U8(0x830B89E3))<<','<<int(REX_LOAD_U8(0x830B89E4))<<','
       <<REX_LOAD_U32(0x82AAECB8)<<','<<REX_LOAD_U32(0x82AAEDD0)<<','
       <<REX_LOAD_U32(0x82AA6EF0)<<','<<REX_LOAD_U32(0x82AA6EF4)<<'\n';
  trace.flush();
}
}
REX_HOOK_RAW(sub_821830D8) {
  if(!vf2::demo::AllowUpdate(base))return;
  if(vf2::fight::SimulationFrozen())return; // F6 open: hold the current frame
  // Test-only single stepping: once VF2_STEP_TEST holds a number N, run
  // exactly N more simulation updates and hold (rendering continues), so
  // slow GPU readbacks can capture consecutive ticks.
  if(const char* path=std::getenv("VF2_STEP_TEST")) {
    static int armed_at=-1;static unsigned done=0;
    std::ifstream file(path);long target=-1;
    if(file>>target && target>=0) {
      if(armed_at<0){armed_at=int(REX_LOAD_U32(0x82E7E38C));done=0;
        REXLOG_INFO("VF2 step test: armed at tick {}",armed_at);}
      if(done>=unsigned(target))return;
      ++done;
    }
  }
  const unsigned steps=vf2::render::SimulationSteps();
  if(!steps)return;
  for(unsigned i=0;i<steps;++i) {
    __imp__sub_821830D8(ctx,base);
    vf2::render::CommitStep();
  }
  vf2::fight::Update(ctx,base,true);
}
REX_HOOK_RAW(sub_8212F838) {
  // Mutate the match only when the old frame has finished and the native
  // mode dispatcher is about to begin. An outer Xbox frame can interrupt
  // i960 execution inside the previous attract/replay continuation.
  const uint32_t program=REX_LOAD_U32(0x82AB6718);
  if(program){
    const uint32_t pc=REX_LOAD_U32(0x82AB6750)-program;
    vf2::arcade::NativeInstruction(ctx,base,pc);
    vf2::render::ShadowBoardInstruction(base,pc);
  }
  // Diagnostics: i960 PCs executed per update tick (window from
  // VF2_PC_TRACE_START, 64 ticks), written as ranges "tick: a-b a-b ...".
  static const char* pc_trace=std::getenv("VF2_PC_TRACE");
  if(const char* path=pc_trace; path && program) {
    static std::vector<uint8_t> hit(0x100000);static uint32_t current=~0u;
    static const uint32_t start=std::getenv("VF2_PC_TRACE_START")?std::atoi(std::getenv("VF2_PC_TRACE_START")):400;
    const uint32_t tick=REX_LOAD_U32(0x82E7E38C);
    if(tick!=current) {
      if(current>=start && current<start+64) {
        static std::ofstream out(path);
        out<<current<<':';
        for(uint32_t a=0;a<hit.size();) {
          if(!hit[a]){++a;continue;}
          uint32_t b=a;while(b+4<hit.size() && hit[b+4])b+=4;
          out<<' '<<std::hex<<a<<'-'<<b<<std::dec;a=b+4;
        }
        out<<'\n';out.flush();
      }
      std::fill(hit.begin(),hit.end(),0);current=tick;
    }
    const uint32_t pc=REX_LOAD_U32(0x82AB6750)-program;
    if(pc<hit.size())hit[pc]=1;
  }
  // Feed the selected shared level into the original adaptive-difficulty
  // lookup after its SRAM read, keeping its performance index/table intact.
  // SRAM and ordinary Arcade behavior remain untouched.
  if(program && REX_LOAD_U32(0x82AB6750)==program+0x113B4)
    vf2::fight::NativeDifficulty(base);
  // 0xF208 is the original round-result continuation after MATCHEND_DSP
  // (0xF85C) waits for the winning animation; MatchEnded acts only when the
  // match is decided. Versus can initialize the next pairing without leaving
  // main mode 9, so checking the outer mode byte misses this point.
  if(program && REX_LOAD_U32(0x82AB6750)==program+0xF208)
    vf2::fight::MatchEnded(base);
  if (program && REX_LOAD_U32(0x82AB6750)==program+0xA6C0) {
    vf2::freelook::Restore(base);
    vf2::demo::NativeUpdate(base);
  }
  if (program && REX_LOAD_U32(0x82AB6750)==program+0xA6C0)
    vf2::fight::Update(ctx,base,false);
  if (program && REX_LOAD_U32(0x82AB6750)==program+0x24534) {
    vf2::freelook::Apply(base);
    vf2::panorama::CaptureNativeScroll(ctx,base);
  }
  if (program && REX_LOAD_U32(0x82AB6750)==program+0x24664)
    vf2::panorama::CaptureComputedNativeScroll(ctx,base);
  if (program && REX_LOAD_U32(0x82AB6750)==program+0x2467C)
    vf2::panorama::CommitNativeScroll(ctx,base);
  if (program && REX_LOAD_U32(0x82AB6750)==program+0x1BA64)
    vf2::fight::TracePhysicalControl(base,0);
  if (program && REX_LOAD_U32(0x82AB6750)==program+0x1BAB8)
    vf2::fight::TracePhysicalControl(base,1);
  __imp__sub_8212F838(ctx,base);
}
