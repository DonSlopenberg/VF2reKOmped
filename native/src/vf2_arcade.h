#pragma once
#include <cstdint>
struct PPCContext;
// Arcade extras: random opponent line-up / arenas, Tourist difficulty,
// infinite-health cheats and the achievement gate they imply.
namespace vf2::arcade {
struct Cheats { bool infinite_health[2]{}; };
Cheats GetCheats();
void SetCheats(const Cheats&);
bool CheatsUsedThisSession();
bool AchievementsBlocked();
// Called from the i960 instruction hook with the current program-relative PC.
void NativeInstruction(PPCContext& ctx, uint8_t* base, uint32_t pc);
// Called once per host frame (frontend/gameplay scene known).
void HostFrame(uint8_t* base, bool frontend);
// Hidden Dural: per input poll, true while A should be injected for this user.
bool DuralConfirm(unsigned user, bool start_held);
// Character select costume toggle: per input poll, and the frames to mark.
void CostumeInput(unsigned user, uint16_t buttons);
struct SelectCostumes { int slot[2]{-1, -1}; bool alternate[2]{}; };
SelectCostumes GetSelectCostumes();
}
