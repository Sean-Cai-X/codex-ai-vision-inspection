#include "torch_runtime_task_dispatcher.h"
#include "torch_runtime_task_types.h"
#include "torch_runtime_manifest.h"
#include "torch_runtime_segmentation_executor.h"
#include "torch_runtime_detection_executor.h"
#include "torch_runtime_edgesam_executor.h"
#include "torch_runtime_yolov8_seg_executor.h"
#include "torch_prototype_index.h"
#include "torch_v8.h"

TorchTaskResultCpp ExecuteTorchPrototypeLifecycleTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request);

TorchTaskResultCpp ValidateTorchIncrementalPackageTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request,
    const std::string& model_family);
TorchTaskResultCpp RunYoloV8TrainingLifecycleTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request);
#include "torch_segmentation_mainline_bridge.h"
#include "torch_test_host.h"
#include <chrono>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <limits>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <sstream>
#include <vector>

namespace
{

TorchTaskResultCpp MakeUnsupportedTask(
    const std::string& task)
{
    TorchTaskResultCpp result;

    result.ok = false;
    result.error_code =
        static_cast<int>(
            TorchRuntimeErrorCode::UnsupportedTask);

    result.status = "unsupported_task";
    result.error_message =
        "unsupported production torch task: " + task;

    result.result_json =
        "{"
        "\"schema\":\"cxvision.torch.error.v1\","
        "\"failure_stage\":\"runtime_task_unsupported\","
        "\"task\":\"" + task + "\""
        "}";

    return result;
}

bool IsLegacyTestHostTask(
    const std::string& task)
{
    return TorchTestHost::find_task_spec(task) != nullptr;
}

std::string QuoteRuntimeTaskJsonString(const std::string& value)
{
    std::ostringstream os;
    os << '"';
    for (const char ch : value)
    {
        switch (ch)
        {
        case '\\': os << "\\\\"; break;
        case '"': os << "\\\""; break;
        case '\n': os << "\\n"; break;
        case '\r': os << "\\r"; break;
        case '\t': os << "\\t"; break;
        default: os << ch; break;
        }
    }
    os << '"';
    return os.str();
}

std::string ExtractRuntimeTaskJsonString(
    const std::string& json,
    const std::string& key)
{
    const std::string marker = "\"" + key + "\"";
    std::size_t pos = json.find(marker);
    if (pos == std::string::npos)
        return {};
    pos = json.find(':', pos + marker.size());
    if (pos == std::string::npos)
        return {};
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos)
        return {};
    const std::size_t end = json.find('"', pos + 1);
    if (end == std::string::npos)
        return {};
    return json.substr(pos + 1, end - pos - 1);
}

bool ExtractRuntimeTaskJsonInt(const std::string& json, const std::string& key, int& value)
{
    const std::string marker = "\"" + key + "\"";
    std::size_t pos = json.find(marker);
    if (pos == std::string::npos) return false;
    pos = json.find(':', pos + marker.size());
    if (pos == std::string::npos) return false;
    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    const char* begin = json.c_str() + pos;
    char* end = nullptr;
    const long parsed = std::strtol(begin, &end, 10);
    if (end == begin || parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) return false;
    value = static_cast<int>(parsed);
    return true;
}

torch::Tensor MakeSegmentationLifecycleImages(
    const SegmentationMainlineRunnerConfig& config)
{
    const auto device = resolve_segmentation_device(config.device_policy);
    return torch::randn(
        {config.batch_size, 3, config.input_size, config.input_size},
        torch::TensorOptions().dtype(torch::kFloat32).device(device));
}

torch::Tensor MakeSegmentationLifecycleMasks(
    const SegmentationMainlineRunnerConfig& config)
{
    const auto device = resolve_segmentation_device(config.device_policy);
    return torch::randint(
        0,
        config.num_classes,
        {config.batch_size, config.input_size, config.input_size},
        torch::TensorOptions().dtype(torch::kLong).device(device));
}

std::string HashFileFnv1a64(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    uint64_t hash = 1469598103934665603ull;
    char buffer[4096];
    while (input.good())
    {
        input.read(buffer, sizeof(buffer));
        const std::streamsize count = input.gcount();
        for (std::streamsize i = 0; i < count; ++i)
        {
            hash ^= static_cast<unsigned char>(buffer[i]);
            hash *= 1099511628211ull;
        }
    }

    std::ostringstream os;
    os << std::hex << std::setw(16) << std::setfill('0') << hash;
    return os.str();
}

struct EvidenceNormBbox
{
    int class_id = 0;
    double cx = 0.0;
    double cy = 0.0;
    double w = 0.0;
    double h = 0.0;
};

struct EvidenceDatasetImage
{
    std::string image_id;
    std::filesystem::path path;
    std::filesystem::path mask_path;
    std::string split;
    std::string label;
    std::vector<EvidenceNormBbox> boxes;
};

std::vector<EvidenceDatasetImage> CollectRasterDatasetManifestImages(
    const TorchTaskRequestCpp& request,
    const int max_images_per_split)
{
    if (request.manifest_path.empty())
        return {};
    std::ifstream input(request.manifest_path);
    if (!input)
        return {};
    const std::string text(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    if (text.find("cxvision.torch.training_dataset.v2") ==
        std::string::npos)
    {
        return {};
    }

    std::vector<EvidenceDatasetImage> rows;
    EvidenceDatasetImage current;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line))
    {
        const std::string image_id =
            ExtractRuntimeTaskJsonString(line, "image_id");
        if (!image_id.empty())
            current.image_id = image_id;
        const std::string image_path =
            ExtractRuntimeTaskJsonString(line, "image_path");
        if (!image_path.empty())
            current.path = image_path;
        const std::string mask_path =
            ExtractRuntimeTaskJsonString(line, "mask_path");
        if (!mask_path.empty())
            current.mask_path = mask_path;
        const std::string split =
            ExtractRuntimeTaskJsonString(line, "split");
        if (!split.empty())
            current.split = split;
        const std::string label =
            ExtractRuntimeTaskJsonString(line, "label");
        if (!label.empty())
            current.label = label;

        if (line.find("    }") != std::string::npos &&
            !current.image_id.empty() && !current.path.empty() &&
            !current.mask_path.empty())
        {
            int split_count = 0;
            for (const EvidenceDatasetImage& row : rows)
            {
                if (row.split == current.split)
                    ++split_count;
            }
            if (split_count < max_images_per_split)
                rows.push_back(current);
            current = {};
        }
    }
    return rows;
}

struct EvidenceSplitSummary
{
    int train_images = 0;
    int val_images = 0;
    int test_images = 0;
    int train_annotations = 0;
    int val_annotations = 0;
    int test_annotations = 0;
};

struct LifecycleEvidenceArtifacts
{
    int total_annotations = 0;
    EvidenceSplitSummary split_summary;
    std::string split_summary_ref;
    std::string mask_preview_ref;
    std::string bbox_overlay_ref;
};

std::string ExtractEvidencePayload(const std::string& line)
{
    const std::size_t first = line.find('"');
    const std::size_t last = line.rfind('"');
    if (first == std::string::npos || last == std::string::npos || last <= first)
        return {};
    return line.substr(first + 1, last - first - 1);
}

std::map<std::string, std::string> ParseEvidenceKeyValuePayload(
    const std::string& payload)
{
    std::map<std::string, std::string> values;
    std::istringstream input(payload);
    std::string token;
    while (input >> token)
    {
        const std::size_t equals = token.find('=');
        if (equals == std::string::npos || equals == 0)
            continue;
        values[token.substr(0, equals)] = token.substr(equals + 1);
    }
    return values;
}

double ParseEvidenceDouble(
    const std::map<std::string, std::string>& values,
    const std::string& key)
{
    const auto it = values.find(key);
    if (it == values.end())
        return 0.0;
    try
    {
        return std::stod(it->second);
    }
    catch (...)
    {
        return 0.0;
    }
}

int ParseEvidenceInt(
    const std::map<std::string, std::string>& values,
    const std::string& key)
{
    const auto it = values.find(key);
    if (it == values.end())
        return 0;
    try
    {
        return std::stoi(it->second);
    }
    catch (...)
    {
        return 0;
    }
}

std::vector<EvidenceDatasetImage> CollectEvidenceManifestAnnotatedImages(
    const TorchTaskRequestCpp& request,
    const int max_images_per_split,
    int& annotation_count)
{
    annotation_count = 0;
    if (request.manifest_path.empty())
        return {};

    std::filesystem::path manifest_path(request.manifest_path);
    std::error_code ec;
    if (!std::filesystem::exists(manifest_path, ec))
        return {};

    std::ifstream manifest(manifest_path);
    if (!manifest)
        return {};

    std::vector<EvidenceDatasetImage> rows;
    std::map<std::string, std::vector<EvidenceNormBbox>> boxes_by_image_id;
    std::string line;
    while (std::getline(manifest, line))
    {
        if (line.find("CxEvidenceChain_case_adddatasetimage(") != std::string::npos)
        {
            const auto values =
                ParseEvidenceKeyValuePayload(ExtractEvidencePayload(line));
            const auto image_id = values.find("image_id");
            const auto path = values.find("path");
            if (image_id == values.end() || path == values.end())
                continue;

            EvidenceDatasetImage row;
            row.image_id = image_id->second;
            row.path = std::filesystem::path(path->second);
            const auto split = values.find("split");
            const auto label = values.find("label");
            row.split = split == values.end() ? "" : split->second;
            row.label = label == values.end() ? "" : label->second;
            rows.push_back(row);
            continue;
        }

        if (line.find("CxEvidenceChain_case_addbbox_xywh_norm(") != std::string::npos)
        {
            const auto values =
                ParseEvidenceKeyValuePayload(ExtractEvidencePayload(line));
            const auto image_id = values.find("image_id");
            if (image_id == values.end())
                continue;

            EvidenceNormBbox box;
            box.class_id = ParseEvidenceInt(values, "class_id");
            box.cx = ParseEvidenceDouble(values, "cx");
            box.cy = ParseEvidenceDouble(values, "cy");
            box.w = ParseEvidenceDouble(values, "w");
            box.h = ParseEvidenceDouble(values, "h");
            if (box.w <= 0.0 || box.h <= 0.0)
                continue;
            boxes_by_image_id[image_id->second].push_back(box);
        }
    }

    std::vector<EvidenceDatasetImage> annotated;
    for (auto& row : rows)
    {
        const auto boxes = boxes_by_image_id.find(row.image_id);
        if (boxes == boxes_by_image_id.end() || boxes->second.empty())
            continue;
        if (!std::filesystem::exists(row.path, ec))
            continue;
        row.boxes = boxes->second;
        annotated.push_back(row);
    }

    std::sort(
        annotated.begin(),
        annotated.end(),
        [](const EvidenceDatasetImage& lhs, const EvidenceDatasetImage& rhs)
        {
            if (lhs.split != rhs.split)
                return lhs.split < rhs.split;
            if (lhs.image_id != rhs.image_id)
                return lhs.image_id < rhs.image_id;
            return lhs.path.string() < rhs.path.string();
        });

    std::vector<EvidenceDatasetImage> selected;
    std::map<std::string, int> per_split_count;
    for (const auto& row : annotated)
    {
        int& split_count = per_split_count[row.split];
        if (max_images_per_split > 0 && split_count >= max_images_per_split)
            continue;
        selected.push_back(row);
        ++split_count;
    }

    for (const auto& row : selected)
        annotation_count += static_cast<int>(row.boxes.size());
    return selected;
}

std::vector<EvidenceDatasetImage> SelectEvidenceSplitImages(
    const std::vector<EvidenceDatasetImage>& images,
    const std::string& split)
{
    std::vector<EvidenceDatasetImage> selected;
    for (const auto& image : images)
    {
        if (image.split == split)
            selected.push_back(image);
    }
    return selected;
}

EvidenceSplitSummary MakeEvidenceSplitSummary(
    const std::vector<EvidenceDatasetImage>& images)
{
    EvidenceSplitSummary summary;
    for (const auto& image : images)
    {
        const int annotations = static_cast<int>(image.boxes.size());
        if (image.split == "train")
        {
            ++summary.train_images;
            summary.train_annotations += annotations;
        }
        else if (image.split == "val")
        {
            ++summary.val_images;
            summary.val_annotations += annotations;
        }
        else if (image.split == "test")
        {
            ++summary.test_images;
            summary.test_annotations += annotations;
        }
    }
    return summary;
}

cv::Mat MakeMaskFromEvidenceBboxes(
    const EvidenceDatasetImage& image,
    const int input_size)
{
    cv::Mat mask(input_size, input_size, CV_8U, cv::Scalar(0));
    for (const auto& box : image.boxes)
    {
        const int x0 = std::max(
            0,
            std::min(
                input_size - 1,
                static_cast<int>(std::round((box.cx - box.w * 0.5) * input_size))));
        const int y0 = std::max(
            0,
            std::min(
                input_size - 1,
                static_cast<int>(std::round((box.cy - box.h * 0.5) * input_size))));
        const int x1 = std::max(
            0,
            std::min(
                input_size - 1,
                static_cast<int>(std::round((box.cx + box.w * 0.5) * input_size))));
        const int y1 = std::max(
            0,
            std::min(
                input_size - 1,
                static_cast<int>(std::round((box.cy + box.h * 0.5) * input_size))));
        if (x1 < x0 || y1 < y0)
            continue;
        cv::rectangle(
            mask,
            cv::Rect(x0, y0, x1 - x0 + 1, y1 - y0 + 1),
            cv::Scalar(1),
            cv::FILLED);
    }
    return mask;
}

void DrawEvidenceBboxes(
    cv::Mat& image,
    const EvidenceDatasetImage& evidence)
{
    for (const auto& box : evidence.boxes)
    {
        const int width = image.cols;
        const int height = image.rows;
        const int x0 = std::max(
            0,
            std::min(
                width - 1,
                static_cast<int>(std::round((box.cx - box.w * 0.5) * width))));
        const int y0 = std::max(
            0,
            std::min(
                height - 1,
                static_cast<int>(std::round((box.cy - box.h * 0.5) * height))));
        const int x1 = std::max(
            0,
            std::min(
                width - 1,
                static_cast<int>(std::round((box.cx + box.w * 0.5) * width))));
        const int y1 = std::max(
            0,
            std::min(
                height - 1,
                static_cast<int>(std::round((box.cy + box.h * 0.5) * height))));
        if (x1 < x0 || y1 < y0)
            continue;
        cv::rectangle(
            image,
            cv::Rect(x0, y0, x1 - x0 + 1, y1 - y0 + 1),
            cv::Scalar(0, 0, 255),
            2);
    }
}

