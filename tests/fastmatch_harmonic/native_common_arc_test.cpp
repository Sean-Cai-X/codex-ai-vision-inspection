#include "../../cxgeom/polar_discrete_harmonic/common_arc_support.h"
#include <iostream>
using namespace cxgeom::polar;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
template<class F> void rejects(F f,const char* why){
 bool caught=false;try{f();}catch(const std::invalid_argument& e){caught=std::string(e.what())==why;}check(caught,why);
}
int main()try{
 // Independent asymmetric geometry; these exact-coordinate checks do not
 // certify image extraction, automatic landmark detection or production fit.
 const std::vector<std::complex<double>> r={{0,0},{3,1},{5,5},{9,7},{10,12},{16,13},{18,11},{23,15}};
 const auto fractions=common_arc_detail::RawFractions(r);
 const auto factor=std::polar(1.7,.63);const std::complex<double> shift(12,-4);
 std::vector<std::complex<double>> o;for(auto p:r)o.push_back(factor*p+shift);
 RawArcAnchors a{0,1};
 const auto d=IntersectCommonArcDomain(a,a,{fractions[1],fractions[6]},{fractions[2],fractions[7]},.2);
 check(d.accepted()&&std::abs(d.begin-fractions[2])<1e-12&&std::abs(d.end-fractions[6])<1e-12,"intersection");
 const std::vector<std::complex<double>> rr(r.begin()+1,r.begin()+7),oo(o.begin()+2,o.end());
 auto rs=common_arc_detail::SampleCommonSlice(r,rr,1,6,a,d,32);
 auto os=common_arc_detail::SampleCommonSlice(o,oo,2,7,a,d,32);
 ArcCorrespondenceConfig cfg;
 auto result=arc_detail::MatchArcPoints(rs.refined,os.refined,"{}","{}",cfg,&rs.refined_samples,&os.refined_samples);
 check(result.accepted_indices.size()==1,"asymmetric common fit");
 check(std::abs(result.candidates[0].factor-factor)<1e-12&&std::abs(result.candidates[0].translation-shift)<1e-12,"no loss interpreted as scale");
 check(rs.refined.size()==5,"all interior vertices retained for audit");
 // Explicit reversed endpoint correspondence, not a silently reversed contour.
 auto reversed=o;std::reverse(reversed.begin(),reversed.end());RawArcAnchors reverse{1,0};
 auto revslice=common_arc_detail::SampleCommonSlice(reversed,reversed,0,7,reverse,d,32);
 result=arc_detail::MatchArcPoints(rs.refined,revslice.refined,"{}","{}",cfg,&rs.refined_samples,&revslice.refined_samples);
 check(std::abs(result.candidates[0].factor-factor)<1e-12,"reverse anchor coordinates");
 // Extra leading/trailing pieces: caller supplies interior landmarks rather
 // than pretending the full observation endpoints correspond.
 auto extra=o;extra.insert(extra.begin(),o.front()-std::complex<double>(3,6));
 extra.push_back(o.back()+std::complex<double>(4,2));
 auto ef=common_arc_detail::RawFractions(extra);RawArcAnchors ea{ef[1],ef[8]};
 auto exslice=common_arc_detail::SampleCommonSlice(extra,extra,0,9,ea,d,32);
 result=arc_detail::MatchArcPoints(rs.refined,exslice.refined,"{}","{}",cfg,&rs.refined_samples,&exslice.refined_samples);
 check(std::abs(result.candidates[0].factor-factor)<1e-12,"extra pieces not scale");
 // Truncated raw observation, with corresponding interior reference anchors.
 std::vector<std::complex<double>> partial(o.begin()+2,o.begin()+7);
 RawArcAnchors ra{fractions[2],fractions[6]};
 auto pd=IntersectCommonArcDomain(ra,a,{fractions[1],fractions[6]},{0,1},.2);
 auto ps=common_arc_detail::SampleCommonSlice(partial,partial,0,4,a,pd,32);
 auto pr=common_arc_detail::SampleCommonSlice(r,rr,1,6,ra,pd,32);
 result=arc_detail::MatchArcPoints(pr.refined,ps.refined,"{}","{}",cfg,&pr.refined_samples,&ps.refined_samples);
 check(std::abs(result.candidates[0].factor-factor)<1e-12,"missing raw endpoints not scale");
 check(IntersectCommonArcDomain(a,a,{0,.2},{.5,.8},.1).status=="COMMON_ARC_EMPTY_INTERSECTION","empty rejected");
 check(IntersectCommonArcDomain(a,a,{0,.5},{.4,1},.2).status=="COMMON_ARC_INSUFFICIENT_COVERAGE","short rejected");
 rejects([&]{IntersectCommonArcDomain({0,0},a,{0,1},{0,1},.2);},"COMMON_ARC_INVALID_ANCHORS");
 rejects([&]{IntersectCommonArcDomain(a,a,{0,1},{0,1},0);},"COMMON_ARC_INVALID_MINIMUM_FRACTION");
 rejects([&]{IntersectCommonArcDomain(a,a,{-1,1},{0,1},.2);},"COMMON_ARC_INVALID_SUPPORT");
 rejects([&]{common_arc_detail::SampleCommonSlice(r,rr,1,6,a,{0,1,"COMMON_ARC_DOMAIN_AVAILABLE"},32);},"COMMON_ARC_CROSSES_REJECTED_GAP");
 rejects([&]{common_arc_detail::SampleCommonSlice(r,rr,1,6,a,{0,1,"COMMON_ARC_EMPTY_INTERSECTION"},32);},"COMMON_ARC_DOMAIN_REQUIRED");
 // Equal retained interval does NOT prove correct semantic landmark matching.
 std::cout<<"COMMON_ARC_SUPPORT_PASS asymmetric rotation+scale / unequal retained spans / reversed / extra / missing / empty / insufficient / gap guards; explicit anchors, not automatic correspondence\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
