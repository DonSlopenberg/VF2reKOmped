#include "generated/default/vf2_pch.h"
#include "vf2_freelook.h"
#include <rex/logging.h>
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>
#include <array>

namespace vf2::freelook {
namespace {
std::mutex mutex;
Offsets offsets;
bool Foreground() {
  DWORD process=0;GetWindowThreadProcessId(GetForegroundWindow(),&process);
  return process==GetCurrentProcessId();
}
// Numpad 4/6 yaw, 8/2 pitch, 5 reset; polled once per native update.
void PollKeys(Offsets& o) {
  if(!o.enabled || !Foreground())return;
  const auto down=[](int key){return (GetAsyncKeyState(key)&0x8000)!=0;};
  constexpr float step=1.5f;
  if(down(VK_NUMPAD4))o.yaw-=step;
  if(down(VK_NUMPAD6))o.yaw+=step;
  if(down(VK_NUMPAD8))o.pitch+=step;
  if(down(VK_NUMPAD2))o.pitch-=step;
  if(down(VK_NUMPAD5))o.yaw=o.pitch=0;
  o.yaw=std::remainder(o.yaw,360.0f);
  o.pitch=std::clamp(o.pitch,-80.0f,80.0f);
}
}

Offsets Get(){std::lock_guard lock(mutex);return offsets;}
// Automated tests only: VF2_FREELOOK_TEST="yaw pitch" (degrees).
const bool test_offsets=[] {
  if(const char* text=std::getenv("VF2_FREELOOK_TEST"))
    offsets={true,float(std::atof(text)),float(std::atof(std::strchr(text,' ')?std::strchr(text,' ')+1:"0"))};
  return true;
}();
void Set(const Offsets& o){std::lock_guard lock(mutex);offsets=o;}

// The 3D view is not modified yet: the RAM matrix at camera +0xB54 turned out
// to be a fighter-object field (initialized at i960 0x141C8), and the native
// angle fields +0x24/+0x26 change shading, not the view. Offsets are applied
// by the panorama renderer only; Apply just polls the keys once per update.
void Restore(uint8_t*) {}
void Apply(uint8_t*) {
  std::lock_guard lock(mutex);PollKeys(offsets);
}
}
