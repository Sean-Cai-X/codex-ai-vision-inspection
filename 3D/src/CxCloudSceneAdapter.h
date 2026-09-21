#pragma once

#include "ThreeDSceneBridge.h"

namespace codex_lan_agent_3d {

struct CxCloudSceneAdapterInput {
    int entity_id = 0;
    bool has_payload = false;
    bool has_render_data = false;
    bool has_bounds = false;
    bool visible = true;
    int point_count = 0;
    std::uint64_t cloud_revision = 0;
};

class CxCloudSceneAdapter {
public:
    static CloudSceneConceptRecord Convert(const CxCloudSceneAdapterInput & input);
    static CommandResult BuildPublishabilityResult(const CloudSceneConceptRecord & record);
};

}  // namespace codex_lan_agent_3d
