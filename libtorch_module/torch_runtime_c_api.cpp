#include "libtorch_module_runtime_c_api.h"
#include "torch_runtime_core.h"
#include "torch_runtime_task_types.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct TorchRuntimeImpl {
  TorchRuntimeCoreConfig config;
};

namespace {

void CopyString(const std::string& src, const char*& dst) {
  dst = nullptr;
  if (src.empty()) return;
  char* buffer = new char[src.size() + 1];
  std::memcpy(buffer, src.c_str(), src.size() + 1);
  dst = buffer;
}

void FreeString(const char*& value) {
  delete[] value;
  value = nullptr;
}

std::string StringValue(const char* value) {
  return value == nullptr ? std::string() : std::string(value);
}

std::string JsonQuote(const std::string& value) {
  std::ostringstream stream;
  stream << '"';
  for (const char ch : value) {
    switch (ch) {
      case '\\': stream << "\\\\"; break;
      case '"': stream << "\\\""; break;
      case '\n': stream << "\\n"; break;
      case '\r': stream << "\\r"; break;
      case '\t': stream << "\\t"; break;
      default: stream << ch; break;
    }
  }
  stream << '"';
  return stream.str();
}

bool IsOpaqueReference(const std::string& ref) {
  if (ref.empty()) return false;
  const fs::path path(ref);
  if (path.is_absolute()) return false;
  const fs::path normalized = path.lexically_normal();
  for (const auto& part : normalized) {
    if (part == "..") return false;
  }
  return normalized != fs::path(".");
}

bool ResolveRegisteredReference(const std::string& root,
                                const std::string& ref,
                                fs::path& resolved,
                                std::string& reason) {
  if (root.empty() || !IsOpaqueReference(ref)) {
    reason = "BUSINESS_REFERENCE_INVALID";
    return false;
  }
  std::error_code error;
  const fs::path root_path = fs::weakly_canonical(fs::path(root), error);
  if (error || !fs::is_directory(root_path, error)) {
    reason = "BUSINESS_ROOT_UNAVAILABLE";
    return false;
  }
  const fs::path candidate = fs::weakly_canonical(root_path / fs::path(ref), error);
  if (error || !fs::is_regular_file(candidate, error)) {
    reason = "BUSINESS_ASSET_MISSING";
    return false;
  }
  const auto root_text = root_path.generic_string();
  const auto candidate_text = candidate.generic_string();
  if (candidate_text.size() < root_text.size() ||
      candidate_text.compare(0, root_text.size(), root_text) != 0 ||
      (candidate_text.size() > root_text.size() &&
       candidate_text[root_text.size()] != '/')) {
    reason = "BUSINESS_REFERENCE_ESCAPE";
    return false;
  }
  resolved = candidate;
  return true;
}

class Sha256 {
 public:
  Sha256() { Reset(); }
  void Update(const unsigned char* data, std::size_t size) {
    for (std::size_t index = 0; index < size; ++index) {
      buffer_[buffer_size_++] = data[index];
      if (buffer_size_ == 64) {
        Transform();
        bit_length_ += 512;
        buffer_size_ = 0;
      }
    }
  }
  std::string FinalHex() {
    const std::uint64_t total_bits = bit_length_ + buffer_size_ * 8;
    buffer_[buffer_size_++] = 0x80;
    if (buffer_size_ > 56) {
      while (buffer_size_ < 64) buffer_[buffer_size_++] = 0;
      Transform();
      buffer_size_ = 0;
    }
    while (buffer_size_ < 56) buffer_[buffer_size_++] = 0;
    for (int index = 7; index >= 0; --index)
      buffer_[buffer_size_++] = static_cast<unsigned char>((total_bits >> (index * 8)) & 0xff);
    Transform();
    std::ostringstream result;
    for (std::uint32_t value : state_)
      result << std::hex << std::setw(8) << std::setfill('0') << value;
    return result.str();
  }
 private:
  static std::uint32_t RotateRight(std::uint32_t value, std::uint32_t bits) {
    return (value >> bits) | (value << (32 - bits));
  }
  void Reset() {
    state_ = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
              0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    buffer_size_ = 0;
    bit_length_ = 0;
  }
  void Transform() {
    static constexpr std::array<std::uint32_t, 64> k = {
      0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
      0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
      0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
      0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
      0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
      0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
      0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
      0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
    std::array<std::uint32_t, 64> words{};
    for (int index = 0; index < 16; ++index) {
      words[index] = (static_cast<std::uint32_t>(buffer_[index * 4]) << 24) |
                     (static_cast<std::uint32_t>(buffer_[index * 4 + 1]) << 16) |
                     (static_cast<std::uint32_t>(buffer_[index * 4 + 2]) << 8) |
                     static_cast<std::uint32_t>(buffer_[index * 4 + 3]);
    }
    for (int index = 16; index < 64; ++index) {
      const std::uint32_t s0 = RotateRight(words[index - 15], 7) ^ RotateRight(words[index - 15], 18) ^ (words[index - 15] >> 3);
      const std::uint32_t s1 = RotateRight(words[index - 2], 17) ^ RotateRight(words[index - 2], 19) ^ (words[index - 2] >> 10);
      words[index] = words[index - 16] + s0 + words[index - 7] + s1;
    }
    std::uint32_t a=state_[0],b=state_[1],c=state_[2],d=state_[3],e=state_[4],f=state_[5],g=state_[6],h=state_[7];
    for (int index = 0; index < 64; ++index) {
      const std::uint32_t s1 = RotateRight(e,6)^RotateRight(e,11)^RotateRight(e,25);
      const std::uint32_t choice = (e & f) ^ ((~e) & g);
      const std::uint32_t temp1 = h + s1 + choice + k[index] + words[index];
      const std::uint32_t s0 = RotateRight(a,2)^RotateRight(a,13)^RotateRight(a,22);
      const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
      const std::uint32_t temp2 = s0 + majority;
      h=g; g=f; f=e; e=d+temp1; d=c; c=b; b=a; a=temp1+temp2;
    }
    state_[0]+=a; state_[1]+=b; state_[2]+=c; state_[3]+=d;
    state_[4]+=e; state_[5]+=f; state_[6]+=g; state_[7]+=h;
  }
  std::array<std::uint32_t, 8> state_{};
  std::array<unsigned char, 64> buffer_{};
  std::size_t buffer_size_ = 0;
  std::uint64_t bit_length_ = 0;
};

bool Sha256File(const fs::path& path, std::string& hash) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  Sha256 sha;
  std::array<unsigned char, 8192> buffer{};
  while (input.good()) {
    input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    const std::streamsize count = input.gcount();
    if (count > 0) sha.Update(buffer.data(), static_cast<std::size_t>(count));
  }
  hash = sha.FinalHex();
  return true;
}

bool IsSha256(const std::string& hash) {
  return hash.size() == 64 &&
      std::all_of(hash.begin(), hash.end(), [](const unsigned char ch) {
        return std::isxdigit(ch) != 0;
      });
}

bool VerifyDigest(const fs::path& path, const std::string& expected) {
  std::string actual;
  return IsSha256(expected) && Sha256File(path, actual) && actual == expected;
}

std::string ExtractJsonString(const std::string& source, const std::string& key) {
  const std::string marker = "\"" + key + "\"";
  std::size_t position = source.find(marker);
  if (position == std::string::npos) return {};
  position = source.find(':', position + marker.size());
  if (position == std::string::npos) return {};
  position = source.find('"', position + 1);
  if (position == std::string::npos) return {};
  const std::size_t end = source.find('"', position + 1);
  return end == std::string::npos ? std::string() : source.substr(position + 1, end - position - 1);
}

std::size_t Occurrences(const std::string& text, const std::string& item) {
  std::size_t result = 0, position = 0;
  while ((position = text.find(item, position)) != std::string::npos) {
    ++result;
    position += item.size();
  }
  return result;
}

bool ValidateBusinessDataset(const fs::path& manifest_path,
                             std::string& first_image_ref,
                             std::string& reason) {
  std::ifstream input(manifest_path, std::ios::binary);
  const std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  if (text.find("cxvision.torch.training_dataset.v2") == std::string::npos) {
    reason = "BUSINESS_DATASET_SCHEMA_INVALID";
    return false;
  }
  const std::size_t train = Occurrences(text, "\"split\":\"train\"") +
                            Occurrences(text, "\"split\": \"train\"");
  const std::size_t validation = Occurrences(text, "\"split\":\"val\"") +
                                 Occurrences(text, "\"split\": \"val\"");
  const std::size_t verify = Occurrences(text, "\"split\":\"verify\"") +
                             Occurrences(text, "\"split\": \"verify\"");
  if (train < 2 || validation < 1 || verify < 1) {
    reason = "BUSINESS_DATASET_SPLIT_INCOMPLETE";
    return false;
  }
  if (Occurrences(text, "\"annotation_status\":\"UNLABELED\"") +
          Occurrences(text, "\"annotation_status\": \"UNLABELED\"") < verify) {
    reason = "BUSINESS_VERIFY_MUST_BE_UNLABELED";
    return false;
  }
  first_image_ref = ExtractJsonString(text, "image_path");
  if (!IsOpaqueReference(first_image_ref)) {
    reason = "BUSINESS_DATASET_IMAGE_REFERENCE_INVALID";
    return false;
  }
  return true;
}

void SetBusinessReceipt(TorchBusinessReceiptV1* receipt, bool ok, int code,
                        const std::string& status, const std::string& json) {
  receipt->ok = ok ? 1 : 0;
  receipt->error_code = code;
  CopyString(status, receipt->status);
  CopyString(json, receipt->receipt_json);
}

int FailBusinessReceipt(TorchBusinessReceiptV1* receipt,
                        int code, const std::string& status) {
  SetBusinessReceipt(receipt, false, code, status,
      "{\"schema\":\"visionai.business_receipt.v1\",\"status\":" +
      JsonQuote(status) + ",\"contains_image_data\":false}");
  return code;
}

bool ValidRequired(const char* value) {
  return value != nullptr && *value != '\0';
}

std::string MakeBusinessExtra(const TorchBusinessTrialRequestV1& request,
                              const fs::path& parent_path) {
  std::ostringstream stream;
  stream << "{\"parent_weights\":" << JsonQuote(parent_path.string())
         << ",\"target_class_id\":" << request.geometry_class_id
         << ",\"epochs\":" << request.epochs << "}";
  return stream.str();
}

} // namespace

