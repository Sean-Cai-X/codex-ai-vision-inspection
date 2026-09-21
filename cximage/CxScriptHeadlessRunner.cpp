#include "pch.h"
#include "CxScriptHeadlessRunner.h"
#include "ImageManager.h"
#include "ManualConsoleUtils.h"
#include "ParserClass.h"
#include "Image.h"
#include "CxScriptRuntimeResultCapture.h"
#include "CxShapeOverlayRenderer.h"
#include "CxScriptRuntimeCaptureSmoke.h"
#include "CxScriptGlobalValueSet.h"
#include "CxScriptCasePackageWriter.h"
#include "measurement_semantics/CxMeasurementSemanticEvidenceWriter.h"

#include <sstream>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstring>
#include <iterator>
#include <limits>
#include <map>
#include <vector>
#include <cmath>
#ifdef _WIN32
#include <Windows.h>
#endif

static const char* kHeadlessGlobalInitScript =
    "cxparser/cxscript/module/cximage/headless/headless_globals.cxsc";

std::string PrepareCxScriptRuntimeSource(const std::string& source, bool contract_context)
{
    std::istringstream input(source);
    std::ostringstream normalized;
    std::string line;
    while (std::getline(input, line))
    {
        if (contract_context &&
            (line.find("global_contract_status") != std::string::npos ||
             line.find("global_contract_conclusion") != std::string::npos))
        {
            continue;
        }
        const size_t first = line.find_first_not_of(" \t");
        if (first != std::string::npos)
        {
            static const char* declaration_types[] = { "int ", "double ", "float " };
            for (const char* type : declaration_types)
            {
                const size_t length = std::strlen(type);
                if (line.compare(first, length, type) == 0)
                {
                    line.erase(first, length);
                    break;
                }
            }
        }
        normalized << line << '\n';
    }

    std::string runtime_source = normalized.str();
    for (size_t i = 0; i < runtime_source.size(); ++i)
    {
        if (runtime_source[i] != '&')
            continue;

        size_t previous = i;
        while (previous > 0 && std::isspace(static_cast<unsigned char>(runtime_source[previous - 1])))
            --previous;
        size_t next = i + 1;
        while (next < runtime_source.size() && std::isspace(static_cast<unsigned char>(runtime_source[next])))
            ++next;

        const bool argument_position =
            previous > 0 && (runtime_source[previous - 1] == '(' || runtime_source[previous - 1] == ',');
        const bool object_identifier =
            next < runtime_source.size() &&
            (std::isalpha(static_cast<unsigned char>(runtime_source[next])) || runtime_source[next] == '_');
        if (argument_position && object_identifier)
        {
            runtime_source.erase(i, 1);
            --i;
        }
    }

    return runtime_source;
}

std::string LoadCxScriptSource(const std::string& script_path, std::string& reason)
{
    std::ifstream script_file(script_path);
    if (!script_file.is_open())
    {
        reason = "cannot open script: " + script_path;
        return "";
    }

    std::string script_source = std::string(
        std::istreambuf_iterator<char>(script_file),
        std::istreambuf_iterator<char>());

    if (script_source.empty())
    {
        reason = "script is empty: " + script_path;
        return "";
    }

    reason.clear();
    return script_source;
}

static std::filesystem::path NormalizeSegmentationArtifactPath(
    const std::string& ref)
{
    std::string normalized = ref;
    for (char& ch : normalized)
    {
        if (ch == '/' || ch == '\\')
            ch = std::filesystem::path::preferred_separator;
    }
    return std::filesystem::path(normalized);
}

static std::filesystem::path SegmentationArtifactIoPath(
    const std::filesystem::path& path)
{
#ifdef _WIN32
    if (!path.is_absolute())
        return path;

    const std::wstring value = path.wstring();
    if (value.rfind(LR"(\\?\)", 0) == 0)
        return path;
    if (value.rfind(LR"(\\)", 0) == 0)
        return std::filesystem::path(
            std::wstring(LR"(\\?\UNC\)") + value.substr(2));
    return std::filesystem::path(std::wstring(LR"(\\?\)") + value);
#else
    return path;
#endif
}

static bool WriteSegmentationArtifactTextFile(
    const std::filesystem::path& path,
    const std::string& text)
{
#ifdef _WIN32
    std::error_code ec;
    const std::filesystem::path temporary =
        std::filesystem::temp_directory_path(ec) /
        ("cx_segmentation_artifact_trace_" +
         std::to_string(GetCurrentProcessId()) + "_" +
         std::to_string(GetTickCount64()) + ".json");
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
        return false;
    file << text;
    file.close();
    if (!file.good())
    {
        std::filesystem::remove(temporary, ec);
        return false;
    }
    ec.clear();
    std::filesystem::copy_file(
        temporary,
        SegmentationArtifactIoPath(path),
        std::filesystem::copy_options::overwrite_existing,
        ec);
    const bool copied = !ec;
    ec.clear();
    std::filesystem::remove(temporary, ec);
    return copied;
#else
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
        return false;
    file << text;
    return file.good();
#endif
}

static void WriteSegmentationArtifactPersistTrace(
    const std::filesystem::path& output_dir,
    const std::string& source_ref,
    const std::filesystem::path& source_path,
    const std::filesystem::path& source_dir,
    const std::filesystem::path& artifact_dir,
    bool source_exists,
    bool source_dir_exists,
    const std::string& status)
{
    int artifact_entry_count = 0;
    std::error_code ec;
    if (!artifact_dir.empty() &&
        std::filesystem::exists(SegmentationArtifactIoPath(artifact_dir), ec) &&
        !ec)
    {
        std::filesystem::recursive_directory_iterator it(
            SegmentationArtifactIoPath(artifact_dir),
            ec);
        std::filesystem::recursive_directory_iterator end;
        while (!ec && it != end)
        {
            ++artifact_entry_count;
            it.increment(ec);
        }
    }
    ec.clear();
    auto has_artifact = [&](const char* name) {
        ec.clear();
        return !artifact_dir.empty() &&
            std::filesystem::exists(
                SegmentationArtifactIoPath(artifact_dir / name),
                ec) &&
            !ec;
    };

    std::ostringstream trace_file;

    const char q = static_cast<char>(34);
    auto write_string = [&](const char* key, const std::string& value, bool comma) {
        trace_file << "  " << q << key << q << ": " << q << JsonEscape(value) << q << (comma ? "," : "") << "\n";
    };
    auto write_bool = [&](const char* key, bool value, bool comma) {
        trace_file << "  " << q << key << q << ": " << (value ? "true" : "false") << (comma ? "," : "") << "\n";
    };
    auto write_int = [&](const char* key, int value, bool comma) {
        trace_file << "  " << q << key << q << ": " << value << (comma ? "," : "") << "\n";
    };

    trace_file << "{\n";
    write_string("source_ref", source_ref, true);
    write_string("source_path", source_path.string(), true);
    write_string("source_dir", source_dir.string(), true);
    write_string("artifact_dir", artifact_dir.string(), true);
    write_bool("source_exists", source_exists, true);
    write_bool("source_dir_exists", source_dir_exists, true);
    write_int("artifact_entry_count", artifact_entry_count, true);
    write_bool("has_torch_runtime_evidence", has_artifact("torch_runtime_evidence.json"), true);
    write_bool("has_tensor_shape_trace", has_artifact("tensor_shape_trace.json"), true);
    write_bool("has_weight_mapping_report", has_artifact("weight_mapping_report.json"), true);
    write_bool("has_measurement_evidence", has_artifact("measurement_evidence.json"), true);
    write_string("status", status, false);
    trace_file << "}\n";
    WriteSegmentationArtifactTextFile(
        output_dir / "segmentation_artifact_persist_trace.json",
        trace_file.str());
}

static void RemapSegmentationArtifactRef(
    std::string& ref,
    const std::filesystem::path& artifact_dir)
{
    if (ref.empty() || artifact_dir.empty())
        return;

    std::error_code ec;
    const std::filesystem::path source_path =
        NormalizeSegmentationArtifactPath(ref);
    if (!std::filesystem::exists(SegmentationArtifactIoPath(source_path), ec) || ec)
        return;

    const std::filesystem::path destination =
        artifact_dir / source_path.filename();
    ec.clear();
    std::filesystem::copy_file(
        SegmentationArtifactIoPath(source_path),
        SegmentationArtifactIoPath(destination),
        std::filesystem::copy_options::overwrite_existing,
        ec);
    if (ec)
        return;

    ref = destination.string();
}


static void PersistSegmentationArtifacts(
    const std::filesystem::path& output_dir,
    CxScriptExecutionCapture& capture)
{
    const std::string source_ref =
        !capture.segmentation_result_ref.empty()
            ? capture.segmentation_result_ref
            : (!capture.segmentation_contour_ref.empty()
                   ? capture.segmentation_contour_ref
                   : capture.segmentation_overlay_ref);
    if (source_ref.empty())
        return;

    std::error_code ec;
    const std::filesystem::path artifact_dir =
        output_dir / "segmentation_artifacts";
    std::filesystem::create_directories(artifact_dir, ec);
    if (ec)
    {
        WriteSegmentationArtifactPersistTrace(
            output_dir,
            source_ref,
            std::filesystem::path(),
            std::filesystem::path(),
            artifact_dir,
            false,
            false,
            "create_artifact_dir_failed");
        return;
    }

    const std::filesystem::path source_path =
        NormalizeSegmentationArtifactPath(source_ref);
    const bool source_exists =
        std::filesystem::exists(source_path, ec) && !ec;
    ec.clear();

    const std::filesystem::path source_dir = source_path.parent_path();
    const bool source_dir_exists =
        !source_dir.empty() && std::filesystem::exists(source_dir, ec) && !ec;
    ec.clear();
    if (!source_exists || !source_dir_exists)
    {
        WriteSegmentationArtifactPersistTrace(
            output_dir,
            source_ref,
            source_path,
            source_dir,
            artifact_dir,
            source_exists,
            source_dir_exists,
            "source_missing");
        return;
    }

    ec.clear();
    const std::filesystem::path canonical_source =
        std::filesystem::weakly_canonical(source_dir, ec);
    ec.clear();
    const std::filesystem::path canonical_artifact =
        std::filesystem::weakly_canonical(artifact_dir, ec);
    if (!canonical_source.empty() && !canonical_artifact.empty() &&
        canonical_source == canonical_artifact)
    {
        WriteSegmentationArtifactPersistTrace(
            output_dir,
            source_ref,
            source_path,
            source_dir,
            artifact_dir,
            source_exists,
            source_dir_exists,
            "already_case_local");
        return;
    }

    ec.clear();
    for (const auto& entry : std::filesystem::directory_iterator(source_dir, ec))
    {
        if (ec)
            break;

        std::error_code copy_ec;
        const std::filesystem::path canonical_entry =
            std::filesystem::weakly_canonical(entry.path(), copy_ec);
        copy_ec.clear();
        if (!canonical_entry.empty() && !canonical_artifact.empty() &&
            canonical_entry == canonical_artifact)
        {
            continue;
        }

        const std::filesystem::path destination =
            artifact_dir / entry.path().filename();
        std::filesystem::copy(
            SegmentationArtifactIoPath(entry.path()),
            SegmentationArtifactIoPath(destination),
            std::filesystem::copy_options::recursive |
                std::filesystem::copy_options::overwrite_existing,
            copy_ec);
    }

    RemapSegmentationArtifactRef(
        capture.segmentation_result_ref, artifact_dir);
    RemapSegmentationArtifactRef(
        capture.segmentation_mask_ref, artifact_dir);
    RemapSegmentationArtifactRef(
        capture.segmentation_contour_ref, artifact_dir);
    RemapSegmentationArtifactRef(
        capture.segmentation_overlay_ref, artifact_dir);
    RemapSegmentationArtifactRef(
        capture.segmentation_raw_result_ref, artifact_dir);
    RemapSegmentationArtifactRef(
        capture.segmentation_raw_mask_ref, artifact_dir);
    RemapSegmentationArtifactRef(
        capture.segmentation_raw_contour_ref, artifact_dir);
    RemapSegmentationArtifactRef(
        capture.segmentation_raw_overlay_ref, artifact_dir);
    RemapSegmentationArtifactRef(
        capture.segmentation_refined_result_ref, artifact_dir);
    RemapSegmentationArtifactRef(
        capture.segmentation_refined_mask_ref, artifact_dir);
    RemapSegmentationArtifactRef(
        capture.segmentation_refined_contour_ref, artifact_dir);
    RemapSegmentationArtifactRef(
        capture.segmentation_refined_overlay_ref, artifact_dir);

    WriteSegmentationArtifactPersistTrace(
        output_dir,
        source_ref,
        source_path,
        source_dir,
        artifact_dir,
        source_exists,
        source_dir_exists,
        "persisted");
}

void DefineCxScriptLocalVariables(
    mu::CxParserRuntime& runtime,
    const std::string& source,
    std::map<std::string, double>& storage)
{
    std::istringstream input(source);
    std::string line;
    while (std::getline(input, line))
    {
        const size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos)
            continue;

        static const char* declaration_types[] = { "int ", "double ", "float " };
        size_t name_begin = std::string::npos;
        for (const char* type : declaration_types)
        {
            const size_t length = std::strlen(type);
            if (line.compare(first, length, type) == 0)
            {
                name_begin = first + length;
                break;
            }
        }
        if (name_begin == std::string::npos)
            continue;

        size_t name_end = name_begin;
        while (name_end < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[name_end])) || line[name_end] == '_'))
        {
            ++name_end;
        }
        if (name_end == name_begin)
            continue;

        const std::string name = line.substr(name_begin, name_end - name_begin);
        auto inserted = storage.emplace(name, 0.0);
        if (inserted.second)
            runtime.m_parser.DefineVar(name, &inserted.first->second);
    }
}

using CxScriptGlobalStorage = std::map<std::string, double>;

static std::string EscapeManualReviewHandoffCell(std::string value)
{
    for (char& ch : value)
    {
        if (ch == '|' || ch == '\r' || ch == '\n' || ch == '\t')
            ch = ' ';
    }
    const size_t first = value.find_first_not_of(" ");
    if (first == std::string::npos)
        return "-";
    const size_t last = value.find_last_not_of(" ");
    return value.substr(first, last - first + 1);
}

static std::string TrimManualReviewValue(std::string value)
{
    const size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

static std::string ExtractManualReviewJsonStringField(
    const std::string& object,
    const std::string& field)
{
    const std::string token = "\"" + field + "\"";
    size_t pos = object.find(token);
    if (pos == std::string::npos)
        return {};
    pos = object.find(':', pos + token.size());
    if (pos == std::string::npos)
        return {};
    pos = object.find('"', pos + 1);
    if (pos == std::string::npos)
        return {};

    std::string value;
    bool escaped = false;
    for (++pos; pos < object.size(); ++pos)
    {
        const char ch = object[pos];
        if (escaped)
        {
            switch (ch)
            {
            case '\\':
                value.push_back('\\');
                break;
            case '"':
                value.push_back('"');
                break;
            case '/':
                value.push_back('/');
                break;
            case 'n':
                value.push_back('\n');
                break;
            case 'r':
                value.push_back('\r');
                break;
            case 't':
                value.push_back('\t');
                break;
            default:
                value.push_back(ch);
                break;
            }
            escaped = false;
            continue;
        }
        if (ch == '\\')
        {
            escaped = true;
            continue;
        }
        if (ch == '"')
            break;
        value.push_back(ch);
    }
    return value;
}

static std::string ExtractManualReviewJsonScalarField(
    const std::string& object,
    const std::string& field)
{
    const std::string token = "\"" + field + "\"";
    size_t pos = object.find(token);
    if (pos == std::string::npos)
        return {};
    pos = object.find(':', pos + token.size());
    if (pos == std::string::npos)
        return {};
    ++pos;
    while (pos < object.size() &&
           std::isspace(static_cast<unsigned char>(object[pos])))
    {
        ++pos;
    }
    if (pos >= object.size())
        return {};
    if (object[pos] == '"')
        return ExtractManualReviewJsonStringField(object, field);

    size_t end = pos;
    while (end < object.size() &&
           object[end] != ',' && object[end] != '}' &&
           object[end] != '\r' && object[end] != '\n')
    {
        ++end;
    }
    return TrimManualReviewValue(object.substr(pos, end - pos));
}

static std::vector<std::string> ExtractManualReviewJsonRowsObjects(
    const std::string& text)
{
    std::vector<std::string> objects;
    const size_t rows_pos = text.find("\"rows\"");
    if (rows_pos == std::string::npos)
        return objects;
    size_t pos = text.find('[', rows_pos);
    if (pos == std::string::npos)
        return objects;

    bool in_string = false;
    bool escaped = false;
    int depth = 0;
    size_t object_begin = std::string::npos;
    for (++pos; pos < text.size(); ++pos)
    {
        const char ch = text[pos];
        if (in_string)
        {
            if (escaped)
            {
                escaped = false;
                continue;
            }
            if (ch == '\\')
            {
                escaped = true;
                continue;
            }
            if (ch == '"')
                in_string = false;
            continue;
        }

        if (ch == '"')
        {
            in_string = true;
            continue;
        }
        if (ch == '{')
        {
            if (depth == 0)
                object_begin = pos;
            ++depth;
            continue;
        }
        if (ch == '}')
        {
            if (depth > 0)
                --depth;
            if (depth == 0 && object_begin != std::string::npos)
            {
                objects.push_back(text.substr(object_begin, pos - object_begin + 1));
                object_begin = std::string::npos;
            }
            continue;
        }
        if (ch == ']' && depth == 0)
            break;
    }
    return objects;
}

struct HeadlessManualReviewStabilityRow
{
    std::string review_item;
    std::string case_id;
    std::string failure_class;
    std::string result_ref;
    std::string overlay_ref;
    std::string extra_evidence;
};

static std::string BuildHeadlessStabilityFailureClass(
    const std::string& delta,
    const std::string& overlay_matches)
{
    if (!delta.empty() && delta != "0")
        return "l3_stability_risk_instance_delta_" + delta;
    if (overlay_matches == "false")
        return "l3_stability_overlay_changed";
    return "l3_stability_pending_human_review";
}

static void AppendManualReviewExtraField(
    std::vector<std::string>& fields,
    const std::string& key,
    const std::string& value)
{
    if (!value.empty())
        fields.push_back(key + "=" + value);
}

static std::vector<HeadlessManualReviewStabilityRow>
LoadHeadlessManualReviewStabilityRows(
    const std::filesystem::path& output_dir,
    const std::string& parent_case_id)
{
    std::vector<HeadlessManualReviewStabilityRow> rows;
    const std::filesystem::path matrix_path = output_dir / "stability_matrix.json";
    std::error_code ec;
    if (!std::filesystem::is_regular_file(matrix_path, ec) || ec)
        return rows;

    std::string text;
    if (!ReadTextFile(matrix_path.string(), text))
        return rows;

    const std::vector<std::string> objects =
        ExtractManualReviewJsonRowsObjects(text);
    auto normalizePathKey = [](const std::filesystem::path& path)
        -> std::string
    {
        std::error_code canonical_ec;
        std::filesystem::path normalized =
            std::filesystem::weakly_canonical(path, canonical_ec);
        if (canonical_ec)
            normalized = path.lexically_normal();
        std::string key = normalized.generic_string();
#ifdef _WIN32
        std::transform(key.begin(), key.end(), key.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
#endif
        return key;
    };

    std::map<std::string, std::string> matrix_rows_by_overlay;
    for (const std::string& object : objects)
    {
        const std::string overlay_ref = ExtractManualReviewJsonStringField(
            object, "inference_overlay_ref");
        if (!overlay_ref.empty())
            matrix_rows_by_overlay[normalizePathKey(overlay_ref)] = object;
    }

    std::vector<std::filesystem::path> evidence_directories;
    std::vector<std::filesystem::path> pending_directories{output_dir};
    while (!pending_directories.empty())
    {
        const std::filesystem::path directory = pending_directories.back();
        pending_directories.pop_back();

        const std::filesystem::path overlay_path = directory / "mask_overlay.png";
        const std::filesystem::path result_path = directory / "instances.json";
        std::error_code overlay_ec;
        std::error_code result_ec;
        if (std::filesystem::is_regular_file(overlay_path, overlay_ec) &&
            !overlay_ec &&
            std::filesystem::is_regular_file(result_path, result_ec) &&
            !result_ec &&
            matrix_rows_by_overlay.find(normalizePathKey(overlay_path)) !=
                matrix_rows_by_overlay.end())
        {
            evidence_directories.push_back(directory);
        }

        std::error_code iterator_ec;
        std::filesystem::directory_iterator it(
            directory,
            std::filesystem::directory_options::skip_permission_denied,
            iterator_ec);
        const std::filesystem::directory_iterator end;
        while (!iterator_ec && it != end)
        {
            std::error_code entry_ec;
            if (it->is_directory(entry_ec) && !entry_ec &&
                !it->is_symlink(entry_ec))
            {
                pending_directories.push_back(it->path());
            }
            it.increment(iterator_ec);
        }
    }

    std::stable_sort(evidence_directories.begin(), evidence_directories.end());
    for (const std::filesystem::path& evidence_directory : evidence_directories)
    {
        const std::filesystem::path overlay_path =
            evidence_directory / "mask_overlay.png";
        const std::filesystem::path result_path =
            evidence_directory / "instances.json";
        const std::string& object =
            matrix_rows_by_overlay.at(normalizePathKey(overlay_path));
        const std::string source_case_id =
            ExtractManualReviewJsonStringField(object, "case_id");

        const std::string delta = ExtractManualReviewJsonScalarField(
            object, "instance_count_delta_from_baseline");
        const std::string overlay_matches = ExtractManualReviewJsonScalarField(
            object, "overlay_hash_matches_baseline");
        const std::string result_ref = result_path.string();
        const std::string overlay_ref = overlay_path.string();
        const std::string model_manifest_ref = ExtractManualReviewJsonStringField(
            object, "model_manifest_ref");
        const std::string perturbation_type = ExtractManualReviewJsonStringField(
            object, "perturbation_type");

        std::vector<std::string> extra_fields;
        AppendManualReviewExtraField(extra_fields, "stability_case_id",
            source_case_id);
        AppendManualReviewExtraField(extra_fields, "perturbation_type",
            perturbation_type);
        AppendManualReviewExtraField(extra_fields, "roi_shift_dx_px",
            ExtractManualReviewJsonScalarField(object, "roi_shift_dx_px"));
        AppendManualReviewExtraField(extra_fields, "roi_shift_dy_px",
            ExtractManualReviewJsonScalarField(object, "roi_shift_dy_px"));
        AppendManualReviewExtraField(extra_fields, "confidence_threshold",
            ExtractManualReviewJsonScalarField(object, "confidence_threshold"));
        AppendManualReviewExtraField(extra_fields, "instance_count",
            ExtractManualReviewJsonScalarField(object, "instance_count"));
        AppendManualReviewExtraField(extra_fields,
            "instance_count_delta_from_baseline", delta);
        AppendManualReviewExtraField(extra_fields, "stability_matrix",
            matrix_path.string());
        AppendManualReviewExtraField(extra_fields, "inference_result",
            result_ref);
        AppendManualReviewExtraField(extra_fields, "inference_overlay",
            overlay_ref);
        AppendManualReviewExtraField(extra_fields, "model_manifest",
            model_manifest_ref);

        std::ostringstream extra;
        for (size_t i = 0; i < extra_fields.size(); ++i)
        {
            if (i > 0)
                extra << "; ";
            extra << extra_fields[i];
        }

        std::error_code relative_ec;
        std::filesystem::path case_folder = std::filesystem::relative(
            evidence_directory, output_dir, relative_ec);
        if (relative_ec || case_folder.empty())
            case_folder = evidence_directory.filename();

        std::string folder_case_id = case_folder.generic_string();
        std::replace(folder_case_id.begin(), folder_case_id.end(), '/', '_');
        std::replace(folder_case_id.begin(), folder_case_id.end(), '\\', '_');

        HeadlessManualReviewStabilityRow row;
        row.review_item = case_folder.generic_string();
        row.case_id = parent_case_id + "__" + folder_case_id;
        row.failure_class = BuildHeadlessStabilityFailureClass(
            delta, overlay_matches);
        row.result_ref = result_ref;
        row.overlay_ref = overlay_ref;
        row.extra_evidence = extra.str();
        rows.push_back(std::move(row));
    }
    return rows;
}

static void WriteManualReviewHandoffTableRow(
    std::ofstream& file,
    const std::string& review_item,
    const std::string& internal_case_id,
    const std::string& tool,
    const std::string& image_id,
    const std::string& target_id,
    const std::string& failure_class,
    const std::string& runtime_summary,
    const std::string& tool_display,
    const std::string& result_overlay,
    const std::string& evidence_overlay,
    const std::string& roi_preview,
    const std::string& script_snapshot,
    const std::string& extra_evidence)
{
    file << "| " << EscapeManualReviewHandoffCell(review_item)
         << " | Manual Review / Evidence > To Verify"
         << " | " << EscapeManualReviewHandoffCell(internal_case_id)
         << " | " << EscapeManualReviewHandoffCell(tool)
         << " | " << EscapeManualReviewHandoffCell(image_id)
         << " | " << EscapeManualReviewHandoffCell(target_id)
         << " | " << EscapeManualReviewHandoffCell(failure_class)
         << " | " << EscapeManualReviewHandoffCell(runtime_summary)
         << " | " << EscapeManualReviewHandoffCell(tool_display)
         << " | " << EscapeManualReviewHandoffCell(result_overlay)
         << " | " << EscapeManualReviewHandoffCell(evidence_overlay)
         << " | " << EscapeManualReviewHandoffCell(roi_preview)
         << " | " << EscapeManualReviewHandoffCell(script_snapshot)
         << " | " << EscapeManualReviewHandoffCell(extra_evidence)
         << " |\n";
}

static std::string BuildManualReviewVisibleItemLabel(
    const CxScriptHeadlessOptions& options,
    const std::string& internal_case_id)
{
    if (!internal_case_id.empty())
        return internal_case_id;
    if (!options.script_path.empty())
    {
        const std::string stem =
            std::filesystem::path(options.script_path).stem().string();
        if (!stem.empty())
            return stem;
    }
    return "current_case";
}

static std::string InferHeadlessManualReviewTool(
    const CxScriptHeadlessOptions& options)
{
    std::string key = options.script_path + " " + options.case_name + " " +
        options.case_id + " " + options.stage25_tool;
    std::transform(key.begin(), key.end(), key.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

    if (key.find("find_segmentation") != std::string::npos ||
        key.find("findsegmentation") != std::string::npos)
        return "FindSegmentation";
    if (key.find("cxtextinspect") != std::string::npos)
        return "CxTextInspect";
    if (key.find("fastmatch") != std::string::npos)
        return "FastMatch";
    if (key.find("torch") != std::string::npos ||
        key.find("yolo") != std::string::npos ||
        key.find("deeplab") != std::string::npos ||
        key.find("backward") != std::string::npos)
        return "TorchTask";
    if (!options.stage25_tool.empty())
        return options.stage25_tool;
    if (key.find("circle") != std::string::npos)
        return "FindCircle";
    if (key.find("ellipse") != std::string::npos)
        return "FindEllipse";
    if (key.find("rect") != std::string::npos)
        return "FindRect";
    return "FindLine";
}

static void AppendExistingManualReviewArtifact(
    std::vector<std::string>& artifacts,
    const std::filesystem::path& path)
{
    if (path.empty())
        return;
    std::error_code ec;
    if (std::filesystem::is_regular_file(path, ec) && !ec)
        artifacts.push_back(path.string());
}

static bool WriteHeadlessManualReviewHandoff(
    const CxScriptHeadlessOptions& options,
    const CxScriptExecutionCapture& capture,
    const CxScriptHeadlessResult& result,
    const std::filesystem::path& output_dir,
    std::string& reason)
{
    reason.clear();
    if (options.contract_context_enabled)
        return true;
    if (result.summary_path.empty())
    {
        reason = "manual review handoff requires result_summary.json";
        return false;
    }

    const std::filesystem::path handoff_path =
        output_dir / "manual_algorithm_review_handoff.md";
    std::ofstream file(handoff_path, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        reason = "failed to open manual review handoff: " +
            handoff_path.string();
        return false;
    }

    const std::string case_id = !options.case_name.empty()
        ? options.case_name
        : (!options.case_id.empty() ? options.case_id : output_dir.filename().string());
    const std::string image_id = !options.image_id.empty()
        ? options.image_id
        : (!options.stage25_image_id.empty() ? options.stage25_image_id : "headless_image");
    const std::string target_id = !options.target_id.empty()
        ? options.target_id
        : (!options.stage25_target_id.empty() ? options.stage25_target_id : "headless_target");
    const std::string tool = InferHeadlessManualReviewTool(options);
    const std::string review_item =
        BuildManualReviewVisibleItemLabel(options, case_id);

    std::vector<std::string> extra_artifacts;
    auto append_artifact = [&](const std::string& key,
                               const std::filesystem::path& path) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(path, ec) && !ec)
            extra_artifacts.push_back(key + "=" + path.string());
    };
    append_artifact("segmentation_trace",
        output_dir / "segmentation_artifact_persist_trace.json");
    append_artifact("yolov8seg_evidence",
        output_dir / "yolov8seg_backward_smoke_evidence.json");
    append_artifact("loss_breakdown", output_dir / "loss_breakdown.json");
    append_artifact("training_trace", output_dir / "training_trace.json");
    append_artifact("gradient_report", output_dir / "gradient_report.json");
    append_artifact("parameter_update_report",
        output_dir / "parameter_update_report.json");
    append_artifact("freeze_ablation_report",
        output_dir / "freeze_ablation_report.json");
    append_artifact("dataset_summary", output_dir / "dataset_summary.json");
    append_artifact("l2_case_matrix", output_dir / "l2_case_matrix.json");
    append_artifact("stability_matrix", output_dir / "stability_matrix.json");
    append_artifact("result_variation", output_dir / "result_variation.json");
    append_artifact("stability_report", output_dir / "stability_report.md");
    append_artifact("timeout_report", output_dir / "timeout_report.md");
    append_artifact("human_review", output_dir / "human_review.json");
    append_artifact("overlay_validation", output_dir / "overlay_validation.json");
    append_artifact("object_state", output_dir / "object_state.json");
    append_artifact("variable_snapshot", output_dir / "variable_snapshot.json");

    std::ostringstream extra;
    for (size_t i = 0; i < extra_artifacts.size(); ++i)
    {
        if (i > 0)
            extra << "; ";
        extra << extra_artifacts[i];
    }

    std::string failure_class = capture.failure_stage.empty()
        ? std::string("pending_human_review")
        : capture.failure_stage;
    if (capture.budget_exceeded)
        failure_class = "algorithm_budget_exceeded";

    file << "# Manual Algorithm Review Handoff\n\n";
    file << "- schema: `cxvision.manual_algorithm_review_handoff.v1`\n";
    file << "- source: `cxscript_headless`\n";
    file << "- conclusion: `PENDING_HUMAN_REVIEW`\n\n";
    file << "Human review must use the visible item under `Manual Review / Evidence > To Verify`; `Internal Case ID` is for trace only. Suite views are automation evidence, not the human project-analysis entry.\n\n";
    file << "| Review Item | Open In | Internal Case ID | Tool | Image | Target | Failure Class | Runtime Summary | Tool Display | Result Overlay | Evidence Overlay | ROI Preview | Script Snapshot | Extra Evidence |\n";
    file << "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n";
    WriteManualReviewHandoffTableRow(
        file,
        review_item,
        case_id,
        tool,
        image_id,
        target_id,
        failure_class,
        result.summary_path,
        result.tool_display_path,
        result.result_overlay_path,
        result.evidence_overlay_path,
        result.evidence_overlay_path,
        options.script_path,
        extra.str());

    const std::vector<HeadlessManualReviewStabilityRow> stability_rows =
        LoadHeadlessManualReviewStabilityRows(output_dir, case_id);
    if (!stability_rows.empty())
    {
        const std::filesystem::path matrix_path =
            output_dir / "stability_matrix.json";
        const std::string matrix_overlay = stability_rows.front().overlay_ref.empty()
            ? result.evidence_overlay_path
            : stability_rows.front().overlay_ref;
        WriteManualReviewHandoffTableRow(
            file,
            matrix_path.filename().string(),
            case_id + "__" + matrix_path.stem().string(),
            tool,
            image_id,
            target_id,
            "pending_human_review",
            result.summary_path,
            result.tool_display_path,
            matrix_overlay,
            matrix_overlay,
            matrix_overlay,
            options.script_path,
            extra.str());
    }
    for (const HeadlessManualReviewStabilityRow& row : stability_rows)
    {
        const std::string overlay = row.overlay_ref.empty()
            ? result.evidence_overlay_path
            : row.overlay_ref;
        WriteManualReviewHandoffTableRow(
            file,
            row.review_item,
            row.case_id,
            tool,
            image_id,
            target_id,
            row.failure_class,
            result.summary_path,
            result.tool_display_path,
            overlay,
            overlay,
            overlay,
            options.script_path,
            row.extra_evidence.empty()
                ? extra.str()
                : row.extra_evidence + "; " + extra.str());
    }

    file.flush();
    if (!file.good())
    {
        reason = "failed while writing manual review handoff: " +
            handoff_path.string();
        return false;
    }

    reason = handoff_path.string();
    return true;
}




std::vector<std::string> ExtractCxScriptGlobalDeclarations(const std::string& source)
{
    std::vector<std::string> names;
    std::istringstream input(source);
    std::string line;
    while (std::getline(input, line))
    {
        const size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos)
            continue;

        static const char* declaration_types[] = { "int ", "double ", "float " };
        bool declaration = false;
        for (const char* type : declaration_types)
        {
            const size_t length = std::strlen(type);
            if (line.compare(first, length, type) == 0)
            {
                declaration = true;
                break;
            }
        }
        if (!declaration)
            continue;

        size_t name_begin = line.find("global_", first);
        if (name_begin == std::string::npos)
            continue;

        size_t name_end = name_begin;
        while (name_end < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[name_end])) || line[name_end] == '_'))
        {
            ++name_end;
        }
        if (name_end > name_begin)
            names.emplace_back(line.substr(name_begin, name_end - name_begin));
    }
    return names;
}

