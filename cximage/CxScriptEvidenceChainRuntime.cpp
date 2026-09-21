#include "CxScriptEvidenceChainRuntime.h"
#include "CxParserRuntimeOwner.h"
#include <filesystem>

const CxScriptEvidenceCase* CxScriptEvidenceChainRuntime::FindCase(const std::string& evidence_id) const
{
    for (const auto& c : cases)
    {
        if (c.evidence_id == evidence_id)
            return &c;
    }
    return nullptr;
}

void CxScriptEvidenceChainRuntime::Clear()
{
    chain_id.clear();
    chain_name.clear();
    catalog_path.clear();
    image_manifest_path.clear();
    output_root.clear();
    cases.clear();
}

bool LoadCxScriptEvidenceChainFile(
    const std::string& script_path,
    CxScriptEvidenceChainRuntime& out_chain,
    std::string& out_reason)
{
    std::filesystem::path path(script_path);
    if (!std::filesystem::exists(path))
    {
        out_reason = "Evidence chain file not found: " + script_path;
        return false;
    }

    CxParserRuntimeOwner owner;
    if (!owner.Initialize(out_reason))
        return false;

    return owner.ParseEvidenceChain(script_path, out_chain, out_reason);
}