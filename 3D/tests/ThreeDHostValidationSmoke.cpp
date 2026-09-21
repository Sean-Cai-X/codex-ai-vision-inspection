#include "BlenderMcpSceneBackend.h"
#include "Nova3DMcpHostedBackend.h"
#include "Nova3DAssetAdapter.h"
#include "ThreeDHostValidation.h"

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
            result.fields["glb_url"] = "https://nova3d.xyz/assets/validation.glb";
            result.fields["preview_url"] = "https://nova3d.xyz/preview/state-validation-generate";
            result.fields["conversation_url"] = "https://nova3d.xyz/chat/conv-validation";
            result.fields["parts"] = "body|door";
            result.fields["joint_count"] = "0";
            result.fields["code_artifact"] = "{\"content\":\"import bpy\"}";
            result.fields["model_artifact"] = "{\"url\":\"https://nova3d.xyz/assets/validation.glb\"}";
            result.fields["workflow_id"] = "state-validation-generate";
            return result;
        }
        if (tool_name == "add_part") {
            CommandResult result;
            result.fields["glb_url"] = "https://nova3d.xyz/assets/validation_v2.glb";
            result.fields["preview_url"] = "https://nova3d.xyz/preview/state-validation-add";
            result.fields["conversation_url"] = "https://nova3d.xyz/chat/conv-validation";
            result.fields["parts"] = "body|door|handle";
            result.fields["joint_count"] = "0";
            result.fields["code_artifact"] = "{\"content\":\"import bpy # handle\"}";
            result.fields["model_artifact"] = "{\"url\":\"https://nova3d.xyz/assets/validation_v2.glb\"}";
            result.fields["workflow_id"] = "state-validation-add";
            return result;
        }
        if (tool_name == "articulate_model") {
            CommandResult result;
            result.fields["glb_url"] = "https://nova3d.xyz/assets/validation_v3.glb";
            result.fields["preview_url"] = "https://nova3d.xyz/preview/state-validation-articulate";
            result.fields["conversation_url"] = "https://nova3d.xyz/chat/conv-validation";
            result.fields["parts"] = "body|door|handle";
            result.fields["joint_count"] = "1";
            result.fields["code_artifact"] = "{\"content\":\"import bpy # joints\"}";
            result.fields["model_artifact"] = "{\"url\":\"https://nova3d.xyz/assets/validation_v3.glb\"}";
            result.fields["workflow_id"] = "state-validation-articulate";
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
                "obj_asset_validation_part_body:asset_validation:part_body:body:0,0,0|"
                "obj_asset_validation_part_door:asset_validation:part_door:door:0,0,0|"
                "obj_asset_validation_part_handle:asset_validation:part_handle:handle:0,0,0";
            CommandResult result;
            result.fields["scene_objects"] = scene_objects_;
            result.fields["scene_object_count"] = "3";
            result.fields["import_status"] = "ok";
            return result;
        }
        if (tool_name == "blender.transform_object") {
            scene_objects_ =
                "obj_asset_validation_part_body:asset_validation:part_body:body:0,0,0|"
                "obj_asset_validation_part_door:asset_validation:part_door:door:" + arguments.at("translation") + "|"
                "obj_asset_validation_part_handle:asset_validation:part_handle:handle:0,0,0";
            CommandResult result;
            result.fields["scene_objects"] = scene_objects_;
            result.fields["transform_status"] = "ok";
            return result;
        }
        if (tool_name == "blender.capture_viewport") {
            CommandResult result;
            result.fields["image_path"] = "D:/captures/validation_suite.png";
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
    auto coordinator = std::make_shared<ThreeDHostedWorkflowCoordinator>(
        state_store,
        nova_backend,
        blender_backend,
        orchestrator,
        "session_workflow");

    ThreeDHostValidationSuite validation(coordinator, state_store);

    const CommandResult end_to_end = validation.RunEndToEndValidationMatrix(
        "validate nova and blender before mcp binding",
        "trace_validation_1",
        "validation cabinet",
        "gemini");
    Require(end_to_end.ok, "end-to-end validation should pass");
    Require(end_to_end.fields.at("all_passed") == "true", "all checks should pass");
    Require(end_to_end.fields.at("validation_rows").find("flow.nova_matrix:pass") != std::string::npos,
        "nova matrix should pass");
    Require(end_to_end.fields.at("validation_rows").find("flow.blender_matrix:pass") != std::string::npos,
        "blender matrix should pass");
    Require(end_to_end.fields.at("validation_rows").find("scene_viewport_latest") != std::string::npos,
        "viewport resource should be validated");

    std::cout << "3D host validation smoke test passed\n";
    return 0;
}
