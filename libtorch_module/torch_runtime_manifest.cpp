#include "torch_runtime_manifest.h"
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

bool LoadTorchModelManifest(
    const std::filesystem::path& manifest_path,
    const std::filesystem::path& model_root,
    TorchModelManifest& manifest,
    std::string& reason)
{
    manifest = {};

    std::filesystem::path full_manifest_path = manifest_path;
    if (full_manifest_path.is_relative())
    {
        full_manifest_path = model_root / manifest_path;
    }

    if (!std::filesystem::exists(full_manifest_path))
    {
        reason = "manifest file not found: " + full_manifest_path.string();
        return false;
    }

    cv::FileStorage fs(full_manifest_path.string(), cv::FileStorage::READ);
    if (!fs.isOpened())
    {
        reason = "failed to open manifest: " + full_manifest_path.string();
        return false;
    }

    fs["schema"] >> manifest.schema;
    fs["schema_version"] >> manifest.schema_version;
    fs["model_id"] >> manifest.model_id;
    fs["task"] >> manifest.task;
    fs["architecture"] >> manifest.architecture;

    if (fs["backbone"].isString())
        fs["backbone"] >> manifest.backbone;
    if (fs["variant"].isString())
        fs["variant"] >> manifest.variant;

    fs["weights"] >> manifest.weights;
    fs["weights_format"] >> manifest.weights_format;
    if (fs["weights_hash"].isString())
        fs["weights_hash"] >> manifest.weights_hash;
    fs["num_classes"] >> manifest.num_classes;
    fs["mask_channels"] >> manifest.mask_channels;
    fs["prototype_channels"] >> manifest.prototype_channels;
    fs["configured_prototype_channels"] >>
        manifest.configured_prototype_channels;

    if (fs["classes"].isSeq())
    {
        cv::FileNode classes_node = fs["classes"];
        for (cv::FileNodeIterator it = classes_node.begin();
             it != classes_node.end(); ++it)
        {
            std::string class_name;
            *it >> class_name;
            manifest.class_names.push_back(class_name);
        }
    }

    if (fs["input"].isMap())
    {
        cv::FileNode input_node = fs["input"];
        input_node["width"] >> manifest.input_width;
        input_node["height"] >> manifest.input_height;
        if (input_node["color"].isString())
            input_node["color"] >> manifest.input_color;
        input_node["scale"] >> manifest.input_scale;
        
        if (input_node["mean"].isSeq())
        {
            cv::FileNode mean_node = input_node["mean"];
            for (cv::FileNodeIterator it = mean_node.begin();
                 it != mean_node.end(); ++it)
            {
                double val;
                *it >> val;
                manifest.mean.push_back(val);
            }
        }
        
        if (input_node["std"].isSeq())
        {
            cv::FileNode std_node = input_node["std"];
            for (cv::FileNodeIterator it = std_node.begin();
                 it != std_node.end(); ++it)
            {
                double val;
                *it >> val;
                manifest.std.push_back(val);
            }
        }
        
        if (input_node["letterbox"].isInt())
            input_node["letterbox"] >> manifest.letterbox;
    }

    if (fs["postprocess"].isMap())
    {
        cv::FileNode post_node = fs["postprocess"];
        post_node["target_class_id"] >> manifest.target_class_id;
        post_node["min_component_area"] >> manifest.min_component_area;
        post_node["confidence_threshold"] >> manifest.confidence_threshold;
        post_node["iou_threshold"] >> manifest.iou_threshold;
        post_node["max_detections"] >> manifest.max_detections;
        const cv::FileNode class_agnostic_node =
            post_node["class_agnostic_nms"];
        if (!class_agnostic_node.empty())
        {
            int class_agnostic_nms = 0;
            class_agnostic_node >> class_agnostic_nms;
            manifest.class_agnostic_nms = class_agnostic_nms != 0;
        }
        post_node["mask_threshold"] >> manifest.mask_threshold;
    }

    fs.release();

    manifest.manifest_directory = full_manifest_path.parent_path();
    manifest.weights_path = manifest.manifest_directory / manifest.weights;
    manifest.model_path = manifest.weights_path;

    return true;
}

