#include "../../cxgeom/geometric_set_matching/types.h"
#include "../../cxgeom/geometric_set_matching/pose_estimation.h"
#include "../../libtorchsegmentation/src/utils/json.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace cxgeom::gsm;
int checks=0,runs=0;
void need(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
double angle(double a){while(a>180)a-=360;while(a< -180)a+=360;return a;}
Geometry move(const Geometry& g,const Similarity2d& s) {
    if(const auto* p=std::get_if<Point2>(&g))return Transform(*p,s);
    if(const auto* l=std::get_if<LineSegment>(&g))return LineSegment{Transform(l->end,s),Transform(l->start,s)};
    auto a=std::get<ArcSegment>(g);a.center=Transform(a.center,s);a.radius*=s.scale;
    // Reverse traversal while preserving arc support.
    a.start_deg=angle(a.start_deg+a.sweep_deg+s.angle_deg);a.sweep_deg=-a.sweep_deg;return a;
}
Request fixture(int kind,const Similarity2d& s,unsigned seed) {
    Request r;r.request_id="GSM1";
    r.reference.set_id="reference";r.target.set_id="target";
    r.reference.source_ref="synthetic:reference";r.target.source_ref="synthetic:target";
    r.reference.roi=r.target.roi={-1000,-1000,2000,2000};
    r.parameters.search_roi=r.target.roi;
    r.parameters.max_residual_px=1e-5;r.parameters.max_elapsed_ms=3000;
    for(int i=0;i<10;++i) {
        Point2 p{double(i*7-30),double((i*i*13+i*3)%71-30)};
        Geometry g=p;int type=kind==3?i%3:kind;
        if(type==1)g=LineSegment{p,{p.x+2+i*.3,p.y+1+i*.7}};
        if(type==2)g=ArcSegment{p,2+i*.4,-100.+i*11,40.+i*3};
        r.reference.elements.push_back({"r"+std::to_string(i),"synthetic:r"+std::to_string(i),1,g});
        r.target.elements.push_back({"t"+std::to_string(i),"synthetic:t"+std::to_string(i),1,move(g,s)});
    }
    std::mt19937 gen(seed);
    std::shuffle(r.reference.elements.begin(),r.reference.elements.end(),gen);
    std::shuffle(r.target.elements.begin(),r.target.elements.end(),gen);
    return r;
}
int main() try {
    nlohmann::json receipt;receipt["schema"]="cxvision.gsm1.test_receipt.v1";
    receipt["cases"]=nlohmann::json::array();
    double maxResidual=0,maxTranslation=0,maxAngle=0,maxScale=0;
    for(int kind=0;kind<4;++kind)for(double a:{-180.,-137.,-45.,0.,31.,90.,179.,180.})
    for(double scale:{.8,1.,1.2})for(unsigned seed:{7u,41u}) {
        Similarity2d truth{{120,-73},a,scale};auto r=fixture(kind,truth,seed);
        auto out=Match(r);++runs;
        need(out.execution_status==ExecutionStatus::Completed&&out.search_complete,"matrix search complete");
        need(out.candidates.size()==1,"asymmetric fixture unique candidate");
        need(!out.production_eligible&&!out.solvability,"no P3/production claim");
        const auto& c=out.candidates.front();const auto& p=*c.pose;
        const double dt=std::hypot(p.translation.x-120,p.translation.y+73);
        const double da=std::abs(angle(p.angle_deg-a)),ds=std::abs(p.scale-scale);
        need(dt<1e-8&&da<1e-8&&ds<1e-10&&c.residual_px<1e-8,"pose precision");
        need(c.correspondences.size()==10&&c.weighted_coverage==1,"full coverage");
        for(const auto& pair:c.correspondences)need(pair.reference_id.substr(1)==pair.target_id.substr(1),"correspondence identity");
        maxResidual=std::max(maxResidual,c.residual_px);maxTranslation=std::max(maxTranslation,dt);
        maxAngle=std::max(maxAngle,da);maxScale=std::max(maxScale,ds);
        receipt["cases"].push_back({{"kind",kind},{"angle",a},{"scale",scale},{"shuffle_seed",seed},
            {"elapsed_ms",out.elapsed_ms},{"pair_checks",out.pair_checks},{"residual_px",c.residual_px},
            {"pose",{{"tx",p.translation.x},{"ty",p.translation.y},{"angle_deg",p.angle_deg},{"scale",p.scale}}},
            {"evidence_refs",out.evidence_refs},{"correspondences",nlohmann::json::array()}});
        for(const auto& pair:c.correspondences)
            receipt["cases"].back()["correspondences"].push_back({
                {"reference_id",pair.reference_id},{"target_id",pair.target_id},
                {"score",pair.score},{"residual_px",pair.residual_px}});

    }
    auto r=fixture(0,{{0,0},0,1},1);
    r.parameters.max_hypotheses=1;auto out=Match(r);
    need(out.execution_status==ExecutionStatus::BudgetExhausted&&out.candidates.empty()&&!out.search_complete,"hypothesis cap");
    r=fixture(0,{{0,0},0,1},1);r.parameters.max_pair_checks=5;out=Match(r);
    need(out.execution_status==ExecutionStatus::BudgetExhausted&&out.pair_checks==5,"work cap");
    r=fixture(0,{{0,0},90,1},1);r.parameters.angle_min_deg=-5;r.parameters.angle_max_deg=5;
    need(Match(r).candidates.empty(),"angle constraint");
    r=fixture(0,{{0,0},0,1.2},1);r.parameters.scale_max=1.;
    need(Match(r).candidates.empty(),"scale constraint");
    r=fixture(0,{{0,0},0,1},1);r.target.elements.pop_back();out=Match(r);
    need(out.execution_status==ExecutionStatus::NotImplemented&&!out.solvability,"partial deferred");
    r=fixture(0,{{0,0},0,1},1);r.parameters.search_roi={-1,-1,2,2};
    need(Match(r).execution_status==ExecutionStatus::NotImplemented,"search ROI enforced");
    r=fixture(0,{{0,0},0,1},1);r.target.elements[0].quality=0;
    need(Match(r).execution_status==ExecutionStatus::NotImplemented,"zero quality not proof");
    r=fixture(0,{{0,0},0,1},1);
    for(auto& e:r.reference.elements)e.geometry=Point2{0,0};
    need(Match(r).execution_status==ExecutionStatus::NotImplemented,"coincident representatives deferred");
    r=fixture(0,{{0,0},0,1},1);r.reference.elements.resize(4);r.target.elements.resize(4);
    int k=0;for(Point2 p: {Point2{-10,-10},Point2{10,-10},Point2{10,10},Point2{-10,10}}) {
        r.reference.elements[k].geometry=p;r.target.elements[k].geometry=p;++k;
    }
    out=Match(r);need(out.execution_status==ExecutionStatus::Completed&&out.candidates.size()==4,"square retains four poses");
    need(!out.solvability&&!out.production_eligible,"symmetry no unique claim");
    r=fixture(0,{{0,0},0,1},1);
    for(auto& e:r.target.elements){auto& p=std::get<Point2>(e.geometry);p.x=-p.x;}
    need(Match(r).candidates.empty(),"reflection rejected");
    r=fixture(0,{{0,0},0,1},1);
    for(auto& e:r.target.elements){auto& p=std::get<Point2>(e.geometry);p.x+=.2*p.y;}
    need(Match(r).candidates.empty(),"shear rejected");
    r=fixture(0,{{0,0},0,1},1);
    r.reference.elements[0].geometry=r.reference.elements[1].geometry;
    r.target.elements[0].geometry=r.target.elements[1].geometry;
    need(Match(r).candidates.empty(),"duplicate ambiguity never tie-broken into success");
    r=fixture(1,{{0,0},0,1},1);
    for(auto& e:r.target.elements) {
        auto& l=std::get<LineSegment>(e.geometry);
        const Point2 c{(l.start.x+l.end.x)/2,(l.start.y+l.end.y)/2};
        const Point2 v{l.end.x-c.x,l.end.y-c.y};
        l.start={c.x+v.y,c.y-v.x};l.end={c.x-v.y,c.y+v.x};
    }
    need(Match(r).candidates.empty(),"line orientation checked beyond midpoint");
    r=fixture(2,{{0,0},0,1},1);
    for(auto& e:r.target.elements)std::get<ArcSegment>(e.geometry).sweep_deg*=2;
    need(Match(r).candidates.empty(),"arc extent checked beyond center");
    r=fixture(0,{{0,0},0,1},1);
    for(auto& e:r.target.elements){auto& p=std::get<Point2>(e.geometry);p.x*=1.1;}
    need(Match(r).candidates.empty(),"anisotropic scaling rejected");
    r=fixture(0,{{0,0},0,1},1);
    for(auto& e:r.reference.elements) {
        const double i=std::stod(e.stable_id.substr(1));
        e.geometry=Point2{i*i+2*i,0};
    }
    for(auto& e:r.target.elements) {
        const double i=std::stod(e.stable_id.substr(1));
        e.geometry=Point2{i*i+2*i,0};
    }
    out=Match(r);need(out.candidates.size()==1,"asymmetric collinear points can constrain similarity");
    r=fixture(2,{{120,-73},31,1.2},1);
    for(auto& e:r.reference.elements)std::get<ArcSegment>(e.geometry).sweep_deg=360;
    for(auto& e:r.target.elements)std::get<ArcSegment>(e.geometry).sweep_deg=-360;
    need(Match(r).candidates.size()==1,"full circles ignore start angle");
    r=fixture(3,{{120,-73},31,1.2},1);
    const auto firstReference=r.reference.elements.front().stable_id;
    const auto firstTarget=r.target.elements.front().stable_id;
    out=Match(r);
    need(r.reference.elements.front().stable_id==firstReference&&
         r.target.elements.front().stable_id==firstTarget,"matching does not reorder caller elements");
    for(auto& e:r.target.elements)e.stable_id="different-id-"+e.stable_id;
    out=Match(r);need(out.candidates.size()==1,"IDs retained but not used as match hints");
    need(!EstimateSimilarity({{{0,0},{0,0}},{{0,0},{2,2}}}),"degenerate least squares");
    receipt["status"]="PASS";receipt["checks"]=checks;receipt["matrix_runs"]=runs;
    receipt["max_residual_px"]=maxResidual;receipt["max_translation_error_px"]=maxTranslation;
    receipt["max_angle_error_deg"]=maxAngle;receipt["max_scale_error"]=maxScale;
    receipt["production_eligible"]=false;
    std::cout<<receipt.dump(2)<<"\n";return 0;
} catch(const std::exception& e){std::cerr<<"GSM1_FAIL "<<e.what()<<" after "<<checks<<" checks\n";return 1;}
