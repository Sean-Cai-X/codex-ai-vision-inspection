#ifndef CXIMAGE_CXRUNTIMEPATHS_H
#define CXIMAGE_CXRUNTIMEPATHS_H

#include <cstdlib>
#include <filesystem>
#include <string>

inline std::filesystem::path CxRuntimeProjectRoot() {
  if (const char *configured = std::getenv("CXVISION_PROJECT_ROOT");
      configured != nullptr && *configured != '\0') {
    return std::filesystem::path(configured);
  }

  std::error_code ec;
  std::filesystem::path current = std::filesystem::current_path(ec);
  if (ec)
    return {};

  for (int depth = 0; depth < 64 && !current.empty(); ++depth) {
    if (std::filesystem::exists(current / "cximage", ec) &&
        std::filesystem::exists(current / "BUILD.gn", ec)) {
      return current;
    }
    const std::filesystem::path parent = current.parent_path();
    if (parent == current)
      break;
    current = parent;
  }
  return std::filesystem::current_path(ec);
}

inline std::filesystem::path CxRuntimeRunRoot() {
  if (const char *configured = std::getenv("CXVISION_RUN_ROOT");
      configured != nullptr && *configured != '\0') {
    return std::filesystem::path(configured);
  }
  return CxRuntimeProjectRoot() / "cxscript_runs";
}

inline std::filesystem::path CxRuntimeInitialImagePath() {
  if (const char *configured = std::getenv("CXVISION_INITIAL_IMAGE");
      configured != nullptr && *configured != '\0') {
    return std::filesystem::path(configured);
  }

  const std::filesystem::path root = CxRuntimeProjectRoot();
  const std::filesystem::path candidates[] = {
      root / "01.jpg",
      root / "images" / "01.jpg",
      std::filesystem::current_path() / "01.jpg",
  };
  std::error_code ec;
  for (const auto &candidate : candidates) {
    if (std::filesystem::is_regular_file(candidate, ec))
      return candidate;
  }
  // Keep a deterministic, platform-neutral diagnostic path when the asset is
  // absent; Image/OpenCV will report the missing asset without blocking startup.
  return root / "01.jpg";
}

#endif
