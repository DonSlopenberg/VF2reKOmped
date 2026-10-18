#include "launcher_config.h"
#include <cassert>
#include <iostream>
int main(int argc,char** argv) {
  assert(argc==2);const std::filesystem::path path=argv[1];
  {std::ofstream out(path);out<<"# preserve this\nresolution = \"3840x2160\"\nlicense_mask = 1\nuser_data_root = \"C:\\\\saved\"\nvf2_no_intro = true # startup\n";}
  auto old=vf2::launcher::Read(path);assert(old["resolution"]=="3840x2160" && old["vf2_no_intro"]=="true");
  for(const auto resolution:{"1280x720","1920x1080","2560x1440","3840x2160"})
    for(bool fullscreen:{false,true})for(bool skip:{false,true}) {
      assert(vf2::launcher::Save(path,resolution,fullscreen,skip,"\\\\.\\DISPLAY2"));
      const auto values=vf2::launcher::Read(path);
      assert(values.at("vf2_output_resolution")==resolution);
      assert(values.at("fullscreen")== (fullscreen?"true":"false"));
      assert(values.at("vf2_no_intro")== (skip?"true":"false"));
      assert(values.at("license_mask")=="1");assert(values.at("user_data_root")==old["user_data_root"]);
      assert(values.at("vf2_display_device")=="\\\\.\\DISPLAY2");
    }
  std::cout<<"PASS: 16 launcher setting combinations; legacy config and unrelated values preserved\n";
}