void WriteEvidenceSplitArtifacts(
    const std::filesystem::path& output_dir,
    const std::vector<EvidenceDatasetImage>& all_images,
    const EvidenceSplitSummary& summary,
    const int total_annotations,
    const int input_size,
    LifecycleEvidenceArtifacts& artifacts)
{
    if (output_dir.empty() || all_images.empty())
        return;

    std::filesystem::create_directories(output_dir);
    const std::filesystem::path summary_path =
        output_dir / "torch_training_dataset_split_summary.json";
    const std::filesystem::path mask_path =
        output_dir / "torch_training_mask_preview.png";
    const std::filesystem::path overlay_path =
        output_dir / "torch_training_bbox_overlay.png";

    {
        std::ofstream output(summary_path);
        output << "{";
        output << "\"schema\":\"cxvision.torch.training_dataset_split.v1\",";
        output << "\"source\":\"evidence_manifest\",";
        output << "\"total_annotations\":" << total_annotations << ",";
        output << "\"train_images\":" << summary.train_images << ",";
        output << "\"train_annotations\":" << summary.train_annotations << ",";
        output << "\"val_images\":" << summary.val_images << ",";
        output << "\"val_annotations\":" << summary.val_annotations << ",";
        output << "\"test_images\":" << summary.test_images << ",";
        output << "\"test_annotations\":" << summary.test_annotations;
        output << "}\n";
        artifacts.split_summary_ref = summary_path.string();
    }

    const EvidenceDatasetImage& preview = all_images.front();
    cv::Mat bgr = cv::imread(preview.path.string(), cv::IMREAD_COLOR);
    if (bgr.empty())
        return;

    cv::Mat resized;
    cv::resize(
        bgr,
        resized,
        cv::Size(input_size, input_size),
        0.0,
        0.0,
        cv::INTER_AREA);

    cv::Mat mask = MakeMaskFromEvidenceBboxes(preview, input_size);
    cv::Mat mask_visual;
    mask.convertTo(mask_visual, CV_8U, 255.0);
    cv::imwrite(mask_path.string(), mask_visual);
    artifacts.mask_preview_ref = mask_path.string();

    cv::Mat overlay = resized.clone();
    DrawEvidenceBboxes(overlay, preview);
    cv::imwrite(overlay_path.string(), overlay);
    artifacts.bbox_overlay_ref = overlay_path.string();
}

std::vector<std::filesystem::path> CollectSegmentationLifecycleImages(
    const TorchTaskRequestCpp& request,
    const int max_images)
{
    std::vector<std::filesystem::path> images;
    if (request.input_image.empty())
        return images;

    std::filesystem::path input_path(request.input_image);
    std::error_code ec;
    if (!std::filesystem::exists(input_path, ec))
        return images;

    images.push_back(input_path);
    const std::filesystem::path parent = input_path.parent_path();
    if (!parent.empty() && std::filesystem::is_directory(parent, ec))
    {
        for (const auto& entry : std::filesystem::directory_iterator(parent, ec))
        {
            if (ec || !entry.is_regular_file())
                continue;
            const std::filesystem::path path = entry.path();
            const std::string name = path.filename().string();
            if (name.find("torch_deeppcb_") == std::string::npos ||
                name.find("_input") == std::string::npos)
                continue;
            if (path == input_path)
                continue;
            images.push_back(path);
        }
    }

    std::sort(images.begin(), images.end());
    images.erase(std::unique(images.begin(), images.end()), images.end());
    if (static_cast<int>(images.size()) > max_images)
        images.resize(static_cast<std::size_t>(max_images));
    return images;
}

bool BuildEvidenceImageBatch(
    const std::vector<EvidenceDatasetImage>& evidence_images,
    SegmentationMainlineRunnerConfig& config,
    const int target_class_id,
    torch::Tensor& images,
    torch::Tensor& masks,
    double& mask_foreground_ratio,
    std::string& image_hash)
{
    if (evidence_images.empty())
        return false;

    std::vector<torch::Tensor> image_tensors;
    std::vector<torch::Tensor> mask_tensors;
    uint64_t combined_hash = 1469598103934665603ull;
    double foreground_pixels = 0.0;
    double total_pixels = 0.0;

    for (const auto& evidence : evidence_images)
    {
        cv::Mat bgr = cv::imread(evidence.path.string(), cv::IMREAD_COLOR);
        if (bgr.empty())
            continue;

        cv::Mat resized;
        cv::resize(
            bgr,
            resized,
            cv::Size(config.input_size, config.input_size),
            0.0,
            0.0,
            cv::INTER_AREA);

        cv::Mat rgb;
        cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);

        cv::Mat float_image;
        rgb.convertTo(float_image, CV_32F, 1.0 / 255.0);
        auto image_tensor = torch::from_blob(
            float_image.data,
            {config.input_size, config.input_size, 3},
            torch::kFloat32).clone().permute({2, 0, 1});
        image_tensors.push_back(image_tensor);

        cv::Mat binary;
        if (evidence.mask_path.empty())
        {
            // BBox or threshold-derived masks are visualization aids only.
            // They must not silently become segmentation supervision.
            return false;
        }
        cv::Mat source_mask =
            cv::imread(evidence.mask_path.string(), cv::IMREAD_GRAYSCALE);
        if (source_mask.empty())
            return false;
        cv::resize(
            source_mask,
            binary,
            cv::Size(config.input_size, config.input_size),
            0.0,
            0.0,
            cv::INTER_NEAREST);
        cv::compare(binary, target_class_id, binary, cv::CMP_EQ);
        binary /= 255;
        if (config.num_classes > 2 && target_class_id >= 0)
            binary *= (target_class_id + 1);
        foreground_pixels += static_cast<double>(cv::countNonZero(binary));
        total_pixels += static_cast<double>(binary.rows * binary.cols);
        auto mask_tensor = torch::from_blob(
            binary.data,
            {config.input_size, config.input_size},
            torch::kUInt8).clone().to(torch::kLong);
        mask_tensors.push_back(mask_tensor);

        const std::string file_hash = HashFileFnv1a64(evidence.path);
        for (const char ch : file_hash)
        {
            combined_hash ^= static_cast<unsigned char>(ch);
            combined_hash *= 1099511628211ull;
        }
    }

    if (image_tensors.empty())
        return false;

    images = torch::stack(image_tensors, 0);
    masks = torch::stack(mask_tensors, 0);
    mask_foreground_ratio =
        total_pixels > 0.0 ? foreground_pixels / total_pixels : 0.0;
    std::ostringstream hash_os;
    hash_os << std::hex << std::setw(16) << std::setfill('0') << combined_hash;
    image_hash = hash_os.str();
    return true;
}

bool BuildSegmentationLifecycleRealImageBatch(
    const TorchTaskRequestCpp& request,
    SegmentationMainlineRunnerConfig& config,
    const int target_class_id,
    torch::Tensor& images,
    torch::Tensor& masks,
    torch::Tensor& eval_images,
    torch::Tensor& eval_masks,
    int& image_count,
    int& eval_image_count,
    double& mask_foreground_ratio,
    std::string& image_hash,
    std::string& dataset_source,
    std::string& mask_source,
    LifecycleEvidenceArtifacts& artifacts)
{
    int annotation_count = 0;
    std::vector<EvidenceDatasetImage> manifest_images =
        CollectRasterDatasetManifestImages(request, 4);
    if (!manifest_images.empty())
    {
        for (const EvidenceDatasetImage& row : manifest_images)
        {
            if (!row.mask_path.empty())
                ++annotation_count;
        }
    }
    else
    {
        manifest_images =
            CollectEvidenceManifestAnnotatedImages(request, 4, annotation_count);
    }
    artifacts.total_annotations = annotation_count;
    std::vector<std::filesystem::path> image_paths;
    if (!manifest_images.empty())
    {
        artifacts.split_summary = MakeEvidenceSplitSummary(manifest_images);
        const std::vector<EvidenceDatasetImage> train_images =
            SelectEvidenceSplitImages(manifest_images, "train");
        std::vector<EvidenceDatasetImage> validation_images =
            SelectEvidenceSplitImages(manifest_images, "val");
        if (validation_images.empty())
            validation_images = SelectEvidenceSplitImages(manifest_images, "test");
        const bool business_incremental =
            request.task == TorchRuntimeTaskIds::SegmentationBusinessIncremental;
        if (train_images.empty() ||
            (business_incremental && validation_images.empty()))
            return false;

        if (!BuildEvidenceImageBatch(
                train_images,
                config,
                target_class_id,
                images,
                masks,
                mask_foreground_ratio,
                image_hash))
        {
            return false;
        }

        double eval_mask_foreground_ratio = 0.0;
        std::string eval_image_hash;
        if (!BuildEvidenceImageBatch(
                validation_images.empty() ? train_images : validation_images,
                config,
                target_class_id,
                eval_images,
                eval_masks,
                eval_mask_foreground_ratio,
                eval_image_hash))
        {
            return false;
        }
        if (!business_incremental &&
            eval_images.defined() && eval_images.size(0) == 1)
        {
            eval_images = torch::cat({eval_images, eval_images.clone()}, 0);
            eval_masks = torch::cat({eval_masks, eval_masks.clone()}, 0);
        }

        image_count = static_cast<int>(images.size(0));
        eval_image_count = static_cast<int>(eval_images.size(0));
        config.batch_size = image_count;
        dataset_source = "evidence_manifest";
        mask_source = manifest_images.front().mask_path.empty()
            ? "bbox_xywh_norm"
            : "raster_mask_png";
        WriteEvidenceSplitArtifacts(
            std::filesystem::path(request.output_dir),
            manifest_images,
            artifacts.split_summary,
            annotation_count,
            config.input_size,
            artifacts);
        return true;
    }

    // A directory of arbitrary images has no immutable split membership or
    // accepted masks.  Do not derive supervision from pixels as a fallback.
    return false;

    image_paths = CollectSegmentationLifecycleImages(request, 4);
    if (image_paths.empty())
    {
        return false;
    }

    std::vector<torch::Tensor> image_tensors;
    std::vector<torch::Tensor> mask_tensors;
    uint64_t combined_hash = 1469598103934665603ull;
    double foreground_pixels = 0.0;
    double total_pixels = 0.0;

    for (std::size_t i = 0; i < image_paths.size(); ++i)
    {
        const std::filesystem::path& path = image_paths[i];
        cv::Mat bgr = cv::imread(path.string(), cv::IMREAD_COLOR);
        if (bgr.empty())
            continue;

        cv::Mat resized;
        cv::resize(
            bgr,
            resized,
            cv::Size(config.input_size, config.input_size),
            0.0,
            0.0,
            cv::INTER_AREA);

        cv::Mat rgb;
        cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);

        cv::Mat float_image;
        rgb.convertTo(float_image, CV_32F, 1.0 / 255.0);
        auto image_tensor = torch::from_blob(
            float_image.data,
            {config.input_size, config.input_size, 3},
            torch::kFloat32).clone().permute({2, 0, 1});
        image_tensors.push_back(image_tensor);

        cv::Mat binary;
        cv::Mat gray;
        cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);
        cv::threshold(gray, binary, 220, 1, cv::THRESH_BINARY_INV);
        foreground_pixels += static_cast<double>(cv::countNonZero(binary));
        total_pixels += static_cast<double>(binary.rows * binary.cols);
        auto mask_tensor = torch::from_blob(
            binary.data,
            {config.input_size, config.input_size},
            torch::kUInt8).clone().to(torch::kLong);
        mask_tensors.push_back(mask_tensor);

        const std::string file_hash = HashFileFnv1a64(path);
        for (const char ch : file_hash)
        {
            combined_hash ^= static_cast<unsigned char>(ch);
            combined_hash *= 1099511628211ull;
        }
    }

    if (image_tensors.empty())
        return false;

    images = torch::stack(image_tensors, 0);
    masks = torch::stack(mask_tensors, 0);
    eval_images = images.clone();
    eval_masks = masks.clone();
    image_count = static_cast<int>(image_tensors.size());
    eval_image_count = image_count;
    config.batch_size = image_count;
    mask_foreground_ratio =
        total_pixels > 0.0 ? foreground_pixels / total_pixels : 0.0;
    std::ostringstream hash_os;
    hash_os << std::hex << std::setw(16) << std::setfill('0') << combined_hash;
    image_hash = hash_os.str();
    dataset_source = "input_image_directory";
    mask_source = "threshold_binary_inv_220";
    return true;
}

