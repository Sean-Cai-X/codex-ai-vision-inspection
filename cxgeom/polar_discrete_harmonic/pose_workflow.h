#pragma once
#include "partial_pipeline.h"
#include <set>

namespace cxgeom::polar {
// Point-support pyramid, not an image pyramid or a topology reconstruction.
// This opt-in workflow is shape-agnostic and never grants production admission.
struct PoseAnchor {
 std::string reference_id,observation_id,provenance;
 double tolerance_px=.3; // observation coordinate units, NOT normalized units
};
struct PoseWorkflowConfig {
 bool enabled=false;
 std::string template_revision,template_digest; // caller binding, not a verified file hash
 std::vector<size_t> support_counts{8,16,0}; // 0 = all; last level must be all
 std::vector<PoseAnchor> anchors;
 PartialSearchConfig search;
};
struct PoseFrame {std::complex<double> center;double radius=1;};
struct PoseWorkflowLevel {
 size_t requested_support=0;
 std::vector<std::string> reference_ids,observation_ids;
 PartialPipelineResult normalized;
};
struct PoseWorkflowEvaluation {
 size_t level=0,candidate=0;
 PartialAssessment original;
 std::vector<double> anchor_errors_px;
 bool anchors_satisfied=false;
};
struct PoseWorkflowResult {
 std::string status="DISABLED",reason;
 PoseWorkflowConfig requested;
 PoseFrame reference_frame,observation_frame;
 std::vector<PoseWorkflowLevel> levels;
 std::vector<PoseWorkflowEvaluation> evaluations;
 std::vector<size_t> retained_evaluations;
 size_t pair_checks=0,seed_checks=0;
 bool complete=false,production_eligible=false;
};
namespace pose_workflow_detail {
inline PoseFrame Frame(const Descriptor& d){return {d.centroid,d.rms_radius};}
inline std::vector<Feature> Normalize(std::vector<Feature> f,const PoseFrame& frame){
 for(auto& p:f){p.point=(p.point-frame.center)/frame.radius;p.curvature*=frame.radius;}
 return f;
}
// Deterministic farthest-point support selection. No point averaging, synthetic
// boundary, endpoint closure or inferred identity. Every retained ID is original.
inline std::vector<Feature> Support(const std::vector<Feature>& f,size_t count,
 const std::set<std::string>& mandatory){
 if(count==0||count>=f.size())return f;
 std::vector<size_t> selected;std::vector<bool> used(f.size(),false);
 for(size_t i=0;i<f.size();++i)if(mandatory.count(f[i].stable_id)){selected.push_back(i);used[i]=true;}
 if(selected.empty()){selected.push_back(0);used[0]=true;}
 count=std::max(count,selected.size());
 while(selected.size()<count){
  size_t best=f.size();double distance=-1;
  for(size_t i=0;i<f.size();++i)if(!used[i]){
   double nearest=std::numeric_limits<double>::infinity();
   for(size_t j:selected)nearest=std::min(nearest,std::norm(f[i].point-f[j].point));
   if(nearest>distance){distance=nearest;best=i;}
  }
  if(best==f.size())break;
  used[best]=true;selected.push_back(best);
 }
 std::vector<Feature> out;for(size_t i=0;i<f.size();++i)if(used[i])out.push_back(f[i]);return out;
}
inline Hypothesis Restore(const Hypothesis& n,const PoseFrame& r,const PoseFrame& o){
 Hypothesis h=n;h.source_id="original_frame:"+n.source_id;
 h.scale=n.scale*o.radius/r.radius;
 const auto factor=std::polar(h.scale,h.angle_deg*std::acos(-1.)/180);
 h.translation=o.center+o.radius*n.translation-factor*r.center;return h;
}
}
inline PoseWorkflowResult RunPoseWorkflow(const PartialRequest& input,const PoseWorkflowConfig& cfg={}){
 PoseWorkflowResult out;out.requested=cfg;
 if(!cfg.enabled)return out;
 auto fail=[&](std::string why){out.status="BLOCKED";out.reason=std::move(why);out.retained_evaluations.clear();out.complete=false;return out;};
 const auto valid=ValidatePartial(input);if(!valid.ready_for_solver)return fail(valid.reason);
 if(cfg.template_revision.empty()||cfg.template_digest.empty())return fail("MISSING_TEMPLATE_BINDING");
 if(cfg.support_counts.empty()||cfg.support_counts.size()>8||cfg.support_counts.back()!=0)return fail("FULL_SUPPORT_FINAL_LEVEL_REQUIRED");
 for(size_t n:cfg.support_counts)if(n!=0&&(n<input.config.minimum_matches||n>input.config.maximum_features))return fail("INVALID_SUPPORT_COUNT");
 const auto r=Encode(input.reference_source,input.reference,input.config.encoding);
 const auto o=Encode(input.observation_source,input.observation,input.config.encoding);
 out.reference_frame=pose_workflow_detail::Frame(r);out.observation_frame=pose_workflow_detail::Frame(o);
 std::map<std::string,std::complex<double>> refs,obs;std::set<std::string> ra,oa;
 for(const auto& f:r.features)refs.emplace(f.stable_id,f.point);
 for(const auto& f:o.features)obs.emplace(f.stable_id,f.point);
 for(const auto& a:cfg.anchors){
  if(!refs.count(a.reference_id)||!obs.count(a.observation_id)||a.provenance.empty()||
    !std::isfinite(a.tolerance_px)||a.tolerance_px<=0||!ra.insert(a.reference_id).second||!oa.insert(a.observation_id).second)
   return fail("INVALID_OR_CONFLICTING_ANCHOR_BINDING");
 }
 const auto nr=pose_workflow_detail::Normalize(r.features,out.reference_frame);
 const auto no=pose_workflow_detail::Normalize(o.features,out.observation_frame);
 for(size_t count:cfg.support_counts){
  if(out.pair_checks>=input.config.maximum_pair_checks||out.seed_checks>=input.config.maximum_hypotheses)return fail("WORKFLOW_BUDGET_EXHAUSTED");
  PartialRequest n=input;n.reference=pose_workflow_detail::Support(nr,count,ra);n.observation=pose_workflow_detail::Support(no,count,oa);
  n.config.max_residual_px=input.config.max_residual_px/out.observation_frame.radius;
  n.config.encoding.minimum_rms_radius=input.config.encoding.minimum_rms_radius/std::max(out.reference_frame.radius,out.observation_frame.radius);
  n.config.maximum_pair_checks-=out.pair_checks;n.config.maximum_hypotheses-=out.seed_checks;
  auto search=cfg.search;const double ratio=out.reference_frame.radius/out.observation_frame.radius;
  search.scale_min*=ratio;search.scale_max*=ratio;search.merge_scale*=ratio;
  search.merge_translation_px/=out.observation_frame.radius;
  search.minimum_seed_length_px/=std::max(out.reference_frame.radius,out.observation_frame.radius);
  PoseWorkflowLevel level;level.requested_support=count;
  for(const auto& f:n.reference)level.reference_ids.push_back(f.stable_id);
  for(const auto& f:n.observation)level.observation_ids.push_back(f.stable_id);
  level.normalized=SearchRefinedPartial(n,search);
  out.pair_checks+=level.normalized.pair_checks;out.seed_checks+=level.normalized.raw.attempted_seeds;
  out.levels.push_back(level);
  if(!level.normalized.complete)return fail("LEVEL_INCOMPLETE:"+level.normalized.status);
  for(size_t i=0;i<level.normalized.candidates.size();++i){
   const auto h=pose_workflow_detail::Restore(level.normalized.candidates[i].supplied,out.reference_frame,out.observation_frame);
   if(!PartialPoseInWindow(h,cfg.search))continue;
   if(out.pair_checks>=input.config.maximum_pair_checks)return fail("ORIGINAL_EVALUATION_BUDGET_EXHAUSTED");
   auto bounded=input;bounded.config.maximum_pair_checks-=out.pair_checks;
   PoseWorkflowEvaluation e;e.level=out.levels.size()-1;e.candidate=i;
   e.original=AssessHypothesis(bounded,h);out.pair_checks+=e.original.pair_checks;e.anchors_satisfied=true;
   const auto factor=std::polar(h.scale,h.angle_deg*std::acos(-1.)/180);
   for(const auto& a:cfg.anchors){
    const double error=std::abs(factor*refs.at(a.reference_id)+h.translation-obs.at(a.observation_id));
    e.anchor_errors_px.push_back(error);bool paired=false;
    for(const auto& p:e.original.pairs)if(p.reference_id==a.reference_id&&p.target_id==a.observation_id)paired=true;
    if(!std::isfinite(error)||error>a.tolerance_px||!paired)e.anchors_satisfied=false;
   }
   out.evaluations.push_back(e);
   if(!e.original.assessment_complete)return fail("ORIGINAL_EVALUATION_INCOMPLETE");
   if(e.original.accepted&&e.anchors_satisfied){
    bool duplicate=false;
    for(size_t previous:out.retained_evaluations){const auto& p=out.evaluations[previous].original;
     if(SamePartialAssessment(p,e.original)){duplicate=true;break;}
    }
    if(!duplicate)out.retained_evaluations.push_back(out.evaluations.size()-1);
   }
  }
 }
 out.complete=true;out.status=out.retained_evaluations.empty()?"NO_VERIFIED_POSE":out.retained_evaluations.size()==1?"POSE_CANDIDATE":"AMBIGUOUS";
 out.reason="finite_support_search;original_coordinate_evaluation;no_global_uniqueness_or_production;template_digest_caller_supplied";
 return out;
}
}
