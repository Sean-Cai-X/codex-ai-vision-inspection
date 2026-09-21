#include "pch.h"
#include "metrology_analytics/CxMetrologyAnalyticsSmoke.h"

#include "metrology_analytics/CxRoughness1D.h"
#include "metrology_analytics/CxBoundaryResponse.h"
#include "metrology_analytics/CxSurfaceAreas.h"
#include "metrology_analytics/CxSurfaceBasicStats.h"
#include "metrology_analytics/CxSurfaceLevelPlane.h"
#include "metrology_analytics/CxSurfaceUnitConversion.h"
#include "metrology_analytics/CxSyntheticSurfaceFactory.h"
#include "metrology_analytics/CxMetrologyUiGlobals.h"
#include "metrology_analytics/CxMetrologyReferenceReplay.h"
#include "metrology_analytics/CxAnalyticsObservationBridgeDraft.h"
#include "metrology_analytics/tests/ManualConsoleAnalyticsSmoke.h"
#include "CxCalibration.h"
#include "FindCircle.h"
#include "FindEllipse.h"
#include "FindLine.h"
#include "Image.h"

#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <opencv2/imgproc.hpp>

namespace cxvision::metrology_analytics
{
namespace
{
constexpr double kPi = 3.141592653589793238462643383279502884;

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

bool Near(double a, double b, double tol)
{
    return std::abs(a - b) <= tol;
}

std::string NewRunId()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmv{};
#if defined(_WIN32)
    localtime_s(&tmv, &t);
#else
    localtime_r(&t, &tmv);
#endif
    std::ostringstream out;
    out << "run_" << std::put_time(&tmv, "%Y%m%d_%H%M%S") << "_metrology_analytics_smoke";
    return out.str();
}

void AddCase(
    CxMetrologyAnalyticsSmokeResult& result,
    const std::string& case_id,
    const std::string& category,
    bool pass,
    double observed,
    double expected,
    double tolerance,
    const std::string& reason)
{
    CxMetrologyAnalyticsSmokeCaseResult c;
    c.case_id = case_id;
    c.category = category;
    c.pass = pass;
    c.observed = observed;
    c.expected = expected;
    c.tolerance = tolerance;
    c.reason = reason;
    result.cases.push_back(c);

    std::cout << "metrology_smoke_case=" << case_id
              << " status=" << (pass ? "pass" : "fail")
              << std::endl;

}

template <typename Fn>
void AddThrowCase(
    CxMetrologyAnalyticsSmokeResult& result,
    const std::string& case_id,
    const std::string& category,
    Fn fn)
{
    bool threw = false;
    std::string reason;
    try
    {
        fn();
        reason = "expected exception was not thrown";
    }
    catch (const std::exception& e)
    {
        threw = true;
        reason = e.what();
    }
    AddCase(result, case_id, category, threw, threw ? 1.0 : 0.0, 1.0, 0.0, reason);
}

double DistributionBcdfAtMean(const CxSurfaceBasicStats& stats)
{
    const auto& h = stats.height_distribution_primary;
    if (h.bin_centers.empty())
        return 0.0;
    std::size_t best = 0;
    double best_dist = std::abs(h.bin_centers[0] - stats.mean);
    for (std::size_t i = 1; i < h.bin_centers.size(); ++i)
    {
        const double d = std::abs(h.bin_centers[i] - stats.mean);
        if (d < best_dist)
        {
            best = i;
            best_dist = d;
        }
    }
    return best < h.bcdf.size() ? h.bcdf[best] : 0.0;
}

CxProfile1D MakeSineProfile(double amplitude, int samples)
{
    CxProfile1D p;
    p.delta_x_physical = 1.0;
    p.z.reserve(static_cast<std::size_t>(samples));
    for (int i = 0; i < samples; ++i)
        p.z.push_back(amplitude * std::sin(2.0 * kPi * static_cast<double>(i) / static_cast<double>(samples)));
    return p;
}

CxProfile1D MakeGaussianProfile(int samples, double sigma, std::uint64_t seed)
{
    std::mt19937_64 rng(seed);
    std::normal_distribution<double> dist(0.0, sigma);
    CxProfile1D p;
    p.delta_x_physical = 1.0;
    p.z.reserve(static_cast<std::size_t>(samples));
    for (int i = 0; i < samples; ++i)
        p.z.push_back(dist(rng));
    return p;
}

CxProfile1D MakeRampProfile(int samples)
{
    CxProfile1D p;
    p.delta_x_physical = 1.0;
    p.z.reserve(static_cast<std::size_t>(samples));
    for (int i = 0; i < samples; ++i)
        p.z.push_back(static_cast<double>(i));
    return p;
}

std::vector<float> MakeStepWaveform(int samples, int edge, bool rising,
                                    float low = 0.0f, float high = 1.0f)
{
    std::vector<float> values(static_cast<std::size_t>(samples), low);
    for (int i = std::max(0, edge); i < samples; ++i)
        values[static_cast<std::size_t>(i)] = rising ? high : low;
    if (!rising)
    {
        for (int i = 0; i < std::min(samples, edge); ++i)
            values[static_cast<std::size_t>(i)] = high;
    }
    return values;
}

std::vector<float> MakeCompetingFallingEdgesWaveform()
{
    std::vector<float> values(160, 0.15f);
    for (int i = 0; i < 77; ++i)
        values[static_cast<std::size_t>(i)] = 0.72f;
    for (int i = 77; i < 106; ++i)
        values[static_cast<std::size_t>(i)] = 0.25f;
    for (int i = 106; i < 121; ++i)
        values[static_cast<std::size_t>(i)] = 1.0f;
    for (int i = 121; i < 160; ++i)
        values[static_cast<std::size_t>(i)] = 0.0f;
    return values;
}

std::vector<float> MakeInternalBrightPlateauWaveform()
{
    std::vector<float> values(160, 0.10f);
    for (int i = 20; i < 82; ++i)
        values[static_cast<std::size_t>(i)] = 0.86f;
    return values;
}

double SelectedBoundaryPosition(const CxBoundaryResponseResult& result)
{
    if (result.selected_candidate < 0 ||
        result.selected_candidate >= static_cast<int>(result.candidates.size()))
        return -1.0;
    return result.candidates[static_cast<std::size_t>(result.selected_candidate)]
        .position_samples;
}

cv::Mat MakeVerticalStepImage(int width, int height, int edge_x, bool falling)
{
    cv::Mat image(height, width, CV_8UC3, cv::Scalar(0, 0, 0));
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const bool left_side = x < edge_x;
            const int value = (left_side == falling) ? 210 : 40;
            image.at<cv::Vec3b>(y, x) =
                cv::Vec3b(static_cast<uchar>(value),
                          static_cast<uchar>(value),
                          static_cast<uchar>(value));
        }
    }
    cv::GaussianBlur(image, image, cv::Size(5, 5), 1.0);
    return image;
}

cv::Mat MakeCircleStepImage(int width, int height, int cx, int cy, int radius)
{
    cv::Mat image(height, width, CV_8UC3, cv::Scalar(35, 35, 35));
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const int dx = x - cx;
            const int dy = y - cy;
            if (dx * dx + dy * dy <= radius * radius)
                image.at<cv::Vec3b>(y, x) = cv::Vec3b(215, 215, 215);
        }
    }
    cv::GaussianBlur(image, image, cv::Size(5, 5), 1.0);
    return image;
}

cv::Mat MakeEllipseStepImage(int width, int height, int cx, int cy, int rx, int ry)
{
    cv::Mat image(height, width, CV_8UC3, cv::Scalar(35, 35, 35));
    cv::ellipse(image,
                cv::Point(cx, cy),
                cv::Size(rx, ry),
                0.0,
                0.0,
                360.0,
                cv::Scalar(215, 215, 215),
                cv::FILLED);
    cv::GaussianBlur(image, image, cv::Size(5, 5), 1.0);
    return image;
}

