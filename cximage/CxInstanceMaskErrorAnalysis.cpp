#include "CxInstanceMaskErrorAnalysis.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace cxvision::mask_error {
namespace {
using J=nlohmann::json;
constexpr const char* names[]={"arc","circle","ellipse","line","open_curve","polygon","closed_curve"};
void require(bool ok,const char* code){if(!ok)throw std::runtime_error(code);}
void validate(const cv::Mat& mask,cv::Size size) {
    require(mask.type()==CV_8UC1&&!mask.empty(),"MASK_TYPE_INVALID");
    require(mask.size()==size,"MASK_COORDINATE_SIZE_MISMATCH");
    require(cv::countNonZero((mask!=0)&(mask!=255))==0,"MASK_NOT_BINARY");
}
double overlap(const cv::Mat& a,const cv::Mat& b) {
    auto total=cv::countNonZero(a|b);return total?double(cv::countNonZero(a&b))/total:0;
}
cv::Point2d center(const cv::Mat& mask) {
    auto m=cv::moments(mask,true);return {m.m10/m.m00,m.m01/m.m00};
}
J distance(const cv::Mat& a,const cv::Mat& b) {
    if(!cv::countNonZero(a)||!cv::countNonZero(b))return nullptr;
    cv::Mat ea,eb,da,db;cv::erode(a,ea,cv::Mat(),cv::Point(-1,-1),1,cv::BORDER_CONSTANT,0);
    cv::erode(b,eb,cv::Mat(),cv::Point(-1,-1),1,cv::BORDER_CONSTANT,0);
    ea=a-ea;eb=b-eb;
    cv::distanceTransform(255-ea,da,cv::DIST_L2,cv::DIST_MASK_PRECISE);
    cv::distanceTransform(255-eb,db,cv::DIST_L2,cv::DIST_MASK_PRECISE);
    return (cv::mean(db,ea)[0]+cv::mean(da,eb)[0])*.5;
}
}
nlohmann::json AnalyzeSingleGroundTruth(cv::Size size,int target,const cv::Mat& truth,
    const std::vector<Prediction>& predictions,double threshold,double boundary_threshold) {
    require(size.width>0&&size.height>0&&target>=0&&target<7,"DIAGNOSTIC_INPUT_INVALID");
    require(std::isfinite(threshold)&&std::isfinite(boundary_threshold)&&threshold>0&&threshold<=boundary_threshold&&boundary_threshold<=1,"DIAGNOSTIC_THRESHOLD_INVALID");
    bool labelled=!truth.empty();int area=0;
    if(labelled){validate(truth,size);area=cv::countNonZero(truth);require(area>0,"EMPTY_GROUND_TRUTH_NOT_UNLABELLED");}
    std::set<std::string> ids;J counts=J::object(),rows=J::array(),tags=J::array();
    for(const auto* name:names)counts[name]=0;
    std::vector<const Prediction*> ordered;
    for(const auto& p:predictions){
        require(!p.id.empty()&&ids.insert(p.id).second,"PREDICTION_ID_INVALID");
        require(p.class_id>=0&&p.class_id<7&&std::isfinite(p.score)&&p.score>=0&&p.score<=1,"PREDICTION_CLASS_SCORE_INVALID");
        validate(p.mask,size);require(cv::countNonZero(p.mask)>0,"EMPTY_PREDICTION_MASK");ordered.push_back(&p);
    }
    std::sort(ordered.begin(),ordered.end(),[](const auto* a,const auto* b){return a->id<b->id;});
    double best=-1,best_target=0;const Prediction* selected=nullptr;int targets=0;
    for(const auto* p:ordered){
        counts[names[p->class_id]]=counts[names[p->class_id]].get<int>()+1;if(p->class_id==target)++targets;
        J row={{"id",p->id},{"class_name",names[p->class_id]},{"score",p->score},
            {"mask_area_pixels",cv::countNonZero(p->mask)},{"mask_iou",nullptr},{"mask_bbox_iou",nullptr},
            {"center_error_pixels",nullptr},{"center_error_image_diagonal",nullptr},{"area_ratio_to_gt",nullptr},
            {"symmetric_boundary_distance_pixels",nullptr}};
        if(labelled){
            double iou=overlap(p->mask,truth);if(iou>best){best=iou;selected=p;}if(p->class_id==target)best_target=std::max(best_target,iou);
            double delta=cv::norm(center(p->mask)-center(truth));auto a=cv::boundingRect(p->mask),b=cv::boundingRect(truth);
            int intersection=(a&b).area();row["mask_iou"]=iou;
            row["mask_bbox_iou"]=double(intersection)/(a.area()+b.area()-intersection);
            row["center_error_pixels"]=delta;row["center_error_image_diagonal"]=delta/std::hypot(size.width,size.height);
            row["area_ratio_to_gt"]=double(cv::countNonZero(p->mask))/area;
            row["symmetric_boundary_distance_pixels"]=distance(p->mask,truth);
        }
        rows.push_back(row);
    }
    int duplicate_pairs=0,same_class_pairs=0;
    for(std::size_t i=0;i<ordered.size();++i)for(std::size_t j=i+1;j<ordered.size();++j)
        if(overlap(ordered[i]->mask,ordered[j]->mask)>=threshold){++duplicate_pairs;if(ordered[i]->class_id==ordered[j]->class_id)++same_class_pairs;}
    bool matched=labelled&&selected&&best>=threshold;
    if(!labelled)tags.push_back("GeometryUncertain");
    else {
        if(!matched)tags.push_back("LocalizationMiss");
        else if(selected->class_id!=target)tags.push_back("ClassConfusion");
        if(matched&&best<boundary_threshold)tags.push_back("BoundaryMismatch");
    }
    if(duplicate_pairs)tags.push_back("DuplicateCandidates");
    if(labelled&&predictions.size()>(matched?1u:0u))tags.push_back("UnmatchedPredictions");
    return {{"schema","visionai.image_model_error.single_gt.v1"},{"metric_version","single_gt_class_agnostic_v1"},
        {"gt_present",labelled},{"matching_iou",threshold},{"boundary_iou",boundary_threshold},
        {"matching_policy","highest_iou_class_agnostic_then_stable_id"},{"target_class",names[target]},
        {"class_counts",counts},{"target_count",targets},{"other_class_count",predictions.size()-targets},
        {"best_target_iou",labelled?J(best_target):J(nullptr)},
        {"best_any_class_iou",labelled?J(std::max(0.0,best)):J(nullptr)},
        {"best_any_class",selected?J(names[selected->class_id]):J(nullptr)},
        {"matched_prediction_id",matched?J(selected->id):J(nullptr)},
        {"unmatched_gt_count",labelled?J(matched?0:1):J(nullptr)},
        {"unmatched_prediction_count",labelled?J(predictions.size()-(matched?1:0)):J(nullptr)},
        {"matched_class_correct",matched?J(selected->class_id==target):J(nullptr)},
        {"duplicate_candidate_pairs",duplicate_pairs},{"same_class_duplicate_pairs",same_class_pairs},
        {"error_tags",tags},{"predictions",rows},{"automatic_quality_approval",false}};
}
}
