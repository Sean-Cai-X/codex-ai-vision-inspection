#include "TorchTask.h"

void TorchTask::settask(const char* value)
{
    task_.task_id = value;
    if (task_.task_id == "torch.runtime.diagnostic")
        task_.kind = CxTorchTaskKind::RuntimeDiagnostic;
    else if (task_.task_id == "torch.device.diagnostic")
        task_.kind = CxTorchTaskKind::DeviceDiagnostic;
    else if (task_.task_id == "torch.weight.diagnostic")
        task_.kind = CxTorchTaskKind::WeightDiagnostic;
    else if (task_.task_id == "torch.runtime.capabilities.v1")
        task_.kind = CxTorchTaskKind::RuntimeCapabilities;
    else if (task_.task_id == "torch.contract.segmentation.v1")
        task_.kind = CxTorchTaskKind::SegmentationContract;
    else if (task_.task_id == "torch.contract.detection.v1")
        task_.kind = CxTorchTaskKind::DetectionContract;
    else if (task_.task_id == "torch.infer.segmentation.deeplabv3plus.v1")
        task_.kind = CxTorchTaskKind::Segmentation;
    else if (task_.task_id == "torch.infer.detection.yolov8.v1")
        task_.kind = CxTorchTaskKind::Detection;
    else if (task_.task_id == "torch.incremental.prototype.lifecycle.v1")
        task_.kind = CxTorchTaskKind::PrototypeLifecycle;
    else if (task_.task_id.find("train") != std::string::npos)
        task_.kind = CxTorchTaskKind::TrainingLifecycle;
    else if (task_.task_id.find("segmentation") != std::string::npos)
        task_.kind = CxTorchTaskKind::Segmentation;
    else if (task_.task_id.find("detection") != std::string::npos || task_.task_id.find("yolo") != std::string::npos)
        task_.kind = CxTorchTaskKind::Detection;
    else if (task_.task_id.find("classification") != std::string::npos)
        task_.kind = CxTorchTaskKind::Classification;
    else if (task_.task_id.find("feature") != std::string::npos)
        task_.kind = CxTorchTaskKind::FeatureExtraction;
    else if (task_.task_id.find("template") != std::string::npos)
        task_.kind = CxTorchTaskKind::TemplateDifference;
    else if (task_.task_id.find("smoke") != std::string::npos)
        task_.kind = CxTorchTaskKind::RuntimeDiagnostic;
    else if (task_.task_id.find("infer") != std::string::npos)
        task_.kind = CxTorchTaskKind::Segmentation;
    else
        task_.kind = CxTorchTaskKind::Unknown;
}

void TorchTask::setcase(const char* value)
{
    task_.case_id = value;
}

void TorchTask::setmodel(const char* value)
{
    task_.model_id = value;
    task_.model_path = value;
}

void TorchTask::setmanifest(const char* value)
{
    task_.manifest_path = value;
}

void TorchTask::setdatasetroot(const char* value)
{
    task_.dataset_root = value == nullptr ? "" : value;
}

void TorchTask::setdevice(const char* value)
{
    task_.requested_device = value;
}

void TorchTask::setinputpath(const char* value)
{
    task_.input_image_path = value;
}

void TorchTask::settemplatepath(const char* value)
{
    task_.template_image_path = value;
}

void TorchTask::setoutputdir(const char* value)
{
    task_.output_dir = value;
}

