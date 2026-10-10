#pragma once
#include "matching.h"
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>
#include <stdexcept>
namespace cxgeom::polar {
namespace receipt_detail {
inline std::string quote(const std::string& s){
 std::ostringstream o;o<<'"';
 for(unsigned char c:s){
  if(c=='"'||c=='\\')o<<'\\'<<char(c);
  else if(c<32)o<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<int(c)<<std::dec;
  else o<<char(c);
 }
 o<<'"';return o.str();
}
}
// Offline evidence, not a signature or production approval. No image/feature coordinates.
inline std::string ReceiptV1(const MatchResult& r){
 if(!r.search_complete&&!r.candidates.empty())throw std::invalid_argument("POLAR_RECEIPT_INCOMPLETE_CANDIDATES");
 const bool accepted=r.status=="FULL_SET_AUDIT_CANDIDATES"||r.status=="AMBIGUOUS";
 if(r.status.empty()||r.reason.empty()||r.reference_source.empty()||r.target_source.empty()||
    accepted!=!r.candidates.empty())throw std::invalid_argument("POLAR_RECEIPT_INVALID_STATE");
 if(r.production_eligible)throw std::invalid_argument("POLAR_RECEIPT_PRODUCTION_CLAIM");
 std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17);
 auto str=[&](const char* k,const std::string& v){o<<receipt_detail::quote(k)<<':'<<receipt_detail::quote(v)<<',';};
 auto num=[&](const char* k,double v){
  if(!std::isfinite(v))throw std::invalid_argument("POLAR_RECEIPT_NONFINITE");
  o<<receipt_detail::quote(k)<<':'<<v<<',';
 };
 o<<'{';str("schema","polar_full_match_receipt.v1");str("reference_source",r.reference_source);
 str("target_source",r.target_source);str("status",r.status);str("reason",r.reason);
 o<<"\"production_eligible\":false,\"search_complete\":"<<(r.search_complete?"true":"false")<<',';
 num("phase_order",r.phase_order);num("phase_channel",r.phase_channel);
 num("evaluated_candidates",r.evaluated_candidates);num("pair_checks",r.pair_checks);
 num("angle_rejected",r.angle_rejected);
 o<<"\"executed\":{";const auto& c=r.executed;
 num("bins",c.encoding.bins);num("max_order",c.encoding.max_order);
 num("minimum_rms_radius",c.encoding.minimum_rms_radius);
 num("angle_min_deg",c.angle_min_deg);num("angle_max_deg",c.angle_max_deg);
 num("scale_min",c.scale_min);num("scale_max",c.scale_max);
 num("max_residual_px",c.max_residual_px);num("max_spectral_distance",c.max_spectral_distance);
 num("min_phase_amplitude",c.min_phase_amplitude);num("maximum_candidates",c.maximum_candidates);
 o<<"\"maximum_pair_checks\":"<<c.maximum_pair_checks<<"},\"candidates\":[";
 bool first=true;
 for(const auto& p:r.candidates){
  if(!first){o<<',';} first=false;o<<'{';
  num("angle_deg",p.angle_deg);num("scale",p.scale);
  num("translation_x",p.translation.real());num("translation_y",p.translation.imag());
  num("observed_rms_px",p.rms_px);num("observed_max_px",p.max_px);
  num("spectral_distance",p.spectral_distance);
  o<<"\"correspondence_ambiguous\":"<<(p.correspondence_ambiguous?"true":"false")<<",\"pairs\":[";
  bool firstPair=true;
  for(const auto& pair:p.pairs){
   if(!firstPair){o<<',';} firstPair=false;o<<'{';
   str("reference_id",pair.reference_id);str("target_id",pair.target_id);
   if(!std::isfinite(pair.residual_px))throw std::invalid_argument("POLAR_RECEIPT_NONFINITE");
   o<<"\"residual_px\":"<<pair.residual_px<<'}';
  }o<<"]}";
 }o<<"]}";return o.str();
}
}
