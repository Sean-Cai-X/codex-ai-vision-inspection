#include "CxScriptSuiteRunner.h"
#include "CxScriptSuiteRuntime.h"
#include "CxScriptCatalogRuntime.h"
#include "CxScriptImageManifestRuntime.h"
#include "CxScriptSuiteReportWriter.h"
#include "CxScriptToolDisplayExporter.h"
#include "CxScriptBestCaseSelector.h"
#include "CxParameterProfileRuntime.h"
#include "CxParserRuntimeOwner.h"
#include "CxScriptEvidenceChainRuntime.h"
#include "CxScriptReviewGateRuntime.h"
#include "CxScriptRunTraceRuntime.h"
#include "CxAlgorithmTraceSink.h"
#include "ManualStateTestConsole.h"
#include "CxScriptHeadlessRuntime.h"
#include "CxUnifiedLog.h"
#include "CxPrecisionEvaluation.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
#include <cctype>
#include <functional>
#include <iomanip>
#include <opencv2/opencv.hpp>

class ScopedSuiteTimer
{
public:
    explicit ScopedSuiteTimer(const std::string& name)
        : name_(name),
          begin_(std::chrono::steady_clock::now())
    {
        CXLOG_INFO("CxScriptSuiteRunner", "phase_begin", "running", name_);
    }

    ~ScopedSuiteTimer()
    {
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - begin_).count();

        CXLOG_INFO("CxScriptSuiteRunner", "phase_end", "finished", 
                   name_ + " elapsed_ms=" + std::to_string(ms));
    }

    long long elapsed_ms() const
    {
        auto end = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            end - begin_).count();
    }

private:
    std::string name_;
    std::chrono::steady_clock::time_point begin_;
};

const CxScriptCatalogEntry* FindCatalogScriptById(
    const CxScriptCatalogRuntime& catalog,
    const std::string& script_id)
{
    for (const auto& script : catalog.scripts)
    {
        if (script.script_id == script_id)
            return &script;
    }
    return nullptr;
}

namespace
{
    std::string NormalizeManifestPathForCompare(std::string path)
    {
        for (char& ch : path)
        {
            if (ch == '\\')
                ch = '/';
            else
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        return path;
    }

    bool IsStage25L1L3CanonicalManifest(const std::string& manifestPath)
    {
        const std::string normalized = NormalizeManifestPathForCompare(manifestPath);
        const std::string canonical =
            "cxparser/cxscript/module/cximage/stage25/manifests/stage25_l1_l3_manifest.json";
        return normalized == canonical ||
               (normalized.size() > canonical.size() &&
                normalized.compare(normalized.size() - canonical.size(), canonical.size(), canonical) == 0);
    }

    bool RequiresStage25L1L3CanonicalManifest(const CxScriptSuiteRuntime& suite)
    {
        return suite.suite_id == "stage25_l1_l3_parameter_consistency";
    }

    bool StopForHumanReviewIfNeeded(
        const CxScriptSuiteRunOptions& options,
        CxScriptSuiteCaseResult& caseResult,
        const std::string& stage,
        const std::string& suggestedAction);

    bool WriteHumanReviewJson(
        const std::filesystem::path& path,
        const CxScriptHumanReview& review,
        std::string& reason);

    void AppendNodeTrace(
        const std::filesystem::path& caseDir,
        const std::string& node_id,
        const std::string& phase,
        const std::string& status,
        const std::string& message = "",
        int elapsed_ms = 0,
        int valid_points = 0)
    {
        auto path = caseDir / "evidence_node_trace.jsonl";
        std::ofstream file(path, std::ios::app);
        if (!file.is_open())
            return;

        std::time_t now = std::time(nullptr);
        char time_buf[64];
        std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&now));

        file << "{\"node_id\":\"" << node_id << "\","
             << "\"phase\":\"" << phase << "\","
             << "\"status\":\"" << status << "\","
             << "\"timestamp\":\"" << time_buf << "\","
             << "\"phase_elapsed_ms\":" << elapsed_ms << ","
             << "\"valid_points\":" << valid_points;

        if (!message.empty())
            file << ",\"message\":\"" << message << "\"";

