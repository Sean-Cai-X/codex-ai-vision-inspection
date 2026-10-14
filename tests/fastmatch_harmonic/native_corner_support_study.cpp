#include "../../cximage/CxGrayBoundaryAudit.h"
#include "CxGeoCornerSupport.h"
#include <iostream>
using J=nlohmann::json;using Z=std::complex<double>;namespace fs=std::filesystem;
void need(bool b,const char*s){if(!b)throw std::runtime_error(s);}
J read(fs::path p){std::ifstream f(p);J j;f>>j;return j;}
void write(fs::path p,J j){std::ofstream f(p);f<<j.dump(2);need(bool(f),"write");}
int main(int argc,char**argv)try{
 need(argc==4,"full baseline / phase truth / fresh output");fs::path baseline=argv[1],truthroot=argv[2],root=argv[3];
 need(!fs::exists(root),"fresh output");fs::create_directories(root);
 J matrix=J::array(),reasons=J::object();size_t hypotheses=0,qualified=0,uniqueVertices=0,duplicates=0,paired=0;double errorSum=0,maxError=0,rawSum=0,pairedSum=0;
 for(auto row:read(baseline/"matrix.json")){
  std::string id=row["case_id"];auto original=read(baseline/id/"native.json");
  CxGrayBoundaryAudit tool;tool.caseid(id.c_str());tool.image((baseline/id/"source.pgm").c_str());
  for(auto p:original["parameters"].items())tool.parameter(p.value(),p.key().c_str());
  tool.run();need(J::parse(tool.receipt())==original,"real image replay mismatch");
  const auto& points=tool.result().points;std::vector<bool> reject;for(auto p:points)reject.push_back(!p.accepted);
  auto gaps=cxgeom::OrderedAcceptedBoundaryRuns(reject);J entries=J::array();
  // No truth is passed to the estimator; truth is read below only for posthoc scoring.
  for(auto gap:gaps.runs){
   size_t n=points.size();std::vector<Z> left,right;std::vector<size_t> li,ri;
   for(size_t k=1;k<=8;++k){size_t ix=(gap.front()+n-k)%n;if(!points[ix].accepted)break;li.push_back(ix);left.push_back(points[ix].refined);}
   for(size_t k=1;k<=8;++k){size_t ix=(gap.back()+k)%n;if(!points[ix].accepted)break;ri.push_back(ix);right.push_back(points[ix].refined);}
   bool corners=true;J gapReasons=J::object();for(auto ix:gap){auto status=points[ix].status;gapReasons[status]=gapReasons.value(status,0)+1;reasons[status]=reasons.value(status,0)+1;corners=corners&&status=="CORNER_OR_HIGH_CURVATURE";}
   std::vector<Z> rawLeft,rawRight;for(auto ix:li)rawLeft.push_back(points[ix].raw);for(auto ix:ri)rawRight.push_back(points[ix].raw);
   auto rawFit=cxgeom::EstimateCornerIntersection(rawLeft,rawRight);
   auto fit=cxgeom::EstimateCornerIntersection(left,right);if(!corners){fit.accepted=false;fit.status="NOT_A_PURE_TURN_REJECTION";}
   J e={{"rejected_parent_indices",gap},{"reasons",gapReasons},{"left_support_indices",li},{"right_support_indices",ri},
    {"status",fit.status},{"hypothesis_available",fit.accepted},{"original_rejections_preserved",true},
    {"cross_sine",fit.cross_sine},{"conditioning_gain_not_uncertainty",fit.conditioning_gain},
    {"left_rms_px",fit.left.rms},{"right_rms_px",fit.right.rms},{"left_span_px",fit.left.span},{"right_span_px",fit.right.span},
    {"left_extrapolation_px",fit.left_extrapolation},{"right_extrapolation_px",fit.right_extrapolation},{"point",nullptr},{"raw_same_support_status",rawFit.status},{"raw_same_support_point",nullptr}};
   if(rawFit.accepted)e["raw_same_support_point"]={rawFit.point.real(),rawFit.point.imag()};
   if(fit.accepted)e["point"]={fit.point.real(),fit.point.imag()};
   entries.push_back(e);++hypotheses;
  }
  std::string source=id.substr(5);source=source.substr(0,source.rfind("_r"));
  auto truth=read(truthroot/source/"provenance.json");std::vector<Z> vertices;for(auto p:truth["truth_polygon"])vertices.emplace_back(p[0],p[1]);
  std::set<size_t> seen;
  for(auto& e:entries)if(e["hypothesis_available"]==true){
   Z p(e["point"][0],e["point"][1]);double distance=1e100;size_t nearest=0;
   for(size_t i=0;i<vertices.size();++i)if(std::abs(p-vertices[i])<distance){distance=std::abs(p-vertices[i]);nearest=i;}
   e["posthoc_nearest_truth_vertex"]=nearest;e["posthoc_vertex_error_px"]=distance;
   if(!seen.insert(nearest).second)++duplicates;
   if(!e["raw_same_support_point"].is_null()){Z raw(e["raw_same_support_point"][0],e["raw_same_support_point"][1]);e["raw_same_support_vertex_error_px"]=std::abs(raw-vertices[nearest]);rawSum+=std::abs(raw-vertices[nearest]);pairedSum+=distance;++paired;}
   ++qualified;errorSum+=distance;maxError=std::max(maxError,distance);
  }
  uniqueVertices+=seen.size();
  auto out=root/id;fs::create_directories(out);
  cv::Mat overlay=tool.overlay();for(auto e:entries)if(e["hypothesis_available"]==true)
   cv::drawMarker(overlay,{cvRound(e["point"][0].get<double>()),cvRound(e["point"][1].get<double>())},{255,0,255},cv::MARKER_CROSS,9,1);
  need(cv::imwrite((out/"overlay.png").string(),overlay),"overlay");
  J result={{"schema","corner_support_diagnostic.v1"},{"case_id",id},{"gray_sha256",original["binding"]["gray_sha256"]},
   {"production_eligible",false},{"pose_evaluated",false},{"uncertainty_calibrated",false},{"runtime_receipt_unchanged",true},
   {"configuration",{{"support_points_per_side",8},{"minimum_points",5},{"minimum_span_px",4},{"maximum_rms_px",.15},{"minimum_cross_sine",.25},{"maximum_extrapolation_px",8}}},
   {"truth_use","posthoc_scoring_only_not_identity_confirmation"},{"gap_hypotheses",entries}};
  write(out/"diagnostic.json",result);matrix.push_back({{"case_id",id},{"gaps",entries.size()}});
 }
 write(root/"matrix.json",matrix);J summary={{"cases",matrix.size()},{"gaps",hypotheses},{"hypotheses",qualified},{"rejection_reasons",reasons},
  {"mean_vertex_error_px",qualified?J(errorSum/qualified):J(nullptr)},{"max_vertex_error_px",maxError},
  {"production_eligible",false},{"original_gaps_closed",0},{"truth_vertices_total",matrix.size()*8},{"unique_nearest_truth_vertices",uniqueVertices},{"duplicate_nearest_assignments",duplicates},
  {"paired_raw_refined_count",paired},{"paired_raw_error_mean_px",paired?J(rawSum/paired):J(nullptr)},{"paired_refined_error_mean_px",paired?J(pairedSum/paired):J(nullptr)}};
 write(root/"summary.json",summary);std::cout<<summary.dump(2)<<"\n";
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
