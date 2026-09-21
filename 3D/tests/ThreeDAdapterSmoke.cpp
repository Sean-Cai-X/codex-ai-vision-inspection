#include "CxCloudSceneAdapter.h"
#include "CxGeomSceneAdapter.h"
#include "ThreeDParserDispatchBridge.h"

#include <iostream>
#include <stdexcept>

using namespace codex_lan_agent_3d;

namespace {

void Require(bool condition, const std::string & message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int main() {
    const GeometrySceneConceptRecord geometry = CxGeomSceneAdapter::Convert(
        CxGeomSceneAdapterInput{7, "curve", true, true, true, 4});
    Require(geometry.publishable, "geometry should be publishable");
    CommandResult geometry_result = CxGeomSceneAdapter::BuildPublishabilityResult(geometry);
    Require(geometry_result.ok, "geometry publishability result should be ok");
    Require(geometry_result.fields.at("shape_kind") == "curve", "geometry shape kind mismatch");

    const CloudSceneConceptRecord cloud = CxCloudSceneAdapter::Convert(
        CxCloudSceneAdapterInput{9, true, true, true, true, 4, 2});
    Require(cloud.publishable, "cloud should be publishable");
    CommandResult cloud_result = CxCloudSceneAdapter::BuildPublishabilityResult(cloud);
    Require(cloud_result.ok, "cloud publishability result should be ok");
    Require(cloud_result.fields.at("point_count") == "4", "cloud point count mismatch");

    ThreeDParserDispatchBridge parser_bridge;
    parser_bridge.UpsertCase(ParserDispatchCaseConcept{
        "smoke",
        "cxgeom",
        "cxgeom_bulk_create_presentation_release",
        "cxparser/cxgeom/smoke/cxgeom_bulk_create_presentation_release.cxsc",
        "default",
        "parser_eval",
        "cxgeom_flow_host",
        "Run",
        "ready",
        true,
        false
    });
    parser_bridge.UpsertExecution(ParserDispatchExecutionConcept{
        "task_dispatch_1",
        "trace_dispatch_1",
        "cxgeom",
        "smoke",
        "cxgeom_bulk_create_presentation_release",
        "default",
        "mainline_execute",
        "CxGeometrySceneRecord",
        "task validated",
        1,
        1,
        true
    });

    CommandResult case_summary = parser_bridge.BuildCaseCatalogSummary();
    Require(case_summary.ok, "dispatch case summary should be ok");
    Require(case_summary.fields.at("dispatch_case_count") == "1", "dispatch case count mismatch");
    Require(case_summary.fields.at("dispatch_active_runtime_count") == "1", "active runtime count mismatch");

    CommandResult execution_summary = parser_bridge.BuildExecutionSummary();
    Require(execution_summary.ok, "dispatch execution summary should be ok");
    Require(execution_summary.fields.at("dispatch_execution_count") == "1", "dispatch execution count mismatch");
    Require(execution_summary.fields.at("dispatch_replay_count") == "1", "dispatch replay count mismatch");

    const ParserWorkflowConceptRecord workflow = ThreeDParserDispatchBridge::ToWorkflowRecord(
        ParserDispatchExecutionConcept{
            "task_dispatch_1",
            "trace_dispatch_1",
            "cxgeom",
            "smoke",
            "cxgeom_bulk_create_presentation_release",
            "default",
            "mainline_execute",
            "CxGeometrySceneRecord",
            "task validated",
            1,
            1,
            true
        });
    Require(workflow.module_name == "cxgeom", "workflow module mismatch");
    Require(workflow.replay_count == 1, "workflow replay count mismatch");

    std::cout << "3D adapter smoke test passed\n";
    return 0;
}
