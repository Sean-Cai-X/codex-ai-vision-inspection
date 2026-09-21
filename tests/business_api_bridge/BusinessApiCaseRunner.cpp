#include "contracts/BusinessWorkflowApi.h"

#include "libtorchsegmentation/src/utils/json.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;
namespace business = vision_ai::business::v1;
using json = nlohmann::json;

constexpr const char* kCaseSchema = "visionai.business_api_case.v1";
constexpr const char* kApiId = "vision_ai.business.v1";
constexpr const char* kTestInterfaceId =
    "vision_ai.business.v1.IBusinessWorkflowTestApi";
constexpr const char* kExecutionClaim = "COMPILED_BUSINESS_API_EXECUTION";
constexpr const char* kBuildIdSource =
    "IBusinessWorkflowApi::ReadBuildIdentity";
constexpr const char* kBuildIdVerificationStatus = "VERIFIED_BY_SDK";
constexpr std::uintmax_t kMaximumJsonBytes = 16U * 1024U * 1024U;

class CaseError final : public std::runtime_error
{
public:
  using std::runtime_error::runtime_error;
};

struct Options
{
  fs::path case_root;
  fs::path output_root;
  std::string run_id;
  std::string implementation_build_id;
  std::string requested_implementation_build_id;
  json sdk_build_identity = json::object();
};

struct AssertionRecord
{
  std::string field;
  json expected;
  json actual;
  bool passed = false;
  std::string detail;
};

struct ExecutionRecord
{
  std::string phase;
  std::size_t ordinal = 0;
  std::string started_at;
  std::string completed_at;
  bool api_invoked = false;
  bool passed = false;
  std::string request_digest;
  json request;
  json response;
  json snapshot_before;
  json snapshot_after;
  std::vector<AssertionRecord> assertions;
  std::string error;
};

struct ScenarioRecord
{
  std::string scenario_id;
  std::string kind;
  std::vector<std::string> cross_cutting_tags;
  bool passed = false;
  std::vector<ExecutionRecord> setup;
  ExecutionRecord primary;
  std::vector<ExecutionRecord> follow_up;
  std::vector<std::string> errors;
};

struct CaseRecord
{
  fs::path source_path;
  std::string case_id;
  std::string display_name;
  std::string primary_capability_id;
  std::string request_type;
  bool schema_valid = false;
  bool success_present = false;
  bool success_passed = false;
  bool boundary_present = false;
  bool boundary_passed = false;
  bool passed = false;
  std::vector<ScenarioRecord> scenarios;
  std::vector<std::string> errors;
};

struct CatalogEntry
{
  std::string capability_id;
  std::string request_type;
  std::string implementation_symbol;
  std::string case_id;
  fs::path case_ref;
};

std::string UtcTimestamp()
{
  const auto now = std::chrono::system_clock::now();
  const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()) % 1000;
  const std::time_t value = std::chrono::system_clock::to_time_t(now);
  std::tm utc{};
#ifdef _WIN32
  gmtime_s(&utc, &value);
#else
  gmtime_r(&value, &utc);
#endif
  std::ostringstream stream;
  stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%S") << '.'
         << std::setw(3) << std::setfill('0') << milliseconds.count() << 'Z';
  return stream.str();
}

std::string LowerAscii(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

bool PathPartEqual(const fs::path& left, const fs::path& right)
{
#ifdef _WIN32
  return LowerAscii(left.generic_string()) == LowerAscii(right.generic_string());
#else
  return left == right;
#endif
}

bool IsStrictDescendant(const fs::path& child, const fs::path& parent)
{
  auto child_it = child.begin();
  auto parent_it = parent.begin();
  for (; parent_it != parent.end(); ++parent_it, ++child_it) {
    if (child_it == child.end() || !PathPartEqual(*child_it, *parent_it)) {
      return false;
    }
  }
  return child_it != child.end();
}

fs::path FindCxscriptRunsAncestor(fs::path path)
{
  while (!path.empty()) {
    if (LowerAscii(path.filename().string()) == "cxscript_runs") {
      return path;
    }
    const fs::path parent = path.parent_path();
    if (parent == path) {
      break;
    }
    path = parent;
  }
  return {};
}

std::string ReadTextFile(const fs::path& path)
{
  std::error_code error;
  const std::uintmax_t size = fs::file_size(path, error);
  if (error) {
    throw CaseError("Cannot read file size: " + path.string() + ": " + error.message());
  }
  if (size > kMaximumJsonBytes) {
    throw CaseError("JSON file exceeds 16 MiB: " + path.string());
  }
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw CaseError("Cannot open file: " + path.string());
  }
  std::ostringstream content;
  content << stream.rdbuf();
  if (!stream.good() && !stream.eof()) {
    throw CaseError("Cannot read file: " + path.string());
  }
  return content.str();
}

json ReadJsonFile(const fs::path& path)
{
  try {
    return json::parse(ReadTextFile(path));
  } catch (const CaseError&) {
    throw;
  } catch (const std::exception& exception) {
    throw CaseError("Cannot parse JSON " + path.string() + ": " + exception.what());
  }
}

void WriteTextFile(const fs::path& path, const std::string& content)
{
  std::error_code existence_error;
  if (fs::exists(path, existence_error) || existence_error) {
    throw CaseError("Refusing to overwrite evidence file: " + path.string());
  }
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    throw CaseError("Cannot create evidence file: " + path.string());
  }
  stream.write(content.data(), static_cast<std::streamsize>(content.size()));
  stream.flush();
  if (!stream) {
    throw CaseError("Cannot write evidence file: " + path.string());
  }
}

std::uint64_t Fnv1a64(const std::string& text)
{
  std::uint64_t value = UINT64_C(14695981039346656037);
  for (const unsigned char ch : text) {
    value ^= static_cast<std::uint64_t>(ch);
    value *= UINT64_C(1099511628211);
  }
  return value;
}

std::string DigestJson(const json& document)
{
  std::ostringstream stream;
  stream << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0')
         << Fnv1a64(document.dump());
  return stream.str();
}

std::string SafeDirectoryName(const std::string& value)
{
  std::string result;
  result.reserve(value.size());
  for (const unsigned char ch : value) {
    if (std::isalnum(ch) != 0) {
      result.push_back(static_cast<char>(std::tolower(ch)));
    } else if (result.empty() || result.back() != '_') {
      result.push_back('_');
    }
  }
  while (!result.empty() && result.back() == '_') {
    result.pop_back();
  }
  if (result.empty()) {
    result = "case";
  }
  return result;
}

template <typename T>
T Required(const json& object, const char* key, const std::string& context)
{
  if (!object.is_object() || !object.contains(key)) {
    throw CaseError(context + " requires '" + key + "'.");
  }
  try {
    return object.at(key).get<T>();
  } catch (const std::exception& exception) {
    throw CaseError(context + "." + key + " has the wrong type: " + exception.what());
  }
}

template <typename T>
T Optional(const json& object, const char* key, T fallback,
           const std::string& context)
{
  if (!object.is_object() || !object.contains(key)) {
    return fallback;
  }
  try {
    return object.at(key).get<T>();
  } catch (const std::exception& exception) {
    throw CaseError(context + "." + key + " has the wrong type: " + exception.what());
  }
}

template <typename Enum>
Enum ParseEnum(const std::string& value,
               const std::map<std::string, Enum>& values,
               const std::string& context)
{
  const auto found = values.find(value);
  if (found == values.end()) {
    throw CaseError(context + " has unknown enum value '" + value + "'.");
  }
  return found->second;
}

business::InspectionKind ParseInspectionKind(const std::string& value,
                                             const std::string& context)
{
  static const std::map<std::string, business::InspectionKind> values = {
      {"Classification", business::InspectionKind::Classification},
      {"Detection", business::InspectionKind::Detection},
      {"Segmentation", business::InspectionKind::Segmentation},
      {"Measurement", business::InspectionKind::Measurement},
      {"Presence", business::InspectionKind::Presence},
      {"Anomaly", business::InspectionKind::Anomaly}};
  return ParseEnum(value, values, context);
}

business::ImagePurpose ParseImagePurpose(const std::string& value,
                                         const std::string& context)
{
  static const std::map<std::string, business::ImagePurpose> values = {
      {"Training", business::ImagePurpose::Training},
      {"Validation", business::ImagePurpose::Validation},
      {"Test", business::ImagePurpose::Test},
      {"Production", business::ImagePurpose::Production},
      {"Reference", business::ImagePurpose::Reference}};
  return ParseEnum(value, values, context);
}

business::ImageSourceType ParseImageSourceType(const std::string& value,
                                               const std::string& context)
{
  static const std::map<std::string, business::ImageSourceType> values = {
      {"File", business::ImageSourceType::File},
      {"Camera", business::ImageSourceType::Camera},
      {"Stream", business::ImageSourceType::Stream},
      {"Generated", business::ImageSourceType::Generated},
      {"Provider", business::ImageSourceType::Provider}};
  return ParseEnum(value, values, context);
}

business::AcquisitionMethod ParseAcquisitionMethod(const std::string& value,
                                                   const std::string& context)
{
  static const std::map<std::string, business::AcquisitionMethod> values = {
      {"Upload", business::AcquisitionMethod::Upload},
      {"Capture", business::AcquisitionMethod::Capture},
      {"Import", business::AcquisitionMethod::Import},
      {"Sample", business::AcquisitionMethod::Sample},
      {"Provider", business::AcquisitionMethod::Provider}};
  return ParseEnum(value, values, context);
}

business::GeometryKind ParseGeometryKind(const std::string& value,
                                         const std::string& context)
{
  static const std::map<std::string, business::GeometryKind> values = {
      {"Rectangle", business::GeometryKind::Rectangle},
      {"Polygon", business::GeometryKind::Polygon},
      {"Polyline", business::GeometryKind::Polyline},
      {"Point", business::GeometryKind::Point},
      {"Mask", business::GeometryKind::Mask},
      {"Segment", business::GeometryKind::Segment},
      {"EdgeCandidate", business::GeometryKind::EdgeCandidate}};
  return ParseEnum(value, values, context);
}

business::CreationMethod ParseCreationMethod(const std::string& value,
                                             const std::string& context)
{
  static const std::map<std::string, business::CreationMethod> values = {
      {"Manual", business::CreationMethod::Manual},
      {"Imported", business::CreationMethod::Imported},
      {"Suggested", business::CreationMethod::Suggested},
      {"Provider", business::CreationMethod::Provider},
      {"Corrected", business::CreationMethod::Corrected}};
  return ParseEnum(value, values, context);
}

business::AnnotationStatus ParseAnnotationStatus(const std::string& value,
                                                 const std::string& context)
{
  static const std::map<std::string, business::AnnotationStatus> values = {
      {"Candidate", business::AnnotationStatus::Candidate},
      {"Confirmed", business::AnnotationStatus::Confirmed},
      {"Rejected", business::AnnotationStatus::Rejected},
      {"Superseded", business::AnnotationStatus::Superseded}};
  return ParseEnum(value, values, context);
}

business::ObservationStatus ParseObservationStatus(const std::string& value,
                                                   const std::string& context)
{
  static const std::map<std::string, business::ObservationStatus> values = {
      {"NotRequested", business::ObservationStatus::NotRequested},
      {"Pending", business::ObservationStatus::Pending},
      {"Completed", business::ObservationStatus::Completed},
      {"NotAvailable", business::ObservationStatus::NotAvailable},
      {"Failed", business::ObservationStatus::Failed}};
  return ParseEnum(value, values, context);
}

business::ReviewAction ParseReviewAction(const std::string& value,
                                         const std::string& context)
{
  static const std::map<std::string, business::ReviewAction> values = {
      {"Accepted", business::ReviewAction::Accepted},
      {"Rejected", business::ReviewAction::Rejected},
      {"Corrected", business::ReviewAction::Corrected}};
  return ParseEnum(value, values, context);
}

business::ProductionOutcome ParseProductionOutcome(const std::string& value,
                                                   const std::string& context)
{
  static const std::map<std::string, business::ProductionOutcome> values = {
      {"Pass", business::ProductionOutcome::Pass},
      {"Fail", business::ProductionOutcome::Fail},
      {"ReviewRequired", business::ProductionOutcome::ReviewRequired}};
  return ParseEnum(value, values, context);
}

const char* ToString(business::ExecutionStatus value)
{
  switch (value) {
    case business::ExecutionStatus::Succeeded: return "Succeeded";
    case business::ExecutionStatus::Pending: return "Pending";
    case business::ExecutionStatus::NotAvailable: return "NotAvailable";
    case business::ExecutionStatus::Rejected: return "Rejected";
    case business::ExecutionStatus::Conflict: return "Conflict";
    case business::ExecutionStatus::NotFound: return "NotFound";
  }
  return "Unknown";
}

const char* ToString(business::InspectionKind value)
{
  switch (value) {
    case business::InspectionKind::Classification: return "Classification";
    case business::InspectionKind::Detection: return "Detection";
    case business::InspectionKind::Segmentation: return "Segmentation";
    case business::InspectionKind::Measurement: return "Measurement";
    case business::InspectionKind::Presence: return "Presence";
    case business::InspectionKind::Anomaly: return "Anomaly";
  }
  return "Unknown";
}

const char* ToString(business::ImageSourceType value)
{
  switch (value) {
    case business::ImageSourceType::File: return "File";
    case business::ImageSourceType::Camera: return "Camera";
    case business::ImageSourceType::Stream: return "Stream";
    case business::ImageSourceType::Generated: return "Generated";
    case business::ImageSourceType::Provider: return "Provider";
  }
  return "Unknown";
}

const char* ToString(business::ImagePurpose value)
{
  switch (value) {
    case business::ImagePurpose::Training: return "Training";
    case business::ImagePurpose::Validation: return "Validation";
    case business::ImagePurpose::Test: return "Test";
    case business::ImagePurpose::Production: return "Production";
    case business::ImagePurpose::Reference: return "Reference";
  }
  return "Unknown";
}

const char* ToString(business::AcquisitionMethod value)
{
  switch (value) {
    case business::AcquisitionMethod::Upload: return "Upload";
    case business::AcquisitionMethod::Capture: return "Capture";
    case business::AcquisitionMethod::Import: return "Import";
    case business::AcquisitionMethod::Sample: return "Sample";
    case business::AcquisitionMethod::Provider: return "Provider";
  }
  return "Unknown";
}

const char* ToString(business::GeometryKind value)
{
  switch (value) {
    case business::GeometryKind::Rectangle: return "Rectangle";
    case business::GeometryKind::Polygon: return "Polygon";
    case business::GeometryKind::Polyline: return "Polyline";
    case business::GeometryKind::Point: return "Point";
    case business::GeometryKind::Mask: return "Mask";
    case business::GeometryKind::Segment: return "Segment";
    case business::GeometryKind::EdgeCandidate: return "EdgeCandidate";
  }
  return "Unknown";
}

const char* ToString(business::CreationMethod value)
{
  switch (value) {
    case business::CreationMethod::Manual: return "Manual";
    case business::CreationMethod::Imported: return "Imported";
    case business::CreationMethod::Suggested: return "Suggested";
    case business::CreationMethod::Provider: return "Provider";
    case business::CreationMethod::Corrected: return "Corrected";
  }
  return "Unknown";
}

const char* ToString(business::AnnotationStatus value)
{
  switch (value) {
    case business::AnnotationStatus::Candidate: return "Candidate";
    case business::AnnotationStatus::Confirmed: return "Confirmed";
    case business::AnnotationStatus::Rejected: return "Rejected";
    case business::AnnotationStatus::Superseded: return "Superseded";
  }
  return "Unknown";
}

const char* ToString(business::ObservationStatus value)
{
  switch (value) {
    case business::ObservationStatus::NotRequested: return "NotRequested";
    case business::ObservationStatus::Pending: return "Pending";
    case business::ObservationStatus::Completed: return "Completed";
    case business::ObservationStatus::NotAvailable: return "NotAvailable";
    case business::ObservationStatus::Failed: return "Failed";
  }
  return "Unknown";
}

const char* ToString(business::ReviewAction value)
{
  switch (value) {
    case business::ReviewAction::Accepted: return "Accepted";
    case business::ReviewAction::Rejected: return "Rejected";
    case business::ReviewAction::Corrected: return "Corrected";
  }
  return "Unknown";
}

const char* ToString(business::ProductionOutcome value)
{
  switch (value) {
    case business::ProductionOutcome::Pass: return "Pass";
    case business::ProductionOutcome::Fail: return "Fail";
    case business::ProductionOutcome::ReviewRequired: return "ReviewRequired";
  }
  return "Unknown";
}

business::Geometry ParseGeometry(const json& document, const std::string& context)
{
  if (!document.is_object()) {
    throw CaseError(context + " must be an object.");
  }
  business::Geometry geometry;
  geometry.kind = ParseGeometryKind(Required<std::string>(document, "kind", context),
                                    context + ".kind");
  geometry.coordinates = Optional<std::vector<double>>(document, "coordinates", {}, context);
  geometry.coordinate_space = Optional<std::string>(document, "coordinate_space", "", context);
  geometry.mask_ref = Optional<std::string>(document, "mask_ref", "", context);
  return geometry;
}

business::ObservationChannel ParseObservation(const json& document,
                                              const std::string& context)
{
  if (!document.is_object()) {
    throw CaseError(context + " must be an object.");
  }
  business::ObservationChannel observation;
  observation.status = ParseObservationStatus(
      Required<std::string>(document, "status", context), context + ".status");
  observation.provider_id = Optional<std::string>(document, "provider_id", "", context);
  observation.code = Optional<std::string>(document, "code", "", context);
  observation.detail = Optional<std::string>(document, "detail", "", context);
  observation.evidence_refs =
      Optional<std::vector<std::string>>(document, "evidence_refs", {}, context);
  return observation;
}

