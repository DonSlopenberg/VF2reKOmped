#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <rex/ui/overlay/settings_overlay.h>
#include <rex/cvar.h>
#include <imgui.h>
#include <rex/logging.h>
#include <fstream>
#include <cstdlib>
#include "vf2_fight.h"
#include "vf2_fight_config.h"
#include "vf2_display.h"
#include "vf2_freelook.h"
#include "vf2_arcade.h"

namespace vf2 {
inline std::string TomlString(std::string_view text) {
  std::string out="\"";
  constexpr char hex[]="0123456789ABCDEF";
  for(unsigned char ch:text) {
    if(ch=='\\' || ch=='\"'){out+='\\';out+=char(ch);}
    else if(ch<32 || ch==127){out+="\\u00";out+=hex[ch>>4];out+=hex[ch&15];}
    else out+=char(ch);
  }
  return out+'\"';
}
inline bool SaveSettings(const std::filesystem::path& path) {
  // SDK 0.10 writes unescaped Windows paths into TOML. Keep its normal flag
  // registry/config format, but serialize strings correctly and replace atomically.
  auto temporary=path;temporary+=".tmp";
  std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
  if(!file)return false;
  file<<"# Virtua Fighter 2 settings\n";
  for(const auto& flag:rex::cvar::GetRegistry()) {
    if(flag.type==rex::cvar::FlagType::Command || !flag.getter)continue;
    if(display::InternalFlag(flag.name))continue;
    const auto value=flag.getter();
    if(value==flag.default_value)continue;
    file<<flag.name<<" = "<<(flag.type==rex::cvar::FlagType::String?TomlString(value):value)<<'\n';
  }
  file.close();
  if(!file){std::filesystem::remove(temporary);return false;}
  if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
    REXLOG_ERROR("VF2 settings replacement failed: {}",GetLastError());return false;
  }
  REXLOG_INFO("VF2 settings saved with escaped TOML strings: {}",path.string());
  return true;
}
}