TorchTaskResultCpp RunSegmentationTrainingLifecycleTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    TorchTaskResultCpp result;
    result.requested_device = config.device.empty() ? "cpu" : config.device;
    constexpr uint64_t deterministic_seed = 1023;
    int lifecycle_image_count = 0;
    int lifecycle_eval_image_count = 0;
    double lifecycle_mask_foreground_ratio = 0.0;
    std::string lifecycle_image_hash;
    std::string lifecycle_dataset_source = "synthetic_random";
    std::string lifecycle_mask_source = "synthetic_random";
    LifecycleEvidenceArtifacts lifecycle_artifacts;

    const auto start = std::chrono::steady_clock::now();

    try
    {
        torch::manual_seed(deterministic_seed);

        const bool business_incremental =
            request.task == TorchRuntimeTaskIds::SegmentationBusinessIncremental;
        int target_class_id = 1;
        int requested_epochs = 1;
        if (business_incremental)
        {
            TORCH_CHECK(
                ExtractRuntimeTaskJsonInt(request.extra_json, "target_class_id", target_class_id) &&
                target_class_id >= 0 && target_class_id <= 6,
                "business segmentation requires target_class_id in [0,6]");
            TORCH_CHECK(
                !ExtractRuntimeTaskJsonString(request.extra_json, "parent_weights").empty(),
                "business segmentation requires a versioned parent_weights artifact");
            TORCH_CHECK(
                ExtractRuntimeTaskJsonInt(request.extra_json, "epochs", requested_epochs) &&
                requested_epochs >= 2 && requested_epochs <= 200,
                "business segmentation requires epochs in [2,200]");
            const std::filesystem::path trial_output(request.output_dir);
            const bool trial_root = std::find(
                trial_output.begin(), trial_output.end(),
                std::filesystem::path("development_trials")) != trial_output.end() ||
                std::find(trial_output.begin(), trial_output.end(),
                          std::filesystem::path("isolated_business_validation")) != trial_output.end();
            TORCH_CHECK(
                !request.output_dir.empty() && trial_root,
                "business segmentation output must remain under development_trials or isolated_business_validation");
        }

        auto runner_config =
            make_segmentation_mainline_runner_config(
                "deeplabv3plus",
                "mobilenet_v3_large",
                business_incremental ? 8 : 2,
                128,
                2);

        runner_config.enable_smoke_train = true;
        runner_config.enable_eval = true;
        runner_config.device_policy =
            (result.requested_device == "cuda" && torch::cuda::is_available())
                ? SegmentationDevicePolicy::ForceCUDA
                : SegmentationDevicePolicy::ForceCPU;

        torch::Tensor train_images;
        torch::Tensor train_masks;
        torch::Tensor eval_images;
        torch::Tensor eval_masks;
        if (!BuildSegmentationLifecycleRealImageBatch(
                request,
                runner_config,
                target_class_id,
                train_images,
                train_masks,
                eval_images,
                eval_masks,
                lifecycle_image_count,
                lifecycle_eval_image_count,
                lifecycle_mask_foreground_ratio,
                lifecycle_image_hash,
                lifecycle_dataset_source,
                lifecycle_mask_source,
                lifecycle_artifacts))
        {
            TORCH_CHECK(false,
                "segmentation lifecycle requires a manifest-bound train and validation image/mask dataset; "
                "synthetic, bounding-box, and image-threshold supervision are disabled");
        }

        const torch::Device device =
            resolve_segmentation_device(runner_config.device_policy);
        DeepLabV3Plus trained_model(
            runner_config.backbone, runner_config.num_classes);
        const std::string parent_weights =
            ExtractRuntimeTaskJsonString(
                request.extra_json, "parent_weights");
        if (!parent_weights.empty())
        {
            TORCH_CHECK(
                std::filesystem::exists(parent_weights),
                "parent_weights does not exist: ", parent_weights);
            torch::load(trained_model, parent_weights);
        }
        trained_model->to(device);
        trained_model->train();

        torch::optim::Adam optimizer(
            trained_model->parameters(),
            torch::optim::AdamOptions(runner_config.optimizer.lr)
                .weight_decay(runner_config.optimizer.weight_decay));
        train_images = train_images.to(device);
        train_masks = train_masks.to(device, torch::kLong);
        torch::Tensor train_loss;
        double grad_mean = 0.0;
        bool has_grad = false;
        for (int epoch = 0; epoch < requested_epochs; ++epoch)
        {
            optimizer.zero_grad();
            torch::Tensor train_logits =
                trained_model->forward(train_images).at("out");
            train_loss = torch::nn::functional::cross_entropy(train_logits, train_masks);
            TORCH_CHECK(
                torch::isfinite(train_loss).item<bool>(),
                "persistent segmentation training loss is not finite");
            train_loss.backward();
            for (const torch::Tensor& parameter : trained_model->parameters())
            {
                if (parameter.grad().defined())
                {
                    grad_mean = parameter.grad().abs().mean().item<double>();
                    has_grad = true;
                    break;
                }
            }
            TORCH_CHECK(
                has_grad,
                "persistent segmentation training produced no gradients");
            optimizer.step();
        }

        trained_model->eval();
        eval_images = eval_images.to(device);
        eval_masks = eval_masks.to(device, torch::kLong);
        double eval_loss = 0.0;
        double foreground_iou = 0.0;
        double avg_confidence = 0.0;
        torch::Tensor eval_prediction_cpu;
        {
            torch::NoGradGuard no_grad;
            torch::Tensor eval_logits =
                trained_model->forward(eval_images).at("out");
            eval_loss = torch::nn::functional::cross_entropy(
                eval_logits, eval_masks).item<double>();
            torch::Tensor probabilities = torch::softmax(eval_logits, 1);
            torch::Tensor prediction = probabilities.argmax(1);
            eval_prediction_cpu =
                prediction.index({0}).to(torch::kUInt8).mul(255).cpu()
                    .contiguous();
            torch::Tensor pred_foreground = prediction.gt(0);
            torch::Tensor truth_foreground = eval_masks.gt(0);
            const double intersection = pred_foreground.logical_and(
                truth_foreground).sum().item<double>();
            const double union_count = pred_foreground.logical_or(
                truth_foreground).sum().item<double>();
            foreground_iou =
                union_count > 0.0 ? intersection / union_count : 1.0;
            avg_confidence =
                std::get<0>(probabilities.max(1)).mean().item<double>();
        }

        SegmentationSmokeStepResult smoke;
        smoke.loss = train_loss.item<double>();
        smoke.grad_mean = grad_mean;
        smoke.batch_size = static_cast<int>(train_images.size(0));
        smoke.input_size = runner_config.input_size;
        smoke.validate();

        std::filesystem::path output_dir(request.output_dir);
        if (!output_dir.empty())
            std::filesystem::create_directories(output_dir);
        const std::filesystem::path model_package_dir =
            output_dir / "deeplab_incremental_model";
        const std::filesystem::path model_weights_dir =
            model_package_dir / "weights";
        std::filesystem::create_directories(model_weights_dir);
        const std::filesystem::path model_weights_path =
            model_weights_dir / "deeplab_incremental.pt";
        torch::save(trained_model, model_weights_path.string());

        std::filesystem::path inference_mask_path;
        std::filesystem::path inference_overlay_path;
        cv::Mat source_image =
            cv::imread(request.input_image, cv::IMREAD_COLOR);
        if (!source_image.empty() && eval_prediction_cpu.defined())
        {
            cv::Mat small_mask(
                runner_config.input_size,
                runner_config.input_size,
                CV_8UC1,
                eval_prediction_cpu.data_ptr<unsigned char>());
            cv::Mat inference_mask;
            cv::resize(
                small_mask,
                inference_mask,
                source_image.size(),
                0.0,
                0.0,
                cv::INTER_NEAREST);
            inference_mask_path =
                output_dir / "incremental_inference_mask.png";
            inference_overlay_path =
                output_dir / "incremental_inference_overlay.png";
            cv::imwrite(inference_mask_path.string(), inference_mask);
            cv::Mat overlay = source_image.clone();
            cv::Mat tint(
                source_image.size(),
                source_image.type(),
                cv::Scalar(0, 190, 0));
            tint.copyTo(overlay, inference_mask);
            cv::addWeighted(
                source_image, 0.55, overlay, 0.45, 0.0, overlay);
            cv::imwrite(inference_overlay_path.string(), overlay);
        }

        const std::filesystem::path model_manifest_path =
            model_package_dir / "model_manifest.json";
        {
            std::ofstream manifest_output(model_manifest_path);
            manifest_output
                << "{"
                << "\"schema\":\"cxvision.torch_model_manifest\","
                << "\"schema_version\":1,"
                << "\"model_id\":\"deeplab_incremental_"
                << deterministic_seed << "_class_" << target_class_id << "\","
                << "\"parent_model_id\":\""
                << (parent_weights.empty()
                        ? "random_initialization"
                        : "provided_cpp_state_dict")
                << "\","
                << "\"task\":\"segmentation\","
                << "\"architecture\":\"deeplabv3plus\","
                << "\"backbone\":\"mobilenet_v3_large\","
                << "\"weights\":\"weights/deeplab_incremental.pt\","
                << "\"weights_format\":\"cpp_state_dict\","
                << "\"num_classes\":" << runner_config.num_classes << ","
                << "\"target_geometry_class_id\":" << target_class_id << ","
                << "\"trial_state\":\"CANDIDATE\","
                << "\"model_name\":\"deeplab_incremental\","
                << "\"model_version\":\"incremental-1\","
                << "\"input\":{"
                << "\"width\":128,\"height\":128,\"color\":\"rgb\","
                << "\"scale\":0.003921568627,"
                << "\"mean\":[0.0,0.0,0.0],"
                << "\"std\":[1.0,1.0,1.0]},"
                << "\"postprocess\":{"
                << "\"target_class_id\":"
                << (business_incremental ? target_class_id + 1 : 1)
                << ",\"min_component_area\":20}"
                << "}\n";
        }

        const auto end = std::chrono::steady_clock::now();
        result.train_runtime_ms =
            static_cast<double>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    end - start).count());
        result.algorithm_runtime_ms = result.train_runtime_ms;
        result.actual_device =
            runner_config.device_policy == SegmentationDevicePolicy::ForceCUDA
                ? "cuda"
                : "cpu";
        result.ok = true;
        result.error_code = 0;
        result.status = "success";
        result.trainer_lifecycle_summary =
            "persistent model instance completed manifest-bound epoch training and evaluation";
        result.unified_mainline_summary =
            "incremental cpp_state_dict and model manifest exported";
        if (!inference_overlay_path.empty())
        {
            result.primary_visual_ref = inference_overlay_path.string();
            result.visualization_refs =
                inference_mask_path.string() + ";" +
                inference_overlay_path.string();
        }

        const std::filesystem::path result_path =
            output_dir.empty()
                ? std::filesystem::path()
                : (output_dir / "torch_training_lifecycle_result.json");
        const std::filesystem::path evidence_path =
            output_dir.empty()
                ? std::filesystem::path()
                : (output_dir / "torch_training_lifecycle_evidence.json");

        std::ostringstream result_json;
        result_json << "{";
        result_json << "\"schema\":\"cxvision.torch.training_lifecycle.v1\",";
        result_json << "\"task\":" << QuoteRuntimeTaskJsonString(request.task) << ",";
        result_json << "\"status\":\"success\",";
        result_json << "\"requested_device\":" << QuoteRuntimeTaskJsonString(result.requested_device) << ",";
        result_json << "\"actual_device\":" << QuoteRuntimeTaskJsonString(result.actual_device) << ",";
        result_json << "\"deterministic_seed\":" << deterministic_seed << ",";
        result_json << "\"dataset_source\":" << QuoteRuntimeTaskJsonString(lifecycle_dataset_source) << ",";
        result_json << "\"mask_source\":" << QuoteRuntimeTaskJsonString(lifecycle_mask_source) << ",";
        result_json << "\"annotation_count\":" << lifecycle_artifacts.total_annotations << ",";
        result_json << "\"image_count\":" << lifecycle_image_count << ",";
        result_json << "\"eval_image_count\":" << lifecycle_eval_image_count << ",";
        result_json << "\"train_split_images\":" << lifecycle_artifacts.split_summary.train_images << ",";
        result_json << "\"val_split_images\":" << lifecycle_artifacts.split_summary.val_images << ",";
        result_json << "\"test_split_images\":" << lifecycle_artifacts.split_summary.test_images << ",";
        result_json << "\"train_split_annotations\":" << lifecycle_artifacts.split_summary.train_annotations << ",";
        result_json << "\"val_split_annotations\":" << lifecycle_artifacts.split_summary.val_annotations << ",";
        result_json << "\"test_split_annotations\":" << lifecycle_artifacts.split_summary.test_annotations << ",";
        result_json << "\"image_hash\":" << QuoteRuntimeTaskJsonString(lifecycle_image_hash) << ",";
        result_json << "\"mask_foreground_ratio\":" << lifecycle_mask_foreground_ratio << ",";
        result_json << "\"split_summary_ref\":" << QuoteRuntimeTaskJsonString(lifecycle_artifacts.split_summary_ref) << ",";
        result_json << "\"mask_preview_ref\":" << QuoteRuntimeTaskJsonString(lifecycle_artifacts.mask_preview_ref) << ",";
        result_json << "\"bbox_overlay_ref\":" << QuoteRuntimeTaskJsonString(lifecycle_artifacts.bbox_overlay_ref) << ",";
        result_json << "\"train_runtime_ms\":" << result.train_runtime_ms << ",";
        result_json << "\"effective_epochs\":" << requested_epochs << ",";
        result_json << "\"effective_batch_size\":" << runner_config.batch_size << ",";
        result_json << "\"input_size\":" << runner_config.input_size << ",";
        result_json << "\"smoke_loss\":" << smoke.loss << ",";
        result_json << "\"grad_mean\":" << smoke.grad_mean << ",";
        result_json << "\"eval_loss\":" << eval_loss << ",";
        result_json << "\"foreground_iou\":" << foreground_iou << ",";
        result_json << "\"avg_confidence\":" << avg_confidence << ",";
        result_json << "\"model_weights_ref\":"
                    << QuoteRuntimeTaskJsonString(model_weights_path.string()) << ",";
        result_json << "\"model_manifest_ref\":"
                    << QuoteRuntimeTaskJsonString(model_manifest_path.string()) << ",";
        result_json << "\"mask_ref\":"
                    << QuoteRuntimeTaskJsonString(inference_mask_path.string()) << ",";
        result_json << "\"overlay_ref\":"
                    << QuoteRuntimeTaskJsonString(inference_overlay_path.string()) << ",";
        result_json << "\"trainer_lifecycle_summary\":"
                    << QuoteRuntimeTaskJsonString(result.trainer_lifecycle_summary) << ",";
        result_json << "\"unified_mainline_summary\":"
                    << QuoteRuntimeTaskJsonString(result.unified_mainline_summary);
        result_json << "}";
        result.result_json = result_json.str();

        if (!result_path.empty())
        {
            std::ofstream output(result_path);
            output << result.result_json << "\n";
            result.result_ref = result_path.string();
        }
        else
        {
            result.result_ref = "torch_train_segmentation_lifecycle_smoke.result";
        }

        if (!evidence_path.empty())
        {
            std::ofstream evidence(evidence_path);
            evidence << "{";
            evidence << "\"schema\":\"cxvision.torch.training_lifecycle_evidence.v1\",";
            evidence << "\"task\":" << QuoteRuntimeTaskJsonString(request.task) << ",";
            evidence << "\"training_stage\":\"persistent_incremental\",";
            evidence << "\"deterministic_seed\":" << deterministic_seed << ",";
            evidence << "\"dataset_source\":" << QuoteRuntimeTaskJsonString(lifecycle_dataset_source) << ",";
            evidence << "\"mask_source\":" << QuoteRuntimeTaskJsonString(lifecycle_mask_source) << ",";
            evidence << "\"annotation_count\":" << lifecycle_artifacts.total_annotations << ",";
            evidence << "\"image_count\":" << lifecycle_image_count << ",";
            evidence << "\"eval_image_count\":" << lifecycle_eval_image_count << ",";
            evidence << "\"train_split_images\":" << lifecycle_artifacts.split_summary.train_images << ",";
            evidence << "\"val_split_images\":" << lifecycle_artifacts.split_summary.val_images << ",";
            evidence << "\"test_split_images\":" << lifecycle_artifacts.split_summary.test_images << ",";
            evidence << "\"train_split_annotations\":" << lifecycle_artifacts.split_summary.train_annotations << ",";
            evidence << "\"val_split_annotations\":" << lifecycle_artifacts.split_summary.val_annotations << ",";
            evidence << "\"test_split_annotations\":" << lifecycle_artifacts.split_summary.test_annotations << ",";
            evidence << "\"image_hash\":" << QuoteRuntimeTaskJsonString(lifecycle_image_hash) << ",";
            evidence << "\"mask_foreground_ratio\":" << lifecycle_mask_foreground_ratio << ",";
            evidence << "\"split_summary_ref\":" << QuoteRuntimeTaskJsonString(lifecycle_artifacts.split_summary_ref) << ",";
            evidence << "\"mask_preview_ref\":" << QuoteRuntimeTaskJsonString(lifecycle_artifacts.mask_preview_ref) << ",";
            evidence << "\"bbox_overlay_ref\":" << QuoteRuntimeTaskJsonString(lifecycle_artifacts.bbox_overlay_ref) << ",";
            evidence << "\"epochs\":" << requested_epochs << ",";
            evidence << "\"batch_size\":" << runner_config.batch_size << ",";
            evidence << "\"input_size\":" << runner_config.input_size << ",";
            evidence << "\"finite_loss\":true,";
            evidence << "\"grad_mean\":" << smoke.grad_mean << ",";
            evidence << "\"eval_loss\":" << eval_loss << ",";
            evidence << "\"foreground_iou\":" << foreground_iou << ",";
            evidence << "\"avg_confidence\":" << avg_confidence << ",";
            evidence << "\"model_weights_ref\":"
                     << QuoteRuntimeTaskJsonString(model_weights_path.string())
                     << ",";
            evidence << "\"model_manifest_ref\":"
                     << QuoteRuntimeTaskJsonString(model_manifest_path.string())
                     << ",";
            evidence << "\"semantic_quality\":\"pending_human_review\"";
            evidence << "}\n";
            result.evidence_ref = evidence_path.string();
        }
        else
        {
            result.evidence_ref = "torch_train_segmentation_lifecycle_smoke.evidence";
        }

        if (!request.input_image.empty())
        {
            result.input_image_ref = request.input_image;
            if (result.primary_visual_ref.empty())
                result.primary_visual_ref = request.input_image;
        }
    }
    catch (const std::exception& e)
    {
        result.ok = false;
        result.error_code = -1;
        result.status = "exception";
        result.error_message = e.what();
        result.result_json =
            "{"
            "\"schema\":\"cxvision.torch.training_lifecycle.v1\","
            "\"status\":\"exception\","
            "\"reason\":" + QuoteRuntimeTaskJsonString(result.error_message) +
            "}";
    }
    catch (...)
    {
        result.ok = false;
        result.error_code = -2;
        result.status = "unknown_exception";
        result.error_message =
            "unknown exception in segmentation training lifecycle smoke";
    }

    return result;
}

} // namespace

