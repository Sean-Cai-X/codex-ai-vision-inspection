#include "CxScriptSuiteReportWriter.h"
#include "CxPrecisionEvaluation.h"
#include <fstream>
#include <iomanip>
#include <map>

namespace
{
bool IsGenericContractStatus(const std::string& status)
{
    return status.empty() ||
           status == "contract_failed" ||
           status == "contract_fail" ||
           status == "failed" ||
           status == "unknown";
}

std::string DeriveFailureClass(const CxScriptSuiteCaseResult& result)
{
    if (!result.headless_ok)
        return "headless_execution_failed";

    if (result.runtime_global_result_mismatch)
        return "runtime_global_result_mismatch";

    if (!IsGenericContractStatus(result.contract_status))
        return result.contract_status;

    if (result.tool == "FindLine" &&
        result.failure_stage == "result_points_available" &&
        result.rendered_measure_points_count > 0 &&
        !result.has_fit_line)
    {
        return "findline_measure_points_below_fit_min";
    }

    if (result.tool == "FindCircle" &&
        result.failure_stage == "result_points_available" &&
        result.valid_points_count > 0 &&
        !result.has_fit_circle)
    {
        return "findcircle_fit_degenerate_after_measure_points";
    }

    if (!result.failure_stage.empty() &&
        result.failure_stage != "ok" &&
        result.failure_stage != "runtime_finished" &&
        result.failure_stage != "geometry_result_available")
    {
        return result.failure_stage;
    }

    if (result.tool == "FindLine")
    {
        if (result.valid_points_count <= 0)
            return "findline_fail_no_points";
        if (!result.has_fit_line)
            return "findline_fail_fit_degenerate";

        const CxPrecisionGateSnapshot precision = EvaluateCxPrecisionGate(result);
        if (precision.evaluated && !precision.residual_pass)
            return "findline_fail_precision_residual";
        if (precision.evaluated && !precision.support_pass)
            return "findline_fail_precision_support";
        if (result.local_support > 0.0 && result.local_support < 0.6)
            return "findline_fail_low_support";
        return "findline_contract_metric_failed";
    }

    if (result.tool == "FindCircle")
    {
        const CxPrecisionGateSnapshot precision = EvaluateCxPrecisionGate(result);
        if (precision.evaluated && !precision.residual_pass)
            return "findcircle_fail_precision_residual";
        if (result.valid_points_count < 3)
            return "findcircle_fail_insufficient_points";
        if (!result.has_fit_circle)
            return "findcircle_fail_no_fit_circle";
        if (result.circle_radius <= 0.0)
            return "findcircle_fail_invalid_radius";
        return "findcircle_contract_metric_failed";
    }

    return "contract_failed_unclassified";
}
std::string SuggestedActionForFailureClass(const std::string& failureClass)
{
    static const std::map<std::string, std::string> suggestions = {
        {"headless_execution_failed", "先检查 image/script/manifest/timeout 和 headless 运行日志"},
        {"runtime_global_result_mismatch", "先修复 cxscript global_* 回写和 runtime capture 一致性，不调算法"},
        {"findline_fail_filter_reject", "优先调整 filter_profile / threshold / polarity，不改 fitline"},
        {"findline_fail_fit_degenerate", "检查 valid point 去重、共线判断、最小点数策略"},
        {"findline_measure_points_below_fit_min", "Measure 已产生点但不足以拟合；优先检查 linegap、采样方向、最小拟合点数，不先改 FindObject"},
        {"findline_fail_low_support", "检查 ROI、采样方向、line normal、局部 evidence"},
        {"findline_fail_precision_residual", "FindLine residual gate 已失败；优先检查亚像素点集、输入/参数/调用顺序和对象状态，不先放宽阈值"},
        {"findline_fail_precision_support", "FindLine support gate 已失败；优先检查 boundary_coverage_ratio、ROI 覆盖和 scan 行有效性"},

        {"findline_fail_no_points", "检查 FindObject 分支、二值图到 scan run 的传递、ROI/方向/极性"},
        {"findline_scan_no_measure_points_after_prefilter", "FindObject 已产生/保留前景但 scan 没有输出点；优先检查 selection mask 与 scan 行带相交、ROI 方向、端点拒绝策略"},
        {"findline_fail_prefilter_foreground_not_visible_to_scan_rows", "检查预过滤前景是否落在 scan 行带内，优先看 scan_rows/evidence_overlay"},
        {"findline_fail_binary_saturated_or_no_segment_boundary", "检查 method/极性导致二值区域饱和或无段边界"},
        {"findcircle_fail_insufficient_points", "检查 gap / linegap / threshold / circle ROI 半径和圆弧覆盖"},
        {"findcircle_fail_no_fit_circle", "检查拟合方法、异常点、圆弧覆盖比例"},
        {"findcircle_fit_degenerate_after_measure_points", "Measure 已有圆点但拟合失败；优先检查弧覆盖、离群点、最小拟合点数和半径约束"},
        {"circle_measure_no_result_points", "FindCircle 未产生测量点；优先检查圆环采样半径、gap/linegap、极性和阈值"},
        {"findcircle_fail_invalid_radius", "检查 setcircle(cx,cy,px,py) 半径语义和 Gauge 初始化"},
        {"findcircle_fail_high_residual", "检查异常点剔除、圆弧遮挡、低对比残差阈值"},
        {"findcircle_fail_precision_residual", "精度 residual gate 已失败；优先做输入/参数/调用顺序/对象状态差分，不先放宽阈值"},
        {"findline_contract_metric_failed", "检查 FindLine contract 指标、overlay/tool_display 与 runtime summary 是否一致"},
        {"findcircle_contract_metric_failed", "检查 FindCircle contract 指标、avgdist 阈值和 runtime summary 是否一致"}
    };

    auto it = suggestions.find(failureClass);
    if (it != suggestions.end())
        return it->second;
    return "查看 case result_summary.json、tool_display.png、evidence_overlay.png 后再决定是否调参或改算法";
}
}

