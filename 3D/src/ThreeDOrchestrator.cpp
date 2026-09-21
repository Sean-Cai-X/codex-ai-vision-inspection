#include "ThreeDOrchestrator.h"

#include <stdexcept>

namespace codex_lan_agent_3d {

namespace {

std::vector<ToolSpec> BuildToolCatalog() {
    return {
        {"asset.generate_structured",
         "Generate a structured 3D asset with stable asset_id, parts, and workflow state.",
         {{"prompt", "string", true, "user prompt for the structured 3D asset"},
          {"preferred_model", "string", false, "optional preferred foundation model"}}},
        {"asset.regenerate_part",
         "Regenerate one existing part inside a structured asset using the latest code artifact state.",
         {{"asset_id", "string", true, "stable asset identifier"},
          {"part_id", "string", true, "stable part identifier"},
          {"description", "string", true, "replacement part description"},
          {"preferred_model", "string", false, "optional preferred foundation model"}}},
        {"asset.add_part",
         "Append a new part to an existing structured asset while preserving prior parts and session state.",
         {{"asset_id", "string", true, "stable asset identifier"},
          {"description", "string", true, "new part description"},
          {"preferred_model", "string", false, "optional preferred foundation model"}}},
        {"asset.articulate",
         "Add or refine articulated behavior for an existing structured asset.",
         {{"asset_id", "string", true, "stable asset identifier"},
          {"articulation_request", "string", true, "articulation request text"},
          {"preferred_model", "string", false, "optional preferred foundation model"}}},
        {"scene.import_asset",
         "Import a generated structured asset into the scene adapter and create scene object bindings.",
         {{"asset_id", "string", true, "stable asset identifier"}}},
        {"scene.transform_object",
         "Transform one scene object through the adapter using stable object_id values.",
         {{"object_id", "string", true, "stable scene object identifier"},
          {"translation", "vec3", true, "translation in x,y,z"},
          {"rotation", "vec3", true, "rotation in x,y,z"},
          {"scale", "vec3", true, "scale in x,y,z"}}},
        {"scene.get_summary",
         "Read the latest scene object snapshot from the scene adapter.",
         {}},
        {"scene.bridge_summary",
         "Read the unified summary built from cxgeom/cxcloud style scene records and refresh planning.",
         {}},
        {"parser.workflow_summary",
         "Read the registered parser workflow execution summary for 3D-related dispatch flows.",
         {}}
    };
}

CommandResult BuildNotFoundResult(const std::string & field_name, const std::string & field_value) {
    CommandResult result;
    result.ok = false;
    result.exit_code = 1;
    result.fields["error"] = field_name + " not found";
    result.fields[field_name] = field_value;
    return result;
}

}  // namespace

ThreeDOrchestrator::ThreeDOrchestrator(
    std::shared_ptr<IStructuredAssetGenerator> generator,
    std::shared_ptr<ISceneAdapter> scene_adapter)
    : generator_(std::move(generator)),
      scene_adapter_(std::move(scene_adapter)),
      tool_catalog_(BuildToolCatalog()) {
}

CommandResult ThreeDOrchestrator::GenerateStructuredAsset(
    const std::string & prompt,
    const std::string & preferred_model) {
    StructuredAssetRecord asset = generator_->Generate(prompt, preferred_model);
    assets_.push_back(asset);
    return BuildAssetResult(assets_.back(), "generate_structured");
}

CommandResult ThreeDOrchestrator::RegenerateAssetPart(
    const std::string & asset_id,
    const std::string & part_id,
    const std::string & description,
    const std::string & preferred_model) {
    StructuredAssetRecord * asset = FindAsset(asset_id);
    if (asset == nullptr) {
        return BuildNotFoundResult("asset_id", asset_id);
    }
    *asset = generator_->RegeneratePart(*asset, part_id, description, preferred_model);
    return BuildAssetResult(*asset, "regenerate_part");
}

CommandResult ThreeDOrchestrator::AddAssetPart(
    const std::string & asset_id,
    const std::string & description,
    const std::string & preferred_model) {
    StructuredAssetRecord * asset = FindAsset(asset_id);
    if (asset == nullptr) {
        return BuildNotFoundResult("asset_id", asset_id);
    }
    *asset = generator_->AddPart(*asset, description, preferred_model);
    return BuildAssetResult(*asset, "add_part");
}

CommandResult ThreeDOrchestrator::ArticulateAsset(
    const std::string & asset_id,
    const std::string & articulation_request,
    const std::string & preferred_model) {
    StructuredAssetRecord * asset = FindAsset(asset_id);
    if (asset == nullptr) {
        return BuildNotFoundResult("asset_id", asset_id);
    }
    *asset = generator_->Articulate(*asset, articulation_request, preferred_model);
    return BuildAssetResult(*asset, "articulate");
}

CommandResult ThreeDOrchestrator::ImportAssetToScene(const std::string & asset_id) {
    const StructuredAssetRecord * asset = FindAsset(asset_id);
    if (asset == nullptr) {
        return BuildNotFoundResult("asset_id", asset_id);
    }
    CommandResult result = scene_adapter_->ImportAsset(*asset);
    result.fields["asset_id"] = asset->asset_id;
    result.fields["imported_part_count"] = std::to_string(asset->parts.size());
    result.fields["scene_object_count"] = std::to_string(scene_adapter_->ReadSnapshot().objects.size());
    return result;
}

CommandResult ThreeDOrchestrator::TransformSceneObject(
    const std::string & object_id,
    const Vec3 & translation,
    const Vec3 & rotation,
    const Vec3 & scale) {
    CommandResult result = scene_adapter_->TransformObject(object_id, translation, rotation, scale);
    result.fields["object_id"] = object_id;
    result.fields["translation"] = FormatVec3(translation);
    result.fields["rotation"] = FormatVec3(rotation);
    result.fields["scale"] = FormatVec3(scale);
    return result;
}

CommandResult ThreeDOrchestrator::GetAssetSummary(const std::string & asset_id) const {
    const StructuredAssetRecord * asset = FindAsset(asset_id);
    if (asset == nullptr) {
        return BuildNotFoundResult("asset_id", asset_id);
    }
    return BuildAssetResult(*asset, "asset_summary");
}

CommandResult ThreeDOrchestrator::GetSceneSummary() const {
    const SceneSnapshot snapshot = scene_adapter_->ReadSnapshot();
    CommandResult result;
    result.fields["scene_object_count"] = std::to_string(snapshot.objects.size());
    std::vector<std::string> object_keys;
    for (const SceneObjectRecord & object : snapshot.objects) {
        object_keys.push_back(object.object_id + ":" + object.display_name);
    }
    result.fields["scene_objects"] = JoinStrings(object_keys, "|");
    return result;
}

CommandResult ThreeDOrchestrator::GetSceneBridgeSummary() const {
    return scene_bridge_.BuildUnifiedSceneSummary();
}

CommandResult ThreeDOrchestrator::GetParserWorkflowSummary() const {
    return scene_bridge_.BuildWorkflowSummary();
}

void ThreeDOrchestrator::UpsertGeometrySceneRecord(const GeometrySceneConceptRecord & record) {
    scene_bridge_.UpsertGeometryRecord(record);
}

void ThreeDOrchestrator::UpsertCloudSceneRecord(const CloudSceneConceptRecord & record) {
    scene_bridge_.UpsertCloudRecord(record);
}

void ThreeDOrchestrator::RegisterParserWorkflow(const ParserWorkflowConceptRecord & workflow) {
    scene_bridge_.RegisterParserWorkflow(workflow);
}

const std::vector<ToolSpec> & ThreeDOrchestrator::ToolCatalog() const {
    return tool_catalog_;
}

CommandResult ThreeDOrchestrator::BuildAssetResult(
    const StructuredAssetRecord & asset,
    const std::string & operation) const {
    CommandResult result;
    result.fields["operation"] = operation;
    result.fields["asset_id"] = asset.asset_id;
    result.fields["prompt"] = asset.prompt;
    result.fields["workflow_id"] = asset.workflow_id;
    result.fields["conversation_id"] = asset.conversation_id;
    result.fields["conversation_url"] = asset.conversation_url;
    result.fields["glb_url"] = asset.glb_url;
    result.fields["preview_url"] = asset.preview_url;
    result.fields["code_artifact"] = asset.code_artifact;
    result.fields["model_artifact"] = asset.model_artifact;
    result.fields["joint_count"] = std::to_string(asset.joint_count);
    result.fields["source_backend"] = asset.source_backend;
    result.fields["part_count"] = std::to_string(asset.parts.size());

    std::vector<std::string> part_values;
    int articulated_count = 0;
    for (const AssetPart & part : asset.parts) {
        if (part.articulated) {
            ++articulated_count;
        }
        part_values.push_back(part.part_id + ":" + part.name + ":" + part.type);
    }
    result.fields["parts"] = JoinStrings(part_values, "|");
    result.fields["articulated_part_count"] = std::to_string(articulated_count);
    return result;
}

StructuredAssetRecord * ThreeDOrchestrator::FindAsset(const std::string & asset_id) {
    for (StructuredAssetRecord & asset : assets_) {
        if (asset.asset_id == asset_id) {
            return &asset;
        }
    }
    return nullptr;
}

const StructuredAssetRecord * ThreeDOrchestrator::FindAsset(const std::string & asset_id) const {
    for (const StructuredAssetRecord & asset : assets_) {
        if (asset.asset_id == asset_id) {
            return &asset;
        }
    }
    return nullptr;
}

}  // namespace codex_lan_agent_3d
