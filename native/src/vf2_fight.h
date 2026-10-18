#pragma once
#include <string>

namespace vf2::fight {
struct Config {
  int characters[2] = {0, 1};
  bool cpu[2] = {false, true};
  int arena = 0;
  // -1 preserves the original game setting. Native difficulty is shared.
  int difficulty = -1;    // 0 Easy, 1 Normal, 2 Hard, 3 Hardest.
  int rounds_to_win = -1; // Native versus threshold: 2 through 5.
  int round_time = -1;    // Seconds: 15, 30, 45 or 60.
  bool tourist = false;   // Tourist CPU (Easy base); see vf2_arcade.cpp.
  bool alternate[2] = {false, false};  // Alternate costume (native +13 slot).
};
void RequestStart(const Config& config);
void RequestRestart();
std::string Status();
bool Active();
bool CanRestart();
bool Pending();
void PollTestCommand();
bool HostReady();
void SetSettingsOpen(bool open);
bool SimulationFrozen();
// 0 gameplay, 1 frontend menus, 2 native pause menu.
int MenuInput();
bool InjectStart();  // startup: hold Start for player 1
void RequestMainMenu();
void Finish();
// Tourist for the running custom fight: 1 on, 0 off, -1 not in a custom fight.
int TouristOverride();
}
