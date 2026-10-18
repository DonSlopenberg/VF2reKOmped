#include "generated/default/vf2_pch.h"
#include "generated/default/vf2_funcs.h"
#include "vf2_arcade.h"
#include "vf2_fight.h"
#include <rex/hook.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <numeric>
#include <random>
#include <chrono>
#include <fstream>
#include <cstdlib>
#include <cstring>

extern "C" REX_FUNC(__imp__sub_821A8520);

REXCVAR_DEFINE_BOOL(vf2_arcade_random_opponents, false, "VF2",
    "Arcade: random opponent order (Dural stays last; not in Ranking Mode)");
REXCVAR_DEFINE_BOOL(vf2_arcade_random_arenas, false, "VF2",
    "Arcade: random arenas without repeats (Dural keeps her arena; not in Ranking Mode)");
REXCVAR_DEFINE_BOOL(vf2_dural_hold_start, true, "VF2",
    "Character select: hold Start for 3 seconds to choose Dural");
REXCVAR_DEFINE_BOOL(vf2_arcade_tourist, false, "VF2",
    "Tourist difficulty: below Easy, CPU strength never escalates");
// The native level is a handicap: 0x3DBF0 derives action parameters as
// 0x52 - 16*level (clamped 0..255), so higher values make the CPU weaker
// (Akira/Dural have the lowest base levels). 16 is the native maximum cap.
REXCVAR_DEFINE_INT32(vf2_tourist_cpu_level, 16, "VF2",
    "Constant native CPU handicap used by Tourist (higher = weaker; native range -32..16)").range(-32,16);

