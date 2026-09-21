

#include "muParser.h"

#include <exception>
#include <iostream>
#include <string>

namespace
{
struct CvPointHost
{
  double x = 0.0;
  double y = 0.0;

  void set(double next_x, double next_y) { x = next_x; y = next_y; }
  double getx() { return x; }
  double gety() { return y; }
};

struct CvRectHost
{
  double x = 0.0;
  double y = 0.0;
  double width = 0.0;
  double height = 0.0;

  void set(double next_x, double next_y, double next_w, double next_h)
  {
    x = next_x;
    y = next_y;
    width = next_w;
    height = next_h;
  }
  double area() { return width * height; }
  double getx() { return x; }
  double gety() { return y; }
  double getw() { return width; }
  double geth() { return height; }
};

struct CvScalarHost
{
  double c0 = 0.0;
  double c1 = 0.0;
  double c2 = 0.0;
  double c3 = 0.0;

  void set(double v0, double v1, double v2, double v3)
  {
    c0 = v0;
    c1 = v1;
    c2 = v2;
    c3 = v3;
  }
  double totalsum() { return c0 + c1 + c2 + c3; }
};

struct CvMatHost
{
  double rows = 0.0;
  double cols = 0.0;
  double type = 0.0;

  void setshape(double next_rows, double next_cols, double next_type)
  {
    rows = next_rows;
    cols = next_cols;
    type = next_type;
  }
  double rowcount() { return rows; }
  double colcount() { return cols; }
  double isvalid() { return (rows > 0.0 && cols > 0.0) ? 1.0 : 0.0; }
};

struct RoiHost
{
  double x = 0.0;
  double y = 0.0;
  double width = 0.0;
  double height = 0.0;
  std::string name;

  void setrect(double next_x, double next_y, double next_w, double next_h)
  {
    x = next_x;
    y = next_y;
    width = next_w;
    height = next_h;
  }
  void setname(const char *next_name) { name = next_name ? next_name : ""; }
  double area() { return width * height; }
  double getx() { return x; }
  double gety() { return y; }
  double getw() { return width; }
  double geth() { return height; }
  double namelen() { return static_cast<double>(name.size()); }
};

struct ImageHost
{
  double width = 0.0;
  double height = 0.0;
  double image_type = 0.0;
  double roi_x = 0.0;
  double roi_y = 0.0;
  double roi_w = 0.0;
  double roi_h = 0.0;

  void setshape(double next_w, double next_h, double next_type)
  {
    width = next_w;
    height = next_h;
    image_type = next_type;
  }
  void setroi(double next_x, double next_y, double next_w, double next_h)
  {
    roi_x = next_x;
    roi_y = next_y;
    roi_w = next_w;
    roi_h = next_h;
  }
  double getw() { return width; }
  double geth() { return height; }
  double gettype() { return image_type; }
  double roix() { return roi_x; }
  double roiy() { return roi_y; }
  double roiw() { return roi_w; }
  double roih() { return roi_h; }
  double roiarea() { return roi_w * roi_h; }
};

struct OutputRectHost
{
  double x = 0.0;
  double y = 0.0;
  double width = 0.0;
  double height = 0.0;

  void set(double next_x, double next_y, double next_w, double next_h)
  {
    x = next_x;
    y = next_y;
    width = next_w;
    height = next_h;
  }
  double area() { return width * height; }
  double centerx() { return x + width / 2.0; }
  double centery() { return y + height / 2.0; }
  double getw() { return width; }
  double geth() { return height; }
};

struct ImageAnalysisOutputHost
{
  double width = 0.0;
  double height = 0.0;
  double image_type = 0.0;
  double component_count = 0.0;

  void setsize(double next_w, double next_h, double next_type)
  {
    width = next_w;
    height = next_h;
    image_type = next_type;
  }
  void setcomponentcount(double next_count) { component_count = next_count; }
  double getw() { return width; }
  double geth() { return height; }
  double gettype() { return image_type; }
  double pixelcount() { return width * height; }
  double components() { return component_count; }
};

struct LineMeasurementOutputHost
{
  double horizontal_count = 0.0;
  double vertical_count = 0.0;
  double measure_w = 0.0;
  double measure_h = 0.0;

