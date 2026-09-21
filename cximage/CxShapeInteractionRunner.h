#ifndef CXIMAGE_CXSHAPE_INTERACTION_RUNNER_H
#define CXIMAGE_CXSHAPE_INTERACTION_RUNNER_H

#include <string>
#include <vector>
#include <memory>

#include "CxShapeInteractionTest.h"
#include "CxShapeTestRuntime.h"
#include "ImageAnnotationLayer.h"
#include "CxParserSnapshotTypes.h"
#include "ICxRuntimeProjectionExecutor.h"
#include "CxScriptImageManifestRuntime.h"

struct CxShapeInteractionOptions
{
    std::string tool_manifest_path;
    std::string test_suite_path;
    std::string out_dir;
    std::string run_id;
    bool render_frames = true;
    int drag_steps = 5;
    double tolerance = 8.0;

    bool unified_log_enabled = false;
    std::string unified_log_path;
    std::string unified_log_status;
    std::string unified_log_reason;
};

struct CxShapeInteractionPointerEvent {
    std::string event;
    double screen_x = 0.0;
    double screen_y = 0.0;
    double image_x = 0.0;
    double image_y = 0.0;
    bool canvas_hovered = false;
    bool inside_image = false;
    std::string phase;
    std::string status;
};

struct CxShapeInteractionCaseResultEx : CxShapeInteractionCaseResult
{
    std::string expected_handle;
    std::string actual_handle;
    int expected_vertex = -1;
    int actual_vertex = -1;
    std::string geometry_assertion;
    bool hit_test_pass = false;
    bool drag_pass = false;
    bool commit_pass = false;
    bool render_pass = false;
    bool runtime_writeback = false;
    std::string acceptance_scope;
    std::string status;
    std::string shape_kind;
    std::string owner_type;
    std::string owner_binding;
    std::string operation;

    int created_points_count = 0;
    int created_handle_count = 0;
    bool shape_visible = false;
    bool shape_editable = false;
    std::string shape_stable_ref;
    std::string created_shape_kind;
    std::string created_ref;
    std::string selected_ref;

    int shape_count_before = 0;
    int shape_count_after = 0;
    int shape_count_delta = 0;

    std::vector<CxShapeInteractionPointerEvent> pointer_events;
    CxShapeCommitResult commit_result;
};

struct CxShapeInteractionBatchResultEx : CxShapeInteractionBatchResult
{
    std::vector<CxShapeInteractionCaseResultEx> extended_cases;
};

class CxShapeInteractionRunner
{
public:
    bool RunSuite(
        const CxAnnotationToolManifestSnapshot& tool_manifest,
        const CxShapeTestSuiteSnapshot& suite,
        const CxScriptImageManifestRuntime& image_manifest,
        ICxRuntimeProjectionExecutor& projection_executor,
        const CxShapeInteractionOptions& options,
        CxShapeInteractionBatchResultEx& result);

private:
    static bool TryFindToolSpec(
        const CxAnnotationToolManifestSnapshot& manifest,
        const std::string& tool_id,
        CxAnnotationToolSpec& output);

    bool RunTestCase(
        const CxShapeTestCase& tc,
        const CxAnnotationToolManifestSnapshot& tool_manifest,
        const CxScriptImageManifestRuntime& image_manifest,
        ICxRuntimeProjectionExecutor& projection_executor,
        const CxShapeInteractionOptions& options,
        CxShapeInteractionCaseResultEx& case_result,
        CxShapeInteractionTrace& trace);
    bool VerifyHitExpectation(const CxShapeTestCase& tc, const CxShapeHitResult& hit, std::string& reason);
    bool VerifyGeometryAssertion(
        const std::string& assertion,
        const CxShapeGeometrySnapshot& before,
        const CxShapeGeometrySnapshot& after,
        std::string& reason);
    void GenerateCaseOutput(
        const CxShapeInteractionCaseResultEx& case_result,
        const CxShapeInteractionTrace& trace,
        const CxShapeInteractionOptions& options,
        const std::string& out_dir);
    void GenerateBatchOutput(
        const CxShapeInteractionBatchResultEx& result,
        const CxShapeInteractionOptions& options,
        const std::string& out_dir);
};

#endif
