#include "generated/default/vf2_pch.h"
#include "generated/default/vf2_funcs.h"
#include "vf2_fight.h"
#include "vf2_demo.h"
#include "vf2_arcade.h"
#include <rex/hook.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <Xinput.h>
#include <rex/logging.h>
#include <rex/cvar.h>
#include <rex/memory.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <cstdlib>
#include <string>

extern "C" REX_FUNC(__imp__sub_82146690);
extern "C" REX_FUNC(__imp__sub_82111858);
extern "C" REX_FUNC(__imp__sub_82186AD8);
extern "C" REX_FUNC(__imp__sub_82120818);
extern "C" REX_FUNC(__imp__sub_8218CB00);
extern "C" REX_FUNC(__imp__sub_821A8A60);
extern "C" REX_FUNC(__imp__sub_8218BEA0);

REXCVAR_DEFINE_BOOL(vf2_auto_create_save, true, "Game",
    "Answer Yes automatically to the first-start 'No Save Data found' prompt");
REXCVAR_DEFINE_BOOL(vf2_arcade_end_to_menu, true, "Game",
    "Return to the Main Menu when an Arcade/Ranking run ends instead of the attract demo");
REXCVAR_DEFINE_BOOL(vf2_startup_press_start, true, "Game",
    "Skip the title by pressing Start (runs the original sign-in and storage-device flow)");
REXCVAR_DEFINE_BOOL(vf2_f6_suspend_m2e, false, "Video",
    "F6 freezes by suspending TaskM2E (legacy: hides the game screen) instead of skipping emulated updates");

