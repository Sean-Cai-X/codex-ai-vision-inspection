#include "OfflineBusinessService.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <thread>

namespace vision_ai::offline_business::v1 {
namespace {
std::atomic<unsigned long long> g_next_id{1};
std::string Id(const char* type) { return std::string(type) + "-" + std::to_string(g_next_id++); }
std::string Digest(const std::string& value) { // Correlation digest only; production executor supplies SHA256 receipt digests.
  const auto h = std::hash<std::string>{}(value); std::ostringstream s; s << "local-request-" << std::hex << h; return s.str();
}
Failure Fail(ErrorCode c, std::string stage, std::string recovery, std::string id) {
  return {c, std::move(stage), std::move(recovery), std::move(id)};
}
bool ValidSha(const std::string& s) { return s.size() == 64 && std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isxdigit(c); }); }
bool GeometryOk(const std::vector<std::string>& values) {
  return values.size() == kGeometryContract.size() && std::equal(values.begin(), values.end(), kGeometryContract.begin(), [](const auto& a, const auto* b) { return a == b; });
}
bool GeometryOk(const std::string& value) { return std::find(kGeometryContract.begin(), kGeometryContract.end(), value) != kGeometryContract.end(); }
bool TrialParametersOk(const BusinessTrialParameters& p, const Capability& c) {
  return p.training_split_percent > 0 && p.training_split_percent < 100 &&
      p.epoch_count > 0 && p.iteration_count > 0 && p.batch_size > 0 &&
      !p.trial_model_id.empty() &&
      std::find(c.trial_model_ids.begin(), c.trial_model_ids.end(), p.trial_model_id) != c.trial_model_ids.end() &&
      !p.annotation_click_mode.empty() &&
      std::find(c.annotation_click_modes.begin(), c.annotation_click_modes.end(), p.annotation_click_mode) != c.annotation_click_modes.end();
}
}

struct Service::TrainingTask { TrainingStatus status; TrainingReceipt receipt; bool cancel = false; };
struct Service::InferenceTask { InferenceStatus status; InferenceReceipt receipt; };

const char* ToString(ErrorCode v) { static const char* x[] = {"NONE","SERVICE_UNAVAILABLE","CAPABILITY_UNAVAILABLE","CAPABILITY_VERSION_MISMATCH","GEOMETRY_CONTRACT_MISMATCH","ASSET_REFERENCE_INVALID","ASSET_HASH_MISMATCH","ASSET_ROOT_OUT_OF_SCOPE","ANNOTATION_RECEIPT_MISSING","DATASET_REVISION_MISSING","DATASET_DIGEST_MISMATCH","SPLIT_INVALID","REAL_MASK_REQUIRED","PARENT_ARTIFACT_UNAVAILABLE","CANDIDATE_ARTIFACT_UNAVAILABLE","RECEIPT_UNVERIFIED","TRAINING_PARAMETER_INVALID","TASK_NOT_FOUND","TASK_CANCELLED","RUNTIME_EXECUTION_FAILED"}; return x[static_cast<int>(v)]; }
const char* ToString(TrainingState v) { static const char* x[] = {"DEVELOPMENT_TRIAL_READY","RUNNING","CANDIDATE","REVIEWED","REJECTED","ROLLED_BACK","CANCELLED"}; return x[static_cast<int>(v)]; }
const char* ToString(InferenceState v) { static const char* x[] = {"QUEUED","RUNNING","COMPLETED","REJECTED","CANCELLED","FAILED"}; return x[static_cast<int>(v)]; }

Service::Service(Capability t, Capability i, std::shared_ptr<AssetBroker> b, std::shared_ptr<TrainingExecutor> tr, std::shared_ptr<InferenceExecutor> in)
  : training_(std::move(t)), inference_(std::move(i)), broker_(std::move(b)), trainer_(std::move(tr)), inferer_(std::move(in)) {}
Service::~Service() { for (auto& worker : workers_) if (worker.joinable()) worker.join(); }
Capability Service::GetCapability(const std::string& id) const { return id == training_.id ? training_ : id == inference_.id ? inference_ : Capability{id, "", "", false, true, kGeometryContract, "Install the requested offline capability."}; }

