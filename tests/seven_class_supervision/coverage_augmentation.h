#pragma once
// R&D-only paired image/polygon augmentation. No production defaults changed.
#include "synthetic_materializer.h"
#include <opencv2/imgproc.hpp>
#include <array>
namespace coverage_augmentation {
using namespace supervision_fixture;
struct Plan {int width=64,height=64;double scale=1,angle=0,dx=0,dy=0;int photo=0;};
inline Plan plan(int exposure) {
    check(exposure>=0&&exposure<80,"COVERAGE_EXPOSURE_INVALID");
    Plan p;int cycle=exposure/10;double sign=cycle%2?1:-1;
    switch(exposure%10){
        case 1:p.dx=sign*7;p.dy=-sign*6;break;
        case 2:p.scale=cycle%2?.7:.55;break;
        case 3:p.angle=sign*(cycle%2?60:30);break;
        case 4:p.width=128;p.height=80;break;
        case 5:p.width=80;p.height=128;break;
        case 6:p.photo=1;break;
        case 7:p.photo=2;break;
        case 8:p.photo=3;break;
        case 9:p.scale=.7;p.angle=sign*30;p.dx=sign*6;p.photo=cycle%2?1:3;break;
    }return p;
}
inline J describe(const Plan& p){return {{"width",p.width},{"height",p.height},{"scale",p.scale},
    {"angle",p.angle},{"dx",p.dx},{"dy",p.dy},{"photo",p.photo}};}
inline J policy(){return {{"version","paired_coverage_v1"},{"exposures",80},{"source_order","exposure modulo 3"},
    {"geometry","isotropic_affine_no_crop"},{"interpolation","image_linear; supervision_from_transformed_polygon"},
    {"photo_modes",{"identity","255-minus-image","gain=.20 offset=100","background_5px_checker_35_170"}},
    {"seed",20261012},{"no_class_relabel",true},{"preserve_source_split",true}};}
struct Applied {cv::Mat image,foreground;std::vector<std::vector<cv::Point2f>> polygons;
    std::vector<std::array<float,4>> boxes;J record;};
inline Applied apply(const cv::Mat& image,const std::vector<std::vector<cv::Point2f>>& polygons,
    const Plan& p){
    check(!image.empty()&&image.type()==CV_8UC1&&!polygons.empty(),"COVERAGE_INPUT_INVALID");
    auto a=cv::getRotationMatrix2D({image.cols*.5f,image.rows*.5f},p.angle,p.scale);
    a.at<double>(0,2)+=p.width*.5-image.cols*.5+p.dx;
    a.at<double>(1,2)+=p.height*.5-image.rows*.5+p.dy;
    Applied out;cv::warpAffine(image,out.image,a,{p.width,p.height},cv::INTER_LINEAR,cv::BORDER_CONSTANT,23);
    out.foreground=cv::Mat::zeros(p.height,p.width,CV_8UC1);
    for(const auto& polygon:polygons){
        std::vector<cv::Point2f> transformed;std::vector<cv::Point> pixels;
        std::array<float,4> box={1,1,0,0};
        for(const auto& point:polygon){
            double x=point.x*image.cols,y=point.y*image.rows;
            double u=a.at<double>(0,0)*x+a.at<double>(0,1)*y+a.at<double>(0,2);
            double v=a.at<double>(1,0)*x+a.at<double>(1,1)*y+a.at<double>(1,2);
            check(std::isfinite(u)&&std::isfinite(v)&&u>=0&&v>=0&&u<p.width&&v<p.height,"COVERAGE_CROP_FORBIDDEN");
            float nx=float(u/p.width),ny=float(v/p.height);transformed.emplace_back(nx,ny);
            pixels.emplace_back(cvRound(u),cvRound(v));box[0]=std::min(box[0],nx);box[1]=std::min(box[1],ny);
            box[2]=std::max(box[2],nx);box[3]=std::max(box[3],ny);
        }
        check(box[2]>box[0]&&box[3]>box[1],"COVERAGE_DEGENERATE_POLYGON");
        cv::fillPoly(out.foreground,std::vector<std::vector<cv::Point>>{pixels},255);
        out.polygons.push_back(std::move(transformed));out.boxes.push_back(box);
    }
    if(p.photo==1)out.image=255-out.image;
    else if(p.photo==2)out.image.convertTo(out.image,CV_8UC1,.2,100);
    else if(p.photo==3)for(int y=0;y<p.height;++y)for(int x=0;x<p.width;++x)
        if(!out.foreground.at<unsigned char>(y,x))out.image.at<unsigned char>(y,x)=((x/5+y/5)%2)?170:35;
    out.record=describe(p);out.record["boxes_norm"]=out.boxes;out.record["polygons_norm"]=J::array();
    for(const auto& polygon:out.polygons){J points=J::array();for(const auto& point:polygon)points.push_back({point.x,point.y});out.record["polygons_norm"].push_back(points);}
    return out;
}
inline void self_test(){
    cv::Mat source(64,64,CV_8UC1,cv::Scalar(20));source(cv::Rect(20,20,24,24)).setTo(210);
    std::vector<std::vector<cv::Point2f>> polygon={{{20.f/64,20.f/64},{43.f/64,20.f/64},{43.f/64,43.f/64},{20.f/64,43.f/64}}};
    const auto before=source.clone();
    for(int i=0;i<80;++i){auto p=plan(i), q=plan(i);auto a=apply(source,polygon,p),b=apply(source,polygon,q);
        check(a.record==b.record&&cv::countNonZero(a.image!=b.image)==0,"COVERAGE_NONDETERMINISTIC");
        auto box=cv::boundingRect(a.foreground);auto geom=a.boxes[0];
        check(std::abs(box.x-geom[0]*p.width)<=1&&std::abs(box.y-geom[1]*p.height)<=1&&
            std::abs(box.x+box.width-1-geom[2]*p.width)<=1&&std::abs(box.y+box.height-1-geom[3]*p.height)<=1,"COVERAGE_BOX_MASK_MISALIGNMENT");
        if(i%10==0)check(cv::countNonZero(a.image!=source)==0,"COVERAGE_IDENTITY_CHANGED");
    }
    check(cv::countNonZero(source!=before)==0,"COVERAGE_SOURCE_MUTATED");
    Plan bad;bad.dx=100;bool rejected=false;try{apply(source,polygon,bad);}catch(const std::runtime_error&){rejected=true;}
    check(rejected,"COVERAGE_SILENT_CLIP");
}
}
