#pragma once

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace cxvision::gwy_reference
{

enum class CxGwyExecutionMode
{
    NativeOnly = 0,
    ReferenceOnly = 1,
    DualCompare = 2
};

inline const char* ToString(CxGwyExecutionMode mode)
{
    switch (mode)
    {
    case CxGwyExecutionMode::NativeOnly: return "native";
    case CxGwyExecutionMode::ReferenceOnly: return "gwy_reference";
    case CxGwyExecutionMode::DualCompare: return "dual_compare";
    }
    return "unknown";
}

struct CxGwyReferenceRequest
{
    std::string schema = "cxvision.gwy_reference_request.v1";
    std::string request_id;
    std::string case_id;
    std::string algorithm_id;
    std::string input_ref;
    std::string input_hash;
    std::string input_data_path;
    std::string input_format = "cx_surface_text_v1";
    int width = 0;
    int height = 0;
    double x_scale = 1.0;
    double y_scale = 1.0;
    double z_scale = 1.0;
    std::string x_unit = "Pixel";
    std::string y_unit = "Pixel";
    std::string z_unit = "Pixel";
    int roi_x = 0;
    int roi_y = 0;
    int roi_width = 0;
    int roi_height = 0;
    std::string mask_mode = "ignore";
    std::map<std::string, double> numeric_parameters;
    CxGwyExecutionMode mode = CxGwyExecutionMode::DualCompare;
};

struct CxGwyPoint
{
    double x = 0.0;
    double y = 0.0;
    double value = 0.0;
};

struct CxGwyNormalizedResult
{
    std::string schema = "cxvision.gwy_normalized_result.v1";
    std::string implementation;
    std::string implementation_version;
    std::string source_hash;
    std::string status = "NOT_RUN";
    std::string conclusion = "PENDING_BINDING";
    std::string failure_stage;
    std::string reason;
    bool backend_available = false;
    bool executed = false;
    bool algorithm_success = false;
    bool promotion_allowed = false;
    double elapsed_ms = 0.0;
    std::map<std::string, double> metrics;
    std::vector<CxGwyPoint> points;
    std::map<std::string, std::string> artifacts;
};

struct CxGwyReferenceBackendDescriptor
{
    std::string backend_id;
    std::string backend_version;
    bool available = false;
    bool external_process = true;
    std::string reason;
};

class IGwyReferenceBackend
{
public:
    virtual ~IGwyReferenceBackend() = default;
    virtual CxGwyReferenceBackendDescriptor descriptor() const = 0;
    virtual CxGwyNormalizedResult execute(const CxGwyReferenceRequest& request) = 0;
};

// Default backend deliberately contains no Gwyddion headers, symbols or binary
// dependency.  It validates the host-side call path while keeping algorithm
// acceptance pending until an external runner is explicitly bound.
class CxNullGwyReferenceBackend final : public IGwyReferenceBackend
{
public:
    CxGwyReferenceBackendDescriptor descriptor() const override
    {
        return {
            "gwy_reference_null",
            "1",
            false,
            true,
            "external GWY reference runner is not bound"
        };
    }

    CxGwyNormalizedResult execute(const CxGwyReferenceRequest& request) override
    {
        const auto d = descriptor();
        CxGwyNormalizedResult result;
        result.implementation = d.backend_id;
        result.implementation_version = d.backend_version;
        result.status = "GWY_REFERENCE_NOT_BOUND";
        result.conclusion = "PENDING_BINDING";
        result.failure_stage = "reference_backend_binding";
        result.reason = d.reason + "; request_id=" + request.request_id;
        result.backend_available = false;
        result.executed = false;
        result.algorithm_success = false;
        result.promotion_allowed = false;
        return result;
    }
};

struct CxGwyMetricDiff
{
    std::string metric;
    bool comparable = false;
    double native_value = 0.0;
    double reference_value = 0.0;
    double absolute_difference = 0.0;
    double relative_difference = 0.0;
};

struct CxGwyComparisonResult
{
    std::string schema = "cxvision.gwy_comparison_result.v1";
    std::string status = "NOT_RUN";
    std::string conclusion = "PENDING_BINDING";
    std::string reason;
    bool comparison_performed = false;
    bool promotion_allowed = false;
    std::vector<CxGwyMetricDiff> metric_diffs;
};

struct CxGwyReferenceRunPackage
{
    CxGwyReferenceRequest request;
    CxGwyNormalizedResult native_result;
    CxGwyNormalizedResult reference_result;
    CxGwyComparisonResult comparison;
    std::filesystem::path request_path;
    std::filesystem::path native_result_path;
    std::filesystem::path reference_result_path;
    std::filesystem::path comparison_path;
    std::filesystem::path report_path;
    bool interface_closure_ok = false;
};

inline std::string JsonEscape(const std::string& value)
{
    std::string out;
    out.reserve(value.size());
    for (char c : value)
    {
        switch (c)
        {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
        }
    }
    return out;
}

inline CxGwyComparisonResult CompareResults(
    const CxGwyNormalizedResult& nativeResult,
    const CxGwyNormalizedResult& referenceResult)
{
    CxGwyComparisonResult comparison;
    if (!referenceResult.backend_available || !referenceResult.executed)
    {
        comparison.status = "GWY_REFERENCE_NOT_BOUND";
        comparison.conclusion = "PENDING_BINDING";
        comparison.reason = referenceResult.reason;
        return comparison;
    }
    if (!nativeResult.executed)
    {
        comparison.status = "NATIVE_RESULT_NOT_AVAILABLE";
        comparison.conclusion = "PENDING_BINDING";
        comparison.reason = "native normalized result was not supplied";
        return comparison;
    }

    comparison.status = "COMPARISON_COMPLETE";
    comparison.conclusion = "PENDING_HUMAN_REVIEW";
    comparison.reason = "normalized values compared; acceptance requires a contract and human review";
    comparison.comparison_performed = true;
    for (const auto& entry : nativeResult.metrics)
    {
        const auto it = referenceResult.metrics.find(entry.first);
        if (it == referenceResult.metrics.end())
            continue;
        CxGwyMetricDiff diff;
        diff.metric = entry.first;
        diff.comparable = true;
        diff.native_value = entry.second;
        diff.reference_value = it->second;
        diff.absolute_difference = std::abs(entry.second - it->second);
        const double denom = std::max(std::abs(it->second), 1e-15);
        diff.relative_difference = diff.absolute_difference / denom;
        comparison.metric_diffs.push_back(diff);
    }
    return comparison;
}

inline bool WriteText(const std::filesystem::path& path,
                      const std::string& text,
                      std::string& reason)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        reason = "failed to open output: " + path.string();
        return false;
    }
    out << text;
    if (!out.good())
    {
        reason = "failed to write output: " + path.string();
        return false;
    }
    return true;
}