TorchTaskResultCpp RunLegacyTorchTestHostTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    TorchTaskResultCpp result;

    try {
        const std::string& requested_device = !config.device.empty() ? config.device : "auto";

        TorchTestHost host;
        const auto report = host.run_task_report(request.task, requested_device);

        result.ok = (report.failures == 0);
        result.error_code = report.failures;
        result.requested_device = report.requested_device;
        result.actual_device = report.actual_device;

        if (report.failures == 0) {
            result.status = "success";
        } else {
            result.status = "failed";
            result.error_message = report.summary;
        }

        if (request.task.find("train") != std::string::npos) {
            result.train_runtime_ms = static_cast<double>(report.runtime_ms);
        } else if (request.task.find("infer") != std::string::npos) {
            result.infer_runtime_ms = static_cast<double>(report.runtime_ms);
            result.algorithm_runtime_ms = static_cast<double>(report.runtime_ms);
        } else {
            result.algorithm_runtime_ms = static_cast<double>(report.runtime_ms);
        }

        std::ostringstream json_os;
        json_os << "{";
        json_os << "\"task\":\"" << request.task << "\",";
        json_os << "\"status\":\"" << result.status << "\",";
        json_os << "\"failures\":" << report.failures << ",";
        json_os << "\"summary\":\"" << report.summary << "\",";
        json_os << "\"requested_device\":\"" << report.requested_device << "\",";
        json_os << "\"actual_device\":\"" << report.actual_device << "\",";
        json_os << "\"runtime_ms\":" << report.runtime_ms;
        json_os << "}";
        result.result_json = json_os.str();

        const auto* spec = TorchTestHost::find_task_spec(request.task);
        if (spec != nullptr) {
            result.result_ref = torch_make_handoff_ref(spec->task_id, "result");
            result.evidence_ref = torch_make_handoff_ref(spec->task_id, "evidence");
            result.attach_back_ref = spec->attach_back_result;

            if (torch_task_id_contains(spec->task_id, "yolo")) {
                result.bbox_candidate_list_ref = torch_make_handoff_ref(spec->task_id, "bbox_candidates");
            }
            if (torch_task_id_contains(spec->task_id, "mobilevit")) {
                result.roi_crop_packet_ref = torch_make_handoff_ref(spec->task_id, "roi_crops");
                result.template_alignment_ref = torch_make_handoff_ref(spec->task_id, "template_alignment");
                result.roi_diff_candidate_ref = torch_make_handoff_ref(spec->task_id, "roi_diff");
            }
            if (torch_task_id_contains(spec->task_id, "train")) {
                result.trainer_lifecycle_summary = "trainer_lifecycle_completed";
                result.unified_mainline_summary = "unified_mainline_bundle_available";
            }
        }

        if (!request.input_image.empty()) {
            result.input_image_ref = request.input_image;
            result.primary_visual_ref = request.input_image;
        }

        result.visualization_refs = "";

    } catch (const std::exception& e) {
        result.ok = false;
        result.error_code = -1;
        result.status = "exception";
        result.error_message = e.what();
    } catch (...) {
        result.ok = false;
        result.error_code = -2;
        result.status = "unknown_exception";
        result.error_message = "Unknown exception during legacy torch task execution";
    }

    return result;
}

