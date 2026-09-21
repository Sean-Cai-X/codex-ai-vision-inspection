#pragma once

#include "ThreeDSessionStateStore.h"

namespace codex_lan_agent_3d {

class INova3DBackend {
public:
    virtual ~INova3DBackend() = default;

    virtual StructuredAssetRecord Generate(
        const std::string & session_id,
        const std::string & prompt,
        const std::string & preferred_model) = 0;
    virtual StructuredAssetRecord RegeneratePart(
        const std::string & session_id,
        const StructuredAssetRecord & asset,
        const std::string & part_id,
        const std::string & description,
        const std::string & preferred_model) = 0;
    virtual StructuredAssetRecord AddPart(
        const std::string & session_id,
        const StructuredAssetRecord & asset,
        const std::string & description,
        const std::string & preferred_model) = 0;
    virtual StructuredAssetRecord Articulate(
        const std::string & session_id,
        const StructuredAssetRecord & asset,
        const std::string & articulation_request,
        const std::string & preferred_model) = 0;

    virtual std::string BackendName() const = 0;
    virtual std::string WorkspaceId() const = 0;
    virtual std::string Endpoint() const = 0;
};

class Nova3DAssetAdapter : public IStructuredAssetGenerator {
public:
    Nova3DAssetAdapter(
        std::shared_ptr<INova3DBackend> backend,
        std::shared_ptr<ThreeDSessionStateStore> state_store,
        std::string session_id);

    StructuredAssetRecord Generate(
        const std::string & prompt,
        const std::string & preferred_model) override;
    StructuredAssetRecord RegeneratePart(
        const StructuredAssetRecord & asset,
        const std::string & part_id,
        const std::string & description,
        const std::string & preferred_model) override;
    StructuredAssetRecord AddPart(
        const StructuredAssetRecord & asset,
        const std::string & description,
        const std::string & preferred_model) override;
    StructuredAssetRecord Articulate(
        const StructuredAssetRecord & asset,
        const std::string & articulation_request,
        const std::string & preferred_model) override;

    CommandResult BuildHostSummary() const;

private:
    std::shared_ptr<INova3DBackend> backend_;
    std::shared_ptr<ThreeDSessionStateStore> state_store_;
    std::string session_id_;
};

}  // namespace codex_lan_agent_3d
