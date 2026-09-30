#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
// Controlled pixel fixtures only; no business images or model weights.
struct Case { const char* name; double angle,scale; const char* mode; const char* status; };
int main(int argc,char** argv) {
 try {
    if(argc!=2)return 64;
    const std::filesystem::path out(argv[1]);
    if(std::filesystem::exists(out))throw std::runtime_error("fresh fixture directory required");
    std::filesystem::create_directories(out);
    const Case cases[]={
      {"base",0,1,"clean","AUDIT_POSE_HYPOTHESES"},
      {"rotate15",15,1,"clean","AUDIT_POSE_HYPOTHESES"},
      {"rotate30",30,1,"clean","AUDIT_POSE_HYPOTHESES"},
      {"rotate60",60,1,"clean","AUDIT_POSE_HYPOTHESES"},
      {"rotate90",90,1,"clean","AUDIT_POSE_HYPOTHESES"},
      {"rotate135",135,1,"clean","AUDIT_POSE_HYPOTHESES"},
      {"scale065",0,.65,"clean","AUDIT_POSE_HYPOTHESES"},
      {"scale085",0,.85,"clean","AUDIT_POSE_HYPOTHESES"},
      {"scale120",0,1.2,"clean","AUDIT_POSE_HYPOTHESES"},
      {"blur",0,1,"blur","AUDIT_POSE_HYPOTHESES"},
      {"intensity_noise",0,1,"noise","AUDIT_POSE_HYPOTHESES"},
      {"occluded_declared",0,1,"partial","PARTIAL_CONTOUR_LEGACY_FALLBACK"},
      {"occluded_unverified",0,1,"unverified","UNVERIFIED_TOPOLOGY_LEGACY_FALLBACK"},
      {"occluded_unmarked",0,1,"unmarked","POSE_RESIDUAL_REJECTED"}
    };
    std::ofstream manifest(out/"raster.cases");
    for(const auto& c:cases) {
        std::vector<int> pixels(320*240);
        const double rad=c.angle*3.14159265358979323846/180;
        for(int y=0;y<240;++y)for(int x=0;x<320;++x) {
            const double dx=x-159.5,dy=y-119.5;
            const double u=(std::cos(rad)*dx+std::sin(rad)*dy)/c.scale;
            const double v=(-std::sin(rad)*dx+std::cos(rad)*dy)/c.scale;
            bool inside=std::abs(u)<70 && std::abs(v)<40;
            if(std::string(c.mode)=="partial" || std::string(c.mode)=="unverified" || std::string(c.mode)=="unmarked")
                inside=inside && x<190; // Raster occlusion, not an open point list.
            int value=inside?30:220;
            if(std::string(c.mode)=="noise")value+=((x*17+y*31+x*y*7)%25)-12;
            pixels[y*320+x]=value;
        }
        if(std::string(c.mode)=="blur") {
            auto original=pixels;
            for(int y=1;y<239;++y)for(int x=1;x<319;++x) {
                int sum=0;for(int j=-1;j<=1;++j)for(int i=-1;i<=1;++i)sum+=original[(y+j)*320+x+i];
                pixels[y*320+x]=sum/9;
            }
        }
        std::ofstream image(out/(std::string(c.name)+".pgm"),std::ios::binary);
        image<<"P5\n320 240\n255\n";
        for(int value:pixels)image.put(static_cast<char>(value));
        image.close();if(!image)throw std::runtime_error("image write failed");
        manifest<<c.name<<' '<<c.angle<<' '<<c.scale<<' '<<c.mode<<' '<<c.status<<'\n';
    }
    manifest.close();if(!manifest)throw std::runtime_error("manifest write failed");
    std::cout<<"RASTER_FIXTURES_WRITTEN 14\n";
 } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
