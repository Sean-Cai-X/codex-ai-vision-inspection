#include "../../cximage/CxObservedGrayBoundary.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
using Z=std::complex<double>;using J=nlohmann::json;namespace fs=std::filesystem;
void need(bool b,const char*s){if(!b)throw std::runtime_error(s);}
void save(fs::path p,const J&j){std::ofstream f(p);f<<j.dump(2);need(bool(f),"write");}
std::vector<Z> shape={{-70,-55},{54,-55},{54,-14},{8,-14},{8,19},{65,19},{65,54},{-70,54}};
double distance(Z p,const std::vector<Z>& poly){double best=1e9;for(size_t i=0;i<poly.size();++i){Z a=poly[i],v=poly[(i+1)%poly.size()]-a;double t=std::clamp(std::real(std::conj(v)*(p-a))/std::norm(v),0.,1.);best=std::min(best,std::abs(p-a-t*v));}return best;}
bool inside(Z p,const std::vector<Z>&poly){bool in=false;for(size_t i=0,j=poly.size()-1;i<poly.size();j=i++){auto a=poly[i],b=poly[j];if((a.imag()>p.imag())!=(b.imag()>p.imag())&&p.real()<(b.real()-a.real())*(p.imag()-a.imag())/(b.imag()-a.imag())+a.real())in=!in;}return in;}
int main(int argc,char**argv)try{
 need(argc==2,"fresh output root");fs::path root=argv[1];need(!fs::exists(root),"fresh output required");fs::create_directories(root);
 J matrix=J::array();int number=0;
 for(double angle:{0.,37.})for(double scale:{.75,1.,1.25})for(double phase:{0.,.25,.5,.75})for(double sigma:{.8,1.6}){
  std::string id="case_"+std::to_string(number++);auto dir=root/id;fs::create_directories(dir);
  std::vector<Z> truth;for(auto p:shape)truth.push_back(Z(199.5+phase,199.5+phase)+std::polar(scale,angle*std::acos(-1.)/180)*p);
  // Independent area-integrated raster at each pose; not a transformed bitmap.
  cv::Mat coverage(400,400,CV_32F);
  for(int y=0;y<400;++y)for(int x=0;x<400;++x){int n=0;for(int sy=0;sy<4;++sy)for(int sx=0;sx<4;++sx)n+=inside(Z(x+(sx+.5)/4-.5,y+(sy+.5)/4-.5),truth);coverage.at<float>(y,x)=220-190*n/16.f;}
  cv::Mat blurred,gray;cv::GaussianBlur(coverage,blurred,cv::Size(),sigma,sigma,cv::BORDER_REPLICATE);blurred.convertTo(gray,CV_8U);
  need(cv::imwrite((dir/"source.pgm").string(),gray),"image write");
  cv::Mat mask;cv::threshold(gray,mask,128,255,cv::THRESH_BINARY_INV);std::vector<std::vector<cv::Point>> contours;cv::findContours(mask,contours,cv::RETR_TREE,cv::CHAIN_APPROX_NONE);need(contours.size()==1,"single contour");
  std::vector<Z> parent;for(auto p:contours[0])parent.emplace_back(p.x,p.y);
  for(int iterations:{0,8}){
   J row={{"case",id},{"angle_deg",angle},{"scale",scale},{"phase_xy_px",phase},{"sigma_px",sigma},{"recenter_iterations",iterations},{"production_eligible",false},{"pose_evaluated",false}};
   J points=J::array(),chunks=J::array(),rejections=J::object();double rawSum=0,refSum=0,cornerRaw=0,cornerRef=0,straightRaw=0,straightRef=0;int accepted=0,cornerN=0,straightN=0;
   for(size_t start=0;start<parent.size();start+=100){
    size_t count=std::min(size_t(100),parent.size()-start);cxgeom::OpenBoundaryRun run;run.parent_point_count=parent.size();
    for(size_t k=0;k<count+6;++k){size_t ix=(start+parent.size()-3+k)%parent.size();if(k&&ix==0)run.wraps_parent_origin=true;run.parent_indices.push_back(ix);run.points.push_back(parent[ix]);}
    cxgeom::polar::ObservedBoundaryInput raw(id,"v1","extracted",std::to_string(start),parent,run);auto before=raw.receipt();
    cxpartialimage::GrayBoundaryConfig cfg;cfg.recenter_iterations=iterations;
    auto result=cxpartialimage::RefineGrayBoundaryFromObserved(gray,raw,id,"v1",{},cfg);need(before==raw.receipt(),"raw changed");chunks.push_back(J::parse(result.receipt()));
    for(size_t k=3;k<count+3;++k){
     auto p=result.points[k];size_t ix=start+k-3;double nearCorner=1e9;for(auto v:truth)nearCorner=std::min(nearCorner,std::abs(p.raw-v));
     double rawError=distance(p.raw,truth);J entry={{"parent_index",ix},{"status",p.status},{"accepted",p.accepted},{"raw_error_px",rawError},{"corner_scoring_only",nearCorner<=6.},{"refined_error_px",nullptr}};
     if(p.accepted){double e=distance(p.refined,truth);entry["refined_error_px"]=e;rawSum+=rawError;refSum+=e;++accepted;if(nearCorner<=6){cornerRaw+=rawError;cornerRef+=e;++cornerN;}else{straightRaw+=rawError;straightRef+=e;++straightN;}}
     else {std::string key=p.status;rejections[key]=rejections.value(key,0)+1;}
     points.push_back(entry);
    }
   }
   need(points.size()==parent.size(),"coverage");
   row["raw_point_count"]=parent.size();row["accepted"]=accepted;row["rejected"]=parent.size()-accepted;row["reasons"]=rejections;
   row["same_support_raw_mean_px"]=accepted?J(rawSum/accepted):J(nullptr);row["refined_mean_px"]=accepted?J(refSum/accepted):J(nullptr);
   row["corner_accepted"]=cornerN;row["corner_raw_mean_px"]=cornerN?J(cornerRaw/cornerN):J(nullptr);row["corner_refined_mean_px"]=cornerN?J(cornerRef/cornerN):J(nullptr);
   row["straight_accepted"]=straightN;row["straight_raw_mean_px"]=straightN?J(straightRaw/straightN):J(nullptr);row["straight_refined_mean_px"]=straightN?J(straightRef/straightN):J(nullptr);
   save(dir/("recenter"+std::to_string(iterations)+".json"),{{"summary",row},{"points",points},{"chunks",chunks}});matrix.push_back(row);
  }
  std::ifstream f(dir/"source.pgm",std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(f)),{});
  J polygon=J::array();for(auto p:truth)polygon.push_back({p.real(),p.imag()});
  save(dir/"provenance.json",{{"image_sha256",cxgeom::so2::Sha256Bytes(bytes)},{"generator","independent_polygon_area_4x4_gaussian_pixel_sigma_then_u8"},{"truth_use","generation_and_posthoc_scoring_only"},{"truth_polygon",polygon},{"threshold",128},{"corner_scoring_radius_px",6},{"runtime_normals","observed_two_sided_chords"},{"chunk_core",100},{"chunk_halo",3},{"gap_policy","never_reclose_rejected_points"}});
 }
 save(root/"matrix.json",matrix);std::cout<<"GRAY_PHASE_PASS images="<<number<<" evaluations="<<matrix.size()<<" no_truth_normals no_pose_claim\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