TORCH_RUNTIME_API int torch_runtime_create(const TorchRuntimeConfig* config,
                                           TorchRuntimeHandle* out_handle) {
  if (!config || !out_handle) return -1;
  try {
    auto* impl = new TorchRuntimeImpl();
    impl->config.model_root = StringValue(config->model_root);
    impl->config.output_root = StringValue(config->output_root);
    impl->config.device = StringValue(config->device);
    impl->config.log_level = StringValue(config->log_level);
    impl->config.business_asset_root = StringValue(config->business_asset_root);
    impl->config.business_trial_root = StringValue(config->business_trial_root);
    *out_handle = impl;
    return 0;
  } catch (...) {
    return -1;
  }
}

TORCH_RUNTIME_API int torch_runtime_destroy(TorchRuntimeHandle handle) {
  if (!handle) return -1;
  delete static_cast<TorchRuntimeImpl*>(handle);
  return 0;
}

TORCH_RUNTIME_API int torch_runtime_run_task(TorchRuntimeHandle handle,
                                             const TorchTaskRequest* request,
                                             TorchTaskResult* out_result) {
  if (!handle || !request || !out_result) return -1;
  std::memset(out_result, 0, sizeof(*out_result));
  try {
    TorchRuntimeImpl* impl = static_cast<TorchRuntimeImpl*>(handle);
    TorchTaskRequestCpp cpp;
    cpp.task = StringValue(request->task); cpp.device = StringValue(request->device);
    cpp.input_image = StringValue(request->input_image); cpp.dataset_root = StringValue(request->dataset_root);
    cpp.manifest_path = StringValue(request->manifest_path); cpp.case_name = StringValue(request->case_name);
    cpp.extra_json = StringValue(request->extra_json); cpp.output_dir = StringValue(request->output_dir);
    const TorchTaskResultCpp result = RunTorchTask(impl->config, cpp);
    out_result->ok = result.ok ? 1 : 0; out_result->error_code = result.error_code;
    out_result->train_runtime_ms = result.train_runtime_ms; out_result->infer_runtime_ms = result.infer_runtime_ms;
    out_result->algorithm_runtime_ms = result.algorithm_runtime_ms; out_result->placeholder_runtime_ms = result.placeholder_runtime_ms;
    CopyString(result.status, out_result->status); CopyString(result.error_message, out_result->error_message);
    CopyString(result.requested_device, out_result->requested_device); CopyString(result.actual_device, out_result->actual_device);
    CopyString(result.result_json, out_result->result_json); CopyString(result.evidence_ref, out_result->evidence_ref); CopyString(result.result_ref, out_result->result_ref);
    CopyString(result.input_image_ref, out_result->input_image_ref); CopyString(result.primary_visual_ref, out_result->primary_visual_ref); CopyString(result.visualization_refs, out_result->visualization_refs);
    CopyString(result.bbox_candidate_list_ref, out_result->bbox_candidate_list_ref); CopyString(result.roi_crop_packet_ref, out_result->roi_crop_packet_ref);
    CopyString(result.attach_back_ref, out_result->attach_back_ref); CopyString(result.template_alignment_ref, out_result->template_alignment_ref);
    CopyString(result.roi_diff_candidate_ref, out_result->roi_diff_candidate_ref); CopyString(result.trainer_lifecycle_summary, out_result->trainer_lifecycle_summary);
    CopyString(result.unified_mainline_summary, out_result->unified_mainline_summary);
    return 0;
  } catch (...) {
    return -1;
  }
}

