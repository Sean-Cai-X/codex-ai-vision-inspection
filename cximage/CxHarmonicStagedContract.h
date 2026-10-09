#pragma once
#include "../cxgeom/include/CxGeoSO2StagedSearch.h"
#include "../libtorchsegmentation/src/utils/json.hpp"
#include <cmath>
#include <optional>
#include <stdexcept>
namespace cxharmonic {
struct StagedSettings { bool enabled=false; cxgeom::so2::StagedConfig config; };
inline bool SetStagedParameter(StagedSettings& s,double v,const std::string& key){
    if(key.rfind("staged_",0)!=0)return false;
    if(!std::isfinite(v)||std::floor(v)!=v||v<0||v>262144)
        throw std::invalid_argument("INVALID_STAGED_PARAMETER_INTEGER");
    const int n=static_cast<int>(v);
    if(key=="staged_search"||key=="staged_capture_response"){
        if(n>1)throw std::invalid_argument("INVALID_STAGED_PARAMETER_BOOLEAN");
        if(key=="staged_search")s.enabled=n!=0;else s.config.capture_response=n!=0;
    }
    else if(key=="staged_coarse_order")s.config.coarse_order=n;
    else if(key=="staged_fine_order")s.config.fine_order=n;
    else if(key=="staged_coarse_samples")s.config.coarse_samples=n;
    else if(key=="staged_fine_samples")s.config.fine_samples=n;
    else if(key=="staged_refinement_iterations")s.config.refinement_iterations=n;
    else if(key=="staged_maximum_evaluations")s.config.maximum_evaluations=n;
    else throw std::invalid_argument("UNKNOWN_STAGED_PARAMETER");
    return true;
}
inline nlohmann::json StagedConfigJson(const cxgeom::so2::StagedConfig& c){
    return {{"coarse_order",c.coarse_order},{"fine_order",c.fine_order},
        {"coarse_samples",c.coarse_samples},{"fine_samples",c.fine_samples},
        {"refinement_iterations",c.refinement_iterations},
        {"maximum_evaluations",c.maximum_evaluations},{"capture_response",c.capture_response}};
}
inline nlohmann::json StagedReceipt(const StagedSettings& s,
    const std::optional<cxgeom::so2::StagedResult>& result,const std::string& reason){
    using J=nlohmann::json;
    J j={{"schema","cxvision.harmonic_staged_search.v1"},{"enabled",s.enabled},
        {"semantics","cyclic_shift_grid_with_analytic_rotation_not_angle_scan"},
        {"requested",StagedConfigJson(s.config)},{"executed",nullptr},
        {"status",s.enabled?"NOT_RUN":"DISABLED"},{"reason",s.enabled?reason:""},
        {"search_complete",false},{"continuous_global_optimum_certified",false},
        {"evaluations",0},{"coarse_peaks",0},{"fine_peaks",0},
        {"coarse_response",J::array()},{"fine_response",J::array()}};
    if(!s.enabled||!result)return j;
    const auto& r=*result;
    j["executed"]=StagedConfigJson(r.executed);
    j["search_complete"]=r.search_complete;
    j["evaluations"]=r.evaluations;
    j["coarse_peaks"]=r.coarse_peaks;j["fine_peaks"]=r.fine_peaks;
    j["reason"]=r.match.status;
    j["status"]=r.search_complete?"COMPLETED":"INCOMPLETE";
    if(r.match.status=="STAGED_SEARCH_BUDGET_EXHAUSTED")j["status"]="BUDGET_EXHAUSTED";
    auto samples=[](const std::vector<cxgeom::so2::ResponseSample>& rows){
        J a=J::array();for(const auto& x:rows)
            a.push_back({{"shift_turns",x.shift_turns},{"angle_deg",x.angle_deg},
                {"correlation",x.correlation},{"residual",x.residual}});
        return a;
    };
    j["coarse_response"]=samples(r.coarse_response);
    j["fine_response"]=samples(r.fine_response);
    return j;
}
}
