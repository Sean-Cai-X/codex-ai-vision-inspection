#include "CxSevenClassSupervision.h"
#include "../cxgeom/include/CxGeoSO2ReferenceAsset.h"
#include "../contracts/OfflineBusinessService.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>

namespace {
using J = nlohmann::json;
using cxgeom::so2::Sha256Bytes;
constexpr std::size_t file_limit = 4 * 1024 * 1024;
constexpr std::size_t pixel_limit = 16 * 1024 * 1024;
constexpr std::size_t work_limit = 200000000;
void require(bool ok, const char* code) { if (!ok) throw std::invalid_argument(code); }
void keys(const J& j, std::initializer_list<const char*> fields) {
    require(j.is_object() && j.size() == fields.size(), "SUPERVISION_FIELDS_INVALID");
    for (auto field : fields) require(j.contains(field), "SUPERVISION_FIELD_MISSING");
}
std::string text(const J& j) {
    require(j.is_string(), "SUPERVISION_STRING_REQUIRED");
    auto s = j.get<std::string>();
    require(!s.empty() && s.size() <= 1024, "SUPERVISION_STRING_INVALID");
    return s;
}
double number(const J& j) {
    require(j.is_number(), "SUPERVISION_NUMBER_REQUIRED");
    double v = j.get<double>();
    require(std::isfinite(v), "SUPERVISION_NONFINITE"); return v;
}
bool is_sha(const std::string& s) {
    return s.size() == 64 && s.find_first_not_of("0123456789abcdef") == std::string::npos;
}
J read_json(const char* path) {
    require(path && *path, "SUPERVISION_PATH_REQUIRED");
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    require(bool(file), "SUPERVISION_FILE_UNREADABLE");
    auto size = file.tellg();
    require(size > 0 && size <= static_cast<std::streamoff>(file_limit), "SUPERVISION_FILE_LIMIT");
    std::string bytes(static_cast<std::size_t>(size), '\0');
    file.seekg(0); file.read(bytes.data(), size);
    require(bool(file), "SUPERVISION_FILE_UNREADABLE");
    return cxvision::supervision::ParseJson(bytes);
}
void validate_rules(const J& r) {
    J expected = {
        {"schema","visionai.seven_class_supervision_rules.v1"}, {"version","1.0.0"},
        {"classes",vision_ai::offline_business::v1::kGeometryContract},
        {"open_classes",{"arc","line","open_curve"}},
        {"coordinates","original_image_integer_pixel_centers_xy_top_left_subpixel_vertices"},
        {"input_geometry","explicit_ordered_polyline_or_simple_ring_no_fitting"},
        {"open_width_px",3.0}, {"sample_max_step_px",1.0},
        {"stroke","euclidean_distance_to_original_segments_round_caps_round_joins"},
        {"fill","even_odd_pixel_center_with_boundary_included"}, {"background",255},
        {"self_intersections","reject"}, {"open_stroke_raster_holes","reject"}, {"holes","reject"},
        {"out_of_bounds_vertices","reject"},
        {"stroke_at_image_border","clip_raster_preserve_source_endpoints_and_report"},
        {"cross_instance_touching_or_overlap","reject_8_connected"},
        {"unlabelled_verify","no_mask_not_training"}, {"open_mask_is_region_not_centerline",true},
        {"admission",{"development_trial","isolated_business_validation"}}
    };
    require(r.is_object() && r.contains("open_width_px") && r.contains("sample_max_step_px"),
            "SUPERVISION_RULES_INVALID");
    double width = number(r.at("open_width_px")), step = number(r.at("sample_max_step_px"));
    require(width >= 1 && width <= 128, "SUPERVISION_WIDTH_INVALID");
    require(step >= 0.125 && step <= 64, "SUPERVISION_STEP_INVALID");
    expected["open_width_px"] = r.at("open_width_px");
    expected["sample_max_step_px"] = r.at("sample_max_step_px");
    require(r == expected, "SUPERVISION_RULES_UNSUPPORTED");
}
struct P { double x, y; };
double cross(P a, P b, P c) { return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x); }
double dist2(P a, P b) { return (a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y); }
double segment_distance2(P p, P a, P b) {
    double t = std::clamp(((p.x-a.x)*(b.x-a.x)+(p.y-a.y)*(b.y-a.y))/dist2(a,b),0.0,1.0);
    return dist2(p,{a.x+t*(b.x-a.x),a.y+t*(b.y-a.y)});
}
bool intersects(P a, P b, P c, P d) {
    double x=cross(a,b,c), y=cross(a,b,d), z=cross(c,d,a), w=cross(c,d,b);
    if (((x>0 && y<0)||(x<0 && y>0)) && ((z>0 && w<0)||(z<0 && w>0))) return true;
    return segment_distance2(c,a,b)<1e-18 || segment_distance2(d,a,b)<1e-18 ||
           segment_distance2(a,c,d)<1e-18 || segment_distance2(b,c,d)<1e-18;
}
// Foreground: 8-connected, exterior: 4-connected. Reject raster topology loss.
void raster_topology(const std::vector<unsigned char>& mask, int width, int height) {
    std::vector<unsigned char> seen(mask.size(),0);
    std::vector<std::size_t> queue;
    auto flood = [&](std::size_t start, bool foreground) {
        queue.clear(); queue.push_back(start); seen[start]=1;
        for (std::size_t i=0;i<queue.size();++i) {
            int x=int(queue[i]%width), y=int(queue[i]/width);
            for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
                if ((!dx&&!dy) || (!foreground && dx && dy)) continue;
                int nx=x+dx, ny=y+dy;
                if(nx<0||ny<0||nx>=width||ny>=height) continue;
                auto n=std::size_t(ny)*width+nx;
                if(!seen[n] && bool(mask[n])==foreground) { seen[n]=1; queue.push_back(n); }
            }
        }
    };
    int components=0;
    for(std::size_t i=0;i<mask.size();++i) if(mask[i] && !seen[i]) { flood(i,true); ++components; }
    require(components==1,"SUPERVISION_RASTER_DISCONNECTED_OR_EMPTY");
    for(int y=0;y<height;++y) for(int x=0;x<width;++x) {
        auto i=std::size_t(y)*width+x;
        if((x==0||y==0||x==width-1||y==height-1) && !mask[i] && !seen[i]) flood(i,false);
    }
    require(std::find(seen.begin(),seen.end(),0)==seen.end(),"SUPERVISION_RASTER_HOLE");
}
}

