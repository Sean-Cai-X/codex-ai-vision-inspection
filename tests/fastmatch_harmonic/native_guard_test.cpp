#include "CxGeoSO2Harmonic.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace cxgeom::so2;
void check(bool v) { if(!v)throw std::runtime_error("guard check failed"); }
void rejected(const std::function<void()>& f,const char* reason) {
    try { f(); } catch(const std::invalid_argument& e) { check(std::string(e.what())==reason);return; }
    throw std::runtime_error(std::string("missing rejection: ")+reason);
}
int main() {
    Contour a;a.topology_verified=true;a.closed=true;
    for(int i=0;i<64;++i) a.points.emplace_back(80*std::cos(i*6.283185307179586/64),45*std::sin(i*6.283185307179586/64));
    auto d=Build(a);
    auto bad=a;bad.topology_verified=false;
    rejected([&]{Build(bad);},"UNVERIFIED_TOPOLOGY_LEGACY_FALLBACK");
    bad=a;bad.closed=false;rejected([&]{Build(bad);},"OPEN_CONTOUR_LEGACY_FALLBACK");
    bad=a;bad.complete=false;rejected([&]{Build(bad);},"PARTIAL_CONTOUR_LEGACY_FALLBACK");
    bad=a;bad.holes=1;rejected([&]{Build(bad);},"UNSUPPORTED_TOPOLOGY_LEGACY_FALLBACK");
    bad=a;bad.components=2;rejected([&]{Build(bad);},"UNSUPPORTED_TOPOLOGY_LEGACY_FALLBACK");
    bad=a;bad.points[2]={std::numeric_limits<double>::quiet_NaN(),0};
    rejected([&]{Build(bad);},"NONFINITE_CONTOUR");
    bad=a;bad.points.resize(4097);rejected([&]{Build(bad);},"SOURCE_POINT_BUDGET_EXCEEDED");
    bad=a;std::swap(bad.points[8],bad.points[32]);
    rejected([&]{Build(bad);},"SELF_INTERSECTING_CONTOUR");
    auto invalid=d;invalid.coefficients.pop_back();
    rejected([&]{Match(d,invalid);},"INVALID_DESCRIPTOR_SIZE");
    invalid=d;invalid.coefficients[0]={std::numeric_limits<double>::infinity(),0};
    rejected([&]{Match(d,invalid);},"INVALID_DESCRIPTOR_COEFFICIENT");
    invalid=d;invalid.method=Method::Efd;
    rejected([&]{Match(d,invalid);},"INCOMPATIBLE_METHOD");
    Config c;c.sample_count=1;rejected([&]{Build(a,c);},"INVALID_SAMPLE_COUNT");
    c=Config{};c.max_order=128;rejected([&]{Build(a,c);},"INVALID_MAX_ORDER");
    c=Config{};c.maximum_pose_residual=std::numeric_limits<double>::quiet_NaN();
    rejected([&]{Build(a,c);},"INVALID_NUMERIC_CONFIG");
    c=Config{};c.maximum_hypotheses=1;
    check(Match(Build(a,c),Build(a,c)).status=="HYPOTHESIS_BUDGET_EXCEEDED");
    auto shifted=a;for(auto& z:shifted.points)z+=std::complex<double>(1e8,-1e8);
    check(Distance(Build(a,Config{},Method::Efd),Build(shifted,Config{},Method::Efd))<1e-8);
    const auto r=Match(d,d);
    check(!r.used_for_seed && !r.used_for_prefilter && !r.measurement_evidence);
    std::cout<<"native guards PASS\n";
}
