#include "CxCloudSceneAdapter.h"

namespace codex_lan_agent_3d {

CloudSceneConceptRecord CxCloudSceneAdapter::Convert(const CxCloudSceneAdapterInput & input) {
    CloudSceneConceptRecord record;
    record.entity_id = input.entity_id;
    record.has_payload = input.has_payload;
    record.has_render_data = input.has_render_data;
    record.has_bounds = input.has_bounds;
    record.visible = input.visible;
    record.point_count = input.point_count;
    record.cloud_revision = input.cloud_revision;
    record.publishable =
        record.entity_id > 0 &&
        record.has_payload &&
        record.has_render_data;
    return record;
}

CommandResult CxCloudSceneAdapter::BuildPublishabilityResult(const CloudSceneConceptRecord & record) {
    CommandResult result;
    result.fields["domain"] = "cloud";
    result.fields["entity_id"] = std::to_string(record.entity_id);
    result.fields["cloud_revision"] = std::to_string(record.cloud_revision);
    result.fields["point_count"] = std::to_string(record.point_count);
    result.fields["publishable"] = record.publishable ? "true" : "false";
    result.fields["visible"] = record.visible ? "true" : "false";
    if (!record.publishable) {
        result.ok = false;
        result.exit_code = 2;
        result.fields["error"] = "cloud scene record is not publishable";
    }
    return result;
}

}  // namespace codex_lan_agent_3d
