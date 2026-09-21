#include "CxScriptReviewGateRuntime.h"
#include <fstream>
#include <sstream>
#include <algorithm>

std::string ToString(CxReviewStage stage)
{
    switch (stage)
    {
    case CxReviewStage::EvidenceResolved: return "evidence";
    case CxReviewStage::RoiPreview: return "roi";
    case CxReviewStage::HeadlessResult: return "headless";
    case CxReviewStage::ToolDisplay: return "result";
    case CxReviewStage::ContractResult: return "contract";
    case CxReviewStage::Promotion: return "promotion";
    default: return "unknown";
    }
}

CxReviewStage ParseReviewStage(const std::string& str)
{
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "evidence") return CxReviewStage::EvidenceResolved;
    if (lower == "roi") return CxReviewStage::RoiPreview;
    if (lower == "headless") return CxReviewStage::HeadlessResult;
    if (lower == "result") return CxReviewStage::ToolDisplay;
    if (lower == "contract") return CxReviewStage::ContractResult;
    if (lower == "promotion") return CxReviewStage::Promotion;
    return CxReviewStage::EvidenceResolved;
}

std::string ToString(CxReviewDecision decision)
{
    switch (decision)
    {
    case CxReviewDecision::Accept: return "accept";
    case CxReviewDecision::RejectRoi: return "reject_roi";
    case CxReviewDecision::RejectParameter: return "reject_parameter";
    case CxReviewDecision::RejectAlgorithm: return "reject_algorithm";
    case CxReviewDecision::RejectOverlay: return "reject_overlay";
    case CxReviewDecision::DeriveProfile: return "derive_profile";
    case CxReviewDecision::Stop: return "stop";
    case CxReviewDecision::None: return "none";
    default: return "unknown";
    }
}

CxReviewDecision ParseReviewDecision(const std::string& str)
{
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower == "accept") return CxReviewDecision::Accept;
    if (lower == "reject_roi") return CxReviewDecision::RejectRoi;
    if (lower == "reject_parameter") return CxReviewDecision::RejectParameter;
    if (lower == "reject_algorithm") return CxReviewDecision::RejectAlgorithm;
    if (lower == "reject_overlay") return CxReviewDecision::RejectOverlay;
    if (lower == "derive_profile") return CxReviewDecision::DeriveProfile;
    if (lower == "stop") return CxReviewDecision::Stop;
    return CxReviewDecision::None;
}

static void EscapeJsonString(std::ostream& os, const std::string& str)
{
    for (char c : str)
    {
        switch (c)
        {
        case '"': os << "\\\""; break;
        case '\\': os << "\\\\"; break;
        case '\n': os << "\\n"; break;
        case '\r': os << "\\r"; break;
        case '\t': os << "\\t"; break;
        default: os << c; break;
        }
    }
}

bool WriteReviewRequestJson(
    const std::filesystem::path& path,
    const CxScriptReviewRequest& request,
    std::string& reason)
{
    try
    {
        std::ofstream file(path);
        if (!file.is_open())
        {
            reason = "Cannot open review request file: " + path.string();
            return false;
        }

        file << "{\n";
        file << "  \"review_id\": \"";
        EscapeJsonString(file, request.review_id);
        file << "\",\n";
        file << "  \"case_id\": \"";
        EscapeJsonString(file, request.case_id);
        file << "\",\n";
        file << "  \"evidence_id\": \"";
        EscapeJsonString(file, request.evidence_id);
        file << "\",\n";
        file << "  \"stage\": \"";
        EscapeJsonString(file, request.stage);
        file << "\",\n";
        file << "  \"image_id\": \"";
        EscapeJsonString(file, request.image_id);
        file << "\",\n";
        file << "  \"target_id\": \"";
        EscapeJsonString(file, request.target_id);
        file << "\",\n";
        file << "  \"script_id\": \"";
        EscapeJsonString(file, request.script_id);
        file << "\",\n";
        file << "  \"parameter_profile_id\": \"";
        EscapeJsonString(file, request.parameter_profile_id);
        file << "\",\n";
        file << "  \"contract_id\": \"";
        EscapeJsonString(file, request.contract_id);
        file << "\",\n";
        file << "  \"reason\": \"";
        EscapeJsonString(file, request.reason);
        file << "\",\n";
        file << "  \"roi_preview_path\": \"";
        EscapeJsonString(file, request.roi_preview_path);
        file << "\",\n";
        file << "  \"tool_display_path\": \"";
        EscapeJsonString(file, request.tool_display_path);
        file << "\",\n";
        file << "  \"result_summary_path\": \"";
        EscapeJsonString(file, request.result_summary_path);
        file << "\",\n";
        file << "  \"evidence_packet_path\": \"";
        EscapeJsonString(file, request.evidence_packet_path);
        file << "\",\n";
        file << "  \"contract_result_path\": \"";
        EscapeJsonString(file, request.contract_result_path);
        file << "\",\n";
        file << "  \"suggested_action\": \"";
        EscapeJsonString(file, request.suggested_action);
        file << "\"\n";
        file << "}\n";

        return true;
    }
    catch (const std::exception& e)
    {
        reason = "Failed to write review request: " + std::string(e.what());
        return false;
    }
}

static std::string JsonLineValue(const std::string& line)
{
    size_t colon = line.find(':');
    if (colon == std::string::npos)
        return "";
    
    size_t start = line.find('"', colon);
    if (start == std::string::npos)
        return "";
    
    size_t end = line.find('"', start + 1);
    if (end == std::string::npos)
        return "";
    
    std::string value = line.substr(start + 1, end - start - 1);
    
    std::string unescaped;
    for (size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == '\\' && i + 1 < value.size())
        {
            switch (value[i + 1])
            {
            case '"': unescaped += '"'; break;
            case '\\': unescaped += '\\'; break;
            case 'n': unescaped += '\n'; break;
            case 'r': unescaped += '\r'; break;
            case 't': unescaped += '\t'; break;
            default: unescaped += value[i + 1]; break;
            }
            ++i;
        }
        else
        {
            unescaped += value[i];
        }
    }
    
    return unescaped;
}

bool LoadHumanReviewJson(
    const std::filesystem::path& path,
    CxScriptHumanReview& review,
    std::string& reason)
{
    try
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            reason = "Cannot open human review file: " + path.string();
            return false;
        }

        std::string line;
        while (std::getline(file, line))
        {
            if (line.find("review_id") != std::string::npos)
                review.review_id = JsonLineValue(line);
            else if (line.find("decision") != std::string::npos)
                review.decision = JsonLineValue(line);
            else if (line.find("reviewer") != std::string::npos)
                review.reviewer = JsonLineValue(line);
            else if (line.find("note") != std::string::npos)
                review.note = JsonLineValue(line);
            else if (line.find("next_action") != std::string::npos)
                review.next_action = JsonLineValue(line);
            else if (line.find("suggested_profile_id") != std::string::npos)
                review.suggested_profile_id = JsonLineValue(line);
        }

        return true;
    }
    catch (const std::exception& e)
    {
        reason = "Failed to read human review: " + std::string(e.what());
        return false;
    }
}