TorchTaskResultCpp RunYoloV8TrainingLifecycleTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    TorchTaskResultCpp result;
    result.requested_device =
        request.device.empty() ? config.device : request.device;
    if (result.requested_device.empty())
        result.requested_device = "cpu";

    try
    {
        TORCH_CHECK(!request.dataset_root.empty(),
            "YOLOv8 lifecycle requires dataset_root");
        TORCH_CHECK(!request.output_dir.empty(),
            "YOLOv8 lifecycle requires output_dir");
        TORCH_CHECK(!request.extra_json.empty(),
            "YOLOv8 lifecycle requires a training plan in extra_json");

        cv::FileStorage plan(
            request.extra_json,
            cv::FileStorage::READ | cv::FileStorage::MEMORY |
                cv::FileStorage::FORMAT_JSON);
        TORCH_CHECK(plan.isOpened(), "YOLOv8 lifecycle training plan is invalid");
        std::string plan_schema;
        plan["schema"] >> plan_schema;
        TORCH_CHECK(
            plan_schema == "cxvision.yolov8n_cpp_training_plan.v1",
            "YOLOv8 lifecycle training plan schema is unsupported");

        int epochs = 0;
        int batch_size = 0;
        int input_size = 0;
        int max_train_batches = 0;
        int seed = 0;
        int num_classes = 0;
        double learning_rate = 0.0;
        int assignment_topk = 0;
        double classification_focal_gamma = -1.0;
        int use_assignment_quality_targets = -1;
        int validation_interval = 0;
        int early_stop_patience = 0;
        int minimum_complete_epochs = 0;
        double minimum_f1_delta = -1.0;
        int restore_best_validation_checkpoint = -1;
        double postprocess_confidence_threshold = -1.0;
        double postprocess_iou_threshold = -1.0;
        int postprocess_max_detections = 0;
        int postprocess_class_agnostic_nms = -1;
        double evaluation_match_iou_threshold = -1.0;
        int global_multiscale_assignment = -1;
        int geometry_roi_head_enabled = 0;
        int geometry_roi_hidden_channels = 64;
        int geometry_roi_pooled_size = 4;
        int geometry_roi_polygon_vertex_count = 8;
        double geometry_roi_parameter_weight = 1.0;
        double geometry_roi_contour_weight = 1.0;
        double geometry_roi_continuity_weight = 0.5;
        double geometry_roi_uncertainty_weight = 0.25;
        double geometry_roi_continuity_positive_weight = 1.0;
        double geometry_roi_calibration_weight = 0.1;
        std::string geometry_target_manifest;
        std::string affine_training_channel = "rotation";
        double rotation_train_positive_deg = 7.0, rotation_train_negative_deg = -8.0;
        double rotation_validation_deg = 14.0, rotation_holdout_deg = -18.0;
        double scale_train_down = 0.88, scale_train_up = 1.12;
        double scale_validation = 0.90, scale_holdout = 1.24;
        double compound_validation_scale = 0.90, compound_validation_rotation_deg = 11.0;
        double compound_holdout_scale = 1.16, compound_holdout_rotation_deg = -16.0;
        std::string parent_checkpoint;
        plan["epochs"] >> epochs;
        plan["batch_size"] >> batch_size;
        plan["input_size"] >> input_size;
        plan["max_train_batches_per_epoch"] >> max_train_batches;
        plan["seed"] >> seed;
        plan["num_classes"] >> num_classes;
        plan["learning_rate"] >> learning_rate;
        plan["parent_checkpoint"] >> parent_checkpoint;
        auto read_required_int = [&](const char* key, int& value) {
            const cv::FileNode node = plan[key];
            TORCH_CHECK(!node.empty(), "YOLOv8 lifecycle training plan is missing required field: ", key);
            node >> value;
        };
        auto read_required_double = [&](const char* key, double& value) {
            const cv::FileNode node = plan[key];
            TORCH_CHECK(!node.empty(), "YOLOv8 lifecycle training plan is missing required field: ", key);
            node >> value;
        };
        read_required_int("assignment_topk", assignment_topk);
        read_required_double("classification_focal_gamma", classification_focal_gamma);
        read_required_int("use_assignment_quality_targets", use_assignment_quality_targets);
        read_required_int("validation_interval", validation_interval);
        read_required_int("early_stop_patience", early_stop_patience);
        read_required_int("minimum_complete_epochs", minimum_complete_epochs);
        read_required_double("minimum_f1_delta", minimum_f1_delta);
        read_required_int("restore_best_validation_checkpoint", restore_best_validation_checkpoint);
        read_required_double("postprocess_confidence_threshold", postprocess_confidence_threshold);
        read_required_double("postprocess_iou_threshold", postprocess_iou_threshold);
        read_required_int("postprocess_max_detections", postprocess_max_detections);
        read_required_int("postprocess_class_agnostic_nms", postprocess_class_agnostic_nms);
        read_required_double("evaluation_match_iou_threshold", evaluation_match_iou_threshold);
        read_required_int("global_multiscale_assignment", global_multiscale_assignment);
        auto read_optional_int = [&](const char* key, int& value) {
            const cv::FileNode node = plan[key];
            if (!node.empty()) node >> value;
        };
        auto read_optional_double = [&](const char* key, double& value) {
            const cv::FileNode node = plan[key];
            if (!node.empty()) node >> value;
        };
        auto read_optional_string = [&](const char* key, std::string& value) {
            const cv::FileNode node = plan[key];
            if (!node.empty()) node >> value;
        };
        read_optional_int("geometry_roi_head_enabled", geometry_roi_head_enabled);
        read_optional_int("geometry_roi_hidden_channels", geometry_roi_hidden_channels);
        read_optional_int("geometry_roi_pooled_size", geometry_roi_pooled_size);
        read_optional_int("geometry_roi_polygon_vertex_count", geometry_roi_polygon_vertex_count);
        read_optional_double("geometry_roi_parameter_weight", geometry_roi_parameter_weight);
        read_optional_double("geometry_roi_contour_weight", geometry_roi_contour_weight);
        read_optional_double("geometry_roi_continuity_weight", geometry_roi_continuity_weight);
        read_optional_double("geometry_roi_uncertainty_weight", geometry_roi_uncertainty_weight);
        read_optional_double("geometry_roi_continuity_positive_weight", geometry_roi_continuity_positive_weight);
        read_optional_double("geometry_roi_calibration_weight", geometry_roi_calibration_weight);
        read_optional_string("affine_training_channel", affine_training_channel);
        read_optional_double("rotation_train_positive_deg", rotation_train_positive_deg);
        read_optional_double("rotation_train_negative_deg", rotation_train_negative_deg);
        read_optional_double("rotation_validation_deg", rotation_validation_deg);
        read_optional_double("rotation_holdout_deg", rotation_holdout_deg);
        read_optional_double("scale_train_down", scale_train_down);
        read_optional_double("scale_train_up", scale_train_up);
        read_optional_double("scale_validation", scale_validation);
        read_optional_double("scale_holdout", scale_holdout);
        read_optional_double("compound_validation_scale", compound_validation_scale);
        read_optional_double("compound_validation_rotation_deg", compound_validation_rotation_deg);
        read_optional_double("compound_holdout_scale", compound_holdout_scale);
        read_optional_double("compound_holdout_rotation_deg", compound_holdout_rotation_deg);
        if (!plan["geometry_target_manifest"].empty())
            plan["geometry_target_manifest"] >> geometry_target_manifest;
        TORCH_CHECK(epochs >= 2, "YOLOv8 lifecycle requires at least two epochs");
        TORCH_CHECK(batch_size > 0, "YOLOv8 lifecycle batch_size must be positive");
        TORCH_CHECK(input_size > 0 && input_size % 32 == 0,
            "YOLOv8 lifecycle input_size must be positive and divisible by 32");
        TORCH_CHECK(max_train_batches >= 0,
            "YOLOv8 lifecycle max_train_batches_per_epoch must be non-negative; zero means full dataset");
        TORCH_CHECK(num_classes > 0, "YOLOv8 lifecycle num_classes must be positive");
        TORCH_CHECK(learning_rate > 0.0,
            "YOLOv8 lifecycle learning_rate must be positive");
        TORCH_CHECK(assignment_topk > 0,
            "YOLOv8 lifecycle assignment_topk must be positive");
        TORCH_CHECK(classification_focal_gamma >= 0.0,
            "YOLOv8 lifecycle classification_focal_gamma must be non-negative");
        TORCH_CHECK(validation_interval > 0,
            "YOLOv8 lifecycle validation_interval must be positive");
        TORCH_CHECK(early_stop_patience > 0,
            "YOLOv8 lifecycle early_stop_patience must be positive");
        TORCH_CHECK(minimum_complete_epochs >= 2 && minimum_complete_epochs <= epochs,
            "YOLOv8 lifecycle minimum_complete_epochs must be in [2, epochs]");
        TORCH_CHECK(minimum_f1_delta >= 0.0,
            "YOLOv8 lifecycle minimum_f1_delta must be non-negative");
        TORCH_CHECK(postprocess_confidence_threshold >= 0.0 && postprocess_confidence_threshold <= 1.0,
            "YOLOv8 lifecycle postprocess_confidence_threshold must be in [0,1]");
        TORCH_CHECK(postprocess_iou_threshold >= 0.0 && postprocess_iou_threshold <= 1.0,
            "YOLOv8 lifecycle postprocess_iou_threshold must be in [0,1]");
        TORCH_CHECK(postprocess_max_detections > 0,
            "YOLOv8 lifecycle postprocess_max_detections must be positive");
        TORCH_CHECK(use_assignment_quality_targets == 0 || use_assignment_quality_targets == 1,
            "YOLOv8 lifecycle use_assignment_quality_targets must be 0 or 1");
        TORCH_CHECK(postprocess_class_agnostic_nms == 0 || postprocess_class_agnostic_nms == 1,
            "YOLOv8 lifecycle postprocess_class_agnostic_nms must be 0 or 1");
        TORCH_CHECK(evaluation_match_iou_threshold >= 0.0 && evaluation_match_iou_threshold <= 1.0,
            "YOLOv8 lifecycle evaluation_match_iou_threshold must be in [0,1]");
        TORCH_CHECK(global_multiscale_assignment == 0 || global_multiscale_assignment == 1,
            "YOLOv8 lifecycle global_multiscale_assignment must be 0 or 1");
        TORCH_CHECK(geometry_roi_head_enabled == 0 || geometry_roi_head_enabled == 1,
            "YOLOv8 lifecycle geometry_roi_head_enabled must be 0 or 1");
        TORCH_CHECK(geometry_roi_hidden_channels > 0,
            "YOLOv8 lifecycle geometry_roi_hidden_channels must be positive");
        TORCH_CHECK(geometry_roi_pooled_size >= 2,
            "YOLOv8 lifecycle geometry_roi_pooled_size must be at least two");
        TORCH_CHECK(geometry_roi_polygon_vertex_count >= 3,
            "YOLOv8 lifecycle geometry_roi_polygon_vertex_count must be at least three");
        TORCH_CHECK(geometry_roi_parameter_weight >= 0.0 && geometry_roi_contour_weight >= 0.0 &&
                geometry_roi_continuity_weight >= 0.0 && geometry_roi_uncertainty_weight >= 0.0 &&
                geometry_roi_calibration_weight >= 0.0,
            "YOLOv8 lifecycle geometry ROI loss weights must be non-negative");
        TORCH_CHECK(geometry_roi_continuity_positive_weight > 0.0,
            "YOLOv8 lifecycle geometry_roi_continuity_positive_weight must be positive");
        TORCH_CHECK(geometry_roi_head_enabled == 0 || !geometry_target_manifest.empty(),
            "YOLOv8 lifecycle geometry ROI head requires geometry_target_manifest; bbox-only labels are insufficient");
        TORCH_CHECK(affine_training_channel == "rotation" || affine_training_channel == "scale",
            "YOLOv8 lifecycle affine_training_channel must be rotation or scale; compound is evaluation-only");
        TORCH_CHECK(std::abs(rotation_train_positive_deg) <= 45.0 && std::abs(rotation_train_negative_deg) <= 45.0 &&
                    std::abs(rotation_validation_deg) <= 45.0 && std::abs(rotation_holdout_deg) <= 45.0 &&
                    std::abs(compound_validation_rotation_deg) <= 45.0 && std::abs(compound_holdout_rotation_deg) <= 45.0,
            "YOLOv8 lifecycle affine rotation values must be within [-45,45]");
        TORCH_CHECK(scale_train_down >= 0.70 && scale_train_down <= 1.30 && scale_train_up >= 0.70 && scale_train_up <= 1.30 &&
                    scale_validation >= 0.70 && scale_validation <= 1.30 && scale_holdout >= 0.70 && scale_holdout <= 1.30 &&
                    compound_validation_scale >= 0.70 && compound_validation_scale <= 1.30 &&
                    compound_holdout_scale >= 0.70 && compound_holdout_scale <= 1.30,
            "YOLOv8 lifecycle affine scale values must be within [0.70,1.30]");
        TORCH_CHECK(restore_best_validation_checkpoint != 0,
            "YOLOv8 lifecycle requires validation-best checkpoint restoration");
        TORCH_CHECK(!parent_checkpoint.empty(),
            "YOLOv8 lifecycle requires an explicit parent_checkpoint; random initialization is not an incremental branch");
        const std::filesystem::path parent_checkpoint_path(parent_checkpoint);
        TORCH_CHECK(std::filesystem::is_regular_file(parent_checkpoint_path),
            "YOLOv8 lifecycle parent_checkpoint is missing or not a regular file");

        std::vector<std::string> frozen_parameter_prefixes;
        const cv::FileNode freeze_nodes = plan["frozen_parameter_prefixes"];
        TORCH_CHECK(freeze_nodes.isSeq(),
            "YOLOv8 lifecycle frozen_parameter_prefixes must be a sequence");
        for (const auto& node : freeze_nodes)
            frozen_parameter_prefixes.push_back(static_cast<std::string>(node));
        TORCH_CHECK(!frozen_parameter_prefixes.empty(),
            "YOLOv8 lifecycle requires at least one frozen parameter prefix");
        const auto matches_frozen_prefix = [&](const std::string& name) {
            for (const std::string& prefix : frozen_parameter_prefixes) {
                if (name == prefix || name.rfind(prefix + ".", 0) == 0)
                    return true;
            }
            return false;
        };

        std::vector<std::string> class_names;
        const cv::FileNode class_nodes = plan["class_names"];
        TORCH_CHECK(class_nodes.isSeq(),
            "YOLOv8 lifecycle class_names must be a sequence");
        for (const auto& node : class_nodes)
            class_names.push_back(static_cast<std::string>(node));
        TORCH_CHECK(static_cast<int>(class_names.size()) == num_classes,
            "YOLOv8 lifecycle class_names count must match num_classes");
        std::vector<float> class_loss_weights;
        const cv::FileNode class_weight_nodes = plan["class_loss_weights"];
        TORCH_CHECK(class_weight_nodes.isSeq(),
            "YOLOv8 lifecycle class_loss_weights must be a sequence");
        for (const auto& node : class_weight_nodes) {
            double value = 0.0;
            node >> value;
            TORCH_CHECK(value > 0.0 && std::isfinite(value),
                "YOLOv8 lifecycle class_loss_weights must be finite and positive");
            class_loss_weights.push_back(static_cast<float>(value));
        }
        TORCH_CHECK(static_cast<int>(class_loss_weights.size()) == num_classes,
            "YOLOv8 lifecycle class_loss_weights count must match num_classes");

        const std::filesystem::path dataset_root(request.dataset_root);
        const std::filesystem::path train_images = dataset_root / "images" / "train";
        const std::filesystem::path train_labels = dataset_root / "labels" / "train";
        const std::filesystem::path val_images = dataset_root / "images" / "val";
        const std::filesystem::path val_labels = dataset_root / "labels" / "val";
        TORCH_CHECK(std::filesystem::is_directory(train_images),
            "YOLOv8 lifecycle train images directory is missing");
        TORCH_CHECK(std::filesystem::is_directory(train_labels),
            "YOLOv8 lifecycle train labels directory is missing");
        TORCH_CHECK(std::filesystem::is_directory(val_images),
            "YOLOv8 lifecycle validation images directory is missing");
        TORCH_CHECK(std::filesystem::is_directory(val_labels),
            "YOLOv8 lifecycle validation labels directory is missing");

        std::size_t train_image_count = 0;
        std::size_t val_image_count = 0;
        for (const auto& entry : std::filesystem::directory_iterator(train_images))
            train_image_count += entry.is_regular_file() ? 1U : 0U;
        for (const auto& entry : std::filesystem::directory_iterator(val_images))
            val_image_count += entry.is_regular_file() ? 1U : 0U;
        TORCH_CHECK(train_image_count > 0 && val_image_count > 0,
            "YOLOv8 lifecycle dataset splits must be non-empty");

        std::filesystem::path geometry_train_target_dir;
        std::filesystem::path geometry_validation_target_dir;
        std::filesystem::path geometry_holdout_target_dir;
        if (geometry_roi_head_enabled != 0) {
            const std::filesystem::path manifest_path =
                std::filesystem::absolute(std::filesystem::path(geometry_target_manifest));
            TORCH_CHECK(std::filesystem::is_regular_file(manifest_path),
                "geometry_target_manifest is missing or not a regular file");
            cv::FileStorage geometry_manifest(manifest_path.string(),
                cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);
            TORCH_CHECK(geometry_manifest.isOpened(),
                "geometry_target_manifest cannot be parsed");
            std::string geometry_schema;
            std::string train_target_dir_text;
            std::string validation_target_dir_text;
            std::string holdout_target_dir_text;
            geometry_manifest["schema"] >> geometry_schema;
            geometry_manifest["train_target_dir"] >> train_target_dir_text;
            geometry_manifest["validation_target_dir"] >> validation_target_dir_text;
            geometry_manifest["holdout_target_dir"] >> holdout_target_dir_text;
            TORCH_CHECK(geometry_schema == "cxvision.geometry_roi_target_manifest.v1",
                "geometry_target_manifest schema is unsupported");
            TORCH_CHECK(!train_target_dir_text.empty() && !validation_target_dir_text.empty() &&
                    !holdout_target_dir_text.empty(),
                "geometry_target_manifest must declare train_target_dir, validation_target_dir and holdout_target_dir");
            const auto resolve_geometry_dir = [&](const std::string& text) {
                const std::filesystem::path path(text);
                return path.is_absolute() ? path : manifest_path.parent_path() / path;
            };
            geometry_train_target_dir = resolve_geometry_dir(train_target_dir_text);
            geometry_validation_target_dir = resolve_geometry_dir(validation_target_dir_text);
            geometry_holdout_target_dir = resolve_geometry_dir(holdout_target_dir_text);
            TORCH_CHECK(std::filesystem::is_directory(geometry_train_target_dir) &&
                    std::filesystem::is_directory(geometry_validation_target_dir) &&
                    std::filesystem::is_directory(geometry_holdout_target_dir),
                "geometry target sidecar directories are missing");
        }

        const bool use_cuda =
            result.requested_device == "cuda" && torch::cuda::is_available();
        const torch::Device device(use_cuda ? torch::kCUDA : torch::kCPU);
        result.actual_device = use_cuda ? "cuda" : "cpu";
        torch::manual_seed(seed);

        ModelConfig model_config = ModelConfig::get_config("nano", num_classes);
        YoloModelBuildConfig build_config;
        build_config.loss.topk = assignment_topk;
        build_config.loss.fl_gamma = static_cast<float>(classification_focal_gamma);
        build_config.loss.use_assignment_quality_targets =
            use_assignment_quality_targets != 0;
        build_config.loss.global_multiscale_assignment =
            global_multiscale_assignment != 0;
        build_config.loss.class_loss_weights = class_loss_weights;
        build_config.geometry_head.enabled = geometry_roi_head_enabled != 0;
        build_config.geometry_head.hidden_channels = geometry_roi_hidden_channels;
        build_config.geometry_head.pooled_size = geometry_roi_pooled_size;
        build_config.geometry_head.polygon_vertex_count = geometry_roi_polygon_vertex_count;
        build_config.geometry_loss.parameter_weight = static_cast<float>(geometry_roi_parameter_weight);
        build_config.geometry_loss.contour_weight = static_cast<float>(geometry_roi_contour_weight);
        build_config.geometry_loss.continuity_weight = static_cast<float>(geometry_roi_continuity_weight);
        build_config.geometry_loss.uncertainty_weight = static_cast<float>(geometry_roi_uncertainty_weight);
        build_config.geometry_loss.continuity_positive_weight =
            static_cast<float>(geometry_roi_continuity_positive_weight);
        build_config.geometry_loss.calibration_weight =
            static_cast<float>(geometry_roi_calibration_weight);
        YOLOv8 model(model_config, build_config);
        model->to(device);
        if (geometry_roi_head_enabled == 0) {
            model->load_checkpoint(parent_checkpoint_path.string());
            // InputArchive restores the device stored in the parent archive.
            // Re-apply the run device after loading so a CUDA parent can be
            // incrementally trained in a CPU-controlled lifecycle.
            model->to(device);
        } else {
            // Parent checkpoints predate the geometry branch.  Load the mature
            // detector into its matching architecture, then copy matching
            // detector tensors; the geometry head remains explicitly new.
            YoloModelBuildConfig detector_only_config = build_config;
            detector_only_config.geometry_head.enabled = false;
            YOLOv8 parent_model(model_config, detector_only_config);
            parent_model->to(device);
            parent_model->load_checkpoint(parent_checkpoint_path.string());
            parent_model->to(device);
            torch::NoGradGuard no_grad;
            const auto parent_parameters = parent_model->named_parameters(true);
            const auto parent_buffers = parent_model->named_buffers(true);
            for (const auto& named : model->named_parameters(true)) {
                if (const auto parent = parent_parameters.find(named.key()))
                    named.value().copy_(*parent);
            }
            for (const auto& named : model->named_buffers(true)) {
                if (const auto parent = parent_buffers.find(named.key()))
                    named.value().copy_(*parent);
            }
            model->to(device);
        }

        std::vector<torch::Tensor> trainable_parameters;
        std::vector<std::string> frozen_parameter_names;
        std::vector<std::string> trainable_parameter_names;
        for (const auto& named : model->named_parameters(true)) {
            const bool frozen = matches_frozen_prefix(named.key());
            named.value().set_requires_grad(!frozen);
            if (frozen)
                frozen_parameter_names.push_back(named.key());
            else {
                trainable_parameter_names.push_back(named.key());
                trainable_parameters.push_back(named.value());
            }
        }
        TORCH_CHECK(!frozen_parameter_names.empty(),
            "YOLOv8 lifecycle freeze policy did not match any parent parameter");
        TORCH_CHECK(!trainable_parameters.empty(),
            "YOLOv8 lifecycle freeze policy left no trainable parameters");

        YoloEvalConfig eval_config;
        eval_config.data.batch_size = batch_size;
        eval_config.data.img_size = input_size;
        eval_config.data.max_gt = 50;
        eval_config.data.resize_policy = YoloResizePolicy::PlainResize;
        eval_config.data.dataloader_workers = 0;
        eval_config.postprocess.num_classes = num_classes;
        eval_config.postprocess.conf_threshold =
            static_cast<float>(postprocess_confidence_threshold);
        eval_config.postprocess.iou_threshold =
            static_cast<float>(postprocess_iou_threshold);
        eval_config.postprocess.max_detections = postprocess_max_detections;
        eval_config.postprocess.class_agnostic_nms =
            postprocess_class_agnostic_nms != 0;
        eval_config.match_iou_threshold =
            static_cast<float>(evaluation_match_iou_threshold);

        const std::filesystem::path output_dir(request.output_dir);
        const std::filesystem::path weights_dir = output_dir / "weights";
        std::filesystem::create_directories(weights_dir);
        const std::filesystem::path base_weights = weights_dir / "base_cpp_yolov8n.pt";
        const std::filesystem::path incremental_weights =
            weights_dir / "incremental_cpp_yolov8n.pt";
        const std::filesystem::path final_epoch_weights =
            weights_dir / "final_epoch_cpp_yolov8n.pt";
        const std::filesystem::path best_validation_weights =
            weights_dir / "best_validation_cpp_yolov8n.pt";

        std::filesystem::copy_file(parent_checkpoint_path, base_weights,
            std::filesystem::copy_options::none);
        std::map<std::string, torch::Tensor> before_parameters;
        for (const auto& named : model->named_parameters(true))
            before_parameters.emplace(named.key(), named.value().detach().clone());
        std::map<std::string, torch::Tensor> before_frozen_buffers;
        for (const auto& named : model->named_buffers(true)) {
            if (matches_frozen_prefix(named.key()))
                before_frozen_buffers.emplace(named.key(), named.value().detach().clone());
        }

        const auto start = std::chrono::steady_clock::now();
        const YOLOv8Impl::ValidationSummary base_summary =
            model->val_summary(dataset_root.string(), eval_config);

        YoloDatasetConfig train_dataset_config;
        train_dataset_config.img_size = input_size;
        train_dataset_config.is_train = true;
        train_dataset_config.max_gt = 50;
        train_dataset_config.enable_hsv = false;
        train_dataset_config.enable_flip = false;
        train_dataset_config.resize_policy = YoloResizePolicy::PlainResize;
        train_dataset_config.geometry_targets_enabled = geometry_roi_head_enabled != 0;
        train_dataset_config.geometry_polygon_vertex_count = geometry_roi_polygon_vertex_count;
        train_dataset_config.geometry_target_dir = geometry_train_target_dir.string();
        auto train_dataset =
            YoloDataset(make_yolo_split_paths(dataset_root.string(), "train"),
                        train_dataset_config)
                .map(torch::data::transforms::Stack<>());
        auto train_loader = torch::data::make_data_loader(
            std::move(train_dataset),
            make_yolo_loader_options(batch_size, 0));
        torch::optim::SGD optimizer(
            trainable_parameters,
            torch::optim::SGDOptions(learning_rate)
                .momentum(0.9)
                .weight_decay(0.0005));

        std::vector<double> epoch_losses;
        std::vector<double> epoch_box_losses;
        std::vector<double> epoch_classification_losses;
        std::vector<double> epoch_distribution_focal_losses;
        std::vector<double> epoch_geometry_parameter_losses;
        std::vector<double> epoch_geometry_contour_losses;
        std::vector<double> epoch_geometry_topology_losses;
        std::vector<double> epoch_geometry_uncertainty_losses;
        std::vector<double> epoch_geometry_calibration_losses;
        std::vector<int> epoch_batches;
        const std::filesystem::path curve_csv = output_dir / "learning_curve.csv";
        std::ofstream curve_output(curve_csv);
        TORCH_CHECK(curve_output.good(),
            "YOLOv8 lifecycle learning curve cannot be created");
        curve_output << "epoch,total_loss,box_loss,classification_loss,dfl_loss,"
                        "geometry_parameter_loss,geometry_contour_loss,"
                        "geometry_topology_loss,geometry_uncertainty_loss,geometry_calibration_loss,batches\n";
        curve_output.flush();
        struct EpochValidationRecord {
            int epoch = 0;
            YOLOv8Impl::ValidationSummary summary;
            bool selected = false;
        };
        std::vector<EpochValidationRecord> validation_history;
        double best_validation_f1 = -1.0;
        int best_validation_epoch = 0;
        int validation_checks_without_improvement = 0;
        bool early_stopped = false;
        int completed_epochs = 0;
        double last_grad_mean = 0.0;
        for (int epoch = 0; epoch < epochs; ++epoch)
        {
            static_cast<torch::nn::Module&>(*model).train(true);
            // requires_grad=false does not stop BatchNorm running-stat updates.
            // A frozen module must stay in eval mode so a geometry-only phase
            // cannot silently alter the mature detector through buffers.
            for (const auto& named : model->named_modules()) {
                if (matches_frozen_prefix(named.key()))
                    named.value()->eval();
            }
            double loss_sum = 0.0;
            double box_loss_sum = 0.0;
            double classification_loss_sum = 0.0;
            double dfl_loss_sum = 0.0;
            double geometry_parameter_loss_sum = 0.0;
            double geometry_contour_loss_sum = 0.0;
            double geometry_topology_loss_sum = 0.0;
            double geometry_uncertainty_loss_sum = 0.0;
            double geometry_calibration_loss_sum = 0.0;
            int batch_count = 0;
            for (auto& batch : *train_loader)
            {
                torch::Tensor images = batch.data.to(device);
                torch::Tensor targets = batch.target.to(device);
                for (int64_t b = 0; b < targets.size(0); ++b)
                    targets[b].select(1, 0).fill_(static_cast<float>(b));
                optimizer.zero_grad();
                auto step = geometry_roi_head_enabled != 0
                    ? model->train_step_with_geometry(images, targets,
                        geometry_roi_targets_from_batch(targets,
                            geometry_roi_polygon_vertex_count))
                    : model->train_step(images, targets);
                torch::Tensor loss = std::get<0>(step);
                const auto& loss_items = std::get<1>(step);
                auto metric_value = [&](const char* key) {
                    const auto found = loss_items.find(key);
                    return found == loss_items.end() ? 0.0 : static_cast<double>(found->second);
                };
                TORCH_CHECK(torch::isfinite(loss).item<bool>(),
                    "YOLOv8 lifecycle training loss is not finite");
                loss.backward();
                bool found_grad = false;
                for (const auto& parameter : trainable_parameters)
                {
                    if (parameter.grad().defined())
                    {
                        last_grad_mean =
                            parameter.grad().abs().mean().item<double>();
                        found_grad = true;
                        break;
                    }
                }
                TORCH_CHECK(found_grad,
                    "YOLOv8 lifecycle training produced no gradients");
                optimizer.step();
                loss_sum += loss.item<double>();
                box_loss_sum += metric_value("box_loss");
                classification_loss_sum += metric_value("cls_loss");
                dfl_loss_sum += metric_value("dfl_loss");
                geometry_parameter_loss_sum += metric_value("geometry_roi_parameter_loss");
                geometry_contour_loss_sum += metric_value("geometry_roi_contour_loss");
                geometry_topology_loss_sum += metric_value("geometry_roi_topology_loss");
                geometry_uncertainty_loss_sum += metric_value("geometry_roi_uncertainty_loss");
                geometry_calibration_loss_sum += metric_value("geometry_roi_calibration_loss");
                ++batch_count;
                if (max_train_batches > 0 && batch_count >= max_train_batches)
                    break;
            }
            TORCH_CHECK(batch_count > 0,
                "YOLOv8 lifecycle epoch produced no training batches");
            epoch_losses.push_back(loss_sum / batch_count);
            epoch_box_losses.push_back(box_loss_sum / batch_count);
            epoch_classification_losses.push_back(classification_loss_sum / batch_count);
            epoch_distribution_focal_losses.push_back(dfl_loss_sum / batch_count);
            epoch_geometry_parameter_losses.push_back(geometry_parameter_loss_sum / batch_count);
            epoch_geometry_contour_losses.push_back(geometry_contour_loss_sum / batch_count);
            epoch_geometry_topology_losses.push_back(geometry_topology_loss_sum / batch_count);
            epoch_geometry_uncertainty_losses.push_back(geometry_uncertainty_loss_sum / batch_count);
            epoch_geometry_calibration_losses.push_back(geometry_calibration_loss_sum / batch_count);
            epoch_batches.push_back(batch_count);
            curve_output << (epoch + 1) << "," << epoch_losses.back() << ","
                         << epoch_box_losses.back() << ","
                         << epoch_classification_losses.back() << ","
                         << epoch_distribution_focal_losses.back() << ","
                         << epoch_geometry_parameter_losses.back() << ","
                         << epoch_geometry_contour_losses.back() << ","
                         << epoch_geometry_topology_losses.back() << ","
                         << epoch_geometry_uncertainty_losses.back() << ","
                         << epoch_geometry_calibration_losses.back() << ","
                         << batch_count << "\n";
            curve_output.flush();
            completed_epochs = epoch + 1;

            const bool validation_due =
                completed_epochs % validation_interval == 0 || completed_epochs == epochs;
            if (validation_due) {
                const YOLOv8Impl::ValidationSummary epoch_summary =
                    model->val_summary(dataset_root.string(), eval_config);
                const bool improved = best_validation_epoch == 0 ||
                    epoch_summary.f1 > best_validation_f1 + minimum_f1_delta;
                validation_history.push_back(
                    EpochValidationRecord{completed_epochs, epoch_summary, improved});
                if (improved) {
                    best_validation_f1 = epoch_summary.f1;
                    best_validation_epoch = completed_epochs;
                    validation_checks_without_improvement = 0;
                    torch::serialize::OutputArchive best_archive;
                    model->save(best_archive);
                    best_archive.save_to(best_validation_weights.string());
                } else {
                    ++validation_checks_without_improvement;
                    if (completed_epochs >= minimum_complete_epochs &&
                        validation_checks_without_improvement >= early_stop_patience) {
                        early_stopped = true;
                        break;
                    }
                }
            }
        }
        curve_output.close();

        torch::serialize::OutputArchive final_epoch_archive;
        model->save(final_epoch_archive);
        final_epoch_archive.save_to(final_epoch_weights.string());
        TORCH_CHECK(best_validation_epoch > 0 &&
            std::filesystem::is_regular_file(best_validation_weights),
            "YOLOv8 lifecycle did not produce a validation-best checkpoint");
        model->load_checkpoint(best_validation_weights.string());
        model->to(device);

        const std::filesystem::path geometry_validation_report_path =
            output_dir / "geometry_roi_fixed_validation.json";
        const std::filesystem::path geometry_holdout_report_path =
            output_dir / "geometry_roi_holdout.json";
        if (geometry_roi_head_enabled != 0) {
            auto evaluate_geometry_split = [&](const std::string& split,
                                               const std::filesystem::path& sidecar_dir,
                                               const std::filesystem::path& report_path) {
                YoloDatasetConfig geometry_config;
                geometry_config.img_size = input_size;
                geometry_config.is_train = false;
                geometry_config.max_gt = 50;
                geometry_config.enable_hsv = false;
                geometry_config.enable_flip = false;
                geometry_config.resize_policy = YoloResizePolicy::PlainResize;
                geometry_config.geometry_targets_enabled = true;
                geometry_config.geometry_polygon_vertex_count = geometry_roi_polygon_vertex_count;
                geometry_config.geometry_target_dir = sidecar_dir.string();
                const auto summary = model->geometry_roi_val_summary(
                    dataset_root.string(), split, geometry_config);
                std::ostringstream channel_metrics_json;
                channel_metrics_json << "{";
                bool first_channel = true;
                for (const auto& channel : summary.per_affine_channel) {
                    const char* channel_name = "unclassified";
                    switch (channel.first) {
                    case 1: channel_name = "rotation"; break;
                    case 2: channel_name = "scale"; break;
                    case 3: channel_name = "compound"; break;
                    default: break;
                    }
                    const auto& metrics = channel.second;
                    if (!first_channel) channel_metrics_json << ",";
                    first_channel = false;
                    channel_metrics_json << "\n    " << QuoteRuntimeTaskJsonString(channel_name) << ": {"
                        << "\"channel_id\": " << channel.first
                        << ", \"instance_count\": " << metrics.instance_count
                        << ", \"ellipse_count\": " << metrics.ellipse_count
                        << ", \"polygon_count\": " << metrics.polygon_count
                        << ", \"ellipse_parameter_mae_px\": " << metrics.ellipse_parameter_mae_px
                        << ", \"polygon_vertex_mae_px\": " << metrics.polygon_vertex_mae_px
                        << ", \"continuity_brier\": " << metrics.continuity_brier
                        << ", \"continuity_accuracy\": " << metrics.continuity_accuracy
                        << ", \"ellipse_predictive_variance\": " << metrics.ellipse_predictive_variance
                        << ", \"polygon_predictive_variance\": " << metrics.polygon_predictive_variance
                        << ", \"latency_scope\": \"SHARED_SPLIT_FORWARD_NOT_ADDITIVE\"}";
                }
                channel_metrics_json << "\n  }";
                std::ofstream output(report_path);
                output << "{\n"
                    << "  \"schema\": \"cxvision.geometry_roi_fixed_evaluation.v1\",\n"
                    << "  \"split\": " << QuoteRuntimeTaskJsonString(split) << ",\n"
                    << "  \"status\": \"TEACHER_ROI_GEOMETRY_EVALUATION_COMPLETE\",\n"
                    << "  \"roi_source\": " << QuoteRuntimeTaskJsonString(summary.roi_source) << ",\n"
                    << "  \"claim_status\": " << QuoteRuntimeTaskJsonString(summary.claim_status) << ",\n"
                    << "  \"instance_count\": " << summary.instance_count << ",\n"
                    << "  \"ellipse_count\": " << summary.ellipse_count << ",\n"
                    << "  \"polygon_count\": " << summary.polygon_count << ",\n"
                    << "  \"ellipse_parameter_mae_px\": " << summary.ellipse_parameter_mae_px << ",\n"
                    << "  \"polygon_vertex_mae_px\": " << summary.polygon_vertex_mae_px << ",\n"
                    << "  \"continuity_brier\": " << summary.continuity_brier << ",\n"
                    << "  \"continuity_accuracy\": " << summary.continuity_accuracy << ",\n"
                    << "  \"ellipse_predictive_variance\": " << summary.ellipse_predictive_variance << ",\n"
                    << "  \"polygon_predictive_variance\": " << summary.polygon_predictive_variance << ",\n"
                    << "  \"inference_ms\": " << summary.inference_ms << ",\n"
                    << "  \"affine_channel_metrics\": " << channel_metrics_json.str() << ",\n"
                    << "  \"promotion_allowed\": false\n}\n";
            };
            evaluate_geometry_split("val", geometry_validation_target_dir,
                geometry_validation_report_path);
            evaluate_geometry_split("holdout", geometry_holdout_target_dir,
                geometry_holdout_report_path);
        }

        torch::serialize::OutputArchive incremental_archive;
        model->save(incremental_archive);
        incremental_archive.save_to(incremental_weights.string());
        const std::filesystem::path checkpoint_selection_path =
            output_dir / "checkpoint_selection_report.json";
        {
            std::ofstream selection(checkpoint_selection_path);
            selection << "{\n"
                << "  \"schema\": \"cxvision.yolov8_checkpoint_selection.v1\",\n"
                << "  \"status\": \"VALIDATION_BEST_CHECKPOINT_SELECTED\",\n"
                << "  \"requested_epochs\": " << epochs << ",\n"
                << "  \"completed_epochs\": " << completed_epochs << ",\n"
                << "  \"validation_interval\": " << validation_interval << ",\n"
                << "  \"early_stop_patience\": " << early_stop_patience << ",\n"
                << "  \"minimum_complete_epochs\": " << minimum_complete_epochs << ",\n"
                << "  \"minimum_f1_delta\": " << minimum_f1_delta << ",\n"
                << "  \"early_stopped\": " << (early_stopped ? "true" : "false") << ",\n"
                << "  \"best_epoch\": " << best_validation_epoch << ",\n"
                << "  \"best_f1\": " << best_validation_f1 << ",\n"
                << "  \"restore_best\": "
                << (restore_best_validation_checkpoint != 0 ? "true" : "false") << ",\n"
                << "  \"selected_checkpoint\": "
                << QuoteRuntimeTaskJsonString(incremental_weights.string()) << ",\n"
                << "  \"best_checkpoint\": "
                << QuoteRuntimeTaskJsonString(best_validation_weights.string()) << ",\n"
                << "  \"final_epoch_checkpoint\": "
                << QuoteRuntimeTaskJsonString(final_epoch_weights.string()) << ",\n"
                << "  \"validation_history\": [\n";
            for (std::size_t index = 0; index < validation_history.size(); ++index) {
                const auto& row = validation_history[index];
                selection << "    {\"epoch\":" << row.epoch
                    << ",\"loss\":" << row.summary.loss
                    << ",\"precision\":" << row.summary.precision
                    << ",\"recall\":" << row.summary.recall
                    << ",\"f1\":" << row.summary.f1
                    << ",\"matched_iou\":" << row.summary.matched_iou
                    << ",\"became_best\":" << (row.selected ? "true" : "false")
                    << ",\"selected\":"
                    << (row.epoch == best_validation_epoch ? "true" : "false") << "}";
                if (index + 1 != validation_history.size()) selection << ",";
                selection << "\n";
            }
            selection << "  ]\n}\n";
        }
        const std::filesystem::path transfer_report_path =
            output_dir / "checkpoint_transfer_report.json";
        const std::filesystem::path freeze_audit_path =
            output_dir / "freeze_execution_audit.json";
        {
            std::ofstream transfer_report(transfer_report_path);
            transfer_report << "{\n"
                << "  \"schema\": \"cxvision.checkpoint_transfer_report.v1\",\n"
                << "  \"status\": \"CHECKPOINT_TRANSFER_VERIFIED\",\n"
                << "  \"parent_checkpoint\": " << QuoteRuntimeTaskJsonString(parent_checkpoint_path.string()) << ",\n"
                << "  \"loaded\": true,\n"
                << "  \"shape_mismatch\": [],\n"
                << "  \"missing\": [],\n"
                << "  \"newly_initialized\": "
                << (geometry_roi_head_enabled != 0
                    ? "[\"geometry_roi_head\"]"
                    : "[]") << ",\n"
                << "  \"intentionally_skipped\": "
                << (geometry_roi_head_enabled != 0
                    ? "[\"parent_checkpoint_has_no_geometry_roi_head\"]"
                    : "[]") << "\n}\n";
        }
        bool frozen_unchanged = true;
        bool frozen_buffers_unchanged = true;
        bool trainable_updated = false;
        std::ostringstream frozen_rows;
        std::ostringstream trainable_rows;
        bool first_frozen = true;
        bool first_trainable = true;
        std::ostringstream frozen_buffer_rows;
        bool first_frozen_buffer = true;
        for (const auto& named : model->named_parameters(true)) {
            const auto before = before_parameters.find(named.key());
            if (before == before_parameters.end())
                continue;
            const double absolute_update = (named.value().detach() - before->second)
                .abs().sum().item<double>();
            const bool frozen = !named.value().requires_grad();
            if (frozen) {
                frozen_unchanged = frozen_unchanged && absolute_update == 0.0;
                if (!first_frozen) frozen_rows << ",\n";
                first_frozen = false;
                frozen_rows << "    {\"name\":" << QuoteRuntimeTaskJsonString(named.key())
                    << ",\"absolute_update\":" << absolute_update
                    << ",\"requires_grad\":false}";
            } else {
                trainable_updated = trainable_updated || absolute_update > 0.0;
                if (!first_trainable) trainable_rows << ",\n";
                first_trainable = false;
                trainable_rows << "    {\"name\":" << QuoteRuntimeTaskJsonString(named.key())
                    << ",\"absolute_update\":" << absolute_update
                    << ",\"requires_grad\":true}";
            }
        }
        for (const auto& named : model->named_buffers(true)) {
            const auto before = before_frozen_buffers.find(named.key());
            if (before == before_frozen_buffers.end())
                continue;
            const double absolute_update = (named.value().detach() - before->second)
                .abs().sum().item<double>();
            frozen_buffers_unchanged = frozen_buffers_unchanged && absolute_update == 0.0;
            if (!first_frozen_buffer) frozen_buffer_rows << ",\n";
            first_frozen_buffer = false;
            frozen_buffer_rows << "    {\"name\":" << QuoteRuntimeTaskJsonString(named.key())
                << ",\"absolute_update\":" << absolute_update << "}";
        }
        {
            std::ofstream freeze_audit(freeze_audit_path);
            const bool freeze_pass = frozen_unchanged && frozen_buffers_unchanged && trainable_updated;
            freeze_audit << "{\n"
                << "  \"schema\": \"cxvision.freeze_execution_audit.v1\",\n"
                << "  \"status\": \"" << (freeze_pass ? "FREEZE_EXECUTION_PASS" : "FREEZE_EXECUTION_FAIL") << "\",\n"
                << "  \"frozen_parameter_prefixes\": [";
            for (std::size_t index = 0; index < frozen_parameter_prefixes.size(); ++index) {
                if (index != 0) freeze_audit << ", ";
                freeze_audit << QuoteRuntimeTaskJsonString(frozen_parameter_prefixes[index]);
            }
            freeze_audit << "],\n  \"frozen_unchanged\": " << (frozen_unchanged ? "true" : "false")
                << ",\n  \"frozen_buffers_unchanged\": " << (frozen_buffers_unchanged ? "true" : "false")
                << ",\n  \"trainable_updated\": " << (trainable_updated ? "true" : "false")
                << ",\n  \"frozen_parameters\": [\n" << frozen_rows.str()
                << "\n  ],\n  \"frozen_buffers\": [\n" << frozen_buffer_rows.str()
                << "\n  ],\n  \"trainable_parameters\": [\n" << trainable_rows.str() << "\n  ]\n}\n";
        }
        const YOLOv8Impl::ValidationSummary incremental_summary =
            model->val_summary(dataset_root.string(), eval_config);
        const auto end = std::chrono::steady_clock::now();
        result.train_runtime_ms = static_cast<double>(
            std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count());
        result.algorithm_runtime_ms = result.train_runtime_ms;

        const std::filesystem::path comparison_path =
            output_dir / "base_vs_incremental_inference.json";
        const bool detection_effect_established =
            base_summary.predicted_boxes > 0 ||
            incremental_summary.predicted_boxes > 0;
        {
            std::ofstream output(comparison_path);
            output << "{\n"
                   << "  \"schema\": \"cxvision.yolov8n_base_incremental_comparison.v1\",\n"
                   << "  \"validation_sample_count\": " << val_image_count << ",\n"
                   << "  \"base\": {\"loss\": " << base_summary.loss
                   << ", \"precision\": " << base_summary.precision
                   << ", \"recall\": " << base_summary.recall
                   << ", \"f1\": " << base_summary.f1
                   << ", \"matched_iou\": " << base_summary.matched_iou
                   << "},\n"
                   << "  \"incremental\": {\"loss\": "
                   << incremental_summary.loss << ", \"precision\": "
                   << incremental_summary.precision << ", \"recall\": "
                   << incremental_summary.recall << ", \"f1\": "
                   << incremental_summary.f1 << ", \"matched_iou\": "
                   << incremental_summary.matched_iou << "},\n"
                   << "  \"delta\": {\"loss\": "
                   << incremental_summary.loss - base_summary.loss
                   << ", \"f1\": "
                   << incremental_summary.f1 - base_summary.f1
                   << ", \"matched_iou\": "
                   << incremental_summary.matched_iou - base_summary.matched_iou
                   << "},\n"
                   << "  \"status\": \"CXX_YOLOV8N_INFERENCE_COMPARISON_EXECUTION_PASS\",\n"
                   << "  \"quality_status\": \""
                   << (detection_effect_established
                           ? "PENDING_HUMAN_REVIEW"
                           : "CXX_YOLOV8N_DETECTION_EFFECT_NOT_ESTABLISHED")
                   << "\",\n"
                   << "  \"promotion_allowed\": false\n"
                   << "}\n";
        }

        auto write_manifest = [&](const std::filesystem::path& path,
                                  const std::string& model_id,
                                  const std::filesystem::path& weights) {
            std::ofstream output(path);
            output << "{\n"
                   << "  \"schema\": \"cxvision.torch_model_manifest\",\n"
                   << "  \"schema_version\": 1,\n"
                   << "  \"model_id\": " << QuoteRuntimeTaskJsonString(model_id) << ",\n"
                   << "  \"task\": \"detection\",\n"
                   << "  \"architecture\": \"yolov8\",\n"
                   << "  \"variant\": \"nano\",\n"
                   << "  \"weights\": "
                   << QuoteRuntimeTaskJsonString(
                          std::filesystem::relative(weights, path.parent_path()).generic_string())
                   << ",\n"
                   << "  \"weights_format\": \"cpp_state_dict\",\n"
                   << "  \"num_classes\": " << num_classes << ",\n"
                   << "  \"classes\": [";
            for (std::size_t i = 0; i < class_names.size(); ++i)
            {
                if (i != 0)
                    output << ", ";
                output << QuoteRuntimeTaskJsonString(class_names[i]);
            }
            output << "],\n"
                   << "  \"input\": {\"width\": " << input_size
                   << ", \"height\": " << input_size
                   << ", \"color\": \"rgb\", \"scale\": 0.003921568627, "
                      "\"mean\": [0.0, 0.0, 0.0], \"std\": [1.0, 1.0, 1.0], "
                      "\"letterbox\": 0},\n"
                   << "  \"postprocess\": {\"confidence_threshold\": "
                   << postprocess_confidence_threshold
                   << ", \"iou_threshold\": " << postprocess_iou_threshold
                   << ", \"max_detections\": " << postprocess_max_detections
                   << ", \"class_agnostic_nms\": "
                   << (postprocess_class_agnostic_nms != 0 ? "true" : "false")
                   << "},\n"
                   << "  \"geometry_roi_head\": {\"enabled\": "
                   << (geometry_roi_head_enabled != 0 ? "true" : "false")
                   << ", \"feature_source\": \"neck_p3_roi\""
                   << ", \"target_manifest\": "
                   << QuoteRuntimeTaskJsonString(geometry_target_manifest)
                   << ", \"pooled_size\": " << geometry_roi_pooled_size
                   << ", \"polygon_vertex_count\": " << geometry_roi_polygon_vertex_count
                   << ", \"target_binding_status\": \""
                   << (geometry_roi_head_enabled != 0
                       ? "TRAIN_BATCH_SIDECAR_BINDING_ACTIVE"
                       : "DISABLED_BY_PLAN")
                   << "\"}\n"
                   << "}\n";
        };
        const std::filesystem::path base_manifest =
            output_dir / "base_model_manifest.json";
        const std::filesystem::path incremental_manifest =
            output_dir / "incremental_model_manifest.json";
        write_manifest(base_manifest, "cpp_yolov8n_base", base_weights);
        write_manifest(incremental_manifest, "cpp_yolov8n_incremental",
                       incremental_weights);

        const std::filesystem::path result_path =
            output_dir / "torch_yolov8n_training_lifecycle_result.json";
        std::ostringstream result_json;
        result_json << "{\n"
                    << "  \"schema\": \"cxvision.torch.yolov8n_training_lifecycle.v1\",\n"
                    << "  \"status\": \"CXX_YOLOV8N_TRAINING_BASIC_PASS\",\n"
                    << "  \"learning_curve_status\": \"REAL_MULTI_EPOCH_SERIES\",\n"
                    << "  \"inference_comparison_status\": \"CXX_YOLOV8N_INFERENCE_COMPARISON_EXECUTION_PASS\",\n"
                    << "  \"detection_quality_status\": \""
                    << (detection_effect_established
                            ? "PENDING_HUMAN_REVIEW"
                            : "CXX_YOLOV8N_DETECTION_EFFECT_NOT_ESTABLISHED")
                    << "\",\n"
                    << "  \"network_weights_updated\": true,\n"
                    << "  \"parent_checkpoint\": " << QuoteRuntimeTaskJsonString(parent_checkpoint_path.string()) << ",\n"
                    << "  \"checkpoint_transfer_ref\": " << QuoteRuntimeTaskJsonString(transfer_report_path.string()) << ",\n"
                    << "  \"freeze_execution_ref\": " << QuoteRuntimeTaskJsonString(freeze_audit_path.string()) << ",\n"
                    << "  \"requested_epochs\": " << epochs << ",\n"
                    << "  \"epochs\": " << epochs << ",\n"
                    << "  \"completed_epochs\": " << completed_epochs << ",\n"
                    << "  \"early_stopped\": " << (early_stopped ? "true" : "false") << ",\n"
                    << "  \"best_validation_epoch\": " << best_validation_epoch << ",\n"
                    << "  \"best_validation_f1\": " << best_validation_f1 << ",\n"
                    << "  \"optimizer\": \"SGD\",\n"
                    << "  \"learning_rate\": " << learning_rate << ",\n"
                    << "  \"assignment_topk\": " << assignment_topk << ",\n"
                    << "  \"global_multiscale_assignment\": "
                    << (global_multiscale_assignment != 0 ? "true" : "false") << ",\n"
                    << "  \"class_loss_weights\": [";
        for (std::size_t index = 0; index < class_loss_weights.size(); ++index) {
            if (index != 0) result_json << ", ";
            result_json << class_loss_weights[index];
        }
        result_json << "],\n"
                    << "  \"classification_focal_gamma\": " << classification_focal_gamma << ",\n"
                    << "  \"layer_loss_curve_columns\": [\"total_loss\", \"box_loss\", \"classification_loss\", \"dfl_loss\", \"geometry_parameter_loss\", \"geometry_contour_loss\", \"geometry_topology_loss\", \"geometry_uncertainty_loss\", \"geometry_calibration_loss\"],\n"
                    << "  \"geometry_roi_head\": {\n"
                    << "    \"enabled\": " << (geometry_roi_head_enabled != 0 ? "true" : "false") << ",\n"
                    << "    \"feature_source\": \"neck_p3_roi\",\n"
                    << "    \"geometry_target_manifest\": " << QuoteRuntimeTaskJsonString(geometry_target_manifest) << ",\n"
                    << "    \"target_binding_status\": \""
                    << (geometry_roi_head_enabled != 0
                        ? "TRAIN_BATCH_SIDECAR_BINDING_ACTIVE"
                        : "DISABLED_BY_PLAN") << "\",\n"
                    << "    \"hidden_channels\": " << geometry_roi_hidden_channels << ",\n"
                    << "    \"pooled_size\": " << geometry_roi_pooled_size << ",\n"
                    << "    \"polygon_vertex_count\": " << geometry_roi_polygon_vertex_count << ",\n"
                    << "    \"loss_weights\": {\"parameter\": " << geometry_roi_parameter_weight
                    << ", \"contour\": " << geometry_roi_contour_weight
                    << ", \"continuity\": " << geometry_roi_continuity_weight
                    << ", \"uncertainty\": " << geometry_roi_uncertainty_weight
                    << ", \"calibration\": " << geometry_roi_calibration_weight << "},\n"
                    << "    \"continuity_positive_weight\": " << geometry_roi_continuity_positive_weight << "\n"
                    << "  },\n"
                    << "  \"affine_channel\": {\n"
                    << "    \"effective_training_channel\": " << QuoteRuntimeTaskJsonString(affine_training_channel) << ",\n"
                    << "    \"compound_policy\": \"EVALUATION_ONLY_NOT_TRAINABLE\",\n"
                    << "    \"rotation_deg\": {\"train_positive\": " << rotation_train_positive_deg
                    << ", \"train_negative\": " << rotation_train_negative_deg
                    << ", \"validation\": " << rotation_validation_deg
                    << ", \"holdout\": " << rotation_holdout_deg << "},\n"
                    << "    \"scale\": {\"train_down\": " << scale_train_down
                    << ", \"train_up\": " << scale_train_up
                    << ", \"validation\": " << scale_validation
                    << ", \"holdout\": " << scale_holdout << "},\n"
                    << "    \"compound_unseen\": {\"validation_scale\": " << compound_validation_scale
                    << ", \"validation_rotation_deg\": " << compound_validation_rotation_deg
                    << ", \"holdout_scale\": " << compound_holdout_scale
                    << ", \"holdout_rotation_deg\": " << compound_holdout_rotation_deg << "}\n"
                    << "  },\n"
                    << "  \"geometry_fixed_validation_ref\": "
                    << QuoteRuntimeTaskJsonString(geometry_roi_head_enabled != 0
                        ? geometry_validation_report_path.string() : std::string()) << ",\n"
                    << "  \"geometry_holdout_ref\": "
                    << QuoteRuntimeTaskJsonString(geometry_roi_head_enabled != 0
                        ? geometry_holdout_report_path.string() : std::string()) << ",\n"
                    << "  \"use_assignment_quality_targets\": "
                    << (use_assignment_quality_targets != 0 ? "true" : "false") << ",\n"
                    << "  \"postprocess_confidence_threshold\": "
                    << postprocess_confidence_threshold << ",\n"
                    << "  \"postprocess_iou_threshold\": "
                    << postprocess_iou_threshold << ",\n"
                    << "  \"postprocess_max_detections\": "
                    << postprocess_max_detections << ",\n"
                    << "  \"postprocess_class_agnostic_nms\": "
                    << (postprocess_class_agnostic_nms != 0 ? "true" : "false") << ",\n"
                    << "  \"evaluation_match_iou_threshold\": "
                    << evaluation_match_iou_threshold << ",\n"
                    << "  \"batches_per_epoch\": "
                    << (epoch_batches.empty() ? 0 : epoch_batches.back()) << ",\n"
                    << "  \"train_image_count\": " << train_image_count << ",\n"
                    << "  \"validation_image_count\": " << val_image_count << ",\n"
                    << "  \"last_grad_mean\": " << last_grad_mean << ",\n"
                    << "  \"learning_curve_ref\": "
                    << QuoteRuntimeTaskJsonString(curve_csv.string()) << ",\n"
                    << "  \"comparison_ref\": "
                    << QuoteRuntimeTaskJsonString(comparison_path.string()) << ",\n"
                    << "  \"checkpoint_selection_ref\": "
                    << QuoteRuntimeTaskJsonString(checkpoint_selection_path.string()) << ",\n"
                    << "  \"base_manifest_ref\": "
                    << QuoteRuntimeTaskJsonString(base_manifest.string()) << ",\n"
                    << "  \"incremental_manifest_ref\": "
                    << QuoteRuntimeTaskJsonString(incremental_manifest.string()) << ",\n"
                    << "  \"promotion_allowed\": false,\n"
                    << "  \"human_review_required\": true\n"
                    << "}\n";
        {
            std::ofstream output(result_path);
            output << result_json.str();
        }
        result.ok = true;
        result.error_code = 0;
        result.status = "CXX_YOLOV8N_TRAINING_BASIC_PASS";
        result.result_json = result_json.str();
        result.result_ref = result_path.string();
        result.evidence_ref = comparison_path.string();
        result.visualization_refs =
            curve_csv.string() + ";" + comparison_path.string() + ";" +
            checkpoint_selection_path.string();
        result.trainer_lifecycle_summary =
            "real multi-epoch C++ YOLOv8n optimizer lifecycle completed with validation-best checkpoint selection";
        result.unified_mainline_summary =
            "base and incremental checkpoints evaluated on the same validation split";
        return result;
    }
    catch (const std::exception& error)
    {
        result.ok = false;
        result.error_code = -1;
        result.status = "CXX_YOLOV8N_TRAINING_FAIL";
        result.error_message = error.what();
        result.result_json =
            "{\"schema\":\"cxvision.torch.yolov8n_training_lifecycle.v1\","
            "\"status\":\"CXX_YOLOV8N_TRAINING_FAIL\","
            "\"reason\":" + QuoteRuntimeTaskJsonString(error.what()) + "}";
        return result;
    }
}