void CxScriptSuiteReportWriter::WriteSuiteRunReport(
    const std::filesystem::path& outRoot,
    const std::vector<CxScriptSuiteCaseResult>& caseResults)
{
    const std::filesystem::path reportsDir = outRoot / "reports";
    std::filesystem::create_directories(reportsDir);

    const std::filesystem::path reportPath = reportsDir / "suite_run_report.md";
    std::ofstream file(reportPath);

    if (!file.is_open())
        return;

    file << "# Suite Run Report\n\n";
    file << "| Case | Level | Image | Tool | Script | Expected | Points | Fit | PrecisionGate | ResidualPx | ResidualLimitPx | PolicyGuard | ContractPass | Conclusion |\n";
    file << "|------|-------|-------|------|--------|----------|--------|-----|---------------|------------|-----------------|-------------|--------------|------------|\n";

    for (const auto& result : caseResults)
    {
        file << "| " << result.case_id << " | ";
        file << result.level << " | ";
        file << result.image_id << " | ";
        file << result.tool << " | ";
        file << result.script_id << " | ";
        file << result.expected_result << " | ";
        file << result.valid_points_count << " | ";
        file << (result.has_fit_line || result.has_fit_circle ? "yes" : "no") << " | ";
        const CxPrecisionGateSnapshot precision = EvaluateCxPrecisionGate(result);
        file << precision.status << " | ";
        file << std::fixed << std::setprecision(2) << precision.residual_px << " | ";
        file << std::fixed << std::setprecision(2) << precision.residual_limit_px << " | ";
        file << result.actual_policy_guard << " | ";
        file << (result.contract_pass ? "yes" : "no") << " | ";
        file << result.conclusion << " |\n";
    }

    file << "\n## Summary\n\n";
    int passed = 0, failed = 0, executed = 0;
    for (const auto& r : caseResults)
    {
        if (r.headless_ok) executed++;
        if (r.contract_pass) passed++;
        else if (r.headless_ok) failed++;
    }
    file << "- Total cases: " << caseResults.size() << "\n";
    file << "- Executed: " << executed << "\n";
    file << "- Contract pass: " << passed << "\n";
    file << "- Contract fail: " << failed << "\n";
}

