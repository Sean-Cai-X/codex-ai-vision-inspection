#include "../../cximage/CxGrayBoundaryRefinement.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace cxpartialimage;using namespace cxgeom::polar;using Json=nlohmann::json;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
ObservedBoundaryInput rawinput(const std::vector<std::complex<double>>& points,const std::string& id){
 auto parent=points;parent.push_back({1,1});cxgeom::OpenBoundaryRun run;run.parent_point_count=parent.size();
 for(size_t i=0;i<points.size();++i){run.points.push_back(points[i]);run.parent_indices.push_back(i);}
 return {id,"v1","boundary","span",parent,run};
}
void save(const std::filesystem::path& path,const Json& j){std::ofstream f(path);f<<j.dump(2);f.close();check(bool(f),"evidence write");}
int main(int argc,char** argv)try{
 check(argc==2,"output directory");std::filesystem::path out(argv[1]);std::filesystem::create_directories(out);
 Json summary={{"schema","gray_refinement_diagnostic.v1"},{"production_eligible",false},
 {"fixture","analytic_blurred_step_sampled_to_8bit_not_business_image"},
 {"normal_basis","analytic_oracle_normal_not_estimated"},{"quality_limit_px",.3},{"cases",Json::array()}};
 size_t cases=0;double worst=0;
 for(double angle:{0.,37.,90.,125.})for(double phase:{.1,.35,.7})for(double sigma:{.8,1.4}){
  auto normal=std::polar(1.,angle*std::acos(-1.)/180.);auto tangent=normal*std::complex<double>(0,1);
  auto center=std::complex<double>(32,32)+phase*normal;
  cv::Mat gray(64,64,CV_8UC1);
  for(int y=0;y<64;++y)for(int x=0;x<64;++x){
   double distance=std::real(std::conj(normal)*(std::complex<double>(x,y)-center));
   gray.at<unsigned char>(y,x)=cv::saturate_cast<unsigned char>(128+100*std::erf(distance/(std::sqrt(2.)*sigma)));
  }
  std::vector<std::complex<double>> points;for(int k=-2;k<=2;++k){
   auto p=center+.6*normal+double(k)*4.*tangent;points.emplace_back(std::round(p.real()),std::round(p.imag()));
  }
  std::string id="gray_"+std::to_string(cases++);auto raw=rawinput(points,id);auto before=raw.receipt();
  auto image=out/(id+".pgm");check(cv::imwrite(image.string(),gray),"image write");
  auto decoded=cv::imread(image.string(),0);
  auto result=RefineGrayBoundary(decoded,raw,std::vector<std::complex<double>>(points.size(),normal),id,"v1");
  check(result.receipt()==RefineGrayBoundary(decoded,raw,std::vector<std::complex<double>>(points.size(),normal),id,"v1").receipt(),"deterministic");
  cv::Mat inverted=255-decoded;
  auto falling=RefineGrayBoundary(inverted,raw,std::vector<std::complex<double>>(points.size(),normal),id+"_falling","v1");
  for(size_t i=0;i<points.size();++i)
   check(falling.points[i].accepted&&std::abs(falling.points[i].refined-result.points[i].refined)<1e-10,"polarity invariance");
  save(out/(id+"_falling.json"),Json::parse(falling.receipt()));
  double rawmax=0,refinedmax=0;
  for(const auto& p:result.points){
   check(p.accepted,"controlled grayscale profile rejected");
   rawmax=std::max(rawmax,std::abs(std::real(std::conj(normal)*(p.raw-center))));
   refinedmax=std::max(refinedmax,std::abs(std::real(std::conj(normal)*(p.refined-center))));
  }
  check(refinedmax<=.3,"controlled normal error exceeds unchanged threshold");check(raw.receipt()==before,"raw modified");
  worst=std::max(worst,refinedmax);
  auto receipt=Json::parse(result.receipt());check(receipt["uncertainty_calibrated"]==false,"uncertainty claim");
  Json entry={{"id",id},{"angle_deg",angle},{"phase_px",phase},{"blur_sigma",sigma},{"raw_max_normal_error_px",rawmax},
   {"refined_max_normal_error_px",refinedmax},{"receipt",receipt}};
  save(out/(id+".json"),entry);entry.erase("receipt");summary["cases"].push_back(entry);
 }
 auto raw=rawinput({{32,20},{32,24},{32,28}},"negative");
 std::vector<std::complex<double>> normals(3,{1,0});
 auto exercise=[&](const cv::Mat& gray,const char* expected,const char* id){
  auto r=RefineGrayBoundary(gray,raw,normals,id,"v1");
  for(const auto& p:r.points)check(!p.accepted&&p.status==expected,p.status.c_str());
  save(out/(std::string(id)+".json"),Json::parse(r.receipt()));
 };
 cv::Mat binary(64,64,CV_8UC1);
 for(int y=0;y<64;++y)for(int x=0;x<64;++x)binary.at<unsigned char>(y,x)=x<32?0:255;
 exercise(binary,"INSUFFICIENT_NATIVE_GRAY_LEVELS","binary");
 exercise(cv::Mat(64,64,CV_8UC1,cv::Scalar(100)),"LOW_CONTRAST","flat");
 cv::Mat low(64,64,CV_8UC1),multiple(64,64,CV_8UC1);
 for(int y=0;y<64;++y)for(int x=0;x<64;++x){
  low.at<unsigned char>(y,x)=cv::saturate_cast<unsigned char>(100+3*std::erf((x-32.)/2));
  double v=x<30?30:x<32?220:x<34?30:220;multiple.at<unsigned char>(y,x)=cv::saturate_cast<unsigned char>(v+(y%3));
 }
 exercise(low,"LOW_CONTRAST","low_contrast");
 exercise(multiple,"AMBIGUOUS_LEVEL_CROSSING","multiple");
 auto border=rawinput({{1,20},{1,24},{1,28}},"border");
 auto br=RefineGrayBoundary(binary,border,normals,"border","v1");
 for(const auto& p:br.points)check(!p.accepted&&p.status=="PROFILE_OUT_OF_IMAGE","border refusal");
 save(out/"border.json",Json::parse(br.receipt()));
 bool rejected=false;try{RefineGrayBoundary(binary,raw,normals,"wrong","v2");}catch(const std::invalid_argument&){rejected=true;}check(rejected,"revision guard");
  rejected=false;try{RefineGrayBoundary(binary,raw,std::vector<std::complex<double>>(3,{2,0}),"badnormal","v1");}catch(const std::invalid_argument&){rejected=true;}check(rejected,"normal guard");
  GrayBoundaryConfig invalid;invalid.step_px=0;
  rejected=false;try{RefineGrayBoundary(binary,raw,normals,"invalid","v1",invalid);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"config guard");
  summary["polarity_tests"]=48;
 summary["case_count"]=cases;summary["worst_refined_normal_error_px"]=worst;summary["negative_profile_cases"]=5;
 save(out/"matrix.json",summary);
 std::cout<<"GRAY_REFINEMENT_PASS cases="<<cases<<" worst_normal_error_px="<<worst<<"; not pose, corners, or business certification\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
