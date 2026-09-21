#pragma once

#include "ThreeDTypes.h"

namespace codex_lan_agent_3d {

enum class SceneSourceDomain {
    kGeometry = 0,
    kCloud
};

enum class RefreshAction {
    kNone = 0,
    kRedrawOnly,
    kUpdatePresentation,
    kRebuildGeometryPresentation,
    kRebuildCloudPresentation,
    kRebuildScene
};

struct GeometrySceneConceptRecord {
    int entity_id = 0;
    std::string shape_kind = "unknown";
    bool has_payload = false;
    bool has_presentation = false;
    bool visible = true;
    std::uint64_t geometry_revision = 0;
    bool publishable = false;
};

struct CloudSceneConceptRecord {
    int entity_id = 0;
    bool has_payload = false;
    bool has_render_data = false;
    bool has_bounds = false;
    bool visible = true;
    int point_count = 0;
    std::uint64_t cloud_revision = 0;
    bool publishable = false;
};

struct ParserWorkflowConceptRecord {
    std::string task_id;
    std::string trace_id;
    std::string module_name;
    std::string layer_name;
    std::string case_id;
    std::string route_key;
    std::string execution_mode;
    std::string result_object;
    std::string summary;
    int executed_task_count = 0;
    int replay_count = 0;
    bool success = false;
};

struct UnifiedSceneObjectRecord {
    std::string object_id;
    SceneSourceDomain domain = SceneSourceDomain::kGeometry;
    int entity_id = 0;
    std::string semantic_kind;
    bool visible = true;
    bool publishable = false;
    std::uint64_t revision = 0;
};

struct SceneRefreshPlan {
    RefreshAction action = RefreshAction::kNone;
    std::vector<std::string> reasons;
    bool geometry_changed = false;
    bool cloud_changed = false;
};

class ThreeDSceneBridge {
public:
    void UpsertGeometryRecord(const GeometrySceneConceptRecord & record);
    void UpsertCloudRecord(const CloudSceneConceptRecord & record);
    void RegisterParserWorkflow(const ParserWorkflowConceptRecord & workflow);

    CommandResult BuildUnifiedSceneSummary() const;
    CommandResult BuildWorkflowSummary() const;
    SceneRefreshPlan BuildRefreshPlan() const;

private:
    std::vector<GeometrySceneConceptRecord> geometry_records_;
    std::vector<CloudSceneConceptRecord> cloud_records_;
    std::vector<ParserWorkflowConceptRecord> workflows_;
};

std::string ToString(SceneSourceDomain domain);
std::string ToString(RefreshAction action);

}  // namespace codex_lan_agent_3d
