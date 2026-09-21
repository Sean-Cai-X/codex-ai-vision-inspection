#include "pch.h"
#include "ManualConsoleUtils.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <iterator>
#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

int StringResizeCallback(ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
    {
        std::string* value = static_cast<std::string*>(data->UserData);
        value->resize(static_cast<std::size_t>(data->BufTextLen));
        data->Buf = value->data();
    }
    return 0;
}

bool InputTextString(const char* label, std::string& value)
{
    if (value.capacity() < 256) value.reserve(256);
    return ImGui::InputText(label, value.data(), value.capacity() + 1,
                            ImGuiInputTextFlags_CallbackResize,
                            StringResizeCallback, &value);
}

bool InputTextMultilineString(const char* label, std::string& value,
                              const ImVec2& size)
{
    if (value.capacity() < 4096) value.reserve(4096);
    return ImGui::InputTextMultiline(label, value.data(), value.capacity() + 1,
                                     size,
                                     ImGuiInputTextFlags_CallbackResize |
                                     ImGuiInputTextFlags_AllowTabInput,
                                     StringResizeCallback, &value);
}

bool ReadTextFile(const std::string& path, std::string& text)
{
    const fs::path resolved = ResolveWorkspaceFile(path);
    std::ifstream stream(resolved, std::ios::binary);
    if (!stream) return false;
    text.assign(std::istreambuf_iterator<char>(stream),
                std::istreambuf_iterator<char>());
    return true;
}

std::filesystem::path ResolveWorkspaceFile(const std::string& path)
{
    const fs::path requested(path);
    if (requested.is_absolute() && fs::exists(requested)) return requested;

#ifdef _WIN32
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH))
    {
        const fs::path exeDir = fs::path(exePath).parent_path();
        const fs::path exeRelative = exeDir / requested;
        if (fs::exists(exeRelative)) return fs::absolute(exeRelative);
    }
#endif

    if (fs::exists(requested)) return fs::absolute(requested);
    fs::path current = fs::current_path();
    while (!current.empty())
    {
        const fs::path direct = current / requested;
        const fs::path nested = current / "cxvisionai" / "cxvision_repo" / requested;
        if (fs::exists(direct)) return fs::absolute(direct);
        if (fs::exists(nested)) return fs::absolute(nested);
        const fs::path parent = current.parent_path();
        if (parent == current) break;
        current = parent;
    }
    return requested;
}

std::filesystem::path ResolveCaseDirectory(const std::string& path)
{
    const fs::path requested(path);
    if (requested.is_absolute()) return requested;
    fs::path current = fs::current_path();
    while (!current.empty())
    {
        const fs::path roots[] = {current, current / "cxvisionai" / "cxvision_repo"};
        for (const fs::path& root : roots)
            if (fs::exists(root / "CMakeLists.txt") && fs::exists(root / "cximage") && fs::exists(root / "cxparser"))
                return root / requested;
        const fs::path parent = current.parent_path();
        if (parent == current) break;
        current = parent;
    }
    return requested;
}

std::filesystem::path ResolveCxVisionRunPath(const std::string& path)
{
    const fs::path requested(path);
    if (requested.is_absolute())
        return requested.lexically_normal();

#ifdef _WIN32
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH))
    {
        fs::path current = fs::path(exePath).parent_path();
        while (!current.empty())
        {
            const fs::path repoRoot = current / "cxvision_repo";
            if (fs::exists(repoRoot / "CMakeLists.txt") &&
                fs::exists(repoRoot / "cximage") &&
                fs::exists(repoRoot / "cxparser"))
            {
                return (current / requested).lexically_normal();
            }
            if (current.filename() == "cxvision_repo" &&
                fs::exists(current / "CMakeLists.txt") &&
                fs::exists(current / "cximage") &&
                fs::exists(current / "cxparser"))
            {
                return (current.parent_path() / requested).lexically_normal();
            }
            const fs::path parent = current.parent_path();
            if (parent == current)
                break;
            current = parent;
        }
    }
