#include "pch.h"
#include "CxFastMatchHarmonicAudit.h"
#include "FindObject.h"
#include "FastMatch.h"
#include "ManualConsoleUtils.h"
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>

void CxFastMatchHarmonicAudit::invalidate() { ran_=false;result_={}; }
void CxFastMatchHarmonicAudit::select(int side) {
    if(side<0 || side>1) throw std::invalid_argument("HARMONIC_INVALID_SIDE");
    selected_=side;
}
void CxFastMatchHarmonicAudit::clear() {
    open_runs_[selected_].reset();
    anchor_evidence_[selected_]="null";
    measured_count_[selected_]=measured_holes_[selected_]=measured_index_[selected_]=-1;
    loaded_[selected_].reset();contours_[selected_]={};sources_[selected_]="script_points";invalidate();
}
void CxFastMatchHarmonicAudit::point(double x,double y) {
    if(open_runs_[selected_])throw std::invalid_argument("OPEN_SUBCURVE_EDIT_REQUIRES_CLEAR");
    if(!std::isfinite(x)||!std::isfinite(y))throw std::invalid_argument("NONFINITE_CONTOUR");
    if(contours_[selected_].points.size()>=4096)throw std::invalid_argument("SOURCE_POINT_BUDGET_EXCEEDED");
    loaded_[selected_].reset();
    measured_count_[selected_]=measured_holes_[selected_]=measured_index_[selected_]=-1;
    anchor_evidence_[selected_]="null";
    sources_[selected_]="script_points";
    contours_[selected_].points.emplace_back(x,y);invalidate();
}
void CxFastMatchHarmonicAudit::topology(int verified,int closed,int complete,int holes,int components) {
    if((verified!=0 && verified!=1)||(closed!=0 && closed!=1)||(complete!=0 && complete!=1)||holes<0||components<1)
        throw std::invalid_argument("HARMONIC_INVALID_TOPOLOGY");
    if(open_runs_[selected_] && (closed || complete))throw std::invalid_argument("OPEN_SUBCURVE_CANNOT_CLOSE");
    if(measured_count_[selected_]>=0 && (holes!=measured_holes_[selected_] || components!=1))
        throw std::invalid_argument("HARMONIC_TOPOLOGY_CONTRADICTS_MEASUREMENT");
    loaded_[selected_].reset();
    auto& c=contours_[selected_];
    c.topology_verified=verified!=0;c.closed=closed!=0;c.complete=complete!=0;
    c.holes=holes;c.components=components;invalidate();
}
void CxFastMatchHarmonicAudit::sourceindex(int index) {
    if(index<0)throw std::invalid_argument("HARMONIC_INVALID_SOURCE_INDEX");
    if(pending_anchor_)throw std::invalid_argument("HARMONIC_SELECTION_CONFLICT");
    source_index_=index;source_index_explicit_=true;
}
void CxFastMatchHarmonicAudit::anchorrect(double x,double y,double width,double height) {
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(width)||!std::isfinite(height)||
       x<0||y<0||width<=0||height<=0||!std::isfinite(x+width)||!std::isfinite(y+height))
        throw std::invalid_argument("HARMONIC_INVALID_ANCHOR");
    if(source_index_explicit_)throw std::invalid_argument("HARMONIC_SELECTION_CONFLICT");
    pending_anchor_=Anchor{x,y,width,height};
}
void CxFastMatchHarmonicAudit::anchorpoints(int minimum) {
    if(minimum<1||minimum>4096)throw std::invalid_argument("HARMONIC_INVALID_ANCHOR_POINTS");
    anchor_minimum_points_=minimum;
}
void CxFastMatchHarmonicAudit::fromobject(void* object) { importobject(object,false); }
void CxFastMatchHarmonicAudit::fromobjectarc(void* object) { importobject(object,true); }
void CxFastMatchHarmonicAudit::importobject(void* object,bool subcurve) {
    if(subcurve && !pending_anchor_)throw std::invalid_argument("OPEN_SUBCURVE_ANCHOR_REQUIRED");
    if(!object)throw std::invalid_argument("HARMONIC_MISSING_SOURCE");
    const auto& finder=*static_cast<const FindObject*>(object);
    const int count=static_cast<int>(finder.getmeasurements().size());
    if(count>1 && !source_index_explicit_ && !pending_anchor_)
        throw std::invalid_argument("HARMONIC_AMBIGUOUS_SOURCE_SELECTION");
    if(!finder.getmeasurementconfig().include_hole_boundaries)
        throw std::invalid_argument("HARMONIC_HOLE_EVIDENCE_DISABLED");
    int chosen=source_index_;
    std::string anchor_record="null";
    if(pending_anchor_) {
        const auto a=*pending_anchor_;
        int candidates=0,selected_hits=0,selected_runs=0;
        for(int index=0;index<count;++index) {
            const auto& points=finder.getmeasurements()[index].outer_boundary;
            std::vector<bool> hits;int hit_count=0,runs=0;
            for(const auto& p:points) {
                bool hit=p.x>=a.x && p.x<a.x+a.width && p.y>=a.y && p.y<a.y+a.height;
                hits.push_back(hit);if(hit)++hit_count;
            }
            if(hit_count==0)continue;
            for(size_t j=0;j<hits.size();++j)
                if(hits[j] && !hits[(j+hits.size()-1)%hits.size()])++runs;
            if(hit_count==static_cast<int>(hits.size()))runs=1;
            ++candidates;chosen=index;selected_hits=hit_count;selected_runs=runs;
        }
        if(candidates==0)throw std::invalid_argument("HARMONIC_ANCHOR_NO_BOUNDARY");
        if(candidates!=1)throw std::invalid_argument("HARMONIC_ANCHOR_AMBIGUOUS_COMPONENTS");
        if(selected_runs!=1)throw std::invalid_argument("HARMONIC_ANCHOR_AMBIGUOUS_ARCS");
        if(selected_hits<anchor_minimum_points_)throw std::invalid_argument("HARMONIC_ANCHOR_INSUFFICIENT_SUPPORT");
        std::ostringstream record;record<<std::setprecision(17);
        record<<"{\"basis\":\"outer_boundary_points\",\"rect_half_open\":["
              <<a.x<<","<<a.y<<","<<a.width<<","<<a.height
              <<"],\"minimum_points\":"<<anchor_minimum_points_
              <<",\"candidate_count\":"<<candidates<<",\"hit_points\":"<<selected_hits
              <<",\"contiguous_runs\":"<<selected_runs<<"}";
        anchor_record=record.str();
    }
    const auto* m=finder.getmeasurement(chosen);
    if(!m)throw std::invalid_argument("HARMONIC_MISSING_MEASUREMENT");
    if(m->outer_boundary.size()>4096)throw std::invalid_argument("SOURCE_POINT_BUDGET_EXCEEDED");
    std::vector<std::complex<double>> parent;
    for(const auto& p:m->outer_boundary)parent.emplace_back(p.x,p.y);
    std::optional<cxgeom::OpenBoundaryRun> extracted;
    if(subcurve) {
        const auto a=*pending_anchor_;
        extracted=cxgeom::ExtractOpenBoundaryRun(parent,a.x,a.y,a.width,a.height,
            anchor_minimum_points_<2?2:anchor_minimum_points_);
    }
    clear();
    auto& c=contours_[selected_];
    c.points=extracted?extracted->points:parent;
    if(extracted){c.closed=false;c.complete=false;open_runs_[selected_]=std::move(extracted);}
    c.holes=static_cast<int>(m->hole_boundaries.size());
    measured_count_[selected_]=count;measured_holes_[selected_]=c.holes;
    measured_index_[selected_]=chosen;
    anchor_evidence_[selected_]=anchor_record;pending_anchor_.reset();
    source_index_=0;source_index_explicit_=false; // Selection is one-shot, never a cross-image identity.
    sources_[selected_]="FindObject.outer_boundary:"+m->object_ref+
        ":generation="+std::to_string(m->generation)+":mask="+std::to_string(m->mask_hash);
    // Remains unverified/open until explicit caller topology evidence is supplied.
}
void CxFastMatchHarmonicAudit::parameter(double v,const char* key) {
    if(!key || !std::isfinite(v))throw std::invalid_argument("HARMONIC_INVALID_PARAMETER");
    const std::string k(key);
    auto integer=[&]() {
        if(std::floor(v)!=v || v<0 || v>4096)throw std::invalid_argument("HARMONIC_INVALID_INTEGER");
        return static_cast<int>(v);
    };
    if(k=="method") { int n=integer();if(n>1)throw std::invalid_argument("UNSUPPORTED_METHOD");method_=static_cast<cxgeom::so2::Method>(n); }
    else if(k=="sample_count")config_.sample_count=integer();
    else if(k=="max_order")config_.max_order=integer();
    else if(k=="minimum_source_points")config_.minimum_source_points=integer();
    else if(k=="minimum_perimeter")config_.minimum_perimeter=v;
    else if(k=="normalize_scale") { if(v!=0 && v!=1)throw std::invalid_argument("HARMONIC_INVALID_BOOLEAN");config_.normalize_scale=v!=0; }
    else if(k=="maximum_pose_residual")config_.maximum_pose_residual=v;
    else if(k=="symmetry_relative_amplitude")config_.symmetry_relative_amplitude=v;
    else if(k=="peak_relative_tolerance")config_.peak_relative_tolerance=v;
    else if(k=="maximum_hypotheses")config_.maximum_hypotheses=integer();
    else if(k=="debug_mode" || k=="save_intermediate_features") {
        if(v!=0 && v!=1)throw std::invalid_argument("HARMONIC_INVALID_BOOLEAN");
        if(k=="debug_mode")debug_mode_=v!=0;else save_intermediate_features_=v!=0;
    }
    else throw std::invalid_argument("HARMONIC_UNKNOWN_PARAMETER");
    invalidate();
}
void CxFastMatchHarmonicAudit::run() {
    result_={};
    std::optional<cxgeom::so2::Descriptor> feature_a,feature_b;
    if(save_intermediate_features_ && !debug_mode_)throw std::invalid_argument("HARMONIC_FEATURES_REQUIRE_DEBUG");
    try {
        if(open_runs_[0]||open_runs_[1])throw std::invalid_argument("OPEN_CONTOUR_LEGACY_FALLBACK");
        const auto a=loaded_[0]?*loaded_[0]:cxgeom::so2::Build(contours_[0],config_,method_);
        auto expected=a;expected.config=config_;expected.method=method_;cxgeom::so2::Distance(a,expected);
        const auto b=loaded_[1]?*loaded_[1]:cxgeom::so2::Build(contours_[1],config_,method_);
        result_=cxgeom::so2::Match(a,b);
        if(debug_mode_ && save_intermediate_features_) {feature_a=a;feature_b=b;}
    } catch(const std::invalid_argument& e) {
        result_.status=e.what();result_.fallback_reason="LEGACY_UNCHANGED";
    }
    ran_=true;
    std::ostringstream s;s<<std::setprecision(17);
    s<<"{\"status\":\""<<JsonEscape(result_.status)<<"\",\"fallback_reason\":\""
     <<JsonEscape(result_.fallback_reason)<<"\",\"invariant_distance\":"<<result_.invariant_distance
     <<",\"symmetry_order\":"<<result_.symmetry_order<<",\"parameters\":{"
     <<"\"method\":"<<static_cast<int>(method_)<<",\"sample_count\":"<<config_.sample_count
     <<",\"max_order\":"<<config_.max_order<<",\"minimum_source_points\":"<<config_.minimum_source_points
     <<",\"minimum_perimeter\":"<<config_.minimum_perimeter<<",\"normalize_scale\":"<<(config_.normalize_scale?"true":"false")
     <<",\"maximum_pose_residual\":"<<config_.maximum_pose_residual
     <<",\"symmetry_relative_amplitude\":"<<config_.symmetry_relative_amplitude
     <<",\"peak_relative_tolerance\":"<<config_.peak_relative_tolerance
     <<",\"maximum_hypotheses\":"<<config_.maximum_hypotheses<<"},\"inputs\":[";
    for(int i=0;i<2;++i) {
        if(i)s<<",";
        if(loaded_[i]) {
            const cxgeom::so2::ReferenceAsset asset{*loaded_[i],sources_[i]};
            s<<"{\"source\":\""<<JsonEscape(sources_[i])
             <<"\",\"input_kind\":\"sha_checked_descriptor_asset\",\"sha256\":\""
             <<cxgeom::so2::ReferenceSha256(cxgeom::so2::EncodeReference(asset))
             <<"\",\"point_count\":null,\"topology_evidence\":\"not_embedded\",\"coefficient_count\":"
             <<loaded_[i]->coefficients.size()<<"}";
            continue;
        }
        const auto& c=contours_[i];
        s<<"{\"source\":\""<<JsonEscape(sources_[i])<<"\",\"point_count\":"<<c.points.size()
         <<",\"topology_verified\":"<<(c.topology_verified?"true":"false")
         <<",\"closed\":"<<(c.closed?"true":"false")<<",\"complete\":"<<(c.complete?"true":"false")
         <<",\"holes\":"<<c.holes<<",\"components\":"<<c.components
         <<",\"measured_source_count\":"<<measured_count_[i]
         <<",\"measured_holes\":"<<measured_holes_[i]
         <<",\"selected_source_index\":"<<measured_index_[i]
         <<",\"anchor_selection\":"<<anchor_evidence_[i]
         <<",\"open_subcurve\":"<<(open_runs_[i]?"true":"false")<<"}";
    }
    s<<"],\"poses\":[";
    for(size_t i=0;i<result_.poses.size();++i) {
        if(i)s<<",";const auto& p=result_.poses[i];
        s<<"{\"angle_deg\":"<<p.angle_deg<<",\"scale\":"<<p.scale
         <<R"(,"translation_x":)"<<p.translation_x
         <<R"(,"translation_y":)"<<p.translation_y
         <<R"(,"cyclic_shift":)"<<p.cyclic_shift
         <<R"(,"mapping":"reference_to_observation")"
         <<R"(,"coordinate_units":"source_units")"
         <<",\"correlation\":"<<p.correlation<<",\"residual\":"<<p.residual<<"}";
    }
    s<<"],\"debug\":{\"enabled\":"<<(debug_mode_?"true":"false")
     <<",\"save_intermediate_features\":"<<(save_intermediate_features_?"true":"false")
     <<",\"status\":\""<<(!debug_mode_?"DISABLED":!save_intermediate_features_?"NOT_REQUESTED":feature_a?"AVAILABLE":"UNAVAILABLE")
     <<"\",\"reason\":\""<<(debug_mode_ && save_intermediate_features_ && !feature_a?JsonEscape(result_.status):"")
     <<"\",\"features\":[";
    if(feature_a && feature_b) {
        int side=0;
        for(const auto* d:{&*feature_a,&*feature_b}) {
            if(side)s<<",";
            s<<"{\"side\":"<<side++<<",\"method\":"<<static_cast<int>(d->method)
             <<",\"centroid\":["<<d->centroid.real()<<","<<d->centroid.imag()<<"]"
             <<",\"scale\":"<<d->scale<<",\"perimeter\":"<<d->perimeter
             <<",\"normalized_scale\":"<<(d->config.normalize_scale?"true":"false")
             <<",\"phase_units\":\"radians\",\"phase_semantics\":\"coefficient_phase_not_rotation_response\",\"coefficients\":[";
            for(size_t k=0;k<d->coefficients.size();++k) {
                if(k)s<<",";
                const auto z=d->coefficients[k];const double amplitude=std::abs(z);
                s<<"{\"frequency\":"<<d->frequencies[k]<<",\"real\":"<<z.real()
                 <<",\"imag\":"<<z.imag()<<",\"amplitude\":"<<amplitude<<",\"phase_rad\":";
                if(amplitude>1e-12)s<<std::arg(z);else s<<"null";
                s<<"}";
            }
            s<<"]}";
        }
    }
    s<<"]}}";history_.push_back(s.str());
}
void CxFastMatchHarmonicAudit::expectstatus(const char* expected) {
    if(!ran_ || !expected || result_.status!=expected)throw std::runtime_error("HARMONIC_STATUS_ASSERTION_FAILED:"+result_.status);
    ++assertions_;
}
void CxFastMatchHarmonicAudit::expectcount(int count) {
    if(!ran_ || count<0 || result_.poses.size()!=static_cast<size_t>(count))throw std::runtime_error("HARMONIC_COUNT_ASSERTION_FAILED");
    ++assertions_;
}
void CxFastMatchHarmonicAudit::expectpose(double angle,double scale,double tolerance) {
    expectposebounds(angle,scale,tolerance,tolerance);
}
void CxFastMatchHarmonicAudit::expectposebounds(double angle,double scale,double angle_tolerance,double scale_tolerance) {
    if(!ran_ || !std::isfinite(angle)||!std::isfinite(scale)||scale<=0 ||
       !std::isfinite(angle_tolerance)||angle_tolerance<=0 ||
       !std::isfinite(scale_tolerance)||scale_tolerance<=0)
        throw std::runtime_error("HARMONIC_POSE_ASSERTION_INVALID");
    for(const auto& p:result_.poses) {
        double e=std::remainder(p.angle_deg-angle,360.0);
        if(std::abs(e)<=angle_tolerance && std::abs(p.scale-scale)<=scale_tolerance){++assertions_;return;}
    }
    throw std::runtime_error("HARMONIC_POSE_ASSERTION_FAILED");
}
void CxFastMatchHarmonicAudit::save(const char* path) {
    if(!ran_ || !path || !*path)throw std::runtime_error("HARMONIC_RECEIPT_NOT_READY");
    std::ofstream f(path,std::ios::trunc);
    f<<"{\"schema\":\"cxvision.harmonic_audit_receipt.v1\",\"mode\":\"AUDIT\","
      <<"\"measurement_evidence\":false,\"used_for_seed\":false,\"used_for_prefilter\":false,"
      <<"\"production_eligible\":false,\"assertions_passed\":"<<assertions_<<",\"runs\":[";
    for(size_t i=0;i<history_.size();++i){if(i)f<<",";f<<history_[i];}
    f<<"],\"asset_events\":[";
    for(size_t i=0;i<asset_events_.size();++i){if(i)f<<",";f<<asset_events_[i];}
    f<<"]}\n";f.close();
    if(!f)throw std::runtime_error(std::string("HARMONIC_RECEIPT_WRITE_FAILED:")+path);
}

