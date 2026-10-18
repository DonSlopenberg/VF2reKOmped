#include "vf2_monitors.h"
#include <cassert>
#include <iostream>
int main() {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  const auto displays=vf2::monitors::List();assert(!displays.empty());
  for(size_t i=0;i<displays.size();++i) {
    const auto& d=displays[i];
    assert(vf2::monitors::Selected(displays,vf2::monitors::Narrow(d.device))==i);
    std::cout<<i<<" | "<<vf2::monitors::Narrow(d.name)<<" | "<<vf2::monitors::Narrow(d.device)<<" | "<<d.screen.right-d.screen.left<<'x'<<d.screen.bottom-d.screen.top<<'\n';
  }
  assert(vf2::monitors::Selected(displays,"disconnected monitor")==0);
}