void CxScriptSuiteReportWriter::WriteSuiteRunReportJson(
    const std::filesystem::path& outRoot,
    const std::vector<CxScriptSuiteCaseResult>& caseResults)
{
    const std::filesystem::path reportsDir = outRoot / "reports";
    std::filesystem::create_directories(reportsDir);

    const std::filesystem::path reportPath = reportsDir / "suite_run_report.json";
    std::ofstream file(reportPath);

    if (!file.is_open())
        return;

    file << "{\n";
    file << "  \"total_cases\": " << caseResults.size() << ",\n";
    int passed = 0, failed = 0, executed = 0;
    for (const auto& r : caseResults)
    {
        if (r.headless_ok) executed++;
        if (r.contract_pass) passed++;
        else if (r.headless_ok) failed++;
    }
    file << "  \"executed_cases\": " << executed << ",\n";
    file << "  \"contract_pass\": " << passed << ",\n";
    file << "  \"contract_fail\": " << failed << ",\n";
    file << "  \"cases\": [\n";

    for (size_t i = 0; i < caseResults.size(); ++i)
    {
        const auto& r = caseResults[i];
        file << "    {\n";
        file << "      \"case_id\": \"" << r.case_id << "\",\n";
        file << "      \"script_id\": \"" << r.script_id << "\",\n";
        file << "      \"image_id\": \"" << r.image_id << "\",\n";
        file << "      \"level\": \"" << r.level << "\",\n";
        file << "      \"tool\": \"" << r.tool << "\",\n";
        file << "      \"expected_result\": \"" << r.expected_result << "\",\n";
        file << "      \"actual_policy_guard\": \"" << r.actual_policy_guard << "\",\n";
        const CxPrecisionGateSnapshot precision = EvaluateCxPrecisionGate(r);
        file << "      \"precision_metric_family\": \"" << precision.metric_family << "\",\n";
        file << "      \"precision_gate_status\": \"" << precision.status << "\",\n";
        file << "      \"precision_gate_evaluated\": " << (precision.evaluated ? "true" : "false") << ",\n";
        file << "      \"precision_residual_px\": " << precision.residual_px << ",\n";
        file << "      \"precision_residual_limit_px\": " << precision.residual_limit_px << ",\n";
        file << R"(      "precision_residual_pass": )" << (precision.residual_pass ? "true" : "false") << ",\n";
        file << R"(      "precision_support_pass": )" << (precision.support_pass ? "true" : "false") << ",\n";
        file << R"(      "precision_subpixel_offset_mean": )" << precision.subpixel_offset_mean << ",\n";
        file << R"(      "precision_subpixel_offset_stddev": )" << precision.subpixel_offset_stddev << ",\n";
        file << R"(      "precision_localization_sigma_mean_px": )" << precision.localization_sigma_mean_px << ",\n";
        file << R"(      "precision_boundary_residual_rmse_px": )" << precision.residual_rmse_px << ",\n";
        file << R"(      "precision_boundary_residual_p95_px": )" << precision.residual_p95_px << ",\n";
        file << R"(      "precision_boundary_residual_max_px": )" << precision.residual_max_px << ",\n";
        file << R"(      "precision_boundary_reliability_score": )" << precision.reliability_score << ",\n";
        file << "      \"local_support\": " << r.local_support << ",\n";
        file << "      \"local_mean_distance\": " << r.local_mean_distance << ",\n";
        file << "      \"fit_offset\": " << r.fit_offset << ",\n";
        file << "      \"precision_support\": " << precision.support << ",\n";
        file << "      \"precision_support_limit\": " << precision.support_limit << ",\n";
        file << "      \"precision_failure_reason\": \"" << precision.reason << "\",\n";
        file << "      \"contract_pass\": " << (r.contract_pass ? "true" : "false") << ",\n";
        file << "      \"conclusion\": \"" << r.conclusion << "\"\n";
        file << "    }";
        if (i < caseResults.size() - 1) file << ",";
        file << "\n";
    }

    file << "  ]\n";
    file << "}\n";
}

