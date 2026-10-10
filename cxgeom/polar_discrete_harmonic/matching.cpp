#include "matching.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>
namespace cxgeom::polar {
MatchResult MatchFull(const std::string& rid,const std::vector<Feature>& rf,
 const std::string& tid,const std::vector<Feature>& tf,const MatchConfig& cfg){
 constexpr double pi=3.14159265358979323846;
 auto need=[](bool b,const char* s){if(!b)throw std::invalid_argument(s);};
 need(std::isfinite(cfg.scale_min)&&std::isfinite(cfg.scale_max)&&cfg.scale_min>0&&
      cfg.scale_max>=cfg.scale_min,"POLAR_INVALID_SCALE_RANGE");
 need(std::isfinite(cfg.max_residual_px)&&cfg.max_residual_px>0&&
      std::isfinite(cfg.max_spectral_distance)&&cfg.max_spectral_distance>=0&&
      std::isfinite(cfg.min_phase_amplitude)&&cfg.min_phase_amplitude>0&&
      cfg.maximum_candidates>0&&cfg.maximum_candidates<=64&&
      cfg.maximum_pair_checks>0,"POLAR_INVALID_MATCH_LIMIT");
 auto a=Encode(rid,rf,cfg.encoding),b=Encode(tid,tf,cfg.encoding);
 MatchResult out;out.reference_source=rid;out.target_source=tid;out.executed=cfg;
 if(rf.size()!=tf.size()){out.status="UNSUPPORTED_PARTIAL_SET";return out;}
 need(rf.size()<=128,"POLAR_FULL_SET_SIZE_LIMIT");
 for(const auto* d:{&a,&b})for(const auto& f:d->features)
  if(f.weight!=d->features.front().weight){out.status="UNSUPPORTED_NONUNIFORM_WEIGHTS";return out;}
 const double scale=b.rms_radius/a.rms_radius;
 if(scale<cfg.scale_min||scale>cfg.scale_max){out.status="SCALE_RANGE_REJECTED";return out;}
 double strength=0;
 for(int c=0;c<3;++c)for(int k=1;k<=cfg.encoding.max_order;++k){
  double v=std::min(std::abs(a.moments[c][k]),std::abs(b.moments[c][k]));
  if(v>strength){strength=v;out.phase_channel=c;out.phase_order=k;}
 }
 if(strength<cfg.min_phase_amplitude){out.status="ORIENTATION_UNOBSERVABLE";return out;}
 const int k=out.phase_order,c=out.phase_channel;
 double base=-std::arg(b.moments[c][k]*std::conj(a.moments[c][k]))/k;
 const size_t n=a.features.size();
 for(int root=0;root<k;++root){
  if(out.evaluated_candidates>=cfg.maximum_candidates){
   out.candidates.clear();out.status="CANDIDATE_BUDGET_EXHAUSTED";return out;
  }
  ++out.evaluated_candidates;
  double angle=std::remainder(base+2*pi*root/k,2*pi),distance=0;
  for(int ch=0;ch<3;++ch)for(int order=0;order<=cfg.encoding.max_order;++order)
   distance+=std::norm(b.moments[ch][order]-a.moments[ch][order]*std::polar(1.,-order*angle));
  distance=std::sqrt(distance/(3*(cfg.encoding.max_order+1)));
  if(distance>cfg.max_spectral_distance)continue;
  PoseCandidate pose;pose.angle_deg=angle*180/pi;pose.scale=scale;pose.spectral_distance=distance;
  auto rot=std::polar(scale,angle);pose.translation=b.centroid-rot*a.centroid;
  std::vector<std::vector<size_t>> edges(n);
  std::vector<std::vector<double>> residual(n,std::vector<double>(n));
  for(size_t i=0;i<n;++i)for(size_t j=0;j<n;++j){
   if(out.pair_checks>=cfg.maximum_pair_checks){
    out.candidates.clear();out.status="PAIR_BUDGET_EXHAUSTED";return out;
   }
   ++out.pair_checks;
   residual[i][j]=std::abs(rot*a.features[i].point+pose.translation-b.features[j].point);
   if(residual[i][j]<=cfg.max_residual_px)edges[i].push_back(j);
  }
  // Deterministic augmenting paths: no greedy duplicate assignment.
  std::vector<int> owner(n,-1);
  std::function<bool(size_t,std::vector<bool>&)> augment=[&](size_t i,std::vector<bool>& seen){
   for(size_t j:edges[i])if(!seen[j]){
    seen[j]=true;
    if(owner[j]<0||augment(size_t(owner[j]),seen)){owner[j]=int(i);return true;}
   }return false;
  };
  bool full=true;
  for(size_t i=0;i<n;++i){std::vector<bool> seen(n,false);if(!augment(i,seen)){full=false;break;}}
  if(!full)continue;
  for(size_t j=0;j<n;++j){
   size_t i=size_t(owner[j]);double e=residual[i][j];
   pose.pairs.push_back({a.features[i].stable_id,b.features[j].stable_id,e});
   pose.rms_px+=e*e;pose.max_px=std::max(pose.max_px,e);
   pose.correspondence_ambiguous=pose.correspondence_ambiguous||edges[i].size()>1;
  }
  pose.rms_px=std::sqrt(pose.rms_px/n);
  std::sort(pose.pairs.begin(),pose.pairs.end(),[](const Pair& x,const Pair& y){return x.reference_id<y.reference_id;});
  out.candidates.push_back(pose);
 }
 out.search_complete=true;
 std::sort(out.candidates.begin(),out.candidates.end(),[](const PoseCandidate& x,const PoseCandidate& y){
  return x.angle_deg<y.angle_deg;
 });
 out.status=out.candidates.empty()?"NO_SPATIAL_MATCH":"FULL_SET_AUDIT_CANDIDATES";
 if(out.candidates.size()>1)out.status="AMBIGUOUS";
 for(const auto& p:out.candidates)if(p.correspondence_ambiguous)out.status="AMBIGUOUS";
 out.reason="bounded_phase_roots_only;no_partial_recovery;no_production_admission";
 return out;
}
}
