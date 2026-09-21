#include "CxBusinessWorkflowRuntime.h"
#include "CxParserRuntimeOwner.h"

#include "../libtorchsegmentation/src/utils/json.hpp"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace
{
namespace fs = std::filesystem;

constexpr const char* kManifestSchema = "cxvision.business_workflow_case.v1";
constexpr std::uintmax_t kMaximumManifestBytes = 8U * 1024U * 1024U;

std::string UtcTimestamp();

enum class FlatJsonType
{
    String,
    Number,
    Boolean,
    Null
};

struct FlatJsonValue
{
    FlatJsonType type = FlatJsonType::Null;
    std::string text;
};

using FlatJsonObject = std::map<std::string, FlatJsonValue>;

std::string ToLowerAscii(std::string value)
{
    for (char& ch : value)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return value;
}

std::string NormalizeToken(const std::string& value)
{
    std::string normalized;
    normalized.reserve(value.size());
    bool lastWasUnderscore = false;
    for (const char raw : value)
    {
        const unsigned char ch = static_cast<unsigned char>(raw);
        if (std::isalnum(ch))
        {
            normalized.push_back(static_cast<char>(std::toupper(ch)));
            lastWasUnderscore = false;
        }
        else if (!normalized.empty() && !lastWasUnderscore)
        {
            normalized.push_back('_');
            lastWasUnderscore = true;
        }
    }
    while (!normalized.empty() && normalized.back() == '_')
        normalized.pop_back();
    return normalized;
}

std::string TrimAscii(const std::string& value)
{
    std::size_t first = 0;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first])))
    {
        ++first;
    }
    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1])))
    {
        --last;
    }
    return value.substr(first, last - first);
}

