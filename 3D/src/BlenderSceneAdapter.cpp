#include "BlenderSceneAdapter.h"

namespace codex_lan_agent_3d {

BlenderSceneAdapter::BlenderSceneAdapter(
    std::shared_ptr<IBlenderSceneBackend> backend,
    std::shared_ptr<ThreeDSessionStateStore> state_store,
    std::string session_id)
    : backend_(std::move(backend)),
      state_store_(std::move(state_store)),
      session_id_(std::move(session_id)) {
    if (state_store_ != nullptr && backend_ != nullptr) {
        state_store_->BindBlenderHost(
            session_id_,
            backend_->BackendName(),
            backend_->SceneId(),
            backend_->SceneName(),
            backend_->BlendFilePath());
    }
}

CommandResult BlenderSceneAdapter::ImportAsset(const StructuredAssetRecord & asset) {
    SceneSnapshot snapshot;
    CommandResult result = backend_->ImportAsset(session_id_, asset, &snapshot);
    if (result.ok && state_store_ != nullptr) {
        state_store_->RememberSceneSnapshot(session_id_, snapshot, "blender_import_asset");
    }
    return result;
}

CommandResult BlenderSceneAdapter::TransformObject(
    const std::string & object_id,
    const Vec3 & translation,
    const Vec3 & rotation,
    const Vec3 & scale) {
    SceneSnapshot snapshot;
    CommandResult result = backend_->TransformObject(
        session_id_,
        object_id,
        translation,
        rotation,
        scale,
        &snapshot);
    if (result.ok && state_store_ != nullptr) {
        state_store_->RememberSceneSnapshot(session_id_, snapshot, "blender_transform_object");
        state_store_->SetActiveObject(session_id_, object_id);
    }
    return result;
}

SceneSnapshot BlenderSceneAdapter::ReadSnapshot() const {
    return backend_->ReadSnapshot(session_id_);
}

CommandResult BlenderSceneAdapter::BuildHostSummary() const {
    CommandResult result;
    result.fields["session_id"] = session_id_;
    result.fields["backend_name"] = backend_ == nullptr ? "" : backend_->BackendName();
    result.fields["scene_id"] = backend_ == nullptr ? "" : backend_->SceneId();
    result.fields["scene_name"] = backend_ == nullptr ? "" : backend_->SceneName();
    result.fields["blend_file_path"] = backend_ == nullptr ? "" : backend_->BlendFilePath();
    result.fields["host_role"] = "scene_visualization_and_control";
    return result;
}

}  // namespace codex_lan_agent_3d