std::string BuildCxScriptGlobalInitRuntimeSource(const std::string& source)
{
    std::ostringstream runtime_source;
    std::istringstream input(source);
    std::string line;
    while (std::getline(input, line))
    {
        const size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos)
            continue;

        static const char* declaration_types[] = { "int ", "double ", "float " };
        bool declaration = false;
        for (const char* type : declaration_types)
        {
            const size_t length = std::strlen(type);
            if (line.compare(first, length, type) == 0)
            {
                declaration = true;
                break;
            }
        }
        if (!declaration)
            runtime_source << line << '\n';
    }

    std::string prepared = runtime_source.str();
    if (prepared.find_first_not_of(" \t\r\n") == std::string::npos)
        prepared = "global_strategy_id = global_strategy_id;\n";
    return PrepareCxScriptRuntimeSource(prepared, false);
}

bool SetCxScriptGlobalValue(
    CxScriptGlobalStorage& storage,
    const std::string& name,
    double value,
    std::string& reason)
{
    auto found = storage.find(name);
    if (found == storage.end())
    {
        reason = "headless global init missing declaration: " + name;
        return false;
    }
    found->second = value;
    return true;
}

double GetCxScriptGlobalValue(
    const CxScriptGlobalStorage& storage,
    const std::string& name,
    double fallback = 0.0)
{
    auto found = storage.find(name);
    return found == storage.end() ? fallback : found->second;
}

bool InjectCxScriptGlobals(
    mu::CxParserRuntime& runtime,
    const CxScriptHeadlessOptions& options,
    CxScriptGlobalStorage& values,
    std::string& reason)
{
    CxScriptGlobalValueSet global_value_set;

    if (!LoadHeadlessGlobalDeclarations(kHeadlessGlobalInitScript, global_value_set, reason))
        return false;

    if (!BindGlobalValueSetToParser(runtime, global_value_set, reason))
        return false;

    const std::string global_init_source =
        LoadCxScriptSource(kHeadlessGlobalInitScript, reason);
    if (global_init_source.empty())
        return false;

    const std::string prepared_global_init =
        BuildCxScriptGlobalInitRuntimeSource(global_init_source);
    if (!runtime.Compile(prepared_global_init.c_str()))
    {
        reason = "cannot compile headless global init script: " +
            std::string(kHeadlessGlobalInitScript);
        return false;
    }

    std::map<std::string, double> option_globals = BuildHeadlessGlobalOverrides(options);
    if (!ApplyGlobalOverrides(global_value_set, option_globals, reason))
        return false;

    if (!options.globals_path.empty())
    {
        std::map<std::string, double> file_globals;
        if (!LoadHeadlessGlobalValuesFile(options.globals_path, file_globals, reason))
            return false;
        if (!ApplyGlobalOverrides(global_value_set, file_globals, reason))
            return false;
    }

    if (!ApplyGlobalOverrides(global_value_set, options.cli_global_overrides, reason))
        return false;

    if (options.contract_context_enabled)
    {
        const std::map<std::string, double> contract_globals = {
            { "global_contract_pass", static_cast<double>(options.contract_pass_initial) },
            { "global_headless_ok", static_cast<double>(options.contract_headless_ok) },
            { "global_algorithm_executed", static_cast<double>(options.contract_algorithm_executed) },
            { "global_budget_exceeded", static_cast<double>(options.contract_budget_exceeded) },
            { "global_valid_points_count", static_cast<double>(options.valid_points_count) },
            { "global_has_fit_line", static_cast<double>(options.has_fit_line) },
            { "global_has_fit_circle", static_cast<double>(options.has_fit_circle) },
            { "global_runtime_valid_points_count", static_cast<double>(options.runtime_valid_points_count) },
            { "global_runtime_has_fit_line", static_cast<double>(options.runtime_has_fit_line) },
            { "global_runtime_has_fit_circle", static_cast<double>(options.runtime_has_fit_circle) },
            { "global_runtime_global_valid_points_count_mismatch", static_cast<double>(options.runtime_global_valid_points_count_mismatch) },
            { "global_runtime_global_has_fit_line_mismatch", static_cast<double>(options.runtime_global_has_fit_line_mismatch) },
            { "global_runtime_global_has_fit_circle_mismatch", static_cast<double>(options.runtime_global_has_fit_circle_mismatch) },
            { "global_runtime_global_result_mismatch", static_cast<double>(options.runtime_global_result_mismatch) },
            { "global_rendered_measure_points_count", static_cast<double>(options.contract_rendered_measure_points_count) },
            { "global_rendered_result_count", static_cast<double>(options.contract_rendered_result_count) },
            { "global_result_overlay_changed_pixels", static_cast<double>(options.contract_result_overlay_changed_pixels) },
            { "global_torch_ok", static_cast<double>(options.contract_torch_ok) },
            { "global_torch_result_count", static_cast<double>(options.contract_torch_result_count) },
            { "global_policy_guard_match", static_cast<double>(options.policy_guard_match) },
            { "global_circle_radius", options.circle_radius },
            { "global_avgdist", options.avgdist },
        };

        if (!ApplyGlobalOverrides(global_value_set, contract_globals, reason))
            return false;
    }

    values.swap(global_value_set.numbers);

    reason.clear();
    return true;
}

bool InjectCxScriptInputImage(
    mu::CxParserRuntime& runtime,
    const cv::Mat& source_image,
    const std::string& object_name,
    std::string& reason)
{
    Image* inputObject = static_cast<Image*>(
        runtime.GetClassObj("Image", object_name));

    if (inputObject == nullptr)
    {
        reason = "Image " + object_name +
            " is unavailable; declare it in " + std::string(kHeadlessGlobalInitScript);
        return false;
    }

    inputObject->copyFromMat(source_image);
    reason.clear();
    return true;
}

bool InjectCxScriptRuntimeStrings(
    mu::CxParserRuntime& runtime,
    const CxScriptHeadlessOptions& options,
    std::string& reason)
{
    try
    {
        const std::string case_id = options.case_name.empty()
            ? options.case_id
            : options.case_name;

        // Read-only values owned by the current serial Headless request.
        // TorchTask therefore uses the same image, case, and artifact root as
        // the surrounding Headless evidence package.
        const char separator = '|';
        std::ostringstream torch_context;
        torch_context << "{\"schema\":\"cx.torch.training_context.v1\""
                      << ",\"source\":\"headless\""
                      << ",\"epochs\":" << options.torch_training_epochs
                      << ",\"learning_rate\":" << std::setprecision(12)
                      << options.torch_learning_rate
                      << ",\"lr_schedule\":\""
                      << JsonEscape(options.torch_lr_schedule) << "\""
                      << ",\"min_learning_rate\":"
                      << options.torch_min_learning_rate
                      << ",\"weight_decay\":" << options.torch_weight_decay
                      << ",\"box_loss_weight\":"
                      << options.torch_box_loss_weight
                      << ",\"class_loss_weight\":"
                      << options.torch_class_loss_weight
                      << ",\"dfl_loss_weight\":"
                      << options.torch_dfl_loss_weight
                      << ",\"mask_loss_weight\":"
                      << options.torch_mask_loss_weight
                      << ",\"use_assignment_quality_targets\":"
                      << (options.torch_use_assignment_quality_targets ? 1 : 0)
                      << ",\"assignment_quality_warmup_epochs\":"
                      << options.torch_assignment_quality_warmup_epochs
                      << ",\"postprocess_confidence_threshold\":"
                      << options.torch_postprocess_confidence_threshold
                      << ",\"postprocess_iou_threshold\":"
                      << options.torch_postprocess_iou_threshold
                      << ",\"postprocess_max_detections\":"
                      << options.torch_postprocess_max_detections
                      << ",\"postprocess_class_agnostic_nms\":"
                      << (options.torch_postprocess_class_agnostic_nms ? 1 : 0)
                      << "}";
        const std::string request_context = case_id + separator +
            options.image_path + separator + options.output_dir + separator +
            torch_context.str() +
            separator + options.torch_dataset_root;
        runtime.DefineStringConstant("global_torch_request_context", request_context);
    }
    catch (const std::exception& e)
    {
        reason = "cannot inject headless torch runtime strings: " +
            std::string(e.what());
        return false;
    }
    catch (...)
    {
        reason = "cannot inject headless torch runtime strings";
        return false;
    }

    reason.clear();
    return true;
}

bool ExecuteCxScriptSequential(
    const CxScriptHeadlessOptions& options,
    const cv::Mat& source_image,
    CxScriptExecutionCapture& capture,
    std::string& reason)
{
    const auto start_time = std::chrono::steady_clock::now();

    if (!ImageManager::EnsureAlgorithmRuntimeResources(source_image.cols, source_image.rows))
    {
        capture.failure_stage = "cximage_runtime_resources";
        reason = "failed to initialize shared cximage algorithm runtime resources";
        return false;
    }

    mu::CxParserRuntime runtime;

    std::ostringstream parser_output;
    runtime.SetStream(&parser_output);

    runtime.ParserInitialClassFunction(0);
    runtime.SetVarFactory();

    CxScriptGlobalStorage global_values;
    if (!InjectCxScriptGlobals(runtime, options, global_values, reason))
        return false;

    if (!InjectCxScriptRuntimeStrings(runtime, options, reason))
        return false;

    if (!InjectCxScriptInputImage(runtime, source_image, "global_matInput", reason))
        return false;

    if (!options.template_image_path.empty())
    {
        cv::Mat template_image = cv::imread(options.template_image_path, cv::IMREAD_COLOR);
        if (template_image.empty())
        {
            reason = "cannot read template image: " + options.template_image_path;
            capture.failure_stage = "template_image";
            return false;
        }

        if (!InjectCxScriptInputImage(runtime, template_image, "global_templateInput", reason))
            return false;
    }

    const std::string script_source =
        LoadCxScriptSource(options.script_path, reason);

    if (script_source.empty())
        return false;

    std::map<std::string, double> script_locals;
    DefineCxScriptLocalVariables(runtime, script_source, script_locals);

    const std::string prepared =
        PrepareCxScriptRuntimeSource(script_source, options.contract_context_enabled);

    if (!runtime.CompileCollectedScript(prepared, reason))
    {
        const std::string parser_diagnostic = parser_output.str();
        if (!parser_diagnostic.empty())
            reason += " | parser: " + parser_diagnostic;
        capture.failure_stage = "script_compile";
        return false;
    }

    capture.script_compiled = true;

    if (!runtime.RunCollectedScript(reason))
    {
        const std::string parser_diagnostic = parser_output.str();
        if (!parser_diagnostic.empty())
            reason += " | parser: " + parser_diagnostic;
        capture.failure_stage = "script_execution";
        return false;
    }

    capture.strategy_id = static_cast<int>(GetCxScriptGlobalValue(global_values, "global_strategy_id"));
    capture.selected_method = static_cast<int>(GetCxScriptGlobalValue(global_values, "global_selected_method"));
    capture.selected_threshold = static_cast<int>(GetCxScriptGlobalValue(global_values, "global_selected_threshold"));
    capture.selected_wgap = static_cast<int>(GetCxScriptGlobalValue(global_values, "global_selected_wgap"));
    capture.selected_hgap = static_cast<int>(GetCxScriptGlobalValue(global_values, "global_selected_hgap"));
    capture.selected_linegap = static_cast<int>(GetCxScriptGlobalValue(global_values, "global_selected_linegap"));
    capture.selected_filterprofile = static_cast<int>(GetCxScriptGlobalValue(global_values, "global_selected_filterprofile"));

    if (options.contract_context_enabled)
    {
        capture.contract_context = true;
        capture.contract_pass = GetCxScriptGlobalValue(global_values, "global_contract_pass") != 0.0;
        capture.contract_status = capture.contract_pass ? "contract_passed" : "contract_failed";
        capture.contract_conclusion = capture.contract_pass
            ? "CxScript contract conditions passed"
            : "CxScript contract conditions failed";
    }

    if (!options.contract_context_enabled) try
    {
        if (!CaptureRuntimeToolResults(runtime, capture, reason))
        {
            capture.failure_stage = "runtime_result_capture";
            return false;
        }
    }
    catch (...)
    {
        reason = "CaptureRuntimeToolResults crashed";
        capture.failure_stage = "runtime_result_capture_crash";
        return false;
    }

    capture.runtime_globals = global_values;

    if (options.runtime_capture_smoke)
    {
        if (!ValidateCxScriptRuntimeCaptureSmoke(runtime, capture, reason))
        {
            capture.failure_stage = "runtime_capture_smoke";
            return false;
        }
    }

    capture.runtime_completed = true;
    capture.elapsed_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count());

    return true;
}

struct CxCircleMeasurePointRadiusStats
{
    int count = 0;
    int inside_inner_count = 0;
    int outside_outer_count = 0;
    double min_radius = 0.0;
    double avg_radius = 0.0;
    double max_radius = 0.0;
};

CxCircleMeasurePointRadiusStats ComputeCircleMeasurePointRadiusStats(
    const std::vector<CxShapeElementSnapshot>& shapes,
    double cx,
    double cy,
    double inner_radius,
    double outer_radius)
{
    CxCircleMeasurePointRadiusStats stats;
    bool has_shape_center = false;
    bool has_shape_inner = false;
    bool has_shape_outer = false;

    for (const auto& shape : shapes)
    {
        if (shape.owner_type != "FindCircle")
            continue;

        if (shape.stable_ref.find(".roi_circle") != std::string::npos &&
            shape.radius > 0.0)
        {
            cx = shape.center_x;
            cy = shape.center_y;
            if (!has_shape_outer && outer_radius <= 0.0)
                outer_radius = shape.radius;
            has_shape_center = true;
        }
        else if (shape.stable_ref.find(".inner_scan_circle") != std::string::npos &&
                 shape.radius > 0.0)
        {
            inner_radius = shape.radius;
            has_shape_inner = true;
        }
        else if (shape.stable_ref.find(".outer_scan_circle") != std::string::npos &&
                 shape.radius > 0.0)
        {
            outer_radius = shape.radius;
            has_shape_outer = true;
        }
    }

    if (!has_shape_center && (cx == 0.0 && cy == 0.0))
        return stats;

    double min_radius = std::numeric_limits<double>::max();
    double max_radius = 0.0;
    double sum_radius = 0.0;

    for (const auto& shape : shapes)
    {
        if (shape.owner_type != "FindCircle" ||
            shape.semantic_role != "measure_points")
        {
            continue;
        }

        for (size_t i = 0; i + 1 < shape.points.size(); i += 2)
        {
            const double dx = shape.points[i] - cx;
            const double dy = shape.points[i + 1] - cy;
            const double radius = std::sqrt(dx * dx + dy * dy);

            min_radius = std::min(min_radius, radius);
            max_radius = std::max(max_radius, radius);
            sum_radius += radius;
            ++stats.count;

            if (inner_radius > 0.0 && radius < inner_radius - 0.5)
                ++stats.inside_inner_count;
            if (outer_radius > 0.0 && radius > outer_radius + 0.5)
                ++stats.outside_outer_count;
        }
    }

    if (stats.count > 0)
    {
        stats.min_radius = min_radius;
        stats.avg_radius = sum_radius / static_cast<double>(stats.count);
        stats.max_radius = max_radius;
    }

    return stats;
}