void CxScriptSuiteReportWriter::WriteImageManifestContractReport(
    const std::filesystem::path& outRoot,
    const CxScriptImageManifestRuntime& manifest,
    const CxScriptImageManifestValidationResult& validation)
{
    const std::filesystem::path reportsDir = outRoot / "reports";
    std::filesystem::create_directories(reportsDir);

    const std::filesystem::path reportPath = reportsDir / "image_manifest_contract_report.md";
    std::ofstream file(reportPath);

    if (!file.is_open())
        return;

    file << "# Stage25 Image Manifest Contract Report\n\n";
    file << "- Manifest: " << manifest.manifest_path << "\n";
    file << "- Schema: " << manifest.schema_version << "\n";
    file << "- Images: " << manifest.total_images << "\n";
    file << "- L0: " << manifest.l0_count << "\n";
    file << "- L1: " << manifest.l1_count << "\n";
    file << "- L2: " << manifest.l2_count << "\n";
    file << "- L3: " << manifest.l3_count << "\n";
    file << "- Raw not cropped: required\n";
    file << "- Raw not enhanced: required\n";
    file << "- Raw not rotated: required\n\n";

    file << "## Validation Status: " << (validation.ok ? "PASS" : "FAIL") << "\n\n";

    if (!validation.issues.empty())
    {
        file << "### Issues\n\n";
        file << "| Severity | Image | Target | Message |\n";
        file << "|----------|-------|--------|----------|\n";
        for (const auto& issue : validation.issues)
        {
            file << "| " << issue.severity << " | ";
            file << issue.image_id << " | ";
            file << issue.target_id << " | ";
            file << issue.message << " |\n";
        }
        file << "\n";
    }

    file << "## Image List\n\n";
    file << "| Image | Level | Path | Size | Status |\n";
    file << "|-------|-------|------|------|--------|\n";
    for (const auto& image : manifest.images)
    {
        bool exists = std::filesystem::exists(image.path);
        file << "| " << image.image_id << " | ";
        file << image.level << " | ";
        file << image.path << " | ";
        file << image.width << "x" << image.height << " | ";
        file << (exists ? "OK" : "MISSING") << " |\n";
    }
}

void CxScriptSuiteReportWriter::WriteImageManifestContractReportJson(
    const std::filesystem::path& outRoot,
    const CxScriptImageManifestRuntime& manifest,
    const CxScriptImageManifestValidationResult& validation)
{
    const std::filesystem::path reportsDir = outRoot / "reports";
    std::filesystem::create_directories(reportsDir);

    const std::filesystem::path reportPath = reportsDir / "image_manifest_contract_report.json";
    std::ofstream file(reportPath);

    if (!file.is_open())
        return;

    file << "{\n";
    file << "  \"manifest_path\": \"" << manifest.manifest_path << "\",\n";
    file << "  \"schema_version\": \"" << manifest.schema_version << "\",\n";
    file << "  \"total_images\": " << manifest.total_images << ",\n";
    file << "  \"l0_count\": " << manifest.l0_count << ",\n";
    file << "  \"l1_count\": " << manifest.l1_count << ",\n";
    file << "  \"l2_count\": " << manifest.l2_count << ",\n";
    file << "  \"l3_count\": " << manifest.l3_count << ",\n";
    file << "  \"validation_ok\": " << (validation.ok ? "true" : "false") << ",\n";
    file << "  \"issues\": [\n";

    for (size_t i = 0; i < validation.issues.size(); ++i)
    {
        const auto& issue = validation.issues[i];
        file << "    {\n";
        file << "      \"severity\": \"" << issue.severity << "\",\n";
        file << "      \"image_id\": \"" << issue.image_id << "\",\n";
        file << "      \"target_id\": \"" << issue.target_id << "\",\n";
        file << "      \"message\": \"" << issue.message << "\"\n";
        file << "    }";
        if (i < validation.issues.size() - 1) file << ",";
        file << "\n";
    }

    file << "  ],\n";
    file << "  \"images\": [\n";

    for (size_t i = 0; i < manifest.images.size(); ++i)
    {
        const auto& image = manifest.images[i];
        file << "    {\n";
        file << "      \"image_id\": \"" << image.image_id << "\",\n";
        file << "      \"level\": \"" << image.level << "\",\n";
        file << "      \"path\": \"" << image.path << "\",\n";
        file << "      \"width\": " << image.width << ",\n";
        file << "      \"height\": " << image.height << "\n";
        file << "    }";
        if (i < manifest.images.size() - 1) file << ",";
        file << "\n";
    }

    file << "  ]\n";
    file << "}\n";
}

