#include "parser_runtime_facade.h"

#include <exception>

#include "../../cxparser/muParser.h"
#include "../scenarios/image_probe_wrapper.h"

namespace cxparser_ext
{
namespace
{
int NormalizeParserPos(std::size_t pos)
{
  return (pos == static_cast<std::size_t>(-1)) ? -1 : static_cast<int>(pos);
}

double GeometryReturnOne1(double)
{
  return 1.0;
}

double GeometryReturnOne2(double, double)
{
  return 1.0;
}

double GeometryReturnOne3(double, double, double)
{
  return 1.0;
}

double GeometryReturnOne4(double, double, double, double)
{
  return 1.0;
}

double ContractCheck(double value)
{
  return value != 0.0 ? 1.0 : 0.0;
}

double ContractPrint(double value)
{
  return value;
}

double ContractLearnTemplateModel(double)
{
  return 1.0;
}

double ContractMeasureLine(double, double)
{
  return 1.0;
}

double ContractMeasureCircle(double, double)
{
  return 1.0;
}

double ContractMatchTemplate(double, double)
{
  return 1.0;
}

double ContractAnalyzeRegionBoundary(double, double)
{
  return 1.0;
}

struct GeometryContractState
{
  bool roi_exported = false;
  bool line_exported = false;
  bool pointset_exported = false;
  bool input_prior_aligned = false;
  bool torch_request_built = false;
  bool mask_exported = false;
  bool boundary_exported = false;
  bool keypoints_exported = false;
  bool label_aligned = false;
  bool label_packet_built = false;
  bool roi_attached = false;
  bool mask_attached = false;
  bool boundary_attached = false;
  bool keypoints_attached = false;
  bool attach_packet_published = false;
};

GeometryContractState &MutableGeometryContractState()
{
  static GeometryContractState state;
  return state;
}

void ResetGeometryContractState()
{
  MutableGeometryContractState() = GeometryContractState();
}

double GeometryExportRoi(double, double)
{
  GeometryContractState &state = MutableGeometryContractState();
  state.roi_exported = true;
  return 1.0;
}

double GeometryExportLine(double)
{
  GeometryContractState &state = MutableGeometryContractState();
  state.line_exported = true;
  return 1.0;
}

double GeometryExportPointSet(double)
{
  GeometryContractState &state = MutableGeometryContractState();
  state.pointset_exported = true;
  return 1.0;
}

double GeometryAlignInputPrior(double, double, double)
{
  GeometryContractState &state = MutableGeometryContractState();
  state.input_prior_aligned =
      state.roi_exported && state.line_exported && state.pointset_exported;
  return state.input_prior_aligned ? 1.0 : 0.0;
}

double GeometryBuildTorchRequest(double, double)
{
  GeometryContractState &state = MutableGeometryContractState();
  state.torch_request_built = state.input_prior_aligned;
  return state.torch_request_built ? 1.0 : 0.0;
}

double GeometryCheckInputReady(double, double)
{
  const GeometryContractState &state = MutableGeometryContractState();
  return (state.roi_exported &&
          state.line_exported &&
          state.pointset_exported &&
          state.input_prior_aligned &&
          state.torch_request_built) ? 1.0 : 0.0;
}

double GeometryExportMaskLabel(double, double, double)
{
  GeometryContractState &state = MutableGeometryContractState();
  state.mask_exported = state.roi_exported;
  return state.mask_exported ? 1.0 : 0.0;
}

double GeometryExportBoundaryLabel(double, double, double)
{
  GeometryContractState &state = MutableGeometryContractState();
  state.boundary_exported = state.roi_exported;
  return state.boundary_exported ? 1.0 : 0.0;
}

double GeometryExportKeypointsLabel(double, double, double)
{
  GeometryContractState &state = MutableGeometryContractState();
  state.keypoints_exported = state.roi_exported;
  return state.keypoints_exported ? 1.0 : 0.0;
}

double GeometryAlignTrainingLabel(double, double, double, double)
{
  GeometryContractState &state = MutableGeometryContractState();
  state.label_aligned =
      state.roi_exported &&
      state.mask_exported &&
      state.boundary_exported &&
      state.keypoints_exported;
  return state.label_aligned ? 1.0 : 0.0;
}

double GeometryBuildTorchLabelPacket(double, double)
{
  GeometryContractState &state = MutableGeometryContractState();
  state.label_packet_built = state.label_aligned;
  return state.label_packet_built ? 1.0 : 0.0;
}

double GeometryCheckLabelReady(double, double)
{
  const GeometryContractState &state = MutableGeometryContractState();
  return (state.roi_exported &&
          state.mask_exported &&
          state.boundary_exported &&
          state.keypoints_exported &&
          state.label_aligned &&
          state.label_packet_built) ? 1.0 : 0.0;
}

double GeometryAttachResultToRoi(double, double, double, double)
{
  GeometryContractState &state = MutableGeometryContractState();
  state.roi_attached = true;
  return 1.0;
}

double GeometryAttachResultMask(double, double)
{
  GeometryContractState &state = MutableGeometryContractState();
  state.mask_attached = state.roi_attached;
  return state.mask_attached ? 1.0 : 0.0;
}

double GeometryAttachResultBoundary(double, double)
{
  GeometryContractState &state = MutableGeometryContractState();
  state.boundary_attached = state.roi_attached;
  return state.boundary_attached ? 1.0 : 0.0;
}

double GeometryAttachResultKeypoints(double, double)
{
  GeometryContractState &state = MutableGeometryContractState();
  state.keypoints_attached = state.roi_attached;
  return state.keypoints_attached ? 1.0 : 0.0;
}

double GeometryPublishAttachPacket(double)
{
  GeometryContractState &state = MutableGeometryContractState();
  state.attach_packet_published =
      state.roi_attached &&
      (state.mask_attached ||
       state.boundary_attached ||
       state.keypoints_attached);
  return state.attach_packet_published ? 1.0 : 0.0;
}

double GeometryCheckAttachReady(double)
{
  const GeometryContractState &state = MutableGeometryContractState();
  return (state.roi_attached &&
          (state.mask_attached ||
           state.boundary_attached ||
           state.keypoints_attached) &&
          state.attach_packet_published) ? 1.0 : 0.0;
}

double GeometryCheckReplayReady(double)
{
  const GeometryContractState &state = MutableGeometryContractState();
  const bool input_ready =
      state.roi_exported &&
      state.line_exported &&
      state.pointset_exported &&
      state.input_prior_aligned &&
      state.torch_request_built;
  const bool label_ready =
      state.roi_exported &&
      state.mask_exported &&
      state.boundary_exported &&
      state.keypoints_exported &&
      state.label_aligned &&
      state.label_packet_built;
  const bool attach_ready =
      state.roi_attached &&
      (state.mask_attached ||
       state.boundary_attached ||
       state.keypoints_attached) &&
      state.attach_packet_published;
  return (input_ready && label_ready && attach_ready) ? 1.0 : 0.0;
}

double GeometrySymbolValue(const char *name)
{
  if (name == 0)
    return 1.0;

  const std::string symbol(name);
  if (symbol == "pred_score")
    return 0.95;

  return 1.0;
}

void RegisterGeometryContractBindings(mu::Parser &parser)
{
  ResetGeometryContractState();

  static double image_path = GeometrySymbolValue("image_path");
  static double roi_id = GeometrySymbolValue("roi_id");
  static double line_id = GeometrySymbolValue("line_id");
  static double pointset_id = GeometrySymbolValue("pointset_id");
  static double input_prior_bundle = GeometrySymbolValue("input_prior_bundle");
  static double mask_id = GeometrySymbolValue("mask_id");
  static double boundary_id = GeometrySymbolValue("boundary_id");
  static double keypoints_id = GeometrySymbolValue("keypoints_id");
  static double label_align_bundle = GeometrySymbolValue("label_align_bundle");
  static double pred_class = GeometrySymbolValue("pred_class");
  static double pred_score = GeometrySymbolValue("pred_score");
  static double embedding_main = GeometrySymbolValue("embedding_main");

  parser.DefineVar("image_path", &image_path);
  parser.DefineVar("roi_id", &roi_id);
  parser.DefineVar("line_id", &line_id);
  parser.DefineVar("pointset_id", &pointset_id);
  parser.DefineVar("input_prior_bundle", &input_prior_bundle);
  parser.DefineVar("mask_id", &mask_id);
  parser.DefineVar("boundary_id", &boundary_id);
  parser.DefineVar("keypoints_id", &keypoints_id);
  parser.DefineVar("label_align_bundle", &label_align_bundle);
  parser.DefineVar("pred_class", &pred_class);
  parser.DefineVar("pred_score", &pred_score);
  parser.DefineVar("embedding_main", &embedding_main);

  parser.DefineFun("geom_export_roi", &GeometryExportRoi);
  parser.DefineFun("geom_export_line", &GeometryExportLine);
  parser.DefineFun("geom_export_pointset", &GeometryExportPointSet);
  parser.DefineFun("geom_align_input_prior", &GeometryAlignInputPrior);
  parser.DefineFun("geom_build_torch_request", &GeometryBuildTorchRequest);
  parser.DefineFun("geom_check_input_ready", &GeometryCheckInputReady);

  parser.DefineFun("geom_export_mask_label", &GeometryExportMaskLabel);
  parser.DefineFun("geom_export_boundary_label", &GeometryExportBoundaryLabel);
  parser.DefineFun("geom_export_keypoints_label", &GeometryExportKeypointsLabel);
  parser.DefineFun("geom_align_training_label", &GeometryAlignTrainingLabel);
  parser.DefineFun("geom_build_torch_label_packet", &GeometryBuildTorchLabelPacket);
  parser.DefineFun("geom_check_label_ready", &GeometryCheckLabelReady);

  parser.DefineFun("geom_attach_result_to_roi", &GeometryAttachResultToRoi);
  parser.DefineFun("geom_attach_result_mask", &GeometryAttachResultMask);
  parser.DefineFun("geom_attach_result_boundary", &GeometryAttachResultBoundary);
  parser.DefineFun("geom_attach_result_keypoints", &GeometryAttachResultKeypoints);
  parser.DefineFun("geom_publish_attach_packet", &GeometryPublishAttachPacket);
  parser.DefineFun("geom_check_attach_ready", &GeometryCheckAttachReady);
  parser.DefineFun("geom_check_replay_ready", &GeometryCheckReplayReady);
}

void RegisterScenarioBindings(mu::Parser &parser, const ParserBindingSpec &spec)
{
  double *org_double = 0;
  parser.DefineOrgClass("double", org_double);
  parser.UsingClass(true);

  bool register_image_probe = false;
  bool register_geometry_contract = false;
  for (size_t m = 0; m < spec.modules.size() &&
                     !(register_image_probe || register_geometry_contract);
       ++m)
  {
    for (size_t c = 0; c < spec.modules[m].classes.size(); ++c)
    {
      if (spec.modules[m].classes[c].parser_alias == "ImageProbe")
      {
        register_image_probe = true;
        break;
      }

      if (spec.modules[m].classes[c].parser_alias == "GeometryContract")
      {
        register_geometry_contract = true;
        break;
      }
    }
  }

  if (register_image_probe)
  {
    ImageProbeWrapper *probe = 0;
    parser.DefineClass("ImageProbe", probe);
    parser.DefineClassFun("ImageProbe", probe, "Load", &ImageProbeWrapper::Load);
    parser.DefineClassFun("ImageProbe", probe, "Detect", &ImageProbeWrapper::Detect);
    parser.DefineClassFun("ImageProbe", probe, "Score", &ImageProbeWrapper::Score);
  }

  if (register_geometry_contract)
  {
    RegisterGeometryContractBindings(parser);
  }
}

void RegisterCxcoreContractBridgeBindings(mu::Parser &parser)
{
  static double success = 1.0;
  static double runtime_ms = 0.0;
  static double true_value = 1.0;
  static double false_value = 0.0;
  static double contract_task_id_ok = 1.0;
  static double contract_result_object_ok = 1.0;
  static double contract_failure_mode_ok = 1.0;
  static double contract_summary_ok = 1.0;

  parser.DefineVar("success", &success);
  parser.DefineVar("runtime_ms", &runtime_ms);
  parser.DefineVar("true", &true_value);
  parser.DefineVar("false", &false_value);
  parser.DefineVar("contract_task_id_ok", &contract_task_id_ok);
  parser.DefineVar("contract_result_object_ok", &contract_result_object_ok);
  parser.DefineVar("contract_failure_mode_ok", &contract_failure_mode_ok);
  parser.DefineVar("contract_summary_ok", &contract_summary_ok);

  parser.DefineFun("check", &ContractCheck);
  parser.DefineFun("print", &ContractPrint);
  parser.DefineFun("emit", &ContractPrint);
  parser.DefineFun("learn_template_model", &ContractLearnTemplateModel);
  parser.DefineFun("measure_line", &ContractMeasureLine);
  parser.DefineFun("measure_circle", &ContractMeasureCircle);
  parser.DefineFun("match_template", &ContractMatchTemplate);
  parser.DefineFun("analyze_region_boundary", &ContractAnalyzeRegionBoundary);
}
}

ParserRuntimeFacade::ParserRuntimeFacade() = default;

ParserRuntimeFacade::~ParserRuntimeFacade() = default;

ParserRuntimeFacade::ParserRuntimeFacade(ParserRuntimeFacade &&) noexcept = default;

ParserRuntimeFacade &ParserRuntimeFacade::operator=(ParserRuntimeFacade &&) noexcept = default;

void ParserRuntimeFacade::Reset()
{
  binding_spec_ = ParserBindingSpec();
  target_ = ExecutionTarget();
  script_text_.clear();
  parser_.reset();
}

bool ParserRuntimeFacade::LoadBindingSpec(const ParserBindingSpec &spec)
{
  binding_spec_ = spec;
  return true;
}

bool ParserRuntimeFacade::LoadScript(const ExecutionTarget &target)
{
  if (target.script_text.empty())
    return false;

  target_ = target;
  script_text_ = target.script_text;
  return true;
}

bool ParserRuntimeFacade::Execute(ExecutionResult &result)
{
  result = ExecutionResult();

  if (script_text_.empty())
  {
    result.error_kind = "runtime_error";
    result.error_message = "script is empty";
    return false;
  }

  try
  {
    parser_.reset(new mu::Parser());
    RegisterScenarioBindings(*parser_, binding_spec_);
    if (target_.target_class == "cxcore_contract_script")
      RegisterCxcoreContractBridgeBindings(*parser_);
    parser_->SetExpr(script_text_);
    result.scalar_result = parser_->Eval();
    result.success = true;
    return true;
  }
  catch (const mu::Parser::exception_type &ex)
  {
    result.error_kind = "parser_exception";
    result.error_message = ex.GetMsg();
    result.parser_error_code = static_cast<int>(ex.GetCode());
    result.parser_error_pos = NormalizeParserPos(ex.GetPos());
    result.parser_error_token = ex.GetToken();
    result.parser_error_expr = ex.GetExpr();
    return false;
  }
  catch (const std::exception &ex)
  {
    result.error_kind = "runtime_error";
    result.error_message = ex.what();
    return false;
  }
  catch (...)
  {
    result.error_kind = "runtime_error";
    result.error_message = "unknown parser runtime error";
    return false;
  }
}

bool ParserRuntimeFacade::CollectRuntimeEvidence(ParserEvidenceBundle &bundle)
{
  bundle.task_id = target_.task_id.empty() ? "runtime_facade" : target_.task_id;
  bundle.trace_id = target_.trace_id;
  bundle.route_key = target_.route.route_key;
  bundle.route_lane = target_.route.lane_name;
  bundle.protocol_name = target_.module_call.protocol_name;

  ParserTraceEntry trace_entry;
  trace_entry.sequence = 1;
  trace_entry.trace_id = target_.trace_id;
  trace_entry.stage = "runtime_facade";
  trace_entry.action = "collect_evidence";
  trace_entry.status = "ok";
  trace_entry.detail = "runtime facade collected execution evidence";
  bundle.trace_entries.push_back(trace_entry);

  ParserLogEntry log_entry;
  log_entry.trace_id = target_.trace_id;
  log_entry.level = "info";
  log_entry.stage = "runtime_facade";
  log_entry.code = "runtime_evidence_ready";
  log_entry.message = "runtime facade collected execution evidence";
  bundle.log_entries.push_back(log_entry);

  EvidenceEvent execute_event;
  execute_event.level = eel_info;
  execute_event.stage = "parser_execute";
  execute_event.code = "parser_runtime_executed";
  execute_event.message = "runtime facade executed current script";
  bundle.events.push_back(execute_event);

  bundle.notes.push_back("runtime facade executed current script");
  if (!script_text_.empty())
    bundle.notes.push_back(script_text_);
  if (!binding_spec_.modules.empty())
  {
    bundle.notes.push_back("binding spec present");
    EvidenceEvent binding_event;
    binding_event.level = eel_info;
    binding_event.stage = "binding_build";
    binding_event.code = "binding_spec_present";
    binding_event.message = "binding spec is available during runtime execution";
    bundle.events.push_back(binding_event);
  }
  return true;
}

void *ParserRuntimeFacade::GetClassObject(const std::string &class_name,
                                         const std::string &object_name)
{
  if (!parser_)
    return 0;

  return parser_->GetClassObj(class_name, object_name);
}
}
