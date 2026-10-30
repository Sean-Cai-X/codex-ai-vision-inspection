#pragma once
#include <opencv2/imgproc.hpp>
#include <cmath>
#include <string>
#include <vector>
#include <stdexcept>

namespace cximage::geometry {
struct ImagePyramidConfig {
 int maximum_levels=4,minimum_dimension=16;
 // 0: legacy pyrDown, 1: area, 2: linear, 3: cubic.
 int downsample_method=0;
 double scale_step=2.0,prefilter_sigma=0.0;
};
struct ImagePyramidLevel {
 int index=0;
 cv::Mat gray; // own buffer, original input is never mutated
 cv::Rect original_roi;
 double original_units_per_pixel=1;
 std::string source_revision;
 double original_units_per_pixel_y=0; // zero preserves legacy aggregate initialization
 cv::Point2d original_offset{0,0};
 double ScaleY() const {return original_units_per_pixel_y>0?original_units_per_pixel_y:original_units_per_pixel;}
 bool IsIsotropic() const {return std::abs(ScaleY()-original_units_per_pixel)<1e-10;}
 cv::Point2d ToOriginal(cv::Point2d p) const {
  return {original_roi.x+original_offset.x+p.x*original_units_per_pixel,
          original_roi.y+original_offset.y+p.y*ScaleY()};
 }
 double ToleranceFromOriginal(double pixels) const {
  if(!std::isfinite(pixels)||pixels<0)throw std::invalid_argument("INVALID_ORIGINAL_TOLERANCE");
  if(!IsIsotropic())throw std::invalid_argument("ANISOTROPIC_LEVEL_REQUIRES_DIRECTIONAL_TOLERANCE");
  return pixels/original_units_per_pixel;
 }
};
struct ImagePyramid {
 std::vector<ImagePyramidLevel> levels;
 std::string filter="opencv_pyrDown_gaussian_5x5",border="REFLECT_101";
 bool production_eligible=false;
 ImagePyramidConfig effective_config;
};
// Explicit grayscale/ROI contract: no implicit conversion, resize-to-fit or mask
// thresholding. Pixel centers map 2*x,2*y under pyrDown (not resize half-pixels).
// Cropped ROI is isolated: REFLECT_101 at ROI edges, not pixels outside the ROI.
inline ImagePyramid BuildGeometryImagePyramid(const cv::Mat& gray,cv::Rect roi,
 const std::string& source_revision,const ImagePyramidConfig& cfg={}) {
 if(gray.empty()||gray.type()!=CV_8UC1||source_revision.empty())throw std::invalid_argument("GRAY8_SOURCE_REVISION_REQUIRED");
 if(roi.width<=0||roi.height<=0||roi.x<0||roi.y<0||roi.width>gray.cols||roi.height>gray.rows||
    roi.x>gray.cols-roi.width||roi.y>gray.rows-roi.height)throw std::invalid_argument("INVALID_SOURCE_ROI");
 if(cfg.maximum_levels<1||cfg.maximum_levels>12||cfg.minimum_dimension<2)throw std::invalid_argument("INVALID_PYRAMID_CONFIG");
 if(cfg.downsample_method<0||cfg.downsample_method>3||!std::isfinite(cfg.scale_step)||cfg.scale_step<1.1||cfg.scale_step>4||
    !std::isfinite(cfg.prefilter_sigma)||cfg.prefilter_sigma<0||cfg.prefilter_sigma>4||
    (cfg.downsample_method==0&&(cfg.scale_step!=2||cfg.prefilter_sigma!=0)))
  throw std::invalid_argument("INVALID_SAMPLING_POLICY: pyrDown requires step=2 and extra sigma=0");
 ImagePyramid out;
 out.effective_config=cfg;
 const char* methods[]={"opencv_pyrDown_gaussian_5x5","resize_area","resize_linear","resize_cubic"};
 out.filter=methods[cfg.downsample_method];
 out.levels.push_back({0,gray(roi).clone(),roi,1,source_revision});
 for(int i=1;i<cfg.maximum_levels;++i){
  const auto& previous=out.levels.back();
  const cv::Size size(cfg.downsample_method==0?(previous.gray.cols+1)/2:cvRound(previous.gray.cols/cfg.scale_step),
                      cfg.downsample_method==0?(previous.gray.rows+1)/2:cvRound(previous.gray.rows/cfg.scale_step));
  if(size.width<cfg.minimum_dimension||size.height<cfg.minimum_dimension)break;
  cv::Mat next;
  double sx=2,sy=2;cv::Point2d offset=previous.original_offset;
  if(cfg.downsample_method==0)cv::pyrDown(previous.gray,next,size,cv::BORDER_REFLECT_101);
  else {
   cv::Mat filtered;
   if(cfg.prefilter_sigma>0)cv::GaussianBlur(previous.gray,filtered,{0,0},cfg.prefilter_sigma,cfg.prefilter_sigma,cv::BORDER_REFLECT_101);
   else filtered=previous.gray;
   const int interpolation[]={0,cv::INTER_AREA,cv::INTER_LINEAR,cv::INTER_CUBIC};
   cv::resize(filtered,next,size,0,0,interpolation[cfg.downsample_method]);
   sx=double(previous.gray.cols)/size.width;sy=double(previous.gray.rows)/size.height;
   offset.x+=(sx-1)*.5*previous.original_units_per_pixel;
   offset.y+=(sy-1)*.5*previous.ScaleY();
  }
  out.levels.push_back({i,next,roi,previous.original_units_per_pixel*sx,source_revision,previous.ScaleY()*sy,offset});
 }
 return out;
}
// Display only: enlargement creates no new source observations.
inline cv::Mat EnlargePyramidPreview(const ImagePyramidLevel& level,double factor,int method){
 if(level.gray.empty()||!std::isfinite(factor)||factor<1||factor>4||method<0||method>3)
  throw std::invalid_argument("INVALID_PREVIEW_POLICY");
 const int modes[]={cv::INTER_NEAREST,cv::INTER_LINEAR,cv::INTER_CUBIC,cv::INTER_LANCZOS4};
 if(level.gray.total()*factor*factor>16000000)throw std::invalid_argument("PREVIEW_PIXEL_BUDGET");
 cv::Mat result;cv::resize(level.gray,result,{},factor,factor,modes[method]);return result;
}
}
