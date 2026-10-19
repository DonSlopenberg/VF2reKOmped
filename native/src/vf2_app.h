// vf2 - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/rex_app.h>
#include <rex/cvar.h>
#include <rex/memory.h>
#include <rex/logging.h>
#include <rex/ui/window.h>
#include <cstdlib>
#include <fstream>
#include <thread>
#include <rex/ui/presenter.h>
#include <rex/ui/keybinds.h>
#include <rex/input/input_system.h>
#include <rex/system/gpu_plugin.h>
#include "vf2_ui.h"
#include "vf2_keyboard.h"
#include "vf2_settings.h"
#include "vf2_display.h"
#include "vf2_monitors.h"
#include "launcher_config.h"
#include "vf2_menu.h"

class Vf2App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Vf2App>(new Vf2App(ctx, "vf2",
        PPCImageConfig));
  }

  // Xbox controllers use Windows XInput by default. Preserve explicit user
  // selections from the command line or config. SDK SlotAssignment supplies
  // independent guest users for separate physical controllers.
  void OnConfigurePaths(rex::PathConfig& paths) override {
    if(const char* test=std::getenv("VF2_SETTINGS_CONFIG_TEST"))paths.config_path=test;
    // Started directly (without the launcher): data/engine/vf2.exe uses the
    // installation's data/game, data/userdata and data/logs. Settings come
    // from data/engine/vf2.toml as with the launcher.
    std::wstring command_line=GetCommandLineW();
    wchar_t module[MAX_PATH]{};GetModuleFileNameW(nullptr,module,MAX_PATH);
    const auto data=std::filesystem::path(module).parent_path().parent_path();
    std::error_code error;
    if(command_line.find(L"--game_data_root")==std::wstring::npos &&
       !std::filesystem::exists(paths.game_data_root/"default.xex",error) &&
       std::filesystem::exists(data/"game"/"default.xex",error)) {
      paths.game_data_root=data/"game";
      if(command_line.find(L"--user_data_root")==std::wstring::npos)paths.user_data_root=data/"userdata";
      if(rex::cvar::GetFlagByName("log_file").empty()) {
        std::filesystem::create_directories(data/"logs",error);
        rex::cvar::SetFlagByName("log_file",(data/"logs"/"game.log").string());
      }
      started_directly_=true;
    }
    // The launcher's defaults: full XBLA license and English, unless set.
    for(const auto& [flag,value]:{std::pair{"license_mask","1"},std::pair{"user_language","1"}})
      if(rex::cvar::GetFlagSource(flag)==rex::cvar::Source::kDefault)rex::cvar::SetFlagByName(flag,value);
    settings_config_path_=paths.config_path;
  }
  void OnPreSetup(rex::RuntimeConfig& config) override {
    const auto requested_refresh=rex::cvar::GetFlagByName("video_mode_refresh_rate");
    // Guest video mode is distinct from monitor refresh. Preserve original
    // animation/gameplay tick speed while the host can present at 120 Hz+.
    constexpr double outer_rate=60;
    rex::cvar::SetFlagByName("video_mode_refresh_rate",std::to_string(outer_rate));
    for(auto& flag:rex::cvar::GetRegistry())if(flag.name=="video_mode_refresh_rate") {
      flag.constraints.min=outer_rate;flag.constraints.max=outer_rate;
      flag.lifecycle=rex::cvar::Lifecycle::kInitOnly;
      flag.description="VF2 guest simulation refresh: fixed 60 Hz; host display refresh is independent";
    }
    REXLOG_INFO("VF2 timing: requested guest refresh={}, fixed guest=60 Hz; host presentation is independent",requested_refresh);
    // The SDK applies fullscreen before Open/SetPresenter. Defer it until
    // the live surface and its DPI are attached, just like a later F4 toggle.
    startup_fullscreen_ = rex::cvar::GetFlagByName("fullscreen") == "true";
    if (startup_fullscreen_) rex::cvar::SetFlagByName("fullscreen", "false");
    if (config.gpu_plugin.empty()) {
      config.gpu_plugin = "xenos";
    }
    // Loading the module registers its resolution CVARs. Set them before
    // presentation/guest GPU setup, when render-target caches are created.
    if (!config.graphics) config.graphics = rex::system::LoadGpuPlugin(config.gpu_plugin);
    // The GPU plugin registers swap_post_effect (antialiasing) only now, after
    // vf2.toml was read, so apply the saved value from the file here.
    if(rex::cvar::GetFlagSource("swap_post_effect")==rex::cvar::Source::kDefault) {
      const auto saved=vf2::launcher::Read(settings_config_path_);
      if(auto it=saved.find("swap_post_effect");it!=saved.end() &&
         (it->second=="none" || it->second=="fxaa" || it->second=="fxaa_extreme"))
        rex::cvar::SetFlagByName("swap_post_effect",it->second);
    }
    if (rex::cvar::GetFlagSource("input_backend") == rex::cvar::Source::kDefault) {
      rex::cvar::SetFlagByName("input_backend", "xinput");
    }
    vf2::display::Configure();
    const int selected_width=vf2::display::output.width;
    const int selected_height=vf2::display::output.height;
    const int draw_scale=vf2::display::draw_scale;
    REXLOG_INFO("VF2 render selection: {}x{}; guest {}x{}; GPU draw scale {}x{}",
                selected_width, selected_height, vf2::display::guest.width,
                vf2::display::guest.height, draw_scale, draw_scale);
    REXLOG_INFO("VF2 GPU settings: resolution_scale={}, draw_scale={}x{}",
                rex::cvar::GetFlagByName("resolution_scale"),
                rex::cvar::GetFlagByName("draw_resolution_scale_x"),
                rex::cvar::GetFlagByName("draw_resolution_scale_y"));
    REXLOG_INFO("VF2 launch: input={}, license_mask={}, video_mode={}x{}, window={}x{}",
        rex::cvar::GetFlagByName("input_backend"),
        rex::cvar::GetFlagByName("license_mask"),
        rex::cvar::GetFlagByName("video_mode_width"),
        rex::cvar::GetFlagByName("video_mode_height"),
        rex::cvar::GetFlagByName("window_width"),
        rex::cvar::GetFlagByName("window_height"));
  }

  void OnPostSetup() override {
    if(started_directly_)
      REXLOG_INFO("VF2 started without the launcher: game {}, user data {}",game_data_root().string(),user_data_root().string());
    // The launcher passes window_width/height in physical pixels, but the SDK
    // treats them as DPI-independent units: at 200 % scaling a 3555x2000
    // request became a 7110x4000 client, larger than the monitor. Apply the
    // requested client size in physical pixels, fitted to the work area.
    if(window() && !startup_fullscreen_) {
      const auto hwnd=static_cast<HWND>(window()->GetNativeWindowHandle());
      MONITORINFO info{sizeof(info)};
      int requested_w=std::atoi(rex::cvar::GetFlagByName("window_width").c_str());
      int requested_h=std::atoi(rex::cvar::GetFlagByName("window_height").c_str());
      // Without the launcher's size, open at the output resolution (fitted below).
      if(rex::cvar::GetFlagSource("window_width")==rex::cvar::Source::kDefault) {
        requested_w=vf2::display::output.width;requested_h=vf2::display::output.height;
      }
      if(hwnd && requested_w>0 && requested_h>0 &&
         GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&info)) {
        RECT borders{};
        AdjustWindowRectExForDpi(&borders,DWORD(GetWindowLongPtrW(hwnd,GWL_STYLE)),FALSE,
                                 DWORD(GetWindowLongPtrW(hwnd,GWL_EXSTYLE)),GetDpiForWindow(hwnd));
        const int bw=borders.right-borders.left,bh=borders.bottom-borders.top;
        const int work_w=info.rcWork.right-info.rcWork.left,work_h=info.rcWork.bottom-info.rcWork.top;
        int w=requested_w,h=requested_h;
        if(w+bw>work_w){h=h*(work_w-bw)/w;w=work_w-bw;}
        if(h+bh>work_h){w=w*(work_h-bh)/h;h=work_h-bh;}
        SetWindowPos(hwnd,nullptr,info.rcWork.left+(work_w-w-bw)/2,info.rcWork.top+(work_h-h-bh)/2,
                     w+bw,h+bh,SWP_NOZORDER|SWP_NOACTIVATE);
        REXLOG_INFO("VF2 window: client {}x{} physical pixels (requested {}x{}, DPI {})",w,h,requested_w,requested_h,GetDpiForWindow(hwnd));
      }
    }
    const auto display_device=rex::cvar::GetFlagByName("vf2_display_device");
    if(window() && !display_device.empty()) {
      const auto displays=vf2::monitors::List();
      if(!displays.empty()) {
        const auto& display=displays[vf2::monitors::Selected(displays,display_device)];
        const auto hwnd=static_cast<HWND>(window()->GetNativeWindowHandle());
        RECT rect;GetWindowRect(hwnd,&rect);
        const int width=rect.right-rect.left,height=rect.bottom-rect.top;
        SetWindowPos(hwnd,nullptr,display.work.left+((display.work.right-display.work.left)-width)/2,
            display.work.top+((display.work.bottom-display.work.top)-height)/2,0,0,SWP_NOZORDER|SWP_NOSIZE|SWP_NOACTIVATE);
        REXLOG_INFO("VF2 selected Windows display: {} ({})",vf2::monitors::Narrow(display.name),vf2::monitors::Narrow(display.device));
      }
    }
    if(std::getenv("VF2_SETTINGS_TEST")) {
      settings_ui_=std::make_unique<vf2::SettingsDialog>(imgui_drawer(),settings_config_path_,user_data_root()/"custom-fight.cfg");
      vf2::SaveSettings(user_data_root()/"settings-test.toml");
    }
    if (startup_fullscreen_) {
      rex::cvar::SetFlagByName("fullscreen", "true");
      REXLOG_INFO("VF2 startup fullscreen: attached surface {}x{}, DPI {}",
          window()->GetActualPhysicalWidth(), window()->GetActualPhysicalHeight(), window()->GetDpi());
    }
    if (runtime()->input_system())
      vf2::InstallKeyboardFallback(*static_cast<rex::input::InputSystem*>(runtime()->input_system()));
    if (window()) window()->SetTitle("Virtua Fighter 2 reKOmped");
    // Opt-in UI check: open the achievements overlay right away.
    if (std::getenv("VF2_ACHIEVEMENTS_SHOW") && runtime() && runtime()->kernel_state())
      vf2::OpenAchievements(imgui_drawer(), *runtime()->kernel_state());
    if(window()) {
      const auto hwnd=static_cast<HWND>(window()->GetNativeWindowHandle());
      MONITORINFOEXW info{};info.cbSize=sizeof(info);
      DEVMODEW mode{};mode.dmSize=sizeof(mode);
      if(GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),reinterpret_cast<MONITORINFO*>(&info)) &&
          EnumDisplaySettingsW(info.szDevice,ENUM_CURRENT_SETTINGS,&mode))
        REXLOG_INFO("VF2 host monitor: {}x{} at {} Hz (guest simulation stays 60 Hz)",
          mode.dmPelsWidth,mode.dmPelsHeight,mode.dmDisplayFrequency);
    }
    if (runtime()->input_system() && imgui_drawer()) {
      static_cast<rex::input::InputSystem*>(runtime()->input_system())->SetActiveCallback([this] {
        const auto& io = imgui_drawer()->GetIO();
        return !vf2::achievement_ui_active && !io.WantCaptureMouse && !io.WantCaptureKeyboard &&
               !vf2::GameInputHeldBack();
      });
    }
  }

  void OnConfigureFonts(ImFontAtlas* atlas) override {
    ImFontConfig config;
    config.SizePixels = 18.0f;
    // The runtime draws in logical coordinates. Bake denser glyphs so a
    // high-DPI display doesn't enlarge a low-resolution 18-pixel atlas.
    config.RasterizerDensity = 4.0f;
    config.OversampleH = 2;
    config.OversampleV = 2;
    // The renderer bakes a static atlas: only listed ranges get glyphs.
    // Latin + General Punctuation + arrows (achievement "Enter ↓↓↑↑←←→→").
    static const ImWchar ranges[] = {0x0020, 0x00FF, 0x2010, 0x2027, 0x2190, 0x2193, 0};
    config.GlyphRanges = ranges;
    auto* font = atlas->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f, &config);
    if (!font) font = atlas->AddFontDefault(&config);
    ImGui::GetIO().FontDefault = font;
  }

  void OnConfigureStyle(ImGuiStyle& style, rex::ui::Style& overlays) override {
    ImGui::StyleColorsDark(&style);
    style.WindowPadding = ImVec2(16, 14);
    style.FramePadding = ImVec2(10, 6);
    style.ItemSpacing = ImVec2(10, 8);
    style.WindowRounding = 8;
    style.FrameRounding = 4;
    style.ScrollbarSize = 18;
    style.FontSizeBase = 18;
    overlays.achievements.unlocked_title = ImVec4(0.55f, 0.8f, 1, 1);
    overlays.achievements.row_unlocked_bg = ImVec4(0.15f, 0.25f, 0.38f, 0.55f);
    overlays.achievements.progress_bar = ImVec4(0.25f, 0.6f, 0.95f, 1);
    overlays.toast.title = ImVec4(0.55f, 0.8f, 1, 1);
  }

  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    // Presentation is created before Runtime. Input callbacks are installed
    // in OnPostSetup, once both runtime and drawer are available.
    controller_ui_ = std::make_unique<vf2::ControllerUi>(drawer, window());
    rex::ui::UnregisterBind("bind_settings");
    rex::ui::RegisterBind("bind_settings", "F6", "Toggle settings", [this] {
      if(settings_ui_ && settings_ui_->IsOpen())settings_ui_.reset();
      else {
        settings_ui_.reset();
        settings_ui_=std::make_unique<vf2::SettingsDialog>(imgui_drawer(),settings_config_path_,user_data_root()/"custom-fight.cfg");
      }
    });
    rex::cvar::SetFlagByName("bind_settings", "F6");
    rex::ui::RegisterBind("bind_windowed", "F4", "Windowed mode at 75% of the monitor", [this] {
      if (window()->IsFullscreen()) vf2::Windowed75(window());
      else {
        window()->SetFullscreen(true);
        REXLOG_INFO("F4: fullscreen mode");
      }
    });
    rex::cvar::SetFlagByName("bind_windowed", "F4");
    rex::ui::UnregisterBind("bind_achievements");
    rex::ui::RegisterBind("bind_achievements", "F7", "Open local achievements", [this] {
      if (runtime() && runtime()->kernel_state()) vf2::OpenAchievements(imgui_drawer(), *runtime()->kernel_state());
    });
    vf2::ui_frame_hook = [this](ImGuiIO&) {
      // Settings > Advanced Settings (main and pause menu) opens F6 on its first tab.
      if (!vf2::menu::TakeOpenSettingsRequest()) return;
      if (!settings_ui_ || !settings_ui_->IsOpen())
        settings_ui_ = std::make_unique<vf2::SettingsDialog>(imgui_drawer(), settings_config_path_,
                                                             user_data_root() / "custom-fight.cfg");
      settings_ui_->SelectFirstTab();
    };
  }

  void OnShutdown() override {
    capture_thread_.request_stop();
    if (capture_thread_.joinable()) capture_thread_.join();
    rex::ui::UnregisterBind("bind_windowed");
    if (vf2::achievement_dialog) delete vf2::achievement_dialog;
    controller_ui_.reset();
    settings_ui_.reset();
  }

  // Opt-in GPU readback for renderer regression tests, including when the
  // Windows desktop cannot be captured. Never reads other applications.
  void OnPostLaunchModule(rex::system::XThread*) override {
    const char* directory=std::getenv("VF2_GPU_CAPTURE");
    if (!directory) return;
    const std::filesystem::path output(directory);
    auto* presenter=runtime()->graphics_system()->presenter();
    if (!presenter) return;
    const auto setting=[](const char* key,int fallback,int low,int high) {
      const char* text=std::getenv(key);
      return text ? std::clamp(std::atoi(text),low,high) : fallback;
    };
    const int delay=setting("VF2_GPU_CAPTURE_DELAY_MS",0,0,120000);
    const int interval=setting("VF2_GPU_CAPTURE_INTERVAL_MS",3000,
        std::getenv("VF2_CAPTURE_FAST")?1:100,10000);
    const int count=setting("VF2_GPU_CAPTURE_COUNT",120,1,2000);
    capture_thread_=std::jthread([presenter,output,delay,interval,count](std::stop_token stop) {
      std::filesystem::create_directories(output);
      for(int wait=0;wait<delay/100 && !stop.stop_requested();++wait)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      for (int i=0; !stop.stop_requested() && i<count; ++i) {
        for(int wait=0;wait<interval && !stop.stop_requested();) {
          const int step=std::min(100,interval-wait);
          std::this_thread::sleep_for(std::chrono::milliseconds(step));wait+=step;
        }
        if(stop.stop_requested()) break;
        rex::ui::RawImage image;
        if(!presenter->CaptureGuestOutput(image)) continue;
        const auto stem=output/std::to_string(i);
        std::ofstream raw(stem.string()+".rgba",std::ios::binary);
        raw.write(reinterpret_cast<const char*>(image.data.data()),image.data.size());
        std::ofstream meta(stem.string()+".txt");
        meta<<image.width<<' '<<image.height<<' '<<image.stride;
      }
    });
  }

  // Opt-in local diagnostic snapshot before guest execution; no code patches.
  void OnPostLoadXexImage() override {
    if (const char* path = std::getenv("VF2_DUMP_IMAGE")) {
      std::ofstream dump(path, std::ios::binary);
      dump.write(reinterpret_cast<const char*>(
          runtime()->memory()->TranslateVirtual(static_cast<uint32_t>(REX_IMAGE_BASE))), REX_IMAGE_SIZE);
    }
  }

 private:
  bool startup_fullscreen_ = false;
  std::filesystem::path settings_config_path_;
  bool started_directly_=false;
  std::unique_ptr<vf2::SettingsDialog> settings_ui_;
  std::jthread capture_thread_;
  std::unique_ptr<vf2::ControllerUi> controller_ui_;
};
