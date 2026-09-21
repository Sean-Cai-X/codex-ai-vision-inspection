#include "pch.h"
#include "metrology_analytics/CxMetrologyReferenceReplay.h"

#include "metrology_analytics/CxRoughness1D.h"
#include "metrology_analytics/CxSurfaceAreas.h"
#include "metrology_analytics/CxSurfaceBasicStats.h"
#include "metrology_analytics/CxSyntheticSurfaceFactory.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace cxvision::metrology_analytics
{
namespace
{

std::string JsonEscape(const std::string& text)
{
    std::string out;
    for (char c : text)
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

std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        throw std::runtime_error("failed to open reference json: " + path.string());
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}

std::size_t FindKey(const std::string& text, const std::string& key)
{
    return text.find("\"" + key + "\"");
}

std::size_t FindObjectEnd(const std::string& text, std::size_t objectBegin)
{
    if (objectBegin == std::string::npos || objectBegin >= text.size() || text[objectBegin] != '{')
        return std::string::npos;
    int depth = 0;
    bool inString = false;
    bool escape = false;
    for (std::size_t i = objectBegin; i < text.size(); ++i)
    {
        const char c = text[i];
        if (escape)
        {
            escape = false;
            continue;
        }
        if (c == '\\' && inString)
        {
            escape = true;
            continue;
        }
        if (c == '"')
        {
            inString = !inString;
            continue;
        }
        if (inString)
            continue;
        if (c == '{')
            ++depth;
        else if (c == '}')
        {
            --depth;
            if (depth == 0)
                return i;
        }
    }
    return std::string::npos;
}

std::string ExtractObject(const std::string& text, const std::string& key)
{
    const std::size_t keyPos = FindKey(text, key);
    if (keyPos == std::string::npos)
        throw std::runtime_error("missing object key: " + key);
    const std::size_t colon = text.find(':', keyPos);
    const std::size_t begin = text.find('{', colon);
    const std::size_t end = FindObjectEnd(text, begin);
    if (colon == std::string::npos || begin == std::string::npos || end == std::string::npos)
        throw std::runtime_error("invalid object for key: " + key);
    return text.substr(begin, end - begin + 1);
}

std::string ExtractString(const std::string& text, const std::string& key)
{
    const std::size_t keyPos = FindKey(text, key);
    if (keyPos == std::string::npos)
        throw std::runtime_error("missing string key: " + key);
    const std::size_t colon = text.find(':', keyPos);
    const std::size_t quote0 = text.find('"', colon + 1);
    if (colon == std::string::npos || quote0 == std::string::npos)
        throw std::runtime_error("invalid string key: " + key);
    bool escape = false;
    for (std::size_t i = quote0 + 1; i < text.size(); ++i)
    {
        if (escape)
        {
            escape = false;
            continue;
        }
        if (text[i] == '\\')
        {
            escape = true;
            continue;
        }
        if (text[i] == '"')
            return text.substr(quote0 + 1, i - quote0 - 1);
    }
    throw std::runtime_error("unterminated string key: " + key);
}

double ExtractNumber(const std::string& text, const std::string& key)
{
    const std::size_t keyPos = FindKey(text, key);
    if (keyPos == std::string::npos)
        throw std::runtime_error("missing number key: " + key);
    const std::size_t colon = text.find(':', keyPos);
    if (colon == std::string::npos)
        throw std::runtime_error("invalid number key: " + key);
    std::size_t start = colon + 1;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])))
        ++start;
    std::size_t end = start;
    while (end < text.size())
    {
        const char c = text[end];
        if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E')
            ++end;
        else
            break;
    }
    if (end == start)
        throw std::runtime_error("empty number key: " + key);
    const double v = std::stod(text.substr(start, end - start));
    if (!std::isfinite(v))
        throw std::runtime_error("non-finite number key: " + key);
    return v;
}

struct GoldenAssert
{
    std::string metric;
    double expected = 0.0;
    double tolerance = 0.0;
};

bool TryExtractGoldenAssert(
    const std::string& goldenObject,
    const std::string& metric,
    GoldenAssert& out)
{
    const std::size_t keyPos = FindKey(goldenObject, metric);
    if (keyPos == std::string::npos)
        return false;
    const std::size_t colon = goldenObject.find(':', keyPos);
    const std::size_t begin = goldenObject.find('{', colon);
    const std::size_t end = FindObjectEnd(goldenObject, begin);
    if (colon == std::string::npos || begin == std::string::npos || end == std::string::npos)
        throw std::runtime_error("invalid golden assert object: " + metric);
    const std::string block = goldenObject.substr(begin, end - begin + 1);
    out.metric = metric;
    out.expected = ExtractNumber(block, "value");
    out.tolerance = ExtractNumber(block, "abs_tol");
    return true;
}

