#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <rex/input/input_system.h>
#include <windows.h>
#include <Xinput.h>
#include <rex/logging.h>
#include <array>
#include <cstring>
#include <cstdlib>
#include "vf2_keyboard.h"
#include "vf2_fight.h"

namespace vf2 {
namespace {
using namespace rex::input;
using rex::X_STATUS;
using rex::X_RESULT;
constexpr uint64_t id_base = 0x5646324B00000000ull;
DeviceId Device(unsigned player) { return DeviceId(id_base + player + 1); }
int Player(DeviceId id) {
  const uint64_t n = uint64_t(id) - id_base;
  return n >= 1 && n <= 2 ? int(n - 1) : -1;
}
class Assignment final : public DeviceAssignment {
  void OnDevicesChanged(const std::vector<DeviceInfo>&) override {}
  void DevicesForUser(uint32_t user, std::vector<DeviceId>& out) const override {
    if (user < 2) out.push_back(Device(user));
  }
};
struct Key { int key; uint16_t button; };
// Xbox buttons: A/menu confirm, X, B/menu back. Action mapping is checked
// against the game's controller configuration during the runtime test.
constexpr std::array<std::array<Key, 8>, 2> bindings{{
  {{{'A',4},{'D',8},{'W',1},{'S',2},{'V',0x1000},{'B',0x4000},{'C',0x2000},{'Z',0x10}}},
  {{{VK_LEFT,4},{VK_RIGHT,8},{VK_UP,1},{VK_DOWN,2},{VK_DELETE,0x1000},
    {VK_END,0x4000},{VK_NEXT,0x2000},{VK_RETURN,0x10}}}
}};
class KeyboardFallback final : public InputDriver {
 public:
  KeyboardFallback() : InputDriver(nullptr, 0) {}
  X_STATUS Setup() override { return 0; }
  void EnumerateDevices(std::vector<DeviceInfo>& out) override {
    for (unsigned p = 0; p < 2; ++p)
      out.push_back({Device(p), 0, "VF2 Player " + std::to_string(p + 1) + " (pad / keyboard)",
                     "vf2-auto-player-" + std::to_string(p + 1), true});
  }
  X_RESULT GetDeviceState(DeviceId id, X_INPUT_STATE* out) override {
    const int p = Player(id);
    if (p < 0 || !out) return ERROR_DEVICE_NOT_CONNECTED;
    XINPUT_STATE physical{};
    static const bool test_keyboard = std::getenv("VF2_FORCE_KEYBOARD_TEST") != nullptr;
    const bool pad = !test_keyboard && XInputGetState(p, &physical) == ERROR_SUCCESS;
    X_INPUT_GAMEPAD state{};
    DWORD foreground = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &foreground);
    if (is_active() && foreground == GetCurrentProcessId()) {
      const int menu = fight::MenuInput();
      if (pad) {
        const auto& g = physical.Gamepad;
        state.buttons=g.wButtons; state.left_trigger=g.bLeftTrigger; state.right_trigger=g.bRightTrigger;
        state.thumb_lx=g.sThumbLX; state.thumb_ly=g.sThumbLY; state.thumb_rx=g.sThumbRX; state.thumb_ry=g.sThumbRY;
      }
      uint16_t keys = 0;
      if (menu) {
        // Menus: the whole keyboard is one navigation device. Both key sets
        // move, Enter confirms (A), Esc goes back (B). Frontend menus listen
        // to the active user (player 1); the pause menu to whoever paused.
        // Enter is also player 2's Start in the frontend (Offline Versus asks
        // for it to join); menus ignore Start from non-active users.
        if (p == 1 && menu == 1 && !pad && (GetAsyncKeyState(VK_RETURN) & 0x8000)) keys |= 0x10;
        if (p == 0 || menu == 2) {
          for (const auto& set : bindings)
            for (const auto& binding : set)
              if (binding.key != VK_RETURN && (GetAsyncKeyState(binding.key) & 0x8000)) keys |= binding.button;
          if (GetAsyncKeyState(VK_RETURN) & 0x8000) keys |= 0x1000;
          if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) keys |= 0x2000;
        }
      } else if (!pad) {
        for (const auto& binding : bindings[p])
          if (GetAsyncKeyState(binding.key) & 0x8000) keys |= binding.button;
      }
      state.buttons = uint16_t(uint16_t(state.buttons) | keys);
    }
    if (source_[p] != int(pad)) {
      source_[p] = int(pad); ++packet_[p];
      REXLOG_INFO("VF2 player {} input: {}", p + 1, pad ? "Xbox controller" : "keyboard fallback");
    }
    if (std::memcmp(&state, &previous_[p], sizeof(state))) {
      previous_[p] = state; ++packet_[p];
    }
    *out = {}; out->packet_number = packet_[p]; out->gamepad = state;
    return ERROR_SUCCESS;
  }
  X_RESULT GetDeviceCapabilities(DeviceId id, uint32_t, X_INPUT_CAPABILITIES* out) override {
    if (Player(id) < 0 || !out) return ERROR_DEVICE_NOT_CONNECTED;
    *out={}; out->type=1; out->sub_type=1; out->gamepad.buttons=0xF33F;
    out->gamepad.left_trigger=255; out->gamepad.right_trigger=255;
    out->gamepad.thumb_lx=32767; out->gamepad.thumb_ly=32767;
    out->gamepad.thumb_rx=32767; out->gamepad.thumb_ry=32767;
    return ERROR_SUCCESS;
  }
  X_RESULT SetDeviceVibration(DeviceId id, X_INPUT_VIBRATION* v) override {
    const int p=Player(id); if(p<0 || !v) return ERROR_DEVICE_NOT_CONNECTED;
    XINPUT_VIBRATION physical{uint16_t(v->left_motor_speed),uint16_t(v->right_motor_speed)};
    XInputSetState(p, &physical);
    return ERROR_SUCCESS;
  }
  X_RESULT GetDeviceKeystroke(DeviceId, uint32_t, X_INPUT_KEYSTROKE*) override { return ERROR_EMPTY; }
 private:
  std::array<X_INPUT_GAMEPAD, 2> previous_{};
  std::array<uint32_t, 2> packet_{};
  std::array<int, 2> source_{{-1,-1}};
};
}
void InstallKeyboardFallback(rex::input::InputSystem& input) {
  input.AddDriver(std::make_unique<KeyboardFallback>());
  input.SetDeviceAssignment(std::make_unique<Assignment>());
}
}
