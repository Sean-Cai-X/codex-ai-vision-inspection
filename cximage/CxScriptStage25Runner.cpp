#include "CxScriptStage25Runner.h"
#include "CxScriptStage25Template.h"
#include "CxScriptImagePreflight.h"
#include "CxScriptStage25ReportWriter.h"
#include "CxScriptStage25Register.h"
#include "CxScriptStage25JsonLite.h"
#include "CxScriptStage25PolicyValidator.h"
#include "CxScriptStage25CaseMatrix.h"
#include "ManualStateTestConsole.h"
#include "muParser.h"
#include <fstream>
#include <sstream>

namespace
{
    std::string GenerateCaseId(
        const std::string& image_id,
        const std::string& target_id,
        const std::string& profile_id,
        const std::string& evidence_profile)
    {
        return image_id + "__" + target_id + "__" + profile_id + "__" + evidence_profile;
    }

    Stage25TemplateContext BuildFindlineContext(
        const std::string& case_id,
        const std::string& image_id,
        const std::string& target_id,
        const Stage25ImageTarget& target,
        const Stage25FindlineProfile& profile)
    {
        Stage25TemplateContext ctx;
        ctx.values["case_id"] = case_id;
        ctx.values["image_id"] = image_id;
        ctx.values["target_id"] = target_id;
        ctx.values["profile_id"] = profile.profile_id;
        ctx.values["parameter_policy_id"] = profile.parameter_policy_id;
        ctx.values["parameter_role"] = profile.parameter_role;
        ctx.values["is_product_default"] = profile.is_product_default ? "true" : "false";
        ctx.values["x0"] = std::to_string(target.x0);
        ctx.values["y0"] = std::to_string(target.y0);
        ctx.values["x1"] = std::to_string(target.x1);
        ctx.values["y1"] = std::to_string(target.y1);
        ctx.values["wgap"] = std::to_string(target.wgap);
        ctx.values["hgap"] = std::to_string(target.hgap);
        ctx.values["method"] = std::to_string(profile.method);
        ctx.values["threshold"] = std::to_string(profile.threshold);
        ctx.values["linegap"] = std::to_string(profile.linegap);
        ctx.values["fitmode"] = std::to_string(profile.fitmode);
        // Findline::setline(..., fifth_arg) uses the tool scan half-width in
        // the current CxScript binding, not an image scale factor. Older
        // Stage25 manifests used findline_setscript_scale(1), which collapsed
        // the generated ROI to a 2px-wide scan box and made otherwise valid
        // line cases produce zero measure points. Keep the manifest field for
        // compatibility, but treat <=1 as "use target hgap as the explicit
        // tool half-width".
        const int tool_half_width =
            profile.script_scale > 1 ? profile.script_scale : target.hgap;
        ctx.values["tool_half_width"] = std::to_string(tool_half_width);
        ctx.values["script_scale"] = std::to_string(profile.script_scale);
        ctx.values["filter_profile"] = std::to_string(profile.filter_profile);

        if (profile.has_explicit_filter)
        {
            ctx.values["optional_findline_filter"] =
                "m_line.setobjfilter(" + std::to_string(profile.objfilter) + ");\n"
                "m_line.setfilter(" + std::to_string(profile.filter_borw) + ", " +
                std::to_string(profile.filter_min) + ", " +
                std::to_string(profile.filter_max) + ");";
        }
        else
        {
            ctx.values["optional_findline_filter"] = "";
        }

        if (profile.has_gamma)
        {
            ctx.values["optional_findline_gamma"] =
                "m_line.setgamarate(" + std::to_string(profile.gamma) + ");";
        }
        else
        {
            ctx.values["optional_findline_gamma"] = "";
        }

        return ctx;
    }

