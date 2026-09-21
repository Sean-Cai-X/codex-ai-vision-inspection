#include "CxParamRegressionRuntime.h"
#include "CxParamRegressionRegister.h"
#include "CxParserRuntimeOwner.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>

namespace
{
    std::string EscapeJson(const std::string& value)
    {
        const char slash = static_cast<char>(92);
        const char quote = static_cast<char>(34);
        std::string out;
        out.reserve(value.size() + 8);
        for (char ch : value)
        {
            switch (static_cast<unsigned char>(ch))
            {
            case 92: out.push_back(slash); out.push_back(slash); break;
            case 34: out.push_back(slash); out.push_back(quote); break;
            case 10: out.push_back(slash); out.push_back('n'); break;
            case 13: out.push_back(slash); out.push_back('r'); break;
            case 9: out.push_back(slash); out.push_back('t'); break;
            default: out.push_back(ch); break;
            }
        }
        return out;
    }

    std::string QuoteCliArg(const std::string& value)
    {
        const char slash = static_cast<char>(92);
        const char quote = static_cast<char>(34);
        std::string out(1, quote);
        for (char ch : value)
        {
            if (ch == slash || ch == quote)
                out.push_back(slash);
            out.push_back(ch);
        }
        out.push_back(quote);
        return out;
    }

    void AppendReplayFlag(std::ostringstream& cmd, const std::string& name)
    {
        cmd << " " << std::string(2, '-') << name;
    }

    void AppendReplayValue(std::ostringstream& cmd, const std::string& name, const std::string& value)
    {
        AppendReplayFlag(cmd, name);
        cmd << " " << QuoteCliArg(value);
    }

    void AppendReplayValue(std::ostringstream& cmd, const std::string& name, int value)
    {
        AppendReplayFlag(cmd, name);
        cmd << " " << value;
    }

    void AppendReplayValue(std::ostringstream& cmd, const std::string& name, double value)
    {
        AppendReplayFlag(cmd, name);
        cmd << " " << value;
    }

    std::string BuildHeadlessReplayCommand(const CxParamEvalRecord& r)
    {
        const std::string replay_case = r.candidate_id + "_" + r.case_id;
        std::ostringstream cmd;
        cmd << "<BINARY>";
        AppendReplayFlag(cmd, "headless");
        AppendReplayFlag(cmd, "cxscript-headless");
        AppendReplayValue(cmd, "image", r.image_path);
        AppendReplayValue(cmd, "script", r.script_path);
        AppendReplayValue(cmd, "case-name", replay_case);
        AppendReplayValue(cmd, "out", "<REPLAY_OUT_DIR>");
        AppendReplayValue(cmd, "max-steps", 10000);
        AppendReplayValue(cmd, "timeout-sec", r.timeout_seconds);
        AppendReplayValue(cmd, "stage25-tool", r.tool);
        AppendReplayValue(cmd, "image-id", r.image_id);
        AppendReplayValue(cmd, "target-id", r.target_id);
        AppendReplayValue(cmd, "roi-x0", r.roi_x0);
        AppendReplayValue(cmd, "roi-y0", r.roi_y0);
        AppendReplayValue(cmd, "roi-x1", r.roi_x1);
        AppendReplayValue(cmd, "roi-y1", r.roi_y1);
        AppendReplayValue(cmd, "tool-half-width", r.tool_half_width);
        AppendReplayValue(cmd, "method", r.method);
        AppendReplayValue(cmd, "threshold", r.threshold);
        AppendReplayValue(cmd, "gap", r.gap);
        AppendReplayValue(cmd, "linegap", r.linegap);
        AppendReplayValue(cmd, "min-edge-run-width-px",
                          r.min_edge_run_width_px);
        AppendReplayValue(cmd, "wgap", r.wgap);
        AppendReplayValue(cmd, "hgap", r.hgap);
        AppendReplayValue(cmd, "filterprofile", r.filterprofile);
        AppendReplayValue(cmd, "samplerate", r.samplerate);
        AppendReplayValue(cmd, "min-score", r.min_score);
        AppendReplayValue(cmd, "find-num", r.find_num);
        AppendReplayValue(cmd, "compare-gap", r.compare_gap);
        AppendReplayValue(cmd, "max-elapsed-ms", r.max_elapsed_ms);
        AppendReplayValue(cmd, "max-scan-lines", r.max_scan_lines);
        AppendReplayValue(cmd, "max-samples", r.max_samples);
        return cmd.str();
    }


    CxParamRange RangeValues(
        const std::string& name,
        std::initializer_list<double> values,
        const std::string& role = "formal")
    {
        CxParamRange r;
        r.name = name;
        r.discrete_values.assign(values.begin(), values.end());
        if (!r.discrete_values.empty())
        {
            r.min_value = *std::min_element(r.discrete_values.begin(), r.discrete_values.end());
            r.max_value = *std::max_element(r.discrete_values.begin(), r.discrete_values.end());
        }
        r.step = 1.0;
        r.integer_only = true;
        r.enabled = true;
        r.role = role;
        return r;
    }

