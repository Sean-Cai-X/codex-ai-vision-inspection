#include "CxAutomaticDiagnosticClosure.h"

#include "CxImageReferenceCandidateGenerator.h"
#include "CxTorchExecutionAdapter.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <numeric>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <set>
#include <string>
#include <vector>

namespace
{
struct ClosureBinding
{
    bool bound = false;
    bool precomputed_result_bound = false;
    std::string status;
    std::string task_id;
    std::string model_id;
    std::filesystem::path manifest_path;
    std::filesystem::path dataset_root;
    std::string device = "cpu";
    std::string extra_json;
    std::string physical_output_contract;
    std::filesystem::path result_ref;
    std::filesystem::path mask_path;
    std::filesystem::path overlay_path;
    std::filesystem::path input_image_path;
    std::string model_hash;
    std::string validation_reason;
};

struct ClosureSample
{

    std::string case_id;
    std::filesystem::path image_path;
    std::filesystem::path label_mask_path;
    std::filesystem::path label_companion_path;
};

struct ClosureRow
{
    std::string row_id;
    std::string review_item;
    int implementation_phase = 0;
    std::string case_track;
    std::string annotation_contract_id;
    std::string model_track_id;
    std::string evaluator_id;
    bool taxonomy_valid = false;
    bool annotation_contract_valid = false;
    bool model_track_valid = false;
    bool evaluator_contract_valid = false;
    bool evaluator_runtime_supported = false;
    std::string evaluator_runtime_status;
    std::filesystem::path binding_package_path;
    bool binding_package_valid = false;
    std::string typed_label_kind;
    std::filesystem::path typed_label_candidate_ref;
    std::filesystem::path typed_label_proposal_manifest;
    bool typed_label_independent = false;
    std::filesystem::path runtime_direction_manifest;
    bool runtime_direction_valid = false;
    std::filesystem::path source_manifest;
    std::string source_set_id;
    ClosureBinding parent;
    ClosureBinding child;
    std::string label_binding_status;
    std::filesystem::path label_mask_path;
    CxImageReferenceCandidateRequest candidate;
    std::vector<ClosureSample> samples;
    bool explicit_frozen_validation = false;
};

struct ProcessStage
{
    int index = 0;
    std::string stage_id;
    std::string status_source;
    std::vector<std::string> requires;
};

struct ProcessDefinition
{
    std::string process_id;
    std::vector<ProcessStage> stages;
};

struct ContractRegistry
{
    std::set<std::string> case_tracks;
    std::map<std::string, std::string> case_assignments;
    std::map<std::string, std::string> annotation_tracks;
    std::map<std::string, std::set<std::string>> model_tracks;
    std::map<std::string, std::set<std::string>> evaluator_tracks;
    std::map<std::string, std::string> evaluator_status;
};

struct CompletedCase
{
    std::string row_id;
    std::string case_id;
    double parent_child_iou = 0.0;
    bool label_available = false;
    double parent_label_iou = 0.0;
    double child_label_iou = 0.0;
    double label_delta = 0.0;
};
struct GatePolicy
{
    int minimum_case_count = 3;
    int maximum_regression_count = 0;
    double minimum_child_label_iou = 0.85;
    double minimum_mean_label_iou_delta = 0.0;
    double maximum_child_label_iou_stddev = 0.05;
};

std::string JsonEscape(const std::string& value)
{
    std::string escaped;
    for (const char ch : value)
    {
        if (ch == '\\' || ch == '"')
            escaped += '\\';
        if (ch == '\n') { escaped += "\\n"; continue; }
        if (ch == '\r') { escaped += "\\r"; continue; }
        escaped += ch;
    }
    return escaped;
}

std::string NodeString(const cv::FileNode& node, const char* key)
{
    const cv::FileNode value = node[key];
    return value.empty() ? std::string() : static_cast<std::string>(value);
}

double NodeDouble(const cv::FileNode& node, const char* key, double fallback)
{
    const cv::FileNode value = node[key];
    return value.empty() ? fallback : static_cast<double>(value);
}

int NodeInt(const cv::FileNode& node, const char* key, int fallback)
{
    const cv::FileNode value = node[key];
    return value.empty() ? fallback : static_cast<int>(value);
}

std::set<std::string> NodeStringSet(const cv::FileNode& node)
{
    std::set<std::string> values;
    if (!node.isSeq())
        return values;
    for (const cv::FileNode& value : node)
    {
        if (value.isString())
            values.insert(static_cast<std::string>(value));
    }
    return values;
}

std::vector<std::string> NodeStringVector(const cv::FileNode& node)
{
    std::vector<std::string> values;
    if (!node.isSeq())
        return values;
    for (const cv::FileNode& value : node)
    {
        if (value.isString())
            values.push_back(static_cast<std::string>(value));
    }
    return values;
}

bool LoadProcessDefinition(
    const std::filesystem::path& path,
    ProcessDefinition& process,
    std::string& reason)
{
    cv::FileStorage storage(path.string(), cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    if (!storage.isOpened() || !storage["stages"].isSeq())
    {
        reason = "automatic diagnostic process asset cannot be opened or has no stages";
        return false;
    }
    if (NodeInt(storage.root(), "training_enabled", 1) != 0 ||
        NodeString(storage.root(), "execution") != "strict_serial")
    {
        reason = "automatic diagnostic process must disable training and require strict serial execution";
        return false;
    }
    process.process_id = NodeString(storage.root(), "process_id");
    const std::set<std::string> allowed_sources = {
        "contract_assets_valid", "source_images_bound", "typed_labels_bound",
        "runtime_models_bound", "evaluators_supported", "cases_executed",
        "frozen_validation_bound", "aggregate_emitted", "promotion_gate_emitted",
        "final_human_review"};
    std::set<std::string> prior_ids;
    int expected_index = 1;
    for (const cv::FileNode& node : storage["stages"])
    {
        ProcessStage stage;
        stage.index = NodeInt(node, "index", 0);
        stage.stage_id = NodeString(node, "stage_id");
        stage.status_source = NodeString(node, "status_source");
        stage.requires = NodeStringVector(node["requires"]);
        if (stage.index != expected_index || stage.stage_id.empty() ||
            allowed_sources.count(stage.status_source) == 0 ||
            prior_ids.count(stage.stage_id) != 0)
        {
            reason = "automatic diagnostic process stages are not uniquely and contiguously ordered";
            return false;
        }
        for (const std::string& requirement : stage.requires)
        {
            if (prior_ids.count(requirement) == 0)
            {
                reason = "automatic diagnostic process dependency does not reference an earlier stage";
                return false;
            }
        }
        prior_ids.insert(stage.stage_id);
        process.stages.push_back(stage);
        ++expected_index;
    }
    return !process.process_id.empty() && !process.stages.empty();
}

bool LoadContractRegistry(
    const std::filesystem::path& taxonomy_path,
    const std::filesystem::path& annotation_path,
    const std::filesystem::path& model_path,
    const std::filesystem::path& evaluator_path,
    ContractRegistry& registry,
    std::string& reason)
{
    cv::FileStorage taxonomy(taxonomy_path.string(), cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    cv::FileStorage annotations(annotation_path.string(), cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    cv::FileStorage models(model_path.string(), cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    cv::FileStorage evaluators(evaluator_path.string(), cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    if (!taxonomy.isOpened() || !annotations.isOpened() || !models.isOpened() || !evaluators.isOpened())
    {
        reason = "one or more automatic diagnostic contract assets cannot be opened";
        return false;
    }
    if (!taxonomy["tracks"].isSeq() || !taxonomy["case_assignments"].isSeq() ||
        !annotations["contracts"].isSeq() || !models["tracks"].isSeq() ||
        !evaluators["evaluators"].isSeq())
    {
        reason = "automatic diagnostic contract assets do not satisfy their collection schema";
        return false;
    }
    for (const cv::FileNode& node : taxonomy["tracks"])
        registry.case_tracks.insert(NodeString(node, "track_id"));
    for (const cv::FileNode& node : taxonomy["case_assignments"])
        registry.case_assignments[NodeString(node, "case_id")] = NodeString(node, "track_id");
    for (const cv::FileNode& node : annotations["contracts"])
        registry.annotation_tracks[NodeString(node, "contract_id")] = NodeString(node, "track_id");
    for (const cv::FileNode& node : models["tracks"])
        registry.model_tracks[NodeString(node, "model_track_id")] = NodeStringSet(node["case_tracks"]);
    for (const cv::FileNode& node : evaluators["evaluators"])
    {
        const std::string id = NodeString(node, "evaluator_id");
        registry.evaluator_tracks[id] = NodeStringSet(node["case_tracks"]);
        registry.evaluator_status[id] = NodeString(node, "runtime_status");
    }
    return true;
}

std::filesystem::path ResolveAssetPath(
    const std::filesystem::path&,
    const std::filesystem::path& value)
{
    if (value.empty() || value.is_absolute())
        return value.lexically_normal();
    return (std::filesystem::current_path() / value).lexically_normal();
}

bool IsReadableImage(const std::filesystem::path& path)
{
    if (!std::filesystem::is_regular_file(path))
        return false;
    const std::string extension = path.extension().string();
    return extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
           extension == ".bmp" || extension == ".tif" || extension == ".tiff";
}

std::filesystem::path FirstImageInPath(const std::filesystem::path& source)
{
    if (IsReadableImage(source))
        return source;
    if (!std::filesystem::is_directory(source))
        return {};

    std::vector<std::filesystem::path> images;
    std::error_code error;
    for (std::filesystem::recursive_directory_iterator iterator(
             source,
             std::filesystem::directory_options::skip_permission_denied,
             error), end;
         iterator != end;
         iterator.increment(error))
    {
        if (error)
        {
            error.clear();
            continue;
        }
        if (iterator->is_symlink(error))
        {
            if (iterator->is_directory(error))
                iterator.disable_recursion_pending();
            continue;
        }
        if (IsReadableImage(iterator->path()))
            images.push_back(iterator->path());
    }
    std::sort(images.begin(), images.end());
    return images.empty() ? std::filesystem::path() : images.front();
}

std::filesystem::path ResolveFirstManifestImage(
    const std::filesystem::path& manifest_path,
    const std::string& set_id)
{
    cv::FileStorage manifest(manifest_path.string(), cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    if (!manifest.isOpened())
        return {};
    const cv::FileNode sets = manifest["sets"];
    if (!sets.isSeq())
        return {};
    for (const cv::FileNode& set : sets)
    {
        if (NodeString(set, "case_id") != set_id)
            continue;
        const cv::FileNode sources = set["image_sources"];
        if (!sources.isSeq())
            return {};
        for (const cv::FileNode& source : sources)
        {
            const std::filesystem::path candidate = NodeString(source, "path");
            const std::filesystem::path image = FirstImageInPath(candidate);
            if (!image.empty())
                return image;
        }
    }
    return {};
}

ClosureBinding ParseBinding(const cv::FileNode& node)
{
    ClosureBinding binding;
    if (node.empty())
    {
        binding.status = "missing";
        return binding;
    }
    if (node.isString())
    {
        binding.status = static_cast<std::string>(node);
        return binding;
    }
    binding.status = NodeString(node, "status");
    binding.task_id = NodeString(node, "task_id");
    binding.model_id = NodeString(node, "model_id");
    binding.manifest_path = NodeString(node, "manifest_path");
    binding.dataset_root = NodeString(node, "dataset_root");
    const std::string device = NodeString(node, "requested_device");
    if (!device.empty())
        binding.device = device;
    binding.extra_json = NodeString(node, "extra_json");
    binding.physical_output_contract = NodeString(node, "physical_output_contract");
    binding.result_ref = NodeString(node, "result_ref");
    binding.mask_path = NodeString(node, "mask_path");
    binding.overlay_path = NodeString(node, "overlay_path");
    binding.input_image_path = NodeString(node, "input_image_path");
    binding.model_hash = NodeString(node, "model_hash");
    return binding;
}

void ValidateBindingManifest(ClosureBinding& binding)
{
    binding.bound = false;
    binding.precomputed_result_bound = false;
    if (binding.status == "result_bound")
    {
        if (binding.task_id.empty() || binding.model_id.empty() ||
            binding.physical_output_contract.empty() ||
            !std::filesystem::is_regular_file(binding.result_ref) ||
            !IsReadableImage(binding.mask_path) ||
            !std::filesystem::is_regular_file(binding.input_image_path))
        {
            binding.validation_reason = "precomputed result binding requires task, model, physical output contract, result_ref, mask_path, and input_image_path";

            return;
        }
        binding.bound = true;
        binding.precomputed_result_bound = true;
        binding.validation_reason = "precomputed inference result and mask asset validated";
        return;
    }
    if (binding.status != "bound")
    {
        binding.validation_reason = "binding status is not bound";
        return;
    }
    if (binding.task_id.empty() || binding.model_id.empty() ||
        binding.physical_output_contract.empty() ||
        !std::filesystem::is_regular_file(binding.manifest_path))
    {
        binding.validation_reason = "required binding fields or physical manifest are missing";
        return;
    }
    cv::FileStorage manifest(
        binding.manifest_path.string(), cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    if (!manifest.isOpened())
    {
        binding.validation_reason = "model manifest is not valid JSON";
        return;
    }
    if (NodeString(manifest.root(), "model_id") != binding.model_id ||
        NodeString(manifest.root(), "task") != binding.task_id ||
        NodeString(manifest.root(), "physical_output_contract") != binding.physical_output_contract)
    {
        binding.validation_reason = "model id, task, or physical output contract does not match binding";
        return;
    }
    std::filesystem::path weights = NodeString(manifest.root(), "weights");
    if (weights.empty())
        weights = NodeString(manifest.root(), "model_path");
    if (!weights.is_absolute())
        weights = (binding.manifest_path.parent_path() / weights).lexically_normal();
    if (!std::filesystem::is_regular_file(weights))
    {
        binding.validation_reason = "model manifest does not resolve to physical weights";
        return;
    }
    binding.bound = true;
    binding.validation_reason = "model manifest and physical output contract validated";
}


bool LoadBindingPackage(
    const std::filesystem::path& package_path,
    const std::string& expected_track,
    ClosureRow& row,
    std::string& reason)
{
    cv::FileStorage package(package_path.string(), cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    if (!package.isOpened() ||
        NodeString(package.root(), "schema") != "cxvision.single_image_runtime_binding.v1" ||
        NodeInt(package.root(), "training_enabled", 1) != 0)
    {
        reason = "single-image binding package is invalid or enables training";
        return false;
    }
    if (NodeString(package.root(), "case_track") != expected_track)
    {
        reason = "single-image binding package track does not match its matrix row";
        return false;
    }
    const cv::FileNode sample_node = package["sample"];
    const cv::FileNode label_node = package["typed_label"];
    ClosureSample sample;
    sample.case_id = NodeString(package.root(), "case_id");
    sample.image_path = ResolveAssetPath(package_path, NodeString(sample_node, "image_path"));
    sample.label_mask_path = ResolveAssetPath(package_path, NodeString(label_node, "path"));
    sample.label_companion_path = ResolveAssetPath(package_path, NodeString(label_node, "companion_path"));
    row.typed_label_kind = NodeString(label_node, "kind");
    row.label_binding_status = NodeString(label_node, "status");
    row.typed_label_candidate_ref = ResolveAssetPath(
        package_path, NodeString(label_node, "candidate_reference"));
    row.typed_label_proposal_manifest = ResolveAssetPath(
        package_path, NodeString(label_node, "proposal_manifest"));
    row.typed_label_independent = NodeInt(label_node, "independent_ground_truth", 0) != 0;
    row.runtime_direction_manifest = ResolveAssetPath(
        package_path, NodeString(package.root(), "runtime_direction_manifest"));
    row.parent = ParseBinding(package["parent_binding"]);
    row.child = ParseBinding(package["child_binding"]);
    row.parent.manifest_path = ResolveAssetPath(package_path, row.parent.manifest_path);
    row.child.manifest_path = ResolveAssetPath(package_path, row.child.manifest_path);
    row.parent.result_ref = ResolveAssetPath(package_path, row.parent.result_ref);
    row.child.result_ref = ResolveAssetPath(package_path, row.child.result_ref);
    row.parent.mask_path = ResolveAssetPath(package_path, row.parent.mask_path);
    row.child.mask_path = ResolveAssetPath(package_path, row.child.mask_path);
    row.parent.overlay_path = ResolveAssetPath(package_path, row.parent.overlay_path);
    row.child.overlay_path = ResolveAssetPath(package_path, row.child.overlay_path);
    row.parent.input_image_path = ResolveAssetPath(package_path, row.parent.input_image_path);
    row.child.input_image_path = ResolveAssetPath(package_path, row.child.input_image_path);
    ValidateBindingManifest(row.parent);
    ValidateBindingManifest(row.child);
    if ((row.parent.precomputed_result_bound && row.parent.input_image_path != sample.image_path) ||
        (row.child.precomputed_result_bound && row.child.input_image_path != sample.image_path))
    {
        reason = "precomputed inference result input image does not match the binding sample image";
        return false;
    }


    cv::FileStorage direction(row.runtime_direction_manifest.string(),
        cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
    if (direction.isOpened() &&
        NodeString(direction.root(), "schema") == "cxvision.parent_child_runtime_direction.v1" &&
        NodeString(direction.root(), "case_track") == expected_track &&
        NodeInt(direction.root(), "training_enabled", 1) == 0)
    {
        const cv::FileNode direction_parent = direction["parent"];
        const cv::FileNode direction_child = direction["child"];
        row.runtime_direction_valid =
            NodeString(direction_parent, "task_id") == row.parent.task_id &&
            NodeString(direction_child, "task_id") == row.child.task_id &&
            NodeString(direction_parent, "physical_output_contract") == row.parent.physical_output_contract &&
            NodeString(direction_child, "physical_output_contract") == row.child.physical_output_contract;
    }

    const std::filesystem::path audit_path = ResolveAssetPath(
        package_path, NodeString(package.root(), "runtime_candidate_audit"));
    if (sample.case_id.empty() || row.typed_label_kind.empty() ||
        !std::filesystem::is_regular_file(sample.image_path) ||
        !std::filesystem::is_regular_file(row.typed_label_candidate_ref) ||
        !std::filesystem::is_regular_file(audit_path) ||
        !row.runtime_direction_valid)
    {
        reason = "single-image binding package is missing or mismatches its sample, typed-label candidate, runtime direction, or runtime audit";
        return false;
    }
    if (row.label_binding_status == "bound")
    {
        if (!std::filesystem::is_regular_file(sample.label_mask_path))
        {
            reason = "bound typed label does not reference a physical label asset";
            return false;
        }
        if (row.typed_label_kind == "instance_id_mask_with_class" &&
            !std::filesystem::is_regular_file(sample.label_companion_path))
        {
            reason = "bound instance label is missing its class companion asset";
            return false;
        }
    }
    if (row.label_binding_status == "auto_provisional")
    {
        if (!std::filesystem::is_regular_file(sample.label_mask_path) ||
            !std::filesystem::is_regular_file(row.typed_label_proposal_manifest))
        {
            reason = "auto-provisional typed label is missing its physical label or proposal manifest";
            return false;
        }
        if ((row.typed_label_kind == "instance_id_mask_with_class" ||
             row.typed_label_kind == "open_boundary_polyline_with_endpoints") &&
            !std::filesystem::is_regular_file(sample.label_companion_path))
        {
            reason = "auto-provisional typed label is missing its required companion asset";
            return false;
        }
    }
    row.samples.clear();
    row.samples.push_back(sample);
    return true;
}

CxTorchTaskKind TaskKindFromId(const std::string& task_id)
{
    if (task_id.find("segmentation") != std::string::npos ||
        task_id.find("deeplab") != std::string::npos)
        return CxTorchTaskKind::Segmentation;
    if (task_id.find("detection") != std::string::npos ||
        task_id.find("yolo") != std::string::npos)
        return CxTorchTaskKind::Detection;
    if (task_id.find("classification") != std::string::npos)
        return CxTorchTaskKind::Classification;
    return CxTorchTaskKind::Unknown;
}

CxTorchTaskSpec MakeTask(
    const ClosureBinding& binding,
    const ClosureSample& sample,
    const std::filesystem::path& output_dir)
{
    CxTorchTaskSpec task;
    task.kind = TaskKindFromId(binding.task_id);
    task.task_id = binding.task_id;
    task.case_id = sample.case_id;
    task.model_id = binding.model_id;
    task.manifest_path = binding.manifest_path;
    task.dataset_root = binding.dataset_root;
    task.input_image_path = sample.image_path;
    task.output_dir = output_dir;
    task.requested_device = binding.device;
    task.extra_json = binding.extra_json;
    task.precomputed_result_ref = binding.result_ref;
    task.precomputed_mask_path = binding.mask_path;
    task.precomputed_overlay_path = binding.overlay_path;
    task.precomputed_model_hash = binding.model_hash;
    return task;
}
double Mean(const std::vector<double>& values)
{

    if (values.empty())
        return 0.0;
    return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
}

double StandardDeviation(const std::vector<double>& values, double mean)
{
    if (values.size() < 2)
        return 0.0;
    double sum = 0.0;
    for (const double value : values)
        sum += (value - mean) * (value - mean);
    return std::sqrt(sum / values.size());
}

bool WritePreflight(
    const std::filesystem::path& path,
    const std::vector<ClosureRow>& rows,
    const CxAutomaticDiagnosticClosureResult& result,
    std::string& reason)
{
    std::ofstream file(path, std::ios::trunc);
    if (!file)
    {
        reason = "cannot write binding preflight";
        return false;
    }
    file << "{\n  \"schema\": \"cxvision.automatic_diagnostic_binding_preflight.v2\",\n"
         << "  \"status\": \"" << JsonEscape(result.status) << "\",\n"
         << "  \"reason\": \"" << JsonEscape(result.reason) << "\",\n"
         << "  \"discovered_rows\": " << result.discovered_rows << ",\n"
         << "  \"bound_rows\": " << result.bound_rows << ",\n"
         << "  \"rows\": [\n";
    for (std::size_t index = 0; index < rows.size(); ++index)
    {
        const ClosureRow& row = rows[index];
        const bool image_bound = !row.samples.empty() &&
            std::filesystem::is_regular_file(row.samples.front().image_path);
        const bool label_bound = row.label_binding_status == "bound" &&
            row.typed_label_independent &&
            !row.samples.empty() &&
            std::filesystem::is_regular_file(row.samples.front().label_mask_path);
        const bool proposal_physical = row.label_binding_status == "auto_provisional" &&
            !row.samples.empty() &&
            std::filesystem::is_regular_file(row.samples.front().label_mask_path) &&
            std::filesystem::is_regular_file(row.typed_label_proposal_manifest);
        const bool contracts_valid = row.taxonomy_valid && row.annotation_contract_valid &&
            row.model_track_valid && row.evaluator_contract_valid &&
            (row.binding_package_path.empty() || row.binding_package_valid);
        file << "    {\"matrix_row\": \"" << JsonEscape(row.row_id)
             << "\", \"review_item\": \"" << JsonEscape(row.review_item)
             << "\", \"implementation_phase\": " << row.implementation_phase
             << ", \"case_track\": \"" << JsonEscape(row.case_track)
             << "\", \"annotation_contract_id\": \"" << JsonEscape(row.annotation_contract_id)
             << "\", \"model_track_id\": \"" << JsonEscape(row.model_track_id)
             << "\", \"evaluator_id\": \"" << JsonEscape(row.evaluator_id)
             << "\", \"contracts_valid\": " << (contracts_valid ? "true" : "false")
             << ", \"taxonomy_valid\": " << (row.taxonomy_valid ? "true" : "false")
             << ", \"annotation_contract_valid\": " << (row.annotation_contract_valid ? "true" : "false")
             << ", \"model_track_valid\": " << (row.model_track_valid ? "true" : "false")
             << ", \"evaluator_contract_valid\": " << (row.evaluator_contract_valid ? "true" : "false")
             << ", \"evaluator_runtime_supported\": " << (row.evaluator_runtime_supported ? "true" : "false")
             << ", \"evaluator_runtime_status\": \"" << JsonEscape(row.evaluator_runtime_status)
             << "\", \"binding_package_ref\": \"" << JsonEscape(row.binding_package_path.string())
             << "\", \"binding_package_valid\": " << (row.binding_package_valid ? "true" : "false")
             << ", \"typed_label_kind\": \"" << JsonEscape(row.typed_label_kind)
             << "\", \"typed_label_candidate_ref\": \"" << JsonEscape(row.typed_label_candidate_ref.string())
             << "\", \"typed_label_proposal_manifest\": \"" << JsonEscape(row.typed_label_proposal_manifest.string())
             << "\", \"runtime_direction_manifest\": \"" << JsonEscape(row.runtime_direction_manifest.string())
             << "\", \"runtime_direction_valid\": " << (row.runtime_direction_valid ? "true" : "false")
             << ", \"image_bound\": " << (image_bound ? "true" : "false")
             << ", \"typed_label_proposal_physical\": " << (proposal_physical ? "true" : "false")
             << R"(, "typed_label_independent_ground_truth": )" << (row.typed_label_independent ? "true" : "false")
             << R"(, "parent_bound": )" << (row.parent.bound ? "true" : "false")
             << R"(, "parent_result_bound": )" << (row.parent.precomputed_result_bound ? "true" : "false")
             << R"(, "child_bound": )" << (row.child.bound ? "true" : "false")
             << R"(, "child_result_bound": )" << (row.child.precomputed_result_bound ? "true" : "false")
             << R"(, "dataset_label_bound": )" << (label_bound ? "true" : "false")
             << R"(, "parent_status": ")" << JsonEscape(row.parent.status)

             << "\", \"child_status\": \"" << JsonEscape(row.child.status)
             << "\", \"parent_validation\": \"" << JsonEscape(row.parent.validation_reason)
             << "\", \"child_validation\": \"" << JsonEscape(row.child.validation_reason)
             << "\", \"label_status\": \"" << JsonEscape(row.label_binding_status) << "\"}"
             << (index + 1 < rows.size() ? "," : "") << "\n";
    }
    file << "  ]\n}\n";
    return file.good();
}

bool WriteProcessStatus(
    const std::filesystem::path& path,
    const ProcessDefinition& process,
    const std::vector<ClosureRow>& rows,
    const CxAutomaticDiagnosticClosureResult& result,
    std::string& reason)
{
    bool contracts_valid = !rows.empty();
    bool images_bound = !rows.empty();
    bool labels_bound = !rows.empty();
    bool models_bound = !rows.empty();
    bool evaluators_supported = !rows.empty();
    bool frozen_validation_bound = !rows.empty();
    int total_samples = 0;
    for (const ClosureRow& row : rows)
    {
        contracts_valid = contracts_valid && row.taxonomy_valid && row.annotation_contract_valid &&
            row.model_track_valid && row.evaluator_contract_valid &&
            (row.binding_package_path.empty() || row.binding_package_valid);
        images_bound = images_bound && !row.samples.empty();
        labels_bound = labels_bound && row.label_binding_status == "bound" &&
            row.typed_label_independent && !row.samples.empty();
        models_bound = models_bound && row.parent.bound && row.child.bound;
        evaluators_supported = evaluators_supported && row.evaluator_runtime_supported;
        frozen_validation_bound = frozen_validation_bound && row.explicit_frozen_validation && !row.samples.empty();
        for (const ClosureSample& sample : row.samples)
        {
            ++total_samples;
            images_bound = images_bound && std::filesystem::is_regular_file(sample.image_path);
            labels_bound = labels_bound && std::filesystem::is_regular_file(sample.label_mask_path);
            frozen_validation_bound = frozen_validation_bound &&
                std::filesystem::is_regular_file(sample.image_path) &&
                std::filesystem::is_regular_file(sample.label_mask_path);
        }
    }
    const bool cases_executed = total_samples > 0 && result.completed_cases >= total_samples;
    const bool aggregate_emitted = std::filesystem::is_regular_file(result.aggregate_ref) &&
        std::filesystem::is_regular_file(result.stability_ref);
    const bool promotion_gate_emitted = std::filesystem::is_regular_file(result.promotion_gate_ref);

    std::map<std::string, bool> facts = {
        {"contract_assets_valid", contracts_valid},
        {"source_images_bound", images_bound},
        {"typed_labels_bound", labels_bound},
        {"runtime_models_bound", models_bound},
        {"evaluators_supported", evaluators_supported},
        {"cases_executed", cases_executed},
        {"frozen_validation_bound", frozen_validation_bound},
        {"aggregate_emitted", aggregate_emitted},
        {"promotion_gate_emitted", promotion_gate_emitted},
        {"final_human_review", false}};
    std::map<std::string, std::string> stage_status;
    std::string next_allowed_stage;
    for (const ProcessStage& stage : process.stages)
    {
        bool dependencies_complete = true;
        for (const std::string& requirement : stage.requires)
            dependencies_complete = dependencies_complete && stage_status[requirement] == "COMPLETE";
        if (!dependencies_complete)
            stage_status[stage.stage_id] = "BLOCKED";
        else if (facts[stage.status_source])
            stage_status[stage.stage_id] = "COMPLETE";
        else if (stage.status_source == "final_human_review")
            stage_status[stage.stage_id] = "PENDING_HUMAN_REVIEW";
        else
            stage_status[stage.stage_id] = "PENDING_BINDING";
        if (next_allowed_stage.empty() && stage_status[stage.stage_id] != "COMPLETE" && dependencies_complete)
            next_allowed_stage = stage.stage_id;
    }

    std::ofstream file(path, std::ios::trunc);
    if (!file)
    {
        reason = "cannot write automatic diagnostic process status";
        return false;
    }
    file << "{\n  \"schema\": \"cxvision.automatic_diagnostic_process_status.v1\",\n"
         << "  \"process_id\": \"" << JsonEscape(process.process_id) << "\",\n"
         << "  \"next_allowed_stage\": \"" << JsonEscape(next_allowed_stage) << "\",\n"
         << "  \"stages\": [\n";
    for (std::size_t index = 0; index < process.stages.size(); ++index)
    {
        const ProcessStage& stage = process.stages[index];
        file << "    {\"index\": " << stage.index
             << ", \"stage_id\": \"" << JsonEscape(stage.stage_id)
             << "\", \"status_source\": \"" << JsonEscape(stage.status_source)
             << "\", \"status\": \"" << stage_status[stage.stage_id] << "\"}"
             << (index + 1 < process.stages.size() ? "," : "") << "\n";
    }
    file << "  ]\n}\n";
    return file.good();
}

bool WriteAggregateReports(
    const std::filesystem::path& output_dir,
    const std::vector<CompletedCase>& cases,
    const GatePolicy& policy,
    CxAutomaticDiagnosticClosureResult& result,
    std::string& reason)
{
    std::vector<double> parent_child;
    std::vector<double> parent_label;
    std::vector<double> child_label;
    std::vector<double> deltas;
    int regression_count = 0;
    int label_case_count = 0;
    for (const CompletedCase& item : cases)
    {
        parent_child.push_back(item.parent_child_iou);
        if (!item.label_available)
            continue;
        ++label_case_count;
        parent_label.push_back(item.parent_label_iou);
        child_label.push_back(item.child_label_iou);
        deltas.push_back(item.label_delta);
        if (item.label_delta < 0.0)
            ++regression_count;
    }

    const bool label_coverage_complete =
        !cases.empty() && label_case_count == static_cast<int>(cases.size());
    const double mean_parent_child = Mean(parent_child);
    const double mean_parent_label = Mean(parent_label);
    const double mean_child_label = Mean(child_label);
    const double mean_delta = Mean(deltas);
    const double child_stddev = StandardDeviation(child_label, mean_child_label);
    const char quote = static_cast<char>(34);
    const auto write_key = [quote](std::ofstream& file, const char* key)
    {
        file << "  " << quote << key << quote << ": ";
    };

    result.aggregate_ref = output_dir / "frozen_validation_aggregate.json";
    std::ofstream aggregate(result.aggregate_ref, std::ios::trunc);
    if (!aggregate)
    {
        reason = "cannot write frozen validation aggregate";
        return false;
    }
    aggregate << "{\n";
    write_key(aggregate, "schema");
    aggregate << quote << "cxvision.frozen_validation_aggregate.v2" << quote << ",\n";
    write_key(aggregate, "case_count");
    aggregate << cases.size() << ",\n";
    write_key(aggregate, "label_case_count");
    aggregate << label_case_count << ",\n";
    write_key(aggregate, "label_coverage_complete");
    aggregate << (label_coverage_complete ? "true" : "false") << ",\n";
    write_key(aggregate, "absolute_accuracy_claimed");
    aggregate << (label_coverage_complete ? "true" : "false") << ",\n";
    write_key(aggregate, "mean_parent_child_iou");
    aggregate << mean_parent_child << ",\n";
    write_key(aggregate, "mean_parent_label_iou");
    if (label_case_count > 0)
        aggregate << mean_parent_label;
    else
        aggregate << "null";
    aggregate << ",\n";
    write_key(aggregate, "mean_child_label_iou");
    if (label_case_count > 0)
        aggregate << mean_child_label;
    else
        aggregate << "null";
    aggregate << ",\n";
    write_key(aggregate, "mean_label_iou_delta");
    if (label_case_count > 0)
        aggregate << mean_delta;
    else
        aggregate << "null";
    aggregate << ",\n";
    write_key(aggregate, "regression_count");
    aggregate << regression_count << "\n}\n";
    if (!aggregate.good())
    {
        reason = "failed to write frozen validation aggregate";
        return false;
    }

    result.stability_ref = output_dir / "stability_analysis.json";
    std::ofstream stability(result.stability_ref, std::ios::trunc);
    if (!stability)
    {
        reason = "cannot write stability analysis";
        return false;
    }
    stability << "{\n";
    write_key(stability, "schema");
    stability << quote << "cxvision.frozen_validation_stability.v2" << quote << ",\n";
    write_key(stability, "sample_count");
    stability << label_case_count << ",\n";
    write_key(stability, "label_coverage_complete");
    stability << (label_coverage_complete ? "true" : "false") << ",\n";
    write_key(stability, "child_label_iou_mean");
    if (label_case_count > 0)
        stability << mean_child_label;
    else
        stability << "null";
    stability << ",\n";
    write_key(stability, "child_label_iou_stddev");
    if (label_case_count > 0)
        stability << child_stddev;
    else
        stability << "null";
    stability << ",\n";
    write_key(stability, "stable");
    stability << (label_coverage_complete &&
                          child_stddev <= policy.maximum_child_label_iou_stddev
                      ? "true"
                      : "false")
              << "\n}\n";
    if (!stability.good())
    {
        reason = "failed to write stability analysis";
        return false;
    }

    const bool candidate =
        label_coverage_complete &&
        static_cast<int>(cases.size()) >= policy.minimum_case_count &&
        regression_count <= policy.maximum_regression_count &&
        mean_child_label >= policy.minimum_child_label_iou &&
        mean_delta >= policy.minimum_mean_label_iou_delta &&
        child_stddev <= policy.maximum_child_label_iou_stddev;
    const std::string gate_status = !label_coverage_complete
        ? "PENDING_BINDING"
        : (candidate ? "PENDING_HUMAN_REVIEW" : "FAIL");
    const std::string gate_reason = !label_coverage_complete
        ? "paired consistency diagnostics completed, but independent dataset label coverage is incomplete"
        : (candidate
               ? "automatic frozen-set checks passed; final promotion requires human confirmation"
               : "automatic frozen-set thresholds were not satisfied");

    result.promotion_gate_ref = output_dir / "promotion_gate.json";
    std::ofstream gate(result.promotion_gate_ref, std::ios::trunc);
    if (!gate)
    {
        reason = "cannot write promotion gate";
        return false;
    }
    gate << "{\n";
    write_key(gate, "schema");
    gate << quote << "cxvision.incremental_promotion_gate.v2" << quote << ",\n";
    write_key(gate, "automatic_checks_complete");
    gate << (label_coverage_complete ? "true" : "false") << ",\n";
    write_key(gate, "label_coverage_complete");
    gate << (label_coverage_complete ? "true" : "false") << ",\n";
    write_key(gate, "candidate_satisfies_thresholds");
    gate << (candidate ? "true" : "false") << ",\n";
    write_key(gate, "promotion_allowed");
    gate << "false,\n";
    write_key(gate, "status");
    gate << quote << gate_status << quote << ",\n";
    write_key(gate, "reason");
    gate << quote << JsonEscape(gate_reason) << quote << "\n}\n";
    if (!gate.good())
    {
        reason = "failed to write promotion gate";
        return false;
    }

    result.status = gate_status;
    result.reason = gate_reason;
    reason = gate_reason;
    return true;
}

} // namespace
bool RunCxAutomaticDiagnosticClosure(
    const CxAutomaticDiagnosticClosureOptions& options,
    CxAutomaticDiagnosticClosureResult& result,
    std::string& reason)
{
    result = {};
    result.executed = true;
    try
    {
        if (!std::filesystem::is_regular_file(options.matrix_path))
        {
            reason = "automatic diagnostic matrix is missing";
            result.status = "ASSET_PREFLIGHT_FAIL";
            result.reason = reason;
            return false;
        }
        if (options.output_dir.empty())
        {
            reason = "automatic diagnostic output directory is empty";
            result.status = "ASSET_PREFLIGHT_FAIL";
            result.reason = reason;
            return false;
        }
        std::filesystem::create_directories(options.output_dir);

        cv::FileStorage matrix(options.matrix_path.string(), cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
        if (!matrix.isOpened() || !matrix["rows"].isSeq())
        {
            reason = "automatic diagnostic matrix is not valid JSON or has no rows";
            result.status = "ASSET_PREFLIGHT_FAIL";
            result.reason = reason;
            return false;
        }

        const cv::FileNode contract_assets = matrix["asset_contracts"];
        const std::filesystem::path taxonomy_path = ResolveAssetPath(
            options.matrix_path, NodeString(contract_assets, "case_taxonomy"));
        const std::filesystem::path annotation_path = ResolveAssetPath(
            options.matrix_path, NodeString(contract_assets, "annotation_contracts"));
        const std::filesystem::path model_path = ResolveAssetPath(
            options.matrix_path, NodeString(contract_assets, "model_tracks"));
        const std::filesystem::path evaluator_path = ResolveAssetPath(
            options.matrix_path, NodeString(contract_assets, "evaluator_contracts"));
        const std::filesystem::path composite_path = ResolveAssetPath(
            options.matrix_path, NodeString(contract_assets, "composite_pipeline"));
        const std::filesystem::path process_path = ResolveAssetPath(
            options.matrix_path, NodeString(contract_assets, "closure_process"));
        const std::filesystem::path single_case_contract_path = ResolveAssetPath(
            options.matrix_path, NodeString(contract_assets, "single_image_case_contract"));
        const std::filesystem::path runtime_contract_path = ResolveAssetPath(
            options.matrix_path, NodeString(contract_assets, "runtime_binding_contract"));
        const std::filesystem::path frozen_contract_path = ResolveAssetPath(
            options.matrix_path, NodeString(contract_assets, "frozen_validation_contract"));
        ContractRegistry contracts;
        ProcessDefinition process;
        if (!std::filesystem::is_regular_file(composite_path) ||
            !std::filesystem::is_regular_file(single_case_contract_path) ||
            !std::filesystem::is_regular_file(runtime_contract_path) ||
            !std::filesystem::is_regular_file(frozen_contract_path) ||
            !LoadContractRegistry(taxonomy_path, annotation_path, model_path, evaluator_path, contracts, reason) ||
            !LoadProcessDefinition(process_path, process, reason))
        {
            if (reason.empty())
                reason = "automatic diagnostic composite pipeline contract is missing";
            result.status = "ASSET_PREFLIGHT_FAIL";
            result.reason = reason;
            return false;
        }

        GatePolicy policy;
        const cv::FileNode gate = matrix["promotion_gate"];
        if (!gate.empty())
        {
            policy.minimum_case_count = NodeInt(gate, "minimum_case_count", policy.minimum_case_count);
            policy.maximum_regression_count = NodeInt(gate, "maximum_regression_count", policy.maximum_regression_count);
            policy.minimum_child_label_iou = NodeDouble(gate, "minimum_child_label_iou", policy.minimum_child_label_iou);
            policy.minimum_mean_label_iou_delta = NodeDouble(gate, "minimum_mean_label_iou_delta", policy.minimum_mean_label_iou_delta);
            policy.maximum_child_label_iou_stddev = NodeDouble(gate, "maximum_child_label_iou_stddev", policy.maximum_child_label_iou_stddev);
        }

        std::vector<ClosureRow> rows;
        for (const cv::FileNode& node : matrix["rows"])
        {
            ClosureRow row;
            row.row_id = NodeString(node, "matrix_row");
            row.review_item = NodeString(node, "review_item");
            row.implementation_phase = NodeInt(node, "implementation_phase", 0);
            row.case_track = NodeString(node, "case_track");
            row.annotation_contract_id = NodeString(node, "annotation_contract_id");
            row.model_track_id = NodeString(node, "model_track_id");
            row.evaluator_id = NodeString(node, "evaluator_id");
            row.source_manifest = ResolveAssetPath(options.matrix_path, NodeString(node, "source_manifest"));
            row.source_set_id = NodeString(node, "source_set_id");
            const auto assignment = contracts.case_assignments.find(row.source_set_id);
            row.taxonomy_valid = contracts.case_tracks.count(row.case_track) != 0 &&
                assignment != contracts.case_assignments.end() && assignment->second == row.case_track;
            const auto annotation = contracts.annotation_tracks.find(row.annotation_contract_id);
            row.annotation_contract_valid = annotation != contracts.annotation_tracks.end() &&
                annotation->second == row.case_track;
            const auto model = contracts.model_tracks.find(row.model_track_id);
            row.model_track_valid = model != contracts.model_tracks.end() &&
                model->second.count(row.case_track) != 0;
            const auto evaluator = contracts.evaluator_tracks.find(row.evaluator_id);
            row.evaluator_contract_valid = evaluator != contracts.evaluator_tracks.end() &&
                evaluator->second.count(row.case_track) != 0;
            const auto evaluator_status = contracts.evaluator_status.find(row.evaluator_id);
            row.evaluator_runtime_status = evaluator_status == contracts.evaluator_status.end()
                ? "missing" : evaluator_status->second;
            row.evaluator_runtime_supported = row.evaluator_runtime_status == "implemented";
            row.binding_package_path = ResolveAssetPath(
                options.matrix_path, NodeString(node, "binding_package"));
            if (!row.binding_package_path.empty())
            {
                std::string package_reason;
                row.binding_package_valid = LoadBindingPackage(
                    row.binding_package_path, row.case_track, row, package_reason);
                if (!row.binding_package_valid)
                    row.label_binding_status = "binding_package_invalid";
            }
            else
            {
                row.parent = ParseBinding(node["parent_binding"]);
                row.child = ParseBinding(node["child_binding"]);
                row.parent.manifest_path = ResolveAssetPath(options.matrix_path, row.parent.manifest_path);
                row.child.manifest_path = ResolveAssetPath(options.matrix_path, row.child.manifest_path);
                row.parent.result_ref = ResolveAssetPath(options.matrix_path, row.parent.result_ref);
                row.child.result_ref = ResolveAssetPath(options.matrix_path, row.child.result_ref);
                row.parent.mask_path = ResolveAssetPath(options.matrix_path, row.parent.mask_path);
                row.child.mask_path = ResolveAssetPath(options.matrix_path, row.child.mask_path);
                row.parent.overlay_path = ResolveAssetPath(options.matrix_path, row.parent.overlay_path);
                row.child.overlay_path = ResolveAssetPath(options.matrix_path, row.child.overlay_path);
                row.parent.input_image_path = ResolveAssetPath(options.matrix_path, row.parent.input_image_path);
                row.child.input_image_path = ResolveAssetPath(options.matrix_path, row.child.input_image_path);
                ValidateBindingManifest(row.parent);
                ValidateBindingManifest(row.child);


                const cv::FileNode label = node["dataset_label_mask_binding"];
                if (label.isString())
                    row.label_binding_status = static_cast<std::string>(label);
                else if (!label.empty())
                {
                    row.label_binding_status = NodeString(label, "status");
                    row.label_mask_path = ResolveAssetPath(options.matrix_path, NodeString(label, "mask_path"));
                }
                else
                    row.label_binding_status = "missing";
            }

            const cv::FileNode candidate = node["cximage_candidate"];
            row.candidate.algorithm_id = NodeString(candidate, "algorithm_id");
            row.candidate.threshold = NodeDouble(candidate, "threshold", 0.5);
            const cv::FileNode roi = candidate["roi_xyxy"];
            if (roi.isSeq() && roi.size() == 4)
            {
                row.candidate.has_roi = true;
                row.candidate.roi_x0 = static_cast<int>(roi[0]);
                row.candidate.roi_y0 = static_cast<int>(roi[1]);
                row.candidate.roi_x1 = static_cast<int>(roi[2]);
                row.candidate.roi_y1 = static_cast<int>(roi[3]);
            }

            if (row.binding_package_path.empty())
            {
                const cv::FileNode samples = node["frozen_validation_set"];
                row.explicit_frozen_validation = samples.isSeq() && !samples.empty();
                if (samples.isSeq())
                {
                    for (const cv::FileNode& sample_node : samples)
                    {
                        ClosureSample sample;
                        sample.case_id = NodeString(sample_node, "case_id");
                        sample.image_path = ResolveAssetPath(options.matrix_path, NodeString(sample_node, "image_path"));
                        sample.label_mask_path = ResolveAssetPath(options.matrix_path, NodeString(sample_node, "label_mask_path"));
                        row.samples.push_back(sample);
                    }
                }
                if (row.samples.empty())
                {
                    ClosureSample sample;
                    sample.case_id = row.row_id;
                    sample.image_path = ResolveFirstManifestImage(row.source_manifest, row.source_set_id);
                    sample.label_mask_path = row.label_mask_path;
                    row.samples.push_back(sample);
                }
            }
            rows.push_back(row);
        }

        result.discovered_rows = static_cast<int>(rows.size());
        int invalid_contract_rows = 0;
        for (const ClosureRow& row : rows)
        {
            const bool contracts_valid = row.taxonomy_valid && row.annotation_contract_valid &&
                row.model_track_valid && row.evaluator_contract_valid &&
                (row.binding_package_path.empty() || row.binding_package_valid);
            if (!contracts_valid)
                ++invalid_contract_rows;
            bool all_sample_images_bound = !row.samples.empty();
            bool all_independent_labels_bound = !row.samples.empty();
            for (const ClosureSample& sample : row.samples)
            {
                all_sample_images_bound = all_sample_images_bound &&
                    std::filesystem::is_regular_file(sample.image_path);
                all_independent_labels_bound = all_independent_labels_bound &&
                    std::filesystem::is_regular_file(sample.label_mask_path);
            }
            const bool independent_label_declared =
                row.label_binding_status == "bound";
            const bool independent_label_binding_valid =
                !independent_label_declared ||
                (row.typed_label_independent && all_independent_labels_bound);
            if (contracts_valid && row.evaluator_runtime_supported &&
                row.parent.bound && row.child.bound &&
                all_sample_images_bound && independent_label_binding_valid)
                ++result.bound_rows;
        }

        result.preflight_ref = options.output_dir / "binding_preflight.json";
        result.process_status_ref = options.output_dir / "closure_process_status.json";
        if (invalid_contract_rows != 0)
        {
            result.status = "ASSET_PREFLIGHT_FAIL";
            result.reason = "one or more matrix rows violate taxonomy, annotation, model, or evaluator contracts";
            if (!WritePreflight(result.preflight_ref, rows, result, reason))
                return false;
            if (!WriteProcessStatus(result.process_status_ref, process, rows, result, reason))
                return false;
            reason = result.reason;
            return false;
        }
        if (result.bound_rows != result.discovered_rows || result.discovered_rows == 0)
        {
            result.status = "PENDING_BINDING";
            result.reason = "typed evaluator runtime, parent, child, or source image binding is incomplete";
            if (!WritePreflight(result.preflight_ref, rows, result, reason))
                return false;
            if (!WriteProcessStatus(result.process_status_ref, process, rows, result, reason))
                return false;
            reason.clear();
            return true;
        }

        result.status = "ASSET_PREFLIGHT_PASS";
        result.reason =
            "paired consistency execution bindings are complete; independent labels remain optional";
        if (!WritePreflight(result.preflight_ref, rows, result, reason))
            return false;
        if (!WriteProcessStatus(result.process_status_ref, process, rows, result, reason))
            return false;

        std::vector<CompletedCase> completed;
        CxTorchExecutionAdapter adapter;
        for (const ClosureRow& row : rows)
        {
            for (const ClosureSample& sample : row.samples)
            {
                const std::filesystem::path case_dir =
                    options.output_dir / "cases" / row.row_id / sample.case_id;
                CxPairedInferenceRequest request;
                request.parent_task = MakeTask(row.parent, sample, case_dir / "parent");
                request.child_task = MakeTask(row.child, sample, case_dir / "child");
                if (row.label_binding_status == "bound" &&
                    row.typed_label_independent &&
                    std::filesystem::is_regular_file(sample.label_mask_path))
                {
                    request.dataset_label_mask_path = sample.label_mask_path;
                }
                request.report_path = case_dir / "paired_inference_diagnostic.json";
                request.cximage_candidate_request = row.candidate;
                request.cximage_candidate_request->input_image_path = sample.image_path;
                request.cximage_candidate_request->output_dir = case_dir / "cximage";

                CxPairedInferenceDiagnostic diagnostic;
                ++result.executed_cases;
                if (!adapter.ExecutePair(request, diagnostic, reason) ||
                    !diagnostic.complete)
                {
                    ++result.rejected_cases;
                    result.status = diagnostic.status.empty() ? "FAIL" : diagnostic.status;
                    result.reason = reason.empty() ? diagnostic.reason : reason;
                    return false;
                }
                CompletedCase item;
                item.row_id = row.row_id;
                item.case_id = sample.case_id;
                item.parent_child_iou = diagnostic.parent_child.iou;
                item.label_available =
                    diagnostic.parent_label.has_value() &&
                    diagnostic.child_label.has_value();
                if (item.label_available)
                {
                    item.parent_label_iou = diagnostic.parent_label->iou;
                    item.child_label_iou = diagnostic.child_label->iou;
                    item.label_delta =
                        item.child_label_iou - item.parent_label_iou;
                }
                completed.push_back(item);
                ++result.completed_cases;
            }
        }

        if (!WriteAggregateReports(options.output_dir, completed, policy, result, reason))
            return false;
        if (!WriteProcessStatus(result.process_status_ref, process, rows, result, reason))
            return false;
        result.complete = true;
        reason.clear();
        return true;
    }
    catch (const cv::Exception& error)
    {
        reason = error.what();
    }
    catch (const std::exception& error)
    {
        reason = error.what();
    }
    result.status = "FAIL";
    result.reason = reason;
    return false;
}