void CxScriptSuiteReportWriter::WriteBestDetectionGallery(
    const std::filesystem::path& outRoot,
    const std::vector<CxScriptSuiteCaseResult>& caseResults)
{
    const std::filesystem::path reportsDir = outRoot / "reports";
    std::filesystem::create_directories(reportsDir);

    const std::filesystem::path reportPath = reportsDir / "best_detection_gallery.md";
    std::ofstream file(reportPath);

    if (!file.is_open())
        return;

    file << "# Best Detection Gallery\n\n";
    file << "| Level | Image | Tool | Script | Expected | Conclusion | Support | MeanDist | FitOffset |\n";
    file << "|-------|-------|------|--------|----------|------------|---------|----------|-----------|\n";

    for (const auto& result : caseResults)
    {
        if (!result.contract_pass)
            continue;

        file << "| " << result.level << " | ";
        file << result.image_id << " | ";
        file << result.tool << " | ";
        file << result.script_id << " | ";
        file << result.expected_result << " | ";
        file << result.conclusion << " | ";
        file << std::fixed << std::setprecision(2) << result.local_support << " | ";
        file << std::fixed << std::setprecision(2) << result.local_mean_distance << " | ";
        file << std::fixed << std::setprecision(2) << result.fit_offset << " |\n";
    }
}

void CxScriptSuiteReportWriter::WriteToolDisplayIndex(
    const std::filesystem::path& outRoot,
    const std::vector<CxScriptSuiteCaseResult>& caseResults)
{
    const std::filesystem::path reportsDir = outRoot / "reports";
    std::filesystem::create_directories(reportsDir);

    const std::filesystem::path reportPath = reportsDir / "tool_display_index.md";
    std::ofstream file(reportPath);

    if (!file.is_open())
        return;

    file << "# Tool Display Index\n\n";
    file << "| Level | Image | Tool | Script | ResultOverlay | EvidenceOverlay | ToolDisplay |\n";
    file << "|-------|-------|------|--------|---------------|-----------------|-------------|\n";

    for (const auto& result : caseResults)
    {
        file << "| " << result.level << " | ";
        file << result.image_id << " | ";
        file << result.tool << " | ";
        file << result.script_id << " | ";
        file << (!result.result_overlay_path.empty() ? "yes" : "no") << " | ";
        file << (!result.evidence_overlay_path.empty() ? "yes" : "no") << " | ";
        file << (!result.tool_display_path.empty() ? "yes" : "no") << " |\n";
    }
}

void CxScriptSuiteReportWriter::WriteFindLineAlgorithmIterationReport(
    const std::filesystem::path& outRoot,
    const std::vector<CxScriptSuiteCaseResult>& caseResults)
{
    const std::filesystem::path reportsDir = outRoot / "reports";
    std::filesystem::create_directories(reportsDir);

    const std::filesystem::path reportPath = reportsDir / "findline_algorithm_iteration_report.md";
    std::ofstream file(reportPath);

    if (!file.is_open())
        return;

    file << "# FindLine Algorithm Iteration Report\n\n";
    file << "| Level | Image | Target | Script | Runtime Points | Global Points | Runtime Fit | Global Fit | Global Echo | Support | MeanDist | FitOffset | Contract | Status | Conclusion | ToolDisplay |\n";
    file << "|-------|-------|--------|--------|----------------|---------------|-------------|------------|-------------|---------|----------|-----------|----------|--------|------------|-------------|\n";

    int passed = 0, failed = 0;
    for (const auto& result : caseResults)
    {
        if (result.tool != "FindLine")
            continue;

        file << "| " << result.level << " | ";
        file << result.image_id << " | ";
        file << result.target_id << " | ";
        file << result.script_id << " | ";
        file << result.valid_points_count << " | ";
        file << result.global_valid_points_count << " | ";
        file << (result.has_fit_line ? "yes" : "no") << " | ";
        file << (result.global_has_fit_line ? "yes" : "no") << " | ";
        file << (result.runtime_global_result_mismatch ? "mismatch" : "ok") << " | ";
        file << std::fixed << std::setprecision(2) << result.local_support << " | ";
        file << std::fixed << std::setprecision(2) << result.local_mean_distance << " | ";
        file << std::fixed << std::setprecision(2) << result.fit_offset << " | ";
        file << (result.contract_pass ? "yes" : "no") << " | ";
        file << result.contract_status << " | ";
        file << result.conclusion << " | ";
        file << (!result.tool_display_path.empty() ? "yes" : "no") << " |\n";

        if (result.contract_pass) passed++;
        else failed++;
    }

    file << "\n## Summary\n\n";
    file << "- Total FindLine cases: " << (passed + failed) << "\n";
    file << "- Contract pass: " << passed << "\n";
    file << "- Contract fail: " << failed << "\n";
}

