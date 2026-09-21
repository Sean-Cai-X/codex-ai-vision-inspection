#include "CxScriptStage25ReportWriter.h"
#include "FindlineParameterPolicy.h"
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <algorithm>

void Stage25ReportWriter::WriteBatchReport(
    const std::filesystem::path& out_root,
    const std::vector<Stage25CaseResult>& results)
{
    std::ofstream file(out_root / "batch_report.md");

    file << "# Stage 2.5 L1~L3 Parameter Consistency Report\n\n";
    file << "## Summary\n\n";

    const int total = static_cast<int>(results.size());

    const int skipped =
        static_cast<int>(std::count_if(
            results.begin(),
            results.end(),
            [](const auto& r) { return r.skipped_by_preflight; }));

    const int executed = total - skipped;

    const int t0_pass =
        static_cast<int>(std::count_if(
            results.begin(),
            results.end(),
            [](const auto& r) {
                return !r.skipped_by_preflight && r.t0_pass;
            }));

    const int t1_pass =
        static_cast<int>(std::count_if(
            results.begin(),
            results.end(),
            [](const auto& r) {
                return !r.skipped_by_preflight && r.t1_pass;
            }));

    const int t2_pass =
        static_cast<int>(std::count_if(
            results.begin(),
            results.end(),
            [](const auto& r) {
                return !r.skipped_by_preflight && r.t2_pass;
            }));

    file << "- Total scheduled cases: " << total << "\n";
    file << "- Skipped by preflight: " << skipped << "\n";
    file << "- Executed cases: " << executed << "\n";
    file << "- T0 execution pass: " << t0_pass << "/" << executed << "\n";
    file << "- T1 algorithm pass: " << t1_pass << "/" << executed << "\n";
    file << "- T2 evidence support pass: " << t2_pass << "/" << executed << "\n\n";

    file << "## Findline Cases\n\n";
    file << "| Level | Image | Target | Orientation | Profile | ParamPolicy | ParamRole | ProductDefault | Stage25Default | Points | Fit | T1 | T2 | LocalSupport | LocalMeanDist | ComponentWarning | Quality |\n";
    file << "|---|---|---|---|---|---|---|---|---|---:|---|---|---|---:|---:|---|---|\n";

    for (const auto& r : results)
    {
        if (r.tool != "FindLine") continue;

        std::string component_warning = "";
        if (r.component_shape == "FOREGROUND_DOMINATED_BY_SINGLE_COMPONENT")
            component_warning = "FOREGROUND_DOMINATED";
        else if (r.component_shape == "BINARY_COMPONENT_COLLAPSED_TO_LARGE_REGION")
            component_warning = "BINARY_COLLAPSED";
        else if (r.line_filter_min_exceeds_component_p90)
            component_warning = "FILTER_EXCEEDS_P90";

        file << "| " << r.level << " | " << r.image_id << " | " << r.target_id << " | "
             << r.orientation << " | " << r.profile_id << " | " << r.parameter_policy_id
             << " | " << r.parameter_role << " | " << (r.is_product_default ? "YES" : "NO")
             << " | " << (r.is_stage25_default ? "YES" : "NO")
             << " | " << r.valid_points_count << " | " << (r.has_fit_line ? "true" : "false")
             << " | " << (r.t1_pass ? "true" : "false")
             << " | " << (r.t2_pass ? "true" : "false")
             << " | " << std::fixed << std::setprecision(3) << r.measured_local_support_score
             << " | " << r.measured_local_mean_distance_px
             << " | " << component_warning
             << " | " << r.quality_classification << " |\n";
    }

    file << "\n## Findcircle Cases\n\n";
    file << "| Level | Image | Target | Orientation | Profile | ParamPolicy | ParamRole | ProductDefault | Stage25Default | Points | FitCircle | T1 | T2 | LocalSupport | LocalMeanRadialDist | Quality |\n";
    file << "|---|---|---|---|---|---|---|---|---|---:|---|---|---|---:|---:|---|\n";

    for (const auto& r : results)
    {
        if (r.tool != "Findcircle") continue;
        file << "| " << r.level << " | " << r.image_id << " | " << r.target_id << " | "
             << r.orientation << " | " << r.profile_id << " | " << r.parameter_policy_id
             << " | " << r.parameter_role << " | " << (r.is_product_default ? "YES" : "NO")
             << " | " << (r.is_stage25_default ? "YES" : "NO")
             << " | " << r.valid_points_count << " | " << (r.has_fit_circle ? "true" : "false")
             << " | " << (r.t1_pass ? "true" : "false")
             << " | " << (r.t2_pass ? "true" : "false")
             << " | " << std::fixed << std::setprecision(3) << r.circle_local_support_score
             << " | " << r.circle_local_mean_radial_distance_px
             << " | " << r.quality_classification << " |\n";
    }

    file << "\n## Skipped By Preflight\n\n";
    file << "| Level | Image | Target | Tool | Reason |\n";
    file << "|---|---|---|---|---|\n";

    for (const auto& r : results)
    {
        if (!r.skipped_by_preflight) continue;
        file << "| " << r.level << " | " << r.image_id << " | " << r.target_id << " | "
             << r.tool << " | " << r.skip_reason << " |\n";
    }
}

