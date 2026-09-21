#pragma once

#include "BlenderSceneAdapter.h"

namespace codex_lan_agent_3d {

class IBlenderMcpInvoker {
public:
    virtual ~IBlenderMcpInvoker() = default;
    virtual CommandResult CallTool(
        const std::string & tool_name,
        const std::map<std::string, std::string> & arguments) = 0;
};

class BlenderMcpSceneBackend : public IBlenderSceneBackend {
public:
    explicit BlenderMcpSceneBackend(std::shared_ptr<IBlenderMcpInvoker> invoker);

    CommandResult ImportAsset(
        const std::string & session_id,
        const StructuredAssetRecord & asset,
        SceneSnapshot * snapshot) override;
    CommandResult TransformObject(
        const std::string & session_id,
        const std::string & object_id,
        const Vec3 & translation,
        const Vec3 & rotation,
        const Vec3 & scale,
        SceneSnapshot * snapshot) override;
    SceneSnapshot ReadSnapshot(const std::string & session_id) const override;

    std::string BackendName() const override;
    std::string SceneId() const override;
    std::string SceneName() const override;
    std::string BlendFilePath() const override;

    CommandResult CaptureViewport(const std::string & session_id);

private:
    static SceneSnapshot BuildSnapshotFromResult(const CommandResult & result);
    static std::string GetOrDefault(
        const CommandResult & result,
        const std::string & key,
        const std::string & fallback = std::string());

    std::shared_ptr<IBlenderMcpInvoker> invoker_;
};

}  // namespace codex_lan_agent_3d