void AppendUtf8(std::string& output, std::uint32_t codePoint)
{
    if (codePoint <= 0x7FU)
    {
        output.push_back(static_cast<char>(codePoint));
    }
    else if (codePoint <= 0x7FFU)
    {
        output.push_back(static_cast<char>(0xC0U | (codePoint >> 6U)));
        output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
    }
    else if (codePoint <= 0xFFFFU)
    {
        output.push_back(static_cast<char>(0xE0U | (codePoint >> 12U)));
        output.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
    }
    else
    {
        output.push_back(static_cast<char>(0xF0U | (codePoint >> 18U)));
        output.push_back(static_cast<char>(0x80U | ((codePoint >> 12U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | ((codePoint >> 6U) & 0x3FU)));
        output.push_back(static_cast<char>(0x80U | (codePoint & 0x3FU)));
    }
}

class FlatJsonParser
{
public:
    explicit FlatJsonParser(const std::string& text)
        : text_(text)
    {
    }

    bool Parse(FlatJsonObject& object, std::string& reason)
    {
        object.clear();
        position_ = 0;
        if (text_.size() >= 3U &&
            static_cast<unsigned char>(text_[0]) == 0xEFU &&
            static_cast<unsigned char>(text_[1]) == 0xBBU &&
            static_cast<unsigned char>(text_[2]) == 0xBFU)
        {
            position_ = 3U;
        }
        SkipWhitespace();
        if (!Consume('{'))
            return Fail("top-level JSON value must be an object", reason);
        SkipWhitespace();
        if (Consume('}'))
        {
            SkipWhitespace();
            return AtEndOrFail(reason);
        }

        while (position_ < text_.size())
        {
            std::string key;
            if (!ParseString(key, reason))
                return false;
            if (object.find(key) != object.end())
                return Fail("duplicate top-level JSON key: " + key, reason);
            SkipWhitespace();
            if (!Consume(':'))
                return Fail("expected ':' after JSON key: " + key, reason);
            SkipWhitespace();
            FlatJsonValue value;
            if (!ParseScalar(value, reason))
                return false;
            object.emplace(std::move(key), std::move(value));
            SkipWhitespace();
            if (Consume('}'))
            {
                SkipWhitespace();
                return AtEndOrFail(reason);
            }
            if (!Consume(','))
                return Fail("expected ',' or '}' in top-level JSON object", reason);
            SkipWhitespace();
        }
        return Fail("unterminated top-level JSON object", reason);
    }

private:
    bool AtEndOrFail(std::string& reason)
    {
        if (position_ == text_.size())
            return true;
        return Fail("unexpected content after top-level JSON object", reason);
    }

    void SkipWhitespace()
    {
        while (position_ < text_.size() &&
               std::isspace(static_cast<unsigned char>(text_[position_])))
        {
            ++position_;
        }
    }

    bool Consume(char expected)
    {
        if (position_ >= text_.size() || text_[position_] != expected)
            return false;
        ++position_;
        return true;
    }

    bool ParseScalar(FlatJsonValue& value, std::string& reason)
    {
        if (position_ >= text_.size())
            return Fail("missing JSON value", reason);
        if (text_[position_] == '"')
        {
            value.type = FlatJsonType::String;
            return ParseString(value.text, reason);
        }
        if (text_[position_] == '{' || text_[position_] == '[')
            return Fail("manifest values must be flat JSON scalars", reason);
        if (MatchLiteral("true"))
        {
            value.type = FlatJsonType::Boolean;
            value.text = "true";
            return true;
        }
        if (MatchLiteral("false"))
        {
            value.type = FlatJsonType::Boolean;
            value.text = "false";
            return true;
        }
        if (MatchLiteral("null"))
        {
            value.type = FlatJsonType::Null;
            value.text.clear();
            return true;
        }
        value.type = FlatJsonType::Number;
        return ParseNumber(value.text, reason);
    }

    bool MatchLiteral(const char* literal)
    {
        const std::size_t length = std::strlen(literal);
        if (text_.compare(position_, length, literal) != 0)
            return false;
        position_ += length;
        return true;
    }

    bool ParseNumber(std::string& output, std::string& reason)
    {
        const std::size_t begin = position_;
        if (position_ < text_.size() && text_[position_] == '-')
            ++position_;
        if (position_ >= text_.size())
            return Fail("invalid JSON number", reason);
        if (text_[position_] == '0')
        {
            ++position_;
        }
        else if (text_[position_] >= '1' && text_[position_] <= '9')
        {
            while (position_ < text_.size() &&
                   std::isdigit(static_cast<unsigned char>(text_[position_])))
            {
                ++position_;
            }
        }
        else
        {
            return Fail("invalid JSON number", reason);
        }
        if (position_ < text_.size() && text_[position_] == '.')
        {
            ++position_;
            const std::size_t fractionBegin = position_;
            while (position_ < text_.size() &&
                   std::isdigit(static_cast<unsigned char>(text_[position_])))
            {
                ++position_;
            }
            if (fractionBegin == position_)
                return Fail("invalid JSON fractional number", reason);
        }
        if (position_ < text_.size() &&
            (text_[position_] == 'e' || text_[position_] == 'E'))
        {
            ++position_;
            if (position_ < text_.size() &&
                (text_[position_] == '+' || text_[position_] == '-'))
            {
                ++position_;
            }
            const std::size_t exponentBegin = position_;
            while (position_ < text_.size() &&
                   std::isdigit(static_cast<unsigned char>(text_[position_])))
            {
                ++position_;
            }
            if (exponentBegin == position_)
                return Fail("invalid JSON exponent", reason);
        }
        output = text_.substr(begin, position_ - begin);
        return true;
    }

    bool ParseHex4(std::uint32_t& value, std::string& reason)
    {
        if (position_ + 4U > text_.size())
            return Fail("incomplete JSON unicode escape", reason);
        value = 0U;
        for (int i = 0; i < 4; ++i)
        {
            const char ch = text_[position_++];
            value <<= 4U;
            if (ch >= '0' && ch <= '9')
                value += static_cast<std::uint32_t>(ch - '0');
            else if (ch >= 'a' && ch <= 'f')
                value += static_cast<std::uint32_t>(ch - 'a' + 10);
            else if (ch >= 'A' && ch <= 'F')
                value += static_cast<std::uint32_t>(ch - 'A' + 10);
            else
                return Fail("invalid JSON unicode escape", reason);
        }
        return true;
    }

    bool ParseString(std::string& output, std::string& reason)
    {
        output.clear();
        if (!Consume('"'))
            return Fail("expected JSON string", reason);
        while (position_ < text_.size())
        {
            const unsigned char ch =
                static_cast<unsigned char>(text_[position_++]);
            if (ch == '"')
                return true;
            if (ch < 0x20U)
                return Fail("unescaped control character in JSON string", reason);
            if (ch != '\\')
            {
                output.push_back(static_cast<char>(ch));
                continue;
            }
            if (position_ >= text_.size())
                return Fail("incomplete JSON string escape", reason);
            const char escaped = text_[position_++];
            switch (escaped)
            {
            case '"': output.push_back('"'); break;
            case '\\': output.push_back('\\'); break;
            case '/': output.push_back('/'); break;
            case 'b': output.push_back('\b'); break;
            case 'f': output.push_back('\f'); break;
            case 'n': output.push_back('\n'); break;
            case 'r': output.push_back('\r'); break;
            case 't': output.push_back('\t'); break;
            case 'u':
            {
                std::uint32_t codePoint = 0U;
                if (!ParseHex4(codePoint, reason))
                    return false;
                if (codePoint >= 0xD800U && codePoint <= 0xDBFFU)
                {
                    if (position_ + 2U > text_.size() ||
                        text_[position_] != '\\' || text_[position_ + 1U] != 'u')
                    {
                        return Fail("high surrogate is missing its low surrogate", reason);
                    }
                    position_ += 2U;
                    std::uint32_t low = 0U;
                    if (!ParseHex4(low, reason) || low < 0xDC00U || low > 0xDFFFU)
                        return Fail("invalid low surrogate in JSON string", reason);
                    codePoint = 0x10000U +
                        ((codePoint - 0xD800U) << 10U) + (low - 0xDC00U);
                }
                else if (codePoint >= 0xDC00U && codePoint <= 0xDFFFU)
                {
                    return Fail("unexpected low surrogate in JSON string", reason);
                }
                AppendUtf8(output, codePoint);
                break;
            }
            default:
                return Fail("unsupported JSON string escape", reason);
            }
        }
        return Fail("unterminated JSON string", reason);
    }

    bool Fail(const std::string& message, std::string& reason) const
    {
        reason = message + " at byte " + std::to_string(position_);
        return false;
    }

    const std::string& text_;
    std::size_t position_ = 0;
};

bool ReadTextFile(const fs::path& path, std::string& text, std::string& reason)
{
    text.clear();
    std::error_code error;
    const std::uintmax_t size = fs::file_size(path, error);
    if (error)
    {
        reason = "cannot determine file size: " + error.message();
        return false;
    }
    if (size > kMaximumManifestBytes)
    {
        reason = "manifest exceeds the maximum accepted byte size";
        return false;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open())
    {
        reason = "cannot open file for reading";
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (input.bad())
    {
        reason = "file read failed";
        return false;
    }
    text = buffer.str();
    return true;
}

const FlatJsonValue* FindValue(
    const FlatJsonObject& object,
    const std::string& key)
{
    const auto found = object.find(key);
    return found == object.end() ? nullptr : &found->second;
}

bool ReadRequiredString(
    const FlatJsonObject& object,
    const std::string& key,
    std::string& value,
    std::string& reason)
{
    const FlatJsonValue* node = FindValue(object, key);
    if (node == nullptr || node->type != FlatJsonType::String)
    {
        reason = "required manifest string is missing or invalid: " + key;
        return false;
    }
    value = TrimAscii(node->text);
    if (value.empty())
    {
        reason = "required manifest string is empty: " + key;
        return false;
    }
    return true;
}

std::string ReadOptionalString(
    const FlatJsonObject& object,
    const std::string& key)
{
    const FlatJsonValue* node = FindValue(object, key);
    return node != nullptr && node->type == FlatJsonType::String
        ? TrimAscii(node->text)
        : std::string();
}

bool ParseUnsignedValue(const FlatJsonValue& node, std::size_t& value)
{
    if (node.type != FlatJsonType::Number && node.type != FlatJsonType::String)
        return false;
    const std::string text = TrimAscii(node.text);
    if (text.empty() || text[0] == '-')
        return false;
    errno = 0;
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(text.c_str(), &end, 10);
    if (errno == ERANGE || end == text.c_str() || *end != '\0' ||
        parsed > static_cast<unsigned long long>(
            std::numeric_limits<std::size_t>::max()))
    {
        return false;
    }
    value = static_cast<std::size_t>(parsed);
    return true;
}

bool ReadRequiredCount(
    const FlatJsonObject& object,
    const std::string& key,
    std::size_t& value,
    std::string& reason)
{
    const FlatJsonValue* node = FindValue(object, key);
    if (node == nullptr || !ParseUnsignedValue(*node, value))
    {
        reason = "required manifest count is missing or invalid: " + key;
        return false;
    }
    return true;
}

bool ReadOptionalCount(
    const FlatJsonObject& object,
    const std::string& key,
    std::size_t& value,
    std::string& reason)
{
    const FlatJsonValue* node = FindValue(object, key);
    if (node == nullptr)
    {
        value = 0;
        return true;
    }
    if (!ParseUnsignedValue(*node, value))
    {
        reason = "optional manifest count is invalid: " + key;
        return false;
    }
    return true;
}

bool ReadOptionalBool(
    const FlatJsonObject& object,
    const std::string& key,
    bool defaultValue,
    bool& value,
    std::string& reason)
{
    const FlatJsonValue* node = FindValue(object, key);
    if (node == nullptr)
    {
        value = defaultValue;
        return true;
    }
    if (node->type == FlatJsonType::Boolean)
    {
        value = node->text == "true";
        return true;
    }
    if (node->type == FlatJsonType::Number &&
        (node->text == "0" || node->text == "1"))
    {
        value = node->text == "1";
        return true;
    }
    reason = "manifest boolean is invalid: " + key;
    return false;
}

bool ReadOptionalInteger(
    const FlatJsonObject& object,
    const std::string& key,
    long long defaultValue,
    long long& value,
    std::string& reason)
{
    const FlatJsonValue* node = FindValue(object, key);
    if (node == nullptr)
    {
        value = defaultValue;
        return true;
    }
    if (node->type != FlatJsonType::Number && node->type != FlatJsonType::String)
    {
        reason = "manifest integer is invalid: " + key;
        return false;
    }
    const std::string text = TrimAscii(node->text);
    errno = 0;
    char* end = nullptr;
    const long long parsed = std::strtoll(text.c_str(), &end, 10);
    if (errno == ERANGE || end == text.c_str() || *end != '\0')
    {
        reason = "manifest integer is invalid: " + key;
        return false;
    }
    value = parsed;
    return true;
}

bool ReadOptionalDouble(
    const FlatJsonObject& object,
    const std::string& key,
    double defaultValue,
    double& value,
    std::string& reason)
{
    const FlatJsonValue* node = FindValue(object, key);
    if (node == nullptr)
    {
        value = defaultValue;
        return true;
    }
    if (node->type != FlatJsonType::Number && node->type != FlatJsonType::String)
    {
        reason = "manifest number is invalid: " + key;
        return false;
    }
    const std::string text = TrimAscii(node->text);
    errno = 0;
    char* end = nullptr;
    const double parsed = std::strtod(text.c_str(), &end);
    if (errno == ERANGE || end == text.c_str() || *end != '\0' ||
        !std::isfinite(parsed))
    {
        reason = "manifest number is invalid: " + key;
        return false;
    }
    value = parsed;
    return true;
}

std::string IndexedKey(
    const std::string& prefix,
    std::size_t index,
    const std::string& suffix)
{
    std::ostringstream key;
    key << prefix << std::setw(2) << std::setfill('0') << index;
    if (!suffix.empty())
        key << '_' << suffix;
    return key.str();
}

bool PathComponentEqual(const fs::path& left, const fs::path& right)
{
#ifdef _WIN32
    return ToLowerAscii(left.generic_string()) ==
           ToLowerAscii(right.generic_string());
#else
    return left == right;
#endif
}

bool HasNormalizedPathComponent(
    const fs::path& path,
    const std::string& expected)
{
    for (const fs::path& part : path)
    {
        if (ToLowerAscii(part.string()) == ToLowerAscii(expected))
            return true;
    }
    return false;
}

bool IsPathWithin(const fs::path& root, const fs::path& candidate)
{
    auto rootPart = root.begin();
    auto candidatePart = candidate.begin();
    for (; rootPart != root.end(); ++rootPart, ++candidatePart)
    {
        if (candidatePart == candidate.end() ||
            !PathComponentEqual(*rootPart, *candidatePart))
        {
            return false;
        }
    }
    return true;
}

bool HasParentTraversal(const fs::path& path)
{
    for (const fs::path& part : path)
    {
        if (part == "..")
            return true;
    }
    return false;
}

bool ContainsSymlinkComponent(
    const fs::path& base,
    const fs::path& relative,
    std::string& reason)
{
    fs::path current = base;
    for (const fs::path& part : relative)
    {
        if (part == "." || part.empty())
            continue;
        current /= part;
        std::error_code error;
        const fs::file_status status = fs::symlink_status(current, error);
        if (error)
        {
            reason = "cannot inspect asset path component: " + error.message();
            return true;
        }
        if (fs::is_symlink(status))
        {
            reason = "asset path contains a symbolic link";
            return true;
        }
    }
    return false;
}

bool ValidateRequiredAsset(
    const fs::path& root,
    const fs::path& caseDirectory,
    const std::string& value,
    fs::path& resolved,
    std::string& reason)
{
    const fs::path relative(value);
    if (relative.empty() || relative.is_absolute() || relative.has_root_name() ||
        relative.has_root_directory() || HasParentTraversal(relative))
    {
        reason = "required asset must be a relative path without parent traversal";
        return false;
    }
    if (ContainsSymlinkComponent(caseDirectory, relative, reason))
        return false;

    const fs::path candidate = (caseDirectory / relative).lexically_normal();
    std::error_code error;
    const fs::path canonical = fs::canonical(candidate, error);
    if (error)
    {
        reason = "required asset cannot be resolved: " + error.message();
        return false;
    }
    if (!IsPathWithin(root, canonical) || !IsPathWithin(caseDirectory, canonical))
    {
        reason = "required asset resolves outside the case directory or scan root";
        return false;
    }
    const fs::file_status status = fs::symlink_status(canonical, error);
    if (error || !fs::is_regular_file(status))
    {
        reason = error
            ? "required asset cannot be inspected: " + error.message()
            : "required asset is not a regular file";
        return false;
    }
    resolved = canonical;
    return true;
}

struct StepDefinition
{
    std::size_t index = 0;
    std::string action;
    std::string provider;
    std::string input_ref;
    std::string output_ref;
    std::string idempotency_key;
    std::string capability_status;
    std::string expected_status;
    CxBusinessObservationRecord expected_observation;
    bool requires_provider = false;
    bool expected_rejection = false;
    std::string evidence_ref;
    std::string manifest_code;
    std::string manifest_reason;
    CxBusinessWorkflowHumanDecision human_decision;
    // Contract fixtures are input-schema examples for the UI and evidence
    // layer.  They are never treated as provider observations: the step keeps
    // NOT_EXECUTED / NOT_EVALUATED and provider_executed=false.
    std::vector<CxBusinessWorkflowDetectionElement> contract_fixture_elements;
    std::vector<CxBusinessRiskRecord> risks;
    std::map<std::string, std::string> metadata;
};

struct CaseDefinition
{
    CxBusinessWorkflowCaseResult seed;
    std::vector<StepDefinition> steps;
    FlatJsonObject manifest;
};

bool IsCapabilityStatus(const std::string& value)
{
    return value == "AVAILABLE" || value == "PENDING" ||
           value == "NOT_AVAILABLE";
}

bool IsStepExpectedStatus(const std::string& value)
{
    return value == "PASS" || value == "PENDING" || value == "FAIL" ||
           value == "NOT_AVAILABLE" || value == "EXPECTED_REJECTION";
}

bool IsExecutionStatus(const std::string& value)
{
    return value == "COMPLETED" || value == "FAILED" ||
           value == "NOT_EXECUTED" ||
           value == "REJECTED";
}

bool IsDetectionStatus(const std::string& value)
{
    return value == "DETECTED" || value == "ZERO_DETECTION" ||
           value == "NOT_EVALUATED" || value == "MODEL_MISSING";
}

bool IsDisplayStatus(const std::string& value)
{
    return value == "PROJECTED" || value == "NO_ELEMENTS_TO_PROJECT" ||
           value == "PROJECTION_FAILED" || value == "NOT_DISPLAYED";
}

bool IsEvaluationStatus(const std::string& value)
{
    return value == "PASS" || value == "FAIL" ||
           value == "PENDING_HUMAN" || value == "NOT_EVALUATED";
}

bool ReadExpectedObservation(
    const FlatJsonObject& manifest,
    const std::string& prefix,
    CxBusinessObservationRecord& observation,
    std::string& reason)
{
    if (!ReadRequiredString(
            manifest, prefix + "_execution_status",
            observation.execution_status, reason) ||
        !ReadRequiredString(
            manifest, prefix + "_detection_status",
            observation.detection_status, reason) ||
        !ReadRequiredString(
            manifest, prefix + "_display_status",
            observation.display_status, reason) ||
        !ReadRequiredString(
            manifest, prefix + "_evaluation_status",
            observation.evaluation_status, reason))
    {
        return false;
    }
    observation.execution_status = NormalizeToken(observation.execution_status);
    observation.detection_status = NormalizeToken(observation.detection_status);
    observation.display_status = NormalizeToken(observation.display_status);
    observation.evaluation_status = NormalizeToken(observation.evaluation_status);
    if (!IsExecutionStatus(observation.execution_status) ||
        !IsDetectionStatus(observation.detection_status) ||
        !IsDisplayStatus(observation.display_status) ||
        !IsEvaluationStatus(observation.evaluation_status))
    {
        reason = "step contains an unsupported observation status: " + prefix;
        return false;
    }
    return true;
}

bool ParseImageRefs(
    const FlatJsonObject& manifest,
    CxBusinessWorkflowCaseResult& result,
    std::string& reason)
{
    std::size_t count = 0;
    if (!ReadOptionalCount(manifest, "image_ref_count", count, reason))
        return false;
    for (std::size_t index = 1; index <= count; ++index)
    {
        const std::string prefix = IndexedKey("image_ref_", index, "");
        CxBusinessWorkflowImageRef ref;
        if (!ReadRequiredString(manifest, prefix + "_image_id", ref.image_id, reason) ||
            !ReadRequiredString(manifest, prefix + "_image_revision", ref.image_revision, reason) ||
            !ReadRequiredString(manifest, prefix + "_source_type", ref.source_type, reason) ||
            !ReadRequiredString(manifest, prefix + "_uri", ref.uri, reason) ||
            !ReadRequiredString(manifest, prefix + "_coordinate_space", ref.coordinate_space, reason))
        {
            return false;
        }
        ref.content_hash = ReadOptionalString(manifest, prefix + "_content_hash");
        long long width = 0;
        long long height = 0;
        long long channels = 0;
        if (!ReadOptionalInteger(manifest, prefix + "_width", 0, width, reason) ||
            !ReadOptionalInteger(manifest, prefix + "_height", 0, height, reason) ||
            !ReadOptionalInteger(manifest, prefix + "_channels", 0, channels, reason) ||
            width < 0 || height < 0 || channels < 0 ||
            width > std::numeric_limits<int>::max() ||
            height > std::numeric_limits<int>::max() ||
            channels > std::numeric_limits<int>::max())
        {
            reason = "image reference dimensions are invalid: " + prefix;
            return false;
        }
        ref.width = static_cast<int>(width);
        ref.height = static_cast<int>(height);
        ref.channels = static_cast<int>(channels);
        result.image_refs.push_back(std::move(ref));
    }
    return true;
}

bool ParseFrameRefs(
    const FlatJsonObject& manifest,
    CxBusinessWorkflowCaseResult& result,
    std::string& reason)
{
    std::size_t count = 0;
    if (!ReadOptionalCount(manifest, "frame_ref_count", count, reason))
        return false;
    for (std::size_t index = 1; index <= count; ++index)
    {
        const std::string prefix = IndexedKey("frame_ref_", index, "");
        CxBusinessWorkflowFrameRef ref;
        if (!ReadRequiredString(manifest, prefix + "_frame_id", ref.frame_id, reason) ||
            !ReadRequiredString(manifest, prefix + "_image_id", ref.image_id, reason) ||
            !ReadRequiredString(manifest, prefix + "_source_type", ref.source_type, reason) ||
            !ReadRequiredString(manifest, prefix + "_uri", ref.uri, reason))
        {
            return false;
        }
        ref.captured_at = ReadOptionalString(manifest, prefix + "_captured_at");
        if (!ReadOptionalInteger(
                manifest, prefix + "_sequence", 0, ref.sequence, reason))
        {
            return false;
        }
        result.frame_refs.push_back(std::move(ref));
    }
    return true;
}

bool ParseRois(
    const FlatJsonObject& manifest,
    CxBusinessWorkflowCaseResult& result,
    std::string& reason)
{
    std::size_t count = 0;
    if (!ReadOptionalCount(manifest, "roi_count", count, reason))
        return false;
    for (std::size_t index = 1; index <= count; ++index)
    {
        const std::string prefix = IndexedKey("roi_", index, "");
        CxBusinessWorkflowRoi roi;
        if (!ReadRequiredString(manifest, prefix + "_roi_id", roi.roi_id, reason) ||
            !ReadRequiredString(manifest, prefix + "_source_image_id", roi.source_image_id, reason) ||
            !ReadRequiredString(manifest, prefix + "_image_revision", roi.image_revision, reason) ||
            !ReadRequiredString(manifest, prefix + "_recipe_revision", roi.recipe_revision, reason) ||
            !ReadRequiredString(manifest, prefix + "_coordinate_space", roi.coordinate_space, reason) ||
            !ReadRequiredString(manifest, prefix + "_geometry", roi.geometry, reason))
        {
            return false;
        }
        roi.transform_ref = ReadOptionalString(manifest, prefix + "_transform_ref");
        result.rois.push_back(std::move(roi));
    }
    return true;
}

bool ParseAnnotations(
    const FlatJsonObject& manifest,
    CxBusinessWorkflowCaseResult& result,
    std::string& reason)
{
    std::size_t count = 0;
    if (!ReadOptionalCount(manifest, "annotation_count", count, reason))
        return false;
    for (std::size_t index = 1; index <= count; ++index)
    {
        const std::string prefix = IndexedKey("annotation_", index, "");
        CxBusinessWorkflowAnnotation annotation;
        if (!ReadRequiredString(manifest, prefix + "_object_id", annotation.object_id, reason) ||
            !ReadRequiredString(manifest, prefix + "_object_type", annotation.object_type, reason) ||
            !ReadRequiredString(manifest, prefix + "_coordinate_space", annotation.coordinate_space, reason) ||
            !ReadRequiredString(manifest, prefix + "_source_image_id", annotation.source_image_id, reason) ||
            !ReadRequiredString(manifest, prefix + "_image_revision", annotation.image_revision, reason) ||
            !ReadRequiredString(manifest, prefix + "_roi_id", annotation.roi_id, reason) ||
            !ReadRequiredString(manifest, prefix + "_recipe_revision", annotation.recipe_revision, reason) ||
            !ReadRequiredString(manifest, prefix + "_geometry", annotation.geometry, reason) ||
            !ReadRequiredString(manifest, prefix + "_creation_method", annotation.creation_method, reason) ||
            !ReadRequiredString(manifest, prefix + "_creator", annotation.creator, reason) ||
            !ReadRequiredString(manifest, prefix + "_status", annotation.status, reason))
        {
            return false;
        }
        annotation.transform_ref = ReadOptionalString(manifest, prefix + "_transform_ref");
        annotation.parent_ref = ReadOptionalString(manifest, prefix + "_parent_ref");
        annotation.evidence_ref = ReadOptionalString(manifest, prefix + "_evidence_ref");
        if (!ReadOptionalDouble(
                manifest, prefix + "_confidence", 0.0,
                annotation.confidence, reason) ||
            annotation.confidence < 0.0 || annotation.confidence > 1.0)
        {
            reason = "annotation confidence must be within [0, 1]: " + prefix;
            return false;
        }
        result.annotations.push_back(std::move(annotation));
    }
    return true;
}

bool IsImageAssetExtension(const std::string& extension)
{
    static const std::set<std::string> extensions = {
        ".bmp", ".jpeg", ".jpg", ".pgm", ".png", ".tif", ".tiff", ".webp"};
    return extensions.count(ToLowerAscii(extension)) != 0U;
}

std::string Fnv1a64FileHash(const fs::path& path, std::string& reason)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open())
    {
        reason = "cannot open asset for hashing";
        return {};
    }
    std::uint64_t hash = 14695981039346656037ULL;
    char buffer[8192];
    while (input.read(buffer, sizeof(buffer)) || input.gcount() > 0)
    {
        for (std::streamsize index = 0; index < input.gcount(); ++index)
        {
            hash ^= static_cast<unsigned char>(buffer[index]);
            hash *= 1099511628211ULL;
        }
    }
    if (input.bad())
    {
        reason = "asset hash read failed";
        return {};
    }
    std::ostringstream value;
    value << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0') << hash;
    return value.str();
}

bool ValidateKnownBusinessJsonContract(
    const fs::path& asset,
    const nlohmann::json& document,
    const CxBusinessWorkflowCaseResult& seed,
    bool& semanticValidated,
    std::string& reason)
{
    semanticValidated = false;
    const std::string name = ToLowerAscii(asset.filename().string());
    const auto requireString = [&](const char* key, std::string& value)
    {
        const auto member = document.find(key);
        if (member == document.end() || !member->is_string() ||
            member->get<std::string>().empty())
        {
            reason = std::string("required non-empty string missing: ") + key;
            return false;
        }
        value = member->get<std::string>();
        return true;
    };
    const auto requireBoolean = [&](const char* key, bool expected)
    {
        const auto member = document.find(key);
        if (member == document.end() || !member->is_boolean() ||
            member->get<bool>() != expected)
        {
            reason = std::string("required boolean has the wrong value: ") + key;
            return false;
        }
        return true;
    };
    const auto stringArray = [&](const char* key, std::set<std::string>& values)
    {
        const auto member = document.find(key);
        if (member == document.end() || !member->is_array() || member->empty())
        {
            reason = std::string("required non-empty string array missing: ") + key;
            return false;
        }
        for (const nlohmann::json& item : *member)
        {
            if (!item.is_string() || item.get<std::string>().empty())
            {
                reason = std::string("array contains a non-string value: ") + key;
                return false;
            }
            values.insert(item.get<std::string>());
        }
        return true;
    };

    if (name == "contract.json")
    {
        semanticValidated = true;
        std::string contractId;
        return requireString("contract_id", contractId) &&
               contractId == kManifestSchema &&
               requireBoolean("flat_manifest", true) &&
               requireBoolean("provider_results_must_be_observed", true) &&
               requireBoolean("no_simulated_algorithm_pass", true) &&
               requireBoolean("no_simulated_human_decision", true);
    }
    if (name == "profile.json")
    {
        semanticValidated = true;
        std::string profileId;
        std::set<std::string> channels;
        if (!requireString("profile_id", profileId) ||
            !requireBoolean("algorithm_execution", false) ||
            !stringArray("required_channels", channels))
        {
            return false;
        }
        static const std::set<std::string> required = {
            "execution", "detection", "display", "evaluation"};
        if (!std::includes(
                channels.begin(), channels.end(), required.begin(), required.end()))
        {
            reason = "profile required_channels does not contain all four independent channels";
            return false;
        }
        return true;
    }
    if (name == "catalog.json")
    {
        semanticValidated = true;
        std::string catalogId;
        std::set<std::string> entries;
        if (!requireString("catalog_id", catalogId) ||
            !stringArray("entries", entries))
        {
            return false;
        }
        for (const std::string& entry : entries)
        {
            const fs::path relative(entry);
            if (relative.is_absolute() || HasParentTraversal(relative))
            {
                reason = "catalog entry is not a safe case-relative path: " + entry;
                return false;
            }
            const std::string expected = ToLowerAscii(
                (seed.case_directory / relative).lexically_normal().generic_string());
            const bool declared = std::any_of(
                seed.required_assets.begin(), seed.required_assets.end(),
                [&](const fs::path& requiredAsset)
                {
                    return ToLowerAscii(requiredAsset.generic_string()) == expected;
                });
            if (!declared)
            {
                reason = "catalog entry is not declared as a required asset: " + entry;
                return false;
            }
        }
        return true;
    }
    if (name == "suite.json")
    {
        semanticValidated = true;
        std::string suiteId;
        std::set<std::string> stages;
        if (!requireString("suite_id", suiteId) ||
            !stringArray("stages", stages) ||
            !requireBoolean("negative_scan_fixtures_are_isolated", true))
        {
            return false;
        }
        for (int index = 0; index <= 9; ++index)
        {
            if (stages.count("T" + std::to_string(index)) == 0U)
            {
                reason = "suite stages must contain T0 through T9";
                return false;
            }
        }
        return true;
    }
    if (name == "output_plan.json")
    {
        semanticValidated = true;
        std::set<std::string> reports;
        if (!requireBoolean("generated_only_at_runtime", true) ||
            !requireBoolean("run_id_scoped", true) ||
            !stringArray("required_reports", reports))
        {
            return false;
        }
        static const std::set<std::string> requiredReports = {
            "business_workflow_summary.json",
            "business_workflow_report.md",
            "business_workflow_report.html",
            "business_workflow_audit.jsonl",
            "business_workflow_scan_debug.json",
            "business_workflow_risk_register.json"};
        if (!std::includes(
                reports.begin(), reports.end(),
                requiredReports.begin(), requiredReports.end()))
        {
            reason = "output plan does not contain every required evidence report";
            return false;
        }
        return true;
    }
    if (name == "roi.json")
    {
        semanticValidated = true;
        std::string roiId;
        std::string sourceImageId;
        std::string imageRevision;
        std::string recipeRevision;
        std::string coordinateSpace;
        if (!requireString("roi_id", roiId) ||
            !requireString("source_image_id", sourceImageId) ||
            !requireString("image_revision", imageRevision) ||
            !requireString("recipe_revision", recipeRevision) ||
            !requireString("coordinate_space", coordinateSpace))
        {
            return false;
        }
        const auto geometry = document.find("geometry");
        if (geometry == document.end() || !geometry->is_object() ||
            geometry->value("type", std::string()) != "rectangle" ||
            !geometry->contains("x") || !geometry->contains("y") ||
            !geometry->contains("width") || !geometry->contains("height") ||
            !(*geometry)["x"].is_number() || !(*geometry)["y"].is_number() ||
            !(*geometry)["width"].is_number() ||
            !(*geometry)["height"].is_number() ||
            (*geometry)["x"].get<double>() < 0.0 ||
            (*geometry)["y"].get<double>() < 0.0 ||
            (*geometry)["width"].get<double>() <= 0.0 ||
            (*geometry)["height"].get<double>() <= 0.0)
        {
            reason = "ROI geometry must be a positive rectangle";
            return false;
        }
        const bool matchesManifest = std::any_of(
            seed.rois.begin(), seed.rois.end(),
            [&](const CxBusinessWorkflowRoi& roi)
            {
                return roi.roi_id == roiId &&
                       roi.source_image_id == sourceImageId &&
                       roi.image_revision == imageRevision &&
                       roi.recipe_revision == recipeRevision &&
                       NormalizeToken(roi.coordinate_space) ==
                           NormalizeToken(coordinateSpace);
            });
        if (!matchesManifest)
        {
            reason = "ROI JSON identity does not match a typed ROI in the manifest";
            return false;
        }
        return true;
    }
    return true;
}

bool ValidateAssetContents(
    CxBusinessWorkflowCaseResult& seed,
    std::string& code,
    std::string& reason)
{
    for (const fs::path& asset : seed.required_assets)
    {
        CxBusinessWorkflowAssetPreflightRecord record;
        record.path = asset;
        std::error_code error;
        record.byte_size = fs::file_size(asset, error);
        if (error)
        {
            code = "ASSET_CONTENT_INVALID";
            reason = "cannot read required asset size: " + error.message();
            record.kind = "FILE";
            record.status = "FAIL";
            record.code = code;
            record.reason = reason;
            seed.asset_preflight.push_back(std::move(record));
            return false;
        }
        record.content_hash = Fnv1a64FileHash(asset, reason);
        if (record.content_hash.empty())
        {
            code = "ASSET_HASH_FAIL";
            record.kind = "FILE";
            record.status = "FAIL";
            record.code = code;
            record.reason = reason;
            seed.asset_preflight.push_back(std::move(record));
            return false;
        }
        const std::string initialContentHash = record.content_hash;

        const std::string extension = ToLowerAscii(asset.extension().string());
        if (extension == ".json")
        {
            record.kind = "JSON";
            bool semanticValidated = false;
            std::string text;
            if (!ReadTextFile(asset, text, reason))
            {
                code = "JSON_READ_FAIL";
                record.status = "FAIL";
                record.code = code;
                record.reason = reason;
                seed.asset_preflight.push_back(std::move(record));
                return false;
            }
            try
            {
                const nlohmann::json document = nlohmann::json::parse(text);
                if (!document.is_object())
                    throw std::runtime_error("top-level JSON value must be an object");
                std::string semanticReason;
                if (!ValidateKnownBusinessJsonContract(
                        asset, document, seed, semanticValidated, semanticReason))
                {
                    throw std::runtime_error(
                        "business JSON contract validation failed: " +
                        semanticReason);
                }
            }
            catch (const std::exception& errorValue)
            {
                code = "JSON_PARSE_FAIL";
                reason = asset.filename().string() + ": " + errorValue.what();
                record.status = "FAIL";
                record.code = code;
                record.reason = reason;
                seed.asset_preflight.push_back(std::move(record));
                return false;
            }
            record.status = "PASS";
            record.code = semanticValidated
                ? "JSON_CONTRACT_VALIDATED"
                : "JSON_PARSED";
            record.reason = semanticValidated
                ? "JSON syntax and known business contract semantics were validated"
                : "JSON syntax and top-level object were validated";
        }
        else if (extension == ".cxsc" || extension == ".cxs")
        {
            record.kind = "CXSCRIPT";
            std::string source;
            if (!ReadTextFile(asset, source, reason))
            {
                code = "CXSCRIPT_READ_FAIL";
                record.status = "FAIL";
                record.code = code;
                record.reason = reason;
                seed.asset_preflight.push_back(std::move(record));
                return false;
            }
            try
            {
                CxParserRuntimeOwner owner;
                std::string parserReason;
                if (!owner.Initialize(parserReason) ||
                    !owner.CompileScriptOnly(source, parserReason))
                {
                    code = "CXSCRIPT_COMPILE_FAIL";
                    reason = asset.filename().string() + ": " +
                        (parserReason.empty()
                             ? "CxScript compile returned false"
                             : parserReason);
                    record.status = "FAIL";
                    record.code = code;
                    record.reason = reason;
                    seed.asset_preflight.push_back(std::move(record));
                    return false;
                }
                owner.ClearAll();
            }
            catch (const std::exception& errorValue)
            {
                code = "CXSCRIPT_COMPILE_EXCEPTION";
                reason = asset.filename().string() + ": " + errorValue.what();
                record.status = "FAIL";
                record.code = code;
                record.reason = reason;
                seed.asset_preflight.push_back(std::move(record));
                return false;
            }
            catch (...)
            {
                code = "CXSCRIPT_COMPILE_EXCEPTION";
                reason = asset.filename().string() +
                    ": CxScript compile threw an unknown exception";
                record.status = "FAIL";
                record.code = code;
                record.reason = reason;
                seed.asset_preflight.push_back(std::move(record));
                return false;
            }
            record.status = "PASS";
            record.code = "CXSCRIPT_COMPILED";
            record.reason = "CxScript compiled serially through CxParserRuntimeOwner";
        }
        else if (IsImageAssetExtension(extension))
        {
            record.kind = "IMAGE";
            const cv::Mat image = cv::imread(asset.string(), cv::IMREAD_UNCHANGED);
            if (image.empty())
            {
                code = "IMAGE_DECODE_FAIL";
                reason = "image could not be decoded: " + asset.filename().string();
                record.status = "FAIL";
                record.code = code;
                record.reason = reason;
                seed.asset_preflight.push_back(std::move(record));
                return false;
            }
            record.width = image.cols;
            record.height = image.rows;
            record.channels = image.channels();
            record.status = "PASS";
            record.code = "IMAGE_DECODED";
            record.reason = "image decode, dimensions, channels, and content hash were observed";
        }
        else
        {
            record.kind = "FILE";
            record.status = "PASS";
            record.code = "FILE_READABLE";
            record.reason =
                "regular-file presence, size, and content hash were validated";
        }
        const std::string finalContentHash = Fnv1a64FileHash(asset, reason);
        if (finalContentHash.empty() || finalContentHash != initialContentHash)
        {
            code = finalContentHash.empty()
                ? "ASSET_HASH_FAIL"
                : "ASSET_CHANGED_DURING_PREFLIGHT";
            reason = finalContentHash.empty()
                ? reason
                : "required asset changed while content preflight was running";
            record.status = "FAIL";
            record.code = code;
            record.reason = reason;
            seed.asset_preflight.push_back(std::move(record));
            return false;
        }
        seed.asset_preflight.push_back(std::move(record));
    }
    return true;
}

const CxBusinessWorkflowAssetPreflightRecord* FindAssetPreflight(
    const CxBusinessWorkflowCaseResult& seed,
    const fs::path& path)
{
    for (const CxBusinessWorkflowAssetPreflightRecord& record : seed.asset_preflight)
    {
        if (PathComponentEqual(record.path, path))
            return &record;
    }
    return nullptr;
}

bool ResolveDeclaredAssetRef(
    const fs::path& canonicalRoot,
    const CxBusinessWorkflowCaseResult& seed,
    const std::string& reference,
    fs::path& resolved,
    std::string& reason)
{
    std::string relative = reference;
    const std::size_t fragment = relative.find('#');
    if (fragment != std::string::npos)
        relative.erase(fragment);
    if (relative.empty() || relative.find("://") != std::string::npos)
    {
        reason = "reference is not a case-local asset: " + reference;
        return false;
    }
    if (!ValidateRequiredAsset(
            canonicalRoot, seed.case_directory, relative, resolved, reason))
    {
        return false;
    }
    const std::string normalized = ToLowerAscii(resolved.generic_string());
    const bool declared = std::any_of(
        seed.required_assets.begin(), seed.required_assets.end(),
        [&](const fs::path& asset)
        {
            return ToLowerAscii(asset.generic_string()) == normalized;
        });
    if (!declared)
    {
        reason = "referenced asset is not declared in required_assets: " + relative;
        return false;
    }
    return true;
}

bool ValidateDetectionElementsShape(
    const std::vector<CxBusinessWorkflowDetectionElement>& elements,
    std::string& reason)
{
    std::set<std::string> elementIds;
    for (const CxBusinessWorkflowDetectionElement& element : elements)
    {
        if (element.element_id.empty() || element.element_type.empty() ||
            element.name.empty() || element.status.empty() ||
            element.source.empty() || element.coordinate_space.empty() ||
            element.geometry.empty() || element.evidence_ref.empty())
        {
            reason =
                "detection elements require identity, type, name, status, source, "
                "coordinate space, geometry, and evidence_ref";
            return false;
        }
        if (!elementIds.insert(element.element_id).second)
        {
            reason = "duplicate detection element_id: " + element.element_id;
            return false;
        }
        if (!std::isfinite(element.score) || element.score < 0.0 ||
            element.score > 1.0)
        {
            reason = "detection element score must be finite and within [0, 1]: " +
                element.element_id;
            return false;
        }
    }
    return true;
}

bool ValidateTypedReferences(
    const fs::path& canonicalRoot,
    const CxBusinessWorkflowCaseResult& seed,
    std::string& reason)
{
    std::map<std::string, const CxBusinessWorkflowImageRef*> images;
    for (const CxBusinessWorkflowImageRef& image : seed.image_refs)
    {
        if (!images.emplace(image.image_id, &image).second)
        {
            reason = "duplicate image_ref image_id: " + image.image_id;
            return false;
        }
        const std::string sourceType = NormalizeToken(image.source_type);
        if (sourceType != "FILE" && sourceType != "CAMERA" &&
            sourceType != "STREAM" && sourceType != "DATASET")
        {
            reason = "unsupported image_ref source_type: " + image.source_type;
            return false;
        }
        if ((sourceType == "FILE" || sourceType == "STREAM") &&
            image.uri.find("://") == std::string::npos)
        {
            fs::path asset;
            if (!ResolveDeclaredAssetRef(canonicalRoot, seed, image.uri, asset, reason))
                return false;
            const CxBusinessWorkflowAssetPreflightRecord* record =
                FindAssetPreflight(seed, asset);
            if (record == nullptr || record->kind != "IMAGE" || record->status != "PASS")
            {
                reason = "image_ref does not resolve to a decoded image: " + image.image_id;
                return false;
            }
            if ((image.width > 0 && image.width != record->width) ||
                (image.height > 0 && image.height != record->height) ||
                (image.channels > 0 && image.channels != record->channels))
            {
                reason = "image_ref dimensions/channels do not match decoded asset: " +
                    image.image_id;
                return false;
            }
            if (image.content_hash.empty() ||
                ToLowerAscii(image.content_hash) != ToLowerAscii(record->content_hash))
            {
                reason = "image_ref content_hash does not match decoded asset: " +
                    image.image_id;
                return false;
            }
        }
    }

    std::set<std::string> frameIds;
    for (const CxBusinessWorkflowFrameRef& frame : seed.frame_refs)
    {
        if (!frameIds.insert(frame.frame_id).second)
        {
            reason = "duplicate frame_ref frame_id: " + frame.frame_id;
            return false;
        }
        if (images.count(frame.image_id) == 0U)
        {
            reason = "frame_ref references an unknown image_id: " + frame.image_id;
            return false;
        }
        if (frame.uri.find("://") == std::string::npos)
        {
            fs::path ignored;
            if (!ResolveDeclaredAssetRef(canonicalRoot, seed, frame.uri, ignored, reason))
                return false;
        }
    }

    std::map<std::string, const CxBusinessWorkflowRoi*> rois;
    for (const CxBusinessWorkflowRoi& roi : seed.rois)
    {
        if (!rois.emplace(roi.roi_id, &roi).second)
        {
            reason = "duplicate ROI roi_id: " + roi.roi_id;
            return false;
        }
        const auto image = images.find(roi.source_image_id);
        if (image == images.end())
        {
            reason = "ROI references an unknown source_image_id: " + roi.source_image_id;
            return false;
        }
        if (roi.image_revision != image->second->image_revision)
        {
            reason = "ROI image_revision does not match its source image: " + roi.roi_id;
            return false;
        }
        const std::string geometry = ToLowerAscii(TrimAscii(roi.geometry));
        if (geometry.rfind("rectangle(", 0) == 0)
        {
            double x = 0.0;
            double y = 0.0;
            double width = 0.0;
            double height = 0.0;
            char trailing = '\0';
            if (std::sscanf(
                    geometry.c_str(), "rectangle(%lf,%lf,%lf,%lf)%c",
                    &x, &y, &width, &height, &trailing) != 4 ||
                x < 0.0 || y < 0.0 || width <= 0.0 || height <= 0.0)
            {
                reason = "ROI rectangle geometry is invalid: " + roi.roi_id;
                return false;
            }
            if (image->second->width > 0 && image->second->height > 0 &&
                (x + width > image->second->width ||
                 y + height > image->second->height))
            {
                reason = "ROI rectangle exceeds decoded image bounds: " + roi.roi_id;
                return false;
            }
        }
    }

    std::set<std::string> annotationIds;
    static const std::set<std::string> annotationTypes = {
        "POINT", "SEGMENT", "POLYLINE", "RECTANGLE", "POLYGON", "MASK",
        "EDGE_CANDIDATE"};
    for (const CxBusinessWorkflowAnnotation& annotation : seed.annotations)
    {
        if (!annotationIds.insert(annotation.object_id).second)
        {
            reason = "duplicate annotation object_id: " + annotation.object_id;
            return false;
        }
        if (annotationTypes.count(NormalizeToken(annotation.object_type)) == 0U)
        {
            reason = "unsupported annotation object_type: " + annotation.object_type;
            return false;
        }
        const auto image = images.find(annotation.source_image_id);
        if (image == images.end() ||
            image->second->image_revision != annotation.image_revision)
        {
            reason = "annotation image reference is unknown or revision-mismatched: " +
                annotation.object_id;
            return false;
        }
        const auto roi = rois.find(annotation.roi_id);
        if (roi == rois.end())
        {
            reason = "annotation references an unknown ROI: " + annotation.object_id;
            return false;
        }
        if (roi->second->source_image_id != annotation.source_image_id &&
            annotation.transform_ref.empty())
        {
            reason = "cross-image annotation/ROI reference requires transform_ref: " +
                annotation.object_id;
            return false;
        }
        if (!annotation.evidence_ref.empty() &&
            annotation.evidence_ref.find("://") == std::string::npos)
        {
            fs::path ignored;
            if (!ResolveDeclaredAssetRef(
                    canonicalRoot, seed, annotation.evidence_ref, ignored, reason))
            {
                reason = "annotation evidence_ref invalid for " +
                    annotation.object_id + ": " + reason;
                return false;
            }
        }
    }
    return true;
}

bool IsStepControlMetadataKey(const std::string& key)
{
    if (key.compare(0, 9, "expected_") == 0 ||
        key.compare(0, 5, "risk_") == 0 ||
        key.compare(0, 27, "contract_detection_element_") == 0)
    {
        return true;
    }
    static const std::set<std::string> controlKeys = {
        "action",
        "provider",
        "input_ref",
        "output_ref",
        "idempotency_key",
        "capability_status",
        "execution_status",
        "detection_status",
        "display_status",
        "evaluation_status",
        "requires_provider",
        "evidence_ref",
        "code",
        "reason",
        "decision",
        "operator_id",
        "occurred_at",
        "input_version",
        "output_version",
        "event_id"
    };
    return controlKeys.count(key) != 0U;
}

bool ParseStep(
    const FlatJsonObject& manifest,
    std::size_t index,
    StepDefinition& step,
    std::string& reason)
{
    step = StepDefinition{};
    step.index = index;
    const std::string prefix = IndexedKey("step_", index, "");
    if (!ReadRequiredString(manifest, prefix + "_action", step.action, reason) ||
        !ReadRequiredString(manifest, prefix + "_provider", step.provider, reason) ||
        !ReadRequiredString(manifest, prefix + "_input_ref", step.input_ref, reason) ||
        !ReadRequiredString(manifest, prefix + "_output_ref", step.output_ref, reason) ||
        !ReadRequiredString(manifest, prefix + "_idempotency_key", step.idempotency_key, reason) ||
        !ReadRequiredString(manifest, prefix + "_capability_status", step.capability_status, reason) ||
        !ReadRequiredString(manifest, prefix + "_expected_status", step.expected_status, reason) ||
        !ReadExpectedObservation(manifest, prefix, step.expected_observation, reason) ||
        !ReadOptionalBool(
            manifest, prefix + "_requires_provider", false,
            step.requires_provider, reason) ||
        !ReadOptionalBool(
            manifest, prefix + "_expected_rejection", false,
            step.expected_rejection, reason))
    {
        return false;
    }

    step.capability_status = NormalizeToken(step.capability_status);
    step.expected_status = NormalizeToken(step.expected_status);
    step.expected_rejection = step.expected_rejection ||
        step.expected_status == "EXPECTED_REJECTION" ||
        NormalizeToken(ReadOptionalString(manifest, prefix + "_decision")) ==
            "EXPECTED_REJECTION";
    if (!IsCapabilityStatus(step.capability_status))
    {
        reason = "unsupported capability status: " + prefix;
        return false;
    }
    if (!IsStepExpectedStatus(step.expected_status))
    {
        reason = "unsupported expected step status: " + prefix;
        return false;
    }

    step.evidence_ref = ReadOptionalString(manifest, prefix + "_evidence_ref");
    step.manifest_code = ReadOptionalString(manifest, prefix + "_code");
    step.manifest_reason = ReadOptionalString(manifest, prefix + "_reason");
    step.human_decision.decision =
        ReadOptionalString(manifest, prefix + "_decision");
    step.human_decision.operator_id =
        ReadOptionalString(manifest, prefix + "_operator_id");
    step.human_decision.occurred_at =
        ReadOptionalString(manifest, prefix + "_occurred_at");
    step.human_decision.input_version =
        ReadOptionalString(manifest, prefix + "_input_version");
    step.human_decision.output_version =
        ReadOptionalString(manifest, prefix + "_output_version");
    step.human_decision.event_id =
        ReadOptionalString(manifest, prefix + "_event_id");

    std::size_t contractElementCount = 0;
    if (!ReadOptionalCount(
            manifest, prefix + "_contract_detection_element_count",
            contractElementCount, reason))
    {
        return false;
    }
    for (std::size_t elementIndex = 1;
         elementIndex <= contractElementCount; ++elementIndex)
    {
        const std::string elementPrefix =
            prefix + IndexedKey("_contract_detection_element_", elementIndex, "");
        CxBusinessWorkflowDetectionElement element;
        if (!ReadRequiredString(
                manifest, elementPrefix + "_element_id", element.element_id, reason) ||
            !ReadRequiredString(
                manifest, elementPrefix + "_element_type", element.element_type, reason) ||
            !ReadRequiredString(
                manifest, elementPrefix + "_name", element.name, reason) ||
            !ReadRequiredString(
                manifest, elementPrefix + "_status", element.status, reason) ||
            !ReadRequiredString(
                manifest, elementPrefix + "_source", element.source, reason) ||
            !ReadRequiredString(
                manifest, elementPrefix + "_coordinate_space",
                element.coordinate_space, reason) ||
            !ReadRequiredString(
                manifest, elementPrefix + "_geometry", element.geometry, reason) ||
            !ReadRequiredString(
                manifest, elementPrefix + "_evidence_ref",
                element.evidence_ref, reason) ||
            !ReadOptionalDouble(
                manifest, elementPrefix + "_score", 0.0,
                element.score, reason) ||
            element.score < 0.0 || element.score > 1.0)
        {
            if (reason.empty())
                reason = "contract detection element score must be within [0, 1]";
            return false;
        }
        step.contract_fixture_elements.push_back(std::move(element));
    }
    if (!ValidateDetectionElementsShape(step.contract_fixture_elements, reason))
        return false;

    CxBusinessRiskRecord risk;
    risk.code = ReadOptionalString(manifest, prefix + "_risk_code");
    if (!risk.code.empty())
    {
        if (!ReadRequiredString(manifest, prefix + "_risk_severity", risk.severity, reason) ||
            !ReadRequiredString(manifest, prefix + "_risk_disposition", risk.disposition, reason) ||
            !ReadRequiredString(manifest, prefix + "_risk_reason", risk.reason, reason) ||
            !ReadOptionalBool(
                manifest, prefix + "_risk_blocking", false,
                risk.blocking, reason))
        {
            return false;
        }
        risk.stage = step.action;
        risk.severity = NormalizeToken(risk.severity);
        risk.disposition = NormalizeToken(risk.disposition);
        if (risk.disposition == "BLOCK_RELEASE")
            risk.blocking = true;
        step.risks.push_back(std::move(risk));
    }

    const std::string metadataPrefix = prefix + '_';
    for (const auto& item : manifest)
    {
        if (item.first.compare(0, metadataPrefix.size(), metadataPrefix) != 0)
            continue;
        const std::string metadataKey =
            item.first.substr(metadataPrefix.size());
        if (!IsStepControlMetadataKey(metadataKey))
            step.metadata[metadataKey] = item.second.text;
    }
    return true;
}

bool LoadCaseDefinition(
    const fs::path& canonicalRoot,
    const fs::path& manifestPath,
    CaseDefinition& definition,
    std::string& code,
    std::string& reason)
{
    definition = CaseDefinition{};
    code.clear();
    reason.clear();
    std::string text;
    if (!ReadTextFile(manifestPath, text, reason))
    {
        code = "MANIFEST_READ_FAIL";
        return false;
    }
    FlatJsonParser parser(text);
    if (!parser.Parse(definition.manifest, reason))
    {
        code = "MANIFEST_PARSE_FAIL";
        return false;
    }

    std::string schema;
    if (!ReadRequiredString(definition.manifest, "schema_version", schema, reason) ||
        schema != kManifestSchema)
    {
        if (reason.empty())
            reason = "unsupported manifest schema_version: " + schema;
        code = "MANIFEST_SCHEMA_INVALID";
        return false;
    }

    CxBusinessWorkflowCaseResult& seed = definition.seed;
    seed.manifest_path = manifestPath;
    seed.case_directory = manifestPath.parent_path();
    seed.normalized_path = seed.case_directory;
    if (!ReadRequiredString(
            definition.manifest, "internal_case_id",
            seed.internal_case_id, reason) ||
        !ReadRequiredString(
            definition.manifest, "case_id", seed.case_id, reason) ||
        !ReadRequiredString(
            definition.manifest, "expected_final_status",
            seed.expected_final_status, reason))
    {
        code = "MANIFEST_SCHEMA_INVALID";
        return false;
    }
    seed.expected_final_status = NormalizeToken(seed.expected_final_status);
    if (seed.expected_final_status != "PASS" &&
        seed.expected_final_status != "CONTRACT_FIXTURE_PASS" &&
        seed.expected_final_status != "FAIL" &&
        seed.expected_final_status !=
            "BUSINESS_WORKFLOW_ACCEPTED_WITH_PENDING")
    {
        code = "MANIFEST_SCHEMA_INVALID";
        reason = "unsupported expected_final_status";
        return false;
    }
    seed.display_name = ReadOptionalString(definition.manifest, "review_item");
    if (seed.display_name.empty())
        seed.display_name = ReadOptionalString(definition.manifest, "display_name");
    if (seed.display_name.empty())
    {
        std::error_code relativeError;
        seed.display_name = fs::relative(
            seed.case_directory, canonicalRoot, relativeError).generic_string();
        if (relativeError || seed.display_name.empty())
        {
            code = "MANIFEST_SCHEMA_INVALID";
            reason = "case display name cannot be derived from its directory";
            return false;
        }
    }
    seed.description = ReadOptionalString(definition.manifest, "description");

    std::size_t requiredAssetCount = 0;
    if (!ReadRequiredCount(
            definition.manifest, "required_asset_count",
            requiredAssetCount, reason))
    {
        code = "MANIFEST_SCHEMA_INVALID";
        return false;
    }
    std::set<std::string> assetKeys;
    for (std::size_t index = 1; index <= requiredAssetCount; ++index)
    {
        const std::string key = IndexedKey("required_asset_", index, "");
        std::string value;
        if (!ReadRequiredString(definition.manifest, key, value, reason))
        {
            code = "ASSET_MISSING";
            return false;
        }
        fs::path asset;
        if (!ValidateRequiredAsset(
                canonicalRoot, seed.case_directory, value, asset, reason))
        {
            code = "ASSET_MISSING";
            return false;
        }
        const std::string normalized = ToLowerAscii(asset.generic_string());
        if (!assetKeys.insert(normalized).second)
        {
            code = "MANIFEST_SCHEMA_INVALID";
            reason = "required asset is declared more than once";
            return false;
        }
        seed.required_assets.push_back(std::move(asset));
    }

    if (!ParseImageRefs(definition.manifest, seed, reason) ||
        !ParseFrameRefs(definition.manifest, seed, reason) ||
        !ParseRois(definition.manifest, seed, reason) ||
        !ParseAnnotations(definition.manifest, seed, reason))
    {
        code = "MANIFEST_SCHEMA_INVALID";
        return false;
    }
    if (!ValidateAssetContents(seed, code, reason))
        return false;
    if (!ValidateTypedReferences(canonicalRoot, seed, reason))
    {
        code = "TYPED_REFERENCE_INVALID";
        return false;
    }

    std::size_t stepCount = 0;
    if (!ReadRequiredCount(
            definition.manifest, "step_count", stepCount, reason) ||
        stepCount == 0U)
    {
        code = "EMPTY_STEPS";
        if (reason.empty())
            reason = "workflow case must contain at least one step";
        return false;
    }
    std::set<std::string> idempotencyKeys;
    for (std::size_t index = 1; index <= stepCount; ++index)
    {
        StepDefinition step;
        if (!ParseStep(definition.manifest, index, step, reason))
        {
            code = "MANIFEST_SCHEMA_INVALID";
            return false;
        }
        // Repeated idempotency keys are allowed only so the runtime can prove
        // replay deduplication.  A conflicting replay is rejected at execution.
        idempotencyKeys.insert(step.idempotency_key);
        definition.steps.push_back(std::move(step));
    }
    for (const StepDefinition& step : definition.steps)
    {
        const auto validateEvidenceRef = [&](const std::string& reference)
        {
            if (reference.empty() || reference.find("://") != std::string::npos)
                return true;
            fs::path ignored;
            return ResolveDeclaredAssetRef(
                canonicalRoot, seed, reference, ignored, reason);
        };
        if (!validateEvidenceRef(step.evidence_ref))
        {
            code = "STEP_EVIDENCE_REFERENCE_INVALID";
            reason = "step " + std::to_string(step.index) +
                " evidence_ref invalid: " + reason;
            return false;
        }
        for (const CxBusinessWorkflowDetectionElement& element :
             step.contract_fixture_elements)
        {
            if (!validateEvidenceRef(element.evidence_ref))
            {
                code = "STEP_EVIDENCE_REFERENCE_INVALID";
                reason = "step " + std::to_string(step.index) +
                    " fixture element evidence_ref invalid: " + reason;
                return false;
            }
        }
    }
    return true;
}

void AddScanRecord(
    CxBusinessWorkflowBatchResult& result,
    const fs::path& path,
    const fs::path& normalizedPath,
    const std::string& internalCaseId,
    const std::string& caseId,
    const std::string& outcome,
    const std::string& code,
    const std::string& reason)
{
    CxBusinessWorkflowScanRecord record;
    record.path = path;
    record.normalized_path = normalizedPath;
    record.internal_case_id = internalCaseId;
    record.case_id = caseId;
    record.outcome = outcome;
    record.code = code;
    record.reason = reason;
    result.scan_records.push_back(std::move(record));
}

bool DiscoverManifestPaths(
    const fs::path& canonicalRoot,
    CxBusinessWorkflowBatchResult& result,
    std::vector<fs::path>& manifests,
    std::string& reason)
{
    manifests.clear();
    std::error_code error;
    fs::recursive_directory_iterator iterator(
        canonicalRoot,
        fs::directory_options::skip_permission_denied,
        error);
    const fs::recursive_directory_iterator end;
    if (error)
    {
        reason = "cannot begin case-root scan: " + error.message();
        return false;
    }

    while (iterator != end)
    {
        const fs::directory_entry entry = *iterator;
        const fs::path entryPath = entry.path();
        std::error_code statusError;
        const fs::file_status status = entry.symlink_status(statusError);
        if (statusError)
        {
            if (entry.is_directory(statusError))
                iterator.disable_recursion_pending();
            AddScanRecord(
                result, entryPath, {}, {}, {}, "SKIPPED",
                "PATH_INSPECTION_FAILED", statusError.message());
            ++result.skipped_count;
        }
        else if (fs::is_symlink(status))
        {
            if (entry.is_directory(statusError))
                iterator.disable_recursion_pending();
            AddScanRecord(
                result, entryPath, {}, {}, {}, "SKIPPED",
                "SYMLINK_SKIPPED", "symbolic links are not scanned");
            ++result.skipped_count;
        }
        else if (fs::is_regular_file(status) &&
                 entryPath.filename() == "manifest.json")
        {
            ++result.discovered_count;
            std::error_code canonicalError;
            const fs::path canonicalManifest =
                fs::canonical(entryPath, canonicalError);
            if (canonicalError ||
                !IsPathWithin(canonicalRoot, canonicalManifest))
            {
                AddScanRecord(
                    result, entryPath, canonicalManifest, {}, {}, "REJECTED",
                    "PATH_OUTSIDE_ROOT",
                    canonicalError
                        ? "manifest cannot be canonicalized: " +
                            canonicalError.message()
                        : "manifest resolves outside the explicit case root");
                ++result.rejected_count;
            }
            else
            {
                manifests.push_back(canonicalManifest);
            }
        }

        error.clear();
        iterator.increment(error);
        if (error)
        {
            AddScanRecord(
                result, entryPath, {}, {}, {}, "SKIPPED",
                "DIRECTORY_ENUMERATION_FAILED", error.message());
            ++result.skipped_count;
            error.clear();
        }
    }

    std::sort(
        manifests.begin(), manifests.end(),
        [](const fs::path& left, const fs::path& right)
        {
            return ToLowerAscii(left.generic_string()) <
                   ToLowerAscii(right.generic_string());
        });
    return true;
}

bool DiscoverCases(
    const CxBusinessWorkflowBatchRequest& request,
    CxBusinessWorkflowBatchResult& result,
    std::vector<CaseDefinition>& definitions,
    std::string& reason)
{
    definitions.clear();
    std::error_code error;
    const fs::file_status rootStatus = fs::symlink_status(request.case_root, error);
    if (error || !fs::is_directory(rootStatus) || fs::is_symlink(rootStatus))
    {
        reason = error
            ? "case root cannot be inspected: " + error.message()
            : "case root must be an existing non-symlink directory";
        AddScanRecord(
            result, request.case_root, {}, {}, {}, "REJECTED",
            "CASE_ROOT_INVALID", reason);
        ++result.rejected_count;
        return false;
    }
    const fs::path canonicalRoot = fs::canonical(request.case_root, error);
    if (error)
    {
        reason = "case root cannot be canonicalized: " + error.message();
        AddScanRecord(
            result, request.case_root, {}, {}, {}, "REJECTED",
            "CASE_ROOT_INVALID", reason);
        ++result.rejected_count;
        return false;
    }
    if (!HasNormalizedPathComponent(canonicalRoot, "cxscript_runs") ||
        HasNormalizedPathComponent(canonicalRoot, "cxvision_repo"))
    {
        reason =
            "case root must be external to cxvision_repo and under cxscript_runs";
        AddScanRecord(
            result, request.case_root, canonicalRoot, {}, {}, "REJECTED",
            "CASE_ROOT_SCOPE_INVALID", reason);
        ++result.rejected_count;
        return false;
    }
    result.case_root = canonicalRoot;

    std::vector<fs::path> manifests;
    if (!DiscoverManifestPaths(canonicalRoot, result, manifests, reason))
    {
        AddScanRecord(
            result, canonicalRoot, canonicalRoot, {}, {}, "REJECTED",
            "CASE_SCAN_FAILED", reason);
        ++result.rejected_count;
        return false;
    }

    std::vector<CaseDefinition> parsed;
    for (const fs::path& manifest : manifests)
    {
        CaseDefinition definition;
        std::string code;
        std::string loadReason;
        if (!LoadCaseDefinition(
                canonicalRoot, manifest, definition, code, loadReason))
        {
            AddScanRecord(
                result, manifest, manifest.parent_path(),
                definition.seed.internal_case_id,
                definition.seed.case_id,
                "REJECTED", code, loadReason);
            ++result.rejected_count;
            continue;
        }
        parsed.push_back(std::move(definition));
    }

    std::map<std::string, std::vector<std::size_t>> byInternalId;
    for (std::size_t index = 0; index < parsed.size(); ++index)
        byInternalId[parsed[index].seed.internal_case_id].push_back(index);

    std::set<std::size_t> duplicateIndexes;
    for (const auto& item : byInternalId)
    {
        if (item.second.size() <= 1U)
            continue;
        duplicateIndexes.insert(item.second.begin(), item.second.end());
    }

    for (std::size_t index = 0; index < parsed.size(); ++index)
    {
        CaseDefinition& definition = parsed[index];
        if (duplicateIndexes.count(index) != 0U)
        {
            AddScanRecord(
                result, definition.seed.manifest_path,
                definition.seed.normalized_path,
                definition.seed.internal_case_id,
                definition.seed.case_id,
                "REJECTED", "DUPLICATE_INTERNAL_CASE_ID",
                "internal_case_id occurs in more than one discovered manifest");
            ++result.rejected_count;
            continue;
        }
        if (request.max_cases > 0U &&
            definitions.size() >= request.max_cases)
        {
            AddScanRecord(
                result, definition.seed.manifest_path,
                definition.seed.normalized_path,
                definition.seed.internal_case_id,
                definition.seed.case_id,
                "SKIPPED", "MAX_CASES_REACHED",
                "case was not executed because the explicit max_cases limit was reached");
            ++result.skipped_count;
            continue;
        }
        AddScanRecord(
            result, definition.seed.manifest_path,
            definition.seed.normalized_path,
            definition.seed.internal_case_id,
            definition.seed.case_id,
            "ACCEPTED", "CASE_ACCEPTED",
            "manifest and required assets satisfy the workflow schema");
        ++result.accepted_count;
        definitions.push_back(std::move(definition));
    }
    return true;
}

enum class ActionKind
{
    Unknown,
    CreateProject,
    OpenProject,
    CreateRecipe,
    CopyRecipe,
    ModifyRecipe,
    SelectImageSource,
    SelectAcquisitionMethod,
    AcquireImage,
    ImportImage,
    FreezeImage,
    CreateTrainingTemplate,
    AddAnnotation,
    SuggestAnnotation,
    ConfirmAnnotation,
    CorrectAnnotation,
    RejectAnnotation,
    RedoAnnotation,
    FreezeDatasetSnapshot,
    RequestTraining,
    RecordTrainingProgress,
    RecordTrainingArtifact,
    RunEvaluation,
    ApproveRelease,
    RejectRelease,
    PublishModel,
    ActivateModel,
    RunInference,
    ProjectResults,
    DisplayResults,
    HumanAccept,
    HumanReject,
    HumanCorrect,
    AddToRetraining,
    FreezeRetrainingSnapshot,
    RequestRetraining,
    RollbackModel,
    RollbackRecipe,
    ExportAudit,
    ExportEvidence
};

ActionKind ParseActionKind(const std::string& action)
{
    const std::string value = ToLowerAscii(TrimAscii(action));
    if (value == "create_project") return ActionKind::CreateProject;
    if (value == "open_project") return ActionKind::OpenProject;
    if (value == "create_recipe") return ActionKind::CreateRecipe;
    if (value == "copy_recipe") return ActionKind::CopyRecipe;
    if (value == "modify_recipe") return ActionKind::ModifyRecipe;
    if (value == "select_image_source") return ActionKind::SelectImageSource;
    if (value == "select_acquisition_method") return ActionKind::SelectAcquisitionMethod;
    if (value == "acquire_image") return ActionKind::AcquireImage;
    if (value == "import_image") return ActionKind::ImportImage;
    if (value == "freeze_image") return ActionKind::FreezeImage;
    if (value == "create_training_template") return ActionKind::CreateTrainingTemplate;
    if (value == "add_annotation" || value == "add_point_annotation" ||
        value == "add_segment_annotation" || value == "add_polyline_annotation" ||
        value == "add_rectangle_annotation" || value == "add_polygon_annotation" ||
        value == "add_mask_annotation" || value == "add_edge_candidate_annotation")
    {
        return ActionKind::AddAnnotation;
    }
    if (value == "suggest_annotation") return ActionKind::SuggestAnnotation;
    if (value == "confirm_annotation") return ActionKind::ConfirmAnnotation;
    if (value == "correct_annotation") return ActionKind::CorrectAnnotation;
    if (value == "reject_annotation") return ActionKind::RejectAnnotation;
    if (value == "redo_annotation") return ActionKind::RedoAnnotation;
    if (value == "freeze_dataset_snapshot") return ActionKind::FreezeDatasetSnapshot;
    if (value == "request_training") return ActionKind::RequestTraining;
    if (value == "record_training_progress") return ActionKind::RecordTrainingProgress;
    if (value == "record_training_artifact") return ActionKind::RecordTrainingArtifact;
    if (value == "run_evaluation") return ActionKind::RunEvaluation;
    if (value == "approve_release") return ActionKind::ApproveRelease;
    if (value == "reject_release") return ActionKind::RejectRelease;
    if (value == "publish_model") return ActionKind::PublishModel;
    if (value == "activate_model") return ActionKind::ActivateModel;
    if (value == "run_inference") return ActionKind::RunInference;
    if (value == "project_results") return ActionKind::ProjectResults;
    if (value == "display_results") return ActionKind::DisplayResults;
    if (value == "human_accept") return ActionKind::HumanAccept;
    if (value == "human_reject") return ActionKind::HumanReject;
    if (value == "human_correct") return ActionKind::HumanCorrect;
    if (value == "add_to_retraining") return ActionKind::AddToRetraining;
    if (value == "freeze_retraining_snapshot") return ActionKind::FreezeRetrainingSnapshot;
    if (value == "request_retraining") return ActionKind::RequestRetraining;
    if (value == "rollback_model") return ActionKind::RollbackModel;
    if (value == "rollback_recipe") return ActionKind::RollbackRecipe;
    if (value == "export_audit") return ActionKind::ExportAudit;
    if (value == "export_evidence") return ActionKind::ExportEvidence;
    return ActionKind::Unknown;
}

struct WorkflowState
{
    std::string label = "EMPTY";
    bool project_ready = false;
    bool recipe_ready = false;
    bool image_source_selected = false;
    bool acquisition_method_selected = false;
    bool image_ready = false;
    bool image_frozen = false;
    bool template_ready = false;
    bool annotation_available = false;
    bool annotation_confirmed = false;
    bool dataset_frozen = false;
    bool training_requested = false;
    bool training_artifact_available = false;
    bool evaluation_complete = false;
    bool release_approved = false;
    bool model_published = false;
    bool model_active = false;
    bool inference_complete = false;
    bool projection_complete = false;
    bool display_complete = false;
    bool human_review_complete = false;
    std::string review_disposition = "NONE";
    std::set<std::string> reviewed_input_versions;
    std::set<std::string> human_event_ids;
    bool retraining_selected = false;
    bool retraining_snapshot_frozen = false;
    bool has_pending = false;
    bool blocking_risk = false;
};

bool RequireState(bool condition, const char* message, std::string& reason)
{
    if (condition)
        return true;
    reason = message;
    return false;
}

bool CheckActionPrecondition(
    ActionKind action,
    const WorkflowState& state,
    std::string& reason)
{
    reason.clear();
    switch (action)
    {
    case ActionKind::CreateProject:
        return RequireState(!state.project_ready, "project already exists", reason);
    case ActionKind::OpenProject:
        return true;
    case ActionKind::CreateRecipe:
        return RequireState(
            state.project_ready && !state.recipe_ready,
            "project must exist and recipe must not already exist", reason);
    case ActionKind::CopyRecipe:
    case ActionKind::ModifyRecipe:
        return RequireState(state.recipe_ready, "recipe is not ready", reason);
    case ActionKind::SelectImageSource:
        return RequireState(state.recipe_ready, "recipe is not ready", reason);
    case ActionKind::SelectAcquisitionMethod:
        return RequireState(
            state.image_source_selected, "image source is not selected", reason);
    case ActionKind::AcquireImage:
        return RequireState(
            state.image_source_selected && state.acquisition_method_selected,
            "image source and acquisition method are required", reason);
    case ActionKind::ImportImage:
        return RequireState(
            state.image_source_selected, "image source is not selected", reason);
    case ActionKind::FreezeImage:
        return RequireState(state.image_ready, "image is not ready", reason);
    case ActionKind::CreateTrainingTemplate:
        return RequireState(state.image_frozen, "image is not frozen", reason);
    case ActionKind::AddAnnotation:
    case ActionKind::SuggestAnnotation:
        return RequireState(state.template_ready, "training template is not ready", reason);
    case ActionKind::ConfirmAnnotation:
    case ActionKind::CorrectAnnotation:
    case ActionKind::RejectAnnotation:
    case ActionKind::RedoAnnotation:
        return RequireState(
            state.annotation_available, "annotation is not available", reason);
    case ActionKind::FreezeDatasetSnapshot:
        return RequireState(
            state.annotation_confirmed, "confirmed annotation is required", reason);
    case ActionKind::RequestTraining:
        return RequireState(state.dataset_frozen, "dataset snapshot is not frozen", reason);
    case ActionKind::RecordTrainingProgress:
    case ActionKind::RecordTrainingArtifact:
        return RequireState(state.training_requested, "training was not requested", reason);
    case ActionKind::RunEvaluation:
        return RequireState(
            state.training_artifact_available,
            "training artifact is not available", reason);
    case ActionKind::ApproveRelease:
        return RequireState(
            state.evaluation_complete && !state.blocking_risk,
            state.blocking_risk
                ? "release is blocked by a declared blocking risk"
                : "evaluation is not complete",
            reason);
    case ActionKind::RejectRelease:
        return RequireState(state.evaluation_complete, "evaluation is not complete", reason);
    case ActionKind::PublishModel:
        return RequireState(state.release_approved, "release is not approved", reason);
    case ActionKind::ActivateModel:
        return RequireState(state.model_published, "model is not published", reason);
    case ActionKind::RunInference:
        return RequireState(state.model_active, "model is not active", reason);
    case ActionKind::ProjectResults:
        return RequireState(state.inference_complete, "inference is not complete", reason);
    case ActionKind::DisplayResults:
        return RequireState(state.projection_complete, "results are not projected", reason);
    case ActionKind::HumanAccept:
    case ActionKind::HumanReject:
    case ActionKind::HumanCorrect:
        return RequireState(state.display_complete, "results are not displayed", reason);
    case ActionKind::AddToRetraining:
        return RequireState(
            state.human_review_complete &&
                (state.review_disposition == "REJECTED" ||
                 state.review_disposition == "CORRECTED"),
            state.human_review_complete
                ? "only rejected or corrected review data may enter retraining"
                : "human review is not complete",
            reason);
    case ActionKind::FreezeRetrainingSnapshot:
        return RequireState(
            state.retraining_selected,
            "reviewed data was not selected for retraining", reason);
    case ActionKind::RequestRetraining:
        return RequireState(
            state.retraining_snapshot_frozen,
            "retraining snapshot is not frozen", reason);
    case ActionKind::RollbackModel:
        return RequireState(
            state.model_published || state.model_active,
            "no published or active model is available to roll back", reason);
    case ActionKind::RollbackRecipe:
        return RequireState(state.recipe_ready, "recipe is not ready", reason);
    case ActionKind::ExportAudit:
    case ActionKind::ExportEvidence:
        return RequireState(state.project_ready, "project is not ready", reason);
    case ActionKind::Unknown:
        reason = "action is not part of the business workflow schema";
        return false;
    }
    reason = "action precondition is undefined";
    return false;
}

void ApplyActionTransition(
    ActionKind action,
    WorkflowState& state,
    bool contractOnly)
{
    switch (action)
    {
    case ActionKind::CreateProject:
    case ActionKind::OpenProject:
        state.project_ready = true;
        state.label = "PROJECT_READY";
        break;
    case ActionKind::CreateRecipe:
    case ActionKind::CopyRecipe:
    case ActionKind::ModifyRecipe:
        state.recipe_ready = true;
        state.image_source_selected = false;
        state.acquisition_method_selected = false;
        state.image_ready = false;
        state.image_frozen = false;
        state.template_ready = false;
        state.annotation_available = false;
        state.annotation_confirmed = false;
        state.dataset_frozen = false;
        state.training_requested = false;
        state.training_artifact_available = false;
        state.evaluation_complete = false;
        state.release_approved = false;
        state.model_published = false;
        state.model_active = false;
        state.inference_complete = false;
        state.projection_complete = false;
        state.display_complete = false;
        state.human_review_complete = false;
        state.review_disposition = "NONE";
        state.retraining_selected = false;
        state.retraining_snapshot_frozen = false;
        state.label = "RECIPE_READY";
        break;
    case ActionKind::SelectImageSource:
        state.image_source_selected = true;
        state.label = "IMAGE_SOURCE_SELECTED";
        break;
    case ActionKind::SelectAcquisitionMethod:
        state.acquisition_method_selected = true;
        state.label = "ACQUISITION_METHOD_SELECTED";
        break;
    case ActionKind::AcquireImage:
    case ActionKind::ImportImage:
        state.image_ready = true;
        state.label = "IMAGE_READY";
        break;
    case ActionKind::FreezeImage:
        state.image_frozen = true;
        state.label = "IMAGE_FROZEN";
        break;
    case ActionKind::CreateTrainingTemplate:
        state.template_ready = true;
        state.label = "TRAINING_TEMPLATE_READY";
        break;
    case ActionKind::AddAnnotation:
    case ActionKind::SuggestAnnotation:
    case ActionKind::RedoAnnotation:
        state.annotation_available = true;
        state.annotation_confirmed = false;
        state.label = "ANNOTATION_DRAFT";
        break;
    case ActionKind::ConfirmAnnotation:
        state.annotation_confirmed = true;
        state.label = "ANNOTATION_CONFIRMED";
        break;
    case ActionKind::CorrectAnnotation:
        state.annotation_confirmed = false;
        state.label = "ANNOTATION_CORRECTED_DRAFT";
        break;
    case ActionKind::RejectAnnotation:
        state.annotation_available = false;
        state.annotation_confirmed = false;
        state.label = "ANNOTATION_REJECTED";
        break;
    case ActionKind::FreezeDatasetSnapshot:
        state.dataset_frozen = true;
        state.label = "DATASET_SNAPSHOT_FROZEN";
        break;
    case ActionKind::RequestTraining:
    case ActionKind::RequestRetraining:
        state.training_requested = true;
        state.label = action == ActionKind::RequestTraining
            ? "TRAINING_REQUESTED"
            : "RETRAINING_REQUESTED";
        break;
    case ActionKind::RecordTrainingProgress:
        state.label = "TRAINING_IN_PROGRESS";
        break;
    case ActionKind::RecordTrainingArtifact:
        state.training_artifact_available = true;
        state.label = "TRAINING_ARTIFACT_RECORDED";
        break;
    case ActionKind::RunEvaluation:
        state.evaluation_complete = true;
        state.label = "EVALUATION_COMPLETE";
        break;
    case ActionKind::ApproveRelease:
        state.release_approved = true;
        state.label = "RELEASE_APPROVED";
        break;
    case ActionKind::RejectRelease:
        state.release_approved = false;
        state.label = "RELEASE_REJECTED";
        break;
    case ActionKind::PublishModel:
        state.model_published = true;
        state.label = "MODEL_PUBLISHED";
        break;
    case ActionKind::ActivateModel:
        state.model_active = true;
        state.label = "MODEL_ACTIVE";
        break;
    case ActionKind::RunInference:
        state.inference_complete = true;
        state.label = "INFERENCE_COMPLETE";
        break;
    case ActionKind::ProjectResults:
        state.projection_complete = true;
        state.label = "RESULTS_PROJECTED";
        break;
    case ActionKind::DisplayResults:
        state.display_complete = true;
        state.label = "RESULTS_DISPLAYED";
        break;
    case ActionKind::HumanAccept:
        state.human_review_complete = true;
        state.review_disposition = "ACCEPTED";
        state.label = "HUMAN_ACCEPTED";
        break;
    case ActionKind::HumanReject:
        state.human_review_complete = true;
        state.review_disposition = "REJECTED";
        state.label = "HUMAN_REJECTED";
        break;
    case ActionKind::HumanCorrect:
        state.human_review_complete = true;
        state.review_disposition = "CORRECTED";
        state.label = "HUMAN_CORRECTED";
        break;
    case ActionKind::AddToRetraining:
        state.retraining_selected = true;
        state.label = "RETRAINING_DATA_SELECTED";
        break;
    case ActionKind::FreezeRetrainingSnapshot:
        state.retraining_snapshot_frozen = true;
        state.label = "RETRAINING_SNAPSHOT_FROZEN";
        break;
    case ActionKind::RollbackModel:
        state.model_published = false;
        state.model_active = false;
        state.release_approved = false;
        state.inference_complete = false;
        state.projection_complete = false;
        state.display_complete = false;
        state.human_review_complete = false;
        state.review_disposition = "NONE";
        state.retraining_selected = false;
        state.retraining_snapshot_frozen = false;
        state.label = "MODEL_ROLLED_BACK";
        break;
    case ActionKind::RollbackRecipe:
        state.image_source_selected = false;
        state.acquisition_method_selected = false;
        state.image_ready = false;
        state.image_frozen = false;
        state.template_ready = false;
        state.annotation_available = false;
        state.annotation_confirmed = false;
        state.dataset_frozen = false;
        state.training_requested = false;
        state.training_artifact_available = false;
        state.evaluation_complete = false;
        state.release_approved = false;
        state.model_published = false;
        state.model_active = false;
        state.inference_complete = false;
        state.projection_complete = false;
        state.display_complete = false;
        state.human_review_complete = false;
        state.review_disposition = "NONE";
        state.retraining_selected = false;
        state.retraining_snapshot_frozen = false;
        state.label = "RECIPE_ROLLED_BACK";
        break;
    case ActionKind::ExportAudit:
        state.label = "AUDIT_EXPORTED";
        break;
    case ActionKind::ExportEvidence:
        state.label = "EVIDENCE_EXPORTED";
        break;
    case ActionKind::Unknown:
        break;
    }
    const char* scopePrefix = contractOnly ? "CONTRACT_" : "BUSINESS_";
    if (state.label != "EMPTY" && state.label.rfind(scopePrefix, 0) != 0)
    {
        state.label = std::string(scopePrefix) + state.label;
    }
}

bool IsHumanAction(ActionKind action)
{
    return action == ActionKind::ConfirmAnnotation ||
           action == ActionKind::CorrectAnnotation ||
           action == ActionKind::RejectAnnotation ||
           action == ActionKind::RedoAnnotation ||
           action == ActionKind::ApproveRelease ||
           action == ActionKind::RejectRelease ||
           action == ActionKind::HumanAccept ||
           action == ActionKind::HumanReject ||
           action == ActionKind::HumanCorrect;
}

bool HasCompleteHumanDecision(const CxBusinessWorkflowHumanDecision& decision)
{
    return !decision.decision.empty() && !decision.operator_id.empty() &&
           !decision.occurred_at.empty() && !decision.input_version.empty() &&
           !decision.output_version.empty();
}

bool IsReviewDispositionAction(ActionKind action)
{
    return action == ActionKind::HumanAccept ||
           action == ActionKind::HumanReject ||
           action == ActionKind::HumanCorrect;
}

bool HumanDecisionMatchesAction(
    ActionKind action,
    const CxBusinessWorkflowHumanDecision& decision)
{
    const std::string value = NormalizeToken(decision.decision);
    if (action == ActionKind::ConfirmAnnotation)
        return value == "CONFIRM" || value == "CONFIRMED" ||
               value == "CONTRACT_FIXTURE_CONFIRM";
    if (action == ActionKind::CorrectAnnotation)
        return value == "CORRECT" || value == "CORRECTED" ||
               value == "CONTRACT_FIXTURE_CORRECT";
    if (action == ActionKind::RejectAnnotation)
        return value == "REJECT" || value == "REJECTED" ||
               value == "CONTRACT_FIXTURE_REJECT";
    if (action == ActionKind::RedoAnnotation)
        return value == "REDO" || value == "REDONE" ||
               value == "CONTRACT_FIXTURE_REDO";
    if (action == ActionKind::ApproveRelease)
        return value == "APPROVE" || value == "APPROVED" ||
               value == "CONTRACT_FIXTURE_APPROVE";
    if (action == ActionKind::RejectRelease)
        return value == "REJECT" || value == "REJECTED" ||
               value == "CONTRACT_FIXTURE_REJECT";
    if (action == ActionKind::HumanAccept)
        return value == "ACCEPT" || value == "ACCEPTED" ||
               value == "HUMAN_ACCEPT" || value == "CONTRACT_FIXTURE_ACCEPT";
    if (action == ActionKind::HumanReject)
        return value == "REJECT" || value == "REJECTED" ||
               value == "HUMAN_REJECT" || value == "CONTRACT_FIXTURE_REJECT";
    if (action == ActionKind::HumanCorrect)
        return value == "CORRECT" || value == "CORRECTED" ||
               value == "HUMAN_CORRECT" || value == "CONTRACT_FIXTURE_CORRECT";
    return true;
}

bool ValidateReviewReceipt(
    ActionKind action,
    const CxBusinessWorkflowHumanDecision& decision,
    bool providerBacked,
    const std::string& expectedInputVersion,
    const std::string& expectedOutputVersion,
    const WorkflowState& state,
    std::string& reason)
{
    if (!IsHumanAction(action))
        return true;
    if (!HasCompleteHumanDecision(decision))
    {
        reason = "review action requires decision, operator, time, input version, and output version";
        return false;
    }
    if (providerBacked && decision.event_id.empty())
    {
        reason = "provider-backed human action requires an immutable event_id";
        return false;
    }
    if (providerBacked && state.human_event_ids.count(decision.event_id) != 0U)
    {
        reason = "human action event_id was already consumed";
        return false;
    }
    if (!HumanDecisionMatchesAction(action, decision))
    {
        reason = "review decision does not match the requested action";
        return false;
    }
    if (providerBacked &&
        (decision.input_version != expectedInputVersion ||
         decision.output_version != expectedOutputVersion))
    {
        reason =
            "provider-backed human receipt input/output versions must match the "
            "requested input_ref and committed output_ref";
        return false;
    }
    if (!IsReviewDispositionAction(action))
        return true;
    if (state.reviewed_input_versions.count(decision.input_version) != 0U)
    {
        reason = "the review input version already has an immutable disposition";
        return false;
    }
    return true;
}

void RecordReviewReceipt(
    ActionKind action,
    const CxBusinessWorkflowHumanDecision& decision,
    WorkflowState& state)
{
    if (!decision.event_id.empty())
        state.human_event_ids.insert(decision.event_id);
    if (!IsReviewDispositionAction(action))
        return;
    state.reviewed_input_versions.insert(decision.input_version);
}

CxBusinessObservationRecord MakeObservation(
    const std::string& execution,
    const std::string& reason)
{
    CxBusinessObservationRecord observation;
    observation.execution_status = execution;
    observation.detection_status = "NOT_EVALUATED";
    observation.display_status = "NOT_DISPLAYED";
    observation.evaluation_status = "NOT_EVALUATED";
    observation.reason = reason;
    return observation;
}

void NormalizeObservation(CxBusinessObservationRecord& observation)
{
    observation.execution_status = NormalizeToken(observation.execution_status);
    observation.detection_status = NormalizeToken(observation.detection_status);
    observation.display_status = NormalizeToken(observation.display_status);
    observation.evaluation_status = NormalizeToken(observation.evaluation_status);
}

bool ValidateObservationContract(
    const CxBusinessObservationRecord& observation,
    std::string& reason)
{
    if (!IsExecutionStatus(observation.execution_status) ||
        !IsDetectionStatus(observation.detection_status) ||
        !IsDisplayStatus(observation.display_status) ||
        !IsEvaluationStatus(observation.evaluation_status))
    {
        reason = "provider returned an unsupported observation status";
        return false;
    }
    if (observation.execution_status == "CONTRACT_COMPLETED")
    {
        reason =
            "provider observations cannot report CONTRACT_COMPLETED as real execution";
        return false;
    }
    if (!ValidateDetectionElementsShape(observation.detection_elements, reason))
        return false;
    if (observation.detection_status == "DETECTED" &&
        observation.detection_elements.empty())
    {
        reason = "DETECTED requires at least one detection element";
        return false;
    }
    if (observation.detection_status == "ZERO_DETECTION" &&
        !observation.detection_elements.empty())
    {
        reason = "ZERO_DETECTION cannot contain detection elements";
        return false;
    }
    if (!observation.detection_elements.empty() &&
        observation.detection_status != "DETECTED")
    {
        reason = "provider detection elements require detection_status=DETECTED";
        return false;
    }
    if ((observation.execution_status == "NOT_EXECUTED" ||
         observation.execution_status == "REJECTED") &&
        observation.detection_status == "DETECTED")
    {
        reason = "a non-executed or rejected step cannot report detections";
        return false;
    }
    return true;
}

CxBusinessRiskRecord MakeGeneratedRisk(
    const std::string& code,
    const std::string& severity,
    const std::string& disposition,
    const std::string& stage,
    const std::string& reason,
    bool blocking)
{
    CxBusinessRiskRecord risk;
    risk.code = code;
    risk.severity = severity;
    risk.disposition = disposition;
    risk.stage = stage;
    risk.reason = reason;
    risk.blocking = blocking;
    return risk;
}

std::string StepFingerprint(const StepDefinition& definition)
{
    std::ostringstream fingerprint;
    const auto append = [&](const std::string& value)
    {
        fingerprint << value.size() << ':' << value;
    };
    append(definition.action);
    append(definition.provider);
    append(definition.input_ref);
    append(definition.output_ref);
    append(definition.requires_provider ? "1" : "0");
    append(definition.capability_status);
    append(definition.human_decision.decision);
    append(definition.human_decision.operator_id);
    append(definition.human_decision.occurred_at);
    append(definition.human_decision.input_version);
    append(definition.human_decision.output_version);
    append(definition.human_decision.event_id);
    for (const CxBusinessWorkflowDetectionElement& element :
         definition.contract_fixture_elements)
    {
        append(element.element_id);
        append(element.element_type);
        append(element.name);
        append(element.status);
        append(element.source);
        append(element.coordinate_space);
        append(element.geometry);
        append(element.evidence_ref);
        std::ostringstream score;
        score << std::setprecision(17) << element.score;
        append(score.str());
    }
    for (const auto& item : definition.metadata)
    {
        append(item.first);
        append(item.second);
    }
    return fingerprint.str();
}

bool ExpectedStatusMatches(
    const StepDefinition& definition,
    const CxBusinessWorkflowStepResult& result)
{
    if (definition.expected_status == "EXPECTED_REJECTION")
        return result.code == "EXPECTED_REJECTION";
    if (definition.expected_status == "NOT_AVAILABLE")
    {
        return result.status == "PENDING" &&
               result.capability_status == "NOT_AVAILABLE";
    }
    return definition.expected_status == result.status;
}

bool ExpectedObservationMatches(
    const CxBusinessObservationRecord& expected,
    const CxBusinessObservationRecord& actual,
    std::string& reason)
{
    if (expected.execution_status != actual.execution_status)
    {
        reason = "execution_status expected " + expected.execution_status +
                 " but observed " + actual.execution_status;
        return false;
    }
    if (expected.detection_status != actual.detection_status)
    {
        reason = "detection_status expected " + expected.detection_status +
                 " but observed " + actual.detection_status;
        return false;
    }
    if (expected.display_status != actual.display_status)
    {
        reason = "display_status expected " + expected.display_status +
                 " but observed " + actual.display_status;
        return false;
    }
    if (expected.evaluation_status != actual.evaluation_status)
    {
        reason = "evaluation_status expected " + expected.evaluation_status +
                 " but observed " + actual.evaluation_status;
        return false;
    }
    return true;
}

struct IdempotencyRecord
{
    std::string fingerprint;
    CxBusinessWorkflowStepResult result;
};

void RejectExpectedAction(
    CxBusinessWorkflowStepResult& result,
    const std::string& rejectionReason)
{
    result.status = "PASS";
    result.code = "EXPECTED_REJECTION";
    result.reason = rejectionReason;
    result.state_after = result.state_before;
    result.state_changed = false;
    result.observation = MakeObservation("REJECTED", rejectionReason);
    if (result.verification_scope == "CONTRACT_FIXTURE")
    {
        result.contract_validation_status = "PASS";
        result.contract_only = true;
    }
}

void SetStepFailure(
    CxBusinessWorkflowStepResult& result,
    const std::string& code,
    const std::string& failureReason)
{
    result.status = "FAIL";
    result.code = code;
    result.reason = failureReason;
    result.state_after = result.state_before;
    result.state_changed = false;
    result.observation = MakeObservation("FAILED", failureReason);
    if (result.verification_scope == "CONTRACT_FIXTURE")
    {
        result.contract_validation_status = "FAIL";
        result.contract_only = true;
    }
    result.risks.push_back(MakeGeneratedRisk(
        "WORKFLOW_CONTRACT_FAILURE", "ERROR", "BLOCK_RELEASE",
        result.action, failureReason, true));
}

void SetStepPending(
    CxBusinessWorkflowStepResult& result,
    const std::string& code,
    const std::string& pendingReason,
    const std::string& detectionStatus = "NOT_EVALUATED")
{
    result.status = "PENDING";
    result.code = code;
    result.reason = pendingReason;
    result.state_after = result.state_before;
    result.state_changed = false;
    result.observation = MakeObservation("NOT_EXECUTED", pendingReason);
    result.observation.detection_status = detectionStatus;
    if (result.verification_scope == "CONTRACT_FIXTURE")
    {
        result.contract_validation_status = "PENDING";
        result.contract_only = true;
    }
    result.risks.push_back(MakeGeneratedRisk(
        code, "WARNING", "DEFER", result.action, pendingReason, false));
}

void ValidateStepExpectations(
    const StepDefinition& definition,
    const WorkflowState& stateBefore,
    WorkflowState& state,
    CxBusinessWorkflowStepResult& result)
{
    if (!ExpectedStatusMatches(definition, result))
    {
        const std::string mismatch =
            "step status expected " + definition.expected_status +
            " but observed " + result.status;
        state = stateBefore;
        SetStepFailure(result, "EXPECTED_STATUS_MISMATCH", mismatch);
        return;
    }
    std::string mismatch;
    if (!ExpectedObservationMatches(
            definition.expected_observation, result.observation, mismatch))
    {
        state = stateBefore;
        SetStepFailure(result, "EXPECTED_OBSERVATION_MISMATCH", mismatch);
    }
}

CxBusinessWorkflowStepResult ExecuteStep(
    const CxBusinessWorkflowBatchRequest& request,
    const CaseDefinition& definition,
    const StepDefinition& step,
    WorkflowState& state,
    const WorkflowState* contractPrerequisiteState,
    std::map<std::string, IdempotencyRecord>& idempotency)
{
    CxBusinessWorkflowStepResult result;
    result.index = step.index;
    result.action = step.action;
    result.provider = step.provider;
    result.input_ref = step.input_ref;
    result.output_ref = step.output_ref;
    result.idempotency_key = step.idempotency_key;
    result.declared_capability_status = step.capability_status;
    result.capability_status = step.capability_status;
    result.expected_status = step.expected_status;
    result.expected_rejection = step.expected_rejection;
    result.requires_provider = step.requires_provider;
    const bool contractValidation =
        !step.requires_provider || step.expected_rejection;
    result.verification_scope = contractValidation
        ? "CONTRACT_FIXTURE"
        : "BUSINESS_EXECUTION";
    result.contract_validation_status = contractValidation
        ? "PENDING"
        : "NOT_APPLICABLE";
    result.evidence_ref = step.evidence_ref;
    result.human_decision = step.human_decision;
    result.contract_fixture_elements = step.contract_fixture_elements;
    result.risks = step.risks;
    result.state_before = state.label;
    result.state_after = state.label;
    const auto finish = [&result]()
    {
        result.recorded_at = UtcTimestamp();
        return result;
    };

    for (const CxBusinessRiskRecord& risk : result.risks)
    {
        if (risk.blocking)
            state.blocking_risk = true;
    }

    const auto capabilityOverride =
        request.provider_capability_status.find(step.provider);
    if (capabilityOverride != request.provider_capability_status.end())
        result.capability_status = NormalizeToken(capabilityOverride->second);
    if (!IsCapabilityStatus(result.capability_status))
    {
        SetStepFailure(
            result, "CAPABILITY_STATUS_INVALID",
            "request-level provider capability status is unsupported");
        return finish();
    }

    const std::string fingerprint = StepFingerprint(step);
    const auto prior = idempotency.find(step.idempotency_key);
    if (prior != idempotency.end())
    {
        if (prior->second.fingerprint != fingerprint)
        {
            if (step.expected_rejection)
                RejectExpectedAction(result, "idempotency key conflict was rejected");
            else
                SetStepFailure(
                    result, "IDEMPOTENCY_CONFLICT",
                    "idempotency key was reused for a different request");
        }
        else
        {
            result.status = prior->second.result.status;
            result.code = "IDEMPOTENT_REPLAY";
            result.reason = "matching idempotent request was deduplicated";
            result.output_ref = prior->second.result.output_ref;
            result.capability_status = prior->second.result.capability_status;
            result.state_after = state.label;
            result.state_changed = false;
            result.deduplicated = true;
            result.contract_only = prior->second.result.contract_only;
            result.contract_validation_status =
                prior->second.result.contract_validation_status;
            result.contract_fixture_elements =
                prior->second.result.contract_fixture_elements;
            result.provider_executed = false;
            result.observation = prior->second.result.observation;
            result.human_decision = prior->second.result.human_decision;
            result.risks = prior->second.result.risks;
        }
        const WorkflowState stateBefore = state;
        ValidateStepExpectations(step, stateBefore, state, result);
        return finish();
    }

    const WorkflowState stateBefore = state;
    const ActionKind action = ParseActionKind(step.action);
    std::string preconditionReason;
    const bool preconditionMet =
        CheckActionPrecondition(action, state, preconditionReason);
    if (!preconditionMet)
    {
        bool contractPreconditionMet = false;
        if (step.requires_provider && contractPrerequisiteState != nullptr)
        {
            std::string ignored;
            contractPreconditionMet = CheckActionPrecondition(
                action, *contractPrerequisiteState, ignored);
        }
        if (step.expected_rejection)
        {
            RejectExpectedAction(result, preconditionReason);
        }
        else if (action != ActionKind::Unknown && contractPreconditionMet)
        {
            SetStepPending(
                result, "UPSTREAM_CONTRACT_FIXTURE",
                "real provider execution cannot consume contract-only prerequisite state",
                result.capability_status == "NOT_AVAILABLE"
                    ? "MODEL_MISSING"
                    : "NOT_EVALUATED");
            state.has_pending = true;
        }
        else if (action != ActionKind::Unknown && state.has_pending)
        {
            SetStepPending(
                result, "UPSTREAM_PENDING",
                "action cannot proceed while an upstream capability is pending: " +
                    preconditionReason);
            state.has_pending = true;
        }
        else
        {
            SetStepFailure(
                result,
                action == ActionKind::Unknown
                    ? "UNKNOWN_ACTION"
                    : "INVALID_STATE_TRANSITION",
                preconditionReason);
        }
        ValidateStepExpectations(step, stateBefore, state, result);
        idempotency.emplace(
            step.idempotency_key, IdempotencyRecord{fingerprint, result});
        return finish();
    }

    if (step.expected_rejection)
    {
        SetStepFailure(
            result, "EXPECTED_REJECTION_NOT_OBSERVED",
            "action preconditions were satisfied, so the expected rejection did not occur");
        ValidateStepExpectations(step, stateBefore, state, result);
        idempotency.emplace(
            step.idempotency_key, IdempotencyRecord{fingerprint, result});
        return finish();
    }

    if (result.capability_status == "NOT_AVAILABLE" ||
        result.capability_status == "PENDING")
    {
        const bool unavailable = result.capability_status == "NOT_AVAILABLE";
        SetStepPending(
            result,
            unavailable ? "CAPABILITY_NOT_AVAILABLE" : "CAPABILITY_PENDING",
            unavailable
                ? "declared provider capability is not available"
                : "declared provider capability is pending",
            unavailable ? "MODEL_MISSING" : "NOT_EVALUATED");
        state.has_pending = true;
        ValidateStepExpectations(step, stateBefore, state, result);
        idempotency.emplace(
            step.idempotency_key, IdempotencyRecord{fingerprint, result});
        return finish();
    }

    if (IsHumanAction(action) && !step.requires_provider &&
        !HasCompleteHumanDecision(step.human_decision))
    {
        SetStepPending(
            result, "HUMAN_DECISION_NOT_BOUND",
            "human action requires decision, operator, time, input version, and output version");
        result.observation.evaluation_status = "PENDING_HUMAN";
        state.has_pending = true;
        ValidateStepExpectations(step, stateBefore, state, result);
        idempotency.emplace(
            step.idempotency_key, IdempotencyRecord{fingerprint, result});
        return finish();
    }

    if (step.requires_provider)
    {
        if (!request.provider_executor)
        {
            SetStepPending(
                result, "PROVIDER_EXECUTOR_NOT_BOUND",
                "step requires a real provider executor, but none was supplied");
            state.has_pending = true;
            ValidateStepExpectations(step, stateBefore, state, result);
            idempotency.emplace(
                step.idempotency_key, IdempotencyRecord{fingerprint, result});
            return finish();
        }

        CxBusinessWorkflowProviderRequest providerRequest;
        providerRequest.run_id = request.run_id;
        providerRequest.internal_case_id = definition.seed.internal_case_id;
        providerRequest.case_id = definition.seed.case_id;
        providerRequest.action = step.action;
        providerRequest.provider = step.provider;
        providerRequest.input_ref = step.input_ref;
        providerRequest.requested_output_ref = step.output_ref;
        providerRequest.idempotency_key = step.idempotency_key;
        providerRequest.state_before = state.label;
        providerRequest.case_directory = definition.seed.case_directory;
        providerRequest.manifest_path = definition.seed.manifest_path;
        providerRequest.metadata = step.metadata;

        CxBusinessWorkflowProviderResult providerResult;
        try
        {
            providerResult = request.provider_executor(providerRequest);
        }
        catch (const std::exception& error)
        {
            SetStepFailure(
                result, "PROVIDER_EXCEPTION",
                std::string("provider executor threw an exception: ") + error.what());
            ValidateStepExpectations(step, stateBefore, state, result);
            idempotency.emplace(
                step.idempotency_key, IdempotencyRecord{fingerprint, result});
            return finish();
        }
        catch (...)
        {
            SetStepFailure(
                result, "PROVIDER_EXCEPTION",
                "provider executor threw an unknown exception");
            ValidateStepExpectations(step, stateBefore, state, result);
            idempotency.emplace(
                step.idempotency_key, IdempotencyRecord{fingerprint, result});
            return finish();
        }

        result.provider_executed = providerResult.executed;
        result.status = NormalizeToken(providerResult.status);
        result.code = providerResult.code.empty()
            ? "PROVIDER_RESULT"
            : providerResult.code;
        result.reason = providerResult.reason;
        if (!providerResult.output_ref.empty())
            result.output_ref = providerResult.output_ref;
        result.observation = std::move(providerResult.observation);
        if (IsHumanAction(action))
            result.human_decision = std::move(providerResult.human_decision);
        if (result.observation.execution_status.empty())
        {
            result.observation.execution_status = providerResult.executed
                ? "COMPLETED"
                : "NOT_EXECUTED";
        }
        if (result.observation.detection_status.empty())
            result.observation.detection_status = "NOT_EVALUATED";
        if (result.observation.display_status.empty())
            result.observation.display_status = "NOT_DISPLAYED";
        if (result.observation.evaluation_status.empty())
            result.observation.evaluation_status = "NOT_EVALUATED";
        NormalizeObservation(result.observation);
        result.risks.insert(
            result.risks.end(),
            providerResult.risks.begin(), providerResult.risks.end());

        if (result.status != "PASS" && result.status != "PENDING" &&
            result.status != "FAIL")
        {
            SetStepFailure(
                result, "PROVIDER_RESULT_INVALID",
                "provider status must be PASS, PENDING, or FAIL");
        }
        else if (result.status == "PASS" && !providerResult.executed)
        {
            SetStepFailure(
                result, "PROVIDER_RESULT_INVALID",
                "provider cannot return PASS without real execution");
        }
        else if (result.status == "PASS" &&
                 result.observation.execution_status != "COMPLETED")
        {
            SetStepFailure(
                result, "PROVIDER_RESULT_INVALID",
                "provider PASS requires execution_status=COMPLETED");
        }
        else
        {
            std::string observationReason;
            if (!ValidateObservationContract(result.observation, observationReason))
            {
                SetStepFailure(
                    result, "OBSERVATION_CONTRACT_INVALID", observationReason);
            }
            else if (result.status == "PASS")
            {
                std::string receiptReason;
                if (!ValidateReviewReceipt(
                        action, result.human_decision, true,
                        step.input_ref, result.output_ref, state, receiptReason))
                {
                    SetStepFailure(
                        result, "HUMAN_RECEIPT_INVALID", receiptReason);
                }
                else
                {
                    ApplyActionTransition(action, state, false);
                    RecordReviewReceipt(action, result.human_decision, state);
                    result.state_after = state.label;
                    result.state_changed = result.state_after != result.state_before;
                }
            }
            else if (result.status == "PENDING")
            {
                state.has_pending = true;
                result.state_after = result.state_before;
                result.state_changed = false;
            }
            else
            {
                result.state_after = result.state_before;
                result.state_changed = false;
            }
        }
    }
    else
    {
        std::string receiptReason;
        if (!ValidateReviewReceipt(
                action, step.human_decision, false,
                {}, {}, state, receiptReason))
        {
            SetStepFailure(result, "HUMAN_FIXTURE_INVALID", receiptReason);
        }
        else
        {
            result.status = "PASS";
            result.code = step.manifest_code.empty()
                ? "WORKFLOW_CONTRACT_VALIDATED"
                : step.manifest_code;
            result.reason = step.manifest_reason.empty()
                ? "workflow contract transition was validated without claiming business execution"
                : step.manifest_reason;
            result.contract_only = true;
            result.contract_validation_status = "PASS";
            result.provider_executed = false;
            result.observation = MakeObservation(
                "NOT_EXECUTED",
                "contract fixture validated; no algorithm, device, or human execution is claimed");
            ApplyActionTransition(action, state, true);
            RecordReviewReceipt(action, step.human_decision, state);
            result.state_after = state.label;
            result.state_changed = result.state_after != result.state_before;
        }
    }

    for (const CxBusinessRiskRecord& risk : result.risks)
    {
        if (risk.blocking)
            state.blocking_risk = true;
    }
    ValidateStepExpectations(step, stateBefore, state, result);
    idempotency.emplace(
        step.idempotency_key, IdempotencyRecord{fingerprint, result});
    return finish();
}

std::uint64_t StableHash(const std::string& value)
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char ch : value)
    {
        hash ^= static_cast<std::uint64_t>(ch);
        hash *= 1099511628211ULL;
    }
    return hash;
}

