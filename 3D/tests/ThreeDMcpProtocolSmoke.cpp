#include "ThreeDMcpProtocolBridge.h"
#include "ThreeDSessionStateStore.h"

#include <iostream>
#include <memory>
#include <stdexcept>

using namespace codex_lan_agent_3d;

namespace {

class FakeStructuredAssetGenerator : public IStructuredAssetGenerator {
public:
    StructuredAssetRecord Generate(
        const std::string & prompt,
        const std::string & preferred_model) override {
        StructuredAssetRecord asset;
        asset.asset_id = "asset_" + ToKey(prompt);
        asset.prompt = prompt;
        asset.workflow_id = "wf_generate_" + preferred_model;
        asset.conversation_id = "conv_" + ToKey(prompt);
        asset.glb_url = "https://example.invalid/" + asset.asset_id + ".glb";
        asset.preview_url = "https://example.invalid/preview/" + asset.asset_id;
        asset.code_artifact = "code:" + prompt;
        asset.model_artifact = "model:" + preferred_model;
        asset.parts = {
            {"part_body", "body", "frame", false},
            {"part_door", "door", "door", false}
        };
        return asset;
    }

    StructuredAssetRecord RegeneratePart(
        const StructuredAssetRecord & asset,
        const std::string &,
        const std::string &,
        const std::string & preferred_model) override {
        StructuredAssetRecord updated = asset;
        updated.workflow_id = "wf_regenerate_" + preferred_model;
        return updated;
    }

    StructuredAssetRecord AddPart(
        const StructuredAssetRecord & asset,
        const std::string & description,
        const std::string & preferred_model) override {
        StructuredAssetRecord updated = asset;
        updated.workflow_id = "wf_add_" + preferred_model;
        updated.parts.push_back({"part_" + ToKey(description), ToKey(description), "addon", false});
        return updated;
    }

    StructuredAssetRecord Articulate(
        const StructuredAssetRecord & asset,
        const std::string &,
        const std::string & preferred_model) override {
        StructuredAssetRecord updated = asset;
        updated.workflow_id = "wf_articulate_" + preferred_model;
        for (AssetPart & part : updated.parts) {
            if (part.part_id == "part_door") {
                part.articulated = true;
            }
        }
        return updated;
    }
};

class FakeSceneAdapter : public ISceneAdapter {
public:
    CommandResult ImportAsset(const StructuredAssetRecord & asset) override {
        snapshot_.objects.clear();
        for (const AssetPart & part : asset.parts) {
            snapshot_.objects.push_back(SceneObjectRecord{
                "obj_" + asset.asset_id + "_" + part.part_id,
                asset.asset_id,
                part.part_id,
                part.name,
                {},
                {},
                {1.0, 1.0, 1.0}
            });
        }
        return CommandResult{};
    }

    CommandResult TransformObject(
        const std::string & object_id,
        const Vec3 & translation,
        const Vec3 & rotation,
        const Vec3 & scale) override {
        for (SceneObjectRecord & object : snapshot_.objects) {
            if (object.object_id == object_id) {
                object.translation = translation;
                object.rotation = rotation;
                object.scale = scale;
                return CommandResult{};
            }
        }
        CommandResult result;
        result.ok = false;
        result.exit_code = 3;
        result.fields["error"] = "missing object";
        return result;
    }

    SceneSnapshot ReadSnapshot() const override {
        return snapshot_;
    }

private:
    SceneSnapshot snapshot_;
};

class FakeResourceSeedSceneAdapter : public ISceneAdapter {
public:
    CommandResult ImportAsset(const StructuredAssetRecord &) override {
        return CommandResult{};
    }

    CommandResult TransformObject(
        const std::string &,
        const Vec3 &,
        const Vec3 &,
        const Vec3 &) override {
        return CommandResult{};
    }