    Stage25TemplateContext BuildFindcircleContext(
        const std::string& case_id,
        const std::string& image_id,
        const std::string& target_id,
        const Stage25ImageTarget& target,
        const Stage25FindcircleProfile& profile)
    {
        Stage25TemplateContext ctx;
        ctx.values["case_id"] = case_id;
        ctx.values["image_id"] = image_id;
        ctx.values["target_id"] = target_id;
        ctx.values["profile_id"] = profile.profile_id;
        ctx.values["cx"] = std::to_string(target.cx);
        ctx.values["cy"] = std::to_string(target.cy);
        ctx.values["px"] = std::to_string(target.px);
        ctx.values["py"] = std::to_string(target.py);
        ctx.values["gap"] = std::to_string(target.gap);
        ctx.values["linegap"] = std::to_string(target.linegap);
        ctx.values["method"] = std::to_string(profile.method);
        ctx.values["threshold"] = std::to_string(profile.threshold);

        if (profile.has_filter)
        {
            ctx.values["optional_findcircle_filter"] =
                "m_circle.setfindsetting(" + std::to_string(profile.findsetting) + ");\n"
                "m_circle.setfilter(" + std::to_string(profile.filter_borw) + ", " +
                std::to_string(profile.filter_min) + ", " +
                std::to_string(profile.filter_max) + ");";
        }
        else
        {
            ctx.values["optional_findcircle_filter"] = "";
        }

        if (profile.has_samplerate)
        {
            ctx.values["optional_findcircle_samplerate"] =
                "m_circle.setlinesamplerate(" + std::to_string(profile.samplerate) + ");";
        }
        else
        {
            ctx.values["optional_findcircle_samplerate"] = "";
        }

        return ctx;
    }

    static bool LoadStage25CaseResultFromFiles(
        const std::filesystem::path& summaryPath,
        const std::filesystem::path& evidenceSummaryPath,
        Stage25CaseResult& out)
    {
        Stage25JsonLite summary;
        Stage25JsonLite evidence;

        const bool hasSummary = summary.LoadFile(summaryPath);
        const bool hasEvidence = evidence.LoadFile(evidenceSummaryPath);

        if (!hasSummary)
        {
            out.failure_stage = "summary_missing";
            out.failure_reason = "result_summary.json not found or unreadable";
            return false;
        }

        out.summary_path = summaryPath.string();

        out.measure_source = summary.GetString("measure_source");
        out.fallback_used = summary.GetBool("fallback_used", false);

        std::string currentResultReason = summary.GetString("current_result_ref.reason", "");
        if (!currentResultReason.empty())
        {
            size_t pos = currentResultReason.find("failure_stage=");
            if (pos != std::string::npos)
            {
                size_t end = currentResultReason.find(",", pos);
                out.failure_stage = currentResultReason.substr(pos + 14, end - pos - 14);
            }
        }

        out.failure_reason = summary.GetString("debug_reason", "");
        out.run_state = summary.GetString("run_state", "");

        out.points_count = summary.GetInt("current_result_ref.points_count", 0);
        out.valid_points_count = summary.GetInt("current_result_ref.valid_points_count", 0);
        out.has_fit_line = summary.GetBool("current_result_ref.has_fit_line", false);
        out.has_fit_circle = summary.GetBool("current_result_ref.has_fit_circle", false);
        out.has_fit = out.has_fit_line || out.has_fit_circle;

        out.line_avgdist = summary.GetDouble("line_avgdist", 0.0);
        out.circle_avgdist = summary.GetDouble("circle_avgdist", 0.0);

        out.snapshot_path = summary.GetString("snapshot_path", "");
        out.overlay_path = summary.GetString("overlay_path", "");

        if (hasEvidence)
        {
            out.evidence_summary_path = evidenceSummaryPath.string();

            out.measured_local_support_score =
                evidence.GetDouble("measured_local_support_score", 0.0);
            out.measured_local_mean_distance_px =
                evidence.GetDouble("measured_local_mean_distance_px", 0.0);
            out.global_reference_mean_distance_px =
                evidence.GetDouble("global_reference_mean_distance_px", 0.0);
            out.fit_offset_error_px =
                evidence.GetDouble("fit_offset_error_px", 0.0);

            out.circle_local_support_score =
                evidence.GetDouble("circle_local_support_score", 0.0);
            out.circle_local_mean_radial_distance_px =
                evidence.GetDouble("circle_local_mean_radial_distance_px", 0.0);
            out.circle_global_reference_mean_distance_px =
                evidence.GetDouble("circle_global_reference_mean_distance_px", 0.0);
            out.circle_center_error_px =
                evidence.GetDouble("circle_center_error_px", 0.0);
        }
        else
        {
            out.evidence_summary_path = "EVIDENCE_MISSING";
        }

        return true;
    }

