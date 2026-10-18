#include "generated/default/vf2_pch.h"
#include "generated/default/vf2_funcs.h"
#include "vf2_panorama.h"
#include "vf2_display.h"
#include "vf2_panorama_math.h"
#include "vf2_fight.h"
#include "vf2_demo.h"
#include "vf2_interpolation.h"
#include <rex/hook.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/memory.h>
#include <rex/system/kernel_state.h>
#include <rex/thread/mutex.h>
#include <array>
#include <atomic>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <chrono>
#include <cmath>
#include <string_view>
#include <fstream>
#include <cstdlib>
#include <iomanip>
#include <filesystem>
#include <windows.h>
#include <wincodec.h>

extern "C" REX_FUNC(__imp__sub_82188848);
extern "C" REX_FUNC(__imp__sub_82188C08);
extern "C" REX_FUNC(__imp__sub_821441D8);
extern "C" REX_FUNC(__imp__sub_821448F0);
extern "C" REX_FUNC(__imp__sub_821038F8);
extern "C" REX_FUNC(__imp__sub_82144548);
extern "C" REX_FUNC(__imp__sub_821445B0);
extern "C" REX_FUNC(__imp__sub_820FE650);
extern "C" REX_FUNC(__imp__sub_82144050);
extern "C" REX_FUNC(__imp__sub_820F7E00);
extern "C" REX_FUNC(__imp__sub_82188AB0);
extern "C" REX_FUNC(__imp__sub_821442A0);
REXCVAR_DECLARE(bool, vf2_background_smooth_scroll);
REXCVAR_DEFINE_DOUBLE(vf2_background_fov, 120.0, "Video",
    "Panoramic background horizontal FOV (degrees); arena camera is unchanged")
    .range(40.0,140.0);
REXCVAR_DEFINE_DOUBLE(vf2_background_horizon, 0.0, "Video",
    "Horizon Height / Vertical Offset (logical pixels; positive raises the panorama)")
    .range(-192.0,192.0);
REXCVAR_DEFINE_BOOL(vf2_background_color_decode, true, "Video",
    "Decode panorama palette colors before filtering, so subpixel camera motion reaches the screen");
REXCVAR_DEFINE_BOOL(vf2_background_wave_smooth, true, "Video",
    "Blend the interlaced per-line background wave (CRT-style [1,2,1] line filter) to remove comb jaggies");
REXCVAR_DEFINE_BOOL(vf2_background_linear_composite, true, "Video",
    "Smooth (linear) background filtering at output resolution; off = original pixelated look");
REXCVAR_DEFINE_BOOL(vf2_background_replacements, false, "Video",
    "Use upscaled arena panoramas from data/textures/replacements/arenas (same aspect as the originals)");
REXCVAR_DEFINE_BOOL(vf2_background_replacement_wave, true, "Video",
    "Apply the native per-line wave effect to upscaled backgrounds");
REXCVAR_DEFINE_INT32(vf2_background_blur, 0, "Video",
    "Distant background blur: 0 off, 1..3 = 2x/4x/8x downsampled and linearly upscaled").range(0,3);
REXCVAR_DEFINE_BOOL(vf2_background_line_align, true, "Video",
    "Align per-line scroll tables with the decoded panorama rows (line-scroll arenas)");
REXCVAR_DEFINE_BOOL(vf2_background_bank_guard, true, "Video",
    "Keep the widened background banks in every map upload (fixes seams on animated arenas)");
REXCVAR_DEFINE_BOOL(vf2_background_static_smooth, true, "Video",
    "Also smooth bitmap screens without an arena panorama (e.g. high-score entry)");
REXCVAR_DEFINE_BOOL(vf2_background_pitch_fix, true, "Video",
    "Take the vertical panorama fraction from the same pitch sample as the native integer port");

