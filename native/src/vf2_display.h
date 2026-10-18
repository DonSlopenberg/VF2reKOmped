#pragma once
#include <rex/cvar.h>
#include <string>
#include <charconv>
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace vf2::display {
struct Size { int width=1920,height=1080; };
inline Size output,guest;
inline int draw_scale=1;
inline bool Parse(std::string_view value,Size& size) {
  const auto split=value.find('x');
  if(split==std::string_view::npos)return false;
  int w=0,h=0;
  const auto a=std::from_chars(value.data(),value.data()+split,w);
  const auto b=std::from_chars(value.data()+split+1,value.data()+value.size(),h);
  if(a.ec!=std::errc{} || b.ec!=std::errc{} || a.ptr!=value.data()+split ||
      b.ptr!=value.data()+value.size() || w<640 || w>7680 || h<480 || h>4320)return false;
  size={w,h};return true;
}
// Left edge, in UI units, of what the sprite/AET projection shows: layouts
// stay centred on the 1920-wide mode 13, so a wider screen adds margins.
inline double UiLeft(double ui_width) { return ui_width>1920.0 ? 960.0-ui_width/2 : 0.0; }
inline bool InternalFlag(std::string_view name) {
  return name=="video_mode_width" || name=="video_mode_height" || name=="resolution" ||
      name=="resolution_scale" || name=="draw_resolution_scale_x" ||
      name=="draw_resolution_scale_y";
}
inline void Configure() {
  using namespace rex::cvar;
  auto requested=GetFlagByName("vf2_output_resolution");
  const auto source=GetFlagSource("vf2_output_resolution");
  // Preserve existing command-line test/custom modes. Migrate old saved UHD
  // configs from their output resolution, never their divided guest dimensions.
  if(source<Source::kCommandLine &&
      (GetFlagSource("video_mode_width")==Source::kCommandLine ||
       GetFlagSource("video_mode_height")==Source::kCommandLine))
    requested=GetFlagByName("video_mode_width")+"x"+GetFlagByName("video_mode_height");
  else if(source==Source::kDefault && GetFlagSource("resolution")!=Source::kDefault)
    requested=GetFlagByName("resolution");
  else if(source==Source::kDefault && GetFlagSource("video_mode_width")!=Source::kDefault)
    requested=GetFlagByName("video_mode_width")+"x"+GetFlagByName("video_mode_height");
  Size selected;
  if(!Parse(requested,selected))requested="1920x1080";
  Parse(requested,output);
  // The guest frame must fit the 10 MB EDRAM: 1920x1080 does, wider 1080-line
  // frames (2400x1080, 2560x1080) wrap into corrupted stripes. 16:9 and
  // narrower outputs render a 1080-line frame; wider (ultrawide) outputs render
  // 1920 x 1920/aspect. The output size is reached with the GPU draw scale plus
  // the presenter's scaling (QHD is downsampled from 2x, 2560x1080 from
  // 3840x1620). The game's 2D layout stays on a virtual 1080-line screen
  // (vf2_video.cpp), so any guest height works.
  const double aspect=double(output.width)/output.height;
  if(aspect>16.0/9.0+0.01)guest={1920,int(std::lround(1920.0/aspect))&~1};
  else guest={int(std::lround(1080.0*aspect))&~1,1080};
  if(const char* lines=std::getenv("VF2_TEST_GUEST_LINES")) {  // diagnostics
    const int h=std::clamp(std::atoi(lines),360,1080);
    guest={int(std::lround(double(h)*aspect))&~1,h};
  }
  draw_scale=output.height>guest.height?2:1;
  SetFlagByName("vf2_output_resolution",requested);
  SetFlagByName("resolution",requested);
  SetFlagByName("video_mode_width",std::to_string(guest.width));
  SetFlagByName("video_mode_height",std::to_string(guest.height));
  for(const char* name:{"resolution_scale","draw_resolution_scale_x","draw_resolution_scale_y"})
    SetFlagByName(name,std::to_string(draw_scale));
  for(auto& flag:GetRegistry())if(InternalFlag(flag.name)) {
    flag.category="Advanced Display";
    flag.lifecycle=Lifecycle::kInitOnly;
    flag.description="Derived from VF2 > Display > Resolution; do not edit independently";
  }
}
}
