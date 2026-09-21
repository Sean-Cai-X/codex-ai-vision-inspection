#ifndef CXIMAGE_CXSCRIPT_DIRECT_BINDINGS_H
#define CXIMAGE_CXSCRIPT_DIRECT_BINDINGS_H

#include "muParser.h"

// Thin phase-one binding surfaces. They only reserve CxScript-visible type and
// method names. No script path, flow, global input/output, or algorithm is
// attached here; unavailable runtime work remains pending_binding.
class PendingObjectBinding
{
public:
  void set_pending(mu::charpvect&) {}
  int pending_binding() { return 1; }
};

class FormfitGaugeBinding : public PendingObjectBinding {};
class CxOverlayBinding : public PendingObjectBinding {};
class TorchOpsBinding : public PendingObjectBinding
{
public:
  int load_model(mu::charpvect&) { return 0; }
  int set_device(mu::charpvect&) { return 0; }
  int preprocess_image(mu::charpvect&) { return 0; }
  int normalize(mu::charpvect&) { return 0; }
  int resize(mu::charpvect&) { return 0; }
  int to_tensor(mu::charpvect&) { return 0; }
  int run_infer(mu::charpvect&) { return 0; }
  int decode_mask(mu::charpvect&) { return 0; }
  int decode_detection(mu::charpvect&) { return 0; }
  int feature_visualize(mu::charpvect&) { return 0; }
  int eval_metric(mu::charpvect&) { return 0; }
};

class MlpackOpsBinding : public PendingObjectBinding
{
public:
  int build_feature(mu::charpvect&) { return 0; }
  int build_feature_from_mask(mu::charpvect&) { return 0; }
  int build_dataset(mu::charpvect&) { return 0; }
  int load_model(mu::charpvect&) { return 0; }
  int normalize_feature(mu::charpvect&) { return 0; }
  int predict(mu::charpvect&) { return 0; }
  int score(mu::charpvect&) { return 0; }
  int attach_result(mu::charpvect&) { return 0; }
};

class EnsmallenOpsBinding : public PendingObjectBinding
{
public:
  int build_objective(mu::charpvect&) { return 0; }
  int define_param_space(mu::charpvect&) { return 0; }
  int optimize_step(mu::charpvect&) { return 0; }
  int evaluate_candidate(mu::charpvect&) { return 0; }
  int choose_best(mu::charpvect&) { return 0; }
  int replay_best(mu::charpvect&) { return 0; }
  int compare_baseline(mu::charpvect&) { return 0; }
  int get_history(mu::charpvect&) { return 0; }
};

class CximageOpsBinding : public PendingObjectBinding
{
public:
  int overlay_mask(mu::charpvect&) { return 0; }
  int measure_circle_metric(mu::charpvect&) { return 0; }
};
class TorchModelBinding : public PendingObjectBinding
{
public:
  void load(const char*) {}
  void set_device(const char*) {}
  int infer(mu::charpvect&) { return 0; }
  void release() {}
};
class TorchTensorBinding : public PendingObjectBinding
{
public:
  void resize(mu::charpvect&) {}
  void normalize(mu::charpvect&) {}
};
class TorchMaskBinding : public PendingObjectBinding {};
class TorchRawOutputBinding : public PendingObjectBinding {};
class TorchFeatureMapBinding : public PendingObjectBinding {};
class TorchDeviceBinding : public PendingObjectBinding {};

class MlpackFeatureBinding : public PendingObjectBinding {};
class MlpackDatasetBinding : public PendingObjectBinding
{
public:
  void normalize(int) {}
};
class MlpackModelBinding : public PendingObjectBinding
{
public:
  void load(const char*) {}
  int predict(mu::charpvect&) { return 0; }
};
class MlpackPredictionBinding : public PendingObjectBinding {};
class MlpackScoreBinding : public PendingObjectBinding {};
class MlpackNormalizerBinding : public PendingObjectBinding {};

