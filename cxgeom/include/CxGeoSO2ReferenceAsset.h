#pragma once
#include "CxGeoSO2Harmonic.h"
namespace cxgeom { namespace so2 {
// Audit-only descriptor asset; checksum is integrity, not a signature/approval.
struct ReferenceAsset { Descriptor descriptor; std::string provenance; };
std::string ReferenceSha256(const std::string& bytes);
// General byte digest; callers own input/work budgets. No approval semantics.
std::string Sha256Bytes(const std::string& bytes);
std::string EncodeReference(const ReferenceAsset& asset);
ReferenceAsset DecodeReference(const std::string& bytes, const std::string& trusted_sha256,
                               const Config& expected_config, Method expected_method);
}}