std::string SafePathComponent(const std::string& value)
{
    std::string safe;
    safe.reserve(std::min<std::size_t>(value.size(), 48U));
    for (const unsigned char ch : value)
    {
        if (safe.size() >= 48U)
            break;
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.')
            safe.push_back(static_cast<char>(ch));
        else
            safe.push_back('_');
    }
    if (safe.empty() || safe == "." || safe == "..")
        safe = "case";
    return safe;
}

std::string HexHash(std::uint64_t value)
{
    std::ostringstream text;
    text << std::hex << std::setw(16) << std::setfill('0') << value;
    return text.str();
}

CxBusinessWorkflowCaseResult ExecuteCase(
    const CxBusinessWorkflowBatchRequest& request,
    const CaseDefinition& definition)
{
    CxBusinessWorkflowCaseResult result = definition.seed;
    result.run_id = request.run_id;
    result.started_at = UtcTimestamp();
    result.identity_key = request.run_id + '|' + result.internal_case_id + '|' +
        result.normalized_path.generic_string();
    const std::string directoryName =
        SafePathComponent(result.internal_case_id) + '_' +
        HexHash(StableHash(result.normalized_path.generic_string()));
    result.output_directory = request.out_dir / "cases" / directoryName;
    result.case_result_path = result.output_directory / "case_result.json";

    WorkflowState contractState;
    WorkflowState businessState;
    std::map<std::string, IdempotencyRecord> contractIdempotency;
    std::map<std::string, IdempotencyRecord> businessIdempotency;
    bool failed = false;
    bool pending = false;
    bool hasContractScope = false;
    bool hasBusinessScope = false;
    for (const StepDefinition& step : definition.steps)
    {
        const bool contractStep =
            !step.requires_provider || step.expected_rejection;
        WorkflowState& selectedState =
            contractStep ? contractState : businessState;
        std::map<std::string, IdempotencyRecord>& selectedIdempotency =
            contractStep ? contractIdempotency : businessIdempotency;
        CxBusinessWorkflowStepResult stepResult =
            ExecuteStep(
                request, definition, step, selectedState,
                contractStep ? nullptr : &contractState,
                selectedIdempotency);
        for (const CxBusinessRiskRecord& risk : stepResult.risks)
            result.risks.push_back(risk);
        if (stepResult.status == "FAIL")
            failed = true;
        else if (stepResult.status == "PENDING")
        {
            pending = true;
            ++result.pending_step_count;
        }
        if (stepResult.verification_scope == "CONTRACT_FIXTURE")
        {
            hasContractScope = true;
            if (stepResult.status == "PASS" &&
                stepResult.contract_validation_status == "PASS")
            {
                ++result.contract_fixture_step_pass_count;
            }
        }
        else if (stepResult.verification_scope == "BUSINESS_EXECUTION")
        {
            hasBusinessScope = true;
            if (stepResult.status == "PASS" && stepResult.provider_executed)
                ++result.provider_execution_step_pass_count;
        }
        result.steps.push_back(std::move(stepResult));
        if (failed)
            break;
    }

    result.verification_scope = hasContractScope && hasBusinessScope
        ? "MIXED"
        : (hasBusinessScope ? "BUSINESS_EXECUTION" : "CONTRACT_FIXTURE");
    result.final_state = result.verification_scope == "MIXED"
        ? "contract=" + contractState.label + ";business=" + businessState.label
        : (hasBusinessScope ? businessState.label : contractState.label);
    if (failed)
    {
        result.final_status = "FAIL";
        result.final_code = result.steps.empty()
            ? "WORKFLOW_CASE_FAILED"
            : result.steps.back().code;
        result.final_reason = result.steps.empty()
            ? "workflow case failed before executing a step"
            : result.steps.back().reason;
    }
    else if (pending || contractState.blocking_risk || businessState.blocking_risk)
    {
        result.final_status = "BUSINESS_WORKFLOW_ACCEPTED_WITH_PENDING";
        result.final_code =
            (contractState.blocking_risk || businessState.blocking_risk)
            ? "BLOCKING_RISK_PENDING"
            : "EXPECTED_CAPABILITY_PENDING";
        result.final_reason =
            (contractState.blocking_risk || businessState.blocking_risk)
            ? "workflow case was processed with a risk that blocks release"
            : "workflow case was processed with one or more explicit pending capabilities";
    }
    else
    {
        const bool businessExecutionObserved =
            result.provider_execution_step_pass_count > 0U && !hasContractScope;
        result.final_status = businessExecutionObserved
            ? "PASS"
            : "CONTRACT_FIXTURE_PASS";
        result.final_code = businessExecutionObserved
            ? "BUSINESS_EXECUTION_PASS"
            : "CONTRACT_FIXTURE_VALIDATED";
        result.final_reason = businessExecutionObserved
            ? "all declared workflow transitions and provider observations were satisfied"
            : "contract fixtures and scoped state transitions passed; no business provider execution is claimed";
    }

    if (result.expected_final_status != result.final_status)
    {
        const std::string observed = result.final_status;
        result.final_status = "FAIL";
        result.final_code = "EXPECTED_FINAL_STATUS_MISMATCH";
        result.final_reason = "expected final status " + result.expected_final_status +
            " but observed " + observed;
        result.risks.push_back(MakeGeneratedRisk(
            "EXPECTED_FINAL_STATUS_MISMATCH", "ERROR", "BLOCK_RELEASE",
            "case_finalization", result.final_reason, true));
    }
    result.completed_at = UtcTimestamp();
    return result;
}

