#include "ThreeDSessionStateStore.h"

namespace codex_lan_agent_3d {

namespace {

std::string BuildSessionResourceUri(const std::string & session_id, const std::string & name) {
    return "session://" + session_id + "/" + name;
}

}  // namespace

CommandResult ThreeDSessionStateStore::BeginSession(
    const std::string & session_id,
    const std::string & task_intent,
    const std::string & trace_id) {
    ThreeDSessionRecord * existing = FindSession(session_id);
    if (existing == nullptr) {
        sessions_.push_back(ThreeDSessionRecord{});
        existing = &sessions_.back();
        existing->session_id = session_id;
    }

    existing->task_intent = task_intent;
    existing->trace_id = trace_id;
    existing->event_log.push_back("begin_session:" + task_intent);

    CommandResult result;
    result.fields["session_id"] = session_id;
    result.fields["task_intent"] = task_intent;
    result.fields["trace_id"] = trace_id;
    result.fields["status"] = "session_ready";
    return result;
}

void ThreeDSessionStateStore::BindNovaHost(
    const std::string & session_id,
    const std::string & backend_name,
    const std::string & workspace_id,
    const std::string & endpoint) {
    ThreeDSessionRecord * session = FindSession(session_id);
    if (session == nullptr) {
        return;
    }
    session->hosts.nova_backend_name = backend_name;
    session->hosts.nova_workspace_id = workspace_id;
    session->hosts.nova_endpoint = endpoint;
    session->event_log.push_back("bind_nova_host:" + backend_name);
}

void ThreeDSessionStateStore::BindBlenderHost(
    const std::string & session_id,
    const std::string & backend_name,
    const std::string & scene_id,
    const std::string & scene_name,
    const std::string & blend_file_path) {
    ThreeDSessionRecord * session = FindSession(session_id);
    if (session == nullptr) {
        return;
    }
    session->hosts.blender_backend_name = backend_name;
    session->hosts.blender_scene_id = scene_id;
    session->hosts.blender_scene_name = scene_name;
    session->hosts.blend_file_path = blend_file_path;
    session->event_log.push_back("bind_blender_host:" + scene_id);
}

void ThreeDSessionStateStore::RememberAsset(
    const std::string & session_id,
    const StructuredAssetRecord & asset,
    const std::string & operation) {
    ThreeDSessionRecord * session = FindSession(session_id);
    if (session == nullptr) {
        return;
    }
    session->active_asset = asset;
    session->active_asset_id = asset.asset_id;
    session->active_workflow_id = asset.workflow_id;
    session->nova_conversation_id = asset.conversation_id;
    AppendUnique(&session->known_asset_ids, asset.asset_id);
    session->event_log.push_back(operation + ":" + asset.asset_id);
}

void ThreeDSessionStateStore::RememberSceneSnapshot(
    const std::string & session_id,
    const SceneSnapshot & snapshot,
    const std::string & operation) {
    ThreeDSessionRecord * session = FindSession(session_id);
    if (session == nullptr) {
        return;
    }
    session->latest_scene_snapshot = snapshot;
    if (!snapshot.objects.empty()) {
        session->active_object_id = snapshot.objects.back().object_id;
    }
    session->event_log.push_back(operation + ":scene_objects=" + std::to_string(snapshot.objects.size()));
}

void ThreeDSessionStateStore::RememberWorkflowStatus(
    const std::string & session_id,
    const std::string & workflow_id,
    const std::string & workflow_state,
    const std::string & progress_label,
    const std::string & current_node,
    const std::string & operation) {
    ThreeDSessionRecord * session = FindSession(session_id);
    if (session == nullptr) {
        return;
    }
    session->active_workflow_id = workflow_id;
    session->active_workflow_state = workflow_state;
    session->active_progress_label = progress_label;
    session->active_current_node = current_node;
    session->event_log.push_back(operation + ":" + workflow_id + ":" + workflow_state);
}

void ThreeDSessionStateStore::RememberViewportCapture(
    const std::string & session_id,
    const std::string & image_path,
    const std::string & operation) {
    ThreeDSessionRecord * session = FindSession(session_id);
    if (session == nullptr) {
        return;
    }
    session->latest_viewport_image_path = image_path;
    session->event_log.push_back(operation + ":" + image_path);
}

void ThreeDSessionStateStore::SetActiveObject(
    const std::string & session_id,
    const std::string & object_id) {
    ThreeDSessionRecord * session = FindSession(session_id);
    if (session == nullptr) {
        return;
    }
    session->active_object_id = object_id;
    session->event_log.push_back("active_object:" + object_id);
}

CommandResult ThreeDSessionStateStore::BuildSessionSummary(const std::string & session_id) const {
    const ThreeDSessionRecord * session = FindSession(session_id);
    if (session == nullptr) {
        return BuildNotFoundResult("session_id", session_id);
    }

    CommandResult result;
    result.fields["session_id"] = session->session_id;
    result.fields["task_intent"] = session->task_intent;
    result.fields["trace_id"] = session->trace_id;
    result.fields["nova_conversation_id"] = session->nova_conversation_id;
    result.fields["active_asset_id"] = session->active_asset_id;
    result.fields["active_workflow_id"] = session->active_workflow_id;
    result.fields["active_workflow_state"] = session->active_workflow_state;
    result.fields["active_progress_label"] = session->active_progress_label;
    result.fields["active_current_node"] = session->active_current_node;
    result.fields["active_object_id"] = session->active_object_id;
    result.fields["latest_viewport_image_path"] = session->latest_viewport_image_path;
    result.fields["known_asset_count"] = std::to_string(session->known_asset_ids.size());
    result.fields["known_asset_ids"] = JoinStrings(session->known_asset_ids, "|");
    result.fields["scene_object_count"] = std::to_string(session->latest_scene_snapshot.objects.size());
    result.fields["blender_scene_id"] = session->hosts.blender_scene_id;
    result.fields["nova_workspace_id"] = session->hosts.nova_workspace_id;
    result.fields["resource_count"] = "7";
    result.fields["summary_text"] = BuildSessionSummaryText(*session);
    return result;
}

CommandResult ThreeDSessionStateStore::BuildResourceCatalog(const std::string & session_id) const {
    const ThreeDSessionRecord * session = FindSession(session_id);
    if (session == nullptr) {
        return BuildNotFoundResult("session_id", session_id);
    }

    CommandResult result;
    result.fields["session_id"] = session_id;
    result.fields["resource_count"] = "7";
    result.fields["resource_uris"] = JoinStrings(
        {
            BuildSessionResourceUri(session_id, "summary"),
            BuildSessionResourceUri(session_id, "asset/current"),
            BuildSessionResourceUri(session_id, "scene/current"),
            BuildSessionResourceUri(session_id, "scene/viewport_latest"),
            BuildSessionResourceUri(session_id, "workflow/nova_status"),
            BuildSessionResourceUri(session_id, "hosts/nova3d"),
            BuildSessionResourceUri(session_id, "hosts/blender")
        },
        "|");
    return result;
}

CommandResult ThreeDSessionStateStore::ReadResource(const std::string & uri) const {
    const std::string prefix = "session://";
    if (uri.find(prefix) != 0) {
        return BuildNotFoundResult("uri", uri);
    }

    const std::string tail = uri.substr(prefix.size());
    const std::size_t slash = tail.find('/');
    if (slash == std::string::npos) {
        return BuildNotFoundResult("uri", uri);
    }

    const std::string session_id = tail.substr(0, slash);
    const std::string path = tail.substr(slash + 1);
    const ThreeDSessionRecord * session = FindSession(session_id);
    if (session == nullptr) {
        return BuildNotFoundResult("session_id", session_id);
    }

    CommandResult result;
    result.fields["uri"] = uri;
    result.fields["session_id"] = session_id;
    result.fields["mime_type"] = "text/plain";
    if (path == "summary") {
        result.fields["resource_name"] = "summary";
        result.fields["content"] = BuildSessionSummaryText(*session);
    }
    else if (path == "asset/current") {
        result.fields["resource_name"] = "asset_current";
        result.fields["content"] = BuildAssetSummaryText(session->active_asset);
    }
    else if (path == "scene/current") {
        result.fields["resource_name"] = "scene_current";
        result.fields["content"] = BuildSceneSummaryText(session->latest_scene_snapshot);
    }
    else if (path == "scene/viewport_latest") {
        result.fields["resource_name"] = "scene_viewport_latest";
        result.fields["content"] = BuildViewportText(*session);
    }
    else if (path == "workflow/nova_status") {
        result.fields["resource_name"] = "workflow_nova_status";
        result.fields["content"] = BuildWorkflowStatusText(*session);
    }
    else if (path == "hosts/nova3d") {
        result.fields["resource_name"] = "hosts_nova3d";
        result.fields["content"] = BuildNovaHostText(*session);
    }
    else if (path == "hosts/blender") {
        result.fields["resource_name"] = "hosts_blender";
        result.fields["content"] = BuildBlenderHostText(*session);
    }
    else {
        return BuildNotFoundResult("uri", uri);
    }
    return result;
}

ThreeDSessionRecord * ThreeDSessionStateStore::FindSession(const std::string & session_id) {
    for (ThreeDSessionRecord & session : sessions_) {
        if (session.session_id == session_id) {
            return &session;
        }
    }
    return nullptr;
}

const ThreeDSessionRecord * ThreeDSessionStateStore::FindSession(const std::string & session_id) const {
    for (const ThreeDSessionRecord & session : sessions_) {
        if (session.session_id == session_id) {
            return &session;
        }
    }
    return nullptr;
}

std::string ThreeDSessionStateStore::BuildSessionSummaryText(const ThreeDSessionRecord & record) {
    std::ostringstream stream;
    stream << "session_id=" << record.session_id << "\n"
           << "task_intent=" << record.task_intent << "\n"
           << "trace_id=" << record.trace_id << "\n"
           << "active_asset_id=" << record.active_asset_id << "\n"
           << "active_workflow_id=" << record.active_workflow_id << "\n"
           << "active_workflow_state=" << record.active_workflow_state << "\n"
           << "active_progress_label=" << record.active_progress_label << "\n"
           << "active_current_node=" << record.active_current_node << "\n"
           << "active_object_id=" << record.active_object_id << "\n"
           << "latest_viewport_image_path=" << record.latest_viewport_image_path << "\n"
           << "nova_conversation_id=" << record.nova_conversation_id << "\n"
           << "known_asset_ids=" << JoinStrings(record.known_asset_ids, "|") << "\n"
           << "scene_object_count=" << record.latest_scene_snapshot.objects.size() << "\n"
           << "blender_scene_id=" << record.hosts.blender_scene_id << "\n"
           << "nova_workspace_id=" << record.hosts.nova_workspace_id << "\n";
    return stream.str();
}

std::string ThreeDSessionStateStore::BuildAssetSummaryText(const StructuredAssetRecord & asset) {
    std::ostringstream stream;
    stream << "asset_id=" << asset.asset_id << "\n"
           << "workflow_id=" << asset.workflow_id << "\n"
           << "conversation_id=" << asset.conversation_id << "\n"
           << "conversation_url=" << asset.conversation_url << "\n"
           << "preview_url=" << asset.preview_url << "\n"
           << "glb_url=" << asset.glb_url << "\n"
           << "source_backend=" << asset.source_backend << "\n"
           << "part_count=" << asset.parts.size() << "\n";
    for (const AssetPart & part : asset.parts) {
        stream << "part=" << part.part_id << ":" << part.name << ":" << part.type << "\n";
    }
    return stream.str();
}

std::string ThreeDSessionStateStore::BuildSceneSummaryText(const SceneSnapshot & snapshot) {
    std::ostringstream stream;
    stream << "scene_object_count=" << snapshot.objects.size() << "\n";
    for (const SceneObjectRecord & object : snapshot.objects) {
        stream << "object=" << object.object_id
               << ":" << object.asset_id
               << ":" << object.part_id
               << ":" << FormatVec3(object.translation)
               << "\n";
    }
    return stream.str();
}

std::string ThreeDSessionStateStore::BuildWorkflowStatusText(const ThreeDSessionRecord & record) {
    std::ostringstream stream;
    stream << "workflow_id=" << record.active_workflow_id << "\n"
           << "state=" << record.active_workflow_state << "\n"
           << "progress_label=" << record.active_progress_label << "\n"
           << "current_node=" << record.active_current_node << "\n";
    return stream.str();
}

std::string ThreeDSessionStateStore::BuildViewportText(const ThreeDSessionRecord & record) {
    std::ostringstream stream;
    stream << "image_path=" << record.latest_viewport_image_path << "\n"
           << "scene_id=" << record.hosts.blender_scene_id << "\n";
    return stream.str();
}

std::string ThreeDSessionStateStore::BuildNovaHostText(const ThreeDSessionRecord & record) {
    std::ostringstream stream;
    stream << "backend=" << record.hosts.nova_backend_name << "\n"
           << "workspace_id=" << record.hosts.nova_workspace_id << "\n"
           << "endpoint=" << record.hosts.nova_endpoint << "\n"
           << "conversation_id=" << record.nova_conversation_id << "\n";
    return stream.str();
}

std::string ThreeDSessionStateStore::BuildBlenderHostText(const ThreeDSessionRecord & record) {
    std::ostringstream stream;
    stream << "backend=" << record.hosts.blender_backend_name << "\n"
           << "scene_id=" << record.hosts.blender_scene_id << "\n"
           << "scene_name=" << record.hosts.blender_scene_name << "\n"
           << "blend_file_path=" << record.hosts.blend_file_path << "\n";
    return stream.str();
}

void ThreeDSessionStateStore::AppendUnique(std::vector<std::string> * values, const std::string & value) {
    if (values == nullptr || value.empty()) {
        return;
    }
    for (const std::string & existing : *values) {
        if (existing == value) {
            return;
        }
    }
    values->push_back(value);
}

CommandResult ThreeDSessionStateStore::BuildNotFoundResult(
    const std::string & key,
    const std::string & value) {
    CommandResult result;
    result.ok = false;
    result.exit_code = 1;
    result.fields["error"] = key + " not found";
    result.fields[key] = value;
    return result;
}

}  // namespace codex_lan_agent_3d