    int ValueAt(const CxParamRangeSet& ranges, const std::string& name, std::size_t index, int fallback)
    {
        for (const auto& r : ranges.ranges)
        {
            if (r.name == name && !r.discrete_values.empty())
            {
                return static_cast<int>(r.discrete_values[index % r.discrete_values.size()]);
            }
        }
        return fallback;
    }

    const CxParamCandidate* PickProfileCandidate(const std::vector<CxParamCandidate>& candidates)
    {
        if (candidates.empty())
            return nullptr;

        return &*std::min_element(
            candidates.begin(),
            candidates.end(),
            [](const CxParamCandidate& a, const CxParamCandidate& b)
            {
                if (a.selected_for_probe != b.selected_for_probe)
                    return a.selected_for_probe && !b.selected_for_probe;
                if (a.predicted_risk != b.predicted_risk)
                    return a.predicted_risk < b.predicted_risk;
                return a.candidate_id < b.candidate_id;
            });
    }

    std::string ReadTextFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return {};
        std::ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    std::vector<double> ParseJsonNumberArray(const std::string& text)
    {
        std::vector<double> values;
        const char* cursor = text.c_str();
        while (*cursor)
        {
            while (*cursor &&
                   !((*cursor >= '0' && *cursor <= '9') ||
                     *cursor == '-' ||
                     *cursor == '+'))
            {
                ++cursor;
            }
            if (!*cursor)
                break;

            char* end = nullptr;
            const double value = std::strtod(cursor, &end);
            if (end == cursor)
            {
                ++cursor;
                continue;
            }
            values.push_back(value);
            cursor = end;
        }
        return values;
    }

    std::vector<std::pair<double, double>> ExtractMeasurePointsFromSummary(
        const std::string& summary_path)
    {
        std::vector<std::pair<double, double>> points;
        const std::string text = ReadTextFile(summary_path);
        if (text.empty())
            return points;

        std::size_t search = 0;
        while (true)
        {
            const std::size_t role =
                text.find("\"semantic_role\": \"measure_points\"", search);
            if (role == std::string::npos)
                break;

            const std::size_t points_key = text.find("\"points\": [", role);
            if (points_key == std::string::npos)
            {
                search = role + 1;
                continue;
            }
            const std::size_t array_begin = text.find('[', points_key);
            const std::size_t array_end = text.find(']', array_begin);
            if (array_begin == std::string::npos || array_end == std::string::npos)
            {
                search = role + 1;
                continue;
            }

            const auto values = ParseJsonNumberArray(
                text.substr(array_begin + 1, array_end - array_begin - 1));
            for (std::size_t i = 0; i + 1 < values.size(); i += 2)
                points.emplace_back(values[i], values[i + 1]);

            search = array_end + 1;
        }
        return points;
    }

    struct RuntimeHitStats
    {
        int total_points = 0;
        double min_x = 0.0;
        double max_x = 0.0;
        double min_y = 0.0;
        double max_y = 0.0;
        std::vector<int> bins;
    };

    RuntimeHitStats BuildRuntimeHitStats(
        const std::vector<std::pair<double, double>>& points,
        int bin_count)
    {
        RuntimeHitStats stats;
        stats.total_points = static_cast<int>(points.size());
        stats.bins.assign(std::max(1, bin_count), 0);
        if (points.empty())
            return stats;

        stats.min_x = std::numeric_limits<double>::max();
        stats.max_x = -std::numeric_limits<double>::max();
        stats.min_y = std::numeric_limits<double>::max();
        stats.max_y = -std::numeric_limits<double>::max();
        for (const auto& p : points)
        {
            stats.min_x = std::min(stats.min_x, p.first);
            stats.max_x = std::max(stats.max_x, p.first);
            stats.min_y = std::min(stats.min_y, p.second);
            stats.max_y = std::max(stats.max_y, p.second);
        }

        const double width = stats.max_x - stats.min_x;
        for (const auto& p : points)
        {
            int index = 0;
            if (width > 1e-9)
            {
                const double ratio = (p.first - stats.min_x) / width;
                index = static_cast<int>(
                    ratio * static_cast<double>(stats.bins.size()));
                if (index >= static_cast<int>(stats.bins.size()))
                    index = static_cast<int>(stats.bins.size()) - 1;
                if (index < 0)
                    index = 0;
            }
            stats.bins[static_cast<std::size_t>(index)]++;
        }
        return stats;
    }

    std::string CaseLevelFromId(const std::string& case_id)
    {
        if (case_id.rfind("L1_", 0) == 0)
            return "L1";
        if (case_id.rfind("L2_", 0) == 0)
            return "L2";
        if (case_id.rfind("L3_", 0) == 0)
            return "L3";
        if (case_id.rfind("L0_", 0) == 0)
            return "L0";
        return "unknown";
    }

    struct FailureClassCount
    {
        std::string key;
        int count = 0;
    };

    void AddFailureClassCount(
        std::vector<FailureClassCount>& counts,
        const std::string& key)
    {
        for (auto& c : counts)
        {
            if (c.key == key)
            {
                ++c.count;
                return;
            }
        }
        FailureClassCount c;
        c.key = key;
        c.count = 1;
        counts.push_back(c);
    }
}

void CxParamRegressionRuntime::Clear()
{
    task = CxParamRegressionTask{};
    range_set = CxParamRangeSet{};
    candidates.clear();
    records.clear();
    optimization = CxParamOptimizationResult{};
}