namespace vf2::panorama {
namespace {
constexpr uint32_t canvases[2] = {0x82BB6D60, 0x82BB6DB8};
struct Panorama { int columns, rows; std::vector<uint16_t> tiles; int arena=-1; uint32_t descriptor=0; };
struct Candidate { int panorama, x, y; };
std::vector<Panorama> panoramas;
std::array<int,16> arena_panorama=[] {std::array<int,16> a;a.fill(-1);return a;}();
std::array<uint8_t, 32768> original_map{};
// Map rows 64..127 (128 entries x 2 bytes) hold background banks 1 and 3.
constexpr uint32_t kBackgroundBankOffset=64*128*2;
// Decoded-canvas row of the sky/ground swatch pixels: below every panorama
// (max 296 rows) and with a zero line-scroll entry, so per-line offsets of
// line-scroll arenas cannot move the swatch off its pixel.
constexpr int kSwatchRow=382;
std::array<uint8_t, 32768-kBackgroundBankOffset> remapped_background{};
bool have_remapped_background=false;
bool active = false, bitmap_pass = false;
bool drawing_background = false;
bool edge_fills_drawn = false;
int drawing_bank=0,sky_swatch_pixel=0,ground_swatch_pixel=0;
bool gpu_map_remapped = false;
uint32_t engine = 0;
int original_mode = 0;
double draw_x = 0, draw_y = 0;
double source_width=2048, background_fov=120;
double panorama_top=0,panorama_bottom=384;
double vertical_offset=0;
std::array<float,3> sky_color{};
bool sky_color_valid=false;
bool ground_color_valid=false;
double original_projection = 0, wide_projection = 0;
uint64_t frame = 0;
uint64_t scrolling_frames=0, widened_frames=0, last_panorama_frame=0;
int trace_sx=0,trace_sy=0,trace_panorama=-1,trace_agrees=0,trace_differs=0;
CameraPose camera_pose;
uint32_t game_tick=0;
struct NativeScroll {
  bool valid=false;
  unsigned arena=0;
  double x=0,y=0;
  uint32_t tick=0;
  uint64_t sequence=0;
  uint16_t committed_x=0,committed_y=0;
  uint16_t delayed_pitch=0;
};
NativeScroll native_scroll;
NativeScroll pending_native_scroll;
uint64_t capture_sequence=0;
uint16_t next_delayed_pitch=0;
bool next_delayed_valid=false,entry_fresh=false;
uint64_t pitch_matches=0,pitch_mismatches=0,entry_missed=0;
std::array<NativeScroll,2> native_scroll_buffers;
uint32_t color_canvases=0, blit_storage=0;
// Downsampled copies of the decoded canvases for the background blur:
// [priority][level-1], each a native canvas of 1024>>level x 512>>level.
uint32_t blur_canvases=0;
std::array<int,2> blurred_level{};
bool decoding_colors=false, color_blit=false;
unsigned color_priority=0;
std::array<uint8_t,88> saved_color_target{};
std::array<float,4> color_uv{};
// Background blits recorded by the color-decode pass, one queue per native
// priority canvas: sky swatch, ground swatch, panorama (logical 496x384 units,
// UV in color-canvas texels).
struct PendingBlits {
  uint64_t frame=0;int count=0;
  std::array<std::array<double,8>,3> items{};
  void Add(double x0,double y0,double x1,double y1,double u0,double v0,double u1,double v1) {
    if(count<3)items[count++]={x0,y0,x1,y1,u0,v0,u1,v1};
  }
};
std::array<PendingBlits,2> pending;
bool transparent_clear=false;
bool blurring=false;
bool last_pass_panorama=false;
bool ColorTrial() {
  if(vf2::demo::Enabled() || std::getenv("VF2_PANORAMA_COLOR_TEST")!=nullptr)return true;
  return REXCVAR_GET(vf2_background_color_decode);
}

uint16_t LE16(const uint8_t* p) { return uint16_t(p[0]) | uint16_t(p[1]) << 8; }
uint32_t LE32(const uint8_t* p) { return uint32_t(LE16(p)) | uint32_t(LE16(p+2)) << 16; }
float Float(uint8_t* base, uint32_t address) {
  PPCRegister r{}; r.u32 = REX_LOAD_U32(address); return r.f32;
}
void PutFloat(uint8_t* base, uint32_t address, float value) {
  PPCRegister r{}; r.f32 = value; REX_STORE_U32(address, r.u32);
}
double Extent() {
  const double aspect = std::stod(rex::cvar::GetFlagByName("video_mode_width")) /
                        std::stod(rex::cvar::GetFlagByName("video_mode_height"));
  // Bitmap canvas depends on output aspect only, never a mutable 3D camera
  // matrix/FOV. Camera orientation is already represented by its scroll ports.
  return std::clamp(WideExtent(aspect),496.0,1024.0);
}
uint32_t MapAddress(uint8_t* base, int bank, int x, int y) {
  // Read the game's bank table: do not assume row-major quadrant numbering.
  constexpr uint32_t table = 0x824A18D0;
  const uint32_t mx = REX_LOAD_U32(table + bank * 16 + 8);
  const uint32_t my = REX_LOAD_U32(table + bank * 16 + 12);
  return engine + 0x100000 + ((my + y) * 128 + mx + x) * 2;
}
void LoadPanoramas(uint8_t* base) {
  if (!panoramas.empty()) return;
  uint32_t program = 0, data = 0;
  for (uint32_t i = 0; i < 7; ++i) {
    const uint32_t name = REX_LOAD_U32(0x82051C5C + i*8);
    const uint32_t ptr = REX_LOAD_U32(0x82AB6348 + i*4);
    if (!ptr || !name) continue;
    const char* text = reinterpret_cast<const char*>(base + name);
    if (std::string_view(text).ends_with("ic12_15.bin")) program = ptr;
    if (std::string_view(text).ends_with("rom_data.bin")) data = ptr;
  }
  if (!program || !data) return;
  for (int arena = 0; arena < 16; ++arena) {
    const uint32_t descriptor = LE32(base + program + 0x2502c + arena*32 + 16);
    if (descriptor > 0x100000-76) continue;
    const uint8_t* desc = base + program + descriptor;
    const int rows = LE16(desc);
    if (rows < 1 || rows > 64) continue;
    int panels = 18;
    for (int period = 1; period <= 18; ++period) {
      bool repeats = true;
      for (int i = 0; i < 18; ++i)
        repeats &= LE32(desc+4+i*4) == LE32(desc+4+(i%period)*4);
      if (repeats) { panels = period; break; }
    }
    Panorama p{panels*32, rows, {}, arena, descriptor};
    p.tiles.resize(p.columns*p.rows);
    bool valid = true;
    for (int panel = 0; panel < panels; ++panel) {
      const uint32_t ptr = LE32(desc + 4 + panel*4);
      if (ptr < 0x02000000 || uint64_t(ptr-0x02000000)+rows*64 > 0x1000000) { valid=false; break; }
      for (int y = 0; y < rows; ++y)
        for (int x = 0; x < 32; ++x)
          p.tiles[y*p.columns+panel*32+x] = LE16(base+data+(ptr-0x02000000)+(y*32+x)*2);
    }
    if (!valid) continue;
    int index=0;
    for(;index<int(panoramas.size());++index)
      if(panoramas[index].columns==p.columns && panoramas[index].tiles==p.tiles)break;
    arena_panorama[arena]=index;
    if(index==int(panoramas.size()))panoramas.push_back(std::move(p));
  }
  REXLOG_INFO("VF2 panorama: {} complete ROM tilemaps indexed; original GPU tile/palette renderer retained", panoramas.size());
}
void UploadMap(PPCContext& ctx, uint8_t* base) {
  // Match the original Model 2 upload transaction in sub_82144618, including
  // its unpack mode; the texture wrapper copies rows synchronously.
  rex::CallFrame call(ctx);
  call.ctx.r3.u64=1; sub_82102FE0(call.ctx,base);
  call.ctx.r3.u64 = REX_LOAD_U32(engine + 0x118c04);
  call.ctx.r4.u64 = engine+0x100000;
  sub_82199CB8(call.ctx,base);
  call.ctx.r3.u64=0; sub_82102FE0(call.ctx,base);
}
}

// Called at original i960 PC 0x24534, before its first instruction. These
// are the exact delayed camera samples consumed by the native scroll update;
// post-update camera work is already one update newer than the bitmap layer.
void CaptureNativeScroll(PPCContext&, uint8_t* base) {
  pending_native_scroll.valid=false;
  const uint32_t memory=REX_LOAD_U32(0x833BD428);
  if(!memory || memory>0xf0000000)return;
  const uint8_t* native=base+memory;
  if(!native[0x503001] || (LE32(native+0x500068)&(1u<<14)))return;
  const unsigned arena=native[0x500064];
  if(arena>=16 || arena!=native[0x503058])return;
  pending_native_scroll.arena=arena;
  pending_native_scroll.x=0;
  pending_native_scroll.y=0;
  // Entry-time sample, kept as a verified fallback candidate only.
  pending_native_scroll.delayed_pitch=LE16(native+0x503010);
  entry_fresh=true;
  pending_native_scroll.tick=REX_LOAD_U32(0x82E7E38C);
  pending_native_scroll.sequence=++capture_sequence;
  // Eligibility is established here; the actual computed values are captured
  // at 0x24664, before this sample becomes valid for publication.
}

// At 0x24664 the original engine has finished its 32-column streaming loop.
// g2 is the following absolute 16-pixel column, g0 is its computed fine X,
// and g4 is the encoded vertical port. Retain the interpreter's integer result
// plus the fraction of the pitch sample that reproduces it exactly (see
// SelectNativePitch); an unmatched sample would desynchronize the two parts.
void CaptureComputedNativeScroll(PPCContext&, uint8_t* base) {
  const uint32_t cpu=REX_LOAD_U32(0x82AB6754);
  if(!cpu)return;
  const uint32_t memory=REX_LOAD_U32(0x833BD428);
  if(!memory || !base[memory+0x503001] ||
      (LE32(base+memory+0x500068)&(1u<<14)) ||
      pending_native_scroll.arena!=base[memory+0x503058])return;
  const uint32_t fine=REX_LOAD_U32(cpu+64);
  const uint32_t next_column=REX_LOAD_U32(cpu+72);
  const uint32_t vertical=REX_LOAD_U32(cpu+80)&1023;
  pending_native_scroll.x=((next_column-32u)&255u)*16u+((0u-fine)&15u);
  // 0x24580 has already copied the current pitch into 0x503010: that value is
  // the delayed sample consumed by the next invocation. Keep it for then.
  const uint16_t post=LE16(base+memory+0x503010);
  const uint8_t bank=base[memory+0x50303e];
  uint16_t pitch=pending_native_scroll.delayed_pitch;
  if(REXCVAR_GET(vf2_background_pitch_fix)) {
    // Measured (analysis/claude-pitch-trace.csv): the integer port changes in
    // exactly the update where the current pitch at 0x50300C crosses a 16-unit
    // boundary; the entry-time 0x503010 sample is one to two updates older.
    // Prefer the current sample, keep the older ones only as verified fallbacks.
    uint16_t candidates[3]={LE16(base+memory+0x50300c),pending_native_scroll.delayed_pitch,
                            next_delayed_pitch};
    const int count=next_delayed_valid?3:2;
    if(!entry_fresh)++entry_missed;
    if(SelectNativePitch(vertical,bank,candidates,count,pitch)) {
      pending_native_scroll.y=NativeVerticalPhase(vertical,pitch);
      ++pitch_matches;
    } else {
      pending_native_scroll.y=NativeVerticalPhase(vertical,uint16_t(23u*32u)); // integer only
      ++pitch_mismatches;
    }
  } else {
    pending_native_scroll.y=NativeVerticalPhase(vertical,pitch);
  }
  pending_native_scroll.delayed_pitch=pitch;
  next_delayed_pitch=post;next_delayed_valid=true;entry_fresh=false;
  pending_native_scroll.valid=true;
}

// Called at the native return PC 0x2467C, after the scalar/line-scroll ports
// have actually been written. An emulation timeslice may stop between entry
// and this point, so publishing the entry sample would expose future state.
void CommitNativeScroll(PPCContext&, uint8_t* base) {
  pending_native_scroll.committed_x=REX_LOAD_U16(0x833C6436);
  pending_native_scroll.committed_y=REX_LOAD_U16(0x833C643E);
  native_scroll=pending_native_scroll;
}

void SetProjection(double original_x_scale, double wide_x_scale) {
  original_projection=std::abs(original_x_scale); wide_projection=std::abs(wide_x_scale);
}

// Upscaled panorama replacements (data/textures/replacements/arenas, same
// file names as the exported originals). Decoded with WIC on the host,
// copied to guest memory and turned into a native RGBA texture (format 470)
// through the game's own texture constructor sub_8219C8D8, as for the
// palette. Drawn instead of the decoded canvas; wraps horizontally.
struct Replacement { bool tried=false; uint32_t texture=0; int width=0,height=0,image_height=0; double scale=1; };
std::vector<Replacement> replacements;
int queued_panorama=-1;
int queued_tile_origin=0;
// Final per-line X offsets of the background line-scroll table (decoded
// canvas row -> source pixels), captured after alignment and smoothing.
std::array<int16_t,512> line_offsets{};
uint64_t line_offsets_frame=0;
bool line_offsets_valid=false;
constexpr double kLineShiftSign=-1;  // measured: +offset moves the image right
constexpr unsigned kBackPriority=1;  // measured: priority 1 holds the panorama behind 3D
std::filesystem::path ReplacementDirectory() {
  wchar_t module[MAX_PATH];GetModuleFileNameW(nullptr,module,MAX_PATH);
  // data/engine/vf2.exe -> data/textures/replacements/arenas
  return std::filesystem::path(module).parent_path().parent_path()/"textures"/"replacements"/"arenas";
}
bool DecodePng(const std::filesystem::path& file,std::vector<uint8_t>& bgra,int& w,int& h) {
  CoInitializeEx(nullptr,COINIT_MULTITHREADED);
  IWICImagingFactory* factory=nullptr;
  if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory))))return false;
  bool ok=false;IWICBitmapDecoder* decoder=nullptr;IWICBitmapFrameDecode* frame0=nullptr;IWICFormatConverter* conv=nullptr;
  if(SUCCEEDED(factory->CreateDecoderFromFilename(file.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&decoder)) &&
     SUCCEEDED(decoder->GetFrame(0,&frame0)) && SUCCEEDED(factory->CreateFormatConverter(&conv)) &&
     SUCCEEDED(conv->Initialize(frame0,GUID_WICPixelFormat32bppBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))) {
    UINT uw=0,uh=0;conv->GetSize(&uw,&uh);w=int(uw);h=int(uh);
    bgra.resize(size_t(w)*h*4);
    ok=SUCCEEDED(conv->CopyPixels(nullptr,UINT(w*4),UINT(bgra.size()),bgra.data()));
  }
  if(conv)conv->Release();
  if(frame0)frame0->Release();
  if(decoder)decoder->Release();
  factory->Release();
  return ok;
}
void LoadReplacement(PPCContext& ctx,uint8_t* base,int index) {
  if(index<0 || index>=int(panoramas.size()))return;
  if(replacements.size()<panoramas.size())replacements.resize(panoramas.size());
  auto& r=replacements[index];
  if(r.tried)return;
  r.tried=true;
  const auto& p=panoramas[index];
  const auto file=ReplacementDirectory()/fmt::format("arena_{:02d}_rom_{:06x}.png",p.arena,p.descriptor);
  std::vector<uint8_t> pixels;int w=0,h=0;
  if(!std::filesystem::exists(file)) {REXLOG_INFO("VF2 replacement: none for panorama {} ({})",index,file.filename().string());return;}
  if(!DecodePng(file,pixels,w,h)) {REXLOG_ERROR("VF2 replacement: could not decode {}",file.string());return;}
  const int ow=p.columns*8,oh=p.rows*8;
  const double sx=double(w)/ow,sy=double(h)/oh;
  if(w>8192 || h>8192 || std::abs(sx-sy)>0.01 || sx<1) {
    REXLOG_ERROR("VF2 replacement: {} is {}x{}, original {}x{}: aspect/size not supported",file.filename().string(),w,h,ow,oh);return;
  }
  // Two extra rows after the image: row h = ground fill, row h+1 = sky fill.
  // Their colour is the average of the image's own bottom/top row, so the
  // extensions above and below match the upscaled art (its colours differ
  // slightly from the original palette).
  const auto row_average=[&](int y) {
    double sum[3]{};int n=0;
    for(int x=0;x<w;++x){const uint8_t* q=&pixels[(size_t(y)*w+x)*4];if(q[3]<128)continue;
      for(int c=0;c<3;++c)sum[c]+=q[c];++n;}
    std::array<uint8_t,4> c{0,0,0,255};
    if(n)for(int k=0;k<3;++k)c[k]=uint8_t(std::lround(sum[k]/n));
    return c;
  };
  const auto ground=row_average(h-1),sky=row_average(0);
  for(int y=0;y<2;++y)for(int x=0;x<w;++x) {
    const auto& c=y==0?ground:sky;
    pixels.insert(pixels.end(),c.begin(),c.end());
  }
  const int texture_h=h+2;
  const uint32_t guest=REX_KERNEL_MEMORY()->SystemHeapAlloc(uint32_t(pixels.size()));
  if(!guest){REXLOG_ERROR("VF2 replacement: no guest memory for {}x{}",w,h);return;}
  std::memcpy(base+guest,pixels.data(),pixels.size());
  rex::CallFrame call(ctx);
  call.ctx.r3.u64=0x26000100u+uint32_t(index);call.ctx.r4.u64=470;
  call.ctx.r5.u64=uint32_t(w);call.ctx.r6.u64=uint32_t(texture_h);call.ctx.r7.u64=guest;call.ctx.r8.u64=uint32_t(pixels.size());
  sub_8219C8D8(call.ctx,base);
  r.texture=call.ctx.r3.u32;r.width=w;r.height=texture_h;r.image_height=h;r.scale=sx;
  REXLOG_INFO("VF2 replacement fills: sky {:02X}{:02X}{:02X} ground {:02X}{:02X}{:02X} (BGR)",sky[0],sky[1],sky[2],ground[0],ground[1],ground[2]);
  REXLOG_INFO("VF2 replacement: {} loaded {}x{} ({}x scale) texture={:08X}",file.filename().string(),w,h,sx,r.texture);
}
const Replacement* ActiveReplacement(int index) {
  if(!REXCVAR_GET(vf2_background_replacements) || index<0 || index>=int(replacements.size()))return nullptr;
  const auto& r=replacements[index];
  return r.texture?&r:nullptr;
}
void Begin(PPCContext& ctx, uint8_t* base) {
  ++frame; active=false; bitmap_pass=true;
  game_tick=REX_LOAD_U32(0x82E7E38C);
  trace_panorama=-1; trace_agrees=0; trace_differs=0;
  engine=REX_LOAD_U32(0x82AB6CF8);
  if (!engine || !REX_LOAD_U32(engine+0x118c04)) return;
  struct RestoreOnFallback {
    PPCContext& ctx; uint8_t* base;
    ~RestoreOnFallback() {
      if(!active && gpu_map_remapped) {
        UploadMap(ctx,base); gpu_map_remapped=false;
      }
    }
  } restore{ctx,base};
  LoadPanoramas(base);
  static bool bank_log=false;
  if(!bank_log && !panoramas.empty()) {
    bank_log=true;
    for(int i=0;i<4;++i) REXLOG_INFO("VF2 panorama bank {} map({}, {}) address={:08X}", i,
      REX_LOAD_U32(0x824A18D0+i*16+8),REX_LOAD_U32(0x824A18D0+i*16+12),MapAddress(base,i,0,0));
    const uint32_t tex=REX_LOAD_U32(engine+0x118c04);
    for(unsigned slot=0;slot<3;++slot) {
      const uint32_t texture=REX_LOAD_U32(engine+0x118c00+slot*4);
      REXLOG_INFO("VF2 texture probe: slot={} size={}x{} format={}",slot,REX_LOAD_U32(texture+12),REX_LOAD_U32(texture+16),REX_LOAD_U32(texture+24));
    }
    REXLOG_INFO("VF2 panorama map texture {}x{} format={}",REX_LOAD_U32(tex+12),REX_LOAD_U32(tex+16),REX_LOAD_U32(tex+24));
  }
  rex::CallFrame query(ctx); query.ctx.r3.u64=1;
  __imp__sub_82188848(query.ctx,base); original_mode=query.ctx.r3.u32;
  if (original_mode!=1 && original_mode!=2) {camera_pose.Invalidate();return;}
  ++scrolling_frames;
  query.ctx.r3.u64=1; sub_821888F0(query.ctx,base); const int sx=query.ctx.r3.s32;
  query.ctx.r3.u64=1; sub_821889E8(query.ctx,base); const int sy=query.ctx.r3.s32;
  trace_sx=sx; trace_sy=sy;
  // Identify the arena through the original loader and use the camera sample
  // captured at the native scroll routine itself. Ring tags/bitmap patterns are
  // deliberately not used as temporal phase estimates: their window changes
  // at 512-pixel boundaries and line-scrolling arenas disable the scalar port.
  const uint32_t memory=REX_LOAD_U32(0x833BD428);
  if(!memory || memory>0xf0000000)return;
  const uint8_t* native=base+memory;
  const unsigned arena=native[0x503058];
  // The scalar ports, line-scroll tables and 3D rendering retain the original
  // double-buffer selection. The writer flips in 0x82143528; getters use the
  // independently delayed reader. Select metadata latched with that same bank.
  const unsigned reader=REX_LOAD_U8(0x833DD4D1);
  if(reader>=native_scroll_buffers.size())return;
  const NativeScroll& rendered_scroll=native_scroll_buffers[reader];
  const uint32_t age=game_tick-rendered_scroll.tick;
  if(!rendered_scroll.valid || !native[0x503001] ||
      (LE32(native+0x500068)&(1u<<14)) || arena!=rendered_scroll.arena ||
      arena!=native[0x500064] || age>2 ||
      arena>=arena_panorama.size() || arena_panorama[arena]<0)return;
  Candidate found{arena_panorama[arena],0,0};
  const auto& p=panoramas[found.panorama];
  double phase_x=Wrap(rendered_scroll.x,p.columns*8);
  const int vertical_period=original_mode==1 ? 1024 : 512;
  const double vertical_phase=REXCVAR_GET(vf2_background_smooth_scroll)
      ? rendered_scroll.y : std::ceil(rendered_scroll.y);
  double phase_y=std::remainder(vertical_phase,double(vertical_period));
  const int offset_x=int(phase_x)+sx,offset_y=0;
  if(const char* path=std::getenv("VF2_PANORAMA_TRACE")) {
    static std::ofstream native_trace(std::string(path)+".native.csv");
    if(frame%30==0) {
      native_trace<<std::setprecision(15)<<frame<<','<<arena<<','<<sx<<','<<sy<<','
          <<rendered_scroll.x<<','<<rendered_scroll.y<<','<<age<<','
          <<rendered_scroll.sequence<<','<<reader<<','
          <<int(REX_LOAD_U8(0x833DD4D0))<<','
          <<rendered_scroll.committed_x<<','<<rendered_scroll.committed_y
          <<','<<rendered_scroll.delayed_pitch
          <<'\n';native_trace.flush();
    }
  }
  trace_panorama=found.panorama;
  trace_agrees=int(rendered_scroll.sequence);trace_differs=0;
  camera_pose.Capture(game_tick,found.panorama,phase_x,phase_y);
  phase_x=camera_pose.x;phase_y=camera_pose.y;
  // Deterministic subtexel sampling probe in the isolated demo lab only.
  if(vf2::demo::Enabled())if(const char* offset=std::getenv("VF2_DEMO_SAMPLE_Y"))
    phase_y+=std::atof(offset);
  if(const char* path=std::getenv("VF2_PANORAMA_TRACE")) {
    static std::ofstream sync(std::string(path)+".sync.csv");
    const uint32_t camera=LE32(native+0x500814);
    sync<<std::setprecision(15)<<frame<<','<<game_tick<<','<<reader<<','
        <<rendered_scroll.sequence<<','<<rendered_scroll.x<<','<<rendered_scroll.y<<','
        <<phase_x<<','<<phase_y<<','<<LE16(native+0x50300c)<<','<<LE16(native+0x503010)<<','
        <<(camera>=0x500000 && camera<0x600000 ? LE16(native+camera+0x24) : 0)<<'\n';
    if(frame%60==0)sync.flush();
  }
  // Explicit regression mode only: sweep all source angles on the GPU while
  // leaving the gameplay camera/geometry untouched. Never enabled by launcher.
  static const bool sweep=std::getenv("VF2_PANORAMA_SWEEP")!=nullptr;
  if(sweep) phase_x+=int((uint64_t(game_tick)*8)%uint64_t(p.columns*8));
  const double horizon=std::clamp(REXCVAR_GET(vf2_background_horizon),-192.0,192.0);
  vertical_offset=horizon;
  source_width=p.columns*8;
  panorama_top=-phase_y;panorama_bottom=panorama_top+p.rows*8;
  background_fov=std::clamp(REXCVAR_GET(vf2_background_fov),40.0,140.0);
  const auto xwin=MakeAxisWindow(0,-phase_x,256);
  queued_panorama=found.panorama;queued_tile_origin=xwin.tile_origin;
  if(REXCVAR_GET(vf2_background_replacements))LoadReplacement(ctx,base,found.panorama);
  // Sample the complete top row through the live character/palette decoding.
  // Use the modal opaque color, so sparse clouds do not tint a flat sky edge.
  const auto sample_edge=[&](int row,int& pixel,std::array<float,3>& rgb_color) {
    std::unordered_map<uint32_t,int> histogram;
    uint32_t dominant=0; int dominant_count=0;
    for(int x=0;x<p.columns*8;++x) {
      const auto attr=p.tiles[(row/8)*p.columns+x/8];
      const unsigned code=attr&0x3fff;
      const uint8_t pen=base[engine+(code/128)*8192+(row%8)*1024+(code%128)*8+x%8];
      if(!pen)continue;
      const uint8_t* color=base+engine+0x108000+(((attr>>7)&255)*16+pen)*4;
      const uint32_t rgb=(uint32_t(color[2])<<16)|(uint32_t(color[1])<<8)|color[0];
      const int count=++histogram[rgb];
      if(count>dominant_count){dominant_count=count;dominant=rgb;pixel=x;}
    }
    for(int channel=0;channel<3;++channel)
      rgb_color[channel]=float((dominant>>(16-channel*8))&255)/255.0f;
    return dominant_count>0;
  };
  sky_color_valid=sample_edge(0,sky_swatch_pixel,sky_color);
  std::array<float,3> ground_color{};
  ground_color_valid=sample_edge(p.rows*8-1,ground_swatch_pixel,ground_color);
  static std::ofstream trace;
  static bool trace_opened=false;
  if(!trace_opened) {
    trace_opened=true;
    if(const char* path=std::getenv("VF2_PANORAMA_TRACE")) {
      trace.open(path); trace<<std::setprecision(15)<<"frame,panorama,mode,phase_x,phase_y,scroll_x,scroll_y,extent,fov,sky_r,sky_g,sky_b,tick,pose_samples,seconds,horizon\n";
    }
  }
  if(trace) {
    trace<<frame<<','<<found.panorama<<','<<original_mode<<','
         <<Wrap(phase_x,p.columns*8)<<','<<phase_y<<','
         <<sx<<','<<sy<<','<<Extent()<<','<<background_fov<<','
         <<sky_color[0]<<','<<sky_color[1]<<','<<sky_color[2]<<','<<game_tick<<','<<camera_pose.samples<<','
         <<std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count()<<','<<horizon<<'\n';
    if(frame%60==0) trace.flush();
  }
  // Keep the entire short panorama in atlas height. A zoomed-out background
  // may expose rows outside the old visible vertical cache during intro tilts.
  draw_x=xwin.screen_origin; draw_y=-phase_y;
  if(!REXCVAR_GET(vf2_background_smooth_scroll)) {
    draw_x=std::round(draw_x);draw_y=std::round(draw_y);
  }
  std::memcpy(original_map.data(),base+engine+0x100000,original_map.size());
  // Cache two overlapping 512x512 banks from the complete panorama, using the original tile
  // attributes, character GPU memory, palette, priority and quality shaders.
  // This extends the original streaming renderer; it does not repeat its old
  // 4:3 crop, stretch sprites, or synthesize mirrored edge pixels.
  for (int y=0;y<64;++y) for(int x=0;x<128;++x) {
    // Overlap the decoded banks by two guard tiles. Linear filtering then
    // samples real panorama neighbors on both sides of their shared edge,
    // rather than unrelated HUD pixels in the next atlas quadrant.
    const int source_x=x*8-(x>=64 ? 2*kPanoramaBankGuard : 0);
    const int px=int(Wrap(xwin.tile_origin+source_x,p.columns*8))/8;
    const int py=y;
    // The panorama repeats horizontally only. Repeating its short vertical
    // strip folds the skyline back into view during scripted camera tilts.
    const uint16_t attr=py>=0 && py<p.rows ? p.tiles[py*p.columns+px] : 0;
    const uint32_t a=MapAddress(base,1+2*(x/64),x%64,y);
    base[a]=uint8_t(attr); base[a+1]=uint8_t(attr>>8);
  }
  // An offscreen tile is a palette-accurate sky swatch. Drawing it through
  // the original background shader also preserves every quality/color mode;
  // a raw RGB clear alone bypasses that shader and creates a visible edge.
  if(sky_color_valid) {
    const uint16_t attr=p.tiles[sky_swatch_pixel/8];
    const uint32_t a=MapAddress(base,1,0,63);
    base[a]=uint8_t(attr);base[a+1]=uint8_t(attr>>8);
  }
  if(ground_color_valid) {
    const uint16_t attr=p.tiles[(p.rows-1)*p.columns+ground_swatch_pixel/8];
    const uint32_t a=MapAddress(base,1,0,62);
    base[a]=uint8_t(attr);base[a+1]=uint8_t(attr>>8);
  }
  // Keep this pass's background banks (map rows 64..127 = banks 1 and 3) for
  // every later upload of the map texture until the next pass, see the
  // sub_82199CB8 hook below.
  std::memcpy(remapped_background.data(),base+engine+0x100000+kBackgroundBankOffset,remapped_background.size());
  have_remapped_background=true;
  UploadMap(ctx,base); active=true; gpu_map_remapped=true; ++widened_frames;
  last_panorama_frame=frame;
  static int previous=-1;
  if (previous!=found.panorama) {
    previous=found.panorama;
    REXLOG_INFO("VF2 panorama active: {}x{} mode={} phase=({}, {}) logical={} originalM00={} wideM00={}",
      p.columns*8,p.rows*8,original_mode,offset_x-sx,offset_y-sy,Extent(),original_projection,wide_projection);
  }
}
void End(PPCContext& ctx,uint8_t* base) {
  static std::ofstream status;
  static bool opened=false;
  if(!opened) {
    opened=true;
    if(const char* path=std::getenv("VF2_PANORAMA_TRACE")) {
      status.open(std::string(path)+".status.csv");
      status<<"frame,active,panorama,mode,sx,sy,agrees,differs\n";
    }
  }
  if(status) {
    status<<frame<<','<<active<<','<<trace_panorama<<','<<original_mode<<','<<trace_sx<<','<<trace_sy<<','<<trace_agrees<<','<<trace_differs<<'\n';
    if(frame%60==0)status.flush();
  }
  // Restore CPU state only. Uploading here would overwrite cached GPU storage
  // while the queued atlas draws (especially bank 3) still consume it. A
  // following static/menu frame restores the GPU map before its own draws.
  if(active) {
    std::memcpy(base+engine+0x100000,original_map.data(),original_map.size());
  } else {
    pending[0].count=pending[1].count=0;  // static/menu pass: no panorama
    have_remapped_background=false;
  }
  last_pass_panorama=active;
  if(frame%600==0) REXLOG_INFO("VF2 panorama coverage: frame={} scrolling={} widened={} active={} pitch_match={} pitch_mismatch={} entry_missed={}",
      frame,scrolling_frames,widened_frames,active,pitch_matches,pitch_mismatches,entry_missed);
  bitmap_pass=false;
}
void FlushPending(PPCContext& ctx,uint8_t* base,unsigned priority) {
  auto& queue=pending[priority];
  if(!queue.count || !color_canvases)return;
  rex::CallFrame call(ctx);
  // Bind the decoded color canvas with LINEAR min/mag (native 428).
  constexpr uint32_t filter=428;
  const int level=blurred_level[priority];
  const auto bind=[&](bool blurred) {
    const uint32_t canvas=blurred ? blur_canvases+(priority*3+level-1)*128 : color_canvases+priority*128;
    call.ctx.r3.u64=REX_LOAD_U32(canvas+36);call.ctx.r4.u64=filter;
    sub_821442A0(call.ctx,base);
  };
  bind(false);
  // The native generic blitter installs the UI projection, independently of
  // the bound target: map logical bitmap units to UI canvas units.
  const double ui_width=REX_LOAD_U32(0x824A0498+36);
  const double ui_height=REX_LOAD_U32(0x824A0498+40);
  const double margin=(Extent()-496)/2;
  const PendingBlits* source=&queue;
  for(int i=0;i<source->count;++i) {
    auto [x0,y0,x1,y1,u0,v0,u1,v1]=source->items[i];
    if(x1<=x0 || y1<=y0)continue;
    const double ui_left=vf2::display::UiLeft(ui_width);
    x0=ui_left+(x0+margin)*ui_width/Extent();x1=ui_left+(x1+margin)*ui_width/Extent();
    y0*=ui_height/384;y1*=ui_height/384;
    // Replacement fills: the sky (u 1000.5) and ground (u 1001.5) swatch
    // items sample the averaged rows appended to the replacement texture.
    if(source==&queue && priority==kBackPriority && i<source->count-1 && (u0==1000.5 || u0==1001.5))
      if(const auto* r=ActiveReplacement(queued_panorama)) {
        call.ctx.r3.u64=r->texture;call.ctx.r4.u64=filter;sub_821442A0(call.ctx,base);
        const float row=float(r->image_height+(u0==1001.5?0:1))+.5f;
        PutFloat(base,blit_storage,float(r->width));PutFloat(base,blit_storage+4,float(r->height));
        PutFloat(base,blit_storage+16,float(x0));PutFloat(base,blit_storage+20,float(y0));
        PutFloat(base,blit_storage+32,float(x1));PutFloat(base,blit_storage+36,float(y1));
        color_uv={float(r->width/2),row,float(r->width/2),row};color_blit=true;
        call.ctx.r3.u64=blit_storage;call.ctx.r4.u64=blit_storage+16;
        call.ctx.r5.u64=blit_storage+32;sub_82144340(call.ctx,base);color_blit=false;
        bind(false);
        continue;
      }
    // Replacement: the panorama item samples the upscaled texture. Canvas
    // texel u maps to panorama x = tile_origin + u (wrapping), row v = v.
    // Only the priority whose canvas is composited behind the 3D scene; the
    // other one is drawn over it and an opaque image there hides the arena.
    if(i==source->count-1 && source==&queue && priority==kBackPriority)
      if(const auto* r=ActiveReplacement(queued_panorama)) {
        const double width=panoramas[queued_panorama].columns*8.0;
        call.ctx.r3.u64=r->texture;call.ctx.r4.u64=filter;sub_821442A0(call.ctx,base);
        const double s0=Wrap(queued_tile_origin+u0,width);
        const double span=u1-u0;
        const auto piece=[&](double from,double to,double sx0) {  // canvas u range
          const double px0=x0+(x1-x0)*(from-u0)/span,px1=x0+(x1-x0)*(to-u0)/span;
          PutFloat(base,blit_storage,float(r->width));PutFloat(base,blit_storage+4,float(r->height));
          PutFloat(base,blit_storage+16,float(px0));PutFloat(base,blit_storage+20,float(y0));
          PutFloat(base,blit_storage+32,float(px1));PutFloat(base,blit_storage+36,float(y1));
          color_uv={float(sx0*r->scale),float(v0*r->scale),float((sx0+to-from)*r->scale),float(v1*r->scale)};
          color_blit=true;
          call.ctx.r3.u64=blit_storage;call.ctx.r4.u64=blit_storage+16;
          call.ctx.r5.u64=blit_storage+32;sub_82144340(call.ctx,base);color_blit=false;
        };
        const auto wrapped=[&](double ya,double yb,double va,double vb,double shift) {
          const double s1=Wrap(queued_tile_origin+u0+shift,width);
          const auto strip=[&](double from,double to,double sx0) {
            const double px0=x0+(x1-x0)*(from-u0)/span,px1=x0+(x1-x0)*(to-u0)/span;
            PutFloat(base,blit_storage,float(r->width));PutFloat(base,blit_storage+4,float(r->height));
            PutFloat(base,blit_storage+16,float(px0));PutFloat(base,blit_storage+20,float(ya));
            PutFloat(base,blit_storage+32,float(px1));PutFloat(base,blit_storage+36,float(yb));
            color_uv={float(sx0*r->scale),float(va*r->scale),float((sx0+to-from)*r->scale),float(vb*r->scale)};
            color_blit=true;
            call.ctx.r3.u64=blit_storage;call.ctx.r4.u64=blit_storage+16;
            call.ctx.r5.u64=blit_storage+32;sub_82144340(call.ctx,base);color_blit=false;
          };
          if(s1+span<=width)strip(u0,u1,s1);
          else {const double cut=u0+(width-s1);strip(u0,cut,s1);strip(cut,u1,0);}
        };
        // Line-scroll arenas: apply the same per-row offsets as the native
        // tile shader (canvas row v reads source x + offset), one strip per
        // run of rows sharing an offset.
        bool wave=false;
        if(line_offsets_valid && line_offsets_frame+2>=frame && REXCVAR_GET(vf2_background_replacement_wave))
          for(int v=int(v0);v<int(v1) && v<512;++v)wave|=line_offsets[v]!=0;
        if(!wave) {
          if(s0+span<=width)piece(u0,u1,s0);
          else {const double cut=u0+(width-s0);piece(u0,cut,s0);piece(cut,u1,0);}
        } else {
          const double dy=(y1-y0)/(v1-v0);
          for(int v=int(v0);v<int(v1);) {
            const int o=v<512?line_offsets[v]:0;int e=v+1;
            while(e<int(v1) && (e<512?line_offsets[e]:0)==o)++e;
            wrapped(y0+(v-v0)*dy,y0+(e-v0)*dy,v,e,kLineShiftSign*o);
            v=e;
          }
        }
        continue;
      }
    // The panorama (last item; swatches are single texels) uses the blur copy.
    const bool blurred=level && i==source->count-1;
    if(blurred)bind(true);
    const double k=blurred?double(1<<level):1.0;
    PutFloat(base,blit_storage,float(1024/k));PutFloat(base,blit_storage+4,float(512/k));
    PutFloat(base,blit_storage+16,float(x0));PutFloat(base,blit_storage+20,float(y0));
    PutFloat(base,blit_storage+32,float(x1));PutFloat(base,blit_storage+36,float(y1));
    color_uv={float(u0/k),float(v0/k),float(u1/k),float(v1/k)};color_blit=true;
    call.ctx.r3.u64=blit_storage;call.ctx.r4.u64=blit_storage+16;
    call.ctx.r5.u64=blit_storage+32;sub_82144340(call.ctx,base);color_blit=false;
  }
}
bool Draw(PPCContext& ctx,uint8_t* base) {
  if (!active || !bitmap_pass || ctx.r3.u32!=1) return false;
  if(decoding_colors) {
    drawing_background=true;edge_fills_drawn=false;
    for(int bank:{1,3}) {
      drawing_bank=bank;rex::CallFrame call(ctx);call.ctx.r3.u64=bank;
      call.ctx.f1.f64=bank==3?kPanoramaBankStep:0;call.ctx.f2.f64=0;
      call.ctx.f3.f64=0;call.ctx.f4.f64=0;sub_821456B0(call.ctx,base);
    }
    drawing_background=false;
    rex::CallFrame call(ctx);
    sub_820F98E8(call.ctx,base);
    call.ctx.r3.u64=canvases[color_priority];sub_82176698(call.ctx,base);
    sub_82283630(call.ctx,base);sub_82176E38(call.ctx,base);
    std::memcpy(base+color_canvases+color_priority*128,base+canvases[color_priority],88);
    decoding_colors=false;
    // Optional blur: draw the decoded canvas into a smaller target with LINEAR
    // filtering; FlushPending then magnifies that copy (also LINEAR).
    blurred_level[color_priority]=0;
    if(const int level=std::clamp(REXCVAR_GET(vf2_background_blur),0,3); level && blur_canvases) {
      const uint32_t target=blur_canvases+(color_priority*3+level-1)*128;
      std::memcpy(base+canvases[color_priority],base+target,88);
      blurring=true;
      call.ctx.r3.u64=color_priority;call.ctx.r4.u64=0;call.ctx.r5.u64=1;
      __imp__sub_82144050(call.ctx,base);
      // The pass setup keeps a viewport larger than this small target.
      call.ctx.r3.u64=0;call.ctx.r4.u64=0;call.ctx.r5.u64=1024>>level;call.ctx.r6.u64=512>>level;
      sub_820FDC50(call.ctx,base);
      call.ctx.r3.u64=REX_LOAD_U32(color_canvases+color_priority*128+36);call.ctx.r4.u64=428;
      sub_821442A0(call.ctx,base);
      // The generic blitter installs the UI projection: cover the whole
      // (viewport-sized) target in UI canvas units.
      const double ui_width=REX_LOAD_U32(0x824A0498+36);
      const double ui_height=REX_LOAD_U32(0x824A0498+40);
      PutFloat(base,blit_storage,1024);PutFloat(base,blit_storage+4,512);
      const double ui_left=vf2::display::UiLeft(ui_width);
      PutFloat(base,blit_storage+16,float(ui_left));PutFloat(base,blit_storage+20,0);
      PutFloat(base,blit_storage+32,float(ui_left+ui_width));PutFloat(base,blit_storage+36,float(ui_height));
      color_uv={0,0,1024,512};color_blit=true;
      call.ctx.r3.u64=blit_storage;call.ctx.r4.u64=blit_storage+16;
      call.ctx.r5.u64=blit_storage+32;sub_82144340(call.ctx,base);color_blit=false;
      sub_820F98E8(call.ctx,base);
      call.ctx.r3.u64=canvases[color_priority];sub_82176698(call.ctx,base);
      sub_82283630(call.ctx,base);sub_82176E38(call.ctx,base);
      std::memcpy(base+target,base+canvases[color_priority],88);
      blurring=false;
      blurred_level[color_priority]=level;
    }
    std::memcpy(base+canvases[color_priority],saved_color_target.data(),88);
    transparent_clear=REXCVAR_GET(vf2_background_linear_composite);
    call.ctx.r3.u64=color_priority;call.ctx.r4.u64=0;call.ctx.r5.u64=1;
    __imp__sub_82144050(call.ctx,base);
    transparent_clear=false;
    const double margin=(Extent()-496)/2;
    const double scale=PanoramaScale(Extent(),source_width,background_fov);
    const double bottom=PanoramaVertical(panorama_bottom,scale,vertical_offset);
    auto& queue=pending[color_priority];
    queue={};queue.frame=frame;
    if(sky_color_valid)queue.Add(-margin,0,496+margin,384,1000.5,kSwatchRow+.5,1000.5,kSwatchRow+.5);
    if(ground_color_valid)queue.Add(-margin,std::clamp(bottom,0.0,384.0),496+margin,384,1001.5,kSwatchRow+.5,1001.5,kSwatchRow+.5);
    queue.Add(PanoramaScreen(draw_x+8-248,Extent(),source_width,background_fov),
              PanoramaVertical(panorama_top,scale,vertical_offset),
              PanoramaScreen(draw_x+1000-248,Extent(),source_width,background_fov),bottom,
              8,0,1000,panorama_bottom-panorama_top);
    // Full output resolution path: leave this canvas transparent where the
    // panorama goes and draw it in Composite, so the 384-line canvas (with
    // its point-filtered HUD) is not what magnifies the background.
    if(!REXCVAR_GET(vf2_background_linear_composite)) {
      FlushPending(ctx,base,color_priority);
    }
    // Reestablish the original palette pipeline for the subsequent HUD pass.
    call.ctx.r3.u64=color_priority;call.ctx.r4.u64=1;call.ctx.r5.u64=1;
    __imp__sub_82144050(call.ctx,base);
    call.ctx.r3.u64=0x82AB6D08;call.ctx.r4.u64=541;sub_82143688(call.ctx,base);
    call.ctx.r3.u64=299;sub_820F8428(call.ctx,base);
    call.ctx.r3.u64=303;sub_820F8428(call.ctx,base);
    return true;
  }
  drawing_background=true;
  edge_fills_drawn=false;
  for(int bank : {1,3}) {
    drawing_bank=bank;
    rex::CallFrame call(ctx);
    call.ctx.r3.u64=bank;
    call.ctx.f1.f64=draw_x+(bank==3?double(kPanoramaBankStep):0.0);
    call.ctx.f2.f64=draw_y;
    call.ctx.f3.f64=0; call.ctx.f4.f64=0;
    sub_821456B0(call.ctx,base);
  }
  drawing_background=false;
  return true;
}
}