namespace vf2 {
// Extend the existing SDK window, retaining its categories, key bindings and
// Save to config action. The generic SDK editor uses InputDouble, not a slider.
class SettingsDialog final : public rex::ui::SettingsDialog {
 public:
  SettingsDialog(rex::ui::ImGuiDrawer* drawer,std::filesystem::path path,
                 std::filesystem::path custom_path={})
      : rex::ui::SettingsDialog(drawer,path),config_path_(std::move(path)),
        custom_path_(custom_path.empty()?config_path_.parent_path()/"custom-fight.cfg":std::move(custom_path)) {
    fight::SetSettingsOpen(true);
    fight_config_=fight::LoadConfig(custom_path_);
    REXLOG_INFO("VF2 Custom Fight profile loaded: chars={},{} arena={} difficulty={} rounds={} time={}",
        fight_config_.characters[0],fight_config_.characters[1],fight_config_.arena,
        fight_config_.difficulty,fight_config_.rounds_to_win,fight_config_.round_time);
    launcher_open_=std::getenv("VF2_FIGHT_LAUNCHER_UI_TEST")!=nullptr;
    std::error_code error;last_write_=std::filesystem::last_write_time(config_path_,error);
  }
  ~SettingsDialog() override { fight::SetSettingsOpen(false); }
  bool IsOpen() const { return open_; }
 protected:
  void OnDraw(ImGuiIO& io) override {
    if(!open_)return;
    // Small game windows: shrink the whole interface so it fits (never enlarged).
    const float scale=UiScale(io);
    ImGui::SetNextWindowSize(ImVec2(std::min(920.0f*scale,io.DisplaySize.x*0.94f),
                                   std::min(740.0f*scale,io.DisplaySize.y*0.94f)),ImGuiCond_Appearing);
    // Opens at the top centre; it can be dragged but is kept on screen.
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x*0.5f,io.DisplaySize.y*0.03f),
                           ImGuiCond_Appearing,ImVec2(0.5f,0.0f));
    // Text of every section, the SDK's Advanced tab included, follows the scale.
    ImGui::PushFont(nullptr,ImGui::GetStyle().FontSizeBase*scale);
    // Multiple Begin calls append to one ImGui window in the same frame.
    // This is the SDK's existing window identity, rather than a second dialog.
    if(ImGui::Begin("Settings##rex",&open_,ImGuiWindowFlags_NoCollapse)) {
      KeepOnScreen(io);
      if(ImGui::Button("Custom Fight Launcher..."))launcher_open_=true;
      ImGui::SameLine();
      ImGui::TextDisabled("F6: return here at any time");
      if(ImGui::BeginTabBar("Settings sections")) {
        if(ImGui::BeginTabItem("VF2 reKOmped")) {
          if(ImGui::BeginTabBar("VF2 sections")) {
            if(ImGui::BeginTabItem("Display")) {
              DrawDisplay();ImGui::EndTabItem();
            }
            if(ImGui::BeginTabItem("Arcade & Cheats")) {
              DrawArcade();ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
          }
          ImGui::EndTabItem();
        }
        draw_advanced_=false;
        if(ImGui::BeginTabItem("Advanced")) {
          draw_advanced_=true;
          ImGui::TextDisabled("Technical runtime settings. Use VF2 > Display for resolution.");
          ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
      }
    }
    ImGui::End();
    if(!open_){ImGui::PopFont();launcher_open_=false;fight::SetSettingsOpen(false);return;}
    DrawLauncher(io);
    if(open_ && draw_advanced_)rex::ui::SettingsDialog::OnDraw(io);
    ImGui::PopFont();
    if(!open_)return;
    std::error_code error;
    const auto written=std::filesystem::last_write_time(config_path_,error);
    if(!error && written!=last_write_) {
      SaveSettings(config_path_);
      last_write_=std::filesystem::last_write_time(config_path_,error);
    }
  }
 private:
  // The windows are laid out for about 960x780 interface units; below that
  // everything (text, widgets, fixed widths) shrinks proportionally.
  // Dragging is allowed, but the title bar always stays reachable.
  static void KeepOnScreen(const ImGuiIO& io) {
    const ImVec2 pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();
    const float keep=80.0f;
    const ImVec2 fixed(std::clamp(pos.x,keep-size.x,io.DisplaySize.x-keep),std::clamp(pos.y,0.0f,io.DisplaySize.y-ImGui::GetFrameHeight()*1.5f));
    if(fixed.x!=pos.x || fixed.y!=pos.y)ImGui::SetWindowPos(fixed);
  }
  static float UiScale(const ImGuiIO& io) {
    return std::clamp(std::min(io.DisplaySize.x/960.0f,io.DisplaySize.y/780.0f),0.45f,1.0f);
  }
  void DrawDisplay() {
      const char* resolutions[]={"1280x720 (HD)","1920x1080 (Full HD)","2560x1440 (QHD)","3840x2160 (UHD)",
                                 "2560x1080 (Ultrawide 21:9)","3440x1440 (Ultrawide QHD 21:9)"};
      const char* values[]={"1280x720","1920x1080","2560x1440","3840x2160","2560x1080","3440x1440"};
      constexpr int count=int(std::size(values));
      const auto requested=rex::cvar::GetFlagByName("vf2_output_resolution");
      int selected=-1;
      for(int i=0;i<count;++i)if(requested==values[i])selected=i;
      ImGui::SetNextItemWidth(300*UiScale(ImGui::GetIO()));
      if(ImGui::Combo("Resolution",&selected,resolutions,count))
        rex::cvar::SetFlagByName("vf2_output_resolution",values[selected]);
      ImGui::TextDisabled("Active rendering: %d x %d",display::output.width,display::output.height);
      if(requested!=std::to_string(display::output.width)+"x"+std::to_string(display::output.height))
        ImGui::TextUnformatted("Resolution change requires restarting the game.");
      {
        // SDK presentation post effect (applied to the final image).
        const char* labels[]={"Off","FXAA","FXAA Extreme"};  // same choices as the launcher
        const char* values[]={"none","fxaa","fxaa_extreme"};
        const auto current=rex::cvar::GetFlagByName("swap_post_effect");
        int aa=0;for(int i=0;i<3;++i)if(current==values[i])aa=i;
        ImGui::SetNextItemWidth(300*UiScale(ImGui::GetIO()));
        if(ImGui::Combo("Antialiasing",&aa,labels,3))rex::cvar::SetFlagByName("swap_post_effect",values[aa]);
      }
      ImGui::Separator();
      ImGui::TextUnformatted("Background");
      Toggle("Smooth Scroll (subpixel background)","vf2_background_smooth_scroll");
      Toggle("Smooth interlaced water wave","vf2_background_wave_smooth");
      Toggle("Smooth background filtering (off = pixelated)","vf2_background_linear_composite");
      {
        int blur=std::stoi(rex::cvar::GetFlagByName("vf2_background_blur"));
        const char* names[]={"Off","Soft (2x)","Medium (4x)","Strong (8x)"};
        ImGui::SetNextItemWidth(300*UiScale(ImGui::GetIO()));
        if(ImGui::SliderInt("Background blur",&blur,0,3,names[std::clamp(blur,0,3)],ImGuiSliderFlags_AlwaysClamp))
          rex::cvar::SetFlagByName("vf2_background_blur",std::to_string(blur));
      }
      Toggle("Smooth static bitmap screens (high-score entry)","vf2_background_static_smooth");
      Toggle("Upscaled arena backgrounds (textures/replacements/arenas)","vf2_background_replacements");
      ImGui::Separator();
      float fov=std::stof(rex::cvar::GetFlagByName("vf2_background_fov"));
      ImGui::TextUnformatted("Background FOV & Offset");
      ImGui::SetNextItemWidth(std::max(180.0f*UiScale(ImGui::GetIO()),ImGui::GetContentRegionAvail().x-190.0f*UiScale(ImGui::GetIO())));
      if(ImGui::SliderFloat("Background FOV",&fov,40.0f,140.0f,"%.1f degrees",ImGuiSliderFlags_AlwaysClamp))
        rex::cvar::SetFlagByName("vf2_background_fov",std::to_string(fov));
      if(!reported_) {
        reported_=true;
        REXLOG_INFO("VF2 F6 background FOV slider drawn: value={} range=40..140 window={}x{}",
                    fov,ImGui::GetWindowSize().x,ImGui::GetWindowSize().y);
      }
      ImGui::SameLine();
      if(ImGui::SmallButton("Reset"))rex::cvar::SetFlagByName("vf2_background_fov","120");
      float horizon=std::stof(rex::cvar::GetFlagByName("vf2_background_horizon"));
      ImGui::SetNextItemWidth(std::max(180.0f*UiScale(ImGui::GetIO()),ImGui::GetContentRegionAvail().x-420.0f*UiScale(ImGui::GetIO())));
      if(ImGui::SliderFloat("Horizon Height / Vertical Offset",&horizon,-192.0f,192.0f,"%.1f px",ImGuiSliderFlags_AlwaysClamp))
        rex::cvar::SetFlagByName("vf2_background_horizon",std::to_string(horizon));
      ImGui::SameLine();
      if(ImGui::SmallButton("Reset##horizon"))rex::cvar::SetFlagByName("vf2_background_horizon","0");
      if(!horizon_reported_) {
        horizon_reported_=true;
        REXLOG_INFO("VF2 F6 horizon slider drawn: value={} range=-192..192; independent FOV={}",horizon,fov);
      }
      ImGui::TextDisabled("Positive offset raises the horizon.");
      ImGui::TextDisabled("Distant sky/scenery only. Arena and gameplay camera stay unchanged.");
      if(ImGui::Button("Save settings"))SaveSettings(config_path_);
  }
  void DrawArcade() {
    ImGui::TextUnformatted("Arcade options (apply from the next Arcade run)");
    Toggle("Random Opponents","vf2_arcade_random_opponents");
    ImGui::TextDisabled("Opponents in random order. Dural is always last. Not available in Ranking Mode.");
    Toggle("Random Arenas","vf2_arcade_random_arenas");
    ImGui::TextDisabled("Each stage gets a random arena instead of the opponent's own; no repeats. Dural keeps her arena. Not available in Ranking Mode.");
    Toggle("Tourist difficulty","vf2_arcade_tourist");
    ImGui::TextDisabled("Below Easy. CPU strength stays constant for the whole run.");
    Toggle("Hidden Dural: hold Start for 3 s at character select","vf2_dural_hold_start");
    if(ImGui::Button("Save settings##arcade"))SaveSettings(config_path_);
    ImGui::Separator();
    ImGui::TextUnformatted("Cheats");
    auto c=arcade::GetCheats();
    bool changed=ImGui::Checkbox("Infinite Health P1",&c.infinite_health[0]);
    changed|=ImGui::Checkbox("Infinite Health P2",&c.infinite_health[1]);
    if(changed)arcade::SetCheats(c);
    ImGui::TextDisabled(arcade::CheatsUsedThisSession()
        ? "Cheats were used: achievements are disabled until the game restarts."
        : "Enabling any cheat disables achievements until the game restarts.");
  }
  static void Toggle(const char* label,const char* flag) {
    bool value=rex::cvar::GetFlagByName(flag)=="true";
    if(ImGui::Checkbox(label,&value))rex::cvar::SetFlagByName(flag,value?"true":"false");
  }
  void DrawLauncher(ImGuiIO& io) {
    if(!launcher_open_)return;
    const float scale=UiScale(io);
    ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(360.0f,360.0f*scale),0),ImVec2(io.DisplaySize.x*0.95f,io.DisplaySize.y*0.95f));
    ImGui::SetNextWindowSize(ImVec2(std::min(650.0f*scale,io.DisplaySize.x*0.94f),0),ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x*0.5f,io.DisplaySize.y*0.03f),ImGuiCond_Appearing,ImVec2(0.5f,0.0f));
    if(ImGui::Begin("Custom Fight Launcher",&launcher_open_,ImGuiWindowFlags_AlwaysAutoResize)) {
      KeepOnScreen(io);
      bool changed=false;
      if(!launcher_reported_) {
        launcher_reported_=true;
        REXLOG_INFO("VF2 F6 Custom Fight Launcher drawn: native difficulty, rounds and time; independent Human/CPU");
      }
      // Native fixed-width name tables at ROM 0x783A8 and 0x0201EC6C;
      // Dural is ID 9, Lion is ID 10 (not alphabetical/menu ordering).
      constexpr const char* characters[]={"Akira","Jacky","Sarah","Kage","Lau","Jeffry","Pai","Wolf","Shun","Dural","Lion"};
      // Native boss encounter table 0x12508 supplies stages 0..10. Additional
      // records 11/12 are selected by change_scene for native versus variants.
      constexpr const char* arenas[]={"Lau's Arena","Sarah's Arena","Shun's Arena","Pai's Arena","Lion's Arena","Jeffry's Arena","Kage's Arena","Jacky's Arena","Wolf's Arena","Akira's Arena","Dural's Boss Arena","Dural's Versus Arena","Sarah / Jacky Arena","Arena 13 (native variant)","Arena 14 (native variant)","Arena 15 (native variant)"};
      if(ImGui::BeginTable("Custom Players",2,ImGuiTableFlags_SizingStretchSame)) {
      for(int player=0;player<2;++player) {
        ImGui::TableNextColumn();
        ImGui::PushID(player);
        ImGui::Text("Player %d",player+1);
        ImGui::SetNextItemWidth(220*UiScale(ImGui::GetIO()));
        changed|=ImGui::Combo("Character",&fight_config_.characters[player],characters,11);
        changed|=ImGui::Checkbox("CPU-controlled",&fight_config_.cpu[player]);
        changed|=ImGui::Checkbox("Alternate costume",&fight_config_.alternate[player]);
        ImGui::PopID();
      }
      ImGui::EndTable();
      }
      ImGui::Separator();
      ImGui::SetNextItemWidth(260*UiScale(ImGui::GetIO()));
      changed|=ImGui::Combo("Arena",&fight_config_.arena,arenas,16);
      // Tourist runs on the Easy base with the Tourist CPU handicap.
      const char* difficulties[]={"Use Game Settings","Tourist","Easy","Normal","Hard","Hardest"};
      int difficulty=fight_config_.tourist?1:fight_config_.difficulty<0?0:fight_config_.difficulty+2;
      ImGui::SetNextItemWidth(260*UiScale(ImGui::GetIO()));
      if(ImGui::Combo("Difficulty (shared)",&difficulty,difficulties,6)) {
        fight_config_.tourist=difficulty==1;
        fight_config_.difficulty=difficulty==0?-1:difficulty==1?0:difficulty-2;changed=true;
      }
      const char* rounds[]={"Use Game Settings","2","3","4","5"};
      int wins=fight_config_.rounds_to_win<0?0:fight_config_.rounds_to_win-1;
      ImGui::SetNextItemWidth(260*UiScale(ImGui::GetIO()));
      if(ImGui::Combo("Rounds to Win",&wins,rounds,5)) {
        fight_config_.rounds_to_win=wins==0?-1:wins+1;changed=true;
      }
      const char* times[]={"Use Game Settings","15 seconds","30 seconds","45 seconds","60 seconds"};
      int time=fight_config_.round_time<0?0:fight_config_.round_time/15;
      ImGui::SetNextItemWidth(260*UiScale(ImGui::GetIO()));
      if(ImGui::Combo("Round Time",&time,times,5)) {
        fight_config_.round_time=time==0?-1:time*15;changed=true;
      }
      if(changed && !fight::SaveConfig(custom_path_,fight_config_))
        REXLOG_ERROR("VF2 Custom Fight settings could not be saved: {}",custom_path_.string());
      if(ImGui::Button("Start Fight")) {
        fight::RequestStart(fight_config_);
        CloseWindows();
      }
      ImGui::SameLine();
      ImGui::BeginDisabled(!fight::CanRestart());
      if(ImGui::Button("Restart Fight")) {
        fight::RequestRestart();
        CloseWindows();
      }
      ImGui::EndDisabled();
      ImGui::TextWrapped("Restart repeats the last match. Edited choices apply with Start Fight.");
      ImGui::TextDisabled("Custom Fight choices are saved separately from Arcade settings.");
      ImGui::TextWrapped("%s",fight::Status().c_str());
      ImGui::TextDisabled("Starting a fight closes settings. Reopen F6 to change the fight.");
    }
    ImGui::End();
  }
  // Keep ownership with Vf2App. Closing during OnDraw must not delete this
  // object while ImGuiDrawer is still iterating its dialogs.
  void CloseWindows() {
    open_=false;
    launcher_open_=false;
    fight::SetSettingsOpen(false);
    REXLOG_INFO("VF2 settings and custom launcher closed for gameplay");
  }
  bool open_=true;
  bool draw_advanced_=false;
  bool launcher_open_=false;
  bool launcher_reported_=false;
  inline static fight::Config fight_config_;
  bool reported_=false;
  bool horizon_reported_=false;
  std::filesystem::path config_path_;
  std::filesystem::path custom_path_;
  std::filesystem::file_time_type last_write_{};
};
}
