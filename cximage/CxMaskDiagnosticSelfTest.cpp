#include "CxMaskDiagnosticSelfTest.h"

#include "CxImageReferenceCandidateGenerator.h"
#include "CxUnifiedLog.h"

#include <fstream>
#include <iostream>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace
{
std::string EscapeMaskSelfTestJson(const std::string& value)
{
    std::string escaped;
    for (char ch : value)
    {
        if (ch == '\\' || ch == '"')
            escaped += '\\';
        if (ch == '\n')
        {
            escaped += "\\n";
            continue;
        }
        if (ch == '\r')
            continue;
        escaped += ch;
    }
    return escaped;
}
} // namespace

int RunCxMaskDiagnosticSelfTest(const std::filesystem::path& out_dir)
{
    CXLOG_INFO("CxMaskDiagnostic", "selftest_begin", "running", "synthetic mask diagnostic selftest");
    std::filesystem::create_directories(out_dir);
    const std::filesystem::path image_path = out_dir / "synthetic_input.png";
    const std::filesystem::path left_mask_path = out_dir / "mask_left.png";
    const std::filesystem::path right_mask_path = out_dir / "mask_right_shifted.png";

    cv::Mat image = cv::Mat::zeros(64, 64, CV_8UC3);
    cv::rectangle(image, cv::Rect(16, 16, 32, 32), cv::Scalar(255, 255, 255), cv::FILLED);
    cv::Mat left_mask = cv::Mat::zeros(64, 64, CV_8UC1);
    cv::Mat right_mask = cv::Mat::zeros(64, 64, CV_8UC1);
    cv::rectangle(left_mask, cv::Rect(16, 16, 32, 32), cv::Scalar(255), cv::FILLED);
    cv::rectangle(right_mask, cv::Rect(21, 16, 32, 32), cv::Scalar(255), cv::FILLED);

    const bool inputs_written =
        cv::imwrite(image_path.string(), image) &&
        cv::imwrite(left_mask_path.string(), left_mask) &&
        cv::imwrite(right_mask_path.string(), right_mask);

    CxImageReferenceCandidateRequest candidate_request;
    candidate_request.algorithm_id = "find_segmentation_opencv_smoke";
    candidate_request.input_image_path = image_path;
    candidate_request.output_dir = out_dir / "cximage_candidate";
    CxImageReferenceCandidateResult candidate_result;
    CxImageReferenceCandidateGenerator generator;
    std::string reason;
    const bool candidate_ok = inputs_written &&
        generator.Generate(candidate_request, candidate_result, reason);

    CxMaskFactsSnapshot facts;
    std::string facts_reason;
    const bool facts_ok = AnalyzeCxMaskFile(left_mask_path.string(), facts, facts_reason);

    CxMaskComparisonSnapshot identical;
    std::string identical_reason;
    const bool identical_ok = CompareCxMaskFiles(
        left_mask_path.string(), left_mask_path.string(), identical, identical_reason);

    CxMaskComparisonSnapshot shifted;
    std::string shifted_reason;
    const bool shifted_ok = CompareCxMaskFiles(
        left_mask_path.string(), right_mask_path.string(), shifted, shifted_reason);

    std::string write_reason;
    const bool comparison_assets_ok =
        WriteCxMaskComparisonJson(
            identical, (out_dir / "identical_mask_comparison.json").string(), write_reason) &&
        WriteCxMaskComparisonJson(
            shifted, (out_dir / "shifted_mask_comparison.json").string(), write_reason);

    const bool pass =
        candidate_ok &&
        std::filesystem::is_regular_file(candidate_result.mask_ref) &&
        std::filesystem::is_regular_file(candidate_result.overlay_ref) &&
        std::filesystem::is_regular_file(candidate_result.instances_ref) &&
        facts_ok && facts.component_count == 1 &&
        identical_ok && identical.iou == 1.0 && identical.dice == 1.0 &&
        shifted_ok && shifted.iou > 0.0 && shifted.iou < 1.0 &&
        shifted.boundary_fscore > 0.0 && shifted.boundary_fscore < 1.0 &&
        comparison_assets_ok;

    const std::filesystem::path report_path = out_dir / "mask_diagnostic_selftest_report.json";
    std::ofstream report(report_path, std::ios::trunc);
    report << "{\n"
           << "  \"schema\": \"cxvision.mask_diagnostic_selftest.v1\",\n"
           << "  \"run_id\": \"" << EscapeMaskSelfTestJson(CxUnifiedLog::Instance().RunId()) << "\",\n"
           << "  \"candidate_ok\": " << (candidate_ok ? "true" : "false") << ",\n"
           << "  \"facts_ok\": " << (facts_ok ? "true" : "false") << ",\n"
           << "  \"component_count\": " << facts.component_count << ",\n"
           << "  \"identical_iou\": " << identical.iou << ",\n"
           << "  \"shifted_iou\": " << shifted.iou << ",\n"
           << "  \"shifted_boundary_fscore\": " << shifted.boundary_fscore << ",\n"
           << "  \"conclusion\": \"" << (pass ? "MASK_EVALUATOR_SELFTEST_PASS" : "FAIL") << "\",\n"
           << "  \"reason\": \"" << EscapeMaskSelfTestJson(
                pass ? "synthetic candidate and evaluator checks completed" :
                (!reason.empty() ? reason : (!facts_reason.empty() ? facts_reason :
                (!identical_reason.empty() ? identical_reason : shifted_reason)))) << "\"\n"
           << "}\n";
    const bool report_ok = report.good();
    report.close();

    const bool final_pass = pass && report_ok;
    CXLOG_INFO(
        "CxMaskDiagnostic", "selftest_end",
        final_pass ? "completed" : "failed",
        "conclusion=" + std::string(final_pass ? "MASK_EVALUATOR_SELFTEST_PASS" : "FAIL"));
    std::cout << "mask_diagnostic_selftest_ok=" << (final_pass ? "true" : "false") << "\n"
              << "conclusion=" << (final_pass ? "MASK_EVALUATOR_SELFTEST_PASS" : "FAIL") << "\n"
              << "out_dir=" << out_dir.string() << "\n"
              << "report=" << report_path.string() << "\n"
              << "candidate_mask=" << candidate_result.mask_ref << "\n"
              << "shifted_iou=" << shifted.iou << "\n"
              << "shifted_boundary_fscore=" << shifted.boundary_fscore << "\n";
    return final_pass ? 0 : 1;
}

