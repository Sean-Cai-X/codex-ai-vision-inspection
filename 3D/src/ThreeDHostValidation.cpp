#include "ThreeDHostValidation.h"

namespace codex_lan_agent_3d {

ThreeDHostValidationSuite::ThreeDHostValidationSuite(
    std::shared_ptr<ThreeDHostedWorkflowCoordinator> coordinator,
    std::shared_ptr<ThreeDSessionStateStore> state_store)
    : coordinator_(std::move(coordinator)),
      state_store_(std::move(state_store)) {
}

CommandResult ThreeDHostValidationSuite::RunNova3DValidationMatrix(
    const std::string & prompt,
    const std::string & preferred_model) {
    std::vector<ValidationCheck> checks;

    const CommandResult generated = coordinator_->GenerateStructuredAsset(prompt, preferred_model);
    AddCheck(
        &checks,
        "nova.generate",
        generated.ok,
        generated.ok
            ? "asset_id=" + GetFieldOrDefault(generated, "asset_id")
            : GetFieldOrDefault(generated, "error", "generate failed"));
    AddCheck(
        &checks,
        "nova.conversation",
        !GetFieldOrDefault(generated, "conversation_url").empty(),
        GetFieldOrDefault(generated, "conversation_url", "missing conversation_url"));
    AddCheck(
        &checks,
        "nova.parts.initial",
        GetFieldOrDefault(generated, "part_count") == "2",
        "part_count=" + GetFieldOrDefault(generated, "part_count"));

    const std::string asset_id = GetFieldOrDefault(generated, "asset_id");
    const CommandResult added = coordinator_->AddAssetPart(asset_id, "handle", preferred_model);
    AddCheck(
        &checks,
        "nova.add_part",
        added.ok && GetFieldOrDefault(added, "part_count") == "3",
        "part_count=" + GetFieldOrDefault(added, "part_count"));

    const CommandResult articulated = coordinator_->ArticulateAsset(
        asset_id,
        "make the door swing open",
        preferred_model);
    AddCheck(
        &checks,
        "nova.articulate",
        articulated.ok && GetFieldOrDefault(articulated, "joint_count") == "1",
        "joint_count=" + GetFieldOrDefault(articulated, "joint_count"));

    const CommandResult status = coordinator_->RefreshNovaWorkflowStatus();
    AddCheck(
        &checks,
        "nova.status",
        status.ok && GetFieldOrDefault(status, "state") == "completed",
        "state=" + GetFieldOrDefault(status, "state"));

    return BuildValidationResult("nova3d", checks);
}

CommandResult ThreeDHostValidationSuite::RunBlenderValidationMatrix() {
    std::vector<ValidationCheck> checks;

    const CommandResult imported = coordinator_->ImportActiveAsset();
    AddCheck(
        &checks,
        "blender.import",
        imported.ok && GetFieldOrDefault(imported, "scene_object_count") == "3",
        "scene_object_count=" + GetFieldOrDefault(imported, "scene_object_count"));

    const CommandResult summary = coordinator_->BuildSessionSummary();
    const std::string object_id = GetFieldOrDefault(summary, "active_object_id");
    AddCheck(
        &checks,
        "blender.active_object",
        !object_id.empty(),
        object_id.empty() ? "missing active_object_id" : object_id);

    const CommandResult transformed = coordinator_->TransformSceneObject(
        object_id,
        Vec3{1.0, 0.0, 0.0},
        Vec3{0.0, 30.0, 0.0},
        Vec3{1.0, 1.0, 1.0});
    AddCheck(
        &checks,
        "blender.transform",
        transformed.ok,
        transformed.ok ? GetFieldOrDefault(transformed, "translation") : GetFieldOrDefault(transformed, "error"));

    const CommandResult captured = coordinator_->CaptureViewport();
    AddCheck(
        &checks,
        "blender.viewport_capture",
        captured.ok && !GetFieldOrDefault(captured, "image_path").empty(),
        GetFieldOrDefault(captured, "image_path", GetFieldOrDefault(captured, "error")));

    const CommandResult viewport_resource = state_store_->ReadResource("session://session_workflow/scene/viewport_latest");
    AddCheck(
        &checks,
        "blender.viewport_resource",
        viewport_resource.ok && viewport_resource.fields.at("content").find(".png") != std::string::npos,
        viewport_resource.ok ? viewport_resource.fields.at("content") : GetFieldOrDefault(viewport_resource, "error"));

    return BuildValidationResult("blender", checks);
}

CommandResult ThreeDHostValidationSuite::RunEndToEndValidationMatrix(
    const std::string & task_intent,
    const std::string & trace_id,
    const std::string & prompt,
    const std::string & preferred_model) {
    std::vector<ValidationCheck> checks;

    const CommandResult started = coordinator_->StartSession(task_intent, trace_id);
    AddCheck(
        &checks,
        "flow.session_start",
        started.ok,
        started.ok ? GetFieldOrDefault(started, "session_id") : GetFieldOrDefault(started, "error"));

    const CommandResult nova = RunNova3DValidationMatrix(prompt, preferred_model);
    AddCheck(
        &checks,
        "flow.nova_matrix",
        IsTruthyField(nova, "all_passed"),
        "passed=" + GetFieldOrDefault(nova, "passed_count") + "/" + GetFieldOrDefault(nova, "check_count"));

    const CommandResult blender = RunBlenderValidationMatrix();
    AddCheck(
        &checks,
        "flow.blender_matrix",
        IsTruthyField(blender, "all_passed"),
        "passed=" + GetFieldOrDefault(blender, "passed_count") + "/" + GetFieldOrDefault(blender, "check_count"));

    for (const std::string & uri : {
             "session://session_workflow/summary",
             "session://session_workflow/asset/current",
             "session://session_workflow/scene/current",
             "session://session_workflow/scene/viewport_latest",
             "session://session_workflow/workflow/nova_status"}) {
        const CommandResult resource = state_store_->ReadResource(uri);
        AddCheck(
            &checks,
            "flow.resource." + ToKey(uri),
            resource.ok,
            resource.ok ? uri : GetFieldOrDefault(resource, "error"));
    }

    return BuildValidationResult("end_to_end", checks);
}

void ThreeDHostValidationSuite::AddCheck(
    std::vector<ValidationCheck> * checks,
    const std::string & id,
    bool ok,
    const std::string & detail) {
    if (checks == nullptr) {
        return;
    }
    checks->push_back(ValidationCheck{id, ok, detail});
}

CommandResult ThreeDHostValidationSuite::BuildValidationResult(
    const std::string & scope,
    const std::vector<ValidationCheck> & checks) {
    CommandResult result;
    result.fields["validation_scope"] = scope;
    result.fields["check_count"] = std::to_string(checks.size());

    int passed_count = 0;
    std::vector<std::string> rows;
    for (const ValidationCheck & check : checks) {
        if (check.ok) {
            ++passed_count;
        }
        rows.push_back(check.id + ":" + (check.ok ? "pass" : "fail") + ":" + check.detail);
    }

    result.ok = passed_count == static_cast<int>(checks.size());
    result.exit_code = result.ok ? 0 : 1;
    result.fields["passed_count"] = std::to_string(passed_count);
    result.fields["failed_count"] = std::to_string(static_cast<int>(checks.size()) - passed_count);
    result.fields["all_passed"] = result.ok ? "true" : "false";
    result.fields["validation_rows"] = JoinStrings(rows, "|");
    return result;
}

bool ThreeDHostValidationSuite::IsTruthyField(const CommandResult & result, const std::string & key) {
    const auto it = result.fields.find(key);
    if (it == result.fields.end()) {
        return false;
    }
    return it->second == "true" || it->second == "1" || it->second == "pass";
}

std::string ThreeDHostValidationSuite::GetFieldOrDefault(
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