std::string JsonEscape(const std::string& value)
{
    static const char hex[] = "0123456789abcdef";
    std::string escaped;
    escaped.reserve(value.size() + 16U);
    for (const unsigned char ch : value)
    {
        switch (ch)
        {
        case '"': escaped += "\\\""; break;
        case '\\': escaped += "\\\\"; break;
        case '\b': escaped += "\\b"; break;
        case '\f': escaped += "\\f"; break;
        case '\n': escaped += "\\n"; break;
        case '\r': escaped += "\\r"; break;
        case '\t': escaped += "\\t"; break;
        default:
            if (ch < 0x20U)
            {
                escaped += "\\u00";
                escaped.push_back(hex[(ch >> 4U) & 0x0FU]);
                escaped.push_back(hex[ch & 0x0FU]);
            }
            else
            {
                escaped.push_back(static_cast<char>(ch));
            }
            break;
        }
    }
    return escaped;
}

std::string HtmlEscape(const std::string& value)
{
    std::string escaped;
    for (const char ch : value)
    {
        switch (ch)
        {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break;
        case '"': escaped += "&quot;"; break;
        case '\'': escaped += "&#39;"; break;
        default: escaped += ch; break;
        }
    }
    return escaped;
}

std::string MarkdownCell(std::string value)
{
    for (char& ch : value)
    {
        if (ch == '\n' || ch == '\r')
            ch = ' ';
    }
    std::string escaped;
    for (const char ch : value)
    {
        if (ch == '|')
            escaped += "\\|";
        else
            escaped += ch;
    }
    return escaped;
}

