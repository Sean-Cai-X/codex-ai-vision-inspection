

#include "muParser.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#include <sys/stat.h>
#define CX_STAT _stat
#else
#include <sys/stat.h>
#include <sys/types.h>
#define CX_STAT stat
#endif

namespace
{
struct ExternalModuleIndex
{
  const char *name;
  const char *root_hint;
  const char *role;
  const char *status;
};

const ExternalModuleIndex kFoundationModules[] = {
  {"cxparser", "cxparser", "script orchestration and object binding", "active"},
  {"cxcore", "cxcore", "image features and parser host integration", "active"},
  {"opencascade", "opencascade-7.7.0", "geometry kernel and scene model", "ready"},
  {"CCCoreLib", "CCCoreLib", "point cloud analysis and octree algorithms", "ready"}
};

bool NearlyEqual(double lhs, double rhs, double eps = 1e-9)
{
  return std::fabs(lhs - rhs) <= eps;
}

bool PathExists(const std::string &path)
{
  struct CX_STAT info;
  return CX_STAT(path.c_str(), &info) == 0;
}

std::string JoinPath(const std::string &lhs, const std::string &rhs)
{
  if (lhs.empty())
    return rhs;
  if (lhs == ".")
    return rhs;
  return lhs + "/" + rhs;
}

std::string FindWorkspacePath(const char *name)
{
  const char *roots[] = {".", "..", "../..", "../../..", "../../../..", "../../../../.."};
  for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i)
  {
    const std::string candidate = JoinPath(roots[i], name);
    if (PathExists(candidate))
      return candidate;
  }
  return std::string();
}

void PrintFoundationIndex()
{
  std::cout << "[GROUP] foundation_module_index" << std::endl;
  for (size_t i = 0; i < sizeof(kFoundationModules) / sizeof(kFoundationModules[0]); ++i)
  {
    const std::string resolved = FindWorkspacePath(kFoundationModules[i].root_hint);
    std::cout << "  [MODULE] name=" << kFoundationModules[i].name
              << " role=" << kFoundationModules[i].role
              << " status=" << kFoundationModules[i].status
              << " path=" << (resolved.empty() ? "missing" : resolved)
              << std::endl;
  }
}

class ImageKernel
{
public:
  ImageKernel()
    : load_count(0)
    , detect_count(0)
    , width(0.0)
    , height(0.0)
  {
  }

  void Load(double image_width, double image_height)
  {
    ++load_count;
    width = image_width;
    height = image_height;
  }

  void Detect()
  {
    ++detect_count;
  }

  int load_count;
  int detect_count;
  double width;
  double height;
};

class GeometryKernel
{
public:
  GeometryKernel()
    : wire_count(0)
    , face_count(0)
    , shape_count(0)
  {
  }

  void BuildWire()
  {
    ++wire_count;
  }

  void BuildFace()
  {
    ++face_count;
  }

  void PublishShape()
  {
    ++shape_count;
  }

  int wire_count;
  int face_count;
  int shape_count;
};

class CloudKernel
{
public:
  CloudKernel()
    : octree_count(0)
    , metric_count(0)
  {
  }

  void BuildOctree()
  {
    ++octree_count;
  }

  void Measure()
  {
    ++metric_count;
  }

  int octree_count;
  int metric_count;
};

class ViewKernel
{
public:
  ViewKernel()
    : bind_count(0)
    , present_count(0)
  {
  }

  void BindScene()
  {
    ++bind_count;
  }

  void Present()
  {
    ++present_count;
  }

  int bind_count;
  int present_count;
};

class FoundationFlow
{
public:
  FoundationFlow()
    : stage_count(0)
    , stage_sum(0.0)
    , last_stage(0.0)
  {
  }

  void Step(double stage)
  {
    if (stage_count > 0)
      stage_trace << ">";
    ++stage_count;
    stage_sum += stage;
    last_stage = stage;
    stage_trace << static_cast<int>(stage);
  }

  double Score()
  {
    return stage_sum;
  }

  int stage_count;
  double stage_sum;
  double last_stage;
  std::ostringstream stage_trace;
};

