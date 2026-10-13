#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <complex>
#include <cmath>
using J=nlohmann::json;namespace fs=std::filesystem;
int main(int argc,char**argv)try{
 if(argc!=2)throw std::runtime_error("replay root");fs::path root=argv[1];J rows=J::array();double maxa=0,maxs=0,maxt=0,maxc=0;int accepted=0,rejected=0;
 for(int method:{0,1}){
 std::ifstream list(root/"fixtures/complex.cases");std::string family,name,mode,expected;double angle,scale;
 while(list>>family>>name>>angle>>scale>>mode>>expected){
  fs::path path=root/("m"+std::to_string(method)+"_"+name)/"harmonic_audit_receipt.json";std::ifstream f(path);J j;f>>j;auto r=j.at("runs").at(0);
  if(r.at("status")!=expected)throw std::runtime_error("status");
  if(j.at("production_eligible")!=false)throw std::runtime_error("production");
  if(r["inputs"][0]["input_kind"]!="sha_checked_descriptor_asset"||r["inputs"][1]["point_count"].get<int>()<=0||r["inputs"][1]["measured_source_count"]!=1)throw std::runtime_error("actual source");
  J q={{"case",name},{"method",method},{"status",expected},{"source","independently_rasterized_images_extracted_by_FindObject"},{"truth_used_only_for_scoring",true},{"topology","declared_controlled_fixture_not_automatically_inferred"},{"pose_count",r["poses"].size()}};
  if(expected=="AUDIT_POSE_HYPOTHESES"){
   if(r["poses"].size()!=1)throw std::runtime_error("unique");auto p=r["poses"][0];std::complex<double> center(199.5,199.5),factor=std::polar(scale,angle*std::acos(-1.)/180),truth=center-factor*center;
   double a=std::abs(std::remainder(p["angle_deg"].get<double>()-angle,360)),s=std::abs(p["scale"].get<double>()-scale),t=std::abs(std::complex<double>(p["translation_x"],p["translation_y"])-truth);
   double ce=std::abs(std::polar(p["scale"].get<double>(),p["angle_deg"].get<double>()*std::acos(-1.)/180)*center+std::complex<double>(p["translation_x"],p["translation_y"])-center);q["center_error_px"]=ce;maxc=std::max(maxc,ce);
   q["angle_error_deg"]=a;q["scale_absolute_error"]=s;q["translation_error_px"]=t;q["descriptor_residual_not_boundary_error"]=p["residual"];
   q["observed_boundary_error_px"]=nullptr;q["observed_boundary_error_status"]="NOT_SCORED_NO_POINTWISE_MAPPING_RECEIPT";
   if(a>1||s>.02)throw std::runtime_error("fixed pose limits");
   maxa=std::max(maxa,a);maxs=std::max(maxs,s);maxt=std::max(maxt,t);++accepted;
  }else{if(!r["poses"].empty())throw std::runtime_error("negative pose");++rejected;}
  rows.push_back(q);
 }}
 if(rows.size()!=48)throw std::runtime_error("count");
 J out={{"schema","independent_image_pose_score.v1"},{"cases",rows},{"accepted_cases",accepted},{"expected_rejections",rejected},{"max_angle_error_deg",maxa},{"max_scale_absolute_error",maxs},{"max_translation_error_px",maxt},{"max_center_error_px",maxc},{"translation_acceptance_limit",nullptr},{"translation_gate_status","diagnostic_only_no_predeclared_gate"},{"production_eligible",false}};
 std::ofstream f(root/"pose_scoring.json");f<<out.dump(2);if(!f)throw std::runtime_error("write");
 std::cout<<out.dump(0)<<"\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
