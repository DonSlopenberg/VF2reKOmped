#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>
namespace vf2::monitors {
struct Display {HMONITOR handle;std::wstring device,name;RECT screen,work;bool primary;};
inline std::vector<Display> List() {
  std::vector<Display> result;
  EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR handle,HDC,LPRECT,LPARAM data)->BOOL {
    auto& displays=*reinterpret_cast<std::vector<Display>*>(data);
    MONITORINFOEXW info{};info.cbSize=sizeof(info);
    if(!GetMonitorInfoW(handle,reinterpret_cast<MONITORINFO*>(&info)))return TRUE;
    DISPLAY_DEVICEW monitor{};monitor.cb=sizeof(monitor);
    EnumDisplayDevicesW(info.szDevice,0,&monitor,0);
    Display display{handle,info.szDevice,monitor.DeviceString,info.rcMonitor,info.rcWork,(info.dwFlags&MONITORINFOF_PRIMARY)!=0};
    displays.push_back(display);return TRUE;
  },reinterpret_cast<LPARAM>(&result));
  UINT32 path_count=0,mode_count=0;
  if(GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,&path_count,&mode_count)==ERROR_SUCCESS) {
    std::vector<DISPLAYCONFIG_PATH_INFO> paths(path_count);std::vector<DISPLAYCONFIG_MODE_INFO> modes(mode_count);
    if(QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,&path_count,paths.data(),&mode_count,modes.data(),nullptr)==ERROR_SUCCESS)
      for(UINT32 i=0;i<path_count;++i) {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};source.header={DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME,sizeof(source),paths[i].sourceInfo.adapterId,paths[i].sourceInfo.id};
        DISPLAYCONFIG_TARGET_DEVICE_NAME target{};target.header={DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME,sizeof(target),paths[i].targetInfo.adapterId,paths[i].targetInfo.id};
        if(DisplayConfigGetDeviceInfo(&source.header)==ERROR_SUCCESS && DisplayConfigGetDeviceInfo(&target.header)==ERROR_SUCCESS)
          for(auto& display:result)if(display.device==source.viewGdiDeviceName && target.monitorFriendlyDeviceName[0])display.name=target.monitorFriendlyDeviceName;
      }
  }
  std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.primary && !b.primary;});
  for(auto& display:result)if(display.name.empty())display.name=display.device;
  return result;
}
inline std::string Narrow(const std::wstring& text) {
  if(text.empty())return {};
  int size=WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),nullptr,0,nullptr,nullptr);
  std::string result(size,0);WideCharToMultiByte(CP_UTF8,0,text.data(),int(text.size()),result.data(),size,nullptr,nullptr);return result;
}
inline size_t Selected(const std::vector<Display>& displays,const std::string& device) {
  for(size_t i=0;i<displays.size();++i)if(Narrow(displays[i].device)==device)return i;
  return 0;
}
}
