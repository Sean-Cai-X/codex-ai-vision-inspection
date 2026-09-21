#include "Nova3DAssetAdapter.h"

namespace codex_lan_agent_3d {

Nova3DAssetAdapter::Nova3DAssetAdapter(
    std::shared_ptr<INova3DBackend> backend,
    std::shared_ptr<ThreeDSessionStateStore> state_store,
    std::string session_id)
    : backend_(std::move(backend)),
      state_store_(std::move(state_store)),
      session_id_(std::move(session_id)) {
    if (state_store_ != nullptr && backend_ != nullptr) {
        state_store_->BindNovaHost(
            session_id_,
            backend_->BackendName(),
            backend_->WorkspaceId(),
            backend_->Endpoint());
    }
}

StructuredAssetRecord Nova3DAssetAdapter::Generate(
    const std::string & prompt,
    const std::string & preferred_model) {
    StructuredAssetRecord asset = backend_->Generate(session_id_, prompt, preferred_model);
    if (state_store_ != nullptr) {
        state_store_->RememberAsset(session_id_, asset, "nova_generate");
        state_store_->RememberWorkflowStatus(
            session_id_,
            asset.workflow_id,
            "completed",
            "Completed: structured asset generation",
            "sketch_to_3d_generator",
            "nova_generate");
    }
    return asset;
}

StructuredAssetRecord Nova3DAssetAdapter::RegeneratePart(
    const StructuredAssetRecord & asset,
    const std::string & part_id,
    const std::string & description,
    const std::string & preferred_model) {
    StructuredAssetRecord updated = backend_->RegeneratePart(
        session_id_,
        asset,
        part_id,
        description,
        preferred_model);
    if (state_store_ != nullptr) {
        state_store_->RememberAsset(session_id_, updated, "nova_regenerate_part");
        state_store_->RememberWorkflowStatus(
            session_id_,
            updated.workflow_id,
            "completed",
            "Completed: regenerate part",
            "regenerate_3d_part",
            "nova_regenerate_part");
    }
    return updated;
}

StructuredAssetRecord Nova3DAssetAdapter::AddPart(
    const StructuredAssetRecord & asset,
    const std::string & description,
    const std::string & preferred_model) {
    StructuredAssetRecord updated = backend_->AddPart(
        session_id_,
        asset,
        description,
        preferred_model);
    if (state_store_ != nullptr) {
        state_store_->RememberAsset(session_id_, updated, "nova_add_part");
        state_store_->RememberWorkflowStatus(
            session_id_,
            updated.workflow_id,
            "completed",
            "Completed: add part",
            "add_3d_part",
            "nova_add_part");
    }
    return updated;
}

StructuredAssetRecord Nova3DAssetAdapter::Articulate(
    const StructuredAssetRecord & asset,
    const std::string & articulation_request,
    const std::string & preferred_model) {
    StructuredAssetRecord updated = backend_->Articulate(
        session_id_,
        asset,
        articulation_request,
        preferred_model);
    if (state_store_ != nullptr) {
        state_store_->RememberAsset(session_id_, updated, "nova_articulate");
        state_store_->RememberWorkflowStatus(
            session_id_,
            updated.workflow_id,
            "completed",
            "Completed: articulate model",
            "articulate_3d_model",
            "nova_articulate");
    }
    return updated;
}

CommandResult Nova3DAssetAdapter::BuildHostSummary() const {
    CommandResult result;
    result.fields["session_id"] = session_id_;
    result.fields["backend_name"] = backend_ == nullptr ? "" : backend_->BackendName();
    result.fields["workspace_id"] = backend_ == nullptr ? "" : backend_->WorkspaceId();
    result.fields["endpoint"] = backend_ == nullptr ? "" : backend_->Endpoint();
    result.fields["host_role"] = "structured_asset_generation";
    return result;
}

}  // namespace codex_lan_agent_3d