namespace cxvision::supervision {
std::string DecodeBusinessSha256(const std::string& digest) {
    require(digest.size()==71 && digest.compare(0,7,"sha256:")==0,"SUPERVISION_RUNTIME_SHA_INVALID");
    auto hex=digest.substr(7);
    for(char& c:hex) if(c>='A'&&c<='F') c=char(c-'A'+'a');
    require(is_sha(hex),"SUPERVISION_RUNTIME_SHA_INVALID");
    return hex;
}
J ParseJson(const std::string& bytes) {
    require(!bytes.empty() && bytes.size()<=file_limit,"SUPERVISION_FILE_LIMIT");
    try {
        std::vector<std::set<std::string>> object_keys;
        return J::parse(bytes,[&](int, J::parse_event_t event,J& parsed) {
            if(event==J::parse_event_t::object_start) object_keys.emplace_back();
            if(event==J::parse_event_t::key)
                require(object_keys.back().insert(parsed.get<std::string>()).second,"SUPERVISION_DUPLICATE_JSON_KEY");
            if(event==J::parse_event_t::object_end) object_keys.pop_back();
            return true;
        });
    } catch(const J::exception&) { throw std::invalid_argument("SUPERVISION_JSON_INVALID"); }
}
Conversion Convert(const J& sample, const J& rules) {
    validate_rules(rules);
    require(sample.dump().size() <= file_limit, "SUPERVISION_INPUT_LIMIT");
    require(sample.contains("split"),"SUPERVISION_FIELD_MISSING");
    const auto split = text(sample.at("split"));
    const bool verify = split=="verify";
    require(verify||split=="train"||split=="valid","SUPERVISION_SPLIT_INVALID");
    if(verify) keys(sample,{"asset_ref","source_group","split","width","height","image_sha256","annotations"});
    else keys(sample,{"asset_ref","source_group","split","width","height","image_sha256","annotation_receipt_digest","annotations"});
    text(sample.at("asset_ref")); text(sample.at("source_group"));
    require(is_sha(text(sample.at("image_sha256"))),"SUPERVISION_IMAGE_SHA_INVALID");
    if(!verify) require(is_sha(text(sample.at("annotation_receipt_digest"))),"SUPERVISION_ANNOTATION_SHA_INVALID");
    require(sample.at("width").is_number_integer() && sample.at("height").is_number_integer(),"SUPERVISION_DIMENSIONS_INVALID");
    double w=number(sample.at("width")), h=number(sample.at("height"));
    require(w>=1 && h>=1 && w*h<=pixel_limit,"SUPERVISION_PIXEL_LIMIT");
    Conversion out; out.width=int(w); out.height=int(h);
    const auto& annotations=sample.at("annotations");
    require(annotations.is_array() && annotations.size()<=128,"SUPERVISION_ANNOTATIONS_INVALID");
    require(verify ? annotations.empty() : !annotations.empty(),"SUPERVISION_ANNOTATIONS_FOR_SPLIT_INVALID");
    out.receipt={{"schema","visionai.seven_class_conversion.v1"},{"status",verify?"VERIFY_NO_MASK":"CONVERTED"},
        {"admission","development_trial"},{"rules_version",rules.at("version")},
        {"rules_sha256",Sha256Bytes(rules.dump())},{"annotation_source",sample},
        {"source_sha256",Sha256Bytes(sample.dump())},{"instances",J::array()},
        {"source_image_digest_verified",false}};
    if(!verify) out.mask.assign(std::size_t(out.width)*out.height,255);
    std::set<std::string> ids;
    std::size_t work=0, sample_count=0;
    const double radius=number(rules.at("open_width_px"))/2, step=number(rules.at("sample_max_step_px"));
    for(const auto& a:annotations) {
        keys(a,{"id","class_name","points","closed"});
        require(ids.insert(text(a.at("id"))).second,"SUPERVISION_DUPLICATE_INSTANCE");
        auto name=text(a.at("class_name"));
        const auto& names=vision_ai::offline_business::v1::kGeometryContract;
        auto it=std::find(names.begin(),names.end(),name);
        require(it!=names.end(),"SUPERVISION_CLASS_INVALID");
        const unsigned char class_id=static_cast<unsigned char>(it-names.begin());
        const bool closed=!(name=="arc"||name=="line"||name=="open_curve");
        require(a.at("closed").is_boolean() && a.at("closed").get<bool>()==closed,"SUPERVISION_TOPOLOGY_MISMATCH");
        const auto& points=a.at("points");
        require(points.is_array() && points.size()>=(closed?3u:2u) && points.size()<=2048,"SUPERVISION_POINT_COUNT");
        require(name!="line" || points.size()==2,"SUPERVISION_LINE_ENDPOINTS_REQUIRED");
        // Include validation and full-frame topology walks in the budget,
        // not just segment/pixel raster evaluations.
        work+=points.size()*points.size()+out.mask.size()*12;
        require(work<=work_limit,"SUPERVISION_WORK_LIMIT");
        std::vector<P> p;
        for(const auto& v:points) {
            require(v.is_array()&&v.size()==2,"SUPERVISION_POINT_INVALID");
            P q{number(v[0]),number(v[1])};
            require(q.x>=0 && q.y>=0 && q.x<=w-1 && q.y<=h-1,"SUPERVISION_VERTEX_OUT_OF_BOUNDS");
            for(auto prev:p) require(dist2(q,prev)>1e-18,"SUPERVISION_REPEATED_VERTEX");
            p.push_back(q);
        }
        const std::size_t edges=closed?p.size():p.size()-1;
        for(std::size_t i=0;i<p.size();++i) {
            if(!closed && (i==0||i+1==p.size())) continue;
            P prev=p[(i+p.size()-1)%p.size()], cur=p[i], next=p[(i+1)%p.size()];
            require(!(std::abs(cross(prev,cur,next))<1e-9 &&
                (prev.x-cur.x)*(next.x-cur.x)+(prev.y-cur.y)*(next.y-cur.y)>0),"SUPERVISION_SEGMENT_BACKTRACK");
        }
        for(std::size_t i=0;i<edges;++i) for(std::size_t j=i+1;j<edges;++j) {
            if(j==i+1 || (closed && i==0 && j==edges-1)) continue;
            require(!intersects(p[i],p[(i+1)%p.size()],p[j],p[(j+1)%p.size()]),"SUPERVISION_SELF_INTERSECTION");
        }
        double area=0;
        for(std::size_t i=0;i<edges;++i) area+=cross({0,0},p[i],p[(i+1)%p.size()]);
        require(!closed || std::abs(area)>1e-9,"SUPERVISION_DEGENERATE_RING");
        J samples=J::array();
        for(std::size_t i=0;i<edges;++i) {
            P a0=p[i], b=p[(i+1)%p.size()];
            int count=int(std::ceil(std::sqrt(dist2(a0,b))/step));
            sample_count+=count;
            require(sample_count<=1000000,"SUPERVISION_SAMPLE_LIMIT");
            for(int k=0;k<count;++k) { double t=double(k)/count; samples.push_back({a0.x+t*(b.x-a0.x),a0.y+t*(b.y-a0.y)}); }
        }
        if(!closed) samples.push_back(points.back());
        double minx=w, miny=h, maxx=0, maxy=0;
        for(auto q:p) {minx=std::min(minx,q.x);miny=std::min(miny,q.y);maxx=std::max(maxx,q.x);maxy=std::max(maxy,q.y);}
        double pad=closed?0:radius;
        int x0=std::max(0,int(std::floor(minx-pad))), y0=std::max(0,int(std::floor(miny-pad)));
        int x1=std::min(out.width-1,int(std::ceil(maxx+pad))), y1=std::min(out.height-1,int(std::ceil(maxy+pad)));
        work+=std::size_t(x1-x0+1)*(y1-y0+1)*edges;
        require(work<=work_limit,"SUPERVISION_WORK_LIMIT");
        std::vector<unsigned char> local(out.mask.size(),0);
        bool border=false;
        for(int y=y0;y<=y1;++y) for(int x=x0;x<=x1;++x) {
            bool inside=false, boundary=false;
            for(std::size_t i=0;i<edges;++i) {
                P a0=p[i], b=p[(i+1)%p.size()];
                double d=segment_distance2({double(x),double(y)},a0,b);
                if(d <= (closed?1e-18:radius*radius)) boundary=true;
                if(closed && ((a0.y>y)!=(b.y>y)) && x<(b.x-a0.x)*(y-a0.y)/(b.y-a0.y)+a0.x) inside=!inside;
            }
            if(boundary||inside) {
                for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
                    int nx=x+dx, ny=y+dy;
                    if(nx>=0&&ny>=0&&nx<out.width&&ny<out.height)
                        require(out.mask[std::size_t(ny)*out.width+nx]==255,"SUPERVISION_INSTANCES_TOUCH_OR_OVERLAP");
                }
                local[std::size_t(y)*out.width+x]=1;
                border=border||x==0||y==0||x==out.width-1||y==out.height-1;
            }
        }
        raster_topology(local,out.width,out.height);
        for(std::size_t i=0;i<local.size();++i) if(local[i]) out.mask[i]=class_id;
        J endpoints=J::array(); if(!closed) {endpoints.push_back(points.front());endpoints.push_back(points.back());}
        out.receipt["instances"].push_back({{"id",a.at("id")},{"class_id",class_id},{"closed",closed},
            {"source_points",points},{"endpoints",endpoints},{"samples",samples},{"touches_image_border",border}});
    }
    out.receipt["mask_sha256"]=verify?J(nullptr):J(Sha256Bytes(std::string(out.mask.begin(),out.mask.end())));
    out.receipt["mask_hash_encoding"]="row_major_u8_without_header";
    // Bind rules, source, split and raster, not merely a file name/revision label.
    J binding={{"schema","visionai.supervision_sample_binding.v1"},{"rules_sha256",out.receipt["rules_sha256"]},
        {"source_sha256",out.receipt["source_sha256"]},{"mask_sha256",out.receipt["mask_sha256"]}};
    out.receipt["sample_binding_sha256"]=Sha256Bytes(binding.dump());
    return out;
}
}

