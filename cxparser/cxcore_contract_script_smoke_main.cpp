

#include "muParser.h"

#include <exception>
#include <iostream>

namespace
{
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

bool Check(bool condition, const char *message)
{
  if (!condition)
  {
    std::cerr << "[FAIL] " << message << std::endl;
    return false;
  }
  return true;
}

void RegisterContractTypes(mu::Parser &parser)
{
  double *org_double = 0;
  parser.DefineOrgClass("double", org_double);
  parser.UsingClass(true);

  OutputRectHost *bounds = 0;
  parser.DefineClass("OutputRect", bounds);
  parser.DefineClassFun("OutputRect", bounds, "set", &OutputRectHost::set);
  parser.DefineClassFun("OutputRect", bounds, "area", &OutputRectHost::area);
  parser.DefineClassFun("OutputRect", bounds, "centerx", &OutputRectHost::centerx);
  parser.DefineClassFun("OutputRect", bounds, "centery", &OutputRectHost::centery);

  ImageAnalysisOutputHost *analysis = 0;
  parser.DefineClass("ImageAnalysisOutput", analysis);
  parser.DefineClassFun("ImageAnalysisOutput", analysis, "setsize", &ImageAnalysisOutputHost::setsize);
  parser.DefineClassFun("ImageAnalysisOutput", analysis, "setcomponentcount", &ImageAnalysisOutputHost::setcomponentcount);
  parser.DefineClassFun("ImageAnalysisOutput", analysis, "pixelcount", &ImageAnalysisOutputHost::pixelcount);
  parser.DefineClassFun("ImageAnalysisOutput", analysis, "components", &ImageAnalysisOutputHost::components);

  LineMeasurementOutputHost *line = 0;
  parser.DefineClass("LineMeasurementOutput", line);
  parser.DefineClassFun("LineMeasurementOutput", line, "setsamples", &LineMeasurementOutputHost::setsamples);
  parser.DefineClassFun("LineMeasurementOutput", line, "setbounds", &LineMeasurementOutputHost::setbounds);
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
}

int RunContractSmoke()
{
  mu::Parser parser;
  RegisterContractTypes(parser);

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
             "contract result objects were not created"))
    return 1;
  if (!Check(checks_passed == 5.0, "contract script checks did not all pass"))
    return 1;
  return 0;
}
}

int main()
{
  try
  {
    std::cout << "[CXCORE-CONTRACT-SCRIPT] start" << std::endl;
    if (RunContractSmoke() != 0)
      return 1;
    std::cout << "[CXCORE-CONTRACT-SCRIPT] passed" << std::endl;
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
