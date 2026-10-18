#include "vf2_fight_config.h"
#include <cassert>
#include <iostream>
int main(int argc,char** argv) {
  assert(argc==2);
  const auto path=std::filesystem::path(argv[1])/"roundtrip.cfg";
  int checks=0;
  for(int difficulty=-1;difficulty<=3;++difficulty)
    for(int wins:{-1,2,3,4,5})for(int time:{-1,15,30,45,60}) {
      vf2::fight::Config c;c.characters[0]=9;c.characters[1]=10;
      c.cpu[0]=true;c.cpu[1]=false;c.arena=10;c.difficulty=difficulty;
      c.rounds_to_win=wins;c.round_time=time;
      assert(vf2::fight::SaveConfig(path,c));
      const auto read=vf2::fight::LoadConfig(path);
      assert(read.characters[0]==9 && read.characters[1]==10 && read.arena==10);
      assert(read.cpu[0] && !read.cpu[1]);
      assert(read.difficulty==difficulty && read.rounds_to_win==wins && read.round_time==time);
      assert(!read.tourist && !read.alternate[0] && !read.alternate[1]);
      ++checks;
    }
  {
    vf2::fight::Config c;c.difficulty=0;c.tourist=true;c.alternate[1]=true;
    assert(vf2::fight::SaveConfig(path,c));
    const auto read=vf2::fight::LoadConfig(path);
    assert(read.tourist && read.difficulty==0 && !read.alternate[0] && read.alternate[1]);
    c.difficulty=2;assert(!vf2::fight::SaveConfig(path,c));  // Tourist needs the Easy base
    std::ofstream(path)<<"VF2_CUSTOM_FIGHT 1\n3 4 0 1 2 1 3 30\n";  // version 1 still loads
    const auto old=vf2::fight::LoadConfig(path);
    assert(old.characters[0]==3 && old.round_time==30 && !old.tourist && !old.alternate[0]);
    ++checks;
  }
  vf2::fight::Config invalid;invalid.round_time=999;
  assert(!vf2::fight::SaveConfig(path,invalid));
  std::ofstream(path)<<"VF2_CUSTOM_FIGHT 1\n9 9 1 1 10 3 5 999\n";
  assert(vf2::fight::LoadConfig(path).characters[0]==0);
  std::cout<<"PASS: "<<checks<<" persisted Custom Fight configurations; invalid values rejected\n";
}
