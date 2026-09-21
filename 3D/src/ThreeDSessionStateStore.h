#pragma once

#include "ThreeDOrchestrator.h"

#include <memory>

namespace codex_lan_agent_3d {

struct ThreeDSessionHostState {
    std::string nova_backend_name;
    std::string nova_workspace_id;
    std::string nova_endpoint;
    std::string blender_backend_name;
    std::string blender_scene_id;
    std::string blender_scene_name;
    std::string blend_file_path;
};

struct ThreeDSessionRecord {
    std::string session_id;
    std::string task_intent;
    std::string trace_id;
    std::string nova_conversation_id;
    std::string active_asset_id;
    std::string active_workflow_id;
    std::string active_workflow_state;
    std::string active_progress_label;
    std::string active_current_node;
    std::string active_object_id;
    std::string latest_viewport_image_path;
    StructuredAssetRecord active_asset;
    SceneSnapshot latest_scene_snapshot;
    ThreeDSessionHostState hosts;
    std::vector<std::string> known_asset_ids;
    std::vector<std::string> event_log;
};

class ThreeDSessionStateStore {
public:
    CommandResult BeginSession(
        const std::string & session_id,
        const std::string & task_intent,
        const std::string & trace_id);

    void BindNovaHost(
        const std::string & session_id,
        const std::string & backend_name,
        const std::string & workspace_id,
        const std::string & endpoint);
    void BindBlenderHost(
        const std::string & session_id,
        const std::string & backend_name,
        const std::string & scene_id,
        const std::string & scene_name,
        const std::string & blend_file_path);
    void RememberAsset(
        const std::string & session_id,
        const StructuredAssetRecord & asset,
        const std::string & operation);
    void RememberSceneSnapshot(
        const std::string & session_id,
        const SceneSnapshot & snapshot,
        const std::string & operation);
    void RememberWorkflowStatus(
        const std::string & session_id,
        const std::string & workflow_id,
        const std::string & workflow_state,
        const std::string & progress_label,
        const std::string & current_node,
        const std::string & operation);
    void RememberViewportCapture(
        const std::string & session_id,
        const std::string & image_path,
        const std::string & operation);
    void SetActiveObject(
        const std::string & session_id,
        const std::string & object_id);

    CommandResult BuildSessionSummary(const std::string & session_id) const;
    CommandResult BuildResourceCatalog(const std::string & session_id) const;
    CommandResult ReadResource(const std::string & uri) const;

private:
    ThreeDSessionRecord * FindSession(const std::string & session_id);
    const ThreeDSessionRecord * FindSession(const std::string & session_id) const;

    static std::string BuildSessionSummaryText(const ThreeDSessionRecord & record);
    static std::string BuildAssetSummaryText(const StructuredAssetRecord & asset);
    static std::string BuildSceneSummaryText(const SceneSnapshot & snapshot);
    static std::string BuildWorkflowStatusText(const ThreeDSessionRecord & record);
    static std::string BuildViewportText(const ThreeDSessionRecord & record);
    static std::string BuildNovaHostText(const ThreeDSessionRecord & record);
    static std::string BuildBlenderHostText(const ThreeDSessionRecord & record);
    static void AppendUnique(std::vector<std::string> * values, const std::string & value);
    static CommandResult BuildNotFoundResult(
        const std::string & key,
        const std::string & value);

    std::vector<ThreeDSessionRecord> sessions_;
};

}  // namespace codex_lan_agent_3d