bool RunFindLineBoundaryTriggerSmoke(double& observed, std::string& reason)
{
    Image image;
    image.copyFromMat(MakeVerticalStepImage(128, 96, 64, true));

    FindLine line;
    line.setline(16, 48, 112, 48, 16);
    line.SetWHgap(4, 4);
    line.setlinegap(2);
    line.setobjfilter(0);
    line.setpointconsistency(0, 0);
    line.setboundaryresponsemode(
        static_cast<int>(CxBoundaryResponseMode::NegativeStep));
    line.setboundarypolarity(
        static_cast<int>(CxBoundaryPolarity::Falling));
    line.setboundarydenoise(
        static_cast<int>(CxBoundaryDenoiseMode::Gaussian));
    line.setboundarysmoothingradius(1);
    line.setboundarythresholdpermille(50);
    line.setboundaryminamplitudepermille(20);
    line.measure(&image);

    const FindLineMeasureInputDebug& debug = line.lastmeasureinputdebug();
    observed = static_cast<double>(debug.boundary_response_points_emitted);

    if (!debug.boundary_response_enabled)
    {
        reason = "FindLine BoundaryResponse was not enabled";
        return false;
    }
    if (debug.boundary_response_scan_rows_evaluated <= 0)
    {
        reason = "FindLine BoundaryResponse evaluated no scan rows";
        return false;
    }
    if (debug.boundary_response_candidate_count <= 0)
    {
        reason = "FindLine BoundaryResponse produced no candidates";
        return false;
    }
    if (debug.boundary_response_points_emitted < 2)
    {
        reason = "FindLine BoundaryResponse emitted fewer than two points";
        return false;
    }
    if ((line.getresultpointsw().size() + line.getresultpointsh().size()) <= 0)
    {
        reason = "FindLine BoundaryResponse emitted diagnostics but no measure points";
        return false;
    }

    int accepted_scan_diagnostics = 0;
    double maximum_projection_error = 0.0;
    for (int i = 0; i < line.getscandiagnosticcount(); ++i)
    {
        FindLineMeasureInputDebug::ScanDiagnostic diag;
        if (line.getscandiagnostic(i, diag) &&
            diag.boundary_response_used &&
            diag.accepted)
        {
            ++accepted_scan_diagnostics;
            const int sampleCount = diag.scan_type == 0
                ? debug.original_scan_w_length
                : debug.original_scan_h_length;
            CxShapePoint projected;
            const double normalizedPosition = sampleCount > 1
                ? diag.boundary_response_selected_position /
                    static_cast<double>(sampleCount - 1)
                : -1.0;
            if (!line.getscanlineprofilepoint(
                    diag.scan_type, diag.scan_index, normalizedPosition,
                    projected))
            {
                reason = "FindLine profile-to-image projection was unavailable";
                return false;
            }
            const double error = std::hypot(
                projected.x - diag.accepted_x, projected.y - diag.accepted_y);
            maximum_projection_error = std::max(maximum_projection_error, error);
        }
    }
    if (accepted_scan_diagnostics <= 0)
    {
        reason = "FindLine BoundaryResponse accepted no scan diagnostic rows";
        return false;
    }
    if (maximum_projection_error > 1.0e-6)
    {
        reason = "FindLine profile-to-image projection diverged from accepted "
                 "diagnostic point by " + std::to_string(maximum_projection_error);
        return false;
    }

    reason = "FindLine BoundaryResponse emits real measure points and profile-to-image "
             "projection matches accepted diagnostics";
    return true;
}

bool RunFindCircleBoundaryTriggerSmoke(double& observed, std::string& reason)
{
    Image image;
    image.copyFromMat(MakeCircleStepImage(128, 128, 64, 64, 34));

    FindCircle circle;
    circle.Setgap(4);
    circle.setcircle(64, 64, 98, 64);
    circle.setlinegap(3);
    circle.setfindsetting(0);
    circle.setpointconsistency(0, 0);
    circle.setboundaryresponsemode(
        static_cast<int>(CxBoundaryResponseMode::NegativeStep));
    circle.setboundarypolarity(
        static_cast<int>(CxBoundaryPolarity::Falling));
    circle.setboundarydenoise(
        static_cast<int>(CxBoundaryDenoiseMode::Gaussian));
    circle.setboundarysmoothingradius(1);
    circle.setboundarythresholdpermille(50);
    circle.setboundaryminamplitudepermille(20);
    circle.setboundaryminplateauwidth(1);
    circle.Measure(image);
    circle.fitcircle();

    const FindCircleMeasureGeometryDebug& debug =
        circle.lastmeasuregeometrydebug();
    observed = static_cast<double>(debug.boundary_response_points_emitted);

    if (debug.boundary_response_enabled != 1)
    {
        reason = "FindCircle boundary response was not enabled";
        return false;
    }
    if (debug.boundary_response_scan_lines_evaluated <= 0)
    {
        reason = "FindCircle boundary response evaluated no scan lines";
        return false;
    }
    if (debug.boundary_response_candidate_count <= 0)
    {
        reason = "FindCircle boundary response found no candidates";
        return false;
    }
    if (debug.boundary_response_points_emitted < 8)
    {
        reason = "FindCircle boundary response emitted too few points";
        return false;
    }
    if (circle.getvalidpointcount() < 8)
    {
        reason = "FindCircle measure point count below fit minimum";
        return false;
    }
    if (!circle.hasfitresult())
    {
        reason = "FindCircle boundary response did not produce fit circle";
        return false;
    }

    const double radius_error = std::abs(circle.getradius() - 34.0);
    if (radius_error > 4.0)
    {
        reason = "FindCircle boundary response radius error too large: " +
            std::to_string(radius_error);
        return false;
    }

    reason = "FindCircle BoundaryResponse emits radial measure points and fit";
    return true;
}

bool RunFindEllipseBoundaryTriggerSmoke(double& observed, std::string& reason)
{
    Image image;
    image.copyFromMat(MakeEllipseStepImage(160, 128, 80, 64, 42, 26));

    FindEllipse ellipse;
    ellipse.setbboxx0(38);
    ellipse.setbboxy0(38);
    ellipse.setbboxx1(122);
    ellipse.setbboxy1(90);
    ellipse.buildbbox();
    ellipse.setinnerpercent(15);
    ellipse.Setgap(4);
    ellipse.setlinegap(3);
    ellipse.setthre(8);
    ellipse.setmethod(0);
    ellipse.setfindsetting(0);
    ellipse.setselectedgenum(0);
    ellipse.setpointconsistency(0, 0);
    ellipse.setboundaryresponsemode(
        static_cast<int>(CxBoundaryResponseMode::NegativeStep));
    ellipse.setboundarypolarity(
        static_cast<int>(CxBoundaryPolarity::Falling));
    ellipse.setboundarydenoise(
        static_cast<int>(CxBoundaryDenoiseMode::Gaussian));
    ellipse.setboundarysmoothingradius(1);
    ellipse.setboundarythresholdpermille(50);
    ellipse.setboundaryminamplitudepermille(20);
    ellipse.setboundaryminplateauwidth(1);
    ellipse.measure(&image);
    ellipse.fitellipse();

    FindEllipseDisplaySnapshot snapshot;
    if (!ellipse.getdisplaysnapshot(snapshot))
    {
        reason = "FindEllipse display snapshot was unavailable";
        return false;
    }

    observed = static_cast<double>(snapshot.boundary_response_points_emitted);

    if (snapshot.boundary_response_enabled != 1)
    {
        reason = "FindEllipse boundary response was not enabled";
        return false;
    }
    if (snapshot.boundary_response_scan_lines_evaluated <= 0)
    {
        reason = "FindEllipse boundary response evaluated no scan lines";
        return false;
    }
    if (snapshot.boundary_response_candidate_count <= 0)
    {
        reason = "FindEllipse boundary response found no candidates";
        return false;
    }
    if (snapshot.boundary_response_points_emitted < 8)
    {
        reason = "FindEllipse boundary response emitted too few points";
        return false;
    }
    if (ellipse.getvalidpointcount() < 8)
    {
        reason = "FindEllipse measure point count below fit minimum";
        return false;
    }
    if (ellipse.hasfitresult() == 0.0)
    {
        reason = "FindEllipse boundary response did not produce fit ellipse";
        return false;
    }

    const double rx_error =
        std::min(std::abs(ellipse.getresultradiusx() - 42.0),
                 std::abs(ellipse.getresultradiusy() - 42.0));
    const double ry_error =
        std::min(std::abs(ellipse.getresultradiusx() - 26.0),
                 std::abs(ellipse.getresultradiusy() - 26.0));
    if (rx_error > 6.0 || ry_error > 6.0)
    {
        reason = "FindEllipse boundary response radius error too large: rx=" +
            std::to_string(rx_error) + " ry=" + std::to_string(ry_error);
        return false;
    }

    reason = "FindEllipse BoundaryResponse emits radial measure points and fit";
    return true;
}