business::DetectionElement ParseDetectionElement(const json& document,
                                                 const std::string& context)
{
  if (!document.is_object()) {
    throw CaseError(context + " must be an object.");
  }
  business::DetectionElement element;
  element.element_id = Required<std::string>(document, "element_id", context);
  element.label = Required<std::string>(document, "label", context);
  element.score = Required<double>(document, "score", context);
  if (!document.contains("geometry")) {
    throw CaseError(context + " requires 'geometry'.");
  }
  element.geometry = ParseGeometry(document.at("geometry"), context + ".geometry");
  return element;
}

business::RequestPayload ParsePayload(const std::string& payload_type,
                                      const json& payload,
                                      const std::string& context)
{
  if (!payload.is_object()) {
    throw CaseError(context + " must be an object.");
  }

  if (payload_type == "ProjectCreateRequest") {
    business::ProjectCreateRequest value;
    value.project_id = Optional<std::string>(payload, "project_id", "", context);
    value.name = Optional<std::string>(payload, "name", "", context);
    return value;
  }
  if (payload_type == "ProductCreateRequest") {
    business::ProductCreateRequest value;
    value.project_id = Optional<std::string>(payload, "project_id", "", context);
    value.expected_project_version =
        Optional<std::uint64_t>(payload, "expected_project_version", 0, context);
    value.product_id = Optional<std::string>(payload, "product_id", "", context);
    value.name = Optional<std::string>(payload, "name", "", context);
    return value;
  }
  if (payload_type == "RecipeCreateRequest") {
    business::RecipeCreateRequest value;
    value.product_id = Optional<std::string>(payload, "product_id", "", context);
    value.expected_product_version =
        Optional<std::uint64_t>(payload, "expected_product_version", 0, context);
    value.recipe_id = Optional<std::string>(payload, "recipe_id", "", context);
    value.name = Optional<std::string>(payload, "name", "", context);
    return value;
  }
  if (payload_type == "RecipeRevisionCreateRequest") {
    business::RecipeRevisionCreateRequest value;
    value.recipe_id = Optional<std::string>(payload, "recipe_id", "", context);
    value.expected_recipe_version =
        Optional<std::uint64_t>(payload, "expected_recipe_version", 0, context);
    value.revision_id = Optional<std::string>(payload, "revision_id", "", context);
    value.base_revision_id = Optional<std::string>(payload, "base_revision_id", "", context);
    value.expected_base_revision_version =
        Optional<std::uint64_t>(payload, "expected_base_revision_version", 0, context);
    value.change_note = Optional<std::string>(payload, "change_note", "", context);
    return value;
  }
  if (payload_type == "InspectionItemDefineRequest") {
    business::InspectionItemDefineRequest value;
    value.revision_id = Optional<std::string>(payload, "revision_id", "", context);
    value.expected_revision_version =
        Optional<std::uint64_t>(payload, "expected_revision_version", 0, context);
    value.inspection_item_id =
        Optional<std::string>(payload, "inspection_item_id", "", context);
    value.name = Optional<std::string>(payload, "name", "", context);
    value.kind = ParseInspectionKind(
        Optional<std::string>(payload, "kind", "Detection", context), context + ".kind");
    return value;
  }
  if (payload_type == "ImageRegisterRequest") {
    business::ImageRegisterRequest value;
    value.revision_id = Optional<std::string>(payload, "revision_id", "", context);
    value.expected_revision_version =
        Optional<std::uint64_t>(payload, "expected_revision_version", 0, context);
    value.inspection_item_id =
        Optional<std::string>(payload, "inspection_item_id", "", context);
    value.image_id = Optional<std::string>(payload, "image_id", "", context);
    value.content_hash = Optional<std::string>(payload, "content_hash", "", context);
    value.storage_ref = Optional<std::string>(payload, "storage_ref", "", context);
    value.source_type = ParseImageSourceType(
        Optional<std::string>(payload, "source_type", "File", context),
        context + ".source_type");
    value.acquisition_method = ParseAcquisitionMethod(
        Optional<std::string>(payload, "acquisition_method", "Upload", context),
        context + ".acquisition_method");
    value.frame_id = Optional<std::string>(payload, "frame_id", "", context);
    value.frame_revision = Optional<std::uint64_t>(payload, "frame_revision", 1, context);
    value.creator_id = Optional<std::string>(payload, "creator_id", "", context);
    value.provider_id = Optional<std::string>(payload, "provider_id", "", context);
    value.evidence_ref = Optional<std::string>(payload, "evidence_ref", "", context);
    value.width = Optional<std::uint32_t>(payload, "width", 0, context);
    value.height = Optional<std::uint32_t>(payload, "height", 0, context);
    value.channels = Optional<std::uint32_t>(payload, "channels", 0, context);
    value.purpose = ParseImagePurpose(
        Optional<std::string>(payload, "purpose", "Training", context),
        context + ".purpose");
    return value;
  }
  if (payload_type == "AnnotationUpsertRequest") {
    business::AnnotationUpsertRequest value;
    value.revision_id = Optional<std::string>(payload, "revision_id", "", context);
    value.expected_revision_version =
        Optional<std::uint64_t>(payload, "expected_revision_version", 0, context);
    value.inspection_item_id =
        Optional<std::string>(payload, "inspection_item_id", "", context);
    value.annotation_id = Optional<std::string>(payload, "annotation_id", "", context);
    value.expected_annotation_version =
        Optional<std::uint64_t>(payload, "expected_annotation_version", 0, context);
    value.image_id = Optional<std::string>(payload, "image_id", "", context);
    value.label = Optional<std::string>(payload, "label", "", context);
    if (payload.contains("geometry")) {
      value.geometry = ParseGeometry(payload.at("geometry"), context + ".geometry");
    }
    value.creation_method = ParseCreationMethod(
        Optional<std::string>(payload, "creation_method", "Manual", context),
        context + ".creation_method");
    value.status = ParseAnnotationStatus(
        Optional<std::string>(payload, "status", "Candidate", context), context + ".status");
    value.creator_id = Optional<std::string>(payload, "creator_id", "", context);
    value.provider_id = Optional<std::string>(payload, "provider_id", "", context);
    value.confidence = Optional<double>(payload, "confidence", 1.0, context);
    value.transform_ref = Optional<std::string>(payload, "transform_ref", "", context);
    value.parent_annotation_id =
        Optional<std::string>(payload, "parent_annotation_id", "", context);
    value.source_result_id = Optional<std::string>(payload, "source_result_id", "", context);
    value.parent_element_id = Optional<std::string>(payload, "parent_element_id", "", context);
    value.evidence_ref = Optional<std::string>(payload, "evidence_ref", "", context);
    return value;
  }
  if (payload_type == "RoiDefineRequest") {
    business::RoiDefineRequest value;
    value.revision_id = Optional<std::string>(payload, "revision_id", "", context);
    value.expected_revision_version =
        Optional<std::uint64_t>(payload, "expected_revision_version", 0, context);
    value.inspection_item_id =
        Optional<std::string>(payload, "inspection_item_id", "", context);
    value.roi_id = Optional<std::string>(payload, "roi_id", "", context);
    value.expected_roi_version =
        Optional<std::uint64_t>(payload, "expected_roi_version", 0, context);
    value.image_id = Optional<std::string>(payload, "image_id", "", context);
    value.name = Optional<std::string>(payload, "name", "", context);
    if (payload.contains("geometry")) {
      value.geometry = ParseGeometry(payload.at("geometry"), context + ".geometry");
    }
    value.creation_method = ParseCreationMethod(
        Optional<std::string>(payload, "creation_method", "Manual", context),
        context + ".creation_method");
    value.status = ParseAnnotationStatus(
        Optional<std::string>(payload, "status", "Candidate", context), context + ".status");
    value.creator_id = Optional<std::string>(payload, "creator_id", "", context);
    value.provider_id = Optional<std::string>(payload, "provider_id", "", context);
    value.confidence = Optional<double>(payload, "confidence", 1.0, context);
    value.transform_ref = Optional<std::string>(payload, "transform_ref", "", context);
    value.parent_roi_id = Optional<std::string>(payload, "parent_roi_id", "", context);
    value.evidence_ref = Optional<std::string>(payload, "evidence_ref", "", context);
    return value;
  }
  if (payload_type == "TemplateDefineRequest") {
    business::TemplateDefineRequest value;
    value.revision_id = Optional<std::string>(payload, "revision_id", "", context);
    value.expected_revision_version =
        Optional<std::uint64_t>(payload, "expected_revision_version", 0, context);
    value.inspection_item_id =
        Optional<std::string>(payload, "inspection_item_id", "", context);
    value.template_id = Optional<std::string>(payload, "template_id", "", context);
    value.expected_template_version =
        Optional<std::uint64_t>(payload, "expected_template_version", 0, context);
    value.source_image_id = Optional<std::string>(payload, "source_image_id", "", context);
    value.source_annotation_id =
        Optional<std::string>(payload, "source_annotation_id", "", context);
    value.name = Optional<std::string>(payload, "name", "", context);
    value.content_hash = Optional<std::string>(payload, "content_hash", "", context);
    return value;
  }
  if (payload_type == "RevisionFreezeRequest") {
    business::RevisionFreezeRequest value;
    value.revision_id = Optional<std::string>(payload, "revision_id", "", context);
    value.expected_revision_version =
        Optional<std::uint64_t>(payload, "expected_revision_version", 0, context);
    value.dataset_snapshot_id =
        Optional<std::string>(payload, "dataset_snapshot_id", "", context);
    value.dataset_digest = Optional<std::string>(payload, "dataset_digest", "", context);
    return value;
  }
  if (payload_type == "TrainingStartRequest") {
    business::TrainingStartRequest value;
    value.revision_id = Optional<std::string>(payload, "revision_id", "", context);
    value.expected_revision_version =
        Optional<std::uint64_t>(payload, "expected_revision_version", 0, context);
    value.training_run_id = Optional<std::string>(payload, "training_run_id", "", context);
    value.dataset_snapshot_id =
        Optional<std::string>(payload, "dataset_snapshot_id", "", context);
    value.algorithm_id = Optional<std::string>(payload, "algorithm_id", "", context);
    value.parameter_digest = Optional<std::string>(payload, "parameter_digest", "", context);
    return value;
  }
  if (payload_type == "TrainingCompleteRequest") {
    business::TrainingCompleteRequest value;
    value.training_run_id = Optional<std::string>(payload, "training_run_id", "", context);
    value.expected_training_version =
        Optional<std::uint64_t>(payload, "expected_training_version", 0, context);
    value.succeeded = Optional<bool>(payload, "succeeded", false, context);
    value.artifact_ref = Optional<std::string>(payload, "artifact_ref", "", context);
    value.artifact_digest = Optional<std::string>(payload, "artifact_digest", "", context);
    return value;
  }
  if (payload_type == "ModelRegisterRequest") {
    business::ModelRegisterRequest value;
    value.revision_id = Optional<std::string>(payload, "revision_id", "", context);
    value.expected_revision_version =
        Optional<std::uint64_t>(payload, "expected_revision_version", 0, context);
    value.training_run_id = Optional<std::string>(payload, "training_run_id", "", context);
    value.expected_training_version =
        Optional<std::uint64_t>(payload, "expected_training_version", 0, context);
    value.model_id = Optional<std::string>(payload, "model_id", "", context);
    value.semantic_version = Optional<std::string>(payload, "semantic_version", "", context);
    value.artifact_ref = Optional<std::string>(payload, "artifact_ref", "", context);
    value.artifact_digest = Optional<std::string>(payload, "artifact_digest", "", context);
    return value;
  }
  if (payload_type == "EvaluationRecordRequest") {
    business::EvaluationRecordRequest value;
    value.revision_id = Optional<std::string>(payload, "revision_id", "", context);
    value.expected_revision_version =
        Optional<std::uint64_t>(payload, "expected_revision_version", 0, context);
    value.model_id = Optional<std::string>(payload, "model_id", "", context);
    value.expected_model_version =
        Optional<std::uint64_t>(payload, "expected_model_version", 0, context);
    value.evaluation_id = Optional<std::string>(payload, "evaluation_id", "", context);
    value.test_dataset_id = Optional<std::string>(payload, "test_dataset_id", "", context);
    value.test_dataset_digest =
        Optional<std::string>(payload, "test_dataset_digest", "", context);
    value.gate_passed = Optional<bool>(payload, "gate_passed", false, context);
    if (payload.contains("metrics")) {
      if (!payload.at("metrics").is_array()) {
        throw CaseError(context + ".metrics must be an array.");
      }
      std::size_t index = 0;
      for (const json& metric_document : payload.at("metrics")) {
        const std::string metric_context = context + ".metrics[" + std::to_string(index++) + "]";
        business::MetricValue metric;
        metric.name = Required<std::string>(metric_document, "name", metric_context);
        metric.value = Required<double>(metric_document, "value", metric_context);
        metric.unit = Optional<std::string>(metric_document, "unit", "", metric_context);
        value.metrics.push_back(std::move(metric));
      }
    }
    return value;
  }
  if (payload_type == "ModelPromoteRequest") {
    business::ModelPromoteRequest value;
    value.revision_id = Optional<std::string>(payload, "revision_id", "", context);
    value.expected_revision_version =
        Optional<std::uint64_t>(payload, "expected_revision_version", 0, context);
    value.model_id = Optional<std::string>(payload, "model_id", "", context);
    value.expected_model_version =
        Optional<std::uint64_t>(payload, "expected_model_version", 0, context);
    value.evaluation_id = Optional<std::string>(payload, "evaluation_id", "", context);
    value.expected_evaluation_version =
        Optional<std::uint64_t>(payload, "expected_evaluation_version", 0, context);
    return value;
  }
  if (payload_type == "ProductionRunStartRequest") {
    business::ProductionRunStartRequest value;
    value.revision_id = Optional<std::string>(payload, "revision_id", "", context);
    value.expected_revision_version =
        Optional<std::uint64_t>(payload, "expected_revision_version", 0, context);
    value.model_id = Optional<std::string>(payload, "model_id", "", context);
    value.expected_model_version =
        Optional<std::uint64_t>(payload, "expected_model_version", 0, context);
    value.production_run_id =
        Optional<std::string>(payload, "production_run_id", "", context);
    value.line_id = Optional<std::string>(payload, "line_id", "", context);
    value.lot_id = Optional<std::string>(payload, "lot_id", "", context);
    return value;
  }
  if (payload_type == "ProductionResultRecordRequest") {
    business::ProductionResultRecordRequest value;
    value.production_run_id =
        Optional<std::string>(payload, "production_run_id", "", context);
    value.expected_run_version =
        Optional<std::uint64_t>(payload, "expected_run_version", 0, context);
    value.result_id = Optional<std::string>(payload, "result_id", "", context);
    value.image_id = Optional<std::string>(payload, "image_id", "", context);
    value.outcome = ParseProductionOutcome(
        Optional<std::string>(payload, "outcome", "ReviewRequired", context),
        context + ".outcome");
    if (payload.contains("elements")) {
      if (!payload.at("elements").is_array()) {
        throw CaseError(context + ".elements must be an array.");
      }
      std::size_t index = 0;
      for (const json& element : payload.at("elements")) {
        value.elements.push_back(ParseDetectionElement(
            element, context + ".elements[" + std::to_string(index++) + "]"));
      }
    }
    if (payload.contains("execution_observation")) {
      value.execution_observation = ParseObservation(
          payload.at("execution_observation"), context + ".execution_observation");
    }
    if (payload.contains("detection_observation")) {
      value.detection_observation = ParseObservation(
          payload.at("detection_observation"), context + ".detection_observation");
    }
    if (payload.contains("display_observation")) {
      value.display_observation = ParseObservation(
          payload.at("display_observation"), context + ".display_observation");
    }
    if (payload.contains("evaluation_observation")) {
      value.evaluation_observation = ParseObservation(
          payload.at("evaluation_observation"), context + ".evaluation_observation");
    }
    return value;
  }
  if (payload_type == "HumanReviewCommitRequest") {
    business::HumanReviewCommitRequest value;
    value.result_id = Optional<std::string>(payload, "result_id", "", context);
    value.expected_result_version =
        Optional<std::uint64_t>(payload, "expected_result_version", 0, context);
    value.receipt_id = Optional<std::string>(payload, "receipt_id", "", context);
    value.action = ParseReviewAction(
        Optional<std::string>(payload, "action", "Accepted", context), context + ".action");
    value.reason_code = Optional<std::string>(payload, "reason_code", "", context);
    value.note = Optional<std::string>(payload, "note", "", context);
    value.corrected_annotation_id =
        Optional<std::string>(payload, "corrected_annotation_id", "", context);
    value.corrected_annotation_version =
        Optional<std::uint64_t>(payload, "corrected_annotation_version", 0, context);
    return value;
  }
  if (payload_type == "FeedbackSubmitRequest") {
    business::FeedbackSubmitRequest value;
    value.result_id = Optional<std::string>(payload, "result_id", "", context);
    value.expected_result_version =
        Optional<std::uint64_t>(payload, "expected_result_version", 0, context);
    value.review_receipt_id =
        Optional<std::string>(payload, "review_receipt_id", "", context);
    value.feedback_id = Optional<std::string>(payload, "feedback_id", "", context);
    value.category = Optional<std::string>(payload, "category", "", context);
    value.note = Optional<std::string>(payload, "note", "", context);
    return value;
  }
  if (payload_type == "RetrainingEnqueueRequest") {
    business::RetrainingEnqueueRequest value;
    value.feedback_id = Optional<std::string>(payload, "feedback_id", "", context);
    value.expected_feedback_version =
        Optional<std::uint64_t>(payload, "expected_feedback_version", 0, context);
    value.queue_entry_id = Optional<std::string>(payload, "queue_entry_id", "", context);
    value.target_revision_id =
        Optional<std::string>(payload, "target_revision_id", "", context);
    value.rationale = Optional<std::string>(payload, "rationale", "", context);
    value.expected_target_revision_version =
        Optional<std::uint64_t>(payload, "expected_target_revision_version", 0, context);
    return value;
  }
  if (payload_type == "RecipeRollbackRequest") {
    business::RecipeRollbackRequest value;
    value.recipe_id = Optional<std::string>(payload, "recipe_id", "", context);
    value.expected_recipe_version =
        Optional<std::uint64_t>(payload, "expected_recipe_version", 0, context);
    value.target_revision_id =
        Optional<std::string>(payload, "target_revision_id", "", context);
    value.expected_target_revision_version =
        Optional<std::uint64_t>(payload, "expected_target_revision_version", 0, context);
    value.reason = Optional<std::string>(payload, "reason", "", context);
    return value;
  }
  if (payload_type == "SnapshotReadRequest") {
    business::SnapshotReadRequest value;
    value.minimum_store_revision =
        Optional<std::uint64_t>(payload, "minimum_store_revision", 0, context);
    return value;
  }
  throw CaseError(context + " has unknown payload_type '" + payload_type + "'.");
}