CxParamRangeSet MakeConservativeRangeSet(const std::string& tool)
{
    CxParamRangeSet set;
    set.tool = tool;
    set.range_set_id = tool == "FindCircle"
        ? "findcircle_conservative_range"
        : "findline_conservative_range";
    set.max_candidates = 12;
    set.max_case_seconds = 10;
    set.max_total_seconds = 60;

    if (tool == "FindCircle")
    {
        set.ranges.push_back(RangeValues("threshold", {8, 12, 20, 30}));
        set.ranges.push_back(RangeValues("gap", {3, 5, 8, 12}));
        set.ranges.push_back(RangeValues("linegap", {4, 6, 10}));
        set.ranges.push_back(RangeValues("method", {0, 1}, "diagnostic"));
        set.ranges.push_back(RangeValues("samplerate", {1, 2, 4}, "diagnostic"));
    }
    else
    {
        set.ranges.push_back(RangeValues("threshold", {8, 12, 20, 30}));
        set.ranges.push_back(RangeValues("linegap", {4, 6, 10, 14}));
        set.ranges.push_back(RangeValues("min_edge_run_width_px",
                                         {1, 3, 5}));
        set.ranges.push_back(RangeValues("wgap", {4, 8, 12}));
        set.ranges.push_back(RangeValues("hgap", {24, 32, 48}));
        set.ranges.push_back(RangeValues("filterprofile", {0, 1}));
        set.ranges.push_back(RangeValues("method", {0, 1}, "diagnostic"));
        set.ranges.push_back(RangeValues("samplerate", {1, 2}, "diagnostic"));
    }

    return set;
}

std::vector<CxParamCandidate> GenerateBasicParamCandidates(
    const CxParamRangeSet& ranges,
    int max_candidates)
{
    std::vector<CxParamCandidate> out;
    const int count = std::max(1, std::min(max_candidates, ranges.max_candidates));

    for (int i = 0; i < count; ++i)
    {
        CxParamCandidate c;
        c.candidate_id = i == 0 ? "manual_seed" : ("grid_" + std::to_string(i));
        c.source = i == 0 ? "manual_seed" : "grid";
        c.method = ValueAt(ranges, "method", i, 0);
        c.threshold = ValueAt(ranges, "threshold", i, 20);
        c.gap = ValueAt(ranges, "gap", i, 5);
        c.linegap = ValueAt(ranges, "linegap", i, 6);
        c.min_edge_run_width_px =
            ValueAt(ranges, "min_edge_run_width_px", i, 3);
        c.wgap = ValueAt(ranges, "wgap", i, 8);
        c.hgap = ValueAt(ranges, "hgap", i, 32);
        c.filterprofile = ValueAt(ranges, "filterprofile", i, 1);
        c.samplerate = ValueAt(ranges, "samplerate", i, 1);
        c.predicted_quality = 0.5;
        c.predicted_risk = i == 0 ? 0.25 : 0.5;
        c.predicted_failure_class = "pending_probe";
        c.selected_for_probe = i < 3;
        out.push_back(c);
    }

    return out;
}