  void setsamples(double h_count, double v_count)
  {
    horizontal_count = h_count;
    vertical_count = v_count;
  }
  void setbounds(double next_w, double next_h)
  {
    measure_w = next_w;
    measure_h = next_h;
  }
  double horizontalsamples() { return horizontal_count; }
  double verticalsamples() { return vertical_count; }
  double totalsamples() { return horizontal_count + vertical_count; }
  double boundarea() { return measure_w * measure_h; }
};

struct CircleMeasurementOutputHost
{
  double radius = 0.0;
  double average_distance = 0.0;
  double fit_valid = 0.0;

  void setfit(double next_radius, double next_avg_dist, double next_fit_valid)
  {
    radius = next_radius;
    average_distance = next_avg_dist;
    fit_valid = next_fit_valid;
  }
  double getradius() { return radius; }
  double getavgdist() { return average_distance; }
  double isvalid() { return fit_valid; }
};

struct MatchOutputHost
{
  double max_score = 0.0;
  double image_model_score = 0.0;
  double candidate_count = 0.0;

  void setsummary(double next_max_score, double next_model_score, double next_count)
  {
    max_score = next_max_score;
    image_model_score = next_model_score;
    candidate_count = next_count;
  }
  double maxscore() { return max_score; }
  double modelscore() { return image_model_score; }
  double candidates() { return candidate_count; }
};

struct CaseSpecHost
{
  std::string name;
  std::string module;
  std::string layer;

  void setname(const char *next_name) { name = next_name ? next_name : ""; }
  void setmodule(const char *next_module) { module = next_module ? next_module : ""; }
  void setlayer(const char *next_layer) { layer = next_layer ? next_layer : ""; }
  double namelen() { return static_cast<double>(name.size()); }
};

struct MetricsHost
{
  double primary = 0.0;
  double stability = 0.0;

  void set(double next_primary, double next_stability)
  {
    primary = next_primary;
    stability = next_stability;
  }
  double primarymetric() { return primary; }
  double stabilitymetric() { return stability; }
};

struct ToleranceHost
{
  double max_error = 0.0;
  double min_score = 0.0;

  void set(double next_max_error, double next_min_score)
  {
    max_error = next_max_error;
    min_score = next_min_score;
  }
  double maxerror() { return max_error; }
  double minscore() { return min_score; }
};

struct FailureContextHost
{
  std::string message;
  double code = 0.0;

  void setmessage(const char *next_message) { message = next_message ? next_message : ""; }
  void setcode(double next_code) { code = next_code; }
  double messagelen() { return static_cast<double>(message.size()); }
  double getcode() { return code; }
};

struct BridgeResultHost
{
  double primary_metric = 0.0;
  double tolerance = 0.0;
  double failure_code = 0.0;

