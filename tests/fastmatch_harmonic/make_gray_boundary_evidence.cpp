
#include "muParser.h"
#include "../../cximage/CxGrayBoundaryAudit.h"
#include <iostream>
using J=nlohmann::json;namespace fs=std::filesystem;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void write(const fs::path& path,const std::string& data){std::ofstream f(path);f<<data;f.close();check(bool(f),"write failed");}
J read(const fs::path& path){std::ifstream f(path);J j;f>>j;return j;}
int main(int argc,char** argv)try{
 check(argc>=2,"output path required");
 const fs::path root=fs::absolute(argv[1]);
 if(argc==3&&std::string(argv[2])=="verify-headless"){
  for(const auto* mode:{"straight","corner","no_refined"}){
   const auto receipt=read(root/"headless"/mode/"gray_boundary_receipt_0.json");
   const auto expected=read(root/mode/"gray_boundary_receipt.json");
   check(receipt==expected,"headless receipt differs from native script replay");
   check(fs::exists(root/"headless"/mode/"manual_algorithm_review_handoff.md"),"handoff missing");
   auto overlay=cv::imread((root/"headless"/mode/"result_overlay.png").string());
   check(!overlay.empty(),"headless overlay missing");
   size_t green=0;
   for(int y=0;y<overlay.rows;++y)for(int x=0;x<overlay.cols;++x){
    const auto p=overlay.at<cv::Vec3b>(y,x);green+=p[1]>p[0]+20&&p[1]>p[2]+20;
   }
   check((std::string(mode)=="no_refined")== (green==0),"refined overlay/status mismatch");
   if(std::string(mode)=="corner"){
    check(overlay.at<cv::Vec3b>(26,26)==cv::Vec3b(228,228,228),"open contour falsely closed across interior");
   }
  }
  std::cout<<"GRAY_HEADLESS_RECEIPTS_PASS\n";return 0;
 }
 fs::create_directories(root);
 const fs::path run_root=argc==3?fs::absolute(argv[2]):root;
#ifdef _WIN32
 _putenv_s("CXVISION_RUN_ROOT",run_root.string().c_str());
#else
 setenv("CXVISION_RUN_ROOT",run_root.string().c_str(),1);
#endif
 for(const auto* mode:{"straight","corner","no_refined"}){
  const std::string name(mode);const bool corner=name=="corner",negative=name=="no_refined";
  fs::path dir=root/name;fs::create_directories(dir);
  cv::Mat image(64,64,CV_8UC1);
  for(int y=0;y<64;++y)for(int x=0;x<64;++x){
   double value=128+100*std::erf((x-31.3)/1.6);
   if(corner)value=28+200*.5*(1+std::erf((32.-x)/1.6))*.5*(1+std::erf((y-20.)/1.6));
   image.at<unsigned char>(y,x)=cv::saturate_cast<unsigned char>(value);
  }
  check(cv::imwrite((dir/"source.pgm").string(),image),"source image write");
  J params={{"threshold",128},{"anchor_x",corner?19:29},{"anchor_y",corner?17:12},
   {"anchor_width",corner?17:6},{"anchor_height",corner?18:30},{"normal_half_window",3},
   {"normal_minimum_span_px",2},{"normal_maximum_turn_deg",30},{"scan_radius_px",4},
   {"scan_step_px",.25},{"minimum_contrast",negative?250:20},{"maximum_shift_px",2},
   {"monotonic_tolerance",2},{"noise_floor_dn",1}};
  std::ostringstream script;script<<"GrayBoundaryAudit g;\ng.caseid(\"gray_boundary_"<<name<<"\");\ng.revision(\"v1\");\n";
  script<<"g.image(\""<<fs::relative(dir/"source.pgm",run_root).generic_string()<<"\");\n";
  for(auto p=params.begin();p!=params.end();++p)script<<"g.parameter("<<p.value()<<",\""<<p.key()<<"\");\n";
  script<<"g.run();\n";
  mu::Parser parser;double* org=nullptr;parser.DefineOrgClass("double",org);parser.UsingClass(true);
  RegisterGrayBoundaryAudit(parser);parser.SetExpr(script.str());parser.Eval();
  auto* tool=static_cast<CxGrayBoundaryAudit*>(parser.GetClassObj("GrayBoundaryAudit","g"));
  check(tool!=nullptr,"parser object");J receipt=J::parse(tool->receipt());
  check(receipt.at("parameters")==params,"requested/executed parameter mismatch");
  if(negative)check(receipt.at("accepted_point_count")==0,"negative must have no accepted points");
  else check(receipt.at("accepted_point_count").get<size_t>()>0,"positive must refine points");
  if(corner)check(receipt.at("refined_spans").size()==2,"corner must split measured spans");
  check(receipt.at("production_eligible")==false,"development gate");
  check(cv::imwrite((dir/"overlay.png").string(),tool->overlay()),"overlay write");
  write(dir/"run.cxsc",script.str());write(dir/"gray_boundary_receipt.json",receipt.dump(2));
  write(dir/"typed_label.json",J({{"schema","controlled_gray_boundary_label.v1"},
   {"annotation_status","PROPOSED"},{"human_reviewed",false},{"training_eligible",false},
   {"source_kind","generated_blurred_step_fixture"},{"geometry_type","open_curve"}}).dump(2));
  write(dir/"result_summary.json",J({{"schema","gray_boundary_evidence_summary.v1"},{"status",receipt.at("status")},
   {"raw_points",receipt.at("raw_point_count")},{"accepted_points",receipt.at("accepted_point_count")},
   {"rejected_points",receipt.at("rejected_point_count")},{"legend",receipt.at("legend")},
   {"production_eligible",false}}).dump(2));
  write(dir/"case_manifest.json",J({{"schema","cxvision.evidence_case.v1"},{"run_id","gray_boundary_audit_v1"},
   {"internal_case_id","gray_boundary_"+name},{"review_item","Gray Boundary - "+name},{"tool","GrayBoundaryAudit"},
   {"display_group","Harmonic / Bound Gray Audit"},{"display_category","To Verify"},
   {"case_role","controlled_gray_boundary_audit"},{"geometry_type","open_curve"},{"binding_status","REFERENCE_ONLY"},
   {"parameter_summary","14 executable script parameters. Raw cyan; Refined green; rejected red. No forced closure; not production."},
   {"source_image","source.pgm"},{"typed_label","typed_label.json"},{"geometry_facts_ref","gray_boundary_receipt.json"},
   {"evidence_overlay","overlay.png"},{"result_summary","result_summary.json"},{"script_snapshot","run.cxsc"},
   {"required_assets",{"source.pgm","typed_label.json","gray_boundary_receipt.json","overlay.png","result_summary.json","run.cxsc"}}}).dump(2));
  // Every parameter mutation must invalidate prior execution, including a rejected mutation.
  tool->parameter(129,"threshold");bool stale=false;
  try{tool->receipt();}catch(const std::runtime_error&){stale=true;}check(stale,"stale receipt exposed");
  bool invalid=false;try{tool->parameter(.5,"anchor_width");}catch(const std::invalid_argument&){invalid=true;}
  check(invalid,"fractional integer accepted");
  std::cout<<name<<" raw="<<receipt.at("raw_point_count")<<" accepted="<<receipt.at("accepted_point_count")
   <<" rejected="<<receipt.at("rejected_point_count")<<" spans="<<receipt.at("refined_spans").size()<<"\n";
 }
 fs::create_directories(root/"_shared");
 write(root/"_shared"/"evidence_case_roots.json",J({{"schema","cxvision.evidence_case_roots.v1"},
  {"roots",{"straight","corner","no_refined"}}}).dump(2));
 std::cout<<"GRAY_CXSCRIPT_EVIDENCE_PASS\n";return 0;
}catch(const mu::Parser::exception_type& e){std::cerr<<e.GetMsg()<<"\n";return 1;}
catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
