#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <Xinput.h>
#include <imgui.h>
#include <rex/hook.h>
#include <rex/runtime.h>
#include <rex/thread.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/windowed_app_context.h>
#include <algorithm>
#include <atomic>
#include <string>
#include <cstdlib>
#include "vf2_arcade.h"

namespace vf2 {
inline std::atomic<bool> achievement_ui_active{false};

// Character select: a side that chose the alternate costume gets its cursor
// frame in the negative colour. The native frame is part of the ROM's 2D
// tiles, so the marker is drawn over it at the side's portrait slot. The 4:3
// select screen fills the 1080-line height, centred horizontally.
inline void DrawCostumeFrames(ImGuiIO& io) {
  const auto costumes = arcade::GetSelectCostumes();
  const float scale = io.DisplaySize.y / 1080.0f, center = io.DisplaySize.x * 0.5f;
  // Native frames (1080-line units, measured): slot 0 spans x 287..417 and
  // slot 9 x 1504..1634, all at y 766..1009.
  constexpr float kLeft = 287.0f, kPitch = 135.2f, kWidth = 131.0f, kTop = 766.0f, kBottom = 1010.0f;
  // Negatives of the native frame colours: 1P (73,197,7), 2P (255,23,255).
  const ImU32 colours[2] = {IM_COL32(182, 58, 248, 255), IM_COL32(0, 232, 0, 255)};
  for (int side = 0; side < 2; ++side) {
    if (!costumes.alternate[side]) continue;
    const float x = kLeft + costumes.slot[side] * kPitch;
    const ImVec2 a(center + (x - 960.0f) * scale, kTop * scale);
    const ImVec2 b(center + (x + kWidth - 960.0f) * scale, kBottom * scale);
    const float thickness = 5.0f * scale;
    ImGui::GetForegroundDrawList()->AddRect(ImVec2(a.x + thickness * 0.5f, a.y + thickness * 0.5f),
        ImVec2(b.x - thickness * 0.5f, b.y - thickness * 0.5f), colours[side], 0.0f, 0, thickness);
  }
}

// Supplies the same Windows XInput navigation to every runtime dialog.
// Events are queued for the next ImGui frame; game input is disabled only
// while a real dialog captures input, never by this invisible bridge itself.
class ControllerUi final : public rex::ui::ImGuiDialog {
 public:
  ControllerUi(rex::ui::ImGuiDrawer* drawer, rex::ui::Window* window)
      : ImGuiDialog(drawer), window_(window) {}
  void OnDraw(ImGuiIO& io) override {
    DrawCostumeFrames(io);
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NavEnableKeyboard;
    // Fullscreen: hide the mouse cursor until the mouse moves (F6 stays usable).
    const auto cursor = window_->IsFullscreen() ? rex::ui::Window::CursorVisibility::kAutoHidden
                                                : rex::ui::Window::CursorVisibility::kVisible;
    if (window_->GetCursorVisibility() != cursor) {
      window_->SetCursorAutoHideDelayMs(1000);
      window_->SetCursorVisibility(cursor);
    }
    WORD buttons = 0;
    SHORT stick_x = 0, stick_y = 0;
    bool connected = false;
    if (window_->HasFocus()) {
      for (DWORD slot = 0; slot < XUSER_MAX_COUNT; ++slot) {
        XINPUT_STATE state{};
        if (XInputGetState(slot, &state) != ERROR_SUCCESS) continue;
        connected = true;
        buttons |= state.Gamepad.wButtons;
        if (std::abs(int(state.Gamepad.sThumbLX)) > std::abs(int(stick_x))) stick_x = state.Gamepad.sThumbLX;
        if (std::abs(int(state.Gamepad.sThumbLY)) > std::abs(int(stick_y))) stick_y = state.Gamepad.sThumbLY;
      }
    }
    if (connected) io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    else io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
    auto key = [&](ImGuiKey key, WORD mask) { io.AddKeyEvent(key, (buttons & mask) != 0); };
    key(ImGuiKey_GamepadFaceDown, XINPUT_GAMEPAD_A);
    key(ImGuiKey_GamepadFaceRight, XINPUT_GAMEPAD_B);
    key(ImGuiKey_GamepadFaceLeft, XINPUT_GAMEPAD_X);
    key(ImGuiKey_GamepadFaceUp, XINPUT_GAMEPAD_Y);
    key(ImGuiKey_GamepadStart, XINPUT_GAMEPAD_START);
    key(ImGuiKey_GamepadBack, XINPUT_GAMEPAD_BACK);
    key(ImGuiKey_GamepadL1, XINPUT_GAMEPAD_LEFT_SHOULDER);
    key(ImGuiKey_GamepadR1, XINPUT_GAMEPAD_RIGHT_SHOULDER);
    io.AddKeyEvent(ImGuiKey_GamepadDpadLeft, (buttons & XINPUT_GAMEPAD_DPAD_LEFT) || stick_x < -16000);
    io.AddKeyEvent(ImGuiKey_GamepadDpadRight, (buttons & XINPUT_GAMEPAD_DPAD_RIGHT) || stick_x > 16000);
    io.AddKeyEvent(ImGuiKey_GamepadDpadUp, (buttons & XINPUT_GAMEPAD_DPAD_UP) || stick_y > 16000);
    io.AddKeyEvent(ImGuiKey_GamepadDpadDown, (buttons & XINPUT_GAMEPAD_DPAD_DOWN) || stick_y < -16000);
  }
 private:
  rex::ui::Window* window_;
};

class AchievementDialog;
inline AchievementDialog* achievement_dialog = nullptr;

// "↓↓↑↑←←→→" reads better as "↓ ↓ ↑ ↑ ← ← → →": space consecutive arrows
// (U+2190..2193, UTF-8 E2 86 90..93).
inline std::string SpaceArrows(const std::string& text) {
  const auto arrow = [&](size_t i) {
    return i + 2 < text.size() && uint8_t(text[i]) == 0xE2 && uint8_t(text[i + 1]) == 0x86 &&
           uint8_t(text[i + 2]) >= 0x90 && uint8_t(text[i + 2]) <= 0x93;
  };
  std::string out;
  for (size_t i = 0; i < text.size();) {
    if (arrow(i)) {
      out.append(text, i, 3);
      if (arrow(i + 3)) out += ' ';
      i += 3;
    } else out += text[i++];
  }
  return out;
}

class AchievementDialog final : public rex::ui::ImGuiDialog {
 public:
  AchievementDialog(rex::ui::ImGuiDrawer* drawer, rex::system::KernelState& kernel)
      : ImGuiDialog(drawer), manager_(kernel.achievements()), kernel_(kernel) {
    achievement_dialog = this;
    achievement_ui_active = true;
    kernel_.BroadcastNotification(0x9, 1);
  }
  ~AchievementDialog() override {
    achievement_dialog = nullptr;
    achievement_ui_active = false;
    kernel_.BroadcastNotification(0x9, 0);
    REXLOG_INFO("VF2: local achievement UI completed");
  }
  void OnDraw(ImGuiIO& io) override {
    bool first = !opened_;
    if (first) { ImGui::OpenPopup("Achievements"); opened_ = true; }
    const ImVec2 size(std::min(800.0f, io.DisplaySize.x * 0.88f),
                      std::min(650.0f, io.DisplaySize.y * 0.86f));
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f),
                           ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Achievements", nullptr,
          ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
      auto items = manager_.ListAchievements();
      int unlocked = 0, earned = 0, total = 0;
      for (const auto& item : items) {
        total += item.gamerscore;
        if (manager_.IsUnlocked(item.id)) { ++unlocked; earned += item.gamerscore; }
      }
      ImGui::Text("Virtua Fighter 2  |  %d / %zu unlocked  |  %d / %d G", unlocked, items.size(), earned, total);
      ImGui::ProgressBar(items.empty() ? 0.0f : float(unlocked) / items.size(), ImVec2(-1, 8), "");
      if (first) ImGui::SetKeyboardFocusHere();
      ImGui::Checkbox("Unlocked only", &unlocked_only_);
      ImGui::BeginChild("Achievement list", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2.4f),
                        ImGuiChildFlags_Borders);
      for (const auto& item : items) {
        const bool unlocked = manager_.IsUnlocked(item.id);
        if (unlocked_only_ && !unlocked) continue;
        ImGui::PushID(int(item.id));
        std::string label = (unlocked ? "Unlocked  |  " : "Locked  |  ") + item.label +
                            "  |  " + std::to_string(item.gamerscore) + " G";
        ImGui::Selectable(label.c_str(), false);
        ImGui::TextWrapped("%s", SpaceArrows(unlocked ? item.description : item.unachieved_description).c_str());
        if (unlocked) {
          const uint64_t ticks = manager_.GetUnlockTime(item.id);
          FILETIME ft{DWORD(ticks), DWORD(ticks >> 32)}, local{};
          SYSTEMTIME time{};
          if (FileTimeToLocalFileTime(&ft, &local) && FileTimeToSystemTime(&local, &time))
            ImGui::TextDisabled("Unlocked: %04u-%02u-%02u %02u:%02u", time.wYear, time.wMonth,
                                time.wDay, time.wHour, time.wMinute);
        }
        ImGui::Separator();
        ImGui::PopID();
      }
      if (items.empty()) ImGui::TextWrapped("No achievement metadata is available for this title.");
      else if (unlocked_only_ && !unlocked) ImGui::TextWrapped("No achievements unlocked yet.");
      ImGui::EndChild();
      ImGui::TextDisabled("D-pad / Left stick: navigate   A: select   B: close");
      if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape) ||
          ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight)) {
        ImGui::CloseCurrentPopup(); Close();
      }
      ImGui::EndPopup();
    } else { Close(); }
  }
 private:
  rex::system::AchievementManager& manager_;
  rex::system::KernelState& kernel_;
  bool opened_ = false;
  bool unlocked_only_ = std::getenv("VF2_ACHIEVEMENTS_SHOW") == nullptr;
};

