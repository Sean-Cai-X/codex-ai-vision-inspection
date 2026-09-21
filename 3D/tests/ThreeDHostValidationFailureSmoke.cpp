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
            result.fields["glb_url"] = "https://nova3d.xyz/assets/failure.glb";
            result.fields["preview_url"] = "https://nova3d.xyz/preview/state-failure-generate";
            result.fields["conversation_url"] = "https://nova3d.xyz/chat/conv-failure";
            result.fields["parts"] = "body|door";
            result.fields["joint_count"] = "0";
            result.fields["code_artifact"] = "{\"content\":\"import bpy\"}";
            result.fields["model_artifact"] = "{\"url\":\"https://nova3d.xyz/assets/failure.glb\"}";
            result.fields["workflow_id"] = "state-failure-generate";
            return result;
        }
        if (tool_name == "add_part") {
            CommandResult result;
            result.fields["glb_url"] = "https://nova3d.xyz/assets/failure_v2.glb";
            result.fields["preview_url"] = "https://nova3d.xyz/preview/state-failure-add";
            result.fields["conversation_url"] = "https://nova3d.xyz/chat/conv-failure";
            result.fields["parts"] = "body|door|handle";
            result.fields["joint_count"] = "0";
            result.fields["code_artifact"] = "{\"content\":\"import bpy # handle\"}";
            result.fields["model_artifact"] = "{\"url\":\"https://nova3d.xyz/assets/failure_v2.glb\"}";
            result.fields["workflow_id"] = "state-failure-add";
            return result;
        }
        if (tool_name == "articulate_model") {
            CommandResult result;
            result.fields["glb_url"] = "https://nova3d.xyz/assets/failure_v3.glb";
            result.fields["preview_url"] = "https://nova3d.xyz/preview/state-failure-articulate";
            result.fields["conversation_url"] = "https://nova3d.xyz/chat/conv-failure";
            result.fields["parts"] = "body|door|handle";
            result.fields["joint_count"] = "1";
            result.fields["code_artifact"] = "{\"content\":\"import bpy # joints\"}";
            result.fields["model_artifact"] = "{\"url\":\"https://nova3d.xyz/assets/failure_v3.glb\"}";
            result.fields["workflow_id"] = "state-failure-articulate";
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

class FailingBlenderMcpInvoker : public IBlenderMcpInvoker {
public:
    CommandResult CallTool(
        const std::string & tool_name,
        const std::map<std::string, std::string> & arguments) override {
        if (tool_name == "blender.import_asset") {
            scene_objects_ =
                "obj_asset_failure_part_body:asset_failure:part_body:body:0,0,0|"
                "obj_asset_failure_part_door:asset_failure:part_door:door:0,0,0|"
                "obj_asset_failure_part_handle:asset_failure:part_handle:handle:0,0,0";
            CommandResult result;
            result.fields["scene_objects"] = scene_objects_;
            result.fields["scene_object_count"] = "3";
            result.fields["import_status"] = "ok";
            return result;
        }
        if (tool_name == "blender.transform_object") {
            CommandResult result;
            result.fields["scene_objects"] = scene_objects_;
            result.fields["transform_status"] = "ok";
            return result;
        }
        if (tool_name == "blender.capture_viewport") {
            CommandResult result;
            result.ok = false;
            result.exit_code = 2;
            result.fields["error"] = "viewport capture unavailable";
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

std::string DescribeResult(const CommandResult & result) {
    std::string description =
        "ok=" + std::string(result.ok ? "true" : "false") +
        ", exit_code=" + std::to_string(result.exit_code);
    for (const auto & entry : result.fields) {
        description += ", " + entry.first + "=" + entry.second;
    }
    return description;
}

}  // namespace

int main() {
    try {
        auto state_store = std::make_shared<ThreeDSessionStateStore>();
        auto nova_backend = std::make_shared<Nova3DMcpHostedBackend>(
            std::make_shared<FakeNova3DMcpInvoker>());
        auto blender_backend = std::make_shared<BlenderMcpSceneBackend>(
            std::make_shared<FailingBlenderMcpInvoker>());
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
            "validate failure surfacing",
            "trace_validation_failure",
            "failure cabinet",
            "gemini");
        Require(!end_to_end.ok, "validation should fail when viewport capture fails: " + DescribeResult(end_to_end));
        Require(
            end_to_end.fields.at("failed_count") != "0",
            "failed count should be nonzero: " + DescribeResult(end_to_end));

        const CommandResult blender = validation.RunBlenderValidationMatrix();
        Require(!blender.ok, "blender validation matrix should fail: " + DescribeResult(blender));
        Require(
            blender.fields.at("validation_rows").find("blender.viewport_capture:fail") != std::string::npos,
            "viewport capture failure should be surfaced at blender matrix level: " + DescribeResult(blender));

        std::cout << "3D host validation failure smoke test passed\n";
        return 0;
    }
    catch (const std::exception & ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    }
}
