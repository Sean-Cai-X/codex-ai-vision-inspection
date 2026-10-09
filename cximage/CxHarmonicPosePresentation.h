#pragma once
#include "../libtorchsegmentation/src/utils/json.hpp"
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace cxharmonicui {
// Stateless: only the current receipt is rendered. Never infer production approval.
inline std::vector<std::string> PoseDisplayLines(const nlohmann::json& result) {
    const auto& poses=result.at("poses");
    if(!poses.is_array()||poses.size()>1024) throw std::runtime_error("INVALID_POSE_LIST");
    if(poses.empty()) return {"No pose candidate: translation / rotation / scale unavailable."};
    std::vector<std::string> lines;
    if(poses.size()>1) lines.push_back("Multiple candidates: ambiguity retained; none selected.");
    std::size_t index=0;
    for(const auto& p:poses) {
        auto number=[&](const char* key){
            const auto& v=p.at(key);
            if(!v.is_number()) throw std::runtime_error("INVALID_POSE_NUMBER");
            double x=v.get<double>();
            if(!std::isfinite(x)) throw std::runtime_error("NONFINITE_POSE_NUMBER");
            return x;
        };
        std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(9);
        s<<"Candidate "<<++index<<" | Angle "<<number("angle_deg")<<" deg";
        s<<" | Scale "<<number("scale")<<" | Correlation "<<number("correlation");
        s<<" | Residual "<<number("residual");
        lines.push_back(s.str());s.str("");s.clear();
        const char* keys[]={"translation_x","translation_y","cyclic_shift","mapping","coordinate_units"};
        int present=0;for(const auto* key:keys) if(p.contains(key)) ++present;
        if(present==0) {
            lines.push_back("Legacy receipt: translation / mapping / cyclic shift unavailable. Run again.");
            continue;
        }
        if(present!=5) throw std::runtime_error("INCOMPLETE_POSE_MAPPING");
        if(p.at("mapping")!="reference_to_observation"||p.at("coordinate_units")!="source_units")
            throw std::runtime_error("UNSUPPORTED_POSE_MAPPING");
        const double shift=number("cyclic_shift");
        if(shift<0||shift>=1) throw std::runtime_error("INVALID_CYCLIC_SHIFT");
        s<<"Translation X "<<number("translation_x")<<" | Y "<<number("translation_y");
        s<<" [source units] | Cyclic shift "<<shift<<" [turn of contour sampling]";
        lines.push_back(s.str());
    }
    return lines;
}
}
