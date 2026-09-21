#pragma once

#include "ThreeDSessionStateStore.h"

namespace codex_lan_agent_3d {

class IBlenderSceneBackend {
public:
    virtual ~IBlenderSceneBackend() = default;

    virtual CommandResult ImportAsset(
        const std::string & session_id,
        const StructuredAssetRecord & asset,
        SceneSnapshot * snapshot) = 0;
    virtual CommandResult TransformObject(
        const std::string & session_id,
        const std::string & object_id,
        const Vec3 & translation,
        const Vec3 & rotation,
        const Vec3 & scale,
        SceneSnapshot * snapshot) = 0;
    virtual SceneSnapshot ReadSnapshot(const std::string & session_id) const = 0;

    virtual std::string BackendName() const = 0;
    virtual std::string SceneId() const = 0;
    virtual std::string SceneName() const = 0;
    virtual std::string BlendFilePath() const = 0;
};

class BlenderSceneAdapter : public ISceneAdapter {
public:
    BlenderSceneAdapter(
        std::shared_ptr<IBlenderSceneBackend> backend,
        std::shared_ptr<ThreeDSessionStateStore> state_store,
        std::string session_id);

    CommandResult ImportAsset(const StructuredAssetRecord & asset) override;
    CommandResult TransformObject(
        const std::string & object_id,
        const Vec3 & translation,
        const Vec3 & rotation,
        const Vec3 & scale) override;
    SceneSnapshot ReadSnapshot() const override;

    CommandResult BuildHostSummary() const;

private:
    std::shared_ptr<IBlenderSceneBackend> backend_;
    std::shared_ptr<ThreeDSessionStateStore> state_store_;
    std::string session_id_;
};

}  // namespace codex_lan_agent_3d
