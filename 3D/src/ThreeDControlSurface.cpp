#include "ThreeDControlSurface.h"

#include <unordered_map>

namespace codex_lan_agent_3d {

namespace {

const std::unordered_map<std::string, ToolRouteContract> & BuildRouteContracts() {
    static const std::unordered_map<std::string, ToolRouteContract> contracts = {
        {"asset.generate_structured",
         {"asset.generate_structured", "scene_asset_generation", "medium", "3d,asset,generate"}},
        {"asset.regenerate_part",
         {"asset.regenerate_part", "scene_asset_mutation", "medium", "3d,asset,regenerate"}},
        {"asset.add_part",
         {"asset.add_part", "scene_asset_mutation", "medium", "3d,asset,extend"}},
        {"asset.articulate",
         {"asset.articulate", "scene_asset_mutation", "medium", "3d,asset,articulate"}},
        {"scene.import_asset",
         {"scene.import_asset", "scene_state_mutation", "medium", "3d,scene,import"}},
        {"scene.transform_object",
         {"scene.transform_object", "scene_state_mutation", "medium", "3d,scene,transform"}},
        {"scene.get_summary",
         {"scene.get_summary", "read_observe", "low", "3d,scene,summary,read_only"}},
        {"scene.bridge_summary",
         {"scene.bridge_summary", "read_observe", "low", "3d,scene,bridge,read_only"}},
        {"parser.workflow_summary",
         {"parser.workflow_summary", "read_observe", "low", "3d,parser,workflow,read_only"}}
    };
    return contracts;
}

}  // namespace

ThreeDControlSurface::ThreeDControlSurface(std::shared_ptr<ThreeDOrchestrator> orchestrator)
    : orchestrator_(std::move(orchestrator)) {
}

const std::vector<ToolSpec> & ThreeDControlSurface::ToolCatalog() const {
    return orchestrator_->ToolCatalog();
}

bool ThreeDControlSurface::TryGetRouteContract(
    const std::string & tool_name,
    ToolRouteContract * contract) const {
    const ToolRouteContract * found = FindRouteContract(tool_name);
    if (found == nullptr) {
        return false;
    }
    if (contract != nullptr) {
        *contract = *found;
    }
    return true;
}

CommandResult ThreeDControlSurface::BuildToolsCatalogResult() const {
    CommandResult result;
    const auto & contracts = BuildRouteContracts();
    result.fields["tool_count"] = std::to_string(contracts.size());

    std::vector<std::string> names;
    std::vector<std::string> route_rows;
    for (const ToolSpec & spec : orchestrator_->ToolCatalog()) {
        names.push_back(spec.name);
        const auto it = contracts.find(spec.name);
        if (it != contracts.end()) {
            route_rows.push_back(
                spec.name + ":" + it->second.request_type + ":" + it->second.risk);
        }
    }
    result.fields["tools"] = JoinStrings(names, "|");
    result.fields["route_contracts"] = JoinStrings(route_rows, "|");
    return result;
}

CommandResult ThreeDControlSurface::ExecuteTool(
    const std::string & tool_name,
    const std::map<std::string, std::string> & arguments) const {
    const ToolRouteContract * contract = FindRouteContract(tool_name);
    if (contract == nullptr) {
        return BuildArgumentError(tool_name, "tool not registered");
    }

    CommandResult result;
    if (tool_name == "asset.generate_structured") {
        result = HandleGenerateStructured(arguments);
    }
    else if (tool_name == "asset.regenerate_part") {
        result = HandleRegeneratePart(arguments);
    }
    else if (tool_name == "asset.add_part") {
        result = HandleAddPart(arguments);
    }
    else if (tool_name == "asset.articulate") {
        result = HandleArticulate(arguments);
    }
    else if (tool_name == "scene.import_asset") {
        result = HandleImportAsset(arguments);
    }
    else if (tool_name == "scene.transform_object") {
        result = HandleTransformObject(arguments);
    }
    else if (tool_name == "scene.get_summary") {
        result = HandleSceneSummary(arguments);
    }
    else if (tool_name == "scene.bridge_summary") {
        result = HandleSceneBridgeSummary(arguments);
    }
    else {
        result = HandleParserWorkflowSummary(arguments);
    }

    DecorateResult(*contract, &result);
    return result;
}

CommandResult ThreeDControlSurface::HandleGenerateStructured(
    const std::map<std::string, std::string> & arguments) const {
    std::string prompt;
    CommandResult error;
    if (!RequireArgument(arguments, "prompt", &prompt, &error, "asset.generate_structured")) {
        return error;
    }
    return orchestrator_->GenerateStructuredAsset(
        prompt,
        GetArgumentOrDefault(arguments, "preferred_model", "default"));
}

CommandResult ThreeDControlSurface::HandleRegeneratePart(
    const std::map<std::string, std::string> & arguments) const {
    std::string asset_id;
    std::string part_id;
    std::string description;
    CommandResult error;
    if (!RequireArgument(arguments, "asset_id", &asset_id, &error, "asset.regenerate_part") ||
        !RequireArgument(arguments, "part_id", &part_id, &error, "asset.regenerate_part") ||
        !RequireArgument(arguments, "description", &description, &error, "asset.regenerate_part")) {
        return error;
    }
    return orchestrator_->RegenerateAssetPart(
        asset_id,
        part_id,
        description,
        GetArgumentOrDefault(arguments, "preferred_model", "default"));
}

CommandResult ThreeDControlSurface::HandleAddPart(
    const std::map<std::string, std::string> & arguments) const {
    std::string asset_id;
    std::string description;
    CommandResult error;
    if (!RequireArgument(arguments, "asset_id", &asset_id, &error, "asset.add_part") ||
        !RequireArgument(arguments, "description", &description, &error, "asset.add_part")) {
        return error;
    }
    return orchestrator_->AddAssetPart(
        asset_id,
        description,
        GetArgumentOrDefault(arguments, "preferred_model", "default"));
}

CommandResult ThreeDControlSurface::HandleArticulate(
    const std::map<std::string, std::string> & arguments) const {
    std::string asset_id;
    std::string request;
    CommandResult error;
    if (!RequireArgument(arguments, "asset_id", &asset_id, &error, "asset.articulate") ||
        !RequireArgument(arguments, "articulation_request", &request, &error, "asset.articulate")) {
        return error;
    }
    return orchestrator_->ArticulateAsset(
        asset_id,
        request,
        GetArgumentOrDefault(arguments, "preferred_model", "default"));
}

CommandResult ThreeDControlSurface::HandleImportAsset(
    const std::map<std::string, std::string> & arguments) const {
    std::string asset_id;
    CommandResult error;
    if (!RequireArgument(arguments, "asset_id", &asset_id, &error, "scene.import_asset")) {
        return error;
    }
    return orchestrator_->ImportAssetToScene(asset_id);
}

CommandResult ThreeDControlSurface::HandleTransformObject(
    const std::map<std::string, std::string> & arguments) const {
    std::string object_id;
    std::string translation_text;
    std::string rotation_text;
    std::string scale_text;
    CommandResult error;
    if (!RequireArgument(arguments, "object_id", &object_id, &error, "scene.transform_object") ||
        !RequireArgument(arguments, "translation", &translation_text, &error, "scene.transform_object") ||
        !RequireArgument(arguments, "rotation", &rotation_text, &error, "scene.transform_object") ||
        !RequireArgument(arguments, "scale", &scale_text, &error, "scene.transform_object")) {
        return error;
    }

    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    if (!TryParseVec3(translation_text, &translation) ||
        !TryParseVec3(rotation_text, &rotation) ||
        !TryParseVec3(scale_text, &scale)) {
        return BuildArgumentError("scene.transform_object", "vec3 arguments must use x,y,z");
    }
    return orchestrator_->TransformSceneObject(object_id, translation, rotation, scale);
}

CommandResult ThreeDControlSurface::HandleSceneSummary(
    const std::map<std::string, std::string> &) const {
    return orchestrator_->GetSceneSummary();
}

CommandResult ThreeDControlSurface::HandleSceneBridgeSummary(
    const std::map<std::string, std::string> &) const {
    return orchestrator_->GetSceneBridgeSummary();
}

CommandResult ThreeDControlSurface::HandleParserWorkflowSummary(
    const std::map<std::string, std::string> &) const {
    return orchestrator_->GetParserWorkflowSummary();
}

CommandResult ThreeDControlSurface::BuildArgumentError(
    const std::string & tool_name,
    const std::string & message) {
    CommandResult result;
    result.ok = false;
    result.exit_code = 2;
    result.fields["tool_name"] = tool_name;
    result.fields["error"] = message;
    return result;
}

std::string ThreeDControlSurface::GetArgumentOrDefault(
    const std::map<std::string, std::string> & arguments,
    const std::string & key,
    const std::string & fallback) {
    const auto it = arguments.find(key);
    if (it == arguments.end()) {
        return fallback;
    }
    return it->second;
}

bool ThreeDControlSurface::RequireArgument(
    const std::map<std::string, std::string> & arguments,
    const std::string & key,
    std::string * value,
    CommandResult * error,
    const std::string & tool_name) {
    const auto it = arguments.find(key);
    if (it == arguments.end() || it->second.empty()) {
        if (error != nullptr) {
            *error = BuildArgumentError(tool_name, "missing required argument: " + key);
        }
        return false;
    }
    if (value != nullptr) {
        *value = it->second;
    }
    return true;
}

void ThreeDControlSurface::DecorateResult(
    const ToolRouteContract & contract,
    CommandResult * result) {
    if (result == nullptr) {
        return;
    }
    result->fields["tool_name"] = contract.tool_name;
    result->fields["request_type"] = contract.request_type;
    result->fields["risk"] = contract.risk;
    result->fields["tags"] = contract.tags;
}

const ToolRouteContract * ThreeDControlSurface::FindRouteContract(const std::string & tool_name) {
    const auto & contracts = BuildRouteContracts();
    const auto it = contracts.find(tool_name);
    if (it == contracts.end()) {
        return nullptr;
    }
    return &it->second;
}

}  // namespace codex_lan_agent_3d