namespace vf2::fight {
bool Pending();
void PollTestCommand();
namespace {
// Static task objects and manager are established by the shipped startup.
constexpr uint32_t kManagerPointer = 0x8346585C;
constexpr uint32_t kBoot = 0x83465878;
constexpr uint32_t kM2E = 0x834658F8;
bool saw_boot = false;
bool boot_complete = false;
bool logged_tasks = false;
constexpr uint32_t kScene = 0x82BB7BF8;
std::atomic_bool settings_open{false}, return_to_menu{false};
bool settings_suspended=false;
std::atomic_bool simulation_frozen{false};
std::atomic_int menu_input{0};
std::atomic_bool inject_start{false};
bool startup_menu_requested=false;
bool gameplay_ready=false;
bool pause_resetting=false;
// A request made during a native Arcade/Versus/Ranking game leaves that game
// first (same reset as Pause > Exit Game); the fight then starts from the
// Main Menu like any other. host_entered marks gameplay entered by the host.
std::atomic_bool host_entered{false};
std::atomic_uint scene{0};  // kScene, refreshed every host frame
bool leave_sent=false;
bool NativeGameRunning() {
  return Pending() && !Active() && !host_entered.load() && scene.load()==1;
}

void Scene(PPCContext& ctx,uint8_t* base,unsigned root,unsigned child) {
  rex::CallFrame call(ctx);
  call.ctx.r3.u64=root;sub_8214B9D8(call.ctx,base);
  call.ctx.r3.u64=child;sub_8214B9F8(call.ctx,base);
}

bool IsMenu(uint32_t vtable) {
  // Exact RTTI-backed TaskMenuMain, TaskMenuArcade, TaskMenuVersus,
  // TaskPromotion and TaskOption vtables from the shipped image.
  return vtable == 0x8204B9FC || vtable == 0x8204B978 ||
         vtable == 0x8204B9A4 || vtable == 0x82043B80 ||
         vtable == 0x82043B58 || vtable == 0x82051EEC; // WAIT_SCREEN
}

void HostFrame(PPCContext& ctx, uint8_t* base) {
  PollTestCommand();
  const uint32_t manager = REX_LOAD_U32(kManagerPointer);
  if (manager < 0x80000000 || manager > 0xF0000000) return;
  const uint32_t head = REX_LOAD_U32(manager);
  if (head < 0x80000000 || head > 0xF0000000) return;
  std::array<uint32_t, 128> menus{};
  size_t count = 0;
  bool boot_present = false;
  uint32_t node = REX_LOAD_U32(head);
  for (uint32_t n = 0; node != head && n < 512; ++n) {
    if (node < 0x80000000 || node > 0xF0000000) return;
    const uint32_t task = REX_LOAD_U32(node + 8);
    if (task < 0x80000000 || task > 0xF0000000) return;
    if (task == kBoot) boot_present = true;
    const uint32_t vtable = REX_LOAD_U32(task);
    if (IsMenu(vtable) && count < menus.size()) menus[count++] = task;
    node = REX_LOAD_U32(node);
  }
  saw_boot = saw_boot || boot_present;
  scene.store(REX_LOAD_U32(kScene));
  // Keyboard menu routing: 1 = frontend scene (not gameplay scene 1),
  // 2 = native pause open (TaskPause +76), 0 = gameplay.
  menu_input.store(REX_LOAD_U32(0x82BB8F08+12)==2 && REX_LOAD_U32(0x82BB8F08+76)!=0 ? 2 :
                   REX_LOAD_U32(kScene)!=1 ? 1 : 0);
  vf2::arcade::HostFrame(base,REX_LOAD_U32(kScene)!=1);
  static int logged_menu=-1;
  if(menu_input.load()!=logged_menu) {
    logged_menu=menu_input.load();
    REXLOG_INFO("VF2 keyboard routing: {} (scene {}/{})",logged_menu==2?"pause menu":logged_menu==1?"frontend menus":"gameplay",
        REX_LOAD_U32(kScene),REX_LOAD_U32(kScene+28));
  }
  if (saw_boot && !boot_present) boot_complete = true;
  if(vf2::demo::Host(ctx,base,boot_complete,std::span<const uint32_t>(menus.data(),count)))return;
  gameplay_ready=REX_LOAD_U32(kScene)==1 && REX_LOAD_U32(kScene+12)==1 &&
      REX_LOAD_U32(kScene+28)==6;
  if(const char* report=std::getenv("VF2_CUSTOM_FIGHT_TEST_REPORT")) {
    static unsigned frame=0;
    if(++frame%30==0) {
      std::ofstream out(std::string(report)+".host.csv",std::ios::app);
      out<<frame<<','<<REX_LOAD_U32(kScene)<<','<<REX_LOAD_U32(kScene+12)<<','
          <<REX_LOAD_U32(kScene+28)<<','<<REX_LOAD_U32(kM2E+16)<<','
          <<REX_LOAD_U32(0x82BB8F08+76)<<','<<REX_LOAD_U32(0x82E7E38C)<<','
          <<REX_LOAD_U32(0x82BB8F08+12)<<','<<int(base[0x82AAEB98])<<','
          <<int(base[0x82AAEB99])<<'\n';
    }
  }
  // Suspend only TaskM2E for F6. Native pause/menu tasks continue running.
  // Resuming a task we suspended leaves the game's own pause timescale intact.
  const bool wants_pause=settings_open.load() && !Pending();
  // TaskM2E also draws the whole Model 2 screen; suspending it leaves only
  // the frontend backdrop. Skip emulated-system updates instead, so the last
  // frame keeps rendering under the settings window.
  simulation_frozen.store(wants_pause && boot_complete && !REXCVAR_GET(vf2_f6_suspend_m2e));
  if(wants_pause && !settings_suspended && boot_complete && REXCVAR_GET(vf2_f6_suspend_m2e) &&
      REX_LOAD_U32(kM2E+16)!=3) {
    rex::CallFrame call(ctx);call.ctx.r3.u64=1;sub_82199420(call.ctx,base);
    settings_suspended=true;
    REXLOG_INFO("VF2 F6: suspended M2E; native pause state retained");
  } else if(!wants_pause && settings_suspended) {
    rex::CallFrame call(ctx);call.ctx.r3.u64=0;sub_82199420(call.ctx,base);
    settings_suspended=false;
    REXLOG_INFO("VF2 F6: restored M2E; native pause state retained");
  }
  if(return_to_menu.exchange(false)) {
    // Same original reset used by TaskPause's confirmed Exit Game action.
    rex::CallFrame call(ctx);sub_82183140(call.ctx,base);sub_82110B50(call.ctx,base);
    Scene(ctx,base,2,8);
    REXLOG_INFO("VF2 custom fight: requested native Main Menu after match completion");
  }
  // Normal Arcade/Ranking runs: after GAME OVER or the ending and name entry
  // the ROM drops from match mode 9 (via 10/11) to attract mode 3, but the
  // frontend stays in gameplay scene 1 and shows the endless attract demo.
  // Return to Main Menu through the same native reset as a confirmed Exit.
  {
    static bool played=false;
    const uint32_t memory=REX_LOAD_U32(0x833BD428);
    const uint8_t mode=memory && memory<0xf0000000 ? base[memory+0x50002a] : 0;
    if(REX_LOAD_U32(kScene)!=1 || Active() || Pending()) played=false;
    else if(mode==9) played=true;
    else if(played && mode==3 && REXCVAR_GET(vf2_arcade_end_to_menu)) {
      played=false;
      rex::CallFrame call(ctx);sub_82183140(call.ctx,base);sub_82110B50(call.ctx,base);
      Scene(ctx,base,2,8);
      REXLOG_INFO("VF2 arcade: run finished (attract mode reached); returning to Main Menu");
    }
  }
  if(Active() && !Pending() && REX_LOAD_U32(kScene)!=1) {
    Finish();Scene(ctx,base,2,8);
    REXLOG_INFO("VF2 custom fight: native gameplay exit completed; opening Main Menu");
  }
  // Scene 2 / child 7 is the native title/attract wait. Child 8 opens the
  // original Main Menu. Wait for all startup/save/profile states to finish.
  // Jumping straight to child 8 skipped the Push Start flow, which assigns
  // the active user and selects the storage device (save data). Without it
  // saving looped and reported "No storage device has been selected". Press
  // Start on the title instead, so the original sign-in/device/save flow runs.
  static unsigned start_frames=0;
  const bool title=(REX_LOAD_U32(kScene)==2 && REX_LOAD_U32(kScene+28)==7) ||
                   (REX_LOAD_U32(kScene)==0 && REX_LOAD_U32(kScene+28)==2);  // Push Start screen
  if(boot_complete && !startup_menu_requested && !Pending() && title) {
    if(REXCVAR_GET(vf2_startup_press_start)) {
      if(start_frames<240) {
        ++start_frames;
        inject_start.store((start_frames/8)%2==0);  // repeated presses
      } else {
        inject_start.store(false);
        Scene(ctx,base,2,8);startup_menu_requested=true;
        REXLOG_INFO("VF2 startup: Push Start did not leave the title; opening Main Menu directly");
      }
    } else {
      Scene(ctx,base,2,8);startup_menu_requested=true;
      REXLOG_INFO("VF2 startup: opening native Main Menu without Push Start");
    }
  } else if(start_frames && !startup_menu_requested && !title) {
    inject_start.store(false);startup_menu_requested=true;
    REXLOG_INFO("VF2 startup: native Push Start flow entered (scene {}/{})",REX_LOAD_U32(kScene),REX_LOAD_U32(kScene+28));
  }
  if(boot_complete && !startup_menu_requested && !Pending() &&
      REX_LOAD_U32(kScene)==0 && REX_LOAD_U32(kScene+28)==0 && !REXCVAR_GET(vf2_startup_press_start)) {
    Scene(ctx,base,2,8);startup_menu_requested=true;
    REXLOG_INFO("VF2 startup: opening native Main Menu without Push Start");
  }
  // The skipped Push Start screen normally assigns the active user through
  // sub_8218B848. Without it 0x824A04E8 stays -1, and the native system-UI
  // items (Achievements, Leaderboards) silently do nothing.
  if(startup_menu_requested && REX_LOAD_U32(0x824A04E8)>=4) {
    rex::CallFrame call(ctx);call.ctx.r3.u64=0;sub_8218B848(call.ctx,base);
    REXLOG_INFO("VF2 startup: assigned active user 0 (Push Start skipped)");
  }
  if(!Pending()){host_entered.store(false);leave_sent=false;}
  if(NativeGameRunning()) {
    if(!leave_sent && boot_complete) {
      rex::CallFrame call(ctx);sub_82183140(call.ctx,base);sub_82110B50(call.ctx,base);
      Scene(ctx,base,2,8);leave_sent=true;
      REXLOG_INFO("VF2 custom fight: leaving the running native game before the custom fight");
    }
    return;
  }
  if(REX_LOAD_U32(kScene)!=1)leave_sent=false;
  if (!Pending() || !boot_complete) return;
  if (!logged_tasks) {
    REXLOG_INFO("VF2 custom fight host: startup complete; {} native menu tasks; M2E state={}/{}/{}",
        count, REX_LOAD_U32(kM2E + 12), REX_LOAD_U32(kM2E + 16),
        REX_LOAD_U32(kM2E + 20));
    uint32_t item = REX_LOAD_U32(head);
    for (unsigned n = 0; item != head && n < 128; ++n) {
      const uint32_t task = REX_LOAD_U32(item + 8);
      REXLOG_INFO("VF2 custom fight host task {:08X} vtable={:08X} state={}/{}/{}",
          task, REX_LOAD_U32(task), REX_LOAD_U32(task + 12),
          REX_LOAD_U32(task + 16), REX_LOAD_U32(task + 20));
      item = REX_LOAD_U32(item);
    }
    logged_tasks = true;
  }
  // Let original exit callbacks finish animations, child tasks and resources.
  // No UI objects are hidden or freed by the launcher itself.
  if (count) {
    for (size_t n = 0; n < count; ++n) {
      rex::CallFrame call(ctx);
      call.ctx.r3.u64 = menus[n];
      sub_82197D10(call.ctx, base);
    }
    return;
  }
  // Direct ROM initialization previously bypassed native gameplay scene 1,
  // which owns TaskPause and the match-completion callbacks. Enter that scene
  // through its dispatcher rather than manually constructing a pause menu.
  if(REX_LOAD_U32(kScene)!=1) {
    Scene(ctx,base,1,6);host_entered.store(true);
    return;
  }
  if(REX_LOAD_U32(kScene+12)!=1 || REX_LOAD_U32(kScene+28)!=6)return;
  constexpr uint32_t pause=0x82BB8F08;
  if(!pause_resetting && REX_LOAD_U32(pause+12)==2 && REX_LOAD_U32(pause+76)!=0) {
    rex::CallFrame call(ctx);
    call.ctx.r3.u64=pause;sub_821558B0(call.ctx,base); // native Resume/audio cleanup
    sub_821557A0(call.ctx,base); // recursively exit pause and its child menus
    pause_resetting=true;
    REXLOG_INFO("VF2 custom fight: closing native pause before Start/Restart");
    return;
  }
  if(pause_resetting) {
    rex::CallFrame call(ctx);call.ctx.r3.u64=pause;sub_82197BB0(call.ctx,base);
    if(call.ctx.r3.u32)return;
    sub_82155788(call.ctx,base);
    pause_resetting=false;
    return;
  }
  // Native TaskM2E suspend/resume API; the boot task pauses it deliberately
  // after its first nine emulated frames while displaying the Xbox title.
  // +16 is the native scheduler's execution state (3 = paused). +12 is
  // task lifecycle and remains 2 while paused. Resume queues command 5 at +20.
  if (REX_LOAD_U32(kM2E + 16) == 3 && REX_LOAD_U32(kM2E + 20) != 5) {
    rex::CallFrame call(ctx);
    call.ctx.r3.u64 = 0;
    sub_82199420(call.ctx, base);
    REXLOG_INFO("VF2 custom fight host: requested original M2E resume");
  }
}
}

bool SimulationFrozen() { return simulation_frozen.load(); }
int MenuInput() { return menu_input.load(); }
bool InjectStart() { return inject_start.load(); }
bool HostReady() { return boot_complete && !pause_resetting && (!Pending() || (gameplay_ready && !NativeGameRunning())); }
void SetSettingsOpen(bool open) { settings_open.store(open); }
void RequestMainMenu() { return_to_menu.store(true); }
}

