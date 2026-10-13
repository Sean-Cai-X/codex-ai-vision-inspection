#include "../../cximage/CxPartialBoundaryMaskInput.h"
#include "../../cxgeom/polar_discrete_harmonic/boundary_order_audit.h"
#include "../../cxgeom/polar_discrete_harmonic/partial_pipeline_receipt.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <functional>
using namespace cxgeom::polar;using namespace cxpartialimage;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void reject(const std::function<void()>& f,const char* why){
 try{f();}catch(const std::invalid_argument& e){check(std::string(e.what())==why,e.what());return;}
 throw std::runtime_error("expected mask rejection");
}
int main(int argc,char** argv)try{
 check(argc==2,"output directory required");std::filesystem::path out(argv[1]);std::filesystem::create_directories(out);
 cv::Mat a=cv::Mat::zeros(32,32,CV_8UC1);
 std::vector<std::vector<cv::Point>> shape={{{5,5},{8,5},{8,6},{6,6},{6,8},{5,8}}};
 cv::fillPoly(a,shape,cv::Scalar(255));
 check(cv::imwrite((out/"reference.pgm").string(),a),"save raster");
 auto decoded=cv::imread((out/"reference.pgm").string(),cv::IMREAD_GRAYSCALE);
 cv::Rect anchor(5,5,4,3);auto ref=FromBinaryMask(decoded,anchor,"controlled-reference","r1","anchor");
 auto j=nlohmann::json::parse(ref.receipt());
 check(j["mask_sha256"].get<std::string>().size()==64&&j["boundary"]["closed"]==false,"pixel provenance");
 // Non-contiguous storage yields identical digest, since hashing uses logical rows.
 cv::Mat storage=cv::Mat::zeros(34,34,CV_8UC1);a.copyTo(storage(cv::Rect(1,1,32,32)));
 check(FromBinaryMask(storage(cv::Rect(1,1,32,32)),anchor,"controlled-reference","r1","anchor").mask_sha256==ref.mask_sha256,"stride-independent hash");
 for(int mode=0;mode<2;++mode){
  cv::Mat b;cv::Rect target_anchor;double angle;std::complex<double> translation;
  if(mode==0){
   b=cv::Mat::zeros(32,32,CV_8UC1);a(cv::Rect(0,0,20,20)).copyTo(b(cv::Rect(10,7,20,20)));
   target_anchor=cv::Rect(15,12,4,3);angle=0;translation={10,7};
  }else{cv::rotate(a,b,cv::ROTATE_90_CLOCKWISE);target_anchor=cv::Rect(24,5,3,4);angle=90;translation={31,0};}
  auto file=out/("target_"+std::to_string(mode)+".pgm");check(cv::imwrite(file.string(),b),"save target");
  auto obs=FromBinaryMask(cv::imread(file.string(),cv::IMREAD_GRAYSCALE),target_anchor,"controlled-target","r1","anchor");
  ObservedBoundaryBundle rb({ref.boundary}),ob({obs.boundary});
  PartialRequest r;r.request_id="raster-"+std::to_string(mode);r.reference_source=ref.boundary.source_id();r.observation_source=obs.boundary.source_id();
  r.reference=rb.features();r.observation=ob.features();r.config.maximum_hypotheses=65536;r.config.maximum_pair_checks=10000000;
  r.config.max_residual_px=1e-7;r.config.minimum_reference_coverage=1;r.config.minimum_observation_coverage=1;
  auto result=SearchRefinedPartial(r);
  check(result.complete&&result.candidates.size()==1,"unique raster candidate");
  const auto& candidate=result.candidates.front();
  check(std::abs(candidate.supplied.angle_deg-angle)<1e-7&&std::abs(candidate.supplied.scale-1)<1e-8,"raster pose");
  check(std::abs(candidate.supplied.translation-translation)<1e-8,"raster translation");
  auto audit=AuditBoundaryOrder(rb,ob,candidate.pairs);check(audit.order_consistent,"raster order");
  nlohmann::json evidence={{"schema","controlled_mask_partial_test.v1"},{"fixture_kind","generated_binary_raster_not_business_image"},
   {"reference",nlohmann::json::parse(ref.receipt())},{"observation",nlohmann::json::parse(obs.receipt())},
   {"pose",nlohmann::json::parse(PartialPipelineReceiptV1(result))},{"order",nlohmann::json::parse(BoundaryOrderReceipt(audit))}};
  std::ofstream f(out/("receipt_"+std::to_string(mode)+".json"));f<<evidence.dump(2);f.close();check(bool(f),"receipt write");
 }
 auto bad=a.clone();bad.at<unsigned char>(0,0)=42;
 reject([&]{FromBinaryMask(bad,anchor,"r","v","s");},"MASK_BOUNDARY_NOT_BINARY");
 reject([&]{FromBinaryMask(a,cv::Rect(31,31,2,2),"r","v","s");},"MASK_BOUNDARY_INVALID_ANCHOR");
 reject([&]{FromBinaryMask(a,cv::Rect(0,0,2,2),"r","v","s");},"MASK_BOUNDARY_NO_ANCHORED_CONTOUR");
 auto two=a.clone();cv::rectangle(two,cv::Rect(12,5,3,3),cv::Scalar(255),cv::FILLED);
 reject([&]{FromBinaryMask(two,cv::Rect(4,4,13,6),"r","v","s");},"MASK_BOUNDARY_AMBIGUOUS_CONTOURS");
 std::cout<<"MASK_BOUNDARY_PASS decoded_pixels/translation/rotate90/hash/order/refusals; controlled_raster_only\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