#endif

    // ResolveCaseDirectory(".") may return either "." or a path ending in a
    // lexical ".".  lexically_normal() can still leave an empty filename on
    // Windows, so canonicalize the existing directory before inspecting its
    // name.  Otherwise a cxvision_repo checkout is mistaken for the project
    // run root and the GUI reads <repo>/cxscript_runs while automated runs and
    // manual guidance assets live in <project>/cxscript_runs.
    std::error_code repoEc;
    fs::path repoRoot = fs::weakly_canonical(
        ResolveCaseDirectory("."),
        repoEc);
    if (repoEc)
        repoRoot = fs::absolute(ResolveCaseDirectory(".")).lexically_normal();
    if (repoRoot.filename().empty())
        repoRoot = repoRoot.parent_path();
    if (fs::exists(repoRoot / "CMakeLists.txt") &&
        fs::exists(repoRoot / "cximage") &&
        fs::exists(repoRoot / "cxparser"))
    {
        const fs::path projectRoot =
            repoRoot.filename() == "cxvision_repo"
                ? repoRoot.parent_path()
                : repoRoot;
        return (projectRoot / requested).lexically_normal();
    }

    std::error_code ec;
    const fs::path absolute = fs::absolute(requested, ec);
    return ec ? requested.lexically_normal() : absolute.lexically_normal();
}

std::string TrimLine(const std::string& text)
{
    const std::size_t first = text.find_first_not_of(" \t\r");
    if (first == std::string::npos) return std::string();
    const std::size_t last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

std::string CurrentTimestamp()
{
    const std::time_t now = std::time(nullptr);
    std::tm local_time = {};
#if defined(_WIN32)
    localtime_s(&local_time, &now);
#else
    localtime_r(&now, &local_time);
#endif
    char buffer[32] = {};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &local_time);
    return buffer;
}

std::vector<std::string> SplitParameters(const std::string& text)
{
    std::vector<std::string> result;
    std::istringstream input(text);
    std::string value;
    while (std::getline(input, value, ',')) result.push_back(TrimLine(value));
    return result;
}

std::vector<std::string> ExtractGlobalNames(const std::string& text)
{
    std::vector<std::string> names;
    // CxScript external inputs/outputs are flat global_* names.  Do not
    // recognize the retired global.xxx pseudo-object syntax here: doing so
    // hides missing bindings and makes the manual path differ from Headless.
    const std::string prefix = "global_";
    std::size_t position = 0;
    while ((position = text.find(prefix, position)) != std::string::npos)
    {
        const bool identifierBoundary =
            position == 0 ||
            !(std::isalnum(static_cast<unsigned char>(text[position - 1])) ||
              text[position - 1] == '_');
        if (!identifierBoundary)
        {
            position += prefix.size();
            continue;
        }

        const std::size_t begin = position;
        std::size_t end = position + prefix.size();
        while (end < text.size() &&
               (std::isalnum(static_cast<unsigned char>(text[end])) ||
                text[end] == '_')) ++end;
        const std::string name = text.substr(begin, end - begin);
        if (!name.empty() && std::find(names.begin(), names.end(), name) == names.end())
            names.push_back(name);
        position = end;
    }
    return names;
}

std::vector<std::string> SplitArgs(const std::string& params)
{
    std::vector<std::string> out;
    std::string current;
    int quote = 0;

    for (char ch : params)
    {
        if (ch == '"')
            quote = !quote;

        if (ch == ',' && quote == 0)
        {
            out.push_back(TrimLine(current));
            current.clear();
        }
        else
        {
            current += ch;
        }
    }

    if (!current.empty())
        out.push_back(TrimLine(current));

    return out;
}

std::string EscapeJsonString(const std::string& s)
{
    std::ostringstream out;

    for (char c : s)
    {
        switch (c)
        {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default: out << c; break;
        }
    }

    return out.str();
}

std::string JsonEscape(const std::string& text)
{
    std::ostringstream out;
    for (const char ch : text)
    {
        if (ch == '\\' || ch == '"') out << '\\' << ch;
        else if (ch == '\n') out << "\\n";
        else if (ch == '\r') out << "\\r";
        else if (ch == '\t') out << "\\t";
        else out << ch;
    }
    return out.str();
}

bool WriteTextFile(const std::filesystem::path& path, const std::string& text)
{
    std::ofstream output(path, std::ios::binary);
    if (!output) return false;
    output << text;
    return output.good();
}
