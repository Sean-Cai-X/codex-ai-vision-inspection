#pragma once
#include "observed_boundary_input.h"
#include <algorithm>
#include <limits>
namespace cxgeom::polar {
// Restricted to two ordered, endpoint-aligned observations of the SAME open span.
// Not partial-overlap discovery, gap completion, or automatic anchor localization.
struct ArcCorrespondenceConfig {
 size_t samples=32;
 double minimum_scale=.25,maximum_scale=4.,maximum_fit_rms_px=.3;
 double maximum_observed_error_px=.3,support_distance_px=.3,minimum_weighted_support=.9;
 double maximum_endpoint_error_px=.5;
 bool allow_reversed=false;
};
struct ArcSample {
 std::complex<double> point;
 size_t edge=0;
 double fraction=0,normalized_length=0;
};
struct ArcSamples {
 double length=0;
 std::vector<ArcSample> values;
};
inline ArcSamples SampleObservedArc(const ObservedBoundaryInput& input,size_t count){
 if(count<3||count>128)throw std::invalid_argument("ARC_SAMPLE_BUDGET");
 const auto& p=input.ordered_run().points;
 std::vector<double> lengths(p.size(),0);
 for(size_t i=1;i<p.size();++i)lengths[i]=lengths[i-1]+std::abs(p[i]-p[i-1]);
 if(!std::isfinite(lengths.back())||lengths.back()<=1e-9)throw std::invalid_argument("ARC_INVALID_LENGTH");
 ArcSamples out;out.length=lengths.back();
 for(size_t k=0;k<count;++k){
  double u=double(k)/double(count-1),distance=u*out.length;
  auto it=std::upper_bound(lengths.begin(),lengths.end(),distance);
  size_t edge=it==lengths.begin()?0:size_t(it-lengths.begin()-1);
  edge=std::min(edge,p.size()-2);
  double t=std::clamp((distance-lengths[edge])/(lengths[edge+1]-lengths[edge]),0.,1.);
  out.values.push_back({p[edge]+t*(p[edge+1]-p[edge]),edge,t,u});
 }
 return out;
}
struct ArcObservedAudit {
 double weighted_rms_px=0,maximum_error_px=0,weighted_vertex_support=0;
 bool monotonic=true;
};
inline ArcObservedAudit AuditObservedPolyline(const std::vector<std::complex<double>>& points,
 const std::vector<std::complex<double>>& target,double support_distance){
 ArcObservedAudit out;double total=0,squares=0,supported=0,previous=-1;
 std::vector<double> arc(target.size(),0);
 for(size_t i=1;i<target.size();++i)arc[i]=arc[i-1]+std::abs(target[i]-target[i-1]);
 for(size_t i=0;i<points.size();++i){
  double best=std::numeric_limits<double>::infinity(),position=0;
  for(size_t j=1;j<target.size();++j){
   auto edge=target[j]-target[j-1];double denominator=std::norm(edge);
   double t=std::clamp(std::real(std::conj(edge)*(points[i]-target[j-1]))/denominator,0.,1.);
   double d=std::abs(points[i]-(target[j-1]+t*edge));
   if(d<best){best=d;position=arc[j-1]+t*(arc[j]-arc[j-1]);}
  }
  if(position+1e-8<previous)out.monotonic=false;
  previous=position;
  double weight=0;
  if(i)weight+=.5*std::abs(points[i]-points[i-1]);
  if(i+1<points.size())weight+=.5*std::abs(points[i+1]-points[i]);
  total+=weight;squares+=weight*best*best;if(best<=support_distance)supported+=weight;
  out.maximum_error_px=std::max(out.maximum_error_px,best);
 }
 out.weighted_rms_px=std::sqrt(squares/total);out.weighted_vertex_support=supported/total;
 if(!std::isfinite(out.weighted_rms_px)||!std::isfinite(out.weighted_vertex_support)||!std::isfinite(out.maximum_error_px))
  throw std::invalid_argument("ARC_NONFINITE_AUDIT");
 return out;
}
struct ArcPoseAssessment {
 std::complex<double> factor{},translation{};
 double fit_rms_px=0,endpoint_error_px=0;
 ArcObservedAudit reference,observation;
 bool reversed=false,accepted=false,observed_audited=false;
 std::string reason;
};
struct ArcCorrespondenceResult {
 ArcCorrespondenceConfig config;
 ArcSamples reference_samples,observation_samples;
 std::string reference_receipt,observation_receipt,status;
 std::vector<ArcPoseAssessment> candidates;
 std::vector<size_t> accepted_indices;
 std::string receipt()const{
  std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17);
  auto b=[](bool v){return v?"true":"false";};
  o<<"{\"schema\":\"boundary_arc_correspondence.v1\",\"production_eligible\":false,\"raw_modified\":false";
  o<<",\"scope\":\"same_open_span_with_endpoint_alignment\",\"sampling\":\"derived_linear_arc_interpolation\"";
  o<<",\"coverage_semantics\":\"arc_length_weighted_observed_vertex_support_not_exact_continuous_coverage\"";
  o<<",\"status\":"<<receipt_detail::quote(status)<<",\"reference\":"<<reference_receipt<<",\"observation\":"<<observation_receipt;
  o<<",\"config\":{\"samples\":"<<config.samples<<",\"minimum_scale\":"<<config.minimum_scale<<",\"maximum_scale\":"<<config.maximum_scale;
  o<<",\"maximum_fit_rms_px\":"<<config.maximum_fit_rms_px<<",\"maximum_observed_error_px\":"<<config.maximum_observed_error_px;
  o<<",\"support_distance_px\":"<<config.support_distance_px<<",\"minimum_weighted_support\":"<<config.minimum_weighted_support;
  o<<",\"maximum_endpoint_error_px\":"<<config.maximum_endpoint_error_px<<",\"allow_reversed\":"<<b(config.allow_reversed)<<'}';
  auto samples=[&](const char* name,const ArcSamples& s){
   o<<",\""<<name<<"\":{\"length\":"<<s.length<<",\"samples\":[";
   for(size_t i=0;i<s.values.size();++i){
    if(i)o<<',';
    const auto& v=s.values[i];
    o<<"{\"derived\":true,\"observed\":false,\"source_run_edge\":"<<v.edge<<",\"fraction\":"<<v.fraction;
    o<<",\"normalized_length\":"<<v.normalized_length<<",\"point\":["<<v.point.real()<<','<<v.point.imag()<<"]}";
   }o<<"]}";
  };
  samples("reference_sampling",reference_samples);samples("observation_sampling",observation_samples);
  o<<",\"accepted_indices\":[";for(size_t i=0;i<accepted_indices.size();++i){if(i)o<<',';o<<accepted_indices[i];}o<<"],\"candidates\":[";
  auto audit=[&](const char* name,const ArcObservedAudit& a){
   o<<",\""<<name<<"\":{\"weighted_rms_px\":"<<a.weighted_rms_px<<",\"maximum_error_px\":"<<a.maximum_error_px;
   o<<",\"weighted_vertex_support\":"<<a.weighted_vertex_support<<",\"monotonic\":"<<b(a.monotonic)<<'}';
  };
  for(size_t i=0;i<candidates.size();++i){
   if(i)o<<',';
   const auto& c=candidates[i];
   o<<"{\"reversed\":"<<b(c.reversed)<<",\"accepted\":"<<b(c.accepted)<<",\"reason\":"<<receipt_detail::quote(c.reason);
   o<<",\"scale\":"<<std::abs(c.factor)<<",\"angle_deg\":"<<std::arg(c.factor)*180/std::acos(-1.);
   o<<",\"translation\":["<<c.translation.real()<<','<<c.translation.imag()<<"],\"fit_rms_px\":"<<c.fit_rms_px;
   o<<",\"endpoint_error_px\":"<<c.endpoint_error_px;
   if(c.observed_audited){audit("reference_observed",c.reference);audit("observation_observed",c.observation);}
   else o<<",\"reference_observed\":null,\"observation_observed\":null";
   o<<'}';
  }o<<"]}";return o.str();
 }
};
inline ArcCorrespondenceResult MatchObservedArc(const ObservedBoundaryInput& reference,
 const ObservedBoundaryInput& observation,const ArcCorrespondenceConfig& cfg={}){
 auto positive=[](double d){return std::isfinite(d)&&d>0;};
 if(cfg.samples<3||cfg.samples>128||!positive(cfg.minimum_scale)||!positive(cfg.maximum_scale)||
    cfg.maximum_scale<cfg.minimum_scale||!positive(cfg.maximum_fit_rms_px)||!positive(cfg.maximum_observed_error_px)||
    !positive(cfg.support_distance_px)||!positive(cfg.minimum_weighted_support)||cfg.minimum_weighted_support>1||
    !positive(cfg.maximum_endpoint_error_px))throw std::invalid_argument("ARC_INVALID_CONFIG");
 ArcCorrespondenceResult out;out.config=cfg;out.reference_receipt=reference.receipt();out.observation_receipt=observation.receipt();
 out.reference_samples=SampleObservedArc(reference,cfg.samples);out.observation_samples=SampleObservedArc(observation,cfg.samples);
 const auto& a=out.reference_samples.values;const auto& b=out.observation_samples.values;
 for(int reverse=0;reverse<=(cfg.allow_reversed?1:0);++reverse){
  ArcPoseAssessment c;c.reversed=reverse!=0;
  auto at=[&](size_t i){return b[reverse?b.size()-1-i:i].point;};
  std::complex<double> ma{},mb{};
  for(size_t i=0;i<a.size();++i){ma+=a[i].point;mb+=at(i);}ma/=double(a.size());mb/=double(a.size());
  double denominator=0;std::complex<double> numerator{};
  for(size_t i=0;i<a.size();++i){denominator+=std::norm(a[i].point-ma);numerator+=std::conj(a[i].point-ma)*(at(i)-mb);}
  if(!std::isfinite(denominator)||denominator<=1e-18)throw std::invalid_argument("ARC_DEGENERATE_FIT");
  c.factor=numerator/denominator;c.translation=mb-c.factor*ma;
  if(!std::isfinite(std::abs(c.factor))||!std::isfinite(std::abs(c.translation)))throw std::invalid_argument("ARC_NONFINITE_FIT");
  double sum=0;for(size_t i=0;i<a.size();++i)sum+=std::norm(c.factor*a[i].point+c.translation-at(i));
  c.fit_rms_px=std::sqrt(sum/double(a.size()));
  auto rp=reference.ordered_run().points,op=observation.ordered_run().points;
  for(auto& p:rp)p=c.factor*p+c.translation;
  if(reverse)std::reverse(op.begin(),op.end());
  c.endpoint_error_px=std::max(std::abs(rp.front()-op.front()),std::abs(rp.back()-op.back()));
  if(!std::isfinite(c.fit_rms_px)||!std::isfinite(c.endpoint_error_px))
   throw std::invalid_argument("ARC_NONFINITE_RESIDUAL");
  if(std::abs(c.factor)<cfg.minimum_scale||std::abs(c.factor)>cfg.maximum_scale)c.reason="ARC_SCALE_OUT_OF_RANGE";
  else{
   c.reference=AuditObservedPolyline(rp,op,cfg.support_distance_px);
   c.observation=AuditObservedPolyline(op,rp,cfg.support_distance_px);
   c.observed_audited=true;
   if(c.fit_rms_px>cfg.maximum_fit_rms_px)c.reason="ARC_FIT_RESIDUAL";
   else if(c.endpoint_error_px>cfg.maximum_endpoint_error_px)c.reason="ARC_ENDPOINT_MISMATCH";
   else if(!c.reference.monotonic||!c.observation.monotonic)c.reason="ARC_ORDER_MISMATCH";
   else if(c.reference.maximum_error_px>cfg.maximum_observed_error_px||c.observation.maximum_error_px>cfg.maximum_observed_error_px)c.reason="ARC_OBSERVED_RESIDUAL";
   else if(c.reference.weighted_vertex_support<cfg.minimum_weighted_support||c.observation.weighted_vertex_support<cfg.minimum_weighted_support)c.reason="ARC_INSUFFICIENT_WEIGHTED_SUPPORT";
   else{c.accepted=true;c.reason="ARC_OBSERVED_VERIFIED_NOT_PRODUCTION";}
  }
  if(c.accepted)out.accepted_indices.push_back(out.candidates.size());
  out.candidates.push_back(c);
 }
 out.status=out.accepted_indices.empty()?"NO_ARC_CANDIDATE":out.accepted_indices.size()==1?"UNIQUE_ARC_CANDIDATE":"ARC_AMBIGUOUS";
 return out;
}
}