std::string UtcTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    std::ostringstream text;
    text << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return text.str();
}

bool EnsureOutputDirectory(
    const fs::path& requested,
    fs::path& canonical,
    std::string& reason)
{
    if (requested.empty())
    {
        reason = "output directory must be provided explicitly";
        return false;
    }
    std::error_code error;
    const fs::path absolute = fs::absolute(requested, error).lexically_normal();
    if (error)
    {
        reason = "output directory cannot be resolved: " + error.message();
        return false;
    }

    const bool alreadyExists = fs::exists(absolute, error);
    if (error)
    {
        reason = "output directory cannot be inspected: " + error.message();
        return false;
    }
    if (alreadyExists)
    {
        reason =
            "output directory already exists; choose a new immutable run directory";
        return false;
    }

    if (!HasNormalizedPathComponent(absolute, "cxscript_runs") ||
        HasNormalizedPathComponent(absolute, "cxvision_repo"))
    {
        reason =
            "output directory must be external to cxvision_repo and under cxscript_runs";
        return false;
    }

    fs::path current;
    const fs::path parent = absolute.parent_path();
    fs::create_directories(parent, error);
    if (error)
    {
        reason = "output parent directory cannot be created: " + error.message();
        return false;
    }
    for (const fs::path& part : parent)
    {
        current /= part;
        const fs::file_status status = fs::symlink_status(current, error);
        if (error)
        {
            error.clear();
            continue;
        }
        if (fs::is_symlink(status))
        {
            reason = "output directory contains a symbolic-link component";
            return false;
        }
    }
    const bool claimed = fs::create_directory(absolute, error);
    if (error || !claimed)
    {
        reason = error
            ? "output directory cannot be claimed atomically: " + error.message()
            : "output directory was claimed by another run";
        return false;
    }
    canonical = fs::canonical(absolute, error);
    if (error)
    {
        reason = "output directory cannot be canonicalized: " + error.message();
        return false;
    }
    return true;
}

