#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "../meta/parser_pseudocode_types.h"
#include "../pipeline/parser_binding_builder.h"
#include "../pipeline/parser_task_types.h"
#include "../pipeline/parser_unified_entry.h"

namespace
{
cxparser_ext::PseudoClassSpec BuildPseudoClass()
{
  cxparser_ext::PseudoClassSpec pseudo_class;
  pseudo_class.module_name = "testdll_image_probe";
  pseudo_class.class_name = "ImageProbeWrapper";
  pseudo_class.parser_alias = "ImageProbe";

  cxparser_ext::PseudoMethodSpec load_method;
  load_method.name = "Load";
  load_method.param_types.push_back("const char*");
  load_method.return_type = "void";
  pseudo_class.methods.push_back(load_method);

  cxparser_ext::PseudoMethodSpec detect_method;
  detect_method.name = "Detect";
  detect_method.param_types.push_back("double");
  detect_method.return_type = "void";
  pseudo_class.methods.push_back(detect_method);

  cxparser_ext::PseudoMethodSpec score_method;
  score_method.name = "Score";
  score_method.return_type = "double";
  pseudo_class.methods.push_back(score_method);

  return pseudo_class;
}

bool NearlyEqual(double lhs, double rhs, double eps = 1e-9)
{
  return std::fabs(lhs - rhs) <= eps;
}

cxparser_ext::CxTaskEnvelope MakeParserEvalEnvelope()
{
  cxparser_ext::CxTaskEnvelope envelope;
  envelope.task_id = "parser_eval_task";
  envelope.task_name = "parser_eval_task";
  envelope.trace_id = "trace.parser_eval_task";
  envelope.task_type = cxparser_ext::task_constants::TaskTypeCoreTest();
  envelope.task_subtype = cxparser_ext::task_constants::TaskSubtypeParserEval();
  envelope.route = cxparser_ext::task_constants::RouteDefault();
  envelope.execution_mode = cxparser_ext::task_constants::ExecutionModeMainline();
  envelope.caller_module = "cxparser_ext_envelope_smoke";
  envelope.callee_module = "cxparser_core";
  envelope.target_class = "native_expr";
  envelope.target_method = "eval";
  envelope.script_text = "1+2";
  return envelope;
}

cxparser_ext::CxTaskEnvelope MakeImageEnvelope(const cxparser_ext::PseudoClassSpec &pseudo_class)
{
  cxparser_ext::CxTaskEnvelope envelope;
  envelope.task_id = "image_process_task";
  envelope.task_name = "image_process_task";
  envelope.trace_id = "trace.image_process_task";
  envelope.task_type = cxparser_ext::task_constants::TaskTypeCoreTest();
  envelope.task_subtype = cxparser_ext::task_constants::TaskSubtypeImageProcess();
  envelope.route = cxparser_ext::task_constants::RouteDefault();
  envelope.execution_mode = cxparser_ext::task_constants::ExecutionModeMainline();
  envelope.caller_module = "cxparser_ext_envelope_smoke";
  envelope.callee_module = pseudo_class.module_name;
  envelope.target_class = pseudo_class.parser_alias;
  envelope.target_method = "Score";
  envelope.script_text = "ImageProbe probe;probe.Load(\"image.png\");probe.Detect(0.8);probe.Score();";
  return envelope;
}

cxparser_ext::CxTaskEnvelope MakeVideoEnvelope(const cxparser_ext::PseudoClassSpec &pseudo_class)
{
  cxparser_ext::CxTaskEnvelope envelope;
  envelope.task_id = "video_frame_task";
  envelope.task_name = "video_frame_task";
  envelope.trace_id = "trace.video_frame_task";
  envelope.task_type = cxparser_ext::task_constants::TaskTypeCoreTest();
  envelope.task_subtype = cxparser_ext::task_constants::TaskSubtypeVideoFrame();
  envelope.route = cxparser_ext::task_constants::RouteRealtime();
  envelope.execution_mode = cxparser_ext::task_constants::ExecutionModeMainline();
  envelope.caller_module = "cxparser_ext_envelope_smoke";
  envelope.callee_module = pseudo_class.module_name;
  envelope.target_class = pseudo_class.parser_alias;
  envelope.target_method = "Score";
  envelope.script_text = "ImageProbe probe;probe.Load(\"frame.jpg\");probe.Detect(0.6);probe.Score();";
  return envelope;
}

cxparser_ext::CxTaskEnvelope MakeYoloEnvelope(const cxparser_ext::PseudoClassSpec &pseudo_class)
{
  cxparser_ext::CxTaskEnvelope envelope;
  envelope.task_id = "yolo_batch_task";
  envelope.task_name = "yolo_batch_task";
  envelope.trace_id = "trace.yolo_batch_task";
  envelope.task_type = cxparser_ext::task_constants::TaskTypeCoreTest();
  envelope.task_subtype = cxparser_ext::task_constants::TaskSubtypeYoloBatch();
  envelope.route = cxparser_ext::task_constants::RouteBatch();
  envelope.execution_mode = cxparser_ext::task_constants::ExecutionModeMainline();
  envelope.caller_module = "cxparser_ext_envelope_smoke";
  envelope.callee_module = pseudo_class.module_name;
  envelope.target_class = pseudo_class.parser_alias;
  envelope.target_method = "Score";
  envelope.script_text = "ImageProbe probe;probe.Load(\"batch.png\");probe.Detect(0.7);probe.Score();";
  return envelope;
}

const cxparser_ext::TaskChainRecord *FindChainRecord(const std::vector<cxparser_ext::TaskChainRecord> &records,
                                                     const char *task_id)
{
  for (size_t i = 0; i < records.size(); ++i)
  {
    if (records[i].task_id == task_id)
      return &records[i];
  }
  return 0;
}

bool VerifyTask(const cxparser_ext::ParserUnifiedEntry &entry,
                const char *task_id,
                double expected_scalar,
                const char *expected_subtype,
                const char *expected_route,
                const char *expected_callee)
{
  const cxparser_ext::ParserTaskUnit *task = entry.FindTask(task_id);
  const cxparser_ext::ParserEvidenceBundle *evidence = entry.FindTaskEvidence(task_id);
  if (!task || !evidence)
  {
    std::cerr << "[FAIL] task/evidence lookup failed for " << task_id << "\n";
    return false;
  }

  if (task->status != cxparser_ext::pts_validated)
  {
    std::cerr << "[FAIL] task did not reach validated state: " << task_id << "\n";
    return false;
  }

  if (!NearlyEqual(task->result.scalar_result, expected_scalar))
  {
    std::cerr << "[FAIL] scalar result mismatch for " << task_id
              << ": " << task->result.scalar_result << "\n";
    return false;
  }

  if (evidence->task_type != cxparser_ext::task_constants::TaskTypeCoreTest())
  {
    std::cerr << "[FAIL] task_type mismatch for " << task_id << ": "
              << evidence->task_type << "\n";
    return false;
  }

  if (evidence->task_subtype != expected_subtype)
  {
    std::cerr << "[FAIL] task_subtype mismatch for " << task_id << ": "
              << evidence->task_subtype << "\n";
    return false;
  }

  if (evidence->execution_mode != cxparser_ext::task_constants::ExecutionModeMainline())
  {
    std::cerr << "[FAIL] execution_mode mismatch for " << task_id << ": "
              << evidence->execution_mode << "\n";
    return false;
  }

  if (evidence->route_lane != expected_route)
  {
    std::cerr << "[FAIL] route_lane mismatch for " << task_id << ": "
              << evidence->route_lane << "\n";
    return false;
  }

  if (evidence->module_chain.size() != 2 ||
      evidence->module_chain[0] != "cxparser_ext_envelope_smoke" ||
      evidence->module_chain[1] != expected_callee)
  {
    std::cerr << "[FAIL] module chain mismatch for " << task_id << "\n";
    return false;
  }

  return true;
}
}

