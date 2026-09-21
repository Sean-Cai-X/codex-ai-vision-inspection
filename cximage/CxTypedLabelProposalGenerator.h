#pragma once

#include <filesystem>
#include <string>

struct CxTypedLabelProposalOptions
{
    std::filesystem::path image_path;
    std::filesystem::path output_dir;
    std::string label_kind;
};

struct CxTypedLabelProposalResult
{
    bool complete = false;
    std::string conclusion = "NOT_RUN";
    std::string reason;
    std::filesystem::path label_ref;
    std::filesystem::path companion_ref;
    std::filesystem::path overlay_ref;
    std::filesystem::path analysis_ref;
    std::filesystem::path manifest_ref;
};

bool GenerateCxTypedLabelProposal(
    const CxTypedLabelProposalOptions& options,
    CxTypedLabelProposalResult& result,
    std::string& reason);

int RunCxTypedLabelProposalCli(const CxTypedLabelProposalOptions& options);
