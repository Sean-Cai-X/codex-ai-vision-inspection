#pragma once

#include "Nova3DAssetAdapter.h"

namespace codex_lan_agent_3d {

class INova3DMcpInvoker {
public:
    virtual ~INova3DMcpInvoker() = default;
    virtual CommandResult CallTool(
        const std::string & tool_name,
        const std::map<std::string, std::string> & arguments) = 0;
};

class Nova3DMcpHostedBackend : public INova3DBackend {
public:
    explicit Nova3DMcpHostedBackend(std::shared_ptr<INova3DMcpInvoker> invoker);

    StructuredAssetRecord Generate(
        const std::string & session_id,
        const std::string & prompt,
        const std::string & preferred_model) override;
    StructuredAssetRecord RegeneratePart(
        const std::string & session_id,
        const StructuredAssetRecord & asset,
        const std::string & part_id,
        const std::string & description,
        const std::string & preferred_model) override;
    StructuredAssetRecord AddPart(
        const std::string & session_id,
        const StructuredAssetRecord & asset,
        const std::string & description,
        const std::string & preferred_model) override;
    StructuredAssetRecord Articulate(
        const std::string & session_id,
        const StructuredAssetRecord & asset,
        const std::string & articulation_request,
        const std::string & preferred_model) override;

    std::string BackendName() const override;
    std::string WorkspaceId() const override;
    std::string Endpoint() const override;

    CommandResult GetGenerationStatus(const std::string & workflow_id);

private:
    static std::string ResolvePartType(
        const StructuredAssetRecord & asset,
        const std::string & part_id);
    static StructuredAssetRecord BuildAssetFromResult(
        const CommandResult & result,
        const std::string & prompt,
        const std::string & fallback_conversation_id);
    static std::string BuildConversationIdFallback(const std::string & conversation_url);
    static std::string GetOrDefault(
        const CommandResult & result,
        const std::string & key,
        const std::string & fallback = std::string());

    std::shared_ptr<INova3DMcpInvoker> invoker_;
};

}  // namespace codex_lan_agent_3d
