#include "FindSetMatch.h"
#include "../libtorchsegmentation/src/utils/json.hpp"
#include "../cxgeom/include/CxGeoSO2ReferenceAsset.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
using namespace cxgeom::gsm;
namespace {
using J=nlohmann::json;
constexpr std::size_t limit=4*1024*1024;
void keys(const J& j,std::initializer_list<const char*> names) {
    if(!j.is_object()||j.size()!=names.size())throw std::invalid_argument("SETMATCH_OBJECT_FIELDS");
    for(auto n:names)if(!j.contains(n))throw std::invalid_argument("SETMATCH_MISSING_FIELD");
}
double number(const J& j) {
    if(!j.is_number())throw std::invalid_argument("SETMATCH_NUMBER_REQUIRED");
    double v=j.get<double>();if(!std::isfinite(v))throw std::invalid_argument("SETMATCH_FINITE_REQUIRED");return v;
}
std::size_t integer(const J& j) {
    if(!j.is_number_integer()||number(j)<0||number(j)>100000000)
        throw std::invalid_argument("SETMATCH_INTEGER_REQUIRED");
    return j.get<std::size_t>();
}
std::string str(const J& j) {
    if(!j.is_string())throw std::invalid_argument("SETMATCH_STRING_REQUIRED");
    return j.get<std::string>();
}
Point2 point(const J& j){keys(j,{"x","y"});return {number(j.at("x")),number(j.at("y"))};}
Rect rect(const J& j) {
    keys(j,{"x","y","width","height"});
    return {number(j.at("x")),number(j.at("y")),number(j.at("width")),number(j.at("height"))};
}
GeometricSet set(const J& j) {
    keys(j,{"set_id","source_ref","roi","elements"});
    GeometricSet s;s.set_id=str(j.at("set_id"));s.source_ref=str(j.at("source_ref"));s.roi=rect(j.at("roi"));
    const auto& es=j.at("elements");
    if(!es.is_array()||es.empty()||es.size()>8192)throw std::invalid_argument("SETMATCH_ELEMENT_LIMIT");
    for(const auto& e:es) {
        keys(e,{"stable_id","source_ref","quality","geometry"});
        Element v;v.stable_id=str(e.at("stable_id"));v.source_ref=str(e.at("source_ref"));v.quality=number(e.at("quality"));
        const auto& g=e.at("geometry");const auto type=str(g.at("type"));
        if(type=="POINT"){keys(g,{"type","point"});v.geometry=point(g.at("point"));}
        else if(type=="LINE_SEGMENT"){keys(g,{"type","start","end"});v.geometry=LineSegment{point(g.at("start")),point(g.at("end"))};}
        else if(type=="ARC_SEGMENT") {
            keys(g,{"type","center","radius","start_deg","sweep_deg"});
            v.geometry=ArcSegment{point(g.at("center")),number(g.at("radius")),number(g.at("start_deg")),number(g.at("sweep_deg"))};
        } else throw std::invalid_argument("SETMATCH_UNKNOWN_GEOMETRY");
        s.elements.push_back(v);
    }
    return s;
}
Request decode(const J& j) {
    keys(j,{"schema","request_id","reference","target","parameters"});
    if(str(j.at("schema"))!="cxvision.geometric_set_match.request.v1")throw std::invalid_argument("SETMATCH_SCHEMA");
    Request r;r.request_id=str(j.at("request_id"));r.reference=set(j.at("reference"));r.target=set(j.at("target"));
    const auto& p=j.at("parameters");
    keys(p,{"search_roi","angle_min_deg","angle_max_deg","scale_min","scale_max","max_residual_px",
        "min_weighted_coverage","min_spatial_span_ratio","min_candidate_score_gap","min_matched_elements",
        "max_elements_per_set","max_hypotheses","max_pair_checks","max_elapsed_ms","seed"});
    auto& q=r.parameters;q.search_roi=rect(p.at("search_roi"));
#define DOUBLE_FIELD(f) q.f=number(p.at(#f))
    DOUBLE_FIELD(angle_min_deg);DOUBLE_FIELD(angle_max_deg);DOUBLE_FIELD(scale_min);DOUBLE_FIELD(scale_max);
    DOUBLE_FIELD(max_residual_px);DOUBLE_FIELD(min_weighted_coverage);
    DOUBLE_FIELD(min_spatial_span_ratio);DOUBLE_FIELD(min_candidate_score_gap);
#undef DOUBLE_FIELD
#define INT_FIELD(f) q.f=integer(p.at(#f))
    INT_FIELD(min_matched_elements);INT_FIELD(max_elements_per_set);INT_FIELD(max_hypotheses);
    INT_FIELD(max_pair_checks);INT_FIELD(max_elapsed_ms);
#undef INT_FIELD
    if(!p.at("seed").is_number_integer()||number(p.at("seed"))<0||number(p.at("seed"))>4294967295.)
        throw std::invalid_argument("SETMATCH_SEED");
    q.seed=p.at("seed").get<unsigned>();
    const auto v=Validate(r);
    if(!v.accepted)throw std::invalid_argument("SETMATCH_PREFLIGHT:"+v.reasons.front());
    return r;
}
J encode(const Result& r) {
    J j={{"schema","cxvision.geometric_set_match.result.v1"},{"request_id",r.request_id},
        {"execution_status",Name(r.execution_status)},{"solvability",nullptr},{"candidates",J::array()},
        {"reasons",r.reasons},{"evidence_refs",r.evidence_refs},{"pair_checks",r.pair_checks},
        {"elapsed_ms",r.elapsed_ms},{"search_complete",r.search_complete},{"production_eligible",false}};
    if(r.solvability)j["solvability"]=Name(*r.solvability);
    for(const auto& c:r.candidates) {
        J v={{"pose",nullptr},{"correspondences",J::array()},{"unmatched_reference_ids",c.unmatched_reference_ids},
            {"unmatched_target_ids",c.unmatched_target_ids},{"weighted_coverage",c.weighted_coverage},
            {"spatial_span_ratio",c.spatial_span_ratio},{"residual_px",c.residual_px},
            {"translation_observable",c.translation_observable},{"rotation_observable",c.rotation_observable},
            {"scale_observable",c.scale_observable}};
        if(c.pose)v["pose"]={{"translation",{{"x",c.pose->translation.x},{"y",c.pose->translation.y}}},
            {"angle_deg",c.pose->angle_deg},{"scale",c.pose->scale}};
        for(const auto& p:c.correspondences)v["correspondences"].push_back({
            {"reference_id",p.reference_id},{"target_id",p.target_id},{"score",p.score},{"residual_px",p.residual_px}});
        j["candidates"].push_back(v);
    }
    return j;
}
}
void FindSetMatch::invalidate(){ran_=false;result_={};assertions_=0;}
void FindSetMatch::clear(){invalidate();loaded_=false;request_={};snapshot_.clear();input_sha_.clear();}
void FindSetMatch::requestjson(const char* text) {
    clear();
    if(!text)throw std::invalid_argument("SETMATCH_REQUEST_REQUIRED");
    std::size_t size=0;while(size<=limit&&text[size])++size;
    if(size>limit)throw std::invalid_argument("SETMATCH_REQUEST_SIZE_LIMIT");
    std::vector<std::set<std::string>> stack;
    const auto j=J::parse(text,text+size,[&](int depth,J::parse_event_t event,J& parsed){
        if(depth>32)throw std::invalid_argument("SETMATCH_DEPTH_LIMIT");
        if(event==J::parse_event_t::object_start)stack.emplace_back();
        if(event==J::parse_event_t::key&&!stack.back().insert(parsed.get<std::string>()).second)
            throw std::invalid_argument("SETMATCH_DUPLICATE_KEY");
        if(event==J::parse_event_t::object_end)stack.pop_back();
        return true;
    });
    auto candidate=decode(j);
    request_=std::move(candidate);snapshot_=j.dump();input_sha_=cxgeom::so2::ReferenceSha256(std::string(text,size));loaded_=true;
}
void FindSetMatch::load(const char* path) {
    clear();
    if(!path||!*path)throw std::invalid_argument("SETMATCH_PATH_REQUIRED");
    std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("SETMATCH_READ_FAILED");
    std::string bytes;char buf[4096];
    while(f.read(buf,sizeof(buf))||f.gcount()) {
        bytes.append(buf,static_cast<std::size_t>(f.gcount()));
        if(bytes.size()>limit)throw std::invalid_argument("SETMATCH_REQUEST_SIZE_LIMIT");
    }
    if(!f.eof())throw std::runtime_error("SETMATCH_READ_FAILED");
    if(bytes.find('\0')!=std::string::npos)throw std::invalid_argument("SETMATCH_NUL_INPUT");
    requestjson(bytes.c_str());
}
void FindSetMatch::parameter(double value,const char* key) {
    invalidate();
    if(!loaded_||!key||!std::isfinite(value))throw std::invalid_argument("SETMATCH_PARAMETER_NOT_READY");
    J j=J::parse(snapshot_);auto& p=j["parameters"];const std::string k(key);
    if(k=="seed"||k=="min_candidate_score_gap")throw std::invalid_argument("SETMATCH_RESERVED_PARAMETER");
    if(!p.contains(k)||!p[k].is_number())throw std::invalid_argument("SETMATCH_UNKNOWN_NUMERIC_PARAMETER");
    const bool count=k=="min_matched_elements"||k=="max_elements_per_set"||k=="max_hypotheses"||
                     k=="max_pair_checks"||k=="max_elapsed_ms";
    if(count) {
        if(value<0||value>100000000||std::floor(value)!=value)throw std::invalid_argument("SETMATCH_INTEGER_REQUIRED");
        p[k]=static_cast<std::size_t>(value);
    } else p[k]=value;
    auto candidate=decode(j);request_=std::move(candidate);snapshot_=j.dump();
}
void FindSetMatch::run() {
    invalidate();if(!loaded_)throw std::runtime_error("SETMATCH_REQUEST_NOT_READY");
    result_=Match(request_);ran_=true;
}
const Result& FindSetMatch::result() const {
    if(!ran_)throw std::runtime_error("SETMATCH_RESULT_NOT_READY");
    return result_;
}
void FindSetMatch::expectstatus(const char* status) {
    if(!status||std::string(status)!=Name(result().execution_status))throw std::runtime_error("SETMATCH_STATUS_ASSERTION_FAILED");
    ++assertions_;
}
void FindSetMatch::expectcount(int count) {
    if(count<0||result().candidates.size()!=static_cast<std::size_t>(count))throw std::runtime_error("SETMATCH_COUNT_ASSERTION_FAILED");
    ++assertions_;
}
std::string FindSetMatch::receipt() const {
    const auto r=encode(result());const auto request=J::parse(snapshot_);
    return J{{"schema","cxvision.geometric_set_match.receipt.v1"},{"mode","DEVELOPMENT_FULL_SET"},
        {"production_eligible",false},{"image_extraction_performed",false},{"input_bytes_sha256",input_sha_},
        {"effective_request_sha256",cxgeom::so2::ReferenceSha256(snapshot_)},{"request",request},
        {"result",r},{"assertions_passed",assertions_}}.dump(2)+"\n";
}
void FindSetMatch::save(const char* path) {
    const auto bytes=receipt();
    if(!path||!*path)throw std::invalid_argument("SETMATCH_OUTPUT_REQUIRED");
    const std::filesystem::path dest(path),pending(std::string(path)+".pending");
    if(std::filesystem::exists(dest)||std::filesystem::exists(pending))throw std::runtime_error("SETMATCH_OUTPUT_EXISTS");
    std::ofstream f(pending,std::ios::binary);f<<bytes;f.close();
    if(!f)throw std::runtime_error("SETMATCH_WRITE_FAILED");
    std::error_code ec;std::filesystem::create_hard_link(pending,dest,ec);
    if(ec)throw std::runtime_error("SETMATCH_PUBLISH_FAILED:"+ec.message());
    std::filesystem::remove(pending,ec);
}