bool ValidateTorchModelManifest(
    const TorchModelManifest& manifest,
    std::string& reason)
{
    if (manifest.schema != "cxvision.torch_model_manifest")
    {
        reason = "invalid manifest schema: " + manifest.schema;
        return false;
    }

    if (manifest.schema_version != 1 &&
        manifest.schema_version != 2)
    {
        reason = "unsupported schema version: " + std::to_string(manifest.schema_version);
        return false;
    }

    if (manifest.model_id.empty())
    {
        reason = "model_id is empty";
        return false;
    }

    if (manifest.task.empty())
    {
        reason = "task is empty";
        return false;
    }

    if (manifest.architecture.empty())
    {
        reason = "architecture is empty";
        return false;
    }

    if (manifest.weights.empty())
    {
        reason = "weights path is empty";
        return false;
    }

    if (manifest.weights_format.empty())
    {
        reason = "weights_format is empty";
        return false;
    }

    if (manifest.num_classes < 1)
    {
        reason = "num_classes must be >= 1";
        return false;
    }

    if (manifest.input_width <= 0 || manifest.input_height <= 0)
    {
        reason = "input width/height must be > 0";
        return false;
    }

    return true;
}

bool ValidateSegmentationManifest(
    const TorchModelManifest& manifest,
    std::string& reason)
{
    if (!ValidateTorchModelManifest(manifest, reason))
        return false;

    if (manifest.task != "segmentation")
    {
        reason = "task must be 'segmentation'";
        return false;
    }

    if (manifest.backbone.empty())
    {
        reason = "backbone is required for segmentation";
        return false;
    }

    if (manifest.target_class_id < 0 || manifest.target_class_id >= manifest.num_classes)
    {
        reason = "target_class_id is out of range";
        return false;
    }

    if (!std::filesystem::exists(manifest.weights_path))
    {
        reason = "weights file not found: " + manifest.weights_path.string();
        return false;
    }

    return true;
}

bool ValidateDetectionManifest(
    const TorchModelManifest& manifest,
    std::string& reason)
{
    if (!ValidateTorchModelManifest(manifest, reason))
        return false;

    if (manifest.task != "detection")
    {
        reason = "task must be 'detection'";
        return false;
    }

    if (manifest.variant.empty())
    {
        reason = "variant is required for detection";
        return false;
    }

    std::vector<std::string> valid_variants = {
        "nano", "small", "medium", "large", "xlarge"
    };
    bool valid_variant = false;
    for (const auto& v : valid_variants)
    {
        if (manifest.variant == v)
        {
            valid_variant = true;
            break;
        }
    }
    if (!valid_variant)
    {
        reason = "invalid variant: " + manifest.variant;
        return false;
    }

    if (!manifest.class_names.empty() &&
        static_cast<int>(manifest.class_names.size()) != manifest.num_classes)
    {
        reason = "class_names count does not match num_classes";
        return false;
    }

    if (manifest.confidence_threshold < 0.0f || manifest.confidence_threshold > 1.0f)
    {
        reason = "confidence_threshold must be in [0,1]";
        return false;
    }

    if (manifest.iou_threshold < 0.0f || manifest.iou_threshold > 1.0f)
    {
        reason = "iou_threshold must be in [0,1]";
        return false;
    }

    if (manifest.max_detections <= 0)
    {
        reason = "max_detections must be > 0";
        return false;
    }

    if (!std::filesystem::exists(manifest.weights_path))
    {
        reason = "weights file not found: " + manifest.weights_path.string();
        return false;
    }

    return true;
}

bool ValidateInstanceSegmentationManifest(
    const TorchModelManifest& manifest,
    std::string& reason)
{
    if (!ValidateTorchModelManifest(manifest, reason))
        return false;
    if (manifest.schema_version != 2)
    {
        reason = "instance segmentation requires manifest schema version 2";
        return false;
    }
    if (manifest.task != "instance_segmentation")
    {
        reason = "task must be 'instance_segmentation'";
        return false;
    }
    if (manifest.architecture != "yolov8_seg")
    {
        reason = "architecture must be 'yolov8_seg'";
        return false;
    }
    if (manifest.variant != "nano")
    {
        reason = "only the preflighted nano variant is supported";
        return false;
    }
    if (manifest.weights_format != "python_state_dict")
    {
        reason = "weights_format must be 'python_state_dict'";
        return false;
    }
    if (manifest.mask_channels != 32 ||
        manifest.prototype_channels != 64)
    {
        reason = "preflighted YOLOv8n-Seg requires nm=32 and effective npr=64";
        return false;
    }
    if (manifest.class_names.size() !=
        static_cast<std::size_t>(manifest.num_classes))
    {
        reason = "class_names count does not match num_classes";
        return false;
    }
    if (manifest.weights_hash.empty())
    {
        reason = "weights_hash is required";
        return false;
    }
    if (manifest.mask_threshold < 0.0f ||
        manifest.mask_threshold > 1.0f)
    {
        reason = "mask_threshold must be in [0,1]";
        return false;
    }
    if (!std::filesystem::exists(manifest.weights_path))
    {
        reason = "weights file not found: " +
                 manifest.weights_path.string();
        return false;
    }
    return true;
}