        file << "}\n";
    }

    std::string TrimJsonScalar(std::string value)
    {
        const size_t comment = value.find(',');
        if (comment != std::string::npos)
            value = value.substr(0, comment);

        size_t start = value.find_first_not_of(" \t\r\n\"");
        size_t end = value.find_last_not_of(" \t\r\n\"");
        if (start == std::string::npos || end == std::string::npos || end < start)
            return "";

        return value.substr(start, end - start + 1);
    }

    bool JsonLineHasKey(const std::string& line, const std::string& key)
    {
        return line.find("\"" + key + "\"") != std::string::npos;
    }

    std::string JsonLineValue(const std::string& line)
    {
        const auto colon = line.find(":");
        if (colon == std::string::npos)
            return "";
        return TrimJsonScalar(line.substr(colon + 1));
    }

    bool ParseJsonBoolValue(const std::string& value)
    {
        return value == "true" || value == "1";
    }

    std::string SanitizePathToken(const std::string& value)
    {
        std::string out;
        out.reserve(value.size());
        for (char ch : value)
        {
            const bool ok =
                (ch >= 'a' && ch <= 'z') ||
                (ch >= 'A' && ch <= 'Z') ||
                (ch >= '0' && ch <= '9') ||
                ch == '_' || ch == '-' || ch == '.';
            out.push_back(ok ? ch : '_');
        }
        return out.empty() ? "case" : out;
    }

    std::string CompactCaseDirectoryName(const std::string& caseId)
    {
        const std::string safe = SanitizePathToken(caseId);
        if (safe.size() <= 72)
            return safe;

        std::ostringstream hash;
        hash << std::hex << std::setw(8) << std::setfill('0')
             << (static_cast<unsigned int>(std::hash<std::string>{}(safe)) & 0xffffffffu);

        return safe.substr(0, 56) + "_" + hash.str();
    }

    std::string JsonEscape(const std::string& value)
    {
        std::string out;
        out.reserve(value.size() + 8);
        for (char ch : value)
        {
            switch (ch)
            {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(ch); break;
            }
        }
        return out;
    }

    bool ExtractJsonIntLoose(
        const std::string& text,
        const std::string& key,
        int& value)
    {
        const std::string pattern = "\"" + key + "\"";
        size_t pos = text.find(pattern);
        if (pos == std::string::npos)
            return false;
        pos = text.find(':', pos);
        if (pos == std::string::npos)
            return false;
        ++pos;
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])))
            ++pos;
        size_t end = pos;
        if (end < text.size() && (text[end] == '-' || text[end] == '+'))
            ++end;
        while (end < text.size() && std::isdigit(static_cast<unsigned char>(text[end])))
            ++end;
        if (end == pos)
            return false;
        try
        {
            value = std::stoi(text.substr(pos, end - pos));
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool ExtractJsonStringLoose(
        const std::string& text,
        const std::string& key,
        std::string& value)
    {
        const std::string pattern = "\"" + key + "\"";
        size_t pos = text.find(pattern);
        if (pos == std::string::npos)
            return false;
        pos = text.find(':', pos);
        if (pos == std::string::npos)
            return false;
        pos = text.find('"', pos + 1);
        if (pos == std::string::npos)
            return false;
        const size_t end = text.find('"', pos + 1);
        if (end == std::string::npos)
            return false;
        value = text.substr(pos + 1, end - pos - 1);
        return true;
    }

    bool ApplyManualGaugeAnnotationFile(
        const std::filesystem::path& path,
        CxScriptHeadlessOptions& headless,
        CxScriptSuiteCaseResult& out,
        std::string& reason)
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            reason = "manual gauge annotation not found: " + path.string();
            return false;
        }
        const std::string text(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>());

        std::string reviewStatus;
        ExtractJsonStringLoose(text, "review_status", reviewStatus);
        if (reviewStatus != "manual_accepted" &&
            reviewStatus != "accepted" &&
            reviewStatus != "promoted")
        {
            reason = "manual gauge annotation is not accepted: " + reviewStatus;
            return false;
        }

        int v = 0;
        bool any = false;
        if (ExtractJsonIntLoose(text, "x0", v) || ExtractJsonIntLoose(text, "roi_x0", v))
        {
            headless.roi_x0 = v; out.roi_x0 = v; any = true;
        }
        if (ExtractJsonIntLoose(text, "y0", v) || ExtractJsonIntLoose(text, "roi_y0", v))
        {
            headless.roi_y0 = v; out.roi_y0 = v; any = true;
        }
        if (ExtractJsonIntLoose(text, "x1", v) || ExtractJsonIntLoose(text, "roi_x1", v))
        {
            headless.roi_x1 = v; out.roi_x1 = v; any = true;
        }
        if (ExtractJsonIntLoose(text, "y1", v) || ExtractJsonIntLoose(text, "roi_y1", v))
        {
            headless.roi_y1 = v; out.roi_y1 = v; any = true;
        }
        if (ExtractJsonIntLoose(text, "tool_half_width", v))
            headless.tool_half_width = v;
        if (ExtractJsonIntLoose(text, "wgap", v))
            headless.wgap = v;
        if (ExtractJsonIntLoose(text, "hgap", v))
            headless.hgap = v;

        if (ExtractJsonIntLoose(text, "cx", v) || ExtractJsonIntLoose(text, "circle_cx", v))
        {
            headless.circle_cx = v; out.circle_cx = v; any = true;
        }
        if (ExtractJsonIntLoose(text, "cy", v) || ExtractJsonIntLoose(text, "circle_cy", v))
        {
            headless.circle_cy = v; out.circle_cy = v; any = true;
        }
        if (ExtractJsonIntLoose(text, "px", v) || ExtractJsonIntLoose(text, "circle_px", v))
        {
            headless.circle_px = v; out.circle_px = v; any = true;
        }
        if (ExtractJsonIntLoose(text, "py", v) || ExtractJsonIntLoose(text, "circle_py", v))
        {
            headless.circle_py = v; out.circle_py = v; any = true;
        }
        if (ExtractJsonIntLoose(text, "gap", v))
            headless.gap = v;
        if (ExtractJsonIntLoose(text, "linegap", v))
            headless.linegap = v;
        if (ExtractJsonIntLoose(text, "threshold", v))
            headless.threshold = v;
        if (ExtractJsonIntLoose(text, "method", v))
            headless.method = v;
        if (ExtractJsonIntLoose(text, "filterprofile", v))
            headless.filterprofile = v;

        reason = any
            ? "Gauge Source: manual_accepted | Gauge Annotation: " + path.string()
            : "manual gauge annotation has no supported gauge fields";
        if (any)
        {
            out.gauge_source = "manual_accepted";
            out.gauge_review_status = reviewStatus;
            out.gauge_annotation_path = path.string();
        }
        return any;
    }

    bool JsonLineHasAnyKey(const std::string& line, const std::vector<std::string>& keys)
    {
        for (const auto& key : keys)
        {
            if (JsonLineHasKey(line, key))
                return true;
        }
        return false;
    }

    void LoadSuiteCaseMetricsFromSummary(
        const std::string& summary_path,
        CxScriptSuiteCaseResult& out)
    {
        if (summary_path.empty())
            return;

        std::ifstream file(summary_path);
        if (!file.is_open())
            return;

        std::cout << "[DEBUG] Loading summary from: " << summary_path << "\n";

        std::string line;
        std::string jsonContent;
        while (std::getline(file, line))
        {
            jsonContent += line + "\n";
        }

        size_t pointsArrayStart = jsonContent.find("\"measure_points_xy\": [");
        if (pointsArrayStart != std::string::npos)
        {
            pointsArrayStart += std::string("\"measure_points_xy\": [").size();
            size_t pointsArrayEnd = jsonContent.find("\n        ]", pointsArrayStart);
            if (pointsArrayEnd == std::string::npos)
                pointsArrayEnd = jsonContent.find("\n      ]", pointsArrayStart);
            if (pointsArrayEnd == std::string::npos)
                pointsArrayEnd = jsonContent.find("]\n", pointsArrayStart);
            if (pointsArrayEnd != std::string::npos)
            {
                std::string pointsArray = jsonContent.substr(pointsArrayStart, pointsArrayEnd - pointsArrayStart);
                size_t pos = 0;
                int parsedPointCount = 0;
                const int maxParsedPointCount = 2000;
                while ((pos = pointsArray.find("[", pos)) != std::string::npos &&
                       parsedPointCount < maxParsedPointCount)
                {
                    size_t endPos = pointsArray.find("]", pos);
                    if (endPos == std::string::npos)
                        break;

                    if (endPos != std::string::npos)
                    {
                        std::string pointPair = pointsArray.substr(pos + 1, endPos - pos - 1);
                        size_t commaPos = pointPair.find(",");
                        if (commaPos != std::string::npos)
                        {
                            try
                            {
                                double x = std::stod(pointPair.substr(0, commaPos));
                                double y = std::stod(pointPair.substr(commaPos + 1));
                                out.measure_points_xy.emplace_back(x, y);
                                ++parsedPointCount;
                            }
                            catch (...) {}
                        }
                    }
                    pos = endPos + 1;
                }
            }
        }

        std::istringstream contentStream(jsonContent);
        while (std::getline(contentStream, line))
        {
            if (JsonLineHasKey(line, "runtime_valid_points_count"))
            {
                try { out.runtime_valid_points_count = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "global_valid_points_count"))
            {
                try { out.global_valid_points_count = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "runtime_has_fit_line"))
            {
                out.runtime_has_fit_line = ParseJsonBoolValue(JsonLineValue(line));
            }
            else if (JsonLineHasKey(line, "global_has_fit_line"))
            {
                out.global_has_fit_line = ParseJsonBoolValue(JsonLineValue(line));
            }
            else if (JsonLineHasKey(line, "runtime_has_fit_circle"))
            {
                out.runtime_has_fit_circle = ParseJsonBoolValue(JsonLineValue(line));
            }
            else if (JsonLineHasKey(line, "global_has_fit_circle"))
            {
                out.global_has_fit_circle = ParseJsonBoolValue(JsonLineValue(line));
            }
            else if (JsonLineHasKey(line, "runtime_global_valid_points_count_mismatch"))
            {
                out.runtime_global_valid_points_count_mismatch =
                    ParseJsonBoolValue(JsonLineValue(line));
            }
            else if (JsonLineHasKey(line, "runtime_global_has_fit_line_mismatch"))
            {
                out.runtime_global_has_fit_line_mismatch =
                    ParseJsonBoolValue(JsonLineValue(line));
            }
            else if (JsonLineHasKey(line, "runtime_global_has_fit_circle_mismatch"))
            {
                out.runtime_global_has_fit_circle_mismatch =
                    ParseJsonBoolValue(JsonLineValue(line));
            }
            else if (JsonLineHasKey(line, "runtime_global_result_mismatch"))
            {
                out.runtime_global_result_mismatch =
                    JsonLineValue(line) == "true" || JsonLineValue(line) == "1";
            }
            else if (JsonLineHasAnyKey(line, {"valid_points_count", "valid_line_points_count", "valid_circle_points_count"}))
            {
                try { out.valid_points_count = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "algorithm_executed"))
            {
                out.algorithm_executed = ParseJsonBoolValue(JsonLineValue(line));
            }
            else if (JsonLineHasKey(line, "budget_exceeded"))
            {
                out.budget_exceeded = ParseJsonBoolValue(JsonLineValue(line));
            }
            else if (JsonLineHasKey(line, "rendered_measure_points_count"))
            {
                try { out.rendered_measure_points_count = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasKey(line, "rendered_result_count"))
            {
                try { out.rendered_result_count = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasKey(line, "result_overlay_changed_pixels"))
            {
                try { out.result_overlay_changed_pixels = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasKey(line, "torch_ok"))
            {
                out.torch_ok = ParseJsonBoolValue(JsonLineValue(line)) ? 1 : 0;
                try { out.torch_ok = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasKey(line, "torch_result_count"))
            {
                try { out.torch_result_count = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasKey(line, "scan_rows_examined"))
            {
                try { out.scan_rows_examined = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasKey(line, "scan_rows_with_foreground"))
            {
                try { out.scan_rows_with_foreground = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasKey(line, "scan_runs_total"))
            {
                try { out.scan_runs_total = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasKey(line, "scan_runs_within_length_limit"))
            {
                try { out.scan_runs_within_length_limit = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasKey(line, "scan_runs_over_length_limit"))
            {
                try { out.scan_runs_over_length_limit = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasKey(line, "scan_runs_rejected_by_selection"))
            {
                try { out.scan_runs_rejected_by_selection = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasKey(line, "scan_runs_rejected_near_endpoint"))
            {
                try { out.scan_runs_rejected_near_endpoint = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasKey(line, "scan_points_emitted"))
            {
                try { out.scan_points_emitted = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasKey(line, "findobject_strategy_id"))
            {
                try { out.findobject_strategy_id = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasKey(line, "findobject_component_count"))
            {
                try { out.findobject_component_count = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasKey(line, "findobject_component_accepted_count"))
            {
                try { out.findobject_component_accepted_count = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasKey(line, "findobject_component_rejected_count"))
            {
                try { out.findobject_component_rejected_count = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasKey(line, "findobject_foreground_before"))
            {
                try { out.findobject_foreground_before = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasKey(line, "findobject_foreground_after"))
            {
                try { out.findobject_foreground_after = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasKey(line, "findobject_algorithm_branch"))
            {
                out.findobject_algorithm_branch = JsonLineValue(line);
            }
            else if (JsonLineHasKey(line, "fit_filter_input_count"))
            {
                try { out.fit_filter_input_count = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasKey(line, "fit_filter_kept_count"))
            {
                try { out.fit_filter_kept_count = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasKey(line, "fit_filter_rejected_count"))
            {
                try { out.fit_filter_rejected_count = std::stoi(JsonLineValue(line)); } catch (...) {}
            }
            else if (JsonLineHasAnyKey(line, {"points_count", "measure_points_count", "line_measure_points_count", "circle_measure_points_count"}))
            {
                try { out.points_count = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasAnyKey(line, {"has_fit_line", "line_has_fit", "fit_line_available"}))
            {
                out.has_fit_line = ParseJsonBoolValue(JsonLineValue(line));
            }
            else if (JsonLineHasAnyKey(line, {"has_fit_circle", "circle_has_fit", "fit_circle_available"}))
            {
                out.has_fit_circle = ParseJsonBoolValue(JsonLineValue(line));
            }
            else if (JsonLineHasKey(line, "local_support"))
            {
                try { out.local_support = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "local_mean_distance"))
            {
                try { out.local_mean_distance = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "fit_offset"))
            {
                try { out.fit_offset = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "boundary_subpixel_offset_mean"))
            {
                try { out.boundary_subpixel_offset_mean = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "boundary_subpixel_offset_stddev"))
            {
                try { out.boundary_subpixel_offset_stddev = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "boundary_localization_sigma_mean_px"))
            {
                try { out.boundary_localization_sigma_mean_px = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "boundary_residual_rmse_px"))
            {
                try { out.boundary_residual_rmse_px = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "boundary_residual_p95_px"))
            {
                try { out.boundary_residual_p95_px = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "boundary_residual_max_px"))
            {
                try { out.boundary_residual_max_px = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "boundary_reliability_score"))
            {
                try { out.boundary_reliability_score = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasAnyKey(line, {"actual_policy_guard", "line_policy_guard_status", "circle_policy_guard_status", "policy_guard_status", "policy_guard"}))
            {
                out.actual_policy_guard = JsonLineValue(line);
                out.policy_guard = out.actual_policy_guard;
            }
            else if (JsonLineHasKey(line, "result_status"))
            {
                out.result_status = JsonLineValue(line);
            }
            else if (JsonLineHasKey(line, "failure_stage"))
            {
                out.failure_stage = JsonLineValue(line);
            }
            else if (JsonLineHasKey(line, "contract_pass"))
            {
                out.contract_pass = ParseJsonBoolValue(JsonLineValue(line));
            }
            else if (JsonLineHasKey(line, "contract_status"))
            {
                out.contract_status = JsonLineValue(line);
            }
            else if (JsonLineHasKey(line, "contract_conclusion"))
            {
                out.contract_conclusion = JsonLineValue(line);
                out.conclusion = JsonLineValue(line);
            }
            else if (JsonLineHasAnyKey(line, {"policy_guard", "actual_policy_guard"}))
            {
                out.policy_guard = JsonLineValue(line);
                out.actual_policy_guard = JsonLineValue(line);
            }
            else if (JsonLineHasAnyKey(line, {"circle_radius", "fit_circle_radius"}))
            {
                try { out.circle_radius = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasAnyKey(line, {"avgdist", "circle_avgdist", "line_avgdist"}))
            {
                try { out.avgdist = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "roi_x0"))
            {
                try { out.roi_x0 = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "roi_y0"))
            {
                try { out.roi_y0 = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "roi_x1"))
            {
                try { out.roi_x1 = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "roi_y1"))
            {
                try { out.roi_y1 = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "circle_cx"))
            {
                try { out.circle_cx = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "circle_cy"))
            {
                try { out.circle_cy = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "circle_px"))
            {
                try { out.circle_px = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "circle_py"))
            {
                try { out.circle_py = std::stoi(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "fit_line_x0"))
            {
                try { out.fit_line_x0 = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "fit_line_y0"))
            {
                try { out.fit_line_y0 = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "fit_line_x1"))
            {
                try { out.fit_line_x1 = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "fit_line_y1"))
            {
                try { out.fit_line_y1 = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "circle_center_x"))
            {
                try { out.circle_center_x = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
            else if (JsonLineHasKey(line, "circle_center_y"))
            {
                try { out.circle_center_y = std::stod(JsonLineValue(line)); }
                catch (...) {}
            }
        }

        std::cout << "[DEBUG] Loaded metrics: points=" << out.points_count 
                  << ", valid=" << out.valid_points_count
                  << ", has_fit_circle=" << out.has_fit_circle
                  << ", circle_radius=" << out.circle_radius << "\n";
    }

    struct ResolvedEvidenceCase
    {
        const CxScriptCatalogEntry* script = nullptr;
        const CxScriptImageManifestEntry* image = nullptr;
        const CxScriptImageTargetRoi* target = nullptr;
        const CxParameterProfile* profile = nullptr;
        std::string contract_path;
    };

    bool ResolveEvidenceCase(
        const CxScriptSuiteCase& suite_case,
        const CxScriptCatalogRuntime& catalog,
        const CxScriptImageManifestRuntime& manifest,
        const CxParameterProfileRuntime& profiles,
        ResolvedEvidenceCase& out,
        std::string& reason)
    {
        out.script = FindCatalogScriptById(catalog, suite_case.script_id);
        if (!out.script)
        {
            reason = "MISSING_SCRIPT: " + suite_case.script_id;
            return false;
        }

        out.image = FindImageById(manifest, suite_case.image_id);
        if (!out.image)
        {
            reason = "MISSING_IMAGE: " + suite_case.image_id;
            return false;
        }

        if (!suite_case.target_id.empty())
        {
            out.target = FindTargetRoiByImageAndTargetId(manifest, suite_case.image_id, suite_case.target_id);
            if (!out.target)
            {
                reason = "MISSING_TARGET: " + suite_case.target_id + " for image " + suite_case.image_id;
                return false;
            }
        }

        if (!suite_case.parameter_profile_id.empty())
        {
            out.profile = profiles.FindProfile(suite_case.parameter_profile_id);
            if (!out.profile)
            {
                reason = "MISSING_PARAMETER_PROFILE: " + suite_case.parameter_profile_id;
                return false;
            }
        }

        out.contract_path = out.script->contract_path;

        if (out.target)
        {
            if (out.target->tool == "FindCircle")
            {
                if (out.target->cx == 0 || out.target->cy == 0 || out.target->px == 0 || out.target->py == 0)
                {
                    reason = "INVALID_ROI: circle ROI coordinates are zero";
                    return false;
                }
            }
            else if (out.target->tool == "FindLine")
            {
                if (out.target->x0 == 0 && out.target->y0 == 0 && out.target->x1 == 0 && out.target->y1 == 0)
                {
                    reason = "INVALID_ROI: line ROI coordinates are zero";
                    return false;
                }
            }
        }

        reason = "OK";
        return true;
    }

    void InjectParameterGlobals(
        CxScriptHeadlessOptions& headless,
        const CxParameterProfile& profile)
    {
        if (profile.has_method)
            headless.method = profile.method;
        if (profile.has_threshold)
            headless.threshold = profile.threshold;
        if (profile.has_gap)
            headless.gap = profile.gap;
        if (profile.has_linegap)
            headless.linegap = profile.linegap;
        if (profile.has_min_edge_run_width_px)
            headless.min_edge_run_width_px =
                profile.min_edge_run_width_px;
        if (profile.has_wgap)
            headless.wgap = profile.wgap;
        if (profile.has_hgap)
            headless.hgap = profile.hgap;
        if (profile.has_filterprofile)
            headless.filterprofile = profile.filterprofile;
    }

    void WriteCaseTrace(
        const std::filesystem::path& caseDir,
        const CxScriptSuiteCase& suite_case,
        const ResolvedEvidenceCase& resolved)
    {
        std::filesystem::path tracePath = caseDir / "case_trace.txt";
        bool exists = std::filesystem::exists(tracePath);
        std::ofstream traceFile(tracePath, exists ? std::ios::app : std::ios::out);
        if (traceFile.is_open())
        {
            std::time_t now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm now_tm;
            localtime_s(&now_tm, &now_c);
            char time_buf[64];
            std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &now_tm);

            traceFile << "Case Trace\n";
            traceFile << "==========\n\n";
            traceFile << "started_at: " << time_buf << "\n";
            traceFile << "case_id: " << suite_case.case_id << "\n";
            traceFile << "image_id: " << suite_case.image_id << "\n";
            traceFile << "target_id: " << suite_case.target_id << "\n";
            traceFile << "script_id: " << suite_case.script_id << "\n";
            traceFile << "parameter_profile_id: " << suite_case.parameter_profile_id << "\n";
            traceFile << "expected_result: " << suite_case.expected_result << "\n";
            traceFile << "expected_policy_guard: " << suite_case.expected_policy_guard << "\n\n";

            if (resolved.image)
                traceFile << "Image: " << resolved.image->path << "\n";
            if (resolved.script)
                traceFile << "Script: " << resolved.script->path << "\n";
            if (resolved.contract_path.empty())
                traceFile << "Contract: NOT FOUND\n";
            else
                traceFile << "Contract: " << resolved.contract_path << "\n";
            if (resolved.profile)
            {
                traceFile << "\nParameter Profile:\n";
                traceFile << "  profile_id: " << resolved.profile->profile_id << "\n";
                traceFile << "  method: " << resolved.profile->method << "\n";
                traceFile << "  threshold: " << resolved.profile->threshold << "\n";
                traceFile << "  gap: " << resolved.profile->gap << "\n";
                traceFile << "  linegap: " << resolved.profile->linegap << "\n";
                traceFile << "  wgap: " << resolved.profile->wgap << "\n";
                traceFile << "  hgap: " << resolved.profile->hgap << "\n";
                traceFile << "  filterprofile: " << resolved.profile->filterprofile << "\n";
            }
            if (resolved.target)
            {
                traceFile << "\nTarget ROI:\n";
                traceFile << "  tool: " << resolved.target->tool << "\n";
                traceFile << "  x0,y0,x1,y1: " << resolved.target->x0 << "," << resolved.target->y0 << "," << resolved.target->x1 << "," << resolved.target->y1 << "\n";
                traceFile << "  cx,cy,px,py: " << resolved.target->cx << "," << resolved.target->cy << "," << resolved.target->px << "," << resolved.target->py << "\n";
            }

            traceFile << "\nPhase Timeline:\n";
            traceFile << "---------------\n";
        }
    }

    void AppendPhaseTrace(
        const std::filesystem::path& caseDir,
        const std::string& phase,
        const std::string& event_type,
        const std::string& message,
        long long elapsed_ms)
    {
        std::ofstream traceFile(caseDir / "case_trace.txt", std::ios::app);
        if (traceFile.is_open())
        {
            std::time_t now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            std::tm now_tm;
            localtime_s(&now_tm, &now_c);
            char time_buf[64];
            std::strftime(time_buf, sizeof(time_buf), "%H:%M:%S", &now_tm);

            if (event_type == "begin")
            {
                traceFile << "[" << time_buf << "] " << phase << " BEGIN\n";
            }
            else if (event_type == "end")
            {
                traceFile << "[" << time_buf << "] " << phase << " END   elapsed_ms=" << elapsed_ms << "\n";
            }
            else if (event_type == "progress")
            {
                traceFile << "[" << time_buf << "] " << phase << " PROGRESS " << message << "\n";
            }
            else if (event_type == "abort")
            {
                traceFile << "[" << time_buf << "] " << phase << " ABORT  " << message << " elapsed_ms=" << elapsed_ms << "\n";
            }
            else
            {
                traceFile << "[" << time_buf << "] " << phase << " " << event_type << " " << message << "\n";
            }
        }
    }

    void ExportRoiPreview(
        const std::string& imagePath,
        const std::filesystem::path& caseDir,
        const CxScriptSuiteCase& suite_case,
        const ResolvedEvidenceCase& resolved)
    {
        cv::Mat img = cv::imread(imagePath);
        if (img.empty())
            return;

        cv::Mat preview = img.clone();

        if (resolved.target)
        {
            if (resolved.target->tool == "FindCircle")
            {
                const double roiRadius = std::hypot(
                    static_cast<double>(resolved.target->px - resolved.target->cx),
                    static_cast<double>(resolved.target->py - resolved.target->cy));

                if (roiRadius > 1.0)
                {
                    cv::circle(
                        preview,
                        cv::Point(resolved.target->cx, resolved.target->cy),
                        static_cast<int>(std::round(roiRadius)),
                        cv::Scalar(0, 255, 0),
                        2,
                        cv::LINE_AA);
                }
            }
            else if (resolved.target->tool == "FindLine")
            {
                cv::line(
                    preview,
                    cv::Point(resolved.target->x0, resolved.target->y0),
                    cv::Point(resolved.target->x1, resolved.target->y1),
                    cv::Scalar(0, 255, 0),
                    2,
                    cv::LINE_AA);
            }
            else if (resolved.target->tool == "FindSegmentation")
            {
                cv::rectangle(
                    preview,
                    cv::Point(resolved.target->x0, resolved.target->y0),
                    cv::Point(resolved.target->x1, resolved.target->y1),
                    cv::Scalar(0, 255, 0),
                    2,
                    cv::LINE_AA);
            }
        }

        int y = 30;
        cv::putText(preview, "image_id: " + suite_case.image_id, {20, y},
            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
        y += 25;
        cv::putText(preview, "target_id: " + suite_case.target_id, {20, y},
            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
        y += 25;
        cv::putText(preview, "tool: " + (resolved.script ? resolved.script->tool : "unknown"), {20, y},
            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
        y += 25;
        cv::putText(preview, "param: " + suite_case.parameter_profile_id, {20, y},
            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);
        y += 25;
        cv::putText(preview, "script: " + suite_case.script_id, {20, y},
            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 255, 255), 2);

        cv::imwrite((caseDir / "roi_preview.png").string(), preview);
    }

    void WriteEvidencePacket(
        const std::filesystem::path& caseDir,
        const CxScriptSuiteCase& suite_case,
        const ResolvedEvidenceCase& resolved,
        const CxScriptSuiteCaseResult& result)
    {
        std::ofstream packetFile(caseDir / "evidence_packet.json");
        if (!packetFile.is_open())
            return;

        packetFile << "{\n";
        packetFile << "  \"evidence_id\": \"" << suite_case.case_id << "\",\n";
        packetFile << "  \"case_id\": \"" << suite_case.case_id << "\",\n";
        packetFile << "\n";
        packetFile << "  \"image\": {\n";
        packetFile << "    \"image_id\": \"" << suite_case.image_id << "\",\n";
        if (resolved.image)
            packetFile << "    \"path\": \"" << resolved.image->path << "\",\n";
        else
            packetFile << "    \"path\": \"\",\n";
        packetFile << "    \"level\": \"" << suite_case.level << "\"\n";
        packetFile << "  },\n";
        packetFile << "\n";
        packetFile << "  \"target\": {\n";
        packetFile << "    \"target_id\": \"" << suite_case.target_id << "\",\n";
        if (resolved.target)
        {
            packetFile << "    \"tool\": \"" << resolved.target->tool << "\",\n";
            packetFile << "    \"cx\": " << resolved.target->cx << ",\n";
            packetFile << "    \"cy\": " << resolved.target->cy << ",\n";
            packetFile << "    \"px\": " << resolved.target->px << ",\n";
            packetFile << "    \"py\": " << resolved.target->py << ",\n";
            packetFile << "    \"roi_valid\": true\n";
        }
        else
        {
            packetFile << "    \"tool\": \"\",\n";
            packetFile << "    \"cx\": 0,\n";
            packetFile << "    \"cy\": 0,\n";
            packetFile << "    \"px\": 0,\n";
            packetFile << "    \"py\": 0,\n";
            packetFile << "    \"roi_valid\": false\n";
        }
        packetFile << "  },\n";
        packetFile << "\n";
        packetFile << "  \"script\": {\n";
        packetFile << "    \"script_id\": \"" << suite_case.script_id << "\",\n";
        if (resolved.script)
            packetFile << "    \"path\": \"" << resolved.script->path << "\",\n";
        else
            packetFile << "    \"path\": \"\",\n";
        packetFile << "    \"expected_result\": \"" << suite_case.expected_result << "\"\n";
        packetFile << "  },\n";
        packetFile << "\n";
        packetFile << "  \"parameter\": {\n";
        packetFile << "    \"profile_id\": \"" << suite_case.parameter_profile_id << "\",\n";
        packetFile << "    \"method\": " << result.effective_method << ",\n";
        packetFile << "    \"threshold\": " << result.effective_threshold << ",\n";
        packetFile << "    \"gap\": " << result.effective_gap << ",\n";
        packetFile << "    \"linegap\": " << result.effective_linegap << ",\n";
        packetFile << "    \"min_edge_run_width_px\": "
                   << result.effective_min_edge_run_width_px << ",\n";
        packetFile << "    \"wgap\": " << result.effective_wgap << ",\n";
        packetFile << "    \"hgap\": " << result.effective_hgap << ",\n";
        packetFile << "    \"tool_half_width\": " << result.effective_tool_half_width << ",\n";
        packetFile << "    \"filterprofile\": " << result.effective_filterprofile << ",\n";
        packetFile << "    \"source\": \"effective_merged_snapshot\"\n";
        packetFile << "  },\n";
        packetFile << "\n";
        packetFile << "  \"contract\": {\n";
        packetFile << "    \"path\": \"" << resolved.contract_path << "\",\n";
        packetFile << "    \"expected_policy_guard\": \"" << suite_case.expected_policy_guard << "\"\n";
        packetFile << "  },\n";
        packetFile << "\n";
        packetFile << "  \"result\": {\n";
        packetFile << "    \"valid_points_count\": " << result.valid_points_count << ",\n";
        packetFile << "    \"has_fit_circle\": " << (result.has_fit_circle ? "true" : "false") << ",\n";
        packetFile << "    \"has_fit_line\": " << (result.has_fit_line ? "true" : "false") << ",\n";
        packetFile << "    \"contract_pass\": " << (result.contract_pass ? "true" : "false") << ",\n";
        packetFile << "    \"failure_stage\": \"" << result.failure_stage << "\",\n";
        packetFile << "    \"conclusion\": \"" << result.conclusion << "\"\n";
        packetFile << "  }\n";
        packetFile << "}\n";
    }

    void CopyScriptSnapshot(
        const std::filesystem::path& scriptPath,
        const std::filesystem::path& caseDir)
    {
        std::filesystem::path snapshotPath = caseDir / "script_snapshot.cxsc";
        try
        {
            if (std::filesystem::exists(scriptPath))
            {
                std::filesystem::copy_file(
                    scriptPath,
                    snapshotPath,
                    std::filesystem::copy_options::overwrite_existing);
            }
        }
        catch (...) {}
    }

    void WriteHumanReview(
        const std::string& case_id,
        const std::string& decision,
        const std::string& note,
        const std::string& next_action,
        const std::filesystem::path& caseDir)
    {
        std::ofstream file(caseDir / "human_review.json");
        if (!file.is_open())
            return;

        std::time_t now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm now_tm;
        localtime_s(&now_tm, &now_c);
        char time_buf[64];
        std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S", &now_tm);

        file << "{\n";
        file << "  \"review_id\": \"" << case_id << "_promotion\",\n";
        file << "  \"case_id\": \"" << case_id << "\",\n";
        file << "  \"decision\": \"" << decision << "\",\n";
        file << "  \"reviewer\": \"manual\",\n";
        file << "  \"timestamp\": \"" << time_buf << "\",\n";
        file << "  \"note\": \"" << note << "\",\n";
        file << "  \"next_action\": \"" << next_action << "\"\n";
        file << "}\n";
    }

    void WriteFinalCaseSummary(
        const CxScriptSuiteCaseResult& r,
        const std::filesystem::path& caseDir)
    {
        std::ofstream file(caseDir / "result_summary.json");
        if (!file.is_open())
            return;

        file << "{\n";
        file << "  \"case_id\": \"" << JsonEscape(r.case_id) << "\",\n";
        file << "  \"evidence_id\": \"" << JsonEscape(r.evidence_id) << "\",\n";
        file << "  \"image_id\": \"" << JsonEscape(r.image_id) << "\",\n";
        file << "  \"target_id\": \"" << JsonEscape(r.target_id) << "\",\n";
        file << "  \"tool\": \"" << JsonEscape(r.tool) << "\",\n";
        file << "  \"script_id\": \"" << JsonEscape(r.script_id) << "\",\n";
        file << "  \"parameter_profile_id\": \"" << JsonEscape(r.parameter_profile_id) << "\",\n";
        file << "\n";
        file << "  \"headless_ok\": " << (r.headless_ok ? "true" : "false") << ",\n";
        file << "  \"contract_pass\": " << (r.contract_pass ? "1" : "0") << ",\n";
        file << "  \"contract_status\": \"" << JsonEscape(r.contract_status) << "\",\n";
        file << "  \"contract_conclusion\": \"" << JsonEscape(r.contract_conclusion) << "\",\n";
        file << "\n";
        file << "  \"valid_points_count\": " << r.valid_points_count << ",\n";
        file << "  \"has_fit_line\": " << (r.has_fit_line ? "true" : "false") << ",\n";
        file << "  \"has_fit_circle\": " << (r.has_fit_circle ? "true" : "false") << ",\n";
        file << "  \"runtime_valid_points_count\": " << r.runtime_valid_points_count << ",\n";
        file << "  \"global_valid_points_count\": " << r.global_valid_points_count << ",\n";
        file << "  \"runtime_has_fit_line\": " << (r.runtime_has_fit_line ? "true" : "false") << ",\n";
        file << "  \"global_has_fit_line\": " << (r.global_has_fit_line ? "true" : "false") << ",\n";
        file << "  \"runtime_has_fit_circle\": " << (r.runtime_has_fit_circle ? "true" : "false") << ",\n";
        file << "  \"global_has_fit_circle\": " << (r.global_has_fit_circle ? "true" : "false") << ",\n";
        file << "  \"runtime_global_valid_points_count_mismatch\": " << (r.runtime_global_valid_points_count_mismatch ? "true" : "false") << ",\n";
        file << "  \"runtime_global_has_fit_line_mismatch\": " << (r.runtime_global_has_fit_line_mismatch ? "true" : "false") << ",\n";
        file << "  \"runtime_global_has_fit_circle_mismatch\": " << (r.runtime_global_has_fit_circle_mismatch ? "true" : "false") << ",\n";
        file << "  \"runtime_global_result_mismatch\": " << (r.runtime_global_result_mismatch ? "true" : "false") << ",\n";
        file << "  \"circle_radius\": " << r.circle_radius << ",\n";
        file << "  \"avgdist\": " << r.avgdist << ",\n";
        file << "  \"local_support\": " << r.local_support << ",\n";
        file << "  \"local_mean_distance\": " << r.local_mean_distance << ",\n";
        file << "  \"fit_offset\": " << r.fit_offset << ",\n";
        const CxPrecisionGateSnapshot precision = EvaluateCxPrecisionGate(r);
        file << "  \"precision_metric_family\": \"" << JsonEscape(precision.metric_family) << "\",\n";
        file << "  \"precision_gate_status\": \"" << JsonEscape(precision.status) << "\",\n";
        file << "  \"precision_gate_evaluated\": " << (precision.evaluated ? "true" : "false") << ",\n";
        file << "  \"precision_residual_px\": " << precision.residual_px << ",\n";
        file << "  \"precision_residual_limit_px\": " << precision.residual_limit_px << ",\n";
        file << "  \"precision_residual_pass\": " << (precision.residual_pass ? "true" : "false") << ",\n";
        file << "  \"precision_support\": " << precision.support << ",\n";
        file << "  \"precision_support_limit\": " << precision.support_limit << ",\n";
        file << "  \"precision_failure_reason\": \"" << JsonEscape(precision.reason) << "\",\n";
        file << "  \"scan_rows_examined\": " << r.scan_rows_examined << ",\n";
        file << "  \"scan_rows_with_foreground\": " << r.scan_rows_with_foreground << ",\n";
        file << "  \"scan_runs_total\": " << r.scan_runs_total << ",\n";
        file << "  \"scan_runs_within_length_limit\": " << r.scan_runs_within_length_limit << ",\n";
        file << "  \"scan_runs_over_length_limit\": " << r.scan_runs_over_length_limit << ",\n";
        file << "  \"scan_runs_rejected_by_selection\": " << r.scan_runs_rejected_by_selection << ",\n";
        file << "  \"scan_runs_rejected_near_endpoint\": " << r.scan_runs_rejected_near_endpoint << ",\n";
        file << "  \"scan_points_emitted\": " << r.scan_points_emitted << ",\n";
        file << "  \"findobject_strategy_id\": " << r.findobject_strategy_id << ",\n";
        file << "  \"findobject_algorithm_branch\": \"" << JsonEscape(r.findobject_algorithm_branch) << "\",\n";
        file << "  \"findobject_component_count\": " << r.findobject_component_count << ",\n";
        file << "  \"findobject_component_accepted_count\": " << r.findobject_component_accepted_count << ",\n";
        file << "  \"findobject_component_rejected_count\": " << r.findobject_component_rejected_count << ",\n";
        file << "  \"findobject_foreground_before\": " << r.findobject_foreground_before << ",\n";
        file << "  \"findobject_foreground_after\": " << r.findobject_foreground_after << ",\n";
        file << "  \"fit_filter_input_count\": " << r.fit_filter_input_count << ",\n";
        file << "  \"fit_filter_kept_count\": " << r.fit_filter_kept_count << ",\n";
        file << "  \"fit_filter_rejected_count\": " << r.fit_filter_rejected_count << ",\n";
        file << "\n";
        file << "  \"policy_guard\": \"" << JsonEscape(r.policy_guard) << "\",\n";
        file << "  \"gauge_source\": \"" << JsonEscape(r.gauge_source) << "\",\n";
        file << "  \"gauge_review_status\": \"" << JsonEscape(r.gauge_review_status) << "\",\n";
        file << "  \"gauge_annotation_path\": \"" << JsonEscape(r.gauge_annotation_path) << "\",\n";
        file << "  \"actual_policy_guard\": \"" << JsonEscape(r.actual_policy_guard) << "\",\n";
        file << "  \"result_status\": \"" << JsonEscape(r.result_status) << "\",\n";
        file << "  \"failure_stage\": \"" << JsonEscape(r.failure_stage) << "\",\n";
        file << "  \"conclusion\": \"" << JsonEscape(r.conclusion) << "\",\n";
        file << "\n";
        file << "  \"snapshot_path\": \"" << JsonEscape(r.snapshot_path) << "\",\n";
        file << "  \"summary_path\": \"" << JsonEscape(r.summary_path) << "\",\n";
        file << "  \"result_overlay_path\": \"" << JsonEscape(r.result_overlay_path) << "\",\n";
        file << "  \"evidence_overlay_path\": \"" << JsonEscape(r.evidence_overlay_path) << "\",\n";
        file << "  \"tool_display_path\": \"" << JsonEscape(r.tool_display_path) << "\",\n";
        file << "\n";
        file << "  \"roi_x0\": " << r.roi_x0 << ",\n";
        file << "  \"roi_y0\": " << r.roi_y0 << ",\n";
        file << "  \"roi_x1\": " << r.roi_x1 << ",\n";
        file << "  \"roi_y1\": " << r.roi_y1 << ",\n";
        file << "  \"circle_cx\": " << r.circle_cx << ",\n";
        file << "  \"circle_cy\": " << r.circle_cy << ",\n";
        file << "  \"circle_px\": " << r.circle_px << ",\n";
        file << "  \"circle_py\": " << r.circle_py << ",\n";
        file << "  \"effective_tool_half_width\": " << r.effective_tool_half_width << ",\n";
        file << "  \"effective_wgap\": " << r.effective_wgap << ",\n";
        file << "  \"effective_hgap\": " << r.effective_hgap << ",\n";
        file << "  \"effective_gap\": " << r.effective_gap << ",\n";
        file << "  \"effective_linegap\": " << r.effective_linegap << ",\n";
        file << "  \"effective_min_edge_run_width_px\": "
             << r.effective_min_edge_run_width_px << ",\n";
        file << "  \"effective_threshold\": " << r.effective_threshold << ",\n";
        file << "  \"effective_filterprofile\": " << r.effective_filterprofile << ",\n";
        file << "  \"effective_method\": " << r.effective_method << "\n";
        file << "}\n";
    }

    void WriteReplayPackage(
        const std::filesystem::path& caseDir,
        const CxScriptSuiteCase& suite_case,
        const ResolvedEvidenceCase& resolved,
        const CxScriptSuiteCaseResult& result)
    {
        std::ofstream file(caseDir / "replay_package.json");
        if (!file.is_open())
            return;

        file << "{\n";
        file << "  \"replay_version\": \"stage25-replay-v1\",\n";

        std::time_t now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm now_tm;
        localtime_s(&now_tm, &now_c);
        std::stringstream ss;
        ss << std::put_time(&now_tm, "%Y-%m-%dT%H:%M:%S");
        file << "  \"created_at\": \"" << ss.str() << "\",\n";

        file << "\n";
        file << "  \"case_id\": \"" << suite_case.case_id << "\",\n";
        file << "  \"evidence_id\": \"" << suite_case.case_id << "\",\n";

        file << "\n";
        file << "  \"image\": {\n";
        file << "    \"image_id\": \"" << suite_case.image_id << "\",\n";
        if (resolved.image)
            file << "    \"path\": \"" << resolved.image->path << "\"\n";
        else
            file << "    \"path\": \"\"\n";
        file << "  },\n";

        file << "\n";
        file << "  \"target\": {\n";
        file << "    \"target_id\": \"" << suite_case.target_id << "\",\n";
        if (resolved.target)
        {
            file << "    \"tool\": \"" << resolved.target->tool << "\",\n";
            file << "    \"circle_cx\": " << resolved.target->cx << ",\n";
            file << "    \"circle_cy\": " << resolved.target->cy << ",\n";
            file << "    \"circle_px\": " << resolved.target->px << ",\n";
            file << "    \"circle_py\": " << resolved.target->py << ",\n";
            file << "    \"roi_x0\": " << resolved.target->x0 << ",\n";
            file << "    \"roi_y0\": " << resolved.target->y0 << ",\n";
            file << "    \"roi_x1\": " << resolved.target->x1 << ",\n";
            file << "    \"roi_y1\": " << resolved.target->y1 << ",\n";
            file << "    \"tool_half_width\": " << resolved.target->tool_half_width << "\n";
        }
        else
        {
            file << "    \"tool\": \"\",\n";
            file << "    \"circle_cx\": 0,\n";
            file << "    \"circle_cy\": 0,\n";
            file << "    \"circle_px\": 0,\n";
            file << "    \"circle_py\": 0,\n";
            file << "    \"roi_x0\": 0,\n";
            file << "    \"roi_y0\": 0,\n";
            file << "    \"roi_x1\": 0,\n";
            file << "    \"roi_y1\": 0,\n";
            file << "    \"tool_half_width\": 0\n";
        }
        file << "  },\n";

        file << "\n";
        file << "  \"effective_gauge\": {\n";
        file << "    \"source\": \"" << JsonEscape(result.gauge_source.empty() ? "suite_effective_globals" : result.gauge_source) << "\",\n";
        file << "    \"review_status\": \"" << JsonEscape(result.gauge_review_status) << "\",\n";
        file << "    \"annotation_path\": \"" << JsonEscape(result.gauge_annotation_path) << "\",\n";
        file << "    \"roi_x0\": " << result.roi_x0 << ",\n";
        file << "    \"roi_y0\": " << result.roi_y0 << ",\n";
        file << "    \"roi_x1\": " << result.roi_x1 << ",\n";
        file << "    \"roi_y1\": " << result.roi_y1 << ",\n";
        file << "    \"circle_cx\": " << result.circle_cx << ",\n";
        file << "    \"circle_cy\": " << result.circle_cy << ",\n";
        file << "    \"circle_px\": " << result.circle_px << ",\n";
        file << "    \"circle_py\": " << result.circle_py << ",\n";
        file << "    \"tool_half_width\": " << result.effective_tool_half_width << ",\n";
        file << "    \"wgap\": " << result.effective_wgap << ",\n";
        file << "    \"hgap\": " << result.effective_hgap << ",\n";
        file << "    \"gap\": " << result.effective_gap << ",\n";
        file << "    \"linegap\": " << result.effective_linegap << ",\n";
        file << "    \"threshold\": " << result.effective_threshold << ",\n";
        file << "    \"filterprofile\": " << result.effective_filterprofile << ",\n";
        file << "    \"method\": " << result.effective_method << "\n";
        file << "  },\n";

        file << "\n";
        file << "  \"script\": {\n";
        file << "    \"script_id\": \"" << suite_case.script_id << "\",\n";
        if (resolved.script)
            file << "    \"path\": \"" << resolved.script->path << "\",\n";
        else
            file << "    \"path\": \"\",\n";
        file << "    \"snapshot_path\": \"script_snapshot.cxsc\"\n";
        file << "  },\n";

        file << "\n";
        file << "  \"parameter\": {\n";
        file << "    \"profile_id\": \"" << suite_case.parameter_profile_id << "\",\n";
        if (resolved.profile)
        {
            file << "    \"method\": " << resolved.profile->method << ",\n";
            file << "    \"threshold\": " << resolved.profile->threshold << ",\n";
            file << "    \"gap\": " << resolved.profile->gap << ",\n";
            file << "    \"linegap\": " << resolved.profile->linegap << ",\n";
            file << "    \"wgap\": " << resolved.profile->wgap << ",\n";
            file << "    \"hgap\": " << resolved.profile->hgap << ",\n";
            file << "    \"filterprofile\": " << resolved.profile->filterprofile << "\n";
        }
        else
        {
            file << "    \"method\": 0,\n";
            file << "    \"threshold\": 0,\n";
            file << "    \"gap\": 0,\n";
            file << "    \"linegap\": 0,\n";
            file << "    \"wgap\": 0,\n";
            file << "    \"hgap\": 0,\n";
            file << "    \"filterprofile\": 0\n";
        }
        file << "  },\n";

        file << "\n";
        file << "  \"globals\": {\n";
        file << "    \"global_circle_cx\": " << result.circle_cx << ",\n";
        file << "    \"global_circle_cy\": " << result.circle_cy << ",\n";
        file << "    \"global_circle_px\": " << result.circle_px << ",\n";
        file << "    \"global_circle_py\": " << result.circle_py << ",\n";
        file << "    \"global_roi_x0\": " << result.roi_x0 << ",\n";
        file << "    \"global_roi_y0\": " << result.roi_y0 << ",\n";
        file << "    \"global_roi_x1\": " << result.roi_x1 << ",\n";
        file << "    \"global_roi_y1\": " << result.roi_y1 << ",\n";
        file << "    \"global_wgap\": " << result.effective_wgap << ",\n";
        file << "    \"global_hgap\": " << result.effective_hgap << ",\n";
        file << "    \"global_gap\": " << result.effective_gap << ",\n";
        file << "    \"global_linegap\": " << result.effective_linegap << ",\n";
        file << "    \"global_min_edge_run_width_px\": "
             << result.effective_min_edge_run_width_px << ",\n";
        file << "    \"global_tool_half_width\": " << result.effective_tool_half_width << ",\n";
        file << "    \"global_method\": " << result.effective_method << ",\n";
        file << "    \"global_threshold\": " << result.effective_threshold << ",\n";
        file << "    \"global_filterprofile\": " << result.effective_filterprofile << "\n";
        file << "  },\n";

        file << "\n";
        file << "  \"last_status\": {\n";
        file << "    \"phase\": \"evidence_resolved\",\n";
        file << "    \"status\": \"completed\",\n";
        file << "    \"message\": \"Replay package generated before headless\",\n";
        file << "    \"elapsed_ms\": 0\n";
        file << "  },\n";

        file << "\n";
        file << "  \"artifacts\": {\n";
        file << "    \"roi_preview\": \"roi_preview.png\",\n";
        file << "    \"live_status\": \"live_status.json\",\n";
        file << "    \"run_trace\": \"run_trace.jsonl\",\n";
        file << "    \"cxparser_ext_trace\": \"cxparser_ext_trace.json\"\n";
        file << "  }\n";
        file << "}\n";
    }

    void EvaluateSuiteCaseContract(CxScriptSuiteCaseResult& r)
    {
        if (!r.headless_ok)
        {
            r.contract_pass = false;
            r.conclusion = "Headless execution failed";
            return;
        }

        if (r.contract_path.empty())
        {
            if (r.expected_result == "diagnostic")
            {
                r.contract_pass = true;
                r.conclusion = "Diagnostic script executed";
            }
            else
            {
                r.contract_pass = false;
                r.contract_status = "missing_contract";
                r.conclusion = "Missing cxscript contract; C++ does not judge OK/NG";
            }
            return;
        }

        const std::filesystem::path contractDir =
            std::filesystem::path(r.case_dir) / "contract";
        std::filesystem::create_directories(contractDir);

        CxScriptHeadlessOptions contractHeadless;
        contractHeadless.enabled = true;
        contractHeadless.image_path = r.image_path;
        contractHeadless.script_path = r.contract_path;
        contractHeadless.output_dir = contractDir.string();
        contractHeadless.case_name = r.case_id + "_contract";
        contractHeadless.save_overlay = false;
        contractHeadless.timeout_sec = 10;
        contractHeadless.summary_path =
            (contractDir / "contract_summary.json").string();
        contractHeadless.snapshot_path =
            (contractDir / "contract_snapshot.txt").string();

        contractHeadless.stage25_image_id = r.image_id;
        contractHeadless.stage25_level = r.level;
        contractHeadless.stage25_target_id = r.target_id;
        contractHeadless.stage25_tool = r.tool;

        contractHeadless.contract_context_enabled = true;
        contractHeadless.contract_headless_ok = r.headless_ok ? 1 : 0;
        contractHeadless.contract_pass_initial = 0;
        contractHeadless.contract_algorithm_executed = r.algorithm_executed ? 1 : 0;
        contractHeadless.contract_budget_exceeded = r.budget_exceeded ? 1 : 0;
        contractHeadless.contract_rendered_measure_points_count = r.rendered_measure_points_count;
        contractHeadless.contract_rendered_result_count = r.rendered_result_count;
        contractHeadless.contract_result_overlay_changed_pixels = r.result_overlay_changed_pixels;
        contractHeadless.contract_torch_ok = r.torch_ok;
        contractHeadless.contract_torch_result_count = r.torch_result_count;
        contractHeadless.points_count = r.points_count;
        contractHeadless.valid_points_count = r.valid_points_count;
        contractHeadless.has_fit_line = r.has_fit_line ? 1 : 0;
        contractHeadless.has_fit_circle = r.has_fit_circle ? 1 : 0;
        contractHeadless.runtime_valid_points_count = r.runtime_valid_points_count;
        contractHeadless.global_valid_points_count = r.global_valid_points_count;
        contractHeadless.runtime_has_fit_line = r.runtime_has_fit_line ? 1 : 0;
        contractHeadless.global_has_fit_line = r.global_has_fit_line ? 1 : 0;
        contractHeadless.runtime_has_fit_circle = r.runtime_has_fit_circle ? 1 : 0;
        contractHeadless.global_has_fit_circle = r.global_has_fit_circle ? 1 : 0;
        contractHeadless.runtime_global_valid_points_count_mismatch =
            r.runtime_global_valid_points_count_mismatch ? 1 : 0;
        contractHeadless.runtime_global_has_fit_line_mismatch =
            r.runtime_global_has_fit_line_mismatch ? 1 : 0;
        contractHeadless.runtime_global_has_fit_circle_mismatch =
            r.runtime_global_has_fit_circle_mismatch ? 1 : 0;
        contractHeadless.runtime_global_result_mismatch =
            r.runtime_global_result_mismatch ? 1 : 0;
        contractHeadless.local_support = r.local_support;
        contractHeadless.local_mean_distance = r.local_mean_distance;
        contractHeadless.fit_offset = r.fit_offset;
        contractHeadless.circle_radius = r.circle_radius;
        contractHeadless.avgdist = r.avgdist;
        contractHeadless.policy_guard = r.actual_policy_guard;
        contractHeadless.policy_guard_match =
            (r.expected_policy_guard.empty()
                 ? !r.actual_policy_guard.empty()
                 : r.actual_policy_guard == r.expected_policy_guard)
                ? 1
                : 0;
        contractHeadless.result_status = r.result_status;
        contractHeadless.failure_stage = r.failure_stage;
        contractHeadless.result_overlay_path = r.result_overlay_path;
        contractHeadless.evidence_overlay_path = r.evidence_overlay_path;
        contractHeadless.tool_display_path = r.tool_display_path;

        CxScriptHeadlessResult contractResult;
        RunCxScriptHeadless(contractHeadless, contractResult);

        if (!contractResult.ok)
        {
            r.contract_pass = false;
            r.conclusion = "Contract script failed: " + contractResult.reason;
            return;
        }

        if (contractResult.summary_path.empty())
        {
            r.contract_pass = false;
            r.conclusion = "Contract summary file not found";
            return;
        }

        std::ifstream contractFile(contractResult.summary_path);
        if (contractFile.is_open())
        {
            std::string line;
            while (std::getline(contractFile, line))
            {
                if (JsonLineHasKey(line, "contract_pass"))
                {
                    r.contract_pass = ParseJsonBoolValue(JsonLineValue(line));
                }
                else if (JsonLineHasKey(line, "contract_status"))
                {
                    r.contract_status = JsonLineValue(line);
                }
                else if (JsonLineHasKey(line, "contract_conclusion"))
                {
                    r.contract_conclusion = JsonLineValue(line);
                    r.conclusion = JsonLineValue(line);
                }
            }
        }
    }

    void RunSingleSuiteCase(
        const CxScriptSuiteCase& suiteCase,
        const CxScriptCatalogEntry& script,
        const CxScriptImageManifestEntry& image,
        const CxScriptImageManifestRuntime& manifest,
        const CxParameterProfileRuntime& profiles,
        const ResolvedEvidenceCase& resolved,
        const std::filesystem::path& outRoot,
        const CxScriptSuiteRunOptions& options,
        CxScriptSuiteCaseResult& out)
    {
        out.case_id = suiteCase.case_id;
        out.evidence_id = suiteCase.case_id;
        out.script_id = suiteCase.script_id;
        out.script_path = script.path;
        out.image_id = image.image_id;
        out.image_path = image.path;
        out.level = image.level;
        out.target_id = suiteCase.target_id;
        out.tool = script.tool;
        out.parameter_profile_id = suiteCase.parameter_profile_id;
        out.contract_id = script.contract_path;

        CxUnifiedLogContext logContext;
        logContext.case_id = suiteCase.case_id;
        logContext.image_id = suiteCase.image_id;
        logContext.target_id = suiteCase.target_id;
        logContext.script_id = suiteCase.script_id;
        logContext.parameter_profile_id = suiteCase.parameter_profile_id;
        logContext.tool = script.tool;
        CxScopedLogContext scopedLogContext(logContext);

        CXLOG_INFO("CxScriptSuiteRunner", "case_begin", "running", 
                   "case_id=" + suiteCase.case_id);

        out.expected_result =
            !suiteCase.expected_result.empty()
                ? suiteCase.expected_result
                : script.expected_result;

        out.expected_policy_guard =
            !suiteCase.expected_policy_guard.empty()
                ? suiteCase.expected_policy_guard
                : script.expected_policy_guard;

        out.contract_path = resolved.contract_path;

        const std::filesystem::path caseDir =
            outRoot /
            "cases" /
            image.level /
            CompactCaseDirectoryName(suiteCase.case_id);

        std::filesystem::create_directories(caseDir);
        out.case_dir = caseDir.string();

        CxScriptRunTraceRuntime trace;
        std::string traceReason;

        if (options.trace_run)
        {
            std::string sessionId = suiteCase.case_id;
            trace.open(caseDir, sessionId, suiteCase.case_id, traceReason);
            trace.set_case_context(
                suiteCase.case_id,
                suiteCase.image_id,
                suiteCase.target_id,
                suiteCase.script_id,
                suiteCase.parameter_profile_id);
        }

        if (options.trace_run)
            trace.event("resolve_evidence", "end", "Evidence resolved");

        {
            ScopedSuiteTimer timer("roi_preview:" + suiteCase.case_id);
            WriteCaseTrace(caseDir, suiteCase, resolved);
            AppendPhaseTrace(caseDir, "roi_preview", "begin", "", 0);
            if (options.trace_run)
                trace.event("roi_preview", "begin", "Generating ROI preview");
            ExportRoiPreview(image.path, caseDir, suiteCase, resolved);
            out.roi_preview_path = (caseDir / "roi_preview.png").string();
            if (options.trace_run)
                trace.event("roi_preview", "end", "ROI preview generated");
            AppendPhaseTrace(caseDir, "roi_preview", "end", "", timer.elapsed_ms());
        }

        if (StopForHumanReviewIfNeeded(options, out, "roi", "accept_roi or reject_roi"))
        {
            WriteEvidencePacket(caseDir, suiteCase, resolved, out);
            if (options.dump_replay_package)
            {
                CopyScriptSnapshot(script.path, caseDir);
                WriteReplayPackage(caseDir, suiteCase, resolved, out);
            }
            return;
        }

        {
            ScopedSuiteTimer timer("headless:" + suiteCase.case_id);
            AppendPhaseTrace(caseDir, "headless", "begin", "", 0);
            if (options.trace_run)
                trace.event("headless", "begin", "Running cxscript headless");

            if (options.trace_run)
            {
                CxAlgorithmTraceScope::SetCallback(
                    [&](const CxAlgorithmTraceEvent& e)
                    {
                        trace.event(
                            "algorithm." + e.tool + "." + e.phase,
                            e.status,
                            e.message + " scan=" + std::to_string(e.scan_index) +
                            " samples=" + std::to_string(e.sample_count) +
                            " valid=" + std::to_string(e.valid_points) +
                            " algorithm_elapsed_ms=" + std::to_string(e.elapsed_ms));
                    });
            }

            CxScriptHeadlessOptions headless;
            headless.enabled = true;
            headless.image_path = image.path;
            headless.script_path = script.path;
            headless.output_dir = caseDir.string();
            headless.case_name = suiteCase.case_id;
            headless.save_overlay = options.save_overlay;
            headless.enable_evidence_analysis = false;
            headless.timeout_sec = std::max(1, options.case_timeout_sec);

            headless.stage25_image_id = image.image_id;
            headless.stage25_level = image.level;
            headless.stage25_target_id = suiteCase.target_id;
            headless.stage25_tool = script.tool;

            if (resolved.profile)
            {
                InjectParameterGlobals(headless, *resolved.profile);
            }

            if (resolved.target)
            {
                headless.roi_x0 = resolved.target->x0;
                headless.roi_y0 = resolved.target->y0;
                headless.roi_x1 = resolved.target->x1;
                headless.roi_y1 = resolved.target->y1;
                headless.circle_cx = resolved.target->cx;
                headless.circle_cy = resolved.target->cy;
                headless.circle_px = resolved.target->px;
                headless.circle_py = resolved.target->py;
                if (resolved.target->has_wgap)
                    headless.wgap = resolved.target->wgap;
                if (resolved.target->has_hgap)
                    headless.hgap = resolved.target->hgap;
                if (resolved.target->has_gap)
                    headless.gap = resolved.target->gap;
                if (resolved.target->has_linegap)
                    headless.linegap = resolved.target->linegap;
                if (resolved.target->has_min_edge_run_width_px)
                    headless.min_edge_run_width_px =
                        resolved.target->min_edge_run_width_px;
                if (resolved.target->has_tool_half_width)
                    headless.tool_half_width = resolved.target->tool_half_width;
                if (resolved.target->has_threshold)
                    headless.threshold = resolved.target->threshold;

                if (resolved.target->has_method)
                    headless.method = resolved.target->method;
                else if (resolved.target->tool == "FindCircle")
                    headless.method = 0;

                out.roi_x0 = resolved.target->x0;
                out.roi_y0 = resolved.target->y0;
                out.roi_x1 = resolved.target->x1;
                out.roi_y1 = resolved.target->y1;
                out.circle_cx = resolved.target->cx;
                out.circle_cy = resolved.target->cy;
                out.circle_px = resolved.target->px;
                out.circle_py = resolved.target->py;
            }

            if (options.use_manual_gauge)
            {
                const std::filesystem::path annotationPath =
                    options.gauge_annotation_path.empty()
                        ? (caseDir / "gauge_annotation.json")
                        : std::filesystem::path(options.gauge_annotation_path);

                std::string gaugeReason;
                const bool gaugeOk = ApplyManualGaugeAnnotationFile(
                    annotationPath,
                    headless,
                    out,
                    gaugeReason);

                AppendPhaseTrace(
                    caseDir,
                    "manual_gauge",
                    gaugeOk ? "end" : "error",
                    gaugeReason,
                    0);
                if (options.trace_run)
                    trace.event(
                        "manual_gauge",
                        gaugeOk ? "end" : "error",
                        gaugeReason);

                {
                    std::ofstream caseTrace(caseDir / "case_trace.txt", std::ios::app);
                    if (caseTrace.is_open())
                    {
                        caseTrace << "\nManual Gauge:\n";
                        caseTrace << "  enabled: true\n";
                        caseTrace << "  status: " << (gaugeOk ? "loaded" : "error") << "\n";
                        caseTrace << "  annotation: " << annotationPath.string() << "\n";
                        caseTrace << "  reason: " << gaugeReason << "\n";
                        caseTrace << "  line gauge: "
                                  << out.roi_x0 << "," << out.roi_y0
                                  << " -> " << out.roi_x1 << "," << out.roi_y1
                                  << "\n";
                        caseTrace << "  circle gauge: "
                                  << out.circle_cx << "," << out.circle_cy
                                  << " pxpy=" << out.circle_px << "," << out.circle_py
                                  << "\n";
                    }
                }

                if (!gaugeOk)
                {
                    out.headless_ok = false;
                    out.contract_pass = false;
                    out.failure_stage = "manual_gauge_not_available";
                    out.conclusion = gaugeReason;
                    WriteFinalCaseSummary(out, caseDir);
                    if (options.trace_run)
                        trace.close();
                    return;
                }
            }

            out.effective_tool_half_width = headless.tool_half_width;
            out.effective_wgap = headless.wgap;
            out.effective_hgap = headless.hgap;
            out.effective_gap = headless.gap;
            out.effective_linegap = headless.linegap;
            out.effective_min_edge_run_width_px =
                headless.min_edge_run_width_px;
            out.effective_threshold = headless.threshold;
            out.effective_filterprofile = headless.filterprofile;
            out.effective_method = headless.method;

            CxScriptHeadlessResult headlessResult;
            AppendPhaseTrace(caseDir, "headless_call", "begin", "", 0);
            std::cout << "[SUITE] before RunCxScriptHeadless case_id="
                      << suiteCase.case_id << "\n" << std::flush;
            if (options.trace_run)
                trace.event("headless_call", "begin", "before RunCxScriptHeadless");

            auto headlessStartTime = std::chrono::steady_clock::now();
            bool headlessRet = RunCxScriptHeadless(headless, headlessResult);
            auto headlessElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - headlessStartTime).count();

            std::cout << "[SUITE] after RunCxScriptHeadless case_id="
                      << suiteCase.case_id
                      << " ret=" << (headlessRet ? "true" : "false")
                      << " ok=" << (headlessResult.ok ? "true" : "false")
                      << " elapsed_ms=" << headlessElapsed
                      << "\n" << std::flush;
            std::cout << "[SUITE] headless paths snapshot=" << headlessResult.snapshot_path
                      << " summary=" << headlessResult.summary_path
                      << " overlay=" << headlessResult.overlay_path
                      << " exit_code=" << headlessResult.exit_code
                      << " reason=" << headlessResult.reason
                      << "\n" << std::flush;
            AppendPhaseTrace(caseDir, "headless_call", headlessRet ? "end" : "error", "", static_cast<int>(headlessElapsed));
            if (options.trace_run)
                trace.event("headless_call", headlessRet ? "end" : "error", "after RunCxScriptHeadless");

            std::cout << "[SUITE] post-headless begin case_id="
                      << suiteCase.case_id << "\n" << std::flush;

            out.headless_ok = headlessResult.ok;
            out.case_dir = caseDir.string();
            out.snapshot_path = headlessResult.snapshot_path;
            out.summary_path = headlessResult.summary_path;
            out.result_overlay_path = headlessResult.result_overlay_path;
            out.evidence_overlay_path = headlessResult.evidence_overlay_path;
            out.tool_display_path = headlessResult.tool_display_path;

            std::cout << "[SUITE] metrics begin summary=" << out.summary_path << "\n" << std::flush;
            LoadSuiteCaseMetricsFromSummary(out.summary_path, out);
            if (out.actual_policy_guard.empty())
            {
                if (out.tool == "FindLine" && out.valid_points_count >= 2 && out.has_fit_line)
                    out.actual_policy_guard = "MEASURE_AND_FIT_AVAILABLE";
                else if (out.tool == "FindCircle" && out.valid_points_count >= 3 &&
                         out.has_fit_circle && out.circle_radius > 0.0)
                    out.actual_policy_guard = "CIRCLE_MEASURE_AND_FIT_AVAILABLE";
                out.policy_guard = out.actual_policy_guard;
            }
            std::cout << "[SUITE] metrics end headless_ok=" << (out.headless_ok ? "true" : "false")
                      << " valid_points_count=" << out.valid_points_count
                      << " has_fit_circle=" << (out.has_fit_circle ? "true" : "false")
                      << " policy_guard=" << out.policy_guard
                      << "\n" << std::flush;

            if (options.stop_after_headless)
            {
                std::cout << "[SUITE] stop_after_headless requested case_id="
                          << suiteCase.case_id << "\n" << std::flush;
                out.contract_pass = out.headless_ok;
                out.contract_status = "stopped_after_headless";
                out.contract_conclusion = "--suite-stop-after-headless requested";
                out.conclusion = out.headless_ok
                    ? "Stopped after headless by --suite-stop-after-headless"
                    : "Stopped after failed headless by --suite-stop-after-headless";
                WriteFinalCaseSummary(out, caseDir);
                if (options.dump_replay_package)
                {
                    CopyScriptSnapshot(script.path, caseDir);
                    WriteReplayPackage(caseDir, suiteCase, resolved, out);
                }
                if (options.trace_run)
                    trace.close();
                std::cout << "[SUITE] post-headless end case_id="
                          << suiteCase.case_id
                          << " reason=stop_after_headless\n" << std::flush;
                return;
            }

            if (!out.headless_ok)
            {
                out.contract_pass = false;
                out.actual_policy_guard = "HEADLESS_FAILED";
                out.failure_stage = "headless_execution_failed";
                out.conclusion = "Headless execution failed: " + headlessResult.reason;

                if (options.trace_run)
                    trace.event("headless", "error", "Headless execution failed: " + headlessResult.reason);

                if (options.export_tool_display)
                {
                    std::cout << "[SUITE] tool_display begin case_id="
                              << suiteCase.case_id << " reason=headless_failed\n" << std::flush;
                    out.tool_display_path =
                        CxScriptToolDisplayExporter::ExportToolDisplay(
                            image.path,
                            out.result_overlay_path,
                            out.evidence_overlay_path,
                            caseDir / "tool_display.png",
                            out);
                    std::cout << "[SUITE] tool_display end case_id="
                              << suiteCase.case_id << " reason=headless_failed\n" << std::flush;
                }
                else
                {
                    std::cout << "[SUITE] tool_display skipped case_id="
                              << suiteCase.case_id << "\n" << std::flush;
                }

                if (options.export_evidence_summary)
                {
                    std::cout << "[SUITE] evidence_packet begin case_id="
                              << suiteCase.case_id << " reason=headless_failed\n" << std::flush;
                    WriteEvidencePacket(caseDir, suiteCase, resolved, out);
                    std::cout << "[SUITE] evidence_packet end case_id="
                              << suiteCase.case_id << " reason=headless_failed\n" << std::flush;
                }
                WriteFinalCaseSummary(out, caseDir);
                if (options.dump_replay_package)
                {
                    CopyScriptSnapshot(script.path, caseDir);
                    WriteReplayPackage(caseDir, suiteCase, resolved, out);
                }
                if (options.trace_run)
                    trace.close();
                std::cout << "[SUITE] post-headless end case_id="
                          << suiteCase.case_id
                          << " reason=headless_failed\n" << std::flush;
                return;
            }

            if (options.trace_run)
                trace.event("headless", "end", "Headless finished");

            CxAlgorithmTraceScope::Clear();
            AppendPhaseTrace(caseDir, "headless", "end", "", timer.elapsed_ms());

            std::cout << "[SUITE] headless phase end case_id="
                      << suiteCase.case_id << "\n" << std::flush;
        }

        {
            ScopedSuiteTimer timer("tool_display:" + suiteCase.case_id);
            AppendPhaseTrace(caseDir, "tool_display", "begin", "", 0);
            AppendNodeTrace(caseDir, "N110_tool_display", "tool_display", "begin", "Exporting tool display");
            std::cout << "[SUITE] tool_display begin case_id="
                      << suiteCase.case_id << "\n" << std::flush;
            if (options.trace_run)
                trace.event("tool_display", "begin", "Exporting tool display");

            if (options.export_tool_display)
            {
                out.tool_display_path =
                    CxScriptToolDisplayExporter::ExportToolDisplay(
                        image.path,
                        out.result_overlay_path,
                        out.evidence_overlay_path,
                    caseDir / "tool_display.png",
                    out);
            }
            else
            {
                std::cout << "[SUITE] tool_display skipped case_id="
                          << suiteCase.case_id << "\n" << std::flush;
            }

            if (options.trace_run)
                trace.event("tool_display", "end", "Tool display exported");
            AppendPhaseTrace(caseDir, "tool_display", "end", "", timer.elapsed_ms());
            AppendNodeTrace(caseDir, "N110_tool_display", "tool_display", "end", "Tool display exported", timer.elapsed_ms());
            std::cout << "[SUITE] tool_display end case_id="
                      << suiteCase.case_id << "\n" << std::flush;
        }

        if (StopForHumanReviewIfNeeded(options, out, "result", "accept_result or reject_overlay or derive_profile"))
        {
            WriteEvidencePacket(caseDir, suiteCase, resolved, out);
            WriteFinalCaseSummary(out, caseDir);
            if (options.dump_replay_package)
            {
                CopyScriptSnapshot(script.path, caseDir);
                WriteReplayPackage(caseDir, suiteCase, resolved, out);
            }
            if (options.trace_run)
                trace.close();
            return;
        }

        {
            ScopedSuiteTimer timer("contract:" + suiteCase.case_id);
            AppendPhaseTrace(caseDir, "contract", "begin", "", 0);
            AppendNodeTrace(caseDir, "N120_contract", "contract", "begin", "Running contract cxscript");
            std::cout << "[SUITE] contract begin case_id="
                      << suiteCase.case_id << "\n" << std::flush;
            if (options.trace_run)
                trace.event("contract", "begin", "Running contract cxscript");
            if (options.run_contract)
            {
                EvaluateSuiteCaseContract(out);
            }
            else
            {
                out.contract_pass = out.headless_ok;
                out.contract_status = "contract_skipped";
                out.contract_conclusion = "--no-contract requested; contract evaluation skipped";
                out.conclusion = out.contract_conclusion;
                std::cout << "[SUITE] contract skipped case_id="
                          << suiteCase.case_id << "\n" << std::flush;
            }
            if (options.trace_run)
                trace.event("contract", "end", "Contract finished");
            AppendPhaseTrace(caseDir, "contract", "end", "", timer.elapsed_ms());
            AppendNodeTrace(caseDir, "N120_contract", "contract", "end", 
                "Contract finished, pass=" + std::to_string(out.contract_pass ? 1 : 0), 
                timer.elapsed_ms());
            std::cout << "[SUITE] contract end case_id="
                      << suiteCase.case_id
                      << " pass=" << (out.contract_pass ? "true" : "false")
                      << "\n" << std::flush;
        }

        if (options.export_tool_display)
        {
            out.tool_display_path =
                CxScriptToolDisplayExporter::ExportToolDisplay(
                    image.path,
                    out.result_overlay_path,
                    out.evidence_overlay_path,
                    caseDir / "tool_display.png",
                    out);
        }

        if (StopForHumanReviewIfNeeded(options, out, "contract", "accept_contract or reject_contract"))
        {
            WriteEvidencePacket(caseDir, suiteCase, resolved, out);
            WriteFinalCaseSummary(out, caseDir);
            if (options.dump_replay_package)
            {
                CopyScriptSnapshot(script.path, caseDir);
                WriteReplayPackage(caseDir, suiteCase, resolved, out);
            }
            if (options.trace_run)
                trace.close();
            return;
        }

        if (options.export_evidence_summary)
        {
            std::cout << "[SUITE] evidence_packet begin case_id="
                      << suiteCase.case_id << "\n" << std::flush;
            WriteEvidencePacket(caseDir, suiteCase, resolved, out);
            std::cout << "[SUITE] evidence_packet end case_id="
                      << suiteCase.case_id << "\n" << std::flush;
        }
        else
        {
            std::cout << "[SUITE] evidence_packet skipped case_id="
                      << suiteCase.case_id << "\n" << std::flush;
        }

        std::cout << "[SUITE] final_summary begin case_id="
                  << suiteCase.case_id << "\n" << std::flush;
        WriteFinalCaseSummary(out, caseDir);
        std::cout << "[SUITE] final_summary end case_id="
                  << suiteCase.case_id << "\n" << std::flush;

        if (options.dump_replay_package)
        {
            std::cout << "[SUITE] replay_package begin case_id="
                      << suiteCase.case_id << "\n" << std::flush;
            CopyScriptSnapshot(script.path, caseDir);
            WriteReplayPackage(caseDir, suiteCase, resolved, out);
            std::cout << "[SUITE] replay_package end case_id="
                      << suiteCase.case_id << "\n" << std::flush;
        }

        AppendPhaseTrace(caseDir, "promotion", "begin", "", 0);
        std::cout << "[SUITE] promotion begin case_id="
                  << suiteCase.case_id << "\n" << std::flush;
        if (options.trace_run)
            trace.event("promotion", "begin", "Promotion stage");

        if (StopForHumanReviewIfNeeded(options, out, "promotion", "accept_to_regression or derive_diagnostic_profile"))
        {
            AppendPhaseTrace(caseDir, "promotion", "end", "", 0);
            if (options.trace_run)
                trace.close();
            return;
        }

        AppendPhaseTrace(caseDir, "promotion", "end", "", 0);
        std::cout << "[SUITE] promotion end case_id="
                  << suiteCase.case_id << "\n" << std::flush;

        if (options.trace_run)
        {
            trace.event("promotion", "end", "Promotion finished");
            trace.close();
        }

        CXLOG_INFO("CxScriptSuiteRunner", "case_end", 
                   out.contract_pass ? "passed" : "failed", 
                   "case_id=" + suiteCase.case_id + 
                   " headless_ok=" + (out.headless_ok ? "true" : "false") + 
                   " contract_pass=" + (out.contract_pass ? "true" : "false"));
    }

    int CountExecuted(const std::vector<CxScriptSuiteCaseResult>& cases)
    {
        int count = 0;
        for (const auto& c : cases)
            if (c.headless_ok)
                ++count;
        return count;
    }

    int CountContractPass(const std::vector<CxScriptSuiteCaseResult>& cases)
    {
        int count = 0;
        for (const auto& c : cases)
            if (c.contract_pass)
                ++count;
        return count;
    }

    bool StopForHumanReviewIfNeeded(
        const CxScriptSuiteRunOptions& options,
        CxScriptSuiteCaseResult& caseResult,
        const std::string& stage,
        const std::string& suggestedAction)
    {
        if (!options.require_human_review)
            return false;

        if (!options.review_stage.empty() &&
            options.review_stage != stage)
            return false;

        if (!options.resume_review_id.empty())
        {
            CxScriptHumanReview review;
            std::string reason;
            std::filesystem::path reviewPath =
                std::filesystem::path(caseResult.case_dir) / "human_review.json";

            if (LoadHumanReviewJson(reviewPath, review, reason))
            {
                if (review.review_id == options.resume_review_id &&
                    (options.review_decision.empty() || review.decision == options.review_decision))
                {
                    return false;
                }
            }

            if (!options.review_decision.empty())
            {
                CxScriptHumanReview manualReview;
                manualReview.review_id = options.resume_review_id;
                manualReview.decision = options.review_decision;
                manualReview.reviewer = "command_line";
                manualReview.note = "Review decision from command line";
                manualReview.next_action = "continue";

                std::string writeReason;
                WriteHumanReviewJson(reviewPath, manualReview, writeReason);
                return false;
            }
        }

        CxScriptReviewRequest request;
        request.review_id = caseResult.case_id + "_" + stage;
        request.case_id = caseResult.case_id;
        request.evidence_id = caseResult.evidence_id;
        request.stage = stage;
        request.image_id = caseResult.image_id;
        request.target_id = caseResult.target_id;
        request.script_id = caseResult.script_id;
        request.parameter_profile_id = caseResult.parameter_profile_id;
        request.contract_id = caseResult.contract_id;

        request.roi_preview_path = caseResult.roi_preview_path;
        request.tool_display_path = caseResult.tool_display_path;
        request.result_summary_path = caseResult.summary_path;
        request.evidence_packet_path = caseResult.evidence_packet_path;
        request.contract_result_path = caseResult.contract_result_path;
        request.suggested_action = suggestedAction;

        if (stage == "roi")
            request.reason = "ROI preview generated. Please confirm whether the green ROI covers the intended target.";
        else if (stage == "result")
            request.reason = "Tool display generated. Please confirm whether the result matches the expected output.";
        else if (stage == "contract")
            request.reason = "Contract evaluation completed. Please confirm whether the contract result is consistent with the visual evidence.";
        else if (stage == "promotion")
            request.reason = "Case execution completed. Please decide whether to promote this case to the formal regression suite.";
        else
            request.reason = "Review required at stage: " + stage;

        std::string reason;
        std::filesystem::path requestPath =
            std::filesystem::path(caseResult.case_dir) / "review_request.json";
        WriteReviewRequestJson(requestPath, request, reason);

        std::cout << "[review-required]\n";
        std::cout << "review_id=" << request.review_id << "\n";
        std::cout << "stage=" << stage << "\n";
        std::cout << "roi_preview=" << request.roi_preview_path << "\n";
        std::cout << "tool_display=" << request.tool_display_path << "\n";
        std::cout << "review_request=" << requestPath.string() << "\n";
        std::cout << std::flush;

        caseResult.stopped_for_review = true;
        caseResult.review_stage = stage;
        return true;
    }

    bool WriteHumanReviewJson(
        const std::filesystem::path& path,
        const CxScriptHumanReview& review,
        std::string& reason)
    {
        try
        {
            std::ofstream file(path);
            if (!file.is_open())
            {
                reason = "Cannot open human review file: " + path.string();
                return false;
            }

            file << "{\n";
            file << "  \"review_id\": \"" << review.review_id << "\",\n";
            file << "  \"decision\": \"" << review.decision << "\",\n";
            file << "  \"reviewer\": \"" << review.reviewer << "\",\n";
            file << "  \"note\": \"" << review.note << "\",\n";
            file << "  \"next_action\": \"" << review.next_action << "\"";

            if (!review.suggested_profile_id.empty())
            {
                file << ",\n";
                file << "  \"suggested_profile_id\": \"" << review.suggested_profile_id << "\"";
            }

            file << "\n";
            file << "}\n";

            return true;
        }
        catch (const std::exception& e)
        {
            reason = "Failed to write human review: " + std::string(e.what());
            return false;
        }
    }
}

bool RunCxScriptSuite(
    const CxScriptSuiteRunOptions& options,
    CxScriptSuiteRunResult& result)
{
    result = CxScriptSuiteRunResult{};

    std::string reason;
    CxScriptSuiteRuntime suite;
    CxScriptCatalogRuntime catalog;
    CxParameterProfileRuntime parameterProfiles;
    std::string catalogPath;

    {
        CxParserRuntimeOwner metadataOwner;
        if (!metadataOwner.Initialize(reason))
        {
            result.reason = "metadata parser owner initialization failed: " + reason;
            return false;
        }

        if (!metadataOwner.ParseScriptSuite(options.suite_path, suite, reason))
        {
            result.reason = reason;
            return false;
        }

        catalogPath = !options.catalog_path_override.empty()
            ? options.catalog_path_override
            : suite.catalog_path;
        if (!metadataOwner.ParseScriptCatalog(catalogPath, catalog, reason))
        {
            result.reason = reason;
            return false;
        }

        if (!options.parameter_profile_path.empty())
        {
            if (!metadataOwner.ParseParameterProfile(
                    options.parameter_profile_path,
                    parameterProfiles,
                    reason))
            {
                result.reason = "parameter profile load failed: " + reason;
                return false;
            }
            std::cout << "[DEBUG] Loaded "
                      << parameterProfiles.profiles.size()
                      << " parameter profiles\n";
        }
    }

    if (RequiresStage25L1L3CanonicalManifest(suite) &&
        !IsStage25L1L3CanonicalManifest(options.image_manifest_path))
    {
        result.reason =
            "stage25_l1_l3_parameter_consistency requires canonical image manifest: "
            "cxparser/cxscript/module/cximage/stage25/manifests/stage25_l1_l3_manifest.json; got: " +
            options.image_manifest_path;
        return false;
    }

    CxScriptImageManifestRuntime imageManifest;

    if (!LoadStage25ImageManifestJson(
            options.image_manifest_path,
            imageManifest,
            reason))
    {
        result.reason = reason;
        return false;
    }

    auto validation = ValidateStage25ImageManifest(imageManifest);

    const std::filesystem::path outRoot =
        !options.out_root_override.empty()
            ? std::filesystem::path(options.out_root_override)
            : std::filesystem::path(suite.output_root);

    std::filesystem::create_directories(outRoot);

    CxScriptSuiteReportWriter::WriteImageManifestContractReport(
        outRoot,
        imageManifest,
        validation);

    if (!validation.ok)
    {
        WriteManifestDryRunReport(imageManifest, outRoot.string());
        result.ok = false;
        result.reason = "image manifest validation failed";
        result.report_root = outRoot.string();
        return false;
    }

    WriteManifestDryRunReport(imageManifest, outRoot.string());

    for (const auto& suiteCase : suite.cases)
    {
        if (!options.only_case_id.empty() &&
            suiteCase.case_id != options.only_case_id)
        {
            continue;
        }

        ResolvedEvidenceCase resolved;
        std::string resolveReason;

        {
            ScopedSuiteTimer timer("resolve_evidence:" + suiteCase.case_id);
            if (!ResolveEvidenceCase(suiteCase, catalog, imageManifest, parameterProfiles, resolved, resolveReason))
            {
                CxScriptSuiteCaseResult failCase;
                failCase.case_id = suiteCase.case_id;
                failCase.script_id = suiteCase.script_id;
                failCase.image_id = suiteCase.image_id;
                failCase.headless_ok = false;
                failCase.contract_pass = false;
                failCase.failure_stage = "evidence_resolution_failed";
                failCase.conclusion = resolveReason;

                const std::filesystem::path caseDir =
                    outRoot /
                    "cases" /
                    suiteCase.level /
                    CompactCaseDirectoryName(suiteCase.case_id);
                std::filesystem::create_directories(caseDir);

                WriteCaseTrace(caseDir, suiteCase, resolved);
                WriteEvidencePacket(caseDir, suiteCase, resolved, failCase);

                if (resolved.image)
                    ExportRoiPreview(resolved.image->path, caseDir, suiteCase, resolved);

                result.case_results.push_back(failCase);
                continue;
            }
        }

        if (options.dry_run)
        {
            CxScriptSuiteCaseResult dryCase;
            dryCase.case_id = suiteCase.case_id;
            dryCase.script_id = suiteCase.script_id;
            dryCase.image_id = suiteCase.image_id;
            dryCase.target_id = suiteCase.target_id;
            dryCase.parameter_profile_id = suiteCase.parameter_profile_id;
            dryCase.headless_ok = false;
            dryCase.contract_pass = false;
            dryCase.failure_stage = "dry_run_completed";
            dryCase.conclusion = "Dry run: all evidence chain components resolved successfully";

            std::cout << "[dry-run] case_id=" << suiteCase.case_id
                      << " script_found=" << (resolved.script ? "true" : "false")
                      << " image_found=" << (resolved.image ? "true" : "false")
                      << " target_found=" << (resolved.target ? "true" : "false")
                      << " parameter_found=" << (resolved.profile ? "true" : "false")
                      << " contract_found=" << (!resolved.contract_path.empty() ? "true" : "false")
                      << "\n";

            result.case_results.push_back(dryCase);
            continue;
        }

        CxScriptSuiteCaseResult caseResult;

        if (options.preview_only)
        {
            const std::filesystem::path caseDir =
                outRoot /
                "cases" /
                suiteCase.level /
                CompactCaseDirectoryName(suiteCase.case_id);
            std::filesystem::create_directories(caseDir);

            caseResult.case_id = suiteCase.case_id;
            caseResult.evidence_id = suiteCase.case_id;
            caseResult.script_id = suiteCase.script_id;
            caseResult.image_id = suiteCase.image_id;
            caseResult.target_id = suiteCase.target_id;
            caseResult.parameter_profile_id = suiteCase.parameter_profile_id;
            caseResult.contract_id = resolved.script ? resolved.script->contract_path : "";
            caseResult.case_dir = caseDir.string();

            CxScriptRunTraceRuntime trace;
            std::string traceReason;
            if (options.trace_run)
            {
                trace.open(caseDir, suiteCase.case_id, suiteCase.case_id, traceReason);
                trace.set_case_context(
                    suiteCase.case_id,
                    suiteCase.image_id,
                    suiteCase.target_id,
                    suiteCase.script_id,
                    suiteCase.parameter_profile_id);
                trace.event("resolve_evidence", "end", "Evidence resolved");
            }

            {
                ScopedSuiteTimer timer("roi_preview:" + suiteCase.case_id);
                if (options.trace_run)
                    trace.event("roi_preview", "begin", "Generating ROI preview");
                WriteCaseTrace(caseDir, suiteCase, resolved);
                ExportRoiPreview(resolved.image->path, caseDir, suiteCase, resolved);
                caseResult.roi_preview_path = (caseDir / "roi_preview.png").string();
                if (options.trace_run)
                    trace.event("roi_preview", "end", "ROI preview generated");
            }

            WriteEvidencePacket(caseDir, suiteCase, resolved, caseResult);

            if (options.dump_replay_package)
            {
                CopyScriptSnapshot(resolved.script ? resolved.script->path : "", caseDir);
                WriteReplayPackage(caseDir, suiteCase, resolved, caseResult);
            }

            if (options.trace_run)
                trace.close();

            if (StopForHumanReviewIfNeeded(options, caseResult, "roi", "accept_roi or reject_roi"))
            {
                caseResult.stopped_for_review = true;
                caseResult.review_stage = "roi";
            }

            result.case_results.push_back(caseResult);
            continue;
        }

        RunSingleSuiteCase(
            suiteCase,
            *resolved.script,
            *resolved.image,
            imageManifest,
            parameterProfiles,
            resolved,
            outRoot,
            options,
            caseResult);

        result.case_results.push_back(caseResult);
    }

    if (options.export_best_examples)
    {
        std::cout << "[SUITE] best_examples begin\n" << std::flush;
        CxScriptBestCaseSelector::SelectAndExportBestExamples(
            outRoot,
            result.case_results);
        std::cout << "[SUITE] best_examples end\n" << std::flush;
    }
    else
    {
        std::cout << "[SUITE] best_examples skipped\n" << std::flush;
    }

    if (options.export_final_report)
    {
        std::cout << "[SUITE] final_report begin\n" << std::flush;

        std::cout << "[SUITE] report suite_run begin\n" << std::flush;
        CxScriptSuiteReportWriter::WriteSuiteRunReport(
            outRoot,
            result.case_results);
        CxScriptSuiteReportWriter::WriteSuiteRunReportJson(
            outRoot,
            result.case_results);
        std::cout << "[SUITE] report suite_run end\n" << std::flush;

        if (options.export_best_examples)
        {
            std::cout << "[SUITE] report best_detection_gallery begin\n" << std::flush;
            CxScriptSuiteReportWriter::WriteBestDetectionGallery(
                outRoot,
                result.case_results);
            std::cout << "[SUITE] report best_detection_gallery end\n" << std::flush;
        }
        else
        {
            std::cout << "[SUITE] report best_detection_gallery skipped\n" << std::flush;
        }

        std::cout << "[SUITE] report findline_algorithm begin\n" << std::flush;
        CxScriptSuiteReportWriter::WriteFindLineAlgorithmIterationReport(
            outRoot,
            result.case_results);
        std::cout << "[SUITE] report findline_algorithm end\n" << std::flush;

        std::cout << "[SUITE] report findcircle_algorithm begin\n" << std::flush;
        CxScriptSuiteReportWriter::WriteFindCircleAlgorithmIterationReport(
            outRoot,
            result.case_results);
        std::cout << "[SUITE] report findcircle_algorithm end\n" << std::flush;

        std::cout << "[SUITE] report failure_classification begin\n" << std::flush;
        CxScriptSuiteReportWriter::WriteFailureClassificationReport(
            outRoot,
            result.case_results);
        std::cout << "[SUITE] report failure_classification end\n" << std::flush;

        std::cout << "[SUITE] final_report end\n" << std::flush;
    }
    else
    {
        std::cout << "[SUITE] final_report skipped\n" << std::flush;
    }

    result.total_cases = static_cast<int>(result.case_results.size());
    result.executed_cases = CountExecuted(result.case_results);
    result.contract_pass = CountContractPass(result.case_results);
    result.contract_fail = result.executed_cases - result.contract_pass;
    result.report_root = outRoot.string();

    bool stoppedForReview = false;
    for (const auto& cr : result.case_results)
    {
        if (cr.stopped_for_review)
        {
            stoppedForReview = true;
            break;
        }
    }

    if (stoppedForReview)
    {
        result.ok = false;
        result.reason = "REVIEW_REQUIRED";
        return false;
    }

    if (options.dry_run)
    {
        bool allEvidenceResolved = result.total_cases > 0;
        for (const auto& cr : result.case_results)
        {
            if (cr.failure_stage != "dry_run_completed")
            {
                allEvidenceResolved = false;
                break;
            }
        }

        result.ok = allEvidenceResolved;
        result.reason = allEvidenceResolved
            ? "suite dry-run passed"
            : "suite dry-run has unresolved evidence";
        std::cout << "[SUITE] RunCxScriptSuite returning ok="
                  << (result.ok ? "true" : "false")
                  << " reason=" << result.reason
                  << "\n" << std::flush;
        return result.ok;
    }

    result.ok = result.total_cases > 0 &&
                result.executed_cases == result.total_cases &&
                result.contract_pass == result.executed_cases &&
                result.contract_fail == 0;
    if (result.ok)
        result.reason = "suite passed";
    else if (result.executed_cases != result.total_cases)
        result.reason = "suite has unexecuted cases";
    else
        result.reason = "suite has contract failures";

    std::cout << "[SUITE] RunCxScriptSuite returning ok="
              << (result.ok ? "true" : "false")
              << " reason=" << result.reason
              << "\n" << std::flush;

    return result.ok;
}
