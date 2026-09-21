#include "ThreeDMcpAdapter.h"

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
    ThreeDMcpAdapter adapter(control_surface);

    CommandResult tools = adapter.BuildToolsListResult();
    Require(tools.ok, "tools/list should succeed");
    Require(tools.fields.at("tool_count") == "9", "tools/list count mismatch");
    Require(tools.fields.at("mutation_tool_count") == "6", "mutation count mismatch");
    Require(tools.fields.at("observe_tool_count") == "3", "observe count mismatch");

    CommandResult transform_description = adapter.DescribeTool("scene.transform_object");
    Require(transform_description.ok, "describe tool should succeed");
    Require(transform_description.fields.at("input_schema").find("translation:vec3:required") != std::string::npos,
        "transform schema should expose vec3");

    CommandResult generated = adapter.CallTool(
        "asset.generate_structured",
        {{"prompt", "Desk lamp"}, {"preferred_model", "gemini"}});
    Require(generated.ok, "tools/call generate should succeed");
    Require(generated.fields.at("mcp_method") == "tools/call", "call method mismatch");

    CommandResult unexpected = adapter.CallTool(
        "asset.generate_structured",
        {{"prompt", "Desk lamp"}, {"preferred_model", "gemini"}, {"unknown", "value"}});
    Require(!unexpected.ok, "unexpected argument should fail");
    Require(unexpected.fields.at("error").find("unexpected argument") != std::string::npos,
        "unexpected argument error mismatch");

    CommandResult missing = adapter.CallTool(
        "scene.import_asset",
        {});
    Require(!missing.ok, "missing required asset_id should fail");

    std::cout << "3D MCP adapter smoke test passed\n";
    return 0;
}