CxScriptResultPackage BuildCxScriptResultPackage(
    const CxScriptExecutionCapture& capture)
{
    CxScriptResultPackage pkg;

    pkg.runtime_globals_after = capture.runtime_globals;
    pkg.shapes = capture.shapes;

    pkg.status = capture.runtime_completed ? "executed" : "not_executed";
    pkg.failure_stage = capture.failure_stage;
    pkg.reason = capture.reason;

    pkg.metrics["elapsed_ms"] = capture.elapsed_ms;
    pkg.metrics["budget_ms"] = capture.budget_ms;
    pkg.metrics["max_steps"] = capture.max_steps;
    pkg.metrics["max_scan_lines"] = capture.max_scan_lines;
    pkg.metrics["max_samples"] = capture.max_samples;
    pkg.metrics["scan_line_count"] = capture.scan_line_count;
    pkg.metrics["sample_count"] = capture.sample_count;
    pkg.metrics["tool_effective_method"] = capture.tool_method;
    pkg.metrics["tool_effective_threshold"] = capture.tool_threshold;
    pkg.metrics["tool_effective_wgap"] = capture.tool_wgap;
    pkg.metrics["tool_effective_hgap"] = capture.tool_hgap;
    pkg.metrics["tool_effective_linegap"] = capture.tool_linegap;
    pkg.metrics["tool_effective_min_edge_run_width_px"] =
        capture.tool_min_edge_run_width_px;
    pkg.metrics["tool_input_line_x0"] = capture.tool_input_line_x0;
    pkg.metrics["tool_input_line_y0"] = capture.tool_input_line_y0;
    pkg.metrics["tool_input_line_x1"] = capture.tool_input_line_x1;
    pkg.metrics["tool_input_line_y1"] = capture.tool_input_line_y1;
    pkg.metrics["tool_input_line_half_width"] =
        capture.tool_input_line_half_width;
    pkg.metrics["tool_input_circle_cx"] = capture.tool_input_circle_cx;
    pkg.metrics["tool_input_circle_cy"] = capture.tool_input_circle_cy;
    pkg.metrics["tool_input_circle_px"] = capture.tool_input_circle_px;
    pkg.metrics["tool_input_circle_py"] = capture.tool_input_circle_py;
    pkg.metrics["tool_input_circle_gap"] = capture.tool_input_circle_gap;
    pkg.metrics["scan_rows_examined"] = capture.scan_rows_examined;
    pkg.metrics["scan_rows_with_foreground"] = capture.scan_rows_with_foreground;
    pkg.metrics["scan_runs_total"] = capture.scan_runs_total;
    pkg.metrics["scan_runs_within_length_limit"] = capture.scan_runs_within_length_limit;
    pkg.metrics["scan_runs_over_length_limit"] = capture.scan_runs_over_length_limit;
    pkg.metrics["scan_runs_rejected_by_min_edge_width"] =
        capture.scan_runs_rejected_by_min_edge_width;
    pkg.metrics["scan_runs_rejected_by_selection"] = capture.scan_runs_rejected_by_selection;
    pkg.metrics["scan_runs_rejected_near_endpoint"] = capture.scan_runs_rejected_near_endpoint;
    pkg.metrics["scan_points_emitted"] = capture.scan_points_emitted;
    pkg.metrics["boundary_trigger_enabled"] = capture.boundary_trigger_enabled;
    pkg.metrics["boundary_trigger_mode"] = capture.boundary_trigger_mode;
    pkg.metrics["boundary_trigger_polarity"] = capture.boundary_trigger_polarity;
    pkg.metrics["boundary_trigger_selection"] = capture.boundary_trigger_selection;
    pkg.metrics["boundary_trigger_scan_rows_evaluated"] =
        capture.boundary_trigger_scan_rows_evaluated;
    pkg.metrics["boundary_trigger_candidate_count"] =
        capture.boundary_trigger_candidate_count;
    pkg.metrics["boundary_trigger_points_emitted"] =
        capture.boundary_trigger_points_emitted;
    pkg.metrics["boundary_trigger_rejected_no_candidate"] =
        capture.boundary_trigger_rejected_no_candidate;
    pkg.metrics["boundary_trigger_rejected_endpoint"] =
        capture.boundary_trigger_rejected_endpoint;
    pkg.metrics["findline_point_consistency_enabled"] = capture.findline_point_consistency_enabled;
    pkg.metrics["findline_point_consistency_range"] = capture.findline_point_consistency_range;
    pkg.metrics["findline_point_consistency_input_points"] = capture.findline_point_consistency_input_points;
    pkg.metrics["findline_point_consistency_output_points"] = capture.findline_point_consistency_output_points;
    pkg.metrics["findline_point_consistency_removed_points"] = capture.findline_point_consistency_removed_points;
    pkg.metrics["findcircle_point_consistency_enabled"] = capture.circle_point_consistency_enabled;
    pkg.metrics["findcircle_point_consistency_range"] = capture.circle_point_consistency_range;
    pkg.metrics["findcircle_point_consistency_input_points"] = capture.circle_point_consistency_input_points;
    pkg.metrics["findcircle_point_consistency_output_points"] = capture.circle_point_consistency_output_points;
    pkg.metrics["findcircle_point_consistency_removed_points"] = capture.circle_point_consistency_removed_points;
    pkg.metrics["findline_selected_edge_index"] = capture.findline_selected_edge_index;
    pkg.metrics["findline_evaluated_edge_count"] = capture.findline_evaluated_edge_count;
    pkg.metrics["findline_best_edge_index"] = capture.findline_best_edge_index;
    pkg.metrics["findline_best_edge_score"] = capture.findline_best_edge_score;
    pkg.metrics["strategy_id"] = capture.strategy_id;
    pkg.metrics["selected_method"] = capture.selected_method;
    pkg.metrics["selected_threshold"] = capture.selected_threshold;
    pkg.metrics["selected_wgap"] = capture.selected_wgap;
    pkg.metrics["selected_hgap"] = capture.selected_hgap;
    pkg.metrics["selected_linegap"] = capture.selected_linegap;
    pkg.metrics["selected_filterprofile"] = capture.selected_filterprofile;
    pkg.metrics["selected_min_edge_run_width_px"] =
        capture.selected_min_edge_run_width_px;
    // Keep injection and script echo separate.  A missing script echo must
    // never be reported as if the CLI/manifest failed to inject its value.
    const auto readRuntimeGlobal = [&capture](const char* name) -> double
    {
        const auto it = capture.runtime_globals.find(name);
        return it == capture.runtime_globals.end() ? 0.0 : it->second;
    };
    const auto hasRuntimeGlobal = [&capture](const char* name) -> bool
    {
        return capture.runtime_globals.find(name) != capture.runtime_globals.end();
    };
    pkg.metrics["injected_threshold"] = readRuntimeGlobal("global_threshold");
    pkg.metrics["injected_method"] = readRuntimeGlobal("global_method");
    pkg.metrics["injected_wgap"] = readRuntimeGlobal("global_wgap");
    pkg.metrics["injected_hgap"] = readRuntimeGlobal("global_hgap");
    pkg.metrics["injected_linegap"] = readRuntimeGlobal("global_linegap");
    pkg.metrics["injected_min_edge_run_width_px"] =
        readRuntimeGlobal("global_min_edge_run_width_px");
    pkg.metrics["injected_filterprofile"] = readRuntimeGlobal("global_filterprofile");
    pkg.metrics["script_selected_threshold"] = capture.selected_threshold;
    pkg.metrics["script_selected_method"] = capture.selected_method;
    pkg.metrics["valid_points_count"] = capture.valid_points_count;
    pkg.metrics["runtime_valid_points_count"] = capture.valid_points_count;
    pkg.metrics["global_valid_points_count"] = readRuntimeGlobal("global_valid_points_count");
    pkg.metrics["runtime_has_fit_line"] = capture.has_fit_line ? 1.0 : 0.0;
    pkg.metrics["global_has_fit_line"] = readRuntimeGlobal("global_has_fit_line");
    pkg.metrics["runtime_has_fit_circle"] = capture.has_fit_circle ? 1.0 : 0.0;
    pkg.metrics["global_has_fit_circle"] = readRuntimeGlobal("global_has_fit_circle");
    pkg.metrics["runtime_global_valid_points_count_mismatch"] =
        hasRuntimeGlobal("global_valid_points_count") &&
        static_cast<int>(readRuntimeGlobal("global_valid_points_count")) != capture.valid_points_count
            ? 1.0 : 0.0;
    pkg.metrics["runtime_global_has_fit_line_mismatch"] =
        hasRuntimeGlobal("global_has_fit_line") &&
        ((readRuntimeGlobal("global_has_fit_line") != 0.0) != capture.has_fit_line)
            ? 1.0 : 0.0;
    pkg.metrics["runtime_global_has_fit_circle_mismatch"] =
        hasRuntimeGlobal("global_has_fit_circle") &&
        ((readRuntimeGlobal("global_has_fit_circle") != 0.0) != capture.has_fit_circle)
            ? 1.0 : 0.0;
    pkg.metrics["circle_radius"] = capture.circle_radius;
    pkg.metrics["avgdist"] = capture.avgdist;
    pkg.metrics["local_support"] = capture.boundary_coverage_ratio;
    pkg.metrics["local_mean_distance"] = capture.boundary_residual_rmse_px;
    pkg.metrics["fit_offset"] = capture.boundary_residual_p95_px;
    pkg.metrics["boundary_coverage_ratio"] = capture.boundary_coverage_ratio;
    pkg.metrics["boundary_residual_rmse_px"] = capture.boundary_residual_rmse_px;
    pkg.metrics["boundary_residual_p95_px"] = capture.boundary_residual_p95_px;
    pkg.metrics["boundary_residual_max_px"] = capture.boundary_residual_max_px;
    pkg.metrics["boundary_subpixel_offset_mean"] = capture.boundary_subpixel_offset_mean;
    pkg.metrics["boundary_subpixel_offset_stddev"] = capture.boundary_subpixel_offset_stddev;
    pkg.metrics["boundary_localization_sigma_mean_px"] = capture.boundary_localization_sigma_mean_px;

    pkg.metrics["boundary_reliability_score"] = capture.boundary_reliability_score;
    const double circle_gauge_cx = readRuntimeGlobal("global_circle_cx");
    const double circle_gauge_cy = readRuntimeGlobal("global_circle_cy");
    const double circle_inner_radius = readRuntimeGlobal("global_circle_inner_radius");
    const double circle_outer_radius = readRuntimeGlobal("global_circle_outer_radius");
    const CxCircleMeasurePointRadiusStats circle_radius_stats =
        ComputeCircleMeasurePointRadiusStats(
            capture.shapes,
            circle_gauge_cx,
            circle_gauge_cy,
            circle_inner_radius,
            circle_outer_radius);
    pkg.metrics["circle_measure_point_count_for_radius_check"] =
        circle_radius_stats.count;
    pkg.metrics["circle_measure_point_radius_min"] =
        circle_radius_stats.min_radius;
    pkg.metrics["circle_measure_point_radius_avg"] =
        circle_radius_stats.avg_radius;
    pkg.metrics["circle_measure_point_radius_max"] =
        circle_radius_stats.max_radius;
    pkg.metrics["circle_measure_points_inside_inner_count"] =
        circle_radius_stats.inside_inner_count;
    pkg.metrics["circle_measure_points_outside_outer_count"] =
        circle_radius_stats.outside_outer_count;
    pkg.metrics["result_rect_count"] = capture.result_rect_count;
    pkg.metrics["top1_rect_x"] = capture.top1_rect_x;
    pkg.metrics["top1_rect_y"] = capture.top1_rect_y;
    pkg.metrics["top1_rect_w"] = capture.top1_rect_w;
    pkg.metrics["top1_rect_h"] = capture.top1_rect_h;
    pkg.metrics["model_point_count"] = capture.model_point_count;
    pkg.metrics["fastmatch_learn_a_count"] = capture.fastmatch_learn_a_count;
    pkg.metrics["fastmatch_learn_b_count"] = capture.fastmatch_learn_b_count;
    pkg.metrics["fastmatch_learn_a2_count"] = capture.fastmatch_learn_a2_count;
    pkg.metrics["fastmatch_learn_b2_count"] = capture.fastmatch_learn_b2_count;
    pkg.metrics["fastmatch_learn_status_code"] = capture.fastmatch_learn_status_code;
    pkg.metrics["fastmatch_normal_trace_candidate_count"] =
        capture.fastmatch_normal_trace.candidate_count;
    pkg.metrics["fastmatch_normal_trace_deduplicated_count"] =
        capture.fastmatch_normal_trace.deduplicated_count;
    pkg.metrics["fastmatch_normal_trace_point_count"] =
        capture.fastmatch_normal_trace.trace_point_count;
    pkg.metrics["fastmatch_normal_trace_pair_count"] =
        capture.fastmatch_normal_trace.normal_pair_count;
    pkg.metrics["fastmatch_normal_trace_directional_side_count"] =
        capture.fastmatch_normal_trace.directional_side_count;
    pkg.metrics["fastmatch_normal_trace_segment_count"] =
        capture.fastmatch_normal_trace.trace_segment_count;
    pkg.metrics["fastmatch_normal_trace_closure_error_px"] =
        capture.fastmatch_normal_trace.closure_error_px;
    pkg.metrics["fastmatch_normal_trace_max_consecutive_gap_px"] =
        capture.fastmatch_normal_trace.max_consecutive_gap_px;
    pkg.metrics["fastmatch_normal_trace_gradient_coverage"] =
        capture.fastmatch_normal_trace.gradient_coverage;
    pkg.metrics["fastmatch_normal_trace_selected_anchor_coverage"] =
        capture.fastmatch_normal_trace.selected_anchor_coverage;
    pkg.metrics["fastmatch_normal_trace_generated_conclusion_count"] =
        capture.fastmatch_normal_trace.generated_conclusion_count;
    pkg.metrics["fastmatch_normal_trace_dijkstra_source_count"] =
        static_cast<double>(
            capture.fastmatch_normal_trace.dijkstra_trace_points.size());
    pkg.metrics["fastmatch_normal_trace_derived_trace_point_count"] =
        static_cast<double>(
            capture.fastmatch_normal_trace.derived_trace_points.size());
    pkg.metrics["fastmatch_normal_trace_derived_junction_count"] =
        static_cast<double>(
            capture.fastmatch_normal_trace.derived_junction_points.size());
    pkg.metrics["fastmatch_normal_trace_tangent_intersection_junction_count"] =
        static_cast<double>(std::count(
            capture.fastmatch_normal_trace.derived_junction_modes.begin(),
            capture.fastmatch_normal_trace.derived_junction_modes.end(), 1));
    pkg.metrics["fastmatch_normal_trace_findline_bound_pairs"] =
        capture.fastmatch_normal_trace.normal_pair_findline_bound_count;
    pkg.metrics["fastmatch_normal_trace_binding_misses"] =
        capture.fastmatch_normal_trace.normal_pair_binding_miss_count;
    pkg.metrics["fastmatch_normal_trace_corner_rejected"] =
        capture.fastmatch_normal_trace.normal_pair_corner_rejected_count;
    pkg.metrics["fastmatch_normal_trace_loop_erased_points"] =
        capture.fastmatch_normal_trace.loop_erased_point_count;
    pkg.facts["fastmatch_normal_trace_executed"] =
        capture.fastmatch_normal_trace.executed ? "true" : "false";
    pkg.facts["fastmatch_normal_trace_succeeded"] =
        capture.fastmatch_normal_trace.succeeded ? "true" : "false";
    pkg.facts["fastmatch_normal_trace_reason"] =
        capture.fastmatch_normal_trace.reason;
    pkg.facts["fastmatch_normal_trace_point_source"] =
        "directional_findline_selected_conclusions";
    pkg.facts["fastmatch_normal_trace_polarity_source"] =
        "directional_findline_domain";
    pkg.facts["fastmatch_normal_trace_image_reprocessing"] = "false";
    pkg.facts["fastmatch_form_fit_status"] =
        capture.fastmatch_form_fit.status;
    pkg.facts["fastmatch_form_fit_failure_stage"] =
        capture.fastmatch_form_fit.failure_stage;
    pkg.facts["fastmatch_form_fit_reference_model"] =
        capture.fastmatch_form_fit.reference_model_id;
    pkg.facts["fastmatch_form_fit_observed_model"] =
        capture.fastmatch_form_fit.observed_model_id;
    pkg.facts["fastmatch_form_fit_gauge_id"] =
        capture.fastmatch_form_fit_gauge.gauge_id;
    pkg.metrics["fastmatch_form_fit_reference_sparse_count"] =
        capture.fastmatch_form_fit.reference_sparse_count;
    pkg.metrics["fastmatch_form_fit_reference_dense_count"] =
        capture.fastmatch_form_fit.reference_dense_count;
    pkg.metrics["fastmatch_form_fit_observed_dense_count"] =
        capture.fastmatch_form_fit.observed_dense_count;
    pkg.metrics["fastmatch_form_fit_subpixel_point_count"] =
        capture.fastmatch_reference_shape_model.subpixel_point_count;
    pkg.metrics["fastmatch_form_fit_structural_anchor_count"] =
        capture.fastmatch_form_fit.structural_anchor_count;
    pkg.metrics["fastmatch_form_fit_mutual_count"] =
        capture.fastmatch_form_fit.dense_mutual_count;
    pkg.metrics["fastmatch_form_fit_forward_coverage"] =
        capture.fastmatch_form_fit.forward_coverage;
    pkg.metrics["fastmatch_form_fit_reverse_coverage"] =
        capture.fastmatch_form_fit.reverse_coverage;
    pkg.metrics["fastmatch_form_fit_mutual_coverage"] =
        capture.fastmatch_form_fit.mutual_coverage;
    pkg.metrics["fastmatch_form_fit_symmetric_residual_px"] =
        capture.fastmatch_form_fit.symmetric_residual_px;
    pkg.metrics["fastmatch_form_fit_normal_residual_deg"] =
        capture.fastmatch_form_fit.normal_residual_deg;
    pkg.metrics["fastmatch_form_fit_score"] =
        capture.fastmatch_form_fit.score;
    pkg.metrics["fastmatch_form_fit_elapsed_ms"] =
        capture.fastmatch_form_fit.elapsed_ms;
    pkg.metrics["fastmatch_form_fit_gauge_element_count"] =
        static_cast<double>(capture.fastmatch_form_fit_gauge.elements.size());
    pkg.metrics["fastmatch_form_fit_gauge_constraint_count"] =
        static_cast<double>(capture.fastmatch_form_fit_gauge.constraints.size());
    pkg.metrics["candidate_count"] = capture.candidate_count;
    pkg.metrics["best_score"] = capture.best_score;
    pkg.metrics["rendered_measure_points_count"] = capture.rendered_measure_points_count;
    pkg.metrics["rendered_result_count"] = capture.rendered_result_count;
    pkg.metrics["result_overlay_changed_pixels"] = capture.result_overlay_changed_pixels;

    pkg.metrics["ellipse_cx"] = capture.ellipse_cx;
    pkg.metrics["ellipse_cy"] = capture.ellipse_cy;
    pkg.metrics["ellipse_radius_x"] = capture.ellipse_radius_x;
    pkg.metrics["ellipse_radius_y"] = capture.ellipse_radius_y;
    pkg.metrics["ellipse_angle_deg"] = capture.ellipse_angle_deg;
    pkg.metrics["ellipse_selected_edge_index"] = capture.ellipse_selected_edge_index;
    pkg.metrics["ellipse_scan_candidate_lines"] = capture.ellipse_scan_candidate_lines;
    pkg.metrics["ellipse_scan_total_candidates"] = capture.ellipse_scan_total_candidates;
    pkg.metrics["ellipse_scan_accepted_points_before_gate"] = capture.ellipse_scan_accepted_points_before_gate;
    pkg.metrics["ellipse_accepted_min_boundary_ratio"] = capture.ellipse_accepted_min_boundary_ratio;
    pkg.metrics["ellipse_accepted_max_boundary_ratio"] = capture.ellipse_accepted_max_boundary_ratio;
    pkg.metrics["ellipse_accepted_avg_boundary_ratio"] = capture.ellipse_accepted_avg_boundary_ratio;
    pkg.metrics["ellipse_scan_lines_cross_outside_ellipse_count"] = capture.ellipse_scan_lines_cross_outside_ellipse_count;
    pkg.metrics["ellipse_scan_endpoint_norm_min"] = capture.ellipse_scan_endpoint_norm_min;
    pkg.metrics["ellipse_scan_endpoint_norm_avg"] = capture.ellipse_scan_endpoint_norm_avg;
    pkg.metrics["ellipse_scan_endpoint_norm_max"] = capture.ellipse_scan_endpoint_norm_max;
    pkg.metrics["ellipse_accepted_points_outside_ellipse_count"] = capture.ellipse_accepted_points_outside_ellipse_count;
    pkg.metrics["ellipse_accepted_point_norm_min"] = capture.ellipse_accepted_point_norm_min;
    pkg.metrics["ellipse_accepted_point_norm_avg"] = capture.ellipse_accepted_point_norm_avg;
    pkg.metrics["ellipse_accepted_point_norm_max"] = capture.ellipse_accepted_point_norm_max;
    pkg.metrics["ellipse_rejected_boundary_band_candidate_count"] =
        capture.ellipse_rejected_boundary_band_candidate_count;
    pkg.metrics["ellipse_rejected_boundary_band_norm_min"] =
        capture.ellipse_rejected_boundary_band_norm_min;
    pkg.metrics["ellipse_rejected_boundary_band_norm_avg"] =
        capture.ellipse_rejected_boundary_band_norm_avg;
    pkg.metrics["ellipse_rejected_boundary_band_norm_max"] =
        capture.ellipse_rejected_boundary_band_norm_max;
    pkg.metrics["ellipse_point_consistency_enabled"] =
        capture.ellipse_point_consistency_enabled;
    pkg.metrics["ellipse_point_consistency_range"] =
        capture.ellipse_point_consistency_range;
    pkg.metrics["ellipse_point_consistency_input_points"] =
        capture.ellipse_point_consistency_input_points;
    pkg.metrics["ellipse_point_consistency_output_points"] =
        capture.ellipse_point_consistency_output_points;
    pkg.metrics["ellipse_point_consistency_removed_points"] =
        capture.ellipse_point_consistency_removed_points;

    pkg.metrics["fastmatch_model_width"] = capture.fastmatch_model_width;
    pkg.metrics["fastmatch_model_height"] = capture.fastmatch_model_height;
    pkg.metrics["fastmatch_pattern_a_count"] = capture.fastmatch_pattern_a_count;
    pkg.metrics["fastmatch_pattern_b_count"] = capture.fastmatch_pattern_b_count;
    pkg.metrics["fastmatch_pattern_a_x"] = capture.fastmatch_pattern_a_x;
    pkg.metrics["fastmatch_pattern_a_y"] = capture.fastmatch_pattern_a_y;
    pkg.metrics["fastmatch_pattern_a_width"] = capture.fastmatch_pattern_a_width;
    pkg.metrics["fastmatch_pattern_a_height"] = capture.fastmatch_pattern_a_height;
    pkg.metrics["fastmatch_pattern_b_x"] = capture.fastmatch_pattern_b_x;
    pkg.metrics["fastmatch_pattern_b_y"] = capture.fastmatch_pattern_b_y;
    pkg.metrics["fastmatch_pattern_b_width"] = capture.fastmatch_pattern_b_width;
    pkg.metrics["fastmatch_pattern_b_height"] = capture.fastmatch_pattern_b_height;
    pkg.metrics["fastmatch_match_call_count"] = capture.fastmatch_match_call_count;
    pkg.metrics["fastmatch_match_ab_call_count"] = capture.fastmatch_match_ab_call_count;
    pkg.metrics["fastmatch_match_sample_ab_call_count"] = capture.fastmatch_match_sample_ab_call_count;
    pkg.metrics["fastmatch_match_last_stage"] = capture.fastmatch_match_last_stage;
    pkg.metrics["fastmatch_match_image_width"] = capture.fastmatch_match_image_width;
    pkg.metrics["fastmatch_match_image_height"] = capture.fastmatch_match_image_height;
    pkg.metrics["fastmatch_learn_rect_x0"] = capture.fastmatch_learn_rect_x0;
    pkg.metrics["fastmatch_learn_rect_y0"] = capture.fastmatch_learn_rect_y0;
    pkg.metrics["fastmatch_learn_rect_x1"] = capture.fastmatch_learn_rect_x1;
    pkg.metrics["fastmatch_learn_rect_y1"] = capture.fastmatch_learn_rect_y1;
    pkg.metrics["fastmatch_match_rect_x0"] = capture.fastmatch_match_rect_x0;
    pkg.metrics["fastmatch_match_rect_y0"] = capture.fastmatch_match_rect_y0;
    pkg.metrics["fastmatch_match_rect_x1"] = capture.fastmatch_match_rect_x1;
    pkg.metrics["fastmatch_match_rect_y1"] = capture.fastmatch_match_rect_y1;
    pkg.metrics["fastmatch_raw_probe_count"] = capture.fastmatch_raw_probe_count;
    pkg.metrics["fastmatch_raw_threshold_hit_count"] = capture.fastmatch_raw_threshold_hit_count;
    pkg.metrics["fastmatch_result_to_list_count"] = capture.fastmatch_result_to_list_count;
    pkg.metrics["fastmatch_candidate_insert_count"] = capture.fastmatch_candidate_insert_count;
    pkg.metrics["fastmatch_candidate_replace_count"] = capture.fastmatch_candidate_replace_count;
    pkg.metrics["fastmatch_candidate_reject_count"] = capture.fastmatch_candidate_reject_count;
    pkg.metrics["fastmatch_rotate_budget_exceeded"] =
        capture.fastmatch_rotate_budget_exceeded ? 1.0 : 0.0;
    pkg.metrics["fastmatch_rotate_candidate_count"] = capture.fastmatch_rotate_candidate_count;
    pkg.metrics["fastmatch_rotate_probe_count"] = capture.fastmatch_rotate_probe_count;
    pkg.metrics["fastmatch_rotate_sample_count"] = capture.fastmatch_rotate_sample_count;
    pkg.metrics["fastmatch_rotate_elapsed_ms"] = capture.fastmatch_rotate_elapsed_ms;
    const CxFastMatchTransformSearchEvidence& transform_search =
        capture.fastmatch_transform_search;
    pkg.metrics["fastmatch_transform_executed"] = transform_search.executed ? 1.0 : 0.0;
    pkg.metrics["fastmatch_transform_converged"] = transform_search.converged ? 1.0 : 0.0;
    pkg.metrics["fastmatch_transform_budget_exceeded"] = transform_search.budget_exceeded ? 1.0 : 0.0;
    pkg.metrics["fastmatch_transform_best_score"] = transform_search.best_score;
    pkg.metrics["fastmatch_transform_gradient_score"] = transform_search.gradient_score;
    pkg.metrics["fastmatch_transform_continuity_score"] = transform_search.continuity_score;
    pkg.metrics["fastmatch_transform_geometric_residual_px"] = transform_search.geometric_residual_px;
    pkg.metrics["fastmatch_transform_rigid_baseline_score"] = transform_search.rigid_baseline_score;
    pkg.metrics["fastmatch_transform_evaluated_candidates"] = transform_search.evaluated_candidates;
    pkg.metrics["fastmatch_transform_sample_count"] = transform_search.sample_count;
    pkg.metrics["fastmatch_transform_elapsed_ms"] = transform_search.elapsed_ms;
    pkg.metrics["fastmatch_transform_calibration_applied"] =
        transform_search.calibration_applied ? 1.0 : 0.0;
    pkg.metrics["fastmatch_transform_calibration_reprojection_rmse_px"] =
        transform_search.calibration_reprojection_rmse_px;
    pkg.metrics["fastmatch_transform_best_physical_cx"] = transform_search.best_physical_cx;
    pkg.metrics["fastmatch_transform_best_physical_cy"] = transform_search.best_physical_cy;
    pkg.facts["fastmatch_transform_seed_source"] = transform_search.seed_source;
    pkg.facts["fastmatch_transform_seed_model_id"] = transform_search.seed_model_id;
    pkg.facts["fastmatch_transform_seed_angle_convention"] = transform_search.seed_angle_convention;
    pkg.facts["fastmatch_transform_failure_stage"] = transform_search.failure_stage;
    pkg.facts["fastmatch_transform_calibration_snapshot_hash"] =
        transform_search.calibration_snapshot_hash;
    pkg.facts["fastmatch_transform_calibration_source_ref"] =
        transform_search.calibration_source_ref;
    pkg.facts["fastmatch_transform_calibration_coordinate_frame_id"] =
        transform_search.calibration_coordinate_frame_id;
    pkg.facts["fastmatch_transform_calibration_xy_unit"] =
        transform_search.calibration_xy_unit;

    pkg.metrics["actual_findsetting"] = capture.actual_findsetting;
    pkg.metrics["object_filter_borw"] = capture.object_filter_borw;
    pkg.metrics["findobject_strategy_id"] = capture.object_filter_strategy_id;
    pkg.metrics["object_filter_min"] = capture.object_filter_min;
    pkg.metrics["object_filter_max"] = capture.object_filter_max;
    pkg.metrics["findobject_component_count"] = capture.object_component_count;
    pkg.metrics["findobject_component_accepted_count"] = capture.object_component_accepted_count;
    pkg.metrics["findobject_component_rejected_count"] = capture.object_component_rejected_count;
    pkg.metrics["findobject_component_max_area"] = capture.object_component_max_area;
    pkg.metrics["findobject_component_max_width"] = capture.object_component_max_width;
    pkg.metrics["findobject_component_max_height"] = capture.object_component_max_height;
    pkg.metrics["findobject_foreground_before"] = capture.object_foreground_before;
    pkg.metrics["findobject_foreground_after"] = capture.object_foreground_after;
    pkg.metrics["findobject_white_component_count"] = capture.object_white_component_count;
    pkg.metrics["findobject_white_accepted_count"] = capture.object_white_accepted_count;
    pkg.metrics["findobject_white_rejected_count"] = capture.object_white_rejected_count;
    pkg.metrics["findobject_black_component_count"] = capture.object_black_component_count;
    pkg.metrics["findobject_black_accepted_count"] = capture.object_black_accepted_count;
    pkg.metrics["findobject_black_rejected_count"] = capture.object_black_rejected_count;

    pkg.metrics["findobject_measurement_count"] =
        static_cast<double>(capture.findobject_measurements.size());
    if (!capture.findobject_measurements.empty())
    {
        const CxFindObjectMeasurementEvidence& top =
            capture.findobject_measurements.front();
        pkg.metrics["findobject_top1_area"] = top.projected_area;
        pkg.metrics["findobject_top1_perimeter"] = top.gwyddion_perimeter;
        pkg.metrics["findobject_top1_equivalent_side"] = top.equivalent_side;
        pkg.metrics["findobject_top1_circularity"] = top.circularity;
        pkg.metrics["findobject_top1_solidity"] = top.solidity;
        pkg.metrics["findobject_top1_major_axis"] = top.major_axis_length;
        pkg.metrics["findobject_top1_minor_axis"] = top.minor_axis_length;
        pkg.metrics["findobject_top1_orientation_deg"] = top.orientation_deg;
        pkg.metrics["findobject_top1_feret_max"] = top.feret_max;
        pkg.metrics["findobject_top1_feret_min"] = top.feret_min;
        pkg.metrics["findobject_top1_inscribed_radius"] =
            top.inscribed_radius;
        pkg.metrics["findobject_top1_enclosing_radius"] =
            top.enclosing_radius;
    }
    pkg.metrics["fit_filter_input_count"] = capture.fit_filter_input_count;
    pkg.metrics["fit_filter_kept_count"] = capture.fit_filter_kept_count;
    pkg.metrics["fit_filter_rejected_count"] = capture.fit_filter_rejected_count;
    pkg.metrics["fit_filter_sigma"] = capture.fit_filter_sigma;
    pkg.metrics["fit_filter_threshold"] = capture.fit_filter_threshold;
    pkg.metrics["findrect_top_points"] = capture.findrect_top_points;
    pkg.metrics["findrect_bottom_points"] = capture.findrect_bottom_points;
    pkg.metrics["findrect_left_points"] = capture.findrect_left_points;
    pkg.metrics["findrect_right_points"] = capture.findrect_right_points;
    pkg.metrics["findrect_coarse_score"] = capture.findrect_coarse_score;
    pkg.metrics["findrect_refine_score"] = capture.findrect_refine_score;

    pkg.metrics["segmentation_status_code"] = capture.segmentation_status_code;
    pkg.metrics["segmentation_contour_count"] = capture.segmentation_contour_count;
    pkg.metrics["segmentation_primary_area"] = capture.segmentation_primary_area;
    pkg.metrics["torch_ok"] = capture.torch_ok;
    pkg.metrics["torch_error_code"] = capture.torch_error_code;
    pkg.metrics["torch_train_ms"] = capture.torch_train_ms;
    pkg.metrics["torch_infer_ms"] = capture.torch_infer_ms;
    pkg.metrics["torch_total_ms"] = capture.torch_total_ms;
    pkg.metrics["torch_result_count"] = capture.torch_result_count;

    pkg.facts["execution_mode"] = "sequential";
    pkg.facts["algorithm_executed"] = capture.runtime_completed ? "true" : "false";
    pkg.facts["budget_exceeded"] = capture.budget_exceeded ? "true" : "false";
    pkg.facts["has_fit_line"] = capture.has_fit_line ? "true" : "false";
    pkg.facts["has_fit_circle"] = capture.has_fit_circle ? "true" : "false";
    pkg.facts["has_fit_ellipse"] = capture.has_fit_ellipse ? "true" : "false";
    pkg.facts["has_result_rect"] = capture.has_result_rect ? "true" : "false";
    pkg.facts["has_result_box"] = capture.has_result_box ? "true" : "false";
    pkg.facts["has_best_result"] = capture.has_best_result ? "true" : "false";
    pkg.facts["failure_stage"] = capture.failure_stage;
    pkg.facts["reason"] = capture.reason;
    pkg.facts["runtime_result_source"] = "runtime_capture";
    pkg.facts["global_result_echo_available"] =
        (hasRuntimeGlobal("global_valid_points_count") ||
         hasRuntimeGlobal("global_has_fit_line") ||
         hasRuntimeGlobal("global_has_fit_circle"))
            ? "true" : "false";
    pkg.facts["runtime_global_result_mismatch"] =
        (pkg.metrics["runtime_global_valid_points_count_mismatch"] != 0.0 ||
         pkg.metrics["runtime_global_has_fit_line_mismatch"] != 0.0 ||
         pkg.metrics["runtime_global_has_fit_circle_mismatch"] != 0.0)
            ? "true" : "false";

    pkg.facts["ellipse_candidate_policy"] = capture.ellipse_candidate_policy;
    pkg.facts["ellipse_scan_geometry_policy"] = capture.ellipse_scan_geometry_policy;
    pkg.facts["actual_findsetting"] = std::to_string(capture.actual_findsetting);
    pkg.facts["object_prefilter_requested"] = capture.object_prefilter_requested ? "true" : "false";
    pkg.facts["object_prefilter_applied"] = capture.object_prefilter_applied ? "true" : "false";
    pkg.facts["findobject_algorithm_branch"] = capture.object_algorithm_branch;
    pkg.facts["findobject_strategy_semantics"] =
        "0=auto_by_filter,1=measure_region_growth,2=measure_fast_region_growth,3=connected_components,4=peak_local_bfs_diagnostic";
    pkg.facts["script_selected_threshold_matches_injected"] =
        capture.selected_threshold == static_cast<int>(readRuntimeGlobal("global_threshold"))
            ? "true" : "false";
    pkg.facts["findrect_seed_valid"] = capture.findrect_seed_valid ? "true" : "false";
    pkg.facts["findrect_top_valid"] = capture.findrect_top_valid ? "true" : "false";
    pkg.facts["findrect_bottom_valid"] = capture.findrect_bottom_valid ? "true" : "false";
    pkg.facts["findrect_left_valid"] = capture.findrect_left_valid ? "true" : "false";
    pkg.facts["findrect_right_valid"] = capture.findrect_right_valid ? "true" : "false";
    pkg.facts["segmentation_result_ref"] = capture.segmentation_result_ref;
    pkg.facts["segmentation_mask_ref"] = capture.segmentation_mask_ref;
    pkg.facts["segmentation_contour_ref"] = capture.segmentation_contour_ref;
    pkg.facts["segmentation_overlay_ref"] = capture.segmentation_overlay_ref;
    pkg.facts["segmentation_task_id"] = capture.segmentation_task_id;

    pkg.facts["segmentation_model_id"] = capture.segmentation_model_id;

    pkg.facts["segmentation_model_package_ref"] = capture.segmentation_model_package_ref;

    pkg.facts["segmentation_manifest_path"] = capture.segmentation_manifest_path;

    pkg.facts["segmentation_postprocess_profile"] = capture.segmentation_postprocess_profile;

    pkg.facts["segmentation_parameter_profile_ref"] = capture.segmentation_parameter_profile_ref;

    pkg.facts["segmentation_region_count"] = std::to_string(capture.segmentation_region_count);

    pkg.facts["segmentation_raw_result_available"] = capture.segmentation_raw_result_available ? "true" : "false";

    pkg.facts["segmentation_refined_result_available"] = capture.segmentation_refined_result_available ? "true" : "false";

    pkg.facts["segmentation_fallback_used"] = capture.segmentation_fallback_used ? "true" : "false";

    pkg.facts["segmentation_result_stage"] = capture.segmentation_result_stage;

    pkg.facts["segmentation_refinement_method"] = capture.segmentation_refinement_method;

    pkg.facts["segmentation_requested_geometry_type"] = capture.segmentation_requested_geometry_type;
    pkg.facts["segmentation_geometry_fit_status"] = capture.segmentation_geometry_fit_status;
    pkg.facts["segmentation_geometry_fit_reason"] = capture.segmentation_geometry_fit_reason;
    pkg.facts["segmentation_geometry_count"] = std::to_string(capture.segmentation_geometry_count);
    pkg.facts["segmentation_geometry_residual_px"] = std::to_string(capture.segmentation_geometry_residual_px);
    pkg.facts["segmentation_geometry_support"] = std::to_string(capture.segmentation_geometry_support);
    pkg.facts["segmentation_geometry_center_x"] = std::to_string(capture.segmentation_geometry_center_x);
    pkg.facts["segmentation_geometry_center_y"] = std::to_string(capture.segmentation_geometry_center_y);
    pkg.facts["segmentation_geometry_radius"] = std::to_string(capture.segmentation_geometry_radius);
    pkg.facts["segmentation_geometry_axis_x"] = std::to_string(capture.segmentation_geometry_axis_x);
    pkg.facts["segmentation_geometry_axis_y"] = std::to_string(capture.segmentation_geometry_axis_y);
    pkg.facts["segmentation_geometry_angle_deg"] = std::to_string(capture.segmentation_geometry_angle_deg);
    pkg.facts["segmentation_oriented_box_evidence_ref"] =
        capture.segmentation_oriented_box_evidence_ref;
    pkg.facts["segmentation_oriented_box_conclusion"] =
        capture.segmentation_oriented_box_conclusion;

    pkg.facts["segmentation_raw_result_ref"] = capture.segmentation_raw_result_ref;

    pkg.facts["segmentation_raw_mask_ref"] = capture.segmentation_raw_mask_ref;

    pkg.facts["segmentation_raw_contour_ref"] = capture.segmentation_raw_contour_ref;

    pkg.facts["segmentation_raw_overlay_ref"] = capture.segmentation_raw_overlay_ref;

    pkg.facts["segmentation_refined_result_ref"] = capture.segmentation_refined_result_ref;

    pkg.facts["segmentation_refined_mask_ref"] = capture.segmentation_refined_mask_ref;

    pkg.facts["segmentation_refined_contour_ref"] = capture.segmentation_refined_contour_ref;

    pkg.facts["segmentation_refined_overlay_ref"] = capture.segmentation_refined_overlay_ref;


    pkg.facts["torch_ok"] = capture.torch_ok != 0 ? "true" : "false";
    pkg.facts["torch_status"] = capture.torch_status;
    pkg.facts["torch_failure_stage"] = capture.torch_failure_stage;
    pkg.facts["torch_reason"] = capture.torch_reason;
    pkg.facts["torch_evidence_ref"] = capture.torch_evidence_ref;
    pkg.facts["torch_primary_visual_ref"] = capture.torch_primary_visual_ref;
    pkg.facts["torch_trainer_lifecycle_summary"] = capture.torch_trainer_lifecycle_summary;
    pkg.facts["torch_unified_mainline_summary"] = capture.torch_unified_mainline_summary;

    if (capture.contract_context)
    {
        pkg.facts["contract_pass"] = capture.contract_pass ? "true" : "false";
        pkg.facts["contract_status"] = capture.contract_status;
        pkg.facts["contract_conclusion"] = capture.contract_conclusion;
    }

    return pkg;
}