TORCH_RUNTIME_API void torch_runtime_free_result(TorchTaskResult* result) {
  if (!result) return;
  FreeString(result->status); FreeString(result->error_message); FreeString(result->requested_device); FreeString(result->actual_device);
  FreeString(result->result_json); FreeString(result->evidence_ref); FreeString(result->result_ref); FreeString(result->input_image_ref);
  FreeString(result->primary_visual_ref); FreeString(result->visualization_refs); FreeString(result->bbox_candidate_list_ref);
  FreeString(result->roi_crop_packet_ref); FreeString(result->attach_back_ref); FreeString(result->template_alignment_ref);
  FreeString(result->roi_diff_candidate_ref); FreeString(result->trainer_lifecycle_summary); FreeString(result->unified_mainline_summary);
  std::memset(result, 0, sizeof(*result));
}

TORCH_RUNTIME_API int torch_runtime_run_business_trial_v1(
    TorchRuntimeHandle handle, const TorchBusinessTrialRequestV1* request,
    TorchBusinessReceiptV1* receipt) {
  if (!receipt) return -1;
  std::memset(receipt, 0, sizeof(*receipt));
  if (!handle || !request) return FailBusinessReceipt(receipt, 1001, "INVALID_REQUEST");
  if (!ValidRequired(request->capability_id) ||
      std::string(request->capability_id) != TorchRuntimeTaskIds::SegmentationBusinessIncremental)
    return FailBusinessReceipt(receipt, 1002, "CAPABILITY_UNAVAILABLE");
  if (!ValidRequired(request->project_id) || !ValidRequired(request->case_id) ||
      !ValidRequired(request->dataset_revision) || !ValidRequired(request->trial_id) ||
      request->geometry_class_id < 0 || request->geometry_class_id > 6 ||
      request->epochs < 2 || request->epochs > 200)
    return FailBusinessReceipt(receipt, 1001, "BUSINESS_CONTRACT_INVALID");

  auto* impl = static_cast<TorchRuntimeImpl*>(handle);
  fs::path dataset_manifest, parent_weights, input_image;
  std::string reason, first_image_ref;
  if (!ResolveRegisteredReference(impl->config.business_asset_root, StringValue(request->dataset_manifest_ref), dataset_manifest, reason))
    return FailBusinessReceipt(receipt, 1201, reason);
  if (!VerifyDigest(dataset_manifest, StringValue(request->dataset_sha256)))
    return FailBusinessReceipt(receipt, 1202, "DATASET_SHA256_MISMATCH");
  if (!ValidateBusinessDataset(dataset_manifest, first_image_ref, reason))
    return FailBusinessReceipt(receipt, 1203, reason);
  if (!ResolveRegisteredReference(impl->config.business_asset_root, first_image_ref, input_image, reason))
    return FailBusinessReceipt(receipt, 1101, reason);
  if (!ResolveRegisteredReference(impl->config.model_root, StringValue(request->parent_model_ref), parent_weights, reason))
    return FailBusinessReceipt(receipt, 1301, reason);
  if (!VerifyDigest(parent_weights, StringValue(request->parent_model_sha256)))
    return FailBusinessReceipt(receipt, 1302, "PARENT_MODEL_SHA256_MISMATCH");
  if (impl->config.business_trial_root.empty() || !IsOpaqueReference(request->trial_id))
    return FailBusinessReceipt(receipt, 1801, "BUSINESS_TRIAL_ROOT_INVALID");

  const fs::path output_dir = fs::path(impl->config.business_trial_root) / "development_trials" / request->trial_id;
  TorchTaskRequestCpp task;
  task.task = TorchRuntimeTaskIds::SegmentationBusinessIncremental;
  task.device = impl->config.device;
  task.input_image = input_image.string();
  task.dataset_root = impl->config.business_asset_root;
  task.manifest_path = dataset_manifest.string();
  task.case_name = request->case_id;
  task.extra_json = MakeBusinessExtra(*request, parent_weights);
  task.output_dir = output_dir.string();
  const TorchTaskResultCpp result = RunTorchTask(impl->config, task);

  const fs::path candidate = output_dir / "deeplab_incremental_model" / "weights" / "deeplab_incremental.pt";
  std::string candidate_sha;
  const bool artifact_available = result.ok && Sha256File(candidate, candidate_sha);
  std::ostringstream json;
  json << "{\"schema\":\"visionai.business_trial_receipt.v1\","
       << "\"trial_id\":" << JsonQuote(request->trial_id) << ","
       << "\"project_id\":" << JsonQuote(request->project_id) << ","
       << "\"case_id\":" << JsonQuote(request->case_id) << ","
       << "\"dataset_revision\":" << JsonQuote(request->dataset_revision) << ","
       << "\"geometry_class_id\":" << request->geometry_class_id << ","
       << "\"status\":" << JsonQuote(result.ok && artifact_available ? "CANDIDATE" : "FAILED") << ","
       << "\"train_runtime_ms\":" << result.train_runtime_ms << ","
       << "\"candidate_artifact_sha256\":" << JsonQuote(candidate_sha) << ","
       << "\"contains_image_data\":false}";
  SetBusinessReceipt(receipt, result.ok && artifact_available,
                     result.ok && artifact_available ? 0 : result.error_code,
                     result.ok && artifact_available ? "CANDIDATE" : "FAILED", json.str());
  return receipt->ok ? 0 : receipt->error_code;
}

