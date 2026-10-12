#include "../../cxgeom/polar_discrete_harmonic/boundary_candidate_gate.h"
#include "../../cximage/CxPartialBoundaryMaskInput.h"
#include "../../cxgeom/polar_discrete_harmonic/boundary_order_audit.h"
#include "../../cxgeom/polar_discrete_harmonic/partial_pipeline_receipt.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace cxpartialimage;using namespace cxgeom::polar;using Json=nlohmann::json;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void save(const std::filesystem::path& p,const Json& j){std::ofstream f(p);f<<j.dump(2);f.close();check(bool(f),"write evidence");}
struct Case {const char* id;double angle,scale;int defect;};
int main(int argc,char** argv)try{
 check(argc==2,"output required");const std::filesystem::path out(argv[1]);std::filesystem::create_directories(out);
 cv::Mat base=cv::Mat::zeros(32,32,CV_8UC1);
 cv::fillPoly(base,std::vector<std::vector<cv::Point>>{{{5,5},{8,5},{8,6},{6,6},{6,8},{5,8}}},cv::Scalar(255));
 check(cv::imwrite((out/"reference.pgm").string(),base),"reference image");
 auto ref=FromBinaryMask(cv::imread((out/"reference.pgm").string(),cv::IMREAD_GRAYSCALE),{5,5,4,3},"matrix-reference","v1","anchor");
 ObservedBoundaryBundle rb({ref.boundary});
 const Case cases[]={{"translate",0,1,0},{"rotate15",15,1,0},{"rotate37",37,1,0},
 {"rotate90",90,1,0},{"scale080",0,.8,0},{"scale150",0,1.5,0},{"scale200",0,2,0},
 {"combined37_150",37,1.5,0},{"missing_pixel",0,1,1},{"extra_outside",0,1,2},{"extra_inside",0,1,3}};
 Json summary={{"schema","controlled_mask_matrix.v1"},{"fixture_kind","tiny_generated_binary_raster_not_business_images"},
 {"anchor_basis","ground_truth_transformed_roi_bounding_box_not_a_detector"},
 {"interpolation","nearest"},{"quality_limits",{{"angle_deg",1},{"scale_absolute",.05},{"translation_px",.5}}},
 {"production_eligible",false},{"cases",Json::array()}};
 size_t accepted=0;
 for(const auto& c:cases){
  const double rad=c.angle*std::acos(-1.)/180.,co=c.scale*std::cos(rad),si=c.scale*std::sin(rad);
  const double tx=16-co*6+si*6,ty=16-si*6-co*6;
  cv::Mat matrix=(cv::Mat_<double>(2,3)<<co,-si,tx,si,co,ty),mask;
  cv::warpAffine(base,mask,matrix,{40,40},cv::INTER_NEAREST,cv::BORDER_CONSTANT,cv::Scalar(0));
  std::vector<cv::Point2d> corners={{5,5},{9,5},{9,8},{5,8}};
  double x0=1e9,y0=1e9,x1=-1e9,y1=-1e9;
  for(auto p:corners){double x=co*p.x-si*p.y+tx,y=si*p.x+co*p.y+ty;x0=std::min(x0,x);x1=std::max(x1,x);y0=std::min(y0,y);y1=std::max(y1,y);}
  cv::Rect anchor(int(std::floor(x0)),int(std::floor(y0)),int(std::ceil(x1)-std::floor(x0)),int(std::ceil(y1)-std::floor(y0)));
  if(c.defect==1){check(mask.at<unsigned char>(15,16)==255,"missing pixel must exist");mask.at<unsigned char>(15,16)=0;}
  if(c.defect==2)cv::rectangle(mask,{30,30,3,3},cv::Scalar(255),cv::FILLED);
  if(c.defect==3){cv::rectangle(mask,{22,14,3,3},cv::Scalar(255),cv::FILLED);anchor.width=25-anchor.x;}
  const auto path=out/(std::string(c.id)+".pgm");check(cv::imwrite(path.string(),mask),"target image");
  Json entry={{"id",c.id},{"expected_pose",{{"angle_deg",c.angle},{"scale",c.scale},{"tx",tx},{"ty",ty}}},
   {"quality_pass",false},{"reference",Json::parse(ref.receipt())},{"candidates",Json::array()}};
  try{
  entry["order_gate_quality_pass"]=false;entry["order_gate_status"]="NOT_RUN";
   auto obs=FromBinaryMask(cv::imread(path.string(),cv::IMREAD_GRAYSCALE),anchor,std::string("matrix-")+c.id,"v1","anchor");
   entry["observation"]=Json::parse(obs.receipt());ObservedBoundaryBundle ob({obs.boundary});
   PartialRequest r;r.request_id=c.id;r.reference_source=ref.boundary.source_id();r.observation_source=obs.boundary.source_id();
   r.reference=rb.features();r.observation=ob.features();
   // One fixed operating configuration for ALL cases; no case-specific threshold search.
   r.config.maximum_hypotheses=65536;r.config.maximum_pair_checks=10000000;
   r.config.max_residual_px=.3;r.config.minimum_reference_coverage=.5;r.config.minimum_observation_coverage=.5;
   const auto result=SearchRefinedPartial(r);
   const auto raw=PartialPipelineReceiptV1(result);
   check(raw==PartialPipelineReceiptV1(SearchRefinedPartial(r)),"deterministic replay");
   entry["pose"]=Json::parse(raw);entry["status"]=result.status;
   BoundaryCandidateGate gate(rb,ob,result);
   entry["order_gate"]=Json::parse(gate.receipt());entry["order_gate_status"]=gate.status();
   if(std::string(c.id)=="translate"){
    check(result.candidates.size()>1&&gate.status()=="UNIQUE_ORDER_CANDIDATE","strict gate translation");
    BoundaryOrderPolicy loose;loose.allow_reversed=true;loose.allow_gaps=true;loose.require_reference_endpoints=false;
    BoundaryCandidateGate ambiguity(rb,ob,result,loose);
    check(ambiguity.status()=="ORDER_AMBIGUOUS"&&ambiguity.eligible_indices().size()>1,"retain order ambiguity");
    entry["relaxed_policy_diagnostic"]=Json::parse(ambiguity.receipt());
   }
   // Truth is consulted below only to evaluate the already-computed gate.
   check(result.complete||result.candidates.empty(),"incomplete exposes candidate");
   bool unique_good=false;
   for(const auto& candidate:result.candidates){
    double ae=std::abs(std::remainder(candidate.supplied.angle_deg-c.angle,360.));
    double se=std::abs(candidate.supplied.scale-c.scale),te=std::abs(candidate.supplied.translation-std::complex<double>(tx,ty));
    auto audit=AuditBoundaryOrder(rb,ob,candidate.pairs);
    const bool good=ae<=1&&se<=.05&&te<=.5&&audit.order_consistent;
    entry["candidates"].push_back({{"angle_error_deg",ae},{"scale_error",se},{"translation_error_px",te},
      {"within_limits",good},{"order",Json::parse(BoundaryOrderReceipt(audit))}});
    unique_good=good;
   }
   entry["quality_pass"]=result.complete&&result.candidates.size()==1&&unique_good;
   if(gate.eligible_indices().size()==1)
    entry["order_gate_quality_pass"]=entry["candidates"][gate.eligible_indices()[0]]["within_limits"];
   check(raw==PartialPipelineReceiptV1(result),"gate altered raw");
   entry["quality_reason"]=entry["quality_pass"].get<bool>()?"UNIQUE_POSE_AND_ORDER_WITHIN_LIMITS":
    result.candidates.empty()?"NO_CONSUMABLE_CANDIDATE":result.candidates.size()>1?"AMBIGUOUS_CANDIDATES":"POSE_OR_ORDER_OUTSIDE_LIMITS";
  }catch(const std::invalid_argument& e){
   entry["status"]="INPUT_REJECTED";entry["quality_reason"]=e.what();
   check(std::string(e.what()).find("SUBCURVE_")!=std::string::npos||
    std::string(e.what()).find("MASK_BOUNDARY_")==0||std::string(e.what()).find("BOUNDARY_")==0,"unexpected validation failure");
  }
  if(std::string(c.id)=="translate"){
   check(entry["status"]!="INPUT_REJECTED","translation control must reach solver");
   bool truth_present=false;for(const auto& x:entry["candidates"])truth_present=truth_present||x["within_limits"].get<bool>();
   check(truth_present,"translation control lost correct pose");
  }
  if(entry["quality_pass"].get<bool>())++accepted;
  save(out/(std::string(c.id)+".json"),entry);
  summary["cases"].push_back({{"id",c.id},{"status",entry["status"]},{"quality_pass",entry["quality_pass"]},{"reason",entry["quality_reason"]}});
  summary["cases"].back()["order_gate_status"]=entry["order_gate_status"];
  summary["cases"].back()["order_gate_quality_pass"]=entry["order_gate_quality_pass"];
  std::cout<<c.id<<": "<<entry["status"]<<" quality="<<entry["quality_pass"]<<" reason="<<entry["quality_reason"]<<'\n';
 }
 summary["quality_pass_count"]=accepted;summary["case_count"]=std::size(cases);
 summary["test_pass_means"]="execution_and_evidence_contract_not_all_case_quality";
 size_t gate_passed=0;
 for(const auto& item:summary["cases"])if(item["order_gate_quality_pass"].get<bool>())++gate_passed;
 summary["order_gate_quality_pass_count"]=gate_passed;
 save(out/"matrix.json",summary);
 check(summary["cases"][10]["status"]=="INPUT_REJECTED","ambiguous distractor must reject");
 std::cout<<"MASK_MATRIX_CONTRACT_PASS quality="<<accepted<<"/"<<std::size(cases)<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
