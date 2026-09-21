#include "CxScriptStage25JsonLite.h"
#include <fstream>
#include <sstream>
#include <cctype>

bool Stage25JsonLite::LoadFile(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        return false;

    std::stringstream buffer;
    buffer << file.rdbuf();
    text_ = buffer.str();
    return true;
}

size_t Stage25JsonLite::FindKey(const std::string& key) const
{
    const auto dot = key.find('.');
    if (dot == std::string::npos)
    {
        const std::string pattern = "\"" + key + "\"";
        return text_.find(pattern);
    }

    const std::string head = key.substr(0, dot);
    const std::string tail = key.substr(dot + 1);
    const std::string headPattern = "\"" + head + "\"";
    const auto headPos = text_.find(headPattern);
    if (headPos == std::string::npos)
        return std::string::npos;

    const auto colon = text_.find(':', headPos + headPattern.size());
    if (colon == std::string::npos)
        return std::string::npos;

    auto objectStart = text_.find('{', colon + 1);
    if (objectStart == std::string::npos)
        return std::string::npos;

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    size_t objectEnd = std::string::npos;
    for (size_t i = objectStart; i < text_.size(); ++i)
    {
        const char ch = text_[i];
        if (inString)
        {
            if (escaped)
            {
                escaped = false;
            }
            else if (ch == '\\')
            {
                escaped = true;
            }
            else if (ch == '"')
            {
                inString = false;
            }
            continue;
        }

        if (ch == '"')
        {
            inString = true;
            continue;
        }
        if (ch == '{')
            ++depth;
        else if (ch == '}')
        {
            --depth;
            if (depth == 0)
            {
                objectEnd = i;
                break;
            }
        }
    }

    if (objectEnd == std::string::npos)
        return std::string::npos;

    const std::string tailPattern = "\"" + tail + "\"";
    const auto tailPos = text_.find(tailPattern, objectStart + 1);
    if (tailPos == std::string::npos || tailPos > objectEnd)
        return std::string::npos;

    return tailPos;
}

bool Stage25JsonLite::Has(const std::string& key) const
{
    return FindKey(key) != std::string::npos;
}

std::string Stage25JsonLite::GetString(
    const std::string& key,
    const std::string& fallback) const
{
    const auto pos = FindKey(key);
    if (pos == std::string::npos)
        return fallback;

    const auto colon = text_.find(":", pos);
    if (colon == std::string::npos)
        return fallback;

    const auto quote = text_.find("\"", colon + 1);
    if (quote == std::string::npos)
        return fallback;

    const auto end = text_.find("\"", quote + 1);
    if (end == std::string::npos)
        return fallback;

    return text_.substr(quote + 1, end - quote - 1);
}

int Stage25JsonLite::GetInt(
    const std::string& key,
    int fallback) const
{
    const auto pos = FindKey(key);
    if (pos == std::string::npos)
        return fallback;

    const auto colon = text_.find(":", pos);
    if (colon == std::string::npos)
        return fallback;

    const auto start = text_.find_first_of("-0123456789", colon + 1);
    if (start == std::string::npos)
        return fallback;

    const auto end = text_.find_first_not_of("-0123456789", start);

    try
    {
        return std::stoi(text_.substr(start, end - start));
    }
    catch (...)
    {
        return fallback;
    }
}

double Stage25JsonLite::GetDouble(
    const std::string& key,
    double fallback) const
{
    const auto pos = FindKey(key);
    if (pos == std::string::npos)
        return fallback;

    const auto colon = text_.find(":", pos);
    if (colon == std::string::npos)
        return fallback;

    const auto start = text_.find_first_of("-0123456789.", colon + 1);
    if (start == std::string::npos)
        return fallback;

    const auto end = text_.find_first_not_of("-+0123456789.eE", start);

    try
    {
        return std::stod(text_.substr(start, end - start));
    }
    catch (...)
    {
        return fallback;
    }
}

bool Stage25JsonLite::GetBool(
    const std::string& key,
    bool fallback) const
{
    const auto pos = FindKey(key);
    if (pos == std::string::npos)
        return fallback;

    const auto colon = text_.find(":", pos);
    if (colon == std::string::npos)
        return fallback;

    auto start = text_.find_first_not_of(" \t\r\n", colon + 1);
    if (start == std::string::npos)
        return fallback;

    if (text_.compare(start, 4, "true") == 0)
        return true;

    if (text_.compare(start, 5, "false") == 0)
        return false;

    if (text_[start] == '"')
    {
        const auto end = text_.find('"', start + 1);
        if (end == std::string::npos)
            return fallback;

        const std::string value =
            text_.substr(start + 1, end - start - 1);

        if (value == "true" || value == "1")
            return true;

        if (value == "false" || value == "0")
            return false;
    }

    return fallback;
}