class EnsmallenObjectiveBinding : public PendingObjectBinding {};
class EnsmallenParamSpaceBinding : public PendingObjectBinding
{
public:
  void set_range(mu::charpvect&) {}
};
class EnsmallenOptimizerBinding : public PendingObjectBinding
{
public:
  void set_max_iter(int) {}
  void set_tolerance(double) {}
  int optimize_step(mu::charpvect&) { return 0; }
};
class EnsmallenCandidateBinding : public PendingObjectBinding
{
public:
  int get_int(mu::charpvect&) { return 0; }
};
class EnsmallenBestParamBinding : public PendingObjectBinding {};
class EnsmallenMetricBinding : public PendingObjectBinding {};
class EnsmallenHistoryBinding : public PendingObjectBinding {};

class FindSegmentationBinding : public PendingObjectBinding
{
public:
    void setbackend(mu::charpvect&) {}
    void setmodel(mu::charpvect&) {}
    void settask(mu::charpvect&) {}

    void setmodelid(mu::charpvect&) {}

    void setmodelpackage(mu::charpvect&) {}

    void setmanifest(mu::charpvect&) {}

    void setpostprocessprofile(mu::charpvect&) {}

    void setparameterprofile(mu::charpvect&) {}


    void setdevice(mu::charpvect&) {}
    void setthreshold(double) {}
    void setpromptrect(int, int, int, int) {}
    void setpoint(int, int) {}
    void setmode(int) {}
    void setgeometrytype(mu::charpvect&) {}

    void segment(mu::charpvect&) {}
    void extractboundary() {}
    void buildoverlay(mu::charpvect&) {}
    int status_code() { return 0; }
    int get_contour_count() { return 0; }
    double get_primary_area() { return 0.0; }
    int get_geometry_count() { return 0; }
    double get_geometry_residual() { return 0.0; }
    double get_geometry_support() { return 0.0; }
    double get_geometry_center_x() { return 0.0; }
    double get_geometry_center_y() { return 0.0; }
    double get_geometry_radius() { return 0.0; }
    double get_geometry_axis_x() { return 0.0; }
    double get_geometry_axis_y() { return 0.0; }
    double get_geometry_angle_deg() { return 0.0; }

};

