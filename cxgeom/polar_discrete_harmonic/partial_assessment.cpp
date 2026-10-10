#include "partial_assessment.h"
#include <algorithm>
#include <functional>
namespace cxgeom::polar {
PartialAssessment AssessHypothesis(const PartialRequest& r,const Hypothesis& h){
 PartialAssessment out;out.request_id=r.request_id;out.supplied=h;out.executed=r.config;
 auto valid=ValidatePartial(r);
 if(!valid.ready_for_solver){out.status="INVALID_REQUEST";out.reason=valid.reason;return out;}
 if(h.source_id.empty()||!std::isfinite(h.angle_deg)||!std::isfinite(h.scale)||h.scale<=0||
    !std::isfinite(h.translation.real())||!std::isfinite(h.translation.imag())){
  out.status="INVALID_HYPOTHESIS";out.reason="nonfinite_or_missing_provenance";return out;
 }
 auto a=Encode(r.reference_source,r.reference,r.config.encoding).features;
 auto b=Encode(r.observation_source,r.observation,r.config.encoding).features;
 size_t n=a.size(),m=b.size();
 auto rot=std::polar(h.scale,std::remainder(h.angle_deg,360.)*3.14159265358979323846/180);
 std::vector<std::vector<size_t>> edges(n);std::vector<size_t> degree(m);
 std::vector<std::vector<double>> residual(n,std::vector<double>(m));
 for(size_t i=0;i<n;++i)for(size_t j=0;j<m;++j){
  if(out.pair_checks>=r.config.maximum_pair_checks){
   out.status="PAIR_BUDGET_EXHAUSTED";out.reason="assessment_incomplete_no_metrics";return out;
  }
  ++out.pair_checks;
  double e=std::abs(rot*a[i].point+h.translation-b[j].point);
  if(!std::isfinite(e)){out.status="INVALID_HYPOTHESIS";out.reason="transform_overflow";return out;}
  residual[i][j]=e;
  if(e<=r.config.max_residual_px){edges[i].push_back(j);++degree[j];}
 }
 std::vector<std::vector<double>> spans(n,std::vector<double>(n));
 for(size_t i=0;i<n;++i)for(size_t j=i+1;j<n;++j){
  if(out.pair_checks>=r.config.maximum_pair_checks){
   out.status="PAIR_BUDGET_EXHAUSTED";out.reason="span_budget_no_metrics";return out;
  }
  ++out.pair_checks;spans[i][j]=std::abs(a[i].point-a[j].point);
 }
 std::vector<int> owner(m,-1);
 std::function<bool(size_t,std::vector<bool>&)> augment=[&](size_t i,std::vector<bool>& seen){
  for(size_t j:edges[i])if(!seen[j]){
   seen[j]=true;
   if(owner[j]<0||augment(size_t(owner[j]),seen)){owner[j]=int(i);return true;}
  }return false;
 };
 for(size_t i=0;i<n;++i){std::vector<bool> seen(m,false);augment(i,seen);}
 std::vector<bool> matched(n,false);double square=0,maximum=0;bool ambiguous=false;
 for(size_t j=0;j<m;++j){
  if(owner[j]<0){out.extra_observation_ids.push_back(b[j].stable_id);continue;}
  size_t i=size_t(owner[j]);matched[i]=true;double e=residual[i][j];
  out.pairs.push_back({a[i].stable_id,b[j].stable_id,e});
  square+=e*e;maximum=std::max(maximum,e);
  ambiguous=ambiguous||edges[i].size()>1||degree[j]>1;
 }
 for(size_t i=0;i<n;++i)if(!matched[i])out.missing_reference_ids.push_back(a[i].stable_id);
 std::sort(out.pairs.begin(),out.pairs.end(),[](const Pair& x,const Pair& y){return x.reference_id<y.reference_id;});
 double fullSpan=0,matchedSpan=0;
 for(size_t i=0;i<n;++i)for(size_t j=i+1;j<n;++j){
  double d=spans[i][j];fullSpan=std::max(fullSpan,d);
  if(matched[i]&&matched[j])matchedSpan=std::max(matchedSpan,d);
 }
 out.assessment_complete=true;
 out.reference_coverage=double(out.pairs.size())/n;out.observation_coverage=double(out.pairs.size())/m;
 out.reference_span=fullSpan>0?matchedSpan/fullSpan:0;
 if(!out.pairs.empty()){
  out.observed_rms_px=std::sqrt(square/out.pairs.size());out.observed_max_px=maximum;
 }
 out.reason="supplied_pose_only;count_coverage;reference_diameter_span;no_pose_search";
 if(out.pairs.size()<r.config.minimum_matches)out.status="INSUFFICIENT_MATCHES";
 else if(*out.reference_coverage<r.config.minimum_reference_coverage||
         *out.observation_coverage<r.config.minimum_observation_coverage)out.status="COVERAGE_REJECTED";
 else if(*out.reference_span<r.config.minimum_spatial_span)out.status="SPAN_REJECTED";
 else if(ambiguous)out.status="AMBIGUOUS_CORRESPONDENCE";
 else {out.status="HYPOTHESIS_VERIFIED";out.accepted=true;}
 return out;
}
}