struct ParsedRequest
{
  business::WorkflowRequest value;
  std::string capability_id;
  std::string payload_type;
  json source;
  json expect;
};

ParsedRequest ParseRequest(
    const json& document,
    const business::ApiVersion api_version,
    const std::map<std::string, const business::CapabilityDescriptor*>& capabilities,
    const std::string& context)
{
  if (!document.is_object()) {
    throw CaseError(context + " must be an object.");
  }
  ParsedRequest parsed;
  parsed.capability_id = Required<std::string>(document, "capability_id", context);
  const auto capability = capabilities.find(parsed.capability_id);
  if (capability == capabilities.end()) {
    throw CaseError(context + " references unknown capability '" + parsed.capability_id + "'.");
  }
  parsed.payload_type = Required<std::string>(document, "payload_type", context);
  if (parsed.payload_type != capability->second->request_type) {
    throw CaseError(context + " payload_type '" + parsed.payload_type +
                    "' does not match Capabilities() request_type '" +
                    capability->second->request_type + "'.");
  }
  if (!document.contains("payload")) {
    throw CaseError(context + " requires 'payload'.");
  }
  parsed.value.api_version = api_version;
  parsed.value.capability = capability->second->id;
  parsed.value.request_id = Required<std::string>(document, "request_id", context);
  parsed.value.event_id = Required<std::string>(document, "event_id", context);
  parsed.value.actor_id = Required<std::string>(document, "actor_id", context);
  parsed.value.occurred_at = Required<std::string>(document, "occurred_at", context);
  parsed.value.payload = ParsePayload(parsed.payload_type, document.at("payload"),
                                      context + ".payload");
  parsed.expect = document.contains("expect") ? document.at("expect") : json::object();
  if (!parsed.expect.is_object()) {
    throw CaseError(context + ".expect must be an object.");
  }
  parsed.source = document;
  parsed.source.erase("expect");
  return parsed;
}

json SerializeConfigurationContract(const business::BusinessConfigurationContract& value)
{
  return {{"schema_version", value.schema_version},
          {"configuration_id", value.configuration_id},
          {"configuration_revision", value.configuration_revision},
          {"default_coordinate_space", value.default_coordinate_space},
          {"observation_channels", value.observation_channels},
          {"retraining_eligible_review_actions",
           value.retraining_eligible_review_actions},
          {"immutable_review_receipt", value.immutable_review_receipt},
          {"event_id_required_for_mutations", value.event_id_required_for_mutations},
          {"script_runtime_enabled", value.script_runtime_enabled},
          {"digest_algorithm", value.digest_algorithm},
          {"canonical_digest", value.canonical_digest}};
}

json SerializeBuildIdentity(const business::BusinessBuildIdentity& value)
{
  return {{"product_line", value.product_line},
          {"api_version", {{"major", value.api_version.major},
                            {"minor", value.api_version.minor}}},
          {"implementation_version", value.implementation_version},
          {"source_revision", value.source_revision},
          {"stable_build_id", value.stable_build_id}};
}

json SerializeGeometry(const business::Geometry& value)
{
  return {{"kind", ToString(value.kind)},
          {"coordinates", value.coordinates},
          {"coordinate_space", value.coordinate_space},
          {"mask_ref", value.mask_ref}};
}

json SerializeObservation(const business::ObservationChannel& value)
{
  return {{"status", ToString(value.status)},
          {"provider_id", value.provider_id},
          {"code", value.code},
          {"detail", value.detail},
          {"evidence_refs", value.evidence_refs}};
}

json SerializeElement(const business::DetectionElement& value)
{
  return {{"element_id", value.element_id},
          {"label", value.label},
          {"score", value.score},
          {"geometry", SerializeGeometry(value.geometry)}};
}

json SerializeResponse(const business::WorkflowResponse& value)
{
  json changes = json::array();
  for (const business::EntityChange& change : value.changes) {
    changes.push_back({{"entity_type", change.entity_type},
                       {"entity_id", change.entity_id},
                       {"before_version", change.before_version},
                       {"after_version", change.after_version}});
  }
  json invalidations = json::array();
  for (const business::DownstreamInvalidation& invalidation : value.invalidations) {
    invalidations.push_back({{"entity_type", invalidation.entity_type},
                             {"entity_id", invalidation.entity_id},
                             {"reason_code", invalidation.reason_code}});
  }
  return {{"api_version", {{"major", value.api_version.major},
                            {"minor", value.api_version.minor}}},
          {"capability_id", business::ToStableName(value.capability)},
          {"status", ToString(value.status)},
          {"code", value.code},
          {"message", value.message},
          {"request_id", value.request_id},
          {"event_id", value.event_id},
          {"primary_entity_type", value.primary_entity_type},
          {"primary_entity_id", value.primary_entity_id},
          {"primary_entity_version", value.primary_entity_version},
          {"store_revision", value.store_revision},
          {"idempotent_replay", value.idempotent_replay},
          {"changes", std::move(changes)},
          {"invalidations", std::move(invalidations)},
          {"observations",
           {{"execution", SerializeObservation(value.execution_observation)},
            {"detection", SerializeObservation(value.detection_observation)},
            {"display", SerializeObservation(value.display_observation)},
            {"evaluation", SerializeObservation(value.evaluation_observation)}}}};
}

json SerializeSnapshotSummary(const business::SnapshotSummary& value)
{
  return {{"store_revision", value.store_revision},
          {"project_count", static_cast<std::uint64_t>(value.project_count)},
          {"product_count", static_cast<std::uint64_t>(value.product_count)},
          {"recipe_count", static_cast<std::uint64_t>(value.recipe_count)},
          {"revision_count", static_cast<std::uint64_t>(value.revision_count)},
          {"inspection_item_count", static_cast<std::uint64_t>(value.inspection_item_count)},
          {"image_count", static_cast<std::uint64_t>(value.image_count)},
          {"annotation_count", static_cast<std::uint64_t>(value.annotation_count)},
          {"roi_count", static_cast<std::uint64_t>(value.roi_count)},
          {"template_count", static_cast<std::uint64_t>(value.template_count)},
          {"training_run_count", static_cast<std::uint64_t>(value.training_run_count)},
          {"model_count", static_cast<std::uint64_t>(value.model_count)},
          {"evaluation_count", static_cast<std::uint64_t>(value.evaluation_count)},
          {"production_run_count", static_cast<std::uint64_t>(value.production_run_count)},
          {"production_result_count", static_cast<std::uint64_t>(value.production_result_count)},
          {"review_receipt_count", static_cast<std::uint64_t>(value.review_receipt_count)},
          {"feedback_count", static_cast<std::uint64_t>(value.feedback_count)},
          {"retraining_queue_count", static_cast<std::uint64_t>(value.retraining_queue_count)}};
}

json SerializeReadModel(const business::BusinessReadModel& value)
{
  json result = {{"store_revision", value.store_revision},
                 {"projects", json::array()},
                 {"products", json::array()},
                 {"recipes", json::array()},
                 {"revisions", json::array()},
                 {"inspection_items", json::array()},
                 {"images", json::array()},
                 {"annotations", json::array()},
                 {"rois", json::array()},
                 {"templates", json::array()},
                 {"training_runs", json::array()},
                 {"models", json::array()},
                 {"evaluations", json::array()},
                 {"production_runs", json::array()},
                 {"production_results", json::array()},
                 {"review_receipts", json::array()},
                 {"feedback", json::array()},
                 {"retraining_queue", json::array()},
                 {"audit", json::array()}};
  for (const business::ProjectView& item : value.projects) {
    result["projects"].push_back({{"id", item.id}, {"name", item.name},
                                  {"version", item.version}});
  }
  for (const business::ProductView& item : value.products) {
    result["products"].push_back({{"id", item.id}, {"project_id", item.project_id},
                                  {"name", item.name}, {"version", item.version}});
  }
  for (const business::RecipeView& item : value.recipes) {
    result["recipes"].push_back({{"id", item.id}, {"product_id", item.product_id},
                                 {"name", item.name},
                                 {"active_revision_id", item.active_revision_id},
                                 {"version", item.version}});
  }
  for (const business::RecipeRevisionView& item : value.revisions) {
    result["revisions"].push_back({{"id", item.id}, {"recipe_id", item.recipe_id},
                                   {"base_revision_id", item.base_revision_id},
                                   {"status", item.status},
                                   {"dataset_snapshot_id", item.dataset_snapshot_id},
                                   {"active_model_id", item.active_model_id},
                                   {"sequence", item.sequence}, {"version", item.version},
                                   {"design_generation", item.design_generation},
                                   {"downstream_invalidated", item.downstream_invalidated}});
  }
  for (const business::InspectionItemView& item : value.inspection_items) {
    result["inspection_items"].push_back({{"id", item.id},
                                          {"revision_id", item.revision_id},
                                          {"name", item.name},
                                          {"kind", ToString(item.kind)},
                                          {"version", item.version}});
  }
  for (const business::ImageView& item : value.images) {
    result["images"].push_back({{"id", item.id}, {"revision_id", item.revision_id},
                                {"inspection_item_id", item.inspection_item_id},
                                {"content_hash", item.content_hash},
                                {"storage_ref", item.storage_ref},
                                {"frame_id", item.frame_id},
                                {"frame_revision", item.frame_revision},
                                {"source_type", ToString(item.source_type)},
                                {"acquisition_method", ToString(item.acquisition_method)},
                                {"creator_id", item.creator_id},
                                {"provider_id", item.provider_id},
                                {"evidence_ref", item.evidence_ref},
                                {"width", item.width}, {"height", item.height},
                                {"channels", item.channels},
                                {"purpose", ToString(item.purpose)},
                                {"version", item.version}});
  }
  for (const business::AnnotationView& item : value.annotations) {
    result["annotations"].push_back({{"id", item.id},
                                     {"revision_id", item.revision_id},
                                     {"image_id", item.image_id},
                                     {"label", item.label},
                                     {"geometry", SerializeGeometry(item.geometry)},
                                     {"status", ToString(item.status)},
                                     {"creation_method", ToString(item.creation_method)},
                                     {"creator_id", item.creator_id},
                                     {"provider_id", item.provider_id},
                                     {"confidence", item.confidence},
                                     {"transform_ref", item.transform_ref},
                                     {"parent_annotation_id", item.parent_annotation_id},
                                     {"source_result_id", item.source_result_id},
                                     {"parent_element_id", item.parent_element_id},
                                     {"evidence_ref", item.evidence_ref},
                                     {"version", item.version}});
  }
  for (const business::RoiView& item : value.rois) {
    result["rois"].push_back({{"id", item.id}, {"revision_id", item.revision_id},
                              {"image_id", item.image_id}, {"name", item.name},
                              {"geometry", SerializeGeometry(item.geometry)},
                              {"creation_method", ToString(item.creation_method)},
                              {"status", ToString(item.status)},
                              {"creator_id", item.creator_id},
                              {"provider_id", item.provider_id},
                              {"confidence", item.confidence},
                              {"transform_ref", item.transform_ref},
                              {"parent_roi_id", item.parent_roi_id},
                              {"evidence_ref", item.evidence_ref},
                              {"version", item.version}});
  }
  for (const business::TemplateView& item : value.templates) {
    result["templates"].push_back(
        {{"id", item.id}, {"revision_id", item.revision_id},
         {"inspection_item_id", item.inspection_item_id},
         {"source_image_id", item.source_image_id},
         {"source_annotation_id", item.source_annotation_id},
         {"name", item.name}, {"content_hash", item.content_hash},
         {"version", item.version}});
  }
  for (const business::TrainingRunView& item : value.training_runs) {
    result["training_runs"].push_back(
        {{"id", item.id}, {"revision_id", item.revision_id},
         {"dataset_snapshot_id", item.dataset_snapshot_id},
         {"algorithm_id", item.algorithm_id},
         {"parameter_digest", item.parameter_digest},
         {"artifact_ref", item.artifact_ref},
         {"artifact_digest", item.artifact_digest},
         {"revision_design_generation", item.revision_design_generation},
         {"version", item.version}, {"status", item.status}});
  }
  for (const business::ModelView& item : value.models) {
    result["models"].push_back({{"id", item.id}, {"revision_id", item.revision_id},
                                {"training_run_id", item.training_run_id},
                                {"semantic_version", item.semantic_version},
                                {"artifact_ref", item.artifact_ref},
                                {"artifact_digest", item.artifact_digest},
                                {"revision_design_generation",
                                 item.revision_design_generation},
                                {"status", item.status}, {"version", item.version}});
  }
  for (const business::EvaluationView& item : value.evaluations) {
    json metrics = json::array();
    for (const business::MetricValue& metric : item.metrics) {
      metrics.push_back({{"name", metric.name}, {"value", metric.value},
                         {"unit", metric.unit}});
    }
    result["evaluations"].push_back(
        {{"id", item.id}, {"revision_id", item.revision_id},
         {"model_id", item.model_id}, {"test_dataset_id", item.test_dataset_id},
         {"test_dataset_digest", item.test_dataset_digest},
         {"metrics", std::move(metrics)},
         {"revision_design_generation", item.revision_design_generation},
         {"version", item.version}, {"gate_passed", item.gate_passed},
         {"valid", item.valid}});
  }
  for (const business::ProductionRunView& item : value.production_runs) {
    result["production_runs"].push_back(
        {{"id", item.id}, {"revision_id", item.revision_id},
         {"model_id", item.model_id}, {"line_id", item.line_id},
         {"lot_id", item.lot_id},
         {"revision_design_generation", item.revision_design_generation},
         {"version", item.version}, {"status", item.status},
         {"result_ids", item.result_ids}});
  }
  for (const business::ProductionResultView& item : value.production_results) {
    json elements = json::array();
    for (const business::DetectionElement& element : item.elements) {
      elements.push_back(SerializeElement(element));
    }
    result["production_results"].push_back(
        {{"id", item.id}, {"production_run_id", item.production_run_id},
         {"image_id", item.image_id}, {"outcome", ToString(item.outcome)},
         {"review_status", item.review_status},
         {"review_receipt_id", item.review_receipt_id},
         {"elements", std::move(elements)},
         {"observations",
          {{"execution", SerializeObservation(item.execution_observation)},
           {"detection", SerializeObservation(item.detection_observation)},
           {"display", SerializeObservation(item.display_observation)},
           {"evaluation", SerializeObservation(item.evaluation_observation)}}},
         {"revision_design_generation", item.revision_design_generation},
         {"valid", item.valid},
         {"invalidation_reason", item.invalidation_reason},
         {"version", item.version}});
  }
  for (const business::HumanReviewView& item : value.review_receipts) {
    result["review_receipts"].push_back(
        {{"id", item.id}, {"event_id", item.event_id}, {"result_id", item.result_id},
         {"action", ToString(item.action)},
         {"input_result_version", item.input_result_version},
         {"output_result_version", item.output_result_version},
         {"reason_code", item.reason_code}, {"note", item.note},
         {"corrected_annotation_id", item.corrected_annotation_id},
         {"corrected_annotation_version", item.corrected_annotation_version},
         {"actor_id", item.actor_id}, {"occurred_at", item.occurred_at}});
  }
  for (const business::FeedbackView& item : value.feedback) {
    result["feedback"].push_back(
        {{"id", item.id}, {"result_id", item.result_id},
         {"review_receipt_id", item.review_receipt_id},
         {"category", item.category}, {"note", item.note},
         {"version", item.version}, {"valid", item.valid},
         {"invalidation_reason", item.invalidation_reason}});
  }
  for (const business::RetrainingQueueView& item : value.retraining_queue) {
    result["retraining_queue"].push_back(
        {{"id", item.id}, {"feedback_id", item.feedback_id},
         {"target_revision_id", item.target_revision_id},
         {"target_revision_version", item.target_revision_version},
         {"rationale", item.rationale}, {"version", item.version},
         {"valid", item.valid},
         {"invalidation_reason", item.invalidation_reason}});
  }
  for (const business::AuditView& item : value.audit) {
    result["audit"].push_back(
        {{"ordinal", item.ordinal}, {"event_id", item.event_id},
         {"request_id", item.request_id}, {"capability_id", item.capability},
         {"status", ToString(item.status)}, {"code", item.code},
         {"primary_entity_type", item.primary_entity_type},
         {"primary_entity_id", item.primary_entity_id},
         {"primary_entity_version", item.primary_entity_version}});
  }
  return result;
}

