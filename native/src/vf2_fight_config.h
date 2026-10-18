#pragma once
#include "vf2_fight.h"
#include <filesystem>
#include <fstream>
#include <windows.h>

namespace vf2::fight {
inline bool ValidConfig(const Config& c) {
  return c.characters[0]>=0 && c.characters[0]<=10 && c.characters[1]>=0 &&
      c.characters[1]<=10 && c.arena>=0 && c.arena<=15 &&
      c.difficulty>=-1 && c.difficulty<=3 &&
      (c.rounds_to_win==-1 || (c.rounds_to_win>=2 && c.rounds_to_win<=5)) &&
      (c.round_time==-1 || c.round_time==15 || c.round_time==30 ||
       c.round_time==45 || c.round_time==60) && (!c.tourist || c.difficulty==0);
}
inline Config LoadConfig(const std::filesystem::path& path) {
  std::ifstream input(path);std::string magic;int version=0,p1=0,p2=0;Config c;
  if(!(input>>magic>>version) || magic!="VF2_CUSTOM_FIGHT" || (version!=1 && version!=2))return {};
  if(!(input>>c.characters[0]>>c.characters[1]>>p1>>p2>>c.arena>>
      c.difficulty>>c.rounds_to_win>>c.round_time) ||
      (p1!=0 && p1!=1) || (p2!=0 && p2!=1))return {};
  if(version==2) {
    int tourist=0,a1=0,a2=0;
    if(!(input>>tourist>>a1>>a2) || tourist<0 || tourist>1 || a1<0 || a1>1 || a2<0 || a2>1)return {};
    c.tourist=tourist;c.alternate[0]=a1;c.alternate[1]=a2;
  }
  if(!ValidConfig(c))return {};
  c.cpu[0]=p1!=0;c.cpu[1]=p2!=0;return c;
}
inline bool SaveConfig(const std::filesystem::path& path,const Config& c) {
  if(!ValidConfig(c))return false;
  std::error_code error;std::filesystem::create_directories(path.parent_path(),error);
  if(error)return false;
  auto temp=path;temp+=".tmp";
  std::ofstream out(temp,std::ios::binary|std::ios::trunc);
  out<<"VF2_CUSTOM_FIGHT 2\n"<<c.characters[0]<<' '<<c.characters[1]<<' '
     <<c.cpu[0]<<' '<<c.cpu[1]<<' '<<c.arena<<' '<<c.difficulty<<' '
     <<c.rounds_to_win<<' '<<c.round_time<<' '<<c.tourist<<' '
     <<c.alternate[0]<<' '<<c.alternate[1]<<'\n';
  out.close();
  return bool(out) && MoveFileExW(temp.c_str(),path.c_str(),
      MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
}
}
