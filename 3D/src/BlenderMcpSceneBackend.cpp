#include "BlenderMcpSceneBackend.h"

namespace codex_lan_agent_3d {

BlenderMcpSceneBackend::BlenderMcpSceneBackend(std::shared_ptr<IBlenderMcpInvoker> invoker)
    : invoker_(std::move(invoker)) {
}

CommandResult BlenderMcpSceneBackend::ImportAsset(
    const std::string & session_id,
    const StructuredAssetRecord & asset,
    SceneSnapshot * snapshot) {
    CommandResult result = invoker_->CallTool(
        "blender.import_asset",
        {
            {"session_id", session_id},
            {"asset_id", asset.asset_id},
            {"glb_url", asset.glb_url},
            {"display_name", asset.prompt}
        });
    if (snapshot != nullptr) {
        *snapshot = BuildSnapshotFromResult(result);
    }
    return result;
}

CommandResult BlenderMcpSceneBackend::TransformObject(
    const std::string & session_id,
    const std::string & object_id,
    const Vec3 & translation,
    const Vec3 & rotation,
    const Vec3 & scale,
    SceneSnapshot * snapshot) {
    CommandResult result = invoker_->CallTool(
        "blender.transform_object",
        {
            {"session_id", session_id},
            {"object_id", object_id},
            {"translation", FormatVec3(translation)},
            {"rotation", FormatVec3(rotation)},
            {"scale", FormatVec3(scale)}
        });
    if (snapshot != nullptr) {
        *snapshot = BuildSnapshotFromResult(result);
    }
    return result;
}

SceneSnapshot BlenderMcpSceneBackend::ReadSnapshot(const std::string & session_id) const {
    return BuildSnapshotFromResult(invoker_->CallTool(
        "blender.get_scene_snapshot",
        {
            {"session_id", session_id}
        }));
}

std::string BlenderMcpSceneBackend::BackendName() const {
    return "blender-mcp";
}

std::string BlenderMcpSceneBackend::SceneId() const {
    return "blender_scene_live";
}

std::string BlenderMcpSceneBackend::SceneName() const {
    return "BlenderMcpScene";
}

std::string BlenderMcpSceneBackend::BlendFilePath() const {
    return "live://blender/current_scene";
}

CommandResult BlenderMcpSceneBackend::CaptureViewport(const std::string & session_id) {
    return invoker_->CallTool(
        "blender.capture_viewport",
        {
            {"session_id", session_id}
        });
}

SceneSnapshot BlenderMcpSceneBackend::BuildSnapshotFromResult(const CommandResult & result) {
    SceneSnapshot snapshot;
    const std::vector<std::string> objects = SplitString(GetOrDefault(result, "scene_objects"), '|');
    for (const std::string & object_row : objects) {
        const std::vector<std::string> parts = SplitString(object_row, ':');
        if (parts.size() < 4) {
            continue;
        }
        SceneObjectRecord object;
        object.object_id = parts[0];
        object.asset_id = parts[1];
        object.part_id = parts[2];
        object.display_name = parts[3];
        if (parts.size() >= 5) {
            Vec3 translation;
            if (TryParseVec3(parts[4], &translation)) {
                object.translation = translation;
            }
        }
        snapshot.objects.push_back(object);
    }
    return snapshot;
}

std::string BlenderMcpSceneBackend::GetOrDefault(
    const CommandResult & result,
    const std::string & key,
    const std::string & fallback) {
    const auto it = result.fields.find(key);
    if (it == result.fields.end()) {
        return fallback;
    }
    return it->second;
}

}  // namespace codex_lan_agent_3d
