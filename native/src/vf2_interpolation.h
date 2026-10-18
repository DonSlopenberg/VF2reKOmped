#pragma once
namespace vf2::render {
double Rate();
double Alpha();
unsigned Tick();
unsigned SimulationSteps();
void CommitStep();
}
#include <cstdint>
namespace vf2::render {
// Records the shadow-board variant fractions (ROM 0x1FACC); see vf2_interpolation.cpp.
void ShadowBoardInstruction(uint8_t* base, uint32_t pc);
}