int RunCxImageReferenceCandidateCli(const CxImageReferenceCandidateCliOptions& options)
{
    if (options.image_path.empty() || options.output_dir.empty() ||
        !std::filesystem::is_regular_file(options.image_path))
    {
        std::cout << "cximage_reference_candidate_ok=false\n"
                  << "conclusion=ASSET_PREFLIGHT_FAIL\n"
                  << "reason=--image must name a readable file and --out is required\n";
        return 2;
    }

    CxImageReferenceCandidateRequest request;
    request.algorithm_id = options.algorithm_id;
    request.input_image_path = options.image_path;
    request.output_dir = options.output_dir;
    request.threshold = options.threshold;
    request.has_roi = options.has_roi;
    request.roi_x0 = options.roi_x0;
    request.roi_y0 = options.roi_y0;
    request.roi_x1 = options.roi_x1;
    request.roi_y1 = options.roi_y1;

    CXLOG_INFO("CxImageReferenceCandidate", "candidate_begin", "running",
               "algorithm_id=" + request.algorithm_id);
    CxImageReferenceCandidateResult result;
    CxImageReferenceCandidateGenerator generator;
    std::string reason;
    const bool ok = generator.Generate(request, result, reason);
    CXLOG_INFO("CxImageReferenceCandidate", "candidate_end", ok ? "completed" : "failed",
               "status=" + result.status + ", reason=" + result.reason);

    std::cout << "cximage_reference_candidate_ok=" << (ok ? "true" : "false") << "\n"
              << "conclusion=" << (ok ? "HEADLESS_EXECUTION_PASS" : "FAIL") << "\n"
              << "status=" << result.status << "\n"
              << "reason=" << result.reason << "\n"
              << "mask_ref=" << result.mask_ref << "\n"
              << "overlay_ref=" << result.overlay_ref << "\n"
              << "instances_ref=" << result.instances_ref << "\n"
              << "summary_ref=" << result.summary_ref << "\n";
    return ok ? 0 : 1;
}