void CxScriptSuiteReportWriter::WriteFindCircleAlgorithmIterationReport(
    const std::filesystem::path& outRoot,
    const std::vector<CxScriptSuiteCaseResult>& caseResults)
{
    const std::filesystem::path reportsDir = outRoot / "reports";
    std::filesystem::create_directories(reportsDir);

    const std::filesystem::path reportPath = reportsDir / "findcircle_algorithm_iteration_report.md";
    std::ofstream file(reportPath);

    if (!file.is_open())
        return;

    file << "# FindCircle Algorithm Iteration Report\n\n";
    file << "| Level | Image | Target | Script | Runtime Points | Global Points | Runtime Fit | Global Fit | Global Echo | Radius | AvgDist | PrecisionGate | ResidualLimitPx | Contract | Status | Conclusion | ToolDisplay |\n";
    file << "|-------|-------|--------|--------|----------------|---------------|-------------|------------|-------------|--------|---------|---------------|-----------------|----------|--------|------------|-------------|\n";

    int passed = 0, failed = 0;
    for (const auto& result : caseResults)
    {
        if (result.tool != "FindCircle")
            continue;

        file << "| " << result.level << " | ";
        file << result.image_id << " | ";
        file << result.target_id << " | ";
        file << result.script_id << " | ";
        file << result.valid_points_count << " | ";
        file << result.global_valid_points_count << " | ";
        file << (result.has_fit_circle ? "yes" : "no") << " | ";
        file << (result.global_has_fit_circle ? "yes" : "no") << " | ";
        file << (result.runtime_global_result_mismatch ? "mismatch" : "ok") << " | ";
        file << std::fixed << std::setprecision(2) << result.circle_radius << " | ";
        file << std::fixed << std::setprecision(2) << result.avgdist << " | ";
        const CxPrecisionGateSnapshot precision = EvaluateCxPrecisionGate(result);
        file << precision.status << " | ";
        file << std::fixed << std::setprecision(2) << precision.residual_limit_px << " | ";
        file << (result.contract_pass ? "yes" : "no") << " | ";
        file << result.contract_status << " | ";
        file << result.conclusion << " | ";
        file << (!result.tool_display_path.empty() ? "yes" : "no") << " |\n";

        if (result.contract_pass) passed++;
        else failed++;
    }

    file << "\n## Summary\n\n";
    file << "- Total FindCircle cases: " << (passed + failed) << "\n";
    file << "- Contract pass: " << passed << "\n";
    file << "- Contract fail: " << failed << "\n";
}