bool ExportParamRegressionReports(
    const std::string& out_dir,
    const CxParamRegressionTask& task,
    const CxParamRangeSet& ranges,
    const std::vector<CxParamCandidate>& candidates,
    const std::vector<CxParamEvalRecord>& records,
    const std::vector<CxParamAccuracyStats>& stats,
    std::string& reason)
{
    namespace fs = std::filesystem;
    const fs::path root(out_dir);
    fs::create_directories(root);

    {
        std::ofstream file(root / "param_regression_task.json");
        if (!file.is_open()) { reason = "failed to write param_regression_task.json"; return false; }
        file << "{\n";
        file << "  \"task_id\": \"" << EscapeJson(task.task_id) << "\",\n";
        file << "  \"case_id\": \"" << EscapeJson(task.case_id) << "\",\n";
        file << "  \"image_id\": \"" << EscapeJson(task.image_id) << "\",\n";
        file << "  \"target_id\": \"" << EscapeJson(task.target_id) << "\",\n";
        file << "  \"tool\": \"" << EscapeJson(task.tool) << "\",\n";
        file << "  \"gauge_annotation_path\": \"" << EscapeJson(task.gauge_annotation_path) << "\",\n";
        file << "  \"require_manual_gauge\": " << (task.require_manual_gauge ? "true" : "false") << ",\n";
        file << "  \"allow_promote\": " << (task.allow_promote ? "true" : "false") << "\n";
        file << "}\n";
    }

    {
        std::ofstream json(root / "param_range_report.json");
        std::ofstream csv(root / "param_range_report.csv");
        std::ofstream md(root / "param_range_report.md");
        if (!json.is_open() || !csv.is_open() || !md.is_open())
        {
            reason = "failed to write parameter range reports";
            return false;
        }
        json << "{\n  \"range_set_id\": \"" << EscapeJson(ranges.range_set_id)
             << "\",\n  \"tool\": \"" << EscapeJson(ranges.tool) << "\",\n  \"ranges\": [\n";
        csv << "parameter,min,max,step,values,enabled,role\n";
        md << "# Parameter Range Report\n\n";
        md << "| Parameter | Min | Max | Step | Values | Enabled | Role |\n";
        md << "|---|---:|---:|---:|---|---|---|\n";
        for (std::size_t i = 0; i < ranges.ranges.size(); ++i)
        {
            const auto& r = ranges.ranges[i];
            std::ostringstream values;
            for (std::size_t j = 0; j < r.discrete_values.size(); ++j)
            {
                if (j) values << ",";
                values << r.discrete_values[j];
            }
            json << "    {\"name\":\"" << EscapeJson(r.name) << "\",\"min\":" << r.min_value
                 << ",\"max\":" << r.max_value << ",\"step\":" << r.step
                 << ",\"values\":\"" << EscapeJson(values.str()) << "\",\"enabled\":"
                 << (r.enabled ? "true" : "false") << ",\"role\":\"" << EscapeJson(r.role) << "\"}"
                 << (i + 1 < ranges.ranges.size() ? "," : "") << "\n";
            csv << r.name << "," << r.min_value << "," << r.max_value << "," << r.step
                << "," << values.str() << "," << (r.enabled ? "true" : "false")
                << "," << r.role << "\n";
            md << "| " << r.name << " | " << r.min_value << " | " << r.max_value
               << " | " << r.step << " | " << values.str() << " | "
               << (r.enabled ? "yes" : "no") << " | " << r.role << " |\n";
        }
        json << "  ]\n}\n";
    }

    {
        std::ofstream json(root / "param_candidates.json");
        std::ofstream csv(root / "param_candidates.csv");
        if (!json.is_open() || !csv.is_open())
        {
            reason = "failed to write candidate reports";
            return false;
        }
        json << "{\n  \"candidates\": [\n";
        csv << "candidate,source,method,threshold,gap,linegap,min_edge_run_width_px,wgap,hgap,filterprofile,samplerate,predicted_quality,predicted_risk,selected\n";
        for (std::size_t i = 0; i < candidates.size(); ++i)
        {
            const auto& c = candidates[i];
            json << "    {\"candidate_id\":\"" << EscapeJson(c.candidate_id)
                 << "\",\"source\":\"" << EscapeJson(c.source)
                 << "\",\"method\":" << c.method
                 << ",\"threshold\":" << c.threshold
                 << ",\"gap\":" << c.gap
                 << ",\"linegap\":" << c.linegap
                 << ",\"min_edge_run_width_px\":"
                 << c.min_edge_run_width_px
                 << ",\"wgap\":" << c.wgap
                 << ",\"hgap\":" << c.hgap
                 << ",\"filterprofile\":" << c.filterprofile
                 << ",\"samplerate\":" << c.samplerate
                 << ",\"predicted_quality\":" << c.predicted_quality
                 << ",\"predicted_risk\":" << c.predicted_risk
                 << ",\"selected_for_probe\":" << (c.selected_for_probe ? "true" : "false")
                 << "}" << (i + 1 < candidates.size() ? "," : "") << "\n";
            csv << c.candidate_id << "," << c.source << "," << c.method << ","
                << c.threshold << "," << c.gap << "," << c.linegap << ","
                << c.min_edge_run_width_px << ","
                << c.wgap << "," << c.hgap << "," << c.filterprofile << ","
                << c.samplerate << "," << c.predicted_quality << ","
                << c.predicted_risk << "," << (c.selected_for_probe ? "true" : "false") << "\n";
        }
        json << "  ]\n}\n";
    }

    {
        std::ofstream jsonl(root / "param_eval_records.jsonl");
        std::ofstream hitJson(root / "hit_distribution.json");
        std::ofstream hitCsv(root / "hit_distribution.csv");
        std::ofstream md(root / "param_hit_distribution_report.md");
        if (!jsonl.is_open() || !hitJson.is_open() || !hitCsv.is_open() || !md.is_open())
        {
            reason = "failed to write eval/hit distribution reports";
            return false;
        }
        hitJson << "{\n  \"schema\": \"param_hit_distribution_runtime_v2\",\n"
                << "  \"source\": \"result_summary.shapes[semantic_role=measure_points]\",\n"
                << "  \"records\": [\n";
        hitCsv << "candidate,total_points,bin_index,bin_start_x,bin_end_x,hit_count,fit,mean_distance,support_score,failure_stage\n";
        md << "# Hit Distribution View\n\n";
        md << "Runtime hit distribution is extracted from `result_summary.json` shape snapshots whose `semantic_role` is `measure_points`.\n";
        md << "If a candidate has zero points, this report records zero bins instead of fabricating hits.\n\n";
        md << "| Candidate | RuntimePoints | Fit | MeanDist | XRange | YRange | FailureStage |\n";
        md << "|---|---:|---|---:|---|---|---|\n";
        for (std::size_t i = 0; i < records.size(); ++i)
        {
            const auto& r = records[i];
            const auto runtime_points = ExtractMeasurePointsFromSummary(r.result_summary_path);
            const RuntimeHitStats hit_stats = BuildRuntimeHitStats(runtime_points, 8);
            jsonl << "{\"candidate_id\":\"" << EscapeJson(r.candidate_id)
                  << "\",\"case_id\":\"" << EscapeJson(r.case_id)
                  << "\",\"tool\":\"" << EscapeJson(r.tool)
                  << "\",\"executed\":" << (r.executed ? "true" : "false")
                  << ",\"timeout\":" << (r.timeout ? "true" : "false")
                  << ",\"points\":" << r.points
                  << ",\"fit_available\":" << (r.fit_available ? "true" : "false")
                  << ",\"support_score\":" << r.support_score
                  << ",\"mean_distance\":" << r.mean_distance
                  << ",\"fit_offset\":" << r.fit_offset
                  << ",\"failure_stage\":\"" << EscapeJson(r.failure_stage) << "\"}\n";
            hitJson << "    {\"candidate_id\":\"" << EscapeJson(r.candidate_id)
                    << "\",\"summary_path\":\"" << EscapeJson(r.result_summary_path)
                    << "\",\"record_points\":" << r.points
                    << ",\"runtime_points\":" << hit_stats.total_points
                    << ",\"fit_available\":" << (r.fit_available ? "true" : "false")
                    << ",\"mean_distance\":" << r.mean_distance
                    << ",\"support_score\":" << r.support_score
                    << ",\"failure_stage\":\"" << EscapeJson(r.failure_stage)
                    << "\",\"min_x\":" << hit_stats.min_x
                    << ",\"max_x\":" << hit_stats.max_x
                    << ",\"min_y\":" << hit_stats.min_y
                    << ",\"max_y\":" << hit_stats.max_y
                    << ",\"bins\":[";
            for (std::size_t b = 0; b < hit_stats.bins.size(); ++b)
            {
                if (b)
                    hitJson << ",";
                hitJson << hit_stats.bins[b];
            }
            hitJson << "]}"
                    << (i + 1 < records.size() ? "," : "") << "\n";
            const double range = hit_stats.max_x - hit_stats.min_x;
            for (std::size_t b = 0; b < hit_stats.bins.size(); ++b)
            {
                const double bin_start = hit_stats.total_points > 0
                    ? hit_stats.min_x + range * static_cast<double>(b) /
                        static_cast<double>(hit_stats.bins.size())
                    : 0.0;
                const double bin_end = hit_stats.total_points > 0
                    ? hit_stats.min_x + range * static_cast<double>(b + 1) /
                        static_cast<double>(hit_stats.bins.size())
                    : 0.0;
                hitCsv << r.candidate_id << "," << hit_stats.total_points << ","
                       << b << "," << bin_start << "," << bin_end << ","
                       << hit_stats.bins[b] << ","
                       << (r.fit_available ? "true" : "false") << ","
                       << r.mean_distance << "," << r.support_score << ","
                       << r.failure_stage << "\n";
            }
            md << "| " << r.candidate_id << " | " << hit_stats.total_points << " | "
               << (r.fit_available ? "yes" : "no") << " | " << r.mean_distance
               << " | " << hit_stats.min_x << ".." << hit_stats.max_x
               << " | " << hit_stats.min_y << ".." << hit_stats.max_y
               << " | " << r.failure_stage << " |\n";
        }
        hitJson << "  ]\n}\n";
    }

    {
        std::ofstream tsv(root / "headless_replay_index.tsv");
        std::ofstream md(root / "headless_replay_index.md");
        if (!tsv.is_open() || !md.is_open())
        {
            reason = "failed to write headless replay index";
            return false;
        }
        tsv << "lookup_key\tcandidate_id\tcase_id\timage_id\ttarget_id\ttool\timage_path\tscript_path\tcontract_path\treplay_package_path\tresult_summary_path\ttimeout_seconds\troi_x0\troi_y0\troi_x1\troi_y1\ttool_half_width\tmethod\tthreshold\tgap\tlinegap\tmin_edge_run_width_px\twgap\thgap\tfilterprofile\tsamplerate\tmin_score\tfind_num\tcompare_gap\tmax_elapsed_ms\tmax_scan_lines\tmax_samples\theadless_command" << std::endl;
        md << "# Headless Replay Index" << std::endl << std::endl;
        md << "- lookup_key: candidate_id::case_id" << std::endl;
        md << "- binary_placeholder: <BINARY>" << std::endl;
        md << "- working_directory: <REPO_ROOT>" << std::endl;
        md << "- replay_out_placeholder: <REPLAY_OUT_DIR>" << std::endl << std::endl;
        md << "| LookupKey | Candidate | Case | Image | Target | Script | Result | Command |" << std::endl;
        md << "|---|---|---|---|---|---|---|---|" << std::endl;
        for (const auto& r : records)
        {
            const std::string lookup = r.candidate_id + "::" + r.case_id;
            const std::string command = BuildHeadlessReplayCommand(r);
            tsv << lookup << '\t' << r.candidate_id << '\t' << r.case_id << '\t'
                << r.image_id << '\t' << r.target_id << '\t' << r.tool << '\t'
                << r.image_path << '\t' << r.script_path << '\t' << r.contract_path << '\t'
                << r.replay_package_path << '\t' << r.result_summary_path << '\t'
                << r.timeout_seconds << '\t' << r.roi_x0 << '\t' << r.roi_y0 << '\t'
                << r.roi_x1 << '\t' << r.roi_y1 << '\t' << r.tool_half_width << '\t'
                << r.method << '\t' << r.threshold << '\t' << r.gap << '\t'
                << r.linegap << '\t' << r.min_edge_run_width_px << '\t'
                << r.wgap << '\t' << r.hgap << '\t'
                << r.filterprofile << '\t' << r.samplerate << '\t' << r.min_score << '\t'
                << r.find_num << '\t' << r.compare_gap << '\t' << r.max_elapsed_ms << '\t'
                << r.max_scan_lines << '\t' << r.max_samples << '\t' << command << std::endl;
            md << "| " << lookup << " | " << r.candidate_id << " | "
               << r.case_id << " | " << r.image_id << " | " << r.target_id
               << " | " << r.script_path << " | " << r.result_summary_path
               << " | `" << command << "` |" << std::endl;
        }
    }

    {
        std::ofstream json(root / "param_accuracy_matrix.json");
        std::ofstream csv(root / "param_accuracy_matrix.csv");
        std::ofstream md(root / "param_accuracy_matrix.md");
        if (!json.is_open() || !csv.is_open() || !md.is_open())
        {
            reason = "failed to write accuracy matrix reports";
            return false;
        }
        json << "{\n  \"schema\": \"param_accuracy_matrix_phase1\",\n  \"stats\": [\n";
        csv << "candidate,tool,total,executed,timeout,geometry_pass,evidence_pass,human_accept,geometry_rate,evidence_rate,human_rate,stability,risk\n";
        md << "# Accuracy / Stability Matrix\n\n";
        md << "| Candidate | Tool | Geometry | Evidence | Human | Stability | Risk |\n";
        md << "|---|---|---:|---:|---:|---:|---:|\n";
        for (std::size_t i = 0; i < stats.size(); ++i)
        {
            const auto& s = stats[i];
            json << "    {\"candidate_id\":\"" << EscapeJson(s.candidate_id)
                 << "\",\"tool\":\"" << EscapeJson(s.tool)
                 << "\",\"geometry_pass_rate\":" << s.geometry_pass_rate
                 << ",\"evidence_pass_rate\":" << s.evidence_pass_rate
                 << ",\"human_accept_rate\":" << s.human_accept_rate
                 << ",\"stability_score\":" << s.stability_score
                 << ",\"risk_score\":" << s.risk_score << "}"
                 << (i + 1 < stats.size() ? "," : "") << "\n";
            csv << s.candidate_id << "," << s.tool << "," << s.total_cases << ","
                << s.executed_cases << "," << s.timeout_cases << ","
                << s.geometry_pass << "," << s.evidence_pass << "," << s.human_accept << ","
                << s.geometry_pass_rate << "," << s.evidence_pass_rate << ","
                << s.human_accept_rate << "," << s.stability_score << "," << s.risk_score << "\n";
            md << "| " << s.candidate_id << " | " << s.tool << " | "
               << s.geometry_pass_rate << " | " << s.evidence_pass_rate
               << " | " << s.human_accept_rate << " | " << s.stability_score
               << " | " << s.risk_score << " |\n";
        }
        json << "  ]\n}\n";
    }

    {
        bool has_l1 = false;
        bool has_l2 = false;
        bool has_l3 = false;
        int executed_count = 0;
        int fit_count = 0;
        int timeout_count = 0;
        std::vector<FailureClassCount> failure_counts;

        for (const auto& r : records)
        {
            const std::string level = CaseLevelFromId(r.case_id);
            has_l1 = has_l1 || level == "L1";
            has_l2 = has_l2 || level == "L2";
            has_l3 = has_l3 || level == "L3";
            executed_count += r.executed ? 1 : 0;
            fit_count += r.fit_available ? 1 : 0;
            timeout_count += r.timeout ? 1 : 0;
            const std::string failure =
                r.fit_available ? "geometry_available" :
                (!r.failure_stage.empty() ? r.failure_stage : "geometry_unavailable");
            AddFailureClassCount(failure_counts, failure);
        }

        const bool coverage_complete = has_l1 && has_l2 && has_l3;

        std::ofstream json(root / "candidate_case_matrix.json");
        std::ofstream md(root / "candidate_case_matrix.md");
        if (!json.is_open() || !md.is_open())
        {
            reason = "failed to write candidate case matrix";
            return false;
        }
        json << "{\n";
        json << "  \"schema\": \"candidate_case_matrix_runtime_v1\",\n";
        json << "  \"coverage_complete\": " << (coverage_complete ? "true" : "false") << ",\n";
        json << "  \"levels\": {\"L1\": " << (has_l1 ? "true" : "false")
             << ", \"L2\": " << (has_l2 ? "true" : "false")
             << ", \"L3\": " << (has_l3 ? "true" : "false") << "},\n";
        json << "  \"records\": [\n";
        md << "# Candidate Case Matrix\n\n";
        md << "- schema: candidate_case_matrix_runtime_v1\n";
        md << "- coverage_complete: " << (coverage_complete ? "true" : "false") << "\n";
        md << "- executed_count: " << executed_count << "\n";
        md << "- fit_count: " << fit_count << "\n";
        md << "- timeout_count: " << timeout_count << "\n\n";
        md << "| Level | Candidate | Case | Tool | Executed | Timeout | Points | Fit | MeanDist | FailureStage | Summary |\n";
        md << "|---|---|---|---|---|---|---:|---|---:|---|---|\n";
        for (std::size_t i = 0; i < records.size(); ++i)
        {
            const auto& r = records[i];
            const std::string level = CaseLevelFromId(r.case_id);
            json << "    {\"level\":\"" << EscapeJson(level)
                 << "\",\"candidate_id\":\"" << EscapeJson(r.candidate_id)
                 << "\",\"case_id\":\"" << EscapeJson(r.case_id)
                 << "\",\"tool\":\"" << EscapeJson(r.tool)
                 << "\",\"executed\":" << (r.executed ? "true" : "false")
                 << ",\"timeout\":" << (r.timeout ? "true" : "false")
                 << ",\"points\":" << r.points
                 << ",\"fit_available\":" << (r.fit_available ? "true" : "false")
                 << ",\"mean_distance\":" << r.mean_distance
                 << ",\"support_score\":" << r.support_score
                 << ",\"failure_stage\":\"" << EscapeJson(r.failure_stage)
                 << "\",\"result_summary_path\":\"" << EscapeJson(r.result_summary_path)
                 << "\"}" << (i + 1 < records.size() ? "," : "") << "\n";
            md << "| " << level << " | " << r.candidate_id << " | "
               << r.case_id << " | " << r.tool << " | "
               << (r.executed ? "yes" : "no") << " | "
               << (r.timeout ? "yes" : "no") << " | "
               << r.points << " | " << (r.fit_available ? "yes" : "no")
               << " | " << r.mean_distance << " | "
               << r.failure_stage << " | " << r.result_summary_path << " |\n";
        }
        json << "  ]\n}\n";

        std::ofstream failMd(root / "failure_classification.md");
        if (!failMd.is_open())
        {
            reason = "failed to write failure classification";
            return false;
        }
        failMd << "# Failure Classification\n\n";
        failMd << "| FailureClass | Count | Suggested Next Action |\n";
        failMd << "|---|---:|---|\n";
        for (const auto& c : failure_counts)
        {
            failMd << "| " << c.key << " | " << c.count
                   << " | inspect locked gauge, runtime summary and overlay before changing baseline parameters |\n";
        }
    }

    {
        std::ofstream md(root / "param_candidate_distribution.md");
        if (!md.is_open())
        {
            reason = "failed to write candidate distribution report";
            return false;
        }
        md << "# Candidate Distribution\n\n";
        md << "| Candidate | Source | Threshold | Gap | LineGap | MinRunPx | WGap | HGap | Filter | Method | Risk | Selected |\n";
        md << "|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n";
        for (const auto& c : candidates)
        {
            md << "| " << c.candidate_id << " | " << c.source << " | "
               << c.threshold << " | " << c.gap << " | " << c.linegap << " | "
               << c.min_edge_run_width_px << " | " << c.wgap << " | "
               << c.hgap << " | " << c.filterprofile
               << " | " << c.method << " | " << c.predicted_risk << " | "
               << (c.selected_for_probe ? "yes" : "no") << " |\n";
        }
    }

    {
        std::ofstream json(root / "param_optimization_trace.json");
        if (!json.is_open())
        {
            reason = "failed to write optimization trace";
            return false;
        }
        json << "{\n";
        json << "  \"schema\": \"param_optimization_trace_phase1\",\n";
        json << "  \"note\": \"mlpack/ensmallen are suggestion layers only; no automatic promotion.\",\n";
        json << "  \"steps\": [\n";
        json << "    {\"step\":\"manual_seed\",\"status\":\"available\"},\n";
        json << "    {\"step\":\"mlpack_rank\",\"status\":\"rule_based_placeholder\"},\n";
        json << "    {\"step\":\"ensmallen_opt\",\"status\":\"bounded_suggestion_placeholder\"}\n";
        json << "  ]\n";
        json << "}\n";
    }

    {
        bool has_l1 = false;
        bool has_l2 = false;
        bool has_l3 = false;
        for (const auto& r : records)
        {
            const std::string level = CaseLevelFromId(r.case_id);
            has_l1 = has_l1 || level == "L1";
            has_l2 = has_l2 || level == "L2";
            has_l3 = has_l3 || level == "L3";
        }
        const bool coverage_complete = has_l1 && has_l2 && has_l3;

        std::ofstream json(root / "stability_matrix.json");
        std::ofstream md(root / "param_stability_report.md");
        if (!json.is_open() || !md.is_open())
        {
            reason = "failed to write stability report";
            return false;
        }
        json << "{\n";
        json << "  \"schema\": \"param_stability_runtime_v1\",\n";
        json << "  \"coverage_complete\": " << (coverage_complete ? "true" : "false") << ",\n";
        json << "  \"levels\": {\"L1\": " << (has_l1 ? "true" : "false")
             << ", \"L2\": " << (has_l2 ? "true" : "false")
             << ", \"L3\": " << (has_l3 ? "true" : "false") << "},\n";
        json << "  \"stats\": [\n";
        md << "# Parameter Stability Report\n\n";
        md << "- schema: param_stability_runtime_v1\n";
        md << "- coverage_complete: " << (coverage_complete ? "true" : "false") << "\n";
        md << "- note: Stability is computed from real EvalRecord rows. Human acceptance is still required before promotion.\n\n";
        md << "| Candidate | Cases | GeometryRate | EvidenceRate | TimeoutCases | Stability | Risk | Note |\n";
        md << "|---|---:|---:|---:|---:|---:|---:|---|\n";
        for (std::size_t i = 0; i < stats.size(); ++i)
        {
            const auto& s = stats[i];
            json << "    {\"candidate_id\":\"" << EscapeJson(s.candidate_id)
                 << "\",\"tool\":\"" << EscapeJson(s.tool)
                 << "\",\"total_cases\":" << s.total_cases
                 << ",\"executed_cases\":" << s.executed_cases
                 << ",\"timeout_cases\":" << s.timeout_cases
                 << ",\"geometry_pass_rate\":" << s.geometry_pass_rate
                 << ",\"evidence_pass_rate\":" << s.evidence_pass_rate
                 << ",\"stability_score\":" << s.stability_score
                 << ",\"risk_score\":" << s.risk_score
                 << "}" << (i + 1 < stats.size() ? "," : "") << "\n";
            md << "| " << s.candidate_id << " | " << s.total_cases
               << " | " << s.geometry_pass_rate << " | " << s.evidence_pass_rate
               << " | " << s.timeout_cases << " | " << s.stability_score
               << " | " << s.risk_score << " | "
               << (coverage_complete ? "L1/L2/L3 matrix available" : "pending L1/L2/L3 coverage")
               << " |\n";
        }
        json << "  ]\n}\n";
    }

    {
        std::ofstream md(root / "param_recommendation_report.md");
        if (!md.is_open())
        {
            reason = "failed to write recommendation report";
            return false;
        }
        const CxParamCandidate* picked = PickProfileCandidate(candidates);
        md << "# Parameter Recommendation Report\n\n";
        md << "Recommendations are advisory only in phase 1.\n\n";
        md << "- Best Geometry: manual review required\n";
        md << "- Best Evidence: manual review required\n";
        md << "- Lowest Risk: " << (picked ? picked->candidate_id : "none") << "\n";
        md << "- Most Stable: requires mini-regression\n";
        md << "- Recommended Diagnostic: " << (picked ? picked->candidate_id : "none") << "\n";
    }

    {
        std::ofstream md(root / "param_profile_promotion_gate.md");
        if (!md.is_open())
        {
            reason = "failed to write promotion gate report";
            return false;
        }
        md << "# Profile Promotion Gate\n\n";
        md << "Can Promote: no\n\n";
        md << "Reason:\n\n";
        md << "- Phase 1 only exports range/candidate/evidence tables.\n";
        md << "- Mini-regression and human accept matrix are required before baseline promotion.\n";
        md << "- Candidates may be saved as diagnostic profile candidates only.\n";
    }

    {
        std::ofstream cxsc(root / "param_profile_candidate.cxsc");
        if (!cxsc.is_open())
        {
            reason = "failed to write param_profile_candidate.cxsc";
            return false;
        }
        const CxParamCandidate* picked = PickProfileCandidate(candidates);
        cxsc << "// Diagnostic profile candidate generated by parameter regression phase 1.\n";
        cxsc << "// Human review and mini-regression are required before promotion.\n\n";
        cxsc << "CxParameterProfile_reset(0);\n";
        cxsc << "CxParameterProfile_settool(\"" << EscapeJson(task.tool) << "\");\n";
        cxsc << "CxParameterProfile_add(\"" << EscapeJson(task.task_id) << "_diagnostic_candidate\");\n";
        cxsc << "CxParameterProfile_setrole(\"DIAGNOSTIC_CANDIDATE\");\n";
        if (picked)
        {
            cxsc << "CxParameterProfile_setmethod(" << picked->method << ");\n";
            cxsc << "CxParameterProfile_setthreshold(" << picked->threshold << ");\n";
            cxsc << "CxParameterProfile_setgap(" << picked->gap << ");\n";
            cxsc << "CxParameterProfile_setlinegap(" << picked->linegap << ");\n";
            cxsc << "CxParameterProfile_setminedgerunwidth("
                 << picked->min_edge_run_width_px << ");\n";
            cxsc << "CxParameterProfile_setwgap(" << picked->wgap << ");\n";
            cxsc << "CxParameterProfile_sethgap(" << picked->hgap << ");\n";
            cxsc << "CxParameterProfile_setfilterprofile(" << picked->filterprofile << ");\n";
        }
        cxsc << "CxParameterProfile_setdescription(\"phase1 diagnostic only; promotion disabled\");\n";
    }

    reason.clear();
    return true;
}

bool LoadCxParamRegressionFile(
    const std::string& script_path,
    CxParamRegressionRuntime& out_runtime,
    std::string& out_reason)
{
    namespace fs = std::filesystem;
    fs::path path(script_path);
    if (!fs::exists(path))
    {
        out_reason = "Param regression file not found: " + script_path;
        return false;
    }

    CxParserRuntimeOwner owner;
    if (!owner.Initialize(out_reason))
        return false;

    return owner.ParseParamRegression(script_path, out_runtime, out_reason);
}