json CaptureSnapshot(const business::IBusinessWorkflowTestApi& api)
{
  return {{"summary", SerializeSnapshotSummary(api.ReadSnapshotSummary())},
          {"read_model", SerializeReadModel(api.ReadSnapshot())}};
}

void AddAssertion(std::vector<AssertionRecord>& assertions,
                  std::string field,
                  json expected,
                  json actual,
                  const bool passed,
                  std::string detail = {})
{
  AssertionRecord assertion;
  assertion.field = std::move(field);
  assertion.expected = std::move(expected);
  assertion.actual = std::move(actual);
  assertion.passed = passed;
  assertion.detail = std::move(detail);
  assertions.push_back(std::move(assertion));
}

void AddEquality(std::vector<AssertionRecord>& assertions,
                 const std::string& field,
                 const json& expected,
                 const json& actual)
{
  AddAssertion(assertions, field, expected, actual, expected == actual,
               expected == actual ? "" : "Declared value differs from the business API observation.");
}

bool JsonContainsDeclaredFields(const json& actual, const json& expected)
{
  if (!expected.is_object()) {
    return actual == expected;
  }
  if (!actual.is_object()) {
    return false;
  }
  for (auto item = expected.begin(); item != expected.end(); ++item) {
    if (!actual.contains(item.key()) ||
        !JsonContainsDeclaredFields(actual.at(item.key()), item.value())) {
      return false;
    }
  }
  return true;
}

void AddArrayIncludesAssertions(std::vector<AssertionRecord>& assertions,
                               const std::string& field,
                               const json& expected,
                               const json& actual)
{
  if (!expected.is_array()) {
    AddAssertion(assertions, field, expected, actual, false,
                 "Declared inclusion assertion must be an array.");
    return;
  }
  if (!actual.is_array()) {
    AddAssertion(assertions, field, expected, actual, false,
                 "Observed value is not an array.");
    return;
  }
  std::size_t index = 0;
  for (const json& expected_item : expected) {
    const bool found = std::any_of(actual.begin(), actual.end(),
                                   [&expected_item](const json& actual_item) {
                                     return JsonContainsDeclaredFields(actual_item, expected_item);
                                   });
    AddAssertion(assertions, field + "[" + std::to_string(index++) + "]",
                 expected_item, found ? expected_item : actual, found,
                 found ? "" : "No observed item contains every declared field.");
  }
}

json SignedDelta(const json& before, const json& after, const std::string& key)
{
  if (!before.is_object() || !after.is_object() ||
      !before.contains(key) || !after.contains(key) ||
      !before.at(key).is_number_unsigned() || !after.at(key).is_number_unsigned()) {
    return nullptr;
  }
  const std::uint64_t before_value = before.at(key).get<std::uint64_t>();
  const std::uint64_t after_value = after.at(key).get<std::uint64_t>();
  if (after_value >= before_value) {
    const std::uint64_t delta = after_value - before_value;
    if (delta <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
      return static_cast<std::int64_t>(delta);
    }
    return delta;
  }
  const std::uint64_t magnitude = before_value - after_value;
  if (magnitude <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
    return -static_cast<std::int64_t>(magnitude);
  }
  return nullptr;
}

json FindActiveRevision(const json& read_model,
                        const business::WorkflowResponse& response)
{
  if (!read_model.is_object() || !read_model.contains("recipes") ||
      !read_model.at("recipes").is_array()) {
    return nullptr;
  }
  const json& recipes = read_model.at("recipes");
  if (recipes.size() == 1 && recipes.front().is_object() &&
      recipes.front().contains("active_revision_id")) {
    return recipes.front().at("active_revision_id");
  }
  std::string recipe_id;
  if (read_model.contains("revisions") && read_model.at("revisions").is_array()) {
    for (const json& revision : read_model.at("revisions")) {
      if (revision.is_object() && revision.value("id", "") == response.primary_entity_id) {
        recipe_id = revision.value("recipe_id", "");
        break;
      }
    }
  }
  for (const json& recipe : recipes) {
    if (recipe.is_object() && recipe.value("id", "") == recipe_id &&
        recipe.contains("active_revision_id")) {
      return recipe.at("active_revision_id");
    }
  }
  return nullptr;
}

const json* FindReviewReceipt(const json& read_model,
                              const business::WorkflowResponse& response)
{
  if (!read_model.is_object() || !read_model.contains("review_receipts") ||
      !read_model.at("review_receipts").is_array()) {
    return nullptr;
  }
  const json& receipts = read_model.at("review_receipts");
  if (!response.primary_entity_id.empty()) {
    for (const json& receipt : receipts) {
      if (receipt.is_object() && receipt.value("id", "") == response.primary_entity_id) {
        return &receipt;
      }
    }
  }
  return receipts.size() == 1 ? &receipts.front() : nullptr;
}

json FindDetectionElementCount(const json& read_model,
                               const business::WorkflowResponse& response)
{
  if (!read_model.is_object() || !read_model.contains("production_results") ||
      !read_model.at("production_results").is_array()) {
    return nullptr;
  }
  const json& results = read_model.at("production_results");
  for (const json& result : results) {
    if (result.is_object() && result.value("id", "") == response.primary_entity_id &&
        result.contains("elements") && result.at("elements").is_array()) {
      return static_cast<std::uint64_t>(result.at("elements").size());
    }
  }
  if (results.size() == 1 && results.front().is_object() &&
      results.front().contains("elements") && results.front().at("elements").is_array()) {
    return static_cast<std::uint64_t>(results.front().at("elements").size());
  }
  return nullptr;
}

void EvaluateDeclaredAssertions(const json& expected,
                                const business::WorkflowResponse& response,
                                const json& response_json,
                                const json& snapshot_before,
                                const json& snapshot_after,
                                std::vector<AssertionRecord>& assertions)
{
  static const std::set<std::string> response_fields = {
      "status", "code", "message", "request_id", "event_id",
      "primary_entity_type", "primary_entity_id", "primary_entity_version",
      "store_revision", "idempotent_replay"};

  for (auto item = expected.begin(); item != expected.end(); ++item) {
    const std::string key = item.key();
    if (response_fields.find(key) != response_fields.end()) {
      const json actual = response_json.contains(key) ? response_json.at(key) : json(nullptr);
      AddEquality(assertions, key, item.value(), actual);
      continue;
    }
    if (key == "store_revision_delta") {
      AddEquality(assertions, key, item.value(),
                  SignedDelta(snapshot_before.at("summary"),
                              snapshot_after.at("summary"), "store_revision"));
      continue;
    }
    if (key == "snapshot_delta") {
      if (!item.value().is_object()) {
        AddAssertion(assertions, key, item.value(), nullptr, false,
                     "snapshot_delta must be an object.");
        continue;
      }
      for (auto delta = item.value().begin(); delta != item.value().end(); ++delta) {
        const json actual = SignedDelta(snapshot_before.at("summary"),
                                        snapshot_after.at("summary"), delta.key());
        AddEquality(assertions, key + "." + delta.key(), delta.value(), actual);
      }
      continue;
    }
    if (key == "changes") {
      AddEquality(assertions, key, item.value(), response_json.at("changes"));
      continue;
    }
    if (key == "changes_include") {
      AddArrayIncludesAssertions(assertions, key, item.value(), response_json.at("changes"));
      continue;
    }
    if (key == "change_count") {
      AddEquality(assertions, key, item.value(),
                  static_cast<std::uint64_t>(response.changes.size()));
      continue;
    }
    if (key == "invalidations") {
      AddEquality(assertions, key, item.value(), response_json.at("invalidations"));
      continue;
    }
    if (key == "invalidations_include") {
      AddArrayIncludesAssertions(assertions, key, item.value(),
                                 response_json.at("invalidations"));
      continue;
    }
    if (key == "invalidation_count") {
      AddEquality(assertions, key, item.value(),
                  static_cast<std::uint64_t>(response.invalidations.size()));
      continue;
    }
    if (key == "observation_statuses") {
      if (!item.value().is_object()) {
        AddAssertion(assertions, key, item.value(), nullptr, false,
                     "observation_statuses must be an object.");
        continue;
      }
      const json& observations = response_json.at("observations");
      for (auto observation = item.value().begin();
           observation != item.value().end(); ++observation) {
        json actual = nullptr;
        if (observations.contains(observation.key()) &&
            observations.at(observation.key()).contains("status")) {
          actual = observations.at(observation.key()).at("status");
        }
        AddEquality(assertions, key + "." + observation.key(),
                    observation.value(), actual);
      }
      continue;
    }
    if (key == "detection_element_count") {
      AddEquality(assertions, key, item.value(),
                  FindDetectionElementCount(snapshot_after.at("read_model"), response));
      continue;
    }
    if (key == "read_model") {
      if (!item.value().is_object()) {
        AddAssertion(assertions, key, item.value(), nullptr, false,
                     "read_model assertion must be an object.");
        continue;
      }
      const json& read_model = snapshot_after.at("read_model");
      const json* receipt = FindReviewReceipt(read_model, response);
      for (auto field = item.value().begin(); field != item.value().end(); ++field) {
        json actual = nullptr;
        if (field.key() == "store_revision" && read_model.contains("store_revision")) {
          actual = read_model.at("store_revision");
        } else if (field.key() == "active_revision_id") {
          actual = FindActiveRevision(read_model, response);
        } else if ((field.key() == "review_action" ||
                    field.key() == "input_result_version" ||
                    field.key() == "output_result_version") && receipt != nullptr) {
          const std::string receipt_key = field.key() == "review_action" ? "action" : field.key();
          if (receipt->contains(receipt_key)) {
            actual = receipt->at(receipt_key);
          }
        } else if (read_model.contains(field.key())) {
          actual = read_model.at(field.key());
        }
        AddEquality(assertions, key + "." + field.key(), field.value(), actual);
      }
      continue;
    }
    AddAssertion(assertions, key, item.value(), nullptr, false,
                 "Unknown declared assertion field; it was not ignored.");
  }
}

ExecutionRecord ExecuteRequest(business::IBusinessWorkflowTestApi& api,
                               const ParsedRequest& request,
                               std::string phase,
                               const std::size_t ordinal)
{
  ExecutionRecord record;
  record.phase = std::move(phase);
  record.ordinal = ordinal;
  record.started_at = UtcTimestamp();
  record.request = request.source;
  record.request_digest = DigestJson(request.source);
  try {
    record.snapshot_before = CaptureSnapshot(api);
    record.api_invoked = true;
    const business::WorkflowResponse response = api.Execute(request.value);
    record.response = SerializeResponse(response);
    record.snapshot_after = CaptureSnapshot(api);

    AddEquality(record.assertions, "$interface.api_version.major",
                request.value.api_version.major, response.api_version.major);
    AddEquality(record.assertions, "$interface.api_version.minor",
                request.value.api_version.minor, response.api_version.minor);
    AddEquality(record.assertions, "$interface.capability_id", request.capability_id,
                std::string(business::ToStableName(response.capability)));
    AddEquality(record.assertions, "$interface.request_id", request.value.request_id,
                response.request_id);
    AddEquality(record.assertions, "$interface.event_id", request.value.event_id,
                response.event_id);
    AddEquality(record.assertions, "$interface.snapshot_store_revision",
                record.snapshot_after.at("summary").at("store_revision"),
                record.response.at("store_revision"));
    EvaluateDeclaredAssertions(request.expect, response, record.response,
                               record.snapshot_before, record.snapshot_after,
                               record.assertions);
    record.passed = record.api_invoked &&
                    std::all_of(record.assertions.begin(), record.assertions.end(),
                                [](const AssertionRecord& assertion) {
                                  return assertion.passed;
                                });
  } catch (const std::exception& exception) {
    record.error = exception.what();
    record.passed = false;
    if (record.snapshot_after.is_null()) {
      try {
        record.snapshot_after = CaptureSnapshot(api);
      } catch (...) {
      }
    }
  }
  record.completed_at = UtcTimestamp();
  return record;
}

Options ParseOptions(const int argc, char** argv)
{
  Options options;
  std::set<std::string> seen;
  for (int index = 1; index < argc; ++index) {
    const std::string argument = argv[index];
    if (argument != "--case-root" && argument != "--out" &&
        argument != "--run-id" && argument != "--implementation-build-id") {
      throw CaseError("Unknown argument: " + argument);
    }
    if (!seen.insert(argument).second) {
      throw CaseError("Duplicate argument: " + argument);
    }
    if (index + 1 >= argc) {
      throw CaseError("Missing value for " + argument);
    }
    const std::string value = argv[++index];
    if (value.empty()) {
      throw CaseError("Empty value for " + argument);
    }
    if (argument == "--case-root") {
      options.case_root = fs::path(value);
    } else if (argument == "--out") {
      options.output_root = fs::path(value);
    } else if (argument == "--run-id") {
      options.run_id = value;
    } else {
      options.implementation_build_id = value;
      options.requested_implementation_build_id = value;
    }
  }
  if (seen.size() != 4) {
    throw CaseError(
        "Usage: vision_ai_business_case_runner --case-root <business_api_v1> "
        "--out <new-dir-under-cxscript_runs> --run-id <id> "
        "--implementation-build-id <id>");
  }
  return options;
}

void ResolveAndClaimPaths(Options& options)
{
  std::error_code error;
  options.case_root = fs::canonical(options.case_root, error);
  if (error || !fs::is_directory(options.case_root)) {
    throw CaseError("--case-root is not an existing directory: " +
                    options.case_root.string());
  }
  const fs::path cxscript_runs = FindCxscriptRunsAncestor(options.case_root);
  if (cxscript_runs.empty()) {
    throw CaseError("--case-root must be located under cxscript_runs.");
  }
  if (LowerAscii(options.case_root.filename().string()) != "business_api_v1") {
    throw CaseError("--case-root must identify the business_api_v1 directory.");
  }

  fs::path absolute_output = fs::absolute(options.output_root, error);
  if (error) {
    throw CaseError("Cannot resolve --out: " + error.message());
  }
  absolute_output = absolute_output.lexically_normal();
  if (fs::exists(absolute_output, error) || error) {
    throw CaseError("--out must be a new, non-existing directory: " +
                    absolute_output.string());
  }
  const fs::path output_parent = fs::canonical(absolute_output.parent_path(), error);
  if (error || !fs::is_directory(output_parent)) {
    throw CaseError("--out parent must already exist: " +
                    absolute_output.parent_path().string());
  }
  options.output_root = (output_parent / absolute_output.filename()).lexically_normal();
  if (!IsStrictDescendant(options.output_root, cxscript_runs) ||
      IsStrictDescendant(options.output_root, options.case_root)) {
    throw CaseError("--out must be external to business_api_v1 and under the same cxscript_runs root.");
  }
  if (!fs::create_directory(options.output_root, error) || error) {
    throw CaseError("Cannot claim new --out directory: " + error.message());
  }
}

std::string NormalizedRelativePath(const fs::path& value)
{
  std::string result = value.lexically_normal().generic_string();
  while (result.rfind("./", 0) == 0) {
    result.erase(0, 2);
  }
#ifdef _WIN32
  result = LowerAscii(result);
#endif
  return result;
}

std::vector<fs::path> DiscoverCaseFiles(const fs::path& case_root)
{
  std::vector<fs::path> files;
  std::error_code error;
  fs::recursive_directory_iterator iterator(
      case_root, fs::directory_options::skip_permission_denied, error);
  const fs::recursive_directory_iterator end;
  if (error) {
    throw CaseError("Cannot scan --case-root: " + error.message());
  }
  while (iterator != end) {
    const fs::directory_entry entry = *iterator;
    iterator.increment(error);
    if (error) {
      throw CaseError("Case discovery failed: " + error.message());
    }
    if (entry.path().filename() != "business_api_case.json") {
      continue;
    }
    const fs::path canonical = fs::canonical(entry.path(), error);
    if (error || !fs::is_regular_file(canonical)) {
      throw CaseError("Discovered case is not a readable regular file: " +
                      entry.path().string());
    }
    if (!IsStrictDescendant(canonical, case_root)) {
      throw CaseError("Discovered case escapes --case-root: " + entry.path().string());
    }
    files.push_back(canonical);
  }
  std::sort(files.begin(), files.end(), [&case_root](const fs::path& left,
                                                     const fs::path& right) {
    return NormalizedRelativePath(fs::relative(left, case_root)) <
           NormalizedRelativePath(fs::relative(right, case_root));
  });
  return files;
}

