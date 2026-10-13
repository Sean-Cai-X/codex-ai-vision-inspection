#include "../../cximage/CxPartialBoundaryMaskInput.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <functional>
#include <iostream>
using namespace cxgeom::polar;using namespace cxpartialimage;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
void reject(const std::function<void()>& f,const char* why){
 try{f();}catch(const std::invalid_argument& e){check(std::string(e.what())==why,e.what());return;}
 throw std::runtime_error("expected endpoint rejection");
}
int main()try{
 std::vector<std::complex<double>> p={{0,0},{3,0},{5,2},{4,5},{1,7},{-2,3}};
 BoundaryEndpointAnchors a{p[4],p[1],p[0],.1,1e-8,"operator-observed-points"};
 auto s=SelectBoundaryEndpoints(p,a);
 check(s.run.parent_indices==std::vector<size_t>({4,5,0,1})&&s.run.wraps_parent_origin,"wrap and order");
 ObservedBoundaryInput original("image","v1","c","s",p,s.run);
 std::swap(a.start,a.end);auto reversed=SelectBoundaryEndpoints(p,a);
 check(reversed.endpoints_swapped&&reversed.run.points==s.run.points,"same via selects same arc");
 check(ObservedBoundaryInput("image","v1","c","s",p,reversed.run).receipt()==original.receipt(),"stable IDs preserved");
 auto receipt=nlohmann::json::parse(reversed.receipt());
 check(receipt["closed"]==false&&receipt["synthetic_count"]==0,"open observed contract");
 for(double angle:{0.,15.,37.,90.,-110.})for(double scale:{.8,1.,1.5,2.}){
  auto transform=[&](std::complex<double> v){return std::polar(scale,angle*std::acos(-1.)/180.)*v+std::complex<double>(20,30);};
  auto q=p;for(auto& v:q)v=transform(v);
  auto b=a;b.start=transform(a.start);b.end=transform(a.end);b.via=transform(a.via);
  auto selected=SelectBoundaryEndpoints(q,b);
  check(selected.run.parent_indices==s.run.parent_indices,"rotation-scale index invariance");
  ObservedBoundaryInput validated("target","v1","c","s",q,selected.run);
 }
 auto bad=a;bad.end=bad.start;
 reject([&]{SelectBoundaryEndpoints(p,bad);},"BOUNDARY_ENDPOINT_COLLAPSED");
 bad=a;bad.via=bad.start;
 reject([&]{SelectBoundaryEndpoints(p,bad);},"BOUNDARY_ENDPOINT_VIA_AT_ENDPOINT");
 bad=a;bad.start={100,100};
 reject([&]{SelectBoundaryEndpoints(p,bad);},"BOUNDARY_ENDPOINT_OUT_OF_RANGE");
 bad=a;bad.start={1.5,0};bad.maximum_snap_px=2;
 reject([&]{SelectBoundaryEndpoints(p,bad);},"BOUNDARY_ENDPOINT_AMBIGUOUS_SNAP");
 try{SelectBoundaryEndpoints(p,bad);throw std::runtime_error("typed rejection missing");}
 catch(const BoundaryEndpointRejection& e){
  const auto detail=nlohmann::json::parse(e.receipt());
  check(detail["selected"]==false&&detail["role"]=="start"&&detail["candidates"].size()==2,"unselected competing endpoints");
  check(detail["candidates"][0]["parent_index"]==0&&detail["candidates"][1]["parent_index"]==1,"parent identity evidence");
 }
 bad=a;bad.provenance.clear();
 reject([&]{SelectBoundaryEndpoints(p,bad);},"BOUNDARY_ENDPOINT_MISSING_PROVENANCE");
 bad=a;bad.start=p[0];bad.end=p[5];bad.via=p[2];
 reject([&]{SelectBoundaryEndpoints(p,bad);},"BOUNDARY_ENDPOINT_WHOLE_CONTOUR");
 auto duplicate=p;duplicate[5]=p[1];
 reject([&]{SelectBoundaryEndpoints(duplicate,a);},"BOUNDARY_ENDPOINT_AMBIGUOUS_SNAP");
 cv::Mat mask=cv::Mat::zeros(32,32,CV_8UC1);
 cv::fillPoly(mask,std::vector<std::vector<cv::Point>>{{{5,5},{8,5},{8,6},{6,6},{6,8},{5,8}}},cv::Scalar(255));
 auto ref=FromBinaryMask(mask,{5,5,4,3},"mask","v1","s");
 const auto& run=ref.boundary.ordered_run();
 BoundaryEndpointAnchors anchors{run.points.front(),run.points.back(),run.points[run.points.size()/2],.1,1e-8,"explicit-observation"};
 reject([&]{FromBinaryMask(mask,{0,0,32,32},"mask","v1","s");},"SUBCURVE_WHOLE_CLOSED_CONTOUR");
 auto selected=FromBinaryMask(mask,{0,0,32,32},"mask","v1","s",128,3,&anchors);
 check(selected.boundary.receipt()==ref.boundary.receipt(),"full ROI plus endpoints preserves exact subcurve");
 check(nlohmann::json::parse(selected.receipt())["endpoint_selection"]["provenance"]=="explicit-observation","bridge evidence");
 std::cout<<"ENDPOINT_SELECTION_PASS 20 similarity transforms; wrap/reverse/tie/distance/provenance/whole-ROI bridge\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
