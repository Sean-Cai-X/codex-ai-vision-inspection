#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using Point=std::pair<double,double>;
std::vector<Point> contour(const std::string& family) {
 if(family=="bevel")return {{-70,-43},{12,-58},{67,-16},{45,51},{-22,63},{-63,18}};
 if(family=="notched")return {{-70,-55},{54,-55},{54,-14},{8,-14},{8,19},{65,19},{65,54},{-70,54}};
 std::vector<Point> v;
 for(int i=0;i<256;++i){
  double a=i*6.283185307179586/256;
  double r=63+15*std::cos(3*a+.3)+10*std::sin(2*a)+7*std::cos(a-.8);
  v.emplace_back(r*std::cos(a),r*std::sin(a));
 }
 return v;
}
bool inside(const std::vector<Point>& p,double x,double y){
 bool yes=false;
 for(size_t i=0,j=p.size()-1;i<p.size();j=i++){
  const auto a=p[i],b=p[j];
  if((a.second>y)!=(b.second>y) &&
     x<(b.first-a.first)*(y-a.second)/(b.second-a.second)+a.first)yes=!yes;
 }
 return yes;
}
struct Variant{const char* name;double angle,scale;const char* mode;};
int main(int argc,char** argv){
 try{
  if(argc!=2)return 64;
  std::filesystem::path out(argv[1]);
  if(std::filesystem::exists(out))throw std::runtime_error("fresh fixture directory required");
  std::filesystem::create_directories(out);
  std::ofstream manifest(out/"complex.cases");
  const Variant variants[]={{"base",0,1,"clean"},{"rotate37",37,1,"clean"},
   {"scale075",0,.75,"clean"},{"mix37_075",37,.75,"clean"},
   {"mix123_125",123,1.25,"clean"},{"blur_mix65_090",65,.9,"blur"},
   {"mirror",0,1,"mirror"},{"partial",37,.85,"partial"}};
  for(const std::string family:{"bevel","notched","wavy"}){
   const auto poly=contour(family);
   for(const auto& v:variants){
    const std::string name=family+"_"+v.name,mode=v.mode;
    const double a=v.angle*3.14159265358979323846/180;
    std::vector<int> pixels(400*400);
    for(int y=0;y<400;++y)for(int x=0;x<400;++x){
     double dx=x-199.5,dy=y-199.5;
     double u=(cos(a)*dx+sin(a)*dy)/v.scale;
     double w=(-sin(a)*dx+cos(a)*dy)/v.scale;
     if(mode=="mirror")u=-u;
     bool hit=inside(poly,u,w);
     if(mode=="partial")hit=hit && u<20;
     pixels[y*400+x]=hit?30:220;
    }
    if(mode=="blur"){
     auto original=pixels;
     for(int y=1;y<399;++y)for(int x=1;x<399;++x){
      int sum=0;for(int j=-1;j<=1;++j)for(int i=-1;i<=1;++i)sum+=original[(y+j)*400+x+i];
      pixels[y*400+x]=sum/9;
     }
    }
    std::ofstream f(out/(name+".pgm"),std::ios::binary);f<<"P5\n400 400\n255\n";
    for(int value:pixels)f.put(static_cast<char>(value));
    f.close();if(!f)throw std::runtime_error("image write failed");
    const char* status=mode=="mirror"?"POSE_RESIDUAL_REJECTED":
      mode=="partial"?"PARTIAL_CONTOUR_LEGACY_FALLBACK":"AUDIT_POSE_HYPOTHESES";
    manifest<<family<<' '<<name<<' '<<v.angle<<' '<<v.scale<<' '<<mode<<' '<<status<<'\n';
   }
  }
  manifest.close();if(!manifest)throw std::runtime_error("manifest write failed");
  std::cout<<"COMPLEX_FIXTURES_WRITTEN 24\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
