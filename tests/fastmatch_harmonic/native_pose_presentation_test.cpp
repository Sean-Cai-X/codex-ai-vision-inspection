#include "../../cximage/CxHarmonicPosePresentation.h"
#include <fstream>
#include <iostream>
#include <limits>
using J=nlohmann::json;
using cxharmonicui::PoseDisplayLines;
void need(bool b){if(!b)throw std::runtime_error("POSE_PRESENTATION_GUARD_FAILED");}
J pose(){return {
 {"angle_deg",23.4},{"scale",1.2},{"correlation",1.0},{"residual",0.0},
 {"translation_x",17.25},{"translation_y",-8.5},{"cyclic_shift",0.25},
 {"mapping","reference_to_observation"},{"coordinate_units","source_units"}};}
void reject(const J& j){
 bool rejected=false;try{PoseDisplayLines(j);}catch(const std::exception&){rejected=true;}
 need(rejected);
}
int main(int argc,char**argv)try{
 J r={{"poses",J::array({pose()})}};
 auto lines=PoseDisplayLines(r);
 need(lines.size()==2&&lines[1].find("17.25")!=std::string::npos);
 need(lines[1].find("-8.5")!=std::string::npos);
 r["poses"].push_back(pose());lines=PoseDisplayLines(r);
 need(lines.size()==5&&lines[0].find("none selected")!=std::string::npos);
 r["poses"]=J::array();lines=PoseDisplayLines(r);
 need(lines.size()==1&&lines[0].find("No pose candidate")!=std::string::npos);
 auto legacy=pose();
 for(const auto* k:{"translation_x","translation_y","cyclic_shift","mapping","coordinate_units"})
  legacy.erase(k);
 r["poses"]=J::array({legacy});lines=PoseDisplayLines(r);
 need(lines[1].find("unavailable")!=std::string::npos);
 r["poses"][0]["translation_x"]=0;reject(r);
 r["poses"]=J::array({pose()});r["poses"][0]["mapping"]="observation_to_reference";reject(r);
 r["poses"]=J::array({pose()});r["poses"][0]["translation_x"]=nullptr;reject(r);
 r["poses"]=J::array({pose()});r["poses"][0]["cyclic_shift"]=1;reject(r);
 r["poses"]=J::array({pose()});r["poses"][0]["angle_deg"]=true;reject(r);
 r["poses"]=J::array({pose()});
 r["poses"][0]["translation_x"]=std::numeric_limits<double>::infinity();reject(r);
 reject(J{{"poses",J::object()}});
 if(argc==2){
  std::ifstream f(argv[1]);J receipt;f>>receipt;
  need(receipt.at("runs").size()==3);
  for(int i=0;i<2;++i) {
   lines=PoseDisplayLines(receipt.at("runs").at(i));
   need(lines.size()==2&&lines[1].find("Translation X")!=std::string::npos);
  }
  need(PoseDisplayLines(receipt.at("runs").at(2)).size()==1);
 }
 std::cout<<"POSE_PRESENTATION_PASS current/legacy/empty/ambiguous/malformed"<<std::endl;
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