namespace {
// System messages say "Slophaus" instead of "console" ("Please don't turn off
// your console" on the save-icon notice and the Save Data prompts). The
// shipped string table (sub_82182170 loads string_array_<lang>) is a char*
// array at *0x82E7CFB0; matching entries get a new guest copy, the loaded
// file data itself is left untouched.
void BrandStrings(uint8_t* base) {
  static uint32_t patched=0;
  const uint32_t table=REX_LOAD_U32(0x82E7CFB0);
  if(table==patched || table<0x80000000u || table>=0xF0000000u)return;
  auto in_data=[&](uint32_t p){return p>table && p<table+0x100000u;};
  if(!in_data(REX_LOAD_U32(table)))return;  // not resolved yet
  uint32_t first=0xFFFFFFFFu;unsigned replaced=0;
  for(uint32_t entry=table;entry<first && entry<table+0x40000u;entry+=4) {
    const uint32_t text=REX_LOAD_U32(entry);
    if(!in_data(text))break;
    first=std::min(first,text);
    std::string value;
    for(uint32_t i=0;i<2048;++i){const char c=char(REX_LOAD_U8(text+i));if(!c)break;value+=c;}
    bool changed=false;
    for(const char* word:{"console","Console"})
      for(size_t at;(at=value.find(word))!=std::string::npos;){value.replace(at,7,"Slophaus");changed=true;}
    if(!changed)continue;
    const uint32_t copy=REX_KERNEL_MEMORY()->SystemHeapAlloc(uint32_t(value.size()+1));
    if(!copy)return;
    for(size_t i=0;i<=value.size();++i)REX_STORE_U8(copy+uint32_t(i),uint8_t(i<value.size()?value[i]:0));
    REX_STORE_U32(entry,copy);++replaced;
  }
  patched=table;
  REXLOG_INFO("VF2 strings: {} system messages now mention Slophaus",replaced);
}
}