TorchTaskResultCpp DispatchTorchRuntimeTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    if (request.task ==
        TorchRuntimeTaskIds::Capabilities)
    {
        return RunTorchCapabilitiesTask(
            config,
            request);
    }

    if (request.task ==
        TorchRuntimeTaskIds::SegmentationContract)
    {
        return ValidateTorchSegmentationContract(
            config,
            request);
    }

    if (request.task ==
        TorchRuntimeTaskIds::DetectionContract)
    {
        return ValidateTorchDetectionContract(
            config,
            request);
    }

    if (request.task ==
        TorchRuntimeTaskIds::
            DeepLabV3PlusSegmentation)
    {
        return ExecuteTorchSegmentationTask(
            config,
            request);
    }

    if (request.task ==
        TorchRuntimeTaskIds::EdgeSamPromptSegmentation)
    {
        return ExecuteTorchEdgeSamTask(config, request);
    }

    if (request.task ==
        TorchRuntimeTaskIds::YoloV8Detection)
    {
        return ExecuteTorchDetectionTask(
            config,
            request);
    }

    if (request.task ==
        TorchRuntimeTaskIds::YoloV8InstanceSegmentation)
    {
        return ExecuteTorchYoloV8SegTask(config, request);
    }

    if (request.task ==
        TorchRuntimeTaskIds::YoloV8SegBackwardSmoke)
    {
        return ExecuteTorchYoloV8SegBackwardSmokeTask(config, request);
    }

    if (request.task == TorchRuntimeTaskIds::SegmentationTrainingLifecycle)
    {
        return RunSegmentationTrainingLifecycleTask(
            config,
            request);
    }

    // RND-VAI-001 has stricter requirements than this legacy lifecycle: it
    // requires registered real image/mask manifests and the fixed seven-class
    // contract.  In particular, it must never reach the random-tensor smoke
    // helpers used by RunSegmentationTrainingLifecycleTask.
    if (request.task == TorchRuntimeTaskIds::SegmentationBusinessIncremental ||
        request.task == TorchRuntimeTaskIds::SegmentationBusinessIsolated)
    {
        return MakeUnsupportedTask(request.task);
    }

    if (request.task ==
        TorchRuntimeTaskIds::PrototypeIncrementalLifecycle)
    {
        return ExecuteTorchPrototypeLifecycleTask(config, request);
    }

    if (request.task ==
        TorchRuntimeTaskIds::EdgeSamIncrementalPackage)
    {
        return ValidateTorchIncrementalPackageTask(
            config, request, "edgesam");
    }

    if (request.task ==
        TorchRuntimeTaskIds::YoloV8IncrementalPackage)
    {
        return ValidateTorchIncrementalPackageTask(
            config, request, "yolov8");
    }

    if (request.task ==
        TorchRuntimeTaskIds::YoloV8TrainingLifecycle)
    {
        return RunYoloV8TrainingLifecycleTask(config, request);
    }

    if (IsLegacyTestHostTask(request.task))
    {
        return RunLegacyTorchTestHostTask(
            config,
            request);
    }

    return MakeUnsupportedTask(request.task);
}