TrainingStatus Service::SubmitBusinessTraining(const TrainingRequest& r) {
  auto task = std::make_shared<TrainingTask>(); task->status.training_task_id = Id("training"); task->status.request_digest = Digest(r.request_id + r.dataset_sha256); task->status.dataset_sha256 = r.dataset_sha256; task->status.parent_model_digest = r.parent_model_digest; task->status.parameters=r.parameters; task->status.state = TrainingState::Rejected;
  auto reject = [&](Failure f) { task->status.error=f; task->receipt.training_task_id=task->status.training_task_id; task->receipt.request_digest=task->status.request_digest; task->receipt.dataset_sha256=r.dataset_sha256; task->receipt.parent_model_digest=r.parent_model_digest; task->receipt.parameters=r.parameters; task->receipt.state=TrainingState::Rejected; task->receipt.error=f; };
  if (r.schema != "visionai.business.training_request.v1" || r.request_id.empty() || r.case_id.empty() || r.project_id.empty()) reject(Fail(ErrorCode::DatasetRevisionMissing,"request_validation","Provide immutable request, case and project ids.",r.request_id));
  else if (!training_.available || !trainer_ || !broker_) reject(Fail(ErrorCode::CapabilityUnavailable,"capability_resolution",training_.unavailable_recovery,r.request_id));
  else if (r.parent_capability != kTrainingCapability) reject(Fail(ErrorCode::CapabilityVersionMismatch,"capability_validation","Use the business incremental segmentation capability.",r.request_id));
  else if (!GeometryOk(r.geometry_contract)) reject(Fail(ErrorCode::GeometryContractMismatch,"geometry_validation","Submit exactly the fixed seven-class contract in its defined order.",r.request_id));
  else if (r.dataset_revision.empty() || !ValidSha(r.dataset_sha256)) reject(Fail(ErrorCode::DatasetDigestMismatch,"dataset_validation","Provide immutable dataset revision and SHA256.",r.request_id));
  else if (!ValidSha(r.parent_model_digest)) reject(Fail(ErrorCode::ParentArtifactUnavailable,"parent_artifact_validation","Provide a registered parent model SHA256.",r.request_id));
  else if (!TrialParametersOk(r.parameters, training_)) reject(Fail(ErrorCode::TrainingParameterInvalid,"parameter_validation","Set a 1-99 split, positive epoch/iteration/batch values, and model/click-mode IDs advertised by GetCapability.",r.request_id));
  else if (!r.split.train || !r.split.valid) reject(Fail(ErrorCode::SplitInvalid,"split_validation","Provide non-empty Train and Valid partitions.",r.request_id));
  else if (r.annotation_receipt_digests.empty()) reject(Fail(ErrorCode::AnnotationReceiptMissing,"annotation_validation","Provide ROI and annotation receipt digests.",r.request_id));
  else if (auto failure = broker_->ValidateTrainingAssets(r)) reject(*failure);
  else { task->status.state = TrainingState::DevelopmentTrialReady; task->receipt.training_task_id=task->status.training_task_id; task->receipt.request_digest=task->status.request_digest; task->receipt.dataset_sha256=r.dataset_sha256; task->receipt.parent_model_digest=r.parent_model_digest; task->receipt.parameters=r.parameters; task->receipt.capability_id=kTrainingCapability; task->receipt.capability_version=training_.version; task->receipt.state=TrainingState::DevelopmentTrialReady; }
  { std::lock_guard<std::mutex> lock(mutex_); training_tasks_[task->status.training_task_id] = task; }
  if (task->status.state != TrainingState::DevelopmentTrialReady) return task->status;
  std::thread worker([this, task, r] { { std::lock_guard<std::mutex> lock(mutex_); if (task->cancel) return; task->status.state=TrainingState::Running; }
    auto receipt=trainer_->Execute(r, [this, task](unsigned progress, const CurvePoint& point) { std::lock_guard<std::mutex> lock(mutex_); if (task->cancel) return false; task->status.progress_percent=std::min(100u,progress); task->status.effective_learning_rate=point.effective_learning_rate; task->status.curve.push_back(point); return true; });
    std::lock_guard<std::mutex> lock(mutex_); if (task->cancel) return; task->receipt=std::move(receipt); task->receipt.training_task_id=task->status.training_task_id; task->receipt.request_digest=task->status.request_digest; task->receipt.dataset_sha256=r.dataset_sha256; task->receipt.parent_model_digest=r.parent_model_digest; task->receipt.parameters=r.parameters; task->receipt.capability_id=kTrainingCapability; task->receipt.capability_version=training_.version; task->status.effective_learning_rate=task->receipt.effective_learning_rate > 0 ? task->receipt.effective_learning_rate : task->status.effective_learning_rate; if (!task->receipt.error && (!task->receipt.verified || !ValidSha(task->receipt.candidate_artifact_digest))) task->receipt.error=Fail(ErrorCode::ReceiptUnverified,"receipt_validation","Verify the candidate artifact receipt before review.",task->status.training_task_id); task->status.error=task->receipt.error; task->status.state=task->receipt.error ? TrainingState::Rejected : task->receipt.state; task->status.progress_percent=task->status.state==TrainingState::Candidate || task->status.state==TrainingState::Reviewed ? 100 : task->status.progress_percent; });
  { std::lock_guard<std::mutex> lock(mutex_); workers_.push_back(std::move(worker)); }
  return task->status;
}
TrainingStatus Service::GetBusinessTrainingStatus(const std::string& id) const { std::lock_guard<std::mutex> lock(mutex_); auto i=training_tasks_.find(id); if(i!=training_tasks_.end()) return i->second->status; TrainingStatus s; s.training_task_id=id; s.error=Fail(ErrorCode::TaskNotFound,"task_lookup","Submit a new immutable request.",id); return s; }
TrainingStatus Service::CancelBusinessTraining(const std::string& id, const std::string&) { std::lock_guard<std::mutex> lock(mutex_); auto i=training_tasks_.find(id); if(i==training_tasks_.end()) { TrainingStatus s; s.training_task_id=id; s.error=Fail(ErrorCode::TaskNotFound,"task_lookup","Submit a new immutable request.",id); return s; } i->second->cancel=true; i->second->status.state=TrainingState::Cancelled; i->second->status.error=Fail(ErrorCode::TaskCancelled,"cancellation","Submit a new immutable request when ready.",id); i->second->receipt.state=TrainingState::Cancelled; i->second->receipt.error=i->second->status.error; return i->second->status; }
TrainingReceipt Service::GetBusinessTrainingReceipt(const std::string& id) const { std::lock_guard<std::mutex> lock(mutex_); auto i=training_tasks_.find(id); if(i!=training_tasks_.end()) return i->second->receipt; TrainingReceipt r; r.training_task_id=id; r.error=Fail(ErrorCode::TaskNotFound,"task_lookup","Submit a new immutable request.",id); return r; }

