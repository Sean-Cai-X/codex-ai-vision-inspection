#pragma once

#include "GridPatternClassNet.h"

#include <string>
#include <vector>

class ICxShapeSink;

class GridPatternClassTool
{
public:
    void setrect(int x, int y, int width, int height);
    void setnormalized(int width, int height);
    void setgrid(int rows, int cols);
    void setlevels(int levels);
    void setorientationbins(int bins);
    void setforegroundthreshold(int threshold);
    void setforegrounddark(int enabled);
    void setequalizecontrast(int enabled);
    void setactiveforegroundpercent(int percent);
    void setactiveedgepercent(int percent);
    void setmaxoverlays(int max_overlays);
    void setfusionmode(int fusion_mode);

    void analyze(void* image);

    int getstatuscode();
    int getactivecellcount();
    int getdescriptordim();
    int getlevelcount();
    int getoverlaycount();
    int getoverlaytruncated();
    double getelapsedms();
    const char* getsummary();

    void PublishDisplayShapes(ICxShapeSink& sink, const std::string& owner_ref) const;

private:
    void RebuildHierarchy();

    cxcore::GridPatternConfig config_;
    cxcore::GridFeatureMap feature_map_;
    cxcore::GridPatternHierarchy hierarchy_;
    int roi_x_ = 0;
    int roi_y_ = 0;
    int roi_width_ = 0;
    int roi_height_ = 0;
    int requested_levels_ = 3;
    int max_overlays_ = 96;
    int fusion_mode_ = 2;
    int status_code_ = 0;
    int overlay_count_ = 0;
    bool overlay_truncated_ = false;
    double elapsed_ms_ = 0.0;
    std::string summary_ = "not_analyzed";
};
