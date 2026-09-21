#include "BlenderMcpSceneBackend.h"
#include "Nova3DMcpHostedBackend.h"
#include "ThreeDSessionStateStore.h"

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
            const std::string prompt = arguments.at("prompt");
            CommandResult result;
            result.fields["glb_url"] = "https://nova3d.xyz/assets/" + ToKey(prompt) + ".glb";
            result.fields["preview_url"] = "https://nova3d.xyz/preview/state-generate-1";
            result.fields["conversation_url"] = "https://nova3d.xyz/chat/conv-demo";
            result.fields["parts"] = "body|door";
            result.fields["joint_count"] = "0";
            result.fields["code_artifact"] = "{\"content\":\"import bpy\",\"_nova3d_conversation_id\":\"conv-demo\"}";
            result.fields["model_artifact"] = "{\"url\":\"https://nova3d.xyz/assets/demo.glb\"}";
            result.fields["workflow_id"] = "state-generate-1";
            return result;
        }
        if (tool_name == "regenerate_part") {
            CommandResult result;
            result.fields["glb_url"] = "https://nova3d.xyz/assets/updated.glb";
            result.fields["preview_url"] = "https://nova3d.xyz/preview/state-regenerate-1";
            result.fields["conversation_url"] = "https://nova3d.xyz/chat/conv-demo";
            result.fields["parts"] = "body|glass_door";
            result.fields["joint_count"] = "0";
            result.fields["code_artifact"] = "{\"content\":\"import bpy # updated\",\"_nova3d_conversation_id\":\"conv-demo\"}";
            result.fields["model_artifact"] = "{\"url\":\"https://nova3d.xyz/assets/updated.glb\"}";
            result.fields["workflow_id"] = "state-regenerate-1";
            return result;
        }
        if (tool_name == "add_part") {
            CommandResult result;
            result.fields["glb_url"] = "https://nova3d.xyz/assets/expanded.glb";
            result.fields["preview_url"] = "https://nova3d.xyz/preview/state-add-1";
            result.fields["conversation_url"] = "https://nova3d.xyz/chat/conv-demo";
            result.fields["parts"] = "body|glass_door|handle_bar";
            result.fields["joint_count"] = "0";
            result.fields["code_artifact"] = "{\"content\":\"import bpy # handle\",\"_nova3d_conversation_id\":\"conv-demo\"}";
            result.fields["model_artifact"] = "{\"url\":\"https://nova3d.xyz/assets/expanded.glb\"}";
            result.fields["workflow_id"] = "state-add-1";
            return result;
        }
        if (tool_name == "articulate_model") {
            CommandResult result;
            result.fields["glb_url"] = "https://nova3d.xyz/assets/articulated.glb";
            result.fields["preview_url"] = "https://nova3d.xyz/preview/state-articulate-1";
            result.fields["conversation_url"] = "https://nova3d.xyz/chat/conv-demo";
            result.fields["parts"] = "body|glass_door|handle_bar";
            result.fields["joint_count"] = "1";
            result.fields["code_artifact"] = "{\"content\":\"import bpy # joints\",\"_nova3d_conversation_id\":\"conv-demo\"}";
            result.fields["model_artifact"] = "{\"url\":\"https://nova3d.xyz/assets/articulated.glb\"}";
            result.fields["workflow_id"] = "state-articulate-1";
            return result;
        }
        CommandResult status;
        status.fields["workflow_id"] = arguments.at("workflow_id");
        status.fields["state"] = "completed";
        status.fields["progress_label"] = "Generating your 3D model...";
        status.fields["current_node"] = "success_final";
        return status;
    }
};