void Stage25ReportWriter::WritePreflightReport(
    const std::filesystem::path& out_root,
    const std::vector<Stage25ImagePreflightResult>& results)
{
    std::ofstream file(out_root / "image_preflight_report.md");

    file << "# Stage 2.5 L1~L3 Image Preflight Report\n\n";
    file << "## Summary\n\n";

    int total = static_cast<int>(results.size());
    int ok = static_cast<int>(std::count_if(results.begin(), results.end(),
        [](const auto& r) { return r.preflight_class == "OK"; }));
    int warning = static_cast<int>(std::count_if(results.begin(), results.end(),
        [](const auto& r) { return r.preflight_class == "WARNING"; }));
    int invalid = static_cast<int>(std::count_if(results.begin(), results.end(),
        [](const auto& r) { return !r.roi_valid; }));

    file << "- Total targets: " << total << "\n";
    file << "- OK: " << ok << "\n";
    file << "- With warnings: " << warning << "\n";
    file << "- Invalid (not run): " << invalid << "\n\n";

    file << "## Preflight Details\n\n";
    file << "| Level | Image | Target | Tool | ROIValid | PreflightClass | Contrast | GradientMean | BlurScore |\n";
    file << "|---|---|---|---|---|---|---:|---:|---:|\n";

    for (const auto& r : results)
    {
        file << "| " << r.level << " | " << r.image_id << " | " << r.target_id << " | "
             << r.tool << " | " << (r.roi_valid ? "true" : "false") << " | "
             << r.preflight_class << " | " << std::fixed << std::setprecision(0)
             << (r.image_loaded ? (r.gray_std * 2.0) : 0) << " | "
             << std::setprecision(2) << r.gradient_mean << " | "
             << r.blur_score << " |\n";
    }
}

void Stage25ReportWriter::WriteCoverageReport(
    const std::filesystem::path& out_root,
    const Stage25Manifest& manifest)
{
    std::ofstream file(out_root / "image_coverage_report.md");

    file << "# Stage 2.5 L1~L3 Image Coverage Report\n\n";
    file << "## Summary\n\n";

    std::map<std::string, int> image_count_by_level;
    std::map<std::string, int> findline_target_count_by_level;
    std::map<std::string, int> findcircle_target_count_by_level;

    int total_images = 0;
    int total_findline = 0;
    int total_findcircle = 0;

    for (const auto& img : manifest.images)
    {
        image_count_by_level[img.level]++;
        total_images++;

        for (const auto& target : img.targets)
        {
            if (target.tool == "FindLine")
            {
                findline_target_count_by_level[img.level]++;
                total_findline++;
            }
            else if (target.tool == "Findcircle")
            {
                findcircle_target_count_by_level[img.level]++;
                total_findcircle++;
            }
        }
    }

    file << "- Total images: " << total_images << "\n";
    file << "- Total Findline targets: " << total_findline << "\n";
    file << "- Total Findcircle targets: " << total_findcircle << "\n\n";

    bool l1_ok = findline_target_count_by_level["L1_high_contrast"] >= 2 &&
                 findcircle_target_count_by_level["L1_high_contrast"] >= 2;
    bool l2_ok = findline_target_count_by_level["L2_low_contrast_illumination"] >= 2 &&
                 findcircle_target_count_by_level["L2_low_contrast_illumination"] >= 2;
    bool l3_ok = findline_target_count_by_level["L3_complex_boundary"] >= 2 &&
                 findcircle_target_count_by_level["L3_complex_boundary"] >= 2;

    std::string coverage_status = (l1_ok && l2_ok && l3_ok) ? "OK" : "INSUFFICIENT_COVERAGE";
    file << "- Coverage status: " << coverage_status << "\n\n";

    file << "## Coverage Details\n\n";
    file << "| Level | ImageCount | FindlineTargets | FindcircleTargets | Coverage |\n";
    file << "|---|---:|---:|---:|---|\n";

    for (const auto& [level, count] : image_count_by_level)
    {
        std::string status = "OK";
        if (level == "L1_high_contrast") status = l1_ok ? "OK" : "INSUFFICIENT";
        else if (level == "L2_low_contrast_illumination") status = l2_ok ? "OK" : "INSUFFICIENT";
        else if (level == "L3_complex_boundary") status = l3_ok ? "OK" : "INSUFFICIENT";

        file << "| " << level << " | " << count << " | "
             << findline_target_count_by_level[level] << " | "
             << findcircle_target_count_by_level[level] << " | "
             << status << " |\n";
    }
}

