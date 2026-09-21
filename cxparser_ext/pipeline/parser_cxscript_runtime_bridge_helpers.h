#ifndef CXPARSER_EXT_PARSER_CXSCRIPT_RUNTIME_BRIDGE_HELPERS_H
#define CXPARSER_EXT_PARSER_CXSCRIPT_RUNTIME_BRIDGE_HELPERS_H

#include <cstdlib>
#include <cstddef>
#include <fstream>
#include <map>
#include <sys/stat.h>
#include <vector>
#include <sstream>
#include <string>

#include "parser_cxscript_types.h"

namespace cxparser_ext
{
namespace detail
{
inline std::string TrimBridgeHelper(const std::string &text)
{
  size_t begin = 0;
  while (begin < text.size() &&
         (text[begin] == ' ' || text[begin] == '\t' ||
          text[begin] == '\r' || text[begin] == '\n'))
  {
    ++begin;
  }

  size_t end = text.size();
  while (end > begin &&
         (text[end - 1] == ' ' || text[end - 1] == '\t' ||
          text[end - 1] == '\r' || text[end - 1] == '\n'))
  {
    --end;
  }

  return text.substr(begin, end - begin);
}

inline std::string StripTrailingSemicolonBridgeHelper(const std::string &text)
{
  const std::string trimmed = TrimBridgeHelper(text);
  if (!trimmed.empty() && trimmed[trimmed.size() - 1] == ';')
    return TrimBridgeHelper(trimmed.substr(0, trimmed.size() - 1));
  return trimmed;
}

inline std::string StripQuotesBridgeHelper(const std::string &value)
{
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
    return value.substr(1, value.size() - 2);
  return value;
}

inline bool TryExtractScriptAssignmentBridgeHelper(const std::string &script_text,
                                                   const std::string &key,
                                                   std::string &value)
{
  value.clear();

  std::istringstream input(script_text);
  std::string line;
  while (std::getline(input, line))
  {
    const std::string trimmed = TrimBridgeHelper(line);
    const size_t split = trimmed.find('=');
    if (split == std::string::npos)
      continue;

    if (TrimBridgeHelper(trimmed.substr(0, split)) != key)
      continue;

    value = StripQuotesBridgeHelper(
      StripTrailingSemicolonBridgeHelper(
        TrimBridgeHelper(trimmed.substr(split + 1))));
    return !value.empty();
  }

  return false;
}

inline std::vector<std::pair<std::string, std::string>> CollectScriptAssignmentsBridgeHelper(
  const std::string &script_text)
{
  std::vector<std::pair<std::string, std::string>> items;

  std::istringstream input(script_text);
  std::string line;
  while (std::getline(input, line))
  {
    const std::string trimmed = TrimBridgeHelper(line);
    const size_t split = trimmed.find('=');
    if (split == std::string::npos)
      continue;

    const std::string key = TrimBridgeHelper(trimmed.substr(0, split));
    if (key.empty())
      continue;

    const std::string value = StripQuotesBridgeHelper(
      StripTrailingSemicolonBridgeHelper(
        TrimBridgeHelper(trimmed.substr(split + 1))));
    if (value.empty())
      continue;

    items.push_back(std::make_pair(key, value));
  }

  return items;
}

inline std::string JoinExecutionProfileItemsBridgeHelper(
  const std::vector<std::string> &items)
{
  std::string joined;
  for (size_t i = 0; i < items.size(); ++i)
  {
    if (items[i].empty())
      continue;
    if (!joined.empty())
      joined += ";";
    joined += items[i];
  }
  return joined;
}

inline bool LooksLikeDirectoryBridgeHelper(const std::string &path_text)
{
  if (path_text.empty())
    return false;

  const char tail = path_text[path_text.size() - 1];
  if (tail == '/' || tail == '\\')
    return true;

  struct _stat path_stat;
  if (_stat(path_text.c_str(), &path_stat) != 0)
    return false;
  return (path_stat.st_mode & _S_IFDIR) != 0;
}

inline std::string JoinPathBridgeHelper(const std::string &base,
                                        const std::string &leaf)
{
  if (base.empty())
    return leaf;
  if (leaf.empty())
    return base;

  const char tail = base[base.size() - 1];
  if (tail == '/' || tail == '\\')
    return base + leaf;
  return base + "\\" + leaf;
}

inline std::string NormalizeFilesystemPathBridgeHelper(const std::string &path_text)
{
  if (path_text.empty())
    return std::string();

  const bool looks_like_path =
    path_text.find('\\') != std::string::npos ||
    path_text.find('/') != std::string::npos ||
    path_text.find(':') != std::string::npos;
  if (!looks_like_path)
    return path_text;

  char normalized[_MAX_PATH] = {0};
  if (_fullpath(normalized, path_text.c_str(), _MAX_PATH) != NULL)
    return std::string(normalized);
  return path_text;
}

inline std::string FilenameOnlyBridgeHelper(const std::string &path_text)
{
  if (path_text.empty())
    return std::string();

  const size_t slash = path_text.find_last_of("/\\");
  if (slash == std::string::npos)
    return path_text;
  return path_text.substr(slash + 1);
}

inline std::string ResolveEnvPathOrDefaultBridgeHelper(const char *env_name,
                                                       const char *fallback_name)
{
  const char *env_value = std::getenv(env_name);
  if (env_value != NULL && env_value[0] != '\0')
  {
    const std::string resolved = env_value;
    if (LooksLikeDirectoryBridgeHelper(resolved))
      return NormalizeFilesystemPathBridgeHelper(
        JoinPathBridgeHelper(resolved, fallback_name));
    return NormalizeFilesystemPathBridgeHelper(resolved);
  }

  return std::string(fallback_name);
}

inline std::string ResolveOptionalEnvValueBridgeHelper(const char *env_name)
{
  const char *env_value = std::getenv(env_name);
  if (env_value != NULL && env_value[0] != '\0')
    return NormalizeFilesystemPathBridgeHelper(std::string(env_value));
  return std::string();
}

inline bool PathExistsBridgeHelper(const std::string &path_text);

inline std::string WorkspaceRootBridgeHelper()
{
#ifdef CXPARSER_WORKSPACE_ROOT
  return std::string(CXPARSER_WORKSPACE_ROOT);
#else
  return std::string();
#endif
}

inline std::string ResolveWorkspaceRelativeBridgeHelper(const std::string &relative_path)
{
  const std::string workspace_root = WorkspaceRootBridgeHelper();
  if (workspace_root.empty() || relative_path.empty())
    return std::string();
  return NormalizeFilesystemPathBridgeHelper(
    JoinPathBridgeHelper(workspace_root, relative_path));
}

inline std::string ResolveEnvWorkspaceOrDefaultBridgeHelper(const char *env_name,
                                                            const char *workspace_relative,
                                                            const char *fallback_name)
{
  const std::string env_or_default =
    ResolveEnvPathOrDefaultBridgeHelper(env_name, fallback_name);
  if (env_or_default != std::string(fallback_name))
    return env_or_default;

  const std::string workspace_candidate =
    ResolveWorkspaceRelativeBridgeHelper(workspace_relative == NULL
                                           ? std::string()
                                           : std::string(workspace_relative));
  if (!workspace_candidate.empty() && PathExistsBridgeHelper(workspace_candidate))
    return workspace_candidate;

  return env_or_default;
}

inline bool PathExistsBridgeHelper(const std::string &path_text)
{
  if (path_text.empty())
    return false;

  struct _stat path_stat;
  return _stat(path_text.c_str(), &path_stat) == 0;
}

inline std::map<std::string, std::string> ReadKeyValueFileBridgeHelper(
  const std::string &path_text)
{
  std::map<std::string, std::string> values;
  if (!PathExistsBridgeHelper(path_text))
    return values;

  std::ifstream input(path_text.c_str(), std::ios::in);
  if (!input.is_open())
    return values;

  std::string line;
  while (std::getline(input, line))
  {
    const std::string trimmed = TrimBridgeHelper(line);
    const size_t split = trimmed.find('=');
    if (split == std::string::npos)
      continue;

    const std::string key = TrimBridgeHelper(trimmed.substr(0, split));
    const std::string value = TrimBridgeHelper(trimmed.substr(split + 1));
    if (!key.empty())
      values[key] = value;
  }

  return values;
}

inline std::string JoinPathSummaryBridgeHelper(
  const std::vector<std::pair<std::string, std::string>> &items,
  bool filename_only)
{
  std::string summary;
  for (size_t i = 0; i < items.size(); ++i)
  {
    const std::string value =
      filename_only
        ? FilenameOnlyBridgeHelper(items[i].second)
        : items[i].second;
    if (value.empty())
      continue;

    if (!summary.empty())
      summary += ";";
    summary += items[i].first + "=" + value;
  }
  return summary;
}
}

struct TorchPreparedDatasetBridge
{
  std::string dataset_profile;
  std::string prepared_root;
  std::string input_task;
  std::string input_profile;
  std::string required_input_contract;
  std::string required_label_contract;
  std::string template_root;
  std::string pairs_ref;
  std::string attach_back_result;
};

struct TorchExecutionProfileBridge
{
  std::string requested_device;
  std::string param_summary;
  std::string train_param_summary;
  std::string infer_param_summary;
  std::string consumed_weight_files;
  std::string consumed_weight_paths;
  std::string manifest_image_ref;
  std::string manifest_template_image_ref;
  std::string manifest_test_image_ref;
  std::string manifest_output_ref;
  std::string input_image_path;
  std::string attach_back_output_path;
  std::string attach_back_overlay_status;
  std::string attach_back_top1_class;
  std::string attach_back_confidence;
};

inline TorchPreparedDatasetBridge ResolveTorchPreparedDatasetBridge(
  const CxScriptExecutionContext &context,
  const std::string &script_text)
{
  TorchPreparedDatasetBridge bridge;
  detail::TryExtractScriptAssignmentBridgeHelper(script_text, "dataset_profile",
                                                 bridge.dataset_profile);
  detail::TryExtractScriptAssignmentBridgeHelper(script_text, "prepared_root",
                                                 bridge.prepared_root);
  detail::TryExtractScriptAssignmentBridgeHelper(script_text, "input_task",
                                                 bridge.input_task);
  detail::TryExtractScriptAssignmentBridgeHelper(script_text, "input_profile",
                                                 bridge.input_profile);
  detail::TryExtractScriptAssignmentBridgeHelper(script_text, "required_input_contract",
                                                 bridge.required_input_contract);
  detail::TryExtractScriptAssignmentBridgeHelper(script_text, "required_label_contract",
                                                 bridge.required_label_contract);
  detail::TryExtractScriptAssignmentBridgeHelper(script_text, "attach_back_result",
                                                 bridge.attach_back_result);

  if (bridge.prepared_root.find("ELPV-ImageFolder") != std::string::npos)
  {
    if (bridge.dataset_profile.empty())
      bridge.dataset_profile = "classification_imagefolder";
    if (bridge.required_input_contract.empty())
      bridge.required_input_contract = "aligned_patch_baseline_feature_prepare";
    if (bridge.required_label_contract.empty())
      bridge.required_label_contract = "aligned_patch_baseline_class_infer";
  }
  else if (bridge.prepared_root.find("DeepPCB-YOLO") != std::string::npos)
  {
    if (bridge.dataset_profile.empty())
      bridge.dataset_profile = "detection_with_template_pair";
  }
  else if (bridge.prepared_root.find("PCBA-sample-YOLO") != std::string::npos)
  {
    if (bridge.dataset_profile.empty())
      bridge.dataset_profile = "detection_yolo_pair";
  }

  if (context.case_name == "torch.mobilevit.session.feature" ||
      context.case_name == "torch.mobilevit.unified.infer")
  {
    if (bridge.dataset_profile.empty())
      bridge.dataset_profile = "two_stage_detection_roi_reclass";
    if (bridge.prepared_root.empty())
      bridge.prepared_root = "prepared/PCBA-sample-YOLO";
    if (bridge.required_input_contract.empty())
      bridge.required_input_contract = "roi_patch_batch_for_reclass";
    if (bridge.required_label_contract.empty())
      bridge.required_label_contract = "roi_patch_batch_plus_roi_class_label";
  }
  else if (context.case_name == "torch.deeplab.contract.feature" ||
           context.case_name == "torch.deeplab.unified.infer")
  {
    if (bridge.dataset_profile.empty())
      bridge.dataset_profile = "detection_with_template_pair";
    if (bridge.prepared_root.empty())
      bridge.prepared_root = "prepared/DeepPCB-YOLO";
    if (bridge.required_input_contract.empty())
      bridge.required_input_contract = "region_tensor_for_segmentation_infer";
    if (bridge.required_label_contract.empty())
      bridge.required_label_contract = "region_tensor_plus_mask_or_region_label";
  }
  else if (context.case_name == "torch.yolov8.eval.feature")
  {
    if (bridge.dataset_profile.empty())
      bridge.dataset_profile = "detection_yolo_pair";
    if (bridge.prepared_root.empty())
      bridge.prepared_root = "prepared/PCBA-sample-YOLO";
    if (bridge.required_input_contract.empty())
      bridge.required_input_contract = "image_window_batch_for_detection_infer";
    if (bridge.required_label_contract.empty())
      bridge.required_label_contract = "image_window_plus_bbox_class_targets";
  }
  else if (context.case_name == "torch.yolo_mobilevit.infer.scenario")
  {
    if (bridge.dataset_profile.empty())
      bridge.dataset_profile = "two_stage_detection_roi_reclass";
    if (bridge.prepared_root.empty())
      bridge.prepared_root = "prepared/DeepPCB-YOLO";
    if (bridge.required_input_contract.empty())
      bridge.required_input_contract = "image_window_batch_for_detection_infer";
    if (bridge.required_label_contract.empty())
      bridge.required_label_contract = "roi_patch_batch_plus_roi_class_label";
  }
  else if (context.case_name == "torch.yolov8.mainline.train")
  {
    if (bridge.dataset_profile.empty())
      bridge.dataset_profile = "detection_yolo_pair";
    if (bridge.prepared_root.empty())
      bridge.prepared_root = "prepared/PCBA-sample-YOLO";
    if (bridge.input_task.empty())
      bridge.input_task = "torch.train.yolo.mainline";
    if (bridge.input_profile.empty())
      bridge.input_profile = "full-train";
    if (bridge.required_input_contract.empty())
      bridge.required_input_contract = "image_window_batch_for_detection_train";
    if (bridge.required_label_contract.empty())
      bridge.required_label_contract = "image_window_plus_bbox_class_targets";
  }
  else if (context.case_name == "torch.mobilevit.mainline.train")
  {
    if (bridge.dataset_profile.empty())
      bridge.dataset_profile = "two_stage_detection_roi_reclass";
    if (bridge.prepared_root.empty())
      bridge.prepared_root = "prepared/PCBA-sample-YOLO";
    if (bridge.input_task.empty())
      bridge.input_task = "torch.train.mobilevit.mainline";
    if (bridge.input_profile.empty())
      bridge.input_profile = "full-train";
    if (bridge.required_input_contract.empty())
      bridge.required_input_contract = "roi_patch_batch_for_reclass_train";
    if (bridge.required_label_contract.empty())
      bridge.required_label_contract = "roi_patch_batch_plus_roi_class_label";
  }
  else if (context.case_name == "torch.deeplab.mainline.train")
  {
    if (bridge.dataset_profile.empty())
      bridge.dataset_profile = "detection_with_template_pair";
    if (bridge.prepared_root.empty())
      bridge.prepared_root = "prepared/DeepPCB-YOLO";
    if (bridge.input_task.empty())
      bridge.input_task = "torch.train.deeplab.mainline";
    if (bridge.input_profile.empty())
      bridge.input_profile = "full-all";
    if (bridge.required_input_contract.empty())
      bridge.required_input_contract = "region_tensor_batch_for_segmentation_train";
    if (bridge.required_label_contract.empty())
      bridge.required_label_contract = "region_tensor_plus_mask_or_region_label";
  }

  if (bridge.prepared_root.find("DeepPCB-YOLO") != std::string::npos)
  {
    bridge.template_root = bridge.prepared_root + "/templates";
    bridge.pairs_ref = bridge.prepared_root + "/pairs.tsv";
  }

  return bridge;
}

inline TorchExecutionProfileBridge ResolveTorchExecutionProfileBridge(
  const CxScriptExecutionContext &context,
  const std::string &script_text)
{
  TorchExecutionProfileBridge bridge;
  const std::vector<std::pair<std::string, std::string>> assignments =
    detail::CollectScriptAssignmentsBridgeHelper(script_text);

  std::vector<std::string> summary_items;
  for (size_t i = 0; i < assignments.size(); ++i)
  {
    const std::string &key = assignments[i].first;
    const std::string &value = assignments[i].second;

    if (key == "input_device")
    {
      bridge.requested_device = value;
      summary_items.push_back(key + "=" + value);
      continue;
    }

    if (key == "manifest_image_ref")
    {
      bridge.manifest_image_ref = value;
      continue;
    }

    if (key == "manifest_template_image_ref")
    {
      bridge.manifest_template_image_ref = value;
      continue;
    }

    if (key == "manifest_test_image_ref")
    {
      bridge.manifest_test_image_ref = value;
      continue;
    }

    if (key == "manifest_output_ref")
    {
      bridge.manifest_output_ref = value;
      continue;
    }

    if (key == "input_profile" || key == "model" ||
        key.find("baseline_param_") == 0)
    {
      if (bridge.requested_device.empty() && key == "baseline_param_device")
        bridge.requested_device = value;
      summary_items.push_back(key + "=" + value);
    }
  }

  bridge.param_summary = detail::JoinExecutionProfileItemsBridgeHelper(summary_items);
  if (bridge.requested_device.empty())
    bridge.requested_device = "auto";

  std::vector<std::pair<std::string, std::string>> weight_items;
  if (context.case_name == "torch.mobilevit.session.feature" ||
      context.case_name == "torch.mobilevit.unified.infer" ||
      context.case_name == "torch.mobilevit.mainline.train")
  {
    weight_items.push_back(
      std::make_pair("mobilevit",
                     detail::ResolveEnvPathOrDefaultBridgeHelper(
                       "LIBTORCH_MODULE_MOBILEVIT_WEIGHTS",
                       "mobilevitv2_weights.pt")));
    if (context.case_name == "torch.mobilevit.unified.infer")
    {
      bridge.input_image_path =
        !bridge.manifest_image_ref.empty()
          ? bridge.manifest_image_ref
          : detail::ResolveOptionalEnvValueBridgeHelper(
              "LIBTORCH_MODULE_MOBILEVIT_INFER_IMAGE");
      if (bridge.input_image_path.empty())
        bridge.input_image_path =
          detail::ResolveWorkspaceRelativeBridgeHelper(
            "local_test\\torch_main_thread\\directional_selection\\D2_local_texture_tolerance\\defect\\cell2105.png");
      bridge.attach_back_output_path =
        !bridge.manifest_output_ref.empty()
          ? bridge.manifest_output_ref
          : detail::ResolveOptionalEnvValueBridgeHelper(
              "LIBTORCH_MODULE_MOBILEVIT_INFER_OUTPUT");
      if (bridge.attach_back_output_path.empty())
        bridge.attach_back_output_path =
          detail::ResolveWorkspaceRelativeBridgeHelper(
            "docs\\notes\\tmp\\mobilevit_unified_review\\cell2105.mobilevit_roi.jpg");
    }
  }
  else if (context.case_name == "torch.yolo_mobilevit.infer.scenario")
  {
    weight_items.push_back(
      std::make_pair("yolo",
                     detail::ResolveEnvWorkspaceOrDefaultBridgeHelper(
                       "LIBTORCH_MODULE_YOLO_WEIGHTS",
                       "analysis_workspace\\model\\yolov8n_dict.pt",
                       "yolov8n_dict.pt")));
    weight_items.push_back(
      std::make_pair("mobilevit",
                     detail::ResolveEnvWorkspaceOrDefaultBridgeHelper(
                       "LIBTORCH_MODULE_MOBILEVIT_WEIGHTS",
                       "analysis_workspace\\model\\mobilevitv2_weights.pt",
                       "mobilevitv2_weights.pt")));
    bridge.input_image_path =
      detail::ResolveOptionalEnvValueBridgeHelper("LIBTORCH_MODULE_YOLO_INFER_IMAGE");
    if (bridge.input_image_path.empty())
      bridge.input_image_path =
        detail::ResolveWorkspaceRelativeBridgeHelper(
          "local_test\\torch_main_thread\\directional_selection\\D3_repeat_region_precision\\pcba_pose_like\\image\\00000.jpg");
    bridge.attach_back_output_path =
      detail::ResolveOptionalEnvValueBridgeHelper("LIBTORCH_MODULE_YOLO_INFER_OUTPUT");
    if (bridge.attach_back_output_path.empty())
      bridge.attach_back_output_path =
        detail::ResolveWorkspaceRelativeBridgeHelper(
          "docs\\notes\\tmp\\mobilevitv2_mainline_validation\\cli_scenario.overlay.jpg");
    const std::string meta_path =
      bridge.attach_back_output_path.empty()
        ? std::string()
        : bridge.attach_back_output_path + ".meta.txt";
    const std::map<std::string, std::string> meta_values =
      detail::ReadKeyValueFileBridgeHelper(meta_path);
    std::map<std::string, std::string>::const_iterator it =
      meta_values.find("attach_back_overlay_status");
    if (it != meta_values.end())
      bridge.attach_back_overlay_status = it->second;
    it = meta_values.find("attach_back_top1_class");
    if (it != meta_values.end())
      bridge.attach_back_top1_class = it->second;
    it = meta_values.find("attach_back_confidence");
    if (it != meta_values.end())
      bridge.attach_back_confidence = it->second;
  }
  else if (context.case_name == "torch.resnet18.baseline.infer" ||
           context.case_name == "torch.resnet18.baseline.feature")
  {
    weight_items.push_back(
      std::make_pair("resnet18",
                     detail::ResolveEnvPathOrDefaultBridgeHelper(
                       "LIBTORCH_MODULE_RESNET18_WEIGHTS",
                       "resnet18_weights.pt")));
  }
  else if (context.case_name == "torch.resnet50.baseline.infer" ||
           context.case_name == "torch.resnet50.baseline.feature")
  {
    weight_items.push_back(
      std::make_pair("resnet50",
                     detail::ResolveEnvPathOrDefaultBridgeHelper(
                       "LIBTORCH_MODULE_RESNET50_WEIGHTS",
                       "resnet50_weights.pt")));
  }
  else if (context.case_name == "torch.deeplab.contract.feature" ||
           context.case_name == "torch.deeplab.unified.infer" ||
           context.case_name == "torch.deeplab.mainline.train")
  {
    weight_items.push_back(
      std::make_pair("deeplab",
                     detail::ResolveEnvWorkspaceOrDefaultBridgeHelper(
                       "LIBTORCH_MODULE_DEEPLAB_WEIGHTS",
                       "analysis_workspace\\model\\deeplabv3_mobilenet_v3_large-fc3c493d.pth",
                       "deeplabv3_mobilenet_v3_large-fc3c493d.pth")));
    if (context.case_name == "torch.deeplab.unified.infer")
    {
      bridge.input_image_path =
        !bridge.manifest_test_image_ref.empty()
          ? bridge.manifest_test_image_ref
          : !bridge.manifest_image_ref.empty()
              ? bridge.manifest_image_ref
              : detail::ResolveOptionalEnvValueBridgeHelper(
                  "LIBTORCH_MODULE_DEEPLAB_TEST_IMAGE");
      if (bridge.input_image_path.empty())
        bridge.input_image_path =
          detail::ResolveWorkspaceRelativeBridgeHelper(
            "local_test\\torch_main_thread\\directional_selection\\D3_repeat_region_precision\\deeppcb_pairs\\test\\20085291.jpg");
      bridge.attach_back_output_path =
        !bridge.manifest_output_ref.empty()
          ? bridge.manifest_output_ref
          : detail::ResolveOptionalEnvValueBridgeHelper(
              "LIBTORCH_MODULE_DEEPLAB_OUTPUT");
      if (bridge.attach_back_output_path.empty())
        bridge.attach_back_output_path =
          detail::ResolveWorkspaceRelativeBridgeHelper(
            "docs\\notes\\tmp\\deeplab_unified_review\\20085291.diff_overlay.jpg");
    }
  }
  else if (context.case_name == "torch.yolov8.mainline.train")
  {
    weight_items.push_back(
      std::make_pair("yolo_pretrained",
                     detail::ResolveEnvWorkspaceOrDefaultBridgeHelper(
                       "LIBTORCH_MODULE_YOLO_PRETRAINED",
                       "analysis_workspace\\model\\yolov8n_dict.pt",
                       "yolov8n_dict.pt")));
  }

  bridge.consumed_weight_files =
    detail::JoinPathSummaryBridgeHelper(weight_items, true);
  bridge.consumed_weight_paths =
    detail::JoinPathSummaryBridgeHelper(weight_items, false);
  if (context.layer == "train")
    bridge.train_param_summary = bridge.param_summary;
  if (context.layer == "infer" || context.layer == "scenario")
    bridge.infer_param_summary = bridge.param_summary;
  return bridge;
}
}

#endif