std::vector<CatalogEntry> ParseTraceabilityCatalog(const json& document,
                                                   const fs::path& case_root)
{
  if (!document.is_object()) {
    throw CaseError("traceability_manifest.json must be an object.");
  }
  if (Required<std::string>(document, "schema_version", "traceability_manifest") !=
      "visionai.business_traceability.v1") {
    throw CaseError("Unsupported traceability_manifest schema_version.");
  }
  if (!document.contains("api_version") || !document.at("api_version").is_object()) {
    throw CaseError("traceability_manifest.api_version must be an object.");
  }
  const json& api_version = document.at("api_version");
  if (Required<std::uint32_t>(api_version, "major", "traceability_manifest.api_version") !=
          business::kApiMajorVersion ||
      Required<std::uint32_t>(api_version, "minor", "traceability_manifest.api_version") !=
          business::kApiMinorVersion) {
    throw CaseError("traceability_manifest API version differs from the compiled API.");
  }
  if (!document.contains("catalog_contract") ||
      !document.at("catalog_contract").is_object()) {
    throw CaseError("traceability_manifest.catalog_contract must be an object.");
  }
  const json& contract = document.at("catalog_contract");
  if (Required<std::string>(contract, "interface_id", "catalog_contract") !=
          kTestInterfaceId ||
      Required<std::string>(contract, "catalog_method", "catalog_contract") !=
          "Capabilities" ||
      Required<std::string>(contract, "execute_method", "catalog_contract") !=
          "Execute" ||
      Required<std::string>(contract, "read_method", "catalog_contract") !=
          "ReadSnapshot" ||
      Required<std::string>(contract, "summary_method", "catalog_contract") !=
          "ReadSnapshotSummary" ||
      Required<std::string>(contract, "factory", "catalog_contract") !=
          "vision_ai::business::v1::MakeCompiledBusinessWorkflowTestApi" ||
      Required<std::string>(contract, "compare_by", "catalog_contract") !=
          "stable_name" ||
      Required<bool>(contract, "enum_ordinals_are_contract", "catalog_contract")) {
    throw CaseError("traceability_manifest names a different compiled test interface.");
  }
  if (!document.contains("case_discovery") ||
      !document.at("case_discovery").is_object()) {
    throw CaseError("traceability_manifest.case_discovery must be an object.");
  }
  const json& discovery = document.at("case_discovery");
  if (Required<std::string>(discovery, "root", "case_discovery") != "cases" ||
      Required<std::string>(discovery, "exact_filename", "case_discovery") !=
          "business_api_case.json" ||
      Required<std::string>(discovery, "repository_scope", "case_discovery") !=
          "FRESH_PER_SCENARIO" ||
      !Required<bool>(discovery, "recursive", "case_discovery") ||
      Required<std::string>(discovery, "optional_follow_up_requests_field",
                            "case_discovery") != "follow_up_requests" ||
      Required<std::string>(discovery, "workflow_request_api_version_source",
                            "case_discovery") != "case.api_version" ||
      Required<bool>(discovery, "setup_counts_as_primary_coverage",
                     "case_discovery") ||
      Required<bool>(discovery, "follow_up_requests_count_as_primary_coverage",
                     "case_discovery")) {
    throw CaseError("traceability_manifest case discovery contract is unsupported.");
  }
  if (!document.contains("capabilities") || !document.at("capabilities").is_array()) {
    throw CaseError("traceability_manifest.capabilities must be an array.");
  }
  std::vector<CatalogEntry> entries;
  std::size_t index = 0;
  for (const json& item : document.at("capabilities")) {
    const std::string context = "traceability_manifest.capabilities[" +
                                std::to_string(index++) + "]";
    CatalogEntry entry;
    entry.capability_id = Required<std::string>(item, "capability_id", context);
    entry.request_type = Required<std::string>(item, "request_type", context);
    entry.implementation_symbol =
        Required<std::string>(item, "implementation_symbol", context);
    entry.case_id = Required<std::string>(item, "case_id", context);
    const std::string case_ref = Required<std::string>(item, "case_ref", context);
    entry.case_ref = fs::path(case_ref).lexically_normal();
    if (entry.capability_id.empty() || entry.request_type.empty() ||
        entry.implementation_symbol.empty() || entry.case_id.empty() ||
        entry.case_ref.empty() || entry.case_ref.is_absolute()) {
      throw CaseError(context + " contains an empty or absolute identity field.");
    }
    const fs::path resolved = fs::weakly_canonical(case_root / entry.case_ref);
    if (!IsStrictDescendant(resolved, case_root)) {
      throw CaseError(context + ".case_ref escapes business_api_v1.");
    }
    entries.push_back(std::move(entry));
  }
  return entries;
}

const std::set<std::string>& SupportedPayloadTypes()
{
  static const std::set<std::string> values = {
      "ProjectCreateRequest", "ProductCreateRequest", "RecipeCreateRequest",
      "RecipeRevisionCreateRequest", "InspectionItemDefineRequest",
      "ImageRegisterRequest", "AnnotationUpsertRequest", "RoiDefineRequest",
      "TemplateDefineRequest", "RevisionFreezeRequest", "TrainingStartRequest",
      "TrainingCompleteRequest", "ModelRegisterRequest", "EvaluationRecordRequest",
      "ModelPromoteRequest", "ProductionRunStartRequest",
      "ProductionResultRecordRequest", "HumanReviewCommitRequest",
      "FeedbackSubmitRequest", "RetrainingEnqueueRequest", "RecipeRollbackRequest",
      "SnapshotReadRequest"};
  return values;
}

void RequireCoreExpectation(const json& expected, const std::string& context)
{
  if (!expected.is_object() || !expected.contains("status") ||
      !expected.at("status").is_string() || !expected.contains("code") ||
      !expected.at("code").is_string()) {
    throw CaseError(context + ".expect must declare string status and code.");
  }
}

CaseRecord ExecuteCaseFile(
    const fs::path& path,
    const fs::path& case_root,
    const std::map<std::string, const business::CapabilityDescriptor*>& capabilities)
{
  CaseRecord record;
  record.source_path = path;
  try {
    const json document = ReadJsonFile(path);
    const std::string source_context =
        NormalizedRelativePath(fs::relative(path, case_root));
    if (!document.is_object()) {
      throw CaseError(source_context + " must contain a JSON object.");
    }
    if (Required<std::string>(document, "schema_version", source_context) != kCaseSchema) {
      throw CaseError(source_context + " has an unsupported schema_version.");
    }
    record.case_id = Required<std::string>(document, "case_id", source_context);
    record.display_name = Required<std::string>(document, "display_name", source_context);
    record.primary_capability_id =
        Required<std::string>(document, "primary_capability_id", source_context);
    if (record.case_id.empty() || record.primary_capability_id.empty()) {
      throw CaseError(source_context + " has an empty case or capability identity.");
    }
    const auto descriptor = capabilities.find(record.primary_capability_id);
    if (descriptor == capabilities.end()) {
      throw CaseError(source_context + " has unknown primary_capability_id '" +
                      record.primary_capability_id + "'.");
    }
    record.request_type = descriptor->second->request_type;
    if (Required<std::string>(document, "test_interface_id", source_context) !=
        kTestInterfaceId) {
      throw CaseError(source_context + " names a different test_interface_id.");
    }
    if (Required<std::string>(document, "repository_scope", source_context) !=
        "FRESH_PER_SCENARIO") {
      throw CaseError(source_context + " must use FRESH_PER_SCENARIO.");
    }
    if (Required<std::string>(document, "fixture_scope", source_context) != "TEST_ONLY") {
      throw CaseError(source_context + " must mark fixtures TEST_ONLY.");
    }
    if (!document.contains("api_version") || !document.at("api_version").is_object()) {
      throw CaseError(source_context + ".api_version must be an object.");
    }
    business::ApiVersion api_version;
    api_version.major = Required<std::uint32_t>(document.at("api_version"), "major",
                                                source_context + ".api_version");
    api_version.minor = Required<std::uint32_t>(document.at("api_version"), "minor",
                                                source_context + ".api_version");
    if (api_version.major != business::kApiMajorVersion ||
        api_version.minor != business::kApiMinorVersion) {
      throw CaseError(source_context + " targets a different compiled API version.");
    }
    if (!document.contains("scenarios") || !document.at("scenarios").is_array() ||
        document.at("scenarios").empty()) {
      throw CaseError(source_context + ".scenarios must be a non-empty array.");
    }

    record.schema_valid = true;
    std::set<std::string> scenario_ids;
    std::size_t scenario_index = 0;
    for (const json& scenario_document : document.at("scenarios")) {
      ScenarioRecord scenario;
      try {
        const std::string context = source_context + ".scenarios[" +
                                    std::to_string(scenario_index++) + "]";
        scenario.scenario_id = Required<std::string>(scenario_document, "scenario_id", context);
        scenario.kind = Required<std::string>(scenario_document, "kind", context);
        if (scenario.scenario_id.empty() || !scenario_ids.insert(scenario.scenario_id).second) {
          throw CaseError(context + " has an empty or duplicate scenario_id.");
        }
        if (scenario.kind != "SUCCESS" && scenario.kind != "NEGATIVE_OR_BOUNDARY") {
          throw CaseError(context + " has unknown scenario kind '" + scenario.kind + "'.");
        }
        if (scenario_document.contains("cross_cutting_tags")) {
          if (!scenario_document.at("cross_cutting_tags").is_array()) {
            throw CaseError(context + ".cross_cutting_tags must be an array.");
          }
          std::set<std::string> unique_tags;
          for (const json& tag : scenario_document.at("cross_cutting_tags")) {
            if (!tag.is_string() || tag.get<std::string>().empty() ||
                !unique_tags.insert(tag.get<std::string>()).second) {
              throw CaseError(context +
                              ".cross_cutting_tags contains an empty, duplicate, or non-string tag.");
            }
            scenario.cross_cutting_tags.push_back(tag.get<std::string>());
          }
        }
        if (!scenario_document.contains("setup") ||
            !scenario_document.at("setup").is_array()) {
          throw CaseError(context + ".setup must be an array.");
        }
        if (!scenario_document.contains("request")) {
          throw CaseError(context + " requires a primary request.");
        }

        std::vector<ParsedRequest> setup_requests;
        std::size_t setup_index = 0;
        for (const json& setup_document : scenario_document.at("setup")) {
          ParsedRequest setup = ParseRequest(
              setup_document, api_version, capabilities,
              context + ".setup[" + std::to_string(setup_index++) + "]");
          RequireCoreExpectation(setup.expect, context + ".setup.expect");
          setup_requests.push_back(std::move(setup));
        }
        ParsedRequest primary = ParseRequest(scenario_document.at("request"), api_version,
                                             capabilities, context + ".request");
        RequireCoreExpectation(primary.expect, context + ".request.expect");
        if (primary.capability_id != record.primary_capability_id) {
          throw CaseError(context + ".request capability does not equal primary_capability_id.");
        }
        if (primary.payload_type != record.request_type) {
          throw CaseError(context + ".request payload_type does not equal the catalog request_type.");
        }
        std::vector<ParsedRequest> follow_up_requests;
        if (scenario_document.contains("follow_up_requests")) {
          if (!scenario_document.at("follow_up_requests").is_array()) {
            throw CaseError(context + ".follow_up_requests must be an array.");
          }
          std::size_t follow_up_index = 0;
          for (const json& follow_up_document : scenario_document.at("follow_up_requests")) {
            ParsedRequest follow_up = ParseRequest(
                follow_up_document, api_version, capabilities,
                context + ".follow_up_requests[" +
                    std::to_string(follow_up_index++) + "]");
            RequireCoreExpectation(follow_up.expect,
                                   context + ".follow_up_requests.expect");
            follow_up_requests.push_back(std::move(follow_up));
          }
        }

        std::unique_ptr<business::IBusinessWorkflowTestApi> api =
            business::MakeCompiledBusinessWorkflowTestApi();
        if (!api) {
          throw CaseError("MakeCompiledBusinessWorkflowTestApi returned null.");
        }
        bool setup_passed = true;
        std::size_t ordinal = 0;
        for (const ParsedRequest& setup : setup_requests) {
          ExecutionRecord execution = ExecuteRequest(*api, setup, "SETUP", ordinal++);
          setup_passed = setup_passed && execution.passed;
          scenario.setup.push_back(std::move(execution));
          if (!setup_passed) {
            scenario.errors.push_back("A setup request failed; primary API invocation was withheld.");
            break;
          }
        }
        scenario.primary.phase = "PRIMARY";
        scenario.primary.ordinal = ordinal;
        scenario.primary.request = primary.source;
        scenario.primary.request_digest = DigestJson(primary.source);
        if (setup_passed && scenario.setup.size() == setup_requests.size()) {
          scenario.primary = ExecuteRequest(*api, primary, "PRIMARY", ordinal);
        } else {
          scenario.primary.error = "SETUP_FAILED";
          scenario.primary.started_at = UtcTimestamp();
          scenario.primary.completed_at = scenario.primary.started_at;
        }
        bool follow_up_passed = true;
        if (scenario.primary.passed) {
          for (const ParsedRequest& follow_up : follow_up_requests) {
            ExecutionRecord execution =
                ExecuteRequest(*api, follow_up, "FOLLOW_UP", ++ordinal);
            follow_up_passed = follow_up_passed && execution.passed;
            scenario.follow_up.push_back(std::move(execution));
            if (!follow_up_passed) {
              scenario.errors.push_back(
                  "A follow-up request assertion failed; later follow-ups were withheld.");
              break;
            }
          }
        } else if (!follow_up_requests.empty()) {
          scenario.errors.push_back(
              "Primary request failed; follow-up API invocations were withheld.");
        }
        scenario.passed = setup_passed &&
                          scenario.setup.size() == setup_requests.size() &&
                          scenario.primary.passed && follow_up_passed &&
                          scenario.follow_up.size() == follow_up_requests.size();
      } catch (const std::exception& exception) {
        record.schema_valid = false;
        scenario.errors.push_back(exception.what());
        scenario.passed = false;
      }

      if (scenario.kind == "SUCCESS") {
        if (!record.success_present) {
          record.success_passed = true;
        }
        record.success_present = true;
        record.success_passed = record.success_passed && scenario.passed;
      } else if (scenario.kind == "NEGATIVE_OR_BOUNDARY") {
        if (!record.boundary_present) {
          record.boundary_passed = true;
        }
        record.boundary_present = true;
        record.boundary_passed = record.boundary_passed && scenario.passed;
      }
      record.scenarios.push_back(std::move(scenario));
    }
    if (!record.success_present) {
      record.errors.push_back("SUCCESS scenario is missing.");
    }
    if (!record.boundary_present) {
      record.errors.push_back("NEGATIVE_OR_BOUNDARY scenario is missing.");
    }
    record.passed = record.schema_valid && record.success_present && record.success_passed &&
                    record.boundary_present && record.boundary_passed && record.errors.empty();
  } catch (const std::exception& exception) {
    record.schema_valid = false;
    record.passed = false;
    record.errors.push_back(exception.what());
  }
  return record;
}

json SerializeAssertion(const AssertionRecord& assertion)
{
  return {{"field", assertion.field}, {"expected", assertion.expected},
          {"actual", assertion.actual}, {"passed", assertion.passed},
          {"detail", assertion.detail}};
}

json SerializeExecution(const ExecutionRecord& execution,
                        const Options& options,
                        const bool counts_toward_coverage)
{
  json assertions = json::array();
  for (const AssertionRecord& assertion : execution.assertions) {
    assertions.push_back(SerializeAssertion(assertion));
  }
  const char* coverage_role = counts_toward_coverage
                                  ? "PRIMARY_CAPABILITY_CASE"
                                  : (execution.phase == "FOLLOW_UP"
                                         ? "CROSS_CUTTING_INVARIANT"
                                         : "SETUP_PREREQUISITE");
  return {{"phase", execution.phase},
          {"ordinal", execution.ordinal},
          {"started_at", execution.started_at},
          {"completed_at", execution.completed_at},
          {"verification_scope", kExecutionClaim},
          {"execution_claim", kExecutionClaim},
          {"real_provider_execution_claim", false},
          {"fixture_scope", "TEST_ONLY"},
          {"coverage_role", coverage_role},
          {"counts_toward_coverage", counts_toward_coverage},
          {"api_invoked", execution.api_invoked},
          {"api_id", kApiId},
          {"test_interface_id", kTestInterfaceId},
          {"api_version", {{"major", business::kApiMajorVersion},
                            {"minor", business::kApiMinorVersion}}},
          {"implementation_build_id", options.implementation_build_id},
          {"requested_implementation_build_id",
           options.requested_implementation_build_id},
          {"sdk_build_identity", options.sdk_build_identity},
          {"implementation_build_id_source", kBuildIdSource},
          {"implementation_build_id_verification_status",
           kBuildIdVerificationStatus},
          {"request_digest", execution.request_digest},
          {"request", execution.request},
          {"response", execution.response},
          {"snapshot_before", execution.snapshot_before},
          {"snapshot_after", execution.snapshot_after},
          {"assertions", std::move(assertions)},
          {"assertions_passed", execution.passed},
          {"error", execution.error}};
}

json SerializeScenario(const ScenarioRecord& scenario, const Options& options)
{
  json setup = json::array();
  for (const ExecutionRecord& execution : scenario.setup) {
    setup.push_back(SerializeExecution(execution, options, false));
  }
  json follow_up = json::array();
  for (const ExecutionRecord& execution : scenario.follow_up) {
    follow_up.push_back(SerializeExecution(execution, options, false));
  }
  json primary = SerializeExecution(scenario.primary, options, true);
  return {{"scenario_id", scenario.scenario_id},
          {"kind", scenario.kind},
          {"cross_cutting_tags", scenario.cross_cutting_tags},
          {"verification_scope", kExecutionClaim},
          {"execution_claim", kExecutionClaim},
          {"real_provider_execution_claim", false},
          {"fixture_scope", "TEST_ONLY"},
          {"api_invoked", scenario.primary.api_invoked},
          {"api_id", kApiId},
          {"test_interface_id", kTestInterfaceId},
          {"api_version", {{"major", business::kApiMajorVersion},
                            {"minor", business::kApiMinorVersion}}},
          {"implementation_build_id", options.implementation_build_id},
          {"requested_implementation_build_id",
           options.requested_implementation_build_id},
          {"sdk_build_identity", options.sdk_build_identity},
          {"implementation_build_id_source", kBuildIdSource},
          {"implementation_build_id_verification_status",
           kBuildIdVerificationStatus},
          {"request_digest", scenario.primary.request_digest},
          {"response", scenario.primary.response},
          {"snapshot_before", scenario.primary.snapshot_before},
          {"snapshot_after", scenario.primary.snapshot_after},
          {"assertions", primary.at("assertions")},
          {"setup_executions", std::move(setup)},
          {"primary_execution", std::move(primary)},
          {"follow_up_executions", std::move(follow_up)},
          {"assertions_passed", scenario.passed},
          {"errors", scenario.errors}};
}

