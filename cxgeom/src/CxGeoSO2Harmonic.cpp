#include "CxGeoSO2Harmonic.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace cxgeom { namespace so2 {
namespace {
using Z = std::complex<double>;
constexpr double tau = 6.283185307179586476925286766559;
void require(bool ok, const char* reason) { if (!ok) throw std::invalid_argument(reason); }
bool finite(Z z) { return std::isfinite(z.real()) && std::isfinite(z.imag()); }
void validate(const Config& c) {
    require(c.sample_count >= 32 && c.sample_count <= 1024, "INVALID_SAMPLE_COUNT");
    require(c.max_order >= 1 && c.max_order < c.sample_count/2, "INVALID_MAX_ORDER");
    require(c.minimum_source_points >= 3 && c.minimum_source_points <= 4096, "INVALID_MINIMUM_POINTS");
    for (double x : {c.minimum_perimeter, c.maximum_pose_residual,
                     c.symmetry_relative_amplitude, c.peak_relative_tolerance})
        require(std::isfinite(x) && x > 0, "INVALID_NUMERIC_CONFIG");
    require(c.maximum_hypotheses >= 1 && c.maximum_hypotheses <= 1024, "INVALID_HYPOTHESIS_COUNT");
}
bool same(const Config& a, const Config& b) {
    return a.sample_count==b.sample_count && a.max_order==b.max_order &&
        a.minimum_source_points==b.minimum_source_points && a.minimum_perimeter==b.minimum_perimeter &&
        a.normalize_scale==b.normalize_scale && a.maximum_pose_residual==b.maximum_pose_residual &&
        a.symmetry_relative_amplitude==b.symmetry_relative_amplitude &&
        a.peak_relative_tolerance==b.peak_relative_tolerance && a.maximum_hypotheses==b.maximum_hypotheses;
}
double cross(Z a, Z b) { return std::imag(std::conj(a)*b); }
bool onSegment(Z p,Z a,Z b) {
    return std::abs(cross(b-a,p-a)) <= 1e-9 &&
        std::real(std::conj(p-a)*(p-b)) <= 1e-9;
}
bool intersects(Z a,Z b,Z c,Z d) {
    const double x=cross(b-a,c-a), y=cross(b-a,d-a);
    const double u=cross(d-c,a-c), v=cross(d-c,b-c);
    return (((x>1e-9 && y< -1e-9) || (x< -1e-9 && y>1e-9)) && ((u>1e-9 && v< -1e-9) || (u< -1e-9 && v>1e-9))) ||
        onSegment(c,a,b) || onSegment(d,a,b) || onSegment(a,c,d) || onSegment(b,c,d);
}
void validate(const Descriptor& d) {
    validate(d.config);
    require(d.method==Method::Dft || d.method==Method::Efd,"UNSUPPORTED_METHOD");
    require(d.frequencies.size()==static_cast<size_t>(2*d.config.max_order) &&
            d.coefficients.size()==d.frequencies.size(), "INVALID_DESCRIPTOR_SIZE");
    require(finite(d.centroid) && std::isfinite(d.scale) && d.scale>1e-9 &&
            std::isfinite(d.perimeter) && d.perimeter>=d.config.minimum_perimeter,"INVALID_DESCRIPTOR_GEOMETRY");
    size_t i=0; double energy=0;
    for(int k=-d.config.max_order;k<=d.config.max_order;++k) if(k) {
        require(d.frequencies[i]==k && finite(d.coefficients[i]),"INVALID_DESCRIPTOR_COEFFICIENT");
        energy+=std::norm(d.coefficients[i++]);
    }
    require(std::isfinite(energy) && energy>1e-20,"DEGENERATE_DESCRIPTOR_ENERGY");
}
double wrap(double x,double period) { x=std::fmod(x,period);return x<0?x+period:x; }
template<class F> double maximize(F f,double left,double right) {
    const double ratio=(std::sqrt(5.0)-1)/2;
    double a=right-ratio*(right-left), b=left+ratio*(right-left);
    double fa=f(a),fb=f(b);
    for(int i=0;i<40;++i) {
        if(fa<fb) { left=a;a=b;fa=fb;b=left+ratio*(right-left);fb=f(b); }
        else { right=b;b=a;fb=fa;a=right-ratio*(right-left);fa=f(a); }
    }
    return (left+right)/2;
}
}
Descriptor Build(const Contour& input,const Config& c,Method method) {
    validate(c);
    require(input.topology_verified,"UNVERIFIED_TOPOLOGY_LEGACY_FALLBACK");
    require(input.closed,"OPEN_CONTOUR_LEGACY_FALLBACK");
    require(input.complete,"PARTIAL_CONTOUR_LEGACY_FALLBACK");
    require(input.holes==0 && input.components==1,"UNSUPPORTED_TOPOLOGY_LEGACY_FALLBACK");
    require(input.points.size()<=4096,"SOURCE_POINT_BUDGET_EXCEEDED");
    require(method==Method::Dft || method==Method::Efd,"UNSUPPORTED_METHOD");
    std::vector<Z> v;
    for(Z z:input.points) {
        require(finite(z),"NONFINITE_CONTOUR");
        if(v.empty() || std::abs(z-v.back())>1e-9) v.push_back(z);
    }
    if(v.size()>1 && std::abs(v.front()-v.back())<=1e-9) v.pop_back();
    require(v.size()>=static_cast<size_t>(c.minimum_source_points),"INSUFFICIENT_SOURCE_POINTS");
    // Shift before area/energy arithmetic to avoid cancellation at large world coordinates.
    const Z origin=v.front();
    for(auto& z:v) z-=origin;
    const size_t n=v.size();
    for(size_t i=0;i<n;++i) {
        const Z prev=v[(i+n-1)%n], next=v[(i+1)%n];
        require(!(std::abs(cross(v[i]-prev,next-v[i]))<=1e-9 &&
                  std::real(std::conj(v[i]-prev)*(next-v[i]))<0),"SELF_INTERSECTING_CONTOUR");
        for(size_t j=i+1;j<n;++j) {
            if(j==i+1 || (i==0 && j==n-1)) continue;
            require(!intersects(v[i],v[(i+1)%n],v[j],v[(j+1)%n]),"SELF_INTERSECTING_CONTOUR");
        }
    }
    double area=0;
    for(size_t i=0;i<n;++i) area+=cross(v[i],v[(i+1)%n])/2;
    require(std::isfinite(area) && std::abs(area)>1e-9,"DEGENERATE_CONTOUR");
    if(area<0) std::reverse(v.begin(),v.end());
    std::vector<double> lengths;
    double perimeter=0;
    for(size_t i=0;i<n;++i) { double l=std::abs(v[(i+1)%n]-v[i]);lengths.push_back(l);perimeter+=l; }
    require(std::isfinite(perimeter) && perimeter>=c.minimum_perimeter,"PERIMETER_TOO_SMALL");
    Descriptor d;d.config=c;d.method=method;d.perimeter=perimeter;
    for(int k=-c.max_order;k<=c.max_order;++k) if(k) d.frequencies.push_back(k);
    Z center{};double energy=0;
    if(method==Method::Dft) {
        std::vector<Z> samples;size_t segment=0;double start=0;
        for(int j=0;j<c.sample_count;++j) {
            const double position=perimeter*j/c.sample_count;
            while(segment<n-1 && start+lengths[segment]<position) start+=lengths[segment++];
            samples.push_back(v[segment]+(position-start)/lengths[segment]*(v[(segment+1)%n]-v[segment]));
        }
        for(Z z:samples) center+=z/static_cast<double>(samples.size());
        for(Z z:samples) energy+=std::norm(z-center)/samples.size();
        for(int k:d.frequencies) {
            Z total{};
            for(int j=0;j<c.sample_count;++j)
                total+=(samples[j]-center)*std::exp(Z(0,-tau*k*j/c.sample_count));
            d.coefficients.push_back(total/static_cast<double>(c.sample_count));
        }
    } else {
        for(size_t i=0;i<n;++i) center+=(v[i]+v[(i+1)%n])*lengths[i]/(2*perimeter);
        for(size_t i=0;i<n;++i) {
            Z a=v[i]-center,b=v[(i+1)%n]-center;
            energy+=lengths[i]*(std::norm(a)+std::real(std::conj(a)*b)+std::norm(b))/(3*perimeter);
        }
        for(int k:d.frequencies) {
            double omega=tau*k/perimeter,start=0;Z total{};
            for(size_t i=0;i<n;++i) {
                total-=(v[(i+1)%n]-v[i])/lengths[i]*
                    (std::exp(Z(0,-omega*start))-std::exp(Z(0,-omega*(start+lengths[i]))))/
                    (omega*omega*perimeter);
                start+=lengths[i];
            }
            d.coefficients.push_back(total);
        }
    }
    d.centroid=center+origin;d.scale=std::sqrt(std::max(0.0,energy));
    require(std::isfinite(d.scale) && d.scale>1e-9,"DEGENERATE_SCALE");
    if(c.normalize_scale) for(auto& z:d.coefficients) z/=d.scale;
    validate(d);return d;
}
double Distance(const Descriptor& a,const Descriptor& b) {
    validate(a);validate(b);
    require(same(a.config,b.config),"INCOMPATIBLE_CONFIG");
    require(a.method==b.method,"INCOMPATIBLE_METHOD");
    double total=0;
    for(size_t i=0;i<a.coefficients.size();++i) {
        double delta=std::abs(a.coefficients[i])-std::abs(b.coefficients[i]);total+=delta*delta;
    }
    return std::sqrt(total);
}
Result Match(const Descriptor& a,const Descriptor& b) {
    Result r;r.invariant_distance=Distance(a,b);
    const auto& c=a.config;
    double top=0,er=0,eo=0;
    for(Z z:a.coefficients) { top=std::max(top,std::abs(z));er+=std::norm(z); }
    for(Z z:b.coefficients) eo+=std::norm(z);
    std::vector<int> active;std::vector<Z> crossTerms;
    for(size_t i=0;i<a.coefficients.size();++i) {
        if(std::abs(a.coefficients[i])>top*c.symmetry_relative_amplitude) active.push_back(a.frequencies[i]);
        crossTerms.push_back(std::conj(a.coefficients[i])*b.coefficients[i]);
    }
    if(active.size()<2) {
        r.status="ORIENTATION_UNOBSERVABLE";r.fallback_reason="CONTINUOUS_ROTATIONAL_SYMMETRY";return r;
    }
    for(size_t i=1;i<active.size();++i) r.symmetry_order=std::gcd(r.symmetry_order,std::abs(active[i]-active[0]));
    auto correlation=[&](double shift) {
        Z total{};
        for(size_t i=0;i<crossTerms.size();++i) total+=crossTerms[i]*std::exp(Z(0,tau*a.frequencies[i]*shift));
        return total;
    };
    auto objective=[&](double shift) { return std::abs(correlation(shift)); };
    std::vector<double> scores;
    for(int i=0;i<c.sample_count;++i) scores.push_back(objective(double(i)/c.sample_count));
    std::vector<Pose> candidates;
    for(int i=0;i<c.sample_count;++i) {
        if(scores[i]<scores[(i+c.sample_count-1)%c.sample_count] || scores[i]<scores[(i+1)%c.sample_count]) continue;
        const double shift=maximize(objective,double(i-1)/c.sample_count,double(i+1)/c.sample_count);
        const Z value=correlation(shift);
        Pose p;p.angle_deg=wrap(std::arg(value)*360/tau,360);p.scale=b.scale/a.scale;
        p.correlation=std::min(1.0,std::abs(value)/std::sqrt(er*eo));
        p.residual=std::sqrt(std::max(0.0,2-2*p.correlation));p.cyclic_shift=wrap(shift,1);
        candidates.push_back(p);
    }
    std::sort(candidates.begin(),candidates.end(),[](const Pose& x,const Pose& y) {
        return x.correlation!=y.correlation?x.correlation>y.correlation:x.angle_deg<y.angle_deg;
    });
    if(candidates.empty() || candidates.front().residual>c.maximum_pose_residual) {
        r.status="POSE_RESIDUAL_REJECTED";r.fallback_reason="SHAPE_OR_REFLECTION_MISMATCH";return r;
    }
    for(const auto& p:candidates) {
        if(candidates.front().correlation-p.correlation>c.peak_relative_tolerance) continue;
        bool duplicate=false;
        for(const auto& q:r.poses) if(std::abs(wrap(p.angle_deg-q.angle_deg+180,360)-180)<0.25) duplicate=true;
        if(!duplicate) r.poses.push_back(p);
    }
    if(r.poses.size()>static_cast<size_t>(c.maximum_hypotheses)) {
        r.poses.clear();r.status="HYPOTHESIS_BUDGET_EXCEEDED";r.fallback_reason="PRESERVE_SYMMETRY_BRANCHES";return r;
    }
    r.succeeded=true;r.status="AUDIT_POSE_HYPOTHESES";return r;
}
}}