namespace cxvision::supervision {
J FreezeDataset(const J& project,const J& rules) {
    validate_rules(rules);
    keys(project,{"project_class","samples"});
    require(project.dump().size()<=file_limit,"SUPERVISION_INPUT_LIMIT");
    auto name=text(project.at("project_class"));
    const auto& names=vision_ai::offline_business::v1::kGeometryContract;
    require(std::find(names.begin(),names.end(),name)!=names.end(),"SUPERVISION_CLASS_INVALID");
    const auto& samples=project.at("samples");
    require(samples.is_array() && samples.size()>=4 && samples.size()<=128,"SUPERVISION_DATASET_SAMPLE_COUNT");
    std::map<std::string,J> ordered;
    std::map<std::string,std::string> groups,images;
    std::map<std::string,int> counts;
    std::size_t pixels=0, point_work=0;
    for(const auto& sample:samples) {
        require(sample.is_object() && sample.contains("asset_ref") && sample.contains("split") &&
            sample.contains("source_group") && sample.contains("image_sha256") &&
            sample.contains("width") && sample.contains("height") && sample.contains("annotations"),"SUPERVISION_FIELD_MISSING");
        auto id=text(sample.at("asset_ref")), split=text(sample.at("split"));
        require(id.size()<=256 && id.find_first_not_of(
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos,
            "SUPERVISION_DATASET_ASSET_ID_INVALID");
        require(ordered.emplace(id,sample).second,"SUPERVISION_DATASET_DUPLICATE_ASSET");
        auto isolate=[&](auto& map,const std::string& value) {
            auto inserted=map.emplace(value,split);
            require(inserted.second || inserted.first->second==split,"SUPERVISION_DATASET_SPLIT_LEAKAGE");
        };
        isolate(groups,text(sample.at("source_group")));
        isolate(images,text(sample.at("image_sha256")));
        double area=number(sample.at("width"))*number(sample.at("height"));
        require(area>=1 && area<=pixel_limit,"SUPERVISION_PIXEL_LIMIT");
        pixels+=static_cast<std::size_t>(area);
        require(pixels<=64*1024*1024,"SUPERVISION_DATASET_PIXEL_LIMIT");
        require(sample.at("annotations").is_array(),"SUPERVISION_ANNOTATIONS_INVALID");
        for(const auto& annotation:sample.at("annotations")) {
            require(annotation.is_object() && annotation.contains("class_name") && annotation.contains("points"),"SUPERVISION_FIELD_MISSING");
            require(text(annotation.at("class_name"))==name,"SUPERVISION_DATASET_CLASS_MIXED");
            const auto n=annotation.at("points").size();
            require(n<=2048,"SUPERVISION_POINT_COUNT");
            point_work+=n*n+static_cast<std::size_t>(area)*(n+12);
            require(point_work<=work_limit,"SUPERVISION_DATASET_WORK_LIMIT");
        }
        ++counts[split];
    }
    require(counts["train"]>=2 && counts["valid"]>=1 && counts["verify"]>=1,"SUPERVISION_DATASET_SPLIT_INSUFFICIENT");
    J canonical={{"project_class",name},{"samples",J::array()}};
    J records=J::array();
    for(const auto& item:ordered) {
        auto conversion=Convert(item.second,rules);
        canonical["samples"].push_back(item.second);
        records.push_back({{"asset_ref",item.first},{"split",item.second.at("split")},
            {"source_sha256",conversion.receipt.at("source_sha256")},
            {"mask_sha256",conversion.receipt.at("mask_sha256")},
            {"sample_binding_sha256",conversion.receipt.at("sample_binding_sha256")}});
    }
    J frozen={{"schema","visionai.seven_class_dataset.v1"},{"admission","development_trial"},
        {"rules",rules},{"rules_version",rules.at("version")},{"rules_sha256",Sha256Bytes(rules.dump())},
        {"project",canonical},{"records",records},{"source_image_bytes_verified",false}};
    auto sha=Sha256Bytes(frozen.dump());
    frozen["dataset_sha256"]=sha;
    frozen["dataset_revision_id"]="sv1-"+sha;
    require(frozen.dump().size()<=file_limit,"SUPERVISION_FILE_LIMIT");
    return frozen;
}

void ValidateMaterializedDataset(const J& frozen,const J& materialized,
    const std::string& revision,const std::string& project_class) {
    require(frozen.is_object() && frozen.contains("project") && frozen.contains("rules"),"SUPERVISION_DATASET_INVALID");
    // Recompute conversions/digests: never trust caller-supplied binding hashes.
    auto expected=FreezeDataset(frozen.at("project"),frozen.at("rules"));
    require(expected==frozen,"SUPERVISION_DATASET_DIGEST_MISMATCH");
    require(expected.at("dataset_revision_id")==revision &&
        expected.at("project").at("project_class")==project_class,"SUPERVISION_DATASET_BINDING_MISMATCH");
    require(materialized.is_array(),"SUPERVISION_MATERIALIZATION_INVALID");
    std::map<std::string,J> sources,bindings;
    for(const auto& s:expected.at("project").at("samples"))
        if(s.at("split")!="verify") sources.emplace(text(s.at("asset_ref")),s);
    for(const auto& b:expected.at("records")) bindings.emplace(text(b.at("asset_ref")),b);
    require(materialized.size()==sources.size(),"SUPERVISION_MATERIALIZATION_COUNT_MISMATCH");
    std::set<std::string> seen;
    for(const auto& row:materialized) {
        keys(row,{"asset_ref","split","image_sha256","mask_pixels_sha256","width","height"});
        auto id=text(row.at("asset_ref"));
        require(sources.count(id) && seen.insert(id).second,"SUPERVISION_MATERIALIZATION_ASSET_MISMATCH");
        const auto& src=sources.at(id);
        const std::string runtime_split=src.at("split")=="valid"?"val":"train";
        require(row.at("split")==runtime_split,"SUPERVISION_MATERIALIZATION_SPLIT_MISMATCH");
        require(row.at("image_sha256")==src.at("image_sha256") &&
            row.at("mask_pixels_sha256")==bindings.at(id).at("mask_sha256") &&
            row.at("width")==src.at("width") && row.at("height")==src.at("height"),
            "SUPERVISION_MATERIALIZATION_CONTENT_MISMATCH");
    }
}
}

void CxSevenClassSupervision::invalidate() { result_={}; frozen_=nullptr; status_="NOT_RUN"; }
void CxSevenClassSupervision::clear() { rules_=nullptr; sample_=nullptr; invalidate(); }
void CxSevenClassSupervision::loadrules(const char* path) {
    invalidate(); rules_=nullptr;
    auto r=read_json(path); validate_rules(r); rules_=std::move(r);
}
void CxSevenClassSupervision::load(const char* path) { invalidate(); sample_=nullptr; sample_=read_json(path); }
void CxSevenClassSupervision::run() {
    invalidate();
    try { result_=cxvision::supervision::Convert(sample_,rules_); status_=result_.receipt.at("status").get<std::string>(); }
    catch(const std::invalid_argument& e) { status_=e.what(); }
    catch(const J::exception&) { status_="SUPERVISION_JSON_INVALID"; }
}
void CxSevenClassSupervision::freeze() {
    invalidate();
    try { frozen_=cxvision::supervision::FreezeDataset(sample_,rules_); status_="DATASET_FROZEN"; }
    catch(const std::invalid_argument& e) { status_=e.what(); }
    catch(const J::exception&) { status_="SUPERVISION_JSON_INVALID"; }
}
void CxSevenClassSupervision::savefreeze(const char* path) {
    require(status_=="DATASET_FROZEN","SUPERVISION_SUCCESS_REQUIRED");
    require(path&&*path,"SUPERVISION_PATH_REQUIRED");
    std::filesystem::path dir(path);
    require(std::filesystem::create_directory(dir),"SUPERVISION_OUTPUT_EXISTS");
    std::ofstream file(dir/"supervision.v1.json",std::ios::binary);
    file<<frozen_.dump(); file.close();
    require(bool(file),"SUPERVISION_EXPORT_FAILED");
}
void CxSevenClassSupervision::expectstatus(const char* expected) {
    require(expected && status_==expected,"SUPERVISION_STATUS_ASSERTION_FAILED");
}
void CxSevenClassSupervision::save(const char* path) {
    require(status_=="CONVERTED"||status_=="VERIFY_NO_MASK","SUPERVISION_SUCCESS_REQUIRED");
    require(path&&*path,"SUPERVISION_PATH_REQUIRED");
    // Never overwrite test images, receipts or previous runs. A failed partial
    // export remains incomplete (no receipt); caller must choose a new directory.
    std::filesystem::path dir(path);
    require(std::filesystem::create_directory(dir),"SUPERVISION_OUTPUT_EXISTS");
    if(!result_.mask.empty()) {
        std::ofstream mask(dir/"supervision.pgm",std::ios::binary);
        mask<<"P5\n"<<result_.width<<' '<<result_.height<<"\n255\n";
        mask.write(reinterpret_cast<const char*>(result_.mask.data()),result_.mask.size());
        mask.close(); require(bool(mask),"SUPERVISION_EXPORT_FAILED");
    }
    std::ofstream receipt(dir/"conversion_receipt.json",std::ios::binary);
    receipt<<result_.receipt.dump(2)<<'\n'; receipt.close();
    require(bool(receipt),"SUPERVISION_EXPORT_FAILED");
}
