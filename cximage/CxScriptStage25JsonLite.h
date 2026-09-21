#pragma once

#include <string>
#include <filesystem>

class Stage25JsonLite
{
public:
    bool LoadFile(const std::filesystem::path& path);

    bool Has(const std::string& key) const;

    std::string GetString(
        const std::string& key,
        const std::string& fallback = "") const;

    int GetInt(
        const std::string& key,
        int fallback = 0) const;

    double GetDouble(
        const std::string& key,
        double fallback = 0.0) const;

    bool GetBool(
        const std::string& key,
        bool fallback = false) const;

private:
    std::string text_;

    size_t FindKey(const std::string& key) const;
};
