#include "ThreeDSceneBridge.h"

namespace codex_lan_agent_3d {

namespace {

template <typename RecordType, typename Predicate>
void UpsertByPredicate(std::vector<RecordType> & records,
                       const RecordType & incoming,
                       Predicate predicate) {
    for (RecordType & record : records) {
        if (predicate(record)) {
            record = incoming;
            return;
        }
    }
    records.push_back(incoming);
}

}  // namespace

std::string ToString(SceneSourceDomain domain) {
    switch (domain) {
        case SceneSourceDomain::kGeometry:
            return "geometry";
        case SceneSourceDomain::kCloud:
            return "cloud";
    }
    return "unknown";
}

std::string ToString(RefreshAction action) {
    switch (action) {
        case RefreshAction::kNone:
            return "none";
        case RefreshAction::kRedrawOnly:
            return "redraw_only";
        case RefreshAction::kUpdatePresentation:
            return "update_presentation";
        case RefreshAction::kRebuildGeometryPresentation:
            return "rebuild_geometry_presentation";
        case RefreshAction::kRebuildCloudPresentation:
            return "rebuild_cloud_presentation";
        case RefreshAction::kRebuildScene:
            return "rebuild_scene";
    }
    return "unknown";
}

void ThreeDSceneBridge::UpsertGeometryRecord(const GeometrySceneConceptRecord & record) {
    UpsertByPredicate(
        geometry_records_,
        record,
        [&record](const GeometrySceneConceptRecord & existing) {
            return existing.entity_id == record.entity_id;
        });
}

void ThreeDSceneBridge::UpsertCloudRecord(const CloudSceneConceptRecord & record) {
    UpsertByPredicate(
        cloud_records_,
        record,
        [&record](const CloudSceneConceptRecord & existing) {
            return existing.entity_id == record.entity_id;
        });
}

void ThreeDSceneBridge::RegisterParserWorkflow(const ParserWorkflowConceptRecord & workflow) {
    UpsertByPredicate(
        workflows_,
        workflow,
        [&workflow](const ParserWorkflowConceptRecord & existing) {
            return existing.task_id == workflow.task_id && existing.trace_id == workflow.trace_id;
        });
}

CommandResult ThreeDSceneBridge::BuildUnifiedSceneSummary() const {
    CommandResult result;
    std::vector<std::string> objects;
    int publishable_count = 0;

    for (const GeometrySceneConceptRecord & record : geometry_records_) {
        if (record.publishable) {
            ++publishable_count;
        }
        objects.push_back(
            "geometry:" + std::to_string(record.entity_id) + ":" + record.shape_kind + ":" +
            (record.publishable ? "publishable" : "blocked"));
    }
    for (const CloudSceneConceptRecord & record : cloud_records_) {
        if (record.publishable) {
            ++publishable_count;
        }
        objects.push_back(
            "cloud:" + std::to_string(record.entity_id) + ":points_" + std::to_string(record.point_count) + ":" +
            (record.publishable ? "publishable" : "blocked"));
    }

    result.fields["geometry_record_count"] = std::to_string(geometry_records_.size());
    result.fields["cloud_record_count"] = std::to_string(cloud_records_.size());
    result.fields["scene_record_count"] = std::to_string(objects.size());
    result.fields["publishable_record_count"] = std::to_string(publishable_count);
    result.fields["scene_records"] = JoinStrings(objects, "|");

    const SceneRefreshPlan refresh = BuildRefreshPlan();
    result.fields["refresh_action"] = ToString(refresh.action);
    result.fields["refresh_reasons"] = JoinStrings(refresh.reasons, "|");
    return result;
}

CommandResult ThreeDSceneBridge::BuildWorkflowSummary() const {
    CommandResult result;
    std::vector<std::string> records;
    int success_count = 0;
    for (const ParserWorkflowConceptRecord & workflow : workflows_) {
        if (workflow.success) {
            ++success_count;
        }
        records.push_back(
            workflow.task_id + ":" + workflow.module_name + ":" + workflow.layer_name + ":" +
            workflow.case_id + ":" + (workflow.success ? "ok" : "fail"));
    }
    result.fields["workflow_count"] = std::to_string(workflows_.size());
    result.fields["workflow_success_count"] = std::to_string(success_count);
    result.fields["workflow_records"] = JoinStrings(records, "|");
    return result;
}

SceneRefreshPlan ThreeDSceneBridge::BuildRefreshPlan() const {
    SceneRefreshPlan plan;

    for (const GeometrySceneConceptRecord & record : geometry_records_) {
        if (!record.publishable) {
            plan.action = RefreshAction::kRebuildGeometryPresentation;
            plan.geometry_changed = true;
            plan.reasons.push_back("geometry:" + std::to_string(record.entity_id) + ":not_publishable");
            continue;
        }
        if (!record.has_presentation) {
            plan.action = std::max(plan.action, RefreshAction::kUpdatePresentation);
            plan.geometry_changed = true;
            plan.reasons.push_back("geometry:" + std::to_string(record.entity_id) + ":missing_presentation");
        }
    }

    for (const CloudSceneConceptRecord & record : cloud_records_) {
        if (!record.publishable) {
            plan.action = RefreshAction::kRebuildCloudPresentation;
            plan.cloud_changed = true;
            plan.reasons.push_back("cloud:" + std::to_string(record.entity_id) + ":not_publishable");
            continue;
        }
        if (!record.has_bounds) {
            plan.action = std::max(plan.action, RefreshAction::kUpdatePresentation);
            plan.cloud_changed = true;
            plan.reasons.push_back("cloud:" + std::to_string(record.entity_id) + ":missing_bounds");
        }
    }

    if (!workflows_.empty()) {
        const ParserWorkflowConceptRecord & last = workflows_.back();
        if (!last.success) {
            plan.action = RefreshAction::kRebuildScene;
            plan.reasons.push_back("workflow:" + last.task_id + ":failed");
        }
        else if (last.replay_count > 0 && plan.action == RefreshAction::kNone) {
            plan.action = RefreshAction::kRedrawOnly;
            plan.reasons.push_back("workflow:" + last.task_id + ":replay");
        }
    }

    return plan;
}

}  // namespace codex_lan_agent_3d
