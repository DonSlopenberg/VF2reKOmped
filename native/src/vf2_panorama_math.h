#pragma once

#include <cmath>
#include <cstdint>

namespace vf2::panorama {

inline constexpr int kPanoramaBankSize=512;
inline constexpr int kPanoramaBankGuard=8;
inline constexpr int kPanoramaBankStep=kPanoramaBankSize-2*kPanoramaBankGuard;

// Period is the complete source panorama width (or original vertical period),
// never the width of the camera-dependent tile cache. Requires period > 0.
inline double Wrap(double position, double period) {
  double phase = std::fmod(position, period);
  if (phase < 0.0) phase += period;
  return phase == 0.0 ? 0.0 : phase;
}

// Native 0x245C0 shifts the signed camera pitch right by four bits,
// then negates it and adds the hardware bank offset. Keep that integer port
// authoritative and restore only its discarded fraction, with the same sign.
// This also preserves negative angles and any native 8-pixel bank adjustment.
inline double NativeVerticalPhase(uint32_t encoded, uint16_t delayed_pitch) {
  const uint32_t vertical=encoded&1023;
  const double coarse=vertical>=512 ? int(vertical)-1024 : int(vertical);
  const unsigned remainder=(uint32_t(delayed_pitch)-23u*32u)&15u;
  return coarse-double(remainder)/16.0;
}

// Exact 10-bit vertical port written at 0x2465C for a delayed pitch sample:
// -(((pitch-736)>>4)) plus the 8-pixel bank byte at 0x50303E. The constant
// 8192 term vanishes modulo 1024.
inline uint32_t NativeVerticalPort(uint16_t pitch, uint8_t bank) {
  const uint32_t coarse=(uint32_t(int32_t(int16_t(pitch)))-23u*32u)>>4;
  return ((0u-coarse)+uint32_t(bank)*8u)&1023u;
}

// The fraction must come from the very sample that produced the integer port.
// Runtime traces show that sample is the current pitch (0x50300C at 0x24664);
// pairing the port with the older entry-time 0x503010 sample produced a whole
// texel spike at every 16-unit boundary. Choose the first candidate that
// reproduces the native port; return false if none does (keep the integer).
inline bool SelectNativePitch(uint32_t encoded, uint8_t bank,
                              const uint16_t* candidates, int count, uint16_t& pitch) {
  for(int i=0;i<count;++i)
    if(NativeVerticalPort(candidates[i],bank)==(encoded&1023u)) {
      pitch=candidates[i];return true;
    }
  return false;
}

struct AxisWindow {
  int tile_origin;
  double screen_origin;
};

// source_offset and guard are integral source pixels. Moving the tile window
// in whole tiles leaves the fractional camera phase in the draw position.
// Consequently source(screen) = source_offset - scroll + screen, including
// when scroll crosses zero or an 8-pixel tile boundary.
inline AxisWindow MakeAxisWindow(double source_offset, double scroll, int guard) {
  const double coarse = std::floor(scroll / 8.0) * 8.0;
  return {static_cast<int>(source_offset - coarse) - guard,
          scroll - coarse - guard};
}

inline double WideExtent(double aspect) { return 384.0 * aspect; }
inline double Margin(double aspect) { return (WideExtent(aspect) - 496.0) / 2.0; }

// Model 2 supplies an already projected, flat scrolling bitmap. Applying tan
// again bends that image and changes magnification across its width. FOV here
// selects the visible fraction of the panorama with one uniform affine scale.
inline double PanoramaSource(double screen, double extent, double width, double fov) {
  return (screen-248.0)*(width*fov/360.0)/extent;
}
inline double PanoramaScreen(double source_offset, double extent, double width, double fov) {
  return 248.0+source_offset*extent/(width*fov/360.0);
}
inline double PanoramaScale(double extent,double width,double fov) {
  return extent/(width*fov/360.0);
}
inline double PanoramaVertical(double y,double scale,double horizon) {
  return 192.0+(y-192.0)*scale-horizon;
}

// Immutable pose shared by every render of one original simulation tick.
// No velocity integration, host refresh dependency, or separate background lag.
struct CameraPose {
  uint32_t tick=0;
  int arena=-1;
  double x=0,y=0;
  uint64_t samples=0;
  void Capture(uint32_t update,int panorama,double phase_x,double phase_y) {
    if(arena==panorama && tick==update)return;
    tick=update;arena=panorama;x=phase_x;y=phase_y;++samples;
  }
  void Invalidate(){arena=-1;}
};

}  // namespace vf2::panorama
