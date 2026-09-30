#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
int main(int argc,char** argv){
 try{
  if(argc!=2)return 64;
  std::filesystem::path out(argv[1]);
  if(std::filesystem::exists(out))throw std::runtime_error("fresh fixture directory required");
  std::filesystem::create_directories(out);
  for(const std::string name:{"interface","ring","multiple"}){
   std::ofstream f(out/(name+".pgm"),std::ios::binary);f<<"P5\n320 240\n255\n";
   for(int y=0;y<240;++y)for(int x=0;x<320;++x){
    double dx=x-159.5,dy=y-119.5,r=dx*dx+dy*dy;
    bool hit=name=="interface"?x<160:
     name=="ring"?(r<3600 && r>625):
     ((x>=40&&x<100&&y>=50&&y<130)||(x>=180&&x<280&&y>=120&&y<155));
    f.put(static_cast<char>(hit?30:220));
   }
   f.close();if(!f)throw std::runtime_error("image write failed");
  }
  std::cout<<"TOPOLOGY_FIXTURES_WRITTEN 3\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