json SerializeCase(const CaseRecord& record,
                   const Options& options,
                   const fs::path& case_root)
{
  json scenarios = json::array();
  for (const ScenarioRecord& scenario : record.scenarios) {
    scenarios.push_back(SerializeScenario(scenario, options));
  }
  return {{"schema_version", "visionai.business_api_case_result.v1"},
          {"run_id", options.run_id},
          {"recorded_at", UtcTimestamp()},
          {"verification_scope", kExecutionClaim},
          {"execution_claim", kExecutionClaim},
          {"real_provider_execution_claim", false},
          {"case_id", record.case_id},
          {"display_name", record.display_name},
          {"capability_id", record.primary_capability_id},
          {"request_type", record.request_type},
          {"case_ref", NormalizedRelativePath(fs::relative(record.source_path, case_root))},
          {"api_id", kApiId},
          {"test_interface_id", kTestInterfaceId},
          {"api_version", {{"major", business::kApiMajorVersion},
                            {"minor", business::kApiMinorVersion}}},
          {"implementation_build_id", options.implementation_build_id},
          {"requested_implementation_build_id",
           options.requested_implementation_build_id},
          {"sdk_build_identity", options.sdk_build_identity},
          {"implementation_build_id_source", kBuildIdSource},
          {"implementation_build_id_verification_status",
           kBuildIdVerificationStatus},
          {"repository_scope", "FRESH_PER_SCENARIO"},
          {"fixture_scope", "TEST_ONLY"},
          {"schema_valid", record.schema_valid},
          {"success_present", record.success_present},
          {"success_passed", record.success_passed},
          {"negative_or_boundary_present", record.boundary_present},
          {"negative_or_boundary_passed", record.boundary_passed},
          {"scenario_results", std::move(scenarios)},
          {"assertions_passed", record.passed},
          {"errors", record.errors}};
}

struct CoverageDiagnostics
{
  bool api_catalog_valid = true;
  bool traceability_catalog_valid = true;
  bool catalog_exact_match = true;
  bool configuration_fixture_valid = true;
  bool configuration_exact_match = true;
  std::string configuration_error;
  bool build_identity_valid = true;
  bool build_identity_cli_match = true;
  std::string build_identity_error;
  std::vector<std::string> catalog_errors;
  std::vector<std::string> unknown_capabilities;
  std::vector<std::string> duplicate_primary_capabilities;
  std::vector<std::string> uncovered_capabilities;
  std::vector<std::string> duplicate_case_ids;
  std::vector<std::string> unknown_or_uncovered_payload_types;
  std::vector<std::string> case_contract_errors;
  std::vector<std::string> failed_success_capabilities;
  std::vector<std::string> failed_negative_or_boundary_capabilities;
  std::vector<std::string> unknown_cross_cutting_invariants;
  std::vector<std::string> uncovered_cross_cutting_invariants;
};

json SerializeApiCapabilities(const std::vector<business::CapabilityDescriptor>& descriptors)
{
  json result = json::array();
  for (const business::CapabilityDescriptor& descriptor : descriptors) {
    result.push_back({{"capability_id", descriptor.stable_name},
                      {"request_type", descriptor.request_type},
                      {"lifecycle_area", descriptor.lifecycle_area},
                      {"mutates_state", descriptor.mutates_state},
                      {"requires_event_id", descriptor.requires_event_id},
                      {"requires_version_binding", descriptor.requires_version_binding},
                      {"legacy_action_aliases", descriptor.legacy_action_aliases}});
  }
  return result;
}

json SerializeTraceabilityCapabilities(const std::vector<CatalogEntry>& entries)
{
  json result = json::array();
  for (const CatalogEntry& entry : entries) {
    result.push_back({{"capability_id", entry.capability_id},
                      {"request_type", entry.request_type},
                      {"implementation_symbol", entry.implementation_symbol},
                      {"case_id", entry.case_id},
                      {"case_ref", entry.case_ref.generic_string()}});
  }
  return result;
}

std::map<std::string, const business::CapabilityDescriptor*> BuildCapabilityMap(
    const std::vector<business::CapabilityDescriptor>& descriptors,
    CoverageDiagnostics& diagnostics)
{
  std::map<std::string, const business::CapabilityDescriptor*> result;
  std::set<int> enum_values;
  std::set<std::string> request_types;
  for (const business::CapabilityDescriptor& descriptor : descriptors) {
    const std::string stable_name = descriptor.stable_name == nullptr
                                        ? ""
                                        : descriptor.stable_name;
    const std::string request_type = descriptor.request_type == nullptr
                                         ? ""
                                         : descriptor.request_type;
    if (stable_name.empty() || request_type.empty()) {
      diagnostics.api_catalog_valid = false;
      diagnostics.catalog_errors.push_back(
          "Capabilities() returned an empty stable_name or request_type.");
      continue;
    }
    if (!result.emplace(stable_name, &descriptor).second) {
      diagnostics.api_catalog_valid = false;
      diagnostics.catalog_errors.push_back(
          "Capabilities() contains duplicate stable_name: " + stable_name);
    }
    if (!enum_values.insert(static_cast<int>(descriptor.id)).second) {
      diagnostics.api_catalog_valid = false;
      diagnostics.catalog_errors.push_back(
          "Capabilities() contains a duplicate CapabilityId ordinal.");
    }
    if (!request_types.insert(request_type).second) {
      diagnostics.api_catalog_valid = false;
      diagnostics.catalog_errors.push_back(
          "Capabilities() contains duplicate request_type: " + request_type);
    }
    if (SupportedPayloadTypes().find(request_type) == SupportedPayloadTypes().end()) {
      diagnostics.api_catalog_valid = false;
      diagnostics.unknown_or_uncovered_payload_types.push_back(request_type);
    }
    if (business::FindBusinessCapability(descriptor.id) != &descriptor ||
        std::string(business::ToStableName(descriptor.id)) != stable_name) {
      diagnostics.api_catalog_valid = false;
      diagnostics.catalog_errors.push_back(
          "Capability lookup functions disagree for: " + stable_name);
    }
  }
  if (descriptors.size() != SupportedPayloadTypes().size()) {
    diagnostics.api_catalog_valid = false;
    diagnostics.catalog_errors.push_back(
        "Capabilities() count does not equal the 22 payload parsers in the v1 bridge.");
  }
  for (const std::string& payload_type : SupportedPayloadTypes()) {
    if (request_types.find(payload_type) == request_types.end()) {
      diagnostics.unknown_or_uncovered_payload_types.push_back(payload_type);
    }
  }
  return result;
}

void ValidateTraceSettings(const json& trace,
                           const std::size_t api_capability_count,
                           CoverageDiagnostics& diagnostics)
{
  try {
    const json& boundary = trace.at("product_boundary");
    if (!boundary.is_object() ||
        Required<std::string>(boundary, "runtime_dependency_policy", "product_boundary") !=
            "COMPILED_API_ONLY") {
      throw CaseError("product_boundary.runtime_dependency_policy must be COMPILED_API_ONLY.");
    }
    const json& contract = trace.at("catalog_contract");
    if (Required<std::size_t>(contract, "expected_capability_count", "catalog_contract") !=
        api_capability_count) {
      throw CaseError("catalog_contract.expected_capability_count differs from Capabilities().");
    }
    const json& gate = trace.at("coverage_gate");
    if (!gate.is_object() ||
        Required<std::size_t>(gate, "required_primary_case_count", "coverage_gate") !=
            api_capability_count ||
        Required<std::size_t>(gate, "required_success_coverage_count", "coverage_gate") !=
            api_capability_count ||
        Required<std::size_t>(gate,
                              "required_negative_or_boundary_coverage_count",
                              "coverage_gate") != api_capability_count ||
        Required<bool>(gate, "setup_counts_as_primary_coverage", "coverage_gate") ||
        !Required<bool>(gate, "unknown_capabilities_fail", "coverage_gate") ||
        !Required<bool>(gate, "duplicate_primary_capabilities_fail", "coverage_gate") ||
        !Required<bool>(gate, "uncovered_capabilities_fail", "coverage_gate") ||
        Required<std::string>(gate, "cross_cutting_tag_field", "coverage_gate") !=
            "cross_cutting_tags" ||
        !Required<bool>(gate, "cross_cutting_coverage_requires_tagged_scenario_pass",
                        "coverage_gate") ||
        !Required<bool>(gate, "uncovered_cross_cutting_invariants_fail",
                        "coverage_gate") ||
        !Required<bool>(gate, "assertion_failure_fails", "coverage_gate") ||
        !Required<bool>(gate, "pass_requires_zero_gaps", "coverage_gate")) {
      throw CaseError("coverage_gate does not express the strict Capabilities() truth gate.");
    }
    const json& evidence = trace.at("evidence_contract");
    if (!evidence.is_object() ||
        Required<std::string>(evidence, "claim_scope", "evidence_contract") !=
            kExecutionClaim) {
      throw CaseError(
          "evidence_contract.claim_scope must be COMPILED_BUSINESS_API_EXECUTION.");
    }
    const json& configuration = trace.at("configuration_consistency_gate");
    if (!configuration.is_object() ||
        !Required<bool>(configuration, "enabled", "configuration_consistency_gate") ||
        Required<std::string>(configuration, "fixture_ref",
                              "configuration_consistency_gate") !=
            "base_configuration_contract.json" ||
        Required<std::string>(configuration, "fixture_scope",
                              "configuration_consistency_gate") != "TEST_ONLY" ||
        Required<std::string>(configuration, "runtime_role",
                              "configuration_consistency_gate") !=
            "NOT_RUNTIME_CONFIGURATION" ||
        Required<std::string>(configuration, "sdk_interface_id",
                              "configuration_consistency_gate") !=
            "vision_ai.business.v1.IBusinessWorkflowApi" ||
        Required<std::string>(configuration, "sdk_method",
                              "configuration_consistency_gate") !=
            "ReadConfigurationContract" ||
        Required<std::string>(configuration, "comparison_mode",
                              "configuration_consistency_gate") !=
            "EXACT_REQUIRED_FIELDS" ||
        !Required<bool>(configuration, "mismatch_fails",
                        "configuration_consistency_gate")) {
      throw CaseError("configuration_consistency_gate is not the strict SDK comparison gate.");
    }
  } catch (const std::exception& exception) {
    diagnostics.traceability_catalog_valid = false;
    diagnostics.catalog_exact_match = false;
    diagnostics.catalog_errors.push_back(exception.what());
  }
}

std::set<std::string> ParseRequiredCrossCuttingInvariants(const json& trace)
{
  const json& gate = trace.at("coverage_gate");
  if (!gate.contains("required_cross_cutting_invariants") ||
      !gate.at("required_cross_cutting_invariants").is_array() ||
      gate.at("required_cross_cutting_invariants").empty()) {
    throw CaseError(
        "coverage_gate.required_cross_cutting_invariants must be a non-empty array.");
  }
  std::set<std::string> required;
  for (const json& tag : gate.at("required_cross_cutting_invariants")) {
    if (!tag.is_string() || tag.get<std::string>().empty() ||
        !required.insert(tag.get<std::string>()).second) {
      throw CaseError(
          "coverage_gate.required_cross_cutting_invariants has an invalid duplicate.");
    }
  }
  return required;
}

void ValidateCatalogAndCaseMapping(
    const std::vector<CatalogEntry>& trace_entries,
    const std::vector<fs::path>& discovered_paths,
    const std::vector<CaseRecord>& cases,
    const fs::path& case_root,
    const std::map<std::string, const business::CapabilityDescriptor*>& capabilities,
    CoverageDiagnostics& diagnostics)
{
  std::map<std::string, const CatalogEntry*> trace_by_capability;
  std::set<std::string> trace_case_ids;
  std::set<std::string> trace_case_refs;
  for (const CatalogEntry& entry : trace_entries) {
    if (!trace_by_capability.emplace(entry.capability_id, &entry).second) {
      diagnostics.catalog_exact_match = false;
      diagnostics.catalog_errors.push_back(
          "Traceability catalog duplicates capability_id: " + entry.capability_id);
    }
    if (!trace_case_ids.insert(entry.case_id).second) {
      diagnostics.catalog_exact_match = false;
      diagnostics.catalog_errors.push_back(
          "Traceability catalog duplicates case_id: " + entry.case_id);
    }
    const std::string case_ref = NormalizedRelativePath(entry.case_ref);
    if (!trace_case_refs.insert(case_ref).second) {
      diagnostics.catalog_exact_match = false;
      diagnostics.catalog_errors.push_back(
          "Traceability catalog duplicates case_ref: " + case_ref);
    }
    const auto api = capabilities.find(entry.capability_id);
    if (api == capabilities.end()) {
      diagnostics.catalog_exact_match = false;
      diagnostics.unknown_capabilities.push_back(entry.capability_id);
    } else if (entry.request_type != api->second->request_type) {
      diagnostics.catalog_exact_match = false;
      diagnostics.catalog_errors.push_back(
          "Traceability request_type mismatch for " + entry.capability_id + ": " +
          entry.request_type + " != " + api->second->request_type);
    }
  }
  for (const auto& capability : capabilities) {
    if (trace_by_capability.find(capability.first) == trace_by_capability.end()) {
      diagnostics.catalog_exact_match = false;
      diagnostics.uncovered_capabilities.push_back(capability.first);
    }
  }

  std::set<std::string> discovered_refs;
  for (const fs::path& path : discovered_paths) {
    discovered_refs.insert(NormalizedRelativePath(fs::relative(path, case_root)));
  }
  if (discovered_refs != trace_case_refs) {
    diagnostics.catalog_exact_match = false;
    for (const std::string& ref : discovered_refs) {
      if (trace_case_refs.find(ref) == trace_case_refs.end()) {
        diagnostics.case_contract_errors.push_back(
            "Discovered business_api_case.json is absent from traceability catalog: " + ref);
      }
    }
    for (const std::string& ref : trace_case_refs) {
      if (discovered_refs.find(ref) == discovered_refs.end()) {
        diagnostics.case_contract_errors.push_back(
            "Traceability case_ref was not discovered: " + ref);
      }
    }
  }

  std::map<std::string, const CaseRecord*> primary_cases;
  std::set<std::string> case_ids;
  std::set<std::string> primary_payload_types;
  for (const CaseRecord& record : cases) {
    const std::string source_ref =
        NormalizedRelativePath(fs::relative(record.source_path, case_root));
    if (record.case_id.empty()) {
      diagnostics.case_contract_errors.push_back(
          "Case could not expose a case_id: " + source_ref);
    } else if (!case_ids.insert(record.case_id).second) {
      diagnostics.duplicate_case_ids.push_back(record.case_id);
    }
    if (record.primary_capability_id.empty()) {
      diagnostics.case_contract_errors.push_back(
          "Case could not expose a primary capability: " + source_ref);
      continue;
    }
    if (capabilities.find(record.primary_capability_id) == capabilities.end()) {
      diagnostics.unknown_capabilities.push_back(record.primary_capability_id);
      continue;
    }
    if (!primary_cases.emplace(record.primary_capability_id, &record).second) {
      diagnostics.duplicate_primary_capabilities.push_back(record.primary_capability_id);
    }
    primary_payload_types.insert(record.request_type);
    const auto trace = trace_by_capability.find(record.primary_capability_id);
    if (trace == trace_by_capability.end()) {
      diagnostics.catalog_exact_match = false;
      diagnostics.case_contract_errors.push_back(
          "Primary case is absent from traceability catalog: " + record.primary_capability_id);
    } else {
      if (trace->second->case_id != record.case_id) {
        diagnostics.catalog_exact_match = false;
        diagnostics.case_contract_errors.push_back(
            "Traceability case_id mismatch for " + record.primary_capability_id);
      }
      if (NormalizedRelativePath(trace->second->case_ref) != source_ref) {
        diagnostics.catalog_exact_match = false;
        diagnostics.case_contract_errors.push_back(
            "Traceability case_ref mismatch for " + record.primary_capability_id);
      }
    }
    if (!record.schema_valid) {
      diagnostics.case_contract_errors.push_back(
          "Case schema failed for " + record.primary_capability_id);
    }
    for (const std::string& error : record.errors) {
      diagnostics.case_contract_errors.push_back(source_ref + ": " + error);
    }
    for (const ScenarioRecord& scenario : record.scenarios) {
      for (const std::string& error : scenario.errors) {
        diagnostics.case_contract_errors.push_back(
            source_ref + "#" + scenario.scenario_id + ": " + error);
      }
    }
    if (!record.success_present || !record.success_passed) {
      diagnostics.failed_success_capabilities.push_back(record.primary_capability_id);
    }
    if (!record.boundary_present || !record.boundary_passed) {
      diagnostics.failed_negative_or_boundary_capabilities.push_back(
          record.primary_capability_id);
    }
  }
  for (const auto& capability : capabilities) {
    if (primary_cases.find(capability.first) == primary_cases.end()) {
      diagnostics.uncovered_capabilities.push_back(capability.first);
    }
  }
  for (const std::string& payload_type : SupportedPayloadTypes()) {
    if (primary_payload_types.find(payload_type) == primary_payload_types.end()) {
      diagnostics.unknown_or_uncovered_payload_types.push_back(payload_type);
    }
  }
}

