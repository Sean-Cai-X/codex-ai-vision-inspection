#include "torch_runtime_artifact_writer.h"
#include <opencv2/imgcodecs.hpp>
#include <fstream>
#include <sstream>
#include <iomanip>

bool WriteTorchTextArtifact(
    const std::filesystem::path& path,
    const std::string& content,
    std::string& reason)
{
    try
    {
        std::filesystem::create_directories(path.parent_path());

        std::ofstream file(path);
        if (!file.is_open())
        {
            reason = "failed to open file for writing: " + path.string();
            return false;
        }

        file << content;
        if (!file)
        {
            reason = "failed to write content to file: " + path.string();
            return false;
        }

        file.close();
        return true;
    }
    catch (const std::exception& e)
    {
        reason = "exception while writing artifact: " + std::string(e.what());
        return false;
    }
}

bool WriteTorchImageArtifact(
    const std::filesystem::path& path,
    const cv::Mat& image,
    std::string& reason)
{
    try
    {
        std::filesystem::create_directories(path.parent_path());

        std::string ext = path.extension().string();
        std::vector<int> params = {cv::IMWRITE_PNG_COMPRESSION, 3};

        bool success = cv::imwrite(path.string(), image, params);
        if (!success)
        {
            reason = "failed to write image to file: " + path.string();
            return false;
        }

        return true;
    }
    catch (const std::exception& e)
    {
        reason = "exception while writing image artifact: " + std::string(e.what());
        return false;
    }
}

std::filesystem::path BuildTorchCaseDirectory(
    const TorchRuntimeCoreConfig& config,
    const TorchTaskRequestCpp& request,
    std::string& reason)
{
    std::filesystem::path base_dir;

    if (!request.output_dir.empty())
    {
        base_dir = std::filesystem::path(request.output_dir);
    }
    else if (!config.output_root.empty())
    {
        std::string case_name = request.case_name;
        if (case_name.empty())
        {
            case_name = "unnamed_case";
        }

        std::replace(case_name.begin(), case_name.end(), '/', '_');
        std::replace(case_name.begin(), case_name.end(), '\\', '_');
        std::replace(case_name.begin(), case_name.end(), ':', '_');
        std::replace(case_name.begin(), case_name.end(), '*', '_');
        std::replace(case_name.begin(), case_name.end(), '?', '_');
        std::replace(case_name.begin(), case_name.end(), '"', '_');
        std::replace(case_name.begin(), case_name.end(), '<', '_');
        std::replace(case_name.begin(), case_name.end(), '>', '_');
        std::replace(case_name.begin(), case_name.end(), '|', '_');

        base_dir = std::filesystem::path(config.output_root) / case_name;
    }
    else
    {
        reason = "output_root and output_dir are both empty";
        return {};
    }

    try
    {
        std::filesystem::create_directories(base_dir);
    }
    catch (const std::exception& e)
    {
        reason = "failed to create case directory: " + std::string(e.what());
        return {};
    }

    return base_dir;
}

std::string EscapeTorchJsonString(const std::string& value)
{
    std::ostringstream os;
    for (char c : value)
    {
        switch (c)
        {
        case '\\': os << "\\\\"; break;
        case '"': os << "\\\""; break;
        case '\n': os << "\\n"; break;
        case '\r': os << "\\r"; break;
        case '\t': os << "\\t"; break;
        case '\b': os << "\\b"; break;
        case '\f': os << "\\f"; break;
        default:
            if (c < 0x20)
            {
                os << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
            }
            else
            {
                os << c;
            }
            break;
        }
    }
    return os.str();
}

std::string QuoteTorchJsonString(const std::string& value)
{
    return "\"" + EscapeTorchJsonString(value) + "\"";
}
