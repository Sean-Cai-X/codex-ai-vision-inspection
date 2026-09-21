#pragma once

#include "RegionPatternNet.h"

#include <string>

class ICxShapeSink;

class RegionPatternTool
{
public:
    void setrect(int x, int y, int width, int height);
    void setnormalized(int width, int height);
    void setpooling(int rows, int cols);
    void setbinary(int enabled);
    void setthreshold(int threshold);
    void setforegrounddark(int enabled);
    void setmaxoverlays(int max_overlays);

    void analyze(void* image);

    int getstatuscode();
    int getdescriptordim();
    int getforegroundpermille();
    int getmeanpermille();
    int getstdpermille();
    int getpoolingrows();
    int getpoolingcols();
    int getoverlaycount();
    int getoverlaytruncated();
    double getelapsedms();
    const char* getsummary();

    void PublishDisplayShapes(ICxShapeSink& sink, const std::string& owner_ref) const;

private:
    cxcore::RegionPatternConfig config_;
    cxcore::RegionPatternDescriptor descriptor_;
    int roi_x_ = 0;
    int roi_y_ = 0;
    int roi_width_ = 0;
    int roi_height_ = 0;
    int max_overlays_ = 64;
    int status_code_ = 0;
    int mean_permille_ = 0;
    int std_permille_ = 0;
    int overlay_count_ = 0;
    bool overlay_truncated_ = false;
    double elapsed_ms_ = 0.0;
    std::string summary_ = "not_analyzed";
};
