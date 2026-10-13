#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <filesystem>
#include <fstream>
#include <complex>
#include <iostream>
using J=nlohmann::json;using Z=std::complex<double>;namespace fs=std::filesystem;
J read(fs::path p){std::ifstream f(p);J j;f>>j;return j;}
void write(fs::path p,J j){std::ofstream f(p);f<<j.dump(2);if(!f)throw std::runtime_error("write");}
std::vector<Z> points(const J& j){std::vector<Z> v;for(auto p:j)v.emplace_back(p["x"],p["y"]);if(v.size()<3)throw std::runtime_error("points");return v;}
std::vector<Z> transform(std::vector<Z> p,Z f,Z t){for(auto& v:p)v=f*v+t;return p;}
J distances(const std::vector<Z>& from,const std::vector<Z>& to){
 J rows=J::array();std::vector<double> ds;double sum=0;
 // Uniform arc sampling avoids giving diagonal pixel staircases disproportionate weight.
 double length=0;for(size_t i=0;i<from.size();++i)length+=std::abs(from[(i+1)%from.size()]-from[i]);
 int count=int(std::ceil(length/.5));size_t edge=0;double traversed=0;
 for(int k=0;k<count;++k){
  double arc=length*k/count;while(edge+1<from.size()&&traversed+std::abs(from[(edge+1)%from.size()]-from[edge])<arc){traversed+=std::abs(from[(edge+1)%from.size()]-from[edge]);++edge;}
  Z delta=from[(edge+1)%from.size()]-from[edge],p=from[edge]+delta*((arc-traversed)/std::abs(delta));
  double best=1e100,bt=0;size_t bi=0;Z bp;
  for(size_t i=0;i<to.size();++i){Z d=to[(i+1)%to.size()]-to[i];double t=std::clamp(std::real(std::conj(d)*(p-to[i]))/std::norm(d),0.,1.);Z q=to[i]+t*d;double e=std::abs(p-q);if(e<best){best=e;bi=i;bt=t;bp=q;}}
  ds.push_back(best);sum+=best;rows.push_back({{"source_edge",edge},{"source_arc_px",arc},{"point",{p.real(),p.imag()}},{"nearest_target_edge",bi},{"target_fraction",bt},{"target_point",{bp.real(),bp.imag()}},{"distance_px",best}});
 }
 std::sort(ds.begin(),ds.end());return {{"mean_px",sum/ds.size()},{"p95_px",ds[size_t(.95*(ds.size()-1))]},{"max_px",ds.back()},{"count",ds.size()},{"mapping_semantics","geometric_nearest_segment_not_identity_confirmation"},{"samples",rows}};
}
void draw(cv::Mat& image,const std::vector<Z>& p,cv::Scalar color){
 std::vector<cv::Point> v;for(auto z:p)v.emplace_back(cvRound(z.real()),cvRound(z.imag()));cv::polylines(image,v,true,color,1,cv::LINE_AA);
}
int main(int argc,char**argv)try{
 if(argc!=3&&argc!=4)throw std::runtime_error("audit root / original replay root [cell]");fs::path root=argv[1],old=argv[2];J matrix=J::array();
 std::vector<Z> shape={{-70,-55},{54,-55},{54,-14},{8,-14},{8,19},{65,19},{65,54},{-70,54}};
 Z center(199.5,199.5);auto reference_truth=transform(shape,1.,center);
 if(argc==4){
 if(std::string(argv[3])!="cell")throw std::runtime_error("mode");
 for(const std::string basis:{"center","cell"})for(int method:{0,1}){
  std::string prefix=basis+"_m"+std::to_string(method);auto reference=points(read(root/(prefix+"_reference.json"))["points"]);
  for(const std::string name:{"notched_scale075","notched_mix37_075"}){
   auto dir=root/(prefix+"_"+name);auto obs=points(read(dir/"observation.json")["points"]);auto pose=read(dir/"native.json")["poses"][0];
   Z fit=std::polar(pose["scale"].get<double>(),pose["angle_deg"].get<double>()*std::acos(-1.)/180),shift(pose["translation_x"],pose["translation_y"]);
   Z truth=std::polar(.75,(name=="notched_scale075"?0:37)*std::acos(-1.)/180),tt=center-truth*center;
   auto fitted=transform(reference,fit,shift),truepose=transform(reference,truth,tt),targettruth=transform(reference_truth,truth,tt);
   J r={{"basis",basis},{"method",method},{"case",name},{"production_eligible",false},
    {"reference_to_truth",distances(reference,reference_truth)},{"observation_to_truth",distances(obs,targettruth)},
    {"fitted_to_observed",distances(fitted,obs)},{"observed_to_fitted",distances(obs,fitted)},{"true_pose_to_observed",distances(truepose,obs)}};
   write(dir/"boundary_distances.json",r);for(const char* k:{"reference_to_truth","observation_to_truth","fitted_to_observed","observed_to_fitted","true_pose_to_observed"})r[k].erase("samples");matrix.push_back(r);
   cv::Mat image=cv::imread((old/"fixtures"/(name+".pgm")).string());if(image.empty())throw std::runtime_error("image");
   draw(image,obs,{0,255,0});draw(image,fitted,{255,0,255});draw(image,targettruth,{255,255,0});cv::putText(image,basis+": observed green / fit magenta / truth cyan",{5,15},cv::FONT_HERSHEY_SIMPLEX,.3,{0,0,0},1);
   if(!cv::imwrite((dir/"pose_boundary_overlay.png").string(),image))throw std::runtime_error("image write");
  }
 }
 write(root/"boundary_summary.json",matrix);std::cout<<"CELL_BOUNDARY_SCORING_PASS "<<matrix.size()<<"\n";return 0;
 }

 for(int method:{0,1}){
 auto ref=read(root/("m"+std::to_string(method)+"_notched_reference")/"harmonic_audit_receipt.json");
 auto refpoints=points(ref["runs"][0]["debug"]["raw_contours"][1]["points"]);
 for(std::string name:{"notched_scale075","notched_mix37_075"}){
 auto dir=root/("m"+std::to_string(method)+"_"+name);auto run=read(dir/"harmonic_audit_receipt.json")["runs"][0];auto obs=points(run["debug"]["raw_contours"][1]["points"]);auto pose=run["poses"][0];
 Z fit=std::polar(pose["scale"].get<double>(),pose["angle_deg"].get<double>()*std::acos(-1.)/180),shift(pose["translation_x"],pose["translation_y"]);
 double angle=name=="notched_scale075"?0:37;Z truth=std::polar(.75,angle*std::acos(-1.)/180),tt=center-truth*center;
 auto fitted=transform(refpoints,fit,shift),truepose=transform(refpoints,truth,tt),obstruth=transform(reference_truth,truth,tt);
 J all={{"schema","observed_boundary_distance.v1"},{"case",name},{"method",method},{"production_eligible",false},{"fitted_to_observed",distances(fitted,obs)},{"observed_to_fitted",distances(obs,fitted)},{"true_pose_reference_to_observed",distances(truepose,obs)},{"reference_extraction_to_truth",distances(refpoints,reference_truth)},{"observation_extraction_to_truth",distances(obs,obstruth)}};
 write(dir/"boundary_distances.json",all);
 J row=all;for(const char* k:{"fitted_to_observed","observed_to_fitted","true_pose_reference_to_observed","reference_extraction_to_truth","observation_extraction_to_truth"})row[k].erase("samples");
 matrix.push_back(row);
 cv::Mat im=cv::imread((old/"fixtures"/(name+".pgm")).string());if(im.empty())throw std::runtime_error("image");
 draw(im,obs,{0,255,0});draw(im,fitted,{255,0,255});draw(im,obstruth,{255,255,0});
 cv::putText(im,"Green: observed | Magenta: fitted template | Cyan: analytic truth",{5,15},cv::FONT_HERSHEY_SIMPLEX,.27,{0,0,0},1);
 if(!cv::imwrite((dir/"pose_boundary_overlay.png").string(),im))throw std::runtime_error("overlay");
 }}
 write(root/"boundary_summary.json",matrix);std::cout<<matrix.dump(2)<<"\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