inline std::string SerializeRequest(const CxGwyReferenceRequest& request)
{
    std::ostringstream out;
    out << std::setprecision(17);
    out << "{\n"
        << "  \"schema\": \"" << JsonEscape(request.schema) << "\",\n"
        << "  \"request_id\": \"" << JsonEscape(request.request_id) << "\",\n"
        << "  \"case_id\": \"" << JsonEscape(request.case_id) << "\",\n"
        << "  \"algorithm_id\": \"" << JsonEscape(request.algorithm_id) << "\",\n"
        << "  \"mode\": \"" << ToString(request.mode) << "\",\n"
        << "  \"input_ref\": \"" << JsonEscape(request.input_ref) << "\",\n"
        << "  \"input_hash\": \"" << JsonEscape(request.input_hash) << "\",\n"
        << "  \"input_data_path\": \"" << JsonEscape(request.input_data_path) << "\",\n"
        << "  \"input_format\": \"" << JsonEscape(request.input_format) << "\",\n"
        << "  \"size\": {\"width\": " << request.width << ", \"height\": " << request.height << "},\n"
        << "  \"scale\": {\"x\": " << request.x_scale << ", \"y\": " << request.y_scale
        << ", \"z\": " << request.z_scale << "},\n"
        << "  \"unit\": {\"x\": \"" << JsonEscape(request.x_unit)
        << "\", \"y\": \"" << JsonEscape(request.y_unit)
        << "\", \"z\": \"" << JsonEscape(request.z_unit) << "\"},\n"
        << "  \"roi\": {\"x\": " << request.roi_x << ", \"y\": " << request.roi_y
        << ", \"width\": " << request.roi_width << ", \"height\": " << request.roi_height << "},\n"
        << "  \"mask_mode\": \"" << JsonEscape(request.mask_mode) << "\",\n"
        << "  \"numeric_parameters\": {";
    bool first = true;
    for (const auto& entry : request.numeric_parameters)
    {
        out << (first ? "\n" : ",\n") << "    \"" << JsonEscape(entry.first)
            << "\": " << entry.second;
        first = false;
    }
    if (!first)
        out << "\n  ";
    out << "}\n}\n";
    return out.str();
}

inline std::string SerializeResult(const CxGwyNormalizedResult& result)
{
    std::ostringstream out;
    out << std::setprecision(17);
    const char q = 34;
    out << "{\n"
        << "  " << q << "schema" << q << ": " << q << JsonEscape(result.schema) << q << ",\n"
        << "  " << q << "implementation" << q << ": " << q << JsonEscape(result.implementation) << q << ",\n"
        << "  " << q << "status" << q << ": " << q << JsonEscape(result.status) << q << ",\n"
        << "  " << q << "conclusion" << q << ": " << q << JsonEscape(result.conclusion) << q << ",\n"
        << "  " << q << "reason" << q << ": " << q << JsonEscape(result.reason) << q << ",\n"
        << "  " << q << "backend_available" << q << ": " << (result.backend_available ? "true" : "false") << ",\n"
        << "  " << q << "executed" << q << ": " << (result.executed ? "true" : "false") << ",\n"
        << "  " << q << "algorithm_success" << q << ": " << (result.algorithm_success ? "true" : "false") << ",\n"
        << "  " << q << "promotion_allowed" << q << ": " << (result.promotion_allowed ? "true" : "false") << ",\n"
        << "  " << q << "elapsed_ms" << q << ": " << result.elapsed_ms << "\n"
        << "}\n";
    return out.str();
}