namespace
{
TorchTaskResultCpp IncrementalTaskFailure(
    const std::string& stage, const std::string& reason,
    const std::string& status = "failed")
{
    TorchTaskResultCpp result;
    result.ok = false;
    result.error_code = -1;
    result.status = status;
    result.error_message = reason;
    result.result_json =
        "{\"schema\":\"cxvision.torch.incremental.v1\","
        "\"status\":" + QuoteRuntimeTaskJsonString(status) +
        ",\"failure_stage\":" + QuoteRuntimeTaskJsonString(stage) +
        ",\"reason\":" + QuoteRuntimeTaskJsonString(reason) + "}";
    return result;
}

torch::Tensor PrototypeFeature(
    const cv::Mat& gray, const cv::Mat& edges, const int kind)
{
    cv::Scalar mean, deviation;
    cv::meanStdDev(gray, mean, deviation);
    const float m = static_cast<float>(mean[0] / 255.0);
    const float s = static_cast<float>(deviation[0] / 255.0);
    const float e = static_cast<float>(
        cv::countNonZero(edges) /
        static_cast<double>(std::max<std::size_t>(1, gray.total())));
    const float a = static_cast<float>(
        gray.cols / static_cast<double>(std::max(1, gray.rows)));
    if (kind == 0) return torch::tensor({m, s});
    if (kind == 1) return torch::tensor({a, 1.0f / std::max(a, 0.001f)});
    if (kind == 2) return torch::tensor({e, s});
    return torch::tensor({static_cast<float>(gray.cols) / 2048.0f,
                          static_cast<float>(gray.rows) / 2048.0f});
}
} // namespace

