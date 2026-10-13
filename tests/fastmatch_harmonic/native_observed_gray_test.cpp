#include "../../cximage/CxObservedGrayBoundary.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace cxgeom::polar;using namespace cxpartialimage;using Json=nlohmann::json;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
ObservedBoundaryInput input(std::vector<std::complex<double>> p,const std::string& id){
 cxgeom::OpenBoundaryRun run;run.parent_point_count=p.size()+1;
 for(size_t i=0;i<p.size();++i){run.points.push_back(p[i]);run.parent_indices.push_back(i);}
 p.push_back({-1,-1});return {id,"v1","contour","span",p,run};
}
void save(const std::filesystem::path& p,const Json& j){std::ofstream f(p);f<<j.dump(2);f.close();check(bool(f),"write");}
int main(int argc,char** argv)try{
 check(argc==2,"output");std::filesystem::path out(argv[1]);std::filesystem::create_directories(out);
 Json summary={{"schema","observed_gray_normal_test.v1"},{"production_eligible",false},{"cases",Json::array()}};
 double worst=0;
 for(double angle:{0.,37.,90.,125.}){
  auto n=std::polar(1.,angle*std::acos(-1.)/180.),t=n*std::complex<double>(0,1),center=std::complex<double>(40.3,40.1);
  std::vector<std::complex<double>> p;
  for(int i=-12;i<=12;++i){auto q=center+.6*n+double(i)*2.*t;p.emplace_back(std::round(q.real()),std::round(q.imag()));}
  auto raw=input(p,"line_"+std::to_string(angle));auto before=raw.receipt();
  cv::Mat gray(80,80,CV_8UC1);
  for(int y=0;y<80;++y)for(int x=0;x<80;++x)
   gray.at<unsigned char>(y,x)=cv::saturate_cast<unsigned char>(128+100*std::erf(std::real(std::conj(n)*(std::complex<double>(x,y)-center))/1.6));
  auto estimated=EstimateBoundaryNormals(raw);
  auto result=RefineGrayBoundaryFromObserved(gray,raw,"gray","v1");
  auto runs=AcceptedGrayRuns(result);
  size_t accepted=0;double error=0;
  for(size_t i=0;i<result.points.size();++i){
   const auto& point=result.points[i];
   if(i<3||i+3>=p.size()){
    check(!point.accepted&&point.profile.empty()&&!point.normal_available,"endpoint must not sample fake normal");
   }else{
    check(estimated.points[i].accepted&&point.accepted,"straight supported point refused");
    ++accepted;error=std::max(error,std::abs(std::real(std::conj(n)*(point.refined-center))));
   }
  }
  check(accepted==19&&runs.size()==1&&runs[0].front()==3&&runs[0].back()==21,"contiguous refined span");
  check(error<=.3,"estimated normal quality");worst=std::max(worst,error);
  check(raw.receipt()==before,"raw unchanged");
  check(result.receipt()==RefineGrayBoundaryFromObserved(gray,raw,"gray","v1").receipt(),"deterministic");
  auto j=Json::parse(result.receipt());check(j["points"][0]["normal"].is_null(),"rejected normal must be null");
  save(out/("line_"+std::to_string(angle)+".json"),j);
  summary["cases"].push_back({{"angle_deg",angle},{"accepted_points",accepted},{"maximum_normal_error_px",error}});
 }
 std::vector<std::complex<double>> corner;
 for(int i=0;i<=12;++i)corner.emplace_back(20+i,20);
 for(int i=1;i<=12;++i)corner.emplace_back(32,20+i);
 auto c=input(corner,"corner");
 auto norm=EstimateBoundaryNormals(c);
 check(norm.points[12].status=="CORNER_OR_HIGH_CURVATURE","90 degree corner rejected");
 cv::Mat gray(64,64,CV_8UC1,cv::Scalar(128));
 for(int y=0;y<64;++y)for(int x=0;x<64;++x){
  double left=.5*(1+std::erf((32.-x)/1.6)),below=.5*(1+std::erf((y-20.)/1.6));
  gray.at<unsigned char>(y,x)=cv::saturate_cast<unsigned char>(28+200*left*below);
 }
 auto blocked=RefineGrayBoundaryFromObserved(gray,c,"corner","v1");
 check(!blocked.points[12].accepted&&blocked.points[12].profile.empty(),"no sampling corner");
 auto corner_runs=AcceptedGrayRuns(blocked);
 check(corner_runs.size()==2,"corner splits two measured spans");
 check(corner_runs[0].back()<12&&corner_runs[1].front()>12,"corner never bridged");
 save(out/"corner.json",Json::parse(blocked.receipt()));
 GrayBoundaryResult grouping;grouping.points.resize(9);
 for(size_t i=0;i<9;++i)grouping.points[i].accepted=i!=4;
 auto split=AcceptedGrayRuns(grouping);
 check(split.size()==2&&split[0]==std::vector<size_t>({0,1,2,3})&&split[1]==std::vector<size_t>({5,6,7,8}),"never bridge rejected point");
 std::reverse(corner.begin(),corner.end());auto reversed=EstimateBoundaryNormals(input(corner,"reverse"));
 check(reversed.points[12].status=="CORNER_OR_HIGH_CURVATURE","reverse corner");
 bool invalid=false;BoundaryNormalConfig cfg;cfg.half_window=0;
 try{EstimateBoundaryNormals(c,cfg);}catch(const std::invalid_argument&){invalid=true;}check(invalid,"invalid config");
 summary["worst_normal_error_px"]=worst;summary["raw_changed"]=false;save(out/"matrix.json",summary);
 std::cout<<"OBSERVED_GRAY_PASS four raster lines / estimated normals / corner / endpoint / gap guards worst="<<worst<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
