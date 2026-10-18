#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdlib>
#include <iostream>

// Temporary display mode only: never persists a registry preference.
int main(int argc,char** argv) {
  DISPLAY_DEVICEW device{};device.cb=sizeof(device);
  for(DWORD i=0;EnumDisplayDevicesW(nullptr,i,&device,0);++i)
    if(device.StateFlags&DISPLAY_DEVICE_PRIMARY_DEVICE)break;
  DEVMODEW mode{};mode.dmSize=sizeof(mode);
  if(!EnumDisplaySettingsW(device.DeviceName,ENUM_CURRENT_SETTINGS,&mode))return 1;
  if(argc>1) {
    const auto requested=DWORD(std::atoi(argv[1]));
    DEVMODEW candidate{};candidate.dmSize=sizeof(candidate);
    bool found=false;
    for(DWORD i=0;EnumDisplaySettingsW(device.DeviceName,i,&candidate);++i)
      if(candidate.dmPelsWidth==mode.dmPelsWidth && candidate.dmPelsHeight==mode.dmPelsHeight &&
         candidate.dmBitsPerPel==mode.dmBitsPerPel && candidate.dmDisplayFrequency==requested) {
        found=true;mode=candidate;break;
      }
    if(!found){std::cerr<<"Requested refresh unavailable\n";return 2;}
    mode.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL|DM_DISPLAYFREQUENCY;
    const auto result=ChangeDisplaySettingsExW(device.DeviceName,&mode,nullptr,CDS_FULLSCREEN,nullptr);
    if(result!=DISP_CHANGE_SUCCESSFUL){std::cerr<<"Display mode failed: "<<result<<'\n';return 3;}
    EnumDisplaySettingsW(device.DeviceName,ENUM_CURRENT_SETTINGS,&mode);
    if(mode.dmDisplayFrequency!=requested)return 4;
  }
  std::cout<<mode.dmPelsWidth<<' '<<mode.dmPelsHeight<<' '<<mode.dmDisplayFrequency<<'\n';
}
