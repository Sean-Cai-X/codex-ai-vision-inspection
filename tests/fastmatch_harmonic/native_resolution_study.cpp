#include "muParser.h"
#include "../../cximage/CxRefinedArcUiParameters.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <chrono>
namespace fs=std::filesystem;using J=nlohmann::json;using Z=std::complex<double>;
void need(bool b,const char* m){if(!b)throw std::runtime_error(m);}
J read(fs::path p){std::ifstream f(p);J j;f>>j;return j;}
void write(fs::path p,const std::string& s){std::ofstream f(p);f<<s;need(bool(f),"write");}
double curve(double x){return 45+.1*(x-40)+3*std::exp(-std::pow((x-28)/5,2))-4*std::exp(-std::pow((x-58)/6,2));}
int main(int argc,char** argv)try{
 need(argc==3||argc==4,"run root / new output root [budget-control]");
 fs::path base=fs::absolute(argv[1]),out=fs::absolute(argv[2]);
 need(!fs::exists(out),"refuse overwrite");fs::create_directories(out);
 setenv("CXVISION_RUN_ROOT",base.c_str(),1);
 J matrix=J::array();
 struct Variant{const char* name;const char* key;double value;bool gray;};
 std::vector<Variant> variants={{"baseline","",0,false},{"scan125","scan_step_px",.125,true},{"scan0625","scan_step_px",.0625,true},
 {"search05","identity_step_px",.05,false},{"search025","identity_step_px",.025,false},
 {"signature24","identity_samples",24,false},{"signature48","identity_samples",48,false},
 {"pose64","samples",64,false},{"pose128","samples",128,false}};
 if(argc==4){need(std::string(argv[3])=="budget-control"||std::string(argv[3])=="recenter","mode");variants=std::string(argv[3])=="recenter"?std::vector<Variant>{{"legacy","",0,false},{"recenter8","recenter_iterations",8,true}}:std::vector<Variant>{{"budget4096","",0,false},{"search025_budget4096","identity_step_px",.025,false}};}
 std::vector<std::string> cases={"blur_3p2","clean_control","repeated_boundary"};
 if(argc==4&&std::string(argv[3])=="recenter")cases={"blur_3p2","clean_control","repeated_boundary","noise_4","sharpen_0p8","blur_8"};
 for(const std::string id:cases){
  fs::path source=base/"harmonic_features_20261008"/(id=="repeated_boundary"?"identity_boundary_controls_20261013":"boundary_stress_20261013")/id;
  J original=read(source/"case_config.json");
  if(argc==4&&std::string(argv[3])=="budget-control")original["parameters"]["identity_maximum_hypotheses"]=4096;
  for(auto v:variants){
   J s=original;if(std::string(v.key)!=""){
    if(v.gray)for(auto& side:s["sides"])side["gray"][v.key]=v.value;
    else s["parameters"][v.key]=v.value;
   }
   auto dir=out/(id+"_"+v.name);fs::create_directories(dir);
   auto script=cxpairui::Build(s);need(cxpairui::Read(script)==s,"canonical");
   write(dir/"case_config.json",s.dump(2));write(dir/"run.cxsc",script);
   mu::Parser p;double* org=nullptr;p.DefineOrgClass("double",org);p.UsingClass(true);RegisterRefinedArcAudit(p);
   p.SetExpr(script);auto start=std::chrono::steady_clock::now();p.Eval();
   auto a=static_cast<CxRefinedArcAudit*>(p.GetClassObj("RefinedArcAudit","a"));need(a,"audit");
   double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
   J j=J::parse(a->receipt());write(dir/"native.json",j.dump(2));
   cv::imwrite((dir/"reference_overlay.png").string(),a->sideoverlay(0));cv::imwrite((dir/"observation_overlay.png").string(),a->sideoverlay(1));
   J row={{"case",id},{"variant",v.name},{"directory",dir.string()},{"elapsed_native_ms",ms},
    {"status",j["status"]},{"accepted",!j["refined"]["accepted_indices"].empty()},{"production_eligible",false},{"identity_queries",J::array()}};
   if(j.contains("identity_diagnostics"))for(auto& d:j["identity_diagnostics"])for(auto& q:d["queries"]){
    J shortq;for(auto key:{"reference_side","endpoint","status","best_score_px","best_arc_px"})shortq[key]=q[key];
    shortq["search_count"]=q["scores_px"].size();if(q["scores_px"].empty()){shortq["best_score_px"]=nullptr;shortq["best_arc_px"]=nullptr;}row["identity_queries"].push_back(shortq);
   }
   if(!j["refined"]["candidates"].empty()){
    const auto& c=j["refined"]["candidates"][0];row["candidate"]=c;
    if(id!="repeated_boundary"){
     auto f=std::polar(1.3,37*std::acos(-1.)/180.),t=Z(67,64)-f*Z(44,45);auto ct=c["translation"];
     row["truth_pose_errors"]={{"angle_deg",std::abs(c["angle_deg"].get<double>()-37)},
      {"scale_absolute",std::abs(c["scale"].get<double>()-1.3)},{"translation_px",std::abs(Z(ct[0],ct[1])-t)}};
    }
   }
   if(id!="repeated_boundary"&&j["observation"].contains("span")){
    const auto& points=j["observation"]["span"]["refinement"]["points"];double sum=0,max=0;size_t n=0;
    auto f=std::polar(1.3,37*std::acos(-1.)/180.),t=Z(67,64)-f*Z(44,45);
    for(auto& point:points)if(point["accepted"].get<bool>()){
     auto xy=point["refined"]["point"];auto p=(Z(xy[0],xy[1])-t)/f;double best=1e100,bx=0;
     for(int k=0;k<=1000;++k){double x=k*.1,d=std::norm(p-Z(x,curve(x)));if(d<best){best=d;bx=x;}}
     double lo=bx-.1,hi=bx+.1;
     for(int k=0;k<40;++k){double l=lo+(hi-lo)/3,h=hi-(hi-lo)/3;if(std::norm(p-Z(l,curve(l)))<std::norm(p-Z(h,curve(h))))hi=h;else lo=l;}
     double x=(lo+hi)*.5,e=std::abs(f)*std::abs(p-Z(x,curve(x)));sum+=e;max=std::max(max,e);++n;
    }
    row["truth_boundary_scoring_only"]={{"count",n},{"mean_px",n?J(sum/n):J(nullptr)},{"max_px",n?J(max):J(nullptr)}};
   }
   need(id!="repeated_boundary"||!row["accepted"].get<bool>(),"repeated boundary false accept");
   need(id!="clean_control"||row["accepted"].get<bool>()||row["identity_queries"].dump().find("BUDGET")!=std::string::npos,"clean control regression");
   write(dir/"summary.json",row.dump(2));matrix.push_back(row);
   std::cout<<id<<" "<<v.name<<" "<<j["status"]<<std::endl;
  }
 }
 write(out/"matrix.json",matrix.dump(2));std::cout<<"RESOLUTION_STUDY_PASS "<<matrix.size()<<" isolated runs; unchanged quality thresholds\n";
 return 0;
}catch(const mu::Parser::exception_type& e){std::cerr<<e.GetMsg()<<"\n";return 1;}
catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
