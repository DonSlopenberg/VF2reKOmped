#include "../src/vf2_render_clock.h"
#include <cassert>
#include <iostream>
int main() {
  for(int hz:{30,60,90,120,144,240,360}) {
    vf2::render::FixedClock clock;
    unsigned ticks=0;
    for(int frame=0;frame<=hz*10;++frame)ticks+=clock.Advance(double(frame)/hz);
    assert(ticks==601);
    assert(clock.Advance(100)==1); // A pause resumes once, without fast-forward.
    assert(clock.Advance(100)==0); // Repeated render callbacks cannot advance logic.
  }
  vf2::render::FixedClock clock;
  unsigned ticks=0;
  for(int frame=0;frame<=1000;++frame)ticks+=clock.Advance(frame*0.01);
  assert(ticks==601); // 100fps renderer must still execute 60Hz simulation.
  std::cout<<"PASS: fixed simulation timing at 30..360Hz, repeated renders, and pause boundaries\n";
}
