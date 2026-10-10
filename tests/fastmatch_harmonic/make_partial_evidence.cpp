#include "muParser.h"
#include "../../cximage/CxPartialMatchAudit.h"
#include "../../cximage/CxPartialMatchUiParameters.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <filesystem>
#include <iostream>
using J=nlohmann::json;
namespace fs=std::filesystem;
void write(const fs::path& path,const std::string& data){
 std::ofstream out(path,std::ios::binary);out<<data;out.close();
 if(!out)throw std::runtime_error("asset_write_failed");
}
struct Diagram {
 static constexpr int w=800,h=600;
 std::vector<unsigned char> pixels=std::vector<unsigned char>(w*h*3,245);
 void dot(double x,double y,int r,int red,int green,int blue){
  int cx=int(std::lround(x))+350,cy=300-int(std::lround(y));
  for(int dy=-r;dy<=r;++dy)for(int dx=-r;dx<=r;++dx){
   int px=cx+dx,py=cy+dy;
   if(dx*dx+dy*dy>r*r||px<0||py<0||px>=w||py>=h)continue;
   size_t i=(size_t(py)*w+px)*3;pixels[i]=red;pixels[i+1]=green;pixels[i+2]=blue;
  }
 }
 void save(const fs::path& path){write(path,"P6\n800 600\n255\n"+std::string(pixels.begin(),pixels.end()));}
};
int main(int argc,char** argv)try{
 if(argc!=2)throw std::runtime_error("usage: make_partial_evidence external_directory");
 fs::path root=fs::absolute(argv[1]);fs::create_directories(root);
 const double points[5][2]={{-40,-10},{10,-30},{60,20},{-20,70},{30,60}};
 for(bool budget:{false,true}){
  std::string id=budget?"partial_match_budget_stop":"partial_match_missing_extra";
  fs::path dir=root/id;fs::create_directories(dir);
  // Case scripts do not save to fixed paths. Application headless exports receipts.
  std::ostringstream script;
  script<<"PartialMatchAudit p;\np.requestid(\""<<id<<"\");\n";
  script<<"p.parameter("<<(budget?1:4096)<<",\"maximum_hypotheses\");\np.parameter(1,\"refine_before_capacity\");\n";
  Diagram source;
  J reference=J::array(),observation=J::array();
  for(int side=0;side<2;++side){
   script<<"p.select("<<side<<");p.source(\""<<(side?"controlled_observation":"controlled_reference")<<"\");\n";
   for(int i=side;i<5;++i){
    double x=side?-1.2*points[i][1]+17:points[i][0];
    double y=side?1.2*points[i][0]-8:points[i][1];
    std::string feature=(side?"t":"r")+std::to_string(i);
    script<<"p.featureid(\""<<feature<<"\");p.point("<<x<<","<<y<<");\n";
    (side?observation:reference).push_back({{"id",feature},{"x",x},{"y",y}});
    source.dot(x,y,side?4:6,side?35:170,side?100:170,side?220:170);
   }
   if(side){
    script<<"p.featureid(\"extra\");p.point(300,-200);\n";
    observation.push_back({{"id","extra"},{"x",300},{"y",-200}});
    source.dot(300,-200,4,35,100,220);
   }
  }
  script<<"p.run();\n";
  std::string text=script.str();cxpartialui::Expose(text);
  mu::Parser parser;double* org=nullptr;parser.DefineOrgClass("double",org);parser.UsingClass(true);
  RegisterPartialMatchAudit(parser);parser.SetExpr(text);parser.Eval();
  auto* p=static_cast<CxPartialMatchAudit*>(parser.GetClassObj("PartialMatchAudit","p"));
  if(!p)throw std::runtime_error("missing_runtime_object");
  J receipt=J::parse(p->receipt());
  if(receipt.at("candidates").size()!=(budget?0u:1u)||receipt.at("complete")!=!budget)
   throw std::runtime_error("unexpected_fixture_result");
  Diagram overlay=source;
  if(!budget){
   // Plot only actual correspondence endpoints; no closed contour or result box.
   for(const auto& pair:receipt.at("candidates").at(0).at("assessment").at("pairs"))
    for(const auto& point:observation)if(point.at("id")==pair.at("target_id"))
     overlay.dot(point.at("x"),point.at("y"),2,0,170,70);
  }
  source.save(dir/"source.ppm");overlay.save(dir/"correspondence.ppm");
  write(dir/"run.cxsc",text);
  write(dir/"partial_match_receipt.json",receipt.dump(2));
  write(dir/"typed_label.json",J({{"schema","controlled_point_set.v1"},{"image_derived",false},
    {"reference",reference},{"observation",observation},{"annotations",J::array()}}).dump(2));
  J summary={{"schema","partial_evidence_summary.v1"},{"status",receipt.at("status")},
    {"production_eligible",false},{"image_extraction",false},{"point_order_semantics","unordered"},
    {"reference",reference},{"observation",observation},
    {"legend","Gray=reference; blue=observation; green=verified correspondence endpoint. Synthetic diagram, not a detected boundary."},
    {"expected_pose",{{"angle_deg",90},{"scale",1.2},{"translation_x",17},{"translation_y",-8}}},
    {"missing_reference_id","r0"},{"extra_observation_id","extra"}};
  write(dir/"result_summary.json",summary.dump(2));
  J manifest={{"schema","cxvision.evidence_case.v1"},{"run_id","partial_match_audit_v1"},
   {"internal_case_id",id},{"review_item",budget?"Partial Match - Budget Stop":"Partial Match - Rotate Scale Missing Extra"},
   {"tool","PartialMatchAudit"},{"display_group","Partial Match / Controlled Point Sets"},
   {"display_category","To Verify"},{"case_role","controlled_point_set_audit"},
   {"geometry_type","unordered_point_set"},{"binding_status","REFERENCE_ONLY"},
   {"parameter_summary","Synthetic point diagram; gray=reference, blue=observation, green=matched. 21 script parameters. Not image extraction. Not production."},
   {"source_image","source.ppm"},{"typed_label","typed_label.json"},
   {"geometry_facts_ref","partial_match_receipt.json"},{"evidence_overlay","correspondence.ppm"},
   {"result_summary","result_summary.json"},{"script_snapshot","run.cxsc"},
   {"required_assets",{"source.ppm","typed_label.json","partial_match_receipt.json","correspondence.ppm","result_summary.json","run.cxsc"}}};
  write(dir/"case_manifest.json",manifest.dump(2));std::cout<<id<<" "<<receipt.at("status")<<"\n";
 }
 fs::create_directories(root/"_shared");
 write(root/"_shared"/"evidence_case_roots.json",J({{"schema","cxvision.evidence_case_roots.v1"},
  {"roots",{"partial_match_missing_extra","partial_match_budget_stop"}}}).dump(2));
 std::cout<<"PARTIAL_EVIDENCE_GENERATED\n";return 0;
}catch(const mu::Parser::exception_type& e){std::cerr<<e.GetMsg()<<"\n";return 1;}
catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