    static bool ComputeT1Pass(const Stage25CaseResult& r)
    {
        if (!r.t0_pass)
            return false;

        if (r.fallback_used)
            return false;

        if (r.tool == "FindLine")
        {
            return r.valid_points_count >= 2 && r.has_fit_line;
        }

        if (r.tool == "Findcircle")
        {
            return r.valid_points_count >= 3 && r.has_fit_circle;
        }

        return false;
    }

    static bool ComputeT2Pass(const Stage25CaseResult& r)
    {
        if (!r.t1_pass)
            return false;

        if (r.evidence_summary_path == "EVIDENCE_MISSING")
            return false;

        if (r.tool == "FindLine")
        {
            return r.measured_local_support_score >= 0.60 &&
                   r.measured_local_mean_distance_px <= 3.0;
        }

        if (r.tool == "Findcircle")
        {
            return r.circle_local_support_score >= 0.60 &&
                   r.circle_local_mean_radial_distance_px <= 3.0;
        }

        return false;
    }

    static std::string ClassifyQuality(const Stage25CaseResult& r)
    {
        if (r.skipped_by_preflight)
            return "SKIPPED_BY_PREFLIGHT";

        if (!r.t0_pass)
            return "HEADLESS_FAILED";

        const double localSupport =
            r.tool == "FindLine"
                ? r.measured_local_support_score
                : r.circle_local_support_score;

        const double localDist =
            r.tool == "FindLine"
                ? r.measured_local_mean_distance_px
                : r.circle_local_mean_radial_distance_px;

        if (!r.t1_pass)
        {
            if (localSupport >= 0.60)
                return "ALGORITHM_NO_RESULT_WITH_IMAGE_EDGE";

            return "ALGORITHM_NO_RESULT";
        }

        if (r.evidence_summary_path == "EVIDENCE_MISSING")
            return "EVIDENCE_MISSING";

        if (r.t2_pass)
            return "ORIGINAL_LOCAL_EDGE_CONFIRMED";

        if (localSupport >= 0.60)
            return "GEOMETRY_MARGINAL_BUT_SAMPLED";

        return "SUSPICIOUS_CANDIDATE";
    }
}