REX_HOOK_RAW(sub_82146690) {
  BrandStrings(base);
  vf2::fight::HostFrame(ctx, base);
  __imp__sub_82146690(ctx, base);
}

REX_HOOK_RAW(sub_82111858) {
  // The original generic pause path polls Back (0x100); Start is 0x200 in
  // the shipped XInput translation. Accept both for custom matches only.
  const bool pause_poll=ctx.lr==0x821561E0;
  if(vf2::fight::Active() && pause_poll && ctx.r3.u32==256)
    ctx.r3.u64=256|512;
  __imp__sub_82111858(ctx,base);
  // Esc pauses any match (Arcade, Versus, custom). The result is the user
  // who owns the pause menu: the keyboard player (player 2 when player 1
  // has a pad), otherwise player 1. Start is not used: in Arcade, Start on
  // the side that is not playing means "new challenger".
  if(pause_poll) {
    static bool held=false;
    DWORD process=0;GetWindowThreadProcessId(GetForegroundWindow(),&process);
    bool down=process==GetCurrentProcessId() && (GetAsyncKeyState(VK_ESCAPE)&0x8000);
    if(const char* test=std::getenv("VF2_TEST_ESC")) {  // test only: file content "1" = Esc held
      std::ifstream in(test);int v=0;in>>v;down=v==1;
    }
    if(down && !held && ctx.r3.s32<0 && vf2::fight::MenuInput()==0 && !vf2::fight::SimulationFrozen()) {
      // Owner: the only side in the game (0x500068 bit 2 = side 1 entered,
      // bit 3 = side 2); with both sides in, the keyboard player.
      XINPUT_STATE pad{};
      int owner=XInputGetState(0,&pad)==ERROR_SUCCESS?1:0;
      const uint32_t memory=REX_LOAD_U32(0x833BD428);
      if(memory && memory<0xf0000000) {
        const uint8_t entered=base[memory+0x500068];
        const bool side[2]={bool(entered&4),bool(entered&8)};
        if(side[0]!=side[1])owner=side[1]?1:0;
      }
      ctx.r3.s64=owner;
      REXLOG_INFO("VF2 Esc: pause for player {}",ctx.r3.s32+1);
    }
    held=down;
  }
}