void Stage25ReportWriter::WriteStabilityReport(
    const std::filesystem::path& out_root,
    const std::vector<Stage25CaseResult>& results,
    const Stage25Manifest& manifest)
{
    std::ofstream file(out_root / "parameter_stability_report.md");

    file << "# Stage 2.5 L1~L3 Parameter Stability Report\n\n";

    for (const std::string& tool : {"FindLine", "Findcircle"})
    {
        std::vector<Stage25CaseResult> tool_results;
        std::copy_if(results.begin(), results.end(), std::back_inserter(tool_results),
            [&tool](const auto& r) { return r.tool == tool && !r.skipped_by_preflight; });

        std::map<std::string, std::vector<Stage25CaseResult>> profile_results;
        for (const auto& r : tool_results)
        {
            profile_results[r.profile_id].push_back(r);
        }

        file << "## " << tool << " Stability By Level\n\n";
        file << "| Profile | Level | Targets | OriginalSuccess | LocalConfirmed | MeanLocalSupport | MeanLocalDist |\n";
        file << "|---|---|---:|---:|---:|---:|---:|\n";

        for (const auto& [profile_id, prof_results] : profile_results)
        {
            std::map<std::string, std::vector<Stage25CaseResult>> level_results;
            for (const auto& r : prof_results)
            {
                level_results[r.level].push_back(r);
            }

            for (const auto& [level, lev_results] : level_results)
            {
                int total = static_cast<int>(lev_results.size());
                int success = static_cast<int>(std::count_if(lev_results.begin(), lev_results.end(),
                    [](const auto& r) { return r.t1_pass; }));
                int confirmed = static_cast<int>(std::count_if(lev_results.begin(), lev_results.end(),
                    [](const auto& r) { return r.quality_classification == "ORIGINAL_LOCAL_EDGE_CONFIRMED"; }));

                double mean_support = 0.0;
                double mean_dist = 0.0;

                for (const auto& r : lev_results)
                {
                    mean_support += (tool == "FindLine") ? r.measured_local_support_score : r.circle_local_support_score;
                    mean_dist += (tool == "FindLine") ? r.measured_local_mean_distance_px : r.circle_local_mean_radial_distance_px;
                }

                if (total > 0)
                {
                    mean_support /= total;
                    mean_dist /= total;
                }

                file << "| " << profile_id << " | " << level << " | " << total
                     << " | " << success << "/" << total << " | " << confirmed << "/" << total
                     << " | " << std::fixed << std::setprecision(3) << mean_support
                     << " | " << mean_dist << " |\n";
            }
        }

        file << "\n## " << tool << " Overall Stability\n\n";
        file << "| Profile | Levels | Targets | Score | Recommendation |\n";
        file << "|---|---|---:|---:|---|\n";

        for (const auto& [profile_id, prof_results] : profile_results)
        {
            int total = static_cast<int>(prof_results.size());
            std::set<std::string> level_set;
            for (const auto& r : prof_results)
                level_set.insert(r.level);
            int levels = static_cast<int>(level_set.size());

            int success = static_cast<int>(std::count_if(prof_results.begin(), prof_results.end(),
                [](const auto& r) { return r.t1_pass; }));
            int confirmed = static_cast<int>(std::count_if(prof_results.begin(), prof_results.end(),
                [](const auto& r) { return r.quality_classification == "ORIGINAL_LOCAL_EDGE_CONFIRMED"; }));

            double mean_support = 0.0;
            for (const auto& r : prof_results)
            {
                mean_support += (tool == "FindLine") ? r.measured_local_support_score : r.circle_local_support_score;
            }
            if (total > 0) mean_support /= total;

            double success_rate = total > 0 ? static_cast<double>(success) / total : 0.0;
            double confirmed_rate = total > 0 ? static_cast<double>(confirmed) / total : 0.0;

            double score = success_rate * 30.0 + confirmed_rate * 30.0 + mean_support * 20.0;

            std::string recommendation = "BASELINE_ONLY";
            if (total >= 6 && levels >= 2)
            {
                if (score >= 80.0) recommendation = "PROFILE_RECOMMENDED";
                else if (score >= 65.0) recommendation = "PROFILE_CONDITIONALLY_RECOMMENDED";
                else if (success_rate < 0.5) recommendation = "PROFILE_UNSTABLE";
                else recommendation = "IMAGE_SPECIFIC_PROFILE";
            }

            file << "| " << profile_id << " | " << levels << " | " << total
                 << " | " << std::fixed << std::setprecision(1) << score
                 << " | " << recommendation << " |\n";
        }

        if (tool == "FindLine")
        {
            file << "\n## Findline Product Default Gate\n\n";
            file << "| Profile | ParamPolicy | Role | Images | Levels | Orientations | T1Rate | T2Rate | LocalConfirmedRate | ComponentWarningRate | MeanFitOffset | CanPromote | GateReason |\n";
            file << "|---|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---|\n";

            for (const auto& [profile_id, prof_results] : profile_results)
            {
                Stage25ProfileAggregate agg;
                agg.profile_id = profile_id;
                agg.tool = "FindLine";

                if (!prof_results.empty())
                {
                    agg.parameter_policy_id = prof_results[0].parameter_policy_id;
                    agg.parameter_role = prof_results[0].parameter_role;
                }

                agg.total_cases = static_cast<int>(prof_results.size());

                std::set<std::string> image_set;
                std::set<std::string> level_set;
                std::set<std::string> orient_set;

                for (const auto& r : prof_results)
                {
                    image_set.insert(r.image_id);
                    level_set.insert(r.level);
                    if (!r.orientation.empty())
                        orient_set.insert(r.orientation);

                    if (r.t1_pass) agg.t1_pass++;
                    if (r.t2_pass) agg.t2_pass++;
                    if (r.quality_classification == "ORIGINAL_LOCAL_EDGE_CONFIRMED")
                        agg.local_confirmed++;
                    if (r.component_shape == "FOREGROUND_DOMINATED_BY_SINGLE_COMPONENT" ||
                        r.component_shape == "BINARY_COMPONENT_COLLAPSED_TO_LARGE_REGION" ||
                        r.line_filter_min_exceeds_component_p90)
                        agg.component_warning++;

                    agg.mean_local_support += r.measured_local_support_score;
                    agg.mean_local_distance += r.measured_local_mean_distance_px;
                    agg.mean_fit_offset += r.fit_offset_error_px;
                }

                agg.total_images = static_cast<int>(image_set.size());
                agg.level_count = static_cast<int>(level_set.size());
                agg.orientation_count = static_cast<int>(orient_set.size());

                if (agg.total_cases > 0)
                {
                    agg.original_success_rate = static_cast<double>(agg.t1_pass) / agg.total_cases;
                    agg.local_confirmed_rate = static_cast<double>(agg.local_confirmed) / agg.total_cases;
                    agg.component_warning_rate = static_cast<double>(agg.component_warning) / agg.total_cases;
                    agg.mean_local_support /= agg.total_cases;
                    agg.mean_local_distance /= agg.total_cases;
                    agg.mean_fit_offset /= agg.total_cases;
                }

                FindlineProductDefaultGateInput gate_input;
                gate_input.total_images = agg.total_images;
                gate_input.level_count = agg.level_count;
                gate_input.orientation_count = agg.orientation_count;
                gate_input.original_success_rate = agg.original_success_rate;
                gate_input.local_confirmed_rate = agg.local_confirmed_rate;
                gate_input.component_warning_rate = agg.component_warning_rate;
                gate_input.mean_fit_offset = agg.mean_fit_offset;

                auto gate = EvaluateFindlineProductDefaultGate(gate_input);

                agg.can_promote_to_product_default = gate.can_promote;
                agg.product_default_gate_reason = gate.reason;

                file << "| " << agg.profile_id << " | " << agg.parameter_policy_id << " | " << agg.parameter_role
                     << " | " << agg.total_images << " | " << agg.level_count << " | " << agg.orientation_count
                     << " | " << std::fixed << std::setprecision(1) << (agg.original_success_rate * 100) << "% | "
                     << std::setprecision(1) << (static_cast<double>(agg.t2_pass) / agg.total_cases * 100) << "% | "
                     << std::setprecision(1) << (agg.local_confirmed_rate * 100) << "% | "
                     << std::setprecision(1) << (agg.component_warning_rate * 100) << "% | "
                     << std::setprecision(1) << agg.mean_fit_offset << " | "
                     << (agg.can_promote_to_product_default ? "YES" : "NO") << " | "
                     << agg.product_default_gate_reason << " |\n";
            }
        }
    }
}