namespace {
std::string HarmonicFastMatchSnapshot(FastMatch& f) {
    const auto& r=f.getformfitresult();
    const auto& ref=f.getreferenceshapemodel();
    const auto& obs=f.getobservedshapemodel();
    std::ostringstream s;s<<std::setprecision(17);
    s<<f.getresultcandidatecount()<<' '<<f.getresultbestscore()<<' ';
    for(int i=0;i<f.getresultcandidatecount();++i) {
        auto box=f.getresultrect(i);
        s<<box.TopLeft().X()<<' '<<box.TopLeft().Y()<<' '<<box.Width()<<' '<<box.Height()<<' ';
    }
    s<<r.executed<<' '<<r.succeeded<<' '<<r.status<<' '<<r.failure_stage<<' '
     <<r.score<<' '<<r.mean_residual_px<<' '<<r.symmetric_residual_px<<' '
     <<r.angle_deg<<' '<<r.scale_x<<' '<<r.scale_y<<' '<<r.translate_x<<' '<<r.translate_y<<' '
     <<r.affine_a<<' '<<r.affine_b<<' '<<r.affine_c<<' '<<r.affine_d<<' ';
    for(const auto* model:{&ref,&obs}) {
        s<<model->model_id<<' '<<model->available<<' '<<model->dense_points.size()<<' ';
        for(const auto& p:model->dense_points)
            s<<p.x<<' '<<p.y<<' '<<p.normal_x<<' '<<p.normal_y<<' '<<p.confidence<<' ';
    }
    for(const auto& c:r.correspondences)
        s<<c.reference_index<<' '<<c.observed_index<<' '<<c.distance_px<<' '
         <<c.normal_delta_deg<<' '<<c.weight<<' '<<c.mutual<<' '<<c.accepted<<' ';
    return s.str();
}
}
void CxFastMatchHarmonicAudit::fromreference(void* object) {
    if(!object)throw std::invalid_argument("HARMONIC_MISSING_FASTMATCH");
    const auto& m=static_cast<const FastMatch*>(object)->getreferenceshapemodel();
    if(!m.available || m.dense_points.empty())throw std::invalid_argument("HARMONIC_REFERENCE_NOT_READY");
    clear();
    for(const auto& p:m.dense_points)point(p.x,p.y);
    sources_[selected_]="FastMatch.reference_dense:"+m.model_id;
    // Legacy model.closed is not independent topology evidence.
}
void CxFastMatchHarmonicAudit::snapshotfastmatch(void* object) {
    if(!object)throw std::invalid_argument("HARMONIC_MISSING_FASTMATCH");
    auto& f=*static_cast<FastMatch*>(object);
    const auto& r=f.getformfitresult();
    if(!f.getreferenceshapemodel().available || f.getresultcandidatecount()<1 ||
       !r.executed || !r.succeeded || r.budget_exceeded || r.dense_mutual_count<1)
        throw std::runtime_error("HARMONIC_FASTMATCH_BASELINE_NOT_READY");
    fastmatch_snapshot_=HarmonicFastMatchSnapshot(f);
    ++assertions_;
}
void CxFastMatchHarmonicAudit::expectunchanged(void* object) {
    if(!object || fastmatch_snapshot_.empty() ||
       fastmatch_snapshot_!=HarmonicFastMatchSnapshot(*static_cast<FastMatch*>(object)))
        throw std::runtime_error("HARMONIC_FASTMATCH_CHANGED");
    ++assertions_;
}

