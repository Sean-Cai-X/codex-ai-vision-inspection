#include "BlenderMcpSceneBackend.h"
#include "Nova3DMcpHostedBackend.h"
#include "Nova3DAssetAdapter.h"
#include "ThreeDHostedWorkflowCoordinator.h"

#include <iostream>
#include <memory>
#include <stdexcept>

using namespace codex_lan_agent_3d;

namespace {

class FakeNova3DMcpInvoker : public INova3DMcpInvoker {
public:
    CommandResult CallTool(
        const std::string & tool_name,
        const std::map<std::string, std::string> & arguments) override {
        if (tool_name == "generate_3d") {
            CommandResult result;
            result.fields["glb_url"] = "https://nova3d.xyz/assets/cabinet.glb";
            result.fields["preview_url"] = "https://nova3d.xyz/preview/state-generate-2";
            result.fields["conversation_url"] = "https://nova3d.xyz/chat/conv-coordinator";
            result.fields["parts"] = "body|door";
            result.fields["joint_count"] = "0";
            result.fields["code_artifact"] = "{\"content\":\"import bpy\"}";
            result.fields["model_artifact"] = "{\"url\":\"https://nova3d.xyz/assets/cabinet.glb\"}";
            result.fields["workflow_id"] = "state-generate-2";
            return result;
        }
        if (tool_name == "add_part") {
            CommandResult result;
            result.fields["glb_url"] = "https://nova3d.xyz/assets/cabinet_v2.glb";
            result.fields["preview_url"] = "https://nova3d.xyz/preview/state-add-2";
            result.fields["parts"] = "body|door|handle";
            result.fields["joint_count"] = "0";
            result.fields["code_artifact"] = "{\"content\":\"import bpy # handle\"}";
            result.fields["model_artifact"] = "{\"url\":\"https://nova3d.xyz/assets/cabinet_v2.glb\"}";
            result.fields["workflow_id"] = "state-add-2";
            return result;
        }
        if (tool_name == "articulate_model") {
            CommandResult result;
            result.fields["glb_url"] = "https://nova3d.xyz/assets/cabinet_v3.glb";
            result.fields["preview_url"] = "https://nova3d.xyz/preview/state-articulate-2";
            result.fields["parts"] = "body|door|handle";
            result.fields["joint_count"] = "1";
            result.fields["code_artifact"] = "{\"content\":\"import bpy # joint\"}";
            result.fields["model_artifact"] = "{\"url\":\"https://nova3d.xyz/assets/cabinet_v3.glb\"}";
            result.fields["workflow_id"] = "state-articulate-2";
            return result;
        }
        CommandResult result;
        result.fields["workflow_id"] = arguments.at("workflow_id");
        result.fields["state"] = "completed";
        result.fields["progress_label"] = "Generating your 3D model...";
        result.fields["current_node"] = "success_final";
        return result;
    }
};

class FakeBlenderMcpInvoker : public IBlenderMcpInvoker {
public:
    CommandResult CallTool(
        const std::string & tool_name,
        const std::map<std::string, std::string> & arguments) override {
        if (tool_name == "blender.import_asset") {
            scene_objects_ =
                "obj_asset_cabinet_part_body:asset_cabinet:part_body:body:0,0,0|"
                "obj_asset_cabinet_part_door:asset_cabinet:part_door:door:0,0,0|"
                "obj_asset_cabinet_part_handle:asset_cabinet:part_handle:handle:0,0,0";
            CommandResult result;
            result.fields["scene_objects"] = scene_objects_;
            result.fields["import_status"] = "ok";
            return result;
        }
        if (tool_name == "blender.transform_object") {
            scene_objects_ =
                "obj_asset_cabinet_part_body:asset_cabinet:part_body:body:0,0,0|"
                "obj_asset_cabinet_part_door:asset_cabinet:part_door:door:" + arguments.at("translation") + "|"
                "obj_asset_cabinet_part_handle:asset_cabinet:part_handle:handle:0,0,0";
            CommandResult result;
            result.fields["scene_objects"] = scene_objects_;
            result.fields["transform_status"] = "ok";
            return result;
        }
        if (tool_name == "blender.capture_viewport") {
            CommandResult result;
            result.fields["image_path"] = "D:/captures/coordinator_view.png";
            return result;
        }
        CommandResult result;
        result.fields["scene_objects"] = scene_objects_;
        return result;
    }

private:
    std::string scene_objects_;
};

void Require(bool condition, const std::string & message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int main() {
    auto state_store = std::make_shared<ThreeDSessionStateStore>();
    auto nova_backend = std::make_shared<Nova3DMcpHostedBackend>(
        std::make_shared<FakeNova3DMcpInvoker>());
    auto blender_backend = std::make_shared<BlenderMcpSceneBackend>(
        std::make_shared<FakeBlenderMcpInvoker>());
    auto generator = std::make_shared<Nova3DAssetAdapter>(nova_backend, state_store, "session_workflow");
    auto scene_adapter = std::make_shared<BlenderSceneAdapter>(blender_backend, state_store, "session_workflow");
    auto orchestrator = std::make_shared<ThreeDOrchestrator>(generator, scene_adapter);

    ThreeDHostedWorkflowCoordinator coordinator(
        state_store,
        nova_backend,
        blender_backend,
        orchestrator,
        "session_workflow");

    CommandResult started = coordinator.StartSession(
        "generate, import, articulate, and inspect",
        "trace_workflow_1");
    Require(started.ok, "session should start");

    CommandResult generated = coordinator.GenerateStructuredAsset("cabinet", "gemini");
    Require(generated.ok, "generate should succeed");
    Require(generated.fields.at("state") == "completed", "generate should merge workflow status");

    CommandResult added = coordinator.AddAssetPart(
        generated.fields.at("asset_id"),
        "handle",
        "gemini");
    Require(added.ok, "add part should succeed");

    CommandResult articulated = coordinator.ArticulateAsset(
        generated.fields.at("asset_id"),
        "make door swing open",
        "gemini");
    Require(articulated.ok, "articulate should succeed");
    Require(articulated.fields.at("joint_count") == "1", "articulation should expose joint count");

    CommandResult imported = coordinator.ImportActiveAsset();
    Require(imported.ok, "import should succeed");

    CommandResult transformed = coordinator.TransformSceneObject(
        "obj_asset_cabinet_part_door",
        Vec3{1.0, 0.0, 0.0},
        Vec3{0.0, 30.0, 0.0},
        Vec3{1.0, 1.0, 1.0});
    Require(transformed.ok, "transform should succeed");

    CommandResult captured = coordinator.CaptureViewport();
    Require(captured.ok, "capture should succeed");
    Require(captured.fields.at("image_path").find("coordinator_view.png") != std::string::npos,
        "capture should expose viewport path");

    CommandResult summary = coordinator.BuildSessionSummary();
    Require(summary.ok, "summary should succeed");
    Require(summary.fields.at("latest_viewport_image_path").find("coordinator_view.png") != std::string::npos,
        "summary should track viewport capture");

    CommandResult viewport_resource = state_store->ReadResource("session://session_workflow/scene/viewport_latest");
    Require(viewport_resource.ok, "viewport resource should read");
    Require(viewport_resource.fields.at("content").find("coordinator_view.png") != std::string::npos,
        "viewport resource should expose image path");

    std::cout << "3D hosted workflow coordinator smoke test passed\n";
    return 0;
}
