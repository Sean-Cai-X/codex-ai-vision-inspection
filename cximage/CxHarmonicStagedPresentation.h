#pragma once
#include "../libtorchsegmentation/src/utils/json.hpp"
#include <cmath>
#include <string>
#include <vector>
#include <stdexcept>
namespace cxharmonicui {
struct StagedDisplay {
 std::vector<std::string> lines;
 std::vector<float> coarse, fine;
};
inline StagedDisplay StagedDisplayData(const nlohmann::json& run) {
 using J=nlohmann::json;StagedDisplay out;
 auto need=[](bool b){if(!b)throw std::runtime_error("INVALID_STAGED_RECEIPT");};
 if(!run.contains("staged_search")) {
  out.lines.push_back("Legacy receipt: staged search unavailable. Run again.");return out;
 }
 const auto& s=run.at("staged_search");
 need(s.at("schema")=="cxvision.harmonic_staged_search.v1");
 need(s.at("semantics")=="cyclic_shift_grid_with_analytic_rotation_not_angle_scan");
 need(s.at("continuous_global_optimum_certified")==false);
 const auto status=s.at("status").get<std::string>();
 const bool idle=status=="DISABLED"||status=="NOT_RUN";
 need(idle||status=="COMPLETED"||status=="INCOMPLETE"||status=="BUDGET_EXHAUSTED");
 need(s.at("enabled").is_boolean()&&s.at("search_complete").is_boolean());
 need(s.at("enabled").get<bool>()==(status!="DISABLED"));
 need(s.at("search_complete").get<bool>()==(status=="COMPLETED"));
 auto integer=[&](const J& j,const char* key,int max){
  const auto& v=j.at(key);need(v.is_number_integer());
  double n=v.get<double>();need(n>=0&&n<=max);return static_cast<int>(n);
 };
 auto config=[&](const J& c,const char* label) {
  for(const char* k:{"coarse_order","fine_order","coarse_samples","fine_samples",
      "refinement_iterations","maximum_evaluations"})integer(c,k,262144);
  need(c.at("capture_response").is_boolean());
  out.lines.push_back(std::string(label)+": "+c.dump());
 };
 out.lines.push_back("Staged search: "+status+" | "+s.at("reason").get<std::string>());
 config(s.at("requested"),"Requested");
 need(idle==s.at("executed").is_null());
 if(!idle)config(s.at("executed"),"Executed");
 const int evaluations=integer(s,"evaluations",262144);
 integer(s,"coarse_peaks",2048);integer(s,"fine_peaks",2048);
 if(idle)need(evaluations==0);
 else need(evaluations<=integer(s.at("executed"),"maximum_evaluations",262144));
 out.lines.push_back("Spectral evaluations: "+std::to_string(evaluations)+
  " | Coarse peaks: "+s.at("coarse_peaks").dump()+" | Fine peaks: "+s.at("fine_peaks").dump());
 out.lines.push_back("X: cyclic shift [0,1) turns. Y: correlation [0,1], not probability.");
 out.lines.push_back("Analytic rotation per shift; NOT angle scan or certified continuous global optimum.");
 auto response=[&](const char* key,std::vector<float>& values) {
  const auto& rows=s.at(key);need(rows.is_array()&&rows.size()<=2048);
  if(idle || !s.at("requested").at("capture_response").get<bool>())need(rows.empty());
  double previous=-1;
  for(const auto& row:rows) {
   auto number=[&](const char* k){
    need(row.at(k).is_number());double v=row.at(k).get<double>();need(std::isfinite(v));return v;
   };
   double shift=number("shift_turns"),corr=number("correlation");
   need(shift>=0&&shift<1&&shift>previous&&corr>=0&&corr<=1);previous=shift;
   double angle=number("angle_deg");need(angle>=0&&angle<360&&number("residual")>=0);
   values.push_back(static_cast<float>(corr));
  }
 };
 response("coarse_response",out.coarse);
 response("fine_response",out.fine);
 if(out.coarse.empty()&&out.fine.empty())
  out.lines.push_back("No response samples recorded; capture requires explicit debug mode.");
 if(status=="BUDGET_EXHAUSTED"||status=="INCOMPLETE")
  out.lines.push_back("Partial search only: no accepted pose. Curves may be truncated.");
 return out;
}
}