bool ReplaceWithTemporaryFile(
    const fs::path& temporary,
    const fs::path& destination,
    std::string& reason)
{
#ifdef _WIN32
    if (::MoveFileExW(
            temporary.c_str(), destination.c_str(),
            MOVEFILE_WRITE_THROUGH) == 0)
    {
        const DWORD code = ::GetLastError();
        reason = "atomic replace failed: " +
            std::system_category().message(static_cast<int>(code));
        return false;
    }
#else
    if (std::rename(temporary.c_str(), destination.c_str()) != 0)
    {
        reason = "atomic replace failed: " +
            std::string(std::strerror(errno));
        return false;
    }
#endif
    return true;
}

bool AtomicWriteText(
    const fs::path& destination,
    const std::string& content,
    std::string& reason)
{
    reason.clear();
    std::error_code error;
    fs::create_directories(destination.parent_path(), error);
    if (error)
    {
        reason = "cannot create evidence directory: " + error.message();
        return false;
    }
    const fs::file_status destinationStatus =
        fs::symlink_status(destination, error);
    if (!error && fs::is_symlink(destinationStatus))
    {
        reason = "refusing to replace a symbolic-link evidence path";
        return false;
    }
    error.clear();
    const bool destinationExists = fs::exists(destination, error);
    if (error || destinationExists)
    {
        reason = error
            ? "cannot inspect evidence destination: " + error.message()
            : "refusing to overwrite an existing evidence artifact";
        return false;
    }
    error.clear();

    static std::atomic<std::uint64_t> sequence{0U};
    const std::uint64_t tick = static_cast<std::uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    const fs::path temporary = destination.parent_path() /
         (".tmp." + HexHash(tick) + "." +
          std::to_string(sequence.fetch_add(1U, std::memory_order_relaxed) + 1U));
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output.is_open())
        {
            reason = "cannot open temporary evidence file";
            return false;
        }
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        output.flush();
        if (!output.good())
        {
            output.close();
            fs::remove(temporary, error);
            reason = "temporary evidence write failed";
            return false;
        }
        output.close();
        if (output.fail())
        {
            fs::remove(temporary, error);
            reason = "temporary evidence close failed";
            return false;
        }
    }
    if (!ReplaceWithTemporaryFile(temporary, destination, reason))
    {
        fs::remove(temporary, error);
        return false;
    }
    return true;
}

void WriteRiskJson(
    std::ostream& output,
    const CxBusinessRiskRecord& risk,
    const std::string& indent)
{
    output << indent << "{\"code\":\"" << JsonEscape(risk.code)
           << "\",\"severity\":\"" << JsonEscape(risk.severity)
           << "\",\"disposition\":\"" << JsonEscape(risk.disposition)
           << "\",\"stage\":\"" << JsonEscape(risk.stage)
           << "\",\"reason\":\"" << JsonEscape(risk.reason)
           << "\",\"blocking\":" << (risk.blocking ? "true" : "false")
           << '}';
}

void WriteDetectionElementsJson(
    std::ostream& output,
    const std::vector<CxBusinessWorkflowDetectionElement>& elements,
    const std::string& indent)
{
    output << '[';
    if (!elements.empty())
        output << '\n';
    for (std::size_t index = 0; index < elements.size(); ++index)
    {
        const CxBusinessWorkflowDetectionElement& element = elements[index];
        output << indent << "  {\"element_id\":\""
               << JsonEscape(element.element_id)
               << "\",\"element_type\":\"" << JsonEscape(element.element_type)
               << "\",\"name\":\"" << JsonEscape(element.name)
               << "\",\"status\":\"" << JsonEscape(element.status)
               << "\",\"score\":" << std::setprecision(17) << element.score
               << ",\"source\":\"" << JsonEscape(element.source)
               << "\",\"coordinate_space\":\""
               << JsonEscape(element.coordinate_space)
               << "\",\"geometry\":\"" << JsonEscape(element.geometry)
               << "\",\"evidence_ref\":\""
               << JsonEscape(element.evidence_ref) << "\"}"
               << (index + 1U == elements.size() ? "\n" : ",\n");
    }
    output << indent << ']';
}

