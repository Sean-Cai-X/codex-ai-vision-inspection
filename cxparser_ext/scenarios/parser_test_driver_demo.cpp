#include <iostream>

#include "../pipeline/parser_test_driver.h"

namespace
{
bool RunCase(cxparser_ext::ParserTestDriver &driver,
             const char *layer,
             const char *module,
             const char *case_name,
             const char *expected_result_object = 0,
             bool expect_cxscript_details = false,
             const char *expected_metrics = 0,
             const char *expected_summary_fragment = 0)
{
  cxparser_ext::ParserTestRequest request;
  request.layer = layer;
  request.module = module;
  request.case_name = case_name;
  request.mode = "build-run";
  request.report_on = true;
  request.debug_on = expect_cxscript_details;

  cxparser_ext::ParserTestRunResult result;
  if (!driver.Execute(request, result))
  {
    std::cerr << "[FAIL] " << module << "/" << layer << "/" << case_name
              << " summary=" << result.summary << "\n";
    return false;
  }

  if (expected_result_object && result.result_object != expected_result_object)
  {
    std::cerr << "[FAIL] " << module << "/" << layer << "/" << case_name
              << " result_object=" << result.result_object
              << " expected=" << expected_result_object << "\n";
    return false;
  }

  if (expected_metrics && result.metrics != expected_metrics)
  {
    std::cerr << "[FAIL] " << module << "/" << layer << "/" << case_name
              << " metrics=" << result.metrics
              << " expected=" << expected_metrics << "\n";
    return false;
  }

  if (expected_summary_fragment &&
      result.summary.find(expected_summary_fragment) == std::string::npos)
  {
    std::cerr << "[FAIL] " << module << "/" << layer << "/" << case_name
              << " summary=" << result.summary
              << " missing=" << expected_summary_fragment << "\n";
    return false;
  }

  if (expect_cxscript_details && result.failure_mode != "none")
  {
    std::cerr << "[FAIL] " << module << "/" << layer << "/" << case_name
              << " failure_mode=" << result.failure_mode
              << " expected=none\n";
    return false;
  }

  if (expect_cxscript_details)
  {
    bool saw_cxscript_summary = false;
    bool saw_cxscript_header = false;
    bool saw_cxscript_exec_summary = false;
    bool saw_cxscript_trace = false;
    bool saw_cxscript_step = false;
    bool saw_cxscript_exec = false;
    bool saw_cxscript_source = false;
    bool saw_cxscript_header_exec = false;
    bool saw_cxscript_header_source = false;
    bool saw_cxscript_check = false;
    bool saw_cxscript_print = false;
    bool saw_cxscript_breakpoint = false;
    bool saw_cxscript_checkpoint = false;
    bool saw_source_check = false;
    bool saw_source_print = false;
    bool saw_source_breakpoint = false;
    bool saw_source_checkpoint = false;
    for (size_t i = 0; i < result.details.size(); ++i)
    {
      if (result.details[i].find("[CXSCRIPT] ") == 0)
        saw_cxscript_summary = true;
      if (result.details[i].find("[CXSCRIPT_HEADER] ") == 0)
      {
        saw_cxscript_header = true;
        if (result.details[i].find("field=kind value=module") == std::string::npos &&
            result.details[i].find("field=layer value=feature") == std::string::npos &&
            result.details[i].find("field=module value=cxcore") == std::string::npos &&
            result.details[i].find("field=case_name value=") == std::string::npos &&
            result.details[i].find("field=mode value=build-run") == std::string::npos &&
            result.details[i].find("field=report value=on") == std::string::npos)
        {
          std::cerr << "[FAIL] " << module << "/" << layer << "/" << case_name
                    << " unexpected cxscript header detail: "
                    << result.details[i] << "\n";
          return false;
        }
      }
      if (result.details[i].find("[CXSCRIPT_SUMMARY] ") == 0)
      {
        saw_cxscript_exec_summary = true;
        if (result.details[i].find("entry_step_id=0") != std::string::npos ||
            result.details[i].find("check_step_id=0") != std::string::npos ||
            result.details[i].find("max_sequence=0") != std::string::npos ||
            result.details[i].find("max_block_depth=0") != std::string::npos ||
            result.details[i].find("header_step_count=0") != std::string::npos ||
            result.details[i].find("call_step_count=0") != std::string::npos ||
            result.details[i].find("check_step_count=0") != std::string::npos ||
            result.details[i].find("print_step_count=0") != std::string::npos ||
            result.details[i].find("breakpoint_step_count=0") != std::string::npos ||
            result.details[i].find("checkpoint_step_count=0") != std::string::npos)
        {
          std::cerr << "[FAIL] " << module << "/" << layer << "/" << case_name
                    << " invalid cxscript execution summary: "
                    << result.details[i] << "\n";
          return false;
        }
      }
      if (result.details[i].find("[CXSCRIPT_TRACE] ") == 0)
      {
        saw_cxscript_trace = true;
        if (result.details[i].find("last_step_id=0") != std::string::npos ||
            result.details[i].find("last_sequence=0") != std::string::npos ||
            result.details[i].find("last_line=0") != std::string::npos ||
            result.details[i].find("failure_phase=") == std::string::npos)
        {
          std::cerr << "[FAIL] " << module << "/" << layer << "/" << case_name
                    << " invalid cxscript trace summary: "
                    << result.details[i] << "\n";
          return false;
        }
      }
      if (result.details[i].find("[CXSCRIPT_STEP] ") == 0)
        saw_cxscript_step = true;
      if (result.details[i].find("[CXSCRIPT_EXEC] ") == 0)
      {
        saw_cxscript_exec = true;
        if (result.details[i].find("kind=header_metadata ") != std::string::npos &&
            result.details[i].find("tag=header_metadata") != std::string::npos)
          saw_cxscript_header_exec = true;
        if (result.details[i].find("kind=check ") != std::string::npos &&
            result.details[i].find("tag=check") != std::string::npos)
          saw_cxscript_check = true;
        if (result.details[i].find("kind=print ") != std::string::npos &&
            result.details[i].find("tag=print") != std::string::npos)
          saw_cxscript_print = true;
        if (result.details[i].find("kind=breakpoint ") != std::string::npos &&
            result.details[i].find("tag=breakpoint") != std::string::npos)
          saw_cxscript_breakpoint = true;
        if (result.details[i].find("kind=checkpoint ") != std::string::npos &&
            result.details[i].find("tag=checkpoint") != std::string::npos)
          saw_cxscript_checkpoint = true;
      }
      if (result.details[i].find("[CXSCRIPT_SOURCE] ") == 0)
      {
        saw_cxscript_source = true;
        if (result.details[i].find("kind=header_metadata ") != std::string::npos &&
            result.details[i].find("tag=header_metadata") != std::string::npos &&
            result.details[i].find("seq=") != std::string::npos)
          saw_cxscript_header_source = true;
        if (result.details[i].find("kind=check ") != std::string::npos &&
            result.details[i].find("tag=check") != std::string::npos &&
            result.details[i].find("seq=") != std::string::npos)
          saw_source_check = true;
        if (result.details[i].find("kind=print ") != std::string::npos &&
            result.details[i].find("tag=print") != std::string::npos &&
            result.details[i].find("seq=") != std::string::npos)
          saw_source_print = true;
        if (result.details[i].find("kind=breakpoint ") != std::string::npos &&
            result.details[i].find("tag=breakpoint") != std::string::npos &&
            result.details[i].find("seq=") != std::string::npos)
          saw_source_breakpoint = true;
        if (result.details[i].find("kind=checkpoint ") != std::string::npos &&
            result.details[i].find("tag=checkpoint") != std::string::npos &&
            result.details[i].find("seq=") != std::string::npos)
          saw_source_checkpoint = true;
      }
    }

    if (!saw_cxscript_summary ||
        !saw_cxscript_header ||
        !saw_cxscript_exec_summary ||
        !saw_cxscript_trace ||
        !saw_cxscript_step ||
        !saw_cxscript_exec ||
        !saw_cxscript_source ||
        !saw_cxscript_header_exec ||
        !saw_cxscript_header_source ||
        !saw_cxscript_check ||
        !saw_cxscript_print ||
        !saw_cxscript_breakpoint ||
        !saw_cxscript_checkpoint ||
        !saw_source_check ||
        !saw_source_print ||
        !saw_source_breakpoint ||
        !saw_source_checkpoint)
    {
      std::cerr << "[FAIL] " << module << "/" << layer << "/" << case_name
                << " missing cxscript preflight execution details\n";
      for (size_t i = 0; i < result.details.size(); ++i)
        std::cerr << "  detail: " << result.details[i] << "\n";
      return false;
    }
  }

  for (size_t i = 0; i < result.details.size(); ++i)
    std::cout << result.details[i] << "\n";
  return true;
}
}

