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

// Initializes a result structure for one call.  Ownership rule: callers must
// either value/zero initialize TorchTaskResult, or call
// torch_runtime_free_result followed by this function before reusing it.  A
// live result must never be overwritten because its allocated strings would
// otherwise be leaked.  Every v1 business call initializes a non-null output
// on entry and supplies a safe failure result when it can.
TORCH_RUNTIME_API void torch_runtime_init_result(TorchTaskResult *result);

// Versioned, typed business-trial callbacks.  They expose numeric execution
// evidence only; image paths, masks, overlays and pixels stay inside the
// local broker/runtime process.  Return non-zero from the epoch callback to
// continue, or zero to request cancellation.  Return non-zero from the
// cancellation callback when an in-flight business task must stop.
typedef int (*TorchBusinessTrialEpochCallbackV1)(
    void *user_data,
    unsigned int progress_percent,
    unsigned int epoch,
    unsigned int completed_iterations,
    double training_loss,
    double validation_score,
    int has_validation_score,
    double effective_learning_rate);

typedef int (*TorchBusinessTrialCancelCallbackV1)(void *user_data);

TORCH_RUNTIME_API int torch_runtime_create(const TorchRuntimeConfig *config,
                                           TorchRuntimeHandle *out_handle);

TORCH_RUNTIME_API int torch_runtime_destroy(TorchRuntimeHandle handle);

TORCH_RUNTIME_API int torch_runtime_run_task(TorchRuntimeHandle handle,
                                             const TorchTaskRequest *request,
                                             TorchTaskResult *out_result);

// These entry points deliberately bypass the generic task dispatcher.  They
// are callable only by the local typed business service after its asset broker
// has resolved and frozen the controlled asset binding.  They never activate
// production models; training can only create a review-required candidate in
// the isolated staging root.
TORCH_RUNTIME_API int torch_runtime_run_business_trial_v1(
    TorchRuntimeHandle handle,
    const TorchTaskRequest *request,
    TorchBusinessTrialEpochCallbackV1 on_epoch,
    TorchBusinessTrialCancelCallbackV1 is_cancelled,
    void *user_data,
    TorchTaskResult *out_result);

TORCH_RUNTIME_API int torch_runtime_run_business_inference_v1(
    TorchRuntimeHandle handle,
    const TorchTaskRequest *request,
    TorchTaskResult *out_result);

TORCH_RUNTIME_API void torch_runtime_free_result(TorchTaskResult *result);

TORCH_RUNTIME_API const char *torch_runtime_version();

#ifdef __cplusplus
}
#endif