void Stage25ReportWriter::WritePolicyReport(
    const std::filesystem::path& out_root,
    const std::vector<Stage25CaseResult>& results,
    const Stage25Manifest& manifest)
{
    std::ofstream file(out_root / "parameter_policy_report.md");

    file << "# Stage 2.5 L1~L3 Parameter Policy Decision\n\n";

    file << "## Important Notice\n\n";
    file << "> **Stage25Filter20 is NOT the product default.** It is a Stage25 recommended template for testing purposes only.\n";
    file << "> The product default remains unchanged (LegacyDefault: filter_profile=0, effective_filter_min=50).\n\n";

    file << "## Findline Parameter Policies\n\n";
    file << "| PolicyID | DisplayName | Role | IsProdDefault | IsStage25Default | threshold | linegap | filter_profile |\n";
    file << "|---|---|---|---|---|---:|---:|---|\n";

    for (const auto& kind : {
        FindlineParameterPolicyKind::LegacyDefault,
        FindlineParameterPolicyKind::Stage25Filter20,
        FindlineParameterPolicyKind::LowContrastThreshold8,
        FindlineParameterPolicyKind::ComplexBoundaryLinegap10,
        FindlineParameterPolicyKind::DebugFilterRelaxMin1,
        FindlineParameterPolicyKind::RiskGamma })
    {
        const auto& p = MakeFindlinePolicy(kind);
        file << "| " << p.policy_id << " | " << p.display_name << " | "
             << ToString(p.role) << " | " << (p.is_product_default ? "YES" : "NO")
             << " | " << (p.is_stage25_default ? "YES" : "NO")
             << " | " << p.threshold << " | " << p.linegap << " | " << p.filter_profile << " |\n";
    }

    std::vector<Stage25CaseResult> findline_results;
    std::copy_if(results.begin(), results.end(), std::back_inserter(findline_results),
        [](const auto& r) { return r.tool == "FindLine" && !r.skipped_by_preflight; });

    file << "\n## Findline Policy Statistics\n\n";
    file << "| PolicyID | Total | T1Pass | T1Rate | T2Pass | T2Rate | MeanSupport | MeanDist |\n";
    file << "|---|---:|---:|---:|---:|---:|---:|---:|\n";

    std::map<std::string, std::vector<Stage25CaseResult>> policy_results;
    for (const auto& r : findline_results)
    {
        policy_results[r.parameter_policy_id].push_back(r);
    }

    for (const auto& [policy_id, pol_results] : policy_results)
    {
        int total = static_cast<int>(pol_results.size());
        int t1_pass = static_cast<int>(std::count_if(pol_results.begin(), pol_results.end(),
            [](const auto& r) { return r.t1_pass; }));
        int t2_pass = static_cast<int>(std::count_if(pol_results.begin(), pol_results.end(),
            [](const auto& r) { return r.t2_pass; }));

        double mean_support = 0.0;
        double mean_dist = 0.0;
        for (const auto& r : pol_results)
        {
            mean_support += r.measured_local_support_score;
            mean_dist += r.measured_local_mean_distance_px;
        }
        if (total > 0)
        {
            mean_support /= total;
            mean_dist /= total;
        }

        double t1_rate = total > 0 ? static_cast<double>(t1_pass) / total : 0.0;
        double t2_rate = total > 0 ? static_cast<double>(t2_pass) / total : 0.0;

        file << "| " << policy_id << " | " << total << " | " << t1_pass
             << " | " << std::fixed << std::setprecision(1) << (t1_rate * 100) << "% | "
             << t2_pass << " | " << std::setprecision(1) << (t2_rate * 100) << "% | "
             << std::setprecision(3) << mean_support << " | " << mean_dist << " |\n";
    }

    file << "\n## Product Default Gate Evaluation\n\n";

    FindlineProductDefaultGateInput gate_input;
    gate_input.total_images = static_cast<int>(manifest.images.size());
    
    std::set<std::string> level_set;
    for (const auto& img : manifest.images)
        level_set.insert(img.level);
    gate_input.level_count = static_cast<int>(level_set.size());

    gate_input.orientation_count = 2;

    int executed = static_cast<int>(findline_results.size());
    int success = static_cast<int>(std::count_if(findline_results.begin(), findline_results.end(),
        [](const auto& r) { return r.t1_pass; }));
    gate_input.original_success_rate = executed > 0 ? static_cast<double>(success) / executed : 0.0;

    int confirmed = static_cast<int>(std::count_if(findline_results.begin(), findline_results.end(),
        [](const auto& r) { return r.quality_classification == "ORIGINAL_LOCAL_EDGE_CONFIRMED"; }));
    gate_input.local_confirmed_rate = executed > 0 ? static_cast<double>(confirmed) / executed : 0.0;

    gate_input.component_warning_rate = 0.0;

    double mean_offset = 0.0;
    for (const auto& r : findline_results)
        mean_offset += r.fit_offset_error_px;
    gate_input.mean_fit_offset = executed > 0 ? mean_offset / executed : 0.0;

    auto gate_result = EvaluateFindlineProductDefaultGate(gate_input);

    file << "| Gate | Value | Threshold | Status |\n";
    file << "|---|---:|---:|---|\n";
    file << "| total_images | " << gate_input.total_images << " | >=12 | "
         << (gate_input.total_images >= 12 ? "PASS" : "FAIL") << " |\n";
    file << "| level_count | " << gate_input.level_count << " | >=3 | "
         << (gate_input.level_count >= 3 ? "PASS" : "FAIL") << " |\n";
    file << "| orientation_count | " << gate_input.orientation_count << " | >=2 | "
         << (gate_input.orientation_count >= 2 ? "PASS" : "FAIL") << " |\n";
    file << "| original_success_rate | " << std::fixed << std::setprecision(1) << (gate_input.original_success_rate * 100) << "% | >=85% | "
         << (gate_input.original_success_rate >= 0.85 ? "PASS" : "FAIL") << " |\n";
    file << "| local_confirmed_rate | " << std::setprecision(1) << (gate_input.local_confirmed_rate * 100) << "% | >=80% | "
         << (gate_input.local_confirmed_rate >= 0.80 ? "PASS" : "FAIL") << " |\n";
    file << "| component_warning_rate | " << std::setprecision(1) << (gate_input.component_warning_rate * 100) << "% | <=20% | "
         << (gate_input.component_warning_rate <= 0.20 ? "PASS" : "FAIL") << " |\n";
    file << "| mean_fit_offset | " << std::setprecision(1) << gate_input.mean_fit_offset << " px | <=6.0 | "
         << (gate_input.mean_fit_offset <= 6.0 ? "PASS" : "FAIL") << " |\n";

    file << "\n**Promotion Decision:** " << (gate_result.can_promote ? "CAN_PROMOTE" : "CANNOT_PROMOTE") << "\n";
    file << "**Reason:** " << gate_result.reason << "\n";

    file << "\n## Decision Summary\n\n";
    file << "| Action | Item | Status |\n";
    file << "|---|---|---|\n";
    file << "| KEEP | LegacyDefault as product default | current default |\n";
    file << "| TEST | Stage25Filter20 as Stage25 recommended template | not promoted |\n";
    file << "| MONITOR | LowContrastThreshold8 for L2 scenarios | candidate |\n";
    file << "| MONITOR | ComplexBoundaryLinegap10 for L3 scenarios | candidate |\n";
    file << "| REJECT | DebugFilterRelaxMin1 | too permissive |\n";
    file << "| REJECT | RiskGamma | introduces binary collapse risk |\n";
}