inline std::string SerializeComparison(const CxGwyComparisonResult& comparison)
{
    std::ostringstream out;
    out << std::setprecision(17);
    out << "{\n"
        << "  \"schema\": \"" << JsonEscape(comparison.schema) << "\",\n"
        << "  \"status\": \"" << JsonEscape(comparison.status) << "\",\n"
        << "  \"conclusion\": \"" << JsonEscape(comparison.conclusion) << "\",\n"
        << "  \"reason\": \"" << JsonEscape(comparison.reason) << "\",\n"
        << "  \"comparison_performed\": " << (comparison.comparison_performed ? "true" : "false") << ",\n"
        << "  \"promotion_allowed\": false,\n"
        << "  \"metric_diffs\": [\n";
    for (std::size_t i = 0; i < comparison.metric_diffs.size(); ++i)
    {
        const auto& d = comparison.metric_diffs[i];
        out << "    {\"metric\":\"" << JsonEscape(d.metric)
            << "\", \"native\":" << d.native_value
            << ", \"reference\":" << d.reference_value
            << ", \"absolute_difference\":" << d.absolute_difference
            << ", \"relative_difference\":" << d.relative_difference << "}"
            << (i + 1 < comparison.metric_diffs.size() ? "," : "") << "\n";
    }
    out << "  ]\n}\n";
    return out.str();
}

inline bool RunReferenceInterfaceClosure(
    const CxGwyReferenceRequest& request,
    IGwyReferenceBackend& backend,
    const CxGwyNormalizedResult* nativeResult,
    const std::filesystem::path& outputDir,
    CxGwyReferenceRunPackage& package,
    std::string& reason)
{
    package = {};
    package.request = request;
    package.native_result = nativeResult ? *nativeResult : CxGwyNormalizedResult{};
    if (!nativeResult)
    {
        package.native_result.implementation = "cx_native_unbound_for_interface_smoke";
        package.native_result.status = "NATIVE_RESULT_NOT_SUPPLIED";
        package.native_result.conclusion = "PENDING_BINDING";
        package.native_result.reason = "interface closure does not execute a native algorithm";
    }
    package.reference_result = backend.execute(request);
    package.comparison = CompareResults(package.native_result, package.reference_result);

    std::error_code ec;
    std::filesystem::create_directories(outputDir, ec);
    if (ec)
    {
        reason = "failed to create GWY interface output directory: " + ec.message();
        return false;
    }

    package.request_path = outputDir / "gwy_reference_request.json";
    package.native_result_path = outputDir / "cx_native_result.json";
    package.reference_result_path = outputDir / "gwy_reference_result.json";
    package.comparison_path = outputDir / "gwy_reference_diff.json";
    package.report_path = outputDir / "gwy_reference_interface_report.md";

    if (!WriteText(package.request_path, SerializeRequest(package.request), reason) ||
        !WriteText(package.native_result_path, SerializeResult(package.native_result), reason) ||
        !WriteText(package.reference_result_path, SerializeResult(package.reference_result), reason) ||
        !WriteText(package.comparison_path, SerializeComparison(package.comparison), reason))
        return false;

    std::ostringstream report;
    report << "# GWY Reference Interface Closure\n\n"
           << "- interface_closure: INTERFACE_CLOSURE_PASS\n"
           << "- reference_status: " << package.reference_result.status << "\n"
           << "- comparison_conclusion: " << package.comparison.conclusion << "\n"
           << "- backend_available: " << (package.reference_result.backend_available ? "true" : "false") << "\n"
           << "- algorithm_executed: " << (package.reference_result.executed ? "true" : "false") << "\n"
           << "- promotion_allowed: false\n"
           << "- reason: " << package.reference_result.reason << "\n\n"
           << "No Gwyddion source, header, library or binary is linked by this closure.\n";
    if (!WriteText(package.report_path, report.str(), reason))
        return false;

    package.interface_closure_ok = true;
    if (package.comparison.comparison_performed)
        reason = "GWY reference and cxvision native results executed; comparison pending human review";
    else if (package.reference_result.backend_available &&
             package.reference_result.executed)
        reason = "GWY reference executed; native comparison result remains pending";
    else
        reason = "GWY reference interface closure completed; external backend remains pending binding";
    return true;
}

} // namespace cxvision::gwy_reference