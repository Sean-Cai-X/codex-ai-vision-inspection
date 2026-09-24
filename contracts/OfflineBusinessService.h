#pragma once

// RND-VAI-001 / RND-VAI-002 local, offline business contract.
// This boundary intentionally has no image bytes, image paths, masks, overlays,
// thumbnails, or model promotion operations.

#include <array>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace vision_ai::offline_business::v1 {

inline constexpr char kTrainingCapability[] =
    "torch.train.segmentation.business_incremental.v1";
inline constexpr char kInferenceCapability[] =
    "torch.infer.segmentation.business_isolated.v1";
inline constexpr std::array<const char*, 7> kGeometryContract = {
    "arc", "circle", "ellipse", "line", "open_curve", "polygon", "closed_curve"};

enum class ErrorCode {
  None, ServiceUnavailable, CapabilityUnavailable, CapabilityVersionMismatch,
  GeometryContractMismatch, AssetReferenceInvalid, AssetHashMismatch,
  AssetRootOutOfScope, AnnotationReceiptMissing, DatasetRevisionMissing,
  DatasetDigestMismatch, SplitInvalid, RealMaskRequired,
  ParentArtifactUnavailable, CandidateArtifactUnavailable, ReceiptUnverified,
  TrainingParameterInvalid, TaskNotFound, TaskCancelled, RuntimeExecutionFailed
};

struct Failure {
  ErrorCode code = ErrorCode::None;
  std::string failure_stage;
  std::string recovery;
  std::string request_or_task_id;
};

struct SplitSummary { unsigned train = 0; unsigned valid = 0; };
// These are the business-controlled trial inputs. The executor reports (rather
// than accepts) the effective learning rate so the UI never invents it.
struct BusinessTrialParameters {
  unsigned training_split_percent = 0;
  unsigned epoch_count = 0;
  unsigned iteration_count = 0;
  unsigned batch_size = 0;
  std::string trial_model_id;
  // Binding rule for the annotation used to create this dataset revision.
  // It affects Train/Valid semantics as well as VERIFY interaction.
  std::string annotation_click_mode;
};
struct RoiSummary { std::string kind; unsigned node_count = 0; };
struct CurvePoint { unsigned epoch = 0; double training_loss = 0; double validation_score = 0; double effective_learning_rate = 0; };
struct GeometrySummary { std::string kind; unsigned node_count = 0; double area = 0; };
struct Prediction { int class_id = -1; std::string class_name; double score = 0; std::array<double,4> bounds{}; GeometrySummary geometry; };
struct TypedConclusion { std::string code; std::string message; }; // PASS | FAIL | REVIEW

struct TrainingRequest {
  std::string schema = "visionai.business.training_request.v1";
  std::string request_id, case_id, project_id, normalized_asset_root_ref;
  std::string dataset_revision, dataset_sha256;
  SplitSummary split;
  BusinessTrialParameters parameters;
  std::vector<std::string> geometry_contract;
  std::vector<std::string> annotation_receipt_digests;
  std::string parent_capability = kTrainingCapability;
  std::string parent_model_digest;
};

struct InferenceRequest {
  std::string schema = "visionai.business.inference_request.v1";
  std::string request_id, project_id, case_id, asset_ref, image_sha256;
  std::string geometry_target;
  RoiSummary roi;
  std::string candidate_model_digest, dataset_revision;
};

enum class TrainingState { DevelopmentTrialReady, Running, Candidate, Reviewed, Rejected, RolledBack, Cancelled };
enum class InferenceState { Queued, Running, Completed, Rejected, Cancelled, Failed };

struct Capability {
  std::string id, version, dll_sha256;
  bool available = false;
  bool trial_only = true;
  std::array<const char*,7> geometry_contract = kGeometryContract;
  std::string unavailable_recovery;
  // GetCapability returns the only model IDs that business users may select.
  // IDs are not paths, checkpoints or arbitrary artifact digests.
  std::vector<std::string> trial_model_ids;
  std::vector<std::string> annotation_click_modes;
};