void TorchTask::setrequestcontext(const char* value)
{
    // Copy the three values from the current serial Headless request into this
    // task. One packed string avoids relying on several parser string constants.
    const std::string context = value == nullptr ? std::string() : std::string(value);
    const char separator = '|';
    const std::size_t first = context.find(separator);
    const std::size_t second = first == std::string::npos
        ? std::string::npos
        : context.find(separator, first + 1);

    if (first == std::string::npos || second == std::string::npos)
    {
        status_ = "invalid_request_context";
        reason_ = "TorchTask request context must contain case, input and output";
        return;
    }

    const std::size_t third = context.find(separator, second + 1);
    task_.case_id = context.substr(0, first);
    task_.input_image_path = context.substr(first + 1, second - first - 1);
    if (third == std::string::npos)
    {
        task_.output_dir = context.substr(second + 1);
        task_.extra_json.clear();
    }
    else
    {
        task_.output_dir = context.substr(second + 1, third - second - 1);
        const std::size_t fourth = context.find(separator, third + 1);
        if (fourth == std::string::npos)
        {
            task_.extra_json = context.substr(third + 1);
        }
        else
        {
            task_.extra_json = context.substr(third + 1, fourth - third - 1);
            const std::string dataset_root = context.substr(fourth + 1);
            if (!dataset_root.empty())
                task_.dataset_root = dataset_root;
        }
    }
}

void TorchTask::settimeout(int value)
{
    task_.timeout_ms = value;
}

void TorchTask::run(void* image_object)
{
    result_ = {};
    status_.clear();
    reason_.clear();

    Image* image = static_cast<Image*>(image_object);
    if (image == nullptr || image->getmat().empty())
    {
        status_ = "invalid_input_image";
        reason_ = "TorchTask input Image is empty";
        return;
    }

    std::string validation_reason;
    if (!ValidateCxTorchTaskSpec(task_, validation_reason))
    {
        status_ = "request_validation_failed";
        reason_ = validation_reason;
        return;
    }

    if (!executor_.Execute(task_, result_, reason_))
    {
        status_ = result_.failure_stage.empty() ? "torch_execute_failed" : result_.failure_stage;
        return;
    }

    status_ = result_.status.empty() ? "torch_execute_completed" : result_.status;
}

int TorchTask::getok()
{
    return result_.ok ? 1 : 0;
}

int TorchTask::geterrorcode()
{
    return result_.error_code;
}

int TorchTask::getresultcount()
{
    return static_cast<int>(result_.detections.size());
}

int TorchTask::getmaskavailable()
{
    return (result_.mask.has_value() && result_.mask->available) ? 1 : 0;
}

double TorchTask::getinferms()
{
    return result_.infer_runtime_ms;
}

double TorchTask::gettrainms()
{
    return result_.train_runtime_ms;
}

double TorchTask::gettotalms()
{
    return result_.total_runtime_ms;
}

char* TorchTask::getstatus()
{
    return const_cast<char*>(status_.c_str());
}

char* TorchTask::getreason()
{
    return const_cast<char*>(reason_.c_str());
}

char* TorchTask::getfailstage()
{
    return const_cast<char*>(result_.failure_stage.c_str());
}

char* TorchTask::getactualdevice()
{
    return const_cast<char*>(result_.actual_device.c_str());
}

char* TorchTask::getresultref()
{
    return const_cast<char*>(result_.result_ref.c_str());
}

char* TorchTask::getevidenceref()
{
    return const_cast<char*>(result_.evidence_ref.c_str());
}

char* TorchTask::getprimaryvisualref()
{
    return const_cast<char*>(result_.primary_visual_ref.c_str());
}

char* TorchTask::getmaskref()
{
    if (result_.mask.has_value())
        return const_cast<char*>(result_.mask->mask_ref.c_str());
    return const_cast<char*>("");
}

char* TorchTask::getoverlayref()
{
    if (result_.mask.has_value())
        return const_cast<char*>(result_.mask->overlay_ref.c_str());
    return const_cast<char*>("");
}

char* TorchTask::gettrainersummary()
{
    return const_cast<char*>(result_.trainer_lifecycle_summary.c_str());
}

char* TorchTask::getmainlinesummary()
{
    return const_cast<char*>(result_.unified_mainline_summary.c_str());
}

const CxInferenceResult& TorchTask::GetInferenceResult() const noexcept
{
    return result_;
}