std::string FindObjectStrategyName(int strategy_id)
{
    switch (strategy_id)
    {
    case 0:
        return "auto_by_filter";
    case 1:
        return "measure_region_growth";
    case 2:
        return "measure_fast_region_growth";
    case 3:
        return "connected_components";
    case 4:
        return "peak_local_bfs_diagnostic";
    default:
        return "unknown";
    }
}

std::string ClassifyFindLineFindObjectBoundary(
    const CxScriptExecutionCapture& capture)
{
    if (capture.has_fit_line)
        return "findline_fit_available";

    if (capture.object_prefilter_applied &&
        capture.object_foreground_after > 0 &&
        capture.scan_rows_with_foreground == 0 &&
        capture.scan_runs_total == 0 &&
        capture.valid_points_count == 0)
    {
        return "findline_fail_prefilter_foreground_not_visible_to_scan_rows";
    }

    if (capture.scan_rows_with_foreground > 0 &&
        capture.scan_runs_total == 0 &&
        capture.valid_points_count == 0)
    {
        return "findline_fail_binary_saturated_or_no_segment_boundary";
    }

    if (capture.object_prefilter_requested && !capture.object_prefilter_applied)
        return "findline_fail_findobject_prefilter_not_applied";

    if (capture.object_prefilter_applied &&
        capture.object_component_count > 0 &&
        capture.object_component_accepted_count == 0)
    {
        return "findline_fail_findobject_component_rejected";
    }

    if (capture.scan_runs_total > 0 &&
        capture.scan_points_emitted == 0)
    {
        return "findline_fail_scan_runs_rejected";
    }

    if (capture.valid_points_count > 0 && !capture.has_fit_line)
        return "findline_fail_fit_degenerate";

    if (!capture.failure_stage.empty())
        return capture.failure_stage;

    return "findline_fail_unknown";
}

bool SaveFindObjectBranchEvidenceJson(
    const CxScriptExecutionCapture& capture,
    const CxScriptHeadlessOptions& options,
    const std::filesystem::path& outputPath,
    std::string& outReason)
{
    std::error_code ec;
    std::filesystem::create_directories(outputPath.parent_path(), ec);
    if (ec)
    {
        outReason = "failed to create findobject branch evidence directory: " + ec.message();
        return false;
    }

    std::ofstream file(outputPath, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        outReason = "failed to open findobject branch evidence json";
        return false;
    }

    const std::string boundary = ClassifyFindLineFindObjectBoundary(capture);

    file << "{\n";
    file << "  \"case_id\": \"" << JsonEscape(options.case_name) << "\",\n";
    file << "  \"script\": \"" << JsonEscape(options.script_path) << "\",\n";
    file << "  \"image\": \"" << JsonEscape(options.image_path) << "\",\n";
    file << "  \"runtime_result_source\": \"runtime_capture\",\n";
    file << "  \"strategy\": {\n";
    file << "    \"id\": " << capture.object_filter_strategy_id << ",\n";
    file << "    \"name\": \"" << JsonEscape(FindObjectStrategyName(capture.object_filter_strategy_id)) << "\",\n";
    file << "    \"algorithm_branch\": \"" << JsonEscape(capture.object_algorithm_branch) << "\",\n";
    file << "    \"semantics\": \"0=auto_by_filter,1=measure_region_growth,2=measure_fast_region_growth,3=connected_components,4=peak_local_bfs_diagnostic\"\n";
    file << "  },\n";
    file << "  \"input\": {\n";
    file << "    \"method\": " << capture.tool_method << ",\n";
    file << "    \"threshold\": " << capture.tool_threshold << ",\n";
    file << "    \"wgap\": " << capture.tool_wgap << ",\n";
    file << "    \"hgap\": " << capture.tool_hgap << ",\n";
    file << "    \"linegap\": " << capture.tool_linegap << ",\n";
    file << "    \"min_edge_run_width_px\": "
         << capture.tool_min_edge_run_width_px << "\n";
    file << "  },\n";
    file << "  \"findobject\": {\n";
    file << "    \"requested\": " << (capture.object_prefilter_requested ? "true" : "false") << ",\n";
    file << "    \"applied\": " << (capture.object_prefilter_applied ? "true" : "false") << ",\n";
    file << "    \"borw\": " << capture.object_filter_borw << ",\n";
    file << "    \"filter_min\": " << capture.object_filter_min << ",\n";
    file << "    \"filter_max\": " << capture.object_filter_max << ",\n";
    file << "    \"foreground_before\": " << capture.object_foreground_before << ",\n";
    file << "    \"foreground_after\": " << capture.object_foreground_after << ",\n";
    file << "    \"component_count\": " << capture.object_component_count << ",\n";
    file << "    \"accepted_count\": " << capture.object_component_accepted_count << ",\n";
    file << "    \"rejected_count\": " << capture.object_component_rejected_count << ",\n";
    file << "    \"max_area\": " << capture.object_component_max_area << ",\n";
    file << "    \"white_component_count\": " << capture.object_white_component_count << ",\n";
    file << "    \"white_accepted_count\": " << capture.object_white_accepted_count << ",\n";
    file << "    \"black_component_count\": " << capture.object_black_component_count << ",\n";
    file << R"json(    "black_accepted_count": )json"
         << capture.object_black_accepted_count << ',' << '\n';
    file << R"json(    "measurement_count": )json"
         << capture.findobject_measurements.size() << ',' << '\n';
    file << R"json(    "measurements": [)json" << '\n';
    for (std::size_t measurement_index = 0;
         measurement_index < capture.findobject_measurements.size();
         ++measurement_index)
    {
        const CxFindObjectMeasurementEvidence& measurement =
            capture.findobject_measurements[measurement_index];
        file << R"json(      {"object_index": )json"
             << measurement.object_index
             << R"json(, "component_label": )json"
             << measurement.component_label
             << R"json(, "status": ")json"
             << JsonEscape(measurement.status)
             << R"json(", "bbox_px": [)json"
             << measurement.bbox_x << ", " << measurement.bbox_y << ", "
             << measurement.bbox_w << ", " << measurement.bbox_h
             << R"json(], "pixel_count": )json" << measurement.pixel_count
             << R"json(, "pixel_size": [)json"
             << measurement.pixel_size_x << ", "
             << measurement.pixel_size_y
             << R"json(], "centroid_px": [)json"
             << measurement.centroid_x << ", " << measurement.centroid_y
             << R"json(], "projected_area": )json"
             << measurement.projected_area
             << R"json(, "equivalent_side": )json"
             << measurement.equivalent_side
             << R"json(, "equivalent_radius": )json"
             << measurement.equivalent_radius
             << R"json(, "intensity": {"min": )json"
             << measurement.intensity_min
             << R"json(, "max": )json" << measurement.intensity_max
             << R"json(, "mean": )json" << measurement.intensity_mean
             << R"json(, "stddev": )json" << measurement.intensity_stddev
             << R"json(, "skewness": )json"
             << measurement.intensity_skewness
             << R"json(, "valid": )json"
             << (measurement.intensity_valid ? "true" : "false")
             << R"json(}, "gwyddion_perimeter": )json"
             << measurement.gwyddion_perimeter
             << R"json(, "polygon_perimeter": )json"
             << measurement.polygon_perimeter
             << R"json(, "polygon_area": )json"
             << measurement.polygon_area
             << R"json(, "pixel_configuration_counts": [)json";
        for (std::size_t code = 0;
             code < measurement.pixel_configuration_counts.size(); ++code)
        {
            if (code > 0)
                file << ", ";
            file << measurement.pixel_configuration_counts[code];
        }
        file << R"json(], "outer_boundary_point_count": )json"
             << measurement.outer_boundary_point_count
             << R"json(, "hole_count": )json" << measurement.hole_count
             << R"json(, "convex_hull_area": )json"
             << measurement.convex_hull_area
             << R"json(, "circularity": )json" << measurement.circularity
             << R"json(, "solidity": )json" << measurement.solidity
             << R"json(, "major_axis_length": )json"
             << measurement.major_axis_length
             << R"json(, "minor_axis_length": )json"
             << measurement.minor_axis_length
             << R"json(, "orientation_deg": )json"
             << measurement.orientation_deg
             << R"json(, "aspect_ratio": )json" << measurement.aspect_ratio
             << R"json(, "eccentricity": )json" << measurement.eccentricity
             << R"json(, "feret_max": )json" << measurement.feret_max
             << R"json(, "feret_max_angle_deg": )json"
             << measurement.feret_max_angle_deg
             << R"json(, "feret_min": )json" << measurement.feret_min
             << R"json(, "feret_min_angle_deg": )json"
             << measurement.feret_min_angle_deg
             << R"json(, "inscribed_circle": [)json"
             << measurement.inscribed_center_x << ", "
             << measurement.inscribed_center_y << ", "
             << measurement.inscribed_radius
             << R"json(], "enclosing_circle": [)json"
             << measurement.enclosing_center_x << ", "
             << measurement.enclosing_center_y << ", "
             << measurement.enclosing_radius
             << R"json(], "boundary_method": ")json"
             << JsonEscape(measurement.boundary_method)
             << R"json(", "topology_policy": ")json"
             << JsonEscape(measurement.topology_policy)
             << R"json(", "inscribed_circle_method": ")json"
             << JsonEscape(measurement.inscribed_circle_method)
             << R"json(", "length_unit": ")json"
             << JsonEscape(measurement.length_unit)
             << R"json(", "area_unit": ")json"
             << JsonEscape(measurement.area_unit) << R"json("})json"
             << (measurement_index + 1 <
                         capture.findobject_measurements.size()
                     ? ","
                     : "")
             << '\n';
    }
    file << "    ]\n";
    file << "  },\n";
    file << "  \"findline_scan\": {\n";
    file << "    \"rows_examined\": " << capture.scan_rows_examined << ",\n";
    file << "    \"rows_with_foreground\": " << capture.scan_rows_with_foreground << ",\n";
    file << "    \"runs_total\": " << capture.scan_runs_total << ",\n";
    file << "    \"runs_within_length_limit\": " << capture.scan_runs_within_length_limit << ",\n";
    file << "    \"runs_over_length_limit\": " << capture.scan_runs_over_length_limit << ",\n";
    file << "    \"runs_rejected_by_min_edge_width\": "
         << capture.scan_runs_rejected_by_min_edge_width << ",\n";
    file << "    \"runs_rejected_by_selection\": " << capture.scan_runs_rejected_by_selection << ",\n";
    file << "    \"runs_rejected_near_endpoint\": " << capture.scan_runs_rejected_near_endpoint << ",\n";
    file << "    \"points_emitted\": " << capture.scan_points_emitted << "\n";
    file << "  },\n";
    file << "  \"result\": {\n";
    file << "    \"has_fit_line\": " << (capture.has_fit_line ? "true" : "false") << ",\n";
    file << "    \"valid_points_count\": " << capture.valid_points_count << ",\n";
    file << "    \"avgdist\": " << capture.avgdist << ",\n";
    file << "    \"local_support\": " << capture.boundary_coverage_ratio << ",\n";
    file << "    \"local_mean_distance\": " << capture.boundary_residual_rmse_px << ",\n";
    file << "    \"fit_offset\": " << capture.boundary_residual_p95_px << ",\n";
    file << "    \"boundary_coverage_ratio\": " << capture.boundary_coverage_ratio << ",\n";
    file << "    \"boundary_residual_rmse_px\": " << capture.boundary_residual_rmse_px << ",\n";
    file << "    \"boundary_residual_p95_px\": " << capture.boundary_residual_p95_px << ",\n";
    file << "    \"boundary_residual_max_px\": " << capture.boundary_residual_max_px << ",\n";
    file << "    \"boundary_reliability_score\": " << capture.boundary_reliability_score << ",\n";
    file << "    \"rendered_measure_points_count\": " << capture.rendered_measure_points_count << ",\n";
    file << "    \"rendered_result_count\": " << capture.rendered_result_count << ",\n";
    file << "    \"failure_stage\": \"" << JsonEscape(capture.failure_stage) << "\",\n";
    file << "    \"boundary_classification\": \"" << JsonEscape(boundary) << "\"\n";
    file << "  }\n";
    file << "}\n";

    file.flush();
    if (!file.good())
    {
        outReason = "failed while writing findobject branch evidence json";
        return false;
    }

    outReason.clear();
    return true;
}

void WriteJsonNumberMap(
    std::ofstream& file,
    const std::string& key,
    const std::map<std::string, double>& values,
    bool trailing_comma)
{
    file << "  \"" << key << "\": {\n";
    for (auto it = values.begin(); it != values.end(); ++it)
    {
        const auto next = std::next(it);
        file << "    \"" << JsonEscape(it->first) << "\": " << it->second
             << (next == values.end() ? "" : ",") << "\n";
    }
    file << "  }" << (trailing_comma ? "," : "") << "\n";
}

void WriteJsonStringMap(
    std::ofstream& file,
    const std::string& key,
    const std::map<std::string, std::string>& values,
    bool trailing_comma)
{
    file << "  \"" << key << "\": {\n";
    for (auto it = values.begin(); it != values.end(); ++it)
    {
        const auto next = std::next(it);
        file << "    \"" << JsonEscape(it->first) << "\": \""
             << JsonEscape(it->second) << "\""
             << (next == values.end() ? "" : ",") << "\n";
    }
    file << "  }" << (trailing_comma ? "," : "") << "\n";
}

void WriteJsonShapeSnapshots(
    std::ofstream& file,
    const std::vector<CxShapeElementSnapshot>& shapes,
    bool trailing_comma)
{
    file << "  \"shapes\": [";
    if (shapes.empty())
    {
        file << "]" << (trailing_comma ? "," : "") << "\n";
        return;
    }

    for (auto it = shapes.begin(); it != shapes.end(); ++it)
    {
        const auto next = std::next(it);
        const CxShapeElementSnapshot& s = *it;
        file << "\n    {\n";
        file << "      \"stable_ref\": \"" << JsonEscape(s.stable_ref) << "\",\n";
        file << "      \"owner_type\": \"" << JsonEscape(s.owner_type) << "\",\n";
        file << "      \"owner_ref\": \"" << JsonEscape(s.owner_ref) << "\",\n";
        file << "      \"semantic_role\": \"" << JsonEscape(s.semantic_role) << "\",\n";
        file << "      \"shape_kind\": \"" << JsonEscape(s.shape_kind) << "\",\n";
        file << "      \"editable\": " << (s.editable ? "true" : "false") << ",\n";
        file << "      \"result_element\": " << (s.result_element ? "true" : "false") << ",\n";
        file << "      \"center_x\": " << s.center_x << ",\n";
        file << "      \"center_y\": " << s.center_y << ",\n";
        file << "      \"radius\": " << s.radius << ",\n";
        file << "      \"radius_x\": " << s.radius_x << ",\n";
        file << "      \"radius_y\": " << s.radius_y << ",\n";
        file << "      \"angle_deg\": " << s.angle_deg << ",\n";
        file << "      \"points\": [";
        if (s.points.empty())
        {
            file << "]";
        }
        else
        {
            for (auto pit = s.points.begin(); pit != s.points.end(); ++pit)
            {
                const auto pnext = std::next(pit);
                file << *pit << (pnext == s.points.end() ? "" : ",");
            }
            file << "]";
        }
        file << "\n    }" << (next == shapes.end() ? "" : ",");
    }
    file << "\n  ]" << (trailing_comma ? "," : "") << "\n";
}

void WriteJsonFindLineScanDiagnostics(
    std::ofstream& file,
    const std::vector<CxFindLineScanDiagnosticSnapshot>& diagnostics,
    bool trailing_comma)
{
    file << "  \"findline_scan_diagnostics\": [";
    if (diagnostics.empty())
    {
        file << "]" << (trailing_comma ? "," : "") << "\n";
        return;
    }

    for (auto it = diagnostics.begin(); it != diagnostics.end(); ++it)
    {
        const auto next = std::next(it);
        const CxFindLineScanDiagnosticSnapshot& d = *it;
        file << "\n    {\n";
        file << "      \"scan_index\": " << d.scan_index << ",\n";
        file << "      \"scan_type\": " << d.scan_type << ",\n";
        file << "      \"scan_line\": {"
             << "\"x0\": " << d.x0 << ", "
             << "\"y0\": " << d.y0 << ", "
             << "\"x1\": " << d.x1 << ", "
             << "\"y1\": " << d.y1 << "},\n";
        file << "      " << '"' << "candidate_count" << '"' << ": "
             << d.candidate_count << ',' << '\n';
        file << "      " << '"' << "binary_logical_row" << '"' << ": "
             << d.binary_logical_row << ',' << '\n';
        file << "      " << '"' << "binary_sample_row" << '"' << ": "
             << d.binary_sample_row << ',' << '\n';
        file << "      " << '"' << "binary_logical_foreground_count" << '"' << ": "
             << d.binary_logical_foreground_count << ',' << '\n';
        file << "      " << '"' << "binary_logical_plus1_foreground_count" << '"' << ": "
             << d.binary_logical_plus1_foreground_count << ',' << '\n';
        file << "      " << '"' << "binary_logical_plus2_foreground_count" << '"' << ": "
             << d.binary_logical_plus2_foreground_count << ',' << '\n';
        file << "      " << '"' << "binary_sample_foreground_count" << '"' << ": "
             << d.binary_sample_foreground_count << ',' << '\n';
        file << "      " << '"' << "threshold_run_count" << '"' << ": "
             << d.threshold_run_count << ',' << '\n';
        file << "      " << '"' << "threshold_max_run_width" << '"' << ": "
             << d.threshold_max_run_width << ',' << '\n';
        file << "      " << '"' << "min_edge_run_width_px" << '"' << ": "
             << d.min_edge_run_width_px << ',' << '\n';
        file << "      " << '"' << "rejected_min_edge_run_width" << '"' << ": "
             << d.rejected_min_edge_run_width << ',' << '\n';
        file << "      " << '"' << "boundary_response_used" << '"' << ": "
             << (d.boundary_response_used ? "true" : "false") << ',' << '\n';
        file << "      " << '"' << "boundary_response_mode" << '"' << ": "
             << d.boundary_response_mode << ',' << '\n';
        file << "      " << '"' << "boundary_response_candidate_count" << '"' << ": "
             << d.boundary_response_candidate_count << ',' << '\n';
        file << "      " << '"' << "boundary_response_selected_index" << '"' << ": "
             << d.boundary_response_selected_index << ',' << '\n';
        file << "      " << '"' << "boundary_response_selected_position" << '"' << ": "
             << d.boundary_response_selected_position << ',' << '\n';
        file << "      " << '"' << "boundary_response_selected_score" << '"' << ": "
             << d.boundary_response_selected_score << ',' << '\n';
        file << "      " << '"' << "boundary_response_status" << '"' << ": " << '"'
             << JsonEscape(d.boundary_response_status) << '"' << ',' << '\n';
        file << "      " << '"' << "boundary_response_reason" << '"' << ": " << '"'
             << JsonEscape(d.boundary_response_reason) << '"' << ',' << '\n';
        file << "      " << '"' << "threshold_first_run" << '"' << ": {"
             << '"' << "start" << '"' << ": " << d.threshold_first_run_start << ", "
             << '"' << "end" << '"' << ": " << d.threshold_first_run_end << "}," << '\n';
        file << "      " << '"' << "threshold_last_run" << '"' << ": {"
             << '"' << "start" << '"' << ": " << d.threshold_last_run_start << ", "
             << '"' << "end" << '"' << ": " << d.threshold_last_run_end << "}," << '\n';
        file << "      " << '"' << "accepted" << '"' << ": "
             << (d.accepted ? "true" : "false") << ',' << '\n';
        file << "      " << '"' << "accepted_point" << '"' << ": {"
             << '"' << "x" << '"' << ": " << d.accepted_x << ", "
             << '"' << "y" << '"' << ": " << d.accepted_y << "}," << '\n';
        file << "      " << '"' << "reject_reason" << '"' << ": " << '"'
             << JsonEscape(d.reject_reason) << '"' << '\n';
        file << "    }" << (next == diagnostics.end() ? "" : ",");
    }
    file << "\n  ]" << (trailing_comma ? "," : "") << "\n";
}

