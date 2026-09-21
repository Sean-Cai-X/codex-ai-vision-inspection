#ifndef CXIMAGE_CXSHAPE_INTERACTION_TEST_H
#define CXIMAGE_CXSHAPE_INTERACTION_TEST_H

#include <string>
#include <vector>

#include "shapebase.h"
#include "LineGaugeShape.h"

struct CxShapeInteractionStep
{
    std::string action;
    std::string target_ref;
    CxShapeHandleRole role = CxShapeHandleRole::None;
    double x0 = 0.0;
    double y0 = 0.0;
    double x1 = 0.0;
    double y1 = 0.0;
    bool ok = false;
    std::string reason;
};

struct CxShapeInteractionCaseResult
{
    std::string case_id;
    std::string tool_id;
    std::string shape_kind;
    bool pass = false;
    std::string conclusion;
    std::string reason;
    std::vector<CxShapeInteractionStep> steps;
};

struct CxShapeInteractionBatchResult
{
    bool pass = false;
    std::vector<CxShapeInteractionCaseResult> cases;
    std::string failure_stage;
    std::string reason;
    std::string manifest_path;
    std::string suite_path;
};

bool WriteShapeInteractionReportJson(
    const CxShapeInteractionBatchResult& result,
    const std::string& path,
    std::string& reason);

bool WriteShapeInteractionReportMd(
    const CxShapeInteractionBatchResult& result,
    const std::string& path,
    std::string& reason);

bool WriteShapeInteractionFailuresMd(
    const CxShapeInteractionBatchResult& result,
    const std::string& path,
    std::string& reason);

bool WriteShapeInteractionSnapshot(
    const CxShapeInteractionBatchResult& result,
    const std::string& path,
    std::string& reason);

bool WriteShapeSuiteLoadReport(
    const std::string& out_dir,
    const std::string& manifest_path,
    const std::string& suite_path,
    bool manifest_exists,
    bool manifest_parse_ok,
    bool suite_exists,
    bool suite_parse_ok,
    int case_count,
    const std::string& failure_stage,
    const std::string& reason);

bool AssertLineMoved(
    const ShapeBase& before,
    const ShapeBase& after,
    double expected_dx,
    double expected_dy,
    std::string& reason);

bool AssertCircleRadiusChangedOnly(
    const ShapeBase& before,
    const ShapeBase& after,
    std::string& reason);

bool AssertCircleCenterMoved(
    const ShapeBase& before,
    const ShapeBase& after,
    double expected_dx,
    double expected_dy,
    std::string& reason);

bool AssertPolylineVertexMoved(
    const ShapeBase& before,
    const ShapeBase& after,
    int vertex_index,
    double target_x,
    double target_y,
    std::string& reason);

bool AssertLineGaugeWidthChangedOnly(
    const LineGaugeShape& before,
    const LineGaugeShape& after,
    std::string& reason);

bool AssertLineGaugeEndpointsMoved(
    const LineGaugeShape& before,
    const LineGaugeShape& after,
    bool start_moved,
    bool end_moved,
    std::string& reason);

bool AssertLineGaugeCenterMoved(
    const LineGaugeShape& before,
    const LineGaugeShape& after,
    double expected_dx,
    double expected_dy,
    std::string& reason);

bool AssertRectCornerMoved(
    const ShapeBase& before,
    const ShapeBase& after,
    int corner_index,
    double target_x,
    double target_y,
    std::string& reason);

bool AssertPointCreated(
    const ShapeBase& shape,
    std::string& reason);

bool AssertLineCreated(
    const ShapeBase& shape,
    std::string& reason);

bool AssertRectCreated(
    const ShapeBase& shape,
    std::string& reason);

bool AssertCircleCreated(
    const ShapeBase& shape,
    std::string& reason);

bool AssertPolylineCreated(
    const ShapeBase& shape,
    std::string& reason);

#endif