void ConfigureFlowParser(mu::Parser &parser)
{
  double *org_double = 0;
  parser.DefineOrgClass("double", org_double);
  parser.UsingClass(true);

  ImageKernel *image = 0;
  parser.DefineClass("ImageKernel", image);
  parser.DefineClassFun("ImageKernel", image, "Load", &ImageKernel::Load);
  parser.DefineClassFun("ImageKernel", image, "Detect", &ImageKernel::Detect);

  GeometryKernel *geometry = 0;
  parser.DefineClass("GeometryKernel", geometry);
  parser.DefineClassFun("GeometryKernel", geometry, "BuildWire", &GeometryKernel::BuildWire);
  parser.DefineClassFun("GeometryKernel", geometry, "BuildFace", &GeometryKernel::BuildFace);
  parser.DefineClassFun("GeometryKernel", geometry, "PublishShape", &GeometryKernel::PublishShape);

  CloudKernel *cloud = 0;
  parser.DefineClass("CloudKernel", cloud);
  parser.DefineClassFun("CloudKernel", cloud, "BuildOctree", &CloudKernel::BuildOctree);
  parser.DefineClassFun("CloudKernel", cloud, "Measure", &CloudKernel::Measure);

  ViewKernel *view = 0;
  parser.DefineClass("ViewKernel", view);
  parser.DefineClassFun("ViewKernel", view, "BindScene", &ViewKernel::BindScene);
  parser.DefineClassFun("ViewKernel", view, "Present", &ViewKernel::Present);

  FoundationFlow *flow = 0;
  parser.DefineClass("FoundationFlow", flow);
  parser.DefineClassFun("FoundationFlow", flow, "Step", &FoundationFlow::Step);
  parser.DefineClassFun("FoundationFlow", flow, "Score", &FoundationFlow::Score);
}

bool Check(bool condition, const char *message)
{
  if (!condition)
  {
    std::cerr << "[FAIL] " << message << std::endl;
    return false;
  }
  return true;
}

bool CheckOrSkip(bool condition, const char *message)
{
  if (!condition)
  {
    std::cout << "[SKIP] " << message << std::endl;
    return false;
  }
  return true;
}

int RunWorkspaceCheck()
{
  std::cout << "[GROUP] workspace_roots" << std::endl;
  bool has_external_deps = true;
  for (size_t i = 0; i < sizeof(kFoundationModules) / sizeof(kFoundationModules[0]); ++i)
  {
    const std::string resolved = FindWorkspacePath(kFoundationModules[i].root_hint);
    std::cout << "  [ROOT] " << kFoundationModules[i].root_hint
              << "=" << (resolved.empty() ? "missing" : resolved)
              << std::endl;

    const bool required = std::string(kFoundationModules[i].name) == "cxparser" ||
                          std::string(kFoundationModules[i].name) == "cxcore";
    if (required)
    {
      if (!Check(!resolved.empty(), kFoundationModules[i].root_hint))
        return 1;
    }
    else if (!resolved.empty())
    {
      continue;
    }
    else
    {
      has_external_deps = false;
      CheckOrSkip(false, kFoundationModules[i].root_hint);
    }
  }

  if (!has_external_deps)
    std::cout << "  [ROOT] external geometry/cloud dependencies are optional for parser-only validation" << std::endl;

  return 0;
}

int RunFoundationFlowSmoke()
{
  std::cout << "[GROUP] foundation_flow" << std::endl;

  mu::Parser parser;
  ConfigureFlowParser(parser);

  double ready = 1.0;
  double score = 0.0;
  parser.DefineVar("ready", &ready);
  parser.DefineVar("score", &score);

  parser.SetExpr(
    "FoundationFlow flow;"
    "ImageKernel image;"
    "GeometryKernel geom;"
    "CloudKernel cloud;"
    "ViewKernel view;"
    "flow.Step(1);"
    "image.Load(1920,1080);"
    "image.Detect();"
    "flow.Step(2);"
    "geom.BuildWire();"
    "geom.BuildFace();"
    "geom.PublishShape();"
    "flow.Step(3);"
    "if(ready){cloud.BuildOctree();cloud.Measure();flow.Step(4);view.BindScene();view.Present();flow.Step(5);}"
    "score=flow.Score();"
  );
  parser.Eval();

  ImageKernel *image = static_cast<ImageKernel *>(parser.GetClassObj("ImageKernel", "image"));
  GeometryKernel *geometry = static_cast<GeometryKernel *>(parser.GetClassObj("GeometryKernel", "geom"));
  CloudKernel *cloud = static_cast<CloudKernel *>(parser.GetClassObj("CloudKernel", "cloud"));
  ViewKernel *view = static_cast<ViewKernel *>(parser.GetClassObj("ViewKernel", "view"));
  FoundationFlow *flow = static_cast<FoundationFlow *>(parser.GetClassObj("FoundationFlow", "flow"));

  if (!Check(image != 0, "image kernel object was not created"))
    return 1;
  if (!Check(geometry != 0, "geometry kernel object was not created"))
    return 1;
  if (!Check(cloud != 0, "cloud kernel object was not created"))
    return 1;
  if (!Check(view != 0, "view kernel object was not created"))
    return 1;
  if (!Check(flow != 0, "foundation flow object was not created"))
    return 1;

  std::cout << "  [FLOW] trace=" << flow->stage_trace.str()
            << " score=" << score
            << " image=" << image->width << "x" << image->height
            << std::endl;

  if (!Check(image->load_count == 1, "image load count mismatch"))
    return 1;
  if (!Check(image->detect_count == 1, "image detect count mismatch"))
    return 1;
  if (!Check(geometry->wire_count == 1, "geometry wire count mismatch"))
    return 1;
  if (!Check(geometry->face_count == 1, "geometry face count mismatch"))
    return 1;
  if (!Check(geometry->shape_count == 1, "geometry publish count mismatch"))
    return 1;
  if (!Check(cloud->octree_count == 1, "cloud octree count mismatch"))
    return 1;
  if (!Check(cloud->metric_count == 1, "cloud metric count mismatch"))
    return 1;
  if (!Check(view->bind_count == 1, "view bind count mismatch"))
    return 1;
  if (!Check(view->present_count == 1, "view present count mismatch"))
    return 1;
  if (!Check(flow->stage_count == 5, "flow stage count mismatch"))
    return 1;
  if (!Check(flow->stage_trace.str() == "1>2>3>4>5", "flow stage order mismatch"))
    return 1;
  if (!Check(NearlyEqual(flow->last_stage, 5.0), "flow last stage mismatch"))
    return 1;
  if (!Check(NearlyEqual(score, 15.0), "flow score mismatch"))
    return 1;

  return 0;
}

