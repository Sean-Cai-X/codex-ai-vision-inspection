#include "ThreeDOrchestrator.h"

#include <iostream>
#include <memory>
#include <stdexcept>

using namespace codex_lan_agent_3d;

namespace {

class NullStructuredAssetGenerator : public IStructuredAssetGenerator {
public:
    StructuredAssetRecord Generate(const std::string &, const std::string &) override {
        return StructuredAssetRecord();
    }
    StructuredAssetRecord RegeneratePart(const StructuredAssetRecord & asset,
                                         const std::string &,
                                         const std::string &,
                                         const std::string &) override {
        return asset;
    }
    StructuredAssetRecord AddPart(const StructuredAssetRecord & asset,
                                  const std::string &,
                                  const std::string &) override {
        return asset;
    }
    StructuredAssetRecord Articulate(const StructuredAssetRecord & asset,
                                     const std::string &,
                                     const std::string &) override {
        return asset;
    }
};

class NullSceneAdapter : public ISceneAdapter {
public:
    CommandResult ImportAsset(const StructuredAssetRecord &) override {
        return CommandResult();
    }
    CommandResult TransformObject(const std::string &,
                                  const Vec3 &,
                                  const Vec3 &,
                                  const Vec3 &) override {
        return CommandResult();
    }
    SceneSnapshot ReadSnapshot() const override {
        return SceneSnapshot();
    }
};

void Require(bool condition, const std::string & message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int main() {
    ThreeDOrchestrator orchestrator(
        std::make_shared<NullStructuredAssetGenerator>(),
        std::make_shared<NullSceneAdapter>());

    GeometrySceneConceptRecord geometry;
    geometry.entity_id = 7;
    geometry.shape_kind = "curve";
    geometry.has_payload = true;
    geometry.has_presentation = true;
    geometry.visible = true;
    geometry.geometry_revision = 3;
    geometry.publishable = true;
    orchestrator.UpsertGeometrySceneRecord(geometry);

    CloudSceneConceptRecord cloud;
    cloud.entity_id = 9;
    cloud.has_payload = true;
    cloud.has_render_data = true;
    cloud.has_bounds = false;
    cloud.visible = true;
    cloud.point_count = 4;
    cloud.cloud_revision = 2;
    cloud.publishable = true;
    orchestrator.UpsertCloudSceneRecord(cloud);

    ParserWorkflowConceptRecord workflow;
    workflow.task_id = "task_bridge_1";
    workflow.trace_id = "trace_bridge_1";
    workflow.module_name = "cxgeom";
    workflow.layer_name = "smoke";
    workflow.case_id = "cxgeom_bulk_create_presentation_release";
    workflow.route_key = "default";
    workflow.execution_mode = "mainline_execute";
    workflow.result_object = "CxGeometrySceneRecord";
    workflow.summary = "task validated";
    workflow.executed_task_count = 1;
    workflow.replay_count = 1;
    workflow.success = true;
    orchestrator.RegisterParserWorkflow(workflow);

    CommandResult bridge_summary = orchestrator.GetSceneBridgeSummary();
    Require(bridge_summary.ok, "bridge summary should succeed");
    Require(bridge_summary.fields.at("geometry_record_count") == "1", "geometry record count mismatch");
    Require(bridge_summary.fields.at("cloud_record_count") == "1", "cloud record count mismatch");
    Require(bridge_summary.fields.at("refresh_action") == "update_presentation", "cloud missing bounds should force presentation update");
    Require(bridge_summary.fields.at("scene_records").find("geometry:7:curve:publishable") != std::string::npos, "geometry scene record missing");
    Require(bridge_summary.fields.at("scene_records").find("cloud:9:points_4:publishable") != std::string::npos, "cloud scene record missing");

    CommandResult workflow_summary = orchestrator.GetParserWorkflowSummary();
    Require(workflow_summary.ok, "workflow summary should succeed");
    Require(workflow_summary.fields.at("workflow_count") == "1", "workflow count mismatch");
    Require(workflow_summary.fields.at("workflow_success_count") == "1", "workflow success count mismatch");
    Require(workflow_summary.fields.at("workflow_records").find("task_bridge_1:cxgeom:smoke") != std::string::npos, "workflow record missing");

    Require(orchestrator.ToolCatalog().size() >= 9, "tool catalog should expose bridge summary tools");

    std::cout << "3D scene bridge smoke test passed\n";
    return 0;
}
