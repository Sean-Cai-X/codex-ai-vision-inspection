#pragma once
#include "CxRefinedArcCandidate.h"
#include <cctype>

namespace cxscriptload {
struct Prepared {
  std::string path, text, case_id;
};
// Pure preparation: no editor mutation, execution, catalog scan or source write.
inline Prepared Prepare(const std::filesystem::path& path, std::string caseId,
                        bool dirty, bool discardConfirmed, bool pending) {
  if (pending) throw std::runtime_error("A run or save is pending; finish it before opening a script.");
  if (dirty && !discardConfirmed)
    throw std::runtime_error("Unsaved edits: confirm discard or keep editing.");
  if (path.empty() || !std::filesystem::is_regular_file(path))
    throw std::runtime_error("Script file not found. Enter an existing .cxsc path.");
  auto extension = path.extension().string();
  for (char& c : extension) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  if (extension != ".cxsc") throw std::runtime_error("Select a .cxsc script file.");
  if (std::filesystem::file_size(path) > 8 * 1024 * 1024)
    throw std::runtime_error("Script exceeds the 8 MiB editor limit.");
  if (caseId.empty()) caseId = path.stem().string();
  if (caseId.empty() || caseId.size() > 128 ||
      caseId.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos)
    throw std::runtime_error("Case ID must contain 1-128 ASCII letters, digits, underscore or hyphen.");
  Prepared result{std::filesystem::absolute(path).lexically_normal().string(),
                  cxpaircandidate::Bytes(path), std::move(caseId)};
  if (result.text.find_first_not_of(" \t\r\n") == std::string::npos ||
      result.text.find('\0') != std::string::npos)
    throw std::runtime_error("Script is empty or contains binary NUL bytes.");
  if (cxpairui::IsCase(result.text)) {
    cxpairui::Read(result.text);
    if (std::filesystem::exists(path.parent_path() / "pair_input_binding.json")) {
      cxpaircandidate::Verify(path.parent_path());
      if (result.text != cxpaircandidate::Bytes(path.parent_path() / "script_snapshot.cxsc"))
        throw std::runtime_error("Paired candidate script does not match its frozen snapshot.");
    }
  }
  return result;
}
}
