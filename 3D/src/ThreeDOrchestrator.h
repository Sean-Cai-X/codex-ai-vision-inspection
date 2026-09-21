#pragma once

#include "ThreeDSceneBridge.h"
#include "ThreeDTypes.h"

#include <memory>

namespace codex_lan_agent_3d {

struct AssetPart {
    std::string part_id;
    std::string name;
    std::string type;
    bool articulated = false;
};

struct StructuredAssetRecord {
    std::string asset_id;
    std::string prompt;
    std::string workflow_id;
    std::string conversation_id;
    std::string conversation_url;
    std::string glb_url;
    std::string preview_url;
    std::string code_artifact;
    std::string model_artifact;
    int joint_count = 0;
    std::string source_backend;
    std::vector<AssetPart> parts;
};

struct SceneObjectRecord {
    std::string object_id;
    std::string asset_id;
    std::string part_id;
    std::string display_name;
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale{1.0, 1.0, 1.0};
};

struct SceneSnapshot {
    std::vector<SceneObjectRecord> objects;
};

struct ToolSpec {
    struct ArgumentSpec {
        std::string name;
        std::string type;
        bool required = true;
        std::string description;
    };

    std::string name;
    std::string description;
    std::vector<ArgumentSpec> arguments;
};

class IStructuredAssetGenerator {
public:
    virtual ~IStructuredAssetGenerator() = default;

    virtual StructuredAssetRecord Generate(
        const std::string & prompt,
        const std::string & preferred_model) = 0;

    virtual StructuredAssetRecord RegeneratePart(
        const StructuredAssetRecord & asset,
        const std::string & part_id,
        const std::string & description,
        const std::string & preferred_model) = 0;

    virtual StructuredAssetRecord AddPart(
        const StructuredAssetRecord & asset,
        const std::string & description,
        const std::string & preferred_model) = 0;

    virtual StructuredAssetRecord Articulate(
        const StructuredAssetRecord & asset,
        const std::string & articulation_request,
        const std::string & preferred_model) = 0;
};

class ISceneAdapter {
public:
    virtual ~ISceneAdapter() = default;

    virtual CommandResult ImportAsset(const StructuredAssetRecord & asset) = 0;
    virtual CommandResult TransformObject(
        const std::string & object_id,
        const Vec3 & translation,
        const Vec3 & rotation,
        const Vec3 & scale) = 0;
    virtual SceneSnapshot ReadSnapshot() const = 0;
};

class ThreeDOrchestrator {
public:
    ThreeDOrchestrator(
        std::shared_ptr<IStructuredAssetGenerator> generator,
        std::shared_ptr<ISceneAdapter> scene_adapter);

    CommandResult GenerateStructuredAsset(
        const std::string & prompt,
        const std::string & preferred_model);

    CommandResult RegenerateAssetPart(
        const std::string & asset_id,
        const std::string & part_id,
        const std::string & description,
        const std::string & preferred_model);

    CommandResult AddAssetPart(
        const std::string & asset_id,
        const std::string & description,
        const std::string & preferred_model);

    CommandResult ArticulateAsset(
        const std::string & asset_id,
        const std::string & articulation_request,
        const std::string & preferred_model);

    CommandResult ImportAssetToScene(const std::string & asset_id);

    CommandResult TransformSceneObject(
        const std::string & object_id,
        const Vec3 & translation,
        const Vec3 & rotation,
        const Vec3 & scale);

    CommandResult GetAssetSummary(const std::string & asset_id) const;
    CommandResult GetSceneSummary() const;
    CommandResult GetSceneBridgeSummary() const;
    CommandResult GetParserWorkflowSummary() const;

    void UpsertGeometrySceneRecord(const GeometrySceneConceptRecord & record);
    void UpsertCloudSceneRecord(const CloudSceneConceptRecord & record);
    void RegisterParserWorkflow(const ParserWorkflowConceptRecord & workflow);

    const std::vector<ToolSpec> & ToolCatalog() const;

private:
    CommandResult BuildAssetResult(
        const StructuredAssetRecord & asset,
        const std::string & operation) const;

    StructuredAssetRecord * FindAsset(const std::string & asset_id);
    const StructuredAssetRecord * FindAsset(const std::string & asset_id) const;

    std::shared_ptr<IStructuredAssetGenerator> generator_;
    std::shared_ptr<ISceneAdapter> scene_adapter_;
    ThreeDSceneBridge scene_bridge_;
    std::vector<StructuredAssetRecord> assets_;
    std::vector<ToolSpec> tool_catalog_;
};

}  // namespace codex_lan_agent_3d
