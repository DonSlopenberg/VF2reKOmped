#pragma once
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <sstream>
#include <windows.h>

namespace vf2::launcher {
inline std::string Trim(std::string s) {
  const auto first=s.find_first_not_of(" \t\r\n");
  if(first==std::string::npos)return {};
  return s.substr(first,s.find_last_not_of(" \t\r\n")-first+1);
}
inline std::map<std::string,std::string> Read(const std::filesystem::path& path) {
  std::map<std::string,std::string> values;
  std::ifstream input(path);std::string line;
  while(std::getline(input,line)) {
    const auto eq=line.find('=');if(eq==std::string::npos)continue;
    auto key=Trim(line.substr(0,eq));
    if(key.empty() || key.front()=='#')continue;
    auto value=Trim(line.substr(eq+1));
    // Launcher-owned scalar values cannot contain a quoted '#'.
    if(key=="vf2_output_resolution" || key=="resolution" || key=="fullscreen" || key=="vf2_no_intro" || key=="vf2_display_device" || key=="vf2_background_replacements" || key=="vf2_arcade_tourist" || key=="vf2_arcade_random_opponents" || key=="vf2_arcade_random_arenas" || key=="swap_post_effect") {
      value=Trim(value.substr(0,value.find('#')));
      if(value.size()>=2 && value.front()=='"' && value.back()=='"')value=value.substr(1,value.size()-2);
      if(key=="vf2_display_device") {std::string decoded;for(size_t i=0;i<value.size();++i){if(value[i]=='\\' && i+1<value.size())++i;decoded+=value[i];}value=decoded;}
    }
    values[key]=value;
  }
  return values;
}
inline bool Save(const std::filesystem::path& path,const std::string& resolution,
                 bool fullscreen,bool skip_intro,const std::string& display_device={},
                 bool upscaled_backgrounds=false,bool tourist=false,
                 bool random_opponents=false,bool random_arenas=false,
                 const std::string& post_effect={}) {
  std::map<std::string,std::string> changes={
      {"vf2_output_resolution","\""+resolution+"\""},
      {"fullscreen",fullscreen?"true":"false"},
      {"vf2_no_intro",skip_intro?"true":"false"},
      {"vf2_background_replacements",upscaled_backgrounds?"true":"false"},
      {"vf2_arcade_tourist",tourist?"true":"false"},
      {"vf2_arcade_random_opponents",random_opponents?"true":"false"},
      {"vf2_arcade_random_arenas",random_arenas?"true":"false"}};
  if(!post_effect.empty())changes["swap_post_effect"]="\""+post_effect+"\"";
  if(!display_device.empty()) {std::string quoted="\"";for(char ch:display_device){if(ch=='\\' || ch=='"')quoted+='\\';quoted+=ch;}changes["vf2_display_device"]=quoted+'"';}
  std::ifstream input(path);std::ostringstream contents;std::string line;
  while(std::getline(input,line)) {
    const auto eq=line.find('=');const auto key=Trim(line.substr(0,eq));
    if(eq!=std::string::npos && changes.contains(key))continue;
    contents<<line<<'\n';
  }
  if(input.bad())return false;
  input.close();
  for(const auto& [key,value]:changes)contents<<key<<" = "<<value<<'\n';
  auto temporary=path;temporary+=".launcher.tmp";
  std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
  if(!output)return false;
  output<<contents.str();output.close();if(!output)return false;
  if(MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return true;
  std::error_code error;std::filesystem::remove(temporary,error);return false;
}
}