bool WriteSummary(const std::filesystem::path& out_dir, CxMetrologyAnalyticsSmokeResult& result, std::string& reason)
{
    std::error_code ec;
    std::filesystem::create_directories(out_dir, ec);
    if (ec)
    {
        reason = "failed to create output dir: " + ec.message();
        return false;
    }

    result.summary_path = out_dir / "metrology_analytics_smoke_summary.json";
    result.report_path = out_dir / "metrology_analytics_smoke_report.md";
    const std::filesystem::path uiGlobalsPath =
        out_dir / "metrology_ui_globals_snapshot.json";

    std::string uiSnapshotReason;
    if (!WriteMetrologyUiGlobalSnapshotJson(
            uiGlobalsPath,
            DefaultMetrologyUiGlobalFields(),
            uiSnapshotReason))
    {
        reason = uiSnapshotReason;
        return false;
    }

    {
        std::ofstream f(result.summary_path, std::ios::binary | std::ios::trunc);
        if (!f.is_open())
        {
            reason = "failed to open summary json";
            return false;
        }
        f << "{\n";
        f << "  \"schema\": \"cxvision.metrology_analytics_smoke.v1\",\n";
        f << "  \"run_id\": \"" << JsonEscape(result.run_id) << "\",\n";
        f << "  \"total_cases\": " << result.total_cases << ",\n";
        f << "  \"pass_count\": " << result.pass_count << ",\n";
        f << "  \"fail_count\": " << result.fail_count << ",\n";
        f << "  \"conclusion\": \"" << (result.fail_count == 0 ? "METROLOGY_ANALYTICS_SMOKE_PASS" : "METROLOGY_ANALYTICS_SMOKE_FAIL") << "\",\n";
        f << "  \"metrology_ui_globals_snapshot_path\": \"" << JsonEscape(uiGlobalsPath.string()) << "\",\n";
        f << "  \"metrology_reference_replay_summary_path\": \"" << JsonEscape((out_dir / "metrology_reference_replay_summary.json").string()) << "\",\n";
        f << "  \"metrology_reference_replay_report_path\": \"" << JsonEscape((out_dir / "metrology_reference_replay_report.md").string()) << "\",\n";
        f << "  \"cases\": [\n";
        for (std::size_t i = 0; i < result.cases.size(); ++i)
        {
            const auto& c = result.cases[i];
            f << "    {\"case_id\":\"" << JsonEscape(c.case_id)
              << "\", \"category\":\"" << JsonEscape(c.category)
              << "\", \"pass\":" << (c.pass ? "true" : "false")
              << ", \"observed\":" << c.observed
              << ", \"expected\":" << c.expected
              << ", \"tolerance\":" << c.tolerance
              << ", \"reason\":\"" << JsonEscape(c.reason) << "\"}"
              << (i + 1 < result.cases.size() ? "," : "") << "\n";
        }
        f << "  ]\n";
        f << "}\n";
    }

    {
        std::ofstream f(result.report_path, std::ios::binary | std::ios::trunc);
        if (!f.is_open())
        {
            reason = "failed to open report md";
            return false;
        }
        f << "# Metrology Analytics Smoke Report\n\n";
        f << "- run_id: " << result.run_id << "\n";
        f << "- total_cases: " << result.total_cases << "\n";
        f << "- pass_count: " << result.pass_count << "\n";
        f << "- fail_count: " << result.fail_count << "\n";
        f << "- conclusion: " << (result.fail_count == 0 ? "METROLOGY_ANALYTICS_SMOKE_PASS" : "METROLOGY_ANALYTICS_SMOKE_FAIL") << "\n\n";
        f << "- metrology_ui_globals_snapshot: " << uiGlobalsPath.string() << "\n\n";
        f << "- metrology_reference_replay_summary: " << (out_dir / "metrology_reference_replay_summary.json").string() << "\n";
        f << "- metrology_reference_replay_report: " << (out_dir / "metrology_reference_replay_report.md").string() << "\n\n";
        f << "| Case | Category | Pass | Observed | Expected | Tol | Reason |\n";
        f << "|---|---|---:|---:|---:|---:|---|\n";
        for (const auto& c : result.cases)
        {
            f << "| " << c.case_id
              << " | " << c.category
              << " | " << (c.pass ? 1 : 0)
              << " | " << c.observed
              << " | " << c.expected
              << " | " << c.tolerance
              << " | " << c.reason
              << " |\n";
        }
    }

    reason.clear();
    return true;
}
}