REX_HOOK_RAW(sub_82186AD8) {
  __imp__sub_82186AD8(ctx,base);
  // Start belongs to the frontend pause handler in a custom match. Do not
  // also send it as Model 2's "join / new challenger" switch to the ROM.
  if(vf2::fight::Active())base[0x830B89E1]|=0x30;
}

REX_HOOK_RAW(sub_82120818) {
  // Main Menu's own music dispatch, after boot audio-bank readiness. Native
  // index 0 explicitly stops BGM; 15 is the shipped Select_48k_loop track.
  if(uint32_t(ctx.lr)==0x821475E8 && REX_LOAD_U32(0x82AB26B4)==0) {
    REX_STORE_U32(0x82AB26B4,15);
    REXLOG_INFO("VF2 Main Menu music: native Select_48k_loop (command 00AD1007)");
  }
  __imp__sub_82120818(ctx,base);
}

namespace {
std::u16string GuestText(uint8_t* base,uint32_t address,size_t limit=256) {
  std::u16string text;
  if(!address)return text;
  for(size_t i=0;i<limit;++i) {
    const char16_t c=char16_t(REX_LOAD_U16(address+uint32_t(i*2)));
    if(!c)break;
    text.push_back(c);
  }
  return text;
}
thread_local bool message_box_answered=false;
std::string Narrow(const std::u16string& text) {
  std::string out;
  for(const char16_t c:text)out.push_back(c<128?char(c):'?');
  return out;
}
}

