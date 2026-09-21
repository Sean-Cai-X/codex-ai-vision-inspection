#ifndef CXIMAGE_CXSCRIPT_GEOMETRY_FRAME_OVERLAY_H
#define CXIMAGE_CXSCRIPT_GEOMETRY_FRAME_OVERLAY_H

#include "CxScriptGeometryFrameProbe.h"
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <string>

bool SaveLineFrameProbeImages(const GaugeLineFrameProbe& probe,
                              const cv::Mat& image,
                              const std::filesystem::path& black_path,
                              const std::filesystem::path& image_path,
                              std::string& reason);

bool SaveCircleFrameProbeImages(const GaugeCircleFrameProbe& probe,
                                const cv::Mat& image,
                                const std::filesystem::path& black_path,
                                const std::filesystem::path& image_path,
                                std::string& reason);

bool SaveCircleRingFrameProbeImages(const GaugeCircleRingFrameProbe& probe,
                                    const cv::Mat& image,
                                    const std::filesystem::path& black_path,
                                    const std::filesystem::path& image_path,
                                    std::string& reason);

bool SaveCircleRingLineFormfitProbeImages(const GaugeCircleRingFrameProbe& ring_probe,
                                          const GaugeLineFrameProbe& line_probe,
                                          const cv::Mat& image,
                                          const std::filesystem::path& black_path,
                                          const std::filesystem::path& image_path,
                                          std::string& reason);
#endif
