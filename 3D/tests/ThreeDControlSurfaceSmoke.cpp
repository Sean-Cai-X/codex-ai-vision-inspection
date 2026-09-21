#include "ThreeDControlSurface.h"

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
        const std::string & part_id,
        const std::string & description,
        const std::string & preferred_model) override {
        StructuredAssetRecord updated = asset;
        updated.workflow_id = "wf_regenerate_" + preferred_model;
        for (AssetPart & part : updated.parts) {
            if (part.part_id == part_id) {
                part.name = ToKey(description);
            }
        }
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
    ThreeDControlSurface control(orchestrator);

    CommandResult catalog = control.BuildToolsCatalogResult();
    Require(catalog.ok, "catalog should succeed");
    Require(catalog.fields.at("tool_count") == "9", "catalog should expose nine tools");

    CommandResult generated = control.ExecuteTool(
        "asset.generate_structured",
        {{"prompt", "Storage cabinet"}, {"preferred_model", "gpt-5.5"}});
    Require(generated.ok, "tool generate should succeed");
    Require(generated.fields.at("request_type") == "scene_asset_generation", "request type mismatch");

    CommandResult missing_arg = control.ExecuteTool(
        "asset.generate_structured",
        {{"preferred_model", "gpt-5.5"}});
    Require(!missing_arg.ok, "missing prompt should fail");
    Require(missing_arg.fields.at("error").find("prompt") != std::string::npos, "missing argument should mention prompt");

    const std::string asset_id = generated.fields.at("asset_id");
    CommandResult imported = control.ExecuteTool(
        "scene.import_asset",
        {{"asset_id", asset_id}});
    Require(imported.ok, "import should succeed");

    CommandResult transformed = control.ExecuteTool(
        "scene.transform_object",
        {
            {"object_id", "obj_asset_storage_cabinet_part_door"},
            {"translation", "1,2,3"},
            {"rotation", "0,45,0"},
            {"scale", "1,1,1"}
        });
    Require(transformed.ok, "transform should succeed");
    Require(transformed.fields.at("risk") == "medium", "transform should keep mutation risk");

    CommandResult bad_vec = control.ExecuteTool(
        "scene.transform_object",
        {
            {"object_id", "obj_asset_storage_cabinet_part_door"},
            {"translation", "1,2"},
            {"rotation", "0,45,0"},
            {"scale", "1,1,1"}
        });
    Require(!bad_vec.ok, "bad vec3 should fail");

    CommandResult unknown_tool = control.ExecuteTool(
        "scene.delete_everything",
        {});
    Require(!unknown_tool.ok, "unknown tool should fail");

    std::cout << "3D control surface smoke test passed\n";
    return 0;
}
