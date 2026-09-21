#ifndef CXIMAGE_CXSCRIPT_CASE_PACKAGE_WRITER_H
#define CXIMAGE_CXSCRIPT_CASE_PACKAGE_WRITER_H

#include <string>
#include <filesystem>

#include "ManualStateTestConsole.h"

struct CxEvidenceCandidateSaveOptions
{
    std::string root_dir = "cxscript_runs/evidence_candidates";
    std::string candidate_id;
    std::string case_id_override;
    // Explicit immutable script provenance for a new candidate run.
    std::string source_evidence_script_path_override;
    std::string mode = "draft";
    bool request_run = false;
    bool add_to_evidence_chain = true;
    bool preserve_input_snapshots = false;
    std::string linked_result_summary_path;
    std::string linked_result_overlay_path;
    std::string linked_evidence_overlay_path;
    std::string linked_tool_display_path;
};

struct CxEvidenceCandidateSaveResult
{
    bool ok = false;
    std::string candidate_id;
    std::string candidate_dir;
    std::string evidence_binding_path;
    std::string script_snapshot_path;
    std::string parameter_snapshot_path;
    std::string gauge_annotation_path;
    std::string analysis_state_path;
    std::string result_summary_path;
    std::string reason;
};

bool SaveEvidenceCandidatePackage(
    ManualTestContext& context,
    const CxEvidenceCandidateSaveOptions& options,
    CxEvidenceCandidateSaveResult& result);

void AppendEvidenceCandidateStateProbe(
    const ManualTestContext& context,
    const std::string& candidateDir,
    const std::string& candidateId,
    const std::string& phase,
    const std::string& status,
    const std::string& reason);

bool SaveCasePackage(
    ManualTestContext& context,
    const std::string& caseName,
    const std::string& outputDir,
    std::string& outPath,
    std::string& outReason);

#endif