void CxScriptSuiteReportWriter::WriteFailureClassificationReport(
    const std::filesystem::path& outRoot,
    const std::vector<CxScriptSuiteCaseResult>& caseResults)
{
    const std::filesystem::path reportsDir = outRoot / "reports";
    std::filesystem::create_directories(reportsDir);

    const std::filesystem::path reportPath = reportsDir / "failure_classification_report.md";
    std::ofstream file(reportPath);

    if (!file.is_open())
        return;

    file << "# Failure Classification Report\n\n";
    file << "## Failure Classification\n\n";
    file << "| Tool | FailureClass | Count | Images | Suggested Next Action |\n";
    file << "|------|--------------|-------|--------|-----------------------|\n";

    std::map<std::string, std::vector<std::string>> failureMap;

    for (const auto& result : caseResults)
    {
        if (result.contract_pass)
            continue;

        const std::string failureClass = DeriveFailureClass(result);
        std::string key = result.tool + ":" + failureClass;
        failureMap[key].push_back(result.image_id);
    }

    for (const auto& [key, images] : failureMap)
    {
        size_t colon = key.find(':');
        std::string tool = key.substr(0, colon);
        std::string status = key.substr(colon + 1);

        std::string imagesList;
        for (size_t i = 0; i < images.size(); ++i)
        {
            if (i > 0) imagesList += ", ";
            imagesList += images[i];
        }

        std::string suggestion = SuggestedActionForFailureClass(status);

        file << "| " << tool << " | ";
        file << status << " | ";
        file << images.size() << " | ";
        file << imagesList << " | ";
        file << suggestion << " |\n";
    }

    file << "\n## Detailed Failure Analysis\n\n";

    for (const auto& result : caseResults)
    {
        if (result.contract_pass)
            continue;

        file << "### " << result.case_id << "\n\n";
        file << "- **Image**: " << result.image_id << "\n";
        file << "- **Target**: " << result.target_id << "\n";
        file << "- **Tool**: " << result.tool << "\n";
        file << "- **Script**: " << result.script_id << "\n";
        file << "- **Level**: " << result.level << "\n";
        const std::string failureClass = DeriveFailureClass(result);
        file << "- **Failure Status**: " << result.contract_status << "\n";
        file << "- **Derived Failure Class**: " << failureClass << "\n";
        file << "- **Conclusion**: " << result.conclusion << "\n";
        file << "- **Failure Stage**: " << result.failure_stage << "\n";
        file << "- **Valid Points**: " << result.valid_points_count << "\n";
        file << "- **Runtime/Global Echo**: " << (result.runtime_global_result_mismatch ? "mismatch" : "ok") << "\n";
        file << "- **Rendered Measure Points**: " << result.rendered_measure_points_count << "\n";
        file << "- **Rendered Result Count**: " << result.rendered_result_count << "\n";
        file << "- **Overlay Changed Pixels**: " << result.result_overlay_changed_pixels << "\n";
        file << "- **Suggested Next Action**: " << SuggestedActionForFailureClass(failureClass) << "\n";

        if (result.tool == "FindLine")
        {
            file << "- **Has Fit Line**: " << (result.has_fit_line ? "yes" : "no") << "\n";
            file << "- **Runtime Points**: " << result.runtime_valid_points_count << "\n";
            file << "- **Global Points**: " << result.global_valid_points_count << "\n";
            file << "- **Runtime Fit Line**: " << (result.runtime_has_fit_line ? "yes" : "no") << "\n";
            file << "- **Global Fit Line**: " << (result.global_has_fit_line ? "yes" : "no") << "\n";
            file << "- **Scan Rows / Foreground Rows**: " << result.scan_rows_examined
                 << " / " << result.scan_rows_with_foreground << "\n";
            file << "- **Scan Runs Total / Within / Over**: " << result.scan_runs_total
                 << " / " << result.scan_runs_within_length_limit
                 << " / " << result.scan_runs_over_length_limit << "\n";
            file << "- **Scan Runs Rejected Selection / Endpoint**: "
                 << result.scan_runs_rejected_by_selection
                 << " / " << result.scan_runs_rejected_near_endpoint << "\n";
            file << "- **Scan Points Emitted**: " << result.scan_points_emitted << "\n";
            file << "- **FindObject Strategy / Branch**: " << result.findobject_strategy_id
                 << " / " << result.findobject_algorithm_branch << "\n";
            file << "- **FindObject Components Accepted / Total / Rejected**: "
                 << result.findobject_component_accepted_count
                 << " / " << result.findobject_component_count
                 << " / " << result.findobject_component_rejected_count << "\n";
            file << "- **FindObject Foreground Before / After**: "
                 << result.findobject_foreground_before
                 << " / " << result.findobject_foreground_after << "\n";
            file << "- **Local Support**: " << std::fixed << std::setprecision(2) << result.local_support << "\n";
            file << "- **Mean Distance**: " << std::fixed << std::setprecision(2) << result.local_mean_distance << "\n";
        }
        else if (result.tool == "FindCircle")
        {
            file << "- **Has Fit Circle**: " << (result.has_fit_circle ? "yes" : "no") << "\n";
            file << "- **Circle Radius**: " << std::fixed << std::setprecision(2) << result.circle_radius << "\n";
            file << "- **Avg Dist**: " << std::fixed << std::setprecision(2) << result.avgdist << "\n";
            const CxPrecisionGateSnapshot precision = EvaluateCxPrecisionGate(result);
            file << "- **Precision Gate**: " << precision.status << "\n";
            file << "- **Precision Residual Limit Px**: " << std::fixed << std::setprecision(2) << precision.residual_limit_px << "\n";
            file << "- **Precision Residual Pass**: " << (precision.residual_pass ? "yes" : "no") << "\n";
            file << "- **Precision Failure Reason**: " << precision.reason << "\n";
            file << "- **Fit Filter Input / Kept / Rejected**: "
                 << result.fit_filter_input_count
                 << " / " << result.fit_filter_kept_count
                 << " / " << result.fit_filter_rejected_count << "\n";
        }

        file << "\n";
    }
}