#pragma once
#include <cmath>
#include <cstdint>

namespace vf2 {
// One result per bitmap frame, shared by the before/after-3D priority passes.
struct ScrollFilter {
  double position = 0, period = 0, time = 0;
  uint64_t frame = 0;
  int mode = -1;
  double Update(double target, double wrap, int layout, uint64_t current_frame,
                double now) {
    if (!std::isfinite(target) || !std::isfinite(now) || wrap <= 0) return target;
    if (frame == current_frame && period == wrap && mode == layout)
      return target + std::remainder(position - target, wrap);
    const double dt = now - time;
    const double delta = std::remainder(target - position, wrap);
    if (period != wrap || mode != layout || current_frame != frame + 1 ||
        dt <= 0 || dt > 0.1 || std::abs(delta) > 48)
      position = target;
    else
      position += delta * (1.0 - std::exp(-dt / 0.012));
    position = target + std::remainder(position - target, wrap);
    period = wrap; mode = layout; frame = current_frame; time = now;
    return position;
  }
};
}