// Associate full panorama phase with the very register bank receiving the
// native latch. This also follows line-scroll arenas whose scalar X port is
// disabled, without introducing an arbitrary frame delay or phase filter.
REX_HOOK_RAW(sub_82188C08) {
  __imp__sub_82188C08(ctx,base);
  using namespace vf2::panorama;
  const unsigned writer=REX_LOAD_U8(0x833DD4D0);
  if(writer<native_scroll_buffers.size())native_scroll_buffers[writer]=native_scroll;
}

REX_HOOK_RAW(sub_82188848) {
  const uint32_t layer=ctx.r3.u32;
  const bool background=vf2::panorama::active && vf2::panorama::bitmap_pass;
  __imp__sub_82188848(ctx,base);
  if(background && layer==1) ctx.r3.u64=2;
  if(background && layer==3) ctx.r3.u64=0;
}
REX_HOOK_RAW(sub_821441D8) {
  // While the color canvas is bound, only the widened panorama may draw into
  // it. Native layer quads drawn in the same priority pass (seen on tilting
  // line-scroll arenas) landed there at native 4:3 positions: a vertically
  // offset copy of the panorama on the left of the screen.
  if(vf2::panorama::decoding_colors && !vf2::panorama::drawing_background) {
    static uint64_t skipped=0;
    if(skipped++%600==0)REXLOG_INFO("VF2 panorama: native quad skipped inside color decode (#{})",skipped);
    return;
  }
  if(vf2::panorama::drawing_background) {
    using namespace vf2::panorama;
    // Original CPU rejection uses a 512-square limit; GPU viewport clipping
    // handles the widened target and the translated adjacent bank correctly.
    const uint32_t vertices=ctx.r3.u32;
    std::array<uint8_t,80> saved;
    std::memcpy(saved.data(),base+vertices,saved.size());
    const double left=Float(base,vertices),right=Float(base,vertices+24);
    const double u0=Float(base,vertices+48),u1=Float(base,vertices+64);
    const double top=Float(base,vertices+4),bottom=Float(base,vertices+16);
    const double v0=Float(base,vertices+52),v1=Float(base,vertices+60);
    const auto submit=[&] {
      rex::CallFrame call(ctx);
      call.ctx.r3.u64=3;call.ctx.r4.u64=353;call.ctx.r5.u64=0;call.ctx.r6.u64=vertices;
      sub_820F7D90(call.ctx,base);
      call.ctx.r3.u64=2;call.ctx.r4.u64=353;call.ctx.r5.u64=0;call.ctx.r6.u64=vertices+48;
      sub_820F7E00(call.ctx,base);
      call.ctx.r3.u64=9;call.ctx.r4.u64=0;call.ctx.r5.u64=4;
      sub_820F8508(call.ctx,base);
    };
    if(decoding_colors) {
      // Decode at integer 1:1 positions, before camera translation/FOV. The
      // resulting target contains real palette colors, not encoded indices.
      if(drawing_bank==1) {
        const auto swatch=[&](int destination,int pixel,double atlas_y) {
          const float u=float(u0+(u1-u0)*((pixel%8)+.5)/512.0);
          const float vv=float(v0+(v1-v0)*atlas_y/512.0);
          for(int v=0;v<4;++v) {
            PutFloat(base,vertices+v*12,float(destination+(v<2?0:1)));
            PutFloat(base,vertices+v*12+4,float(kSwatchRow+((v==0||v==3)?0:1)));
            PutFloat(base,vertices+48+v*8,u);PutFloat(base,vertices+52+v*8,vv);
          }submit();
        };
        if(sky_color_valid)swatch(1000,sky_swatch_pixel,504.5);
        if(ground_color_valid)swatch(1001,ground_swatch_pixel,503.5);
      }
      const double a=left+kPanoramaBankGuard,b=right-kPanoramaBankGuard;
      const double h=panorama_bottom-panorama_top;
      for(int v=0;v<4;++v) {
        const double x=v<2?a:b,y=(v==0||v==3)?0:h;
        PutFloat(base,vertices+v*12,float(x));PutFloat(base,vertices+v*12+4,float(y));
        PutFloat(base,vertices+48+v*8,float(u0+(u1-u0)*(x-left)/(right-left)));
        PutFloat(base,vertices+52+v*8,float(v0+(v1-v0)*(y-top)/(bottom-top)));
      }submit();std::memcpy(base+vertices,saved.data(),saved.size());return;
    }
    if(!edge_fills_drawn && drawing_bank==1 && (sky_color_valid || ground_color_valid)) {
      edge_fills_drawn=true;
      const float margin=float((Extent()-496.0)/2.0);
      const double scale=PanoramaScale(Extent(),source_width,background_fov);
      const float lower_edge=float(std::clamp(PanoramaVertical(panorama_bottom,scale,vertical_offset),0.0,384.0));
      const auto fill=[&](float y0,float y1,int pixel,double atlas_y) {
        if(y1<=y0)return;
        const float u=float(u0+(u1-u0)*((pixel%8)+0.5)/512.0);
        const float vv=float(v0+(v1-v0)*atlas_y/512.0);
        for(int v=0;v<4;++v) {
          PutFloat(base,vertices+v*12,v<2?-margin:496+margin);
          PutFloat(base,vertices+v*12+4,(v==0||v==3)?y0:y1);
          PutFloat(base,vertices+48+v*8,u);
          PutFloat(base,vertices+52+v*8,vv);
        }
        submit();
      };
      // Keep transparent sky pixels on the upper swatch, but extend the lower
      // off-texture region with the arena's own bottom row through the same
      // quality/palette shader. Both boundaries use the panorama transform.
      if(sky_color_valid)fill(0,ground_color_valid?lower_edge:384,sky_swatch_pixel,504.5);
      if(ground_color_valid)fill(lower_edge,384,ground_swatch_pixel,503.5);
    }
    const double clipped_top=std::max(top,panorama_top);
    const double clipped_bottom=std::min(bottom,panorama_bottom);
    if(clipped_bottom<=clipped_top) {
      std::memcpy(base+vertices,saved.data(),saved.size());return;
    }
    const double half_source=source_width*background_fov/720.0;
    const double first=std::max(left+kPanoramaBankGuard,248.0-half_source);
    const double last=std::min(right-kPanoramaBankGuard,248.0+half_source);
    // One uniformly scaled rectangle per atlas bank. The bitmap was already
    // projected by the original engine; do not apply a second curved mapping.
    for(double a=first;a<last;) {
      const double b=last;
      for(int v=0;v<4;++v) {
        const double x=v<2?a:b;
        PutFloat(base,vertices+v*12,float(PanoramaScreen(x-248,Extent(),source_width,background_fov)));
        PutFloat(base,vertices+48+v*8,float(u0+(u1-u0)*(x-left)/(right-left)));
        // Xenos quad-list order is TL, BL, BR, TR (not triangle-strip order).
        const double y=(v==0 || v==3)?clipped_top:clipped_bottom;
        PutFloat(base,vertices+v*12+4,float(PanoramaVertical(y,
            PanoramaScale(Extent(),source_width,background_fov),vertical_offset)));
        PutFloat(base,vertices+52+v*8,float(v0+(v1-v0)*(y-top)/(bottom-top)));
      }
      if(drawing_bank==1)if(const char* path=std::getenv("VF2_PANORAMA_TRACE")) {
        static std::ofstream geometry(std::string(path)+".geometry.csv");
        geometry<<std::setprecision(15)<<frame<<','<<draw_y<<','<<top<<','
            <<Float(base,vertices+4)<<','<<Float(base,vertices+52)<<'\n';
        if(frame%60==0)geometry.flush();
      }
      submit();
      a=b;
    }
    std::memcpy(base+vertices,saved.data(),saved.size());
  } else if(vf2::panorama::bitmap_pass) {
    // Widening the target must not expose the old 16-pixel overscan or stale
    // offscreen HUD/logo tiles. Preserve their authored 0..496 horizontal clip.
    using namespace vf2::panorama;
    const uint32_t vertices=ctx.r3.u32;
    const float left=Float(base,vertices),right=Float(base,vertices+24);
    if(right<=0 || left>=496) return;
    if(left<0 || right>496) {
      std::array<uint8_t,80> saved;
      std::memcpy(saved.data(),base+vertices,saved.size());
      const float u0=Float(base,vertices+48),u1=Float(base,vertices+64);
      const float clipped_left=std::max(left,0.0f),clipped_right=std::min(right,496.0f);
      const float clipped_u0=u0+(u1-u0)*(clipped_left-left)/(right-left);
      const float clipped_u1=u0+(u1-u0)*(clipped_right-left)/(right-left);
      for(int v=0;v<4;++v) {
        PutFloat(base,vertices+v*12,v<2?clipped_left:clipped_right);
        PutFloat(base,vertices+48+v*8,v<2?clipped_u0:clipped_u1);
      }
      __imp__sub_821441D8(ctx,base);
      std::memcpy(base+vertices,saved.data(),saved.size());
    } else __imp__sub_821441D8(ctx,base);
  } else __imp__sub_821441D8(ctx,base);
}
REX_HOOK_RAW(sub_820FE650) {
  // The original bitmap pass clears with the global dark-blue RGB. Override
  // only its opaque arena-background clear; HUD clears remain transparent.
  using namespace vf2::panorama;
  if(transparent_clear && uint32_t(ctx.lr)==0x8214419C) {
    ctx.f1.f64=0;ctx.f2.f64=0;ctx.f3.f64=0;ctx.f4.f64=0;
  } else
  if(active && bitmap_pass && sky_color_valid && uint32_t(ctx.lr)==0x8214419C && ctx.f4.f64>0.5) {
    ctx.f1.f64=sky_color[0];ctx.f2.f64=sky_color[1];ctx.f3.f64=sky_color[2];
  }
  __imp__sub_820FE650(ctx,base);
}
REX_HOOK_RAW(sub_821448F0) {
  const uint32_t initialized_canvas=ctx.r3.u32;
  if(ctx.r3.u32==vf2::panorama::canvases[0] || ctx.r3.u32==vf2::panorama::canvases[1])
    ctx.r4.u64=uint32_t(std::ceil(vf2::panorama::Extent()));
  __imp__sub_821448F0(ctx,base);
  using namespace vf2::panorama;
  // Allocate unconditionally so the F6 toggle can switch paths at runtime.
  if(initialized_canvas==canvases[0] && !color_canvases) {
    color_canvases=REX_KERNEL_MEMORY()->SystemHeapAlloc(384);
    if(color_canvases) {
      std::memset(base+color_canvases,0,384);blit_storage=color_canvases+256;
      blur_canvases=REX_KERNEL_MEMORY()->SystemHeapAlloc(6*128);
      if(blur_canvases) {
        std::memset(base+blur_canvases,0,6*128);
        for(unsigned i=0;i<6;++i) {
          const unsigned level=i%3+1;
          rex::CallFrame call(ctx);call.ctx.r3.u64=blur_canvases+i*128;
          call.ctx.r4.u64=1024>>level;call.ctx.r5.u64=512>>level;__imp__sub_821448F0(call.ctx,base);
        }

      }
      for(unsigned i=0;i<2;++i) {
        rex::CallFrame call(ctx);call.ctx.r3.u64=color_canvases+i*128;
        call.ctx.r4.u64=1024;call.ctx.r5.u64=512;__imp__sub_821448F0(call.ctx,base);
      }
    }
  }
}
REX_HOOK_RAW(sub_82144050) {
  using namespace vf2::panorama;
  if(active && bitmap_pass && ctx.r5.u32==1 && ctx.r3.u32<2 && color_canvases &&
      ColorTrial() && !vf2::demo::Before() && REXCVAR_GET(vf2_background_smooth_scroll)) {
    color_priority=ctx.r3.u32;
    std::memcpy(saved_color_target.data(),base+canvases[color_priority],88);
    std::memcpy(base+canvases[color_priority],base+color_canvases+color_priority*128,88);
    decoding_colors=true;
  }
  __imp__sub_82144050(ctx,base);
}
REX_HOOK_RAW(sub_820F7E00) {
  using namespace vf2::panorama;
  if(color_blit && ctx.r3.u32==2 && uint32_t(ctx.lr)==0x821444DC) {
    for(unsigned v=0;v<4;++v) {
      PutFloat(base,ctx.r6.u32+v*8,color_uv[v<2?0:2]);
      PutFloat(base,ctx.r6.u32+v*8+4,color_uv[v%2?3:1]);
    }
  }
  __imp__sub_820F7E00(ctx,base);
}
REX_HOOK_RAW(sub_821038F8) {
  if(vf2::panorama::decoding_colors || vf2::panorama::blurring) {
    ctx.f1.f64=0;ctx.f2.f64=1024;ctx.f3.f64=512;ctx.f4.f64=0;
  } else
  if(uint32_t(ctx.lr)==0x821440CC) {
    const double margin=(vf2::panorama::Extent()-496.0)/2;
    ctx.f1.f64=-margin; ctx.f2.f64=496+margin;
  }
  __imp__sub_821038F8(ctx,base);
}
namespace {
void Composite(PPCContext& ctx,uint8_t* base,void (*original)(PPCContext&,uint8_t*),unsigned priority) {
  using namespace vf2::panorama;
  // The bitmap pass does not run on every presented frame (Arcade measured
  // ~36/s): the canvas then still holds the last pass, so redraw that pass's
  // background as well. Requiring the same frame made the background vanish
  // on frames without a pass (flicker, notably in the native pause).
  if(REXCVAR_GET(vf2_background_linear_composite))
    FlushPending(ctx,base,priority);
  const auto uv=ctx.r3.u32, first=ctx.r4.u32, last=ctx.r5.u32;
  if (Float(base,uv)!=496 || Float(base,uv+4)!=384) {
    original(ctx,base); return;
  }
  const float left=Float(base,first),right=Float(base,last);
  const float top=Float(base,first+4),bottom=Float(base,last+4);
  // Cover what the UI projection shows: the virtual screen (vf2_video.cpp),
  // centred on the 1920-wide layout.
  const double ui_width=REX_LOAD_U32(0x824A0498+36),ui_height=REX_LOAD_U32(0x824A0498+40);
  const double ui_left=vf2::display::UiLeft(ui_width);
  PutFloat(base,uv,float(std::ceil(Extent())));
  PutFloat(base,first,float(ui_left));
  PutFloat(base,last,float(ui_left+ui_width));
  PutFloat(base,first+4,0);PutFloat(base,last+4,float(ui_height));
  original(ctx,base);
  PutFloat(base,uv,496); PutFloat(base,first,left); PutFloat(base,last,right);
  PutFloat(base,first+4,top);PutFloat(base,last+4,bottom);
}
}
REX_HOOK_RAW(sub_82144548) { Composite(ctx,base,__imp__sub_82144548,1); }
REX_HOOK_RAW(sub_821445B0) { Composite(ctx,base,__imp__sub_821445B0,0); }