void WriteObservationJson(
    std::ostream& output,
    const CxBusinessObservationRecord& observation,
    const std::string& indent)
{
    output << indent << "{\n"
           << indent << "  \"execution_status\":\""
           << JsonEscape(observation.execution_status) << "\",\n"
           << indent << "  \"detection_status\":\""
           << JsonEscape(observation.detection_status) << "\",\n"
           << indent << "  \"display_status\":\""
           << JsonEscape(observation.display_status) << "\",\n"
           << indent << "  \"evaluation_status\":\""
           << JsonEscape(observation.evaluation_status) << "\",\n"
           << indent << "  \"reason\":\""
           << JsonEscape(observation.reason) << "\",\n"
           << indent << "  \"detection_elements\":";
    WriteDetectionElementsJson(
        output, observation.detection_elements, indent + "  ");
    output << "\n" << indent << '}';
}

std::string SerializeCaseResult(const CxBusinessWorkflowCaseResult& result)
{
    std::ostringstream output;
    output << "{\n"
           << "  \"schema_version\":\"cxvision.business_workflow_case_result.v1\",\n"
           << "  \"run_id\":\"" << JsonEscape(result.run_id) << "\",\n"
           << "  \"started_at\":\"" << JsonEscape(result.started_at) << "\",\n"
           << "  \"completed_at\":\"" << JsonEscape(result.completed_at) << "\",\n"
           << "  \"internal_case_id\":\"" << JsonEscape(result.internal_case_id) << "\",\n"
           << "  \"case_id\":\"" << JsonEscape(result.case_id) << "\",\n"
           << "  \"display_name\":\"" << JsonEscape(result.display_name) << "\",\n"
           << "  \"description\":\"" << JsonEscape(result.description) << "\",\n"
           << "  \"identity_key\":\"" << JsonEscape(result.identity_key) << "\",\n"
           << "  \"manifest_path\":\"" << JsonEscape(result.manifest_path.generic_string()) << "\",\n"
           << "  \"case_directory\":\"" << JsonEscape(result.case_directory.generic_string()) << "\",\n"
           << "  \"normalized_path\":\"" << JsonEscape(result.normalized_path.generic_string()) << "\",\n"
           << "  \"expected_final_status\":\"" << JsonEscape(result.expected_final_status) << "\",\n"
           << "  \"final_status\":\"" << JsonEscape(result.final_status) << "\",\n"
           << "  \"final_code\":\"" << JsonEscape(result.final_code) << "\",\n"
           << "  \"final_reason\":\"" << JsonEscape(result.final_reason) << "\",\n"
           << "  \"final_state\":\"" << JsonEscape(result.final_state) << "\",\n"
           << "  \"verification_scope\":\"" << JsonEscape(result.verification_scope) << "\",\n"
           << "  \"contract_fixture_step_pass_count\":"
           << result.contract_fixture_step_pass_count << ",\n"
           << "  \"provider_execution_step_pass_count\":"
           << result.provider_execution_step_pass_count << ",\n"
           << "  \"pending_step_count\":" << result.pending_step_count << ",\n"
           << "  \"required_assets\":[";
    for (std::size_t index = 0; index < result.required_assets.size(); ++index)
    {
        if (index > 0U) output << ',';
        output << "\"" << JsonEscape(result.required_assets[index].generic_string()) << "\"";
    }
    output << "],\n  \"asset_preflight\":[";
    if (!result.asset_preflight.empty()) output << '\n';
    for (std::size_t index = 0; index < result.asset_preflight.size(); ++index)
    {
        const CxBusinessWorkflowAssetPreflightRecord& record =
            result.asset_preflight[index];
        output << "    {\"path\":\"" << JsonEscape(record.path.generic_string())
               << "\",\"kind\":\"" << JsonEscape(record.kind)
               << "\",\"status\":\"" << JsonEscape(record.status)
               << "\",\"code\":\"" << JsonEscape(record.code)
               << "\",\"reason\":\"" << JsonEscape(record.reason)
               << "\",\"byte_size\":" << record.byte_size
               << ",\"width\":" << record.width
               << ",\"height\":" << record.height
               << ",\"channels\":" << record.channels
               << ",\"content_hash\":\"" << JsonEscape(record.content_hash)
               << "\"}" << (index + 1U == result.asset_preflight.size() ? "\n" : ",\n");
    }
    output << "  ],\n  \"image_refs\":[";
    if (!result.image_refs.empty()) output << '\n';
    for (std::size_t index = 0; index < result.image_refs.size(); ++index)
    {
        const CxBusinessWorkflowImageRef& ref = result.image_refs[index];
        output << "    {\"image_id\":\"" << JsonEscape(ref.image_id)
               << "\",\"image_revision\":\"" << JsonEscape(ref.image_revision)
               << "\",\"source_type\":\"" << JsonEscape(ref.source_type)
               << "\",\"uri\":\"" << JsonEscape(ref.uri)
               << "\",\"coordinate_space\":\""
               << JsonEscape(ref.coordinate_space)
               << "\",\"content_hash\":\"" << JsonEscape(ref.content_hash)
               << "\",\"width\":" << ref.width
               << ",\"height\":" << ref.height
               << ",\"channels\":" << ref.channels << '}'
               << (index + 1U == result.image_refs.size() ? "\n" : ",\n");
    }
    output << "  ],\n  \"frame_refs\":[";
    if (!result.frame_refs.empty()) output << '\n';
    for (std::size_t index = 0; index < result.frame_refs.size(); ++index)
    {
        const CxBusinessWorkflowFrameRef& ref = result.frame_refs[index];
        output << "    {\"frame_id\":\"" << JsonEscape(ref.frame_id)
               << "\",\"image_id\":\"" << JsonEscape(ref.image_id)
               << "\",\"source_type\":\"" << JsonEscape(ref.source_type)
               << "\",\"uri\":\"" << JsonEscape(ref.uri)
               << "\",\"captured_at\":\"" << JsonEscape(ref.captured_at)
               << "\",\"sequence\":" << ref.sequence << '}'
               << (index + 1U == result.frame_refs.size() ? "\n" : ",\n");
    }
    output << "  ],\n  \"rois\":[";
    if (!result.rois.empty()) output << '\n';
    for (std::size_t index = 0; index < result.rois.size(); ++index)
    {
        const CxBusinessWorkflowRoi& roi = result.rois[index];
        output << "    {\"roi_id\":\"" << JsonEscape(roi.roi_id)
               << "\",\"source_image_id\":\"" << JsonEscape(roi.source_image_id)
               << "\",\"image_revision\":\"" << JsonEscape(roi.image_revision)
               << "\",\"recipe_revision\":\"" << JsonEscape(roi.recipe_revision)
               << "\",\"coordinate_space\":\""
               << JsonEscape(roi.coordinate_space)
               << "\",\"geometry\":\"" << JsonEscape(roi.geometry)
               << "\",\"transform_ref\":\"" << JsonEscape(roi.transform_ref)
               << "\"}" << (index + 1U == result.rois.size() ? "\n" : ",\n");
    }
    output << "  ],\n  \"annotations\":[";
    if (!result.annotations.empty()) output << '\n';
    for (std::size_t index = 0; index < result.annotations.size(); ++index)
    {
        const CxBusinessWorkflowAnnotation& annotation = result.annotations[index];
        output << "    {\"object_id\":\"" << JsonEscape(annotation.object_id)
               << "\",\"object_type\":\"" << JsonEscape(annotation.object_type)
               << "\",\"coordinate_space\":\""
               << JsonEscape(annotation.coordinate_space)
               << "\",\"source_image_id\":\""
               << JsonEscape(annotation.source_image_id)
               << "\",\"image_revision\":\"" << JsonEscape(annotation.image_revision)
               << "\",\"roi_id\":\"" << JsonEscape(annotation.roi_id)
               << "\",\"recipe_revision\":\""
               << JsonEscape(annotation.recipe_revision)
               << "\",\"geometry\":\"" << JsonEscape(annotation.geometry)
               << "\",\"creation_method\":\""
               << JsonEscape(annotation.creation_method)
               << "\",\"creator\":\"" << JsonEscape(annotation.creator)
               << "\",\"confidence\":" << std::setprecision(17)
               << annotation.confidence
               << ",\"transform_ref\":\"" << JsonEscape(annotation.transform_ref)
               << "\",\"status\":\"" << JsonEscape(annotation.status)
               << "\",\"parent_ref\":\"" << JsonEscape(annotation.parent_ref)
               << "\",\"evidence_ref\":\"" << JsonEscape(annotation.evidence_ref)
               << "\"}" << (index + 1U == result.annotations.size() ? "\n" : ",\n");
    }
    output << "  ],\n  \"steps\":[";
    if (!result.steps.empty()) output << '\n';
    for (std::size_t index = 0; index < result.steps.size(); ++index)
    {
        const CxBusinessWorkflowStepResult& step = result.steps[index];
        output << "    {\n"
               << "      \"index\":" << step.index << ",\n"
               << "      \"action\":\"" << JsonEscape(step.action) << "\",\n"
               << "      \"provider\":\"" << JsonEscape(step.provider) << "\",\n"
               << "      \"input_ref\":\"" << JsonEscape(step.input_ref) << "\",\n"
               << "      \"output_ref\":\"" << JsonEscape(step.output_ref) << "\",\n"
               << "      \"idempotency_key\":\"" << JsonEscape(step.idempotency_key) << "\",\n"
               << "      \"declared_capability_status\":\""
               << JsonEscape(step.declared_capability_status) << "\",\n"
               << "      \"capability_status\":\"" << JsonEscape(step.capability_status) << "\",\n"
               << "      \"expected_status\":\"" << JsonEscape(step.expected_status) << "\",\n"
               << "      \"status\":\"" << JsonEscape(step.status) << "\",\n"
               << "      \"code\":\"" << JsonEscape(step.code) << "\",\n"
               << "      \"reason\":\"" << JsonEscape(step.reason) << "\",\n"
               << "      \"state_before\":\"" << JsonEscape(step.state_before) << "\",\n"
               << "      \"state_after\":\"" << JsonEscape(step.state_after) << "\",\n"
               << "      \"recorded_at\":\"" << JsonEscape(step.recorded_at) << "\",\n"
               << "      \"verification_scope\":\""
               << JsonEscape(step.verification_scope) << "\",\n"
               << "      \"contract_validation_status\":\""
               << JsonEscape(step.contract_validation_status) << "\",\n"
               << "      \"state_changed\":" << (step.state_changed ? "true" : "false") << ",\n"
               << "      \"deduplicated\":" << (step.deduplicated ? "true" : "false") << ",\n"
               << "      \"expected_rejection\":" << (step.expected_rejection ? "true" : "false") << ",\n"
               << "      \"requires_provider\":" << (step.requires_provider ? "true" : "false") << ",\n"
               << "      \"provider_executed\":" << (step.provider_executed ? "true" : "false") << ",\n"
               << "      \"contract_only\":" << (step.contract_only ? "true" : "false") << ",\n"
               << "      \"evidence_ref\":\"" << JsonEscape(step.evidence_ref) << "\",\n"
               << "      \"human_decision\":{\"decision\":\""
               << JsonEscape(step.human_decision.decision)
               << "\",\"operator_id\":\"" << JsonEscape(step.human_decision.operator_id)
               << "\",\"occurred_at\":\"" << JsonEscape(step.human_decision.occurred_at)
               << "\",\"input_version\":\"" << JsonEscape(step.human_decision.input_version)
               << "\",\"output_version\":\"" << JsonEscape(step.human_decision.output_version)
               << "\",\"event_id\":\"" << JsonEscape(step.human_decision.event_id)
               << "\"},\n      \"contract_fixture_elements\":";
        WriteDetectionElementsJson(
            output, step.contract_fixture_elements, "      ");
        output << ",\n      \"observation\":";
        WriteObservationJson(output, step.observation, "      ");
        output << ",\n      \"risks\":[";
        for (std::size_t riskIndex = 0; riskIndex < step.risks.size(); ++riskIndex)
        {
            if (riskIndex > 0U) output << ',';
            WriteRiskJson(output, step.risks[riskIndex], "");
        }
        output << "]\n    }" << (index + 1U == result.steps.size() ? "\n" : ",\n");
    }
    output << "  ],\n  \"risks\":[";
    for (std::size_t index = 0; index < result.risks.size(); ++index)
    {
        if (index > 0U) output << ',';
        WriteRiskJson(output, result.risks[index], "");
    }
    output << "]\n}\n";
    return output.str();
}

std::string SerializeScanDebug(const CxBusinessWorkflowBatchResult& result)
{
    std::ostringstream output;
    output << "{\n  \"schema_version\":\"cxvision.business_workflow_scan.v1\",\n"
           << "  \"run_id\":\"" << JsonEscape(result.run_id) << "\",\n"
           << "  \"root\":\"" << JsonEscape(result.case_root.generic_string()) << "\",\n"
           << "  \"discovered_count\":" << result.discovered_count << ",\n"
           << "  \"accepted_count\":" << result.accepted_count << ",\n"
           << "  \"rejected_count\":" << result.rejected_count << ",\n"
           << "  \"skipped_count\":" << result.skipped_count << ",\n"
           << "  \"records\":[";
    if (!result.scan_records.empty()) output << '\n';
    for (std::size_t index = 0; index < result.scan_records.size(); ++index)
    {
        const CxBusinessWorkflowScanRecord& record = result.scan_records[index];
        output << "    {\"path\":\"" << JsonEscape(record.path.generic_string())
               << "\",\"normalized_path\":\"" << JsonEscape(record.normalized_path.generic_string())
               << "\",\"internal_case_id\":\"" << JsonEscape(record.internal_case_id)
               << "\",\"case_id\":\"" << JsonEscape(record.case_id)
               << "\",\"outcome\":\"" << JsonEscape(record.outcome)
               << "\",\"code\":\"" << JsonEscape(record.code)
               << "\",\"reason\":\"" << JsonEscape(record.reason) << "\"}"
               << (index + 1U == result.scan_records.size() ? "\n" : ",\n");
    }
    output << "  ]\n}\n";
    return output.str();
}

std::string SerializeRiskRegister(const CxBusinessWorkflowBatchResult& result)
{
    std::ostringstream output;
    output << "{\n  \"schema_version\":\"cxvision.business_workflow_risks.v1\",\n"
           << "  \"run_id\":\"" << JsonEscape(result.run_id) << "\",\n"
           << "  \"risk_count\":" << result.risks.size() << ",\n"
           << "  \"risks\":[";
    if (!result.risks.empty()) output << '\n';
    for (std::size_t index = 0; index < result.risks.size(); ++index)
    {
        WriteRiskJson(output, result.risks[index], "    ");
        output << (index + 1U == result.risks.size() ? "\n" : ",\n");
    }
    output << "  ]\n}\n";
    return output.str();
}

std::string SerializeSummary(const CxBusinessWorkflowBatchResult& result)
{
    std::ostringstream output;
    output << "{\n  \"schema_version\":\"cxvision.business_workflow_summary.v1\",\n"
           << "  \"run_id\":\"" << JsonEscape(result.run_id) << "\",\n"
           << "  \"started_at\":\"" << JsonEscape(result.started_at) << "\",\n"
           << "  \"completed_at\":\"" << JsonEscape(result.completed_at) << "\",\n"
           << "  \"case_root\":\"" << JsonEscape(result.case_root.generic_string()) << "\",\n"
           << "  \"output_dir\":\"" << JsonEscape(result.out_dir.generic_string()) << "\",\n"
           << "  \"claim_scope\":\"WORKFLOW_CONTRACT_AND_BOUND_PROVIDER_FACTS\",\n"
           << "  \"discovered_count\":" << result.discovered_count << ",\n"
           << "  \"accepted_count\":" << result.accepted_count << ",\n"
           << "  \"rejected_count\":" << result.rejected_count << ",\n"
           << "  \"skipped_count\":" << result.skipped_count << ",\n"
           << "  \"processed_count\":" << result.processed_count << ",\n"
           << "  \"pass_count\":" << result.pass_count << ",\n"
           << "  \"contract_fixture_pass_count\":"
           << result.contract_fixture_pass_count << ",\n"
           << "  \"provider_execution_step_pass_count\":"
           << result.provider_execution_step_pass_count << ",\n"
           << "  \"pending_count\":" << result.pending_count << ",\n"
           << "  \"fail_count\":" << result.fail_count << ",\n"
           << "  \"final_status\":\"" << JsonEscape(result.final_status) << "\",\n"
           << "  \"final_code\":\"" << JsonEscape(result.final_code) << "\",\n"
           << "  \"final_reason\":\"" << JsonEscape(result.final_reason) << "\",\n"
           << "  \"cases\":[";
    if (!result.case_results.empty()) output << '\n';
    for (std::size_t index = 0; index < result.case_results.size(); ++index)
    {
        const CxBusinessWorkflowCaseResult& item = result.case_results[index];
        output << "    {\"internal_case_id\":\"" << JsonEscape(item.internal_case_id)
               << "\",\"case_id\":\"" << JsonEscape(item.case_id)
               << "\",\"display_name\":\"" << JsonEscape(item.display_name)
               << "\",\"normalized_path\":\"" << JsonEscape(item.normalized_path.generic_string())
                << "\",\"status\":\"" << JsonEscape(item.final_status)
                << "\",\"code\":\"" << JsonEscape(item.final_code)
                 << "\",\"verification_scope\":\""
                 << JsonEscape(item.verification_scope)
                 << "\",\"started_at\":\"" << JsonEscape(item.started_at)
                 << "\",\"completed_at\":\"" << JsonEscape(item.completed_at)
                << "\",\"contract_fixture_step_pass_count\":"
                << item.contract_fixture_step_pass_count
                << ",\"provider_execution_step_pass_count\":"
                << item.provider_execution_step_pass_count
                << ",\"pending_step_count\":" << item.pending_step_count
                << ",\"case_result_ref\":\"" << JsonEscape(item.case_result_path.generic_string())
               << "\"}" << (index + 1U == result.case_results.size() ? "\n" : ",\n");
    }
    output << "  ],\n  \"output_errors\":[";
    for (std::size_t index = 0; index < result.output_errors.size(); ++index)
    {
        if (index > 0U) output << ',';
        output << "\"" << JsonEscape(result.output_errors[index]) << "\"";
    }
    output << "]\n}\n";
    return output.str();
}

std::string SerializeMarkdownReport(const CxBusinessWorkflowBatchResult& result)
{
    std::ostringstream output;
    output << "# Business Workflow Acceptance\n\n"
           << "- Run ID: `" << result.run_id << "`\n"
           << "- Started: `" << result.started_at << "`\n"
           << "- Completed: `" << result.completed_at << "`\n"
           << "- Case root: `" << result.case_root.generic_string() << "`\n"
           << "- Output: `" << result.out_dir.generic_string() << "`\n"
           << "- Conclusion: `" << result.final_status << "`\n"
           << "- Code: `" << result.final_code << "`\n"
           << "- Reason: " << result.final_reason << "\n\n"
           << "| Discovered | Accepted | Rejected | Skipped | Processed | Business pass | Contract fixture pass | Pending | Fail |\n"
           << "|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n| "
           << result.discovered_count << " | " << result.accepted_count << " | "
           << result.rejected_count << " | " << result.skipped_count << " | "
           << result.processed_count << " | " << result.pass_count << " | "
           << result.contract_fixture_pass_count << " | "
           << result.pending_count << " | " << result.fail_count << " |\n\n"
           << "## Cases\n\n"
           << "| Review item | Case ID | Scope | Status | Contract steps | Provider steps | Pending steps | Final state | Result |\n"
           << "|---|---|---|---|---:|---:|---:|---|---|\n";
    for (const CxBusinessWorkflowCaseResult& item : result.case_results)
    {
        output << "| " << MarkdownCell(item.display_name)
                << " | " << MarkdownCell(item.case_id)
                << " | " << MarkdownCell(item.verification_scope)
                << " | " << MarkdownCell(item.final_status)
                << " | " << item.contract_fixture_step_pass_count
                << " | " << item.provider_execution_step_pass_count
                << " | " << item.pending_step_count
                << " | " << MarkdownCell(item.final_state)
               << " | `" << item.case_result_path.generic_string() << "` |\n";
    }
    output << "\n## Risks\n\n"
           << "| Code | Severity | Disposition | Stage | Blocking | Reason |\n"
           << "|---|---|---|---|---|---|\n";
    for (const CxBusinessRiskRecord& risk : result.risks)
    {
        output << "| " << MarkdownCell(risk.code)
               << " | " << MarkdownCell(risk.severity)
               << " | " << MarkdownCell(risk.disposition)
               << " | " << MarkdownCell(risk.stage)
               << " | " << (risk.blocking ? "true" : "false")
               << " | " << MarkdownCell(risk.reason) << " |\n";
    }
    if (!result.output_errors.empty())
    {
        output << "\n## Evidence write errors\n\n";
        for (const std::string& error : result.output_errors)
            output << "- " << error << '\n';
    }
    return output.str();
}