void Stage25ReportWriter::WritePolicyValidationReport(
    const std::filesystem::path& out_root,
    const Stage25PolicyValidationResult& validation)
{
    std::ofstream file(out_root / "parameter_policy_validation_report.md");

    file << "# Stage 2.5 L1~L3 Parameter Policy Validation Report\n\n";

    if (validation.ok)
    {
        file << "## Validation Status: PASSED\n\n";
        file << "All parameter policies are correctly configured.\n";
    }
    else
    {
        file << "## Validation Status: FAILED\n\n";
        file << "One or more parameter policies have configuration errors.\n";
    }

    file << "\n## Issues\n\n";
    file << "| Severity | Scope | ProfileID | Message |\n";
    file << "|---|---|---|---|\n";

    if (validation.issues.empty())
    {
        file << "| - | - | - | No issues found |\n";
    }
    else
    {
        for (const auto& issue : validation.issues)
        {
            file << "| " << issue.severity << " | " << issue.scope
                 << " | " << issue.profile_id << " | " << issue.message << " |\n";
        }
    }

    int error_count = static_cast<int>(std::count_if(validation.issues.begin(),
        validation.issues.end(), [](const auto& i) { return i.severity == "error"; }));
    int warning_count = static_cast<int>(std::count_if(validation.issues.begin(),
        validation.issues.end(), [](const auto& i) { return i.severity == "warning"; }));

    file << "\n## Summary\n\n";
    file << "- Errors: " << error_count << "\n";
    file << "- Warnings: " << warning_count << "\n";
    file << "- Result: " << (validation.ok ? "PASS" : "FAIL") << "\n";
}

