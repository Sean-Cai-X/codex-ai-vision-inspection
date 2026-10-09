#include "CxGeoSO2StagedSearch.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <stdexcept>
namespace cxgeom { namespace so2 {
namespace {
using Z=std::complex<double>;
constexpr double tau=6.283185307179586476925286766559;
double wrap(double x,double period){x=std::fmod(x,period);return x<0?x+period:x;}
void need(bool b,const char* why){if(!b)throw std::invalid_argument(why);}
struct Spectrum {
    std::vector<int> frequencies;
    std::vector<Z> terms;
    double denominator=0;
};
Spectrum spectrum(const Descriptor& a,const Descriptor& b,int order){
    Spectrum s;double ea=0,eb=0;
    for(std::size_t i=0;i<a.frequencies.size();++i)if(std::abs(a.frequencies[i])<=order){
        s.frequencies.push_back(a.frequencies[i]);
        s.terms.push_back(std::conj(a.coefficients[i])*b.coefficients[i]);
        ea+=std::norm(a.coefficients[i]);eb+=std::norm(b.coefficients[i]);
    }
    s.denominator=std::sqrt(ea)*std::sqrt(eb);
    need(std::isfinite(s.denominator)&&s.denominator>1e-20,"STAGED_DEGENERATE_ENERGY");
    return s;
}
ResponseSample evaluate(const Spectrum& s,double shift){
    Z z{};
    for(std::size_t i=0;i<s.terms.size();++i)
        z+=s.terms[i]*std::exp(Z(0,tau*s.frequencies[i]*shift));
    need(std::isfinite(z.real())&&std::isfinite(z.imag()),"STAGED_NONFINITE_RESPONSE");
    ResponseSample r;r.shift_turns=wrap(shift,1);
    r.angle_deg=wrap(std::arg(z)*360/tau,360);
    r.correlation=std::min(1.0,std::abs(z)/s.denominator);
    r.residual=std::sqrt(std::max(0.0,2-2*r.correlation));
    return r;
}
std::vector<int> peaks(const std::vector<ResponseSample>& samples){
    std::vector<int> p;const int n=static_cast<int>(samples.size());
    for(int i=0;i<n;++i)
        if(samples[i].correlation>=samples[(i+n-1)%n].correlation&&
           samples[i].correlation>=samples[(i+1)%n].correlation)p.push_back(i);
    return p;
}
}
StagedResult MatchStaged(const Descriptor& a,const Descriptor& b,const StagedConfig& c){
    StagedResult out;out.executed=c;
    out.match.invariant_distance=Distance(a,b); // Validates descriptors and compatibility.
    need(c.coarse_order>=1&&c.coarse_order<=c.fine_order&&
         c.fine_order<=a.config.max_order,"INVALID_STAGED_ORDER");
    need(c.coarse_samples>=4*c.coarse_order&&c.coarse_samples<=2048&&
         c.fine_samples>=4*c.fine_order&&c.fine_samples<=2048&&
         c.fine_samples>=c.coarse_samples,"INVALID_STAGED_SAMPLES");
    need(c.refinement_iterations>=1&&c.refinement_iterations<=64,"INVALID_STAGED_REFINEMENT");
    need(c.maximum_evaluations>=1&&c.maximum_evaluations<=262144,"INVALID_STAGED_BUDGET");
    const auto coarse=spectrum(a,b,c.coarse_order),fine=spectrum(a,b,c.fine_order);
    double top=0;std::vector<int> active;
    for(std::size_t i=0;i<a.frequencies.size();++i)
        if(std::abs(a.frequencies[i])<=c.fine_order)top=std::max(top,std::abs(a.coefficients[i]));
    for(std::size_t i=0;i<a.frequencies.size();++i)
        if(std::abs(a.frequencies[i])<=c.fine_order&&
           std::abs(a.coefficients[i])>top*a.config.symmetry_relative_amplitude)
            active.push_back(a.frequencies[i]);
    if(active.size()<2){
        out.search_complete=true;out.match.status="ORIENTATION_UNOBSERVABLE";
        out.match.fallback_reason="CONTINUOUS_ROTATIONAL_SYMMETRY";return out;
    }
    for(std::size_t i=1;i<active.size();++i)
        out.match.symmetry_order=std::gcd(out.match.symmetry_order,std::abs(active[i]-active[0]));
    auto exhausted=[&](){
        out.match.succeeded=false;out.match.poses.clear();
        out.match.status="STAGED_SEARCH_BUDGET_EXHAUSTED";
        out.match.fallback_reason="NO_PARTIAL_POSE_ACCEPTANCE";return out;
    };
    std::vector<ResponseSample> cg,fg;
    for(int i=0;i<c.coarse_samples;++i){
        if(out.evaluations>=c.maximum_evaluations)return exhausted();
        cg.push_back(evaluate(coarse,double(i)/c.coarse_samples));++out.evaluations;
        if(c.capture_response)out.coarse_response.push_back(cg.back());
    }
    const auto cp=peaks(cg);out.coarse_peaks=cp.size();
    for(int i=0;i<c.fine_samples;++i){
        if(out.evaluations>=c.maximum_evaluations)return exhausted();
        fg.push_back(evaluate(fine,double(i)/c.fine_samples));++out.evaluations;
        if(c.capture_response)out.fine_response.push_back(fg.back());
    }
    auto fp=peaks(fg);out.fine_peaks=fp.size();
    // No low-order pruning: asymmetric high-frequency detail can invalidate coarse symmetry.
    auto proximity=[&](int index){
        double d=1;
        for(int k:cp)d=std::min(d,std::abs(std::remainder(
            double(index)/c.fine_samples-double(k)/c.coarse_samples,1.0)));
        return d;
    };
    std::sort(fp.begin(),fp.end(),[&](int x,int y){
        const double dx=proximity(x),dy=proximity(y);return dx!=dy?dx<dy:x<y;
    });
    const std::size_t required=fp.size()*static_cast<std::size_t>(c.refinement_iterations+3);
    if(required>c.maximum_evaluations-out.evaluations)return exhausted();
    std::vector<Pose> candidates;
    const double ratio=(std::sqrt(5.0)-1)/2;
    for(int index:fp){
        double left=double(index-1)/c.fine_samples,right=double(index+1)/c.fine_samples;
        double x=right-ratio*(right-left),y=left+ratio*(right-left);
        auto eval=[&](double shift){++out.evaluations;return evaluate(fine,shift);};
        double fx=eval(x).correlation,fy=eval(y).correlation;
        for(int iteration=0;iteration<c.refinement_iterations;++iteration){
            if(fx<fy){left=x;x=y;fx=fy;y=left+ratio*(right-left);fy=eval(y).correlation;}
            else {right=y;y=x;fy=fx;x=right-ratio*(right-left);fx=eval(x).correlation;}
        }
        const auto sample=eval((left+right)/2);
        Pose p;p.angle_deg=sample.angle_deg;p.scale=b.scale/a.scale;
        p.correlation=sample.correlation;p.residual=sample.residual;p.cyclic_shift=sample.shift_turns;
        const Z t=b.centroid-p.scale*std::polar(1.0,p.angle_deg*tau/360)*a.centroid;
        need(std::isfinite(t.real())&&std::isfinite(t.imag()),"NONFINITE_POSE_TRANSLATION");
        p.translation_x=t.real();p.translation_y=t.imag();candidates.push_back(p);
    }
    out.search_complete=true;
    std::sort(candidates.begin(),candidates.end(),[](const Pose& x,const Pose& y){
        if(x.correlation!=y.correlation)return x.correlation>y.correlation;
        if(x.angle_deg!=y.angle_deg)return x.angle_deg<y.angle_deg;
        return x.cyclic_shift<y.cyclic_shift;
    });
    if(candidates.empty()||candidates.front().residual>a.config.maximum_pose_residual){
        out.match.status="POSE_RESIDUAL_REJECTED";
        out.match.fallback_reason="SHAPE_OR_REFLECTION_MISMATCH";return out;
    }
    for(const auto& p:candidates){
        if(candidates.front().correlation-p.correlation>a.config.peak_relative_tolerance)continue;
        if(p.residual>a.config.maximum_pose_residual)continue;
        bool duplicate=false;
        for(const auto& q:out.match.poses)
            if(std::abs(std::remainder(p.angle_deg-q.angle_deg,360))<.25)duplicate=true;
        if(!duplicate)out.match.poses.push_back(p);
    }
    if(out.match.poses.size()>static_cast<std::size_t>(a.config.maximum_hypotheses)){
        out.match.poses.clear();out.match.status="HYPOTHESIS_BUDGET_EXCEEDED";
        out.match.fallback_reason="PRESERVE_SYMMETRY_BRANCHES";return out;
    }
    out.match.succeeded=true;out.match.status="AUDIT_STAGED_POSE_HYPOTHESES";return out;
}
}}