inline void OpenAchievements(rex::ui::ImGuiDrawer* drawer, rex::system::KernelState& kernel) {
  if (!achievement_dialog) new AchievementDialog(drawer, kernel);
}

inline void Windowed75(rex::ui::Window* window) {
  if (!window) return;
  auto hwnd = static_cast<HWND>(window->GetNativeWindowHandle());
  if (!hwnd) return;
  MONITORINFO info{sizeof(info)};
  if (!GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &info)) return;
  window->SetFullscreen(false);
  ShowWindow(hwnd, SW_RESTORE);
  RECT borders{};
  const auto style = DWORD(GetWindowLongPtrW(hwnd, GWL_STYLE));
  const auto ex_style = DWORD(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
  AdjustWindowRectExForDpi(&borders, style, FALSE, ex_style, GetDpiForWindow(hwnd));
  const int border_width = borders.right - borders.left;
  const int border_height = borders.bottom - borders.top;
  const int available_width = (info.rcWork.right - info.rcWork.left) * 3 / 4 - border_width;
  const int available_height = (info.rcWork.bottom - info.rcWork.top) * 3 / 4 - border_height;
  const int units = std::max(1, std::min(available_width / 16, available_height / 9));
  const int width = units * 16 + border_width;
  const int height = units * 9 + border_height;
  const int x = info.rcWork.left + ((info.rcWork.right - info.rcWork.left) - width) / 2;
  const int y = info.rcWork.top + ((info.rcWork.bottom - info.rcWork.top) - height) / 2;
  SetWindowPos(hwnd, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
  REXLOG_INFO("F4: windowed mode, client {}x{}, outer {}x{} at {},{} (fits 75% of monitor)",
              units * 16, units * 9, width, height, x, y);
}
}  // namespace vf2

// The SDK implementation is a stub which doesn't complete the game's UI
// request. Return after opening, so the guest can keep presenting frames.
// Closing the dialog delivers XN_SYS_UI=false to release the game's UI state.
REX_HOOK_RAW(__imp__XamShowAchievementsUI) {
  (void)base;
  auto* kernel = REX_KERNEL_STATE();
  auto* runtime = kernel->emulator();
  auto* drawer = runtime->imgui_drawer();
  auto* context = runtime->app_context();
  REXLOG_INFO("VF2: opening local achievement UI for user {}", ctx.r3.u32);
  if (drawer && context) {
    context->CallInUIThreadSynchronous([&] { vf2::OpenAchievements(drawer, *kernel); });
  }
  ctx.r3.u64 = 0;
}
