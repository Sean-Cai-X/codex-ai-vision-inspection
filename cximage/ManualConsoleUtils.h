#ifndef CXIMAGE_MANUAL_CONSOLE_UTILS_H
#define CXIMAGE_MANUAL_CONSOLE_UTILS_H

#include <string>
#include <vector>
#include <filesystem>
#include <imgui.h>

int StringResizeCallback(ImGuiInputTextCallbackData* data);

bool InputTextString(const char* label, std::string& value);

bool InputTextMultilineString(const char* label, std::string& value,
                              const ImVec2& size);

bool ReadTextFile(const std::string& path, std::string& text);

std::filesystem::path ResolveWorkspaceFile(const std::string& path);

std::filesystem::path ResolveCaseDirectory(const std::string& path);

// Resolve generated run assets below the cxvisionai project root, independent
// of whether the GUI was started from the repository, a build directory, VS,
// or Explorer.  Relative run paths must not follow the process CWD.
std::filesystem::path ResolveCxVisionRunPath(const std::string& path);

std::string TrimLine(const std::string& text);

std::string CurrentTimestamp();

std::vector<std::string> SplitParameters(const std::string& text);

std::vector<std::string> ExtractGlobalNames(const std::string& text);

std::vector<std::string> SplitArgs(const std::string& params);

std::string EscapeJsonString(const std::string& s);

std::string JsonEscape(const std::string& text);

bool WriteTextFile(const std::filesystem::path& path, const std::string& text);

#endif
