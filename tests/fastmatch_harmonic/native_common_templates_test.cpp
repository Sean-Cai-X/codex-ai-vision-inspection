#include "CxGeoCommonTemplates.h"
#include "CxGeoSO2ReferenceAsset.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <iomanip>
using namespace cxgeom::so2;using J=nlohmann::json;namespace fs=std::filesystem;using Z=std::complex<double>;
void need(bool b,const char* s){if(!b)throw std::runtime_error(s);}
void write(fs::path p,std::string s){std::ofstream f(p);f<<s;need(bool(f),"write");}
int main(int argc,char**argv)try{
 need(argc==2,"output root");fs::path root=argv[1];fs::create_directories(root);
 J catalog=J::array(),matrix=J::array();
 for(auto spec:templates::Catalog()){
  auto c=templates::Generate(spec.id);auto dir=root/spec.id;fs::create_directories(dir);
  J points=J::array();for(size_t i=0;i<c.points.size();++i)points.push_back({{"id",std::string(spec.id)+":"+std::to_string(i)},{"x",c.points[i].real()},{"y",c.points[i].imag()}});
  J entry={{"template_id",spec.id},{"template_revision","v1"},{"parent_revision",nullptr},{"geometry_type",spec.geometry},{"closed",c.closed},
   {"source","analytic_parameterized_geometry_not_business_image"},{"topology_basis","generator_constructed_single_component_no_holes"},
   {"symmetry_order_default_instance",spec.symmetry},{"actual_source_point_count",c.points.size()},{"parameters",{{"width",80},{"height",50},{"corner_radius",8},{"samples",256}}},
   {"status","REFERENCE_ONLY"},{"production_eligible",false},{"image_extraction_verified",false},{"points",points}};
  write(dir/"template.json",entry.dump(2));
  if(!c.closed){
   bool rejected=false;try{Build(c);}catch(const std::invalid_argument&){rejected=true;}
   need(rejected,"open contour must not enter closed solver");entry["closed_solver_status"]="REJECTED_OPEN_INPUT";entry.erase("points");catalog.push_back(entry);continue;
  }
  for(auto method:{Method::Dft,Method::Efd}){
   std::string name=method==Method::Dft?"dft":"efd";auto descriptor=Build(c,Config{},method);
   ReferenceAsset asset{descriptor,std::string("common-template:v1:")+spec.id};
   auto bytes=EncodeReference(asset),sha=ReferenceSha256(bytes);auto loaded=DecodeReference(bytes,sha,Config{},method);
   need(EncodeReference(loaded)==bytes,"asset roundtrip");write(dir/(name+".asset"),bytes);write(dir/(name+".sha256"),sha+"\n");
   entry[name+"_sha256"]=sha;
   for(double angle:{-43.,37.})for(double scale:{.8,1.3}){
    auto observed=c;Z f=std::polar(scale,angle*std::acos(-1.)/180),t(17.25,-8.5);
    for(auto& p:observed.points)p=f*p+t;
    std::rotate(observed.points.begin(),observed.points.begin()+7,observed.points.end());
    auto result=Match(loaded.descriptor,Build(observed,Config{},method));
    J row={{"template_id",spec.id},{"method",name},{"truth_angle",angle},{"truth_scale",scale},{"status",result.status},{"symmetry",result.symmetry_order},{"poses",J::array()},{"production_eligible",false},{"truth_supplied_to_solver",false},{"observation_source","transformed_analytic_contour_not_image_extraction"}};
    double best=1e10;for(auto pose:result.poses){double e=std::abs(std::remainder(pose.angle_deg-angle,360));best=std::min(best,e);
     row["poses"].push_back({{"angle_deg",pose.angle_deg},{"scale",pose.scale},{"translation_x",pose.translation_x},{"translation_y",pose.translation_y},{"residual",pose.residual}});}
    row["nearest_truth_angle_error_deg"]=result.poses.empty()?J(nullptr):J(best);
    if(std::string(spec.id)=="circle")need(result.status=="ORIENTATION_UNOBSERVABLE","circle direction");
    if(spec.symmetry>0){need(result.succeeded&&result.poses.size()==size_t(spec.symmetry)&&best<1,"pose/equivalent count");
     bool accurate=false;for(const auto& pose:result.poses)if(std::abs(std::remainder(pose.angle_deg-angle,360))<1&&std::abs(pose.scale-scale)<.001&&std::abs(Z(pose.translation_x,pose.translation_y)-t)<.3)accurate=true;need(accurate,"similarity accuracy");}
    matrix.push_back(row);
   }
   // A replay receives only a persisted reference and observed coordinates, no correspondence pairs.
   auto observation=c;for(auto& p:observation.points)p=std::polar(1.3,37*std::acos(-1.)/180)*p+Z(17.25,-8.5);
   std::rotate(observation.points.begin(),observation.points.begin()+7,observation.points.end());
   std::ostringstream s;s<<std::setprecision(17)<<"Image carrier; FindObject carrier_object;\ncarrier.copyFromMat(global_matInput);\ncarrier_object.setrect(0,0,100,100);\ncarrier_object.setsearchtype(0);\ncarrier_object.setdistance(1);\ncarrier_object.setbrow(2);\ncarrier_object.setroithre(128);\ncarrier_object.setminmax(1,99999999);\ncarrier_object.measurecc(carrier);\n// Carrier is unrelated to analytic geometry below.\nHarmonicAudit a;\na.parameter("<<(method==Method::Dft?0:1)<<",\"method\");\na.select(0);\na.trustedsha(\""<<sha<<"\");\na.loadasset(\""<<fs::absolute(dir/(name+".asset")).generic_string()<<"\");\na.select(1);\n";
   for(auto p:observation.points)s<<"a.point("<<p.real()<<","<<p.imag()<<");\n";
   s<<"a.topology(1,1,1,0,1);\na.run();\na.save(global_harmonic_receipt_path);\n";
   write(dir/(name+".cxsc"),s.str());
  }
  entry.erase("points");catalog.push_back(entry);
 }
 write(root/"catalog.json",catalog.dump(2));write(root/"matrix.json",matrix.dump(2));
 std::cout<<"COMMON_TEMPLATES_PASS templates="<<catalog.size()<<" pose_runs="<<matrix.size()<<" no production or image detection claim\n";
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
