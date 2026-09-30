#include "CxGeoSO2Harmonic.h"
#include <iomanip>
#include <iostream>
#include <stdexcept>
using namespace cxgeom::so2;
Contour readContour() {
    Contour c;size_t n=0;
    if(!(std::cin>>c.topology_verified>>c.closed>>c.complete>>c.holes>>c.components>>n) || n>4097)
        throw std::invalid_argument("INVALID_TEST_PROTOCOL");
    for(size_t i=0;i<n;++i) {
        double x=0,y=0;
        if(!(std::cin>>x>>y)) throw std::invalid_argument("INVALID_TEST_PROTOCOL");
        c.points.emplace_back(x,y);
    }
    return c;
}
void printDescriptor(const Descriptor& d) {
    std::cout<<"{\"centroid\":["<<d.centroid.real()<<","<<d.centroid.imag()<<"],\"scale\":"<<d.scale
        <<",\"perimeter\":"<<d.perimeter<<",\"coefficients\":[";
    for(size_t i=0;i<d.coefficients.size();++i) {
        if(i)std::cout<<",";
        std::cout<<"["<<d.coefficients[i].real()<<","<<d.coefficients[i].imag()<<"]";
    }
    std::cout<<"]}";
}
int main() {
    std::cout<<std::setprecision(17);
    try {
        int method=0;Config c;
        if(!(std::cin>>method>>c.sample_count>>c.max_order>>c.minimum_source_points
             >>c.minimum_perimeter>>c.normalize_scale>>c.maximum_pose_residual
             >>c.symmetry_relative_amplitude>>c.peak_relative_tolerance>>c.maximum_hypotheses))
            throw std::invalid_argument("INVALID_TEST_PROTOCOL");
        auto a=Build(readContour(),c,static_cast<Method>(method));
        auto b=Build(readContour(),c,static_cast<Method>(method));
        auto r=Match(a,b);
        std::cout<<"{\"reference\":";printDescriptor(a);
        std::cout<<",\"observation\":";printDescriptor(b);
        std::cout<<",\"status\":\""<<r.status<<"\",\"distance\":"<<r.invariant_distance
            <<",\"symmetry_order\":"<<r.symmetry_order<<",\"poses\":[";
        for(size_t i=0;i<r.poses.size();++i) {
            if(i)std::cout<<",";
            auto p=r.poses[i];
            std::cout<<"{\"angle_deg\":"<<p.angle_deg<<",\"scale\":"<<p.scale
                <<",\"correlation\":"<<p.correlation<<",\"residual\":"<<p.residual<<"}";
        }
        std::cout<<"]}\n";
    } catch(const std::exception& e) {
        std::cout<<"{\"error\":\""<<e.what()<<"\"}\n";
        return 2;
    }
}