bool RunMlpackFirstStageRouterContract(cxparser_ext::ParserTestDriver &driver)
{
  if (!RunCase(driver, "feature", "mlpack", "model_entry_min"))
    return false;
  if (!RunCase(driver, "feature", "mlpack", "minimal_model"))
    return false;
  if (!RunCase(driver, "train", "mlpack", "min_train"))
    return false;
  if (!RunCase(driver, "train", "mlpack", "minimal_train"))
    return false;
  if (!RunCase(driver, "infer", "mlpack", "min_infer"))
    return false;
  if (!RunCase(driver, "infer", "mlpack", "minimal_infer"))
    return false;
  if (!RunCase(driver, "train", "mlpack", "baseline_logreg_flow_min"))
    return false;
  if (!RunCase(driver, "infer", "mlpack", "baseline_logreg_flow_min"))
    return false;
  if (!RunCase(driver, "train", "mlpack", "baseline_knn_flow_min"))
    return false;
  if (!RunCase(driver, "infer", "mlpack", "baseline_knn_flow_min"))
    return false;
  if (!RunCase(driver, "score", "mlpack", "baseline_classification_flow_min"))
    return false;
  return true;
}

int main(int argc, char **argv)
{
  cxparser_ext::ParserTestDriver driver;

  if (argc > 1 && std::string(argv[1]) == "mlpack_first_stage_router_contract")
  {
    if (!RunMlpackFirstStageRouterContract(driver))
      return 1;
    std::cout << "[PASS] parser_test_driver_demo completed mlpack first-stage router contract\n";
    return 0;
  }

  if (!RunCase(driver, "smoke", "cxcore", "minimal_binding"))
    return 1;
  if (!RunCase(driver, "feature", "cxcore", "image_operator_min"))
    return 1;
  if (!RunCase(driver,
               "feature",
               "cxcore",
               "line_measurement_balanced",
               "LineMeasurementOutput",
               true,
               "fit_error_avg,fit_error_max,line_angle,line_offset,subpixel_adjust_avg,chain_switch_count,neighbor_inconsistency_count",
               "metrics=fit_error_avg,fit_error_max,line_angle,line_offset,subpixel_adjust_avg,chain_switch_count,neighbor_inconsistency_count"))
    return 1;
  if (!RunCase(driver,
               "feature",
               "cxcore",
               "circle_measurement_balanced",
               "CircleMeasurementOutput",
               true,
               "center_x,center_y,radius,avg_distance,sample_points",
               "metrics=center_x,center_y,radius,avg_distance,sample_points"))
    return 1;
  if (!RunCase(driver,
               "feature",
               "cxcore",
               "template_feature_match",
               "MatchOutput",
               true,
               "candidate_count,top_score,max_score,match_center",
               "task validated"))
    return 1;
  if (!RunCase(driver, "scenario", "cxcore", "image_analysis_baseline"))
    return 1;
  if (!RunMlpackFirstStageRouterContract(driver))
    return 1;
  if (!RunCase(driver, "smoke", "torch", "torch_runner_min"))
    return 1;
  if (!RunCase(driver, "train", "torch", "yolo_min_train"))
    return 1;
  if (!RunCase(driver, "infer", "torch", "yolo_min_infer"))
    return 1;
  if (!RunCase(driver, "smoke", "rag", "rag_query_min"))
    return 1;
  if (!RunCase(driver, "scenario", "rag", "rag_script_assist"))
    return 1;
  if (!RunCase(driver, "feature", "ensmallen_layer", "geometry_fit_tuning"))
    return 1;

  std::cout << "[PASS] parser_test_driver_demo completed first-stage unified routing\n";
  return 0;
}