void CxFastMatchHarmonicAudit::trustedsha(const char* hash) {
    trusted_sha_=hash?hash:"";
}
void CxFastMatchHarmonicAudit::saveasset(const char* path) {
    if(open_runs_[selected_])throw std::invalid_argument("OPEN_SUBCURVE_NOT_CLOSED_DESCRIPTOR");
    if(!path || !*path)throw std::invalid_argument("ASSET_PATH_REQUIRED");
    cxgeom::so2::ReferenceAsset a{
        loaded_[selected_]?*loaded_[selected_]:cxgeom::so2::Build(contours_[selected_],config_,method_),
        sources_[selected_]};
    auto expected=a.descriptor;expected.config=config_;expected.method=method_;
    cxgeom::so2::Distance(a.descriptor,expected);
    const auto bytes=cxgeom::so2::EncodeReference(a);
    const std::filesystem::path dest(path), pending(std::string(path)+".pending");
    if(std::filesystem::exists(dest)||std::filesystem::exists(pending))
        throw std::runtime_error("ASSET_OUTPUT_EXISTS");
    std::ofstream f(pending,std::ios::binary);
    f.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));f.close();
    if(!f)throw std::runtime_error("ASSET_WRITE_FAILED");
    std::filesystem::rename(pending,dest);
    trusted_sha_=cxgeom::so2::ReferenceSha256(bytes);
    asset_events_.push_back("{\"operation\":\"save\",\"sha256\":\""+trusted_sha_+"\"}");
}
void CxFastMatchHarmonicAudit::loadasset(const char* path) {
    if(!path || !*path)throw std::invalid_argument("ASSET_PATH_REQUIRED");
    std::ifstream f(path,std::ios::binary|std::ios::ate);
    if(!f)throw std::runtime_error("ASSET_READ_FAILED");
    const auto n=f.tellg();
    if(n<0 || n>262144)throw std::invalid_argument("ASSET_SIZE_LIMIT");
    std::string bytes(static_cast<size_t>(n),'\0');f.seekg(0);
    if(!f.read(bytes.data(),static_cast<std::streamsize>(bytes.size())))throw std::runtime_error("ASSET_READ_FAILED");
    auto a=cxgeom::so2::DecodeReference(bytes,trusted_sha_,config_,method_);
    // Commit only after all validation; a failed load preserves the previous slot.
    open_runs_[selected_].reset();
    loaded_[selected_]=std::move(a.descriptor);sources_[selected_]=std::move(a.provenance);
    contours_[selected_]={};anchor_evidence_[selected_]="null";
    measured_count_[selected_]=measured_holes_[selected_]=measured_index_[selected_]=-1;invalidate();
    asset_events_.push_back("{\"operation\":\"load\",\"sha256\":\""+trusted_sha_+"\"}");
}