void Stage25ReportWriter::WriteCaseMatrixReport(
    const std::filesystem::path& out_root,
    const std::vector<Stage25CaseMatrixEntry>& matrix)
{
    std::ofstream file(out_root / "stage25_case_matrix_report.md");

    file << "# Stage 2.5 L1~L3 Case Matrix Report\n\n";

    file << "## Summary\n\n";
    int enabled_count = static_cast<int>(std::count_if(matrix.begin(), matrix.end(),
        [](const auto& e) { return e.enabled; }));
    int disabled_count = static_cast<int>(matrix.size()) - enabled_count;

    file << "- Total entries: " << matrix.size() << "\n";
    file << "- Enabled cases: " << enabled_count << "\n";
    file << "- Disabled cases: " << disabled_count << "\n\n";

    file << "## Case Matrix\n\n";
    file << "| Level | Image | Target | Tool | Profile | ParamPolicy | ParamRole | Evidence | Enabled | Reason |\n";
    file << "|---|---|---|---|---|---|---|---|---|---|\n";

    for (const auto& entry : matrix)
    {
        file << "| " << entry.level << " | " << entry.image_id << " | " << entry.target_id
             << " | " << entry.tool << " | " << entry.profile_id
             << " | " << entry.parameter_policy_id << " | " << entry.parameter_role
             << " | " << entry.evidence_profile
             << " | " << (entry.enabled ? "YES" : "NO")
             << " | " << entry.reason << " |\n";
    }
}

