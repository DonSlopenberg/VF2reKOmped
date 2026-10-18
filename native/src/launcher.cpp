#define UNICODE
#define _UNICODE
#define NOMINMAX
#include <windows.h>
#include <Xinput.h>
#include <filesystem>
#include <string>
#include <iterator>
#include <vector>
#include <algorithm>
#include <gdiplus.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <commctrl.h>
#include <memory>
#include "launcher_config.h"
#include "vf2_monitors.h"
#include "vf2_version.h"

namespace {
HWND resolutionBox, monitorBox, fullscreenBox, introBox, upscaledBox, touristBox, opponentsBox, arenasBox, controllerStatus, playButton;
std::vector<vf2::monitors::Display> monitors;
size_t selectedMonitor=0;
std::wstring MonitorLabel(size_t index) {
  const auto& display=monitors[index];
  return display.name+(monitors.size()>1?L"  ·  Display "+std::to_wstring(index+1):L"");
}
HFONT font, smallFont;
HBRUSH backgroundBrush;
COLORREF backgroundColor, textColor, mutedColor, accentColor;
bool dark=false, loading=false;
ULONG_PTR gdiplusToken;
IStream* logoStream=nullptr;
std::unique_ptr<Gdiplus::Bitmap> logo;
UINT dpi = 96;
int Scale(int pixels) { return MulDiv(pixels, dpi, 96); }
std::filesystem::path folder;
const wchar_t* presets[] = {L"1280x720", L"1920x1080", L"2560x1440", L"3840x2160", L"2560x1080", L"3440x1440"};
const wchar_t* presetLabels[] = {L"1280 × 720  ·  HD", L"1920 × 1080  ·  Full HD", L"2560 × 1440  ·  QHD", L"3840 × 2160  ·  UHD",
                                 L"2560 × 1080  ·  Ultrawide 21:9", L"3440 × 1440  ·  Ultrawide QHD 21:9"};
const int presetSize[][2] = {{1280,720},{1920,1080},{2560,1440},{3840,2160},{2560,1080},{3440,1440}};
constexpr int presetCount = int(std::size(presets));
std::filesystem::path ConfigPath() {
  return std::filesystem::exists(folder/L"data"/L"engine")?
      folder/L"data"/L"engine"/L"vf2.toml":folder/L"vf2.toml";
}
void LoadSettings() {
  loading=true;
  const auto values=vf2::launcher::Read(ConfigPath());
  auto value=[&](const char* key,std::string fallback) {auto it=values.find(key);return it==values.end()?fallback:it->second;};
  const auto resolution=value("vf2_output_resolution",value("resolution","1920x1080"));
  selectedMonitor=vf2::monitors::Selected(monitors,value("vf2_display_device",""));
  if(monitorBox)SendMessageW(monitorBox,CB_SETCURSEL,selectedMonitor,0);
  int selected=1;
  for(int i=0;i<presetCount;++i)if(resolution==std::string(presets[i],presets[i]+wcslen(presets[i])))selected=i;
  SendMessageW(resolutionBox,CB_SETCURSEL,selected,0);
  SendMessageW(fullscreenBox,BM_SETCHECK,value("fullscreen","false")=="true"?BST_CHECKED:BST_UNCHECKED,0);
  SendMessageW(introBox,BM_SETCHECK,value("vf2_no_intro","false")=="true"?BST_CHECKED:BST_UNCHECKED,0);
  SendMessageW(upscaledBox,BM_SETCHECK,value("vf2_background_replacements","false")=="true"?BST_CHECKED:BST_UNCHECKED,0);
  SendMessageW(touristBox,BM_SETCHECK,value("vf2_arcade_tourist","false")=="true"?BST_CHECKED:BST_UNCHECKED,0);
  SendMessageW(opponentsBox,BM_SETCHECK,value("vf2_arcade_random_opponents","false")=="true"?BST_CHECKED:BST_UNCHECKED,0);
  SendMessageW(arenasBox,BM_SETCHECK,value("vf2_arcade_random_arenas","false")=="true"?BST_CHECKED:BST_UNCHECKED,0);
  loading=false;
}
bool SaveSettings(HWND parent) {
  if(loading)return true;
  int selected=int(SendMessageW(resolutionBox,CB_GETCURSEL,0,0));
  if(selected<0 || selected>=presetCount)selected=1;
  if(vf2::launcher::Save(ConfigPath(),std::string(presets[selected],presets[selected]+wcslen(presets[selected])),
      SendMessageW(fullscreenBox,BM_GETCHECK,0,0)==BST_CHECKED,
      SendMessageW(introBox,BM_GETCHECK,0,0)==BST_CHECKED,
      monitors.empty()?"":vf2::monitors::Narrow(monitors[selectedMonitor].device),
      SendMessageW(upscaledBox,BM_GETCHECK,0,0)==BST_CHECKED,
      SendMessageW(touristBox,BM_GETCHECK,0,0)==BST_CHECKED,
      SendMessageW(opponentsBox,BM_GETCHECK,0,0)==BST_CHECKED,
      SendMessageW(arenasBox,BM_GETCHECK,0,0)==BST_CHECKED))return true;
  MessageBoxW(parent,L"Could not save game settings. Check that the game folder is writable.",L"Virtua Fighter 2 reKOmped",MB_ICONERROR);
  return false;
}
void Theme(HWND window) {
  DWORD light=1, bytes=sizeof(light);
  RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
               L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&light,&bytes);
  dark=!light;
  if(const char* theme=std::getenv("VF2_LAUNCHER_THEME_TEST"))dark=std::string(theme)=="dark";
  backgroundColor=dark?RGB(28,28,28):GetSysColor(COLOR_WINDOW);
  textColor=dark?RGB(245,245,245):GetSysColor(COLOR_WINDOWTEXT);
  mutedColor=dark?RGB(174,174,174):RGB(96,96,96);
  DWORD color=0;BOOL opaque=FALSE;
  accentColor=SUCCEEDED(DwmGetColorizationColor(&color,&opaque))?
      RGB((color>>16)&255,(color>>8)&255,color&255):RGB(0,103,192);
  if(backgroundBrush)DeleteObject(backgroundBrush);
  backgroundBrush=CreateSolidBrush(backgroundColor);
  BOOL enabled=dark;DwmSetWindowAttribute(window,20,&enabled,sizeof(enabled));
  for(HWND control:{resolutionBox,monitorBox,fullscreenBox,introBox,upscaledBox,touristBox,opponentsBox,arenasBox,playButton})
    if(control)SetWindowTheme(control,dark?L"DarkMode_Explorer":L"Explorer",nullptr);
  RedrawWindow(window,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_ERASE);
}
void LoadLogo() {
  const auto instance=GetModuleHandleW(nullptr);
  const auto resource=FindResourceW(instance,MAKEINTRESOURCEW(101),RT_RCDATA);
  if(!resource)return;
  const auto size=SizeofResource(instance,resource);
  auto memory=GlobalAlloc(GMEM_MOVEABLE,size);
  if(!memory)return;
  const auto target=GlobalLock(memory);
  if(!target){GlobalFree(memory);return;}
  memcpy(target,LockResource(LoadResource(instance,resource)),size);GlobalUnlock(memory);
  if(FAILED(CreateStreamOnHGlobal(memory,TRUE,&logoStream))){GlobalFree(memory);return;}
  logo.reset(Gdiplus::Bitmap::FromStream(logoStream));
  if(logo->GetLastStatus()!=Gdiplus::Ok)logo.reset();
}
void CapturePreview(HWND window) {
  // Opt-in readback of this application's own client surface only.
  const wchar_t* path=_wgetenv(L"VF2_LAUNCHER_CAPTURE");if(!path)return;
  RECT area;GetClientRect(window,&area);
  HDC source=GetDC(window),dc=CreateCompatibleDC(source);
  HBITMAP bitmap=CreateCompatibleBitmap(source,area.right,area.bottom);
  const auto previous=SelectObject(dc,bitmap);
  SendMessageW(window,WM_PRINT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT|PRF_CHILDREN|PRF_ERASEBKGND);
  SelectObject(dc,previous);
  BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth=area.right;info.bmiHeader.biHeight=-area.bottom;
  info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
  std::vector<unsigned char> pixels(size_t(area.right)*area.bottom*4);
  const auto rows=GetDIBits(dc,bitmap,0,area.bottom,pixels.data(),&info,DIB_RGB_COLORS);
  if(rows) {
    BITMAPFILEHEADER header{};header.bfType=0x4d42;
    header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);header.bfSize=DWORD(header.bfOffBits+pixels.size());
    std::ofstream output(std::filesystem::path(path),std::ios::binary);
    output.write(reinterpret_cast<const char*>(&header),sizeof(header));
    output.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(info.bmiHeader));
    output.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());
  }
  DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(window,source);
}
// Version of the installed engine executable, read from its VERSIONINFO, so
// the footer stays correct when only data/engine/vf2.exe is replaced.
std::string GameVersion() {
  static std::string cached;
  if(!cached.empty())return cached;
  cached="unknown";
  for(const auto& exe:{folder/L"data"/L"engine"/L"vf2.exe",folder/L"vf2.exe"}) {
    DWORD ignored=0;const DWORD size=GetFileVersionInfoSizeW(exe.c_str(),&ignored);
    if(!size)continue;
    std::vector<uint8_t> data(size);
    VS_FIXEDFILEINFO* info=nullptr;UINT length=0;
    if(!GetFileVersionInfoW(exe.c_str(),0,size,data.data()) ||
       !VerQueryValueW(data.data(),L"\\",reinterpret_cast<void**>(&info),&length) || !info)continue;
    cached=std::to_string(HIWORD(info->dwFileVersionMS))+"."+std::to_string(LOWORD(info->dwFileVersionMS))+
           "."+std::to_string(HIWORD(info->dwFileVersionLS));
    break;
  }
  return cached;
}
void PaintContent(HDC dc) {
  if(logo) {
    Gdiplus::Graphics graphics(dc);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    // Keep the artwork's aspect: full width, height from the image.
    const int height=MulDiv(496,int(logo->GetHeight()),int(logo->GetWidth()));
    graphics.DrawImage(logo.get(),Scale(32),Scale(16),Scale(496),Scale(height));
  }
  SetBkMode(dc,TRANSPARENT);SetTextColor(dc,mutedColor);SelectObject(dc,font);
  RECT edition{Scale(32),Scale(262),Scale(528),Scale(286)};
  DrawTextW(dc,L"2012 Xbox360 Live Arcade Edition",-1,&edition,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
  SetBkMode(dc,TRANSPARENT);SetTextColor(dc,mutedColor);SelectObject(dc,smallFont);
  RECT credits{Scale(32),Scale(808),Scale(528),Scale(828)};
  DrawTextW(dc,L"Created by Sol, Opus and Don Slopenberg",-1,&credits,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
  RECT footer{Scale(16),Scale(830),Scale(544),Scale(852)};
  std::string build="Launcher " VF2_LAUNCHER_VERSION_TEXT "  |  Game "+GameVersion()+"  |  ReXGlue SDK " VF2_REXGLUE_VERSION
                    "  |  Build " VF2_BUILD_STAMP;
  std::wstring label(build.begin(),build.end());
  DrawTextW(dc,label.c_str(),-1,&footer,DT_CENTER|DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
}
void PaintToggle(HWND control,HDC dc) {
  RECT area;GetClientRect(control,&area);FillRect(dc,&area,backgroundBrush);
  const bool checked=SendMessageW(control,BM_GETCHECK,0,0)==BST_CHECKED;
  const int side=Scale(18),top=(area.bottom-side)/2;
  const auto pen=CreatePen(PS_SOLID,Scale(1),checked?accentColor:mutedColor);
  const auto brush=CreateSolidBrush(checked?accentColor:backgroundColor);
  const auto oldPen=SelectObject(dc,pen),oldBrush=SelectObject(dc,brush);
  RoundRect(dc,0,top,side,top+side,Scale(5),Scale(5));
  SelectObject(dc,oldBrush);SelectObject(dc,oldPen);DeleteObject(pen);DeleteObject(brush);
  if(checked) {
    const auto mark=CreatePen(PS_SOLID,Scale(2),RGB(255,255,255));const auto old=SelectObject(dc,mark);
    MoveToEx(dc,Scale(4),top+Scale(9),nullptr);LineTo(dc,Scale(8),top+Scale(13));LineTo(dc,Scale(14),top+Scale(5));
    SelectObject(dc,old);DeleteObject(mark);
  }
  wchar_t label[128];GetWindowTextW(control,label,128);area.left=Scale(28);
  SelectObject(dc,font);SetTextColor(dc,textColor);SetBkMode(dc,TRANSPARENT);
  DrawTextW(dc,label,-1,&area,DT_SINGLELINE|DT_VCENTER);
  if(GetFocus()==control){InflateRect(&area,-1,-Scale(3));DrawFocusRect(dc,&area);}
}
LRESULT CALLBACK ToggleProc(HWND control,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR) {
  if(message==WM_PAINT) {PAINTSTRUCT paint;const auto dc=BeginPaint(control,&paint);PaintToggle(control,dc);EndPaint(control,&paint);return 0;}
  if(message==WM_PRINTCLIENT){PaintToggle(control,reinterpret_cast<HDC>(w));return 0;}
  if(message==WM_ERASEBKGND)return 1;
  return DefSubclassProc(control,message,w,l);
}
void PaintResolution(HWND control,HDC dc) {
  RECT area;GetClientRect(control,&area);FillRect(dc,&area,backgroundBrush);
  const auto pen=CreatePen(PS_SOLID,Scale(1),GetFocus()==control?accentColor:mutedColor);
  const auto brush=CreateSolidBrush(dark?RGB(39,39,39):backgroundColor);
  const auto oldPen=SelectObject(dc,pen),oldBrush=SelectObject(dc,brush);
  RoundRect(dc,0,0,area.right,area.bottom,Scale(6),Scale(6));
  const int selected=int(SendMessageW(control,CB_GETCURSEL,0,0));
  RECT text=area;text.left+=Scale(12);text.right-=Scale(32);
  SelectObject(dc,font);SetTextColor(dc,textColor);SetBkMode(dc,TRANSPARENT);
  if(control==monitorBox && selected>=0 && size_t(selected)<monitors.size())
    DrawTextW(dc,MonitorLabel(selected).c_str(),-1,&text,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
  else if(control==resolutionBox && selected>=0 && selected<presetCount)
    DrawTextW(dc,presetLabels[selected],-1,&text,DT_SINGLELINE|DT_VCENTER);
  const int x=area.right-Scale(18),y=area.bottom/2;
  MoveToEx(dc,x-Scale(4),y-Scale(2),nullptr);LineTo(dc,x,y+Scale(2));LineTo(dc,x+Scale(4),y-Scale(2));
  SelectObject(dc,oldPen);SelectObject(dc,oldBrush);DeleteObject(pen);DeleteObject(brush);
}
LRESULT CALLBACK ResolutionProc(HWND control,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR) {
  if(message==WM_PAINT) {PAINTSTRUCT paint;auto dc=BeginPaint(control,&paint);PaintResolution(control,dc);EndPaint(control,&paint);return 0;}
  if(message==WM_PRINTCLIENT){PaintResolution(control,reinterpret_cast<HDC>(w));return 0;}
  if(message==WM_ERASEBKGND)return 1;
  return DefSubclassProc(control,message,w,l);
}
HWND Control(HWND parent, const wchar_t* type, const wchar_t* text, DWORD style,
             int x, int y, int width, int height, int id = 0) {
  HWND control = CreateWindowW(type, text, WS_CHILD | WS_VISIBLE | style,
      Scale(x), Scale(y), Scale(width), Scale(height), parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
      GetModuleHandleW(nullptr), nullptr);
  SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
  return control;
}
void RefreshPads() {
  int connected = 0;
  for (DWORD slot = 0; slot < 4; ++slot) {
    XINPUT_STATE state{};
    if (XInputGetState(slot, &state) == ERROR_SUCCESS) ++connected;
  }
  std::wstring status = L"Xbox controllers (Windows XInput): " + std::to_wstring(connected) +
      L" connected\r\nKeyboard fallback: automatic, per player.";
  SetWindowTextW(controllerStatus, status.c_str());
}
bool StartGame(HWND parent) {
  const bool packaged = std::filesystem::exists(folder / L"data" / L"engine" / L"vf2.exe");
  const auto engine = packaged ? folder / L"data" / L"engine" : folder;
  const auto exe = engine / L"vf2.exe";
  const auto assets = packaged ? folder / L"data" / L"game" : folder.parent_path() / L"extracted";
  const auto userdata = packaged ? folder / L"data" / L"userdata" : folder / L"userdata";
  if (!std::filesystem::exists(exe) || !std::filesystem::exists(assets / L"default.xex")) {
    MessageBoxW(parent, L"vf2.exe or the extracted game files are missing.",
                L"Virtua Fighter 2 reKOmped", MB_ICONERROR);
    return false;
  }
  int selected = static_cast<int>(SendMessageW(resolutionBox, CB_GETCURSEL, 0, 0));
  if (selected < 0 || selected >= presetCount) selected = 1;
  if(!SaveSettings(parent))return false;
  std::wstring command = L"\"" + exe.wstring() + L"\" --game_data_root \"" +
      assets.wstring() + L"\" --user_data_root \"" + userdata.wstring() +
      L"\" --input_backend xinput --license_mask 1 --user_language 1 --vf2_output_resolution " + presets[selected];
  command += SendMessageW(introBox,BM_GETCHECK,0,0)==BST_CHECKED?
      L" --vf2_no_intro=true":L" --vf2_no_intro=false";
  if(!monitors.empty())command+=L" --vf2_display_device \""+monitors[selectedMonitor].device+L"\"";
  if (packaged) {
    std::filesystem::create_directories(folder / L"data" / L"logs");
    command += L" --log_file \"" + (folder / L"data" / L"logs" / L"game.log").wstring() + L"\"";
  }
  if (SendMessageW(fullscreenBox, BM_GETCHECK, 0, 0) == BST_CHECKED)
    command += L" --fullscreen";
  else {
    command += L" --no-fullscreen";
    RECT work{};
    if(!monitors.empty())work=monitors[selectedMonitor].work;
    else SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int presetW = presetSize[selected][0], presetH = presetSize[selected][1];
    int width = std::min<int>(presetW, std::max(640L, work.right - work.left - 80));
    int height = width * presetH / presetW;
    if (height > work.bottom - work.top - 80) {
      height = std::max(360L, work.bottom - work.top - 80);
      width = height * presetW / presetH;
    }
    command += L" --window_width " + std::to_wstring(width) +
        L" --window_height " + std::to_wstring(height);
  }
  std::vector<wchar_t> buffer(command.begin(), command.end());
  buffer.push_back(0);
  STARTUPINFOW startup{sizeof(startup)};
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(exe.c_str(), buffer.data(), nullptr, nullptr, FALSE, 0,
                      nullptr, engine.c_str(), &startup, &process)) {
    MessageBoxW(parent, (L"Could not start the game. Windows error: " +
        std::to_wstring(GetLastError())).c_str(), L"Virtua Fighter 2 reKOmped", MB_ICONERROR);
    return false;
  }
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  return true;
}
void ControllerNavigation(HWND window) {
  static WORD previous = 0;
  static ULONGLONG repeatAt = 0;
  WORD buttons = 0;
  for (DWORD slot = 0; slot < XUSER_MAX_COUNT; ++slot) {
    XINPUT_STATE state{};
    if (XInputGetState(slot, &state) != ERROR_SUCCESS) continue;
    buttons |= state.Gamepad.wButtons;
    if (state.Gamepad.sThumbLX < -16000) buttons |= XINPUT_GAMEPAD_DPAD_LEFT;
    if (state.Gamepad.sThumbLX > 16000) buttons |= XINPUT_GAMEPAD_DPAD_RIGHT;
    if (state.Gamepad.sThumbLY < -16000) buttons |= XINPUT_GAMEPAD_DPAD_DOWN;
    if (state.Gamepad.sThumbLY > 16000) buttons |= XINPUT_GAMEPAD_DPAD_UP;
  }
  WORD pressed = buttons & ~previous;
  const WORD directions = XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN |
                          XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT;
  const auto now = GetTickCount64();
  if ((pressed & directions) != 0) repeatAt = now + 400;
  else if ((buttons & directions) && now >= repeatAt) {
    pressed |= buttons & directions;
    repeatAt = now + 160;
  }
  previous = buttons;
  if (GetForegroundWindow() != window) return;
  std::vector<HWND> controls;
  if(monitorBox)controls.push_back(monitorBox);
  controls.insert(controls.end(),{resolutionBox,fullscreenBox,introBox,upscaledBox,touristBox,opponentsBox,arenasBox,playButton});
  HWND focused = GetFocus();
  int index = 0;
  const int count=int(controls.size());
  for (int i = 0; i < count; ++i) if (focused == controls[i]) index = i;
  if (pressed & XINPUT_GAMEPAD_DPAD_UP) SetFocus(controls[(index+count-1)%count]);
  if (pressed & XINPUT_GAMEPAD_DPAD_DOWN) SetFocus(controls[(index+1)%count]);
  if (pressed & (XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT)) {
    if (focused == resolutionBox || focused==monitorBox) {
      int selected = int(SendMessageW(focused, CB_GETCURSEL, 0, 0));
      const int maximum=focused==monitorBox?int(monitors.size())-1:presetCount-1;
      selected = std::clamp(selected + ((pressed & XINPUT_GAMEPAD_DPAD_RIGHT) ? 1 : -1),0,maximum);
      SendMessageW(focused, CB_SETCURSEL, selected, 0);
      if(focused==monitorBox)selectedMonitor=size_t(selected);
      SaveSettings(window);
    } else if (focused == fullscreenBox || focused == introBox || focused == upscaledBox || focused == touristBox || focused==opponentsBox || focused==arenasBox) {
      SendMessageW(focused, BM_SETCHECK,
          (pressed & XINPUT_GAMEPAD_DPAD_RIGHT) ? BST_CHECKED : BST_UNCHECKED, 0);
      SaveSettings(window);
    }
  }
  if (pressed & XINPUT_GAMEPAD_A) {
    if (focused == playButton && StartGame(window)) DestroyWindow(window);
    else if (focused == fullscreenBox || focused == introBox || focused == upscaledBox || focused == touristBox || focused==opponentsBox || focused==arenasBox) SendMessageW(focused, BM_CLICK, 0, 0);
    else if(focused==monitorBox)SetFocus(resolutionBox);
    else SetFocus(fullscreenBox);
  }
}
LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM w, LPARAM l) {
  switch (message) {
    case WM_CREATE: {
      monitors=vf2::monitors::List();monitorBox=nullptr;
      if(std::getenv("VF2_LAUNCHER_CAPTURE") && std::getenv("VF2_LAUNCHER_MONITORS_TEST") && !monitors.empty()) {
        auto secondary=monitors.front();secondary.name=L"Secondary display (test fixture)";secondary.device=L"\\\\.\\DISPLAY2";
        secondary.primary=false;monitors.push_back(secondary);
      }
      font = CreateFontW(Scale(-16), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
          CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI Variable");
      smallFont=CreateFontW(Scale(-12),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
          OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
      LoadLogo();
      Control(window,L"STATIC",L"Monitor",0,32,280,496,24);
      if(monitors.size()>1) {
        monitorBox=Control(window,L"COMBOBOX",L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_TABSTOP,
            32,306,496,180,105);
        for(size_t i=0;i<monitors.size();++i)SendMessageW(monitorBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(MonitorLabel(i).c_str()));
        SetWindowSubclass(monitorBox,ResolutionProc,1,0);
      } else Control(window,L"STATIC",monitors.empty()?L"Windows default display":monitors[0].name.c_str(),SS_ENDELLIPSIS,32,310,496,28);
      Control(window, L"STATIC", L"Resolution", 0, 32, 356, 180, 24);
      resolutionBox = Control(window, L"COMBOBOX", L"", CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_TABSTOP,
          32, 382, 496, 180, 101);
      for (const auto preset : presetLabels)
        SendMessageW(resolutionBox, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(preset));
      SetWindowSubclass(resolutionBox,ResolutionProc,1,0);
      fullscreenBox = Control(window, L"BUTTON", L"Fullscreen", BS_AUTOCHECKBOX | WS_TABSTOP,
          32, 427, 220, 28, 102);
      introBox=Control(window,L"BUTTON",L"Skip Startup Logos",BS_AUTOCHECKBOX|WS_TABSTOP,
          270,427,258,28,104);
      upscaledBox=Control(window,L"BUTTON",L"Upscaled 2D Backgrounds",BS_AUTOCHECKBOX|WS_TABSTOP,
          32,460,496,28,106);
      Control(window,L"STATIC",L"ARCADE",0,32,504,496,22);
      touristBox=Control(window,L"BUTTON",L"Tourist - Easy Mode is now REALLY merciful",BS_AUTOCHECKBOX|WS_TABSTOP,
          32,530,496,28,107);
      opponentsBox=Control(window,L"BUTTON",L"Random Opponents - new order every run, Dural last",BS_AUTOCHECKBOX|WS_TABSTOP,
          32,563,496,28,108);
      arenasBox=Control(window,L"BUTTON",L"Random Arenas - no arena twice in a run",BS_AUTOCHECKBOX|WS_TABSTOP,
          32,596,496,28,109);
      for(HWND toggle:{fullscreenBox,introBox,upscaledBox,touristBox,opponentsBox,arenasBox})SetWindowSubclass(toggle,ToggleProc,1,0);
      controllerStatus = Control(window, L"STATIC", L"", 0, 32, 636, 496, 40);
      HWND help=Control(window, L"STATIC", L"F4: fullscreen    F6: Settings && Custom Fight Launcher", 0,
          32, 680, 496, 22);
      SendMessageW(help,WM_SETFONT,reinterpret_cast<WPARAM>(smallFont),TRUE);
      playButton = Control(window, L"BUTTON", L"Play", BS_OWNERDRAW | WS_TABSTOP,
          32, 716, 496, 48, 103);
      LoadSettings();Theme(window);
      RefreshPads();
      SetTimer(window, 1, 1000, nullptr);
      SetTimer(window, 2, 33, nullptr);
      if(std::getenv("VF2_LAUNCHER_CAPTURE"))SetTimer(window,3,800,nullptr);
      SetFocus(resolutionBox);
      return 0;
    }
    case WM_ACTIVATE:
      if(LOWORD(w)!=WA_INACTIVE && resolutionBox)LoadSettings();
      break;
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
      Theme(window);return 0;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
      SetTextColor(reinterpret_cast<HDC>(w),textColor);
      SetBkColor(reinterpret_cast<HDC>(w),backgroundColor);
      return reinterpret_cast<LRESULT>(backgroundBrush);
    case WM_ERASEBKGND: {
      RECT area;GetClientRect(window,&area);FillRect(reinterpret_cast<HDC>(w),&area,backgroundBrush);return 1;
    }
    case WM_PAINT: {
      PAINTSTRUCT paint;HDC dc=BeginPaint(window,&paint);
      PaintContent(dc);
      EndPaint(window,&paint);return 0;
    }
    case WM_PRINTCLIENT:
      PaintContent(reinterpret_cast<HDC>(w));return 0;
    case WM_DRAWITEM: {
      const auto* draw=reinterpret_cast<DRAWITEMSTRUCT*>(l);
      if(draw->CtlID==101 || draw->CtlID==105) {
        const auto brush=CreateSolidBrush((draw->itemState&ODS_SELECTED)?accentColor:backgroundColor);
        FillRect(draw->hDC,&draw->rcItem,brush);DeleteObject(brush);
        SetTextColor(draw->hDC,(draw->itemState&ODS_SELECTED)?RGB(255,255,255):textColor);
        SetBkMode(draw->hDC,TRANSPARENT);SelectObject(draw->hDC,font);
        RECT text=draw->rcItem;text.left+=Scale(10);
        if(draw->CtlID==105 && draw->itemID<monitors.size())DrawTextW(draw->hDC,MonitorLabel(draw->itemID).c_str(),-1,&text,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);
        else if(draw->CtlID==101 && draw->itemID<UINT(presetCount))DrawTextW(draw->hDC,presetLabels[draw->itemID],-1,&text,DT_SINGLELINE|DT_VCENTER);
        return TRUE;
      }
      if(draw->CtlID!=103)break;
      const auto brush=CreateSolidBrush((draw->itemState&ODS_SELECTED)?RGB(0,75,135):accentColor);
      const auto pen=CreatePen(PS_SOLID,1,accentColor);
      const auto oldBrush=SelectObject(draw->hDC,brush),oldPen=SelectObject(draw->hDC,pen);
      RoundRect(draw->hDC,draw->rcItem.left,draw->rcItem.top,draw->rcItem.right,draw->rcItem.bottom,Scale(8),Scale(8));
      SelectObject(draw->hDC,oldBrush);SelectObject(draw->hDC,oldPen);DeleteObject(brush);DeleteObject(pen);
      SetBkMode(draw->hDC,TRANSPARENT);
      const int luminance=GetRValue(accentColor)*299+GetGValue(accentColor)*587+GetBValue(accentColor)*114;
      SetTextColor(draw->hDC,luminance>160000?RGB(0,0,0):RGB(255,255,255));SelectObject(draw->hDC,font);
      RECT text=draw->rcItem;DrawTextW(draw->hDC,L"Play",-1,&text,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
      if(draw->itemState&ODS_FOCUS){InflateRect(&text,-Scale(5),-Scale(5));DrawFocusRect(draw->hDC,&text);}
      return TRUE;
    }
    case WM_MEASUREITEM: {
      auto* measure=reinterpret_cast<MEASUREITEMSTRUCT*>(l);
      if(measure->CtlID==101 || measure->CtlID==105){measure->itemHeight=Scale(30);return TRUE;}
      break;
    }
    case WM_DPICHANGED: {
      // Recreate controls at the target monitor DPI, preserving on-disk choices.
      dpi=HIWORD(w);SaveSettings(window);
      const auto rect=reinterpret_cast<RECT*>(l);
      SetWindowPos(window,nullptr,rect->left,rect->top,rect->right-rect->left,rect->bottom-rect->top,SWP_NOZORDER|SWP_NOACTIVATE);
      // Fixed layout keeps text and controls aligned at any monitor scaling.
      HWND child=GetWindow(window,GW_CHILD);
      while(child){HWND next=GetWindow(child,GW_HWNDNEXT);DestroyWindow(child);child=next;}
      logo.reset();if(logoStream){logoStream->Release();logoStream=nullptr;}
      DeleteObject(font);DeleteObject(smallFont);
      return WindowProc(window,WM_CREATE,0,0);
    }
    case WM_TIMER:
      if (w == 1) RefreshPads();
      else if (w == 2) ControllerNavigation(window);
      else if(w==3){KillTimer(window,3);CapturePreview(window);DestroyWindow(window);}
      return 0;
    case WM_COMMAND:
      // The launcher's job ends once the game process exists.
      if (LOWORD(w) == 103 && HIWORD(w) == BN_CLICKED && StartGame(window)) DestroyWindow(window);
      else if((LOWORD(w)==101 && HIWORD(w)==CBN_SELCHANGE) ||
              ((LOWORD(w)==102 || LOWORD(w)==104 || (LOWORD(w)>=106 && LOWORD(w)<=109)) && HIWORD(w)==BN_CLICKED))SaveSettings(window);
      else if(LOWORD(w)==105 && HIWORD(w)==CBN_SELCHANGE) {
        const int selected=int(SendMessageW(monitorBox,CB_GETCURSEL,0,0));
        if(selected>=0 && size_t(selected)<monitors.size()){selectedMonitor=selected;SaveSettings(window);}
      }
      return 0;
    case WM_DESTROY:
      KillTimer(window, 1);
      KillTimer(window, 2);
      DeleteObject(font);
      DeleteObject(smallFont);DeleteObject(backgroundBrush);
      PostQuitMessage(0);
      return 0;
  }
  return DefWindowProcW(window, message, w, l);
}
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  dpi = GetDpiForSystem();
  wchar_t path[32768];
  GetModuleFileNameW(nullptr, path, 32768);
  folder = std::filesystem::path(path).parent_path();
  Gdiplus::GdiplusStartupInput gdiplusInput;
  if(Gdiplus::GdiplusStartup(&gdiplusToken,&gdiplusInput,nullptr)!=Gdiplus::Ok)return 1;
  INITCOMMONCONTROLSEX common{sizeof(common),ICC_STANDARD_CLASSES};InitCommonControlsEx(&common);
  WNDCLASSW wc{};
  wc.lpfnWndProc = WindowProc;
  wc.hInstance = instance;
  wc.lpszClassName = L"VF2Settings";
  wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
  wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));
  RegisterClassW(&wc);
  RECT bounds{0,0,Scale(560),Scale(866)};
  const DWORD style=WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
  AdjustWindowRectExForDpi(&bounds,style,FALSE,0,dpi);
  HWND window = CreateWindowW(wc.lpszClassName, L"Virtua Fighter 2 reKOmped",
      style,CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,
      nullptr,nullptr,instance,nullptr);
  if (!window) return 1;
  ShowWindow(window, show);
  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    if (!IsDialogMessageW(window, &message)) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
  }
  logo.reset();if(logoStream)logoStream->Release();Gdiplus::GdiplusShutdown(gdiplusToken);
  return 0;
}