  void setsummary(double next_primary_metric, double next_tolerance, double next_failure_code)
  {
    primary_metric = next_primary_metric;
    tolerance = next_tolerance;
    failure_code = next_failure_code;
  }
  double passflag() { return (primary_metric <= tolerance && failure_code == 0.0) ? 1.0 : 0.0; }
  double metric() { return primary_metric; }
};

bool Check(bool condition, const char *message)
{
  if (!condition)
  {
    std::cerr << "[FAIL] " << message << std::endl;
    return false;
  }
  return true;
}

void RegisterCxcoreTypeSubset(mu::Parser &parser)
{
  double *org_double = 0;
  parser.DefineOrgClass("double", org_double);
  parser.UsingClass(true);

  CvPointHost *cv_point = 0;
  parser.DefineClass("CvPoint", cv_point);
  parser.DefineClassFun("CvPoint", cv_point, "set", &CvPointHost::set);
  parser.DefineClassFun("CvPoint", cv_point, "getx", &CvPointHost::getx);
  parser.DefineClassFun("CvPoint", cv_point, "gety", &CvPointHost::gety);

  CvRectHost *cv_rect = 0;
  parser.DefineClass("CvRect", cv_rect);
  parser.DefineClassFun("CvRect", cv_rect, "set", &CvRectHost::set);
  parser.DefineClassFun("CvRect", cv_rect, "area", &CvRectHost::area);
  parser.DefineClassFun("CvRect", cv_rect, "getx", &CvRectHost::getx);
  parser.DefineClassFun("CvRect", cv_rect, "gety", &CvRectHost::gety);
  parser.DefineClassFun("CvRect", cv_rect, "getw", &CvRectHost::getw);
  parser.DefineClassFun("CvRect", cv_rect, "geth", &CvRectHost::geth);

  CvScalarHost *cv_scalar = 0;
  parser.DefineClass("CvScalar", cv_scalar);
  parser.DefineClassFun("CvScalar", cv_scalar, "set", &CvScalarHost::set);
  parser.DefineClassFun("CvScalar", cv_scalar, "totalsum", &CvScalarHost::totalsum);

  CvMatHost *cv_mat = 0;
  parser.DefineClass("CvMat", cv_mat);
  parser.DefineClassFun("CvMat", cv_mat, "setshape", &CvMatHost::setshape);
  parser.DefineClassFun("CvMat", cv_mat, "rowcount", &CvMatHost::rowcount);
  parser.DefineClassFun("CvMat", cv_mat, "colcount", &CvMatHost::colcount);
  parser.DefineClassFun("CvMat", cv_mat, "isvalid", &CvMatHost::isvalid);

  RoiHost *roi = 0;
  parser.DefineClass("Roi", roi);
  parser.DefineClassFun("Roi", roi, "setrect", &RoiHost::setrect);
  parser.DefineClassFun("Roi", roi, "setname", &RoiHost::setname);
  parser.DefineClassFun("Roi", roi, "area", &RoiHost::area);
  parser.DefineClassFun("Roi", roi, "getx", &RoiHost::getx);
  parser.DefineClassFun("Roi", roi, "gety", &RoiHost::gety);
  parser.DefineClassFun("Roi", roi, "getw", &RoiHost::getw);
  parser.DefineClassFun("Roi", roi, "geth", &RoiHost::geth);
  parser.DefineClassFun("Roi", roi, "namelen", &RoiHost::namelen);

  ImageHost *image = 0;
  parser.DefineClass("Image", image);
  parser.DefineClassFun("Image", image, "setshape", &ImageHost::setshape);
  parser.DefineClassFun("Image", image, "setroi", &ImageHost::setroi);
  parser.DefineClassFun("Image", image, "getw", &ImageHost::getw);
  parser.DefineClassFun("Image", image, "geth", &ImageHost::geth);
  parser.DefineClassFun("Image", image, "gettype", &ImageHost::gettype);
  parser.DefineClassFun("Image", image, "roix", &ImageHost::roix);
  parser.DefineClassFun("Image", image, "roiy", &ImageHost::roiy);
  parser.DefineClassFun("Image", image, "roiw", &ImageHost::roiw);
  parser.DefineClassFun("Image", image, "roih", &ImageHost::roih);
  parser.DefineClassFun("Image", image, "roiarea", &ImageHost::roiarea);

  OutputRectHost *output_rect = 0;
  parser.DefineClass("OutputRect", output_rect);
  parser.DefineClassFun("OutputRect", output_rect, "set", &OutputRectHost::set);
  parser.DefineClassFun("OutputRect", output_rect, "area", &OutputRectHost::area);
  parser.DefineClassFun("OutputRect", output_rect, "centerx", &OutputRectHost::centerx);
  parser.DefineClassFun("OutputRect", output_rect, "centery", &OutputRectHost::centery);
  parser.DefineClassFun("OutputRect", output_rect, "getw", &OutputRectHost::getw);
  parser.DefineClassFun("OutputRect", output_rect, "geth", &OutputRectHost::geth);

  ImageAnalysisOutputHost *analysis = 0;
  parser.DefineClass("ImageAnalysisOutput", analysis);
  parser.DefineClassFun("ImageAnalysisOutput", analysis, "setsize", &ImageAnalysisOutputHost::setsize);
  parser.DefineClassFun("ImageAnalysisOutput", analysis, "setcomponentcount", &ImageAnalysisOutputHost::setcomponentcount);
  parser.DefineClassFun("ImageAnalysisOutput", analysis, "getw", &ImageAnalysisOutputHost::getw);
  parser.DefineClassFun("ImageAnalysisOutput", analysis, "geth", &ImageAnalysisOutputHost::geth);
  parser.DefineClassFun("ImageAnalysisOutput", analysis, "gettype", &ImageAnalysisOutputHost::gettype);
  parser.DefineClassFun("ImageAnalysisOutput", analysis, "pixelcount", &ImageAnalysisOutputHost::pixelcount);
  parser.DefineClassFun("ImageAnalysisOutput", analysis, "components", &ImageAnalysisOutputHost::components);

  LineMeasurementOutputHost *line = 0;
  parser.DefineClass("LineMeasurementOutput", line);
  parser.DefineClassFun("LineMeasurementOutput", line, "setsamples", &LineMeasurementOutputHost::setsamples);
  parser.DefineClassFun("LineMeasurementOutput", line, "setbounds", &LineMeasurementOutputHost::setbounds);
  parser.DefineClassFun("LineMeasurementOutput", line, "horizontalsamples", &LineMeasurementOutputHost::horizontalsamples);
  parser.DefineClassFun("LineMeasurementOutput", line, "verticalsamples", &LineMeasurementOutputHost::verticalsamples);
  parser.DefineClassFun("LineMeasurementOutput", line, "totalsamples", &LineMeasurementOutputHost::totalsamples);
  parser.DefineClassFun("LineMeasurementOutput", line, "boundarea", &LineMeasurementOutputHost::boundarea);

  CircleMeasurementOutputHost *circle = 0;
  parser.DefineClass("CircleMeasurementOutput", circle);
  parser.DefineClassFun("CircleMeasurementOutput", circle, "setfit", &CircleMeasurementOutputHost::setfit);
  parser.DefineClassFun("CircleMeasurementOutput", circle, "getradius", &CircleMeasurementOutputHost::getradius);
  parser.DefineClassFun("CircleMeasurementOutput", circle, "getavgdist", &CircleMeasurementOutputHost::getavgdist);
  parser.DefineClassFun("CircleMeasurementOutput", circle, "isvalid", &CircleMeasurementOutputHost::isvalid);

  MatchOutputHost *match = 0;
  parser.DefineClass("MatchOutput", match);
  parser.DefineClassFun("MatchOutput", match, "setsummary", &MatchOutputHost::setsummary);
  parser.DefineClassFun("MatchOutput", match, "maxscore", &MatchOutputHost::maxscore);
  parser.DefineClassFun("MatchOutput", match, "modelscore", &MatchOutputHost::modelscore);
  parser.DefineClassFun("MatchOutput", match, "candidates", &MatchOutputHost::candidates);

  CaseSpecHost *case_spec = 0;
  parser.DefineClass("CaseSpec", case_spec);
  parser.DefineClassFun("CaseSpec", case_spec, "setname", &CaseSpecHost::setname);
  parser.DefineClassFun("CaseSpec", case_spec, "setmodule", &CaseSpecHost::setmodule);
  parser.DefineClassFun("CaseSpec", case_spec, "setlayer", &CaseSpecHost::setlayer);
  parser.DefineClassFun("CaseSpec", case_spec, "namelen", &CaseSpecHost::namelen);

  MetricsHost *metrics = 0;
  parser.DefineClass("Metrics", metrics);
  parser.DefineClassFun("Metrics", metrics, "set", &MetricsHost::set);
  parser.DefineClassFun("Metrics", metrics, "primarymetric", &MetricsHost::primarymetric);
  parser.DefineClassFun("Metrics", metrics, "stabilitymetric", &MetricsHost::stabilitymetric);

  ToleranceHost *tolerance = 0;
  parser.DefineClass("Tolerance", tolerance);
  parser.DefineClassFun("Tolerance", tolerance, "set", &ToleranceHost::set);
  parser.DefineClassFun("Tolerance", tolerance, "maxerror", &ToleranceHost::maxerror);
  parser.DefineClassFun("Tolerance", tolerance, "minscore", &ToleranceHost::minscore);

  FailureContextHost *failure = 0;
  parser.DefineClass("FailureContext", failure);
  parser.DefineClassFun("FailureContext", failure, "setmessage", &FailureContextHost::setmessage);
  parser.DefineClassFun("FailureContext", failure, "setcode", &FailureContextHost::setcode);
  parser.DefineClassFun("FailureContext", failure, "messagelen", &FailureContextHost::messagelen);
  parser.DefineClassFun("FailureContext", failure, "getcode", &FailureContextHost::getcode);

  BridgeResultHost *bridge = 0;
  parser.DefineClass("BridgeResult", bridge);
  parser.DefineClassFun("BridgeResult", bridge, "setsummary", &BridgeResultHost::setsummary);
  parser.DefineClassFun("BridgeResult", bridge, "passflag", &BridgeResultHost::passflag);
  parser.DefineClassFun("BridgeResult", bridge, "metric", &BridgeResultHost::metric);
}

int RunTypeRegistrationSmoke()
{
  mu::Parser parser;
  RegisterCxcoreTypeSubset(parser);

  double total = 0.0;
  parser.DefineVar("total", &total);
  parser.SetExpr(
    "CvRect cvroi;"
    "cvroi.set(10,20,30,40);"
    "CvPoint center;"
    "center.set(25,40);"
    "CvScalar color;"
    "color.set(10,20,30,40);"
    "CvMat mat;"
    "mat.setshape(480,640,16);"
    "Image image;"
    "image.setshape(640,480,1);"
    "image.setroi(10,20,30,40);"
    "Roi roi;"
    "roi.setrect(10,20,30,40);"
    "roi.setname(\"main_roi\");"
    "OutputRect bounds;"
    "bounds.set(10,20,30,40);"
    "ImageAnalysisOutput analysis;"
    "analysis.setsize(640,480,1);"
    "analysis.setcomponentcount(2);"
    "LineMeasurementOutput line;"
    "line.setsamples(6,8);"
    "line.setbounds(30,40);"
    "CircleMeasurementOutput circle;"
    "circle.setfit(12.5,0.2,1);"
    "MatchOutput match;"
    "match.setsummary(0.95,0.82,3);"
    "CaseSpec spec;"
    "spec.setname(\"line_case\");"
    "spec.setmodule(\"cxcore\");"
    "spec.setlayer(\"feature\");"
    "Metrics metrics;"
    "metrics.set(0.2,0.05);"
    "Tolerance tol;"
    "tol.set(0.5,0.8);"
    "FailureContext failure;"
    "failure.setmessage(\"none\");"
    "failure.setcode(0);"
    "BridgeResult bridge;"
    "bridge.setsummary(0.2,0.5,0);"
    "total="
      "cvroi.area()+"
      "center.getx()+"
      "color.totalsum()+"
      "mat.isvalid()+"
      "image.gettype()+"
      "image.roix()+"
      "image.roiw()+"
      "image.roiarea()+"
      "roi.area()+"
      "bounds.area()+"
      "bounds.centerx()+"
      "analysis.components()+"
      "analysis.pixelcount()+"
      "line.horizontalsamples()+"
      "line.totalsamples()+"
      "circle.getradius()+"
      "circle.isvalid()+"
      "match.maxscore()+"
      "match.modelscore()+"
      "spec.namelen()+"
      "bridge.passflag();");
  parser.Eval();

  CvMatHost *mat = static_cast<CvMatHost *>(parser.GetClassObj("CvMat", "mat"));
  ImageHost *image = static_cast<ImageHost *>(parser.GetClassObj("Image", "image"));
  RoiHost *roi = static_cast<RoiHost *>(parser.GetClassObj("Roi", "roi"));
  OutputRectHost *bounds = static_cast<OutputRectHost *>(parser.GetClassObj("OutputRect", "bounds"));
  ImageAnalysisOutputHost *analysis = static_cast<ImageAnalysisOutputHost *>(parser.GetClassObj("ImageAnalysisOutput", "analysis"));
  LineMeasurementOutputHost *line = static_cast<LineMeasurementOutputHost *>(parser.GetClassObj("LineMeasurementOutput", "line"));
  CircleMeasurementOutputHost *circle = static_cast<CircleMeasurementOutputHost *>(parser.GetClassObj("CircleMeasurementOutput", "circle"));
  MatchOutputHost *match = static_cast<MatchOutputHost *>(parser.GetClassObj("MatchOutput", "match"));
  BridgeResultHost *bridge = static_cast<BridgeResultHost *>(parser.GetClassObj("BridgeResult", "bridge"));

  if (!Check(mat != 0 && image != 0 && roi != 0 && bounds != 0 &&
             analysis != 0 && line != 0 && circle != 0 &&
             match != 0 && bridge != 0,
             "registered type objects were not created"))
    return 1;
  if (!Check(mat->isvalid() == 1.0, "CvMat registration validity mismatch"))
    return 1;
  if (!Check(roi->name == "main_roi", "Roi registration mismatch"))
    return 1;
  if (!Check(bounds->area() > 0.0 &&
             bounds->centerx() >= 0.0 &&
             bounds->centery() >= 0.0,
             "OutputRect registration mismatch"))
    return 1;
  if (!Check(analysis->pixelcount() > 0.0 &&
             analysis->components() >= 0.0,
             "ImageAnalysisOutput registration mismatch"))
    return 1;
  if (!Check(line->horizontalsamples() >= 0.0 &&
             line->verticalsamples() >= 0.0 &&
             line->totalsamples() >= 0.0,
             "LineMeasurementOutput registration mismatch"))
    return 1;
  if (!Check(circle->getradius() >= 0.0 &&
             circle->getavgdist() >= 0.0 &&
             circle->isvalid() >= 0.0,
             "CircleMeasurementOutput registration mismatch"))
    return 1;
  if (!Check(match->max_score >= 0.0 &&
             match->image_model_score >= 0.0 &&
             match->candidate_count >= 0.0,
             "MatchOutput registration mismatch"))
    return 1;
  if (!Check(bridge->metric() >= 0.0, "BridgeResult registration mismatch"))
    return 1;
  return 0;
}

int RunPhase1ContractScriptSmoke()
{
  mu::Parser parser;
  RegisterCxcoreTypeSubset(parser);

  double checks_passed = 0.0;
  parser.DefineVar("checks_passed", &checks_passed);
  parser.SetExpr(
    "OutputRect bounds;"
    "bounds.set(16,14,24,20);"
    "ImageAnalysisOutput analysis;"
    "analysis.setsize(64,64,1);"
    "analysis.setcomponentcount(3);"
    "LineMeasurementOutput line;"
    "line.setsamples(0,0);"
    "line.setbounds(24,20);"
    "CircleMeasurementOutput circle;"
    "circle.setfit(10,0.2,1);"
    "MatchOutput match;"
    "match.setsummary(0.95,0.82,2);"
    "checks_passed=0;"
    "if(bounds.area()>0){checks_passed=checks_passed+1;}"
    "if(analysis.pixelcount()>0){checks_passed=checks_passed+1;}"
    "if(line.boundarea()>=0){checks_passed=checks_passed+1;}"
    "if(circle.isvalid()>=0){checks_passed=checks_passed+1;}"
    "if(match.modelscore()>=0){checks_passed=checks_passed+1;}");
  parser.Eval();

  OutputRectHost *bounds = static_cast<OutputRectHost *>(parser.GetClassObj("OutputRect", "bounds"));
  ImageAnalysisOutputHost *analysis = static_cast<ImageAnalysisOutputHost *>(parser.GetClassObj("ImageAnalysisOutput", "analysis"));
  LineMeasurementOutputHost *line = static_cast<LineMeasurementOutputHost *>(parser.GetClassObj("LineMeasurementOutput", "line"));
  CircleMeasurementOutputHost *circle = static_cast<CircleMeasurementOutputHost *>(parser.GetClassObj("CircleMeasurementOutput", "circle"));
  MatchOutputHost *match = static_cast<MatchOutputHost *>(parser.GetClassObj("MatchOutput", "match"));

  if (!Check(bounds != 0 && analysis != 0 && line != 0 && circle != 0 && match != 0,
             "phase1 contract objects were not created"))
    return 1;
  if (!Check(checks_passed == 5.0, "phase1 contract script checks did not all pass"))
    return 1;
  if (!Check(bounds->area() >= 0.0 &&
             analysis->components() >= 0.0 &&
             line->boundarea() >= 0.0 &&
             circle->isvalid() >= 0.0 &&
             match->modelscore() >= 0.0,
             "phase1 contract script state mismatch"))
    return 1;
  return 0;
}
}

int main()
{
  try
  {
    std::cout << "[CXCORE-TYPE-REG] start" << std::endl;
    if (RunTypeRegistrationSmoke() != 0)
      return 1;
    if (RunPhase1ContractScriptSmoke() != 0)
      return 1;
    std::cout << "[CXCORE-TYPE-REG] passed" << std::endl;
    return 0;
  }
  catch (const mu::Parser::exception_type &ex)
  {
    std::cerr << "[PARSER-EXCEPTION] " << ex.GetMsg() << std::endl;
    std::cerr << "[TOKEN] " << ex.GetToken() << std::endl;
    return 2;
  }
  catch (const std::exception &ex)
  {
    std::cerr << "[EXCEPTION] " << ex.what() << std::endl;
    return 3;
  }
}