void Stage25ReportWriter::WriteFastMatchReadinessReport(
    const std::filesystem::path& out_root,
    const std::vector<Stage25CaseResult>& results)
{
    std::ofstream file(out_root / "fastmatch_readiness_report.md");

    file << "# Stage 2.5 FastMatch Readiness Report\n\n";

    file << "## Important Notice\n\n";
    file << "> **FastMatch is in Readiness/Diagnostic Shell stage.**\n";
    file << "> It does NOT replace Findline/Findcircle Measure.\n";
    file << "> It does NOT affect Product Default.\n";
    file << "> It is only allowed as a diagnostic candidate for L3 or component-warning cases.\n\n";

    file << "## FastMatch Readiness Summary\n\n";

    int allowed_count = 0;
    int blocked_count = 0;
    int no_fastmatch_count = 0;

    for (const auto& r : results)
    {
        if (r.tool != "FastMatchDiagnostic")
        {
            no_fastmatch_count++;
            continue;
        }

        if (r.fastmatch_allowed)
            allowed_count++;
        else
            blocked_count++;
    }

    file << "- Total cases: " << results.size() << "\n";
    file << "- Non-FastMatch cases: " << no_fastmatch_count << "\n";
    file << "- FastMatch allowed: " << allowed_count << "\n";
    file << "- FastMatch blocked: " << blocked_count << "\n\n";

    file << "## FastMatch Readiness Gate Details\n\n";
    file << "| Case | Tool | Level | Profile | L1L3Coverage | PolicyValid | ProductDefaultChanged | OriginalMeasure | LocalEvidence | ComponentWarning | Allowed | Status | Reason |\n";
    file << "|---|---|---|---|---|---|---|---|---|---|---|---|---|\n";

    for (const auto& r : results)
    {
        if (r.tool != "FastMatchDiagnostic")
            continue;

        file << "| " << r.case_id << " | " << r.tool << " | " << r.level << " | "
             << r.profile_id << " | "
             << (r.l1_l3_coverage_ok ? "YES" : "NO") << " | "
             << (r.parameter_policy_valid ? "YES" : "NO") << " | "
             << (r.product_default_changed ? "YES" : "NO") << " | "
             << (r.original_measure_available ? "YES" : "NO") << " | "
             << (r.local_evidence_confirmed ? "YES" : "NO") << " | "
             << (r.component_warning ? "YES" : "NO") << " | "
             << (r.fastmatch_allowed ? "YES" : "NO") << " | "
             << r.fastmatch_status << " | " << r.fastmatch_reason << " |\n";
    }

    file << "\n## Classification\n\n";

    file << "### FASTMATCH_BLOCKED_NO_COVERAGE\n";
    for (const auto& r : results)
    {
        if (r.tool != "FastMatchDiagnostic") continue;
        if (!r.fastmatch_allowed && r.fastmatch_reason.find("L1/L2/L3 coverage") != std::string::npos)
            file << "- " << r.case_id << ": " << r.fastmatch_reason << "\n";
    }

    file << "\n### FASTMATCH_BLOCKED_POLICY_INVALID\n";
    for (const auto& r : results)
    {
        if (r.tool != "FastMatchDiagnostic") continue;
        if (!r.fastmatch_allowed && r.fastmatch_reason.find("parameter policy validation") != std::string::npos)
            file << "- " << r.case_id << ": " << r.fastmatch_reason << "\n";
    }

    file << "\n### FASTMATCH_BLOCKED_PRODUCT_DEFAULT_CHANGED\n";
    for (const auto& r : results)
    {
        if (r.tool != "FastMatchDiagnostic") continue;
        if (!r.fastmatch_allowed && r.fastmatch_reason.find("product default was changed") != std::string::npos)
            file << "- " << r.case_id << ": " << r.fastmatch_reason << "\n";
    }

    file << "\n### FASTMATCH_BLOCKED_NO_ORIGINAL_MEASURE\n";
    for (const auto& r : results)
    {
        if (r.tool != "FastMatchDiagnostic") continue;
        if (!r.fastmatch_allowed && r.fastmatch_reason.find("original Measure result is unavailable") != std::string::npos)
            file << "- " << r.case_id << ": " << r.fastmatch_reason << "\n";
    }

    file << "\n### FASTMATCH_BLOCKED_NOT_L3_NO_COMPONENT_WARNING\n";
    for (const auto& r : results)
    {
        if (r.tool != "FastMatchDiagnostic") continue;
        if (!r.fastmatch_allowed && r.fastmatch_reason.find("reserved for L3 or component-warning") != std::string::npos)
            file << "- " << r.case_id << ": " << r.fastmatch_reason << "\n";
    }

    file << "\n### FASTMATCH_ALLOWED_DIAGNOSTIC\n";
    for (const auto& r : results)
    {
        if (r.tool != "FastMatchDiagnostic") continue;
        if (r.fastmatch_allowed)
            file << "- " << r.case_id << ": " << r.fastmatch_reason << "\n";
    }

    file << "\n## Important Reminder\n\n";
    file << "Even when **Allowed=true**, FastMatch remains:\n";
    file << "- allowed for diagnostic only\n";
    file << "- NOT promoted to Measure default\n";
    file << "- NOT replacing original Measure\n";
    file << "- NOT affecting product default parameters\n";
}