TORCH_RUNTIME_API int torch_runtime_run_business_inference_v1(
    TorchRuntimeHandle handle, const TorchBusinessInferenceRequestV1* request,
    TorchBusinessReceiptV1* receipt) {
  if (!receipt) return -1;
  std::memset(receipt, 0, sizeof(*receipt));
  if (!handle || !request) return FailBusinessReceipt(receipt, 1001, "INVALID_REQUEST");
  if (!ValidRequired(request->capability_id) ||
      std::string(request->capability_id) != TorchRuntimeTaskIds::SegmentationBusinessIsolatedInference)
    return FailBusinessReceipt(receipt, 1002, "CAPABILITY_UNAVAILABLE");
  if (!ValidRequired(request->project_id) || !ValidRequired(request->case_id) ||
      !ValidRequired(request->dataset_revision) || !ValidRequired(request->request_id))
    return FailBusinessReceipt(receipt, 1001, "BUSINESS_CONTRACT_INVALID");

  auto* impl = static_cast<TorchRuntimeImpl*>(handle);
  fs::path candidate_manifest, asset;
  std::string reason;
  if (!ResolveRegisteredReference(impl->config.business_trial_root, StringValue(request->candidate_manifest_ref), candidate_manifest, reason))
    return FailBusinessReceipt(receipt, 1301, reason);
  if (!VerifyDigest(candidate_manifest, StringValue(request->candidate_manifest_sha256)))
    return FailBusinessReceipt(receipt, 1302, "CANDIDATE_MANIFEST_SHA256_MISMATCH");
  if (!ResolveRegisteredReference(impl->config.business_asset_root, StringValue(request->asset_ref), asset, reason))
    return FailBusinessReceipt(receipt, 1101, reason);
  if (!VerifyDigest(asset, StringValue(request->asset_sha256)))
    return FailBusinessReceipt(receipt, 1102, "VERIFY_ASSET_SHA256_MISMATCH");
  if (!IsOpaqueReference(request->request_id))
    return FailBusinessReceipt(receipt, 1801, "ISOLATED_REQUEST_ID_INVALID");

  TorchRuntimeCoreConfig inference_config = impl->config;
  inference_config.model_root = candidate_manifest.parent_path().string();
  TorchTaskRequestCpp task;
  task.task = TorchRuntimeTaskIds::DeepLabV3PlusSegmentation;
  task.device = impl->config.device;
  task.input_image = asset.string();
  task.manifest_path = candidate_manifest.string();
  task.case_name = request->case_id;
  task.output_dir = (fs::path(impl->config.business_trial_root) / "isolated_business_validation" / request->request_id).string();
  const TorchTaskResultCpp result = RunTorchTask(inference_config, task);

  std::ostringstream json;
  json << "{\"schema\":\"visionai.business_inference_receipt.v1\","
       << "\"request_id\":" << JsonQuote(request->request_id) << ","
       << "\"project_id\":" << JsonQuote(request->project_id) << ","
       << "\"case_id\":" << JsonQuote(request->case_id) << ","
       << "\"dataset_revision\":" << JsonQuote(request->dataset_revision) << ","
       << "\"status\":" << JsonQuote(result.ok ? "COMPLETED" : "FAILED") << ","
       << "\"infer_runtime_ms\":" << result.infer_runtime_ms << ","
       << "\"contains_image_data\":false}";
  SetBusinessReceipt(receipt, result.ok, result.error_code,
                     result.ok ? "COMPLETED" : "FAILED", json.str());
  return receipt->ok ? 0 : receipt->error_code;
}

TORCH_RUNTIME_API void torch_runtime_free_business_receipt_v1(TorchBusinessReceiptV1* receipt) {
  if (!receipt) return;
  FreeString(receipt->status);
  FreeString(receipt->receipt_json);
  std::memset(receipt, 0, sizeof(*receipt));
}

TORCH_RUNTIME_API const char* torch_runtime_version() {
  static const char version[] = "libtorch_module_runtime 1.1.0-business-v1";
  return version;
}