REX_HOOK_RAW(sub_821A8A60) {
  // Wrapper of XamShowMessageBoxUI(user, title, text, button count, buttons,
  // focus, flags, result*, overlapped* at r1+84). On a fresh installation the
  // game asks whether to create Save Data; answer Yes the way the dialog
  // would: write the chosen button, complete the event-less XOVERLAPPED
  // (polled by the caller) and return ERROR_IO_PENDING.
  const auto text=GuestText(base,ctx.r5.u32);
  const uint32_t buttons=ctx.r7.u32,count=ctx.r6.u32,result=ctx.r10.u32;
  const uint32_t overlapped=REX_LOAD_U32(ctx.r1.u32+84);
  REXLOG_INFO("VF2 message box: \"{}\" ({} buttons)",Narrow(text),count);
  if(REXCVAR_GET(vf2_auto_create_save) && text.find(u"No Save Data found")!=std::u16string::npos &&
     result && overlapped && count>0 && count<=3) {
    uint32_t yes=0;
    for(uint32_t i=0;i<count;++i)
      if(GuestText(base,REX_LOAD_U32(buttons+i*4),16)==u"Yes"){yes=i;break;}
    REX_STORE_U32(result,yes);
    REX_STORE_U32(overlapped+0,0);   // InternalLow: ERROR_SUCCESS
    REX_STORE_U32(overlapped+4,0);   // InternalHigh
    REXLOG_INFO("VF2 message box: answered \"{}\" automatically (vf2_auto_create_save)",
                Narrow(GuestText(base,REX_LOAD_U32(buttons+yes*4),16)));
    ctx.r3.u64=997;                  // ERROR_IO_PENDING, as the real call returns
    message_box_answered=true;
    return;
  }
  __imp__sub_821A8A60(ctx,base);
}

REX_HOOK_RAW(sub_8218BEA0) {
  // Message-box request. After a pending call it sets the "system UI open"
  // flag 0x82AA6EF0, which only the XN_SYS_UI close notification clears. An
  // automatically answered box shows no UI, so clear the flag here instead.
  message_box_answered=false;
  __imp__sub_8218BEA0(ctx,base);
  if(message_box_answered) {
    REX_STORE_U32(0x82AA6EF0,0);
    message_box_answered=false;
  }
}

REX_HOOK_RAW(sub_8218CB00) {
  // Native system-UI dispatcher (Achievements, Leaderboards, ...). It returns
  // without doing anything while the active user at 0x824A04E8 is >= 4. The
  // shipped Push Start screen assigns it via sub_8218B848; startup skips that
  // screen, so assign the first user through the same setter here.
  if(REX_LOAD_U32(0x824A04E8)>=4) {
    rex::CallFrame call(ctx);call.ctx.r3.u64=0;sub_8218B848(call.ctx,base);
    REXLOG_INFO("VF2 system UI: no active user after skipped Push Start; assigned user 0");
  }
  static uint32_t last_task=0,last_kind=~0u;
  const uint32_t kind=REX_LOAD_U32(ctx.r3.u32+76);
  if(ctx.r3.u32!=last_task || kind!=last_kind) {
    last_task=ctx.r3.u32;last_kind=kind;
    REXLOG_INFO("VF2 system UI request {} for user {}",kind,REX_LOAD_U32(0x824A04E8));
  }
  __imp__sub_8218CB00(ctx,base);
}
