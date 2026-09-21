#ifndef CXPARSER_EXT_PARSER_CXSCRIPT_CALL_ALIAS_H
#define CXPARSER_EXT_PARSER_CXSCRIPT_CALL_ALIAS_H

#include <string>

namespace cxparser_ext
{
inline std::string ResolveCxScriptCallAlias(const std::string &module,
                                            const std::string &call_name)
{
  if (module == "cxcore")
  {
    if (call_name == "minimal_binding")
      return "minimal_binding";
    if (call_name == "detect" || call_name == "image_operator_min")
      return "image_operator_min";
    if (call_name == "image_analysis" || call_name == "analyze_image")
      return "image_analysis_baseline";
  }

  if (module == "torch")
  {
    if (call_name == "torch_runner_min")
      return "torch_runner_min";
    if (call_name == "train_min" || call_name == "yolo_train")
      return "yolo_min_train";
    if (call_name == "infer_min" || call_name == "yolo_infer")
      return "yolo_min_infer";
  }

  if (module == "mlpack")
  {
    if (call_name == "feature_min" || call_name == "model_entry")
      return "model_entry_min";
    if (call_name == "train_min")
      return "min_train";
    if (call_name == "infer_min")
      return "min_infer";
  }

  if (module == "rag")
  {
    if (call_name == "query_min" || call_name == "rag_query")
      return "rag_query_min";
    if (call_name == "script_assist" || call_name == "rag_assist")
      return "rag_script_assist";
  }

  return std::string();
}
}

#endif