inline void RegisterPendingDirectCxScriptBindings(mu::Parser& parser)
{
  FormfitGaugeBinding* formfit_gauge = nullptr; parser.DefineClass("FormfitGauge", formfit_gauge);
  CxOverlayBinding* overlay = nullptr; parser.DefineClass("CxOverlay", overlay);
  TorchOpsBinding* torch_ops = nullptr;
  parser.DefineClass("TorchOps", torch_ops);
  parser.DefineClassFun("TorchOps", torch_ops, "load_model", &TorchOpsBinding::load_model);
  parser.DefineClassFun("TorchOps", torch_ops, "set_device", &TorchOpsBinding::set_device);
  parser.DefineClassFun("TorchOps", torch_ops, "preprocess_image", &TorchOpsBinding::preprocess_image);
  parser.DefineClassFun("TorchOps", torch_ops, "normalize", &TorchOpsBinding::normalize);
  parser.DefineClassFun("TorchOps", torch_ops, "resize", &TorchOpsBinding::resize);
  parser.DefineClassFun("TorchOps", torch_ops, "to_tensor", &TorchOpsBinding::to_tensor);
  parser.DefineClassFun("TorchOps", torch_ops, "run_infer", &TorchOpsBinding::run_infer);
  parser.DefineClassFun("TorchOps", torch_ops, "decode_mask", &TorchOpsBinding::decode_mask);
  parser.DefineClassFun("TorchOps", torch_ops, "decode_detection", &TorchOpsBinding::decode_detection);
  parser.DefineClassFun("TorchOps", torch_ops, "feature_visualize", &TorchOpsBinding::feature_visualize);
  parser.DefineClassFun("TorchOps", torch_ops, "eval_metric", &TorchOpsBinding::eval_metric);

  MlpackOpsBinding* mlpack_ops = nullptr;
  parser.DefineClass("MlpackOps", mlpack_ops);
  parser.DefineClassFun("MlpackOps", mlpack_ops, "build_feature", &MlpackOpsBinding::build_feature);
  parser.DefineClassFun("MlpackOps", mlpack_ops, "build_feature_from_mask", &MlpackOpsBinding::build_feature_from_mask);
  parser.DefineClassFun("MlpackOps", mlpack_ops, "build_dataset", &MlpackOpsBinding::build_dataset);
  parser.DefineClassFun("MlpackOps", mlpack_ops, "load_model", &MlpackOpsBinding::load_model);
  parser.DefineClassFun("MlpackOps", mlpack_ops, "normalize_feature", &MlpackOpsBinding::normalize_feature);
  parser.DefineClassFun("MlpackOps", mlpack_ops, "predict", &MlpackOpsBinding::predict);
  parser.DefineClassFun("MlpackOps", mlpack_ops, "score", &MlpackOpsBinding::score);
  parser.DefineClassFun("MlpackOps", mlpack_ops, "attach_result", &MlpackOpsBinding::attach_result);

  EnsmallenOpsBinding* ensmallen_ops = nullptr;
  parser.DefineClass("EnsmallenOps", ensmallen_ops);
  parser.DefineClassFun("EnsmallenOps", ensmallen_ops, "build_objective", &EnsmallenOpsBinding::build_objective);
  parser.DefineClassFun("EnsmallenOps", ensmallen_ops, "define_param_space", &EnsmallenOpsBinding::define_param_space);
  parser.DefineClassFun("EnsmallenOps", ensmallen_ops, "optimize_step", &EnsmallenOpsBinding::optimize_step);
  parser.DefineClassFun("EnsmallenOps", ensmallen_ops, "evaluate_candidate", &EnsmallenOpsBinding::evaluate_candidate);
  parser.DefineClassFun("EnsmallenOps", ensmallen_ops, "choose_best", &EnsmallenOpsBinding::choose_best);
  parser.DefineClassFun("EnsmallenOps", ensmallen_ops, "replay_best", &EnsmallenOpsBinding::replay_best);
  parser.DefineClassFun("EnsmallenOps", ensmallen_ops, "compare_baseline", &EnsmallenOpsBinding::compare_baseline);
  parser.DefineClassFun("EnsmallenOps", ensmallen_ops, "get_history", &EnsmallenOpsBinding::get_history);

  CximageOpsBinding* cximage_ops = nullptr;
  parser.DefineClass("CximageOps", cximage_ops);
  parser.DefineClassFun("CximageOps", cximage_ops, "overlay_mask", &CximageOpsBinding::overlay_mask);
  parser.DefineClassFun("CximageOps", cximage_ops, "measure_circle_metric", &CximageOpsBinding::measure_circle_metric);
  TorchModelBinding* torch_model = nullptr;
  parser.DefineClass("TorchModel", torch_model);
  parser.DefineClass("TorchSegModel", torch_model);
  parser.DefineClass("TorchDetectModel", torch_model);
  parser.DefineClassFun("TorchModel", torch_model, "load", &TorchModelBinding::load);
  parser.DefineClassFun("TorchModel", torch_model, "set_device", &TorchModelBinding::set_device);
  parser.DefineClassFun("TorchModel", torch_model, "infer", &TorchModelBinding::infer);
  parser.DefineClassFun("TorchModel", torch_model, "release", &TorchModelBinding::release);
  parser.DefineClassFun("TorchSegModel", torch_model, "load", &TorchModelBinding::load);
  parser.DefineClassFun("TorchSegModel", torch_model, "set_device", &TorchModelBinding::set_device);
  parser.DefineClassFun("TorchSegModel", torch_model, "infer", &TorchModelBinding::infer);
  parser.DefineClassFun("TorchDetectModel", torch_model, "load", &TorchModelBinding::load);
  parser.DefineClassFun("TorchDetectModel", torch_model, "set_device", &TorchModelBinding::set_device);
  parser.DefineClassFun("TorchDetectModel", torch_model, "infer", &TorchModelBinding::infer);

  TorchTensorBinding* tensor = nullptr;
  parser.DefineClass("TorchTensor", tensor);
  parser.DefineClassFun("TorchTensor", tensor, "resize", &TorchTensorBinding::resize);
  parser.DefineClassFun("TorchTensor", tensor, "normalize", &TorchTensorBinding::normalize);
  TorchMaskBinding* mask = nullptr; parser.DefineClass("TorchMask", mask);
  TorchRawOutputBinding* raw = nullptr; parser.DefineClass("TorchRawOutput", raw);
  TorchFeatureMapBinding* feature_map = nullptr; parser.DefineClass("TorchFeatureMap", feature_map);
  TorchDeviceBinding* device = nullptr; parser.DefineClass("TorchDevice", device);

  MlpackFeatureBinding* feature = nullptr; parser.DefineClass("MlpackFeature", feature);
  MlpackDatasetBinding* dataset = nullptr;
  parser.DefineClass("MlpackDataset", dataset);
  parser.DefineClassFun("MlpackDataset", dataset, "normalize", &MlpackDatasetBinding::normalize);
  MlpackModelBinding* model = nullptr;
  parser.DefineClass("MlpackModel", model);
  parser.DefineClass("MlpackLogRegModel", model);
  parser.DefineClassFun("MlpackModel", model, "load", &MlpackModelBinding::load);
  parser.DefineClassFun("MlpackModel", model, "predict", &MlpackModelBinding::predict);
  parser.DefineClassFun("MlpackLogRegModel", model, "load", &MlpackModelBinding::load);
  parser.DefineClassFun("MlpackLogRegModel", model, "predict", &MlpackModelBinding::predict);
  MlpackPredictionBinding* prediction = nullptr; parser.DefineClass("MlpackPrediction", prediction);
  MlpackScoreBinding* score = nullptr; parser.DefineClass("MlpackScore", score);
  MlpackNormalizerBinding* normalizer = nullptr; parser.DefineClass("MlpackNormalizer", normalizer);

  EnsmallenObjectiveBinding* objective = nullptr; parser.DefineClass("EnsmallenObjective", objective);
  EnsmallenParamSpaceBinding* space = nullptr;
  parser.DefineClass("EnsmallenParamSpace", space);
  parser.DefineClassFun("EnsmallenParamSpace", space, "set_range", &EnsmallenParamSpaceBinding::set_range);
  EnsmallenOptimizerBinding* optimizer = nullptr;
  parser.DefineClass("EnsmallenOptimizer", optimizer);
  parser.DefineClassFun("EnsmallenOptimizer", optimizer, "set_max_iter", &EnsmallenOptimizerBinding::set_max_iter);
  parser.DefineClassFun("EnsmallenOptimizer", optimizer, "set_tolerance", &EnsmallenOptimizerBinding::set_tolerance);
  parser.DefineClassFun("EnsmallenOptimizer", optimizer, "optimize_step", &EnsmallenOptimizerBinding::optimize_step);
  EnsmallenCandidateBinding* candidate = nullptr;
  parser.DefineClass("EnsmallenCandidate", candidate);
  parser.DefineClassFun("EnsmallenCandidate", candidate, "get_int", &EnsmallenCandidateBinding::get_int);
  EnsmallenBestParamBinding* best = nullptr; parser.DefineClass("EnsmallenBestParam", best);
  EnsmallenMetricBinding* metric = nullptr; parser.DefineClass("EnsmallenMetric", metric);
  EnsmallenHistoryBinding* history = nullptr; parser.DefineClass("EnsmallenHistory", history);

  // FindSegmentation pending binding is intentionally not registered here.
  // The real cximage FindSegmentation backend and AutoBoundary ShapeElement
  // tool are available, but parser binding must be enabled in a separate
  // reviewed step so it cannot break the shared Parser initialization path.
}

#endif