struct TrainingStatus {
  std::string training_task_id, request_digest, dataset_sha256, parent_model_digest;
  TrainingState state = TrainingState::Rejected;
  unsigned progress_percent = 0;
  BusinessTrialParameters parameters;
  double effective_learning_rate = 0;
  std::vector<CurvePoint> curve;
  std::optional<Failure> error;
};
struct InferenceStatus {
  std::string inference_task_id, request_digest;
  InferenceState state = InferenceState::Rejected;
  unsigned duration_ms = 0;
  std::optional<Failure> error;
};
struct TrainingReceipt {
  std::string schema = "visionai.business.training_receipt.v1";
  std::string training_task_id, request_digest, dataset_sha256, parent_model_digest;
  std::string candidate_artifact_digest, runtime_digest, capability_id, capability_version, signature;
  BusinessTrialParameters parameters;
  double effective_learning_rate = 0;
  TrainingState state = TrainingState::Rejected;
  bool verified = false;
  std::optional<Failure> error;
};
struct InferenceReceipt {
  std::string schema = "visionai.business.inference_receipt.v1";
  std::string inference_task_id, model_digest, image_sha256, dataset_revision;
  InferenceState state = InferenceState::Rejected;
  unsigned duration_ms = 0;
  TypedConclusion conclusion;
  std::vector<Prediction> predictions;
  std::string external_callback_request_id;
  std::string external_callback_state = "NOT_CONFIGURED";
  std::optional<Failure> error;
};

// The only component allowed to resolve an asset reference. It must validate
// scope/hash/real manifests without returning the file system path to callers.
struct AssetBroker {
  virtual ~AssetBroker() = default;
  virtual std::optional<Failure> ValidateTrainingAssets(const TrainingRequest&) = 0;
  virtual std::optional<Failure> ValidateInferenceAsset(const InferenceRequest&) = 0;
};
struct TrainingExecutor {
  virtual ~TrainingExecutor() = default;
  virtual TrainingReceipt Execute(const TrainingRequest&, std::function<bool(unsigned, const CurvePoint&)>) = 0;
};
struct InferenceExecutor {
  virtual ~InferenceExecutor() = default;
  virtual InferenceReceipt Execute(const InferenceRequest&) = 0;
};

class Service final {
 public:
  Service(Capability training, Capability inference, std::shared_ptr<AssetBroker>,
          std::shared_ptr<TrainingExecutor>, std::shared_ptr<InferenceExecutor>);
  ~Service();
  Service(const Service&) = delete;
  Service& operator=(const Service&) = delete;
  Capability GetCapability(const std::string& capability_id) const;
  TrainingStatus SubmitBusinessTraining(const TrainingRequest&);
  TrainingStatus GetBusinessTrainingStatus(const std::string& task_id) const;
  TrainingStatus CancelBusinessTraining(const std::string& task_id, const std::string& reason_code);
  TrainingReceipt GetBusinessTrainingReceipt(const std::string& task_id) const;
  InferenceStatus SubmitBusinessInference(const InferenceRequest&);
  InferenceStatus GetBusinessInferenceStatus(const std::string& task_id) const;
  InferenceReceipt GetBusinessInferenceReceipt(const std::string& task_id) const;

 private:
  struct TrainingTask; struct InferenceTask;
  Capability training_, inference_; std::shared_ptr<AssetBroker> broker_;
  std::shared_ptr<TrainingExecutor> trainer_; std::shared_ptr<InferenceExecutor> inferer_;
  mutable std::mutex mutex_;
  std::map<std::string, std::shared_ptr<TrainingTask>> training_tasks_;
  std::map<std::string, std::shared_ptr<InferenceTask>> inference_tasks_;
  std::vector<std::thread> workers_;
};

const char* ToString(ErrorCode);
const char* ToString(TrainingState);
const char* ToString(InferenceState);
}  // namespace vision_ai::offline_business::v1
