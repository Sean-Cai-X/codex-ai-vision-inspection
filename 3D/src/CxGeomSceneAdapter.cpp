#include "CxGeomSceneAdapter.h"

namespace codex_lan_agent_3d {

GeometrySceneConceptRecord CxGeomSceneAdapter::Convert(const CxGeomSceneAdapterInput & input) {
    GeometrySceneConceptRecord record;
    record.entity_id = input.entity_id;
    record.shape_kind = input.shape_kind;
    record.has_payload = input.has_payload;
    record.has_presentation = input.has_presentation;
    record.visible = input.visible;
    record.geometry_revision = input.geometry_revision;
    record.publishable = record.entity_id > 0 && record.has_payload;
    return record;
}

CommandResult CxGeomSceneAdapter::BuildPublishabilityResult(const GeometrySceneConceptRecord & record) {
    CommandResult result;
    result.fields["domain"] = "geometry";
    result.fields["entity_id"] = std::to_string(record.entity_id);
    result.fields["shape_kind"] = record.shape_kind;
    result.fields["geometry_revision"] = std::to_string(record.geometry_revision);
    result.fields["publishable"] = record.publishable ? "true" : "false";
    result.fields["visible"] = record.visible ? "true" : "false";
    if (!record.publishable) {
        result.ok = false;
        result.exit_code = 2;
        result.fields["error"] = "geometry scene record is not publishable";
    }
    return result;
}

}  // namespace codex_lan_agent_3d