int RunFoundationStandardFlow()
{
  std::cout << "[GROUP] foundation_standard" << std::endl;

  for (int run = 0; run < 10; ++run)
  {
    mu::Parser parser;
    ConfigureFlowParser(parser);

    double ready = 1.0;
    double score = 0.0;
    parser.DefineVar("ready", &ready);
    parser.DefineVar("score", &score);

    parser.SetExpr(
      "FoundationFlow flow;"
      "ImageKernel image;"
      "GeometryKernel geom;"
      "CloudKernel cloud;"
      "ViewKernel view;"
      "flow.Step(1);"
      "image.Load(640,480);"
      "image.Detect();"
      "flow.Step(2);"
      "geom.BuildWire();"
      "geom.PublishShape();"
      "flow.Step(3);"
      "if(ready){cloud.BuildOctree();view.BindScene();view.Present();flow.Step(4);}"
      "score=flow.Score();"
    );
    parser.Eval();

    FoundationFlow *flow = static_cast<FoundationFlow *>(parser.GetClassObj("FoundationFlow", "flow"));
    if (!Check(flow != 0, "standard flow object was not created"))
      return 1;
    if (!Check(flow->stage_count == 4, "standard flow stage count mismatch"))
      return 1;
    if (!Check(flow->stage_trace.str() == "1>2>3>4", "standard flow stage order mismatch"))
      return 1;
    if (!Check(NearlyEqual(score, 10.0), "standard flow score mismatch"))
      return 1;
  }

  std::cout << "  [STANDARD] repeated flow cycles=10" << std::endl;
  return 0;
}

int RunFoundationStressFlow()
{
  std::cout << "[GROUP] foundation_stress" << std::endl;

  const int parser_runs = 200;
  double score_acc = 0.0;

  for (int run = 0; run < parser_runs; ++run)
  {
    mu::Parser parser;
    ConfigureFlowParser(parser);

    double ready = 1.0;
    double score = 0.0;
    parser.DefineVar("ready", &ready);
    parser.DefineVar("score", &score);

    parser.SetExpr(
      "FoundationFlow flow;"
      "ImageKernel image;"
      "GeometryKernel geom;"
      "CloudKernel cloud;"
      "ViewKernel view;"
      "double idx=0;"
      "while(idx<5){flow.Step(idx+1);idx=idx+1;}"
      "image.Load(320,240);"
      "image.Detect();"
      "geom.BuildWire();"
      "geom.BuildFace();"
      "cloud.BuildOctree();"
      "cloud.Measure();"
      "view.BindScene();"
      "view.Present();"
      "score=flow.Score();"
    );
    parser.Eval();

    FoundationFlow *flow = static_cast<FoundationFlow *>(parser.GetClassObj("FoundationFlow", "flow"));
    if (!Check(flow != 0, "stress flow object was not created"))
      return 1;
    if (!Check(flow->stage_count == 5, "stress flow stage count mismatch"))
      return 1;
    if (!Check(NearlyEqual(score, 15.0), "stress flow score mismatch"))
      return 1;

    score_acc += score;
  }

  std::cout << "  [STRESS] parser_runs=" << parser_runs
            << " accumulated_score=" << score_acc << std::endl;
  return 0;
}
}

int main()
{
  try
  {
    std::cout << "[FOUNDATION-SMOKE] start" << std::endl;
    PrintFoundationIndex();

    int status = 0;
    status += RunWorkspaceCheck();
    status += RunFoundationFlowSmoke();
    status += RunFoundationStandardFlow();
    status += RunFoundationStressFlow();

    if (status != 0)
      return 1;

    std::cout << "[FOUNDATION-SMOKE] passed" << std::endl;
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