InferenceStatus Service::SubmitBusinessInference(const InferenceRequest& r) {
  auto task=std::make_shared<InferenceTask>(); task->status.inference_task_id=Id("inference"); task->status.request_digest=Digest(r.request_id+r.image_sha256); auto reject=[&](Failure f){task->status.state=InferenceState::Rejected;task->status.error=f;task->receipt.inference_task_id=task->status.inference_task_id;task->receipt.error=f;task->receipt.state=InferenceState::Rejected;};
  if(r.schema!="visionai.business.inference_request.v1"||r.request_id.empty()||r.case_id.empty()||r.project_id.empty()) reject(Fail(ErrorCode::AssetReferenceInvalid,"request_validation","Provide immutable request, case and project ids.",r.request_id)); else if(!inference_.available||!inferer_||!broker_) reject(Fail(ErrorCode::CapabilityUnavailable,"capability_resolution",inference_.unavailable_recovery,r.request_id)); else if(!GeometryOk(r.geometry_target)) reject(Fail(ErrorCode::GeometryContractMismatch,"geometry_validation","Use one of the fixed seven geometry classes.",r.request_id)); else if(r.asset_ref.empty()||!ValidSha(r.image_sha256)) reject(Fail(ErrorCode::AssetHashMismatch,"asset_validation","Provide asset_ref and image SHA256 only.",r.request_id)); else if(!ValidSha(r.candidate_model_digest)) reject(Fail(ErrorCode::CandidateArtifactUnavailable,"candidate_validation","Provide a registered candidate model SHA256.",r.request_id)); else if(auto f=broker_->ValidateInferenceAsset(r)) reject(*f); else task->status.state=InferenceState::Queued;
  {std::lock_guard<std::mutex> lock(mutex_); inference_tasks_[task->status.inference_task_id]=task;} if(task->status.state!=InferenceState::Queued) return task->status;
  std::thread worker([this,task,r]{ const auto start=std::chrono::steady_clock::now(); {std::lock_guard<std::mutex> lock(mutex_);task->status.state=InferenceState::Running;} auto receipt=inferer_->Execute(r); const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count(); std::lock_guard<std::mutex> lock(mutex_); task->receipt=std::move(receipt); task->receipt.inference_task_id=task->status.inference_task_id;task->receipt.model_digest=r.candidate_model_digest;task->receipt.image_sha256=r.image_sha256;task->receipt.dataset_revision=r.dataset_revision;task->receipt.duration_ms=static_cast<unsigned>(elapsed);task->status.duration_ms=task->receipt.duration_ms; if(task->receipt.error)task->status.state=InferenceState::Failed;else if(task->receipt.state!=InferenceState::Completed)task->status.state=InferenceState::Failed;else task->status.state=InferenceState::Completed;task->status.error=task->receipt.error;}); { std::lock_guard<std::mutex> lock(mutex_); workers_.push_back(std::move(worker)); } return task->status;
}
InferenceStatus Service::GetBusinessInferenceStatus(const std::string& id) const {std::lock_guard<std::mutex> lock(mutex_);auto i=inference_tasks_.find(id);if(i!=inference_tasks_.end())return i->second->status;InferenceStatus s;s.inference_task_id=id;s.error=Fail(ErrorCode::TaskNotFound,"task_lookup","Submit a new immutable request.",id);return s;}
InferenceReceipt Service::GetBusinessInferenceReceipt(const std::string& id) const {std::lock_guard<std::mutex> lock(mutex_);auto i=inference_tasks_.find(id);if(i!=inference_tasks_.end())return i->second->receipt;InferenceReceipt r;r.inference_task_id=id;r.error=Fail(ErrorCode::TaskNotFound,"task_lookup","Submit a new immutable request.",id);return r;}
} // namespace vision_ai::offline_business::v1
