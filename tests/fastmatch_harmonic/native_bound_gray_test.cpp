#include "../../cximage/CxBoundGrayFrame.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <type_traits>
using namespace cxpartialimage;using Json=nlohmann::json;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void save(const std::filesystem::path& p,const Json& j){std::ofstream f(p);f<<j.dump(2);f.close();check(bool(f),"write");}
int main(int argc,char** argv)try{
 static_assert(!std::is_convertible<BoundRefinedSpan,cxgeom::polar::ObservedBoundaryInput>::value,"never refined as raw");
 check(argc==2,"output");std::filesystem::path out(argv[1]);std::filesystem::create_directories(out);
 cv::Mat image(64,64,CV_8UC1);
 for(int y=0;y<64;++y)for(int x=0;x<64;++x)image.at<unsigned char>(y,x)=cv::saturate_cast<unsigned char>(128+100*std::erf((x-31.3)/1.6));
 auto frame=BoundGrayFrame::Capture(image,"image","v1",128,{29,12,6,30});
 const auto binding=frame.receipt(),raw=frame.raw().receipt();
 auto refinement=frame.refine();auto before=refinement.receipt();
 auto spans=frame.refined_spans();
 check(spans.size()==1,"one straight span");
 const auto& span=spans[0];
 check(span.source_begin()==3&&span.source_end()==26,"trimmed source range explicit");
 check(span.raw_points().size()==24&&span.source_ids().size()==24,"mapping count");
 for(size_t i=0;i<span.refined_points().size();++i){
  check(std::abs(span.refined_points()[i].real()-31.3)<.3,"refined position");
  check(span.source_ids()[i]==frame.raw().features()[i+3].stable_id,"source identity");
 }
 auto j=Json::parse(span.receipt());
 check(j["representation"]=="gray_derived_not_raw"&&j["binding"]["same_pixel_frame"]==true,"typed evidence");
 check(j["binding"]["gray_sha256"]==j["refinement"]["gray_sha256"],"gray binding");
 save(out/"bound_span.json",j);save(out/"frame.json",Json::parse(binding));
 // External image writes and public diagnostic receipt edits cannot mutate snapshot/span.
 image.setTo(0);refinement.points[5].refined={999,999};refinement.gray_sha256="forged";
 check(frame.refine().receipt()==before&&frame.receipt()==binding&&frame.raw().receipt()==raw,"owned immutable snapshot");
 check(frame.refined_spans()[0].receipt()==span.receipt(),"cannot import mutable forged result");
 cv::Mat changed(64,64,CV_8UC1);
 for(int y=0;y<64;++y)for(int x=0;x<64;++x)changed.at<unsigned char>(y,x)=cv::saturate_cast<unsigned char>(128+100*std::erf((x-32.3)/1.6));
 auto other=BoundGrayFrame::Capture(changed,"image","v1",128,{29,12,6,30});
 check(Json::parse(other.receipt())["gray_sha256"]!=Json::parse(binding)["gray_sha256"],"same ID does not imply same frame");
 check(other.raw().features()[0].stable_id!=frame.raw().features()[0].stable_id,"pixel change changes source identity");
 auto threshold=BoundGrayFrame::Capture(changed,"image","v1",140,{29,12,6,30});
 check(threshold.raw().source_id()!=other.raw().source_id(),"threshold changes lineage");
 bool rejected=false;try{BoundGrayFrame::Capture(changed,"image","v1",255,{29,12,6,30});}catch(const std::invalid_argument&){rejected=true;}check(rejected,"threshold guard");
 save(out/"other_frame.json",Json::parse(other.receipt()));
 std::cout<<"BOUND_GRAY_FRAME_PASS owned pixels / mask derivation / source IDs / immutable refined spans / no raw conversion\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