void WriteJsonFindLineEdgeEvaluations(
    std::ofstream& file,
    const std::vector<CxFindLineEdgeEvaluationSnapshot>& evaluations,
    bool trailing_comma)
{
    file << "  \"findline_edge_evaluations\": [";
    if (evaluations.empty())
    {
        file << "]" << (trailing_comma ? "," : "") << "\n";
        return;
    }

    for (auto it = evaluations.begin(); it != evaluations.end(); ++it)
    {
        const auto next = std::next(it);
        const CxFindLineEdgeEvaluationSnapshot& e = *it;
        file << "\n    {\n";
        file << "      \"edge_index\": " << e.edge_index << ",\n";
        file << "      \"candidate_scan_rows\": " << e.candidate_scan_rows << ",\n";
        file << "      \"accepted_points\": " << e.accepted_points << ",\n";
        file << "      \"rejected_by_selection\": " << e.rejected_by_selection << ",\n";
        file << "      \"rejected_near_endpoint\": " << e.rejected_near_endpoint << ",\n";
        file << "      \"over_length_runs\": " << e.over_length_runs << ",\n";
        file << "      \"coverage\": " << e.coverage << ",\n";
        file << "      \"score\": " << e.score << ",\n";
        file << "      \"selected\": " << (e.selected ? "true" : "false") << ",\n";
        file << "      \"fit_possible\": " << (e.fit_possible ? "true" : "false") << "\n";
        file << "    }" << (next == evaluations.end() ? "" : ",");
    }
    file << "\n  ]" << (trailing_comma ? "," : "") << "\n";
}

