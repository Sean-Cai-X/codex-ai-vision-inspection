#pragma once

#include "ThreeDSceneBridge.h"

namespace codex_lan_agent_3d {

struct CxGeomSceneAdapterInput {
    int entity_id = 0;
    std::string shape_kind = "unknown";
    bool has_payload = false;
    bool has_presentation = false;
    bool visible = true;
    std::uint64_t geometry_revision = 0;
};

class CxGeomSceneAdapter {
public:
    static GeometrySceneConceptRecord Convert(const CxGeomSceneAdapterInput & input);
    static CommandResult BuildPublishabilityResult(const GeometrySceneConceptRecord & record);
};

}  // namespace codex_lan_agent_3d
