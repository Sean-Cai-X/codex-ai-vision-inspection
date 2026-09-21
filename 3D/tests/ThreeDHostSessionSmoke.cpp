#include "BlenderSceneAdapter.h"
#include "Nova3DAssetAdapter.h"
#include "ThreeDSessionStateStore.h"

#include <iostream>
#include <memory>
#include <stdexcept>

using namespace codex_lan_agent_3d;

namespace {

class FakeNova3DBackend : public INova3DBackend {
public:
    StructuredAssetRecord Generate(
        const std::string & session_id,
        const std::string & prompt,
        const std::string & preferred_model) override {
        StructuredAssetRecord asset;
        asset.asset_id = "asset_" + ToKey(prompt);
        asset.prompt = prompt;
        asset.workflow_id = "wf_generate_" + preferred_model;
        asset.conversation_id = "nova_conv_" + session_id;
        asset.glb_url = "https://nova.invalid/" + asset.asset_id + ".glb";
        asset.preview_url = "https://nova.invalid/preview/" + asset.asset_id;
        asset.code_artifact = "code:" + prompt;
        asset.model_artifact = "model:" + preferred_model;
        asset.parts = {
            {"part_body", "body", "frame", false},
            {"part_door", "door", "door", false}
        };
        return asset;
    }

    StructuredAssetRecord RegeneratePart(
        const std::string & session_id,
        const StructuredAssetRecord & asset,
        const std::string & part_id,
        const std::string & description,
        const std::string & preferred_model) override {
        StructuredAssetRecord updated = asset;
        updated.workflow_id = "wf_regenerate_" + preferred_model;
        updated.conversation_id = "nova_conv_" + session_id;
        for (AssetPart & part : updated.parts) {
            if (part.part_id == part_id) {
                part.name = ToKey(description);
            }
        }
        return updated;
    }

    StructuredAssetRecord AddPart(
        const std::string & session_id,
        const StructuredAssetRecord & asset,
        const std::string & description,
        const std::string & preferred_model) override {
        StructuredAssetRecord updated = asset;
        updated.workflow_id = "wf_add_" + preferred_model;
        updated.conversation_id = "nova_conv_" + session_id;
        updated.parts.push_back({"part_" + ToKey(description), ToKey(description), "addon", false});
        return updated;
    }

    StructuredAssetRecord Articulate(
        const std::string & session_id,
        const StructuredAssetRecord & asset,
        const std::string &,
        const std::string & preferred_model) override {
        StructuredAssetRecord updated = asset;
        updated.workflow_id = "wf_articulate_" + preferred_model;
        updated.conversation_id = "nova_conv_" + session_id;
        for (AssetPart & part : updated.parts) {
            if (part.part_id == "part_door") {
                part.articulated = true;
            }
        }
        return updated;
    }

    std::string BackendName() const override {
        return "nova3d";
    }

    std::string WorkspaceId() const override {
        return "nova_workspace_main";
    }

    std::string Endpoint() const override {
        return "http://nova3d.invalid/api";
    }
};

class FakeBlenderBackend : public IBlenderSceneBackend {
public:
    CommandResult ImportAsset(
        const std::string &,
        const StructuredAssetRecord & asset,
        SceneSnapshot * snapshot) override {
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
        if (snapshot != nullptr) {
            *snapshot = snapshot_;
        }
        CommandResult result;
        result.fields["import_status"] = "ok";
        return result;
    }

    CommandResult TransformObject(
        const std::string &,
        const std::string & object_id,
        const Vec3 & translation,
        const Vec3 & rotation,
        const Vec3 & scale,
        SceneSnapshot * snapshot) override {
        for (SceneObjectRecord & object : snapshot_.objects) {
            if (object.object_id == object_id) {
                object.translation = translation;
                object.rotation = rotation;
                object.scale = scale;
                if (snapshot != nullptr) {
                    *snapshot = snapshot_;
                }
                CommandResult result;
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

    SceneSnapshot ReadSnapshot(const std::string &) const override {
        return snapshot_;
    }

    std::string BackendName() const override {
        return "blender";
    }

    std::string SceneId() const override {
        return "scene_main";
    }

    std::string SceneName() const override {
        return "MainScene";
    }

    std::string BlendFilePath() const override {
        return "D:/demo/main_scene.blend";
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
    auto state_store = std::make_shared<ThreeDSessionStateStore>();
    CommandResult started = state_store->BeginSession(
        "session_demo",
        "build a cabinet and inspect it in scene",
        "trace_demo_1");
    Require(started.ok, "session should start");

    auto generator = std::make_shared<Nova3DAssetAdapter>(
        std::make_shared<FakeNova3DBackend>(),
        state_store,
        "session_demo");
    auto scene_adapter = std::make_shared<BlenderSceneAdapter>(
        std::make_shared<FakeBlenderBackend>(),
        state_store,
        "session_demo");

    ThreeDOrchestrator orchestrator(generator, scene_adapter);
    CommandResult generated = orchestrator.GenerateStructuredAsset("cabinet body and door", "gemini");
    Require(generated.ok, "generate should succeed");
    CommandResult added = orchestrator.AddAssetPart(
        generated.fields.at("asset_id"),
        "handle bar",
        "gpt-5.5");
    Require(added.ok, "add part should succeed");
    CommandResult imported = orchestrator.ImportAssetToScene(generated.fields.at("asset_id"));
    Require(imported.ok, "import should succeed");
    CommandResult transformed = orchestrator.TransformSceneObject(
        "obj_asset_cabinet_body_and_door_part_door",
        Vec3{1.0, 2.0, 3.0},
        Vec3{0.0, 45.0, 0.0},
        Vec3{1.0, 1.0, 1.0});
    Require(transformed.ok, "transform should succeed");

    CommandResult session_summary = state_store->BuildSessionSummary("session_demo");
    Require(session_summary.ok, "session summary should succeed");
    Require(session_summary.fields.at("active_asset_id") == "asset_cabinet_body_and_door",
        "active asset should be tracked");
    Require(session_summary.fields.at("scene_object_count") == "3",
        "scene object count should be tracked");
    Require(session_summary.fields.at("blender_scene_id") == "scene_main",
        "blender scene binding should be tracked");
    Require(session_summary.fields.at("active_workflow_state") == "completed",
        "workflow state should be present");

    CommandResult resources = state_store->BuildResourceCatalog("session_demo");
    Require(resources.ok, "resource catalog should succeed");
    Require(resources.fields.at("resource_count") == "7", "resource count mismatch");

    CommandResult host_resource = state_store->ReadResource("session://session_demo/hosts/nova3d");
    Require(host_resource.ok, "nova host resource should read");
    Require(host_resource.fields.at("content").find("workspace_id=nova_workspace_main") != std::string::npos,
        "nova resource should include workspace");

    CommandResult workflow_resource = state_store->ReadResource("session://session_demo/workflow/nova_status");
    Require(workflow_resource.ok, "workflow resource should read");
    Require(workflow_resource.fields.at("content").find("state=completed") != std::string::npos,
        "workflow resource should include state");

    state_store->RememberViewportCapture("session_demo", "D:/captures/demo.png", "seed_viewport");
    CommandResult viewport_resource = state_store->ReadResource("session://session_demo/scene/viewport_latest");
    Require(viewport_resource.ok, "viewport resource should read");
    Require(viewport_resource.fields.at("content").find("demo.png") != std::string::npos,
        "viewport resource should include image path");

    std::cout << "3D host session smoke test passed\n";
    return 0;
}