class FakeBlenderMcpInvoker : public IBlenderMcpInvoker {
public:
    CommandResult CallTool(
        const std::string & tool_name,
        const std::map<std::string, std::string> & arguments) override {
        if (tool_name == "blender.import_asset") {
            scene_objects_ =
                "obj_asset_cabinet_with_door_part_body:asset_cabinet_with_door:part_body:body:0,0,0|"
                "obj_asset_cabinet_with_door_part_glass_door:asset_cabinet_with_door:part_glass_door:glass_door:0,0,0|"
                "obj_asset_cabinet_with_door_part_handle_bar:asset_cabinet_with_door:part_handle_bar:handle_bar:0,0,0";
            CommandResult result;
            result.fields["import_status"] = "ok";
            result.fields["scene_objects"] = scene_objects_;
            result.fields["session_id"] = arguments.at("session_id");
            return result;
        }
        if (tool_name == "blender.transform_object") {
            scene_objects_ =
                "obj_asset_cabinet_with_door_part_body:asset_cabinet_with_door:part_body:body:0,0,0|"
                "obj_asset_cabinet_with_door_part_glass_door:asset_cabinet_with_door:part_glass_door:glass_door:" +
                arguments.at("translation") + "|"
                "obj_asset_cabinet_with_door_part_handle_bar:asset_cabinet_with_door:part_handle_bar:handle_bar:0,0,0";
            CommandResult result;
            result.fields["transform_status"] = "ok";
            result.fields["scene_objects"] = scene_objects_;
            return result;
        }
        if (tool_name == "blender.capture_viewport") {
            CommandResult result;
            result.fields["image_path"] = "D:/captures/session_demo_viewport.png";
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
    state_store->BeginSession("session_demo", "hosted nova to blender flow", "trace_demo_2");

    auto hosted_nova = std::make_shared<Nova3DMcpHostedBackend>(
        std::make_shared<FakeNova3DMcpInvoker>());
    auto hosted_blender = std::make_shared<BlenderMcpSceneBackend>(
        std::make_shared<FakeBlenderMcpInvoker>());
    auto generator = std::make_shared<Nova3DAssetAdapter>(hosted_nova, state_store, "session_demo");
    auto scene_adapter = std::make_shared<BlenderSceneAdapter>(hosted_blender, state_store, "session_demo");

    ThreeDOrchestrator orchestrator(generator, scene_adapter);
    CommandResult generated = orchestrator.GenerateStructuredAsset("cabinet with door", "gemini");
    Require(generated.ok, "hosted generate should succeed");
    Require(generated.fields.at("conversation_url") == "https://nova3d.xyz/chat/conv-demo",
        "conversation url should map through");

    CommandResult regenerated = orchestrator.RegenerateAssetPart(
        generated.fields.at("asset_id"),
        "part_door",
        "glass door",
        "claude-sonnet");
    Require(regenerated.ok, "hosted regenerate should succeed");
    Require(regenerated.fields.at("parts").find("glass_door") != std::string::npos,
        "regenerate should remap part names");

    CommandResult added = orchestrator.AddAssetPart(
        generated.fields.at("asset_id"),
        "handle bar",
        "gpt-5.5");
    Require(added.ok, "hosted add part should succeed");
    Require(added.fields.at("part_count") == "3", "hosted add part should expose third part");

    CommandResult articulated = orchestrator.ArticulateAsset(
        generated.fields.at("asset_id"),
        "make the door swing",
        "gemini");
    Require(articulated.ok, "hosted articulate should succeed");
    Require(articulated.fields.at("joint_count") == "1", "joint count should flow through");

    CommandResult imported = orchestrator.ImportAssetToScene(generated.fields.at("asset_id"));
    Require(imported.ok, "hosted blender import should succeed");
    Require(imported.fields.at("scene_object_count") == "3", "scene should contain imported objects");

    CommandResult transformed = orchestrator.TransformSceneObject(
        "obj_asset_cabinet_with_door_part_glass_door",
        Vec3{2.0, 0.0, 0.0},
        Vec3{0.0, 45.0, 0.0},
        Vec3{1.0, 1.0, 1.0});
    Require(transformed.ok, "hosted blender transform should succeed");

    CommandResult session_summary = state_store->BuildSessionSummary("session_demo");
    Require(session_summary.ok, "session summary should succeed");
    Require(session_summary.fields.at("active_workflow_state") == "completed",
        "workflow state should be tracked");

    CommandResult workflow_resource = state_store->ReadResource("session://session_demo/workflow/nova_status");
    Require(workflow_resource.ok, "workflow resource should read");
    Require(workflow_resource.fields.at("content").find("state=completed") != std::string::npos,
        "workflow resource should include completion state");

    CommandResult status = hosted_nova->GetGenerationStatus("state-articulate-1");
    Require(status.ok, "hosted status should succeed");
    Require(status.fields.at("state") == "completed", "status state should map");

    CommandResult viewport = hosted_blender->CaptureViewport("session_demo");
    Require(viewport.ok, "viewport capture should succeed");
    Require(viewport.fields.at("image_path").find(".png") != std::string::npos,
        "viewport capture should expose image path");

    std::cout << "3D hosted bridge smoke test passed\n";
    return 0;
}
