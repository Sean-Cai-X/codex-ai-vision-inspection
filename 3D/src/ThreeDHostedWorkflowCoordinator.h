#pragma once

#include "BlenderMcpSceneBackend.h"
#include "Nova3DMcpHostedBackend.h"
#include "ThreeDSessionStateStore.h"

namespace codex_lan_agent_3d {

class ThreeDHostedWorkflowCoordinator {
public:
    ThreeDHostedWorkflowCoordinator(
        std::shared_ptr<ThreeDSessionStateStore> state_store,
        std::shared_ptr<Nova3DMcpHostedBackend> nova_backend,
        std::shared_ptr<BlenderMcpSceneBackend> blender_backend,
        std::shared_ptr<ThreeDOrchestrator> orchestrator,
        std::string session_id);

    CommandResult StartSession(
        const std::string & task_intent,
        const std::string & trace_id);
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
    CommandResult ImportActiveAsset();
    CommandResult TransformSceneObject(
        const std::string & object_id,
        const Vec3 & translation,
        const Vec3 & rotation,
        const Vec3 & scale);
    CommandResult RefreshNovaWorkflowStatus();
    CommandResult CaptureViewport();
    CommandResult BuildSessionSummary() const;

private:
    static void MergeFields(const CommandResult & source, CommandResult * target);

    std::shared_ptr<ThreeDSessionStateStore> state_store_;
    std::shared_ptr<Nova3DMcpHostedBackend> nova_backend_;
    std::shared_ptr<BlenderMcpSceneBackend> blender_backend_;
    std::shared_ptr<ThreeDOrchestrator> orchestrator_;
    std::string session_id_;
};

}  // namespace codex_lan_agent_3d