// Line-scroll table copy (sub_82144618 -> sub_82188AB0): dst[i] =
// src[(i + vertical scroll) & 511] for 384 lines, then uploaded as the
// per-line X offset texture read by the line-scroll tile shaders.
REX_HOOK_RAW(sub_82188AB0) {
  const uint32_t layer=ctx.r3.u32,source=ctx.r4.u32,destination=ctx.r5.u32,count=ctx.r6.u32;
  __imp__sub_82188AB0(ctx,base);
  // Dural's arenas (10 boss, 11 versus): the native water wave alternates two
  // opposite waves on even/odd lines and ticks, made for a CRT. With the
  // widened panorama the table is replaced by one smooth wave of the same
  // amplitude and 128-tick period, a function of panorama row and time, so
  // it loops seamlessly. No other processing applies to these backgrounds.
  if((layer==1 || layer==3) && count<=512 && vf2::panorama::last_pass_panorama) {
    const uint32_t memory=REX_LOAD_U32(0x833BD428);
    const uint8_t arena=memory && memory<0xf0000000 ? base[memory+0x500064] : 0xFF;
    if(arena==10 || arena==11) {
      constexpr double kPi=3.14159265358979323846;
      constexpr double kAmplitude=3.5,kWavelength=112.0,kPeriod=128.0;  // source px, rows, ticks
      const double time=std::fmod(double(REX_LOAD_U32(0x82E7E38C))+vf2::render::Alpha(),kPeriod);
      for(uint32_t i=0;i<count;++i) {
        const double v=kAmplitude*std::sin(2*kPi*(double(i)/kWavelength-time/kPeriod));
        REX_STORE_U16(destination+i*2,uint16_t(int16_t(std::lround(v))));
      }
      for(int i=vf2::panorama::kSwatchRow;i<vf2::panorama::kSwatchRow+2 && i<int(count);++i)REX_STORE_U16(destination+i*2,0);
      if(layer==1) {
        using namespace vf2::panorama;
        line_offsets.fill(0);
        for(uint32_t i=0;i<count;++i)line_offsets[i]=int16_t(REX_LOAD_U16(destination+i*2));
        line_offsets_frame=frame;line_offsets_valid=true;
      }
      ctx.r3.u64=1;  // changed: upload every frame, the wave moves continuously
      return;
    }
  }
  // The native copy indexes the table by screen line: dst[i]=src[(i+vscroll)
  // &511]. The widened renderer decodes the panorama with map row = canvas
  // line (no vertical scroll), so it needs dst[i]=src[i]. Using the native
  // phase shifted the per-line offsets by the camera pitch: rows slid out of
  // place on line-scroll arenas (Akira, Jacky, Dural), most visibly on tilts.
  // Native: screen line i shows panorama row i+phase_y and uses
  // src[i+vscroll], i.e. row r uses src[r-phase_y+vscroll]. The decoded
  // canvas has line == row, so it needs dst[y]=src[y-phase_y+vscroll].
  if(ctx.r3.u32 && (layer==1 || layer==3) && count<=512 && vf2::panorama::last_pass_panorama &&
     vf2::panorama::ColorTrial() && REXCVAR_GET(vf2_background_smooth_scroll) &&
     REXCVAR_GET(vf2_background_line_align)) {
    rex::CallFrame call(ctx);call.ctx.r3.u64=layer;sub_821889E8(call.ctx,base);
    const int vscroll=call.ctx.r3.s32;
    const int shift=vscroll-int(std::lround(vf2::panorama::camera_pose.y));
    for(uint32_t i=0;i<count;++i)REX_STORE_U16(destination+i*2,REX_LOAD_U16(source+((int(i)+shift)&511)*2));
    // Line-scroll arenas carry the camera's horizontal scroll in every entry
    // ((-phase_x) mod 512, measured) plus the per-line wave. phase_x already
    // places the widened panorama, so keep only the deviation from the modal
    // entry; otherwise rows were displaced by up to ~250 px on camera turns.
    std::unordered_map<int,int> histogram;int mode=0,best=0;
    for(uint32_t i=0;i<count;++i) {
      const int v=REX_LOAD_U16(destination+i*2)&511;
      if(++histogram[v]>best){best=histogram[v];mode=v;}
    }
    for(uint32_t i=0;i<count;++i) {
      const int v=REX_LOAD_U16(destination+i*2)&511;
      REX_STORE_U16(destination+i*2,uint16_t(int16_t(((v-mode+256)&511)-256)));
    }
    for(int i=vf2::panorama::kSwatchRow;i<vf2::panorama::kSwatchRow+2 && i<int(count);++i)REX_STORE_U16(destination+i*2,0);
    static uint64_t logged=0;
    if(logged++%600==0) {
      auto v=[&](int r){return int(int16_t(REX_LOAD_U16(source+(r&511)*2)));};
      REXLOG_INFO("VF2 line align: layer={} vscroll={} phase=({},{}) shift={} src[0,50,100,150,200,250,300]={},{},{},{},{},{},{}",
          layer,vscroll,vf2::panorama::camera_pose.x,vf2::panorama::camera_pose.y,shift,v(0),v(50),v(100),v(150),v(200),v(250),v(300));
    }
  }
  // Background layers (1/3) use two interleaved, opposite-phase waves on even
  // and odd lines: on a CRT adjacent lines blended. Magnified on a modern
  // display they become a comb. Apply that blend: [1,2,1]/4 across lines.
  if(ctx.r3.u32 && (layer==1 || layer==3) && count>=3 && count<=512 &&
     REXCVAR_GET(vf2_background_wave_smooth)) {
    std::array<int,512> v{};
    for(uint32_t i=0;i<count;++i)v[i]=int16_t(REX_LOAD_U16(destination+i*2));
    for(uint32_t i=0;i<count;++i) {
      const int sum=v[i>0?i-1:i]+2*v[i]+v[i+1<count?i+1:i];
      REX_STORE_U16(destination+i*2,uint16_t(int16_t(std::lround(sum/4.0))));
    }
  }
  if(ctx.r3.u32 && layer==1 && count<=512 && vf2::panorama::last_pass_panorama) {
    using namespace vf2::panorama;
    line_offsets.fill(0);
    for(uint32_t i=0;i<count;++i)line_offsets[i]=int16_t(REX_LOAD_U16(destination+i*2));
    line_offsets_frame=frame;line_offsets_valid=true;
  }
}



