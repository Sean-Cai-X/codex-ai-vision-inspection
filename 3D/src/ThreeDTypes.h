#pragma once

#include <algorithm>
#include <cctype>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace codex_lan_agent_3d {

struct CommandResult {
    int exit_code = 0;
    bool ok = true;
    std::unordered_map<std::string, std::string> fields;
};

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

inline std::string ToKey(const std::string & value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (const unsigned char ch : value) {
        if (std::isalnum(ch) != 0) {
            normalized.push_back(static_cast<char>(std::tolower(ch)));
        }
        else if (ch == ' ' || ch == '-' || ch == '_' || ch == '/' || ch == '.') {
            normalized.push_back('_');
        }
    }
    if (normalized.empty()) {
        normalized = "item";
    }
    while (!normalized.empty() && normalized.front() == '_') {
        normalized.erase(normalized.begin());
    }
    while (!normalized.empty() && normalized.back() == '_') {
        normalized.pop_back();
    }
    if (normalized.empty()) {
        normalized = "item";
    }
    return normalized;
}

inline std::string JoinStrings(const std::vector<std::string> & values, const std::string & separator) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) {
            stream << separator;
        }
        stream << values[index];
    }
    return stream.str();
}

inline std::vector<std::string> SplitString(const std::string & text, char separator) {
    std::vector<std::string> values;
    std::string current;
    for (const char ch : text) {
        if (ch == separator) {
            if (!current.empty()) {
                values.push_back(current);
            }
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        values.push_back(current);
    }
    return values;
}

inline std::string FormatVec3(const Vec3 & value) {
    std::ostringstream stream;
    stream << value.x << "," << value.y << "," << value.z;
    return stream.str();
}

inline std::string JsonEscape(const std::string & text) {
    std::ostringstream stream;
    for (const char ch : text) {
        switch (ch) {
        case '\\':
            stream << "\\\\";
            break;
        case '"':
            stream << "\\\"";
            break;
        case '\n':
            stream << "\\n";
            break;
        case '\r':
            stream << "\\r";
            break;
        case '\t':
            stream << "\\t";
            break;
        default:
            stream << ch;
            break;
        }
    }
    return stream.str();
}

inline bool TryParseVec3(const std::string & text, Vec3 * value) {
    if (value == nullptr) {
        return false;
    }

    std::istringstream stream(text);
    Vec3 parsed;
    char first = 0;
    char second = 0;
    if (!(stream >> parsed.x >> first >> parsed.y >> second >> parsed.z)) {
        return false;
    }
    if (first != ',' || second != ',') {
        return false;
    }
    *value = parsed;
    return true;
}

}  // namespace codex_lan_agent_3d