bool SaveCxScriptHeadlessSummaryJson(
    const CxScriptExecutionCapture& capture,
    const std::filesystem::path& outputPath,
    std::string& outReason)
{
    std::error_code ec;
    std::filesystem::create_directories(outputPath.parent_path(), ec);
    if (ec)
    {
        outReason = "failed to create headless summary directory: " + ec.message();
        return false;
    }
    const std::filesystem::path temporary = outputPath.string() + ".tmp";
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    if (!file.is_open())
    {
        outReason = "failed to open headless summary json";
        return false;
    }

    CxScriptResultPackage pkg = BuildCxScriptResultPackage(capture);

    file << "{\n";

    file << "  \"execution_mode\": \"sequential\",\n";

    WriteJsonStringMap(file, "facts", pkg.facts, true);
    WriteJsonNumberMap(file, "metrics", pkg.metrics, true);
    WriteJsonNumberMap(file, "runtime_globals", capture.runtime_globals, true);
    WriteJsonShapeSnapshots(file, pkg.shapes, true);
    WriteJsonFindLineEdgeEvaluations(
        file,
        capture.findline_edge_evaluations,
        true);
    WriteJsonFindLineScanDiagnostics(
        file,
        capture.findline_scan_diagnostics,
        false);

    file << "}\n";

    file.flush();
    const bool write_ok = file.good();
    file.close();
    if (!write_ok)
    {
        std::filesystem::remove(temporary, ec);
        outReason = "failed while writing headless summary json";
        return false;
    }
#ifdef _WIN32
    if (!MoveFileExW(
            temporary.c_str(), outputPath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    {
        std::filesystem::remove(temporary, ec);
        outReason = "failed to atomically replace headless summary json";
        return false;
    }
#else
    std::filesystem::rename(temporary, outputPath, ec);
    if (ec)
    {
        std::filesystem::remove(temporary, ec);
        outReason = "failed to atomically replace headless summary json";
        return false;
    }
#endif

    outReason.clear();
    return true;
}

bool ParseCxScriptHeadlessArgs(
    int argc,
    char** argv,
    CxScriptHeadlessOptions& options)
{
    for (int i = 1; i < argc; ++i)
    {
        std::string arg(argv[i]);
        if (arg == "--cxscript-headless")
            options.enabled = true;
        else if (arg == "--image" && i + 1 < argc)
            options.image_path = argv[++i];
        else if (arg == "--template-image" && i + 1 < argc)
            options.template_image_path = argv[++i];
        else if (arg == "--torch-dataset-root" && i + 1 < argc)
            options.torch_dataset_root = argv[++i];
        else if (arg == "--torch-epochs" && i + 1 < argc)
            options.torch_training_epochs = std::max(1, std::stoi(argv[++i]));
        else if (arg == "--torch-learning-rate" && i + 1 < argc)
            options.torch_learning_rate = std::stod(argv[++i]);
        else if (arg == "--torch-lr-schedule" && i + 1 < argc)
            options.torch_lr_schedule = argv[++i];
        else if (arg == "--torch-min-learning-rate" && i + 1 < argc)
            options.torch_min_learning_rate = std::stod(argv[++i]);
        else if (arg == "--torch-weight-decay" && i + 1 < argc)
            options.torch_weight_decay = std::stod(argv[++i]);
        else if (arg == "--torch-box-loss-weight" && i + 1 < argc)
            options.torch_box_loss_weight = std::stod(argv[++i]);
        else if (arg == "--torch-class-loss-weight" && i + 1 < argc)
            options.torch_class_loss_weight = std::stod(argv[++i]);
        else if (arg == "--torch-dfl-loss-weight" && i + 1 < argc)
            options.torch_dfl_loss_weight = std::stod(argv[++i]);
        else if (arg == "--torch-mask-loss-weight" && i + 1 < argc)
            options.torch_mask_loss_weight = std::stod(argv[++i]);
        else if (arg == "--torch-use-assignment-quality-targets" && i + 1 < argc)
            options.torch_use_assignment_quality_targets = std::stoi(argv[++i]) != 0;
        else if (arg == "--torch-assignment-quality-warmup-epochs" && i + 1 < argc)
            options.torch_assignment_quality_warmup_epochs = std::stoi(argv[++i]);
        else if (arg == "--torch-postprocess-confidence-threshold" && i + 1 < argc)
            options.torch_postprocess_confidence_threshold = std::stod(argv[++i]);
        else if (arg == "--torch-postprocess-iou-threshold" && i + 1 < argc)
            options.torch_postprocess_iou_threshold = std::stod(argv[++i]);
        else if (arg == "--torch-postprocess-max-detections" && i + 1 < argc)
            options.torch_postprocess_max_detections = std::stoi(argv[++i]);
        else if (arg == "--torch-postprocess-class-agnostic-nms" && i + 1 < argc)
            options.torch_postprocess_class_agnostic_nms = std::stoi(argv[++i]) != 0;
        else if (arg == "--script" && i + 1 < argc)
            options.script_path = argv[++i];
        else if (arg == "--globals" && i + 1 < argc)
            options.globals_path = argv[++i];
        else if (arg == "--image-id" && i + 1 < argc)
            options.image_id = argv[++i];
        else if (arg == "--target-id" && i + 1 < argc)
            options.target_id = argv[++i];
        else if (arg == "--stage25-tool" && i + 1 < argc)
            options.stage25_tool = argv[++i];
        else if (arg == "--case-name" && i + 1 < argc)
            options.case_name = argv[++i];
        else if (arg == "--out" && i + 1 < argc)
            options.output_dir = argv[++i];
        else if (arg == "--max-steps" && i + 1 < argc)
            options.max_steps = std::stoi(argv[++i]);
        else if (arg == "--timeout-sec" && i + 1 < argc)
            options.timeout_sec = std::stoi(argv[++i]);
        else if (arg == "--roi-x0" && i + 1 < argc)
            options.roi_x0 = std::stoi(argv[++i]);
        else if (arg == "--roi-y0" && i + 1 < argc)
            options.roi_y0 = std::stoi(argv[++i]);
        else if (arg == "--roi-x1" && i + 1 < argc)
            options.roi_x1 = std::stoi(argv[++i]);
        else if (arg == "--roi-y1" && i + 1 < argc)
            options.roi_y1 = std::stoi(argv[++i]);
        else if (arg == "--circle-cx" && i + 1 < argc)
            options.circle_cx = std::stoi(argv[++i]);
        else if (arg == "--circle-cy" && i + 1 < argc)
            options.circle_cy = std::stoi(argv[++i]);
        else if (arg == "--circle-px" && i + 1 < argc)
            options.circle_px = std::stoi(argv[++i]);
        else if (arg == "--circle-py" && i + 1 < argc)
            options.circle_py = std::stoi(argv[++i]);
        else if (arg == "--findcircle-arc-enabled" && i + 1 < argc)
            options.findcircle_arc_enabled = std::stoi(argv[++i]);
        else if (arg == "--findcircle-arc-start-deg" && i + 1 < argc)
            options.findcircle_arc_start_deg = std::stoi(argv[++i]);
        else if (arg == "--findcircle-arc-end-deg" && i + 1 < argc)
            options.findcircle_arc_end_deg = std::stoi(argv[++i]);
        else if (arg == "--ellipse-x0" && i + 1 < argc)
            options.ellipse_x0 = std::stoi(argv[++i]);
        else if (arg == "--ellipse-y0" && i + 1 < argc)
            options.ellipse_y0 = std::stoi(argv[++i]);
        else if (arg == "--ellipse-x1" && i + 1 < argc)
            options.ellipse_x1 = std::stoi(argv[++i]);
        else if (arg == "--ellipse-y1" && i + 1 < argc)
            options.ellipse_y1 = std::stoi(argv[++i]);
        else if ((arg == "--findellipse-inner-scale-percent" ||
                  arg == "--ellipse-inner-scale-percent") &&
                 i + 1 < argc)
            options.ellipse_inner_scale_percent =
                std::max(0, std::min(99, std::stoi(argv[++i])));
        else if (arg == "--tool-half-width" && i + 1 < argc)
            options.tool_half_width = std::stoi(argv[++i]);
        else if (arg == "--wgap" && i + 1 < argc)
            options.wgap = std::stoi(argv[++i]);
        else if (arg == "--hgap" && i + 1 < argc)
            options.hgap = std::stoi(argv[++i]);
        else if (arg == "--gap" && i + 1 < argc)
            options.gap = std::stoi(argv[++i]);
        else if (arg == "--linegap" && i + 1 < argc)
            options.linegap = std::stoi(argv[++i]);
        else if ((arg == "--min-edge-run-width-px" ||
                  arg == "--minedgerunwidth") && i + 1 < argc)
            options.min_edge_run_width_px =
                std::max(1, std::min(20, std::stoi(argv[++i])));
        else if (arg == "--threshold" && i + 1 < argc)
            options.threshold = std::stoi(argv[++i]);
        else if (arg == "--method" && i + 1 < argc)
            options.method = std::stoi(argv[++i]);
        else if (arg == "--filterprofile" && i + 1 < argc)
            options.filterprofile = std::stoi(argv[++i]);
        else if (arg == "--samplerate" && i + 1 < argc)
            options.samplerate = std::stoi(argv[++i]);
        else if (arg == "--min-score" && i + 1 < argc)
            options.min_score = std::stod(argv[++i]);
        else if (arg == "--find-num" && i + 1 < argc)
            options.find_num = std::stoi(argv[++i]);
        else if (arg == "--compare-gap" && i + 1 < argc)
            options.compare_gap = std::stoi(argv[++i]);
        else if (arg == "--strategy-id" && i + 1 < argc)
            options.strategy_id = std::stoi(argv[++i]);
        else if (arg == "--max-elapsed-ms" && i + 1 < argc)
            options.max_elapsed_ms = std::stoi(argv[++i]);
        else if (arg == "--max-scan-lines" && i + 1 < argc)
            options.max_scan_lines = std::stoi(argv[++i]);
        else if (arg == "--max-samples" && i + 1 < argc)
            options.max_samples = std::stoi(argv[++i]);
        else if (arg == "--runtime-capture-smoke")
            options.runtime_capture_smoke = true;
        else if (arg == "--save-evidence-candidate")
            options.save_evidence_candidate = true;
        else if (arg == "--evidence-candidate-root" && i + 1 < argc)
            options.evidence_candidate_root = argv[++i];
        else if (arg == "--evidence-candidate-id" && i + 1 < argc)
            options.evidence_candidate_id = argv[++i];
    }
    return options.enabled && !options.image_path.empty() && !options.script_path.empty() && !options.output_dir.empty();
}

static bool ApplyHeadlessCandidateGlobalOverrides(
    const CxScriptHeadlessOptions& options,
    ManualTestContext& context,
    std::string& reason)
{
    auto apply = [&](const std::map<std::string, double>& values)
    {
        for (const auto& entry : values)
            context.runtime_int_vars[entry.first] =
                static_cast<int>(std::lround(entry.second));
    };

    if (!options.globals_path.empty())
    {
        std::map<std::string, double> fileGlobals;
        if (!LoadHeadlessGlobalValuesFile(options.globals_path, fileGlobals, reason))
            return false;
        apply(fileGlobals);
    }

    apply(options.cli_global_overrides);
    reason.clear();
    return true;
}

static int GetHeadlessCandidateGlobal(
    const ManualTestContext& context,
    const std::string& name,
    int fallback)
{
    const auto found = context.runtime_int_vars.find(name);
    return found == context.runtime_int_vars.end() ? fallback : found->second;
}

static void PopulateHeadlessCandidateGaugeFromGlobals(
    const CxScriptHeadlessOptions& options,
    ManualTestContext& context,
    const std::string& tool)
{
    ManualGaugeState& gauge = context.current_gauge;
    gauge.case_id = context.active_case_id;
    gauge.image_id = context.active_image_id;
    gauge.target_id = context.active_target_id;
    gauge.source = "headless";
    gauge.review_status = "pending_human_review";
    gauge.tool = tool;
    gauge.tool_half_width = GetHeadlessCandidateGlobal(
        context, "global_tool_half_width", options.tool_half_width);
    gauge.threshold = GetHeadlessCandidateGlobal(
        context, "global_threshold", options.threshold);
    gauge.method = GetHeadlessCandidateGlobal(
        context, "global_method", options.method);
    gauge.linegap = GetHeadlessCandidateGlobal(
        context, "global_linegap", options.linegap);
    gauge.min_edge_run_width_px = GetHeadlessCandidateGlobal(
        context,
        "global_min_edge_run_width_px",
        options.min_edge_run_width_px);
    gauge.wgap = GetHeadlessCandidateGlobal(
        context, "global_wgap", options.wgap);
    gauge.hgap = GetHeadlessCandidateGlobal(
        context, "global_hgap", options.hgap);
    gauge.gap = GetHeadlessCandidateGlobal(
        context, "global_gap", options.gap);
    gauge.filterprofile = GetHeadlessCandidateGlobal(
        context, "global_filterprofile", options.filterprofile);

    if (tool == "FindCircle")
    {
        context.findcircle_scan_edge_count = GetHeadlessCandidateGlobal(
            context, "global_findcircle_edge_count",
            context.findcircle_scan_edge_count);
        context.findcircle_selected_scan_edge = GetHeadlessCandidateGlobal(
            context, "global_findcircle_selected_edge",
            context.findcircle_selected_scan_edge);
        context.findcircle_point_consistency_enabled =
            GetHeadlessCandidateGlobal(
                context, "global_findcircle_point_consistency_enabled",
                context.findcircle_point_consistency_enabled ? 1 : 0) != 0;
        context.findcircle_point_consistency_range = GetHeadlessCandidateGlobal(
            context, "global_findcircle_point_consistency_range",
            context.findcircle_point_consistency_range);
        gauge.has_circle_gauge = true;
        gauge.circle_cx = GetHeadlessCandidateGlobal(
            context, "global_circle_cx", options.circle_cx);
        gauge.circle_cy = GetHeadlessCandidateGlobal(
            context, "global_circle_cy", options.circle_cy);
        gauge.circle_px = GetHeadlessCandidateGlobal(
            context, "global_circle_px", options.circle_px);
        gauge.circle_py = GetHeadlessCandidateGlobal(
            context, "global_circle_py", options.circle_py);
        const double dx = static_cast<double>(gauge.circle_px - gauge.circle_cx);
        const double dy = static_cast<double>(gauge.circle_py - gauge.circle_cy);
        gauge.radius = static_cast<int>(std::lround(std::hypot(dx, dy)));
        gauge.inner_radius = GetHeadlessCandidateGlobal(
            context, "global_circle_inner_radius", 0);
        gauge.outer_radius = GetHeadlessCandidateGlobal(
            context, "global_circle_outer_radius", gauge.radius);
        gauge.circle_arc_enabled = GetHeadlessCandidateGlobal(
            context, "global_findcircle_arc_enabled", options.findcircle_arc_enabled) != 0;
        gauge.circle_arc_start_deg = GetHeadlessCandidateGlobal(
            context, "global_findcircle_arc_start_deg", options.findcircle_arc_start_deg);
        gauge.circle_arc_end_deg = GetHeadlessCandidateGlobal(
            context, "global_findcircle_arc_end_deg", options.findcircle_arc_end_deg);
    }
    else if (tool == "FindEllipse")
    {
        gauge.has_ellipse_gauge = true;
        gauge.ellipse_x0 = GetHeadlessCandidateGlobal(
            context, "global_ellipse_x0", options.ellipse_x0);
        gauge.ellipse_y0 = GetHeadlessCandidateGlobal(
            context, "global_ellipse_y0", options.ellipse_y0);
        gauge.ellipse_x1 = GetHeadlessCandidateGlobal(
            context, "global_ellipse_x1", options.ellipse_x1);
        gauge.ellipse_y1 = GetHeadlessCandidateGlobal(
            context, "global_ellipse_y1", options.ellipse_y1);
        gauge.ellipse_inner_scale_percent = GetHeadlessCandidateGlobal(
            context, "global_findellipse_inner_scale_percent",
            options.ellipse_inner_scale_percent);
    }
    else
    {
        gauge.has_line_gauge = true;
        gauge.line_x0 = GetHeadlessCandidateGlobal(
            context, "global_roi_x0", options.roi_x0);
        gauge.line_y0 = GetHeadlessCandidateGlobal(
            context, "global_roi_y0", options.roi_y0);
        gauge.line_x1 = GetHeadlessCandidateGlobal(
            context, "global_roi_x1", options.roi_x1);
        gauge.line_y1 = GetHeadlessCandidateGlobal(
            context, "global_roi_y1", options.roi_y1);
    }
}

bool RunCxScriptHeadless(const CxScriptHeadlessOptions& options, CxScriptHeadlessResult& result)
{
    result = CxScriptHeadlessResult{};
    result.exit_code = 1;

    std::filesystem::path output_dir(options.output_dir);
    if (!std::filesystem::exists(output_dir))
    {
        std::error_code ec;
        std::filesystem::create_directories(output_dir, ec);
        if (ec)
        {
            result.reason = "cannot create output directory: " + output_dir.string();
            result.failure_stage = "output_path";
            return false;
        }
    }

    std::filesystem::path script_path(options.script_path);
    if (!std::filesystem::exists(script_path))
    {
        result.reason = "script not found: " + script_path.string();
        result.failure_stage = "script";
        return false;
    }

    std::filesystem::path image_path(options.image_path);
    if (!std::filesystem::exists(image_path))
    {
        result.reason = "image not found: " + image_path.string();
        result.failure_stage = "image";
        return false;
    }

    cv::Mat source_image = cv::imread(image_path.string(), cv::IMREAD_COLOR);
    if (source_image.empty())
    {
        result.reason = "cannot read image: " + image_path.string();
        result.failure_stage = "image";
        return false;
    }

    result.launched = true;

    if (!options.template_image_path.empty())
    {
        std::filesystem::path template_path(options.template_image_path);
        if (!std::filesystem::exists(template_path))
        {
            result.reason = "template image not found: " + template_path.string();
            result.failure_stage = "template_image";
            return false;
        }
    }

    CxScriptExecutionCapture capture;
    const int timeout_ms = std::max(1, options.timeout_sec) * 1000;
    capture.budget_ms = options.max_elapsed_ms > 0
        ? std::min(options.max_elapsed_ms, timeout_ms)
        : timeout_ms;
    capture.max_steps = options.max_steps;
    capture.max_scan_lines = options.max_scan_lines;
    capture.max_samples = options.max_samples;
    std::string reason;

    CxScriptHeadlessOptions effective_options = options;
    effective_options.max_elapsed_ms = capture.budget_ms;
    bool execution_ok = ExecuteCxScriptSequential(
        effective_options,
        source_image,
        capture,
        reason);

    if (!execution_ok)
    {
        result.reason = reason;
        result.failure_stage = capture.failure_stage.empty() ? "script_execution" : capture.failure_stage;
        if (options.save_evidence_candidate)
        {
            ManualTestContext candidateContext;
            candidateContext.active_case_id =
                options.case_name.empty() ? options.case_id : options.case_name;
            candidateContext.active_image_id =
                options.image_id.empty() ? options.stage25_image_id : options.image_id;
            candidateContext.active_target_id =
                options.target_id.empty() ? options.stage25_target_id : options.target_id;
            candidateContext.image_file_path = options.image_path;
            candidateContext.loaded_script_path = options.script_path;
            candidateContext.script_file_path = options.script_path;
            candidateContext.editor_source = "headless";
            ReadTextFile(options.script_path, candidateContext.editor_text);
            candidateContext.debug_status = "HEADLESS_EXECUTION_FAIL";
            candidateContext.debug_reason = result.reason;
            candidateContext.current_result_ref.status = "runtime_result_failed";
            candidateContext.current_result_ref.reason = result.reason;

            auto setGlobal = [&](const std::string& name, int value)
            {
                candidateContext.runtime_int_vars[name] = value;
            };
            setGlobal("global_roi_x0", options.roi_x0);
            setGlobal("global_roi_y0", options.roi_y0);
            setGlobal("global_roi_x1", options.roi_x1);
            setGlobal("global_roi_y1", options.roi_y1);
            setGlobal("global_circle_cx", options.circle_cx);
            setGlobal("global_circle_cy", options.circle_cy);
            setGlobal("global_circle_px", options.circle_px);
            setGlobal("global_circle_py", options.circle_py);
            setGlobal("global_circle_inner_radius", 0);
            setGlobal("global_circle_outer_radius", 0);
            setGlobal("global_circle_ring_width", 0);
            setGlobal("global_findcircle_arc_enabled", options.findcircle_arc_enabled);
            setGlobal("global_findcircle_arc_start_deg", options.findcircle_arc_start_deg);
            setGlobal("global_findcircle_arc_end_deg", options.findcircle_arc_end_deg);
            setGlobal("global_ellipse_x0", options.ellipse_x0);
            setGlobal("global_ellipse_y0", options.ellipse_y0);
            setGlobal("global_ellipse_x1", options.ellipse_x1);
            setGlobal("global_ellipse_y1", options.ellipse_y1);
            setGlobal("global_findellipse_inner_scale_percent", options.ellipse_inner_scale_percent);
            setGlobal("global_tool_half_width", options.tool_half_width);
            setGlobal("global_wgap", options.wgap);
            setGlobal("global_hgap", options.hgap);
            setGlobal("global_gap", options.gap);
            setGlobal("global_linegap", options.linegap);
            setGlobal("global_min_edge_run_width_px",
                      options.min_edge_run_width_px);
            setGlobal("global_threshold", options.threshold);
            setGlobal("global_method", options.method);
            setGlobal("global_filterprofile", options.filterprofile);
            setGlobal("global_max_elapsed_ms", options.max_elapsed_ms);
            setGlobal("global_max_scan_lines", options.max_scan_lines);
            setGlobal("global_max_samples", options.max_samples);

            std::string candidateGlobalsReason;
            if (!ApplyHeadlessCandidateGlobalOverrides(
                    options, candidateContext, candidateGlobalsReason))
            {
                candidateContext.debug_reason +=
                    "\ncandidate globals restore failed: " + candidateGlobalsReason;
            }

            std::string lowerScript = options.script_path;
            std::transform(lowerScript.begin(), lowerScript.end(), lowerScript.begin(),
                [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
            const std::string tool = !options.stage25_tool.empty()
                ? options.stage25_tool
                : (lowerScript.find("cxtextinspect") != std::string::npos ? "CxTextInspect" :
                   lowerScript.find("fastmatch") != std::string::npos ? "FastMatch" :
                   lowerScript.find("circle") != std::string::npos ? "FindCircle" :
                   lowerScript.find("ellipse") != std::string::npos ? "FindEllipse" :
                   lowerScript.find("rect") != std::string::npos ? "FindRect" :
                   "FindLine");
            PopulateHeadlessCandidateGaugeFromGlobals(
                options, candidateContext, tool);

            CxEvidenceCandidateSaveOptions candidateOptions;
            candidateOptions.root_dir = options.evidence_candidate_root.empty()
                ? "cxscript_runs/evidence_candidates"
                : options.evidence_candidate_root;
            candidateOptions.candidate_id = options.evidence_candidate_id;
            candidateOptions.mode = "headless_failed";
            candidateOptions.add_to_evidence_chain = false;
            CxEvidenceCandidateSaveResult candidateResult;
            SaveEvidenceCandidatePackage(
                candidateContext,
                candidateOptions,
                candidateResult);
        }
        return false;
    }

    result.executed = true;
    result.runtime_ok = true;

    std::filesystem::path snapshot_path = output_dir / "snapshot.txt";
    std::filesystem::path summary_path = output_dir / "result_summary.json";
    std::filesystem::path result_overlay_path = output_dir / "result_overlay.png";
    std::filesystem::path evidence_overlay_path = output_dir / "evidence_overlay.png";
    std::filesystem::path tool_display_path = output_dir / "tool_display.png";
    std::filesystem::path line_trace_path = output_dir / "line_trace.json";
    std::filesystem::path shape_model_path = output_dir / "shape_model.json";
    std::filesystem::path form_fit_path = output_dir / "formfit_correspondence.json";
    std::filesystem::path form_fit_gauge_path = output_dir / "formfit_gauge.json";
    std::filesystem::path variable_snapshot_path = output_dir / "variable_snapshot.json";
    std::filesystem::path object_state_path = output_dir / "object_state.json";
    std::filesystem::path findobject_branch_evidence_path = output_dir / "findobject_branch_evidence.json";
    std::filesystem::path overlay_validation_path = output_dir / "overlay_validation.json";
    std::filesystem::path log_path = output_dir / "log.txt";

    std::ofstream snapshot_file(snapshot_path);
    if (snapshot_file.is_open())
    {
        snapshot_file << "case_id: " << options.case_name << "\n";
        snapshot_file << "image: " << options.image_path << "\n";
        if (!options.template_image_path.empty())
            snapshot_file << "template_image: " << options.template_image_path << "\n";
        snapshot_file << "script: " << options.script_path << "\n";
        snapshot_file << "status: executed\n";
        snapshot_file << "timeout: false\n";
        snapshot_file << "execution_mode: sequential\n";
        snapshot_file << "elapsed_ms: " << capture.elapsed_ms << "\n";
        snapshot_file << "budget_exceeded: " << (capture.budget_exceeded ? "true" : "false") << "\n";
        snapshot_file.close();
        result.snapshot_path = snapshot_path.string();
    }

    cv::Mat result_overlay;
    cv::Mat evidence_overlay;
    cv::Mat tool_display;

    CxOverlayRenderResult render_result;

    const auto render_normal_trace_evidence = [&capture](cv::Mat& canvas)
    {
        if (canvas.empty() || !capture.fastmatch_normal_trace.executed)
            return;
        const auto to_point = [](const CxFastMatchNormalTracePointEvidence& point)
        {
            return cv::Point(static_cast<int>(std::lround(point.x)),
                             static_cast<int>(std::lround(point.y)));
        };
        // Always render the last valid FindLine measurement facts.  These
        // remain meaningful when slope/domain filtering rejects every anchor
        // and no domain, ANN component, Dijkstra trace, or A/B pair exists.
        const cv::Scalar anchor_colors[4] = {
            cv::Scalar(0, 255, 255),   // Top: yellow
            cv::Scalar(0, 165, 255),   // Bottom: orange
            cv::Scalar(255, 0, 255),   // Left: magenta
            cv::Scalar(255, 180, 0)};  // Right: cyan-blue
        for (std::size_t direction = 0;
             direction < capture.fastmatch_normal_trace
                             .anchor_points_by_direction.size();
             ++direction)
        {
            for (const auto& point : capture.fastmatch_normal_trace
                                         .anchor_points_by_direction[direction])
            {
                cv::drawMarker(canvas, to_point(point),
                               anchor_colors[direction],
                               cv::MARKER_CROSS, 9, 2, cv::LINE_AA);
            }
        }
        std::vector<cv::Point> derived;
        derived.reserve(capture.fastmatch_normal_trace.derived_trace_points.size());
        for (const auto& point : capture.fastmatch_normal_trace.derived_trace_points)
            derived.push_back(to_point(point));
        if (derived.size() >= 2)
            cv::polylines(canvas, derived, false, cv::Scalar(255, 255, 0), 1,
                          cv::LINE_AA);
        for (const auto& point : capture.fastmatch_normal_trace.dijkstra_trace_points)
            cv::circle(canvas, to_point(point), 2, cv::Scalar(0, 255, 255), -1,
                       cv::LINE_AA);
        for (const auto& point : capture.fastmatch_normal_trace.derived_junction_points)
            cv::circle(canvas, to_point(point), 5, cv::Scalar(0, 165, 255), 2,
                       cv::LINE_AA);
        const std::size_t pair_count = std::min(
            capture.fastmatch_normal_trace.normal_pair_source_points.size(),
            std::min(capture.fastmatch_normal_trace.normal_pair_a.size(),
                     capture.fastmatch_normal_trace.normal_pair_b.size()));
        for (std::size_t index = 0; index < pair_count; ++index)
        {
            const cv::Point source = to_point(
                capture.fastmatch_normal_trace.normal_pair_source_points[index]);
            const cv::Point a =
                to_point(capture.fastmatch_normal_trace.normal_pair_a[index]);
            const cv::Point b =
                to_point(capture.fastmatch_normal_trace.normal_pair_b[index]);
            cv::line(canvas, b, a, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
            cv::circle(canvas, source, 2, cv::Scalar(0, 255, 255), -1,
                       cv::LINE_AA);
            cv::circle(canvas, a, 3, cv::Scalar(0, 0, 255), -1, cv::LINE_AA);
            cv::circle(canvas, b, 3, cv::Scalar(0, 255, 0), -1, cv::LINE_AA);
        }
        cv::putText(canvas,
                    "anchors: top=yellow bottom=orange left=magenta right=blue",
                    cv::Point(8, 18), cv::FONT_HERSHEY_SIMPLEX, 0.42,
                    cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
        cv::putText(canvas,
                    "trace=cyan junction=orange A=red B=green",
                    cv::Point(8, 36), cv::FONT_HERSHEY_SIMPLEX, 0.42,
                    cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    };
    const auto render_form_fit_evidence = [&capture](cv::Mat& canvas)
    {
        if (canvas.empty() || !capture.fastmatch_form_fit.executed)
            return;
        const auto point = [](const CxFastMatchShapePoint& value) {
            return cv::Point(static_cast<int>(std::lround(value.x)),
                             static_cast<int>(std::lround(value.y)));
        };
        std::vector<cv::Point> reference;
        reference.reserve(capture.fastmatch_reference_shape_model.dense_points.size());
        for (const auto& value : capture.fastmatch_reference_shape_model.dense_points)
            reference.push_back(point(value));
        if (reference.size() >= 2)
            cv::polylines(canvas, reference, true, cv::Scalar(255, 180, 0), 1,
                          cv::LINE_AA);
        std::vector<cv::Point> observed;
        observed.reserve(capture.fastmatch_observed_shape_model.dense_points.size());
        for (const auto& value : capture.fastmatch_observed_shape_model.dense_points)
            observed.push_back(point(value));
        if (observed.size() >= 2)
            cv::polylines(canvas, observed, true, cv::Scalar(180, 0, 255), 1,
                          cv::LINE_AA);
        for (const auto& anchor : capture.fastmatch_reference_shape_model.anchors)
            cv::circle(canvas,
                       cv::Point(static_cast<int>(std::lround(anchor.x)),
                                 static_cast<int>(std::lround(anchor.y))),
                       4, cv::Scalar(0, 128, 255), 1, cv::LINE_AA);
        cv::putText(canvas,
                    "FormFit reference=blue observed=magenta anchors=orange",
                    cv::Point(8, 36), cv::FONT_HERSHEY_SIMPLEX, 0.42,
                    cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    };

    if (RenderCxShapeOverlay(source_image, capture.shapes, CxOverlayLayer::RESULT, result_overlay, render_result))
    {
        std::filesystem::create_directories(result_overlay_path.parent_path());
        capture.result_overlay_changed_pixels = render_result.changed_pixel_count;
        capture.rendered_measure_points_count = render_result.rendered_measure_points_count;
        capture.rendered_result_count = render_result.rendered_result_count;
        if (cv::imwrite(result_overlay_path.string(), result_overlay))
            result.result_overlay_path = result_overlay_path.string();
    }

    if (RenderCxShapeOverlay(source_image, capture.shapes, CxOverlayLayer::EVIDENCE, evidence_overlay, render_result))
    {
        render_normal_trace_evidence(evidence_overlay);
        render_form_fit_evidence(evidence_overlay);
        std::filesystem::create_directories(evidence_overlay_path.parent_path());
        if (cv::imwrite(evidence_overlay_path.string(), evidence_overlay))
            result.evidence_overlay_path = evidence_overlay_path.string();
    }

    if (RenderCxShapeOverlay(source_image, capture.shapes, CxOverlayLayer::TOOL_DISPLAY, tool_display, render_result))
    {
        render_normal_trace_evidence(tool_display);
        render_form_fit_evidence(tool_display);
        std::filesystem::create_directories(tool_display_path.parent_path());
        if (cv::imwrite(tool_display_path.string(), tool_display))
            result.tool_display_path = tool_display_path.string();
    }

    PersistSegmentationArtifacts(output_dir, capture);

    // Rendering facts are part of the contract input, so persist the summary
    // only after all overlay passes have updated the capture.
    if (SaveCxScriptHeadlessSummaryJson(capture, summary_path, reason))
    {
        result.summary_path = summary_path.string();
    }
    else
    {
        result.failure_stage = "summary_export";
        result.reason = reason;
    }

    std::string measurement_semantic_reason;
    if (!WriteMeasurementSemanticSidecars(
            capture,
            effective_options,
            output_dir,
            measurement_semantic_reason))
    {
        if (result.reason.empty())
            result.reason = measurement_semantic_reason;
    }

    std::string branch_evidence_reason;
    if (!SaveFindObjectBranchEvidenceJson(
            capture,
            options,
            findobject_branch_evidence_path,
            branch_evidence_reason))
    {
        if (result.reason.empty())
            result.reason = branch_evidence_reason;
    }

    std::ofstream line_trace_file(line_trace_path);
    if (line_trace_file.is_open())
    {
        const auto write_point_array = [&line_trace_file](
            const char* name,
            const std::vector<CxFastMatchNormalTracePointEvidence>& points,
            bool comma) {
            line_trace_file << "  \"" << name << "\": [";
            for (std::size_t i = 0; i < points.size(); ++i)
            {
                if (i != 0)
                    line_trace_file << ",";
                line_trace_file << "[" << points[i].x << "," << points[i].y << "]";
            }
            line_trace_file << "]" << (comma ? "," : "") << "\n";
        };
        const auto write_int4 = [&line_trace_file](
            const char* name,
            const std::array<int, 4>& values) {
            line_trace_file << "  \"" << name << "\": ["
                << values[0] << "," << values[1] << ","
                << values[2] << "," << values[3] << "],\n";
        };
        const auto write_double4 = [&line_trace_file](
            const char* name,
            const std::array<double, 4>& values) {
            line_trace_file << "  \"" << name << "\": ["
                << values[0] << "," << values[1] << ","
                << values[2] << "," << values[3] << "],\n";
        };
        const auto write_int_array = [&line_trace_file](
            const char* name, const std::vector<int>& values) {
            line_trace_file << "  \"" << name << "\": [";
            for (std::size_t i = 0; i < values.size(); ++i)
            {
                if (i != 0)
                    line_trace_file << ",";
                line_trace_file << values[i];
            }
            line_trace_file << "],\n";
        };
        const auto write_direction_point_arrays = [&line_trace_file](
            const char* name,
            const std::array<std::vector<CxFastMatchNormalTracePointEvidence>, 4>&
                values) {
            line_trace_file << "  \"" << name << "\": [";
            for (std::size_t direction = 0; direction < values.size(); ++direction)
            {
                if (direction != 0)
                    line_trace_file << ",";
                line_trace_file << "[";
                for (std::size_t index = 0; index < values[direction].size(); ++index)
                {
                    if (index != 0)
                        line_trace_file << ",";
                    line_trace_file << "[" << values[direction][index].x << ","
                                    << values[direction][index].y << "]";
                }
                line_trace_file << "]";
            }
            line_trace_file << "],\n";
        };
        line_trace_file << "{\n";
        line_trace_file << "  \"scan_line_count\": " << capture.scan_line_count << ",\n";
        line_trace_file << "  \"sample_count\": " << capture.sample_count << ",\n";
        line_trace_file << "  \"normal_trace_executed\": "
            << (capture.fastmatch_normal_trace.executed ? "true" : "false") << ",\n";
        line_trace_file << "  \"normal_trace_succeeded\": "
            << (capture.fastmatch_normal_trace.succeeded ? "true" : "false") << ",\n";
        line_trace_file << "  \"normal_trace_reason\": \""
            << JsonEscape(capture.fastmatch_normal_trace.reason) << "\",\n";
        line_trace_file << "  \"candidate_count\": "
            << capture.fastmatch_normal_trace.candidate_count << ",\n";
        line_trace_file << "  \"deduplicated_count\": "
            << capture.fastmatch_normal_trace.deduplicated_count << ",\n";
        line_trace_file << "  \"trace_point_count\": "
            << capture.fastmatch_normal_trace.trace_point_count << ",\n";
        line_trace_file << "  \"normal_pair_count\": "
            << capture.fastmatch_normal_trace.normal_pair_count << ",\n";
        line_trace_file << "  \"directional_side_count\": "
            << capture.fastmatch_normal_trace.directional_side_count << ",\n";
        line_trace_file << "  \"trace_segment_count\": "
            << capture.fastmatch_normal_trace.trace_segment_count << ",\n";
        line_trace_file << "  \"closure_error_px\": "
            << capture.fastmatch_normal_trace.closure_error_px << ",\n";
        line_trace_file << "  \"max_consecutive_gap_px\": "
            << capture.fastmatch_normal_trace.max_consecutive_gap_px << ",\n";
        line_trace_file << "  \"gradient_coverage\": "
            << capture.fastmatch_normal_trace.gradient_coverage << ",\n";
        line_trace_file << "  \"selected_anchor_coverage\": "
            << capture.fastmatch_normal_trace.selected_anchor_coverage << ",\n";
        line_trace_file << "  \"generated_conclusion_count\": "
            << capture.fastmatch_normal_trace.generated_conclusion_count << ",\n";
        line_trace_file << "  \"normal_pair_findline_bound_count\": "
            << capture.fastmatch_normal_trace.normal_pair_findline_bound_count
            << ",\n";
        line_trace_file << "  \"normal_pair_binding_miss_count\": "
            << capture.fastmatch_normal_trace.normal_pair_binding_miss_count
            << ",\n";
        line_trace_file << "  \"normal_pair_corner_rejected_count\": "
            << capture.fastmatch_normal_trace.normal_pair_corner_rejected_count
            << ",\n";
        line_trace_file << "  \"loop_erased_point_count\": "
            << capture.fastmatch_normal_trace.loop_erased_point_count
            << ",\n";
        write_int4("normal_pair_counts_by_direction",
            capture.fastmatch_normal_trace.normal_pair_counts_by_direction);
        line_trace_file << "  \"anchor_normal_band_px\": "
            << capture.fastmatch_normal_trace.anchor_normal_band_px << ",\n";
        write_int4("anchor_near_candidate_counts",
            capture.fastmatch_normal_trace.anchor_near_candidate_counts);
        write_int4("anchor_band_rejected_counts",
            capture.fastmatch_normal_trace.anchor_band_rejected_counts);
        write_int4("anchor_slope_rejected_counts",
            capture.fastmatch_normal_trace.anchor_slope_rejected_counts);
        write_int4("anchor_normal_rejected_counts",
            capture.fastmatch_normal_trace.anchor_normal_rejected_counts);
        write_int4("anchor_prefilter_accepted_counts",
            capture.fastmatch_normal_trace.anchor_prefilter_accepted_counts);
        write_int4("domain_deduplicated_counts",
            capture.fastmatch_normal_trace.domain_deduplicated_counts);
        write_int4("compressed_keypoint_counts",
            capture.fastmatch_normal_trace.compressed_keypoint_counts);
        write_int4("ann_edge_counts",
            capture.fastmatch_normal_trace.ann_edge_counts);
        write_int4("ann_component_counts",
            capture.fastmatch_normal_trace.ann_component_counts);
        write_int4("ann_selected_point_counts",
            capture.fastmatch_normal_trace.ann_selected_point_counts);
        write_double4("ann_selected_coverage",
            capture.fastmatch_normal_trace.ann_selected_coverage);
        write_double4("domain_join_allowed_gap_px",
            capture.fastmatch_normal_trace.domain_join_allowed_gap_px);
        write_double4("domain_join_selected_gap_px",
            capture.fastmatch_normal_trace.domain_join_selected_gap_px);
        write_int4("endpoint_spike_pruned_counts",
            capture.fastmatch_normal_trace.endpoint_spike_pruned_counts);
        line_trace_file << "  \"endpoint_spike_ratio_percent\": "
            << capture.fastmatch_normal_trace.endpoint_spike_ratio_percent << ",\n";
        line_trace_file << "  \"junction_tangent_window_points\": "
            << capture.fastmatch_normal_trace.junction_tangent_window_points << ",\n";
        line_trace_file << "  \"junction_min_cross_angle_deg\": "
            << capture.fastmatch_normal_trace.junction_min_cross_angle_deg << ",\n";
        line_trace_file << "  \"junction_max_extrapolation_percent\": "
            << capture.fastmatch_normal_trace.junction_max_extrapolation_percent << ",\n";
        line_trace_file << "  \"junction_join_spacing_multiplier_percent\": "
            << capture.fastmatch_normal_trace.junction_join_spacing_multiplier_percent
            << ",\n";
        write_int4("derived_junction_modes",
            capture.fastmatch_normal_trace.derived_junction_modes);
        write_double4("derived_junction_cross_angle_deg",
            capture.fastmatch_normal_trace.derived_junction_cross_angle_deg);
        write_double4("derived_junction_current_extrapolation_px",
            capture.fastmatch_normal_trace.derived_junction_current_extrapolation_px);
        write_double4("derived_junction_next_extrapolation_px",
            capture.fastmatch_normal_trace.derived_junction_next_extrapolation_px);
        write_direction_point_arrays("anchor_points_by_direction",
            capture.fastmatch_normal_trace.anchor_points_by_direction);
        write_direction_point_arrays("domain_points_by_direction",
            capture.fastmatch_normal_trace.domain_points_by_direction);
        write_direction_point_arrays("compressed_points_by_direction",
            capture.fastmatch_normal_trace.compressed_points_by_direction);
        write_direction_point_arrays("ann_selected_points_by_direction",
            capture.fastmatch_normal_trace.ann_selected_points_by_direction);
        write_point_array("derived_junction_points",
            capture.fastmatch_normal_trace.derived_junction_points, true);
        write_point_array("dijkstra_trace_points",
            capture.fastmatch_normal_trace.dijkstra_trace_points, true);
        write_int_array("dijkstra_source_directions",
            capture.fastmatch_normal_trace.dijkstra_source_directions);
        write_int_array("dijkstra_source_scans",
            capture.fastmatch_normal_trace.dijkstra_source_scans);
        write_point_array("derived_trace_points",
            capture.fastmatch_normal_trace.derived_trace_points, true);
        write_point_array("normal_pair_a",
            capture.fastmatch_normal_trace.normal_pair_a, true);
        write_point_array("normal_pair_b",
            capture.fastmatch_normal_trace.normal_pair_b, true);
        write_point_array("normal_pair_source_points",
            capture.fastmatch_normal_trace.normal_pair_source_points, true);
        write_int_array("normal_pair_source_directions",
            capture.fastmatch_normal_trace.normal_pair_source_directions);
        write_int_array("normal_pair_source_scans",
            capture.fastmatch_normal_trace.normal_pair_source_scans);
        line_trace_file << "  \"polarity_source\": "
            << "\"directional_findline_domain\"\n";
        line_trace_file << "}\n";
        line_trace_file.close();
    }

    const auto write_shape_model = [](std::ofstream& file, const char* key,
                                      const CxFastMatchShapeModel& model,
                                      bool trailing_comma) {
        file << "  \"" << key << "\": {\n"
             << "    \"model_id\": \"" << JsonEscape(model.model_id) << "\",\n"
             << "    \"status\": \"" << JsonEscape(model.status) << "\",\n"
             << "    \"available\": " << (model.available ? "true" : "false") << ",\n"
             << "    \"closed\": " << (model.closed ? "true" : "false") << ",\n"
             << "    \"source_conclusion_count\": " << model.source_conclusion_count << ",\n"
             << "    \"derived_point_count\": " << model.derived_point_count << ",\n"
             << "    \"subpixel_point_count\": " << model.subpixel_point_count << ",\n"
             << "    \"bbox\": [" << model.bbox_x << "," << model.bbox_y << ","
             << model.bbox_width << "," << model.bbox_height << "],\n"
             << "    \"centroid\": [" << model.centroid_x << "," << model.centroid_y << "],\n"
             << "    \"sparse_points\": [";
        for (std::size_t index = 0; index < model.sparse_points.size(); ++index) {
            if (index) file << ",";
            const auto& point = model.sparse_points[index];
            file << "{\"x\":" << point.x << ",\"y\":" << point.y
                 << ",\"nx\":" << point.normal_x << ",\"ny\":" << point.normal_y
                 << ",\"direction\":" << point.source_direction
                 << ",\"scan\":" << point.source_scan
                 << ",\"polarity\":" << point.polarity << "}";
        }
        file << "],\n    \"dense_points\": [";
        for (std::size_t index = 0; index < model.dense_points.size(); ++index) {
            if (index) file << ",";
            const auto& point = model.dense_points[index];
            file << "{\"x\":" << point.x << ",\"y\":" << point.y
                 << ",\"nx\":" << point.normal_x << ",\"ny\":" << point.normal_y
                 << ",\"curvature\":" << point.curvature
                 << ",\"gradient\":" << point.gradient_magnitude
                 << ",\"confidence\":" << point.confidence << "}";
        }
        file << "],\n    \"anchors\": [";
        for (std::size_t index = 0; index < model.anchors.size(); ++index) {
            if (index) file << ",";
            const auto& anchor = model.anchors[index];
            file << "{\"type\":\""
                 << CxFastMatchStructuralAnchorTypeName(anchor.type)
                 << "\",\"point_index\":" << anchor.point_index
                 << ",\"x\":" << anchor.x << ",\"y\":" << anchor.y
                 << ",\"strength\":" << anchor.strength
                 << ",\"source\":\"" << JsonEscape(anchor.source) << "\"}";
        }
        file << "]\n  }" << (trailing_comma ? "," : "") << "\n";
    };
    std::ofstream shape_model_file(shape_model_path);
    if (shape_model_file.is_open()) {
        shape_model_file << "{\n";
        write_shape_model(shape_model_file, "reference",
                          capture.fastmatch_reference_shape_model, true);
        write_shape_model(shape_model_file, "observed",
                          capture.fastmatch_observed_shape_model, false);
        shape_model_file << "}\n";
    }

    std::ofstream form_fit_file(form_fit_path);
    if (form_fit_file.is_open()) {
        const auto& fit = capture.fastmatch_form_fit;
        form_fit_file << "{\n"
            << "  \"executed\": " << (fit.executed ? "true" : "false") << ",\n"
            << "  \"succeeded\": " << (fit.succeeded ? "true" : "false") << ",\n"
            << "  \"budget_exceeded\": " << (fit.budget_exceeded ? "true" : "false") << ",\n"
            << "  \"status\": \"" << JsonEscape(fit.status) << "\",\n"
            << "  \"failure_stage\": \"" << JsonEscape(fit.failure_stage) << "\",\n"
            << "  \"counts\": {\"reference_sparse\":" << fit.reference_sparse_count
            << ",\"observed_sparse\":" << fit.observed_sparse_count
            << ",\"reference_dense\":" << fit.reference_dense_count
            << ",\"observed_dense\":" << fit.observed_dense_count
            << ",\"mutual\":" << fit.dense_mutual_count << "},\n"
            << "  \"coverage\": {\"forward\":" << fit.forward_coverage
            << ",\"reverse\":" << fit.reverse_coverage
            << ",\"mutual\":" << fit.mutual_coverage << "},\n"
            << "  \"residual\": {\"mean_px\":" << fit.mean_residual_px
            << ",\"max_px\":" << fit.max_residual_px
            << ",\"symmetric_px\":" << fit.symmetric_residual_px
            << ",\"normal_deg\":" << fit.normal_residual_deg << "},\n"
            << "  \"affine\": [" << fit.affine_a << "," << fit.affine_b << ","
            << fit.translate_x << "," << fit.affine_c << "," << fit.affine_d
            << "," << fit.translate_y << "],\n"
            << "  \"score\": " << fit.score << ",\n"
            << "  \"elapsed_ms\": " << fit.elapsed_ms << ",\n"
            << "  \"correspondences\": [";
        for (std::size_t index = 0; index < fit.correspondences.size(); ++index) {
            if (index) form_fit_file << ",";
            const auto& pair = fit.correspondences[index];
            form_fit_file << "{\"reference_index\":" << pair.reference_index
                << ",\"observed_index\":" << pair.observed_index
                << ",\"distance_px\":" << pair.distance_px
                << ",\"normal_delta_deg\":" << pair.normal_delta_deg
                << ",\"weight\":" << pair.weight
                << ",\"mutual\":" << (pair.mutual ? "true" : "false")
                << ",\"accepted\":" << (pair.accepted ? "true" : "false") << "}";
        }
        form_fit_file << "]\n}\n";
    }

    std::ofstream gauge_file(form_fit_gauge_path);
    if (gauge_file.is_open()) {
        const auto& gauge = capture.fastmatch_form_fit_gauge;
        gauge_file << "{\n  \"gauge_id\": \"" << JsonEscape(gauge.gauge_id)
                   << "\",\n  \"name\": \"" << JsonEscape(gauge.name)
                   << "\",\n  \"learn_score\": " << gauge.learn_score
                   << ",\n  \"elements\": [";
        for (std::size_t index = 0; index < gauge.elements.size(); ++index) {
            if (index) gauge_file << ",";
            const auto& element = gauge.elements[index];
            gauge_file << "{\"element_id\":\"" << JsonEscape(element.element_id)
                       << "\",\"type\":\""
                       << cxcore::formfit::GaugeElementTypeName(element.element_type)
                       << "\",\"source\":\"" << JsonEscape(element.source_entity_id)
                       << "\",\"confidence\":" << element.confidence
                       << ",\"bbox\":[" << element.bbox_x << "," << element.bbox_y
                       << "," << element.bbox_width << "," << element.bbox_height
                       << "],\"variables\":[";
            for (std::size_t variable_index = 0;
                 variable_index < element.variables.size(); ++variable_index) {
                if (variable_index) gauge_file << ",";
                const auto& variable = element.variables[variable_index];
                gauge_file << "{\"name\":\"" << JsonEscape(variable.name)
                           << "\",\"value\":" << variable.value << "}";
            }
            gauge_file << "]}";
        }
        gauge_file << "],\n  \"constraints\": [";
        for (std::size_t index = 0; index < gauge.constraints.size(); ++index) {
            if (index) gauge_file << ",";
            const auto& constraint = gauge.constraints[index];
            gauge_file << "{\"constraint_id\":\""
                       << JsonEscape(constraint.constraint_id)
                       << "\",\"type\":\""
                       << cxcore::formfit::GaugeConstraintTypeName(
                              constraint.constraint_type)
                       << "\",\"target\":" << constraint.target_value
                       << ",\"tolerance\":" << constraint.tolerance << "}";
        }
        gauge_file << "]\n}\n";
    }

    const std::map<std::string, double> parameter_snapshot =
        BuildHeadlessGlobalOverrides(options);
    const auto parameter_value = [&parameter_snapshot](const char* name) {
        const auto it = parameter_snapshot.find(name);
        return it == parameter_snapshot.end() ? 0.0 : it->second;
    };
    const int ocr_glyph_source = static_cast<int>(
        parameter_value("global_ocr_glyph_source"));
    const std::string ocr_effective_source =
        ocr_glyph_source == 2 ? "findobject_glyph" :
        (ocr_glyph_source == 1 || !capture.fastmatch_pose_candidates.empty())
            ? "fastmatch_pose" : "findobject_glyph";

    std::ofstream variable_snapshot_file(variable_snapshot_path);
    if (variable_snapshot_file.is_open())
    {
        variable_snapshot_file << "{\n";
        variable_snapshot_file << "  \"roi_x0\": " << options.roi_x0 << ",\n";
        variable_snapshot_file << "  \"roi_y0\": " << options.roi_y0 << ",\n";
        variable_snapshot_file << "  \"roi_x1\": " << options.roi_x1 << ",\n";
        variable_snapshot_file << "  \"roi_y1\": " << options.roi_y1 << ",\n";
        variable_snapshot_file << "  \"strategy_id\": " << capture.strategy_id << ",\n";
        variable_snapshot_file << "  \"threshold\": " << options.threshold << ",\n";
        variable_snapshot_file << "  \"method\": " << options.method << ",\n";
        variable_snapshot_file << "  \"wgap\": " << options.wgap << ",\n";
        variable_snapshot_file << "  \"hgap\": " << options.hgap << ",\n";
        variable_snapshot_file << "  \"linegap\": " << options.linegap << ",\n";
        variable_snapshot_file << "  \"min_edge_run_width_px\": "
                               << options.min_edge_run_width_px << ",\n";
        variable_snapshot_file << "  \"selected_threshold\": " << capture.selected_threshold << ",\n";
        variable_snapshot_file << "  \"selected_method\": " << capture.selected_method << ",\n";
        variable_snapshot_file << "  \"selected_wgap\": " << capture.selected_wgap << ",\n";
        variable_snapshot_file << "  \"selected_hgap\": " << capture.selected_hgap << ",\n";
        variable_snapshot_file << "  \"selected_linegap\": " << capture.selected_linegap << ",\n";
        variable_snapshot_file << "  \"selected_filterprofile\": " << capture.selected_filterprofile << ",\n";
        variable_snapshot_file << "  \"selected_min_edge_run_width_px\": "
                               << capture.selected_min_edge_run_width_px << ",\n";
        variable_snapshot_file << "  \"fastmatch_geometry_source_index\": "
                               << parameter_value("global_fastmatch_geometry_source_index") << ",\n";
        variable_snapshot_file << "  \"fastmatch_geometry_weight_percent\": "
                               << parameter_value("global_fastmatch_geometry_weight_percent") << ",\n";
        variable_snapshot_file << "  \"fastmatch_max_pose_candidates\": "
                               << parameter_value("global_fastmatch_max_pose_candidates") << ",\n";
        variable_snapshot_file << "  \"ocr_threshold\": "
                               << parameter_value("global_ocr_threshold") << ",\n";
        variable_snapshot_file << "  \"ocr_foreground_mode\": "
                               << parameter_value("global_ocr_foreground_mode") << ",\n";
        variable_snapshot_file << "  \"ocr_min_area\": "
                               << parameter_value("global_ocr_min_area") << ",\n";
        variable_snapshot_file << "  \"ocr_component_distance\": "
                               << parameter_value("global_ocr_component_distance") << ",\n";
        variable_snapshot_file << "  \"ocr_glyph_source_policy\": "
                               << ocr_glyph_source << ",\n";
        variable_snapshot_file << "  \"ocr_layout_direction\": "
                               << parameter_value("global_ocr_layout_direction") << ",\n";
        variable_snapshot_file << "  \"ocr_line_overlap_percent\": "
                               << parameter_value("global_ocr_line_overlap_percent") << ",\n";
        variable_snapshot_file << "  \"ocr_match_threshold\": "
                               << parameter_value("global_ocr_match_threshold") << ",\n";
        variable_snapshot_file << "  \"ocr_min_score_percent\": "
                               << parameter_value("global_ocr_min_score_percent") << ",\n";
        variable_snapshot_file << "  \"ocr_effective_source\": \""
                               << ocr_effective_source << "\"\n";
        variable_snapshot_file << "}\n";
        variable_snapshot_file.close();
    }

    std::ofstream object_state_file(object_state_path);
    if (object_state_file.is_open())
    {
        object_state_file << "{\n";
        object_state_file << "  \"valid_points_count\": " << capture.valid_points_count << ",\n";
        object_state_file << "  \"has_fit_line\": " << (capture.has_fit_line ? "true" : "false") << ",\n";
        object_state_file << "  \"has_fit_circle\": " << (capture.has_fit_circle ? "true" : "false") << ",\n";
        object_state_file << "  \"has_fit_ellipse\": " << (capture.has_fit_ellipse ? "true" : "false") << ",\n";
        object_state_file << "  \"has_result_rect\": " << (capture.has_result_rect ? "true" : "false") << ",\n";
        object_state_file << "  \"circle_radius\": " << capture.circle_radius << ",\n";
        object_state_file << "  \"ellipse_cx\": " << capture.ellipse_cx << ",\n";
        object_state_file << "  \"ellipse_cy\": " << capture.ellipse_cy << ",\n";
        object_state_file << "  \"ellipse_radius_x\": " << capture.ellipse_radius_x << ",\n";
        object_state_file << "  \"ellipse_radius_y\": " << capture.ellipse_radius_y << ",\n";
        object_state_file << "  \"ellipse_angle_deg\": " << capture.ellipse_angle_deg << ",\n";
        object_state_file << "  \"ellipse_selected_edge_index\": " << capture.ellipse_selected_edge_index << ",\n";
        object_state_file << "  \"ellipse_point_consistency_enabled\": " << capture.ellipse_point_consistency_enabled << ",\n";
        object_state_file << "  \"ellipse_point_consistency_range\": " << capture.ellipse_point_consistency_range << ",\n";
        object_state_file << "  \"ellipse_point_consistency_input_points\": " << capture.ellipse_point_consistency_input_points << ",\n";
        object_state_file << "  \"ellipse_point_consistency_output_points\": " << capture.ellipse_point_consistency_output_points << ",\n";
        object_state_file << "  \"ellipse_point_consistency_removed_points\": " << capture.ellipse_point_consistency_removed_points << ",\n";
        object_state_file << "  \"avgdist\": " << capture.avgdist << ",\n";
        object_state_file << "  \"local_support\": " << capture.boundary_coverage_ratio << ",\n";
        object_state_file << "  \"local_mean_distance\": " << capture.boundary_residual_rmse_px << ",\n";
        object_state_file << "  \"fit_offset\": " << capture.boundary_residual_p95_px << ",\n";
        object_state_file << "  \"boundary_coverage_ratio\": " << capture.boundary_coverage_ratio << ",\n";
        object_state_file << "  \"boundary_residual_rmse_px\": " << capture.boundary_residual_rmse_px << ",\n";
        object_state_file << "  \"boundary_residual_p95_px\": " << capture.boundary_residual_p95_px << ",\n";
        object_state_file << R"(  "boundary_residual_max_px": )" << capture.boundary_residual_max_px << ",\n";
        object_state_file << R"(  "boundary_subpixel_offset_mean": )" << capture.boundary_subpixel_offset_mean << ",\n";
        object_state_file << R"(  "boundary_subpixel_offset_stddev": )" << capture.boundary_subpixel_offset_stddev << ",\n";
        object_state_file << R"(  "boundary_localization_sigma_mean_px": )" << capture.boundary_localization_sigma_mean_px << ",\n";
        object_state_file << "  \"boundary_reliability_score\": " << capture.boundary_reliability_score << ",\n";
        object_state_file << "  \"result_rect_count\": " << capture.result_rect_count << ",\n";
        object_state_file << "  \"top1_rect_x\": " << capture.top1_rect_x << ",\n";
        object_state_file << "  \"top1_rect_y\": " << capture.top1_rect_y << ",\n";
        object_state_file << "  \"top1_rect_w\": " << capture.top1_rect_w << ",\n";
        object_state_file << "  \"top1_rect_h\": " << capture.top1_rect_h << ",\n";
        object_state_file << "  \"model_point_count\": " << capture.model_point_count << ",\n";
        object_state_file << "  \"fastmatch_learn_a_count\": " << capture.fastmatch_learn_a_count << ",\n";
        object_state_file << "  \"fastmatch_learn_b_count\": " << capture.fastmatch_learn_b_count << ",\n";
        object_state_file << "  \"fastmatch_learn_a2_count\": " << capture.fastmatch_learn_a2_count << ",\n";
        object_state_file << "  \"fastmatch_learn_b2_count\": " << capture.fastmatch_learn_b2_count << ",\n";
        object_state_file << "  \"fastmatch_learn_status_code\": " << capture.fastmatch_learn_status_code << ",\n";
        object_state_file << "  \"fastmatch_normal_trace\": {"
            << "\"executed\":" << (capture.fastmatch_normal_trace.executed ? "true" : "false")
            << ",\"succeeded\":" << (capture.fastmatch_normal_trace.succeeded ? "true" : "false")
            << ",\"reason\":\"" << JsonEscape(capture.fastmatch_normal_trace.reason) << "\""
            << ",\"candidate_count\":" << capture.fastmatch_normal_trace.candidate_count
            << ",\"deduplicated_count\":" << capture.fastmatch_normal_trace.deduplicated_count
            << ",\"trace_point_count\":" << capture.fastmatch_normal_trace.trace_point_count
            << ",\"normal_pair_count\":" << capture.fastmatch_normal_trace.normal_pair_count
            << ",\"directional_side_count\":" << capture.fastmatch_normal_trace.directional_side_count
            << ",\"trace_segment_count\":" << capture.fastmatch_normal_trace.trace_segment_count
            << ",\"closure_error_px\":" << capture.fastmatch_normal_trace.closure_error_px
            << ",\"max_consecutive_gap_px\":" << capture.fastmatch_normal_trace.max_consecutive_gap_px
            << ",\"gradient_coverage\":" << capture.fastmatch_normal_trace.gradient_coverage
            << ",\"selected_anchor_coverage\":"
            << capture.fastmatch_normal_trace.selected_anchor_coverage
            << ",\"generated_conclusion_count\":"
            << capture.fastmatch_normal_trace.generated_conclusion_count
            << ",\"normal_pair_findline_bound_count\":"
            << capture.fastmatch_normal_trace.normal_pair_findline_bound_count
            << ",\"normal_pair_binding_miss_count\":"
            << capture.fastmatch_normal_trace.normal_pair_binding_miss_count
            << ",\"normal_pair_corner_rejected_count\":"
            << capture.fastmatch_normal_trace.normal_pair_corner_rejected_count
            << ",\"loop_erased_point_count\":"
            << capture.fastmatch_normal_trace.loop_erased_point_count
            << ",\"anchor_normal_band_px\":"
            << capture.fastmatch_normal_trace.anchor_normal_band_px
            << "},\n";
        object_state_file << "  \"fastmatch_model_width\": " << capture.fastmatch_model_width << ",\n";
        object_state_file << "  \"fastmatch_model_height\": " << capture.fastmatch_model_height << ",\n";
        object_state_file << "  \"fastmatch_pattern_a_count\": " << capture.fastmatch_pattern_a_count << ",\n";
        object_state_file << "  \"fastmatch_pattern_b_count\": " << capture.fastmatch_pattern_b_count << ",\n";
        object_state_file << "  \"fastmatch_pattern_a_x\": " << capture.fastmatch_pattern_a_x << ",\n";
        object_state_file << "  \"fastmatch_pattern_a_y\": " << capture.fastmatch_pattern_a_y << ",\n";
        object_state_file << "  \"fastmatch_pattern_a_width\": " << capture.fastmatch_pattern_a_width << ",\n";
        object_state_file << "  \"fastmatch_pattern_a_height\": " << capture.fastmatch_pattern_a_height << ",\n";
        object_state_file << "  \"fastmatch_pattern_b_x\": " << capture.fastmatch_pattern_b_x << ",\n";
        object_state_file << "  \"fastmatch_pattern_b_y\": " << capture.fastmatch_pattern_b_y << ",\n";
        object_state_file << "  \"fastmatch_pattern_b_width\": " << capture.fastmatch_pattern_b_width << ",\n";
        object_state_file << "  \"fastmatch_pattern_b_height\": " << capture.fastmatch_pattern_b_height << ",\n";
        object_state_file << "  \"candidate_count\": " << capture.candidate_count << ",\n";
        object_state_file << "  \"best_score\": " << capture.best_score << ",\n";
        object_state_file << "  \"has_result_box\": " << (capture.has_result_box ? "true" : "false") << ",\n";
        object_state_file << "  \"has_best_result\": " << (capture.has_best_result ? "true" : "false") << ",\n";
        object_state_file << "  \"fastmatch_match_call_count\": " << capture.fastmatch_match_call_count << ",\n";
        object_state_file << "  \"fastmatch_match_ab_call_count\": " << capture.fastmatch_match_ab_call_count << ",\n";
        object_state_file << "  \"fastmatch_match_sample_ab_call_count\": " << capture.fastmatch_match_sample_ab_call_count << ",\n";
        object_state_file << "  \"fastmatch_match_last_stage\": " << capture.fastmatch_match_last_stage << ",\n";
        object_state_file << "  \"fastmatch_match_image_width\": " << capture.fastmatch_match_image_width << ",\n";
        object_state_file << "  \"fastmatch_match_image_height\": " << capture.fastmatch_match_image_height << ",\n";
        object_state_file << "  \"fastmatch_learn_rect_x0\": " << capture.fastmatch_learn_rect_x0 << ",\n";
        object_state_file << "  \"fastmatch_learn_rect_y0\": " << capture.fastmatch_learn_rect_y0 << ",\n";
        object_state_file << "  \"fastmatch_learn_rect_x1\": " << capture.fastmatch_learn_rect_x1 << ",\n";
        object_state_file << "  \"fastmatch_learn_rect_y1\": " << capture.fastmatch_learn_rect_y1 << ",\n";
        object_state_file << "  \"fastmatch_match_rect_x0\": " << capture.fastmatch_match_rect_x0 << ",\n";
        object_state_file << "  \"fastmatch_match_rect_y0\": " << capture.fastmatch_match_rect_y0 << ",\n";
        object_state_file << "  \"fastmatch_match_rect_x1\": " << capture.fastmatch_match_rect_x1 << ",\n";
        object_state_file << "  \"fastmatch_match_rect_y1\": " << capture.fastmatch_match_rect_y1 << ",\n";
        object_state_file << "  \"fastmatch_raw_probe_count\": " << capture.fastmatch_raw_probe_count << ",\n";
        object_state_file << "  \"fastmatch_raw_threshold_hit_count\": " << capture.fastmatch_raw_threshold_hit_count << ",\n";
        object_state_file << "  \"fastmatch_result_to_list_count\": " << capture.fastmatch_result_to_list_count << ",\n";
        object_state_file << "  \"fastmatch_candidate_insert_count\": " << capture.fastmatch_candidate_insert_count << ",\n";
        object_state_file << "  \"fastmatch_candidate_replace_count\": " << capture.fastmatch_candidate_replace_count << ",\n";
        object_state_file
            << R"(  "fastmatch_candidate_reject_count": )"
            << capture.fastmatch_candidate_reject_count << ','
            << static_cast<char>(10);
        object_state_file
            << R"(  "fastmatch_rotate_budget": {"exceeded": )"
            << (capture.fastmatch_rotate_budget_exceeded ? "true" : "false")
            << R"(, "candidate_count": )" << capture.fastmatch_rotate_candidate_count
            << R"(, "probe_count": )" << capture.fastmatch_rotate_probe_count
            << R"(, "sample_count": )" << capture.fastmatch_rotate_sample_count
            << R"(, "elapsed_ms": )" << capture.fastmatch_rotate_elapsed_ms
            << R"(},)" << static_cast<char>(10);
        const CxFastMatchTemplateGeometryEvidence &template_geometry =
            capture.fastmatch_template_geometry;
        object_state_file
            << R"(  "fastmatch_template_geometry": {"available": )"
            << (template_geometry.available ? "true" : "false")
            << R"(, "source_object_index": )"
            << template_geometry.source_object_index
            << R"(, "source_object_ref": )" << static_cast<char>(34)
            << JsonEscape(template_geometry.source_object_ref)
            << static_cast<char>(34) << R"(, "bbox": [)"
            << template_geometry.bbox_x << ", " << template_geometry.bbox_y
            << ", " << template_geometry.bbox_w << ", "
            << template_geometry.bbox_h << R"(], "centroid": [)"
            << template_geometry.centroid_x << ", "
            << template_geometry.centroid_y << R"(], "projected_area": )"
            << template_geometry.projected_area
            << R"(, "major_axis_length": )"
            << template_geometry.major_axis_length
            << R"(, "minor_axis_length": )"
            << template_geometry.minor_axis_length
            << R"(, "orientation_deg": )"
            << template_geometry.orientation_deg
            << R"(, "aspect_ratio": )" << template_geometry.aspect_ratio
            << R"(, "solidity": )" << template_geometry.solidity
            << R"(, "boundary_point_count": )"
            << template_geometry.boundary_point_count
            << R"(, "status": )" << static_cast<char>(34)
            << JsonEscape(template_geometry.status) << static_cast<char>(34)
            << "}," << static_cast<char>(10);
        object_state_file << R"(  "fastmatch_pose_candidates": [)";
        for (std::size_t index = 0;
             index < capture.fastmatch_pose_candidates.size(); ++index) {
            const CxFastMatchPoseCandidateEvidence &candidate =
                capture.fastmatch_pose_candidates[index];
            if (index > 0)
                object_state_file << ", ";
            object_state_file
                << R"({"candidate_index": )" << candidate.candidate_index
                << R"(, "observed_geometry_index": )"
                << candidate.observed_geometry_index
                << R"(, "observed_geometry_ref": )"
                << static_cast<char>(34)
                << JsonEscape(candidate.observed_geometry_ref)
                << static_cast<char>(34) << R"(, "bbox": [)"
                << candidate.bbox_x << ", " << candidate.bbox_y << ", "
                << candidate.bbox_w << ", " << candidate.bbox_h
                << R"(], "center": [)" << candidate.center_x << ", "
                << candidate.center_y << R"(], "angle_deg": )"
                << candidate.angle_deg << R"(, "scale_x": )"
                << candidate.scale_x << R"(, "scale_y": )"
                << candidate.scale_y << R"(, "appearance_score": )"
                << candidate.appearance_score << R"(, "geometry_score": )"
                << candidate.geometry_score << R"(, "combined_score": )"
                << candidate.combined_score
                << R"(, "boundary_point_count": )"
                << candidate.boundary_point_count << R"(, "status": )"
                << static_cast<char>(34) << JsonEscape(candidate.status)
                << static_cast<char>(34) << '}';
        }
        object_state_file << "]," << static_cast<char>(10);
        const CxFastMatchTransformSearchEvidence& transform_search =
            capture.fastmatch_transform_search;
        object_state_file
            << R"(  "fastmatch_transform_search": {"executed": )"
            << (transform_search.executed ? "true" : "false")
            << R"(, "converged": )" << (transform_search.converged ? "true" : "false")
            << R"(, "budget_exceeded": )" << (transform_search.budget_exceeded ? "true" : "false")
            << R"(, "calibration_applied": )" << (transform_search.calibration_applied ? "true" : "false")
            << R"(, "calibration_snapshot_hash": ")" << JsonEscape(transform_search.calibration_snapshot_hash)
            << R"(", "calibration_source_ref": ")" << JsonEscape(transform_search.calibration_source_ref)
            << R"(", "calibration_coordinate_frame_id": ")" << JsonEscape(transform_search.calibration_coordinate_frame_id)
            << R"(", "calibration_xy_unit": ")" << JsonEscape(transform_search.calibration_xy_unit)
            << R"(", "calibration_reprojection_rmse_px": )" << transform_search.calibration_reprojection_rmse_px
            << R"(, "seed_available": )" << (transform_search.seed_available ? "true" : "false")
            << R"(, "seed_source": ")" << JsonEscape(transform_search.seed_source)
            << R"(", "seed_model_id": ")" << JsonEscape(transform_search.seed_model_id)
            << R"(", "seed_detection_index": )" << transform_search.seed_detection_index
            << R"(, "seed_class_id": )" << transform_search.seed_class_id
            << R"(, "seed_confidence": )" << transform_search.seed_confidence
            << R"(, "seed_angle_convention": ")" << JsonEscape(transform_search.seed_angle_convention)
            << R"(", "seed_reason": ")" << JsonEscape(transform_search.seed_reason)
            << R"(", "initial": {"cx": )" << transform_search.initial_cx
            << R"(, "cy": )" << transform_search.initial_cy
            << R"(, "angle_deg": )" << transform_search.initial_angle_deg
            << R"(, "scale_x": )" << transform_search.initial_scale_x
            << R"(, "scale_y": )" << transform_search.initial_scale_y
            << R"(}, "best": {"cx": )" << transform_search.best_cx
            << R"(, "cy": )" << transform_search.best_cy
            << R"(, "angle_deg": )" << transform_search.best_angle_deg
            << R"(, "scale_x": )" << transform_search.best_scale_x
            << R"(, "scale_y": )" << transform_search.best_scale_y
            << R"(, "shear": )" << transform_search.best_shear
            << R"(, "projective_u": )" << transform_search.best_projective_u
            << R"(, "projective_v": )" << transform_search.best_projective_v
            << R"(, "physical_cx": )" << transform_search.best_physical_cx
            << R"(, "physical_cy": )" << transform_search.best_physical_cy
            << R"(}, "best_score": )" << transform_search.best_score
            << R"(, "appearance_score": )" << transform_search.appearance_score
            << R"(, "continuity_score": )" << transform_search.continuity_score
            << R"(, "gradient_score": )" << transform_search.gradient_score
            << R"(, "geometric_residual_px": )" << transform_search.geometric_residual_px
            << R"(, "rigid_baseline_score": )" << transform_search.rigid_baseline_score
            << R"(, "evaluated_candidates": )" << transform_search.evaluated_candidates
            << R"(, "accepted_candidates": )" << transform_search.accepted_candidates
            << R"(, "sample_count": )" << transform_search.sample_count
            << R"(, "elapsed_ms": )" << transform_search.elapsed_ms
            << R"(, "failure_stage": ")" << JsonEscape(transform_search.failure_stage)
            << R"("},)" << static_cast<char>(10);
        object_state_file << R"(  "ocr_glyph_source_policy": )"
                          << ocr_glyph_source << ',' << static_cast<char>(10);
        object_state_file << R"(  "ocr_effective_source": )"
                          << static_cast<char>(34)
                          << JsonEscape(ocr_effective_source)
                          << static_cast<char>(34) << ',' << static_cast<char>(10);
        object_state_file << R"(  "ocr_final_text": )"
                          << static_cast<char>(34)
                          << JsonEscape(capture.ocr_final_text)
                          << static_cast<char>(34) << ','
                          << static_cast<char>(10);
        object_state_file << R"(  "ocr_failure_reason": )"
                          << static_cast<char>(34)
                          << JsonEscape(capture.ocr_failure_reason)
                          << static_cast<char>(34) << ','
                          << static_cast<char>(10);
        object_state_file << R"(  "ocr_glyph_candidates": [)";
        for (std::size_t index = 0;
             index < capture.ocr_glyph_candidates.size(); ++index) {
            const CxOcrGlyphCandidateEvidence &glyph =
                capture.ocr_glyph_candidates[index];
            if (index > 0)
                object_state_file << ", ";
            object_state_file
                << R"({"source_object_index": )"
                << glyph.source_object_index
                << R"(, "source_object_ref": )" << static_cast<char>(34)
                << JsonEscape(glyph.source_object_ref)
                << static_cast<char>(34) << R"(, "line_index": )"
                << glyph.line_index << R"(, "reading_order": )"
                << glyph.reading_order << R"(, "bbox": [)" << glyph.bbox_x
                << ", " << glyph.bbox_y << ", " << glyph.bbox_w << ", "
                << glyph.bbox_h << R"(], "centroid": [)"
                << glyph.centroid_x << ", " << glyph.centroid_y
                << R"(], "orientation_deg": )" << glyph.orientation_deg
                << R"(, "projected_area": )" << glyph.projected_area
                << R"(, "aspect_ratio": )" << glyph.aspect_ratio
                << R"(, "solidity": )" << glyph.solidity
                << R"(, "label": )" << static_cast<char>(34)
                << JsonEscape(glyph.label) << static_cast<char>(34)
                << R"(, "appearance_score": )" << glyph.appearance_score
                << R"(, "geometry_score": )" << glyph.geometry_score
                << R"(, "confidence": )" << glyph.confidence
                << R"(, "status": )" << static_cast<char>(34)
                << JsonEscape(glyph.status) << static_cast<char>(34) << '}';
        }
        object_state_file << "]," << static_cast<char>(10);
        object_state_file << "  \"object_prefilter_requested\": " << (capture.object_prefilter_requested ? "true" : "false") << ",\n";
        object_state_file << "  \"object_prefilter_applied\": " << (capture.object_prefilter_applied ? "true" : "false") << ",\n";
        object_state_file << "  \"actual_findsetting\": " << capture.actual_findsetting << ",\n";
        object_state_file << "  \"findobject_strategy_id\": " << capture.object_filter_strategy_id << ",\n";
        object_state_file << "  \"object_filter_borw\": " << capture.object_filter_borw << ",\n";
        object_state_file << "  \"object_filter_min\": " << capture.object_filter_min << ",\n";
        object_state_file << "  \"object_filter_max\": " << capture.object_filter_max << ",\n";
        object_state_file << "  \"tool_effective_method\": " << capture.tool_method << ",\n";
        object_state_file << "  \"tool_effective_threshold\": " << capture.tool_threshold << ",\n";
        object_state_file << "  \"tool_input_line_x0\": " << capture.tool_input_line_x0 << ",\n";
        object_state_file << "  \"tool_input_line_y0\": " << capture.tool_input_line_y0 << ",\n";
        object_state_file << "  \"tool_input_line_x1\": " << capture.tool_input_line_x1 << ",\n";
        object_state_file << "  \"tool_input_line_y1\": " << capture.tool_input_line_y1 << ",\n";
        object_state_file << "  \"tool_input_line_half_width\": " << capture.tool_input_line_half_width << ",\n";
        object_state_file << "  \"tool_input_circle_cx\": " << capture.tool_input_circle_cx << ",\n";
        object_state_file << "  \"tool_input_circle_cy\": " << capture.tool_input_circle_cy << ",\n";
        object_state_file << "  \"tool_input_circle_px\": " << capture.tool_input_circle_px << ",\n";
        object_state_file << "  \"tool_input_circle_py\": " << capture.tool_input_circle_py << ",\n";
        object_state_file << "  \"tool_input_circle_gap\": " << capture.tool_input_circle_gap << ",\n";
        object_state_file << "  \"scan_rows_examined\": " << capture.scan_rows_examined << ",\n";
        object_state_file << "  \"scan_rows_with_foreground\": " << capture.scan_rows_with_foreground << ",\n";
        object_state_file << "  \"scan_runs_total\": " << capture.scan_runs_total << ",\n";
        object_state_file << "  \"scan_runs_within_length_limit\": " << capture.scan_runs_within_length_limit << ",\n";
        object_state_file << "  \"scan_runs_over_length_limit\": " << capture.scan_runs_over_length_limit << ",\n";
        object_state_file << "  \"scan_runs_rejected_by_min_edge_width\": "
                          << capture.scan_runs_rejected_by_min_edge_width
                          << ",\n";
        object_state_file << "  \"scan_runs_rejected_by_selection\": " << capture.scan_runs_rejected_by_selection << ",\n";
        object_state_file << "  \"scan_runs_rejected_near_endpoint\": " << capture.scan_runs_rejected_near_endpoint << ",\n";
        object_state_file << "  \"scan_points_emitted\": " << capture.scan_points_emitted << ",\n";
        object_state_file << "  \"findline_point_consistency_enabled\": " << capture.findline_point_consistency_enabled << ",\n";
        object_state_file << "  \"findline_point_consistency_range\": " << capture.findline_point_consistency_range << ",\n";
        object_state_file << "  \"findline_point_consistency_input_points\": " << capture.findline_point_consistency_input_points << ",\n";
        object_state_file << "  \"findline_point_consistency_output_points\": " << capture.findline_point_consistency_output_points << ",\n";
        object_state_file << "  \"findline_point_consistency_removed_points\": " << capture.findline_point_consistency_removed_points << ",\n";
        object_state_file << "  \"findcircle_point_consistency_enabled\": " << capture.circle_point_consistency_enabled << ",\n";
        object_state_file << "  \"findcircle_point_consistency_range\": " << capture.circle_point_consistency_range << ",\n";
        object_state_file << "  \"findcircle_point_consistency_input_points\": " << capture.circle_point_consistency_input_points << ",\n";
        object_state_file << "  \"findcircle_point_consistency_output_points\": " << capture.circle_point_consistency_output_points << ",\n";
        object_state_file << "  \"findcircle_point_consistency_removed_points\": " << capture.circle_point_consistency_removed_points << ",\n";
        object_state_file << "  \"findobject_foreground_before\": " << capture.object_foreground_before << ",\n";
        object_state_file << "  \"findobject_foreground_after\": " << capture.object_foreground_after << ",\n";
        object_state_file << "  \"fit_filter_input_count\": " << capture.fit_filter_input_count << ",\n";
        object_state_file << "  \"fit_filter_kept_count\": " << capture.fit_filter_kept_count << ",\n";
        object_state_file << "  \"fit_filter_rejected_count\": " << capture.fit_filter_rejected_count << ",\n";
        object_state_file << "  \"fit_filter_sigma\": " << capture.fit_filter_sigma << ",\n";
        object_state_file << "  \"fit_filter_threshold\": " << capture.fit_filter_threshold << ",\n";
        object_state_file << "  \"findrect_seed_valid\": " << (capture.findrect_seed_valid ? "true" : "false") << ",\n";
        object_state_file << "  \"findrect_top_valid\": " << (capture.findrect_top_valid ? "true" : "false") << ",\n";
        object_state_file << "  \"findrect_bottom_valid\": " << (capture.findrect_bottom_valid ? "true" : "false") << ",\n";
        object_state_file << "  \"findrect_left_valid\": " << (capture.findrect_left_valid ? "true" : "false") << ",\n";
        object_state_file << "  \"findrect_right_valid\": " << (capture.findrect_right_valid ? "true" : "false") << ",\n";
        object_state_file << "  \"findrect_top_points\": " << capture.findrect_top_points << ",\n";
        object_state_file << "  \"findrect_bottom_points\": " << capture.findrect_bottom_points << ",\n";
        object_state_file << "  \"findrect_left_points\": " << capture.findrect_left_points << ",\n";
        object_state_file << "  \"findrect_right_points\": " << capture.findrect_right_points << ",\n";
        object_state_file << "  \"findrect_coarse_score\": " << capture.findrect_coarse_score << ",\n";
        object_state_file << "  \"findrect_refine_score\": " << capture.findrect_refine_score << ",\n";
        object_state_file << "  \"segmentation_status_code\": " << capture.segmentation_status_code << ",\n";
        object_state_file << "  \"segmentation_contour_count\": " << capture.segmentation_contour_count << ",\n";
        object_state_file << "  \"segmentation_primary_area\": " << capture.segmentation_primary_area << ",\n";
        object_state_file << "  \"segmentation_result_ref\": \"" << JsonEscape(capture.segmentation_result_ref) << "\",\n";
        object_state_file << "  \"segmentation_mask_ref\": \"" << JsonEscape(capture.segmentation_mask_ref) << "\",\n";
        object_state_file << "  \"segmentation_contour_ref\": \"" << JsonEscape(capture.segmentation_contour_ref) << "\",\n";
        object_state_file << "  \"segmentation_overlay_ref\": \"" << JsonEscape(capture.segmentation_overlay_ref) << "\",\n";
        object_state_file << "  \"segmentation_task_id\": \"" << JsonEscape(capture.segmentation_task_id) << "\",\n";
        object_state_file << "  \"segmentation_model_id\": \"" << JsonEscape(capture.segmentation_model_id) << "\",\n";
        object_state_file << "  \"segmentation_model_package_ref\": \"" << JsonEscape(capture.segmentation_model_package_ref) << "\",\n";
        object_state_file << "  \"segmentation_manifest_path\": \"" << JsonEscape(capture.segmentation_manifest_path) << "\",\n";
        object_state_file << "  \"segmentation_postprocess_profile\": \"" << JsonEscape(capture.segmentation_postprocess_profile) << "\",\n";
        object_state_file << "  \"segmentation_parameter_profile_ref\": \"" << JsonEscape(capture.segmentation_parameter_profile_ref) << "\",\n";
        object_state_file << "  \"segmentation_region_count\": " << capture.segmentation_region_count << ",\n";
        object_state_file << "  \"segmentation_raw_result_available\": " << (capture.segmentation_raw_result_available ? "true" : "false") << ",\n";
        object_state_file << "  \"segmentation_refined_result_available\": " << (capture.segmentation_refined_result_available ? "true" : "false") << ",\n";
        object_state_file << "  \"segmentation_fallback_used\": " << (capture.segmentation_fallback_used ? "true" : "false") << ",\n";
        object_state_file << "  \"segmentation_result_stage\": \"" << JsonEscape(capture.segmentation_result_stage) << "\",\n";
        object_state_file << "  \"segmentation_refinement_method\": \"" << JsonEscape(capture.segmentation_refinement_method) << "\",\n";
        object_state_file << "  \"segmentation_requested_geometry_type\": \"" << JsonEscape(capture.segmentation_requested_geometry_type) << "\",\n";
        object_state_file << "  \"segmentation_geometry_fit_status\": \"" << JsonEscape(capture.segmentation_geometry_fit_status) << "\",\n";
        object_state_file << "  \"segmentation_geometry_fit_reason\": \"" << JsonEscape(capture.segmentation_geometry_fit_reason) << "\",\n";
        object_state_file << "  \"segmentation_geometry_count\": " << capture.segmentation_geometry_count << ",\n";
        object_state_file << "  \"segmentation_geometry_residual_px\": " << capture.segmentation_geometry_residual_px << ",\n";
        object_state_file << "  \"segmentation_geometry_support\": " << capture.segmentation_geometry_support << ",\n";
        object_state_file << "  \"segmentation_geometry_center_x\": " << capture.segmentation_geometry_center_x << ",\n";
        object_state_file << "  \"segmentation_geometry_center_y\": " << capture.segmentation_geometry_center_y << ",\n";
        object_state_file << "  \"segmentation_geometry_radius\": " << capture.segmentation_geometry_radius << ",\n";
        object_state_file << "  \"segmentation_geometry_axis_x\": " << capture.segmentation_geometry_axis_x << ",\n";
        object_state_file << "  \"segmentation_geometry_axis_y\": " << capture.segmentation_geometry_axis_y << ",\n";
        object_state_file << "  \"segmentation_geometry_angle_deg\": " << capture.segmentation_geometry_angle_deg << ",\n";
        object_state_file << "  \"segmentation_oriented_box_evidence_ref\": \"" << JsonEscape(capture.segmentation_oriented_box_evidence_ref) << "\",\n";
        object_state_file << "  \"segmentation_oriented_box_conclusion\": \"" << JsonEscape(capture.segmentation_oriented_box_conclusion) << "\",\n";
        object_state_file << "  \"segmentation_raw_result_ref\": \"" << JsonEscape(capture.segmentation_raw_result_ref) << "\",\n";
        object_state_file << "  \"segmentation_raw_mask_ref\": \"" << JsonEscape(capture.segmentation_raw_mask_ref) << "\",\n";
        object_state_file << "  \"segmentation_raw_contour_ref\": \"" << JsonEscape(capture.segmentation_raw_contour_ref) << "\",\n";
        object_state_file << "  \"segmentation_raw_overlay_ref\": \"" << JsonEscape(capture.segmentation_raw_overlay_ref) << "\",\n";
        object_state_file << "  \"segmentation_refined_result_ref\": \"" << JsonEscape(capture.segmentation_refined_result_ref) << "\",\n";
        object_state_file << "  \"segmentation_refined_mask_ref\": \"" << JsonEscape(capture.segmentation_refined_mask_ref) << "\",\n";
        object_state_file << "  \"segmentation_refined_contour_ref\": \"" << JsonEscape(capture.segmentation_refined_contour_ref) << "\",\n";
        object_state_file << "  \"segmentation_refined_overlay_ref\": \"" << JsonEscape(capture.segmentation_refined_overlay_ref) << "\",\n";
        object_state_file << "  \"torch_ok\": " << capture.torch_ok << ",\n";
        object_state_file << "  \"torch_error_code\": " << capture.torch_error_code << ",\n";
        object_state_file << "  \"torch_train_ms\": " << capture.torch_train_ms << ",\n";
        object_state_file << "  \"torch_infer_ms\": " << capture.torch_infer_ms << ",\n";
        object_state_file << "  \"torch_total_ms\": " << capture.torch_total_ms << ",\n";
        object_state_file << "  \"torch_result_count\": " << capture.torch_result_count << ",\n";
        object_state_file << "  \"torch_status\": \"" << JsonEscape(capture.torch_status) << "\",\n";
        object_state_file << "  \"torch_evidence_ref\": \"" << JsonEscape(capture.torch_evidence_ref) << "\",\n";
        object_state_file << "  \"torch_primary_visual_ref\": \"" << JsonEscape(capture.torch_primary_visual_ref) << "\",\n";
        object_state_file << "  \"torch_trainer_lifecycle_summary\": \"" << JsonEscape(capture.torch_trainer_lifecycle_summary) << "\",\n";
        object_state_file << "  \"torch_unified_mainline_summary\": \"" << JsonEscape(capture.torch_unified_mainline_summary) << "\",\n";
        object_state_file << "  \"budget_exceeded\": " << (capture.budget_exceeded ? "true" : "false") << "\n";
        object_state_file << "}\n";
        object_state_file.close();
    }

    if (options.runtime_capture_smoke)
    {
        std::filesystem::path smoke_path = output_dir / "runtime_capture_smoke.json";
        std::ofstream smoke_file(smoke_path);
        if (smoke_file.is_open())
        {
            smoke_file << "{\n";
            smoke_file << "  \"pass\": " << (capture.smoke_pass ? "true" : "false") << ",\n";
            smoke_file << "  \"execution_mode\": \"sequential\",\n";
            smoke_file << "  \"algorithm_scope\": \"geometry_capture_only\",\n";
            smoke_file << "  \"findline_object_name\": \"" << capture.smoke_findline_object_name << "\",\n";
            smoke_file << "  \"findline_roi\": " << (capture.smoke_findline_roi ? "true" : "false") << ",\n";
            smoke_file << "  \"findline_scan\": " << (capture.smoke_findline_scan ? "true" : "false") << ",\n";
            smoke_file << "  \"findcircle_object_name\": \"" << capture.smoke_findcircle_object_name << "\",\n";
            smoke_file << "  \"findcircle_roi_shape_kind\": \"" << capture.smoke_findcircle_roi_shape_kind << "\",\n";
            smoke_file << "  \"findcircle_roi_radius\": " << capture.smoke_findcircle_roi_radius << ",\n";
            smoke_file << "  \"findcircle_outer_scan_radius\": " << capture.smoke_findcircle_outer_scan_radius << ",\n";
            smoke_file << "  \"reason\": \"" << capture.reason << "\"\n";
            smoke_file << "}\n";
            smoke_file.close();
        }
    }

    std::ofstream overlay_validation_file(overlay_validation_path);
    if (overlay_validation_file.is_open())
    {
        const bool source_equals_result_overlay =
            capture.result_overlay_changed_pixels == 0;
        const bool fit_geometry_matches_overlay =
            (!capture.has_fit_line && !capture.has_fit_circle) ||
            capture.rendered_result_count > 0;
        const bool point_geometry_matches_overlay =
            capture.valid_points_count <= 0 ||
            capture.rendered_measure_points_count > 0 ||
            (capture.has_result_rect && capture.rendered_result_count > 0);
        const bool summary_geometry_matches_overlay =
            fit_geometry_matches_overlay && point_geometry_matches_overlay;

        overlay_validation_file << "{\n";
        overlay_validation_file << "  \"execution_mode\": \"sequential\",\n";
        overlay_validation_file << "  \"source_equals_result_overlay\": "
                                << (source_equals_result_overlay ? "true" : "false") << ",\n";
        overlay_validation_file << "  \"changed_pixel_count\": " << capture.result_overlay_changed_pixels << ",\n";
        overlay_validation_file << "  \"rendered_roi_count\": " << capture.rendered_roi_count << ",\n";
        overlay_validation_file << "  \"rendered_scan_count\": " << capture.rendered_scan_count << ",\n";
        overlay_validation_file << "  \"rendered_measure_points_count\": " << capture.rendered_measure_points_count << ",\n";
        overlay_validation_file << "  \"rendered_result_count\": " << capture.rendered_result_count << ",\n";
        overlay_validation_file << "  \"summary_geometry_matches_overlay\": "
                                << (summary_geometry_matches_overlay ? "true" : "false") << "\n";
        overlay_validation_file << "}\n";
        overlay_validation_file.close();
    }

    std::ofstream log_file(log_path);
    if (log_file.is_open())
    {
        log_file << "run_start\n";
        log_file << "case_begin: " << options.case_name << "\n";
        log_file << "image: " << options.image_path << "\n";
        log_file << "script: " << options.script_path << "\n";
        log_file << "execution_mode: sequential\n";
        log_file << "elapsed_ms: " << capture.elapsed_ms << "\n";
        log_file << "budget_exceeded: " << (capture.budget_exceeded ? "true" : "false") << "\n";
        log_file << "tool_effective_method: " << capture.tool_method << "\n";
        log_file << "tool_effective_threshold: " << capture.tool_threshold << "\n";
        log_file << "tool_input_line: "
                 << capture.tool_input_line_x0 << ","
                 << capture.tool_input_line_y0 << ","
                 << capture.tool_input_line_x1 << ","
                 << capture.tool_input_line_y1 << ","
                 << capture.tool_input_line_half_width << "\n";
        log_file << "tool_input_circle: "
                 << capture.tool_input_circle_cx << ","
                 << capture.tool_input_circle_cy << ","
                 << capture.tool_input_circle_px << ","
                 << capture.tool_input_circle_py << ","
                 << capture.tool_input_circle_gap << "\n";
        log_file << "scan_rows_examined: " << capture.scan_rows_examined << "\n";
        log_file << "scan_rows_with_foreground: " << capture.scan_rows_with_foreground << "\n";
        log_file << "scan_runs_total: " << capture.scan_runs_total << "\n";
        log_file << "scan_runs_within_length_limit: " << capture.scan_runs_within_length_limit << "\n";
        log_file << "scan_runs_over_length_limit: " << capture.scan_runs_over_length_limit << "\n";
        log_file << "scan_runs_rejected_by_min_edge_width: "
                 << capture.scan_runs_rejected_by_min_edge_width << "\n";
        log_file << "scan_runs_rejected_by_selection: " << capture.scan_runs_rejected_by_selection << "\n";
        log_file << "scan_runs_rejected_near_endpoint: " << capture.scan_runs_rejected_near_endpoint << "\n";
        log_file << "scan_points_emitted: " << capture.scan_points_emitted << "\n";
        log_file << "findline_point_consistency_enabled: " << capture.findline_point_consistency_enabled << "\n";
        log_file << "findline_point_consistency_range: " << capture.findline_point_consistency_range << "\n";
        log_file << "findline_point_consistency_input_points: " << capture.findline_point_consistency_input_points << "\n";
        log_file << "findline_point_consistency_output_points: " << capture.findline_point_consistency_output_points << "\n";
        log_file << "findline_point_consistency_removed_points: " << capture.findline_point_consistency_removed_points << "\n";
        log_file << "findcircle_point_consistency_enabled: " << capture.circle_point_consistency_enabled << "\n";
        log_file << "findcircle_point_consistency_range: " << capture.circle_point_consistency_range << "\n";
        log_file << "findcircle_point_consistency_input_points: " << capture.circle_point_consistency_input_points << "\n";
        log_file << "findcircle_point_consistency_output_points: " << capture.circle_point_consistency_output_points << "\n";
        log_file << "findcircle_point_consistency_removed_points: " << capture.circle_point_consistency_removed_points << "\n";
        log_file << "findobject_foreground_before: " << capture.object_foreground_before << "\n";
        log_file << "findobject_foreground_after: " << capture.object_foreground_after << "\n";
        log_file << "actual_findsetting: " << capture.actual_findsetting << "\n";
        log_file << "object_prefilter_requested: " << (capture.object_prefilter_requested ? "true" : "false") << "\n";
        log_file << "object_prefilter_applied: " << (capture.object_prefilter_applied ? "true" : "false") << "\n";
        log_file << "valid_points_count: " << capture.valid_points_count << "\n";
        log_file << "has_fit_line: " << (capture.has_fit_line ? "true" : "false") << "\n";
        log_file << "has_fit_circle: " << (capture.has_fit_circle ? "true" : "false") << "\n";
        log_file << "has_fit_ellipse: " << (capture.has_fit_ellipse ? "true" : "false") << "\n";
        log_file << "has_result_rect: " << (capture.has_result_rect ? "true" : "false") << "\n";
        log_file << "model_point_count: " << capture.model_point_count << "\n";
        log_file << "fastmatch_learn_a_count: " << capture.fastmatch_learn_a_count << "\n";
        log_file << "fastmatch_learn_b_count: " << capture.fastmatch_learn_b_count << "\n";
        log_file << "fastmatch_learn_a2_count: " << capture.fastmatch_learn_a2_count << "\n";
        log_file << "fastmatch_learn_b2_count: " << capture.fastmatch_learn_b2_count << "\n";
        log_file << "fastmatch_learn_status_code: " << capture.fastmatch_learn_status_code << "\n";
        log_file << "fastmatch_learn_rect: "
                 << capture.fastmatch_learn_rect_x0 << ","
                 << capture.fastmatch_learn_rect_y0 << ","
                 << capture.fastmatch_learn_rect_x1 << ","
                 << capture.fastmatch_learn_rect_y1 << "\n";
        log_file << "fastmatch_match_rect: "
                 << capture.fastmatch_match_rect_x0 << ","
                 << capture.fastmatch_match_rect_y0 << ","
                 << capture.fastmatch_match_rect_x1 << ","
                 << capture.fastmatch_match_rect_y1 << "\n";
        log_file << "candidate_count: " << capture.candidate_count << "\n";
        log_file << "best_score: " << capture.best_score << "\n";
        log_file << "torch_ok: " << capture.torch_ok << "\n";
        log_file << "torch_status: " << capture.torch_status << "\n";
        log_file << "torch_train_ms: " << capture.torch_train_ms << "\n";
        log_file << "torch_infer_ms: " << capture.torch_infer_ms << "\n";
        log_file << "torch_total_ms: " << capture.torch_total_ms << "\n";
        log_file << "torch_trainer_lifecycle_summary: " << capture.torch_trainer_lifecycle_summary << "\n";
        log_file << "torch_unified_mainline_summary: " << capture.torch_unified_mainline_summary << "\n";
        log_file << "case_end\n";
        log_file << "run_end\n";
        log_file.close();
    }

    std::string manual_review_handoff_reason;
    const bool manual_review_handoff_ok = WriteHeadlessManualReviewHandoff(
        options,
        capture,
        result,
        output_dir,
        manual_review_handoff_reason);
    if (!manual_review_handoff_ok && result.reason.empty())
        result.reason = manual_review_handoff_reason;

    bool snapshot_ok = !result.snapshot_path.empty();
    bool summary_ok = !result.summary_path.empty();
    const bool torch_task_ok = capture.torch_ok != 0;
    const bool segmentation_result_ok =
        capture.segmentation_status_code != 0 ||
        capture.segmentation_contour_count > 0 ||
        capture.rendered_result_count > 0;
    bool evidence_ok = options.contract_context_enabled ||
        (!result.evidence_overlay_path.empty() &&
            (capture.rendered_roi_count > 0 ||
             torch_task_ok ||
             segmentation_result_ok));
    bool result_ok = !result.result_overlay_path.empty();
    bool tool_display_ok = !result.tool_display_path.empty();
    bool manual_review_handoff_asset_ok =
        options.contract_context_enabled || manual_review_handoff_ok;

    result.assets_complete = options.contract_context_enabled
        ? (snapshot_ok && summary_ok)
        : (snapshot_ok && summary_ok && evidence_ok && result_ok &&
           tool_display_ok && manual_review_handoff_asset_ok);
    result.ok = result.executed && result.runtime_ok && result.assets_complete;
    result.exit_code = result.ok ? 0 : 1;

    result.valid_points_count = capture.valid_points_count;
    result.has_fit_line = capture.has_fit_line;
    result.has_fit_circle = capture.has_fit_circle;
    result.has_fit_ellipse = capture.has_fit_ellipse;
    result.has_result_rect = capture.has_result_rect;
    result.model_point_count = capture.model_point_count;
    result.fastmatch_learn_a_count = capture.fastmatch_learn_a_count;
    result.fastmatch_learn_b_count = capture.fastmatch_learn_b_count;
    result.fastmatch_learn_a2_count = capture.fastmatch_learn_a2_count;
    result.fastmatch_learn_b2_count = capture.fastmatch_learn_b2_count;
    result.fastmatch_learn_status_code = capture.fastmatch_learn_status_code;
    result.candidate_count = capture.candidate_count;
    result.best_score = capture.best_score;
    result.has_result_box = capture.has_result_box;
    result.has_best_result = capture.has_best_result;
    result.circle_radius = capture.circle_radius;
    result.avgdist = capture.avgdist;

    if (options.save_evidence_candidate)
    {
        ManualTestContext candidateContext;
        candidateContext.active_case_id =
            options.case_name.empty() ? options.case_id : options.case_name;
        candidateContext.active_image_id =
            options.image_id.empty() ? options.stage25_image_id : options.image_id;
        candidateContext.active_target_id =
            options.target_id.empty() ? options.stage25_target_id : options.target_id;
        candidateContext.image_file_path = options.image_path;
        candidateContext.loaded_script_path = options.script_path;
        candidateContext.script_file_path = options.script_path;
        candidateContext.editor_source = "headless";
        ReadTextFile(options.script_path, candidateContext.editor_text);
        candidateContext.debug_status =
            result.ok ? "HEADLESS_EXECUTION_PASS" : "HEADLESS_EXECUTION_PARTIAL";
        candidateContext.debug_reason = result.reason;
        candidateContext.current_result_ref.status =
            result.ok ? "runtime_result_available" : "runtime_result_incomplete";
        candidateContext.current_result_ref.reason = result.reason;

        auto setGlobal = [&](const std::string& name, int value)
        {
            candidateContext.runtime_int_vars[name] = value;
        };
        setGlobal("global_roi_x0", options.roi_x0);
        setGlobal("global_roi_y0", options.roi_y0);
        setGlobal("global_roi_x1", options.roi_x1);
        setGlobal("global_roi_y1", options.roi_y1);
        setGlobal("global_circle_cx", options.circle_cx);
        setGlobal("global_circle_cy", options.circle_cy);
        setGlobal("global_circle_px", options.circle_px);
        setGlobal("global_circle_py", options.circle_py);
        setGlobal("global_circle_inner_radius", 0);
        setGlobal("global_circle_outer_radius", 0);
        setGlobal("global_circle_ring_width", 0);
        setGlobal("global_findcircle_arc_enabled", options.findcircle_arc_enabled);
        setGlobal("global_findcircle_arc_start_deg", options.findcircle_arc_start_deg);
        setGlobal("global_findcircle_arc_end_deg", options.findcircle_arc_end_deg);
        setGlobal("global_ellipse_x0", options.ellipse_x0);
        setGlobal("global_ellipse_y0", options.ellipse_y0);
        setGlobal("global_ellipse_x1", options.ellipse_x1);
        setGlobal("global_ellipse_y1", options.ellipse_y1);
        setGlobal("global_findellipse_inner_scale_percent", options.ellipse_inner_scale_percent);
        setGlobal("global_tool_half_width", options.tool_half_width);
        setGlobal("global_wgap", options.wgap);
        setGlobal("global_hgap", options.hgap);
        setGlobal("global_gap", options.gap);
        setGlobal("global_linegap", options.linegap);
        setGlobal("global_min_edge_run_width_px",
                  options.min_edge_run_width_px);
        setGlobal("global_threshold", options.threshold);
        setGlobal("global_method", options.method);
        setGlobal("global_filterprofile", options.filterprofile);
        setGlobal("global_find_num", options.find_num);
        setGlobal("global_compare_gap", options.compare_gap);
        // FastMatch has one physical parameter bucket per side.  Keep scalar
        // detection settings shared, but seed exterior-edge polarity and
        // sampling windows in opposite pairs instead of flattening all four
        // native FindLine probes to one method.
        setGlobal("global_fastmatch_learn_shared", 0);
        const int fastMatchBaseMethod =
            std::max(0, std::min(3, options.method));
        for (int dir = 0; dir < 4; ++dir)
        {
            const std::string suffix = "_" + std::to_string(dir);
            const bool horizontalEdge = dir < 2;
            const bool oppositeSide = dir == 1 || dir == 3;
            setGlobal("global_fastmatch_learn_wgap" + suffix,
                      horizontalEdge ? options.wgap : options.hgap);
            setGlobal("global_fastmatch_learn_hgap" + suffix,
                      horizontalEdge ? options.hgap : options.wgap);
            setGlobal("global_fastmatch_learn_method" + suffix,
                      oppositeSide ? (fastMatchBaseMethod ^ 1)
                                   : fastMatchBaseMethod);
            setGlobal("global_fastmatch_learn_threshold" + suffix, options.threshold);
            setGlobal("global_fastmatch_learn_linegap" + suffix, options.linegap);
            setGlobal("global_fastmatch_learn_objfilter" + suffix, 1);
            setGlobal("global_fastmatch_learn_compare_gap" + suffix, options.compare_gap);
            setGlobal("global_fastmatch_learn_edge_count" + suffix, 2);
            setGlobal("global_fastmatch_learn_selected_edge" + suffix, 0);
        }
        setGlobal("global_max_elapsed_ms", options.max_elapsed_ms);
        setGlobal("global_max_scan_lines", options.max_scan_lines);
        setGlobal("global_max_samples", options.max_samples);

        std::string candidateGlobalsReason;
        if (!ApplyHeadlessCandidateGlobalOverrides(
                options, candidateContext, candidateGlobalsReason))
        {
            candidateContext.debug_reason +=
                "\ncandidate globals restore failed: " + candidateGlobalsReason;
        }

        std::string lowerScript = options.script_path;
        std::transform(lowerScript.begin(), lowerScript.end(), lowerScript.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    const std::string tool = !options.stage25_tool.empty()
        ? options.stage25_tool
        : (lowerScript.find("cxtextinspect") != std::string::npos ? "CxTextInspect" :
           lowerScript.find("fastmatch") != std::string::npos ? "FastMatch" :
           lowerScript.find("circle") != std::string::npos ? "FindCircle" :
           lowerScript.find("ellipse") != std::string::npos ? "FindEllipse" :
           lowerScript.find("rect") != std::string::npos ? "FindRect" :
           "FindLine");
        PopulateHeadlessCandidateGaugeFromGlobals(
            options, candidateContext, tool);

        CxEvidenceCandidateSaveOptions candidateOptions;
        candidateOptions.root_dir = options.evidence_candidate_root.empty()
            ? "cxscript_runs/evidence_candidates"
            : options.evidence_candidate_root;
        candidateOptions.candidate_id = options.evidence_candidate_id;
        candidateOptions.mode = "headless_result";
        candidateOptions.request_run = false;
        candidateOptions.add_to_evidence_chain = false;
        candidateOptions.linked_result_summary_path = result.summary_path;
        candidateOptions.linked_result_overlay_path = result.result_overlay_path;
        candidateOptions.linked_evidence_overlay_path = result.evidence_overlay_path;
        candidateOptions.linked_tool_display_path = result.tool_display_path;

        CxEvidenceCandidateSaveResult candidateResult;
        if (!SaveEvidenceCandidatePackage(
                candidateContext,
                candidateOptions,
                candidateResult) &&
            result.reason.empty())
        {
            result.reason = candidateResult.reason;
        }
    }

    return result.ok;
}

bool RunCxScriptHeadlessCapture(
    const CxScriptHeadlessOptions& options,
    CxScriptExecutionCapture& capture,
    std::string& reason)
{
    capture = CxScriptExecutionCapture{};

    std::filesystem::path script_path(options.script_path);
    if (!std::filesystem::exists(script_path))
    {
        reason = "script not found: " + script_path.string();
        capture.failure_stage = "script";
        return false;
    }

    std::filesystem::path image_path(options.image_path);
    if (!std::filesystem::exists(image_path))
    {
        reason = "image not found: " + image_path.string();
        capture.failure_stage = "image";
        return false;
    }

    cv::Mat source_image = cv::imread(image_path.string(), cv::IMREAD_COLOR);
    if (source_image.empty())
    {
        reason = "cannot read image: " + image_path.string();
        capture.failure_stage = "image";
        return false;
    }

    if (!options.template_image_path.empty())
    {
        std::filesystem::path template_path(options.template_image_path);
        if (!std::filesystem::exists(template_path))
        {
            reason = "template image not found: " + template_path.string();
            capture.failure_stage = "template_image";
            return false;
        }
    }

    const int timeout_ms = std::max(1, options.timeout_sec) * 1000;
    capture.budget_ms = options.max_elapsed_ms > 0
        ? std::min(options.max_elapsed_ms, timeout_ms)
        : timeout_ms;
    capture.max_steps = options.max_steps;
    capture.max_scan_lines = options.max_scan_lines;
    capture.max_samples = options.max_samples;

    CxScriptHeadlessOptions effective_options = options;
    effective_options.max_elapsed_ms = capture.budget_ms;
    const bool execution_ok = ExecuteCxScriptSequential(
        effective_options,
        source_image,
        capture,
        reason);

    if (!execution_ok && capture.failure_stage.empty())
        capture.failure_stage = "script_execution";

    return execution_ok;
}

int RunCxScriptHeadless(int argc, char* argv[])
{
    CxScriptHeadlessOptions options;
    if (!ParseCxScriptHeadlessArgs(argc, argv, options))
        return -1;
    CxScriptHeadlessResult result;
    if (!RunCxScriptHeadless(options, result))
        return -1;
    return result.exit_code;
}