// Bitmap screens without a recognised panorama (high-score entry, static
// stage screens): the whole 384-line canvas 0 is the background image, so
// composite it with LINEAR (428) instead of POINT (427) when smoothing is on.
REX_HOOK_RAW(sub_821442A0) {
  using namespace vf2::panorama;
  if(ctx.r4.u32==427 && uint32_t(ctx.lr)==0x821445E8 && !last_pass_panorama &&
     REXCVAR_GET(vf2_background_linear_composite) && REXCVAR_GET(vf2_background_static_smooth))
    ctx.r4.u64=428;
  __imp__sub_821442A0(ctx,base);
}

// Map-texture uploads. Animated arenas (e.g. Akira, Jacky, Dural) re-upload
// the native 4:3 streaming map every frame from sub_821881B0, before the
// bitmap pass. The GPU consumes the previous pass's panorama draws
// asynchronously, so that upload replaced background banks still in use and
// one bank showed the native window: a vertical seam with mismatched sky.
// While a panorama is active, every map upload carries its background banks.
extern "C" REX_FUNC(__imp__sub_82199CB8);
// Model textures (sub_8213B2A0) and the colour table (sub_8213A6B0) are
// written into guest memory while the GPU keeps rendering, over many frames
// of a load. Occasionally the GPU's copy of a page missed a write (about one
// fight start in four in a test loop): characters and arena parts stayed
// black/green, untextured or recoloured until the next load, although guest
// memory was correct. A few frames after a burst of these uploads ends, the
// physical memory views are invalidated once, so the GPU reloads them.
namespace {
std::atomic<unsigned> texture_uploads{0};
void InvalidateGpuMemory() {
  auto* memory=rex::system::kernel_memory();
  if(!memory)return;
  for(const auto& [start,length]:{std::pair<uint32_t,uint32_t>{0xA0000000u,0x20000000u},
                                  {0xC0000000u,0x20000000u},{0xE0000000u,0x1FFFF000u}})
    memory->TriggerPhysicalMemoryCallbacks(rex::thread::global_critical_region::AcquireDirect(),
                                           start,length,true,false);
}
}
extern "C" REX_FUNC(__imp__sub_821433B8);
REX_HOOK_RAW(sub_821433B8) {  // once per frame
  static unsigned seen=0,quiet=0;
  const unsigned uploads=texture_uploads.load();
  if(uploads!=seen){seen=uploads;quiet=1;}
  else if(quiet && ++quiet>3){quiet=0;InvalidateGpuMemory();}
  __imp__sub_821433B8(ctx,base);
}
REX_HOOK_RAW(sub_82199CB8) {
  using namespace vf2::panorama;
  if(uint32_t(ctx.lr)==0x8213B5F4 || uint32_t(ctx.lr)==0x8213A848)++texture_uploads;
  if(have_remapped_background && engine && REXCVAR_GET(vf2_background_bank_guard) &&
     ctx.r3.u32==REX_LOAD_U32(engine+0x118c04) && ctx.r4.u32==engine+0x100000) {
    std::array<uint8_t,32768-kBackgroundBankOffset> native;
    uint8_t* map=base+engine+0x100000+kBackgroundBankOffset;
    std::memcpy(native.data(),map,native.size());
    std::memcpy(map,remapped_background.data(),native.size());
    __imp__sub_82199CB8(ctx,base);
    std::memcpy(map,native.data(),native.size());
    return;
  }
  __imp__sub_82199CB8(ctx,base);
}
