#pragma once
#include <cmath>
#include <algorithm>
namespace vf2::render {
struct FixedClock {
  double boundary=0;
  bool started=false;
  unsigned Advance(double now) {
    if(!started || now-boundary>0.25 || now<boundary) {
      started=true;boundary=now;return 1;
    }
    const auto due=unsigned(std::floor((now-boundary)*60+1e-8));
    boundary+=double(due)/60;
    return due;
  }
};
struct Clock {
  double phase=1;
  unsigned tick=0,frame=0;
  bool Step(double hz) {
    ++frame;
    bool update=phase>=1-1e-9;
    if(update){phase-=1;++tick;}
    phase+=60.0/hz;
    return update;
  }
  double Alpha(double hz) const {return std::clamp(phase-60.0/hz,0.0,1.0);}
};
}