bool RunMetrologyAnalyticsSmoke(
    const std::filesystem::path& output_dir,
    CxMetrologyAnalyticsSmokeResult& result,
    std::string& reason)
{
    result = {};
    result.run_id = NewRunId();

    std::error_code progressEc;
    std::filesystem::create_directories(output_dir, progressEc);
    std::ofstream progress(
        output_dir / "metrology_analytics_smoke_progress.log",
        std::ios::binary | std::ios::trunc);
    if (progress.is_open())
    {
        progress << "run_id=" << result.run_id << "\n";
        progress << "stage=run_begin\n";
        progress.flush();
    }


    try
    {
        CxPhysUnit pixel;
        AddCase(result, "phys_unit_roundtrip", "unit", pixel == CxPhysUnit{}, 1.0, 1.0, 0.0, "default unit equality is stable");

        CxSurfaceField paraboloid(3, 3, pixel);
        paraboloid.fillFromGenerator([](int x, int y) { return static_cast<double>(x * x + y * y); });
        AddCase(result, "field_generator_x2y2", "field", Near(paraboloid.at(1, 1), 2.0, 1e-15), paraboloid.at(1, 1), 2.0, 1e-15, "fillFromGenerator computes z=x^2+y^2");

        CxSurfaceField flat10 = CxSyntheticSurfaceFactory::flat(10, 10, 5.0);
        CxSurfaceAreaResult flatArea = computeProjectedAndSurfaceArea(flat10);
        AddCase(result, "flat_area_ratio_10", "area", Near(flatArea.area_ratio, 1.0, 1e-12), flatArea.area_ratio, 1.0, 1e-12, "flat surface area equals projected area");

        CxSurfaceField saddle(2, 2, pixel);
        saddle.setAt(0, 0, 0.0);
        saddle.setAt(1, 0, 1.0);
        saddle.setAt(0, 1, 1.0);
        saddle.setAt(1, 1, 0.0);
        CxSurfaceAreaResult saddleArea = computeProjectedAndSurfaceArea(saddle);
        AddCase(result, "saddle_2x2_surface_gt_projected", "area", saddleArea.surface_area > saddleArea.projected_area, saddleArea.surface_area, saddleArea.projected_area, 0.0, "non-flat saddle has larger surface area than projection");

        CxSurfaceBasicStats flatStats = computeSurfaceBasicStats(flat10);
        AddCase(result, "flat_stats_mean", "stats", Near(flatStats.mean, 5.0, 1e-12), flatStats.mean, 5.0, 1e-12, "flat mean is exact");
        AddCase(result, "flat_stats_rms_zero", "stats", Near(flatStats.rms, 0.0, 1e-12), flatStats.rms, 0.0, 1e-12, "flat rms deviation is zero");

        CxSurfaceField sine = CxSyntheticSurfaceFactory::sine1D(4, 1, 1.0, 4.0);
        AddCase(result, "sine_sample_x1", "synthetic", Near(sine.at(1, 0), 1.0, 1e-12), sine.at(1, 0), 1.0, 1e-12, "lambda=4 sine has z(1)=1");
        AddCase(result, "sine_sample_x3", "synthetic", Near(sine.at(3, 0), -1.0, 1e-12), sine.at(3, 0), -1.0, 1e-12, "lambda=4 sine has z(3)=-1");

        CxRoughness1DResult sineR = computeProfileRoughness(MakeSineProfile(1.0, 10000));
        AddCase(result, "sine_roughness_Ra", "roughness", Near(sineR.Ra, 2.0 / kPi, 1e-3), sineR.Ra, 2.0 / kPi, 1e-3, "sine Ra matches analytic 2A/pi");
        AddCase(result, "sine_roughness_Rq", "roughness", Near(sineR.Rq, 1.0 / std::sqrt(2.0), 1e-3), sineR.Rq, 1.0 / std::sqrt(2.0), 1e-3, "sine Rq matches analytic A/sqrt(2)");

        CxSurfaceField g = CxSyntheticSurfaceFactory::gaussianRandom(1024, 1024, 0.0, 1.0, 42);
        CxSurfaceBasicStats g64 = computeSurfaceBasicStats(g, 64);
        CxSurfaceBasicStats g256 = computeSurfaceBasicStats(g, 256);
        CxSurfaceBasicStats g1024 = computeSurfaceBasicStats(g, 1024);
        AddCase(result, "gaussian_stats_mean_64", "stats", std::abs(g64.mean) < 0.005, g64.mean, 0.0, 0.005, "gaussian mean in expected window");
        AddCase(result, "gaussian_stats_rms_256", "stats", g256.rms > 0.995 && g256.rms < 1.005, g256.rms, 1.0, 0.005, "gaussian sigma in expected window");
        AddCase(result, "gaussian_stats_kurtosis_1024", "stats", std::abs(g1024.kurtosis_excess) < 0.10, g1024.kurtosis_excess, 0.0, 0.10, "gaussian excess kurtosis near zero");

        CxSurfaceField b = CxSyntheticSurfaceFactory::bimodal(512, 512, 0.5, -3.0, 1.0, 3.0, 1.0, 7);
        CxSurfaceBasicStats bs = computeSurfaceBasicStats(b, 256);
        AddCase(result, "bimodal_skew_near_zero", "stats", std::abs(bs.skewness) < 0.08, bs.skewness, 0.0, 0.08, "symmetric bimodal skewness near zero");
        AddCase(result, "bimodal_kurtosis_negative", "stats", bs.kurtosis_excess < 0.0, bs.kurtosis_excess, -1.0, 0.0, "wide bimodal distribution has negative excess kurtosis");
        AddCase(result, "bimodal_bcdf_at_mean", "stats", Near(DistributionBcdfAtMean(bs), 0.5, 0.03), DistributionBcdfAtMean(bs), 0.5, 0.03, "symmetric bimodal BCDF around mean");

        CxHeightDistribution peakDistribution;
        peakDistribution.bin_centers =
            {0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0};
        peakDistribution.adf =
            {0.0, 1.0, 4.0, 3.0, 0.5, 1.0, 5.0, 0.0};
        peakDistribution.bcdf.assign(
            peakDistribution.adf.size(), 0.0);
        CxHeightPeakOptions peakOptions;
        peakOptions.max_peaks = 2;
        peakOptions.min_prominence = 0.5;
        peakOptions.min_distance_bins = 2;
        peakOptions.order = CxHeightPeakOrder::Position;
        peakOptions.background = CxHeightPeakBackground::Zero;
        const std::vector<CxHeightPeak> detectedPeaks =
            findHeightDistributionPeaks(peakDistribution, peakOptions);
        AddCase(
            result, "height_peaks_count", "height_peaks",
            detectedPeaks.size() == 2,
            static_cast<double>(detectedPeaks.size()), 2.0, 0.0,
            "height distribution returns the requested two separated peaks");
        AddCase(
            result, "height_peaks_sub_bin_position", "height_peaks",
            !detectedPeaks.empty() &&
                Near(detectedPeaks.front().position, 2.25, 1e-12),
            detectedPeaks.empty() ? 0.0 : detectedPeaks.front().position,
            2.25, 1e-12,
            "quadratic interpolation localizes the first peak at sub-bin precision");

        CxHeightDistribution valleyDistribution;
        valleyDistribution.bin_centers = {0.0, 1.0, 2.0, 3.0, 4.0};
        valleyDistribution.adf = {5.0, 4.0, 1.0, 3.0, 5.0};
        valleyDistribution.bcdf.assign(
            valleyDistribution.adf.size(), 0.0);
        peakOptions.max_peaks = 1;
        peakOptions.invert = true;
        const std::vector<CxHeightPeak> detectedValleys =
            findHeightDistributionPeaks(valleyDistribution, peakOptions);
        AddCase(
            result, "height_peaks_invert_valley", "height_peaks",
            detectedValleys.size() == 1 &&
                Near(detectedValleys.front().curve_value, 1.0, 1e-12),
            detectedValleys.empty() ? 0.0
                                    : detectedValleys.front().curve_value,
            1.0, 1e-12,
            "invert mode detects a valley while preserving the original curve value");

        CxPhysUnit mm;
        mm.x_unit = CxLengthUnit::Millimeter;
        mm.y_unit = CxLengthUnit::Millimeter;
        mm.z_unit = CxLengthUnit::Millimeter;
        mm.x_scale_per_pixel = 0.01;
        mm.y_scale_per_pixel = 0.01;
        mm.z_scale_per_pixel = 1.0;
        CxSurfaceField plane = CxSyntheticSurfaceFactory::plane(80, 60, 2.0, 3.0, 5.0, mm);
        CxPlaneCoeffs p = fitPlane(plane, PlaneLevelMethod::OrdinaryLeastSquares);
        AddCase(result, "plane_fit_a", "level_plane", Near(p.a, 2.0, 1e-5), p.a, 2.0, 1e-5, "OLS plane coefficient a");
        AddCase(result, "plane_fit_b", "level_plane", Near(p.b, 3.0, 1e-5), p.b, 3.0, 1e-5, "OLS plane coefficient b");
        subtractPlaneInPlace(plane, p);
        AddCase(result, "plane_subtract_rms", "level_plane", computeSurfaceBasicStats(plane).rms < 1e-5, computeSurfaceBasicStats(plane).rms, 0.0, 1e-5, "plane subtraction leaves near-zero residual");

        AddCase(result, "unit_mm_to_um", "unit", Near(convertLength(1.0, CxLengthUnit::Millimeter, CxLengthUnit::Micrometer), 1000.0, 1e-12), convertLength(1.0, CxLengthUnit::Millimeter, CxLengthUnit::Micrometer), 1000.0, 1e-12, "1 mm = 1000 um");
        AddCase(result, "unit_width_mm", "unit", Near(CxSurfaceField(1000, 1, mm).physicalWidth(), 10.0, 1e-12), CxSurfaceField(1000, 1, mm).physicalWidth(), 10.0, 1e-12, "1000 px at 0.01 mm/px is 10 mm");
        AddCase(result, "uncertainty_missing_pixel", "unit", deriveStatusFromUnit(pixel, false) == CxUncertaintyStatus::Missing, static_cast<double>(static_cast<int>(deriveStatusFromUnit(pixel, false))), static_cast<double>(static_cast<int>(CxUncertaintyStatus::Missing)), 0.0, "pixel unit without calibration is missing uncertainty");
        AddCase(result, "uncertainty_incomplete_z", "unit", deriveStatusFromUnit(mm, false) == CxUncertaintyStatus::IncompleteZOnly, static_cast<double>(static_cast<int>(deriveStatusFromUnit(mm, false))), static_cast<double>(static_cast<int>(CxUncertaintyStatus::IncompleteZOnly)), 0.0, "physical unit without z repeatability is incomplete");

        CxCalibration calibration;
        AddCase(
            result,
            "calibration_default_empty",
            "calibration",
            calibration.status_code() == CxCalibrationStatusCode::Empty &&
                !calibration.hasxytransform() &&
                !calibration.haszscale(),
            static_cast<double>(static_cast<int>(calibration.status_code())),
            static_cast<double>(static_cast<int>(CxCalibrationStatusCode::Empty)),
            0.0,
            "CxCalibration has a typed empty state before values are bound");
        calibration.setsource("smoke.synthetic.calibration");
        calibration.setunits("um", "um", "um");
        calibration.setxytransform(2.0, 3.0, 10.0, 20.0, 0.0);
        calibration.setzscale(0.5, 1.0);
        const cv::Point2d physical = calibration.pixelToPhysical(4.0, 5.0);
        AddCase(
            result,
            "calibration_xy_forward",
            "calibration",
            Near(physical.x, 18.0, 1e-12) && Near(physical.y, 35.0, 1e-12),
            physical.x + physical.y,
            53.0,
            1e-12,
            "CxCalibration converts pixel XY through scale and offset");
        const cv::Point2d pixelBack = calibration.physicalToPixel(physical.x, physical.y);
        AddCase(
            result,
            "calibration_xy_inverse",
            "calibration",
            Near(pixelBack.x, 4.0, 1e-12) && Near(pixelBack.y, 5.0, 1e-12),
            pixelBack.x + pixelBack.y,
            9.0,
            1e-12,
            "CxCalibration inverse transform round-trips XY");
        AddCase(
            result,
            "calibration_z_scale",
            "calibration",
            Near(calibration.zToPhysical(8.0), 5.0, 1e-12),
            calibration.zToPhysical(8.0),
            5.0,
            1e-12,
            "CxCalibration converts Z through scale and offset");
        const CxCalibrationSnapshot calibrationSnapshot = calibration.snapshot();
        AddCase(
            result,
            "calibration_snapshot_typed",
            "calibration",
            calibrationSnapshot.has_xy_transform &&
                calibrationSnapshot.has_z_scale &&
                calibrationSnapshot.status == "READY" &&
                calibrationSnapshot.source_ref == "smoke.synthetic.calibration",
            calibrationSnapshot.has_xy_transform && calibrationSnapshot.has_z_scale ? 1.0 : 0.0,
            1.0,
            0.0,
            "CxCalibration exposes a value snapshot suitable for evidence and future case contracts");
        AddCase(
            result,
            "calibration_snapshot_hash",
            "calibration",
            !calibrationSnapshot.snapshot_hash.empty() &&
                calibrationSnapshot.snapshot_hash.find("fnv1a64:") == 0,
            calibrationSnapshot.snapshot_hash.empty() ? 0.0 : 1.0,
            1.0,
            0.0,
            "CxCalibration snapshot has a stable evidence hash");

        CxCalibration rotationCalibration;
        rotationCalibration.setxytransform(1.0, 1.0, 0.0, 0.0, 90.0);
        const cv::Point2d rotated = rotationCalibration.pixelToPhysical(1.0, 0.0);
        AddCase(
            result,
            "calibration_xy_rotation_90",
            "calibration",
            Near(rotated.x, 0.0, 1e-12) && Near(rotated.y, 1.0, 1e-12),
            rotated.x + rotated.y,
            1.0,
            1e-12,
            "CxCalibration applies global XY rotation");

        CxCalibrationAdapter calibrationAdapter(calibrationSnapshot);
        AddCase(
            result,
            "calibration_adapter_snapshot_roundtrip",
            "calibration",
            calibrationAdapter.ready() &&
                calibrationAdapter.snapshot().snapshot_hash == calibrationSnapshot.snapshot_hash &&
                Near(calibrationAdapter.zToPhysical(8.0), 5.0, 1e-12),
            calibrationAdapter.ready() ? 1.0 : 0.0,
            1.0,
            0.0,
            "CxCalibrationAdapter binds a value snapshot without retaining legacy pointers");

        CxCalibration uncertaintyCalibration;
        uncertaintyCalibration.setuncertainty(0.1, 0.2, 0.3, 0.4, 0.5);
        const double propagated =
            uncertaintyCalibration.propagateLinearUncertainty({ 3.0, 4.0 }, { 0.1, 0.2 });
        AddCase(
            result,
            "calibration_linear_uncertainty",
            "calibration",
            Near(propagated, std::sqrt(0.73), 1e-12) &&
                uncertaintyCalibration.uncertainty_status() == CxCalibrationUncertaintyStatus::Complete,
            propagated,
            std::sqrt(0.73),
            1e-12,
            "CxCalibration supports simple value-semantic uncertainty propagation");

        CxRoughness1DResult gr = computeProfileRoughness(MakeGaussianProfile(100000, 1.0, 123));
        AddCase(result, "roughness_gaussian_Rku", "roughness", gr.Rku_std > 2.9 && gr.Rku_std < 3.1, gr.Rku_std, 3.0, 0.1, "gaussian profile Rku_std near 3");

        CxRoughness1DResult ramp = computeProfileRoughness(MakeRampProfile(10000));
        AddCase(result, "roughness_ramp_positive", "roughness", ramp.Ra > 0.0 && ramp.Rq > ramp.Ra, ramp.Rq, ramp.Ra, 0.0, "ramp roughness produces positive Ra/Rq");

        CxBoundaryResponseConfig boundaryConfig;
        boundaryConfig.baseline_mode = CxBoundaryBaselineMode::Offset;
        boundaryConfig.denoise_mode = CxBoundaryDenoiseMode::None;
        boundaryConfig.response_mode = CxBoundaryResponseMode::NegativeStep;
        boundaryConfig.polarity = CxBoundaryPolarity::Falling;
        boundaryConfig.wavelet_scale = 1;
        boundaryConfig.trigger_threshold_permille = 100;
        boundaryConfig.min_amplitude_permille = 100;
        const CxBoundaryResponseResult fallingStep =
            EvaluateBoundaryResponse(MakeStepWaveform(128, 64, false), boundaryConfig);
        AddCase(result, "boundary_response_falling_step", "boundary_response",
                fallingStep.status == "PREVIEW_READY" &&
                    Near(SelectedBoundaryPosition(fallingStep), 63.5, 1.0),
                SelectedBoundaryPosition(fallingStep), 63.5, 1.0,
                "negative step response localizes the falling edge");

        boundaryConfig.response_mode = CxBoundaryResponseMode::Auto;
        boundaryConfig.polarity = CxBoundaryPolarity::Either;
        boundaryConfig.selection_mode = CxBoundarySelectionMode::NearestGateCenter;
        const CxBoundaryResponseResult automaticStep =
            EvaluateBoundaryResponse(MakeStepWaveform(128, 64, false), boundaryConfig);
        AddCase(result, "boundary_response_auto_step", "boundary_response",
                automaticStep.status == "PREVIEW_READY" &&
                    automaticStep.effective_response_mode ==
                        CxBoundaryResponseMode::NegativeStep,
                static_cast<double>(automaticStep.effective_response_mode ==
                                    CxBoundaryResponseMode::NegativeStep),
                1.0, 0.0,
                "automatic model recognition selects a negative step");

        boundaryConfig.gate_start_permille = 420;
        boundaryConfig.gate_end_permille = 620;
        const CxBoundaryResponseResult automaticInternalPlateauStep =
            EvaluateBoundaryResponse(MakeInternalBrightPlateauWaveform(),
                                     boundaryConfig);
        AddCase(result, "boundary_response_auto_internal_plateau_step",
                "boundary_response",
                automaticInternalPlateauStep.status == "PREVIEW_READY" &&
                    automaticInternalPlateauStep.effective_response_mode ==
                        CxBoundaryResponseMode::NegativeStep &&
                    Near(SelectedBoundaryPosition(automaticInternalPlateauStep),
                         81.5, 2.0),
                SelectedBoundaryPosition(automaticInternalPlateauStep), 81.5, 2.0,
                "automatic model recognition treats a target-side plateau edge as a negative step, not a peak");
        boundaryConfig.gate_start_permille = 0;
        boundaryConfig.gate_end_permille = 1000;

        boundaryConfig.response_mode = CxBoundaryResponseMode::NegativeStep;
        boundaryConfig.polarity = CxBoundaryPolarity::Falling;

        const CxBoundaryResponseResult brightnessScaledStep =
            EvaluateBoundaryResponse(
                MakeStepWaveform(128, 64, false, 40.0f, 180.0f),
                boundaryConfig);
        AddCase(result, "boundary_response_brightness_invariant",
                "boundary_response",
                brightnessScaledStep.status == "PREVIEW_READY" &&
                    Near(SelectedBoundaryPosition(brightnessScaledStep),
                         SelectedBoundaryPosition(fallingStep), 0.25),
                SelectedBoundaryPosition(brightnessScaledStep),
                SelectedBoundaryPosition(fallingStep), 0.25,
                "offset and range normalization preserve edge location under brightness scaling");

        boundaryConfig.response_mode = CxBoundaryResponseMode::NegativeStep;
        boundaryConfig.polarity = CxBoundaryPolarity::Falling;
        boundaryConfig.selection_mode = CxBoundarySelectionMode::Strongest;
        boundaryConfig.reference_profile.clear();
        const CxBoundaryResponseResult competingStrongest =
            EvaluateBoundaryResponse(MakeCompetingFallingEdgesWaveform(),
                                     boundaryConfig);
        boundaryConfig.selection_mode = CxBoundarySelectionMode::NearestReference;
        boundaryConfig.reference_position_permille =
            static_cast<int>(std::lround(76.5 * 1000.0 / 159.0));
        const CxBoundaryResponseResult competingNearestReference =
            EvaluateBoundaryResponse(MakeCompetingFallingEdgesWaveform(),
                                     boundaryConfig);
        AddCase(result, "boundary_response_nearest_reference_override",
                "boundary_response",
                competingStrongest.status == "PREVIEW_READY" &&
                    competingNearestReference.status == "PREVIEW_READY" &&
                    Near(SelectedBoundaryPosition(competingStrongest), 120.5, 1.0) &&
                    Near(SelectedBoundaryPosition(competingNearestReference),
                         76.5, 1.0),
                SelectedBoundaryPosition(competingNearestReference), 76.5, 1.0,
                "manual reference selection chooses the reviewed edge instead of the strongest later edge");
        boundaryConfig.selection_mode = CxBoundarySelectionMode::Strongest;

        boundaryConfig.response_mode = CxBoundaryResponseMode::PositiveStep;
        boundaryConfig.polarity = CxBoundaryPolarity::Rising;
        const CxBoundaryResponseResult risingStep =
            EvaluateBoundaryResponse(MakeStepWaveform(128, 48, true), boundaryConfig);
        AddCase(result, "boundary_response_rising_step", "boundary_response",
                risingStep.status == "PREVIEW_READY" &&
                    Near(SelectedBoundaryPosition(risingStep), 47.5, 1.0),
                SelectedBoundaryPosition(risingStep), 47.5, 1.0,
                "positive step response localizes the rising edge");

        boundaryConfig.response_mode = CxBoundaryResponseMode::RisingGradient;
        const CxBoundaryResponseResult risingGradient =
            EvaluateBoundaryResponse(MakeStepWaveform(128, 48, true),
                                     boundaryConfig);
        AddCase(result, "boundary_response_rising_gradient",
                "boundary_response",
                risingGradient.status == "PREVIEW_READY" &&
                    Near(SelectedBoundaryPosition(risingGradient), 47.5, 1.0),
                SelectedBoundaryPosition(risingGradient), 47.5, 1.0,
                "rising-gradient mode localizes the positive derivative peak");

        boundaryConfig.response_mode = CxBoundaryResponseMode::FallingGradient;
        boundaryConfig.polarity = CxBoundaryPolarity::Falling;
        const CxBoundaryResponseResult fallingGradient =
            EvaluateBoundaryResponse(MakeStepWaveform(128, 64, false),
                                     boundaryConfig);
        AddCase(result, "boundary_response_falling_gradient",
                "boundary_response",
                fallingGradient.status == "PREVIEW_READY" &&
                    Near(SelectedBoundaryPosition(fallingGradient), 63.5, 1.0),
                SelectedBoundaryPosition(fallingGradient), 63.5, 1.0,
                "falling-gradient mode localizes the negative derivative peak");

        boundaryConfig.response_mode = CxBoundaryResponseMode::LevelCrossing;
        boundaryConfig.polarity = CxBoundaryPolarity::Falling;
        boundaryConfig.level_permille = 500;
        boundaryConfig.subpixel_mode = CxBoundarySubpixelMode::LinearCrossing;
        const CxBoundaryResponseResult levelCrossing =
            EvaluateBoundaryResponse(MakeStepWaveform(128, 64, false), boundaryConfig);
        AddCase(result, "boundary_response_level_crossing", "boundary_response",
                levelCrossing.status == "PREVIEW_READY" &&
                    Near(SelectedBoundaryPosition(levelCrossing), 63.5, 1e-12),
                SelectedBoundaryPosition(levelCrossing), 63.5, 1e-12,
                "50 percent crossing uses linear subpixel interpolation");

        std::vector<float> peakWaveform(96, 0.1f);
        peakWaveform[39] = 0.6f;
        peakWaveform[40] = 1.0f;
        peakWaveform[41] = 0.6f;
        boundaryConfig.response_mode = CxBoundaryResponseMode::Peak;
        boundaryConfig.polarity = CxBoundaryPolarity::Either;
        boundaryConfig.subpixel_mode = CxBoundarySubpixelMode::ParabolicResponse;
        const CxBoundaryResponseResult peakResponse =
            EvaluateBoundaryResponse(peakWaveform, boundaryConfig);
        AddCase(result, "boundary_response_peak", "boundary_response",
                peakResponse.status == "PREVIEW_READY" &&
                    Near(SelectedBoundaryPosition(peakResponse), 40.0, 1e-12),
                SelectedBoundaryPosition(peakResponse), 40.0, 1e-12,
                "peak trigger localizes a symmetric waveform maximum");

        std::vector<float> valleyWaveform(96, 0.9f);
        valleyWaveform[39] = 0.4f;
        valleyWaveform[40] = 0.0f;
        valleyWaveform[41] = 0.4f;
        boundaryConfig.response_mode = CxBoundaryResponseMode::Valley;
        const CxBoundaryResponseResult valleyResponse =
            EvaluateBoundaryResponse(valleyWaveform, boundaryConfig);
        AddCase(result, "boundary_response_valley", "boundary_response",
                valleyResponse.status == "PREVIEW_READY" &&
                    Near(SelectedBoundaryPosition(valleyResponse), 40.0, 1.0),
                SelectedBoundaryPosition(valleyResponse), 40.0, 1.0,
                "valley trigger localizes a symmetric waveform minimum");

        boundaryConfig.response_mode =
            CxBoundaryResponseMode::CurvatureZeroCrossing;
        boundaryConfig.polarity = CxBoundaryPolarity::Falling;
        const CxBoundaryResponseResult curvatureResponse =
            EvaluateBoundaryResponse(MakeStepWaveform(128, 64, false),
                                     boundaryConfig);
        AddCase(result, "boundary_response_curvature_zero_crossing",
                "boundary_response",
                curvatureResponse.status == "PREVIEW_READY" &&
                    Near(SelectedBoundaryPosition(curvatureResponse), 63.5, 2.0),
                SelectedBoundaryPosition(curvatureResponse), 63.5, 2.0,
                "curvature zero crossing is constrained by falling polarity");

        boundaryConfig.response_mode = CxBoundaryResponseMode::PullUp;
        boundaryConfig.polarity = CxBoundaryPolarity::Rising;
        const CxBoundaryResponseResult pullUpResponse =
            EvaluateBoundaryResponse(MakeStepWaveform(128, 48, true),
                                     boundaryConfig);
        AddCase(result, "boundary_response_pull_up", "boundary_response",
                pullUpResponse.status == "PREVIEW_READY",
                pullUpResponse.status == "PREVIEW_READY" ? 1.0 : 0.0,
                1.0, 0.0,
                "pull-up state validates rising transition and held plateau");

        boundaryConfig.response_mode = CxBoundaryResponseMode::PullDown;
        boundaryConfig.polarity = CxBoundaryPolarity::Falling;
        const CxBoundaryResponseResult pullDownResponse =
            EvaluateBoundaryResponse(MakeStepWaveform(128, 64, false),
                                     boundaryConfig);
        AddCase(result, "boundary_response_pull_down", "boundary_response",
                pullDownResponse.status == "PREVIEW_READY",
                pullDownResponse.status == "PREVIEW_READY" ? 1.0 : 0.0,
                1.0, 0.0,
                "pull-down state validates falling transition and held plateau");

        std::vector<float> pulse(128, 0.0f);
        for (int i = 30; i < 70; ++i)
            pulse[static_cast<std::size_t>(i)] = 1.0f;
        boundaryConfig.response_mode = CxBoundaryResponseMode::RiseFallPair;
        boundaryConfig.pair_min_width = 20;
        boundaryConfig.pair_max_width = 60;
        const CxBoundaryResponseResult pairResponse =
            EvaluateBoundaryResponse(pulse, boundaryConfig);
        AddCase(result, "boundary_response_edge_pair", "boundary_response",
                pairResponse.status == "PREVIEW_READY" &&
                    Near(SelectedBoundaryPosition(pairResponse), 49.5, 2.0),
                SelectedBoundaryPosition(pairResponse), 49.5, 2.0,
                "rise-then-fall mode returns the center of a valid edge pair");

        std::vector<float> valleyPulse(128, 1.0f);
        for (int i = 30; i < 70; ++i)
            valleyPulse[static_cast<std::size_t>(i)] = 0.0f;
        boundaryConfig.response_mode = CxBoundaryResponseMode::FallRisePair;
        const CxBoundaryResponseResult reversePairResponse =
            EvaluateBoundaryResponse(valleyPulse, boundaryConfig);
        AddCase(result, "boundary_response_reverse_edge_pair",
                "boundary_response",
                reversePairResponse.status == "PREVIEW_READY" &&
                    Near(SelectedBoundaryPosition(reversePairResponse), 49.5,
                         2.0),
                SelectedBoundaryPosition(reversePairResponse), 49.5, 2.0,
                "fall-then-rise mode returns the center of a valid valley pair");

        boundaryConfig.response_mode = CxBoundaryResponseMode::TemplateCorrelation;
        boundaryConfig.reference_profile =
            MakeStepWaveform(17, 9, false);
        const CxBoundaryResponseResult templateResponse =
            EvaluateBoundaryResponse(MakeStepWaveform(128, 64, false),
                                     boundaryConfig);
        AddCase(result, "boundary_response_template_correlation",
                "boundary_response",
                templateResponse.status == "PREVIEW_READY" &&
                    Near(SelectedBoundaryPosition(templateResponse), 64.0, 3.0),
                SelectedBoundaryPosition(templateResponse), 64.0, 3.0,
                "bound waveform template produces a localized correlation candidate");
        boundaryConfig.reference_profile.clear();

        struct BoundaryWaveletSmokeMode
        {
            CxBoundaryResponseMode mode;
            const char* case_suffix;
        };
        const BoundaryWaveletSmokeMode waveletModes[] = {
            {CxBoundaryResponseMode::Haar, "haar"},
            {CxBoundaryResponseMode::Daubechies4, "daubechies4"},
            {CxBoundaryResponseMode::Daubechies20, "daubechies20"},
            {CxBoundaryResponseMode::DerivativeOfGaussian, "dog"}};
        for (const BoundaryWaveletSmokeMode& item : waveletModes)
        {
            boundaryConfig.response_mode = item.mode;
            boundaryConfig.polarity = CxBoundaryPolarity::Either;
            boundaryConfig.wavelet_scale = 2;
            const CxBoundaryResponseResult wavelet = EvaluateBoundaryResponse(
                MakeStepWaveform(128, 64, false), boundaryConfig);
            AddCase(result,
                    std::string("boundary_response_wavelet_") + item.case_suffix,
                    "boundary_response", wavelet.status == "PREVIEW_READY",
                    wavelet.status == "PREVIEW_READY" ? 1.0 : 0.0, 1.0, 0.0,
                    std::string(CxBoundaryResponseModeName(item.mode)) +
                        " produces a reviewable step candidate");
        }

        double findlineBoundaryObserved = 0.0;
        std::string findlineBoundaryReason;
        const bool findlineBoundaryPass =
            RunFindLineBoundaryTriggerSmoke(
                findlineBoundaryObserved,
                findlineBoundaryReason);
        AddCase(result, "findline_boundary_trigger_measure_points",
                "boundary_response",
                findlineBoundaryPass,
                findlineBoundaryObserved,
                2.0,
                0.0,
                findlineBoundaryReason);

        double findcircleBoundaryObserved = 0.0;
        std::string findcircleBoundaryReason;
        const bool findcircleBoundaryPass =
            RunFindCircleBoundaryTriggerSmoke(
                findcircleBoundaryObserved,
                findcircleBoundaryReason);
        AddCase(result, "findcircle_boundary_trigger_measure_points",
                "boundary_response",
                findcircleBoundaryPass,
                findcircleBoundaryObserved,
                8.0,
                0.0,
                findcircleBoundaryReason);

        double findellipseBoundaryObserved = 0.0;
        std::string findellipseBoundaryReason;
        const bool findellipseBoundaryPass =
            RunFindEllipseBoundaryTriggerSmoke(
                findellipseBoundaryObserved,
                findellipseBoundaryReason);
        AddCase(result, "findellipse_boundary_trigger_measure_points",
                "boundary_response",
                findellipseBoundaryPass,
                findellipseBoundaryObserved,
                8.0,
                0.0,
                findellipseBoundaryReason);

        AddCase(result, "boundary_1x1", "boundary", CxSurfaceField(1, 1).valueCount() == 1, 1.0, 1.0, 0.0, "1x1 field is valid");
        AddThrowCase(result, "boundary_out_of_range", "boundary", [] { CxSurfaceField(1, 1).at(2, 0); });
        AddThrowCase(result, "boundary_negative_scale", "boundary", []
        {
            CxPhysUnit invalid;
            invalid.x_scale_per_pixel = -1.0;
            CxSurfaceField f(4, 4, invalid);
        });

        const CxMetrologyUiGlobalFields uiDefaults =
            DefaultMetrologyUiGlobalFields();
        const std::vector<CxMetrologyUiGlobalPair> uiGlobals =
            BuildMetrologyUiGlobalSnapshot(uiDefaults);
        auto findUiValue = [](const std::vector<CxMetrologyUiGlobalPair>& items,
                              const std::string& name,
                              int fallback) -> int
        {
            for (const auto& item : items)
            {
                if (item.name == name)
                    return item.value;
            }
            return fallback;
        };
        bool allGlobalMetrologyPrefixed = true;
        for (const auto& item : uiGlobals)
        {
            if (item.name.rfind("global_metrology_", 0) != 0)
            {
                allGlobalMetrologyPrefixed = false;
                break;
            }
        }
        AddCase(result, "ui_global_count", "ui_globals",
                uiGlobals.size() == 90,
                static_cast<double>(uiGlobals.size()), 90.0, 0.0,
                "Key Parameter Controls metrology panel exposes 90 global_metrology_* values");
        AddCase(result, "ui_global_prefix", "ui_globals", allGlobalMetrologyPrefixed, allGlobalMetrologyPrefixed ? 1.0 : 0.0, 1.0, 0.0, "all metrology UI globals use the global_metrology_ prefix");
        AddCase(result, "ui_scan_max_lines_default", "ui_globals", findUiValue(uiGlobals, "global_metrology_scan_profile_max_lines", -1) == 256, static_cast<double>(findUiValue(uiGlobals, "global_metrology_scan_profile_max_lines", -1)), 256.0, 0.0, "scan profile max lines default is locked");
        AddCase(result, "ui_surface_area_method_default", "ui_globals", findUiValue(uiGlobals, "global_metrology_surface_area_method", -1) == 1, static_cast<double>(findUiValue(uiGlobals, "global_metrology_surface_area_method", -1)), 1.0, 0.0, "area method default is four-triangle fan");
        AddCase(result, "ui_unit_defaults", "ui_globals", findUiValue(uiGlobals, "global_metrology_x_unit", -1) == 2 && findUiValue(uiGlobals, "global_metrology_y_unit", -1) == 2 && findUiValue(uiGlobals, "global_metrology_z_unit", -1) == 2, static_cast<double>(findUiValue(uiGlobals, "global_metrology_x_unit", -1)), 2.0, 0.0, "x/y/z default units are micrometer entries");
        AddCase(result, "ui_roughness_bins_default", "ui_globals", findUiValue(uiGlobals, "global_metrology_roughness_bins", -1) == 1024, static_cast<double>(findUiValue(uiGlobals, "global_metrology_roughness_bins", -1)), 1024.0, 0.0, "ISO 1D roughness bins default is locked");
        AddCase(result, "ui_boundary_trigger_defaults", "ui_globals",
                findUiValue(uiGlobals, "global_metrology_boundary_preview_enabled", -1) == 1 &&
                    findUiValue(uiGlobals, "global_metrology_boundary_response_mode", -1) == 0 &&
                    findUiValue(uiGlobals, "global_metrology_boundary_polarity", -1) == 2 &&
                    findUiValue(uiGlobals, "global_metrology_boundary_selection_mode", -1) ==
                        static_cast<int>(CxBoundarySelectionMode::NearestGateCenter) &&
                    findUiValue(uiGlobals, "global_metrology_boundary_level_permille", -1) == 500 &&
                    findUiValue(uiGlobals, "global_metrology_boundary_reference_bound", -1) == 0,
                static_cast<double>(findUiValue(uiGlobals, "global_metrology_boundary_selection_mode", -1)),
                static_cast<double>(CxBoundarySelectionMode::NearestGateCenter),
                0.0,
                "Boundary Trigger defaults use automatic model selection and nearest gate-center arbitration without a fabricated reference");

        AddCase(result, "ui_peak_defaults", "ui_globals",
                findUiValue(uiGlobals, "global_metrology_peak_max_count", -1) == 12 &&
                    findUiValue(uiGlobals, "global_metrology_peak_order", -1) == 0 &&
                    findUiValue(uiGlobals, "global_metrology_peak_background", -1) == 1 &&
                    findUiValue(uiGlobals, "global_metrology_peak_invert", -1) == 0,
                static_cast<double>(findUiValue(uiGlobals, "global_metrology_peak_max_count", -1)),
                12.0, 0.0,
                "Find Peaks reliable defaults use position order and bilateral minimum background");
        AddCase(result, "ui_curve_fit_defaults", "ui_globals",
                findUiValue(uiGlobals, "global_metrology_curve_fit_function", -1) == 0 &&
                    findUiValue(uiGlobals, "global_metrology_curve_fit_auto_estimate", -1) == 1 &&
                    findUiValue(uiGlobals, "global_metrology_curve_fit_auto_plot", -1) == 1 &&
                    findUiValue(uiGlobals, "global_metrology_curve_fit_full_range", -1) == 1,
                static_cast<double>(findUiValue(uiGlobals, "global_metrology_curve_fit_function", -1)),
                0.0, 0.0,
                "Curve Fitting defaults use Gaussian, automatic estimate/plot and full range");
        AddCase(result, "ui_critical_dimension_defaults", "ui_globals",
                findUiValue(uiGlobals, "global_metrology_critical_dimension_function", -1) == 0 &&
                    findUiValue(uiGlobals, "global_metrology_critical_dimension_auto_fit", -1) == 1 &&
                    findUiValue(uiGlobals, "global_metrology_critical_dimension_full_range", -1) == 1 &&
                    findUiValue(uiGlobals, "global_metrology_critical_dimension_draw_whole_circle", -1) == 0,
                static_cast<double>(findUiValue(uiGlobals, "global_metrology_critical_dimension_function", -1)),
                0.0, 0.0,
                "Critical Dimension defaults use right-edge preset, auto fit and full range");

        CxMetrologyUiGlobalFields uiEdited = uiDefaults;
        uiEdited.enabled = true;
        uiEdited.active_tab = 10;
        uiEdited.show_scan_profile = true;
        uiEdited.surface_width = 512;
        uiEdited.enable_plane_correction = true;
        uiEdited.roughness_bins = 2048;
        uiEdited.peak_max_count = 9;
        uiEdited.peak_invert = true;
        uiEdited.curve_fit_function = 1;
        uiEdited.curve_fit_full_range = false;
        uiEdited.critical_dimension_function = 4;
        uiEdited.critical_dimension_draw_whole_circle = true;
        uiEdited.boundary_response_mode =
            static_cast<int>(CxBoundaryResponseMode::NegativeStep);
        uiEdited.boundary_polarity =
            static_cast<int>(CxBoundaryPolarity::Falling);
        uiEdited.boundary_gate_start_permille = 250;
        uiEdited.boundary_reference_bound = true;
        const std::vector<CxMetrologyUiGlobalPair> uiEditedGlobals =
            BuildMetrologyUiGlobalSnapshot(uiEdited);
        const bool uiEditPropagation =
            findUiValue(uiEditedGlobals, "global_metrology_enabled", -1) == 1 &&
            findUiValue(uiEditedGlobals, "global_metrology_active_tab", -1) == 10 &&
            findUiValue(uiEditedGlobals, "global_metrology_show_scan_profile", -1) == 1 &&
            findUiValue(uiEditedGlobals, "global_metrology_surface_width", -1) == 512 &&
            findUiValue(uiEditedGlobals, "global_metrology_enable_plane_correction", -1) == 1 &&
            findUiValue(uiEditedGlobals, "global_metrology_roughness_bins", -1) == 2048 &&
            findUiValue(uiEditedGlobals, "global_metrology_peak_max_count", -1) == 9 &&
            findUiValue(uiEditedGlobals, "global_metrology_peak_invert", -1) == 1 &&
            findUiValue(uiEditedGlobals, "global_metrology_curve_fit_function", -1) == 1 &&
            findUiValue(uiEditedGlobals, "global_metrology_curve_fit_full_range", -1) == 0 &&
            findUiValue(uiEditedGlobals, "global_metrology_critical_dimension_function", -1) == 4 &&
            findUiValue(uiEditedGlobals, "global_metrology_critical_dimension_draw_whole_circle", -1) == 1 &&
            findUiValue(uiEditedGlobals, "global_metrology_boundary_response_mode", -1) ==
                static_cast<int>(CxBoundaryResponseMode::NegativeStep) &&
            findUiValue(uiEditedGlobals, "global_metrology_boundary_polarity", -1) ==
                static_cast<int>(CxBoundaryPolarity::Falling) &&
            findUiValue(uiEditedGlobals, "global_metrology_boundary_gate_start_permille", -1) == 250 &&
            findUiValue(uiEditedGlobals, "global_metrology_boundary_reference_position_permille", -1) == 500 &&
            findUiValue(uiEditedGlobals, "global_metrology_boundary_reference_bound", -1) == 1;
        AddCase(result, "ui_edit_global_propagation", "ui_globals", uiEditPropagation, uiEditPropagation ? 1.0 : 0.0, 1.0, 0.0, "edited metrology UI fields propagate to global_metrology_* snapshot values");


        const CxManualConsoleAnalyticsSmokePanelContract panelContract =
            ManualConsoleAnalyticsSmokePanelContract();
        AddCase(
            result,
            "s3_9_manual_console_analytics_window",
            "manual_console_analytics_smoke",
            panelContract.window_title.find("Analytics Smoke") != std::string::npos,
            panelContract.window_title.find("Analytics Smoke") != std::string::npos ? 1.0 : 0.0,
            1.0,
            0.0,
            "Manual Console exposes a separated Analytics Smoke / Metrology Bridge window");
        AddCase(
            result,
            "s3_9_manual_console_no_ci_trigger",
            "manual_console_analytics_smoke",
            !panelContract.triggers_ci,
            panelContract.triggers_ci ? 1.0 : 0.0,
            0.0,
            0.0,
            "Manual Console analytics smoke is explicitly non-CI and user-triggered");
        AddCase(
            result,
            "s3_9_manual_console_min_case_contract",
            "manual_console_analytics_smoke",
            panelContract.minimum_expected_cases >= 23,
            static_cast<double>(panelContract.minimum_expected_cases),
            23.0,
            0.0,
            "S3-9 panel contract keeps the Next3_1 minimum 23-case smoke floor");
        AddCase(
            result,
            "s3_9_manual_console_reference_and_bridge_sections",
            "manual_console_analytics_smoke",
            panelContract.has_reference_replay_section &&
                panelContract.has_s4_bridge_section,
            panelContract.has_reference_replay_section &&
                    panelContract.has_s4_bridge_section
                ? 1.0
                : 0.0,
            1.0,
            0.0,
            "Manual Console analytics window advertises reference replay and S3->S4 draft bridge sections");

        CxSurfaceBasicStats draftFlatStats;
        const bool bridgeFlatStatsOk =
            CxAnalyticsObservationBridgeDraft::TryQuerySurfaceBasicStats(
                "draft.flat5",
                draftFlatStats);
        AddCase(
            result,
            "s3_11_bridge_flat_stats_mean",
            "s4_bridge_draft",
            bridgeFlatStatsOk && Near(draftFlatStats.mean, 5.0, 1e-12),
            bridgeFlatStatsOk ? draftFlatStats.mean : 0.0,
            5.0,
            1e-12,
            "draft bridge returns value-only surface basic stats");

        CxSurfaceAreaResult draftFlatArea;
        const bool bridgeFlatAreaOk =
            CxAnalyticsObservationBridgeDraft::TryQueryAreaResult(
                "draft.flat5",
                draftFlatArea);
        AddCase(
            result,
            "s3_11_bridge_flat_area_ratio",
            "s4_bridge_draft",
            bridgeFlatAreaOk && Near(draftFlatArea.area_ratio, 1.0, 1e-12),
            bridgeFlatAreaOk ? draftFlatArea.area_ratio : 0.0,
            1.0,
            1e-12,
            "draft bridge returns value-only area result");

        CxRoughness1DResult draftSineRoughness;
        const bool bridgeSineRoughnessOk =
            CxAnalyticsObservationBridgeDraft::TryQueryRoughness1D(
                "draft.sine_A1_full_cycle",
                draftSineRoughness);
        AddCase(
            result,
            "s3_11_bridge_sine_roughness_ra",
            "s4_bridge_draft",
            bridgeSineRoughnessOk &&
                Near(draftSineRoughness.Ra, 2.0 / kPi, 1e-3),
            bridgeSineRoughnessOk ? draftSineRoughness.Ra : 0.0,
            2.0 / kPi,
            1e-3,
            "draft bridge returns value-only roughness result");

        CxSurfaceBasicStats missingStats;
        const bool missingReturnsFalse =
            !CxAnalyticsObservationBridgeDraft::TryQuerySurfaceBasicStats(
                "draft.missing_observation",
                missingStats);
        AddCase(
            result,
            "s3_11_bridge_unknown_pending",
            "s4_bridge_draft",
            missingReturnsFalse,
            missingReturnsFalse ? 1.0 : 0.0,
            1.0,
            0.0,
            "unknown analytics observation id is pending binding, not an algorithm failure");
        AddCase(
            result,
            "s3_11_bridge_draft_status",
            "s4_bridge_draft",
            std::string(CxAnalyticsObservationBridgeDraft::DraftStatus()) ==
                "S3_S4_BRIDGE_DRAFT_ONLY_PENDING_S4_REVIEW",
            1.0,
            1.0,
            0.0,
            "bridge status remains draft-only until S4 review");

        CxMetrologyReferenceReplayResult referenceReplay;
        std::string referenceReplayReason;
        const std::filesystem::path referenceDir =
            std::filesystem::current_path() /
            "cximage" / "metrology_analytics" / "ref_analytics";
        const bool referenceReplayOk =
            RunMetrologyReferenceReplay(
                referenceDir,
                output_dir,
                referenceReplay,
                referenceReplayReason);
        AddCase(
            result,
            "reference_replay_artifacts",
            "reference_replay",
            referenceReplayOk,
            referenceReplayOk ? 1.0 : 0.0,
            1.0,
            0.0,
            referenceReplayOk
                ? "reference replay artifacts generated"
                : referenceReplayReason);
        if (referenceReplayOk)
        {
            AddCase(
                result,
                "reference_replay_case_count",
                "reference_replay",
                referenceReplay.reference_case_count >= 2,
                static_cast<double>(referenceReplay.reference_case_count),
                2.0,
                0.0,
                "ref_case_*.json files are discovered and replayed");
            for (const auto& a : referenceReplay.assertions)
            {
                AddCase(
                    result,
                    "reference_" + a.case_id + "_" + a.metric,
                    "reference_replay",
                    a.pass,
                    a.observed,
                    a.expected,
                    a.tolerance,
                    a.reason);
            }
        }
    }
    catch (const std::exception& e)
    {
        reason = std::string("metrology analytics smoke aborted: ") + e.what();
        return false;
    }

    result.total_cases = static_cast<int>(result.cases.size());
    result.pass_count = 0;
    result.fail_count = 0;
    for (const auto& c : result.cases)
    {
        if (c.pass)
            ++result.pass_count;
        else
            ++result.fail_count;
    }

    return WriteSummary(output_dir, result, reason);
}

} // namespace cxvision::metrology_analytics
