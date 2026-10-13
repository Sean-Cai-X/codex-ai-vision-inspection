#pragma once
#include "../libtorchsegmentation/src/utils/json.hpp"
#include <string>

namespace vision_ai::offline_feedback {
using Json = nlohmann::json;
// No IO, registration, execution or status mutation. Throws stable error codes.
// contract is the shipped versioned JSON asset, not an inferred schema.
Json ParseStrict(const std::string& bytes);
void ValidateRecord(const Json& record, const Json& contract, const std::string& kind);
// Missing extension on an old runtime is null, never manufactured zero counts.
Json ExtractInferenceDiagnostics(const std::string& runtimeResultJson, const Json& contract);
// Digests are SHA256 of the ORIGINAL issue/lock file bytes, not JSON reserialization.
void ValidateLinks(const Json& issue, const std::string& issueFileSha256,
                   const Json& disposition, const Json* repair = nullptr);
}