bool RunStage25ManifestFile(
    const Stage25RunOptions& options,
    Stage25RunResult& result)
{
    std::filesystem::create_directories(options.out_root);

    auto manifest_dir = options.manifest_path.parent_path();
    auto stage25_dir = manifest_dir.parent_path();
    auto templates_dir = stage25_dir / "templates";

    std::ifstream manifestFile(options.manifest_path);
    if (!manifestFile.is_open())
    {
        result.reason = "Cannot open manifest: " + options.manifest_path.string();
        return false;
    }

    std::stringstream buffer;
    buffer << manifestFile.rdbuf();
    std::string manifest_script = buffer.str();

    g_stage25_manifest = Stage25Manifest{};
    g_current_image = nullptr;
    g_current_findline_profile = nullptr;
    g_current_findcircle_profile = nullptr;
    g_current_evidence_profile = nullptr;

    mu::Parser parser;
    parser.UsingClass(true);

    RegisterStage25CxScriptBindings(parser);

    try
    {
        parser.SetExpr(manifest_script);
        parser.Eval();
    }
    catch (const mu::Parser::exception_type& e)
    {
        result.reason = "Manifest parse error: " + std::string(e.GetMsg());
        return false;
    }

    Stage25Manifest& manifest = g_stage25_manifest;
    if (manifest.outroot.empty())
    {
        manifest.outroot = options.out_root.string();
    }

    auto policy_validation = ValidateStage25ParameterPolicies(manifest);
    if (!policy_validation.ok)
    {
        result.ok = false;
        result.reason = "Stage25 parameter policy validation failed";
        Stage25ReportWriter::WritePolicyValidationReport(
            options.out_root,
            policy_validation);
        return false;
    }
    Stage25ReportWriter::WritePolicyValidationReport(
        options.out_root,
        policy_validation);

    std::vector<Stage25ImagePreflightResult> preflight_results;

    for (const auto& img : manifest.images)
    {
        for (const auto& target : img.targets)
        {
            auto preflight = Stage25ImagePreflight::Run(
                img.image_id,
                target.target_id,
                target.tool,
                img.level,
                img.path,
                target.tool == "FindLine" ? target.x0 : target.cx,
                target.tool == "FindLine" ? target.y0 : target.cy,
                target.tool == "FindLine" ? target.x1 : target.px,
                target.tool == "FindLine" ? target.y1 : target.py,
                target.wgap, target.hgap,
                target.gap, target.linegap);

            preflight_results.push_back(preflight);

            if (!preflight.roi_valid)
            {
                for (const auto& ev_profile : manifest.evidence_profiles)
                {
                    if (target.tool == "FindLine")
                    {
                        for (const auto& profile : manifest.findline_profiles)
                        {
                            Stage25CaseResult skipped;
                            skipped.case_id = GenerateCaseId(
                                img.image_id, target.target_id, profile.profile_id, ev_profile.name);
                            skipped.image_id = img.image_id;
                            skipped.level = img.level;
                            skipped.target_id = target.target_id;
                            skipped.tool = target.tool;
                            skipped.profile_id = profile.profile_id;
                            skipped.evidence_profile = ev_profile.name;
                            skipped.policy_classification = profile.policy;
                            skipped.parameter_policy_id = profile.parameter_policy_id;
                            skipped.parameter_role = profile.parameter_role;
                            skipped.is_product_default = profile.is_product_default;
                            skipped.is_stage25_default = profile.is_stage25_default;
                            skipped.skipped_by_preflight = true;
                            skipped.skip_reason = preflight.preflight_class;
                            skipped.t0_pass = false;
                            skipped.t1_pass = false;
                            skipped.t2_pass = false;
                            skipped.quality_classification = "SKIPPED_BY_PREFLIGHT";
                            result.case_results.push_back(skipped);
                        }
                    }
                    else if (target.tool == "Findcircle")
                    {
                        for (const auto& profile : manifest.findcircle_profiles)
                        {
                            Stage25CaseResult skipped;
                            skipped.case_id = GenerateCaseId(
                                img.image_id, target.target_id, profile.profile_id, ev_profile.name);
                            skipped.image_id = img.image_id;
                            skipped.level = img.level;
                            skipped.target_id = target.target_id;
                            skipped.tool = target.tool;
                            skipped.profile_id = profile.profile_id;
                            skipped.evidence_profile = ev_profile.name;
                            skipped.policy_classification = profile.policy;
                            skipped.skipped_by_preflight = true;
                            skipped.skip_reason = preflight.preflight_class;
                            skipped.t0_pass = false;
                            skipped.t1_pass = false;
                            skipped.t2_pass = false;
                            skipped.quality_classification = "SKIPPED_BY_PREFLIGHT";
                            result.case_results.push_back(skipped);
                        }
                    }
                }
                continue;
            }

            for (const auto& ev_profile : manifest.evidence_profiles)
            {
                if (target.tool == "FindLine")
                {
                    for (const auto& profile : manifest.findline_profiles)
                    {
                        std::string case_id = GenerateCaseId(
                            img.image_id, target.target_id, profile.profile_id, ev_profile.name);

                        std::filesystem::path generated_script = options.out_root /
                            "generated_scripts" / img.image_id / target.target_id /
                            (profile.profile_id + ".cxsc");

                        Stage25TemplateContext ctx = BuildFindlineContext(case_id, img.image_id, target.target_id, target, profile);
                        Stage25TemplateEngine engine;
                        std::string reason;
                        engine.RenderFile(
                            templates_dir / "find_line_stage25_template.cxsc",
                            ctx, generated_script, reason);

                        std::filesystem::path case_dir = options.out_root /
                            img.level / img.image_id / target.target_id /
                            profile.profile_id / ev_profile.name;

                        CxScriptHeadlessOptions headlessOptions;
                        headlessOptions.enabled = true;
                        headlessOptions.image_path = img.path;
                        headlessOptions.script_path = generated_script.string();
                        headlessOptions.output_dir = case_dir.string();
                        headlessOptions.case_name = case_id;

                        CxScriptHeadlessResult headlessResult;
                        RunCxScriptHeadless(headlessOptions, headlessResult);

                        Stage25CaseResult case_result;
                        case_result.case_id = case_id;
                        case_result.image_id = img.image_id;
                        case_result.level = img.level;
                        case_result.target_id = target.target_id;
                        case_result.tool = target.tool;
                        case_result.orientation = target.orientation;
                        case_result.profile_id = profile.profile_id;
                        case_result.evidence_profile = ev_profile.name;
                        case_result.policy_classification = profile.policy;
                        case_result.parameter_policy_id = profile.parameter_policy_id;
                        case_result.parameter_role = profile.parameter_role;
                        case_result.is_product_default = profile.is_product_default;
                        case_result.is_stage25_default = profile.is_stage25_default;
                        case_result.generated_script_path = generated_script.string();

                        case_result.headless_ok = headlessResult.ok;
                        case_result.t0_pass = headlessResult.ok;

                        case_result.generated_script_exists =
                            std::filesystem::exists(generated_script);

                        if (headlessResult.ok)
                        {
                            std::filesystem::path summaryPath = case_dir / "result_summary.json";
                            std::filesystem::path evidencePath = case_dir / "evidence_summary.json";

                            LoadStage25CaseResultFromFiles(summaryPath, evidencePath, case_result);

                            case_result.snapshot_path = headlessResult.snapshot_path;
                            case_result.overlay_path = headlessResult.overlay_path;
                            case_result.snapshot_exists =
                                std::filesystem::exists(std::filesystem::path(headlessResult.snapshot_path));
                            case_result.overlay_exists =
                                std::filesystem::exists(std::filesystem::path(headlessResult.overlay_path));
                            case_result.summary_exists =
                                std::filesystem::exists(summaryPath);
                            case_result.evidence_summary_exists =
                                std::filesystem::exists(evidencePath);
                        }

                        case_result.t1_pass = ComputeT1Pass(case_result);
                        case_result.t2_pass = ComputeT2Pass(case_result);
                        case_result.quality_classification = ClassifyQuality(case_result);

                        result.case_results.push_back(case_result);
                    }
                }
                else if (target.tool == "Findcircle")
                {
                    for (const auto& profile : manifest.findcircle_profiles)
                    {
                        std::string case_id = GenerateCaseId(
                            img.image_id, target.target_id, profile.profile_id, ev_profile.name);

                        std::filesystem::path generated_script = options.out_root /
                            "generated_scripts" / img.image_id / target.target_id /
                            (profile.profile_id + ".cxsc");

                        Stage25TemplateContext ctx = BuildFindcircleContext(case_id, img.image_id, target.target_id, target, profile);
                        Stage25TemplateEngine engine;
                        std::string reason;
                        engine.RenderFile(
                            templates_dir / "find_circle_stage25_template.cxsc",
                            ctx, generated_script, reason);

                        std::filesystem::path case_dir = options.out_root /
                            img.level / img.image_id / target.target_id /
                            profile.profile_id / ev_profile.name;

                        CxScriptHeadlessOptions headlessOptions;
                        headlessOptions.enabled = true;
                        headlessOptions.image_path = img.path;
                        headlessOptions.script_path = generated_script.string();
                        headlessOptions.output_dir = case_dir.string();
                        headlessOptions.case_name = case_id;

                        CxScriptHeadlessResult headlessResult;
                        RunCxScriptHeadless(headlessOptions, headlessResult);

                        Stage25CaseResult case_result;
                        case_result.case_id = case_id;
                        case_result.image_id = img.image_id;
                        case_result.level = img.level;
                        case_result.target_id = target.target_id;
                        case_result.tool = target.tool;
                        case_result.orientation = target.orientation;
                        case_result.profile_id = profile.profile_id;
                        case_result.evidence_profile = ev_profile.name;
                        case_result.policy_classification = profile.policy;
                        case_result.parameter_policy_id = profile.parameter_policy_id;
                        case_result.parameter_role = profile.parameter_role;
                        case_result.is_product_default = profile.is_product_default;
                        case_result.is_stage25_default = profile.is_stage25_default;
                        case_result.generated_script_path = generated_script.string();

                        case_result.headless_ok = headlessResult.ok;
                        case_result.t0_pass = headlessResult.ok;

                        case_result.generated_script_exists =
                            std::filesystem::exists(generated_script);

                        if (headlessResult.ok)
                        {
                            std::filesystem::path summaryPath = case_dir / "result_summary.json";
                            std::filesystem::path evidencePath = case_dir / "evidence_summary.json";

                            LoadStage25CaseResultFromFiles(summaryPath, evidencePath, case_result);

                            case_result.snapshot_path = headlessResult.snapshot_path;
                            case_result.overlay_path = headlessResult.overlay_path;
                            case_result.snapshot_exists =
                                std::filesystem::exists(std::filesystem::path(headlessResult.snapshot_path));
                            case_result.overlay_exists =
                                std::filesystem::exists(std::filesystem::path(headlessResult.overlay_path));
                            case_result.summary_exists =
                                std::filesystem::exists(summaryPath);
                            case_result.evidence_summary_exists =
                                std::filesystem::exists(evidencePath);
                        }

                        case_result.t1_pass = ComputeT1Pass(case_result);
                        case_result.t2_pass = ComputeT2Pass(case_result);
                        case_result.quality_classification = ClassifyQuality(case_result);

                        result.case_results.push_back(case_result);
                    }
                }
            }
        }
    }

    result.total_cases = static_cast<int>(result.case_results.size());
    result.t0_pass = static_cast<int>(std::count_if(result.case_results.begin(),
        result.case_results.end(), [](const auto& r) { return r.t0_pass; }));
    result.t1_pass = static_cast<int>(std::count_if(result.case_results.begin(),
        result.case_results.end(), [](const auto& r) { return r.t1_pass; }));
    result.t2_pass = static_cast<int>(std::count_if(result.case_results.begin(),
        result.case_results.end(), [](const auto& r) { return r.t2_pass; }));

    auto case_matrix = BuildStage25CaseMatrix(manifest);
    Stage25ReportWriter::WriteCaseMatrixReport(options.out_root, case_matrix);

    Stage25ReportWriter::WriteBatchReport(options.out_root, result.case_results);
    Stage25ReportWriter::WritePreflightReport(options.out_root, preflight_results);
    Stage25ReportWriter::WriteCoverageReport(options.out_root, manifest);
    Stage25ReportWriter::WriteStabilityReport(options.out_root, result.case_results, manifest);
    Stage25ReportWriter::WritePolicyReport(options.out_root, result.case_results, manifest);
    Stage25ReportWriter::WriteFastMatchReadinessReport(options.out_root, result.case_results);
    Stage25ReportWriter::WriteCaseFileIndex(options.out_root, result.case_results);
    Stage25ReportWriter::WriteDiagnosticReport(options.out_root, result.case_results);

    result.ok = true;
    return true;
}
