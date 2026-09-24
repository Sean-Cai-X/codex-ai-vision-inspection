#include "contracts/OfflineBusinessService.h"

#include <chrono>
#include <iostream>
#include <thread>

namespace ob = vision_ai::offline_business::v1;
constexpr char kDigest[] = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

struct Broker final : ob::AssetBroker {
  std::optional<ob::Failure> ValidateTrainingAssets(const ob::TrainingRequest&) override { return std::nullopt; }
  std::optional<ob::Failure> ValidateInferenceAsset(const ob::InferenceRequest&) override { return std::nullopt; }
};
struct Trainer final : ob::TrainingExecutor {
  ob::TrainingReceipt Execute(const ob::TrainingRequest&, std::function<bool(unsigned, const ob::CurvePoint&)> progress) override {
    progress(35, {1, .8, .6, .001}); progress(80, {2, .4, .8, .0005});
    ob::TrainingReceipt receipt; receipt.state=ob::TrainingState::Candidate; receipt.verified=true;
    receipt.candidate_artifact_digest=kDigest; receipt.runtime_digest=kDigest; receipt.effective_learning_rate=.0005; receipt.signature="offline-test-signature"; return receipt;
  }
};
struct Inferer final : ob::InferenceExecutor {
  ob::InferenceReceipt Execute(const ob::InferenceRequest&) override {
    ob::InferenceReceipt receipt; receipt.state=ob::InferenceState::Completed; receipt.conclusion={"PASS", "typed business conclusion"};
    receipt.predictions.push_back({6,"closed_curve",.95,{1,2,3,4},{"polygon",4,12}}); return receipt;
  }
};
template<typename Predicate> bool Wait(Predicate p) { for (int i=0;i<100;++i) { if (p()) return true; std::this_thread::sleep_for(std::chrono::milliseconds(5)); } return false; }
int main() {
  ob::Capability training{ob::kTrainingCapability,"v1",kDigest,true,true,ob::kGeometryContract,""};
  training.trial_model_ids={"segmentation-small-v1"};
  training.annotation_click_modes={"center"};
  ob::Capability inference{ob::kInferenceCapability,"v1",kDigest,true,true,ob::kGeometryContract,""};
  ob::Service service(training,inference,std::make_shared<Broker>(),std::make_shared<Trainer>(),std::make_shared<Inferer>());
  ob::TrainingRequest train; train.request_id="request-train"; train.case_id="case"; train.project_id="project"; train.normalized_asset_root_ref="root-ref"; train.dataset_revision="r1"; train.dataset_sha256=kDigest; train.parent_model_digest=kDigest; train.geometry_contract.assign(ob::kGeometryContract.begin(),ob::kGeometryContract.end()); train.annotation_receipt_digests={kDigest}; train.split={1,1}; train.parameters={70,120,500,2,"segmentation-small-v1","center"};
  auto submitted=service.SubmitBusinessTraining(train);
  if (!Wait([&]{ auto s=service.GetBusinessTrainingStatus(submitted.training_task_id); return s.state==ob::TrainingState::Candidate; })) return 1;
  auto receipt=service.GetBusinessTrainingReceipt(submitted.training_task_id);
  if (!receipt.verified || receipt.state!=ob::TrainingState::Candidate || receipt.candidate_artifact_digest!=kDigest) return 2;
  if (receipt.parameters.epoch_count!=120 || receipt.parameters.batch_size!=2 || receipt.parameters.trial_model_id!="segmentation-small-v1" || receipt.parameters.annotation_click_mode!="center") return 6;
  if (service.GetBusinessTrainingStatus(submitted.training_task_id).effective_learning_rate!=.0005 || receipt.effective_learning_rate!=.0005) return 7;
  ob::InferenceRequest infer; infer.request_id="request-infer";infer.case_id="case";infer.project_id="project";infer.asset_ref="asset-ref";infer.image_sha256=kDigest;infer.geometry_target="closed_curve";infer.roi={"polygon",4};infer.candidate_model_digest=kDigest;infer.dataset_revision="r1";
  auto pending=service.SubmitBusinessInference(infer);
  if (!Wait([&]{return service.GetBusinessInferenceStatus(pending.inference_task_id).state==ob::InferenceState::Completed;})) return 3;
  auto inferenceReceipt=service.GetBusinessInferenceReceipt(pending.inference_task_id);
  if (inferenceReceipt.predictions.size()!=1 || inferenceReceipt.predictions[0].class_id!=6 || inferenceReceipt.conclusion.code!="PASS") return 4;
  train.request_id="bad-geometry"; train.geometry_contract[0]="rectangle";
  auto rejected=service.SubmitBusinessTraining(train);
  if (!rejected.error || rejected.error->code!=ob::ErrorCode::GeometryContractMismatch) return 5;
  std::cout << "offline business service contract: PASS\n";
}