void Stage25ReportWriter::WriteCaseFileIndex(
    const std::filesystem::path& out_root,
    const std::vector<Stage25CaseResult>& results)
{
    std::ofstream file(out_root / "case_file_index.md");

    file << "# Stage25 Case File Index\n\n";
    file << "| CaseId | Level | Image | Target | Tool | Profile | GeneratedScript | Snapshot | Summary | EvidenceSummary |\n";
    file << "|---|---|---|---|---|---|---|---|---|---|\n";

    for (const auto& r : results)
    {
        file << "| " << r.case_id << " | " << r.level << " | " << r.image_id << " | "
             << r.target_id << " | " << r.tool << " | " << r.profile_id << " | "
             << r.generated_script_path << " | " << r.snapshot_path << " | "
             << r.summary_path << " | " << r.evidence_summary_path << " |\n";
    }
}

static std::string DiagnoseStage25Case(const Stage25CaseResult& r)
{
    if (r.skipped_by_preflight)
        return "Skipped by preflight: " + r.skip_reason;

    if (!r.t0_pass)
        return "Headless execution failed";

    if (!r.summary_exists)
        return "Missing result_summary.json";

    if (!r.evidence_summary_exists)
        return "Missing evidence_summary.json";

    const double support =
        r.tool == "FindLine"
            ? r.measured_local_support_score
            : r.circle_local_support_score;

    if (!r.t1_pass && support >= 0.60)
    {
        return "Image edge exists but original Measure produced no fitted result; check generated cxscript, ROI geometry, tool parameters, and SetLine/SetCircle mapping.";
    }

    if (!r.t1_pass)
    {
        return "Original Measure produced no result; check failure_stage/failure_reason and ROI.";
    }

    if (!r.t2_pass)
    {
        return "Original Measure produced result but local evidence support is weak.";
    }

    return "OK";
}

void Stage25ReportWriter::WriteDiagnosticReport(
    const std::filesystem::path& out_root,
    const std::vector<Stage25CaseResult>& results)
{
    std::ofstream file(out_root / "case_diagnostic_report.md");

    file << "# Stage25 Case Diagnostic Report\n\n";
    file << "## Summary\n\n";

    const int total = static_cast<int>(results.size());
    const int skipped = static_cast<int>(std::count_if(results.begin(), results.end(),
        [](const auto& r) { return r.skipped_by_preflight; }));
    const int executed = total - skipped;

    const int headless_failed = static_cast<int>(std::count_if(results.begin(), results.end(),
        [](const auto& r) { return !r.skipped_by_preflight && !r.t0_pass; }));

    const int no_result = static_cast<int>(std::count_if(results.begin(), results.end(),
        [](const auto& r) { return !r.skipped_by_preflight && r.t0_pass && !r.t1_pass; }));

    const int no_result_with_edge = static_cast<int>(std::count_if(results.begin(), results.end(),
        [](const auto& r) { return r.quality_classification == "ALGORITHM_NO_RESULT_WITH_IMAGE_EDGE"; }));

    const int ok = static_cast<int>(std::count_if(results.begin(), results.end(),
        [](const auto& r) { return r.t2_pass; }));

    file << "- Total scheduled cases: " << total << "\n";
    file << "- Skipped by preflight: " << skipped << "\n";
    file << "- Executed cases: " << executed << "\n";
    file << "- Headless failed: " << headless_failed << "\n";
    file << "- Algorithm no result: " << no_result << "\n";
    file << "- Algorithm no result but image edge detected: " << no_result_with_edge << "\n";
    file << "- Fully verified (T2 pass): " << ok << "\n\n";

    file << "## Case Diagnostics\n\n";
    file << "| CaseId | Tool | Profile | Points | Fit | FailureStage | FailureReason | LocalSupport | Diagnosis |\n";
    file << "|---|---|---|---:|---|---|---|---:|---|\n";

    for (const auto& r : results)
    {
        const double support =
            r.tool == "FindLine"
                ? r.measured_local_support_score
                : r.circle_local_support_score;

        file << "| " << r.case_id << " | " << r.tool << " | " << r.profile_id << " | "
             << r.valid_points_count << " | "
             << (r.tool == "FindLine" ? (r.has_fit_line ? "true" : "false") : (r.has_fit_circle ? "true" : "false"))
             << " | " << r.failure_stage << " | " << r.failure_reason << " | "
             << std::fixed << std::setprecision(3) << support << " | "
             << DiagnoseStage25Case(r) << " |\n";
    }
}