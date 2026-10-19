#pragma once
#include <cstdint>

namespace vf2::menu {
// Applied to each polled controller state (users 0/1) after every input source.
void FilterInput(uint8_t* base, unsigned user, uint32_t state);
// True once after A was pressed on Settings > Advanced Settings.
bool TakeOpenSettingsRequest();
}