namespace vf2::input { bool StartHeld(unsigned user); }
namespace vf2::arcade {
namespace {
std::mutex mutex;
Cheats cheats;
std::atomic_bool cheats_used{false};

// Native Arcade progression (decoded ROM): opponent of stage s (1-based) is
// ROM[0x12509 + (s-1)*2], its arena ROM[0x12508 + s*2]. Stage 11 = Dural.
constexpr uint32_t kTable=0x12508;
constexpr int kStages=11;
std::array<uint8_t,2*kStages+2> original{};
bool have_original=false,locked=false,patched=false;
std::mt19937 rng{std::random_device{}()};

uint8_t* Ram(uint8_t* base) {
  const uint32_t memory=REX_LOAD_U32(0x833BD428);
  return memory && memory<0xf0000000 ? base+memory : nullptr;
}
uint8_t* Program(uint8_t* base) {
  const uint32_t program=REX_LOAD_U32(0x82AB6718);
  return program && program<0xfff00000 ? base+program : nullptr;
}
uint32_t LE32(const uint8_t* p){return uint32_t(p[0])|p[1]<<8|p[2]<<16|uint32_t(p[3])<<24;}
// RANK_INT (0x10BC0) runs only with bit 9 of 0x500088: Ranking Mode.
bool RankingMode(const uint8_t* ram){return (LE32(ram+0x500088)>>9)&1;}

void WriteTable(uint8_t* rom,const std::array<uint8_t,2*kStages+2>& table) {
  std::copy(table.begin(),table.end(),rom+kTable);
}
// Lock one line-up per run: the first table read of a run decides it. The
// lock is released only in frontend scenes, so Continue keeps the order.
void PrepareRun(uint8_t* base) {
  if(locked)return;
  uint8_t* rom=Program(base);uint8_t* ram=Ram(base);
  if(!rom || !ram)return;
  if(!have_original){std::copy_n(rom+kTable,original.size(),original.begin());have_original=true;}
  locked=true;
  const bool ranking=RankingMode(ram);
  const bool opponents=REXCVAR_GET(vf2_arcade_random_opponents) && !ranking;
  const bool arenas=REXCVAR_GET(vf2_arcade_random_arenas) && !ranking;
  // Automated tests only: VF2_ARCADE_FIRST="opponent@arena" for stage 1.
  if(const char* first=std::getenv("VF2_ARCADE_FIRST")) {
    auto table=original;table[1]=uint8_t(std::atoi(first));
    if(const char* at=std::strchr(first,'@'))table[2]=uint8_t(std::atoi(at+1));
    WriteTable(rom,table);patched=true;
    REXLOG_INFO("VF2 arcade test: stage 1 = opponent {} on arena {}",table[1],table[2]);
    return;
  }
  if(!opponents && !arenas) {
    if(patched){WriteTable(rom,original);patched=false;}
    return;
  }
  auto table=original;
  std::array<int,kStages-1> order;std::iota(order.begin(),order.end(),0);
  if(opponents)std::shuffle(order.begin(),order.end(),rng);
  // Arenas follow their characters unless randomized; 0..9 only, so no
  // repeats, no Dural arena (10) and no test/variant arenas (11..15).
  std::array<uint8_t,kStages-1> arena;
  for(int i=0;i<kStages-1;++i)arena[i]=original[2*(order[i]+1)];
  if(arenas){
    std::array<uint8_t,kStages-1> pool;std::iota(pool.begin(),pool.end(),uint8_t(0));
    std::shuffle(pool.begin(),pool.end(),rng);arena=pool;
  }
  for(int i=0;i<kStages-1;++i){table[2*i+1]=original[2*order[i]+1];table[2*(i+1)]=arena[i];}
  WriteTable(rom,table);patched=true;
  std::string line;
  for(int i=0;i<kStages;++i)line+=std::to_string(table[2*i+1])+"@"+std::to_string(table[2*(i+1)])+" ";
  REXLOG_INFO("VF2 arcade line-up (opponent@arena): {} random_opponents={} random_arenas={} ranking={}",
      line,opponents,arenas,ranking);
}
}

namespace {
// Hidden Dural (character select). Native check, side 1 at 0xC8BC..0xC8E0,
// side 2 at 0xCA00..0xCA24, runs when that side's selection is confirmed:
// Ver.2.1 flag (options+0x3351 bit 6), the side's Start (0x500700 bit 4/5)
// and options+0x3001 % 17 == 0. Holding Start for 3 s (counted in host
// frames from the controller of user 0 = side 1, user 1 = side 2) confirms
// the selection (A is injected) and satisfies that check by setting the
// interpreter register before each test, so the native branch picks Dural.
std::array<std::atomic<unsigned>,2> start_held{};
std::array<std::atomic<bool>,2> dural_armed{};
std::atomic<int64_t> select_seen_ms{0};
int64_t NowMs(){return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count();}
void DuralSelect(uint8_t* base,uint32_t pc) {
  const uint32_t cpu=REX_LOAD_U32(0x82AB6754);
  if(!cpu || !REXCVAR_GET(vf2_dural_hold_start))return;
  constexpr uint32_t kR15=60;  // interpreter register file: r3 at +12
  const int side=(pc>=0xC8BC && pc<=0xC8E0)?0:(pc>=0xCA00 && pc<=0xCA24)?1:-1;
  if(pc==0xC880)select_seen_ms.store(NowMs());  // character select loop ran
  if(pc==0xC914 || pc==0xCA58) {
    const int s=pc==0xC914?0:1;
    if(dural_armed[s].exchange(false))REXLOG_INFO("VF2 select: side {} confirmed as Dural",s+1);
  }
  if(side<0 || !dural_armed[side].load())return;
  uint32_t r15=REX_LOAD_U32(cpu+kR15);
  if(pc==0xC8BC || pc==0xCA00)r15|=1u<<6;              // Ver.2.1 flag
  if(pc==0xC8C8)r15|=1u<<4;                            // side 1 Start
  if(pc==0xCA0C)r15|=1u<<5;                            // side 2 Start
  if(pc==0xC8E0 || pc==0xCA24)r15=0;                   // counter % 17 == 0
  REX_STORE_U32(cpu+kR15,r15);
}
}
// Called per host frame with the final controller state of user 0/1.
bool DuralConfirm(unsigned user,bool start) {
  if(user>1 || !REXCVAR_GET(vf2_dural_hold_start))return false;
  // Only while the character select loop is running.
  const bool selecting=NowMs()-select_seen_ms.load()<100;
  const unsigned held=start && selecting ? start_held[user].load()+1 : 0;
  start_held[user].store(held);
  if(held==180) {
    dural_armed[user].store(true);
    REXLOG_INFO("VF2 select: user {} held Start for 3 s, confirming Dural",user+1);
  }
  if(!start)dural_armed[user].store(false);
  return dural_armed[user].load() && held>=180 && held<180+12;  // ~0.2 s of A
}

namespace {
// Alternate costume at character select (Arcade and Versus). Up/Down toggles
// it per side while selecting; the overlay marks the choice. The select
// screen itself always previews the normal costume: its cursor paths test the
// held-input word 0x500700 (bit 13 side 1, bit 21 side 2) and would add the
// +13 costume offset, which the preview heads and palettes do not support,
// so that bit is cleared. The chosen costume is written when the selection
// ends (see CostumeInstruction). The choice lasts until the menus return.
constexpr uint32_t kCostumeSide1[]={0x32158,0x322D0,0x32524,0x3269C,0x3281C,0x32998,0x32B4C,0x32D10,0x32E88,0x33008,0x33184,0x33338};
constexpr uint32_t kCostumeSide2[]={0x32168,0x322E0,0x32534,0x326AC,0x3282C,0x329A8,0x32B5C,0x32D20,0x32E98,0x33018,0x33194,0x33348};
std::array<std::atomic<bool>,2> alternate{};
std::array<std::atomic<int>,2> select_slot{-1,-1};
std::array<std::atomic<bool>,2> select_open{},costume_touched{};
void CostumeInstruction(uint8_t* base,uint32_t pc) {
  if(pc==0xC880) {  // select loop: cursor slot (+6) of both side tasks
    uint8_t* ram=Ram(base);
    if(!ram)return;
    for(int side=0;side<2;++side) {
      const uint32_t task=LE32(ram+0x50083C+side*4);
      if(task<0x500000 || task>=0x580000){select_slot[side].store(-1);continue;}
      const uint32_t flags=LE32(ram+task);
      select_slot[side].store(ram[task+6]);
      select_open[side].store((flags>>1)&1);  // bit 1: still choosing
    }
    return;
  }
  // Selection finished: before the CPU opponent is assigned (0xCE4C, its
  // mirror check reads the player's costume) and at GAME_INT (0xD014), a side
  // that toggled gets its chosen costume. Writing it during the select screen
  // instead desynchronised the 3D preview head (colour flicker).
  if(pc==0xCE4C || pc==0xD014) {
    uint8_t* ram=Ram(base);
    if(!ram || !(costume_touched[0].load() || costume_touched[1].load()))return;
    uint8_t* id[2]{};
    for(int side=0;side<2;++side) {
      const uint32_t fighter=LE32(ram+0x500804+side*4);
      if(fighter>=0x500000 && fighter<0x580000)id[side]=ram+fighter+0x1B0;
    }
    for(int side=0;side<2;++side) {
      if(!id[side] || !costume_touched[side].load())continue;
      const uint8_t character=*id[side]>=13?*id[side]-13:*id[side];
      *id[side]=uint8_t(character+(alternate[side].load()?13:0));
    }
    if(id[0] && id[1] && *id[0]==*id[1])  // same character and costume: P2 gets the other one
      *id[1]=*id[1]>=13?*id[1]-13:*id[1]+13;
    return;
  }
  if(pc<0x32158 || pc>0x33348)return;
  for(int side=0;side<2;++side)
    for(uint32_t at:side?kCostumeSide2:kCostumeSide1) {
      if(pc!=at)continue;
      const uint32_t cpu=REX_LOAD_U32(0x82AB6754);
      if(!cpu)return;
      const uint32_t bit=side?1u<<21:1u<<13;
      // The preview always uses the normal costume: an alternate ID (+13)
      // during selection breaks the 3D preview heads and the palettes.
      REX_STORE_U32(cpu+60,REX_LOAD_U32(cpu+60)&~bit);
      return;
    }
}
}
void CostumeInput(unsigned user,uint16_t buttons) {
  if(user>1)return;
  static uint16_t previous[2]={};
  const uint16_t pressed=buttons&~previous[user];previous[user]=buttons;
  const bool selecting=NowMs()-select_seen_ms.load()<100;
  if(!selecting || !select_open[user].load() || !(pressed&0x0003))return;  // D-pad up/down
  const bool value=!alternate[user].load();
  alternate[user].store(value);costume_touched[user].store(true);
  REXLOG_INFO("VF2 select: side {} costume {}",user+1,value?"alternate":"normal");
}
SelectCostumes GetSelectCostumes() {
  SelectCostumes result;
  const bool selecting=NowMs()-select_seen_ms.load()<100;
  for(int side=0;side<2;++side) {
    result.slot[side]=select_slot[side].load();
    result.alternate[side]=selecting && alternate[side].load() && result.slot[side]>=0;
  }
  return result;
}

Cheats GetCheats(){std::lock_guard lock(mutex);return cheats;}
void SetCheats(const Cheats& c) {
  std::lock_guard lock(mutex);cheats=c;
  if((c.infinite_health[0] || c.infinite_health[1]) && !cheats_used.exchange(true))
    REXLOG_INFO("VF2 cheats enabled: achievements disabled until restart");
}
bool CheatsUsedThisSession(){return cheats_used.load();}
bool AchievementsBlocked(){return cheats_used.load();}

void HostFrame(uint8_t* base,bool frontend) {
  if(frontend && locked)locked=false;
  if(frontend){for(auto& a:alternate)a.store(false);for(auto& t:costume_touched)t.store(false);}
  // Automated tests only: VF2_CHEAT_TEST="p1 p2".
  static const bool test_cheats=[] {
    if(const char* t=std::getenv("VF2_CHEAT_TEST"))
      SetCheats({{t[0]=='1',t[1]&&t[2]=='1'}});
    return true;
  }();
  // Cheats: keep the selected fighters at the native round maximum.
  const Cheats c=GetCheats();
  if(!c.infinite_health[0] && !c.infinite_health[1])return;
  uint8_t* ram=Ram(base);
  if(!ram)return;
  const uint16_t maximum=uint16_t(ram[0x5000a0]|ram[0x5000a1]<<8);
  if(!maximum)return;
  for(int player=0;player<2;++player) {
    if(!c.infinite_health[player])continue;
    const uint32_t fighter=LE32(ram+0x500804+player*4);
    if(fighter<0x500000 || fighter>=0x580000)continue;
    ram[fighter+0x1ac]=uint8_t(maximum);ram[fighter+0x1ad]=uint8_t(maximum>>8);
  }
}

void NativeInstruction(PPCContext& ctx,uint8_t* base,uint32_t pc) {
  (void)ctx;
  // Opponent (0xCE68) and arena (0xCF6C) reads of the progression table.
  if(pc==0xCE68 || pc==0xCF6C)PrepareRun(base);
  DuralSelect(base,pc);
  CostumeInstruction(base,pc);
  if(pc!=0x113B4 && pc!=0x1140C && pc!=0x3D274 && pc!=0x3D34C && pc!=0x3DB68 && pc!=0x3DB80 && pc!=0x3DBC4)return;
  // A custom fight decides Tourist itself; otherwise the Arcade switch does.
  const int custom=vf2::fight::TouristOverride();
  if(custom==0 || (custom<0 && !REXCVAR_GET(vf2_arcade_tourist)))return;
  const uint32_t cpu=REX_LOAD_U32(0x82AB6754);
  if(!cpu)return;
  // variable_diff_calc: difficulty byte (r3) and working rate (r4) index
  // ROM 0x11510; Easy with rate 0 never escalates the global outputs.
  if(pc==0x113B4){REX_STORE_U32(cpu+12,0);REX_STORE_U32(cpu+16,0);}
  if(pc==0x1140C)REX_STORE_U32(cpu+20,0);  // set_game_setting override base
  // Arcade difficulty rises with the opponent order because every character
  // has its own AI tuning. Tourist gives every CPU the tables of Lau (id 4),
  // the first and easiest Arcade opponent. Each hook runs before the i960
  // instruction at that PC and replaces only the character index register.
  constexpr uint32_t kEasiest=4;
  // 0x3D274 lda 0x3BD54(r3): r3 = character*128 + style*32 -> AI parameter
  // row (+0x44..+0x5A). Keep the style, take Lau's row.
  if(pc==0x3D274)REX_STORE_U32(cpu+12,(REX_LOAD_U32(cpu+12)&0x7Fu)|(kEasiest<<7));
  // 0x3D34C ldib 0x4356C(r3): base CPU level per character (Akira/Dural -3).
  if(pc==0x3D34C)REX_STORE_U32(cpu+12,kEasiest);
  // 0x3DB68 ldob 0x4355C(r3): per-character level reduction under flag bit 4.
  if(pc==0x3DB68)REX_STORE_U32(cpu+12,kEasiest);
  // 0x3DB80 lda 0x4353C[r3*2]: per-character min/max level (Akira max +1).
  if(pc==0x3DB80)REX_STORE_U32(cpu+12,kEasiest);
  // CPU level store (0x3DBC4: r9 -> +0x5B, r10 -> +0xB3): constant level,
  // without the per-character base and the health-difference adjustment.
  if(pc==0x3DBC4) {
    const int32_t level=REXCVAR_GET(vf2_tourist_cpu_level);
    static uint64_t overrides=0;
    if(overrides++%600==0)
      REXLOG_INFO("VF2 Tourist: CPU level {} -> {} (override #{})",int32_t(REX_LOAD_U32(cpu+36)),level,overrides);
    REX_STORE_U32(cpu+36,uint32_t(level));REX_STORE_U32(cpu+40,uint32_t(level));
  }
}
}

// XUserWriteAchievements wrapper (XGI message 0x000B0008, 8-byte buffer
// {count = r3, XUSER_ACHIEVEMENT array = r4: user index, id}). While blocked,
// keep the count (the runtime's count=0 path skips completion, which left the
// game's save indicator spinning) and send ids that match no achievement;
// the game's own buffer is restored afterwards.
REX_HOOK_RAW(sub_821A8520) {
  const uint32_t count=ctx.r3.u32,entries=ctx.r4.u32;
  std::array<uint32_t,32> saved{};
  const bool block=vf2::arcade::AchievementsBlocked() && count && count<=saved.size() && entries;
  if(block) {
    REXLOG_INFO("VF2 achievements blocked ({} requested): cheats used this session",count);
    for(uint32_t i=0;i<count;++i){saved[i]=REX_LOAD_U32(entries+i*8+4);REX_STORE_U32(entries+i*8+4,0xFFFFFFFFu);}
  }
  __imp__sub_821A8520(ctx,base);
  if(block)for(uint32_t i=0;i<count;++i)REX_STORE_U32(entries+i*8+4,saved[i]);
}