    SceneSnapshot ReadSnapshot() const override {
        return SceneSnapshot{};
    }
};

void Require(bool condition, const std::string & message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

}  // namespace

int main() {
    auto orchestrator = std::make_shared<ThreeDOrchestrator>(
        std::make_shared<FakeStructuredAssetGenerator>(),
        std::make_shared<FakeSceneAdapter>());
    auto control_surface = std::make_shared<ThreeDControlSurface>(orchestrator);
    auto adapter = std::make_shared<ThreeDMcpAdapter>(control_surface);
    auto state_store = std::make_shared<ThreeDSessionStateStore>();
    state_store->BeginSession("session_demo", "inspect and iterate", "trace_demo");
    state_store->BindNovaHost("session_demo", "nova3d", "workspace_demo", "http://nova.invalid");
    state_store->BindBlenderHost(
        "session_demo",
        "blender",
        "scene_demo",
        "MainScene",
        "D:/demo/main_scene.blend");
    state_store->RememberAsset(
        "session_demo",
        FakeStructuredAssetGenerator().Generate("Ceiling fan", "gemini"),
        "seed_asset");
    auto resource_adapter = std::make_shared<ThreeDMcpResourceAdapter>(state_store);
    ThreeDMcpProtocolBridge protocol(adapter, resource_adapter);

    const std::string initialize = protocol.BuildInitializeResponse("1");
    Require(initialize.find("\"protocolVersion\":\"2025-03-26\"") != std::string::npos,
        "initialize should expose protocol version");

    const std::string tools = protocol.BuildToolsListResponse("2");
    Require(tools.find("\"name\":\"asset.generate_structured\"") != std::string::npos,
        "tools/list should include asset.generate_structured");
    Require(tools.find("\"preferred_model\"") != std::string::npos,
        "tools/list should include optional preferred_model");
    Require(tools.find("\"additionalProperties\":false") != std::string::npos,
        "tools/list should lock schema extras");

    const std::string described = protocol.BuildToolDescribeResponse("3", "scene.transform_object");
    Require(described.find("\"isError\":false") != std::string::npos,
        "tools/describe should succeed");
    Require(described.find("translation:vec3:required") != std::string::npos,
        "tools/describe should expose required vec3");

    const std::string generated = protocol.BuildToolCallResponse(
        "4",
        "asset.generate_structured",
        {{"prompt", "Ceiling fan"}});
    Require(generated.find("\"isError\":false") != std::string::npos,
        "tools/call should allow optional preferred_model omission");
    Require(generated.find("asset_ceiling_fan") != std::string::npos,
        "tools/call should include generated asset id");

    const std::string bad_call = protocol.Dispatch(
        McpRpcRequestEnvelope{"5", "tools/call", "scene.import_asset", {}});
    Require(bad_call.find("\"isError\":true") != std::string::npos,
        "missing required tool args should surface as tool error payload");

    const std::string unknown_method = protocol.Dispatch(
        McpRpcRequestEnvelope{"6", "toolz/list", "", "", "", {}});
    Require(unknown_method.find("\"code\":-32601") != std::string::npos,
        "unknown method should return method not found");

    const std::string resources = protocol.BuildResourcesListResponse("7", "session_demo");
    Require(resources.find("\"uri\":\"session://session_demo/summary\"") != std::string::npos,
        "resources/list should include session summary");
    Require(resources.find("\"uri\":\"session://session_demo/scene/viewport_latest\"") != std::string::npos,
        "resources/list should include viewport resource");
    Require(resources.find("\"uri\":\"session://session_demo/workflow/nova_status\"") != std::string::npos,
        "resources/list should include workflow status");

    const std::string read_summary = protocol.BuildResourceReadResponse(
        "8",
        "session://session_demo/hosts/blender");
    Require(read_summary.find("scene_id=scene_demo") != std::string::npos,
        "resources/read should include blender scene");

    std::cout << "3D MCP protocol smoke test passed\n";
    return 0;
}