std::string SerializeHtmlReport(const CxBusinessWorkflowBatchResult& result)
{
    std::ostringstream output;
    output << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
           << "<title>Business Workflow Acceptance</title>"
           << "<style>body{font-family:system-ui,sans-serif;margin:2rem;color:#18212b}"
           << "table{border-collapse:collapse;width:100%;margin:1rem 0}"
           << "th,td{border:1px solid #ccd4dc;padding:.45rem;text-align:left}"
           << "th{background:#eef2f5}code{word-break:break-all}</style></head><body>"
           << "<h1>Business Workflow Acceptance</h1>"
           << "<p><strong>Run ID:</strong> <code>" << HtmlEscape(result.run_id)
           << "</code><br><strong>Started:</strong> "
           << HtmlEscape(result.started_at)
           << "<br><strong>Completed:</strong> "
           << HtmlEscape(result.completed_at)
           << "<br><strong>Conclusion:</strong> " << HtmlEscape(result.final_status)
           << "<br><strong>Code:</strong> " << HtmlEscape(result.final_code)
           << "<br><strong>Reason:</strong> " << HtmlEscape(result.final_reason)
           << "<br><strong>Business execution pass:</strong> " << result.pass_count
           << "<br><strong>Contract fixture pass:</strong> "
           << result.contract_fixture_pass_count
           << "<br><strong>Pending:</strong> " << result.pending_count << "</p>"
           << "<h2>Cases</h2><table><thead><tr><th>Review item</th><th>Case ID</th>"
           << "<th>Scope</th><th>Status</th><th>Code</th><th>Final state</th><th>Result</th>"
           << "</tr></thead><tbody>";
    for (const CxBusinessWorkflowCaseResult& item : result.case_results)
    {
        output << "<tr><td>" << HtmlEscape(item.display_name)
                << "</td><td>" << HtmlEscape(item.case_id)
                << "</td><td>" << HtmlEscape(item.verification_scope)
                << "</td><td>" << HtmlEscape(item.final_status)
               << "</td><td>" << HtmlEscape(item.final_code)
               << "</td><td>" << HtmlEscape(item.final_state)
               << "</td><td><code>" << HtmlEscape(item.case_result_path.generic_string())
               << "</code></td></tr>";
    }
    output << "</tbody></table><h2>Step evidence</h2><table><thead><tr>"
           << "<th>Case</th><th>#</th><th>Action</th><th>Scope</th><th>Status</th>"
           << "<th>Provider executed</th><th>Execution</th><th>Detection</th>"
           << "<th>Display</th><th>Evaluation</th><th>Observed elements</th>"
           << "<th>Fixture elements</th><th>State</th><th>Reason</th>"
           << "</tr></thead><tbody>";
    for (const CxBusinessWorkflowCaseResult& item : result.case_results)
    {
        for (const CxBusinessWorkflowStepResult& step : item.steps)
        {
            output << "<tr><td>" << HtmlEscape(item.display_name)
                   << "</td><td>" << step.index
                   << "</td><td>" << HtmlEscape(step.action)
                   << "</td><td>" << HtmlEscape(step.verification_scope)
                   << "</td><td>" << HtmlEscape(step.status)
                   << "</td><td>" << (step.provider_executed ? "true" : "false")
                   << "</td><td>" << HtmlEscape(step.observation.execution_status)
                   << "</td><td>" << HtmlEscape(step.observation.detection_status)
                   << "</td><td>" << HtmlEscape(step.observation.display_status)
                   << "</td><td>" << HtmlEscape(step.observation.evaluation_status)
                   << "</td><td>" << step.observation.detection_elements.size()
                   << "</td><td>" << step.contract_fixture_elements.size()
                   << "</td><td>" << HtmlEscape(step.state_before) << " &rarr; "
                   << HtmlEscape(step.state_after)
                   << "</td><td>" << HtmlEscape(step.reason) << "</td></tr>";
        }
    }
    output << "</tbody></table><h2>Risks</h2><table><thead><tr><th>Code</th>"
           << "<th>Severity</th><th>Disposition</th><th>Stage</th><th>Blocking</th>"
           << "<th>Reason</th></tr></thead><tbody>";
    for (const CxBusinessRiskRecord& risk : result.risks)
    {
        output << "<tr><td>" << HtmlEscape(risk.code)
               << "</td><td>" << HtmlEscape(risk.severity)
               << "</td><td>" << HtmlEscape(risk.disposition)
               << "</td><td>" << HtmlEscape(risk.stage)
               << "</td><td>" << (risk.blocking ? "true" : "false")
               << "</td><td>" << HtmlEscape(risk.reason) << "</td></tr>";
    }
    output << "</tbody></table></body></html>\n";
    return output.str();
}

std::string SerializeAuditJsonl(const CxBusinessWorkflowBatchResult& result)
{
    std::ostringstream output;
    std::size_t eventIndex = 0;
    const std::string serializedAt = UtcTimestamp();
    const std::string startedAt = result.started_at.empty()
        ? serializedAt
        : result.started_at;
    const std::string completedAt = result.completed_at.empty()
        ? serializedAt
        : result.completed_at;
    const auto eventPrefix = [&](const std::string& type, const std::string& timestamp)
    {
        output << "{\"event_index\":" << ++eventIndex
               << ",\"timestamp\":\"" << JsonEscape(timestamp)
               << "\",\"event\":\"" << type
               << "\",\"run_id\":\"" << JsonEscape(result.run_id) << '"';
    };
    eventPrefix("run_start", startedAt);
    output << ",\"case_root\":\"" << JsonEscape(result.case_root.generic_string()) << "\"}\n";
    for (const CxBusinessWorkflowScanRecord& record : result.scan_records)
    {
        eventPrefix("scan_record", startedAt);
        output << ",\"path\":\"" << JsonEscape(record.path.generic_string())
               << "\",\"normalized_path\":\""
               << JsonEscape(record.normalized_path.generic_string())
               << "\",\"internal_case_id\":\""
               << JsonEscape(record.internal_case_id)
               << "\",\"case_id\":\"" << JsonEscape(record.case_id)
               << "\",\"outcome\":\"" << JsonEscape(record.outcome)
               << "\",\"code\":\"" << JsonEscape(record.code)
               << "\",\"reason\":\"" << JsonEscape(record.reason) << "\"}\n";
    }
    for (const CxBusinessWorkflowCaseResult& item : result.case_results)
    {
        eventPrefix(
            "case_begin", item.started_at.empty() ? startedAt : item.started_at);
        output << ",\"internal_case_id\":\"" << JsonEscape(item.internal_case_id)
               << "\",\"case_id\":\"" << JsonEscape(item.case_id)
               << "\",\"manifest_path\":\"" << JsonEscape(item.manifest_path.generic_string())
               << "\",\"verification_scope\":\""
               << JsonEscape(item.verification_scope)
               << "\"}\n";
        for (const CxBusinessWorkflowStepResult& step : item.steps)
        {
            eventPrefix(
                "step_end", step.recorded_at.empty() ? completedAt : step.recorded_at);
            output << ",\"internal_case_id\":\"" << JsonEscape(item.internal_case_id)
                   << "\",\"case_id\":\"" << JsonEscape(item.case_id)
                   << "\",\"step_index\":" << step.index
                   << ",\"action\":\"" << JsonEscape(step.action)
                   << "\",\"provider\":\"" << JsonEscape(step.provider)
                   << "\",\"input_ref\":\"" << JsonEscape(step.input_ref)
                   << "\",\"output_ref\":\"" << JsonEscape(step.output_ref)
                   << "\",\"idempotency_key\":\""
                   << JsonEscape(step.idempotency_key)
                   << "\",\"state_before\":\"" << JsonEscape(step.state_before)
                   << "\",\"state_after\":\"" << JsonEscape(step.state_after)
                   << "\",\"verification_scope\":\""
                   << JsonEscape(step.verification_scope)
                   << "\",\"contract_validation_status\":\""
                   << JsonEscape(step.contract_validation_status)
                   << "\",\"status\":\"" << JsonEscape(step.status)
                   << "\",\"code\":\"" << JsonEscape(step.code)
                   << "\",\"reason\":\"" << JsonEscape(step.reason)
                   << "\",\"execution_status\":\"" << JsonEscape(step.observation.execution_status)
                   << "\",\"detection_status\":\"" << JsonEscape(step.observation.detection_status)
                   << "\",\"display_status\":\"" << JsonEscape(step.observation.display_status)
                   << "\",\"evaluation_status\":\"" << JsonEscape(step.observation.evaluation_status)
                   << "\",\"requires_provider\":"
                   << (step.requires_provider ? "true" : "false")
                   << ",\"provider_executed\":"
                   << (step.provider_executed ? "true" : "false")
                   << ",\"contract_only\":" << (step.contract_only ? "true" : "false")
                   << ",\"deduplicated\":" << (step.deduplicated ? "true" : "false")
                   << ",\"expected_rejection\":"
                   << (step.expected_rejection ? "true" : "false")
                   << ",\"observed_detection_element_count\":"
                   << step.observation.detection_elements.size()
                   << ",\"contract_fixture_element_count\":"
                   << step.contract_fixture_elements.size()
                   << ",\"human_decision\":\""
                   << JsonEscape(step.human_decision.decision)
                   << "\",\"human_operator_id\":\""
                   << JsonEscape(step.human_decision.operator_id)
                   << "\",\"human_occurred_at\":\""
                   << JsonEscape(step.human_decision.occurred_at)
                   << "\",\"human_input_version\":\""
                   << JsonEscape(step.human_decision.input_version)
                   << "\",\"human_output_version\":\""
                   << JsonEscape(step.human_decision.output_version)
                   << "\",\"human_event_id\":\""
                    << JsonEscape(step.human_decision.event_id)
                    << "\",\"risk_count\":" << step.risks.size()
                    << "}\n";
        }
        eventPrefix(
            "case_end",
            item.completed_at.empty()
                ? completedAt
                : item.completed_at);
        output << ",\"internal_case_id\":\"" << JsonEscape(item.internal_case_id)
               << "\",\"case_id\":\"" << JsonEscape(item.case_id)
               << "\",\"status\":\"" << JsonEscape(item.final_status)
               << "\",\"code\":\"" << JsonEscape(item.final_code)
               << "\",\"verification_scope\":\""
               << JsonEscape(item.verification_scope)
               << "\",\"contract_fixture_step_pass_count\":"
               << item.contract_fixture_step_pass_count
               << ",\"provider_execution_step_pass_count\":"
               << item.provider_execution_step_pass_count
               << ",\"pending_step_count\":" << item.pending_step_count << "}\n";
    }
    eventPrefix("run_end", completedAt);
    output << ",\"status\":\"" << JsonEscape(result.final_status)
           << "\",\"code\":\"" << JsonEscape(result.final_code)
           << "\",\"processed_count\":" << result.processed_count
           << ",\"pass_count\":" << result.pass_count
           << ",\"contract_fixture_pass_count\":"
           << result.contract_fixture_pass_count
           << ",\"provider_execution_step_pass_count\":"
           << result.provider_execution_step_pass_count
           << ",\"pending_count\":" << result.pending_count
           << ",\"fail_count\":" << result.fail_count << "}\n";
    return output.str();
}

void RecordWriteFailure(
    CxBusinessWorkflowBatchResult& result,
    const fs::path& path,
    const std::string& reason)
{
    result.output_errors.push_back(path.generic_string() + ": " + reason);
}

bool WriteEvidenceArtifacts(CxBusinessWorkflowBatchResult& result)
{
    const fs::path evidenceRoot = result.summary_path.parent_path();
    const fs::path stagingRoot = result.out_dir / ".stage";
    std::error_code directoryError;
    if (!fs::create_directory(stagingRoot, directoryError))
    {
        const std::string directoryReason = directoryError
            ? directoryError.message()
            : "staging directory already exists";
        RecordWriteFailure(result, stagingRoot, directoryReason);
        result.final_status = "FAIL";
        result.final_code = "EVIDENCE_STAGE_CREATE_FAIL";
        result.final_reason =
            "the immutable evidence staging directory could not be created";
        return false;
    }

    const auto stagingPathFor = [&](const fs::path& finalPath, fs::path& staged)
    {
        std::error_code relativeError;
        const fs::path relative =
            fs::relative(finalPath, evidenceRoot, relativeError);
        if (relativeError || relative.empty() || relative.is_absolute() ||
            HasParentTraversal(relative))
        {
            return false;
        }
        staged = stagingRoot / relative;
        return true;
    };
    const auto writeStaged = [&](const fs::path& finalPath,
                                 const std::string& content)
    {
        fs::path staged;
        if (!stagingPathFor(finalPath, staged))
        {
            RecordWriteFailure(
                result, finalPath,
                "final evidence path cannot be mapped into the staging root");
            return;
        }
        std::string writeReason;
        if (!AtomicWriteText(staged, content, writeReason))
            RecordWriteFailure(result, finalPath, writeReason);
    };

    for (const CxBusinessWorkflowCaseResult& item : result.case_results)
        writeStaged(item.case_result_path, SerializeCaseResult(item));
    writeStaged(result.scan_debug_path, SerializeScanDebug(result));
    writeStaged(result.risk_register_path, SerializeRiskRegister(result));
    writeStaged(result.audit_jsonl_path, SerializeAuditJsonl(result));
    writeStaged(result.summary_path, SerializeSummary(result));
    writeStaged(result.report_md_path, SerializeMarkdownReport(result));
    writeStaged(result.report_html_path, SerializeHtmlReport(result));

    std::ostringstream commit;
    commit << "{\n"
           << "  \"schema_version\":\"cxvision.business_workflow_commit.v1\",\n"
           << "  \"run_id\":\"" << JsonEscape(result.run_id) << "\",\n"
           << "  \"started_at\":\"" << JsonEscape(result.started_at) << "\",\n"
           << "  \"completed_at\":\"" << JsonEscape(result.completed_at) << "\",\n"
           << "  \"status\":\"" << JsonEscape(result.final_status) << "\",\n"
           << "  \"artifact_count\":" << (result.case_results.size() + 6U)
           << "\n}\n";
    std::string commitReason;
    if (!AtomicWriteText(
            stagingRoot / "business_workflow_commit.json",
            commit.str(), commitReason))
    {
        RecordWriteFailure(
            result, evidenceRoot / "business_workflow_commit.json",
            commitReason);
    }

    if (!result.output_errors.empty())
    {
        result.final_status = "FAIL";
        result.final_code = "EVIDENCE_WRITE_FAIL";
        result.final_reason = "one or more required evidence artifacts could not be written atomically";
        return false;
    }

    const bool evidenceRootExists = fs::exists(evidenceRoot, directoryError);
    if (directoryError || evidenceRootExists)
    {
        RecordWriteFailure(
            result, evidenceRoot,
            directoryError
                ? "cannot inspect final evidence root: " + directoryError.message()
                : "final evidence root already exists");
        result.final_status = "FAIL";
        result.final_code = "EVIDENCE_COMMIT_FAIL";
        result.final_reason =
            "the immutable evidence package could not be committed";
        return false;
    }
    directoryError.clear();
    fs::rename(stagingRoot, evidenceRoot, directoryError);
    if (directoryError)
    {
        RecordWriteFailure(result, evidenceRoot, directoryError.message());
        result.final_status = "FAIL";
        result.final_code = "EVIDENCE_COMMIT_FAIL";
        result.final_reason =
            "the immutable evidence package could not be committed";
        return false;
    }
    return true;
}
} // namespace

bool RunCxBusinessWorkflowAcceptance(
    const CxBusinessWorkflowBatchRequest& request,
    CxBusinessWorkflowBatchResult& result,
    std::string& reason)
{
    result = CxBusinessWorkflowBatchResult{};
    result.run_id = request.run_id;
    result.started_at = UtcTimestamp();
    result.case_root = request.case_root;
    result.out_dir = request.out_dir;
    reason.clear();

    if (TrimAscii(request.run_id).empty())
    {
        reason = "run_id must be provided explicitly";
        result.final_status = "FAIL";
        result.final_code = "REQUEST_INVALID";
        result.final_reason = reason;
        return false;
    }
    if (request.case_root.empty())
    {
        reason = "case_root must be provided explicitly";
        result.final_status = "FAIL";
        result.final_code = "REQUEST_INVALID";
        result.final_reason = reason;
        return false;
    }

    fs::path canonicalOutput;
    if (!EnsureOutputDirectory(request.out_dir, canonicalOutput, reason))
    {
        result.final_status = "FAIL";
        result.final_code = "OUTPUT_ROOT_INVALID";
        result.final_reason = reason;
        return false;
    }
    result.out_dir = canonicalOutput;
    const fs::path evidenceRoot = canonicalOutput / "evidence";
    result.summary_path = evidenceRoot / "business_workflow_summary.json";
    result.report_md_path = evidenceRoot / "business_workflow_report.md";
    result.report_html_path = evidenceRoot / "business_workflow_report.html";
    result.audit_jsonl_path = evidenceRoot / "business_workflow_audit.jsonl";
    result.scan_debug_path = evidenceRoot / "business_workflow_scan_debug.json";
    result.risk_register_path = evidenceRoot / "business_workflow_risk_register.json";

    CxBusinessWorkflowBatchRequest normalizedRequest = request;
    normalizedRequest.out_dir = evidenceRoot;
    std::vector<CaseDefinition> definitions;
    std::string scanReason;
    const bool scanCompleted =
        DiscoverCases(normalizedRequest, result, definitions, scanReason);
    if (scanCompleted)
        normalizedRequest.case_root = result.case_root;

    if (scanCompleted &&
        (IsPathWithin(result.case_root, canonicalOutput) ||
         IsPathWithin(canonicalOutput, result.case_root)))
    {
        ++result.rejected_count;
        AddScanRecord(
            result, canonicalOutput, canonicalOutput, {}, {}, "REJECTED",
            "ROOTS_OVERLAP",
            "case root and output root must not contain one another");
        scanReason = "case root and output root overlap";
        definitions.clear();
    }

    for (const CaseDefinition& definition : definitions)
    {
        CxBusinessWorkflowCaseResult caseResult =
            ExecuteCase(normalizedRequest, definition);
        for (const CxBusinessRiskRecord& risk : caseResult.risks)
            result.risks.push_back(risk);
        if (caseResult.final_status == "PASS")
        {
            ++result.pass_count;
        }
        else if (caseResult.final_status == "CONTRACT_FIXTURE_PASS")
        {
            ++result.contract_fixture_pass_count;
        }
        else if (caseResult.final_status ==
                 "BUSINESS_WORKFLOW_ACCEPTED_WITH_PENDING")
            ++result.pending_count;
        else
            ++result.fail_count;
        result.provider_execution_step_pass_count +=
            caseResult.provider_execution_step_pass_count;
        ++result.processed_count;
        result.case_results.push_back(std::move(caseResult));
    }

    if (!scanCompleted || result.rejected_count > 0U || result.fail_count > 0U)
    {
        result.final_status = "FAIL";
        result.final_code = !scanCompleted
            ? "CASE_SCAN_FAILED"
            : (result.rejected_count > 0U
                ? "CASE_ASSET_REJECTED"
                : "WORKFLOW_CASE_FAILED");
        result.final_reason = !scanReason.empty()
            ? scanReason
            : "one or more manifests, assets, or workflow cases failed validation";
    }
    else if (result.accepted_count == 0U)
    {
        result.final_status = "FAIL";
        result.final_code = "NO_VALID_CASES";
        result.final_reason = "no valid workflow case was discovered";
    }
    else if (result.pending_count > 0U)
    {
        result.final_status = "BUSINESS_WORKFLOW_ACCEPTED_WITH_PENDING";
        result.final_code = "EXPECTED_CAPABILITIES_PENDING";
        result.final_reason =
            "all executable workflow contracts passed; one or more capabilities remain explicitly pending";
    }
    else if (result.contract_fixture_pass_count > 0U)
    {
        result.final_status = "CONTRACT_FIXTURE_PASS";
        if (result.provider_execution_step_pass_count > 0U)
        {
            result.final_code = "BUSINESS_WORKFLOW_MIXED_SCOPE_VALIDATED";
            result.final_reason =
                "contract fixtures passed and some provider steps executed, but end-to-end business execution is not claimed";
        }
        else
        {
            result.final_code = "BUSINESS_WORKFLOW_CONTRACT_FIXTURE_PASS";
            result.final_reason =
                "all contract fixtures passed; provider-backed business execution was not claimed";
        }
    }
    else
    {
        result.final_status = "PASS";
        result.final_code = "BUSINESS_WORKFLOW_PASS";
        result.final_reason =
            "all discovered workflow cases and required evidence contracts passed";
    }

    result.completed_at = UtcTimestamp();
    const bool evidenceWritten = WriteEvidenceArtifacts(result);
    reason = result.final_reason;
    return scanCompleted && evidenceWritten;
}
