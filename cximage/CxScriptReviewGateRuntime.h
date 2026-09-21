#pragma once

#include <string>
#include <filesystem>

enum class CxReviewStage
{
    EvidenceResolved,
    RoiPreview,
    HeadlessResult,
    ToolDisplay,
    ContractResult,
    Promotion
};

enum class CxReviewDecision
{
    None,
    Accept,
    RejectRoi,
    RejectParameter,
    RejectAlgorithm,
    RejectOverlay,
    DeriveProfile,
    Stop
};

struct CxScriptReviewRequest
{
    std::string review_id;
    std::string case_id;
    std::string evidence_id;

    std::string stage;
    std::string image_id;
    std::string target_id;
    std::string script_id;
    std::string parameter_profile_id;
    std::string contract_id;

    std::string reason;
    std::string roi_preview_path;
    std::string tool_display_path;
    std::string result_summary_path;
    std::string evidence_packet_path;
    std::string contract_result_path;

    std::string suggested_action;
};

struct CxScriptHumanReview
{
    std::string review_id;
    std::string decision;
    std::string reviewer;
    std::string note;

    std::string next_action;
    std::string suggested_profile_id;
};

bool WriteReviewRequestJson(
    const std::filesystem::path& path,
    const CxScriptReviewRequest& request,
    std::string& reason);

bool LoadHumanReviewJson(
    const std::filesystem::path& path,
    CxScriptHumanReview& review,
    std::string& reason);

std::string ToString(CxReviewStage stage);
CxReviewStage ParseReviewStage(const std::string& str);

std::string ToString(CxReviewDecision decision);
CxReviewDecision ParseReviewDecision(const std::string& str);