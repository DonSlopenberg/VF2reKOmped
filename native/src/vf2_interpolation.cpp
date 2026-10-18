#include "generated/default/vf2_pch.h"
#include "generated/default/vf2_funcs.h"
#include "vf2_render_clock.h"
#include "vf2_interpolation.h"
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/cvar.h>
#include <unordered_map>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <array>
#include <algorithm>
#include <cmath>
#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>
#include <rex/memory.h>
REXCVAR_DEFINE_BOOL(vf2_smooth_shadow_edges, false, "Video",
    "Move cast-shadow edges on board decks (the bridge on Shun's raft) smoothly instead of in 0.05-unit variant steps");
extern "C" REX_FUNC(__imp__sub_82197A38);
extern "C" REX_FUNC(__imp__sub_821549B8);
extern "C" REX_FUNC(__imp__sub_82154050);
extern "C" REX_FUNC(__imp__sub_82141F98);
namespace vf2::render {
Clock clock;
std::atomic_uint simulation_tick=0;
std::atomic<double> boundary=0;
double Seconds(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
thread_local FixedClock simulation;
double Rate() {
  const char* rate=std::getenv("VF2_DEMO_RENDER_HZ");
  return rate?std::clamp(std::atof(rate),60.0,240.0):60;
}
double Alpha(){return std::clamp((Seconds()-boundary.load())*60,0.0,1.0);}
unsigned Tick(){return simulation_tick.load();}
unsigned SimulationSteps() {
  if(!std::getenv("VF2_DEMO_RENDER_HZ"))return 1;
  return simulation.Advance(Seconds());
}
void CommitStep() {
  ++simulation_tick;
  boundary.store(simulation.boundary);
}
}
REX_HOOK_RAW(sub_82197A38) {
  vf2::render::clock.Step(vf2::render::Rate());
  __imp__sub_82197A38(ctx,base);
}
REX_HOOK_RAW(sub_821549B8) {
  if(const char* path=std::getenv("VF2_PANORAMA_TRACE")) {
    static std::ofstream out(std::string(path)+".mesh.csv");
    const uint32_t renderer=ctx.r3.u32;
    out<<vf2::render::clock.frame<<','<<vf2::render::Tick()<<','<<renderer;
    for(unsigned offset:{52u,56u,68u,72u,180u,184u})out<<','<<REX_LOAD_U32(renderer+offset);
    const uint32_t records=REX_LOAD_U32(renderer+68);
    if(records)for(unsigned i=0;i<18;++i)out<<','<<REX_LOAD_U32(records+i*4);
    out<<'\n';if(vf2::render::clock.frame%60==0)out.flush();
  }
  __imp__sub_821549B8(ctx,base);
}
REX_HOOK_RAW(sub_82154050) {
  if(const char* path=std::getenv("VF2_PANORAMA_TRACE")) {
    static std::ofstream out(std::string(path)+".sources.csv");
    out<<vf2::render::clock.frame<<','<<vf2::render::Tick()<<','<<std::hex<<uint32_t(ctx.lr);
    out<<','<<ctx.r3.u32<<','<<ctx.r4.u32<<','<<ctx.r5.u32<<','<<ctx.r6.u32<<','<<ctx.r7.u32<<','<<ctx.r8.u32;
    out<<std::dec<<'\n';if(vf2::render::clock.frame%60==0)out.flush();
  }
  __imp__sub_82154050(ctx,base);
}
namespace {
// Cast shadows on board decks (the bridge over Shun's raft) are pre-split
// model variants. ROM 0x1FACC picks one per partially shadowed board with
// cvtri((edge - board) / 0.05), so the split jumps one variant at a time and
// moves on two of three ticks. Consecutive variants have the same topology,
// so the fraction the ROM rounds away is recorded and the chosen variant is
// drawn with its vertices blended toward the next one. Nothing the game
// simulates changes; only this draw reads a blended copy.
struct BoardBlend { uint32_t id=0,neighbour=0,opposite=0,tick=0; float weight=0; };
std::mutex board_mutex;
std::array<BoardBlend,8> board_blends;
unsigned board_count=0;
float board_fraction=0;bool board_pending=false;
uint32_t board_base=0;  // instance address of model id 0 (instance = base + id*0x44)
std::vector<uint32_t> drawn;uint32_t drawn_tick=0;
uint32_t base_candidate=0,candidate_votes=0,candidate_tick=~0u,base_matched_tick=0;
constexpr uint32_t kInstanceStride=0x44,kPolygonRecord=24,kMaxBlendVertices=4096;
uint32_t blend_buffer=0;

bool GuestPointer(uint32_t address){return address>=0x80000000u && address<0xF0000000u;}
float LoadFloat(uint8_t* base,uint32_t address){PPCRegister v{};v.u32=REX_LOAD_U32(address);return v.f32;}

bool SmoothShadowBoard(PPCContext& ctx,uint8_t* base) {
  if(!REXCVAR_GET(vf2_smooth_shadow_edges))return false;
  const uint32_t model=ctx.r3.u32,tick=REX_LOAD_U32(0x82E7E38C);
  BoardBlend blend{};
  {
    std::lock_guard lock(board_mutex);
    if(drawn_tick!=tick){drawn_tick=tick;drawn.clear();}
    if(drawn.size()<256)drawn.push_back(model);
    // This tick's record for this instance (base learned below). Fully lit
    // or shadowed boards reuse the end variants, which are never recorded.
    auto find=[&](uint32_t table_base){
      const BoardBlend* best=nullptr;
      for(unsigned i=0;i<board_count;++i) {
        const BoardBlend& b=board_blends[i];
        if(b.tick==tick && table_base+b.id*kInstanceStride==model && !best)best=&b;
      }
      return best;};
    const BoardBlend* match=board_base?find(board_base):nullptr;
    if(match)base_matched_tick=tick;
    else if(!board_base || tick-base_matched_tick>120) {
      // Learn the table base: two boards of one tick whose instances were
      // both drawn in this tick give a candidate; instances are evenly
      // spaced, so a candidate must recur on 3 ticks before it is used.
      for(unsigned i=0;i<board_count;++i)for(unsigned j=0;j<board_count;++j) {
        const BoardBlend &b=board_blends[i],&o=board_blends[j];
        if(b.tick!=tick || o.tick!=b.tick || o.id==b.id)continue;
        const uint32_t candidate=model-b.id*kInstanceStride,other=candidate+o.id*kInstanceStride;
        if(std::find(drawn.begin(),drawn.end(),other)==drawn.end())continue;
        if(candidate!=base_candidate){base_candidate=candidate;candidate_votes=0;candidate_tick=~0u;}
        if(candidate_tick!=tick){candidate_tick=tick;++candidate_votes;}
        if(candidate_votes>=3){board_base=candidate;base_matched_tick=tick;}
      }
    }
    if(!match)return false;
    blend=*match;
  }
  if(blend.weight<=0.001f)return false;
  // Blend toward the next variant when it is a sibling: same polygon/vertex/
  // normal counts and the adjacent polygon block (24-byte records). Where
  // the split reaches a board end the next variant has another topology;
  // then continue the motion from the previous variant instead.
  const uint32_t vertices=REX_LOAD_U32(model+20),normals=REX_LOAD_U32(model+32);
  auto sibling=[&](uint32_t id){
    const int32_t step=int32_t(id)-int32_t(blend.id);
    if(step!=1 && step!=-1)return false;
    const uint32_t other=board_base+id*kInstanceStride;
    return REX_LOAD_U32(other+8)==REX_LOAD_U32(model+8) && REX_LOAD_U32(other+20)==vertices &&
        REX_LOAD_U32(other+32)==normals &&
        REX_LOAD_U32(other+16)==REX_LOAD_U32(model+16)+uint32_t(step*int32_t(REX_LOAD_U32(model+8)*kPolygonRecord));};
  float w=blend.weight;uint32_t target=blend.neighbour;
  if(!sibling(target)) {
    if(!blend.opposite || !sibling(blend.opposite))return false;
    target=blend.opposite;w=-w;
  }
  const uint32_t other=board_base+target*kInstanceStride;
  if(!vertices || vertices>kMaxBlendVertices || normals>kMaxBlendVertices)return false;
  const uint32_t a=REX_LOAD_U32(model+28),b=REX_LOAD_U32(other+28);
  const uint32_t na=REX_LOAD_U32(model+40),nb=REX_LOAD_U32(other+40);
  if(!GuestPointer(a) || !GuestPointer(b) || (normals && (!GuestPointer(na) || !GuestPointer(nb))))return false;
  if(!blend_buffer)blend_buffer=REX_KERNEL_MEMORY()->SystemHeapAlloc(2*kMaxBlendVertices*12);
  if(!blend_buffer)return false;
  auto mix=[&](uint32_t out,uint32_t from,uint32_t to,uint32_t count){
    for(uint32_t i=0;i<count*3;++i) {
      const float x=LoadFloat(base,from+i*4),y=LoadFloat(base,to+i*4);
      PPCRegister v{};v.f32=x+(y-x)*w;REX_STORE_U32(out+i*4,v.u32);
    }};
  mix(blend_buffer,a,b,vertices);
  const uint32_t normal_buffer=blend_buffer+kMaxBlendVertices*12;
  if(normals)mix(normal_buffer,na,nb,normals);
  REX_STORE_U32(model+28,blend_buffer);
  if(normals)REX_STORE_U32(model+40,normal_buffer);
  __imp__sub_82141F98(ctx,base);
  REX_STORE_U32(model+28,a);
  if(normals)REX_STORE_U32(model+40,na);
  return true;
}
}
namespace vf2::render {
// Called before each i960 instruction (program-relative pc).
void ShadowBoardInstruction(uint8_t* base,uint32_t pc) {
  if(pc!=0x1FB64 && pc!=0x1FB94 && pc!=0x1FB70 && pc!=0x1FB9C)return;
  if(!REXCVAR_GET(vf2_smooth_shadow_edges))return;
  const uint32_t cpu=REX_LOAD_U32(0x82AB6754),program=REX_LOAD_U32(0x82AB6718);
  if(!cpu || !program)return;
  if(pc==0x1FB64 || pc==0x1FB94) {  // cvtri r3: r3 = variant position (float)
    const float x=LoadFloat(base,cpu+12);
    board_pending=std::isfinite(x);
    if(board_pending)board_fraction=x-std::nearbyint(x);
    return;
  }
  // st r15,(g4)[r6*4]: r3 = variant index, r7 = table, r15 = id. r9 splits
  // the table: 0x1FB9C (shadow ends on the board) uses indices 0..r9, 0x1FB70
  // (shadow starts on the board) r9 upward; r9 itself is the fully shadowed
  // board and 0 the fully lit one, shared by whole boards, so never blended.
  if(!board_pending)return;
  board_pending=false;
  const int32_t index=int32_t(REX_LOAD_U32(cpu+12)),split=int32_t(REX_LOAD_U32(cpu+36));
  const uint32_t table=REX_LOAD_U32(cpu+28),id=REX_LOAD_U32(cpu+60);
  const int32_t next=index+(board_fraction>0?1:-1);
  const bool leading=pc==0x1FB70;
  const int32_t low=leading?split:0,high=leading?split+64:split;
  if(split<=0 || split>64 || table>=0x100000 || index<=low || index>=high || next<low || next>high)return;
  auto entry=[&](int32_t i){const uint8_t* p=base+program+table+uint32_t(i)*4;return uint32_t(p[0])|p[1]<<8|p[2]<<16|uint32_t(p[3])<<24;};
  const int32_t previous=2*index-next;
  const uint32_t neighbour=entry(next),opposite=previous>=low && previous<=high?entry(previous):0;
  const uint32_t tick=REX_LOAD_U32(0x82E7E38C);
  std::lock_guard lock(board_mutex);
  // Keep this and the previous tick's boards (drawing may trail the ROM).
  unsigned kept=0;
  for(unsigned i=0;i<board_count;++i) {
    const BoardBlend& b=board_blends[i];
    if(b.tick<=tick && b.tick+1>=tick && !(b.tick==tick && b.id==id))board_blends[kept++]=b;
  }
  board_count=kept;
  if(board_count<board_blends.size())board_blends[board_count++]={id,neighbour,opposite,tick,std::abs(board_fraction)};
}
}
// Opt-in identity probe before visibility clipping and polygon compaction.
// Source faces/vertices and command slots can be examined without attempting
// to blend unrelated indices from the final, camera-dependent draw buffer.
REX_HOOK_RAW(sub_82141F98) {
  if(const char* path=std::getenv("VF2_MESH_ID_TRACE")) {
    const unsigned tick=REX_LOAD_U32(0x82E7E38C);
    static const unsigned start=std::getenv("VF2_MESH_ID_TRACE_START")?unsigned(std::atoi(std::getenv("VF2_MESH_ID_TRACE_START"))):300;
    if(tick>=start && tick<start+120) {
      static std::ofstream out(path);
      static unsigned last_frame=~0u;
      if(last_frame!=vf2::render::clock.frame) {
        last_frame=vf2::render::clock.frame;
        const uint32_t m2=REX_LOAD_U32(0x82AB6CC8);
        out<<"F,"<<last_frame<<','<<tick<<','<<std::hex<<m2<<','<<REX_LOAD_U32(m2+0x4B350)<<','<<REX_LOAD_U32(m2+0x4B354)
           <<','<<REX_LOAD_U32(m2+0x4B358)<<std::dec<<'\n';
        static unsigned dumped_tick=~0u;
        if(std::getenv("VF2_M2_STATE_DUMP") && dumped_tick!=tick) {  // M2 context words +0x40000..+0x58000
          dumped_tick=tick;
          out<<"S,"<<last_frame<<','<<tick<<std::hex;
          static const uint32_t dump_lo=std::getenv("VF2_M2_STATE_LO")?std::strtoul(std::getenv("VF2_M2_STATE_LO"),nullptr,16):0x40000,dump_hi=std::getenv("VF2_M2_STATE_HI")?std::strtoul(std::getenv("VF2_M2_STATE_HI"),nullptr,16):0x58000;
          for(uint32_t a=dump_lo;a<dump_hi;a+=4)out<<','<<REX_LOAD_U32(m2+a);
          out<<std::dec<<'\n';
        }
      }
      const uint32_t model=ctx.r3.u32;
      out<<vf2::render::clock.frame<<','<<tick<<','<<std::hex<<model<<','
         <<REX_LOAD_U32(model+16)<<','<<REX_LOAD_U32(model+28)<<','
         <<ctx.r30.u32<<','<<ctx.r31.u32<<std::dec<<','
         <<REX_LOAD_U32(model+8)<<','<<REX_LOAD_U32(model+20);
      // Content hashes: model block, and 64 bytes at +28 / +40 / +32.
      auto hash=[&](uint32_t address,unsigned bytes){
        uint64_t h=1469598103934665603ull;
        if(address>=0x80000000u && address<0xF0000000u)for(unsigned i=0;i<bytes;++i)h=(h^REX_LOAD_U8(address+i))*1099511628211ull;
        return uint32_t(h^(h>>32));};
      const uint32_t m2=REX_LOAD_U32(0x82AB6CC8);
      out<<std::hex<<','<<hash(m2+0x4B204,64)<<','<<hash(REX_LOAD_U32(model+28),64)<<','<<hash(REX_LOAD_U32(model+40),64)
         <<','<<hash(REX_LOAD_U32(model+32),64)<<std::dec<<','<<ctx.r4.u32<<','<<ctx.r5.u32<<','<<ctx.r6.u32;
      for(unsigned i=0;i<16;++i){PPCRegister v{};v.u32=REX_LOAD_U32(m2+0x4B204+i*4);out<<','<<v.f32;}
      if(std::getenv("VF2_MESH_RAW")) {  // raw words at +28 (first 32) and +40 (first 16)
        out<<std::hex;
        for(uint32_t base_address:{REX_LOAD_U32(model+28),REX_LOAD_U32(model+40)}) {
          const unsigned words=base_address==REX_LOAD_U32(model+28)?32:16;
          for(unsigned i=0;i<words;++i)out<<','<<(base_address>=0x80000000u && base_address<0xF0000000u?REX_LOAD_U32(base_address+i*4):0);
        }
        out<<std::dec;
      }
      out<<'\n';
      if(tick%10==0)out.flush();
    }
  }
  // Diagnostics: VF2_TEST_SKIP_MODELS="lo-hi" skips models whose polygon count
  // (+8) is in [lo,hi]; optional ":keylo-keyhi" restricts by +16 address.
  if(static const char* spec=std::getenv("VF2_TEST_SKIP_MODELS"); spec) {
    // ';'-separated rules
    const uint32_t polys=REX_LOAD_U32(ctx.r3.u32+8),key=REX_LOAD_U32(ctx.r3.u32+16);
    for(const char* rule=spec;rule && *rule;) {
      unsigned lo=0,hi=0,klo=0,khi=0xFFFFFFFFu;
      if(std::sscanf(rule,"%u-%u:%x-%x",&lo,&hi,&klo,&khi)>=2 && polys>=lo && polys<=hi && key>=klo && key<=khi)return;
      rule=std::strchr(rule,';');if(rule)++rule;
    }
  }
  if(!SmoothShadowBoard(ctx,base))__imp__sub_82141F98(ctx,base);
}