int main()
{
  const cxparser_ext::PseudoClassSpec pseudo_class = BuildPseudoClass();
  cxparser_ext::ParserBindingSpec binding_spec;
  if (!cxparser_ext::BuildBindingSpec(pseudo_class, binding_spec))
  {
    std::cerr << "[FAIL] BuildBindingSpec failed for task envelope smoke\n";
    return 1;
  }

  cxparser_ext::ParserUnifiedEntry entry;
  entry.SetBindingSpec(binding_spec);

  const cxparser_ext::CxTaskEnvelope parser_eval = MakeParserEvalEnvelope();
  const cxparser_ext::CxTaskEnvelope image_task = MakeImageEnvelope(pseudo_class);
  const cxparser_ext::CxTaskEnvelope video_task = MakeVideoEnvelope(pseudo_class);
  const cxparser_ext::CxTaskEnvelope yolo_task = MakeYoloEnvelope(pseudo_class);

  if (!entry.SubmitEnvelope(parser_eval) ||
      !entry.SubmitEnvelope(image_task) ||
      !entry.SubmitEnvelope(video_task) ||
      !entry.SubmitEnvelope(yolo_task))
  {
    std::cerr << "[FAIL] SubmitEnvelope failed\n";
    return 1;
  }

  if (!entry.ExecuteMainThreadCycle())
  {
    std::cerr << "[FAIL] ExecuteMainThreadCycle failed for envelope smoke\n";
    return 1;
  }

  const cxparser_ext::ParserMainThreadTick &tick = entry.GetLastTick();
  if (!tick.success || tick.accepted_task_count != 4 || tick.executed_task_count != 4)
  {
    std::cerr << "[FAIL] main thread tick mismatch accepted="
              << tick.accepted_task_count << " executed="
              << tick.executed_task_count << "\n";
    return 1;
  }

  if (!VerifyTask(entry,
                  "parser_eval_task",
                  3.0,
                  cxparser_ext::task_constants::TaskSubtypeParserEval(),
                  cxparser_ext::task_constants::RouteDefault(),
                  "cxparser_core") ||
      !VerifyTask(entry,
                  "image_process_task",
                  8.0,
                  cxparser_ext::task_constants::TaskSubtypeImageProcess(),
                  cxparser_ext::task_constants::RouteDefault(),
                  pseudo_class.module_name.c_str()) ||
      !VerifyTask(entry,
                  "video_frame_task",
                  6.0,
                  cxparser_ext::task_constants::TaskSubtypeVideoFrame(),
                  cxparser_ext::task_constants::RouteRealtime(),
                  pseudo_class.module_name.c_str()) ||
      !VerifyTask(entry,
                  "yolo_batch_task",
                  7.0,
                  cxparser_ext::task_constants::TaskSubtypeYoloBatch(),
                  cxparser_ext::task_constants::RouteBatch(),
                  pseudo_class.module_name.c_str()))
  {
    return 1;
  }

  const std::vector<cxparser_ext::TaskChainRecord> &chain_records = entry.GetChainRecords();
  if (chain_records.size() != 4)
  {
    std::cerr << "[FAIL] chain record count mismatch: " << chain_records.size() << "\n";
    return 1;
  }

  const cxparser_ext::TaskChainRecord *parser_chain = FindChainRecord(chain_records, "parser_eval_task");
  const cxparser_ext::TaskChainRecord *image_chain = FindChainRecord(chain_records, "image_process_task");
  const cxparser_ext::TaskChainRecord *video_chain = FindChainRecord(chain_records, "video_frame_task");
  const cxparser_ext::TaskChainRecord *yolo_chain = FindChainRecord(chain_records, "yolo_batch_task");
  if (!parser_chain || !image_chain || !video_chain || !yolo_chain)
  {
    std::cerr << "[FAIL] missing one or more chain records\n";
    return 1;
  }

  if (parser_chain->route != cxparser_ext::task_constants::RouteDefault() ||
      video_chain->route != cxparser_ext::task_constants::RouteRealtime() ||
      yolo_chain->route != cxparser_ext::task_constants::RouteBatch())
  {
    std::cerr << "[FAIL] chain route mapping mismatch\n";
    return 1;
  }

  if (!entry.ReplayTask("video_frame_task"))
  {
    std::cerr << "[FAIL] ReplayTask failed for video_frame_task\n";
    return 1;
  }

  const cxparser_ext::ParserTaskUnit *replayed_task = entry.FindTask("video_frame_task");
  if (!replayed_task ||
      replayed_task->status != cxparser_ext::pts_validated ||
      !NearlyEqual(replayed_task->result.scalar_result, 6.0))
  {
    std::cerr << "[FAIL] replay result mismatch for video_frame_task\n";
    return 1;
  }

  const std::vector<cxparser_ext::TaskChainRecord> &replay_chain_records = entry.GetChainRecords();
  if (replay_chain_records.size() != 5)
  {
    std::cerr << "[FAIL] replay chain record count mismatch: " << replay_chain_records.size() << "\n";
    return 1;
  }

  const cxparser_ext::TaskChainRecord &replay_record = replay_chain_records.back();
  if (replay_record.task_id != "video_frame_task" ||
      replay_record.execution_mode != cxparser_ext::task_constants::ExecutionModeReplay() ||
      replay_record.replay_source_task_id != "video_frame_task")
  {
    std::cerr << "[FAIL] replay chain metadata mismatch\n";
    return 1;
  }

  const cxparser_ext::ParserMainThreadTick &replay_tick = entry.GetLastTick();
  const cxparser_ext::ParserReplaySummary replay_summary = entry.GetLastReplaySummary();
  if (replay_tick.replay_count != 1 ||
      replay_tick.last_replay_task_id != "video_frame_task" ||
      replay_tick.last_replay_source_task_id != "video_frame_task" ||
      replay_tick.last_replay_modules.size() != 2 ||
      replay_tick.last_replay_stages.size() < 4)
  {
    std::cerr << "[FAIL] replay tick metadata mismatch\n";
    return 1;
  }

  if (replay_summary.replay_count != 1 ||
      replay_summary.replay_task_id != "video_frame_task" ||
      replay_summary.replay_source_task_id != "video_frame_task" ||
      replay_summary.replay_modules.size() != 2 ||
      replay_summary.replay_stages.size() != replay_tick.last_replay_stages.size())
  {
    std::cerr << "[FAIL] replay summary mismatch\n";
    return 1;
  }

  std::cout << "[PASS] task_envelope accepted=" << tick.accepted_task_count
            << " executed=" << tick.executed_task_count
            << " chains=" << replay_chain_records.size()
            << " replay=video_frame_task"
            << " replay_count=" << replay_tick.replay_count
            << " replay_modules=" << replay_tick.last_replay_modules[0]
            << "->" << replay_tick.last_replay_modules[1]
            << " replay_stage_count=" << replay_tick.last_replay_stages.size()
            << "\n";
  return 0;
}
