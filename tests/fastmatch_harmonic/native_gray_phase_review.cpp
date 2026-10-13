#include "../../cximage/CxGrayBoundaryAudit.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
using J=nlohmann::json;namespace fs=std::filesystem;
void need(bool b,const char*s){if(!b)throw std::runtime_error(s);}
J read(fs::path p){std::ifstream f(p);J j;f>>j;return j;}
void write(fs::path p,std::string s){std::ofstream f(p);f<<s;need(bool(f),"write");}
cv::Point pt(const J&p){return {cvRound(p[0].get<double>()*16),cvRound(p[1].get<double>()*16)};}
int main(int argc,char**argv)try{
 need(argc==3,"source matrix / fresh review output");fs::path source=fs::absolute(argv[1]),out=fs::absolute(argv[2]);need(!fs::exists(out),"fresh output");fs::create_directories(out);J summary=J::array();
 auto matrix=read(source/"matrix.json");
 for(auto row:matrix){
  std::string id=row["case"];int iterations=row["recenter_iterations"];std::string name=id+"_r"+std::to_string(iterations);
  auto dir=out/name;fs::create_directories(dir);auto imagePath=source/id/"source.pgm";auto image=cv::imread(imagePath.string());need(!image.empty(),"image");
  auto evidence=read(source/id/("recenter"+std::to_string(iterations)+".json"));J samples=J::array();
  for(auto chunk:evidence["chunks"]){auto points=chunk["points"];for(size_t i=3;i+3<points.size();++i)samples.push_back(points[i]);}
  need(samples.size()==row["raw_point_count"].get<size_t>(),"coverage");
  for(size_t i=0;i<samples.size();++i){
   const auto&a=samples[i];const auto&b=samples[(i+1)%samples.size()];
   cv::line(image,pt(a["raw"]),pt(b["raw"]),{255,255,0},1,cv::LINE_AA,4);
  }
  // Never join Refined across a rejected point, including the cyclic seam.
  for(size_t i=0;i<samples.size();++i){const auto&a=samples[i];const auto&b=samples[(i+1)%samples.size()];
   if(a["accepted"].get<bool>()&&b["accepted"].get<bool>())cv::line(image,pt(a["refined"]["point"]),pt(b["refined"]["point"]),{0,220,0},1,cv::LINE_AA,4);
   if(!a["accepted"].get<bool>())cv::circle(image,pt(a["raw"]),16,{0,0,255},1,cv::LINE_AA,4);
  }
  need(cv::imwrite((dir/"overlay.png").string(),image),"overlay");
  // Interior anchor surrounding the actually extracted object, not the image border.
  std::vector<cv::Point> raw;for(auto p:samples)raw.emplace_back(cvRound(p["raw"][0].get<double>()),cvRound(p["raw"][1].get<double>()));
  auto box=cv::boundingRect(raw);box.x-=2;box.y-=2;box.width+=4;box.height+=4;
  CxGrayBoundaryAudit tool;tool.caseid(name.c_str());tool.image(imagePath.c_str());tool.parameter(box.x,"anchor_x");tool.parameter(box.y,"anchor_y");tool.parameter(box.width,"anchor_width");tool.parameter(box.height,"anchor_height");tool.parameter(iterations,"recenter_iterations");
  bool rejected=false;std::string reason;J failure;
  try{tool.run();}catch(const cxpartialimage::MaskBoundaryRejection&e){rejected=true;reason=e.what();failure=J::parse(e.receipt());}
  need(rejected&&reason=="MASK_BOUNDARY_NO_ANCHORED_CONTOUR","expected polarity rejection not reproduced");
  need(failure["outer_count"]==1&&failure["hole_count"]==1&&failure["candidate_count"]==0,"unexpected topology");
  std::ostringstream script;script<<"GrayBoundaryAudit g;\ng.caseid(\""<<name<<"\");\ng.image(\""<<imagePath.string()<<"\");\n";
  for(auto p:std::vector<std::pair<const char*,int>>{{"anchor_x",box.x},{"anchor_y",box.y},{"anchor_width",box.width},{"anchor_height",box.height},{"recenter_iterations",iterations}})script<<"g.parameter("<<p.second<<",\""<<p.first<<"\");\n";
  script<<"// Expected current interface rejection: fixed bright foreground sees object as a hole.\ng.run();\n";
  write(dir/"run.cxsc",script.str());
  J record={{"schema","gray_phase_entry_review.v1"},{"case_id",name},{"production_eligible",false},{"native_scoring",row},
   {"native_threshold_mode","DARK_LE_128"},{"application_threshold_mode","BRIGHT_GT_128"},{"application_status",reason},{"application_rejection",failure},
   {"legend","Native diagnostic: Raw cyan, accepted Refined green, rejected red. Not application output; no pose or completion."},{"source_image",imagePath.string()}};
  write(dir/"result_summary.json",record.dump(2));write(dir/"input_attempt.json",tool.inputreceipt().dump(2));summary.push_back(record);
  if(id=="case_0"||id=="case_24"){
   auto label=J({{"annotation_status","PROPOSED"},{"human_reviewed",false},{"training_eligible",false},{"source_kind","independent_synthetic_gray_polygon"},{"geometry_type","polygon"}});
   write(dir/"typed_label.json",label.dump(2));
   auto manifest=J({{"schema","cxvision.evidence_case.v1"},{"run_id","gray_phase_entry_review_20261013"},{"internal_case_id","gray_phase_"+name},
    {"review_item","Gray Phase Boundary - "+id+" recenter"+std::to_string(iterations)+" - entry blocked"},
    {"tool","GrayBoundaryAudit"},{"display_group","Harmonic / Bound Gray Audit"},{"display_category","To Verify"},{"case_role","polarity_gap_diagnostic"},
    {"geometry_type","polygon"},{"binding_status","REFERENCE_ONLY"},{"parameter_summary","Native dark-boundary overlay. Run reproduces BRIGHT_GT threshold entry rejection; NOT successful runtime refinement."},
    {"source_image",imagePath.string()},{"typed_label","typed_label.json"},{"geometry_facts_ref","result_summary.json"},{"evidence_overlay","overlay.png"},{"result_summary","result_summary.json"},{"script_snapshot","run.cxsc"},
    {"required_assets",{imagePath.string(),"typed_label.json","result_summary.json","overlay.png","run.cxsc"}}});
   write(dir/"case_manifest.json",manifest.dump(2));
  }
 }
 write(out/"matrix.json",summary.dump(2));std::cout<<"GRAY_PHASE_ENTRY_REVIEW_PASS cases="<<summary.size()<<" expected_polarity_rejections="<<summary.size()<<"\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