void CxFastMatchHarmonicAudit::expectsubcurve(void* object) {
    if(!object || !open_runs_[selected_])throw std::runtime_error("OPEN_OBSERVATION_NOT_READY");
    const auto& r=*open_runs_[selected_];
    const auto* m=static_cast<const FindObject*>(object)->getmeasurement(measured_index_[selected_]);
    if(!m || r.parent_point_count!=m->outer_boundary.size() || r.points.size()<2 ||
       contours_[selected_].closed || contours_[selected_].complete ||
       sources_[selected_]!="FindObject.outer_boundary:"+m->object_ref+
         ":generation="+std::to_string(m->generation)+":mask="+std::to_string(m->mask_hash))
        throw std::runtime_error("OPEN_OBSERVATION_PARENT_MISMATCH");
    for(size_t k=0;k<r.points.size();++k) {
        const size_t i=r.parent_indices[k];
        if(i>=m->outer_boundary.size() ||
           (k && i!=(r.parent_indices[k-1]+1)%r.parent_point_count) ||
           r.points[k]!=std::complex<double>(m->outer_boundary[i].x,m->outer_boundary[i].y))
            throw std::runtime_error("OPEN_OBSERVATION_ORDER_MISMATCH");
    }
    if(r.points.front()==r.points.back())throw std::runtime_error("OPEN_OBSERVATION_CLOSED");
    ++assertions_;
}
void CxFastMatchHarmonicAudit::saveopen(const char* path) {
    if(!path||!*path||!open_runs_[selected_])throw std::runtime_error("OPEN_OBSERVATION_NOT_READY");
    const auto& r=*open_runs_[selected_];
    const std::filesystem::path dest(path),pending(std::string(path)+".pending");
    if(std::filesystem::exists(dest)||std::filesystem::exists(pending))throw std::runtime_error("OPEN_OUTPUT_EXISTS");
    std::ofstream f(pending,std::ios::binary);f.imbue(std::locale::classic());f<<std::setprecision(17);
    f<<"{\"schema\":\"cxvision.open_boundary_observation.v1\",\"mode\":\"AUDIT\","
     <<"\"closed\":false,\"complete_parent\":false,\"physical_boundary_verified\":false,"
     <<"\"production_eligible\":false,\"order\":\"parent_cyclic_forward\","
     <<"\"endpoint_semantics\":\"first_last_retained_samples\","
     <<"\"source\":\""<<JsonEscape(sources_[selected_])<<"\","
     <<"\"selected_source_index\":"<<measured_index_[selected_]
     <<",\"parent_holes\":"<<measured_holes_[selected_]
     <<",\"parent_point_count\":"<<r.parent_point_count
     <<",\"wraps_parent_origin\":"<<(r.wraps_parent_origin?"true":"false")
     <<",\"anchor\":"<<anchor_evidence_[selected_]
     <<",\"start\":["<<r.points.front().real()<<","<<r.points.front().imag()<<"]"
     <<",\"end\":["<<r.points.back().real()<<","<<r.points.back().imag()<<"]"
     <<",\"point_count\":"<<r.points.size()<<",\"samples\":[";
    for(size_t k=0;k<r.points.size();++k) {
        if(k)f<<",";
        f<<"{\"parent_index\":"<<r.parent_indices[k]<<",\"x\":"<<r.points[k].real()<<",\"y\":"<<r.points[k].imag()<<"}";
    }
    f<<"]}\n";f.close();if(!f)throw std::runtime_error("OPEN_OBSERVATION_WRITE_FAILED");
    std::filesystem::rename(pending,dest);
}