template <typename T>
void SortAndUnique(std::vector<T>& values)
{
  std::sort(values.begin(), values.end());
  values.erase(std::unique(values.begin(), values.end()), values.end());
}

void NormalizeDiagnostics(CoverageDiagnostics& diagnostics)
{
  SortAndUnique(diagnostics.catalog_errors);
  SortAndUnique(diagnostics.unknown_capabilities);
  SortAndUnique(diagnostics.duplicate_primary_capabilities);
  SortAndUnique(diagnostics.uncovered_capabilities);
  SortAndUnique(diagnostics.duplicate_case_ids);
  SortAndUnique(diagnostics.unknown_or_uncovered_payload_types);
  SortAndUnique(diagnostics.case_contract_errors);
  SortAndUnique(diagnostics.failed_success_capabilities);
  SortAndUnique(diagnostics.failed_negative_or_boundary_capabilities);
  SortAndUnique(diagnostics.unknown_cross_cutting_invariants);
  SortAndUnique(diagnostics.uncovered_cross_cutting_invariants);
}

json MakeExecutionAudit(const Options& options,
                        const CaseRecord& case_record,
                        const ScenarioRecord& scenario,
                        const ExecutionRecord& execution,
                        const bool counts_toward_coverage,
                        const fs::path& case_root)
{
  json assertions = json::array();
  for (const AssertionRecord& assertion : execution.assertions) {
    assertions.push_back(SerializeAssertion(assertion));
  }
  const char* coverage_role = counts_toward_coverage
                                  ? "PRIMARY_CAPABILITY_CASE"
                                  : (execution.phase == "FOLLOW_UP"
                                         ? "CROSS_CUTTING_INVARIANT"
                                         : "SETUP_PREREQUISITE");
  return {{"event_type", "BUSINESS_API_CALL"},
          {"recorded_at", execution.completed_at.empty() ? UtcTimestamp()
                                                          : execution.completed_at},
          {"started_at", execution.started_at},
          {"completed_at", execution.completed_at},
          {"run_id", options.run_id},
          {"verification_scope", kExecutionClaim},
          {"execution_claim", kExecutionClaim},
          {"real_provider_execution_claim", false},
          {"fixture_scope", "TEST_ONLY"},
          {"case_id", case_record.case_id},
          {"case_ref", NormalizedRelativePath(
                           fs::relative(case_record.source_path, case_root))},
          {"capability_id", case_record.primary_capability_id},
          {"scenario_id", scenario.scenario_id},
          {"scenario_kind", scenario.kind},
          {"phase", execution.phase},
          {"coverage_role", coverage_role},
          {"counts_toward_coverage", counts_toward_coverage},
          {"api_invoked", execution.api_invoked},
          {"api_id", kApiId},
          {"test_interface_id", kTestInterfaceId},
          {"api_version", {{"major", business::kApiMajorVersion},
                            {"minor", business::kApiMinorVersion}}},
          {"implementation_build_id", options.implementation_build_id},
          {"requested_implementation_build_id",
           options.requested_implementation_build_id},
          {"sdk_build_identity", options.sdk_build_identity},
          {"implementation_build_id_source", kBuildIdSource},
          {"implementation_build_id_verification_status",
           kBuildIdVerificationStatus},
          {"request_digest", execution.request_digest},
          {"request", execution.request},
          {"response", execution.response},
          {"snapshot_before", execution.snapshot_before},
          {"snapshot_after", execution.snapshot_after},
          {"assertions", std::move(assertions)},
          {"assertions_passed", execution.passed},
          {"error", execution.error}};
}

std::vector<std::string> FlattenGaps(const CoverageDiagnostics& diagnostics)
{
  std::vector<std::string> gaps;
  const auto append = [&gaps](const std::string& prefix,
                              const std::vector<std::string>& values) {
    for (const std::string& value : values) {
      gaps.push_back(prefix + value);
    }
  };
  append("CATALOG: ", diagnostics.catalog_errors);
  append("UNKNOWN_CAPABILITY: ", diagnostics.unknown_capabilities);
  append("DUPLICATE_PRIMARY_CAPABILITY: ",
         diagnostics.duplicate_primary_capabilities);
  append("UNCOVERED_CAPABILITY: ", diagnostics.uncovered_capabilities);
  append("DUPLICATE_CASE_ID: ", diagnostics.duplicate_case_ids);
  append("PAYLOAD_TYPE_GAP: ", diagnostics.unknown_or_uncovered_payload_types);
  append("CASE_CONTRACT: ", diagnostics.case_contract_errors);
  append("SUCCESS_FAILED: ", diagnostics.failed_success_capabilities);
  append("NEGATIVE_OR_BOUNDARY_FAILED: ",
         diagnostics.failed_negative_or_boundary_capabilities);
  append("UNKNOWN_CROSS_CUTTING_INVARIANT: ",
         diagnostics.unknown_cross_cutting_invariants);
  append("UNCOVERED_CROSS_CUTTING_INVARIANT: ",
         diagnostics.uncovered_cross_cutting_invariants);
  if (!diagnostics.configuration_fixture_valid ||
      !diagnostics.configuration_exact_match) {
    gaps.push_back("CONFIGURATION_CONSISTENCY: " + diagnostics.configuration_error);
  }
  if (!diagnostics.build_identity_valid ||
      !diagnostics.build_identity_cli_match) {
    gaps.push_back("BUILD_IDENTITY: " + diagnostics.build_identity_error);
  }
  return gaps;
}

bool DiagnosticsEmpty(const CoverageDiagnostics& diagnostics)
{
  return diagnostics.catalog_errors.empty() &&
         diagnostics.unknown_capabilities.empty() &&
         diagnostics.duplicate_primary_capabilities.empty() &&
         diagnostics.uncovered_capabilities.empty() &&
         diagnostics.duplicate_case_ids.empty() &&
         diagnostics.unknown_or_uncovered_payload_types.empty() &&
         diagnostics.case_contract_errors.empty() &&
         diagnostics.failed_success_capabilities.empty() &&
         diagnostics.failed_negative_or_boundary_capabilities.empty() &&
         diagnostics.unknown_cross_cutting_invariants.empty() &&
         diagnostics.uncovered_cross_cutting_invariants.empty() &&
         diagnostics.configuration_fixture_valid &&
         diagnostics.configuration_exact_match &&
         diagnostics.build_identity_valid &&
         diagnostics.build_identity_cli_match;
}