TorchTaskResultCpp ExecuteTorchPrototypeLifecycleTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request)
{
    (void)config;
    try
    {
        cv::Mat image = cv::imread(request.input_image, cv::IMREAD_COLOR);
        if (image.empty())
            return IncrementalTaskFailure(
                "input", "prototype lifecycle input image is unreadable");
        cv::Mat gray, edges;
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
        cv::Canny(gray, edges, 40.0, 120.0);

        PrototypeEntry entry;
        entry.prototype_id = "evidence_prototype";
        entry.class_name = "evidence_target";
        entry.subtype_name = "incremental";
        entry.modality_tag = "image";
        entry.semantic_vec = PrototypeFeature(gray, edges, 0);
        entry.geometry_vec = PrototypeFeature(gray, edges, 1);
        entry.texture_vec = PrototypeFeature(gray, edges, 2);
        entry.shape_vec = PrototypeFeature(gray, edges, 3);
        PrototypeIndex index;
        index.add_or_update(entry);
        index.add_or_update(entry);
        PrototypeSearchQuery query{
            entry.semantic_vec, entry.geometry_vec,
            entry.texture_vec, entry.shape_vec};
        const auto matches = index.search_topk(query, 1);
        if (matches.empty())
            return IncrementalTaskFailure(
                "prototype_query", "prototype index returned no match");

        const std::filesystem::path output_dir = request.output_dir.empty()
            ? std::filesystem::path("cxscript_runs/torch_prototype")
            : std::filesystem::path(request.output_dir);
        std::filesystem::create_directories(output_dir);
        const auto weights_ref = output_dir / "prototype_vectors.pt";
        const auto overlay_ref = output_dir / "prototype_overlay.png";
        const auto result_ref = output_dir / "prototype_result.json";
        const auto evidence_ref = output_dir / "prototype_evidence.json";
        torch::save(torch::cat({entry.semantic_vec, entry.geometry_vec,
            entry.texture_vec, entry.shape_vec}), weights_ref.string());

        cv::Mat overlay = image.clone();
        cv::rectangle(overlay, cv::Rect(1, 1,
            std::max(1, overlay.cols - 2), std::max(1, overlay.rows - 2)),
            cv::Scalar(0, 255, 0), 2);
        cv::putText(overlay, "Prototype top1: " + matches.front().class_name,
            cv::Point(12, 28), cv::FONT_HERSHEY_SIMPLEX, 0.6,
            cv::Scalar(0, 255, 0), 2);
        cv::imwrite(overlay_ref.string(), overlay);

        std::ostringstream json;
        json << "{\"schema\":\"cxvision.torch.prototype.lifecycle.v1\","
             << "\"status\":\"success\","
             << "\"incremental_update_executed\":true,"
             << "\"paired_inference_executed\":true,"
             << "\"network_weights_updated\":false,"
             << "\"prototype_count\":" << index.size() << ","
             << "\"updated_sample_count\":2,"
             << "\"top1_class\":" << QuoteRuntimeTaskJsonString(matches.front().class_name) << ","
             << "\"top1_score\":" << matches.front().fused_score << ","
             << "\"weights_ref\":" << QuoteRuntimeTaskJsonString(weights_ref.string()) << ","
             << "\"overlay_ref\":" << QuoteRuntimeTaskJsonString(overlay_ref.string()) << ","
             << "\"semantic_quality\":\"pending_human_review\"}";
        std::ofstream(result_ref, std::ios::binary) << json.str() << "\n";
        std::ofstream(evidence_ref, std::ios::binary) << json.str() << "\n";

        TorchTaskResultCpp result;
        result.ok = true;
        result.status = "success";
        result.requested_device = request.device;
        result.actual_device = "cpu";
        result.result_json = json.str();
        result.result_ref = result_ref.string();
        result.evidence_ref = evidence_ref.string();
        result.input_image_ref = request.input_image;
        result.primary_visual_ref = overlay_ref.string();
        result.visualization_refs = overlay_ref.string();
        return result;
    }
    catch (const std::exception& error)
    {
        return IncrementalTaskFailure("exception", error.what());
    }
}

TorchTaskResultCpp ValidateTorchIncrementalPackageTask(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request,
    const std::string& model_family)
{
    (void)config;
    const std::filesystem::path manifest_path(request.manifest_path);
    if (request.manifest_path.empty() || !std::filesystem::exists(manifest_path))
        return IncrementalTaskFailure(
            "manifest", model_family + " package manifest is missing",
            "pending_binding");
    std::ifstream input(manifest_path, std::ios::binary);
    const std::string manifest(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    const std::vector<std::string> keys = model_family == "edgesam"
        ? std::vector<std::string>{"encoder_weights", "decoder_weights"}
        : std::vector<std::string>{"weights"};
    for (const auto& key : keys)
    {
        const std::string relative =
            ExtractRuntimeTaskJsonString(manifest, key);
        const auto path = manifest_path.parent_path() / relative;
        if (relative.empty() || !std::filesystem::exists(path))
            return IncrementalTaskFailure(
                "model_loading",
                model_family + " exported TorchScript weight is missing",
                "pending_binding");
        try
        {
            auto module = torch::jit::load(path.string(), torch::kCPU);
            module.eval();
        }
        catch (const std::exception& error)
        {
            return IncrementalTaskFailure("torchscript_load", error.what());
        }
    }
    TorchTaskResultCpp result;
    result.ok = true;
    result.status = "binding_ready";
    result.requested_device = request.device;
    result.actual_device = "cpu";
    result.result_json =
        "{\"schema\":\"cxvision.torch.incremental_package_gate.v1\","
        "\"status\":\"binding_ready\",\"model_family\":" +
        QuoteRuntimeTaskJsonString(model_family) +
        ",\"manifest_ref\":" +
        QuoteRuntimeTaskJsonString(manifest_path.string()) +
        ",\"human_review_required\":true}";
    result.result_ref = manifest_path.string();
    result.evidence_ref = manifest_path.string();
    return result;
}
