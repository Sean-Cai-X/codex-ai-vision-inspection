#include "Nova3DMcpHostedBackend.h"

namespace codex_lan_agent_3d {

Nova3DMcpHostedBackend::Nova3DMcpHostedBackend(std::shared_ptr<INova3DMcpInvoker> invoker)
    : invoker_(std::move(invoker)) {
}

StructuredAssetRecord Nova3DMcpHostedBackend::Generate(
    const std::string &,
    const std::string & prompt,
    const std::string & preferred_model) {
    CommandResult result = invoker_->CallTool(
        "generate_3d",
        {
            {"prompt", prompt},
            {"model", preferred_model}
        });
    return BuildAssetFromResult(result, prompt, "");
}

StructuredAssetRecord Nova3DMcpHostedBackend::RegeneratePart(
    const std::string &,
    const StructuredAssetRecord & asset,
    const std::string & part_id,
    const std::string & description,
    const std::string & preferred_model) {
    CommandResult result = invoker_->CallTool(
        "regenerate_part",
        {
            {"code_artifact", asset.code_artifact},
            {"part_type", ResolvePartType(asset, part_id)},
            {"description", description},
            {"model", preferred_model}
        });
    StructuredAssetRecord updated = BuildAssetFromResult(result, asset.prompt, asset.conversation_id);
    updated.asset_id = asset.asset_id;
    if (updated.conversation_url.empty()) {
        updated.conversation_url = asset.conversation_url;
    }
    if (updated.model_artifact.empty()) {
        updated.model_artifact = asset.model_artifact;
    }
    return updated;
}

StructuredAssetRecord Nova3DMcpHostedBackend::AddPart(
    const std::string &,
    const StructuredAssetRecord & asset,
    const std::string & description,
    const std::string & preferred_model) {
    CommandResult result = invoker_->CallTool(
        "add_part",
        {
            {"code_artifact", asset.code_artifact},
            {"description", description},
            {"model", preferred_model}
        });
    StructuredAssetRecord updated = BuildAssetFromResult(result, asset.prompt, asset.conversation_id);
    updated.asset_id = asset.asset_id;
    if (updated.conversation_url.empty()) {
        updated.conversation_url = asset.conversation_url;
    }
    if (updated.model_artifact.empty()) {
        updated.model_artifact = asset.model_artifact;
    }
    return updated;
}

StructuredAssetRecord Nova3DMcpHostedBackend::Articulate(
    const std::string &,
    const StructuredAssetRecord & asset,
    const std::string & articulation_request,
    const std::string & preferred_model) {
    CommandResult result = invoker_->CallTool(
        "articulate_model",
        {
            {"code_artifact", asset.code_artifact},
            {"articulation_request", articulation_request},
            {"model_url", asset.glb_url},
            {"model_artifact", asset.model_artifact},
            {"model", preferred_model}
        });
    StructuredAssetRecord updated = BuildAssetFromResult(result, asset.prompt, asset.conversation_id);
    updated.asset_id = asset.asset_id;
    if (updated.conversation_url.empty()) {
        updated.conversation_url = asset.conversation_url;
    }
    if (updated.model_artifact.empty()) {
        updated.model_artifact = asset.model_artifact;
    }
    return updated;
}

std::string Nova3DMcpHostedBackend::BackendName() const {
    return "nova3d-mcp";
}

std::string Nova3DMcpHostedBackend::WorkspaceId() const {
    return "nova3d_hosted_conversation";
}

std::string Nova3DMcpHostedBackend::Endpoint() const {
    return "mcp://nova3d";
}

CommandResult Nova3DMcpHostedBackend::GetGenerationStatus(const std::string & workflow_id) {
    return invoker_->CallTool(
        "get_generation_status",
        {
            {"workflow_id", workflow_id}
        });
}

std::string Nova3DMcpHostedBackend::ResolvePartType(
    const StructuredAssetRecord & asset,
    const std::string & part_id) {
    for (const AssetPart & part : asset.parts) {
        if (part.part_id == part_id) {
            return part.name;
        }
    }
    return part_id;
}

StructuredAssetRecord Nova3DMcpHostedBackend::BuildAssetFromResult(
    const CommandResult & result,
    const std::string & prompt,
    const std::string & fallback_conversation_id) {
    StructuredAssetRecord asset;
    asset.prompt = prompt;
    asset.workflow_id = GetOrDefault(result, "workflow_id");
    asset.conversation_url = GetOrDefault(result, "conversation_url");
    asset.conversation_id = GetOrDefault(
        result,
        "conversation_id",
        fallback_conversation_id.empty()
            ? BuildConversationIdFallback(asset.conversation_url)
            : fallback_conversation_id);
    asset.glb_url = GetOrDefault(result, "glb_url");
    asset.preview_url = GetOrDefault(result, "preview_url");
    asset.code_artifact = GetOrDefault(result, "code_artifact");
    asset.model_artifact = GetOrDefault(result, "model_artifact");
    asset.joint_count = std::atoi(GetOrDefault(result, "joint_count", "0").c_str());
    asset.source_backend = "nova3d-mcp";

    const std::vector<std::string> parts = SplitString(GetOrDefault(result, "parts"), '|');
    for (const std::string & part_name : parts) {
        asset.parts.push_back(AssetPart{"part_" + ToKey(part_name), part_name, part_name, false});
    }
    asset.asset_id = "asset_" + ToKey(prompt);
    return asset;
}

std::string Nova3DMcpHostedBackend::BuildConversationIdFallback(const std::string & conversation_url) {
    if (conversation_url.empty()) {
        return "";
    }
    const std::size_t slash = conversation_url.find_last_of('/');
    if (slash == std::string::npos || slash + 1 >= conversation_url.size()) {
        return "";
    }
    return conversation_url.substr(slash + 1);
}

std::string Nova3DMcpHostedBackend::GetOrDefault(
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