int RunEvidence(Options options)
{
  const std::string started_at = UtcTimestamp();
  CoverageDiagnostics diagnostics;

  std::unique_ptr<business::IBusinessWorkflowTestApi> catalog_probe =
      business::MakeCompiledBusinessWorkflowTestApi();
  if (!catalog_probe) {
    throw CaseError("MakeCompiledBusinessWorkflowTestApi returned null.");
  }
  const std::vector<business::CapabilityDescriptor>& api_descriptors =
      catalog_probe->Capabilities();
  const std::map<std::string, const business::CapabilityDescriptor*> capabilities =
      BuildCapabilityMap(api_descriptors, diagnostics);

  const business::BusinessBuildIdentity& build_identity =
      catalog_probe->ReadBuildIdentity();
  const std::string build_identity_checked_at = UtcTimestamp();
  options.sdk_build_identity = SerializeBuildIdentity(build_identity);
  const bool source_revision_valid =
      build_identity.source_revision.size() == 71 &&
      build_identity.source_revision.rfind("sha256:", 0) == 0 &&
      std::all_of(build_identity.source_revision.begin() + 7,
                  build_identity.source_revision.end(), [](const char ch) {
                    return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
                  });
  const std::string source_revision_hex =
      source_revision_valid ? build_identity.source_revision.substr(7) : "";
  const std::string canonical_build_id =
      build_identity.product_line + "@" + build_identity.implementation_version +
      "+sha256-" +
      (source_revision_hex.size() >= 16 ? source_revision_hex.substr(0, 16) : "");
  if (build_identity.product_line != "Vision-AI" ||
      build_identity.api_version.major != business::kApiMajorVersion ||
      build_identity.api_version.minor != business::kApiMinorVersion ||
      build_identity.implementation_version.empty() ||
      !source_revision_valid ||
      build_identity.stable_build_id.empty() ||
      build_identity.stable_build_id != canonical_build_id) {
    diagnostics.build_identity_valid = false;
    diagnostics.build_identity_error =
        "ReadBuildIdentity returned an invalid or non-canonical SDK identity.";
  }
  diagnostics.build_identity_cli_match =
      options.requested_implementation_build_id == build_identity.stable_build_id;
  if (!diagnostics.build_identity_cli_match) {
    if (!diagnostics.build_identity_error.empty()) {
      diagnostics.build_identity_error += " ";
    }
    diagnostics.build_identity_error +=
        "--implementation-build-id does not equal SDK stable_build_id.";
  }
  options.implementation_build_id = build_identity.stable_build_id;

  json configuration_fixture = json::object();
  json expected_configuration = json::object();
  json sdk_configuration = SerializeConfigurationContract(
      catalog_probe->ReadConfigurationContract());
  std::string configuration_fixture_digest;
  try {
    const fs::path configuration_path =
        options.case_root / "base_configuration_contract.json";
    configuration_fixture = ReadJsonFile(configuration_path);
    if (!configuration_fixture.is_object() ||
        Required<std::string>(configuration_fixture, "schema_version",
                              "base_configuration_contract") !=
            "visionai.business_configuration_fixture.v1" ||
        Required<std::string>(configuration_fixture, "fixture_scope",
                              "base_configuration_contract") != "TEST_ONLY" ||
        Required<std::string>(configuration_fixture, "runtime_role",
                              "base_configuration_contract") !=
            "NOT_RUNTIME_CONFIGURATION" ||
        Required<std::string>(configuration_fixture, "sdk_interface_id",
                              "base_configuration_contract") !=
            "vision_ai.business.v1.IBusinessWorkflowApi" ||
        Required<std::string>(configuration_fixture, "sdk_method",
                              "base_configuration_contract") !=
            "ReadConfigurationContract" ||
        Required<std::string>(configuration_fixture, "comparison_mode",
                              "base_configuration_contract") !=
            "EXACT_REQUIRED_FIELDS" ||
        !Required<bool>(configuration_fixture, "mismatch_fails",
                        "base_configuration_contract")) {
      throw CaseError("base_configuration_contract metadata is invalid.");
    }
    if (!configuration_fixture.contains("required_fields") ||
        !configuration_fixture.at("required_fields").is_array() ||
        !configuration_fixture.contains("expected_contract") ||
        !configuration_fixture.at("expected_contract").is_object()) {
      throw CaseError("base_configuration_contract requires required_fields and expected_contract.");
    }
    static const std::set<std::string> required_configuration_fields = {
        "schema_version", "configuration_id", "configuration_revision",
        "default_coordinate_space", "observation_channels",
        "retraining_eligible_review_actions", "immutable_review_receipt",
        "event_id_required_for_mutations", "script_runtime_enabled",
        "digest_algorithm", "canonical_digest"};
    std::set<std::string> declared_fields;
    for (const json& field : configuration_fixture.at("required_fields")) {
      if (!field.is_string() || !declared_fields.insert(field.get<std::string>()).second) {
        throw CaseError("base_configuration_contract.required_fields has invalid duplicates.");
      }
    }
    if (declared_fields != required_configuration_fields) {
      throw CaseError("base_configuration_contract.required_fields is not the exact SDK field set.");
    }
    expected_configuration = configuration_fixture.at("expected_contract");
    std::set<std::string> expected_fields;
    for (auto field = expected_configuration.begin();
         field != expected_configuration.end(); ++field) {
      expected_fields.insert(field.key());
    }
    if (expected_fields != required_configuration_fields) {
      throw CaseError("base_configuration_contract.expected_contract has missing or extra fields.");
    }
    configuration_fixture_digest = DigestJson(configuration_fixture);
    diagnostics.configuration_exact_match = expected_configuration == sdk_configuration;
    if (!diagnostics.configuration_exact_match) {
      diagnostics.configuration_error =
          "SDK ReadConfigurationContract differs from the independent TEST_ONLY fixture.";
    }
  } catch (const std::exception& exception) {
    diagnostics.configuration_fixture_valid = false;
    diagnostics.configuration_exact_match = false;
    diagnostics.configuration_error = exception.what();
  }
  const std::string configuration_checked_at = UtcTimestamp();

  json traceability = json::object();
  std::vector<CatalogEntry> trace_entries;
  std::set<std::string> required_cross_cutting_invariants;
  const fs::path traceability_path = options.case_root / "traceability_manifest.json";
  try {
    traceability = ReadJsonFile(traceability_path);
    trace_entries = ParseTraceabilityCatalog(traceability, options.case_root);
    ValidateTraceSettings(traceability, api_descriptors.size(), diagnostics);
    required_cross_cutting_invariants =
        ParseRequiredCrossCuttingInvariants(traceability);
  } catch (const std::exception& exception) {
    diagnostics.traceability_catalog_valid = false;
    diagnostics.catalog_exact_match = false;
    diagnostics.catalog_errors.push_back(exception.what());
  }

  const std::vector<fs::path> discovered_paths = DiscoverCaseFiles(options.case_root);
  std::vector<CaseRecord> cases;
  cases.reserve(discovered_paths.size());
  for (const fs::path& path : discovered_paths) {
    cases.push_back(ExecuteCaseFile(path, options.case_root, capabilities));
  }
  ValidateCatalogAndCaseMapping(trace_entries, discovered_paths, cases,
                                options.case_root, capabilities, diagnostics);
  NormalizeDiagnostics(diagnostics);

  std::set<std::string> passed_cross_cutting_invariants;
  for (const CaseRecord& record : cases) {
    for (const ScenarioRecord& scenario : record.scenarios) {
      for (const std::string& tag : scenario.cross_cutting_tags) {
        if (required_cross_cutting_invariants.find(tag) ==
            required_cross_cutting_invariants.end()) {
          diagnostics.unknown_cross_cutting_invariants.push_back(tag);
        } else if (scenario.passed) {
          passed_cross_cutting_invariants.insert(tag);
        }
      }
    }
  }
  for (const std::string& tag : required_cross_cutting_invariants) {
    if (passed_cross_cutting_invariants.find(tag) ==
        passed_cross_cutting_invariants.end()) {
      diagnostics.uncovered_cross_cutting_invariants.push_back(tag);
    }
  }
  NormalizeDiagnostics(diagnostics);

  std::set<std::string> unique_primary_capabilities;
  std::set<std::string> success_passed;
  std::set<std::string> boundary_passed;
  std::size_t primary_scenario_count = 0;
  std::size_t primary_api_invoked_count = 0;
  std::size_t setup_api_invoked_count = 0;
  std::size_t follow_up_api_invoked_count = 0;
  std::size_t assertion_count = 0;
  std::size_t assertion_pass_count = 0;
  std::size_t passed_case_count = 0;
  for (const CaseRecord& record : cases) {
    if (!record.primary_capability_id.empty() &&
        capabilities.find(record.primary_capability_id) != capabilities.end()) {
      unique_primary_capabilities.insert(record.primary_capability_id);
      if (record.success_present && record.success_passed) {
        success_passed.insert(record.primary_capability_id);
      }
      if (record.boundary_present && record.boundary_passed) {
        boundary_passed.insert(record.primary_capability_id);
      }
    }
    if (record.passed) {
      ++passed_case_count;
    }
    for (const ScenarioRecord& scenario : record.scenarios) {
      ++primary_scenario_count;
      if (scenario.primary.api_invoked) {
        ++primary_api_invoked_count;
      }
      for (const AssertionRecord& assertion : scenario.primary.assertions) {
        ++assertion_count;
        assertion_pass_count += assertion.passed ? 1U : 0U;
      }
      for (const ExecutionRecord& setup : scenario.setup) {
        setup_api_invoked_count += setup.api_invoked ? 1U : 0U;
        for (const AssertionRecord& assertion : setup.assertions) {
          ++assertion_count;
          assertion_pass_count += assertion.passed ? 1U : 0U;
        }
      }
      for (const ExecutionRecord& follow_up : scenario.follow_up) {
        follow_up_api_invoked_count += follow_up.api_invoked ? 1U : 0U;
        for (const AssertionRecord& assertion : follow_up.assertions) {
          ++assertion_count;
          assertion_pass_count += assertion.passed ? 1U : 0U;
        }
      }
    }
  }

  const bool passed = diagnostics.api_catalog_valid &&
                      diagnostics.traceability_catalog_valid &&
                      diagnostics.catalog_exact_match && DiagnosticsEmpty(diagnostics) &&
                      diagnostics.configuration_fixture_valid &&
                      diagnostics.configuration_exact_match &&
                      api_descriptors.size() == SupportedPayloadTypes().size() &&
                      trace_entries.size() == api_descriptors.size() &&
                      unique_primary_capabilities.size() == api_descriptors.size() &&
                      success_passed.size() == api_descriptors.size() &&
                      boundary_passed.size() == api_descriptors.size() &&
                      passed_cross_cutting_invariants.size() ==
                          required_cross_cutting_invariants.size() &&
                      cases.size() == api_descriptors.size() &&
                      passed_case_count == cases.size() &&
                      primary_api_invoked_count == primary_scenario_count &&
                      assertion_count == assertion_pass_count;

  std::error_code error;
  const fs::path cases_output = options.output_root / "cases";
  if (!fs::create_directory(cases_output, error) || error) {
    throw CaseError("Cannot create evidence cases directory: " + error.message());
  }
  json case_summaries = json::array();
  std::vector<json> audit;
  audit.push_back({{"event_type", "BUSINESS_API_RUN_STARTED"},
                   {"recorded_at", started_at},
                   {"run_id", options.run_id},
                   {"verification_scope", kExecutionClaim},
                   {"execution_claim", kExecutionClaim},
                   {"real_provider_execution_claim", false},
                   {"api_id", kApiId},
                   {"test_interface_id", kTestInterfaceId},
                   {"implementation_build_id", options.implementation_build_id},
                   {"requested_implementation_build_id",
                    options.requested_implementation_build_id},
                   {"sdk_build_identity", options.sdk_build_identity},
                   {"implementation_build_id_source", kBuildIdSource},
                   {"implementation_build_id_verification_status",
                    kBuildIdVerificationStatus},
                   {"case_root", options.case_root.generic_string()},
                   {"output_root", options.output_root.generic_string()}});
  audit.push_back(
      {{"event_type", "BUSINESS_SDK_BUILD_IDENTITY_VERIFIED"},
       {"recorded_at", build_identity_checked_at},
       {"run_id", options.run_id},
       {"verification_scope", kExecutionClaim},
       {"api_id", kApiId},
       {"test_interface_id", kTestInterfaceId},
       {"sdk_method", "ReadBuildIdentity"},
       {"requested_implementation_build_id",
        options.requested_implementation_build_id},
       {"implementation_build_id", options.implementation_build_id},
       {"sdk_build_identity", options.sdk_build_identity},
       {"sdk_identity_valid", diagnostics.build_identity_valid},
       {"cli_exact_match", diagnostics.build_identity_cli_match},
       {"verification_status",
        diagnostics.build_identity_valid && diagnostics.build_identity_cli_match
            ? kBuildIdVerificationStatus
            : "FAILED"},
       {"error", diagnostics.build_identity_error}});
  audit.push_back(
      {{"event_type", "BUSINESS_CONFIGURATION_CONSISTENCY_CHECKED"},
       {"recorded_at", configuration_checked_at},
       {"run_id", options.run_id},
       {"verification_scope", kExecutionClaim},
       {"fixture_scope", "TEST_ONLY"},
       {"runtime_role", "NOT_RUNTIME_CONFIGURATION"},
       {"fixture_consumed_as_runtime_configuration", false},
       {"api_id", kApiId},
       {"test_interface_id", kTestInterfaceId},
       {"sdk_method", "ReadConfigurationContract"},
       {"implementation_build_id", options.implementation_build_id},
       {"requested_implementation_build_id",
        options.requested_implementation_build_id},
       {"sdk_build_identity", options.sdk_build_identity},
       {"implementation_build_id_source", kBuildIdSource},
       {"implementation_build_id_verification_status",
        kBuildIdVerificationStatus},
       {"fixture_ref", "base_configuration_contract.json"},
       {"fixture_digest", configuration_fixture_digest},
       {"fixture_valid", diagnostics.configuration_fixture_valid},
       {"exact_match", diagnostics.configuration_exact_match},
       {"expected_contract", expected_configuration},
       {"sdk_contract", sdk_configuration},
       {"error", diagnostics.configuration_error}});

  std::set<std::string> output_names;
  std::size_t case_index = 0;
  for (const CaseRecord& record : cases) {
    std::string output_name = SafeDirectoryName(
        record.case_id.empty() ? "invalid_case_" + std::to_string(case_index) : record.case_id);
    if (!output_names.insert(output_name).second) {
      std::ostringstream suffix;
      suffix << output_name << '_' << std::hex << Fnv1a64(
          NormalizedRelativePath(fs::relative(record.source_path, options.case_root)));
      output_name = suffix.str();
      output_names.insert(output_name);
    }
    const fs::path case_output = cases_output / output_name;
    if (!fs::create_directory(case_output, error) || error) {
      throw CaseError("Cannot create case evidence directory: " + error.message());
    }
    const fs::path case_result_path = case_output / "case_result.json";
    WriteTextFile(case_result_path,
                  SerializeCase(record, options, options.case_root).dump(2) + "\n");
    const std::string relative_result =
        NormalizedRelativePath(fs::relative(case_result_path, options.output_root));
    case_summaries.push_back(
        {{"case_id", record.case_id},
         {"capability_id", record.primary_capability_id},
         {"request_type", record.request_type},
         {"case_ref", NormalizedRelativePath(
                          fs::relative(record.source_path, options.case_root))},
         {"case_result_ref", relative_result},
         {"schema_valid", record.schema_valid},
         {"success_passed", record.success_passed},
         {"negative_or_boundary_passed", record.boundary_passed},
         {"passed", record.passed}});
    for (const ScenarioRecord& scenario : record.scenarios) {
      for (const ExecutionRecord& setup : scenario.setup) {
        audit.push_back(MakeExecutionAudit(options, record, scenario, setup, false,
                                           options.case_root));
      }
      audit.push_back(MakeExecutionAudit(options, record, scenario, scenario.primary,
                                         true, options.case_root));
      for (const ExecutionRecord& follow_up : scenario.follow_up) {
        audit.push_back(MakeExecutionAudit(options, record, scenario, follow_up,
                                           false, options.case_root));
      }
    }
    ++case_index;
  }

  const std::string completed_at = UtcTimestamp();
  const std::vector<std::string> gaps = FlattenGaps(diagnostics);
  json coverage = {
      {"schema_version", "visionai.business_api_coverage.v1"},
      {"run_id", options.run_id},
      {"started_at", started_at},
      {"completed_at", completed_at},
      {"verification_scope", kExecutionClaim},
      {"execution_claim", kExecutionClaim},
      {"real_provider_execution_claim", false},
      {"fixture_scope", "TEST_ONLY"},
      {"status", passed ? "PASS" : "FAIL"},
      {"api_id", kApiId},
      {"test_interface_id", kTestInterfaceId},
      {"api_version", {{"major", business::kApiMajorVersion},
                        {"minor", business::kApiMinorVersion}}},
      {"implementation_build_id", options.implementation_build_id},
      {"requested_implementation_build_id",
       options.requested_implementation_build_id},
      {"sdk_build_identity", options.sdk_build_identity},
      {"implementation_build_id_source", kBuildIdSource},
      {"implementation_build_id_verification_status",
       kBuildIdVerificationStatus},
      {"dependency_boundary",
       {{"business_sdk_acquisition", "INSTALLED_CMAKE_PACKAGE"},
        {"cmake_package", "VisionAIBusiness"},
        {"linked_target", "VisionAI::BusinessCore"},
        {"vision_ai_source_tree_consumed", false},
        {"runtime_configuration_consumed_from_fixture", false}}},
      {"test_interface_coverage",
       {{"Capabilities", {{"invoked", true},
                            {"descriptor_count", api_descriptors.size()}}},
        {"ReadConfigurationContract",
         {{"invoked", true},
          {"exact_fixture_match", diagnostics.configuration_exact_match}}},
        {"ReadBuildIdentity",
         {{"invoked", true},
          {"identity_valid", diagnostics.build_identity_valid},
          {"cli_exact_match", diagnostics.build_identity_cli_match}}},
        {"Execute",
         {{"invoked", primary_api_invoked_count + setup_api_invoked_count +
                          follow_up_api_invoked_count > 0},
          {"primary_invocation_count", primary_api_invoked_count},
          {"setup_invocation_count", setup_api_invoked_count},
          {"follow_up_invocation_count", follow_up_api_invoked_count}}},
        {"ReadSnapshot",
         {{"invoked_around_execute",
           primary_api_invoked_count + setup_api_invoked_count +
                   follow_up_api_invoked_count >
               0}}},
        {"ReadSnapshotSummary",
         {{"invoked_around_execute",
           primary_api_invoked_count + setup_api_invoked_count +
                   follow_up_api_invoked_count >
               0}}}}},
      {"case_root", options.case_root.generic_string()},
      {"output_root", options.output_root.generic_string()},
      {"traceability_manifest_ref", "traceability_manifest.json"},
      {"traceability_manifest_digest",
       traceability.empty() ? "" : DigestJson(traceability)},
      {"build_identity_binding",
       {{"sdk_method", "ReadBuildIdentity"},
        {"requested_implementation_build_id",
         options.requested_implementation_build_id},
        {"implementation_build_id", options.implementation_build_id},
        {"sdk_build_identity", options.sdk_build_identity},
        {"sdk_identity_valid", diagnostics.build_identity_valid},
        {"cli_exact_match", diagnostics.build_identity_cli_match},
        {"verification_status",
         diagnostics.build_identity_valid && diagnostics.build_identity_cli_match
             ? kBuildIdVerificationStatus
             : "FAILED"},
        {"error", diagnostics.build_identity_error}}},
      {"configuration_consistency",
       {{"fixture_ref", "base_configuration_contract.json"},
        {"fixture_digest", configuration_fixture_digest},
        {"fixture_scope", "TEST_ONLY"},
        {"runtime_role", "NOT_RUNTIME_CONFIGURATION"},
        {"fixture_consumed_as_runtime_configuration", false},
        {"sdk_method", "ReadConfigurationContract"},
        {"comparison_mode", "EXACT_REQUIRED_FIELDS"},
        {"fixture_valid", diagnostics.configuration_fixture_valid},
        {"exact_match", diagnostics.configuration_exact_match},
        {"expected_contract", expected_configuration},
        {"sdk_contract", sdk_configuration},
        {"error", diagnostics.configuration_error}}},
      {"catalog",
       {{"truth_source", "IBusinessWorkflowTestApi::Capabilities"},
        {"api_catalog_valid", diagnostics.api_catalog_valid},
        {"traceability_catalog_valid", diagnostics.traceability_catalog_valid},
        {"catalog_exact_match", diagnostics.catalog_exact_match},
        {"api_capability_count", api_descriptors.size()},
        {"traceability_capability_count", trace_entries.size()},
        {"supported_payload_type_count", SupportedPayloadTypes().size()},
        {"api_capabilities", SerializeApiCapabilities(api_descriptors)},
        {"traceability_capabilities",
         SerializeTraceabilityCapabilities(trace_entries)},
        {"errors", diagnostics.catalog_errors}}},
      {"coverage",
       {{"discovered_case_count", cases.size()},
        {"unique_primary_case_count", unique_primary_capabilities.size()},
        {"passed_case_count", passed_case_count},
        {"success_coverage_count", success_passed.size()},
        {"negative_or_boundary_coverage_count", boundary_passed.size()},
        {"primary_scenario_count", primary_scenario_count},
        {"primary_api_invoked_count", primary_api_invoked_count},
        {"setup_api_invoked_count", setup_api_invoked_count},
        {"follow_up_api_invoked_count", follow_up_api_invoked_count},
        {"assertion_count", assertion_count},
        {"assertion_pass_count", assertion_pass_count},
        {"setup_counts_as_primary_coverage", false},
        {"follow_up_counts_as_primary_coverage", false},
        {"required_cross_cutting_invariants",
         required_cross_cutting_invariants},
        {"passed_cross_cutting_invariants",
         passed_cross_cutting_invariants},
        {"cross_cutting_invariant_coverage_count",
         passed_cross_cutting_invariants.size()},
        {"unknown_cross_cutting_invariants",
         diagnostics.unknown_cross_cutting_invariants},
        {"uncovered_cross_cutting_invariants",
         diagnostics.uncovered_cross_cutting_invariants},
        {"unknown_capabilities", diagnostics.unknown_capabilities},
        {"duplicate_primary_capabilities",
         diagnostics.duplicate_primary_capabilities},
        {"uncovered_capabilities", diagnostics.uncovered_capabilities},
        {"duplicate_case_ids", diagnostics.duplicate_case_ids},
        {"unknown_or_uncovered_payload_types",
         diagnostics.unknown_or_uncovered_payload_types},
        {"case_contract_errors", diagnostics.case_contract_errors},
        {"failed_success_capabilities", diagnostics.failed_success_capabilities},
        {"failed_negative_or_boundary_capabilities",
         diagnostics.failed_negative_or_boundary_capabilities}}},
      {"cases", std::move(case_summaries)},
      {"gaps", gaps}};

  audit.push_back({{"event_type", "BUSINESS_API_COVERAGE_COMPLETED"},
                   {"recorded_at", completed_at},
                   {"run_id", options.run_id},
                   {"verification_scope", kExecutionClaim},
                   {"execution_claim", kExecutionClaim},
                   {"real_provider_execution_claim", false},
                   {"api_id", kApiId},
                   {"test_interface_id", kTestInterfaceId},
                   {"implementation_build_id", options.implementation_build_id},
                   {"requested_implementation_build_id",
                    options.requested_implementation_build_id},
                   {"sdk_build_identity", options.sdk_build_identity},
                   {"implementation_build_id_source", kBuildIdSource},
                   {"implementation_build_id_verification_status",
                    kBuildIdVerificationStatus},
                   {"status", passed ? "PASS" : "FAIL"},
                   {"catalog_exact_match", diagnostics.catalog_exact_match},
                   {"configuration_exact_match",
                    diagnostics.configuration_exact_match},
                   {"build_identity_verified",
                    diagnostics.build_identity_valid &&
                        diagnostics.build_identity_cli_match},
                   {"unique_primary_case_count", unique_primary_capabilities.size()},
                   {"success_coverage_count", success_passed.size()},
                   {"negative_or_boundary_coverage_count", boundary_passed.size()},
                   {"cross_cutting_invariant_coverage_count",
                    passed_cross_cutting_invariants.size()},
                   {"gap_count", gaps.size()}});

  std::ostringstream audit_text;
  for (const json& event : audit) {
    audit_text << event.dump() << '\n';
  }
  WriteTextFile(options.output_root / "business_api_audit.jsonl", audit_text.str());
  WriteTextFile(options.output_root / "business_api_coverage.json",
                coverage.dump(2) + "\n");

  std::cout << "business_api_coverage_status=" << (passed ? "PASS" : "FAIL") << '\n'
            << "business_api_catalog_capability_count=" << api_descriptors.size() << '\n'
            << "business_api_unique_primary_case_count="
            << unique_primary_capabilities.size() << '\n'
            << "business_api_success_coverage_count=" << success_passed.size() << '\n'
            << "business_api_negative_or_boundary_coverage_count="
            << boundary_passed.size() << '\n'
            << "business_api_cross_cutting_invariant_coverage_count="
            << passed_cross_cutting_invariants.size() << '\n'
            << "business_api_implementation_build_id="
            << options.implementation_build_id << '\n'
            << "business_api_gap_count=" << gaps.size() << '\n'
            << "business_api_evidence_root=" << options.output_root.string() << '\n';
  return passed ? 0 : 1;
}

void TryWriteFatalEvidence(const Options& options, const std::string& message)
{
  try {
    if (options.output_root.empty() || !fs::is_directory(options.output_root)) {
      return;
    }
    const std::string recorded_at = UtcTimestamp();
    json coverage = {{"schema_version", "visionai.business_api_coverage.v1"},
                     {"run_id", options.run_id},
                     {"recorded_at", recorded_at},
                     {"verification_scope", kExecutionClaim},
                     {"execution_claim", kExecutionClaim},
                     {"real_provider_execution_claim", false},
                     {"status", "FAIL"},
                     {"api_id", kApiId},
                     {"test_interface_id", kTestInterfaceId},
                     {"api_version", {{"major", business::kApiMajorVersion},
                                       {"minor", business::kApiMinorVersion}}},
                     {"implementation_build_id", options.implementation_build_id},
                     {"implementation_build_id_source",
                      "CALLER_DECLARED_DURING_FATAL_ERROR"},
                     {"implementation_build_id_verification_status",
                      "PENDING_SDK_BUILD_IDENTITY"},
                     {"fatal_error", message}};
    if (!fs::exists(options.output_root / "business_api_coverage.json")) {
      WriteTextFile(options.output_root / "business_api_coverage.json",
                    coverage.dump(2) + "\n");
    }
    if (!fs::exists(options.output_root / "business_api_audit.jsonl")) {
      const json event = {{"event_type", "BUSINESS_API_RUN_FATAL"},
                          {"recorded_at", recorded_at},
                          {"run_id", options.run_id},
                          {"verification_scope", kExecutionClaim},
                          {"execution_claim", kExecutionClaim},
                          {"real_provider_execution_claim", false},
                          {"api_id", kApiId},
                          {"test_interface_id", kTestInterfaceId},
                          {"implementation_build_id", options.implementation_build_id},
                          {"implementation_build_id_source",
                           "CALLER_DECLARED_DURING_FATAL_ERROR"},
                          {"implementation_build_id_verification_status",
                           "PENDING_SDK_BUILD_IDENTITY"},
                          {"error", message}};
      WriteTextFile(options.output_root / "business_api_audit.jsonl",
                    event.dump() + "\n");
    }
  } catch (...) {
  }
}

}  // namespace

int main(int argc, char** argv)
{
  Options options;
  try {
    options = ParseOptions(argc, argv);
    ResolveAndClaimPaths(options);
  } catch (const std::exception& exception) {
    std::cerr << "vision_ai_business_case_runner: " << exception.what() << '\n';
    return 2;
  }

  try {
    return RunEvidence(options);
  } catch (const std::exception& exception) {
    TryWriteFatalEvidence(options, exception.what());
    std::cerr << "vision_ai_business_case_runner: " << exception.what() << '\n';
    return 1;
  }
}
