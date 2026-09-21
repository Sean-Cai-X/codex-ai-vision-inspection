#include "ThreeDOrchestrator.h"

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
        updated.code_artifact = asset.code_artifact + "|regen:" + part_id + ":" + description;
        for (AssetPart & part : updated.parts) {
            if (part.part_id == part_id) {
                part.name = ToKey(description);
                return updated;
            }
        }
        throw std::runtime_error("part_id not found");
    }

    StructuredAssetRecord AddPart(
        const StructuredAssetRecord & asset,
        const std::string & description,
        const std::string & preferred_model) override {
        StructuredAssetRecord updated = asset;
        updated.workflow_id = "wf_add_" + preferred_model;
        const std::string key = ToKey(description);
        updated.parts.push_back({"part_" + key, key, "addon", false});
        updated.code_artifact = asset.code_artifact + "|add:" + description;
        return updated;
    }

    StructuredAssetRecord Articulate(
        const StructuredAssetRecord & asset,
        const std::string & articulation_request,
        const std::string & preferred_model) override {
        StructuredAssetRecord updated = asset;
        updated.workflow_id = "wf_articulate_" + preferred_model;
        updated.code_artifact = asset.code_artifact + "|articulate:" + articulation_request;
        for (AssetPart & part : updated.parts) {
            if (part.type == "door" || part.name.find("door") != std::string::npos) {
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
            SceneObjectRecord object;
            object.object_id = "obj_" + asset.asset_id + "_" + part.part_id;
            object.asset_id = asset.asset_id;
            object.part_id = part.part_id;
            object.display_name = part.name;
            snapshot_.objects.push_back(object);
        }
        CommandResult result;
        result.fields["scene_adapter"] = "fake";
        result.fields["import_status"] = "ok";
        return result;
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
                CommandResult result;
                result.fields["scene_adapter"] = "fake";
                result.fields["transform_status"] = "ok";
                return result;
            }
        }
        CommandResult result;
        result.ok = false;
        result.exit_code = 2;
        result.fields["error"] = "object_id not found";
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
    auto generator = std::make_shared<FakeStructuredAssetGenerator>();
    auto scene_adapter = std::make_shared<FakeSceneAdapter>();
    ThreeDOrchestrator orchestrator(generator, scene_adapter);

    Require(orchestrator.ToolCatalog().size() >= 7, "tool catalog should expose fixed 3D control tools");

    CommandResult generated = orchestrator.GenerateStructuredAsset(
        "Cabinet with one door and one body",
        "gemini");
    Require(generated.ok, "generate should succeed");
    Require(generated.fields.at("asset_id") == "asset_cabinet_with_one_door_and_one_body", "asset id should be stable");
    Require(generated.fields.at("part_count") == "2", "generate should create two parts");

    CommandResult regenerated = orchestrator.RegenerateAssetPart(
        generated.fields.at("asset_id"),
        "part_door",
        "glass door",
        "claude-sonnet");
    Require(regenerated.ok, "regenerate should succeed");
    Require(regenerated.fields.at("workflow_id") == "wf_regenerate_claude-sonnet", "workflow id should update after regenerate");
    Require(regenerated.fields.at("parts").find("glass_door") != std::string::npos, "door part should be renamed");

    CommandResult added = orchestrator.AddAssetPart(
        generated.fields.at("asset_id"),
        "handle bar",
        "gpt-5.5");
    Require(added.ok, "add_part should succeed");
    Require(added.fields.at("part_count") == "3", "part count should grow after add_part");

    CommandResult articulated = orchestrator.ArticulateAsset(
        generated.fields.at("asset_id"),
        "make the door swing open",
        "gemini");
    Require(articulated.ok, "articulate should succeed");
    Require(articulated.fields.at("articulated_part_count") == "1", "one door should become articulated");

    CommandResult imported = orchestrator.ImportAssetToScene(generated.fields.at("asset_id"));
    Require(imported.ok, "scene import should succeed");
    Require(imported.fields.at("scene_object_count") == "3", "scene import should expose three objects after add_part");

    const std::string object_id = "obj_asset_cabinet_with_one_door_and_one_body_part_door";
    CommandResult transformed = orchestrator.TransformSceneObject(
        object_id,
        Vec3{1.0, 2.0, 3.0},
        Vec3{0.0, 90.0, 0.0},
        Vec3{1.0, 1.0, 1.0});
    Require(transformed.ok, "scene transform should succeed");

    CommandResult scene_summary = orchestrator.GetSceneSummary();
    Require(scene_summary.ok, "scene summary should succeed");
    Require(scene_summary.fields.at("scene_objects").find(object_id) != std::string::npos, "scene summary should list transformed object");

    std::cout << "3D smoke test passed\n";
    return 0;
}
