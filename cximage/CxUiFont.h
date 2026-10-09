#pragma once
#include "../imgui/imgui.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace cxuifont {
// Local font assets only. Preserve the default Latin font and widget sizing.
inline std::string Load(ImGuiIO& io, const char* configured = nullptr) {
  io.Fonts->AddFontDefault();
  std::vector<std::string> paths;
  if (configured && *configured) paths.emplace_back(configured);
  else {
#ifdef _WIN32
    paths = {"C:/Windows/Fonts/msyh.ttc", "C:/Windows/Fonts/simsun.ttc"};
#else
    paths = {"/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf",
             "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
             "/usr/share/fonts/truetype/wqy/wqy-microhei.ttc"};
#endif
  }
  for (const auto& path : paths) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec)) continue;
    const auto bytes = std::filesystem::file_size(path, ec);
    if (ec || bytes < 12 || bytes > 64 * 1024 * 1024) continue;
    std::ifstream file(path, std::ios::binary);
    unsigned char magic[4]{};
    file.read(reinterpret_cast<char*>(magic), 4);
    if (!file) continue;
    const bool sfnt = magic[0] == 0 && magic[1] == 1 && magic[2] == 0 && magic[3] == 0;
    const std::string signature(reinterpret_cast<char*>(magic), 4);
    if (!sfnt && signature != "ttcf" && signature != "OTTO") continue;
    ImFontConfig config;
    config.MergeMode = true;
    config.OversampleH = config.OversampleV = 1;
    ImFont* font = io.Fonts->AddFontFromFileTTF(path.c_str(), 13.0f, &config,
                                              io.Fonts->GetGlyphRangesChineseFull());
    if (!font || !io.Fonts->Build()) break;
    bool chinese = true;
    for (ImWchar ch : {ImWchar(0x56fe), ImWchar(0x50cf), ImWchar(0x6807),
                       ImWchar(0x6ce8), ImWchar(0x8fb9), ImWchar(0x754c)})
      chinese = chinese && font->FindGlyphNoFallback(ch) != nullptr;
    return std::string(chinese ? "UI_FONT_CJK_READY path=" : "UI_FONT_CJK_INCOMPLETE path=") + path;
  }
  io.Fonts->Build();
  return "UI_FONT_CJK_MISSING: set CXVISION_UI_FONT to a local CJK TTF/TTC; Latin fallback active";
}
} // namespace cxuifont