std::vector<GoldenAssert> ExtractGoldenAsserts(const std::string& text)
{
    const std::string goldenObject = ExtractObject(text, "golden_asserts");
    const char* knownMetrics[] = {
        "basic_stats.mean",
        "basic_stats.rms",
        "surface_area.area_ratio",
        "roughness_1d.Ra",
        "roughness_1d.Rq"};
    std::vector<GoldenAssert> out;
    for (const char* metric : knownMetrics)
    {
        GoldenAssert g;
        if (TryExtractGoldenAssert(goldenObject, metric, g))
            out.push_back(g);
    }
    if (out.empty())
        throw std::runtime_error("no supported golden_asserts found");
    return out;
}

CxSurfaceField GenerateSurfaceFromReference(const std::string& text, const std::string& caseKind)
{
    const std::string params = ExtractObject(text, "generator_params");
    const int sizeX = static_cast<int>(ExtractNumber(params, "size_x"));
    const int sizeY = static_cast<int>(ExtractNumber(params, "size_y"));
    if (caseKind == "flat")
    {
        return CxSyntheticSurfaceFactory::flat(
            sizeX,
            sizeY,
            ExtractNumber(params, "z0"));
    }
    if (caseKind == "sine1d")
    {
        return CxSyntheticSurfaceFactory::sine1D(
            sizeX,
            sizeY,
            ExtractNumber(params, "A"),
            ExtractNumber(params, "lambda_px"),
            static_cast<int>(ExtractNumber(params, "axis")));
    }
    throw std::runtime_error("unsupported reference case_kind: " + caseKind);
}

CxProfile1D BuildReferenceProfile(const CxSurfaceField& field, int axis)
{
    CxProfile1D p;
    p.delta_x_physical = 1.0;
    if (axis == 0)
    {
        p.z.reserve(static_cast<std::size_t>(field.xres()));
        for (int x = 0; x < field.xres(); ++x)
            p.z.push_back(field.at(x, 0));
    }
    else
    {
        p.z.reserve(static_cast<std::size_t>(field.yres()));
        for (int y = 0; y < field.yres(); ++y)
            p.z.push_back(field.at(0, y));
    }
    return p;
}

double ComputeMetric(
    const std::string& metric,
    const CxSurfaceField& field,
    const std::string& sourceJson)
{
    if (metric == "basic_stats.mean")
        return computeSurfaceBasicStats(field).mean;
    if (metric == "basic_stats.rms")
        return computeSurfaceBasicStats(field).rms;
    if (metric == "surface_area.area_ratio")
        return computeProjectedAndSurfaceArea(field).area_ratio;
    if (metric == "roughness_1d.Ra" || metric == "roughness_1d.Rq")
    {
        int axis = 0;
        try
        {
            axis = static_cast<int>(ExtractNumber(ExtractObject(sourceJson, "generator_params"), "axis"));
        }
        catch (...)
        {
            axis = 0;
        }
        const CxRoughness1DResult r =
            computeProfileRoughness(BuildReferenceProfile(field, axis));
        return metric == "roughness_1d.Ra" ? r.Ra : r.Rq;
    }
    throw std::runtime_error("unsupported reference metric: " + metric);
}

std::string CaseIdFromPath(const std::filesystem::path& path)
{
    return path.stem().string();
}

void ReplayOneReferenceFile(
    const std::filesystem::path& path,
    CxMetrologyReferenceReplayResult& result)
{
    const std::string text = ReadTextFile(path);
    const int schemaVersion = static_cast<int>(ExtractNumber(text, "schema_version"));
    if (schemaVersion != 1)
        throw std::runtime_error("unsupported schema_version in " + path.string());
    const std::string caseKind = ExtractString(text, "case_kind");
    const CxSurfaceField field = GenerateSurfaceFromReference(text, caseKind);
    const std::vector<GoldenAssert> asserts = ExtractGoldenAsserts(text);

    ++result.reference_case_count;
    for (const GoldenAssert& g : asserts)
    {
        CxMetrologyReferenceAssertionResult a;
        a.case_id = CaseIdFromPath(path);
        a.case_kind = caseKind;
        a.metric = g.metric;
        a.expected = g.expected;
        a.tolerance = g.tolerance;
        a.observed = ComputeMetric(g.metric, field, text);
        a.pass = std::abs(a.observed - a.expected) <= a.tolerance;
        a.reason = a.pass ? "reference assertion matched" : "reference assertion mismatch";
        result.assertions.push_back(a);
    }
}

