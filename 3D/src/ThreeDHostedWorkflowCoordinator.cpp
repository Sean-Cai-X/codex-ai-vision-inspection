#include "ThreeDHostedWorkflowCoordinator.h"

namespace codex_lan_agent_3d {

ThreeDHostedWorkflowCoordinator::ThreeDHostedWorkflowCoordinator(
    std::shared_ptr<ThreeDSessionStateStore> state_store,
    std::shared_ptr<Nova3DMcpHostedBackend> nova_backend,
    std::shared_ptr<BlenderMcpSceneBackend> blender_backend,
    std::shared_ptr<ThreeDOrchestrator> orchestrator,
    std::string session_id)
    : state_store_(std::move(state_store)),
      nova_backend_(std::move(nova_backend)),
      blender_backend_(std::move(blender_backend)),
      orchestrator_(std::move(orchestrator)),
      session_id_(std::move(session_id)) {
}

CommandResult ThreeDHostedWorkflowCoordinator::StartSession(
    const std::string & task_intent,
    const std::string & trace_id) {
    return state_store_->BeginSession(session_id_, task_intent, trace_id);
}

CommandResult ThreeDHostedWorkflowCoordinator::GenerateStructuredAsset(
    const std::string & prompt,
    const std::string & preferred_model) {
    CommandResult result = orchestrator_->GenerateStructuredAsset(prompt, preferred_model);
    CommandResult status = RefreshNovaWorkflowStatus();
    MergeFields(status, &result);
    return result;
}

CommandResult ThreeDHostedWorkflowCoordinator::RegenerateAssetPart(
    const std::string & asset_id,
    const std::string & part_id,
    const std::string & description,
    const std::string & preferred_model) {
    CommandResult result = orchestrator_->RegenerateAssetPart(
        asset_id,
        part_id,
        description,
        preferred_model);
    CommandResult status = RefreshNovaWorkflowStatus();
    MergeFields(status, &result);
    return result;
}

CommandResult ThreeDHostedWorkflowCoordinator::AddAssetPart(
    const std::string & asset_id,
    const std::string & description,
    const std::string & preferred_model) {
    CommandResult result = orchestrator_->AddAssetPart(asset_id, description, preferred_model);
    CommandResult status = RefreshNovaWorkflowStatus();
    MergeFields(status, &result);
    return result;
}

CommandResult ThreeDHostedWorkflowCoordinator::ArticulateAsset(
    const std::string & asset_id,
    const std::string & articulation_request,
    const std::string & preferred_model) {
    CommandResult result = orchestrator_->ArticulateAsset(
        asset_id,
        articulation_request,
        preferred_model);
    CommandResult status = RefreshNovaWorkflowStatus();
    MergeFields(status, &result);
    return result;
}

CommandResult ThreeDHostedWorkflowCoordinator::ImportActiveAsset() {
    CommandResult session_summary = state_store_->BuildSessionSummary(session_id_);
    if (!session_summary.ok) {
        return session_summary;
    }
    return orchestrator_->ImportAssetToScene(session_summary.fields["active_asset_id"]);
}

CommandResult ThreeDHostedWorkflowCoordinator::TransformSceneObject(
    const std::string & object_id,
    const Vec3 & translation,
    const Vec3 & rotation,
    const Vec3 & scale) {
    return orchestrator_->TransformSceneObject(object_id, translation, rotation, scale);
}

CommandResult ThreeDHostedWorkflowCoordinator::RefreshNovaWorkflowStatus() {
    CommandResult session_summary = state_store_->BuildSessionSummary(session_id_);
    if (!session_summary.ok) {
        return session_summary;
    }

    const auto it = session_summary.fields.find("active_workflow_id");
    if (it == session_summary.fields.end() || it->second.empty()) {
        CommandResult result;
        result.ok = false;
        result.exit_code = 1;
        result.fields["error"] = "active_workflow_id not available";
        return result;
    }

    CommandResult status = nova_backend_->GetGenerationStatus(it->second);
    if (status.ok) {
        state_store_->RememberWorkflowStatus(
            session_id_,
            it->second,
            status.fields["state"],
            status.fields["progress_label"],
            status.fields["current_node"],
            "refresh_nova_status");
    }
    return status;
}

CommandResult ThreeDHostedWorkflowCoordinator::CaptureViewport() {
    CommandResult result = blender_backend_->CaptureViewport(session_id_);
    if (result.ok) {
        const auto it = result.fields.find("image_path");
        if (it != result.fields.end()) {
            state_store_->RememberViewportCapture(session_id_, it->second, "capture_viewport");
        }
    }
    return result;
}

CommandResult ThreeDHostedWorkflowCoordinator::BuildSessionSummary() const {
    return state_store_->BuildSessionSummary(session_id_);
}

void ThreeDHostedWorkflowCoordinator::MergeFields(const CommandResult & source, CommandResult * target) {
    if (target == nullptr) {
        return;
    }
    for (const auto & entry : source.fields) {
        if (target->fields.find(entry.first) == target->fields.end()) {
            target->fields[entry.first] = entry.second;
        }
    }
}

}  // namespace codex_lan_agent_3d
