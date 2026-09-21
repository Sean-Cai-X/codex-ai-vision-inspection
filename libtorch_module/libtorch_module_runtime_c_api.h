#pragma once

#ifdef _WIN32
#ifdef LIBTORCH_MODULE_RUNTIME_EXPORTS
#define TORCH_RUNTIME_API __declspec(dllexport)
#else
#define TORCH_RUNTIME_API __declspec(dllimport)
#endif
#else
#define TORCH_RUNTIME_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void *TorchRuntimeHandle;

typedef struct TorchRuntimeConfig {
  const char *model_root;
  const char *output_root;
  const char *device;
  const char *log_level;
} TorchRuntimeConfig;

typedef struct TorchTaskRequest {
  const char *task;
  const char *device;
  const char *input_image;
  const char *dataset_root;
  const char *manifest_path;
  const char *case_name;
  const char *extra_json;
  const char *output_dir;
} TorchTaskRequest;

typedef struct TorchTaskResult {
  int ok;
  int error_code;

  const char *status;
  const char *error_message;

  const char *requested_device;
  const char *actual_device;

  double train_runtime_ms;
  double infer_runtime_ms;
  double algorithm_runtime_ms;
  double placeholder_runtime_ms;

  const char *result_json;
  const char *evidence_ref;
  const char *result_ref;

  const char *input_image_ref;
  const char *primary_visual_ref;
  const char *visualization_refs;

  const char *bbox_candidate_list_ref;
  const char *roi_crop_packet_ref;
  const char *attach_back_ref;
  const char *template_alignment_ref;
  const char *roi_diff_candidate_ref;

  const char *trainer_lifecycle_summary;
  const char *unified_mainline_summary;
} TorchTaskResult;

TORCH_RUNTIME_API int torch_runtime_create(const TorchRuntimeConfig *config,
                                           TorchRuntimeHandle *out_handle);

TORCH_RUNTIME_API int torch_runtime_destroy(TorchRuntimeHandle handle);

TORCH_RUNTIME_API int torch_runtime_run_task(TorchRuntimeHandle handle,
                                             const TorchTaskRequest *request,
                                             TorchTaskResult *out_result);

TORCH_RUNTIME_API void torch_runtime_free_result(TorchTaskResult *result);

TORCH_RUNTIME_API const char *torch_runtime_version();

#ifdef __cplusplus
}
#endif
