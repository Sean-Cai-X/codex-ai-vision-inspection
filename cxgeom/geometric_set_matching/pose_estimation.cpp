#include "pose_estimation.h"
#include <cmath>
namespace cxgeom::gsm {
Point2 Transform(Point2 p,const Similarity2d& s) {
    const double a=s.angle_deg*0.01745329251994329577;
    return {s.scale*(std::cos(a)*p.x-std::sin(a)*p.y)+s.translation.x,
            s.scale*(std::sin(a)*p.x+std::cos(a)*p.y)+s.translation.y};
}
std::optional<Similarity2d> EstimateSimilarity(
    const std::vector<std::pair<Point2,Point2>>& pairs) {
    if(pairs.size()<2)return {};
    Point2 a{},b{};
    for(const auto& q:pairs) {
        if(!std::isfinite(q.first.x)||!std::isfinite(q.first.y)||
           !std::isfinite(q.second.x)||!std::isfinite(q.second.y))return {};
        a.x+=q.first.x/pairs.size();a.y+=q.first.y/pairs.size();
        b.x+=q.second.x/pairs.size();b.y+=q.second.y/pairs.size();
    }
    double dot=0,cross=0,norm=0;
    for(const auto& q:pairs) {
        const double x=q.first.x-a.x,y=q.first.y-a.y;
        const double u=q.second.x-b.x,v=q.second.y-b.y;
        dot+=x*u+y*v;cross+=x*v-y*u;norm+=x*x+y*y;
    }
    if(norm<=1e-18)return {};
    Similarity2d s;s.scale=std::hypot(dot,cross)/norm;
    s.angle_deg=std::atan2(cross,dot)/0.01745329251994329577;
    const auto t=Transform(a,s);s.translation={b.x-t.x,b.y-t.y};
    if(!std::isfinite(s.scale)||s.scale<=0||!std::isfinite(s.translation.x)||
       !std::isfinite(s.translation.y))return {};
    return s;
}
}
