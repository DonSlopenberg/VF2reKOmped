#include "../src/vf2_panorama_math.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
unsigned long long checks = 0;
void Require(bool condition, std::string_view message) {
  ++checks;
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
void Near(double actual, double expected, std::string_view message,
          double tolerance = 1e-9) {
  Require(std::isfinite(actual) && std::abs(actual - expected) < tolerance,
          message);
}
double Source(const vf2::panorama::AxisWindow& window, double screen) {
  return window.tile_origin + screen - window.screen_origin;
}
}  // namespace

int main() {
  using namespace vf2::panorama;
  // Replay the original 0x245BC..0x24658 vertical port calculation. Its
  // integer division discards four camera bits; the renderer must retain
  // those bits without changing the selected scroll bank or adding lag.
  for(int raw=0;raw<65536;++raw) {
    const int pitch=raw>=32768?raw-65536:raw;
    const uint32_t shifted=uint32_t(pitch-23*32)>>4;
    for(uint32_t bank : {0u,1u,7u,31u}) {
      const uint32_t encoded=((0u-shifted)&1023)+8192+bank*8;
      const double expected=Wrap(-(pitch-23*32)/16.0+bank*8,1024);
      Near(Wrap(NativeVerticalPhase(encoded,uint16_t(raw)),1024),expected,
           "native pitch retains fractional texels including negative angles and bank offsets");
    }
  }
  // The port encoder must equal the native arithmetic for every sample/bank.
  for(int raw=0;raw<65536;++raw) {
    const int pitch=raw>=32768?raw-65536:raw;
    const uint32_t shifted=uint32_t(pitch-23*32)>>4;
    for(uint32_t bank : {0u,1u,7u,31u})
      Require(NativeVerticalPort(uint16_t(raw),uint8_t(bank))==(((0u-shifted)&1023)+8192+bank*8&1023),
              "port encoder replays 0x245BC..0x2465C");
  }
  // Regression: a stale entry sample (one update old) combined with a fresh
  // integer port produced +1 texel spikes at every 16-unit pitch boundary.
  // Selecting the sample that reproduces the port removes them.
  for(int start : {-700,-1,0,736,5000,32000})
   for(int direction : {-1,1}) {
    double previous=0;bool have=false;
    for(int tick=1;tick<400;++tick) {
      const uint16_t used=uint16_t(start+direction*tick);
      const uint16_t stale=uint16_t(start+direction*(tick-1));
      const uint32_t port=NativeVerticalPort(used,3);
      // Renderer order: a missed entry hook puts the previous post-advance
      // value (the true entry sample) first, the stale entry sample second.
      const bool missed=tick%3!=0;
      const uint16_t candidates[2]={used,missed?stale:used};
      uint16_t chosen=0;
      Require(SelectNativePitch(port,3,candidates,2,chosen),"matching sample is found");
      const double y=NativeVerticalPhase(port,chosen);
      if(have)Near(std::remainder(y-previous,1024.0),-direction/16.0,
                   "vertical phase advances by exactly one pitch unit per update",1e-9);
      previous=y;have=true;
      // The legacy pairing visibly jumps a whole texel at boundaries.
      if(tick%3 && ((uint32_t(used)-736)&15)==(direction<0?15u:0u))
        Require(std::abs(std::remainder(NativeVerticalPhase(port,stale)-y,1024.0))>0.5,
                "stale fraction reproduces the measured texel spike");
    }
   }
  for(int step=0;step<=128;++step) {
    const double phase=step/16.0;
    const double left=-256+phase;
    const double seam=left+kPanoramaBankSize-kPanoramaBankGuard;
    Near(seam,left+kPanoramaBankStep+kPanoramaBankGuard,
         "guarded RGBA banks meet at the same subpixel position");
    Require(kPanoramaBankGuard>=1,
            "linear sample footprint cannot reach a neighboring atlas quadrant");
    Require(248-2048*140.0/720>left+kPanoramaBankGuard &&
            248+2048*140.0/720<left+kPanoramaBankStep+kPanoramaBankSize-kPanoramaBankGuard,
            "widest supported FOV stays inside both banks' safe sample area");
  }
  for(double extent : {512.0,682.666666666667})for(double width : {256.0,1024.0,2048.0})
    for(double fov : {40.0,60.0,90.0,120.0,140.0}) {
      double previous=-1e20;
      for(int step=0;step<=4096;++step) {
        const double screen=248.0+extent*(double(step)/4096.0-0.5);
        const double source=PanoramaSource(screen,extent,width,fov);
        Require(source>previous,"panorama samples remain monotonic, never fold");
        if(step)Near(source-previous,width*fov/360.0/4096.0,"uniform scale across the entire image");
        previous=source;
        Near(PanoramaScreen(source,extent,width,fov),screen,"affine projection inverse",1e-8);
      }
      Near(PanoramaSource(248,extent,width,fov),0,"background center independent of FOV");
      Near(PanoramaSource(248+extent/2,extent,width,fov),width*fov/720.0,"horizontal FOV edge");
      const double scale=PanoramaScale(extent,width,fov);
      for(double horizon : {-192.0,-64.0,0.0,64.0,192.0}) {
        Near(PanoramaVertical(212,scale,horizon)-PanoramaVertical(192,scale,horizon),
             PanoramaScreen(20,extent,width,fov)-PanoramaScreen(0,extent,width,fov),
             "FOV scales both axes equally, without aspect distortion",1e-8);
        Near(PanoramaVertical(192,scale,horizon),192-horizon,
             "horizon offset is independent of FOV zoom");
      }
    }
  for(int host_hz : {60,90,120,144,240,360}) {
    CameraPose pose;
    for(int render=0;render<=host_hz*10;++render) {
      const uint32_t tick=uint64_t(render)*60/host_hz;
      const double x=Wrap(tick*3.25,2048);
      const double y=NativeVerticalPhase((0u-(tick>>4))&1023,uint16_t(736+tick));
      pose.Capture(tick,0,x,y);
      Near(pose.x,x,"camera is synchronized at every host refresh");
      Near(pose.y,y,"pitch remains synchronized at every host refresh");
      Near(pose.y,-double(tick)/16,"slow pitch retains sixteenth-texel steps at every refresh schedule");
      const auto samples=pose.samples;
      pose.Capture(tick,0,x+123,y-999);
      Near(pose.x,x,"repeated passes cannot apply yaw twice");
      Near(pose.y,y,"repeated passes cannot apply pitch twice");
      Require(pose.samples==samples,"once per original simulation tick");
    }
    Require(pose.samples==601,"refresh-independent number of simulation samples");
    pose.Capture(601,0,900,-120);
    Near(pose.x,900,"camera cuts are not smoothed or delayed");
    pose.Invalidate();pose.Capture(601,0,12,34);
    Near(pose.x,12,"scene reset invalidates cached pose");
  }
  for (double period : {256.0, 512.0, 1024.0, 2048.0}) {
    for (int step = -32768; step <= 32768; ++step) {
      const double position = step / 8.0;
      const double phase = Wrap(position, period);
      Require(phase >= 0 && phase < period, "positive wrap range");
      Near(Wrap(position + 3 * period, period), phase, "forward wrap");
      Near(Wrap(position - 3 * period, period), phase, "backward wrap");
    }
    Near(Wrap(-period, period), 0, "negative exact period");
    Require(!std::signbit(Wrap(-period, period)), "wrap has positive zero");
  }

  // Sweep a complete panorama rotation in both directions, including every
  // fractional subpixel phase and every original 512/1024 cache boundary.
  for (double period : {512.0, 1024.0, 2048.0}) {
    for (int step = -16384; step <= 16384; ++step) {
      const double scroll = step / 8.0;
      for (int offset : {-2048, -256, 0, 256, 1792}) {
        const auto window = MakeAxisWindow(offset, scroll, 16);
        Require(window.screen_origin >= -16 && window.screen_origin < -8,
                "bounded fractional draw phase");
        Require(window.tile_origin % 8 == 0, "tile origin stays aligned");
        for (double x : {-93.333333333333, 0.0, 248.0, 496.0,
                         589.333333333333}) {
          Near(Source(window, x), offset - scroll + x,
               "unchanged original camera mapping");
          Near(Wrap(Source(window, x), period),
               Wrap(offset - scroll + x, period), "wrapped source mapping");
        }
      }
    }
  }

  // Tiles on either side of every edge must produce exactly the same texture
  // coordinate there; crossing a cache/source seam must not change the phase.
  for (int boundary = -256; boundary <= 256; ++boundary) {
    const double scroll = boundary * 8.0;
    const auto before = MakeAxisWindow(256, scroll - 0.001, 16);
    const auto after = MakeAxisWindow(256, scroll + 0.001, 16);
    Near(Source(before, 248) - Source(after, 248), 0.002,
         "continuous camera phase across tile boundary", 1e-8);
    for (int tile = 0; tile < 90; ++tile) {
      const double edge = after.screen_origin + (tile + 1) * 8.0;
      const double left_uv = after.tile_origin + tile * 8.0 + 8.0;
      const double right_uv = after.tile_origin + (tile + 1) * 8.0;
      Near(left_uv, right_uv, "adjacent tile edges coincide");
      Near(Source(after, edge), right_uv, "tile edge source agrees");
    }
  }

  Near(WideExtent(4.0 / 3.0), 512, "4:3 viewport extent");
  Near(WideExtent(16.0 / 9.0), 2048.0 / 3.0, "16:9 viewport extent");
  Near(Margin(496.0 / 384.0), 0, "original authored viewport unchanged");
  for (double aspect : {496.0 / 384.0, 4.0 / 3.0, 16.0 / 9.0}) {
    const double extent = WideExtent(aspect);
    Near(extent / 2 - Margin(aspect), 248, "camera center unchanged");
    Near(-Margin(aspect) + extent, 496 + Margin(aspect),
         "symmetric horizontal extension");
    const double tan_half_fov_y = std::tan(45.0 * std::acos(-1.0) / 360.0);
    const double focal = 384.0 / (2 * tan_half_fov_y);
    const double fov_x = 2 * std::atan(tan_half_fov_y * aspect);
    Near(focal * std::tan(fov_x / 2), extent / 2,
         "horizontal extent agrees with fixed vertical projection");
  }
  std::cout << "PASS: " << checks
            << " checks; full 2048-pixel rotations, 512/1024 vertical wraps, "
               "fractional negative scroll, tile continuity, 4:3/16:9 center "
               "and projection extents.\n";
}