bool WriteReplayArtifacts(
    const std::filesystem::path& outputDir,
    CxMetrologyReferenceReplayResult& result,
    std::string& reason)
{
    std::error_code ec;
    std::filesystem::create_directories(outputDir, ec);
    if (ec)
    {
        reason = "failed to create reference replay output dir: " + ec.message();
        return false;
    }

    result.summary_path = outputDir / "metrology_reference_replay_summary.json";
    result.report_path = outputDir / "metrology_reference_replay_report.md";

    {
        std::ofstream f(result.summary_path, std::ios::binary | std::ios::trunc);
        if (!f.is_open())
        {
            reason = "failed to open reference replay summary";
            return false;
        }
        f << "{\n";
        f << "  \"schema\": \"cxvision.metrology_reference_replay.v1\",\n";
        f << "  \"reference_dir\": \"" << JsonEscape(result.reference_dir.string()) << "\",\n";
        f << "  \"reference_case_count\": " << result.reference_case_count << ",\n";
        f << "  \"assertion_count\": " << result.assertion_count << ",\n";
        f << "  \"pass_count\": " << result.pass_count << ",\n";
        f << "  \"fail_count\": " << result.fail_count << ",\n";
        f << "  \"conclusion\": \"" << (result.fail_count == 0 ? "REFERENCE_REPLAY_PASS" : "REFERENCE_REPLAY_FAIL") << "\",\n";
        f << "  \"assertions\": [\n";
        for (std::size_t i = 0; i < result.assertions.size(); ++i)
        {
            const auto& a = result.assertions[i];
            f << "    {\"case_id\":\"" << JsonEscape(a.case_id)
              << "\", \"case_kind\":\"" << JsonEscape(a.case_kind)
              << "\", \"metric\":\"" << JsonEscape(a.metric)
              << "\", \"pass\":" << (a.pass ? "true" : "false")
              << ", \"observed\":" << a.observed
              << ", \"expected\":" << a.expected
              << ", \"tolerance\":" << a.tolerance
              << ", \"reason\":\"" << JsonEscape(a.reason) << "\"}"
              << (i + 1 < result.assertions.size() ? "," : "") << "\n";
        }
        f << "  ]\n";
        f << "}\n";
    }

    {
        std::ofstream f(result.report_path, std::ios::binary | std::ios::trunc);
        if (!f.is_open())
        {
            reason = "failed to open reference replay report";
            return false;
        }
        f << "# Metrology Reference Replay Report\n\n";
        f << "- reference_dir: " << result.reference_dir.string() << "\n";
        f << "- reference_case_count: " << result.reference_case_count << "\n";
        f << "- assertion_count: " << result.assertion_count << "\n";
        f << "- pass_count: " << result.pass_count << "\n";
        f << "- fail_count: " << result.fail_count << "\n";
        f << "- conclusion: " << (result.fail_count == 0 ? "REFERENCE_REPLAY_PASS" : "REFERENCE_REPLAY_FAIL") << "\n\n";
        f << "| Case | Kind | Metric | Pass | Observed | Expected | Tol | Reason |\n";
        f << "|---|---|---|---:|---:|---:|---:|---|\n";
        for (const auto& a : result.assertions)
        {
            f << "| " << a.case_id
              << " | " << a.case_kind
              << " | " << a.metric
              << " | " << (a.pass ? 1 : 0)
              << " | " << a.observed
              << " | " << a.expected
              << " | " << a.tolerance
              << " | " << a.reason
              << " |\n";
        }
    }

    reason.clear();
    return true;
}

} // namespace

bool RunMetrologyReferenceReplay(
    const std::filesystem::path& reference_dir,
    const std::filesystem::path& output_dir,
    CxMetrologyReferenceReplayResult& result,
    std::string& reason)
{
    result = {};
    result.reference_dir = reference_dir;

    try
    {
        if (!std::filesystem::exists(reference_dir))
        {
            reason = "reference dir does not exist: " + reference_dir.string();
            return false;
        }

        const std::filesystem::path schemaPath = reference_dir / "schema_v1.json";
        if (!std::filesystem::exists(schemaPath))
        {
            reason = "missing schema_v1.json in reference dir";
            return false;
        }
        const std::string schema = ReadTextFile(schemaPath);
        if (schema.find("independent cxvision implementation") == std::string::npos)
        {
            reason = "reference schema missing independence policy marker";
            return false;
        }

        std::vector<std::filesystem::path> caseFiles;
        for (const auto& entry : std::filesystem::directory_iterator(reference_dir))
        {
            if (!entry.is_regular_file())
                continue;
            const std::filesystem::path path = entry.path();
            const std::string name = path.filename().string();
            if (path.extension() == ".json" && name.rfind("ref_case_", 0) == 0)
                caseFiles.push_back(path);
        }
        std::sort(caseFiles.begin(), caseFiles.end());
        if (caseFiles.empty())
        {
            reason = "no ref_case_*.json files found";
            return false;
        }
        for (const std::filesystem::path& path : caseFiles)
            ReplayOneReferenceFile(path, result);
    }
    catch (const std::exception& e)
    {
        reason = std::string("reference replay failed: ") + e.what();
        return false;
    }

    result.assertion_count = static_cast<int>(result.assertions.size());
    for (const auto& a : result.assertions)
    {
        if (a.pass)
            ++result.pass_count;
        else
            ++result.fail_count;
    }

    return WriteReplayArtifacts(output_dir, result, reason);
}

} // namespace cxvision::metrology_analytics
