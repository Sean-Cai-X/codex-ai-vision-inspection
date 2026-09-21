#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <commdlg.h>

#include "CircleShape.h"
#include "CxScriptCasePackageWriter.h"
#include "CxScriptCatalogRuntime.h"
#include "CxScriptEvidenceChainRuntime.h"
#include "CxGeometryReferenceEvaluator.h"
#include "CxTorchExecutionAdapter.h"
#include "CxUnifiedLog.h"
#include "EllipseShape.h"
#include "LineGaugeShape.h"
#include "ManualConsoleEvidenceChain.h"
#include "ManualConsoleGauge.h"
#include "ManualConsoleUtils.h"
#include "ManualStateTestConsole.h"
#include "PolylineShape.h"
#include "RectShape.h"
#include "pch.h"

#include <glad/glad.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>

static std::string
NormalizeEvidenceToolTypeLocal(const std::string &typeOrTool) {
  std::string lowered = typeOrTool;
  std::transform(
      lowered.begin(), lowered.end(), lowered.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

  if (lowered == "findline")
    return "FindLine";
  if (lowered == "findcircle")
    return "FindCircle";
  if (lowered == "findellipse")
    return "FindEllipse";
  if (lowered == "findrect")
    return "FindRect";
  if (lowered == "findobject" || lowered == "find_object")
    return "FindObject";
  if (lowered == "fastmatch" || lowered == "cfastmatch")
    return "FastMatch";
  if (lowered == "gridpatternclasstool" || lowered == "gridpatternclass")
    return "GridPatternClassTool";
  if (lowered == "regionpatterntool" || lowered == "regionpattern")
    return "RegionPatternTool";
  if (lowered == "findsegmentation" ||
      lowered.find("findsegmentation") != std::string::npos ||
      lowered.find("find_segmentation") != std::string::npos)
    return "FindSegmentation";
  if (lowered == "torchtask" || lowered == "torch" ||
      lowered.find("torch") != std::string::npos)
    return "TorchTask";
  return typeOrTool;
}

static std::string EvidenceSourceFileNameLocal(const std::string &pathOrId) {
  if (pathOrId.empty())
    return {};
  return std::filesystem::path(pathOrId).filename().string();
}

static bool
SameEvidenceSourceScriptLocal(const std::string &candidateSourcePath,
                              const ScriptEvidenceThumb &original) {
  if (candidateSourcePath.empty())
    return false;

  const std::filesystem::path candidateSource =
      ResolveWorkspaceFile(candidateSourcePath).lexically_normal();
  const std::string originalPath = !original.source_evidence_script_path.empty()
                                       ? original.source_evidence_script_path
                                       : original.script_path;
  if (!originalPath.empty()) {
    const std::filesystem::path originalSource =
        ResolveWorkspaceFile(originalPath).lexically_normal();
    if (candidateSource == originalSource)
      return true;
  }

  const std::string candidateFile =
      EvidenceSourceFileNameLocal(candidateSourcePath);
  if (candidateFile.empty())
    return false;
  return candidateFile == EvidenceSourceFileNameLocal(
                              original.source_evidence_script_path) ||
         candidateFile == EvidenceSourceFileNameLocal(original.script_path) ||
         candidateFile == EvidenceSourceFileNameLocal(original.script_id);
}
static std::string BuildEvidenceClassificationKeyLocal(
    const std::string &tool, const std::string &scriptId,
    const std::string &scriptPath, const std::string &label,
    const std::string &status, const std::string &reason,
    const std::string &parameterSummary) {
  std::string key = tool + " " + scriptId + " " + scriptPath + " " + label +
                    " " + status + " " + reason + " " + parameterSummary;
  std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return key;
}

static std::string
BuildEvidenceCategoryOverrideKeyLocal(const ScriptEvidenceThumb &thumb) {
  std::ostringstream oss;
  oss << "case=" << thumb.case_id << "|script_id=" << thumb.script_id
      << "|script_path=" << thumb.script_path
      << "|source_script=" << thumb.source_evidence_script_path
      << "|image=" << thumb.image_id << "|target=" << thumb.target_id
      << "|candidate=" << thumb.candidate_id;
  return oss.str();
}

static std::vector<std::string>
BuildEvidenceCategoryOverrideLookupKeysLocal(const ScriptEvidenceThumb &thumb) {
  std::vector<std::string> keys;
  keys.push_back(BuildEvidenceCategoryOverrideKeyLocal(thumb));

  if (!thumb.case_id.empty() && !thumb.candidate_id.empty()) {
    std::ostringstream oss;
    oss << "case=" << thumb.case_id << "|candidate=" << thumb.candidate_id;
    keys.push_back(oss.str());
  }

  if (!thumb.case_id.empty()) {
    std::ostringstream oss;
    oss << "case=" << thumb.case_id;
    keys.push_back(oss.str());
  }

  if (!thumb.image_id.empty() && !thumb.target_id.empty()) {
    std::ostringstream oss;
    oss << "image=" << thumb.image_id << "|target=" << thumb.target_id;
    keys.push_back(oss.str());
  }

  return keys;
}

static std::string
PreferEvidenceCategoryOverrideLocal(const std::string &current,
                                    const std::string &candidate) {
  if (candidate.empty())
    return current;
  if (current.empty())
    return candidate;
  if (current == candidate)
    return current;
  if (candidate == "Verified" || current == "Verified")
    return "Verified";
  if (candidate == "Defect" || current == "Defect")
    return "Defect";
  if (candidate == "To Verify" || current == "To Verify")
    return "To Verify";
  return current;
}

static void StoreEvidenceCategoryOverrideLocal(ManualTestContext &context,
                                               const ScriptEvidenceThumb &thumb,
                                               const std::string &category) {
  for (const std::string &key :
       BuildEvidenceCategoryOverrideLookupKeysLocal(thumb)) {
    if (!key.empty())
      context.evidence_category_overrides[key] = category;
  }
}

static std::string
ResolveEvidenceCategoryOverrideLocal(const ManualTestContext &context,
                                     const ScriptEvidenceThumb &thumb) {
  std::string resolved;
  for (const std::string &key :
       BuildEvidenceCategoryOverrideLookupKeysLocal(thumb)) {
    const auto it = context.evidence_category_overrides.find(key);
    if (it != context.evidence_category_overrides.end())
      resolved = PreferEvidenceCategoryOverrideLocal(resolved, it->second);
  }

  const std::string haystack =
      thumb.case_id + " " + thumb.script_id + " " + thumb.script_path + " " +
      thumb.source_evidence_script_path + " " + thumb.image_id + " " +
      thumb.target_id + " " + thumb.reason + " " + thumb.parameter_summary;

  for (const auto &entry : context.evidence_category_overrides) {
    const std::string &key = entry.first;
    if (key.rfind("case=", 0) == 0) {
      const std::string caseId = key.substr(5);
      if (!caseId.empty() && (thumb.case_id == caseId ||
                              haystack.find(caseId) != std::string::npos)) {
        resolved = PreferEvidenceCategoryOverrideLocal(resolved, entry.second);
      }
      continue;
    }

    if (key.rfind("image=", 0) == 0) {
      const std::size_t targetPos = key.find("|target=");
      if (targetPos == std::string::npos)
        continue;
      const std::string imageId = key.substr(6, targetPos - 6);
      const std::string targetId = key.substr(targetPos + 8);
      if (!imageId.empty() && !targetId.empty() && thumb.image_id == imageId &&
          thumb.target_id == targetId) {
        resolved = PreferEvidenceCategoryOverrideLocal(resolved, entry.second);
      }
    }
  }

  return resolved.empty() ? thumb.evidence_category_override : resolved;
}

static bool
HasCuratedFindGeometryCategoryOverridesLocal(const ManualTestContext &context) {
  for (const auto &entry : context.evidence_category_overrides) {
    if (entry.first.rfind("case=", 0) == 0 ||
        entry.first.rfind("image=", 0) == 0) {
      return true;
    }
  }
  return false;
}

static bool IsTorchEvidenceCandidateRowLocal(const ScriptEvidenceThumb &thumb,
                                             const std::string &groupLabel) {
  const std::string normalizedTool = NormalizeEvidenceToolTypeLocal(thumb.tool);

  return normalizedTool == "TorchTask" &&
         thumb.evidence_category_override == "Torch Evidence Candidates";
}

static bool
IsPendingRestorableEvidenceCandidateLocal(const ScriptEvidenceThumb &thumb) {
  if (!thumb.is_candidate && !thumb.has_saved_state)
    return false;

  std::string key = thumb.status + " " + thumb.reason + " " +
                    thumb.evidence_category_override;
  std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  if (key.find("manual_confirmed") != std::string::npos ||
      key.find("manual_gui_pass") != std::string::npos ||
      key.find("manual accepted") != std::string::npos) {
    return false;
  }

  return key.find("pending_human_review") != std::string::npos ||
         key.find("pending manual acceptance") != std::string::npos ||
         key.find("gauge_accepted=false") != std::string::npos;
}

static std::pair<int, std::string>
ClassifyEvidenceMajorBucketLocal(const ManualTestContext &context,
                                 const ScriptEvidenceThumb &thumb,
                                 const std::string &groupLabel) {
  if (IsPendingRestorableEvidenceCandidateLocal(thumb))
    return {0, "To Verify"};

  const std::string categoryOverride =
      ResolveEvidenceCategoryOverrideLocal(context, thumb);
  if (!categoryOverride.empty()) {
    if (categoryOverride == "To Verify")
      return {0, categoryOverride};
    if (categoryOverride == "Verified")
      return {1, categoryOverride};
    if (categoryOverride == "Defect")
      return {2, categoryOverride};
    return {10, categoryOverride};
  }

  if (!groupLabel.empty())
    return {20, groupLabel};

  return {30, "Uncategorized"};
}

static std::filesystem::path EvidenceCategoryOverridesPathLocal() {
  return ResolveCxVisionRunPath(
      "cxscript_runs/evidence_chain/evidence_category_overrides.tsv");
}

static std::string EscapeEvidenceOverrideFieldLocal(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  for (char ch : text) {
    switch (ch) {
    case '\\':
      out += "\\\\";
      break;
    case '\t':
      out += "\\t";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      break;
    default:
      out += ch;
      break;
    }
  }
  return out;
}

static std::string UnescapeEvidenceOverrideFieldLocal(const std::string &text) {
  std::string out;
  out.reserve(text.size());
  bool escaped = false;
  for (char ch : text) {
    if (escaped) {
      if (ch == 't')
        out += '\t';
      else if (ch == 'n')
        out += '\n';
      else
        out += ch;
      escaped = false;
      continue;
    }
    if (ch == '\\') {
      escaped = true;
      continue;
    }
    out += ch;
  }
  if (escaped)
    out += '\\';
  return out;
}

static bool LoadEvidenceCategoryOverridesLocal(ManualTestContext &context) {
  std::string text;
  const std::filesystem::path path = EvidenceCategoryOverridesPathLocal();
  if (!ReadTextFile(path.string(), text))
    return false;

  std::istringstream input(text);
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#')
      continue;
    const std::size_t tab = line.find('\t');
    if (tab == std::string::npos)
      continue;
    const std::string key =
        TrimLine(UnescapeEvidenceOverrideFieldLocal(line.substr(0, tab)));
    const std::string category =
        TrimLine(UnescapeEvidenceOverrideFieldLocal(line.substr(tab + 1)));
    if (!key.empty() && !category.empty())
      context.evidence_category_overrides[key] = category;
  }
  return true;
}

static bool SaveEvidenceCategoryOverridesLocal(const ManualTestContext &context,
                                               std::string &reason) {
  const std::filesystem::path path = EvidenceCategoryOverridesPathLocal();
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    reason = "failed to create Evidence category override directory: " +
             path.parent_path().string() + " reason=" + ec.message();
    return false;
  }

  std::vector<std::pair<std::string, std::string>> entries(
      context.evidence_category_overrides.begin(),
      context.evidence_category_overrides.end());
  std::stable_sort(entries.begin(), entries.end(),
                   [](const auto &left, const auto &right) {
                     return left.first < right.first;
                   });

  std::ostringstream out;
  out << "# Evidence Chain manual category overrides\n";
  out << "# key<TAB>category\n";
  for (const auto &entry : entries) {
    out << EscapeEvidenceOverrideFieldLocal(entry.first) << '\t'
        << EscapeEvidenceOverrideFieldLocal(entry.second) << '\n';
  }

  if (!WriteTextFile(path, out.str())) {
    reason = "failed to write Evidence category overrides: " + path.string();
    return false;
  }
  reason = "Evidence category overrides saved: " + path.string();
  return true;
}

static std::string InferEvidenceChainToolBucketLocal(
    const std::string &tool, const std::string &scriptId,
    const std::string &scriptPath, const std::string &label,
    const std::string &status = {}, const std::string &reason = {},
    const std::string &parameterSummary = {}) {
  const std::string normalizedTool = NormalizeEvidenceToolTypeLocal(tool);
  const std::string key = BuildEvidenceClassificationKeyLocal(
      tool, scriptId, scriptPath, label, status, reason, parameterSummary);

  if (normalizedTool == "TorchTask") {
    if (key.find("detect") != std::string::npos ||
        key.find("yolo") != std::string::npos) {
      return "Torch Detection - Model Unverified";
    }
    if (key.find("segment") != std::string::npos ||
        key.find("mask") != std::string::npos ||
        key.find("deeplab") != std::string::npos ||
        key.find("edgesam") != std::string::npos) {
      return "Torch Segmentation - Runtime Smoke";
    }
    return "Torch / Model Validation";
  }

  if (normalizedTool == "FindSegmentation" ||
      key.find("find_segmentation") != std::string::npos ||
      key.find("findsegmentation") != std::string::npos ||
      key.find("edgesam") != std::string::npos) {
    return "FindSegmentation Prompt / EdgeSam";
  }

  if (key.find("torch") != std::string::npos ||
      key.find("deeplab") != std::string::npos ||
      key.find("yolo") != std::string::npos) {
    if (key.find("detect") != std::string::npos ||
        key.find("yolo") != std::string::npos) {
      return "Torch Detection - Model Unverified";
    }
    if (key.find("segment") != std::string::npos ||
        key.find("mask") != std::string::npos ||
        key.find("deeplab") != std::string::npos) {
      return "Torch Segmentation - Runtime Smoke";
    }
    return "Torch / Model Validation";
  }

  return normalizedTool.empty() ? (label.empty() ? "Other" : label)
                                : normalizedTool;
}

static std::string
ResolveEvidenceImagePathByIdFromDiskLocal(const std::string &imageId) {
  if (imageId.empty() || imageId.rfind("fallback_image_", 0) == 0)
    return {};

  const std::filesystem::path root = ResolveCxVisionRunPath("test_images");
  std::error_code ec;
  if (!std::filesystem::is_directory(root, ec))
    return {};

  static const char *exts[] = {".jpg", ".jpeg", ".png",
                               ".bmp", ".tif",  ".tiff"};

  std::filesystem::recursive_directory_iterator it(
      root, std::filesystem::directory_options::skip_permission_denied, ec);
  const std::filesystem::recursive_directory_iterator end;
  for (; !ec && it != end; it.increment(ec)) {
    if (!it->is_regular_file(ec))
      continue;
    const std::filesystem::path path = it->path();
    const std::string stem = path.stem().string();
    if (stem != imageId)
      continue;
    const std::string ext = path.extension().string();
    std::string lowerExt = ext;
    std::transform(
        lowerExt.begin(), lowerExt.end(), lowerExt.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    for (const char *allowed : exts) {
      if (lowerExt == allowed)
        return path.string();
    }
  }
  return {};
}

struct HDReferenceImageBindingLocal {
  std::string script_id;
  std::string image_id;
  std::string image_path;
};

static bool
ResolveHDReferenceImageBindingLocal(const std::string &scriptId,
                                    HDReferenceImageBindingLocal &out) {
  auto normalizeKey = [](const std::string &value) -> std::string {
    if (value.empty())
      return {};
    std::string normalized = value;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    const std::size_t slash = normalized.find_last_of('/');
    if (slash != std::string::npos)
      normalized = normalized.substr(slash + 1);
    const std::string suffix = ".cxsc";
    if (normalized.size() > suffix.size() &&
        normalized.compare(normalized.size() - suffix.size(), suffix.size(),
                           suffix) == 0) {
      normalized.resize(normalized.size() - suffix.size());
    }
    return normalized;
  };

  auto splitTsv = [](const std::string &line) -> std::vector<std::string> {
    std::vector<std::string> cells;
    std::string current;
    for (char ch : line) {
      if (ch == '\t') {
        cells.push_back(current);
        current.clear();
        continue;
      }
      if (ch != '\r')
        current += ch;
    }
    cells.push_back(current);
    return cells;
  };

  static bool loaded = false;
  static std::vector<HDReferenceImageBindingLocal> bindings;
  if (!loaded) {
    loaded = true;
    const std::filesystem::path bindingPath =
        ResolveWorkspaceFile("cxparser/cxscript/module/cximage/evidence/"
                             "hd_reference_image_bindings.tsv");
    std::string text;
    if (ReadTextFile(bindingPath.string(), text)) {
      std::istringstream input(text);
      std::string line;
      bool headerSeen = false;
      while (std::getline(input, line)) {
        line = TrimLine(line);
        if (line.empty() || line[0] == '#')
          continue;
        const std::vector<std::string> cells = splitTsv(line);
        if (!headerSeen) {
          headerSeen = true;
          if (!cells.empty() && cells[0] == "script_id")
            continue;
        }
        if (cells.size() < 3)
          continue;
        HDReferenceImageBindingLocal binding;
        binding.script_id = TrimLine(cells[0]);
        binding.image_id = TrimLine(cells[1]);
        binding.image_path = TrimLine(cells[2]);
        if (!binding.script_id.empty() && !binding.image_id.empty() &&
            !binding.image_path.empty()) {
          bindings.push_back(std::move(binding));
        }
      }
    }
  }

  const std::string key = normalizeKey(scriptId);
  for (const auto &binding : bindings) {
    if (key == binding.script_id) {
      out = binding;
      return true;
    }
  }
  return false;
}

static void ApplyHDReferenceImageBindingLocal(ScriptEvidenceThumb &thumb) {
  HDReferenceImageBindingLocal binding{};
  if (!ResolveHDReferenceImageBindingLocal(thumb.script_id, binding) &&
      !ResolveHDReferenceImageBindingLocal(thumb.script_path, binding)) {
    return;
  }

  thumb.image_id = binding.image_id;
  thumb.image_path = binding.image_path;
  thumb.thumbnail_path = binding.image_path;
  if (thumb.case_id.empty())
    thumb.case_id = binding.script_id;
  if (thumb.status.empty())
    thumb.status = "hd_reference_ready";
  if (thumb.reason.empty())
    thumb.reason = "HD reference image binding";
  else
    thumb.reason += "; HD reference image binding";
}

static bool SelectEvidenceImageFileFromDialogLocal(std::string &outPath,
                                                   std::string &reason) {
  outPath.clear();
  // Loading a test image is deliberately a read-only UI transaction. The
  // common dialog returns a path only; callers decode it into the Image View
  // and never write source pixels, manifests, or Evidence assets.
  std::array<char, MAX_PATH> fileName{};
  OPENFILENAMEA dialog{};
  dialog.lStructSize = sizeof(dialog);
  dialog.hwndOwner = ::GetActiveWindow();
  dialog.lpstrFilter =
      "Supported images (*.bmp;*.png;*.jpg;*.jpeg;*.tif;*.tiff)\0"
      "*.bmp;*.png;*.jpg;*.jpeg;*.tif;*.tiff\0"
      "All files (*.*)\0*.*\0\0";
  dialog.lpstrFile = fileName.data();
  dialog.nMaxFile = static_cast<DWORD>(fileName.size());
  dialog.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST |
                 OFN_HIDEREADONLY | OFN_NOCHANGEDIR;
  dialog.lpstrTitle = "Load Evidence Image (read-only)";

  if (!::GetOpenFileNameA(&dialog)) {
    const DWORD dialogError = ::CommDlgExtendedError();
    if (dialogError == 0) {
      reason = "image selection cancelled by operator";
    } else {
      reason = "image selection dialog failed, error=" +
               std::to_string(static_cast<unsigned long>(dialogError));
    }
    return false;
  }

  const std::filesystem::path selected(fileName.data());
  std::error_code ec;
  if (!std::filesystem::is_regular_file(selected, ec)) {
    reason = "selected path is not a readable regular file: " +
             selected.string();
    return false;
  }

  outPath = selected.lexically_normal().string();
  reason = "operator selected read-only image: " + outPath;
  return true;
}

static bool IsEvidenceEditableToolTypeLocal(const std::string &type) {
  const std::string normalized = NormalizeEvidenceToolTypeLocal(type);
  return normalized == "FindLine" || normalized == "FindCircle" ||
         normalized == "FindEllipse" || normalized == "FindRect" ||
         normalized == "FindObject" || normalized == "FastMatch" ||
         normalized == "GridPatternClassTool" ||
         normalized == "RegionPatternTool" || normalized == "FindSegmentation";
}

static std::string StripCxScriptLineCommentLocal(const std::string &line) {
  const std::size_t comment = line.find("//");
  if (comment == std::string::npos)
    return line;
  return line.substr(0, comment);
}

static void AnalyzeEditableObjectsFromCxScriptLocal(
    const std::string &scriptText,
    std::vector<CxEvidenceEditableObjectRef> &outObjects) {
  outObjects.clear();

  std::istringstream input(scriptText);
  std::string raw;
  int lineNo = 1;
  while (std::getline(input, raw)) {
    const std::string statement = TrimLine(StripCxScriptLineCommentLocal(raw));
    if (statement.empty() || statement.find('(') != std::string::npos ||
        statement.find('=') != std::string::npos) {
      ++lineNo;
      continue;
    }

    std::istringstream tokens(statement);
    std::string type;
    std::string name;
    tokens >> type >> name;
    if (type.empty() || name.empty()) {
      ++lineNo;
      continue;
    }

    const std::size_t suffix = name.find_first_of(";");
    if (suffix != std::string::npos)
      name.erase(suffix);

    type = NormalizeEvidenceToolTypeLocal(type);
    if (IsEvidenceEditableToolTypeLocal(type) && !name.empty()) {
      CxEvidenceEditableObjectRef ref;
      ref.type = type;
      ref.name = name;
      ref.declared_line = lineNo;
      outObjects.push_back(ref);
    }

    ++lineNo;
  }
}

static std::string
EnsureFindLineSelectedEdgeStatementLocal(const std::string &tool,
                                         const std::string &scriptText) {
  if (NormalizeEvidenceToolTypeLocal(tool) != "FindLine")
    return scriptText;
  if (scriptText.find("setselectedgenum") != std::string::npos)
    return scriptText;
  if (scriptText.find("FindLine") == std::string::npos &&
      scriptText.find("Findline") == std::string::npos) {
    return scriptText;
  }

  const std::string statement =
      "m_line.setselectedgenum(global_findline_selected_edge);\n";
  std::string migrated = scriptText;
  std::size_t insertAt = migrated.find("m_line.measure");
  if (insertAt == std::string::npos)
    insertAt = migrated.find("m_line.measureRobust");
  if (insertAt == std::string::npos) {
    if (!migrated.empty() && migrated.back() != '\n')
      migrated += "\n";
    migrated += statement;
    return migrated;
  }

  migrated.insert(insertAt, statement);
  return migrated;
}

static std::string
ReadKeyValueFromEvidenceParamSummaryLocal(const std::string &summary,
                                          const std::string &key) {
  std::istringstream tokens(summary);
  std::string token;
  const std::string prefix = key + "=";
  while (tokens >> token) {
    if (token.rfind(prefix, 0) == 0)
      return token.substr(prefix.size());
  }
  return {};
}

static bool ReadJsonIntFieldLocal(const std::string &text,
                                  const std::string &key, int &outValue) {
  const std::string pattern = "\"" + key + "\"";
  const std::size_t keyPos = text.find(pattern);
  if (keyPos == std::string::npos)
    return false;
  const std::size_t colon = text.find(':', keyPos + pattern.size());
  if (colon == std::string::npos)
    return false;

  std::size_t begin = colon + 1;
  while (begin < text.size() &&
         std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }

  std::size_t end = begin;
  while (end < text.size() &&
         (std::isdigit(static_cast<unsigned char>(text[end])) ||
          text[end] == '-' || text[end] == '+')) {
    ++end;
  }

  if (end == begin)
    return false;

  try {
    outValue = std::stoi(text.substr(begin, end - begin));
    return true;
  } catch (...) {
    return false;
  }
}

static bool ReadJsonBoolFieldLocal(const std::string &text,
                                   const std::string &key, bool &outValue) {
  const std::string pattern = "\"" + key + "\"";
  const std::size_t keyPos = text.find(pattern);
  if (keyPos == std::string::npos)
    return false;
  const std::size_t colon = text.find(':', keyPos + pattern.size());
  if (colon == std::string::npos)
    return false;

  std::size_t begin = colon + 1;
  while (begin < text.size() &&
         std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }

  if (text.compare(begin, 4, "true") == 0) {
    outValue = true;
    return true;
  }
  if (text.compare(begin, 5, "false") == 0) {
    outValue = false;
    return true;
  }
  return false;
}

static std::string ReadJsonStringFieldLocal(const std::string &text,
                                            const std::string &key) {
  const std::string pattern = "\"" + key + "\"";
  const std::size_t keyPos = text.find(pattern);
  if (keyPos == std::string::npos)
    return {};
  const std::size_t colon = text.find(':', keyPos + pattern.size());
  if (colon == std::string::npos)
    return {};

  std::size_t begin = colon + 1;
  while (begin < text.size() &&
         std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }
  if (begin >= text.size() || text[begin] != '"')
    return {};
  ++begin;

  std::string value;
  bool escaped = false;
  for (std::size_t i = begin; i < text.size(); ++i) {
    const char ch = text[i];
    if (escaped) {
      value.push_back(ch);
      escaped = false;
      continue;
    }
    if (ch == '\\') {
      escaped = true;
      continue;
    }
    if (ch == '"')
      return value;
    value.push_back(ch);
  }

  return {};
}

static bool ReadJsonDoubleFieldLocal(const std::string &text,
                                     const std::string &key, double &outValue) {
  const std::string pattern = "\"" + key + "\"";
  const std::size_t keyPos = text.find(pattern);
  if (keyPos == std::string::npos)
    return false;
  const std::size_t colon = text.find(':', keyPos + pattern.size());
  if (colon == std::string::npos)
    return false;

  std::size_t begin = colon + 1;
  while (begin < text.size() &&
         std::isspace(static_cast<unsigned char>(text[begin]))) {
    ++begin;
  }

  std::size_t end = begin;
  while (end < text.size() &&
         (std::isdigit(static_cast<unsigned char>(text[end])) ||
          text[end] == '-' || text[end] == '+' || text[end] == '.' ||
          text[end] == 'e' || text[end] == 'E')) {
    ++end;
  }
  if (end == begin)
    return false;

  try {
    outValue = std::stod(text.substr(begin, end - begin));
    return true;
  } catch (...) {
    return false;
  }
}

static std::string
ExtractJsonObjectByStringFieldLocal(const std::string &text,
                                    const std::string &key,
                                    const std::string &expectedValue) {
  const std::string pattern = "\"" + key + "\"";
  std::size_t search = 0;
  while ((search = text.find(pattern, search)) != std::string::npos) {
    const std::string value =
        ReadJsonStringFieldLocal(text.substr(search), key);
    if (value != expectedValue) {
      search += pattern.size();
      continue;
    }

    const std::size_t objectBegin = text.rfind('{', search);
    if (objectBegin == std::string::npos)
      return {};

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (std::size_t i = objectBegin; i < text.size(); ++i) {
      const char ch = text[i];
      if (inString) {
        if (escaped) {
          escaped = false;
        } else if (ch == '\\') {
          escaped = true;
        } else if (ch == '"') {
          inString = false;
        }
        continue;
      }
      if (ch == '"') {
        inString = true;
      } else if (ch == '{') {
        ++depth;
      } else if (ch == '}' && --depth == 0) {
        return text.substr(objectBegin, i - objectBegin + 1);
      }
    }
    return {};
  }
  return {};
}

static std::string ReadSemicolonFieldLocal(const std::string &fields,
                                           const std::string &field) {
  const std::string token = field + "=";
  const std::size_t begin = fields.find(token);
  if (begin == std::string::npos)
    return {};
  const std::size_t valueBegin = begin + token.size();
  const std::size_t end = fields.find(';', valueBegin);
  return TrimLine(fields.substr(valueBegin, end - valueBegin));
}

static bool
ValidateCandidateGaugeAnnotationLocal(const std::filesystem::path &gaugePath,
                                       const std::string &bindingTool,
                                       const std::filesystem::path &runtimeGlobalsPath,
                                       std::string &reason) {
  std::string text;
  if (gaugePath.empty() || !ReadTextFile(gaugePath.string(), text)) {
    reason = "gauge_annotation.json is missing";
    return false;
  }

  std::string tool =
      NormalizeEvidenceToolTypeLocal(ReadJsonStringFieldLocal(text, "tool"));
  if (tool.empty())
    tool = NormalizeEvidenceToolTypeLocal(bindingTool);

  if (tool == "FindObject") {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    if (!ReadJsonIntFieldLocal(text, "findobject_x0", x0) ||
        !ReadJsonIntFieldLocal(text, "findobject_y0", y0) ||
        !ReadJsonIntFieldLocal(text, "findobject_x1", x1) ||
        !ReadJsonIntFieldLocal(text, "findobject_y1", y1)) {
      std::string runtimeGlobals;
      if (runtimeGlobalsPath.empty() ||
          !ReadTextFile(runtimeGlobalsPath.string(), runtimeGlobals) ||
          !ReadJsonIntFieldLocal(runtimeGlobals, "global_roi_x0", x0) ||
          !ReadJsonIntFieldLocal(runtimeGlobals, "global_roi_y0", y0) ||
          !ReadJsonIntFieldLocal(runtimeGlobals, "global_roi_x1", x1) ||
          !ReadJsonIntFieldLocal(runtimeGlobals, "global_roi_y1", y1)) {
        reason = "FindObject ROI gauge fields are incomplete";
        return false;
      }
    }
    if (x0 == x1 || y0 == y1) {
      reason = "FindObject ROI must have positive width and height";
      return false;
    }
    reason.clear();
    return true;
  }

  if (tool == "FindLine" || tool == "FindRect") {
    bool hasLineGauge = false;
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    int halfWidth = 0;
    if (!ReadJsonBoolFieldLocal(text, "has_line_gauge", hasLineGauge) ||
        !hasLineGauge || !ReadJsonIntFieldLocal(text, "line_x0", x0) ||
        !ReadJsonIntFieldLocal(text, "line_y0", y0) ||
        !ReadJsonIntFieldLocal(text, "line_x1", x1) ||
        !ReadJsonIntFieldLocal(text, "line_y1", y1) ||
        !ReadJsonIntFieldLocal(text, "tool_half_width", halfWidth)) {
      reason = "FindLine/FindRect gauge fields are incomplete";
      return false;
    }
    if (x0 == x1 && y0 == y1) {
      reason = "line gauge length is zero";
      return false;
    }
    if (halfWidth <= 0) {
      reason = "tool_half_width must be positive";
      return false;
    }
    reason.clear();
    return true;
  }

  if (tool == "FindCircle") {
    bool hasCircleGauge = false;
    int cx = 0;
    int cy = 0;
    int px = 0;
    int py = 0;
    if (!ReadJsonBoolFieldLocal(text, "has_circle_gauge", hasCircleGauge) ||
        !hasCircleGauge || !ReadJsonIntFieldLocal(text, "circle_cx", cx) ||
        !ReadJsonIntFieldLocal(text, "circle_cy", cy) ||
        !ReadJsonIntFieldLocal(text, "circle_px", px) ||
        !ReadJsonIntFieldLocal(text, "circle_py", py)) {
      reason = "FindCircle gauge fields are incomplete";
      return false;
    }
    if (cx == px && cy == py) {
      reason = "circle radius is zero";
      return false;
    }
    reason.clear();
    return true;
  }

  if (tool == "FindEllipse") {
    bool hasEllipseGauge = false;
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    if (!ReadJsonBoolFieldLocal(text, "has_ellipse_gauge", hasEllipseGauge) ||
        !hasEllipseGauge || !ReadJsonIntFieldLocal(text, "ellipse_x0", x0) ||
        !ReadJsonIntFieldLocal(text, "ellipse_y0", y0) ||
        !ReadJsonIntFieldLocal(text, "ellipse_x1", x1) ||
        !ReadJsonIntFieldLocal(text, "ellipse_y1", y1)) {
      reason = "FindEllipse gauge fields are incomplete";
      return false;
    }
    if (x0 == x1 || y0 == y1) {
      reason = "ellipse gauge radius is zero";
      return false;
    }
    reason.clear();
    return true;
  }

  reason.clear();
  return true;
}

static void
SyncFindEllipseRuntimeGlobalsToManualContextLocal(ManualTestContext &context);

static bool
ApplyCandidateRuntimeGlobalsLocal(ManualTestContext &context,
                                  const std::string &runtimeGlobalsPath,
                                  std::string &reason) {
  reason.clear();
  std::string text;
  if (!ReadTextFile(runtimeGlobalsPath, text)) {
    reason = "failed to read candidate runtime globals: " + runtimeGlobalsPath;
    return false;
  }

  const std::size_t globalsKey = text.find("\"globals\"");
  const std::size_t objectBegin = globalsKey == std::string::npos
                                      ? std::string::npos
                                      : text.find('{', globalsKey);
  const std::size_t objectEnd = objectBegin == std::string::npos
                                    ? std::string::npos
                                    : text.find('}', objectBegin);
  if (objectBegin == std::string::npos || objectEnd == std::string::npos) {
    reason = "candidate runtime_globals.json has no globals object";
    return false;
  }

  std::size_t pos = objectBegin + 1;
  int applied = 0;
  while (pos < objectEnd) {
    const std::size_t keyBegin = text.find('"', pos);
    if (keyBegin == std::string::npos || keyBegin >= objectEnd)
      break;
    const std::size_t keyEnd = text.find('"', keyBegin + 1);
    if (keyEnd == std::string::npos || keyEnd >= objectEnd)
      break;
    const std::string key = text.substr(keyBegin + 1, keyEnd - keyBegin - 1);
    const std::size_t colon = text.find(':', keyEnd + 1);
    if (colon == std::string::npos || colon >= objectEnd)
      break;

    std::size_t valueBegin = colon + 1;
    while (valueBegin < objectEnd &&
           std::isspace(static_cast<unsigned char>(text[valueBegin])))
      ++valueBegin;
    std::size_t valueEnd = valueBegin;
    if (valueEnd < objectEnd &&
        (text[valueEnd] == '-' || text[valueEnd] == '+'))
      ++valueEnd;
    while (valueEnd < objectEnd &&
           std::isdigit(static_cast<unsigned char>(text[valueEnd])))
      ++valueEnd;

    if (key.rfind("global_", 0) == 0 && valueEnd > valueBegin) {
      try {
        context.runtime_int_vars[key] =
            std::stoi(text.substr(valueBegin, valueEnd - valueBegin));
        ++applied;
      } catch (...) {
        reason = "invalid candidate runtime global: " + key;
        return false;
      }
    }
    pos = valueEnd > valueBegin ? valueEnd : colon + 1;
  }

  if (applied == 0) {
    reason = "candidate runtime_globals.json contains no global_* values";
    return false;
  }

  auto getRuntimeInt = [&](const std::string &key, int fallback) -> int {
    const auto found = context.runtime_int_vars.find(key);
    return found == context.runtime_int_vars.end() ? fallback : found->second;
  };

  context.findline_scan_edge_count =
      std::max(1, std::min(16, getRuntimeInt("global_findline_edge_count", 4)));
  context.findline_selected_scan_edge =
      std::max(-1, std::min(getRuntimeInt("global_findline_selected_edge", 0),
                            context.findline_scan_edge_count));
  context.findline_best_fit_edge =
      std::max(0, std::min(getRuntimeInt("global_findline_best_edge", 0),
                           context.findline_scan_edge_count));
  context.findline_recommended_fit_edge =
      std::max(0, std::min(getRuntimeInt("global_findline_recommended_edge", 0),
                           context.findline_scan_edge_count));
  context.findline_relation_edge =
      std::max(0, std::min(getRuntimeInt("global_findline_relation_edge", 0),
                           context.findline_scan_edge_count));
  context.findline_attach_edge =
      std::max(0, std::min(getRuntimeInt("global_findline_attach_edge", 0),
                           context.findline_scan_edge_count));
  context.findline_edge_params.resize(
      static_cast<std::size_t>(context.findline_scan_edge_count + 1));
  for (int edge = 1; edge <= context.findline_scan_edge_count; ++edge) {
    ManualFindLineEdgeParamState &params =
        context.findline_edge_params[static_cast<std::size_t>(edge)];
    const std::string prefix =
        "global_findline_edge" + std::to_string(edge) + "_";
    params.initialized = true;
    params.threshold = getRuntimeInt(prefix + "threshold",
                                     getRuntimeInt("global_threshold", 20));
    params.method =
        getRuntimeInt(prefix + "method", getRuntimeInt("global_method", 0));
    params.linegap =
        getRuntimeInt(prefix + "linegap", getRuntimeInt("global_linegap", 6));
    params.wgap =
        getRuntimeInt(prefix + "wgap", getRuntimeInt("global_wgap", 8));
    params.hgap =
        getRuntimeInt(prefix + "hgap", getRuntimeInt("global_hgap", 32));
    params.filterprofile = getRuntimeInt(
        prefix + "filterprofile", getRuntimeInt("global_filterprofile", 0));
  }

  context.findcircle_scan_edge_count = std::max(
      1, std::min(32, getRuntimeInt("global_findcircle_edge_count", 4)));
  context.findcircle_selected_scan_edge =
      std::max(-1, std::min(getRuntimeInt("global_findcircle_selected_edge", 0),
                            context.findcircle_scan_edge_count));
  context.findcircle_best_fit_edge =
      std::max(0, std::min(getRuntimeInt("global_findcircle_best_edge", 0),
                           context.findcircle_scan_edge_count));
  context.findcircle_recommended_fit_edge = std::max(
      0, std::min(getRuntimeInt("global_findcircle_recommended_edge", 0),
                  context.findcircle_scan_edge_count));
  context.findcircle_relation_edge =
      std::max(0, std::min(getRuntimeInt("global_findcircle_relation_edge", 0),
                           context.findcircle_scan_edge_count));
  context.findcircle_attach_edge =
      std::max(0, std::min(getRuntimeInt("global_findcircle_attach_edge", 0),
                           context.findcircle_scan_edge_count));
  context.findcircle_edge_params.resize(
      static_cast<std::size_t>(context.findcircle_scan_edge_count + 1));
  const int sharedCircleThreshold = getRuntimeInt("global_threshold", 20);
  const int sharedCircleMethod = getRuntimeInt("global_method", 0);
  const int sharedCircleLinegap = getRuntimeInt("global_linegap", 3);
  const int sharedCircleGap = getRuntimeInt("global_gap", 6);
  for (int edge = 1; edge <= context.findcircle_scan_edge_count; ++edge) {
    ManualFindCircleEdgeParamState &params =
        context.findcircle_edge_params[static_cast<std::size_t>(edge)];
    params.initialized = true;
    params.threshold = sharedCircleThreshold;
    params.method = sharedCircleMethod;
    params.linegap = sharedCircleLinegap;
    params.gap = sharedCircleGap;
  }
  SyncFindEllipseRuntimeGlobalsToManualContextLocal(context);
  return true;
}

static std::string
ResolveEvidencePacketPathFromSummaryLocal(const std::string &runtimeSummary) {
  if (runtimeSummary.empty() || runtimeSummary == "-")
    return {};

  try {
    std::filesystem::path path(runtimeSummary);
    path = path.parent_path() / "evidence_packet.json";
    return path.string();
  } catch (...) {
    return {};
  }
}

static bool IsDerivedEvidenceRenderPathLocal(const std::string &value) {
  if (value.empty())
    return false;
  std::string name = std::filesystem::path(value).filename().string();
  std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return name == "evidence_overlay.png" || name == "result_overlay.png" ||
         name == "tool_display.png" || name == "roi_preview.png";
}

static std::string ResolveRunnableImagePathLocal(
    const std::filesystem::path &metadataPath, const std::string &value) {
  if (value.empty() || IsDerivedEvidenceRenderPathLocal(value))
    return {};
  std::filesystem::path path(value);
  if (path.is_relative()) {
    const std::filesystem::path relativeToMetadata =
        metadataPath.parent_path() / path;
    std::error_code ec;
    if (std::filesystem::is_regular_file(relativeToMetadata, ec))
      path = relativeToMetadata;
    else
      path = ResolveWorkspaceFile(path.string());
  }
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec) ||
      IsDerivedEvidenceRenderPathLocal(path.string()))
    return {};
  return path.lexically_normal().string();
}

static std::string ResolveOriginalImageFromRunDirectoryLocal(
    const std::filesystem::path &directory) {
  const std::filesystem::path semanticInput =
      directory / "measurement_semantic_input.json";
  std::string text;
  if (ReadTextFile(semanticInput.string(), text)) {
    const std::string resolved = ResolveRunnableImagePathLocal(
        semanticInput, ReadJsonStringFieldLocal(text, "image_path"));
    if (!resolved.empty())
      return resolved;
  }

  const std::filesystem::path snapshot = directory / "snapshot.txt";
  if (ReadTextFile(snapshot.string(), text)) {
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
      const std::string prefix = "image:";
      if (line.rfind(prefix, 0) != 0)
        continue;
      const std::string resolved = ResolveRunnableImagePathLocal(
          snapshot, TrimLine(line.substr(prefix.size())));
      if (!resolved.empty())
        return resolved;
    }
  }
  return {};
}

static std::string ResolveOriginalImagePathFromEvidencePacketLocal(
    const std::string &runtimeSummary) {
  const std::string evidencePacket =
      ResolveEvidencePacketPathFromSummaryLocal(runtimeSummary);
  if (evidencePacket.empty())
    return {};

  std::string text;
  if (ReadTextFile(evidencePacket, text)) {
    const std::string resolved = ResolveRunnableImagePathLocal(
        evidencePacket, ReadJsonStringFieldLocal(text, "path"));
    if (!resolved.empty())
      return resolved;
  }

  return ResolveOriginalImageFromRunDirectoryLocal(
      std::filesystem::path(runtimeSummary).parent_path());
}

static void ResolvePrimaryEditableObjectLocal(
    const std::string &tool, const std::string &targetId,
    const std::string &parameterSummary,
    const std::vector<CxEvidenceEditableObjectRef> &objects,
    std::string &outType, std::string &outName, std::string &outStatus) {
  outType.clear();
  outName.clear();
  outStatus = "none";

  if (objects.empty()) {
    outStatus = "no_editable_object_declared";
    return;
  }

  std::string explicitName = ReadKeyValueFromEvidenceParamSummaryLocal(
      parameterSummary, "primary_object_ref");
  if (explicitName.empty())
    explicitName = ReadKeyValueFromEvidenceParamSummaryLocal(parameterSummary,
                                                             "primary_object");
  if (explicitName.empty())
    explicitName = ReadKeyValueFromEvidenceParamSummaryLocal(parameterSummary,
                                                             "object_ref");

  auto bindObject = [&](const CxEvidenceEditableObjectRef &ref,
                        const char *status) {
    outType = ref.type;
    outName = ref.name;
    outStatus = status;
  };

  if (!explicitName.empty()) {
    for (const auto &ref : objects) {
      if (ref.name == explicitName) {
        bindObject(ref, "explicit_object_ref");
        return;
      }
    }
    outStatus = "explicit_object_ref_not_found";
    return;
  }

  if (!targetId.empty() && targetId != "-") {
    for (const auto &ref : objects) {
      if (targetId.find(ref.name) != std::string::npos ||
          ref.name.find(targetId) != std::string::npos) {
        bindObject(ref, "target_id_matched_object");
        return;
      }
    }
  }

  const std::string normalizedTool = NormalizeEvidenceToolTypeLocal(tool);
  std::vector<const CxEvidenceEditableObjectRef *> toolMatches;
  for (const auto &ref : objects) {
    if (ref.type == normalizedTool)
      toolMatches.push_back(&ref);
  }

  if (toolMatches.size() == 1) {
    bindObject(*toolMatches.front(), "single_matching_tool_object");
    return;
  }

  if (objects.size() == 1) {
    bindObject(objects.front(), "single_editable_object");
    return;
  }

  outStatus = toolMatches.size() > 1 ? "needs_object_selection"
                                     : "needs_object_selection_no_tool_match";
}

static bool EvidenceSnapshotHasLockedParamSummaryLocal(
    const CxEvidenceSelectionSnapshot &snapshot, std::string &reason) {
  if (!snapshot.valid) {
    reason = "invalid evidence snapshot";
    return false;
  }
  if (snapshot.parameter_summary.empty() || snapshot.parameter_summary == "-") {
    reason = "evidence parameter summary is empty";
    return false;
  }
  if (snapshot.parameter_summary.find('=') == std::string::npos) {
    reason = "evidence parameter summary is not key=value locked data: " +
             snapshot.parameter_summary;
    return false;
  }
  reason.clear();
  return true;
}

static bool EvidenceParamSummaryHasKeyLocal(const std::string &summary,
                                            const std::string &key) {
  std::istringstream iss(summary);
  std::string token;
  const std::string needle = key + "=";
  while (iss >> token) {
    if (!token.empty() && token.back() == ';')
      token.pop_back();
    if (token.rfind(needle, 0) == 0)
      return true;
  }
  return false;
}

static bool
EvidenceSnapshotLooksLikeToolLocal(const CxEvidenceSelectionSnapshot &snapshot,
                                   const std::string &normalizedTool) {
  if (NormalizeEvidenceToolTypeLocal(snapshot.tool) == normalizedTool)
    return true;

  std::string haystack = snapshot.script_id + " " + snapshot.script_path + " " +
                         snapshot.source_evidence_script_path;
  std::transform(
      haystack.begin(), haystack.end(), haystack.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

  if (normalizedTool == "FindEllipse") {
    return haystack.find("findellipse") != std::string::npos ||
           haystack.find("find_ellipse") != std::string::npos;
  }
  return false;
}

static bool MigrateLegacyEvidenceSelectionSnapshotLocal(
    CxEvidenceSelectionSnapshot &snapshot) {
  const bool looksLikeSavedCandidate =
      snapshot.is_candidate || snapshot.has_saved_state ||
      !snapshot.candidate_id.empty() || !snapshot.candidate_dir.empty() ||
      !snapshot.runtime_globals_path.empty() ||
      !snapshot.gauge_annotation_path.empty();
  if (!looksLikeSavedCandidate)
    return false;

  if (!EvidenceSnapshotLooksLikeToolLocal(snapshot, "FindEllipse"))
    return false;

  if (EvidenceParamSummaryHasKeyLocal(snapshot.parameter_summary,
                                      "ellipse_inner_scale_percent") ||
      EvidenceParamSummaryHasKeyLocal(snapshot.parameter_summary,
                                      "inner_scale_percent")) {
    return false;
  }

  if (snapshot.parameter_summary.empty() || snapshot.parameter_summary == "-") {
    snapshot.parameter_summary = "ellipse_inner_scale_percent=0";
  } else {
    snapshot.parameter_summary += "; ellipse_inner_scale_percent=0";
  }
  snapshot.parameter_profile_id = snapshot.parameter_summary;
  if (!snapshot.reason.empty())
    snapshot.reason += " | ";
  snapshot.reason += "migrated legacy FindEllipse candidate default "
                     "ellipse_inner_scale_percent=0";
  return true;
}

static void
SyncFindEllipseRuntimeGlobalsToManualContextLocal(ManualTestContext &context) {
  auto getRuntimeInt = [&](const std::string &key, int fallback) -> int {
    const auto found = context.runtime_int_vars.find(key);
    return found == context.runtime_int_vars.end() ? fallback : found->second;
  };

  context.findellipse_scan_edge_count = std::max(
      1, std::min(3, getRuntimeInt("global_findellipse_edge_count", 3)));
  context.findellipse_selected_scan_edge = std::max(
      -1, std::min(getRuntimeInt("global_findellipse_selected_edge", 0),
                   context.findellipse_scan_edge_count));
  context.findellipse_best_fit_edge =
      std::max(0, std::min(getRuntimeInt("global_findellipse_best_edge", 0),
                           context.findellipse_scan_edge_count));
  context.findellipse_recommended_fit_edge = std::max(
      0, std::min(getRuntimeInt("global_findellipse_recommended_edge", 0),
                  context.findellipse_scan_edge_count));
  context.findellipse_relation_edge =
      std::max(0, std::min(getRuntimeInt("global_findellipse_relation_edge", 0),
                           context.findellipse_scan_edge_count));
  context.findellipse_attach_edge =
      std::max(0, std::min(getRuntimeInt("global_findellipse_attach_edge", 0),
                           context.findellipse_scan_edge_count));
  context.findellipse_point_consistency_enabled =
      getRuntimeInt("global_findellipse_point_consistency_enabled", 0) != 0;
  context.findellipse_point_consistency_range = std::max(
      0, getRuntimeInt("global_findellipse_point_consistency_range", 0));

  context.findellipse_edge_params.clear();
  context.findellipse_edge_params.resize(
      static_cast<std::size_t>(context.findellipse_scan_edge_count + 1));
  const int sharedThreshold = getRuntimeInt("global_threshold", 8);
  const int sharedMethod = getRuntimeInt("global_method", 1);
  const int sharedLinegap = getRuntimeInt("global_linegap", 3);
  const int sharedGap = getRuntimeInt("global_gap", 5);
  for (int edge = 1; edge <= context.findellipse_scan_edge_count; ++edge) {
    ManualFindCircleEdgeParamState &params =
        context.findellipse_edge_params[static_cast<std::size_t>(edge)];
    const std::string prefix =
        "global_findellipse_edge" + std::to_string(edge) + "_";
    params.initialized = true;
    params.threshold = getRuntimeInt(prefix + "threshold", sharedThreshold);
    params.method = getRuntimeInt(prefix + "method", sharedMethod);
    params.linegap = getRuntimeInt(prefix + "linegap", sharedLinegap);
    params.gap = getRuntimeInt(prefix + "gap", sharedGap);
  }
}

static void SyncEvidenceLockedGlobalsToManualGaugeLocal(
    ManualTestContext &context, const std::string &scriptPath,
    const std::string &source, const std::string &primaryObjectType,
    const std::string &primaryObjectName,
    const std::string &primaryObjectStatus) {
  auto getInt = [&](const std::string &key, int fallback) -> int {
    const auto it = context.runtime_int_vars.find(key);
    return it == context.runtime_int_vars.end() ? fallback : it->second;
  };

  const bool isCircleScript =
      scriptPath.find("find_circle") != std::string::npos ||
      scriptPath.find("findcircle") != std::string::npos ||
      scriptPath.find("FindCircle") != std::string::npos;
  const bool isLineScript = scriptPath.find("find_line") != std::string::npos ||
                            scriptPath.find("findline") != std::string::npos ||
                            scriptPath.find("FindLine") != std::string::npos;
  const bool isEllipseScript =
      scriptPath.find("find_ellipse") != std::string::npos ||
      scriptPath.find("findellipse") != std::string::npos ||
      scriptPath.find("FindEllipse") != std::string::npos;
  const bool isFindObjectScript =
      scriptPath.find("find_object") != std::string::npos ||
      scriptPath.find("findobject") != std::string::npos ||
      scriptPath.find("FindObject") != std::string::npos;
  const bool isFindSegmentationScript =
      scriptPath.find("find_segmentation") != std::string::npos ||
      scriptPath.find("findsegmentation") != std::string::npos ||
      scriptPath.find("FindSegmentation") != std::string::npos;
  const bool isFastMatchScript =
      scriptPath.find("fastmatch") != std::string::npos ||
      scriptPath.find("FastMatch") != std::string::npos ||
      scriptPath.find("CFastMatch") != std::string::npos;

  ManualGaugeState gauge;
  gauge.case_id = context.active_case_id;
  gauge.image_id = context.active_image_id;
  gauge.target_id = context.active_target_id;
  gauge.source = source;
  gauge.primary_object_type = primaryObjectType;
  gauge.primary_object_name = primaryObjectName;
  gauge.primary_object_status = primaryObjectStatus;
  gauge.review_status = "editing";
  gauge.threshold = getInt("global_threshold", 20);
  gauge.method = getInt("global_method", 0);
  gauge.linegap = getInt("global_linegap", 3);
  gauge.wgap = getInt("global_wgap", 32);
  gauge.hgap = getInt("global_hgap", 8);
  gauge.gap = getInt("global_gap", 5);
  gauge.tool_half_width = getInt("global_tool_half_width", 32);
  gauge.filterprofile = getInt("global_filterprofile", 1);

  const std::string primaryType =
      NormalizeEvidenceToolTypeLocal(primaryObjectType);

  if (primaryType == "FindCircle" || (primaryType.empty() && isCircleScript)) {
    gauge.tool = "FindCircle";
    gauge.has_circle_gauge = true;
    gauge.circle_cx = getInt("global_circle_cx", 0);
    gauge.circle_cy = getInt("global_circle_cy", 0);
    gauge.circle_px = getInt("global_circle_px", gauge.circle_cx);
    gauge.circle_py = getInt("global_circle_py", gauge.circle_cy);
    gauge.radius = static_cast<int>(std::lround(
        std::hypot(static_cast<double>(gauge.circle_px - gauge.circle_cx),
                   static_cast<double>(gauge.circle_py - gauge.circle_cy))));
    gauge.inner_radius = getInt("global_circle_inner_radius", 0);
    gauge.outer_radius = getInt("global_circle_outer_radius", gauge.radius);
    if (gauge.outer_radius <= 0)
      gauge.outer_radius = gauge.radius;
  } else if (primaryType == "FindEllipse" ||
             (primaryType.empty() && isEllipseScript)) {
    gauge.tool = "FindEllipse";
    gauge.has_ellipse_gauge = true;
    gauge.ellipse_x0 = getInt("global_ellipse_x0", 0);
    gauge.ellipse_y0 = getInt("global_ellipse_y0", 0);
    gauge.ellipse_x1 = getInt("global_ellipse_x1", 0);
    gauge.ellipse_y1 = getInt("global_ellipse_y1", 0);
    gauge.ellipse_inner_scale_percent = std::max(
        0, std::min(99, getInt("global_findellipse_inner_scale_percent",
                               getInt("ellipse_inner_scale_percent",
                                      getInt("inner_scale_percent", 0)))));
    gauge.findsetting = getInt("global_findellipse_findsetting",
                               getInt("global_findsetting", gauge.findsetting));
    SyncFindEllipseRuntimeGlobalsToManualContextLocal(context);

  } else if (primaryType == "FindLine" ||
             (primaryType.empty() && isLineScript)) {
    gauge.tool = "FindLine";
    gauge.has_line_gauge = true;
    gauge.line_x0 = getInt("global_roi_x0", 0);
    gauge.line_y0 = getInt("global_roi_y0", 0);
    gauge.line_x1 = getInt("global_roi_x1", 0);
    gauge.line_y1 = getInt("global_roi_y1", 0);
  } else if (primaryType == "FindObject" ||
             (primaryType.empty() && isFindObjectScript)) {
    gauge.tool = "FindObject";
    gauge.primary_object_type = "FindObject";
    if (gauge.primary_object_name.empty())
      gauge.primary_object_name =
          primaryObjectName.empty() ? "m_object" : primaryObjectName;
    gauge.has_findobject_roi = true;
    gauge.findobject_x0 = getInt("global_roi_x0", getInt("global_roi_x", 0));
    gauge.findobject_y0 = getInt("global_roi_y0", getInt("global_roi_y", 0));
    const int roiW = getInt("global_roi_width", 0);
    const int roiH = getInt("global_roi_height", 0);
    gauge.findobject_x1 = getInt("global_roi_x1", gauge.findobject_x0 + roiW);
    gauge.findobject_y1 = getInt("global_roi_y1", gauge.findobject_y0 + roiH);
    gauge.findobject_foreground_mode = getInt("global_method", gauge.method);
    gauge.findobject_threshold = getInt("global_threshold", gauge.threshold);
    gauge.findobject_min_area = getInt("global_object_min_area", 10);
  } else if (primaryType == "FindSegmentation" ||
             (primaryType.empty() && isFindSegmentationScript)) {
    gauge.tool = "FindSegmentation";
    gauge.primary_object_type = "FindSegmentation";
    if (gauge.primary_object_name.empty())
      gauge.primary_object_name =
          primaryObjectName.empty() ? "m_seg" : primaryObjectName;
    if (gauge.primary_object_status.empty())
      gauge.primary_object_status = "evidence_findsegmentation_selected";
    gauge.has_segmentation_prompt_rect = true;
    gauge.segmentation_prompt_x0 =
        getInt("global_roi_x0", getInt("global_roi_x", 120));
    gauge.segmentation_prompt_y0 =
        getInt("global_roi_y0", getInt("global_roi_y", 120));
    const int roiW = getInt("global_roi_width", 860);
    const int roiH = getInt("global_roi_height", 700);
    gauge.segmentation_prompt_x1 =
        getInt("global_roi_x1", gauge.segmentation_prompt_x0 + roiW);
    gauge.segmentation_prompt_y1 =
        getInt("global_roi_y1", gauge.segmentation_prompt_y0 + roiH);
    gauge.segmentation_mode = getInt("global_segmentation_mode", gauge.method);
    gauge.segmentation_threshold_percent =
        getInt("global_segmentation_threshold_percent",
               gauge.segmentation_threshold_percent);
    gauge.has_segmentation_positive_point =
        getInt("global_segmentation_positive_enabled", 0) != 0;
    gauge.segmentation_positive_x =
        getInt("global_segmentation_positive_x", gauge.segmentation_positive_x);
    gauge.segmentation_positive_y =
        getInt("global_segmentation_positive_y", gauge.segmentation_positive_y);
    gauge.has_segmentation_negative_point =
        getInt("global_segmentation_negative_enabled", 0) != 0;
    gauge.segmentation_negative_x =
        getInt("global_segmentation_negative_x", gauge.segmentation_negative_x);
    gauge.segmentation_negative_y =
        getInt("global_segmentation_negative_y", gauge.segmentation_negative_y);
  } else if (primaryType == "FastMatch" ||
             (primaryType.empty() && isFastMatchScript)) {
    gauge.tool = "FastMatch";
    gauge.primary_object_type = "FastMatch";
    if (gauge.primary_object_name.empty())
      gauge.primary_object_name = "m_match";
    if (gauge.primary_object_status.empty())
      gauge.primary_object_status = "evidence_fastmatch_selected";
  }

  if (gauge.has_circle_gauge || gauge.has_line_gauge ||
      gauge.has_ellipse_gauge || gauge.has_findobject_roi ||
      gauge.has_segmentation_prompt_rect || gauge.tool == "FastMatch") {
    context.current_gauge = gauge;
  }
}

static std::vector<std::string>
BuildEvidenceFallbackImageCandidates(const ManualTestContext &context) {
  std::vector<std::string> candidates;

  auto addCandidate = [&](const std::string &path) {
    if (path.empty())
      return;
    if (!std::filesystem::exists(path))
      return;
    if (std::find(candidates.begin(), candidates.end(), path) !=
        candidates.end())
      return;
    candidates.push_back(path);
  };

  for (const auto &item : context.image_manifest_items) {
    addCandidate(item.image_path);
  }

  const std::filesystem::path testImageRoot =
      ResolveCxVisionRunPath("test_images");
  std::error_code ec;
  if (std::filesystem::is_directory(testImageRoot, ec)) {
    std::filesystem::recursive_directory_iterator it(
        testImageRoot,
        std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::recursive_directory_iterator end;
    for (; !ec && it != end; it.increment(ec)) {
      if (!it->is_regular_file(ec))
        continue;
      std::string ext = it->path().extension().string();
      std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
      });
      if (ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp" ||
          ext == ".tif" || ext == ".tiff") {
        addCandidate(it->path().string());
        if (candidates.size() >= 64)
          break;
      }
    }
  }

  std::stable_sort(candidates.begin(), candidates.end());
  return candidates;
}

static bool IsDeprecatedCxScriptPath(const std::string &path) {
  return path.find("/deprecated/") != std::string::npos ||
         path.find("\\deprecated\\") != std::string::npos;
}

static bool IsAllowedEvidenceFallbackScript(const std::string &path) {
  if (path.empty())
    return false;

  if (IsDeprecatedCxScriptPath(path))
    return false;

  return path.find("/headless/") != std::string::npos ||
         path.find("\\headless\\") != std::string::npos ||
         path.find("/frozen/") != std::string::npos ||
         path.find("\\frozen\\") != std::string::npos ||
         path.find("/diagnostic/") != std::string::npos ||
         path.find("\\diagnostic\\") != std::string::npos ||
         path.find("_direct") != std::string::npos ||
         path.find("_smoke") != std::string::npos;
}

static void AssignFallbackImageToThumb(
    ScriptEvidenceThumb &thumb, const std::vector<std::string> &candidates,
    std::unordered_map<std::string, std::size_t> &nextIndexByPool) {
  if (!thumb.image_path.empty())
    return;

  if (candidates.empty())
    return;

  auto toLower = [](std::string value) -> std::string {
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
  };

  const std::string identity =
      toLower(thumb.tool + " " + thumb.script_id + " " + thumb.script_path);
  std::string poolKey = "other";

  std::vector<const std::string *> preferred;
  auto collectPreferred = [&](const char *token) {
    for (const std::string &candidate : candidates) {
      const std::string lowerCandidate = toLower(candidate);
      if (lowerCandidate.find(token) != std::string::npos)
        preferred.push_back(&candidate);
    }
  };

  if (identity.find("findellipse") != std::string::npos ||
      identity.find("find_ellipse") != std::string::npos) {
    poolKey = "ellipse";
    collectPreferred("ellipse");
  } else if (identity.find("findcircle") != std::string::npos ||
             identity.find("find_circle") != std::string::npos) {
    poolKey = "circle";
    collectPreferred("circle");
  } else if (identity.find("findline") != std::string::npos ||
             identity.find("find_line") != std::string::npos) {
    poolKey = "line";
    collectPreferred("line");
  } else if (identity.find("findrect") != std::string::npos ||
             identity.find("find_rect") != std::string::npos) {
    poolKey = "rect";
    collectPreferred("rect");
  } else if (identity.find("fastmatch") != std::string::npos) {
    poolKey = "fastmatch";
    collectPreferred("match");
  }

  std::stable_sort(preferred.begin(), preferred.end(),
                   [](const std::string *left, const std::string *right) {
                     return *left < *right;
                   });

  const std::size_t index = nextIndexByPool[poolKey]++;
  const std::string &path = preferred.empty()
                                ? candidates[index % candidates.size()]
                                : *preferred[index % preferred.size()];
  thumb.image_path = path;
  thumb.thumbnail_path = path;

  if (thumb.image_id.empty())
    thumb.image_id = std::filesystem::path(path).stem().string();

  if (thumb.reason.empty())
    thumb.reason = "fallback image bound for evidence placeholder";
}

static std::string
ResolveEvidenceImagePathFromContextLocal(const ManualTestContext &context,
                                         const std::string &imageId) {
  if (imageId.empty())
    return {};

  for (const auto &item : context.evidence_items) {
    if (item.image_id == imageId && !item.image_path.empty())
      return item.image_path;
  }

  for (const auto &item : context.image_manifest_items) {
    if (item.image_id == imageId && !item.image_path.empty())
      return item.image_path;
  }

  if (imageId.rfind("fallback_image_", 0) == 0) {
    const std::string suffix =
        imageId.substr(std::string("fallback_image_").size());
    char *endPtr = nullptr;
    const long parsed = std::strtol(suffix.c_str(), &endPtr, 10);
    if (endPtr != suffix.c_str() && endPtr != nullptr && *endPtr == '\0' &&
        parsed >= 0) {
      const std::vector<std::string> candidates =
          BuildEvidenceFallbackImageCandidates(context);
      if (!candidates.empty()) {
        return candidates[static_cast<std::size_t>(parsed) % candidates.size()];
      }
    }
  }

  return ResolveEvidenceImagePathByIdFromDiskLocal(imageId);
}

static std::string
BuildDefaultEvidenceParamSummaryForScript(const std::string &scriptPath) {
  ManualTestContext temp;
  SeedDefaultManualGlobals(temp, scriptPath);

  auto getInt = [&](const std::string &key, int fallback) -> int {
    auto it = temp.runtime_int_vars.find(key);
    return it == temp.runtime_int_vars.end() ? fallback : it->second;
  };

  std::ostringstream oss;
  oss << "method=" << getInt("global_method", 0)
      << " threshold=" << getInt("global_threshold", 20)
      << " wgap=" << getInt("global_wgap", 0)
      << " hgap=" << getInt("global_hgap", 0)
      << " gap=" << getInt("global_gap", 0)
      << " linegap=" << getInt("global_linegap", 0)
      << " filterprofile=" << getInt("global_filterprofile", 1)
      << " tool_half_width=" << getInt("global_tool_half_width", 0)
      << " roi_x0=" << getInt("global_roi_x0", 0)
      << " roi_y0=" << getInt("global_roi_y0", 0)
      << " roi_x1=" << getInt("global_roi_x1", 0)
      << " roi_y1=" << getInt("global_roi_y1", 0)
      << " roi_x=" << getInt("global_roi_x", 0)
      << " roi_y=" << getInt("global_roi_y", 0)
      << " roi_width=" << getInt("global_roi_width", 0)
      << " roi_height=" << getInt("global_roi_height", 0)
      << " circle_cx=" << getInt("global_circle_cx", 0)
      << " circle_cy=" << getInt("global_circle_cy", 0)
      << " circle_px=" << getInt("global_circle_px", 0)
      << " circle_py=" << getInt("global_circle_py", 0)
      << " circle_inner_radius=" << getInt("global_circle_inner_radius", 0)
      << " circle_outer_radius=" << getInt("global_circle_outer_radius", 0)
      << " circle_ring_width=" << getInt("global_circle_ring_width", 0)
      << " ellipse_x0=" << getInt("global_ellipse_x0", 0)
      << " ellipse_y0=" << getInt("global_ellipse_y0", 0)
      << " ellipse_x1=" << getInt("global_ellipse_x1", 0)
      << " ellipse_y1=" << getInt("global_ellipse_y1", 0)
      << " learn_roi_x=" << getInt("global_learn_roi_x", 120)
      << " learn_roi_y=" << getInt("global_learn_roi_y", 120)
      << " learn_roi_w=" << getInt("global_learn_roi_w", 120)
      << " learn_roi_h=" << getInt("global_learn_roi_h", 90)
      << " search_roi_x=" << getInt("global_search_roi_x", 60)
      << " search_roi_y=" << getInt("global_search_roi_y", 60)
      << " search_roi_w=" << getInt("global_search_roi_w", 640)
      << " search_roi_h=" << getInt("global_search_roi_h", 480)
      << " compare_gap=" << getInt("global_compare_gap", 0)
      << " objfilter=" << getInt("global_objfilter", 0)
      << " find_num=" << getInt("global_find_num", 1)
      << " grid_normalized_width=" << getInt("global_grid_normalized_width", 48)
      << " grid_normalized_height="
      << getInt("global_grid_normalized_height", 48)
      << " grid_rows=" << getInt("global_grid_rows", 12)
      << " grid_cols=" << getInt("global_grid_cols", 12)
      << " grid_levels=" << getInt("global_grid_levels", 3)
      << " grid_orientation_bins=" << getInt("global_grid_orientation_bins", 8)
      << " grid_foreground_threshold="
      << getInt("global_grid_foreground_threshold", -1)
      << " grid_active_foreground_percent="
      << getInt("global_grid_active_foreground_percent", 5)
      << " grid_active_edge_percent="
      << getInt("global_grid_active_edge_percent", 3)
      << " grid_max_overlays=" << getInt("global_grid_max_overlays", 96)
      << " grid_fusion_mode=" << getInt("global_grid_fusion_mode", 2)
      << " region_roi_x=" << getInt("global_region_roi_x", 120)
      << " region_roi_y=" << getInt("global_region_roi_y", 120)
      << " region_roi_w=" << getInt("global_region_roi_w", 120)
      << " region_roi_h=" << getInt("global_region_roi_h", 90)
      << " region_normalized_width="
      << getInt("global_region_normalized_width", 32)
      << " region_normalized_height="
      << getInt("global_region_normalized_height", 32)
      << " region_pooling_rows=" << getInt("global_region_pooling_rows", 4)
      << " region_pooling_cols=" << getInt("global_region_pooling_cols", 4)
      << " region_use_binary=" << getInt("global_region_use_binary", 0)
      << " region_threshold=" << getInt("global_region_threshold", 128)
      << " region_foreground_dark="
      << getInt("global_region_foreground_dark", 1)
      << " region_max_overlays=" << getInt("global_region_max_overlays", 64)
      << " max_elapsed_ms=" << getInt("global_max_elapsed_ms", 2000)
      << " max_scan_lines=" << getInt("global_max_scan_lines", 2000)
      << " max_samples=" << getInt("global_max_samples", 200000);

  return oss.str();
}

static void
PopulateEditableObjectBindingForThumbLocal(ScriptEvidenceThumb &thumb) {
  thumb.primary_object_type.clear();
  thumb.primary_object_name.clear();
  thumb.primary_object_status.clear();

  if (thumb.script_path.empty()) {
    thumb.primary_object_status = "script_path_empty";
    return;
  }

  std::string scriptText;
  if (!ReadTextFile(thumb.script_path, scriptText)) {
    thumb.primary_object_status = "script_read_failed";
    return;
  }

  std::vector<CxEvidenceEditableObjectRef> objects;
  AnalyzeEditableObjectsFromCxScriptLocal(scriptText, objects);
  ResolvePrimaryEditableObjectLocal(
      thumb.tool, thumb.target_id, thumb.parameter_summary, objects,
      thumb.primary_object_type, thumb.primary_object_name,
      thumb.primary_object_status);
}

static std::string StripMarkdownCellDecorLocal(std::string value) {
  value = TrimLine(value);
  if (value.size() >= 2 && value.front() == '`' && value.back() == '`')
    value = value.substr(1, value.size() - 2);
  return TrimLine(value);
}

static std::vector<std::string>
SplitMarkdownTableRowLocal(const std::string &line) {
  std::vector<std::string> cells;
  std::string current;
  bool inBacktick = false;
  for (char ch : line) {
    if (ch == '`')
      inBacktick = !inBacktick;
    if (ch == '|' && !inBacktick) {
      cells.push_back(StripMarkdownCellDecorLocal(current));
      current.clear();
      continue;
    }
    current.push_back(ch);
  }
  cells.push_back(StripMarkdownCellDecorLocal(current));

  if (!cells.empty() && cells.front().empty())
    cells.erase(cells.begin());
  if (!cells.empty() && cells.back().empty())
    cells.pop_back();
  return cells;
}

static std::string BuildManualReviewParamSummaryLocal(
    const std::string &tool, const std::string &failureClass,
    const std::string &runtimeSummary, const std::string &extraEvidence) {
  auto appendInt = [](std::ostringstream &oss, const std::string &text,
                      const std::string &summaryKey,
                      const std::string &paramKey,
                      std::string &missing) -> void {
    int value = 0;
    if (ReadJsonIntFieldLocal(text, summaryKey, value)) {
      oss << " " << paramKey << "=" << value;
      return;
    }

    if (!missing.empty())
      missing += ",";
    missing += summaryKey;
  };

  std::string summaryText;
  ReadTextFile(runtimeSummary, summaryText);

  const std::string normalizedTool = NormalizeEvidenceToolTypeLocal(tool);
  std::string missing;

  std::ostringstream oss;
  oss << "evidence_locked=1"
      << " tool=" << normalizedTool;

  if (normalizedTool == "TorchTask") {
    std::string matrixPath =
        ReadSemicolonFieldLocal(extraEvidence, "stability_matrix");
    if (matrixPath.empty()) {
      std::istringstream fields(extraEvidence);
      std::string candidate;
      while (std::getline(fields, candidate, ';')) {
        candidate = TrimLine(candidate);
        std::string candidateText;
        if (!candidate.empty() && ReadTextFile(candidate, candidateText) &&
            ReadJsonStringFieldLocal(candidateText, "schema")
                    .find("stability_matrix") != std::string::npos) {
          matrixPath = candidate;
          break;
        }
      }
    }

    std::string matrixText;
    ReadTextFile(matrixPath, matrixText);
    std::string stabilityCaseId =
        ReadSemicolonFieldLocal(extraEvidence, "stability_case_id");
    if (stabilityCaseId.empty())
      stabilityCaseId =
          ReadJsonStringFieldLocal(matrixText, "baseline_case_id");
    const std::string matrixRow = ExtractJsonObjectByStringFieldLocal(
        matrixText, "case_id", stabilityCaseId);

    std::string modelManifestPath =
        ReadSemicolonFieldLocal(extraEvidence, "model_manifest");
    if (modelManifestPath.empty())
      modelManifestPath =
          ReadJsonStringFieldLocal(matrixRow, "model_manifest_ref");
    std::string modelManifestText;
    ReadTextFile(modelManifestPath, modelManifestText);

    const std::string task =
        ReadJsonStringFieldLocal(modelManifestText, "task");
    if (!task.empty())
      oss << " torch_feature=" << task;

    auto appendManifestInt = [&](const std::string &jsonKey,
                                 const std::string &paramKey) {
      int value = 0;
      if (ReadJsonIntFieldLocal(modelManifestText, jsonKey, value))
        oss << " " << paramKey << "=" << value;
    };
    appendManifestInt("width", "torch_input_width");
    appendManifestInt("height", "torch_input_height");
    appendManifestInt("max_detections", "torch_max_detections");
    appendManifestInt("num_classes", "torch_num_classes");
    appendManifestInt("sample_count", "torch_training_sample_count");
    appendManifestInt("instance_count", "torch_training_instance_count");

    auto appendPercent = [&](const std::string &summaryValue,
                             const std::string &manifestKey,
                             const std::string &paramKey) {
      double value = 0.0;
      bool available = false;
      if (!summaryValue.empty()) {
        try {
          value = std::stod(summaryValue);
          available = true;
        } catch (...) {
        }
      }
      if (!available)
        available =
            ReadJsonDoubleFieldLocal(modelManifestText, manifestKey, value);
      if (available)
        oss << " " << paramKey << "="
            << static_cast<int>(std::lround(value * 100.0));
    };
    appendPercent(
        ReadSemicolonFieldLocal(extraEvidence, "confidence_threshold"),
        "confidence_threshold", "torch_confidence_percent");
    appendPercent({}, "iou_threshold", "torch_iou_threshold_percent");
    appendPercent({}, "mask_threshold", "torch_mask_threshold_percent");

    auto appendMatrixInt = [&](const std::string &jsonKey,
                               const std::string &paramKey) {
      int value = 0;
      if (ReadJsonIntFieldLocal(matrixRow, jsonKey, value))
        oss << " " << paramKey << "=" << value;
    };
    appendMatrixInt("roi_shift_dx_px", "torch_roi_shift_dx_px");
    appendMatrixInt("roi_shift_dy_px", "torch_roi_shift_dy_px");
    appendMatrixInt("instance_count", "torch_result_count");
    appendMatrixInt("instance_count_delta_from_baseline",
                    "torch_result_count_delta");

    bool trainingStepExecuted = false;
    if (ReadJsonBoolFieldLocal(matrixRow, "training_step_executed",
                               trainingStepExecuted)) {
      oss << " torch_training_step_executed=" << (trainingStepExecuted ? 1 : 0);
    }
    bool inferenceOk = false;
    if (ReadJsonBoolFieldLocal(matrixRow, "inference_ok", inferenceOk))
      oss << " torch_inference_ok=" << (inferenceOk ? 1 : 0);

    const char *lossKeys[] = {"total_loss", "box_loss", "class_loss",
                              "dfl_loss", "mask_loss"};
    for (const char *lossKey : lossKeys) {
      double value = 0.0;
      if (ReadJsonDoubleFieldLocal(matrixRow, lossKey, value))
        oss << " torch_" << lossKey << "=" << value;
    }

    const std::string lossPhase =
        ReadJsonStringFieldLocal(modelManifestText, "loss_phase");
    if (!lossPhase.empty())
      oss << " torch_loss_phase=" << lossPhase;
    if (!matrixPath.empty())
      oss << " torch_stability_matrix=" << matrixPath;
    if (!modelManifestPath.empty())
      oss << " torch_model_manifest=" << modelManifestPath;

    std::string inferenceResult =
        ReadSemicolonFieldLocal(extraEvidence, "inference_result");
    if (inferenceResult.empty())
      inferenceResult =
          ReadJsonStringFieldLocal(matrixRow, "inference_result_ref");
    if (!inferenceResult.empty())
      oss << " torch_result_ref=" << inferenceResult;

    std::string inferenceOverlay =
        ReadSemicolonFieldLocal(extraEvidence, "inference_overlay");
    if (inferenceOverlay.empty())
      inferenceOverlay =
          ReadJsonStringFieldLocal(matrixRow, "inference_overlay_ref");
    if (!inferenceOverlay.empty())
      oss << " torch_overlay_ref=" << inferenceOverlay;
  } else if (normalizedTool == "FindLine") {
    appendInt(oss, summaryText, "roi_x0", "roi_x0", missing);
    appendInt(oss, summaryText, "roi_y0", "roi_y0", missing);
    appendInt(oss, summaryText, "roi_x1", "roi_x1", missing);
    appendInt(oss, summaryText, "roi_y1", "roi_y1", missing);
    appendInt(oss, summaryText, "effective_tool_half_width", "tool_half_width",
              missing);
  } else if (normalizedTool == "FindObject") {
    appendInt(oss, summaryText, "roi_x0", "roi_x0", missing);
    appendInt(oss, summaryText, "roi_y0", "roi_y0", missing);
    appendInt(oss, summaryText, "roi_x1", "roi_x1", missing);
    appendInt(oss, summaryText, "roi_y1", "roi_y1", missing);
  } else if (normalizedTool == "FindCircle") {
    appendInt(oss, summaryText, "circle_cx", "circle_cx", missing);
    appendInt(oss, summaryText, "circle_cy", "circle_cy", missing);
    appendInt(oss, summaryText, "circle_px", "circle_px", missing);
    appendInt(oss, summaryText, "circle_py", "circle_py", missing);
  } else if (normalizedTool == "FindEllipse") {
    appendInt(oss, summaryText, "ellipse_x0", "ellipse_x0", missing);
    appendInt(oss, summaryText, "ellipse_y0", "ellipse_y0", missing);
    appendInt(oss, summaryText, "ellipse_x1", "ellipse_x1", missing);
    appendInt(oss, summaryText, "ellipse_y1", "ellipse_y1", missing);
  } else if (normalizedTool == "FindRect") {
    appendInt(oss, summaryText, "roi_x", "roi_x", missing);
    appendInt(oss, summaryText, "roi_y", "roi_y", missing);
    appendInt(oss, summaryText, "roi_width", "roi_width", missing);
    appendInt(oss, summaryText, "roi_height", "roi_height", missing);
  } else if (normalizedTool == "FastMatch") {
    // A headless result_summary owns the exact inputs that produced its
    // evidence.  Restore those inputs when the case is selected; falling back
    // to SeedDefaultManualGlobals() changes the learn ROI and makes a replay
    // diagnose a different image region.  Keep this list schema-based so case
    // discovery remains entirely asset driven.
    const std::pair<const char *, const char *> fastMatchGlobals[] = {
        {"global_learn_roi_x", "learn_roi_x"},
        {"global_learn_roi_y", "learn_roi_y"},
        {"global_learn_roi_w", "learn_roi_w"},
        {"global_learn_roi_h", "learn_roi_h"},
        {"global_search_roi_x", "search_roi_x"},
        {"global_search_roi_y", "search_roi_y"},
        {"global_search_roi_w", "search_roi_w"},
        {"global_search_roi_h", "search_roi_h"},
        {"global_threshold", "threshold"},
        {"global_method", "method"},
        {"global_wgap", "wgap"},
        {"global_hgap", "hgap"},
        {"global_linegap", "linegap"},
        {"global_compare_gap", "compare_gap"},
        {"global_objfilter", "objfilter"},
        {"global_find_num", "find_num"},
        {"global_fastmatch_action", "fastmatch_action"},
        {"global_fastmatch_scan_rotation_deg",
         "fastmatch_scan_rotation_deg"},
        {"global_fastmatch_learn_shared", "fastmatch_learn_shared"},
        {"global_fastmatch_normaltrace_enabled",
         "fastmatch_normaltrace_enabled"},
        {"global_fastmatch_normaltrace_overlap_radius_px",
         "fastmatch_normaltrace_overlap_radius_px"},
        {"global_fastmatch_normaltrace_xy_compression_bin_px",
         "fastmatch_normaltrace_xy_compression_bin_px"},
        {"global_fastmatch_normaltrace_knn_neighbors",
         "fastmatch_normaltrace_knn_neighbors"},
        {"global_fastmatch_normaltrace_ann_radius_px",
         "fastmatch_normaltrace_ann_radius_px"},
        {"global_fastmatch_normaltrace_ann_min_component_points",
         "fastmatch_normaltrace_ann_min_component_points"},
        {"global_fastmatch_normaltrace_ann_min_component_coverage_percent",
         "fastmatch_normaltrace_ann_min_component_coverage_percent"},
        {"global_fastmatch_normaltrace_pair_offset_px",
         "fastmatch_normaltrace_pair_offset_px"},
        {"global_fastmatch_normaltrace_min_keypoints_per_domain",
         "fastmatch_normaltrace_min_keypoints_per_domain"},
        {"global_fastmatch_normaltrace_min_length_px",
         "fastmatch_normaltrace_min_length_px"},
        {"global_fastmatch_normaltrace_min_gradient",
         "fastmatch_normaltrace_min_gradient"},
        {"global_fastmatch_normaltrace_max_nodes",
         "fastmatch_normaltrace_max_nodes"},
        {"global_fastmatch_normaltrace_max_trace_gap_px",
         "fastmatch_normaltrace_max_trace_gap_px"},
        {"global_fastmatch_normaltrace_anchor_radius_px",
         "fastmatch_normaltrace_anchor_radius_px"},
        {"global_fastmatch_normaltrace_ann_tangent_deviation_deg",
         "fastmatch_normaltrace_ann_tangent_deviation_deg"},
        {"global_fastmatch_normaltrace_ann_normal_deviation_deg",
         "fastmatch_normaltrace_ann_normal_deviation_deg"},
        {"global_fastmatch_normaltrace_polarity",
         "fastmatch_normaltrace_polarity"}};
    for (const auto &entry : fastMatchGlobals)
      appendInt(oss, summaryText, entry.first, entry.second, missing);

    for (int direction = 0; direction < 4; ++direction) {
      const std::string suffix = std::to_string(direction);
      const std::pair<std::string, std::string> directionalGlobals[] = {
          {"global_fastmatch_learn_threshold_" + suffix,
           "fastmatch_learn_threshold_" + suffix},
          {"global_fastmatch_learn_method_" + suffix,
           "fastmatch_learn_method_" + suffix},
          {"global_fastmatch_learn_wgap_" + suffix,
           "fastmatch_learn_wgap_" + suffix},
          {"global_fastmatch_learn_hgap_" + suffix,
           "fastmatch_learn_hgap_" + suffix},
          {"global_fastmatch_learn_linegap_" + suffix,
           "fastmatch_learn_linegap_" + suffix},
          {"global_fastmatch_learn_compare_gap_" + suffix,
           "fastmatch_learn_compare_gap_" + suffix},
          {"global_fastmatch_learn_objfilter_" + suffix,
           "fastmatch_learn_objfilter_" + suffix},
          {"global_fastmatch_learn_edge_count_" + suffix,
           "fastmatch_learn_edge_count_" + suffix},
          {"global_fastmatch_learn_selected_edge_" + suffix,
           "fastmatch_learn_selected_edge_" + suffix}};
      for (const auto &entry : directionalGlobals)
        appendInt(oss, summaryText, entry.first, entry.second, missing);
    }
  }

  appendInt(oss, summaryText, "effective_gap", "gap", missing);
  appendInt(oss, summaryText, "effective_linegap", "linegap", missing);
  appendInt(oss, summaryText, "effective_threshold", "threshold", missing);
  appendInt(oss, summaryText, "effective_filterprofile", "filterprofile",
            missing);
  appendInt(oss, summaryText, "effective_method", "method", missing);
  appendInt(oss, summaryText, "effective_wgap", "wgap", missing);
  appendInt(oss, summaryText, "effective_hgap", "hgap", missing);

  oss << " max_elapsed_ms=2000"
      << " max_scan_lines=2000"
      << " max_samples=200000"
      << " review_status=pending_algorithm_review"
      << " failure_class=" << failureClass
      << " result_summary_path=" << runtimeSummary;
  if (!missing.empty())
    oss << " locked_param_missing=" << missing;
  if (!extraEvidence.empty() && extraEvidence != "-")
    oss << " extra_evidence=" << extraEvidence;
  return oss.str();
}

static bool
IsManualReviewHandoffCaseRowLocal(const std::vector<std::string> &cells) {
  if (cells.size() < 11)
    return false;
  if (cells[0] == "Case" || cells[0] == "Review Item" ||
      cells[0].find("---") != std::string::npos)
    return false;
  if (cells.size() >= 14 &&
      cells[1].find("Manual Review / Evidence") != std::string::npos) {
    return !cells[0].empty() && !cells[2].empty() && !cells[3].empty() &&
           !cells[7].empty() && !cells[12].empty();
  }
  return !cells[0].empty() && !cells[1].empty() && !cells[5].empty() &&
         !cells[10].empty();
}

static std::string
BuildManualReviewVisibleCaseLabelLocal(const std::string &internalCaseId) {
  const std::size_t separator = internalCaseId.rfind("__");
  return separator == std::string::npos ? internalCaseId
                                        : internalCaseId.substr(separator + 2);
}

static std::string
ReadManualReviewExtraFieldLocal(const std::string &extraEvidence,
                                const std::string &field) {
  return ReadSemicolonFieldLocal(extraEvidence, field);
}

static std::string
FindManualReviewArtifactPathLocal(const std::string &extraEvidence,
                                  const std::string &field,
                                  const std::string &fileName) {
  std::string path = ReadManualReviewExtraFieldLocal(extraEvidence, field);
  if (!path.empty())
    return path;

  std::istringstream fields(extraEvidence);
  std::string candidate;
  while (std::getline(fields, candidate, ';')) {
    candidate = TrimLine(candidate);
    const std::size_t equals = candidate.find('=');
    if (equals != std::string::npos)
      candidate = TrimLine(candidate.substr(equals + 1));
    if (std::filesystem::path(candidate).filename() == fileName)
      return candidate;
  }
  return {};
}

static bool LoadTorchDatasetBindingsLocal(
    const std::string &summaryPath,
    std::vector<CxEvidenceDatasetImageBinding> &images,
    std::vector<CxEvidenceAnnotationBinding> &annotations,
    std::string &reason) {
  reason.clear();
  images.clear();
  annotations.clear();
  if (summaryPath.empty()) {
    reason = "dataset summary path is empty";
    return false;
  }

  try {
    cv::FileStorage storage(summaryPath, cv::FileStorage::READ);
    if (!storage.isOpened()) {
      reason = "cannot open dataset summary: " + summaryPath;
      return false;
    }
    std::string schema;
    storage["schema"] >> schema;
    if (schema.find("cxvision.yolov8seg.dataset_summary") != 0) {
      reason = "unsupported dataset summary schema: " + schema;
      return false;
    }

    std::set<std::string> imageKeys;
    std::set<std::string> annotationKeys;
    const cv::FileNode rows = storage["rows"];
    for (const auto &row : rows) {
      CxEvidenceDatasetImageBinding image;
      row["image_id"] >> image.image_id;
      row["image_ref"] >> image.image_path;
      row["split"] >> image.split;
      row["label"] >> image.label;
      image.source = "yolov8seg_dataset_summary";
      if (image.split.empty())
        image.split = "train";
      if (image.label.empty())
        image.label = "unlabeled";
      const std::string imageKey =
          image.split + "|" + image.image_id + "|" + image.image_path;
      if (!image.image_path.empty() && imageKeys.insert(imageKey).second)
        images.push_back(image);

      const cv::FileNode boxes = row["annotations"];
      for (const auto &box : boxes) {
        CxEvidenceAnnotationBinding annotation;
        annotation.image_id = image.image_id;
        box["class_id"] >> annotation.class_id;
        box["x0"] >> annotation.x0;
        box["y0"] >> annotation.y0;
        box["x1"] >> annotation.x1;
        box["y1"] >> annotation.y1;
        int normalized = 1;
        box["normalized"] >> normalized;
        annotation.normalized = normalized != 0;
        annotation.label = image.label.empty() ? "annotated" : image.label;
        annotation.semantic_role = "bbox";
        std::ostringstream key;
        key << annotation.image_id << '|' << annotation.class_id << '|'
            << annotation.x0 << '|' << annotation.y0 << '|' << annotation.x1
            << '|' << annotation.y1;
        if (annotationKeys.insert(key.str()).second)
          annotations.push_back(annotation);
      }
    }
  } catch (const cv::Exception &error) {
    reason = "dataset summary parse failed: " + std::string(error.what());
    return false;
  }

  reason = "images=" + std::to_string(images.size()) +
           " annotations=" + std::to_string(annotations.size());
  return !images.empty();
}

static bool LoadTorchTrainingRunBindingLocal(const std::string &tracePath,
                                             CxTorchTrainingRunBinding &run,
                                             std::string &reason) {
  reason.clear();
  run = CxTorchTrainingRunBinding{};
  if (tracePath.empty()) {
    reason = "training trace path is empty";
    return false;
  }

  try {
    cv::FileStorage storage(tracePath, cv::FileStorage::READ);
    if (!storage.isOpened()) {
      reason = "cannot open training trace: " + tracePath;
      return false;
    }
    std::string schema;
    storage["schema"] >> schema;
    if (schema == "cxvision.torch.yolov8n_training_lifecycle.v1") {
      storage["status"] >> run.status;
      storage["epochs"] >> run.configured_epochs;
      storage["train_image_count"] >> run.train_sample_count;
      storage["learning_rate"] >> run.learning_rate;
      storage["batches_per_epoch"] >> run.batches_per_epoch;
      storage["completed_epochs"] >> run.completed_epochs;
      int earlyStopped = 0;
      int qualityTargets = 0;
      int classAgnosticNms = 0;
      storage["early_stopped"] >> earlyStopped;
      storage["best_validation_epoch"] >> run.best_validation_epoch;
      storage["best_validation_f1"] >> run.best_validation_f1;
      storage["assignment_topk"] >> run.assignment_topk;
      storage["classification_focal_gamma"] >>
          run.classification_focal_gamma;
      storage["use_assignment_quality_targets"] >> qualityTargets;
      storage["postprocess_confidence_threshold"] >>
          run.postprocess_confidence_threshold;
      storage["postprocess_iou_threshold"] >> run.postprocess_iou_threshold;
      storage["postprocess_max_detections"] >> run.postprocess_max_detections;
      storage["postprocess_class_agnostic_nms"] >> classAgnosticNms;
      storage["checkpoint_selection_ref"] >> run.checkpoint_selection_path;
      run.early_stopped = earlyStopped != 0;
      run.use_assignment_quality_targets = qualityTargets != 0;
      run.postprocess_class_agnostic_nms = classAgnosticNms != 0;
      run.task = "detection";
      run.optimizer = "SGD";
      run.dataset_source = "asset-driven YOLO dataset package";
      run.loss_phase = "YOLOv8 detection total loss";
      run.training_trace_path = tracePath;

      std::string curveRef;
      std::string modelManifestRef;
      storage["learning_curve_ref"] >> curveRef;
      storage["incremental_manifest_ref"] >> modelManifestRef;
      const std::filesystem::path traceFile(tracePath);
      auto resolveRef = [&](const std::string &ref) {
        std::filesystem::path path(ref);
        if (path.is_relative())
          path = traceFile.parent_path() / path;
        return path.lexically_normal();
      };
      const std::filesystem::path curvePath = resolveRef(curveRef);
      run.learning_curve_path = curvePath.string();
      run.incremental_model_manifest_path =
          resolveRef(modelManifestRef).string();

      std::ifstream curve(curvePath);
      std::string line;
      std::getline(curve, line);
      while (std::getline(curve, line)) {
        std::istringstream row(line);
        std::string epochText;
        std::string lossText;
        std::string batchText;
        if (!std::getline(row, epochText, ',') ||
            !std::getline(row, lossText, ',') ||
            !std::getline(row, batchText, ','))
          continue;
        try {
          CxTorchTrainingEpochMetric metric;
          metric.epoch = std::stoi(epochText);
          metric.total_loss = std::stod(lossText);
          metric.learning_rate = run.learning_rate;
          metric.sample_count = run.train_sample_count;
          run.batches_per_epoch = std::stoi(batchText);
          run.epochs.push_back(metric);
        } catch (const std::exception &) {
        }
      }
      if (!run.incremental_model_manifest_path.empty()) {
        cv::FileStorage manifest(run.incremental_model_manifest_path,
                                 cv::FileStorage::READ);
        if (manifest.isOpened()) {
          std::string weights;
          manifest["weights"] >> weights;
          std::filesystem::path weightsPath(weights);
          if (weightsPath.is_relative())
            weightsPath = std::filesystem::path(
                              run.incremental_model_manifest_path)
                              .parent_path() /
                          weightsPath;
          run.incremental_model_path = weightsPath.lexically_normal().string();
        }
      }
      if (run.completed_epochs <= 0)
        run.completed_epochs = static_cast<int>(run.epochs.size());
      run.available = !run.epochs.empty();
      reason = "epochs=" + std::to_string(run.epochs.size()) +
               " model=" + run.incremental_model_path;
      return run.available;
    }
    if (schema != "cxvision.yolov8seg.training_trace.v1") {
      reason = "unsupported training trace schema: " + schema;
      return false;
    }
    storage["status"] >> run.status;
    storage["task"] >> run.task;
    storage["dataset_source"] >> run.dataset_source;
    storage["optimizer"] >> run.optimizer;
    storage["lr_schedule"] >> run.lr_schedule;
    storage["loss_phase"] >> run.loss_phase;
    storage["configured_epochs"] >> run.configured_epochs;
    storage["completed_epochs"] >> run.completed_epochs;
    storage["train_sample_count"] >> run.train_sample_count;
    storage["train_instance_count"] >> run.train_instance_count;
    storage["learning_rate"] >> run.learning_rate;
    storage["min_learning_rate"] >> run.min_learning_rate;
    storage["weight_decay"] >> run.weight_decay;
    const cv::FileNode lossWeights = storage["loss_weights"];
    if (!lossWeights.empty()) {
      lossWeights["box"] >> run.box_loss_weight;
      lossWeights["class"] >> run.class_loss_weight;
      lossWeights["dfl"] >> run.dfl_loss_weight;
      lossWeights["mask"] >> run.mask_loss_weight;
    }
    run.training_trace_path = tracePath;

    const cv::FileNode epochs = storage["epochs"];
    for (const auto &node : epochs) {
      CxTorchTrainingEpochMetric metric;
      node["epoch"] >> metric.epoch;
      node["learning_rate"] >> metric.learning_rate;
      node["total_loss"] >> metric.total_loss;
      node["box_loss"] >> metric.box_loss;
      node["class_loss"] >> metric.class_loss;
      node["dfl_loss"] >> metric.dfl_loss;
      node["mask_loss"] >> metric.mask_loss;
      node["sample_count"] >> metric.sample_count;
      node["instance_count"] >> metric.instance_count;
      const cv::FileNode parameterGroups = node["parameter_groups"];
      for (const char *groupName :
           {"backbone", "pan_fpn", "box_head", "class_head", "mask_coeff_head",
            "proto_branch", "other"}) {
        const cv::FileNode group = parameterGroups[groupName];
        if (group.empty())
          continue;
        CxTorchTrainingEpochMetric::ParameterGroup parsed;
        parsed.name = groupName;
        int gradDefined = 0;
        group["grad_defined"] >> gradDefined;
        parsed.grad_defined = gradDefined != 0;
        group["grad_mean"] >> parsed.grad_mean;
        group["grad_max"] >> parsed.grad_max;
        group["grad_norm"] >> parsed.grad_norm;
        group["param_norm"] >> parsed.param_norm;
        group["update_norm"] >> parsed.update_norm;
        group["parameter_count"] >> parsed.parameter_count;
        metric.parameter_groups.push_back(std::move(parsed));
      }
      run.epochs.push_back(metric);
    }
    run.available = !run.epochs.empty();
  } catch (const cv::Exception &error) {
    reason = "training trace parse failed: " + std::string(error.what());
    return false;
  }

  reason = "epochs=" + std::to_string(run.epochs.size());
  return run.available;
}

static std::filesystem::path FindLatestRegularFileLocal(
    const std::filesystem::path &root, const std::string &fileName) {
  std::error_code ec;
  if (!std::filesystem::is_directory(root, ec) || ec)
    return {};
  std::filesystem::path latest;
  std::filesystem::file_time_type latestTime{};
  for (std::filesystem::recursive_directory_iterator it(
           root, std::filesystem::directory_options::skip_permission_denied,
           ec),
       end;
       it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    if (it->is_symlink(ec) || ec || !it->is_regular_file(ec) || ec ||
        it->path().filename() != fileName)
      continue;
    const auto writeTime = it->last_write_time(ec);
    if (ec) {
      ec.clear();
      continue;
    }
    if (latest.empty() || writeTime > latestTime) {
      latest = it->path();
      latestTime = writeTime;
    }
  }
  return latest;
}

static bool LoadLatestYoloTrainingRunLocal(CxTorchTrainingRunBinding &run,
                                           std::string &reason) {
  const std::filesystem::path root =
      ResolveCxVisionRunPath("cxscript_runs/yolov8n_incremental");
  const std::filesystem::path trace = FindLatestRegularFileLocal(
      root, "torch_yolov8n_training_lifecycle_result.json");
  if (trace.empty()) {
    reason = "no C++ YOLOv8-n lifecycle result found under " + root.string();
    return false;
  }
  return LoadTorchTrainingRunBindingLocal(trace.string(), run, reason);
}

static bool RefreshPartialYoloTrainingCurveLocal(
    const std::string &curvePath, CxTorchTrainingRunBinding &run) {
  std::ifstream curve(curvePath);
  if (!curve)
    return false;
  std::vector<CxTorchTrainingEpochMetric> metrics;
  std::string line;
  std::getline(curve, line);
  while (std::getline(curve, line)) {
    std::istringstream row(line);
    std::string epochText;
    std::string lossText;
    std::string batchText;
    if (!std::getline(row, epochText, ',') ||
        !std::getline(row, lossText, ',') ||
        !std::getline(row, batchText, ','))
      continue;
    try {
      CxTorchTrainingEpochMetric metric;
      metric.epoch = std::stoi(epochText);
      metric.total_loss = std::stod(lossText);
      metric.learning_rate = run.learning_rate;
      metric.sample_count = run.train_sample_count;
      run.batches_per_epoch = std::stoi(batchText);
      metrics.push_back(metric);
    } catch (const std::exception &) {
    }
  }
  if (metrics.empty())
    return false;
  run.epochs = std::move(metrics);
  run.completed_epochs = static_cast<int>(run.epochs.size());
  run.available = true;
  return true;
}

static void PollYoloTrainingProcessLocal(ManualTestContext &context) {
#ifdef _WIN32
  if (!context.torch_training_process_running ||
      context.torch_training_process_handle == 0)
    return;
  RefreshPartialYoloTrainingCurveLocal(
      context.torch_training_active_curve_path, context.torch_training_run);
  HANDLE process = reinterpret_cast<HANDLE>(
      context.torch_training_process_handle);
  DWORD exitCode = STILL_ACTIVE;
  if (!GetExitCodeProcess(process, &exitCode)) {
    context.torch_training_process_running = false;
    context.geometry_aug_run_status = "CXX_YOLOV8N_TRAINING_FAIL";
    context.geometry_aug_run_reason = "cannot query C++ training process";
    CloseHandle(process);
    context.torch_training_process_handle = 0;
    return;
  }
  if (exitCode == STILL_ACTIVE) {
    context.torch_training_run.status = "CXX_YOLOV8N_TRAINING_RUNNING";
    context.torch_training_image_status =
        context.torch_training_run.status;
    context.torch_training_image_reason =
        "completed_epoch=" +
        std::to_string(context.torch_training_run.completed_epochs) + "/" +
        std::to_string(context.torch_training_run.configured_epochs);
    return;
  }
  CloseHandle(process);
  context.torch_training_process_handle = 0;
  context.torch_training_process_running = false;
  std::string reason;
  if (exitCode == 0 && LoadTorchTrainingRunBindingLocal(
                           context.torch_training_active_result_path,
                           context.torch_training_run, reason)) {
    context.geometry_aug_run_status = context.torch_training_run.status;
    context.geometry_aug_run_reason =
        "C++ training complete; final curve and model are bound";
    context.torch_training_image_status = context.torch_training_run.status;
    context.torch_training_image_reason = reason;
  } else {
    context.geometry_aug_run_status = "CXX_YOLOV8N_TRAINING_FAIL";
    context.geometry_aug_run_reason =
        "C++ training process exit code=" + std::to_string(exitCode) +
        (reason.empty() ? std::string() : " reason=" + reason);
  }
#else
  (void)context;
#endif
}

static void DrawEpochLossCurveLocal(const char *id,
                                    const CxTorchTrainingRunBinding &run,
                                    std::size_t startIndex = 0,
                                    float height = 170.0f,
                                    int fixedEpochAxisCapacity = 0,
                                    float fixedLossAxisMaximum = 0.0f) {
  if (!run.HasRealMultiEpochSeries())
    return;
  startIndex = std::min(startIndex, run.epochs.size() - 2);
  // This is deliberately independent from the number of received samples.
  // A completed 50-epoch run still has an X maximum of 100 when the operator
  // selected 100, so finished and live runs are directly comparable.
  const int axisEpochMaximum = std::max(
      {1, run.configured_epochs, fixedEpochAxisCapacity});
  const int firstRecordedEpoch = run.epochs.front().epoch;
  const int lastRecordedIndex = std::max(
      0, run.epochs.back().epoch - firstRecordedEpoch);
  const double observedMaximum = [&run, startIndex]() {
    double maximum = 0.0;
    for (std::size_t i = startIndex; i < run.epochs.size(); ++i)
      maximum = std::max(maximum, run.epochs[i].total_loss);
    return maximum;
  }();
  const double lossAxisMaximum = std::max(
      0.001, fixedLossAxisMaximum > 0.0f
                 ? static_cast<double>(fixedLossAxisMaximum)
                 : observedMaximum);
  ImGui::TextDisabled(
      "Fixed axes: X min 0 / max %d | Y min 0 / max %.4g | recorded X 0-%d",
      axisEpochMaximum, lossAxisMaximum, lastRecordedIndex);
  if (observedMaximum > lossAxisMaximum)
    ImGui::TextColored(ImVec4(0.95f, 0.72f, 0.22f, 1.0f),
                       "Observed loss %.4g exceeds the fixed Y maximum; clipped at the chart edge.",
                       observedMaximum);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const ImVec2 size(std::max(320.0f, ImGui::GetContentRegionAvail().x), height);
  ImGui::InvisibleButton(id, size);
  ImDrawList *draw = ImGui::GetWindowDrawList();
  const float left = origin.x + 46.0f;
  const float right = origin.x + size.x - 12.0f;
  const float top = origin.y + 12.0f;
  const float bottom = origin.y + size.y - 30.0f;
  const double minLoss = 0.0;
  const double maxLoss = lossAxisMaximum;
  // The X axis is a declared viewing capacity, not the number of samples
  // currently received.  A live training run therefore grows from x=0 while
  // the right-hand future range remains intentionally empty.
  const ImU32 axis = IM_COL32(150, 160, 172, 255);
  const ImU32 grid = IM_COL32(75, 82, 92, 180);
  const ImU32 curve = IM_COL32(52, 190, 132, 255);
  draw->AddRect(ImVec2(left, top), ImVec2(right, bottom), axis);
  for (int tick = 0; tick <= 4; ++tick) {
    const float t = static_cast<float>(tick) / 4.0f;
    const float x = left + (right - left) * t;
    draw->AddLine(ImVec2(x, top), ImVec2(x, bottom), grid);
    const int epoch =
        static_cast<int>(std::lround(axisEpochMaximum * t));
    const std::string label = std::to_string(epoch);
    draw->AddText(ImVec2(x - ImGui::CalcTextSize(label.c_str()).x * 0.5f,
                         bottom + 4.0f),
                  axis, label.c_str());
  }
  if (lastRecordedIndex < axisEpochMaximum) {
    const float futureStart = left + (right - left) *
                                         static_cast<float>(lastRecordedIndex) /
                                         static_cast<float>(axisEpochMaximum);
    draw->AddRectFilled(ImVec2(futureStart, top), ImVec2(right, bottom),
                        IM_COL32(16, 20, 25, 84));
    draw->AddText(ImVec2(futureStart + 7.0f, top + 7.0f), axis,
                  "Future range: no samples");
  }
  char maxLabel[32]{};
  char minLabel[32]{};
  std::snprintf(maxLabel, sizeof(maxLabel), "%.4g", maxLoss);
  std::snprintf(minLabel, sizeof(minLabel), "%.4g", minLoss);
  draw->AddText(ImVec2(origin.x, top - 5.0f), axis, maxLabel);
  draw->AddText(ImVec2(origin.x, bottom - 7.0f), axis, minLabel);
  draw->AddText(ImVec2((left + right) * 0.5f - 18.0f, bottom + 18.0f), axis,
                "Epoch");
  ImVec2 previous;
  for (std::size_t i = startIndex; i < run.epochs.size(); ++i) {
    const auto &metric = run.epochs[i];
    const float x = left + (right - left) *
                                static_cast<float>(std::max(
                                    0, metric.epoch - firstRecordedEpoch)) /
                                static_cast<float>(axisEpochMaximum);
    const float yRatio = std::clamp(
        static_cast<float>((metric.total_loss - minLoss) / (maxLoss - minLoss)),
        0.0f, 1.0f);
    const float y = bottom - (bottom - top) * yRatio;
    const ImVec2 point(x, y);
    if (i > startIndex)
      draw->AddLine(previous, point, curve, 2.0f);
    previous = point;
  }
  if (ImGui::IsItemHovered()) {
    const float mouseX = ImGui::GetIO().MousePos.x;
    const float ratio = std::clamp((mouseX - left) / (right - left), 0.0f, 1.0f);
    const int targetEpoch =
        static_cast<int>(std::lround(axisEpochMaximum * ratio));
    if (targetEpoch > lastRecordedIndex) {
      ImGui::SetTooltip("Epoch index %d / %d\nFuture epoch: no sample recorded yet",
                        targetEpoch, axisEpochMaximum);
      return;
    }
    const auto nearest = std::min_element(
        run.epochs.begin() + static_cast<std::ptrdiff_t>(startIndex),
        run.epochs.end(), [&](const auto &a, const auto &b) {
          return std::abs((a.epoch - firstRecordedEpoch) - targetEpoch) <
                 std::abs((b.epoch - firstRecordedEpoch) - targetEpoch);
        });
    ImGui::SetTooltip("Epoch index %d / %d (runtime epoch %d)\nLoss %.7g\nBatches %d",
                      nearest->epoch - firstRecordedEpoch, axisEpochMaximum,
                      nearest->epoch, nearest->total_loss,
                      run.batches_per_epoch);
  }
}

struct ActiveManualReviewProjectionLocal {
  std::filesystem::path manifest_path;
  std::unordered_set<std::string> managed_tools;
  std::unordered_set<std::string> selected_binding_paths;

  bool ManagesTool(const std::string &tool) const {
    const std::string normalized = NormalizeEvidenceToolTypeLocal(tool);
    return !normalized.empty() &&
           managed_tools.find(normalized) != managed_tools.end();
  }
};

static std::string NormalizeManualReviewProjectionPathLocal(
    const std::filesystem::path &path) {
  std::string value = path.lexically_normal().generic_string();
#ifdef _WIN32
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
#endif
  return value;
}

static ActiveManualReviewProjectionLocal
LoadActiveManualReviewProjectionLocal() {
  ActiveManualReviewProjectionLocal projection;
  projection.manifest_path = ResolveCxVisionRunPath(
      "cxscript_runs/evidence_chain/active_manual_review_projection.tsv");

  std::string text;
  if (!ReadTextFile(projection.manifest_path.string(), text))
    return projection;

  std::istringstream input(text);
  std::string line;
  while (std::getline(input, line)) {
    line = TrimLine(line);
    if (line.empty() || line[0] == '#')
      continue;

    const std::size_t separator = line.find('\t');
    if (separator == std::string::npos)
      continue;

    const std::string tool = TrimLine(line.substr(0, separator));
    std::string bindingValue = TrimLine(line.substr(separator + 1));
    if (tool.empty() || bindingValue.empty() || tool == "tool" ||
        tool == "schema")
      continue;

    std::filesystem::path bindingPath(bindingValue);
    if (bindingPath.is_relative())
      bindingPath = projection.manifest_path.parent_path() / bindingPath;

    const std::string normalizedTool =
        NormalizeEvidenceToolTypeLocal(tool);
    const std::string normalizedBinding =
        NormalizeManualReviewProjectionPathLocal(bindingPath);
    if (normalizedTool.empty() || normalizedBinding.empty())
      continue;

    projection.managed_tools.insert(normalizedTool);
    projection.selected_binding_paths.insert(normalizedBinding);
  }
  return projection;
}

static bool IsSelectedManualReviewBindingLocal(
    const ActiveManualReviewProjectionLocal &projection,
    const std::filesystem::path &bindingPath) {
  return projection.selected_binding_paths.find(
             NormalizeManualReviewProjectionPathLocal(bindingPath)) !=
         projection.selected_binding_paths.end();
}

static int AppendManualAlgorithmReviewHandoffLocal(
    ManualTestContext &context, const std::string &handoffPath,
    const std::function<std::string(const std::string &)> &resolveImagePath,
    const std::function<ScriptEvidenceGroup &(const std::string &)>
        &findGroup,
    const ActiveManualReviewProjectionLocal &projection,
    int &projectionManagedRows) {
  std::string text;
  if (!ReadTextFile(handoffPath, text))
    return 0;

  std::istringstream input(text);
  std::string line;
  int appended = 0;
  while (std::getline(input, line)) {
    if (line.find('|') == std::string::npos)
      continue;

    const std::vector<std::string> cells = SplitMarkdownTableRowLocal(line);
    if (!IsManualReviewHandoffCaseRowLocal(cells))
      continue;

    const bool hasReviewItemColumns =
        cells.size() >= 14 &&
        cells[1].find("Manual Review / Evidence") != std::string::npos;
    const std::string internalCaseId =
        hasReviewItemColumns ? cells[2] : cells[0];
    const std::string visibleCaseId =
        hasReviewItemColumns
            ? cells[0]
            : BuildManualReviewVisibleCaseLabelLocal(internalCaseId);
    const std::string tool = NormalizeEvidenceToolTypeLocal(
        hasReviewItemColumns ? cells[3] : cells[1]);
    if (projection.ManagesTool(tool)) {
      // Projection is an overlay selection aid. It must not hide asset-backed
      // handoff cases from Manual Review / Evidence.
      ++projectionManagedRows;
    }
    const std::string imageId = hasReviewItemColumns ? cells[4] : cells[2];
    const std::string targetId = hasReviewItemColumns ? cells[5] : cells[3];
    const std::string failureClass = hasReviewItemColumns ? cells[6] : cells[4];
    const std::string runtimeSummary =
        hasReviewItemColumns ? cells[7] : cells[5];
    const std::string toolDisplay = hasReviewItemColumns ? cells[8] : cells[6];
    const std::string resultOverlay =
        hasReviewItemColumns ? cells[9] : cells[7];
    const std::string evidenceOverlay =
        hasReviewItemColumns ? cells[10] : cells[8];
    const std::string roiPreview = hasReviewItemColumns ? cells[11] : cells[9];
    const std::string scriptSnapshot =
        hasReviewItemColumns ? cells[12] : cells[10];
    const std::string extraEvidence =
        hasReviewItemColumns ? (cells.size() > 13 ? cells[13] : std::string())
                             : (cells.size() > 11 ? cells[11] : std::string());

    ScriptEvidenceThumb thumb;
    thumb.case_id = internalCaseId;
    thumb.script_id = visibleCaseId;
    thumb.script_path = scriptSnapshot;
    thumb.image_id = imageId;
    thumb.image_path =
        ResolveOriginalImagePathFromEvidencePacketLocal(runtimeSummary);
    if (thumb.image_path.empty()) {
      const std::string manifestImage = resolveImagePath(imageId);
      if (!IsDerivedEvidenceRenderPathLocal(manifestImage))
        thumb.image_path = manifestImage;
    }
    auto firstExistingImage = [](const std::vector<std::string> &paths) {
      for (const std::string &path : paths) {
        std::error_code ec;
        if (!path.empty() && std::filesystem::is_regular_file(path, ec))
          return path;
      }
      return std::string{};
    };
    const std::string reviewImage = firstExistingImage(
        {evidenceOverlay, resultOverlay, roiPreview, toolDisplay});
    thumb.thumbnail_path = firstExistingImage(
        {roiPreview, evidenceOverlay, resultOverlay, toolDisplay});
    thumb.target_id = targetId;
    thumb.tool = tool;
    thumb.parameter_summary = BuildManualReviewParamSummaryLocal(
        tool, failureClass, runtimeSummary, extraEvidence);
    if (tool == "TorchTask") {
      const std::string datasetSummaryPath = FindManualReviewArtifactPathLocal(
          extraEvidence, "dataset_summary", "dataset_summary.json");
      const std::string trainingTracePath = FindManualReviewArtifactPathLocal(
          extraEvidence, "training_trace", "training_trace.json");
      std::string bindingReason;
      LoadTorchDatasetBindingsLocal(datasetSummaryPath, thumb.dataset_images,
                                    thumb.annotations, bindingReason);
      thumb.training_run.dataset_summary_path = datasetSummaryPath;
      std::string traceReason;
      LoadTorchTrainingRunBindingLocal(trainingTracePath, thumb.training_run,
                                       traceReason);
      thumb.training_run.dataset_summary_path = datasetSummaryPath;
    }
    thumb.status = "pending_algorithm_review";
    thumb.evidence_category_override = "To Verify";
    thumb.reason =
        "manual algorithm review from handoff; failure_class=" + failureClass +
        "; internal_case_id=" + internalCaseId +
        "; result_summary=" + runtimeSummary + "; tool_display=" + toolDisplay +
        "; result_overlay=" + resultOverlay +
        "; evidence_overlay=" + evidenceOverlay +
        "; roi_preview=" + roiPreview + "; source_image=" + thumb.image_path +
        "; dataset_images=" + std::to_string(thumb.dataset_images.size()) +
        "; annotations=" + std::to_string(thumb.annotations.size()) +
        "; training_epochs=" +
        std::to_string(thumb.training_run.completed_epochs) +
        "; handoff=" + handoffPath;

    PopulateEditableObjectBindingForThumbLocal(thumb);

    ScriptEvidenceGroup &group = findGroup(tool);
    bool exists = false;
    for (const auto &existing : group.thumbs) {
      if (existing.case_id == thumb.case_id) {
        exists = true;
        break;
      }
    }
    if (!exists) {
      group.thumbs.push_back(thumb);
      ++appended;
    }

    const std::string matrixPath =
        ReadManualReviewExtraFieldLocal(extraEvidence, "stability_matrix");
    if (!matrixPath.empty()) {
      ScriptEvidenceThumb matrixThumb = thumb;
      const std::filesystem::path matrixFile(matrixPath);
      const std::size_t separator = internalCaseId.rfind("__");
      const std::string parentCaseId =
          separator == std::string::npos ? internalCaseId
                                         : internalCaseId.substr(0, separator);
      matrixThumb.case_id = parentCaseId + "__" + matrixFile.stem().string();
      matrixThumb.script_id = matrixFile.filename().string();
      matrixThumb.status = "pending_algorithm_review";
      matrixThumb.reason =
          "manual algorithm review from folder artifact; matrix=" + matrixPath +
          "; handoff=" + handoffPath;

      bool matrixExists = false;
      for (const auto &existing : group.thumbs) {
        if (existing.case_id == matrixThumb.case_id) {
          matrixExists = true;
          break;
        }
      }
      if (!matrixExists) {
        group.thumbs.push_back(matrixThumb);
        ++appended;
      }
    }
  }

  context.debug_status = "MANUAL_ALGORITHM_REVIEW_HANDOFF_LOADED";
  context.debug_reason = handoffPath;
  return appended;
}

static int AppendManualAlgorithmReviewHandoffsFromRunFoldersLocal(
    ManualTestContext &context,
    const std::function<std::string(const std::string &)> &resolveImagePath,
    const std::function<ScriptEvidenceGroup &(const std::string &)> &findGroup,
    std::string &reason) {
  reason.clear();
  const std::filesystem::path root = ResolveCxVisionRunPath("cxscript_runs");
  std::error_code ec;
  if (!std::filesystem::is_directory(root, ec)) {
    reason = "cxscript_runs folder not found: " + root.string();
    return 0;
  }

  std::vector<std::filesystem::path> handoffs;
  std::vector<std::string> scanErrors;
  std::vector<std::filesystem::path> pendingDirectories{root};
  while (!pendingDirectories.empty()) {
    const std::filesystem::path directory = pendingDirectories.back();
    pendingDirectories.pop_back();

    std::error_code directoryEc;
    std::filesystem::directory_iterator it(
        directory, std::filesystem::directory_options::skip_permission_denied,
        directoryEc);
    const std::filesystem::directory_iterator end;
    if (directoryEc) {
      scanErrors.push_back(directory.string() + ": " + directoryEc.message());
      continue;
    }

    while (it != end) {
      const std::filesystem::directory_entry entry = *it;
      std::error_code entryEc;
      if (entry.is_regular_file(entryEc)) {
        if (entry.path().filename() == "manual_algorithm_review_handoff.md")
          handoffs.push_back(entry.path());
      } else if (!entryEc && entry.is_directory(entryEc) &&
                 !entry.is_symlink(entryEc)) {
        pendingDirectories.push_back(entry.path());
      }
      if (entryEc) {
        scanErrors.push_back(entry.path().string() + ": " + entryEc.message());
      }

      it.increment(directoryEc);
      if (directoryEc) {
        scanErrors.push_back(directory.string() + ": " + directoryEc.message());
        break;
      }
    }
  }

  std::stable_sort(handoffs.begin(), handoffs.end(),
                   [](const std::filesystem::path &left,
                      const std::filesystem::path &right) {
                     std::error_code leftEc;
                     std::error_code rightEc;
                     const auto leftTime =
                         std::filesystem::last_write_time(left, leftEc);
                     const auto rightTime =
                         std::filesystem::last_write_time(right, rightEc);
                     if (!leftEc && !rightEc && leftTime != rightTime)
                       return leftTime < rightTime;
                     return left.string() < right.string();
                   });

  int before = 0;
  for (const auto &group : context.script_evidence_groups)
    before += static_cast<int>(group.thumbs.size());

  const ActiveManualReviewProjectionLocal projection =
      LoadActiveManualReviewProjectionLocal();
  int projectionManagedRows = 0;

  std::ostringstream scanDebug;
  scanDebug << "root\t" << root.string() << '\n';
  scanDebug << "projection_manifest\t"
            << projection.manifest_path.string() << '\n';
  scanDebug << "managed_tools\t" << projection.managed_tools.size() << '\n';
  scanDebug << "selected_bindings\t"
            << projection.selected_binding_paths.size() << '\n';
  for (const std::string &scanError : scanErrors)
    scanDebug << "scan_error\t" << scanError << '\n';
  scanDebug << "handoff_path\tappended_cases\tprojection_managed_rows_retained\n";
  for (const auto &handoff : handoffs) {
    const int beforeProjectionManaged = projectionManagedRows;
    const int handoffAppended = AppendManualAlgorithmReviewHandoffLocal(
        context, handoff.string(), resolveImagePath, findGroup, projection,
        projectionManagedRows);
    scanDebug << handoff.string() << '\t' << handoffAppended << '\t'
              << (projectionManagedRows - beforeProjectionManaged) << '\n';
  }

  const std::filesystem::path scanDebugPath = ResolveCxVisionRunPath(
      "cxscript_runs/evidence_chain/manual_review_handoff_scan_debug.tsv");
  WriteTextFile(scanDebugPath, scanDebug.str());

  int after = 0;
  for (const auto &group : context.script_evidence_groups)
    after += static_cast<int>(group.thumbs.size());

  const int appended = std::max(0, after - before);
  std::ostringstream oss;
  oss << "manual algorithm review handoffs scanned=" << handoffs.size()
      << " appended_cases=" << appended
      << " projection_managed_rows_retained=" << projectionManagedRows
      << " managed_tools=" << projection.managed_tools.size()
      << " selected_bindings=" << projection.selected_binding_paths.size()
      << " projection_manifest=" << projection.manifest_path.string()
      << " root=" << root.string() << " debug=" << scanDebugPath.string();
  reason = oss.str();
  return appended;
}

static std::filesystem::path ManualGuidanceQueuePathLocal() {
  return ResolveCxVisionRunPath(
      "cxscript_runs/evidence_chain/manual_guidance_queue.tsv");
}

static std::vector<std::string>
SplitEvidenceTsvRowLocal(const std::string &line) {
  std::vector<std::string> cells;
  std::string current;
  for (char ch : line) {
    if (ch == '\t') {
      cells.push_back(current);
      current.clear();
      continue;
    }
    if (ch != '\r')
      current += ch;
  }
  cells.push_back(current);
  return cells;
}

static std::string
BuildManualGuidanceGlobalsSummaryLocal(const std::string &candidateDir) {
  if (candidateDir.empty())
    return {};

  std::string text;
  const std::filesystem::path globalsPath =
      std::filesystem::path(candidateDir) / "globals.txt";
  if (!ReadTextFile(globalsPath.string(), text))
    return {};

  std::ostringstream summary;
  std::istringstream input(text);
  std::string line;
  bool first = true;
  while (std::getline(input, line)) {
    line = TrimLine(line);
    if (line.rfind("global_", 0) != 0 || line.find('=') == std::string::npos) {
      continue;
    }
    if (!first)
      summary << ' ';
    summary << line;
    first = false;
  }
  return summary.str();
}

static void AppendManualGuidanceQueueLocal(
    ManualTestContext &context,
    const std::function<std::string(const std::string &)> &resolveImagePath,
    const std::function<ScriptEvidenceGroup &(const std::string &)>
        &findGroup) {
  const std::filesystem::path queuePath = ManualGuidanceQueuePathLocal();
  std::string text;
  if (!ReadTextFile(queuePath.string(), text))
    return;

  std::istringstream input(text);
  std::string line;
  bool headerSeen = false;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#')
      continue;

    const std::vector<std::string> cells = SplitEvidenceTsvRowLocal(line);
    if (!headerSeen) {
      headerSeen = true;
      if (!cells.empty() && cells[0] == "case_id")
        continue;
    }

    if (cells.size() < 9)
      continue;

    ScriptEvidenceThumb thumb;
    thumb.case_id = cells[0];
    thumb.tool = NormalizeEvidenceToolTypeLocal(cells[1]);
    thumb.image_id = cells[2];
    thumb.target_id = cells[3];
    thumb.candidate_id = cells[4];
    thumb.status = "pending_human_guidance";
    thumb.reason = cells[5];
    thumb.parameter_summary = cells[6];
    thumb.script_path = cells[7];
    thumb.script_id = thumb.case_id;
    thumb.image_path = resolveImagePath(thumb.image_id);
    thumb.thumbnail_path = cells[8];
    thumb.evidence_category_override = "To Verify";
    if (cells.size() > 10)
      thumb.candidate_dir = cells[10];

    if (!thumb.candidate_dir.empty()) {
      const std::filesystem::path candidateRoot(thumb.candidate_dir);
      const std::filesystem::path scriptSnapshot =
          candidateRoot / "script_snapshot.cxsc";
      const std::filesystem::path runtimeGlobals =
          candidateRoot / "runtime_globals.json";
      const std::filesystem::path gaugeAnnotation =
          candidateRoot / "gauge_annotation.json";
      if (std::filesystem::is_regular_file(scriptSnapshot) &&
          std::filesystem::is_regular_file(runtimeGlobals) &&
          std::filesystem::is_regular_file(gaugeAnnotation)) {
        thumb.working_script_snapshot_path = scriptSnapshot.string();
        thumb.runtime_globals_path = runtimeGlobals.string();
        thumb.gauge_annotation_path = gaugeAnnotation.string();
        thumb.has_saved_state = true;
      }

      const std::string lockedGlobals =
          BuildManualGuidanceGlobalsSummaryLocal(thumb.candidate_dir);
      if (!lockedGlobals.empty()) {
        if (!thumb.parameter_summary.empty())
          thumb.parameter_summary += "; ";
        thumb.parameter_summary += lockedGlobals;
      }
    }

    thumb.source_evidence_script_path = thumb.script_path;

    if (thumb.image_path.empty())
      thumb.image_path = ResolveOriginalImagePathFromEvidencePacketLocal(
          thumb.parameter_summary);

    PopulateEditableObjectBindingForThumbLocal(thumb);

    ScriptEvidenceGroup &group = findGroup(thumb.tool);
    bool exists = false;
    for (auto &existing : group.thumbs) {
      if (existing.case_id == thumb.case_id &&
          existing.target_id == thumb.target_id) {

        existing = thumb;
        exists = true;
        break;
      }
    }
    if (!exists)
      group.thumbs.push_back(std::move(thumb));
  }
}

static void
WriteEvidenceChainLoadedElementsDebugLocal(const ManualTestContext &context) {
  auto escapeTsv = [](std::string value) -> std::string {
    for (char &ch : value) {
      if (ch == '\t' || ch == '\r' || ch == '\n')
        ch = ' ';
    }
    return value;
  };

  int totalThumbs = 0;
  int guidanceThumbs = 0;
  int toVerifyOverrideThumbs = 0;
  int processOverrideThumbs = 0;
  int displayToVerifyThumbs = 0;
  int displayProcessThumbs = 0;

  std::ostringstream rows;
  rows << "group_label\tcase_id\timage_id\ttarget_id\ttool\tcandidate_id\t"
       << "status\toverride\tdisplay_major\tdisplay_priority\tis_candidate\t"
       << "has_saved_state\tscript_id\tscript_path\timage_path\t"
       << "thumbnail_path\tdataset_images\tannotations\tparameter_summary\t"
       << "reason\n";

  for (const auto &group : context.script_evidence_groups) {
    for (const auto &thumb : group.thumbs) {
      ++totalThumbs;
      const std::string resolvedOverride =
          ResolveEvidenceCategoryOverrideLocal(context, thumb);
      if (thumb.status == "pending_human_guidance")
        ++guidanceThumbs;
      if (resolvedOverride == "To Verify")
        ++toVerifyOverrideThumbs;
      if (resolvedOverride == "Process Validation")
        ++processOverrideThumbs;
      const auto displayMajor =
          ClassifyEvidenceMajorBucketLocal(context, thumb, group.label);
      if (displayMajor.second == "To Verify")
        ++displayToVerifyThumbs;
      if (displayMajor.second == "Process Validation")
        ++displayProcessThumbs;

      rows << escapeTsv(group.label) << '\t' << escapeTsv(thumb.case_id) << '\t'
           << escapeTsv(thumb.image_id) << '\t' << escapeTsv(thumb.target_id)
           << '\t' << escapeTsv(thumb.tool) << '\t'
           << escapeTsv(thumb.candidate_id) << '\t' << escapeTsv(thumb.status)
           << '\t' << escapeTsv(resolvedOverride) << '\t'
           << escapeTsv(displayMajor.second) << '\t' << displayMajor.first
           << '\t' << (thumb.is_candidate ? "1" : "0") << '\t'
           << (thumb.has_saved_state ? "1" : "0") << '\t'
           << escapeTsv(thumb.script_id) << '\t' << escapeTsv(thumb.script_path)
           << '\t' << escapeTsv(thumb.image_path) << '\t'
           << escapeTsv(thumb.thumbnail_path) << '\t'
           << thumb.dataset_images.size() << '\t' << thumb.annotations.size()
           << '\t' << escapeTsv(thumb.parameter_summary) << '\t'
           << escapeTsv(thumb.reason) << '\n';
    }
  }

  const std::filesystem::path detailPath =
      ResolveCxVisionRunPath("cxscript_runs/evidence_chain/"
                             "evidence_chain_loaded_elements_debug.tsv");
  WriteTextFile(detailPath, rows.str());

  std::ostringstream summary;
  summary << "{\n"
          << "  \"groups\": " << context.script_evidence_groups.size() << ",\n"
          << "  \"thumbs\": " << totalThumbs << ",\n"
          << "  \"pending_human_guidance\": " << guidanceThumbs << ",\n"
          << "  \"to_verify_override\": " << toVerifyOverrideThumbs << ",\n"
          << "  \"process_validation_override\": " << processOverrideThumbs
          << ",\n"
          << "  \"display_to_verify\": " << displayToVerifyThumbs << ",\n"
          << "  \"display_process_validation\": " << displayProcessThumbs
          << ",\n"
          << "  \"manual_guidance_queue\": \""
          << ManualGuidanceQueuePathLocal().string() << "\",\n"
          << "  \"category_overrides\": \""
          << EvidenceCategoryOverridesPathLocal().string() << "\"\n"
          << "}\n";
  const std::filesystem::path summaryPath =
      ResolveCxVisionRunPath("cxscript_runs/evidence_chain/"
                             "evidence_chain_loaded_summary_debug.json");
  WriteTextFile(summaryPath, summary.str());

  CXLOG_INFO(
      "EvidenceChain", "evidence_chain_ui_loaded_elements", "snapshot_written",
      "groups=" + std::to_string(context.script_evidence_groups.size()) +
          " thumbs=" + std::to_string(totalThumbs) +
          " pending_human_guidance=" + std::to_string(guidanceThumbs) +
          " to_verify_override=" + std::to_string(toVerifyOverrideThumbs) +
          " process_validation_override=" +
          std::to_string(processOverrideThumbs) +
          " display_to_verify=" + std::to_string(displayToVerifyThumbs) +
          " display_process_validation=" +
          std::to_string(displayProcessThumbs) + " detail=" +
          detailPath.string() + " summary=" + summaryPath.string());
}

static void
EnsureStructuredCxImageCatalogEntriesLoaded(ManualTestContext &context) {
  if (!context.catalog_entries.empty())
    return;

  const char *catalogPath =
      "cxparser/cxscript/module/cximage/catalog/cximage_catalog.cxsc";

  CxScriptCatalogRuntime catalog;
  std::string reason;
  if (!LoadCxScriptCatalogFile(catalogPath, catalog, reason)) {
    context.catalog_loaded = false;
    context.catalog_path = catalogPath;
    return;
  }

  context.catalog_entries = catalog.scripts;
  context.catalog_loaded = true;
  context.catalog_path = catalogPath;
}

static bool
EvidenceThumbLooksLikeFindEllipseLocal(const ScriptEvidenceThumb &thumb,
                                       const ScriptEvidenceGroup &group);

static std::string StripEvidenceCandidateDisplaySuffixLocal(std::string value) {
  const std::string marker = " [candidate_";
  std::size_t pos = value.find(marker);
  while (pos != std::string::npos &&
         value.find(']', pos) != std::string::npos) {
    value.erase(pos);
    pos = value.find(marker);
  }
  return value;
}

static std::string EvidenceFolderLabelFromPathPartLocal(std::string value);

static void AppendSavedEvidenceCandidatesLocal(
    ManualTestContext &context,
    const std::function<ScriptEvidenceGroup &(const std::string &)>
        &findGroup) {
  std::vector<std::filesystem::path> roots;
  roots.push_back(ResolveCxVisionRunPath("cxscript_runs/evidence_candidates"));
  const std::filesystem::path repoLocalRoot =
      (ResolveCaseDirectory(".") / "cxscript_runs/evidence_candidates")
          .lexically_normal();
  if (repoLocalRoot != roots.front().lexically_normal())
    roots.push_back(repoLocalRoot);
  std::error_code ec;
  const ActiveManualReviewProjectionLocal projection =
      LoadActiveManualReviewProjectionLocal();
  int selectedBindingCount = 0;
  int retainedUnselectedBindingCount = 0;
  std::vector<std::filesystem::path> bindings;
  for (const auto &root : roots) {
    ec.clear();
    if (!std::filesystem::is_directory(root, ec))
      continue;

    std::filesystem::recursive_directory_iterator it(
        root, std::filesystem::directory_options::skip_permission_denied, ec);
    const std::filesystem::recursive_directory_iterator end;
    for (; !ec && it != end; it.increment(ec)) {
      if (it->is_regular_file(ec) &&
          it->path().filename() == "evidence_binding.json")
        bindings.push_back(it->path());
    }
  }

  auto normalizeCandidateKeyPart = [](std::string value) {
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return TrimLine(value);
  };

  auto buildCandidateDedupKey =
      [&](const ScriptEvidenceThumb &candidate) -> std::string {
    std::ostringstream key;
    const std::string caseName =
        !candidate.case_id.empty()
            ? candidate.case_id
            : (!candidate.source_case_id.empty() ? candidate.source_case_id
                                                 : candidate.script_id);
    if (!caseName.empty())
      key << "case=" << normalizeCandidateKeyPart(caseName);

    const std::string sourceScript =
        !candidate.source_evidence_script_path.empty()
            ? candidate.source_evidence_script_path
            : (!candidate.script_path.empty() ? candidate.script_path
                                              : candidate.script_id);
    if (!sourceScript.empty()) {
      if (!key.str().empty())
        key << "|";
      key << "source="
          << normalizeCandidateKeyPart(std::filesystem::path(sourceScript)
                                           .lexically_normal()
                                           .string());
    }

    if (!candidate.image_id.empty()) {
      if (!key.str().empty())
        key << "|";
      key << "image=" << normalizeCandidateKeyPart(candidate.image_id);
    }
    if (!candidate.target_id.empty()) {
      if (!key.str().empty())
        key << "|";
      key << "target=" << normalizeCandidateKeyPart(candidate.target_id);
    }

    const std::string tool = normalizeCandidateKeyPart(candidate.tool);
    if (!tool.empty()) {
      if (!key.str().empty())
        key << "|";
      key << "tool=" << tool;
    }

    return key.str();
  };

  auto bindWorkingRevisionToOriginal =
      [&](const ScriptEvidenceThumb &candidate) -> bool {
    for (auto &group : context.script_evidence_groups) {
      for (auto &original : group.thumbs) {
        if (original.is_candidate)
          continue;

        const bool sameCase = !candidate.case_id.empty() &&
                              !original.case_id.empty() &&
                              original.case_id == candidate.case_id;
        const bool sameScript = SameEvidenceSourceScriptLocal(
            candidate.source_evidence_script_path, original);
        const bool sameImage = candidate.image_id.empty() ||
                               original.image_id.empty() ||
                               original.image_id == candidate.image_id;
        const bool sameTarget = candidate.target_id.empty() ||
                                original.target_id.empty() ||
                                original.target_id == candidate.target_id;
        const bool sameEvidence =
            sameCase || (sameScript && sameImage && sameTarget);
        if (!sameEvidence)
          continue;

        original.candidate_id = candidate.candidate_id;
        original.candidate_dir = candidate.candidate_dir;
        original.evidence_binding_path = candidate.evidence_binding_path;
        original.parameter_snapshot_path = candidate.parameter_snapshot_path;
        original.runtime_globals_path = candidate.runtime_globals_path;
        original.gauge_annotation_path = candidate.gauge_annotation_path;
        original.working_script_snapshot_path =
            candidate.working_script_snapshot_path;
        original.has_saved_state = candidate.has_saved_state;
        original.source_evidence_script_path =
            candidate.source_evidence_script_path;
        const std::string originalTool =
            NormalizeEvidenceToolTypeLocal(original.tool);
        const std::string candidateTool =
            NormalizeEvidenceToolTypeLocal(candidate.tool);
        if (!candidateTool.empty() &&
            (originalTool.empty() || originalTool == "module" ||
             originalTool == "unknown")) {
          original.tool = candidateTool;
        }
        original.parameter_summary = candidate.parameter_summary;
        original.status = candidate.status;
        original.reason = candidate.reason;
        CXLOG_INFO("EvidenceChain", "working_revision_rebound", "restored",
                   "case_id=" + candidate.case_id +
                       " candidate_id=" + candidate.candidate_id + " match=" +
                       (sameCase ? "case_id" : "script_image_target") +
                       " source=" + candidate.source_evidence_script_path +
                       " original_script=" + original.script_path +
                       " image_id=" + original.image_id +
                       " target_id=" + original.target_id);
        return true;
      }
    }
    return false;
  };

  std::stable_sort(bindings.begin(), bindings.end(),
                   [](const std::filesystem::path &left,
                      const std::filesystem::path &right) {
                     std::error_code leftEc;
                     std::error_code rightEc;
                     const auto leftTime =
                         std::filesystem::last_write_time(left, leftEc);
                     const auto rightTime =
                         std::filesystem::last_write_time(right, rightEc);
                     if (!leftEc && !rightEc && leftTime != rightTime)
                       return rightTime < leftTime;
                     return left.string() < right.string();
                   });

  std::unordered_set<std::string> seenCandidateKeys;

  for (const auto &bindingPath : bindings) {
    std::string binding;
    if (!ReadTextFile(bindingPath.string(), binding))
      continue;

    const std::string candidateId =
        ReadJsonStringFieldLocal(binding, "candidate_id");
    const std::string caseId = ReadJsonStringFieldLocal(binding, "case_id");
    const std::string originalScriptId =
        StripEvidenceCandidateDisplaySuffixLocal(
            ReadJsonStringFieldLocal(binding, "script_id"));
    const std::string scriptSnapshot =
        ReadJsonStringFieldLocal(binding, "script_snapshot_path");
    const std::string imagePath =
        ReadJsonStringFieldLocal(binding, "image_path");
    if (candidateId.empty() || caseId.empty() || scriptSnapshot.empty() ||
        imagePath.empty())
      continue;

    const std::string bindingTool = NormalizeEvidenceToolTypeLocal(
        ReadJsonStringFieldLocal(binding, "tool"));
    if (projection.ManagesTool(bindingTool)) {
      if (!IsSelectedManualReviewBindingLocal(projection, bindingPath)) {
        ++retainedUnselectedBindingCount;
      } else {
        ++selectedBindingCount;
      }
    }

    bool gaugeAccepted = false;
    std::string gaugeReviewStatus;
    bool candidateGaugeValid = false;
    std::string candidateGaugeInvalidReason;
    {
      const std::string gaugePath =
          ReadJsonStringFieldLocal(binding, "gauge_annotation_path");
      const std::string runtimeGlobalsPath =
          ReadJsonStringFieldLocal(binding, "runtime_globals_path");
      std::string gaugeText;
      if (!gaugePath.empty() && ReadTextFile(gaugePath, gaugeText)) {
        ReadJsonBoolFieldLocal(gaugeText, "accepted", gaugeAccepted);
        gaugeReviewStatus =
            ReadJsonStringFieldLocal(gaugeText, "review_status");
      }
      candidateGaugeValid = ValidateCandidateGaugeAnnotationLocal(
          gaugePath, bindingTool, runtimeGlobalsPath,
          candidateGaugeInvalidReason);
    }
    const bool humanConfirmed = candidateGaugeValid && gaugeAccepted &&
                                gaugeReviewStatus == "manual_accepted";

    ScriptEvidenceThumb thumb;
    thumb.is_candidate = candidateGaugeValid;
    thumb.candidate_id = candidateId;
    thumb.candidate_dir = bindingPath.parent_path().string();
    thumb.evidence_binding_path = bindingPath.string();
    thumb.parameter_snapshot_path =
        ReadJsonStringFieldLocal(binding, "parameter_snapshot_path");
    thumb.runtime_globals_path =
        candidateGaugeValid
            ? ReadJsonStringFieldLocal(binding, "runtime_globals_path")
            : std::string();
    thumb.gauge_annotation_path =
        candidateGaugeValid
            ? ReadJsonStringFieldLocal(binding, "gauge_annotation_path")
            : std::string();
    thumb.working_script_snapshot_path =
        candidateGaugeValid ? scriptSnapshot : std::string();
    thumb.has_saved_state = candidateGaugeValid;
    thumb.source_evidence_script_path =
        ReadJsonStringFieldLocal(binding, "source_evidence_script_path");
    if (thumb.source_evidence_script_path.empty())
      thumb.source_evidence_script_path =
          ReadJsonStringFieldLocal(binding, "script_path");
    thumb.case_id = caseId;
    thumb.script_id = originalScriptId.empty() ? caseId : originalScriptId;
    if (thumb.script_id.empty() && !thumb.source_evidence_script_path.empty())
      thumb.script_id = std::filesystem::path(thumb.source_evidence_script_path)
                            .stem()
                            .string();
    if (thumb.script_id.empty() && !scriptSnapshot.empty())
      thumb.script_id = std::filesystem::path(scriptSnapshot).stem().string();
    thumb.script_path = scriptSnapshot;
    thumb.image_id = ReadJsonStringFieldLocal(binding, "image_id");
    thumb.image_path = imagePath;
    thumb.thumbnail_path = imagePath;
    thumb.target_id = ReadJsonStringFieldLocal(binding, "target_id");
    thumb.tool = bindingTool;
    thumb.parameter_summary =
        ReadJsonStringFieldLocal(binding, "parameter_summary");

    const std::string candidateDirName =
        bindingPath.parent_path().parent_path().filename().string();
    const std::string evidenceCaseFolderLabel =
        EvidenceFolderLabelFromPathPartLocal(candidateDirName);

    if (!candidateGaugeValid) {
      thumb.status = "invalid_saved_candidate";
      thumb.evidence_category_override = "Defect";
      thumb.reason = "saved evidence candidate is not restorable: " +
                     candidateGaugeInvalidReason +
                     "; candidate_id=" + candidateId +
                     "; candidate_dir=" + thumb.candidate_dir;
      CXLOG_WARN("EvidenceChain", "candidate_package_invalid", "invalid_gauge",
                 "case_id=" + caseId + " candidate_id=" + candidateId +
                     " tool=" + thumb.tool +
                     " reason=" + candidateGaugeInvalidReason);
    } else {
      thumb.status = humanConfirmed ? "manual_confirmed_candidate"
                                    : "pending_human_review";
      thumb.reason =
          std::string(
              humanConfirmed
                  ? "manual accepted evidence candidate"
                  : "saved evidence candidate pending manual acceptance") +
          "; candidate_id=" + candidateId +
          "; candidate_dir=" + thumb.candidate_dir +
          "; gauge_accepted=" + (gaugeAccepted ? "true" : "false") +
          "; gauge_review_status=" + gaugeReviewStatus;
    }

    std::string analysis;
    const std::filesystem::path analysisPath =
        bindingPath.parent_path() / "analysis_state.json";
    if (ReadTextFile(analysisPath.string(), analysis)) {
      thumb.primary_object_type =
          ReadJsonStringFieldLocal(analysis, "primary_object_type");
      thumb.primary_object_name =
          ReadJsonStringFieldLocal(analysis, "primary_object_name");
      thumb.primary_object_status =
          ReadJsonStringFieldLocal(analysis, "primary_object_status");
    }
    PopulateEditableObjectBindingForThumbLocal(thumb);

    if (thumb.parameter_summary.empty() ||
        thumb.parameter_summary.find('=') == std::string::npos)
      thumb.parameter_summary =
          BuildDefaultEvidenceParamSummaryForScript(scriptSnapshot);

    const std::string dedupKey = buildCandidateDedupKey(thumb);
    if (seenCandidateKeys.find(dedupKey) != seenCandidateKeys.end())
      continue;

    const bool reboundToOriginal =
        candidateGaugeValid && bindWorkingRevisionToOriginal(thumb);
    if (reboundToOriginal &&
        EvidenceThumbLooksLikeFindEllipseLocal(thumb, ScriptEvidenceGroup{}))
      continue;

    const std::string baseLabel = evidenceCaseFolderLabel.empty()
                                      ? "Saved Candidates"
                                      : evidenceCaseFolderLabel;
    const std::string candidateTool =
        NormalizeEvidenceToolTypeLocal(thumb.tool);
    const std::string groupLabel =
        candidateTool.empty() ? baseLabel : (baseLabel + " / " + candidateTool);

    if (reboundToOriginal) {
      seenCandidateKeys.insert(dedupKey);
      continue;
    }

    ScriptEvidenceGroup &group = findGroup(groupLabel);
    bool exists = false;
    for (const auto &existing : group.thumbs) {
      if (existing.is_candidate &&
          existing.candidate_id == thumb.candidate_id &&
          existing.case_id == thumb.case_id) {
        exists = true;
        break;
      }
      if (!existing.is_candidate && existing.case_id == thumb.case_id &&
          existing.script_id == thumb.script_id) {
        exists = true;
        break;
      }
    }

    seenCandidateKeys.insert(dedupKey);
    if (!exists)
      group.thumbs.push_back(std::move(thumb));
  }
  std::ostringstream projectionDebug;
  projectionDebug << "manifest\t" << projection.manifest_path.string() << '\n';
  projectionDebug << "managed_tools\t" << projection.managed_tools.size()
                  << '\n';
  projectionDebug << "discovered_bindings\t" << bindings.size() << '\n';
  projectionDebug << "selected_bindings\t" << selectedBindingCount << '\n';
  projectionDebug << "retained_unselected_bindings\t"
                  << retainedUnselectedBindingCount << '\n';
  for (const auto &root : roots)
    projectionDebug << "scan_root\t" << root.string() << '\n';
  const std::filesystem::path projectionDebugPath = ResolveCxVisionRunPath(
      "cxscript_runs/evidence_chain/manual_review_projection_scan_debug.tsv");
  WriteTextFile(projectionDebugPath, projectionDebug.str());
}

static int ApplyActiveManualReviewProjectionLocal(
    ManualTestContext &context, std::string &reason) {
  reason.clear();
  const ActiveManualReviewProjectionLocal projection =
      LoadActiveManualReviewProjectionLocal();
  if (projection.managed_tools.empty())
    return 0;

  int before = 0;
  std::vector<std::string> selectedRows;
  for (auto &group : context.script_evidence_groups) {
    before += static_cast<int>(group.thumbs.size());
    for (const ScriptEvidenceThumb &thumb : group.thumbs) {
      const bool managed =
          projection.ManagesTool(thumb.tool) || projection.ManagesTool(group.label);
      if (!managed || thumb.evidence_binding_path.empty() ||
          !IsSelectedManualReviewBindingLocal(projection,
                                             thumb.evidence_binding_path))
        continue;
      std::ostringstream row;
      row << NormalizeEvidenceToolTypeLocal(thumb.tool) << '\t'
          << (thumb.script_id.empty() ? thumb.case_id : thumb.script_id)
          << '\t' << thumb.case_id << '\t' << thumb.evidence_binding_path;
      selectedRows.push_back(row.str());
    }
  }

  int after = 0;
  for (const auto &group : context.script_evidence_groups)
    after += static_cast<int>(group.thumbs.size());

  std::ostringstream debug;
  debug << "manifest\t" << projection.manifest_path.string() << '\n';
  debug << "managed_tools\t" << projection.managed_tools.size() << '\n';
  debug << "selected_bindings\t"
        << projection.selected_binding_paths.size() << '\n';
  debug << "projected_selected_rows\t" << selectedRows.size() << '\n';
  debug << "tool\treview_item\tcase_id\tevidence_binding_path\n";
  for (const std::string &row : selectedRows)
    debug << row << '\n';
  debug << "before\t" << before << '\n';
  debug << "after\t" << after << '\n';
  debug << "removed\t0\n";
  debug << "policy\tprojection_retains_asset_backed_cases\n";
  const std::filesystem::path debugPath = ResolveCxVisionRunPath(
      "cxscript_runs/evidence_chain/manual_review_projection_apply_debug.tsv");
  WriteTextFile(debugPath, debug.str());

  std::ostringstream summary;
  summary << "active manual review projection managed_tools="
          << projection.managed_tools.size()
          << " selected_bindings="
          << projection.selected_binding_paths.size()
          << " removed=0 remaining=" << after
          << " manifest=" << projection.manifest_path.string()
          << " debug=" << debugPath.string();
  reason = summary.str();
  return 0;
}

static bool LooksLikeCxScriptPathLocal(const std::string &value) {
  if (value.empty())
    return false;
  if (value.find('/') != std::string::npos ||
      value.find('\\') != std::string::npos)
    return true;

  std::string lower = value;
  std::transform(
      lower.begin(), lower.end(), lower.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return lower.size() >= 5 && lower.compare(lower.size() - 5, 5, ".cxsc") == 0;
}

static std::string DeriveEvidenceScriptIdLocal(const std::string &scriptValue) {
  if (scriptValue.empty())
    return {};
  if (!LooksLikeCxScriptPathLocal(scriptValue))
    return StripEvidenceCandidateDisplaySuffixLocal(scriptValue);

  std::filesystem::path path(scriptValue);
  std::string id = path.stem().string();
  return StripEvidenceCandidateDisplaySuffixLocal(id.empty() ? scriptValue
                                                             : id);
}

static std::string
ResolveEvidenceChainScriptPathLocal(const ManualTestContext &context,
                                    const std::string &scriptValue) {
  if (scriptValue.empty())
    return {};
  if (LooksLikeCxScriptPathLocal(scriptValue))
    return scriptValue;

  for (const auto &entry : context.catalog_entries) {
    if (entry.script_id == scriptValue)
      return entry.path;
  }
  return {};
}

static std::string
ResolveEvidenceChainImagePathLocal(const ManualTestContext &context,
                                   const std::string &imageId) {
  if (imageId.empty())
    return {};
  for (const auto &item : context.image_manifest_items) {
    if (item.image_id == imageId && !item.image_path.empty())
      return item.image_path;
  }
  return ResolveEvidenceImagePathFromContextLocal(context, imageId);
}

static bool
HasEvidenceChainThumbIdentityLocal(const ManualTestContext &context,
                                   const ScriptEvidenceThumb &candidate) {
  for (const auto &group : context.script_evidence_groups) {
    for (const auto &thumb : group.thumbs) {
      const bool sameCase =
          !candidate.case_id.empty() && thumb.case_id == candidate.case_id;
      const bool sameScript = !candidate.script_id.empty() &&
                              thumb.script_id == candidate.script_id;
      const bool samePath = !candidate.script_path.empty() &&
                            thumb.script_path == candidate.script_path;
      const bool sameImageTarget = candidate.image_id == thumb.image_id &&
                                   candidate.target_id == thumb.target_id;

      if (sameCase && (sameScript || samePath || sameImageTarget))
        return true;
    }
  }
  return false;
}

static std::string
EvidenceThumbCaseNameLocal(const ScriptEvidenceThumb &thumb) {
  std::string value;
  if (!thumb.case_id.empty())
    value = thumb.case_id;
  else if (!thumb.source_case_id.empty())
    value = thumb.source_case_id;
  else if (!thumb.script_id.empty())
    value = thumb.script_id;
  else if (!thumb.script_path.empty())
    value = std::filesystem::path(thumb.script_path).stem().string();

  const std::size_t candidateSuffix = value.find(" [");
  if (candidateSuffix != std::string::npos)
    value.erase(candidateSuffix);
  return value;
}

static std::string LowerEvidenceKeyLocal(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

static bool
EvidenceThumbLooksLikeFindEllipseLocal(const ScriptEvidenceThumb &thumb,
                                       const ScriptEvidenceGroup &group) {
  const std::string normalizedTool = NormalizeEvidenceToolTypeLocal(
      thumb.tool.empty() ? group.label : thumb.tool);
  if (normalizedTool == "FindEllipse")
    return true;

  const std::string key = LowerEvidenceKeyLocal(
      thumb.case_id + " " + thumb.source_case_id + " " + thumb.script_id + " " +
      thumb.script_path + " " + thumb.source_evidence_script_path + " " +
      thumb.reason + " " + group.label);
  return key.find("findellipse") != std::string::npos ||
         key.find("find_ellipse") != std::string::npos;
}

static std::string
FindEllipseCaseDedupeKeyLocal(const ScriptEvidenceThumb &thumb) {
  std::string caseName = EvidenceThumbCaseNameLocal(thumb);
  if (caseName.empty())
    caseName = thumb.case_id;
  if (caseName.empty())
    caseName = thumb.source_case_id;
  if (caseName.empty())
    caseName = thumb.script_id;
  if (caseName.empty())
    caseName = thumb.script_path;
  if (caseName.empty())
    caseName = thumb.source_evidence_script_path;
  if (caseName.empty())
    caseName = thumb.image_id + "|" + thumb.target_id;
  return LowerEvidenceKeyLocal(caseName);
}

static bool FindEllipseThumbIsVerifiedLocal(const ScriptEvidenceThumb &thumb) {
  const std::string key =
      LowerEvidenceKeyLocal(thumb.evidence_category_override + " " +
                            thumb.status + " " + thumb.reason);
  return key.find("verified") != std::string::npos;
}

static int FindEllipseThumbRestoreScoreLocal(const ScriptEvidenceThumb &thumb) {
  int score = 0;
  if (thumb.has_saved_state)
    score += 1000;
  if (thumb.is_candidate)
    score += 300;
  if (!thumb.runtime_globals_path.empty())
    score += 250;
  if (!thumb.gauge_annotation_path.empty())
    score += 250;
  if (!thumb.working_script_snapshot_path.empty())
    score += 250;
  if (!thumb.candidate_dir.empty())
    score += 150;
  if (!thumb.evidence_binding_path.empty())
    score += 120;
  if (!thumb.parameter_snapshot_path.empty())
    score += 80;
  if (FindEllipseThumbIsVerifiedLocal(thumb))
    score += 70;
  if (thumb.parameter_summary.find('=') != std::string::npos)
    score += 40;
  if (!thumb.image_path.empty())
    score += 20;
  if (!thumb.script_path.empty())
    score += 10;
  return score;
}

static void MergeFindEllipseThumbPayloadLocal(ScriptEvidenceThumb &dst,
                                              const ScriptEvidenceThumb &src) {
  auto copyIfEmpty = [](std::string &target, const std::string &value) {
    if (target.empty() && !value.empty())
      target = value;
  };

  copyIfEmpty(dst.candidate_id, src.candidate_id);
  copyIfEmpty(dst.candidate_dir, src.candidate_dir);
  copyIfEmpty(dst.evidence_binding_path, src.evidence_binding_path);
  copyIfEmpty(dst.parameter_snapshot_path, src.parameter_snapshot_path);
  copyIfEmpty(dst.runtime_globals_path, src.runtime_globals_path);
  copyIfEmpty(dst.gauge_annotation_path, src.gauge_annotation_path);
  copyIfEmpty(dst.working_script_snapshot_path,
              src.working_script_snapshot_path);
  copyIfEmpty(dst.source_evidence_script_path, src.source_evidence_script_path);
  copyIfEmpty(dst.case_id, src.case_id);
  copyIfEmpty(dst.script_id, src.script_id);
  copyIfEmpty(dst.script_path, src.script_path);
  copyIfEmpty(dst.image_id, src.image_id);
  copyIfEmpty(dst.image_path, src.image_path);
  copyIfEmpty(dst.thumbnail_path, src.thumbnail_path);
  copyIfEmpty(dst.target_id, src.target_id);
  copyIfEmpty(dst.tool, src.tool);
  copyIfEmpty(dst.parameter_summary, src.parameter_summary);
  copyIfEmpty(dst.evidence_output_root, src.evidence_output_root);
  copyIfEmpty(dst.contract_id, src.contract_id);
  copyIfEmpty(dst.expected_result, src.expected_result);
  copyIfEmpty(dst.expected_policy_guard, src.expected_policy_guard);
  copyIfEmpty(dst.evidence_level, src.evidence_level);
  copyIfEmpty(dst.evidence_case_role, src.evidence_case_role);
  copyIfEmpty(dst.source_case_id, src.source_case_id);
  copyIfEmpty(dst.evidence_head_folder, src.evidence_head_folder);
  copyIfEmpty(dst.evidence_case_folder, src.evidence_case_folder);
  copyIfEmpty(dst.primary_object_type, src.primary_object_type);
  copyIfEmpty(dst.primary_object_name, src.primary_object_name);
  copyIfEmpty(dst.primary_object_status, src.primary_object_status);

  if (dst.dataset_images.empty() && !src.dataset_images.empty())
    dst.dataset_images = src.dataset_images;
  if (dst.annotations.empty() && !src.annotations.empty())
    dst.annotations = src.annotations;

  dst.has_saved_state = dst.has_saved_state || src.has_saved_state;
  dst.is_candidate = dst.is_candidate || src.is_candidate;
  dst.manual_review_required =
      dst.manual_review_required || src.manual_review_required;
  dst.promotion_candidate = dst.promotion_candidate || src.promotion_candidate;

  if (FindEllipseThumbIsVerifiedLocal(src)) {
    dst.evidence_category_override = "Verified";
    dst.status = "verified";
    if (dst.reason.empty() ||
        dst.reason.find("manual category:") != std::string::npos) {
      dst.reason =
          src.reason.empty() ? "manual category: verified" : src.reason;
    }
  } else {
    copyIfEmpty(dst.evidence_category_override, src.evidence_category_override);
    copyIfEmpty(dst.status, src.status);
    copyIfEmpty(dst.reason, src.reason);
  }
}

static void
PruneFindEllipseDuplicateCasesByNameLocal(ManualTestContext &context) {
  std::vector<std::string> caseOrder;
  std::unordered_map<std::string, ScriptEvidenceThumb> bestThumbByCase;
  std::unordered_map<std::string, int> bestScoreByCase;

  for (const ScriptEvidenceGroup &group : context.script_evidence_groups) {
    for (const ScriptEvidenceThumb &thumb : group.thumbs) {
      if (!EvidenceThumbLooksLikeFindEllipseLocal(thumb, group))
        continue;

      const std::string caseKey = FindEllipseCaseDedupeKeyLocal(thumb);
      if (caseKey.empty())
        continue;

      auto found = bestThumbByCase.find(caseKey);
      if (found == bestThumbByCase.end()) {
        if (caseOrder.size() >= 3)
          continue;
        caseOrder.push_back(caseKey);
        bestThumbByCase[caseKey] = thumb;
        bestScoreByCase[caseKey] = FindEllipseThumbRestoreScoreLocal(thumb);
        continue;
      }

      const int score = FindEllipseThumbRestoreScoreLocal(thumb);
      if (score > bestScoreByCase[caseKey]) {
        ScriptEvidenceThumb merged = thumb;
        MergeFindEllipseThumbPayloadLocal(merged, found->second);
        found->second = std::move(merged);
        bestScoreByCase[caseKey] = score;
      } else {
        MergeFindEllipseThumbPayloadLocal(found->second, thumb);
      }
    }
  }

  std::unordered_map<std::string, bool> emitted;
  for (ScriptEvidenceGroup &group : context.script_evidence_groups) {
    std::vector<ScriptEvidenceThumb> kept;
    kept.reserve(group.thumbs.size());

    for (ScriptEvidenceThumb &thumb : group.thumbs) {
      if (!EvidenceThumbLooksLikeFindEllipseLocal(thumb, group)) {
        kept.push_back(std::move(thumb));
        continue;
      }

      const std::string caseKey = FindEllipseCaseDedupeKeyLocal(thumb);
      auto best = bestThumbByCase.find(caseKey);
      if (best == bestThumbByCase.end() || emitted[caseKey])
        continue;

      kept.push_back(best->second);
      emitted[caseKey] = true;
    }

    group.thumbs.swap(kept);
  }
}

struct EvidenceChainFolderPlacementLocal {
  std::string category;
  std::string group;
};

static std::string EvidenceFolderLabelFromPathPartLocal(std::string value) {
  value = std::filesystem::path(value).filename().string();
  const std::size_t dot = value.find('.');
  const std::size_t sep = value.find_first_of("_-");
  if (dot != std::string::npos && sep != std::string::npos && dot < sep)
    value.erase(0, sep + 1);
  for (char &ch : value) {
    if (ch == '_' || ch == '-')
      ch = ' ';
  }
  return TrimLine(value);
}

static EvidenceChainFolderPlacementLocal
ResolveEvidenceChainFolderPlacementLocal(const std::filesystem::path &root,
                                         const std::filesystem::path &file) {
  EvidenceChainFolderPlacementLocal placement;
  std::error_code ec;
  const std::filesystem::path rel = std::filesystem::relative(file, root, ec);
  if (ec)
    return placement;

  std::vector<std::string> dirs;
  for (const auto &part : rel.parent_path()) {
    const std::string label =
        EvidenceFolderLabelFromPathPartLocal(part.string());
    if (!label.empty())
      dirs.push_back(label);
  }

  if (!dirs.empty())
    placement.category = dirs.front();
  if (dirs.size() >= 2)
    placement.group = dirs[1];
  return placement;
}

static void LoadSavedEvidenceManualReviewLocal(ScriptEvidenceThumb &thumb) {
  if (thumb.case_id.empty())
    return;
  std::string safeCase = thumb.case_id;
  for (char &ch : safeCase) {
    const unsigned char value = static_cast<unsigned char>(ch);
    if (!std::isalnum(value) && ch != '-' && ch != '_')
      ch = '_';
  }
  const std::filesystem::path reviewPath =
      ResolveCxVisionRunPath("cxscript_runs/evidence_chain/manual_reviews/" +
                             safeCase + "/human_review.json");
  if (!std::filesystem::is_regular_file(reviewPath))
    return;

  cv::FileStorage storage(reviewPath.string(), cv::FileStorage::READ);
  if (!storage.isOpened())
    return;
  std::string decision;
  storage["decision"] >> decision;
  if (decision == "MANUAL_GUI_PASS")
    thumb.status = "manual_gui_pass";
  else if (decision == "MANUAL_GUI_PARTIAL")
    thumb.status = "manual_gui_partial";
  else if (decision == "MANUAL_GUI_FAIL")
    thumb.status = "manual_gui_fail";
  else
    return;
  thumb.reason += "; persisted_review=" + reviewPath.string();
}

static int AppendCxScriptEvidenceChainFilesLocal(
    ManualTestContext &context,
    const std::function<ScriptEvidenceGroup &(const std::string &)> &findGroup,
    const std::vector<std::string> &fallbackImages,
    std::unordered_map<std::string, std::size_t> &fallbackImageIndexByPool,
    const std::unordered_set<std::string> *selectedCaseKeys,
    std::string &reason) {
  reason.clear();
  const std::filesystem::path root =
      ResolveWorkspaceFile("cxparser/cxscript/module/cximage/evidence");
  std::error_code ec;
  if (!std::filesystem::is_directory(root, ec)) {
    reason = "cxscript evidence chain root not found: " + root.string();
    return 0;
  }

  std::vector<std::filesystem::path> files;
  std::filesystem::recursive_directory_iterator it(
      root, std::filesystem::directory_options::skip_permission_denied, ec);
  const std::filesystem::recursive_directory_iterator end;
  for (; !ec && it != end; it.increment(ec)) {
    if (!it->is_regular_file(ec))
      continue;
    const std::filesystem::path path = it->path();
    if (path.extension() == ".cxsc")
      files.push_back(path);
  }

  std::stable_sort(files.begin(), files.end(),
                   [](const std::filesystem::path &left,
                      const std::filesystem::path &right) {
                     return left.string() < right.string();
                   });

  int appended = 0;
  int loadedFiles = 0;
  std::vector<std::string> errors;
  for (const auto &file : files) {
    CxScriptEvidenceChainRuntime chain;
    std::string loadReason;
    if (!LoadCxScriptEvidenceChainFile(file.string(), chain, loadReason)) {
      errors.push_back(file.filename().string() + ": " + loadReason);
      continue;
    }
    ++loadedFiles;
    const EvidenceChainFolderPlacementLocal folderPlacement =
        ResolveEvidenceChainFolderPlacementLocal(root, file);

    for (const CxScriptEvidenceCase &c : chain.cases) {
      const std::string catalogKey =
          file.lexically_normal().generic_string() + "::" + c.evidence_id;
      if (selectedCaseKeys != nullptr &&
          selectedCaseKeys->find(catalogKey) == selectedCaseKeys->end()) {
        continue;
      }
      const auto existingCase = std::find_if(
          context.evidence_items.begin(), context.evidence_items.end(),
          [&](const ManualEvidenceItem &item) {
            return item.case_id == c.evidence_id;
          });
      if (existingCase == context.evidence_items.end()) {
        ManualEvidenceItem item;
        item.case_id = c.evidence_id;
        item.level = c.level;
        item.image_id = c.image_id;
        item.target_id = c.target_id;
        item.tool = NormalizeEvidenceToolTypeLocal(c.tool);
        item.script_id = DeriveEvidenceScriptIdLocal(c.script_id);
        item.parameter_profile_id = c.parameter_profile_id;
        item.gauge_status = c.annotations.empty() ? "unannotated" : "annotated";
        item.probe_status = "pending";
        item.contract_status = c.contract_id.empty() ? "missing" : "pending";
        item.review_status =
            c.manual_review_required ? "pending_human_review" : "unreviewed";
        item.image_path =
            ResolveEvidenceChainImagePathLocal(context, c.image_id);
        item.source_evidence_chain_path = file.string();
        context.evidence_items.push_back(std::move(item));
      }
      ScriptEvidenceThumb thumb;
      thumb.case_id = c.evidence_id;
      thumb.review_item = c.display_name;
      thumb.script_id = DeriveEvidenceScriptIdLocal(c.script_id);
      thumb.script_path =
          ResolveEvidenceChainScriptPathLocal(context, c.script_id);
      thumb.source_evidence_script_path = file.string();
      thumb.image_id = c.image_id;
      thumb.image_path =
          ResolveEvidenceChainImagePathLocal(context, c.image_id);
      thumb.thumbnail_path = thumb.image_path;
      thumb.target_id = c.target_id;
      thumb.tool = NormalizeEvidenceToolTypeLocal(c.tool);
      thumb.parameter_summary = c.parameter_profile_id;
      thumb.evidence_output_root = chain.output_root;
      thumb.contract_id = c.contract_id;
      thumb.expected_result = c.expected_result;
      thumb.expected_policy_guard = c.expected_policy_guard;
      thumb.evidence_level = c.level;
      thumb.evidence_case_role = c.case_role;
      thumb.source_case_id = c.source_case_id;
      thumb.manual_review_required = c.manual_review_required;
      thumb.promotion_candidate = c.promotion_candidate;
      thumb.evidence_category_override = c.display_category;
      thumb.evidence_group_override = c.display_group;
      thumb.evidence_head_folder = folderPlacement.category;
      thumb.evidence_case_folder = folderPlacement.group;
      const std::string groupLabel =
          !folderPlacement.group.empty()
              ? folderPlacement.group
              : (!c.display_group.empty()
                     ? c.display_group
                     : std::filesystem::path(file).stem().string());
      thumb.workflow_id = c.workflow_id;
      thumb.workflow_stage = c.workflow_stage;
      thumb.workflow_status = c.workflow_status;
      thumb.workflow_prerequisites = c.workflow_prerequisites;
      thumb.dataset_role = c.dataset_role;
      thumb.annotation_policy = c.annotation_policy;
      thumb.gate_policy = c.gate_policy;
      thumb.parent_model_ref = c.parent_model_ref;
      thumb.child_model_ref = c.child_model_ref;
      thumb.workflow_stage_index = c.workflow_stage_index;
      thumb.workflow_stage_count = c.workflow_stage_count;
      thumb.dataset_frozen = c.dataset_frozen;
      thumb.admission_status = c.admission_status;
      thumb.admission_reason = c.admission_reason;
      thumb.dataset_summary_ref = c.dataset_summary_ref;
      thumb.training_receipt_ref = c.training_receipt_ref;
      thumb.candidate_artifact_ref = c.candidate_artifact_ref;
      thumb.evaluation_report_ref = c.evaluation_report_ref;
      thumb.evidence_bundle_ref = c.evidence_bundle_ref;
      thumb.rollback_model_ref = c.rollback_model_ref;
      thumb.quality_conclusion_ref = c.quality_conclusion_ref;
      thumb.training_config_ref = c.training_config_ref;
      thumb.model_manifest_ref = c.model_manifest_ref;
      thumb.inference_config_ref = c.inference_config_ref;
      thumb.quality_policy_ref = c.quality_policy_ref;
      thumb.ontology_ref = c.ontology_ref;
      thumb.failure_samples_ref = c.failure_samples_ref;
      thumb.status =
          c.manual_review_required ? "pending_human_review" : "ready";
      thumb.reason = "cxscript evidence chain: " + chain.chain_id +
                     "; role=" + c.case_role +
                     "; expected=" + c.expected_result +
                     "; policy=" + c.expected_policy_guard;

      for (const CxScriptEvidenceDatasetImage &image : c.dataset_images) {
        CxEvidenceDatasetImageBinding binding;
        binding.image_id = image.image_id;
        binding.image_path = image.image_path;
        binding.split = image.split;
        binding.label = image.label;
        binding.source = image.source;
        thumb.dataset_images.push_back(std::move(binding));
      }

      for (const CxScriptEvidenceAnnotation &annotation : c.annotations) {
        CxEvidenceAnnotationBinding binding;
        binding.image_id = annotation.image_id;
        binding.shape_kind = annotation.shape_kind;
        binding.semantic_role = annotation.semantic_role;
        binding.owner_binding = annotation.owner_binding;
        binding.label = annotation.label;
        binding.class_id = annotation.class_id;
        binding.x0 = annotation.x0;
        binding.y0 = annotation.y0;
        binding.x1 = annotation.x1;
        binding.y1 = annotation.y1;
        binding.normalized = annotation.normalized;
        binding.closed = annotation.closed;
        binding.points_xy = annotation.points_xy;
        thumb.annotations.push_back(std::move(binding));
      }

      if (thumb.parameter_summary.empty() ||
          thumb.parameter_summary.find('=') == std::string::npos) {
        thumb.parameter_summary =
            BuildDefaultEvidenceParamSummaryForScript(thumb.script_path);
      }

      ApplyHDReferenceImageBindingLocal(thumb);
      if (thumb.workflow_id.empty()) {
        AssignFallbackImageToThumb(thumb, fallbackImages,
                                   fallbackImageIndexByPool);
      }
      PopulateEditableObjectBindingForThumbLocal(thumb);
      LoadSavedEvidenceManualReviewLocal(thumb);

      if (HasEvidenceChainThumbIdentityLocal(context, thumb))
        continue;

      findGroup(groupLabel).thumbs.push_back(std::move(thumb));
      ++appended;
    }
  }

  std::ostringstream oss;
  oss << "cxscript evidence chains loaded files=" << loadedFiles
      << " appended_cases=" << appended;
  if (!errors.empty())
    oss << " skipped=" << errors.size() << " first_error=" << errors.front();
  reason = oss.str();
  return appended;
}

static void RefreshHiddenEvidenceCaseCatalogLocal(ManualTestContext &context) {
  if (!context.hidden_evidence_case_catalog_dirty)
    return;
  context.hidden_evidence_case_catalog.clear();

  const std::filesystem::path root =
      ResolveWorkspaceFile("cxparser/cxscript/module/cximage/evidence");
  std::error_code ec;
  if (!std::filesystem::is_directory(root, ec))
    return;

  std::vector<std::filesystem::path> files;
  std::filesystem::recursive_directory_iterator it(
      root, std::filesystem::directory_options::skip_permission_denied, ec);
  const std::filesystem::recursive_directory_iterator end;
  for (; !ec && it != end; it.increment(ec)) {
    if (it->is_regular_file(ec) && it->path().extension() == ".cxsc")
      files.push_back(it->path());
  }
  std::stable_sort(files.begin(), files.end());
  for (const std::filesystem::path &file : files) {
    CxScriptEvidenceChainRuntime chain;
    std::string ignored;
    if (!LoadCxScriptEvidenceChainFile(file.string(), chain, ignored))
      continue;
    for (const CxScriptEvidenceCase &c : chain.cases) {
      ScriptEvidenceThumb entry;
      entry.case_id = c.evidence_id;
      entry.review_item = c.display_name.empty() ? c.evidence_id : c.display_name;
      entry.source_evidence_script_path =
          file.lexically_normal().generic_string();
      entry.tool = NormalizeEvidenceToolTypeLocal(c.tool);
      entry.evidence_level = c.level;
      entry.evidence_case_role = c.case_role;
      entry.workflow_id = c.workflow_id;
      entry.workflow_status = c.workflow_status;
      entry.admission_status = c.admission_status;
      entry.parameter_summary = c.parameter_profile_id;
      entry.expected_result = c.expected_result;
      entry.expected_policy_guard = c.expected_policy_guard;
      context.hidden_evidence_case_catalog.push_back(std::move(entry));
      const std::string caseKey =
          file.lexically_normal().generic_string() + "::" + c.evidence_id;
      if (c.manual_visible &&
          context.operator_hidden_evidence_case_keys.find(caseKey) ==
              context.operator_hidden_evidence_case_keys.end()) {
        context.selected_hidden_evidence_case_keys.insert(caseKey);
      }
    }
  }
  context.hidden_evidence_case_catalog_dirty = false;
}

struct AssetCaseScanRejectionLocal {
  std::string manifest_path;
  std::string reason;
};

static std::string FileNodeStringLocal(const cv::FileNode &node,
                                       const char *key) {
  const cv::FileNode value = node[key];
  return value.empty() ? std::string() : static_cast<std::string>(value);
}

static bool IsAssetCasePathWithinRootLocal(const std::filesystem::path &root,
                                           const std::filesystem::path &path) {
  std::error_code ec;
  const std::filesystem::path canonicalRoot =
      std::filesystem::weakly_canonical(root, ec);
  if (ec)
    return false;
  const std::filesystem::path canonicalPath =
      std::filesystem::weakly_canonical(path, ec);
  if (ec)
    return false;
  const std::filesystem::path relative =
      std::filesystem::relative(canonicalPath, canonicalRoot, ec);
  if (ec)
    return false;
  const auto first = relative.begin();
  return first == relative.end() || *first != "..";
}

static int AppendTypedLabelDraftAnnotationsLocal(
    const std::filesystem::path &typedLabelPath,
    const std::string &internalCaseId, const std::string &label,
    int classId, std::vector<CxEvidenceAnnotationBinding> &annotations) {
  cv::Mat typedLabel = cv::imread(typedLabelPath.string(), cv::IMREAD_GRAYSCALE);
  if (typedLabel.empty())
    return 0;

  cv::Mat binary;
  cv::threshold(typedLabel, binary, 0, 255, cv::THRESH_BINARY);
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(binary, contours, cv::RETR_EXTERNAL,
                   cv::CHAIN_APPROX_SIMPLE);
  std::sort(contours.begin(), contours.end(),
            [](const std::vector<cv::Point> &left,
               const std::vector<cv::Point> &right) {
              return std::abs(cv::contourArea(left)) >
                     std::abs(cv::contourArea(right));
            });

  int appended = 0;
  for (const std::vector<cv::Point> &contour : contours) {
    if (std::abs(cv::contourArea(contour)) < 4.0)
      continue;
    std::vector<cv::Point> polygon;
    cv::approxPolyDP(contour, polygon,
                     std::max(1.0, cv::arcLength(contour, true) * 0.005), true);
    if (polygon.size() < 3)
      continue;

    CxEvidenceAnnotationBinding binding;
    binding.image_id = internalCaseId;
    binding.shape_kind = "PolylineShape";
    binding.semantic_role = "mask_polygon";
    binding.owner_binding = "typed_label";
    binding.label = label;
    binding.class_id = classId;
    binding.normalized = false;
    binding.closed = true;
    for (const cv::Point &point : polygon) {
      binding.points_xy.push_back(static_cast<float>(point.x));
      binding.points_xy.push_back(static_cast<float>(point.y));
    }
    annotations.push_back(std::move(binding));
    ++appended;
  }
  return appended;
}

// Asset cases are immutable test inputs.  Operator edits are stored beneath
// each case in working_state/<logical-case-id>/current and re-applied when the
// same asset case is selected again.  The loader deliberately identifies this
// only from the case package and binding facts, never from a case name.
static bool RestoreAssetCaseWorkingStateLocal(
    const std::filesystem::path &caseDirectory,
    const std::string &internalCaseId, ScriptEvidenceThumb &thumb,
    std::string &reason) {
  reason.clear();
  if (caseDirectory.empty() || internalCaseId.empty())
    return false;

  const std::filesystem::path workingRoot = caseDirectory / "working_state";
  std::error_code ec;
  if (!std::filesystem::is_directory(workingRoot, ec) ||
      std::filesystem::is_symlink(workingRoot, ec)) {
    return false;
  }

  std::filesystem::recursive_directory_iterator it(
      workingRoot, std::filesystem::directory_options::skip_permission_denied,
      ec);
  const std::filesystem::recursive_directory_iterator end;
  for (; !ec && it != end; it.increment(ec)) {
    if (!it->is_regular_file(ec) ||
        it->path().filename() != "evidence_binding.json") {
      continue;
    }
    const std::filesystem::path bindingPath = it->path();
    if (!IsAssetCasePathWithinRootLocal(caseDirectory, bindingPath) ||
        std::filesystem::is_symlink(bindingPath, ec)) {
      continue;
    }
    std::string binding;
    if (!ReadTextFile(bindingPath.string(), binding) ||
        ReadJsonStringFieldLocal(binding, "case_id") != internalCaseId) {
      continue;
    }

    const std::filesystem::path candidateDir = bindingPath.parent_path();
    const std::string candidateId =
        ReadJsonStringFieldLocal(binding, "candidate_id");
    const std::string scriptSnapshot =
        ReadJsonStringFieldLocal(binding, "script_snapshot_path");
    const std::string runtimeGlobals =
        ReadJsonStringFieldLocal(binding, "runtime_globals_path");
    const std::string gaugeAnnotation =
        ReadJsonStringFieldLocal(binding, "gauge_annotation_path");
    const std::filesystem::path scriptPath(scriptSnapshot);
    const std::filesystem::path globalsPath(runtimeGlobals);
    const std::filesystem::path gaugePath(gaugeAnnotation);
    if (candidateId.empty() || scriptSnapshot.empty() || runtimeGlobals.empty() ||
        gaugeAnnotation.empty() ||
        !std::filesystem::is_regular_file(scriptPath, ec) ||
        !std::filesystem::is_regular_file(globalsPath, ec) ||
        !std::filesystem::is_regular_file(gaugePath, ec)) {
      continue;
    }

    thumb.candidate_id = candidateId;
    thumb.candidate_dir = candidateDir.string();
    thumb.evidence_binding_path = bindingPath.string();
    thumb.parameter_snapshot_path =
        ReadJsonStringFieldLocal(binding, "parameter_snapshot_path");
    thumb.runtime_globals_path = runtimeGlobals;
    thumb.gauge_annotation_path = gaugeAnnotation;
    thumb.working_script_snapshot_path = scriptSnapshot;
    thumb.source_evidence_script_path =
        ReadJsonStringFieldLocal(binding, "source_evidence_script_path");
    if (thumb.source_evidence_script_path.empty())
      thumb.source_evidence_script_path = thumb.script_path;
    const std::string savedSummary =
        ReadJsonStringFieldLocal(binding, "parameter_summary");
    if (!savedSummary.empty())
      thumb.parameter_summary = savedSummary;
    thumb.has_saved_state = true;
    thumb.is_candidate = false;
    thumb.status = "pending_human_review";
    thumb.reason += "; restored asset working_state=" + candidateDir.string();
    reason = "restored asset working state: " + candidateDir.string();
    return true;
  }
  return false;
}

static int AppendAssetDrivenEvidenceCasesLocal(
    ManualTestContext &context,
    const std::function<ScriptEvidenceGroup &(const std::string &)> &findGroup,
    std::string &reason) {
  const std::filesystem::path runRoot =
      ResolveCxVisionRunPath("cxscript_runs");
  int discovered = 0;
  int accepted = 0;
  std::vector<AssetCaseScanRejectionLocal> rejected;
  std::set<std::string> identities;
  std::set<std::string> normalizedPaths;

  auto reject = [&](const std::filesystem::path &manifest,
                    const std::string &message) {
    rejected.push_back({manifest.string(), message});
  };

  std::vector<std::filesystem::path> scanRoots;
  const std::filesystem::path registryPath =
      runRoot / "_shared" / "evidence_case_roots.json";
  std::error_code iteratorError;
  if (!std::filesystem::is_directory(runRoot, iteratorError)) {
    reject(runRoot, "RUN_ROOT is missing or is not a directory");
  } else {
    cv::FileStorage registry;
    bool registryOpened = false;
    bool registryParseRejected = false;
    try {
      registryOpened = registry.open(registryPath.string(),
                                     cv::FileStorage::READ |
                                         cv::FileStorage::FORMAT_JSON);
    } catch (const cv::Exception &error) {
      reject(registryPath,
             "Evidence case root registry parse exception: " +
                 std::string(error.what()));
      registryParseRejected = true;
    }
    if (!registryOpened) {
      if (!registryParseRejected)
        reject(registryPath, "Evidence case root registry is missing or invalid");
    } else if (FileNodeStringLocal(registry.root(), "schema") !=
            "cxvision.evidence_case_roots.v1" ||
        !registry["roots"].isSeq()) {
      reject(registryPath, "Evidence case root registry is missing or invalid");
    } else {
      std::set<std::string> uniqueRoots;
      for (const cv::FileNode &rootNode : registry["roots"]) {
        const std::string relativeRoot = static_cast<std::string>(rootNode);
        const std::filesystem::path scanRoot = runRoot / relativeRoot;
        std::error_code rootError;
        const std::string normalizedRoot =
            std::filesystem::weakly_canonical(scanRoot, rootError).generic_string();
        if (relativeRoot.empty() || rootError ||
            !IsAssetCasePathWithinRootLocal(runRoot, scanRoot) ||
            !std::filesystem::is_directory(scanRoot, rootError) ||
            std::filesystem::is_symlink(scanRoot, rootError)) {
          reject(registryPath, "registered case root is missing or unsafe: " +
                                   relativeRoot);
          continue;
        }
        if (uniqueRoots.insert(normalizedRoot).second)
          scanRoots.push_back(scanRoot);
      }
    }
  }

  for (const std::filesystem::path &scanRoot : scanRoots) {
    std::filesystem::recursive_directory_iterator iterator(
        scanRoot, std::filesystem::directory_options::skip_permission_denied,
        iteratorError);
    const std::filesystem::recursive_directory_iterator end;
    while (iterator != end) {
      if (iteratorError) {
        reject(scanRoot, "directory iteration failed: " + iteratorError.message());
        iteratorError.clear();
        iterator.increment(iteratorError);
        continue;
      }

      const std::filesystem::directory_entry entry = *iterator;
      std::error_code entryError;
      if (entry.is_symlink(entryError)) {
        if (entry.is_directory(entryError))
          iterator.disable_recursion_pending();
        iterator.increment(iteratorError);
        continue;
      }
      if (!entry.is_regular_file(entryError) ||
          entry.path().filename() != "case_manifest.json") {
        iterator.increment(iteratorError);
        continue;
      }

      ++discovered;
      const std::filesystem::path manifestPath = entry.path();
      const std::filesystem::path caseDirectory = manifestPath.parent_path();
      if (!IsAssetCasePathWithinRootLocal(runRoot, manifestPath) ||
          std::filesystem::is_symlink(caseDirectory, entryError)) {
        reject(manifestPath, "manifest is outside RUN_ROOT or its case directory is a symlink");
        iterator.increment(iteratorError);
        continue;
      }

      cv::FileStorage manifest;
      bool manifestOpened = false;
      bool manifestParseRejected = false;
      try {
        manifestOpened = manifest.open(manifestPath.string(),
                                       cv::FileStorage::READ |
                                           cv::FileStorage::FORMAT_JSON);
      } catch (const cv::Exception &error) {
        reject(manifestPath,
               "Evidence case manifest parse exception: " +
                   std::string(error.what()));
        manifestParseRejected = true;
      }
      if (!manifestOpened) {
        if (!manifestParseRejected)
          reject(manifestPath, "unsupported or invalid Evidence case schema");
        iterator.increment(iteratorError);
        continue;
      }
      if (FileNodeStringLocal(manifest.root(), "schema") !=
          "cxvision.evidence_case.v1") {
        reject(manifestPath, "unsupported Evidence case schema");
        iterator.increment(iteratorError);
        continue;
      }

      const std::string runId = FileNodeStringLocal(manifest.root(), "run_id");
      std::string internalCaseId =
          FileNodeStringLocal(manifest.root(), "internal_case_id");
      std::string reviewItem =
          FileNodeStringLocal(manifest.root(), "review_item");
      if (reviewItem.empty())
        reviewItem = FileNodeStringLocal(manifest.root(), "display_name");
      std::error_code relativeError;
      const std::filesystem::path relativeCase =
          std::filesystem::relative(caseDirectory, runRoot, relativeError);
      if (reviewItem.empty() && !relativeError)
        reviewItem = relativeCase.generic_string();
      if (internalCaseId.empty() && !relativeError)
        internalCaseId = "asset_case:" + relativeCase.generic_string();

      const std::string sourceRef =
          FileNodeStringLocal(manifest.root(), "source_image");
      const std::string labelRef =
          FileNodeStringLocal(manifest.root(), "typed_label");
      const std::string factsRef =
          FileNodeStringLocal(manifest.root(), "geometry_facts_ref");
      const std::string overlayRef =
          FileNodeStringLocal(manifest.root(), "evidence_overlay");
      const std::string summaryRef =
          FileNodeStringLocal(manifest.root(), "result_summary");
      const std::string scriptRef =
          FileNodeStringLocal(manifest.root(), "script_snapshot");
      const std::string imageSetDirRef =
          FileNodeStringLocal(manifest.root(), "image_set_dir");
      std::string caseTool = NormalizeEvidenceToolTypeLocal(
          FileNodeStringLocal(manifest.root(), "tool"));
      if (caseTool.empty())
        caseTool = "GeometryReference";
      const cv::FileNode requiredAssets = manifest["required_assets"];
      if (runId.empty() || internalCaseId.empty() || reviewItem.empty() ||
          sourceRef.empty() || labelRef.empty() || factsRef.empty() ||
          overlayRef.empty() || summaryRef.empty() || !requiredAssets.isSeq()) {
        reject(manifestPath, "mandatory identity or asset fields are missing");
        iterator.increment(iteratorError);
        continue;
      }

      bool assetsValid = true;
      std::set<std::string> requiredNames;
      for (const cv::FileNode &assetNode : requiredAssets) {
        const std::string assetRef = static_cast<std::string>(assetNode);
        requiredNames.insert(assetRef);
        const std::filesystem::path assetPath = caseDirectory / assetRef;
        if (assetRef.empty() ||
            !IsAssetCasePathWithinRootLocal(caseDirectory, assetPath) ||
            !IsAssetCasePathWithinRootLocal(runRoot, assetPath) ||
            !std::filesystem::is_regular_file(assetPath, entryError) ||
            std::filesystem::is_symlink(assetPath, entryError)) {
          reject(manifestPath, "ASSET_MISSING or unsafe required asset: " + assetRef);
          assetsValid = false;
          break;
        }
      }
      if (assetsValid) {
        for (const std::string &mandatoryRef :
             {sourceRef, labelRef, factsRef, overlayRef, summaryRef}) {
          if (requiredNames.count(mandatoryRef) == 0) {
            reject(manifestPath,
                   "mandatory asset is absent from required_assets: " +
                       mandatoryRef);
            assetsValid = false;
            break;
          }
        }
      }
      if (assetsValid && !scriptRef.empty() &&
          requiredNames.count(scriptRef) == 0) {
        reject(manifestPath,
               "script_snapshot is absent from required_assets: " +
                   scriptRef);
        assetsValid = false;
      }
      std::vector<std::filesystem::path> imageSetPaths;
      if (assetsValid && !imageSetDirRef.empty()) {
        const std::filesystem::path imageSetDir = caseDirectory / imageSetDirRef;
        if (!IsAssetCasePathWithinRootLocal(caseDirectory, imageSetDir) ||
            !IsAssetCasePathWithinRootLocal(runRoot, imageSetDir) ||
            !std::filesystem::is_directory(imageSetDir, entryError) ||
            std::filesystem::is_symlink(imageSetDir, entryError)) {
          reject(manifestPath, "image_set_dir is missing or unsafe: " + imageSetDirRef);
          assetsValid = false;
        } else {
          std::filesystem::recursive_directory_iterator imageIterator(
              imageSetDir, std::filesystem::directory_options::skip_permission_denied,
              entryError);
          const std::filesystem::recursive_directory_iterator imageEnd;
          while (assetsValid && imageIterator != imageEnd) {
            if (entryError) {
              reject(manifestPath, "image_set_dir iteration failed: " + entryError.message());
              assetsValid = false;
              break;
            }
            const std::filesystem::directory_entry imageEntry = *imageIterator;
            std::error_code imageError;
            if (imageEntry.is_symlink(imageError)) {
              if (imageEntry.is_directory(imageError))
                imageIterator.disable_recursion_pending();
              imageIterator.increment(entryError);
              continue;
            }
            if (imageEntry.is_regular_file(imageError)) {
              std::string extension = imageEntry.path().extension().string();
              std::transform(extension.begin(), extension.end(), extension.begin(),
                             [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
              if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
                  extension == ".bmp" || extension == ".tif" || extension == ".tiff")
                imageSetPaths.push_back(imageEntry.path());
            }
            imageIterator.increment(entryError);
          }
          std::sort(imageSetPaths.begin(), imageSetPaths.end());
          if (assetsValid && imageSetPaths.empty()) {
            reject(manifestPath, "image_set_dir contains no supported immutable images");
            assetsValid = false;
          }
        }
      }
      if (!assetsValid) {
        iterator.increment(iteratorError);
        continue;
      }

      const std::filesystem::path sourcePath = caseDirectory / sourceRef;
      const std::filesystem::path labelPath = caseDirectory / labelRef;
      const std::filesystem::path overlayPath = caseDirectory / overlayRef;
      const std::string identity = runId + "|" + internalCaseId;
      const std::string normalizedPath =
          std::filesystem::weakly_canonical(caseDirectory, entryError).generic_string();
      if (!identities.insert(identity).second) {
        reject(manifestPath, "duplicate internal_case_id within RUN_ID");
        iterator.increment(iteratorError);
        continue;
      }
      if (!normalizedPaths.insert(normalizedPath).second) {
        reject(manifestPath, "duplicate normalized case directory");
        iterator.increment(iteratorError);
        continue;
      }

      ScriptEvidenceThumb thumb;
      thumb.case_id = internalCaseId;
      thumb.review_item = reviewItem;
      thumb.script_id = reviewItem;
      if (!scriptRef.empty()) {
        thumb.script_path = (caseDirectory / scriptRef).string();
        thumb.source_evidence_script_path = thumb.script_path;
        thumb.working_script_snapshot_path = thumb.script_path;
      }
      thumb.image_id = internalCaseId;
      thumb.image_path = sourcePath.string();
      thumb.thumbnail_path = overlayPath.string();
      thumb.target_id = FileNodeStringLocal(manifest.root(), "target_id");
      if (thumb.target_id.empty())
        thumb.target_id = FileNodeStringLocal(manifest.root(), "geometry_type");
      thumb.tool = caseTool;
      thumb.parameter_summary =
          FileNodeStringLocal(manifest.root(), "parameter_summary");
      if (thumb.parameter_summary.empty()) {
        thumb.parameter_summary =
            "track=" + FileNodeStringLocal(manifest.root(), "case_track") +
            " geometry=" + thumb.target_id +
            " topology=" + FileNodeStringLocal(manifest.root(), "topology") +
            " split=" + FileNodeStringLocal(manifest.root(), "split") +
            " variant=" + FileNodeStringLocal(manifest.root(), "variant_id") +
            " degradation=" +
            FileNodeStringLocal(manifest.root(), "degradation_bucket") +
            " training_enabled=0";
      }
      if (!imageSetPaths.empty())
        thumb.parameter_summary += " image_set_count=" +
                                   std::to_string(imageSetPaths.size());
      thumb.evidence_output_root = caseDirectory.string();
      thumb.contract_id = FileNodeStringLocal(manifest.root(), "schema");
      thumb.expected_result = summaryRef;
      thumb.expected_policy_guard =
          "controlled fixture review only; production quality is not claimed";
      thumb.evidence_level = FileNodeStringLocal(manifest.root(), "evidence_level");
      if (thumb.evidence_level.empty())
        thumb.evidence_level = "T0";
      thumb.evidence_case_role =
          FileNodeStringLocal(manifest.root(), "case_role");
      if (thumb.evidence_case_role.empty())
        thumb.evidence_case_role = "asset_driven_reference";
      thumb.source_case_id = internalCaseId;
      thumb.manual_review_required = true;
      thumb.promotion_candidate = false;
      thumb.evidence_category_override =
          FileNodeStringLocal(manifest.root(), "display_category");
      if (thumb.evidence_category_override.empty())
        thumb.evidence_category_override = "To Verify";
      thumb.evidence_head_folder = runId;
      thumb.evidence_case_folder = reviewItem;
      thumb.workflow_id = runId;
      thumb.workflow_stage = "controlled_geometry_review";
      thumb.workflow_status = "PENDING_HUMAN_REVIEW";
      std::string evidenceSplit = FileNodeStringLocal(manifest.root(), "split");
      std::transform(evidenceSplit.begin(), evidenceSplit.end(),
                     evidenceSplit.begin(), [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                     });
      if (evidenceSplit == "validation" || evidenceSplit == "validate" ||
          evidenceSplit == "valid")
        evidenceSplit = "test";
      if (evidenceSplit != "train" && evidenceSplit != "val" &&
          evidenceSplit != "test")
        evidenceSplit = "test";
      thumb.dataset_role = evidenceSplit;
      thumb.annotation_policy =
          FileNodeStringLocal(manifest.root(), "typed_label_kind");
      thumb.gate_policy = "human_review_required";
      thumb.dataset_frozen = true;
      thumb.status = FileNodeStringLocal(manifest.root(), "binding_status");
      thumb.reason = "asset-driven Evidence case; manifest=" + manifestPath.string() +
                     "; typed_label=" + labelPath.string() +
                     "; summary=" + (caseDirectory / summaryRef).string();
      std::string workingStateReason;
      RestoreAssetCaseWorkingStateLocal(caseDirectory, internalCaseId, thumb,
                                        workingStateReason);
      std::string sourceHash;
      cv::FileStorage geometryFacts;
      try {
        geometryFacts.open((caseDirectory / factsRef).string(),
                           cv::FileStorage::READ |
                               cv::FileStorage::FORMAT_JSON);
      } catch (const cv::Exception &) {
      }
      if (geometryFacts.isOpened())
        sourceHash =
            FileNodeStringLocal(geometryFacts.root(), "source_input_sha256");
      if (imageSetPaths.empty()) {
        CxEvidenceDatasetImageBinding sourceBinding;
        sourceBinding.image_id = internalCaseId;
        sourceBinding.image_path = sourcePath.string();
        sourceBinding.split = evidenceSplit;
        sourceBinding.label = thumb.target_id;
        sourceBinding.source = sourceHash.empty()
                                   ? "evidence_case_manifest"
                                   : "source_hash:" + sourceHash;
        thumb.dataset_images.push_back(sourceBinding);
      } else {
        for (const std::filesystem::path &imagePath : imageSetPaths) {
          std::error_code imageRelativeError;
          const std::filesystem::path imageRelative =
              std::filesystem::relative(imagePath, caseDirectory, imageRelativeError);
          CxEvidenceDatasetImageBinding imageBinding;
          imageBinding.image_id = internalCaseId + "::" +
                                  (imageRelativeError ? imagePath.filename().string()
                                                      : imageRelative.generic_string());
          imageBinding.image_path = imagePath.string();
          imageBinding.split = evidenceSplit;
          imageBinding.label = thumb.target_id;
          imageBinding.source = "evidence_image_set:" + imageSetDirRef;
          thumb.dataset_images.push_back(std::move(imageBinding));
        }
      }

      int classId = 0;
      const cv::FileNode classIdNode = manifest.root()["class_id"];
      if (!classIdNode.empty())
        classId = static_cast<int>(classIdNode);
      AppendTypedLabelDraftAnnotationsLocal(labelPath, internalCaseId,
                                            thumb.target_id, classId,
                                            thumb.annotations);

      std::string displayGroup =
          FileNodeStringLocal(manifest.root(), "display_group");
      if (displayGroup.empty())
        displayGroup = caseTool + " / Asset Cases";
      findGroup(displayGroup).thumbs.push_back(thumb);

      ManualEvidenceItem item;
      item.case_id = internalCaseId;
      item.review_item = reviewItem;
      item.level = "T0";
      item.image_id = internalCaseId;
      item.target_id = thumb.target_id;
      item.tool = thumb.tool;
      item.script_id = reviewItem;
      item.gauge_status = "not_applicable";
      item.probe_status = "controlled_fixture_ready";
      item.contract_status = "validated";
      item.review_status = "PENDING_HUMAN_REVIEW";
      item.image_path = sourcePath.string();
      item.source_evidence_chain_path = manifestPath.string();
      context.evidence_items.push_back(item);
      ++accepted;

      iterator.increment(iteratorError);
    }
  }

  const std::filesystem::path debugPath =
      runRoot / "evidence_chain" / "case_asset_scan_debug.json";
  std::error_code debugError;
  std::filesystem::create_directories(debugPath.parent_path(), debugError);
  std::ofstream debug(debugPath, std::ios::trunc);
  debug << "{\n"
        << "  \"schema\": \"cxvision.evidence_case_scan.v1\",\n"
        << "  \"root\": \"" << JsonEscape(runRoot.string()) << "\",\n"
        << "  \"registry\": \"" << JsonEscape(registryPath.string()) << "\",\n"
        << "  \"registered_roots\": " << scanRoots.size() << ",\n"
        << "  \"discovered\": " << discovered << ",\n"
        << "  \"accepted\": " << accepted << ",\n"
        << "  \"rejected\": " << rejected.size() << ",\n"
        << "  \"rejections\": [\n";
  for (std::size_t index = 0; index < rejected.size(); ++index) {
    debug << "    {\"manifest\": \""
          << JsonEscape(rejected[index].manifest_path) << "\", \"reason\": \""
          << JsonEscape(rejected[index].reason) << "\"}"
          << (index + 1 == rejected.size() ? "\n" : ",\n");
  }
  debug << "  ]\n}\n";

  std::ostringstream summary;
  summary << "asset case scan root=" << runRoot.string()
          << " discovered=" << discovered << " accepted=" << accepted
          << " rejected=" << rejected.size() << " debug=" << debugPath.string();
  reason = summary.str();
  return accepted;
}

// Headless tool executions are also Evidence cases.  They deliberately do
// not depend on a hand-maintained case list: a directory becomes visible only
// when its runtime output contains the normal FastMatch evidence assets below.
// This keeps the immutable input image separate from its review overlay while
// making an actual run discoverable in Manual Review / Evidence.
static std::string ReadSnapshotValueLocal(const std::filesystem::path &path,
                                          const char *key) {
  std::string text;
  if (!ReadTextFile(path.string(), text))
    return {};
  const std::string prefix = std::string(key) + ":";
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    line = TrimLine(line);
    if (line.rfind(prefix, 0) == 0)
      return TrimLine(line.substr(prefix.size()));
  }
  return {};
}

static int AppendFastMatchHeadlessEvidenceCasesLocal(
    ManualTestContext &context,
    const std::function<ScriptEvidenceGroup &(const std::string &)> &findGroup,
    std::string &reason) {
  const std::filesystem::path runRoot = ResolveCxVisionRunPath("cxscript_runs");
  const std::filesystem::path headlessRoot = runRoot / "headless";
  int discovered = 0;
  int accepted = 0;
  std::vector<AssetCaseScanRejectionLocal> rejected;
  std::set<std::string> identities;
  std::error_code ec;

  auto reject = [&](const std::filesystem::path &path,
                    const std::string &message) {
    rejected.push_back({path.string(), message});
  };
  auto regularAsset = [&](const std::filesystem::path &caseDirectory,
                          const char *name) -> bool {
    const std::filesystem::path path = caseDirectory / name;
    std::error_code assetError;
    return IsAssetCasePathWithinRootLocal(runRoot, path) &&
           std::filesystem::is_regular_file(path, assetError) &&
           !std::filesystem::is_symlink(path, assetError);
  };

  if (!std::filesystem::is_directory(headlessRoot, ec)) {
    reject(headlessRoot, "headless execution root is missing or inaccessible");
  } else {
    std::filesystem::recursive_directory_iterator iterator(
        headlessRoot, std::filesystem::directory_options::skip_permission_denied,
        ec);
    const std::filesystem::recursive_directory_iterator end;
    while (iterator != end) {
      if (ec) {
        reject(headlessRoot, "directory iteration failed: " + ec.message());
        ec.clear();
        iterator.increment(ec);
        continue;
      }

      const std::filesystem::directory_entry entry = *iterator;
      std::error_code entryError;
      if (entry.is_symlink(entryError)) {
        if (entry.is_directory(entryError))
          iterator.disable_recursion_pending();
        iterator.increment(ec);
        continue;
      }
      if (!entry.is_regular_file(entryError) ||
          entry.path().filename() != "line_trace.json") {
        iterator.increment(ec);
        continue;
      }

      ++discovered;
      const std::filesystem::path caseDirectory = entry.path().parent_path();
      const std::filesystem::path provenancePath =
          caseDirectory / "algorithm_provenance.json";
      std::string provenance;
      if (!regularAsset(caseDirectory, "result_summary.json") ||
          !regularAsset(caseDirectory, "snapshot.txt") ||
          !regularAsset(caseDirectory, "evidence_overlay.png") ||
          !regularAsset(caseDirectory, "algorithm_provenance.json") ||
          !ReadTextFile(provenancePath.string(), provenance) ||
          NormalizeEvidenceToolTypeLocal(
              ReadJsonStringFieldLocal(provenance, "tool")) != "FastMatch") {
        reject(caseDirectory,
               "ASSET_MISSING or not a FastMatch headless evidence directory");
        iterator.increment(ec);
        continue;
      }

      const std::string imagePath =
          ReadSnapshotValueLocal(caseDirectory / "snapshot.txt", "image");
      const std::string scriptPath =
          ReadSnapshotValueLocal(caseDirectory / "snapshot.txt", "script");
      std::string reviewItem =
          ReadSnapshotValueLocal(caseDirectory / "snapshot.txt", "case_id");
      std::error_code relativeError;
      const std::filesystem::path relativeCase =
          std::filesystem::relative(caseDirectory, runRoot, relativeError);
      if (reviewItem.empty() && !relativeError)
        reviewItem = relativeCase.generic_string();
      const std::filesystem::path sourceImage = ResolveWorkspaceFile(imagePath);
      if (imagePath.empty() || scriptPath.empty() || reviewItem.empty() ||
          !std::filesystem::is_regular_file(sourceImage, entryError)) {
        reject(caseDirectory,
               "ASSET_MISSING: snapshot lacks a resolvable immutable input image, script, or display name");
        iterator.increment(ec);
        continue;
      }

      const std::string normalizedDirectory =
          std::filesystem::weakly_canonical(caseDirectory, entryError)
              .generic_string();
      const std::string runId = caseDirectory.parent_path().filename().string();
      const std::string internalCaseId =
          "headless_fastmatch:" + normalizedDirectory;
      if (!identities.insert(runId + "|" + internalCaseId).second) {
        reject(caseDirectory, "duplicate internal_case_id within RUN_ID");
        iterator.increment(ec);
        continue;
      }

      ScriptEvidenceThumb thumb;
      thumb.case_id = internalCaseId;
      thumb.review_item = reviewItem;
      thumb.script_id = reviewItem;
      thumb.script_path = scriptPath;
      thumb.source_evidence_script_path = scriptPath;
      thumb.image_id = sourceImage.stem().string();
      thumb.image_path = sourceImage.string();
      thumb.thumbnail_path = (caseDirectory / "evidence_overlay.png").string();
      thumb.target_id = "FastMatch Normal Trace";
      thumb.tool = "FastMatch";
      const std::filesystem::path resultSummaryPath =
          caseDirectory / "result_summary.json";
      std::string resultSummaryText;
      ReadTextFile(resultSummaryPath.string(), resultSummaryText);
      std::string failureClass =
          ReadJsonStringFieldLocal(resultSummaryText, "failure_stage");
      if (failureClass.empty())
        failureClass =
            ReadJsonStringFieldLocal(resultSummaryText, "failure_class");
      thumb.parameter_summary = BuildManualReviewParamSummaryLocal(
          thumb.tool, failureClass, resultSummaryPath.string(),
          "line_trace=" + (caseDirectory / "line_trace.json").string());
      thumb.evidence_output_root = caseDirectory.string();
      thumb.contract_id = "cxvision.headless_fastmatch_evidence.v1";
      thumb.expected_result = "runtime evidence; human analysis required";
      thumb.expected_policy_guard =
          "source image is immutable; overlays are review-only";
      thumb.evidence_level = "T3";
      thumb.evidence_case_role = "headless_fastmatch_execution";
      thumb.source_case_id = internalCaseId;
      thumb.manual_review_required = true;
      thumb.promotion_candidate = false;
      // A completed runtime trace is a diagnostic review asset, not an
      // unclassified business case.  Keep it in a dedicated, asset-derived
      // navigation bucket so a human can reach its line_trace evidence without
      // opening the entire generic To Verify backlog.
      thumb.evidence_category_override = "FastMatch Trace Diagnostics";
      thumb.evidence_head_folder = runId;
      thumb.evidence_case_folder = reviewItem;
      thumb.workflow_id = runId;
      thumb.workflow_stage = "headless_fastmatch_review";
      thumb.workflow_status = "PENDING_HUMAN_REVIEW";
      thumb.dataset_role = "test";
      thumb.annotation_policy = "runtime_trace";
      thumb.gate_policy = "human_review_required";
      thumb.dataset_frozen = true;
      thumb.status = "headless_execution_available";
      thumb.reason = "asset-driven FastMatch headless evidence; output=" +
                     caseDirectory.string();

      CxEvidenceDatasetImageBinding image;
      image.image_id = thumb.image_id;
      image.image_path = thumb.image_path;
      image.split = "test";
      image.label = "FastMatch";
      image.source = "headless_snapshot_immutable_input";
      thumb.dataset_images.push_back(std::move(image));

      findGroup("FastMatch / Headless Evidence").thumbs.push_back(thumb);
      ++accepted;
      iterator.increment(ec);
    }
  }

  const std::filesystem::path debugPath =
      runRoot / "evidence_chain" / "fastmatch_headless_case_scan_debug.json";
  std::error_code debugError;
  std::filesystem::create_directories(debugPath.parent_path(), debugError);
  std::ofstream debug(debugPath, std::ios::trunc);
  debug << "{\n"
        << "  \"schema\": \"cxvision.fastmatch_headless_case_scan.v1\",\n"
        << "  \"root\": \"" << JsonEscape(headlessRoot.string()) << "\",\n"
        << "  \"discovered\": " << discovered << ",\n"
        << "  \"accepted\": " << accepted << ",\n"
        << "  \"rejected\": " << rejected.size() << ",\n"
        << "  \"rejections\": [\n";
  for (std::size_t index = 0; index < rejected.size(); ++index) {
    debug << "    {\"path\": \"" << JsonEscape(rejected[index].manifest_path)
          << "\", \"reason\": \"" << JsonEscape(rejected[index].reason)
          << "\"}" << (index + 1 == rejected.size() ? "\n" : ",\n");
  }
  debug << "  ]\n}\n";

  std::ostringstream summary;
  summary << "FastMatch headless case scan root=" << headlessRoot.string()
          << " discovered=" << discovered << " accepted=" << accepted
          << " rejected=" << rejected.size() << " debug=" << debugPath.string();
  reason = summary.str();
  return accepted;
}

static bool IsCuratedAssetOnlyEvidenceQueueLocal() {
  const std::filesystem::path policyPath = ResolveCxVisionRunPath(
      "cxscript_runs/_shared/evidence_case_queue.json");
  cv::FileStorage policy;
  try {
    if (!policy.open(policyPath.string(),
                     cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON))
      return false;
  } catch (const cv::Exception &) {
    return false;
  }
  return FileNodeStringLocal(policy.root(), "schema") ==
             "cxvision.evidence_case_queue.v1" &&
         FileNodeStringLocal(policy.root(), "mode") ==
             "CURATED_ASSET_ONLY";
}

void ViewController::EnsureCxScriptWorkbenchAssetsLoaded() {
  if (m_manualTest.script_evidence_groups_dirty == false)
    return;

  LoadEvidenceCategoryOverridesLocal(m_manualTest);
  RefreshHiddenEvidenceCaseCatalogLocal(m_manualTest);

  const bool curatedAssetOnly = IsCuratedAssetOnlyEvidenceQueueLocal();
  if (curatedAssetOnly) {
    for (auto &group : m_manualTest.script_evidence_groups) {
      for (auto &thumb : group.thumbs)
        ResetEvidenceThumbTexture(thumb);
    }
    m_manualTest.script_evidence_groups.clear();
    m_manualTest.evidence_items.erase(
        std::remove_if(m_manualTest.evidence_items.begin(),
                       m_manualTest.evidence_items.end(),
                       [](const ManualEvidenceItem &item) {
                         return !item.source_evidence_chain_path.empty();
                       }),
        m_manualTest.evidence_items.end());
    auto findCuratedGroup = [&](const std::string &label)
        -> ScriptEvidenceGroup & {
      for (auto &group : m_manualTest.script_evidence_groups) {
        if (group.label == label)
          return group;
      }
      ScriptEvidenceGroup group;
      group.label = label;
      m_manualTest.script_evidence_groups.push_back(std::move(group));
      return m_manualTest.script_evidence_groups.back();
    };
    std::string curatedAssetReason;
    AppendAssetDrivenEvidenceCasesLocal(m_manualTest, findCuratedGroup,
                                        curatedAssetReason);
    if (!m_manualTest.selected_hidden_evidence_case_keys.empty()) {
      const std::vector<std::string> fallbackImages =
          BuildEvidenceFallbackImageCandidates(m_manualTest);
      std::unordered_map<std::string, std::size_t> fallbackImageIndexByPool;
      std::string selectedCatalogReason;
      AppendCxScriptEvidenceChainFilesLocal(
          m_manualTest, findCuratedGroup, fallbackImages,
          fallbackImageIndexByPool,
          &m_manualTest.selected_hidden_evidence_case_keys,
          selectedCatalogReason);
      curatedAssetReason += "; selected hidden cases: " + selectedCatalogReason;
    }
    m_manualTest.debug_reason =
        "Evidence queue mode=CURATED_ASSET_ONLY; " + curatedAssetReason;
    ++m_manualTest.script_evidence_groups_revision;
    if (m_manualTest.script_evidence_groups_debug_revision !=
        m_manualTest.script_evidence_groups_revision) {
      WriteEvidenceChainLoadedElementsDebugLocal(m_manualTest);
      m_manualTest.script_evidence_groups_debug_revision =
          m_manualTest.script_evidence_groups_revision;
    }
    m_manualTest.script_evidence_groups_dirty = false;
    m_manualTest.script_evidence_row_refs_dirty = true;
    return;
  }

  EnsureStructuredCxImageCatalogEntriesLoaded(m_manualTest);
  m_manualTest.evidence_items.erase(
      std::remove_if(m_manualTest.evidence_items.begin(),
                     m_manualTest.evidence_items.end(),
                     [](const ManualEvidenceItem &item) {
                       return !item.source_evidence_chain_path.empty();
                     }),
      m_manualTest.evidence_items.end());

  for (auto &group : m_manualTest.script_evidence_groups) {
    for (auto &thumb : group.thumbs)
      ResetEvidenceThumbTexture(thumb);
  }
  m_manualTest.script_evidence_groups.clear();

  const std::vector<std::string> fallbackImages =
      BuildEvidenceFallbackImageCandidates(m_manualTest);

  std::unordered_map<std::string, std::size_t> fallbackImageIndexByPool;

  auto hasThumbForScript = [&](const std::string &scriptId,
                               const std::string &scriptPath) -> bool {
    for (const auto &group : m_manualTest.script_evidence_groups) {
      for (const auto &thumb : group.thumbs) {
        if (!scriptId.empty() && thumb.script_id == scriptId)
          return true;
        if (scriptId.empty() && !scriptPath.empty() &&
            thumb.script_path == scriptPath)
          return true;
      }
    }
    return false;
  };

  auto findOrCreateGroup =
      [&](const std::string &scriptId, const std::string &scriptPath,
          const std::string &tool) -> ScriptEvidenceGroup & {
    for (auto &group : m_manualTest.script_evidence_groups) {
      if (!scriptId.empty() && group.script_id == scriptId)
        return group;
      if (scriptId.empty() && group.label == tool)
        return group;
    }

    ScriptEvidenceGroup group;
    group.script_id = scriptId;
    group.script_path = scriptPath;
    group.label = InferEvidenceChainToolBucketLocal(
        tool, scriptId, scriptPath,
        tool.empty() ? (scriptId.empty() ? "unknown" : scriptId) : tool);
    m_manualTest.script_evidence_groups.push_back(group);
    return m_manualTest.script_evidence_groups.back();
  };

  for (const auto &item : m_manualTest.evidence_items) {
    if (item.script_id.empty())
      continue;

    const std::string scriptPath = ResolveCatalogScriptPathById(item.script_id);
    const std::string groupLabel = InferEvidenceChainToolBucketLocal(
        item.tool, item.script_id, scriptPath, item.tool,
        item.probe_status.empty() ? item.contract_status : item.probe_status,
        item.review_status, item.parameter_profile_id);
    ScriptEvidenceGroup &group =
        findOrCreateGroup(item.script_id, scriptPath, groupLabel);

    ScriptEvidenceThumb thumb;
    thumb.case_id = item.case_id;
    thumb.script_id = item.script_id;
    thumb.script_path = scriptPath;
    thumb.image_id = item.image_id;
    thumb.image_path = item.image_path;
    thumb.target_id = item.target_id;
    thumb.tool = item.tool;
    thumb.parameter_summary = item.parameter_profile_id;
    if (thumb.parameter_summary.empty()) {
      for (const auto &entry : m_manualTest.catalog_entries) {
        if (entry.script_id == item.script_id) {
          thumb.parameter_summary = entry.parameter_policy_id;
          break;
        }
      }
    }
    if (thumb.parameter_summary.empty() ||
        thumb.parameter_summary.find('=') == std::string::npos) {
      thumb.parameter_summary =
          BuildDefaultEvidenceParamSummaryForScript(scriptPath);
    }
    thumb.status =
        item.probe_status.empty() ? item.contract_status : item.probe_status;
    thumb.reason = item.review_status;

    AssignFallbackImageToThumb(thumb, fallbackImages, fallbackImageIndexByPool);
    PopulateEditableObjectBindingForThumbLocal(thumb);

    group.thumbs.push_back(thumb);
  }

  {
    std::string evidenceChainReason;
    AppendCxScriptEvidenceChainFilesLocal(
        m_manualTest,
        [&](const std::string &label) -> ScriptEvidenceGroup & {
          return findOrCreateGroup("", "", label);
        },
        fallbackImages, fallbackImageIndexByPool,
        &m_manualTest.selected_hidden_evidence_case_keys,
        evidenceChainReason);
    if (!evidenceChainReason.empty()) {
      if (!m_manualTest.debug_reason.empty() &&
          m_manualTest.debug_reason != "not started") {
        m_manualTest.debug_reason += "; ";
      } else {
        m_manualTest.debug_reason.clear();
      }
      m_manualTest.debug_reason += evidenceChainReason;
    }
  }

  {
    std::string assetCaseReason;
    AppendAssetDrivenEvidenceCasesLocal(
        m_manualTest,
        [&](const std::string &label) -> ScriptEvidenceGroup & {
          return findOrCreateGroup("", "", label);
        },
        assetCaseReason);
    if (!assetCaseReason.empty()) {
      if (!m_manualTest.debug_reason.empty() &&
          m_manualTest.debug_reason != "not started") {
        m_manualTest.debug_reason += "; ";
      } else {
        m_manualTest.debug_reason.clear();
      }
      m_manualTest.debug_reason += assetCaseReason;
    }
  }

  {
    std::string headlessFastMatchReason;
    AppendFastMatchHeadlessEvidenceCasesLocal(
        m_manualTest,
        [&](const std::string &label) -> ScriptEvidenceGroup & {
          return findOrCreateGroup("", "", label);
        },
        headlessFastMatchReason);
    if (!headlessFastMatchReason.empty()) {
      if (!m_manualTest.debug_reason.empty() &&
          m_manualTest.debug_reason != "not started") {
        m_manualTest.debug_reason += "; ";
      } else {
        m_manualTest.debug_reason.clear();
      }
      m_manualTest.debug_reason += headlessFastMatchReason;
    }
  }

  for (const auto &entry : m_manualTest.catalog_entries) {
    if (!IsAllowedEvidenceFallbackScript(entry.path))
      continue;

    const std::string normalizedTool =
        NormalizeEvidenceToolTypeLocal(entry.tool);
    const bool isSmokeEvidence =
        entry.expected_result == "smoke" &&
        (normalizedTool == "TorchTask" ||
         entry.path.find("/torch/") != std::string::npos ||
         entry.path.find("\\torch\\") != std::string::npos);
    const bool isTorchVerificationEvidence =
        normalizedTool == "TorchTask" &&
        (entry.expected_result == "pending_human_review" ||
         entry.expected_result == "pending_binding");
    const bool isDescriptorEvidence =
        normalizedTool == "RegionPatternTool" ||
        normalizedTool == "GridPatternClassTool" ||
        entry.expected_result == "descriptor_available" ||
        entry.expected_result == "grid_feature_available";
    bool isVisible =
        entry.manual_visible && entry.frozen &&
        (entry.expected_result == "ok" ||
         entry.expected_result == "ng_expected" || isSmokeEvidence ||
         isTorchVerificationEvidence || isDescriptorEvidence);
    if (!isVisible)
      continue;

    if (hasThumbForScript(entry.script_id, entry.path))
      continue;

    const std::string groupLabel = InferEvidenceChainToolBucketLocal(
        entry.tool, entry.script_id, entry.path, entry.label,
        entry.expected_status, entry.expected_policy_guard,
        entry.parameter_policy_id);
    ScriptEvidenceGroup &group =
        findOrCreateGroup(entry.script_id, entry.path, groupLabel);

    ScriptEvidenceThumb thumb;
    thumb.script_id = entry.script_id;
    thumb.script_path = entry.path;
    thumb.tool = entry.tool;
    thumb.parameter_summary =
        BuildDefaultEvidenceParamSummaryForScript(entry.path);
    thumb.reason = entry.parameter_policy_id.empty()
                       ? "catalog fallback default params"
                       : "catalog fallback default params from policy " +
                             entry.parameter_policy_id;

    ApplyHDReferenceImageBindingLocal(thumb);
    AssignFallbackImageToThumb(thumb, fallbackImages, fallbackImageIndexByPool);
    PopulateEditableObjectBindingForThumbLocal(thumb);

    group.thumbs.push_back(thumb);
  }

  {
    std::string handoffReason;
    AppendManualAlgorithmReviewHandoffsFromRunFoldersLocal(
        m_manualTest,
        [this](const std::string &imageId) -> std::string {
          return ResolveImagePathFromManifest(imageId);
        },
        [&](const std::string &label) -> ScriptEvidenceGroup & {
          return findOrCreateGroup("", "", label);
        },
        handoffReason);
    if (!handoffReason.empty()) {
      if (!m_manualTest.debug_reason.empty() &&
          m_manualTest.debug_reason != "not started") {
        m_manualTest.debug_reason += "; ";
      } else {
        m_manualTest.debug_reason.clear();
      }
      m_manualTest.debug_reason += handoffReason;
    }
  }

  AppendManualGuidanceQueueLocal(
      m_manualTest,
      [this](const std::string &imageId) -> std::string {
        return ResolveImagePathFromManifest(imageId);
      },
      [&](const std::string &label) -> ScriptEvidenceGroup & {
        return findOrCreateGroup("", "", label);
      });

  {
    for (const auto &item : m_scriptCatalog) {
      if (!IsAllowedEvidenceFallbackScript(item.path))
        continue;

      const bool isDirectLike =
          item.name.find("direct_test") != std::string::npos ||
          item.name.find("_direct") != std::string::npos ||
          item.name.find("_smoke") != std::string::npos ||
          item.type == "GridPatternClassTool" ||
          item.type == "RegionPatternTool" ||
          item.path.find("grid_pattern_class_evidence") != std::string::npos ||
          item.path.find("region_pattern_evidence") != std::string::npos ||
          item.path.find("/headless/") != std::string::npos ||
          item.path.find("\\headless\\") != std::string::npos;

      if (!m_showAllScripts && !isDirectLike)
        continue;

      if (hasThumbForScript(item.name, item.path))
        continue;

      ScriptEvidenceGroup group;
      group.script_id = item.name;
      group.script_path = item.path;
      group.label = InferEvidenceChainToolBucketLocal(
          item.type, item.name, item.path,
          item.type.empty() ? "script" : item.type, item.status,
          item.description, "");

      ScriptEvidenceThumb thumb;
      thumb.script_id = item.name;
      thumb.script_path = item.path;
      thumb.tool = group.label;
      thumb.status = item.status;
      thumb.reason = item.description;

      ApplyHDReferenceImageBindingLocal(thumb);

      if (thumb.parameter_summary.empty() ||
          thumb.parameter_summary.find('=') == std::string::npos) {
        thumb.parameter_summary =
            BuildDefaultEvidenceParamSummaryForScript(item.path);
      }

      AssignFallbackImageToThumb(thumb, fallbackImages,
                                 fallbackImageIndexByPool);
      PopulateEditableObjectBindingForThumbLocal(thumb);

      group.thumbs.push_back(thumb);
      m_manualTest.script_evidence_groups.push_back(group);
    }
  }

  AppendSavedEvidenceCandidatesLocal(
      m_manualTest, [&](const std::string &label) -> ScriptEvidenceGroup & {
        return findOrCreateGroup("", "", label);
      });

  {
    std::string projectionReason;
    ApplyActiveManualReviewProjectionLocal(m_manualTest, projectionReason);
    if (!projectionReason.empty()) {
      if (!m_manualTest.debug_reason.empty() &&
          m_manualTest.debug_reason != "not started") {
        m_manualTest.debug_reason += "; ";
      } else {
        m_manualTest.debug_reason.clear();
      }
      m_manualTest.debug_reason += projectionReason;
    }
  }

  PruneFindEllipseDuplicateCasesByNameLocal(m_manualTest);

  std::stable_sort(
      m_manualTest.script_evidence_groups.begin(),
      m_manualTest.script_evidence_groups.end(),
      [](const ScriptEvidenceGroup &left, const ScriptEvidenceGroup &right) {
        auto priority = [](const ScriptEvidenceGroup &group) -> int {
          const std::string key =
              group.label + " " + group.script_id + " " + group.script_path;
          if (key.find("Findline") != std::string::npos ||
              key.find("findline") != std::string::npos ||
              key.find("find_line") != std::string::npos)
            return 0;
          if (key.find("Findcircle") != std::string::npos ||
              key.find("findcircle") != std::string::npos ||
              key.find("find_circle") != std::string::npos)
            return 1;
          if (key.find("FindEllipse") != std::string::npos ||
              key.find("findellipse") != std::string::npos ||
              key.find("find_ellipse") != std::string::npos)
            return 2;
          if (key.find("FindRect") != std::string::npos ||
              key.find("findrect") != std::string::npos ||
              key.find("find_rect") != std::string::npos)
            return 3;
          if (key.find("fastmatch") != std::string::npos ||
              key.find("FastMatch") != std::string::npos)
            return 4;
          if (key.find("FindSegmentation") != std::string::npos ||
              key.find("find_segmentation") != std::string::npos)
            return 5;
          return 10;
        };

        const int lp = priority(left);
        const int rp = priority(right);
        if (lp != rp)
          return lp < rp;
        return left.script_id < right.script_id;
      });

  ++m_manualTest.script_evidence_groups_revision;
  if (m_manualTest.script_evidence_groups_debug_revision !=
      m_manualTest.script_evidence_groups_revision) {
    WriteEvidenceChainLoadedElementsDebugLocal(m_manualTest);
    m_manualTest.script_evidence_groups_debug_revision =
        m_manualTest.script_evidence_groups_revision;
  }

  m_manualTest.script_evidence_groups_dirty = false;
  m_manualTest.script_evidence_row_refs_dirty = true;
}

bool ViewController::WriteEvidenceChainCatalogSemanticSelfTest(
    const std::string &outDir, std::string &reason) {
  reason.clear();

  EnsureCxScriptWorkbenchAssetsLoaded();

  struct ExpectedBucket {
    const char *label;
    const char *semantic_status;
  };

  const ExpectedBucket expected[] = {
      {"Torch / Model Validation", "FLOW_SMOKE_ONLY"},
      {"Torch Detection - Model Unverified",
       "DETECTION_NON_EMPTY_RESULT_UNVERIFIED"},
      {"Torch Segmentation - Runtime Smoke", "RUNTIME_SMOKE_ONLY"},
      {"FindSegmentation Prompt / EdgeSam", "PROMPT_EDGESAM_PENDING_BINDING"}};

  struct BucketSummary {
    std::string label;
    std::string semantic_status;
    int group_count = 0;
    int script_count = 0;
    std::vector<std::string> scripts;
  };

  std::vector<BucketSummary> summaries;
  for (const ExpectedBucket &e : expected) {
    BucketSummary s;
    s.label = e.label;
    s.semantic_status = e.semantic_status;
    summaries.push_back(s);
  }

  auto findSummary = [&](const std::string &label) -> BucketSummary * {
    for (auto &summary : summaries) {
      if (summary.label == label)
        return &summary;
    }
    return nullptr;
  };

  for (const ScriptEvidenceGroup &group : m_manualTest.script_evidence_groups) {
    BucketSummary *summary = findSummary(group.label);
    if (summary == nullptr)
      continue;

    ++summary->group_count;
    summary->script_count += static_cast<int>(group.thumbs.size());
    for (const ScriptEvidenceThumb &thumb : group.thumbs) {
      std::string script =
          thumb.script_id.empty() ? thumb.script_path : thumb.script_id;
      if (script.empty())
        script = "(unnamed)";
      if (std::find(summary->scripts.begin(), summary->scripts.end(), script) ==
          summary->scripts.end()) {
        summary->scripts.push_back(script);
      }
    }
  }

  bool allPresent = true;
  for (const BucketSummary &summary : summaries) {
    if (summary.script_count <= 0)
      allPresent = false;
  }

  const std::filesystem::path root(
      outDir.empty() ? "cxscript_runs/evidence_selftest/catalog_semantics"
                     : outDir);
  std::filesystem::create_directories(root);

  const std::filesystem::path jsonPath =
      root / "evidence_chain_catalog_semantics.json";
  const std::filesystem::path mdPath =
      root / "evidence_chain_catalog_semantics.md";

  auto escapeJson = [](const std::string &text) -> std::string {
    std::string out;
    for (char ch : text) {
      switch (ch) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += ch;
        break;
      }
    }
    return out;
  };

  {
    std::ofstream file(jsonPath, std::ios::binary);
    if (!file.is_open()) {
      reason = "failed to write evidence chain catalog semantics json";
      return false;
    }

    file << "{\n";
    file << "  \"final_code\": \""
         << (allPresent ? "ASSET_PREFLIGHT_PASS" : "ASSET_PREFLIGHT_FAIL")
         << "\",\n";
    file << "  \"final_status\": \"" << (allPresent ? "PASS" : "FAIL")
         << "\",\n";
    file << "  \"reason\": \"Torch evidence chain classification semantic "
            "check\",\n";
    file << "  \"manual_ui_panel_scope\": \"handled_by_other_thread\",\n";
    file << "  \"model_semantic_quality\": \"NOT_CLAIMED\",\n";
    file << "  \"detection_non_empty_result\": \"UNVERIFIED\",\n";
    file << "  \"findsegmentation_prompt_edgesam\": \"PENDING_BINDING\",\n";
    file << "  \"buckets\": [\n";
    for (std::size_t i = 0; i < summaries.size(); ++i) {
      const BucketSummary &summary = summaries[i];
      file << "    {\n";
      file << "      \"label\": \"" << escapeJson(summary.label) << "\",\n";
      file << "      \"semantic_status\": \""
           << escapeJson(summary.semantic_status) << "\",\n";
      file << "      \"group_count\": " << summary.group_count << ",\n";
      file << "      \"script_count\": " << summary.script_count << ",\n";
      file << "      \"present\": "
           << (summary.script_count > 0 ? "true" : "false") << ",\n";
      file << "      \"scripts\": [";
      for (std::size_t si = 0; si < summary.scripts.size(); ++si) {
        if (si > 0)
          file << ", ";
        file << "\"" << escapeJson(summary.scripts[si]) << "\"";
      }
      file << "]\n";
      file << "    }";
      if (i + 1 < summaries.size())
        file << ",";
      file << "\n";
    }
    file << "  ]\n";
    file << "}\n";
  }

  {
    std::ofstream file(mdPath, std::ios::binary);
    if (!file.is_open()) {
      reason = "failed to write evidence chain catalog semantics md";
      return false;
    }

    file << "# Evidence Chain Catalog Semantic Self Test\n\n";
    file << "- conclusion: `"
         << (allPresent ? "ASSET_PREFLIGHT_PASS" : "ASSET_PREFLIGHT_FAIL")
         << "`\n";
    file << "- manual_ui_panel_scope: `handled_by_other_thread`\n";
    file << "- model_semantic_quality: `NOT_CLAIMED`\n";
    file << "- detection_non_empty_result: `UNVERIFIED`\n";
    file << "- findsegmentation_prompt_edgesam: `PENDING_BINDING`\n\n";

    file << "| Bucket | Scripts | Semantic Status | Example Scripts |\n";
    file << "|---|---:|---|---|\n";
    for (const BucketSummary &summary : summaries) {
      file << "| " << summary.label << " | " << summary.script_count << " | "
           << summary.semantic_status << " | ";
      for (std::size_t i = 0; i < summary.scripts.size() && i < 4; ++i) {
        if (i > 0)
          file << "<br>";
        file << "`" << summary.scripts[i] << "`";
      }
      if (summary.scripts.empty())
        file << "`MISSING`";
      file << " |\n";
    }
  }

  reason = allPresent
               ? "evidence chain catalog semantic buckets are present: " +
                     jsonPath.string()
               : "one or more Torch/FindSegmentation evidence chain buckets "
                 "are missing: " +
                     jsonPath.string();

  return allPresent;
}

void ViewController::EnsureEvidenceChainThumbnailsLoaded() {
  if (m_manualTest.workbench_assets_loaded)
    return;

  for (const auto &group : m_manualTest.script_evidence_groups) {
    for (const auto &thumb : group.thumbs) {
      EvidenceChainThumb ect;
      ect.script_id = thumb.script_id;
      ect.tool = thumb.tool;
      m_manualTest.evidence_chain_thumbs.push_back(ect);
    }
  }

  m_manualTest.workbench_assets_loaded = true;
}

void ViewController::SelectEvidenceChainThumb(int index) {
  if (index < 0 ||
      index >= static_cast<int>(m_manualTest.evidence_chain_thumbs.size()))
    return;

  m_manualTest.selected_evidence_thumb = index;
  const EvidenceChainThumb &ect = m_manualTest.evidence_chain_thumbs[index];

  for (const auto &entry : m_manualTest.catalog_entries) {
    if (entry.script_id == ect.script_id) {
      m_manualTest.editor_source = "catalog";
      ReadTextFile(entry.path, m_manualTest.editor_text);
      m_manualTest.editor_dirty = false;
      m_manualTest.debug_action = "Load Script";
      m_manualTest.debug_status = "PENDING";
      m_manualTest.debug_reason = "Loaded from catalog: " + ect.script_id;
      break;
    }
  }
}

void ViewController::DrawEvidenceChainThumbnailRail() {
  if (m_manualTest.evidence_chain_thumbs.empty())
    return;

  ImGui::BeginChild("evidence_chain_rail", ImVec2(-1, 80), true);

  const float thumbWidth = 64.0f;
  const float thumbHeight = 64.0f;
  const float spacing = 8.0f;
  const int visibleCount = static_cast<int>(ImGui::GetContentRegionAvail().x /
                                            (thumbWidth + spacing));

  const int startIndex =
      std::max(0, m_manualTest.selected_evidence_thumb - visibleCount / 2);

  for (int i = startIndex;
       i < static_cast<int>(m_manualTest.evidence_chain_thumbs.size()) &&
       i < startIndex + visibleCount + 1;
       ++i) {
    if (i > startIndex)
      ImGui::SameLine();
    const EvidenceChainThumb &ect = m_manualTest.evidence_chain_thumbs[i];
    const bool isSelected = i == m_manualTest.selected_evidence_thumb;

    ImGui::PushID(i);
    if (isSelected)
      ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(40, 100, 200, 255));

    if (ImGui::ImageButton(
            ("thumb_" + std::to_string(i)).c_str(),
            static_cast<ImU64>(ect.texture_id ? ect.texture_id : 1),
            ImVec2(thumbWidth, thumbHeight))) {
      SelectEvidenceChainThumb(i);
    }

    if (isSelected)
      ImGui::PopStyleColor();

    ImGui::SetTooltip("%s\n%s", ect.script_id.c_str(), ect.tool.c_str());
    ImGui::PopID();
  }

  ImGui::EndChild();
}

void ViewController::RebuildScriptEvidenceGroups() {
  for (auto &group : m_manualTest.script_evidence_groups) {
    for (auto &thumb : group.thumbs)
      ResetEvidenceThumbTexture(thumb);
  }
  m_manualTest.script_evidence_groups.clear();
  m_manualTest.selected_evidence_group = -1;
  m_manualTest.selected_evidence_thumb = -1;
  m_manualTest.current_evidence_selection = CxEvidenceSelectionSnapshot{};
  m_manualTest.script_evidence_groups_dirty = true;
  m_manualTest.script_evidence_row_refs_dirty = true;
}

void ViewController::RebuildScriptEvidenceRowRefs() {
  m_manualTest.script_evidence_row_refs.clear();

  for (std::size_t gi = 0; gi < m_manualTest.script_evidence_groups.size();
       ++gi) {
    ScriptEvidenceRowRef header;
    header.group_index = static_cast<int>(gi);
    header.thumb_index = -1;
    header.is_group_header = true;
    header.label = m_manualTest.script_evidence_groups[gi].label;
    m_manualTest.script_evidence_row_refs.push_back(header);

    for (std::size_t ti = 0;
         ti < m_manualTest.script_evidence_groups[gi].thumbs.size(); ++ti) {
      ScriptEvidenceRowRef row;
      row.group_index = static_cast<int>(gi);
      row.thumb_index = static_cast<int>(ti);
      row.is_group_header = false;
      row.label = m_manualTest.script_evidence_groups[gi].thumbs[ti].script_id;
      m_manualTest.script_evidence_row_refs.push_back(row);
    }
  }

  m_manualTest.script_evidence_row_refs_dirty = false;
}

std::string
ViewController::ResolveImagePathFromManifest(const std::string &imageId) const {
  for (const auto &item : m_manualTest.evidence_items) {
    if (item.image_id == imageId && !item.image_path.empty())
      return item.image_path;
  }
  for (const auto &item : m_manualTest.image_manifest_items) {
    if (item.image_id == imageId && !item.image_path.empty())
      return item.image_path;
  }
  return "";
}

std::string ViewController::ResolveCatalogScriptPathById(
    const std::string &scriptId) const {
  for (const auto &entry : m_manualTest.catalog_entries) {
    if (entry.script_id == scriptId)
      return entry.path;
  }
  return "";
}

std::string ViewController::ResolveCatalogScriptLabelById(
    const std::string &scriptId) const {
  for (const auto &entry : m_manualTest.catalog_entries) {
    if (entry.script_id == scriptId)
      return entry.label.empty() ? entry.script_id : entry.label;
  }
  return scriptId;
}

void ViewController::EnsureScriptEvidenceThumbTexture(
    ScriptEvidenceThumb &thumb) {
  if (thumb.texture_loaded && !thumb.texture_placeholder)
    return;

  if (thumb.texture_failed && thumb.thumbnail_path.empty() &&
      thumb.image_path.empty() && thumb.image_id.empty() &&
      thumb.dataset_images.empty() && thumb.workflow_id.empty()) {
    return;
  }

  if (m_manualTest.script_evidence_thumb_load_count_this_frame >=
      m_manualTest.script_evidence_thumb_load_budget_per_frame) {
    return;
  }

  if (thumb.image_path.empty() && !thumb.image_id.empty())
    thumb.image_path = ResolveImagePathFromManifest(thumb.image_id);
  if (thumb.image_path.empty() && !thumb.image_id.empty())
    thumb.image_path =
        ResolveEvidenceImagePathFromContextLocal(m_manualTest, thumb.image_id);

  std::vector<std::string> previewCandidates;
  auto addPreviewCandidate = [&](const std::string &path) {
    if (path.empty())
      return;
    if (std::find(previewCandidates.begin(), previewCandidates.end(), path) ==
        previewCandidates.end()) {
      previewCandidates.push_back(path);
    }
  };
  addPreviewCandidate(thumb.thumbnail_path);
  addPreviewCandidate(thumb.image_path);
  if (!thumb.image_id.empty())
    addPreviewCandidate(ResolveImagePathFromManifest(thumb.image_id));
  if (!thumb.image_id.empty())
    addPreviewCandidate(
        ResolveEvidenceImagePathFromContextLocal(m_manualTest, thumb.image_id));
  for (const CxEvidenceDatasetImageBinding &datasetImage :
       thumb.dataset_images) {
    addPreviewCandidate(datasetImage.image_path);
  }
  if (previewCandidates.empty() && !thumb.workflow_id.empty()) {
    for (const ScriptEvidenceGroup &group :
         m_manualTest.script_evidence_groups) {
      for (const ScriptEvidenceThumb &workflowThumb : group.thumbs) {
        if (workflowThumb.workflow_id != thumb.workflow_id)
          continue;
        for (const CxEvidenceDatasetImageBinding &datasetImage :
             workflowThumb.dataset_images) {
          addPreviewCandidate(datasetImage.image_path);
        }
        if (!previewCandidates.empty())
          break;
      }
      if (!previewCandidates.empty())
        break;
    }
  }

  if (previewCandidates.empty()) {
    cv::Mat placeholder(60, 80, CV_8UC3, cv::Scalar(90, 120, 150));
    cv::putText(placeholder, "NO IMG", cv::Point(12, 36),
                cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255), 1,
                cv::LINE_AA);

    thumb.texture_id = CreateTextureFromMat0(placeholder);
    thumb.texture_w = placeholder.cols;
    thumb.texture_h = placeholder.rows;
    thumb.texture_loaded = thumb.texture_id != 0;
    thumb.texture_failed = !thumb.texture_loaded;
    thumb.texture_placeholder = thumb.texture_loaded;
    thumb.reason = "placeholder thumbnail generated; image path is empty";
    ++m_manualTest.script_evidence_thumb_load_count_this_frame;
    return;
  }

  cv::Mat image;
  std::string loadedPreviewPath;
  std::string lastFailure;
  for (const std::string &previewPath : previewCandidates) {
    const std::filesystem::path resolvedPreview =
        ResolveWorkspaceFile(previewPath).lexically_normal();
    if (!std::filesystem::exists(resolvedPreview)) {
      lastFailure = "thumbnail image not found: " + resolvedPreview.string();
      continue;
    }

    image = cv::imread(resolvedPreview.string(), cv::IMREAD_COLOR);
    if (!image.empty()) {
      loadedPreviewPath = resolvedPreview.string();
      break;
    }
    lastFailure = "thumbnail image read failed: " + resolvedPreview.string();
  }

  if (image.empty()) {
    cv::Mat placeholder(60, 80, CV_8UC3, cv::Scalar(90, 120, 150));
    cv::putText(placeholder, "NO IMG", cv::Point(12, 36),
                cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255), 1,
                cv::LINE_AA);

    thumb.texture_id = CreateTextureFromMat0(placeholder);
    thumb.texture_w = placeholder.cols;
    thumb.texture_h = placeholder.rows;
    thumb.texture_loaded = thumb.texture_id != 0;
    thumb.texture_failed = !thumb.texture_loaded;
    thumb.texture_placeholder = thumb.texture_loaded;
    thumb.reason =
        lastFailure.empty() ? "thumbnail image unavailable" : lastFailure;
    ++m_manualTest.script_evidence_thumb_load_count_this_frame;
    return;
  }

  cv::Mat preview;
  const int maxSide = 80;
  const int srcMaxSide = std::max(image.cols, image.rows);
  const double scale = srcMaxSide > 0 ? static_cast<double>(maxSide) /
                                            static_cast<double>(srcMaxSide)
                                      : 1.0;
  if (scale > 0.0 && scale < 1.0)
    cv::resize(image, preview, cv::Size(), scale, scale, cv::INTER_AREA);
  else
    preview = image;

  thumb.texture_id = CreateTextureFromMat0(preview);
  thumb.texture_w = preview.cols;
  thumb.texture_h = preview.rows;
  thumb.texture_loaded = thumb.texture_id != 0;
  thumb.texture_failed = !thumb.texture_loaded;
  thumb.texture_placeholder = false;
  if (thumb.texture_loaded)
    thumb.thumbnail_path = loadedPreviewPath;
  if (thumb.texture_failed)
    thumb.reason = "failed to create thumbnail texture";
  else if (thumb.thumbnail_path != loadedPreviewPath)
    thumb.reason = "thumbnail loaded from image fallback: " + loadedPreviewPath;
  ++m_manualTest.script_evidence_thumb_load_count_this_frame;
}

bool ViewController::ActivateScriptEvidenceThumb(
    const ScriptEvidenceThumb &thumb, bool loadImageToView,
    std::string &reason) {
  reason.clear();

  int groupIndex = m_manualTest.selected_evidence_group;
  int thumbIndex = m_manualTest.selected_evidence_thumb;

  bool indexMatches = false;
  if (groupIndex >= 0 &&
      groupIndex <
          static_cast<int>(m_manualTest.script_evidence_groups.size())) {
    const ScriptEvidenceGroup &group =
        m_manualTest.script_evidence_groups[groupIndex];

    if (thumbIndex >= 0 && thumbIndex < static_cast<int>(group.thumbs.size())) {
      const ScriptEvidenceThumb &selectedThumb = group.thumbs[thumbIndex];
      indexMatches = selectedThumb.script_id == thumb.script_id &&
                     selectedThumb.script_path == thumb.script_path;
    }
  }

  if (!indexMatches) {
    groupIndex = -1;
    thumbIndex = -1;
  }

  CxEvidenceSelectionSnapshot snapshot;
  if (!BuildEvidenceSnapshotFromThumb(groupIndex, thumbIndex, thumb, snapshot,
                                      reason)) {
    return false;
  }

  return ApplyEvidenceSelectionSnapshotToManualContext(snapshot,
                                                       loadImageToView, reason);
}

bool ViewController::BuildEvidenceSnapshotFromThumb(
    int groupIndex, int thumbIndex, const ScriptEvidenceThumb &thumb,
    CxEvidenceSelectionSnapshot &out, std::string &reason) const {
  reason.clear();
  out = CxEvidenceSelectionSnapshot{};

  std::string scriptPath = thumb.script_path;
  if (scriptPath.empty())
    scriptPath = ResolveCatalogScriptPathById(thumb.script_id);

  const bool isDeprecatedScript = IsDeprecatedCxScriptPath(scriptPath);

  if (thumb.script_id.empty() && scriptPath.empty()) {
    reason = "evidence thumb has neither script_id nor script_path";
    return false;
  }

  out.valid = true;
  out.group_index = groupIndex;
  out.thumb_index = thumbIndex;

  out.case_id = thumb.case_id;
  out.review_item = thumb.review_item;

  out.candidate_id = thumb.candidate_id;
  out.candidate_dir = thumb.candidate_dir;
  out.evidence_binding_path = thumb.evidence_binding_path;
  out.parameter_snapshot_path = thumb.parameter_snapshot_path;
  out.runtime_globals_path = thumb.runtime_globals_path;
  out.gauge_annotation_path = thumb.gauge_annotation_path;
  out.working_script_snapshot_path = thumb.working_script_snapshot_path;
  out.is_candidate = thumb.is_candidate;
  out.has_saved_state = thumb.has_saved_state;
  out.source_evidence_script_path = thumb.source_evidence_script_path.empty()
                                        ? scriptPath
                                        : thumb.source_evidence_script_path;

  out.script_id = thumb.script_id.empty() ? scriptPath : thumb.script_id;
  out.script_path = scriptPath;

  out.image_id = thumb.image_id;
  out.image_path = thumb.image_path;
  if (out.image_path.empty() && !out.image_id.empty())
    out.image_path =
        ResolveEvidenceImagePathFromContextLocal(m_manualTest, out.image_id);

  out.target_id = thumb.target_id;
  out.tool = thumb.tool;

  out.parameter_summary = thumb.parameter_summary;
  out.parameter_profile_id = thumb.parameter_summary;

  out.evidence_output_root = thumb.evidence_output_root;
  out.contract_id = thumb.contract_id;
  out.expected_result = thumb.expected_result;
  out.expected_policy_guard = thumb.expected_policy_guard;
  out.evidence_level = thumb.evidence_level;
  out.evidence_case_role = thumb.evidence_case_role;
  out.source_case_id = thumb.source_case_id;
  out.manual_review_required = thumb.manual_review_required;
  out.promotion_candidate = thumb.promotion_candidate;
  out.evidence_group_override = thumb.evidence_group_override;
  out.workflow_id = thumb.workflow_id;
  out.workflow_stage = thumb.workflow_stage;
  out.workflow_status = thumb.workflow_status;
  out.workflow_prerequisites = thumb.workflow_prerequisites;
  out.dataset_role = thumb.dataset_role;
  out.annotation_policy = thumb.annotation_policy;
  out.gate_policy = thumb.gate_policy;
  out.parent_model_ref = thumb.parent_model_ref;
  out.child_model_ref = thumb.child_model_ref;
  out.workflow_stage_index = thumb.workflow_stage_index;
  out.workflow_stage_count = thumb.workflow_stage_count;
  out.dataset_frozen = thumb.dataset_frozen;
  out.admission_status = thumb.admission_status;
  out.admission_reason = thumb.admission_reason;
  out.dataset_summary_ref = thumb.dataset_summary_ref;
  out.training_receipt_ref = thumb.training_receipt_ref;
  out.candidate_artifact_ref = thumb.candidate_artifact_ref;
  out.evaluation_report_ref = thumb.evaluation_report_ref;
  out.evidence_bundle_ref = thumb.evidence_bundle_ref;
  out.rollback_model_ref = thumb.rollback_model_ref;
  out.quality_conclusion_ref = thumb.quality_conclusion_ref;
  out.training_config_ref = thumb.training_config_ref;
  out.model_manifest_ref = thumb.model_manifest_ref;
  out.inference_config_ref = thumb.inference_config_ref;
  out.quality_policy_ref = thumb.quality_policy_ref;
  out.ontology_ref = thumb.ontology_ref;
  out.failure_samples_ref = thumb.failure_samples_ref;

  out.status = thumb.status;
  out.reason = thumb.reason;
  if (isDeprecatedScript) {
    if (out.status.empty())
      out.status = "legacy_script";
    if (out.reason.empty()) {
      out.reason = "deprecated cxscript selected for viewing only; run/bind "
                   "gates may reject it: " +
                   scriptPath;
    }
  }
  out.source = "evidence_thumb";
  out.primary_object_type = thumb.primary_object_type;
  out.primary_object_name = thumb.primary_object_name;
  out.primary_object_status = thumb.primary_object_status;
  out.dataset_images = thumb.dataset_images;
  out.annotations = thumb.annotations;
  out.training_run = thumb.training_run;

  if (MigrateLegacyEvidenceSelectionSnapshotLocal(out)) {
    CXLOG_INFO("EvidenceChain", "legacy_candidate_schema_migrated",
               "default_parameter_added",
               "script_id=" + out.script_id + " case_id=" + out.case_id +
                   " candidate_id=" + out.candidate_id +
                   " parameter_summary=" + out.parameter_summary);
  }

  if (!scriptPath.empty()) {
    std::string scriptText;
    if (ReadTextFile(scriptPath, scriptText)) {
      AnalyzeEditableObjectsFromCxScriptLocal(scriptText, out.editable_objects);
      if (out.primary_object_status.empty() ||
          out.primary_object_status == "script_read_failed" ||
          out.primary_object_status == "script_path_empty") {
        ResolvePrimaryEditableObjectLocal(
            out.tool, out.target_id, out.parameter_summary,
            out.editable_objects, out.primary_object_type,
            out.primary_object_name, out.primary_object_status);
      }
    }
  }

  return true;
}

bool ViewController::GetSelectedEvidenceSnapshot(
    CxEvidenceSelectionSnapshot &out, std::string &reason) const {
  reason.clear();
  out = CxEvidenceSelectionSnapshot{};

  const int groupIndex = m_manualTest.selected_evidence_group;
  const int thumbIndex = m_manualTest.selected_evidence_thumb;

  if (groupIndex < 0 ||
      groupIndex >=
          static_cast<int>(m_manualTest.script_evidence_groups.size())) {
    reason = "no evidence group selected";
    return false;
  }

  const ScriptEvidenceGroup &group =
      m_manualTest.script_evidence_groups[groupIndex];

  if (thumbIndex < 0 || thumbIndex >= static_cast<int>(group.thumbs.size())) {
    reason = "no evidence thumb selected";
    return false;
  }

  return BuildEvidenceSnapshotFromThumb(groupIndex, thumbIndex,
                                        group.thumbs[thumbIndex], out, reason);
}

static bool
IsEvidenceSelectionImageSetLocal(const CxEvidenceSelectionSnapshot &sel);

bool ViewController::ApplyEvidenceSelectionSnapshotToManualContext(
    const CxEvidenceSelectionSnapshot &snapshot, bool loadImageToView,
    std::string &reason) {
  reason.clear();

  if (!snapshot.valid) {
    reason = "invalid evidence selection snapshot";
    return false;
  }

  CXLOG_INFO(
      "EvidenceChain", "evidence_selection_begin", "staging",
      "script_id=" + snapshot.script_id + " case_id=" + snapshot.case_id +
          " image_id=" + snapshot.image_id +
          " image_path=" + snapshot.image_path +
          " candidate=" + (snapshot.is_candidate ? "true" : "false") +
          " has_saved_state=" + (snapshot.has_saved_state ? "true" : "false") +
          " dataset_images=" + std::to_string(snapshot.dataset_images.size()) +
          " annotations=" + std::to_string(snapshot.annotations.size()) +
          " working_script=" + snapshot.working_script_snapshot_path +
          " load_image=" + (loadImageToView ? "true" : "false"));

  auto abortSelection = [&](const std::string &stage,
                            const std::string &message) -> bool {
    reason = message;
    CXLOG_ERROR(
        "EvidenceChain", "evidence_selection_abort", stage,
        "script_id=" + snapshot.script_id + " case_id=" + snapshot.case_id +
            " candidate_id=" + snapshot.candidate_id + " reason=" + message);
    return false;
  };

  CxEvidenceSelectionSnapshot resolved = snapshot;
  if (IsDerivedEvidenceRenderPathLocal(resolved.image_path)) {
    const std::string originalImage = ResolveOriginalImageFromRunDirectoryLocal(
        std::filesystem::path(resolved.image_path).parent_path());
    if (originalImage.empty()) {
      return abortSelection(
          "derived_image_rejected",
          "Evidence overlay is review-only and no immutable source image could be resolved");
    }
    CXLOG_INFO("EvidenceChain", "execution_image_remapped", "source_resolved",
               "review_image=" + resolved.image_path +
                   " execution_image=" + originalImage);
    resolved.image_path = originalImage;
  }
  const bool loadWorkingRevision =
      !resolved.is_candidate && resolved.has_saved_state;
  if (loadWorkingRevision && resolved.working_script_snapshot_path.empty()) {
    return abortSelection(
        "working_script_missing",
        "Evidence working revision is missing script_snapshot_path");
  }
  const std::string effectiveScriptPath =
      loadWorkingRevision ? resolved.working_script_snapshot_path
                          : resolved.script_path;
  std::string scriptText;
  std::filesystem::path resolvedScriptPath;
  if (!effectiveScriptPath.empty()) {
    resolvedScriptPath = ResolveWorkspaceFile(effectiveScriptPath);
    if (!ReadTextFile(resolvedScriptPath.string(), scriptText)) {
      return abortSelection("script_read", "failed to read evidence script: " +
                                               resolvedScriptPath.string());
    }

    scriptText =
        EnsureFindLineSelectedEdgeStatementLocal(resolved.tool, scriptText);

    if (resolved.is_candidate || loadWorkingRevision)
      resolved.editable_objects.clear();
    if (resolved.editable_objects.empty())
      AnalyzeEditableObjectsFromCxScriptLocal(scriptText,
                                              resolved.editable_objects);

    if (resolved.primary_object_name.empty() ||
        resolved.primary_object_status.empty() ||
        resolved.primary_object_status == "none" ||
        resolved.primary_object_status == "unresolved") {
      ResolvePrimaryEditableObjectLocal(
          resolved.tool, resolved.target_id, resolved.parameter_summary,
          resolved.editable_objects, resolved.primary_object_type,
          resolved.primary_object_name, resolved.primary_object_status);
    }
  }

  cv::Mat stagedImage;
  std::filesystem::path resolvedImagePath;
  if (loadImageToView) {
    std::string imagePathForLoad = resolved.image_path;
    if (imagePathForLoad.empty() && !snapshot.image_id.empty())
      imagePathForLoad = ResolveImagePathFromManifest(snapshot.image_id);
    if (imagePathForLoad.empty() && !snapshot.image_id.empty())
      imagePathForLoad = ResolveEvidenceImagePathFromContextLocal(
          m_manualTest, snapshot.image_id);

    if (imagePathForLoad.empty()) {
      return abortSelection("image_path",
                            "selected evidence has empty image_path");
    }

    resolved.image_path = imagePathForLoad;
    resolvedImagePath = ResolveWorkspaceFile(imagePathForLoad);
    if (!std::filesystem::is_regular_file(resolvedImagePath)) {
      return abortSelection("image_file", "image file not found: " +
                                              resolvedImagePath.string());
    }

    stagedImage = cv::imread(resolvedImagePath.string(), cv::IMREAD_COLOR);
    if (stagedImage.empty()) {
      return abortSelection("image_decode", "failed to read image: " +
                                                resolvedImagePath.string());
    }
  }

  ManualTestContext staged = m_manualTest;
  staged.runtime_int_vars.clear();

  staged.runtime_objects.clear();

  staged.current_evidence_selection = resolved;
  staged.torch_training_run = resolved.training_run;
  staged.selected_evidence_group = resolved.group_index;
  staged.selected_evidence_thumb = resolved.thumb_index;
  staged.active_case_id = resolved.case_id;
  staged.active_image_id = resolved.image_id;
  staged.active_target_id = resolved.target_id;
  staged.current_gauge = ManualGaugeState{};
  staged.current_gauge.case_id = resolved.case_id;
  staged.current_gauge.image_id = resolved.image_id;
  staged.current_gauge.target_id = resolved.target_id;

  staged.key_parameter_edit_revision = 0;
  staged.last_key_parameter_edit_summary =
      "evidence selection baseline: " + resolved.script_id;
  staged.fastmatch_last_requested_revision = 0;
  staged.fastmatch_last_requested_action = 0;

  staged.manual_operation_trace_sequence = 0;
  staged.pending_manual_operation_trace_events.clear();

  if (!resolved.image_path.empty())
    staged.image_file_path = resolved.image_path;

  if (!effectiveScriptPath.empty()) {
    staged.editor_text = scriptText;
    staged.loaded_script_path = resolvedScriptPath.string();
    staged.script_file_path = resolvedScriptPath.string();
    staged.editor_source =
        resolved.is_candidate
            ? "evidence_candidate"
            : (loadWorkingRevision ? "evidence_working_revision" : "evidence");
    staged.editor_dirty = false;

    SeedDefaultManualGlobals(staged, effectiveScriptPath);
    staged.current_gauge.case_id = resolved.case_id;
    staged.current_gauge.image_id = resolved.image_id;
    staged.current_gauge.target_id = resolved.target_id;
    if (!resolved.tool.empty()) {
      const std::string normalizedEvidenceTool =
          NormalizeEvidenceToolTypeLocal(resolved.tool);
      if (normalizedEvidenceTool == "TorchTask") {
        ManualGaugeState torchContext;
        torchContext.case_id = resolved.case_id;
        torchContext.image_id = resolved.image_id;
        torchContext.target_id = resolved.target_id;
        torchContext.tool = "TorchTask";
        torchContext.source = "evidence";
        torchContext.review_status = "editing";
        staged.current_gauge = torchContext;
      } else {
        staged.current_gauge.tool = normalizedEvidenceTool;
      }
    }
    staged.current_gauge.primary_object_type = resolved.primary_object_type;
    staged.current_gauge.primary_object_name = resolved.primary_object_name;
    staged.current_gauge.primary_object_status =
        resolved.primary_object_status.empty() ? "unresolved"
                                               : resolved.primary_object_status;
  }

  std::string parameterSource = "tool_defaults";
  if (resolved.is_candidate || loadWorkingRevision) {
    if (resolved.runtime_globals_path.empty() ||
        resolved.gauge_annotation_path.empty()) {
      return abortSelection(
          "working_assets",
          "candidate is missing runtime_globals_path or gauge_annotation_path");
    }

    std::string restoreReason;
    if (!ApplyCandidateRuntimeGlobalsLocal(
            staged, resolved.runtime_globals_path, restoreReason)) {
      return abortSelection("runtime_globals_restore", restoreReason);
    }
    if (!LoadManualGaugeWorkingCopyFromPath(
            staged, resolved.gauge_annotation_path, restoreReason)) {
      return abortSelection("gauge_restore",
                            "failed to restore candidate gauge: " +
                                restoreReason);
    }

    CXLOG_INFO(
        "EvidenceChain", "candidate_parameter_state_restored", "RESTORED",
        "case_id=" + resolved.case_id +
            " candidate_id=" + resolved.candidate_id + " circle_arc_enabled=" +
            std::to_string(staged.current_gauge.circle_arc_enabled ? 1 : 0) +
            " circle_arc_start_deg=" +
            std::to_string(staged.current_gauge.circle_arc_start_deg) +
            " circle_arc_end_deg=" +
            std::to_string(staged.current_gauge.circle_arc_end_deg) +
            " findcircle_selected_edge=" +
            std::to_string(staged.findcircle_selected_scan_edge) +
            " findcircle_edge_count=" +
            std::to_string(staged.findcircle_scan_edge_count) +
            " findline_selected_edge=" +
            std::to_string(staged.findline_selected_scan_edge) +
            " findline_edge_count=" +
            std::to_string(staged.findline_scan_edge_count));

    staged.current_gauge.source = resolved.is_candidate
                                      ? "evidence_candidate"
                                      : "evidence_working_revision";
    staged.current_gauge.case_id = resolved.case_id;
    staged.current_gauge.image_id = resolved.image_id;
    staged.current_gauge.target_id = resolved.target_id;
    if (staged.current_gauge.primary_object_type.empty())
      staged.current_gauge.primary_object_type = resolved.primary_object_type;
    if (staged.current_gauge.primary_object_name.empty())
      staged.current_gauge.primary_object_name = resolved.primary_object_name;
    if (staged.current_gauge.primary_object_status.empty() ||
        staged.current_gauge.primary_object_status == "unresolved") {
      staged.current_gauge.primary_object_status =
          resolved.primary_object_status.empty()
              ? "restored_from_candidate"
              : resolved.primary_object_status;
    }
    staged.current_gauge.dirty = false;
    staged.current_evidence_selection.primary_object_type =
        staged.current_gauge.primary_object_type;
    staged.current_evidence_selection.primary_object_name =
        staged.current_gauge.primary_object_name;
    staged.current_evidence_selection.primary_object_status =
        staged.current_gauge.primary_object_status;
    parameterSource = resolved.is_candidate ? "candidate_snapshot"
                                            : "active_working_revision";
  } else {
    std::string lockedParamReason;
    if (EvidenceSnapshotHasLockedParamSummaryLocal(resolved,
                                                    lockedParamReason)) {
      if (!ApplyEvidenceParameterSummaryToRuntimeGlobals(
              staged, resolved.parameter_summary, lockedParamReason)) {
        const std::string unsupportedPrefix =
            "no supported key=value token found in parameter summary:";
        if (lockedParamReason.rfind(unsupportedPrefix, 0) == 0) {
          parameterSource = "evidence_metadata";
        } else {
          return abortSelection(
              "parameter_summary_apply",
              "failed to apply evidence locked parameters: " +
                  lockedParamReason);
        }
      } else {
        SyncEvidenceLockedGlobalsToManualGaugeLocal(
            staged, resolved.script_path, "evidence_locked",
            resolved.primary_object_type, resolved.primary_object_name,
            resolved.primary_object_status);
        staged.current_evidence_selection.primary_object_type =
            staged.current_gauge.primary_object_type;
        staged.current_evidence_selection.primary_object_name =
            staged.current_gauge.primary_object_name;
        staged.current_evidence_selection.primary_object_status =
            staged.current_gauge.primary_object_status;
        parameterSource = resolved.workflow_id.empty()
                              ? "evidence_parameter_snapshot"
                              : "workflow_metadata+evidence_parameter_snapshot";
      }
    } else if (!resolved.workflow_id.empty()) {
      // Workflow fields describe the review/promotion route.  They are not a
      // replacement for the image/tool inputs that created the Evidence.
      parameterSource = "workflow_metadata";
    }
  }

  if (NormalizeEvidenceToolTypeLocal(resolved.tool) == "TorchTask") {
    auto readSummaryInt = [&](const std::string &key, int fallback) {
      const std::string value = ReadKeyValueFromEvidenceParamSummaryLocal(
          resolved.parameter_summary, key);
      if (value.empty())
        return fallback;
      try {
        return std::stoi(value);
      } catch (...) {
        return fallback;
      }
    };
    const std::string resultRef = ReadKeyValueFromEvidenceParamSummaryLocal(
        resolved.parameter_summary, "torch_result_ref");
    const std::string overlayRef = ReadKeyValueFromEvidenceParamSummaryLocal(
        resolved.parameter_summary, "torch_overlay_ref");
    const std::string matrixRef = ReadKeyValueFromEvidenceParamSummaryLocal(
        resolved.parameter_summary, "torch_stability_matrix");
    const std::string manifestRef = ReadKeyValueFromEvidenceParamSummaryLocal(
        resolved.parameter_summary, "torch_model_manifest");
    std::error_code resultEc;
    std::error_code overlayEc;
    const bool hasResultArtifact =
        !resultRef.empty() &&
        std::filesystem::is_regular_file(resultRef, resultEc);
    const bool hasOverlayArtifact =
        !overlayRef.empty() &&
        std::filesystem::is_regular_file(overlayRef, overlayEc);
    if (hasResultArtifact || hasOverlayArtifact) {
      RuntimeObjectView artifact;
      artifact.name = resolved.primary_object_name.empty()
                          ? "evidence_artifact"
                          : resolved.primary_object_name;
      artifact.type = "TorchTask";
      artifact.exists_in_parser = false;
      artifact.last_runtime_status = "EVIDENCE_ARTIFACT_LOADED";
      artifact.runtime_state = "READ_ONLY_EVIDENCE";
      artifact.display_summary =
          "Existing Evidence artifacts loaded without algorithm re-execution";
      artifact.visualizable = hasOverlayArtifact;
      artifact.visual_source = "evidence_artifact";
      artifact.stale = false;
      artifact.is_torch_task = true;
      artifact.torch_ok =
          readSummaryInt("torch_inference_ok", hasResultArtifact ? 1 : 0);
      artifact.torch_result_count = readSummaryInt("torch_result_count", 0);
      artifact.torch_mask_available = hasOverlayArtifact ? 1 : 0;
      artifact.torch_status = "EVIDENCE_ARTIFACT_LOADED";
      artifact.torch_reason =
          "Read-only Evidence projection; TorchTask was not re-executed";
      artifact.torch_result_ref = resultRef;
      artifact.torch_evidence_ref = matrixRef.empty() ? manifestRef : matrixRef;
      artifact.torch_primary_visual_ref = overlayRef;
      artifact.torch_overlay_ref = overlayRef;
      artifact.torch_unified_mainline_summary =
          "result and overlay references restored from Evidence package";
      staged.runtime_objects.push_back(std::move(artifact));
    }
  }

  const std::string selectionReason =
      "script=" + resolved.script_id + " image=" + resolved.image_id +
      " image_path=" + resolved.image_path + " target=" + resolved.target_id +
      " parameter_source=" + parameterSource +
      ((resolved.is_candidate || loadWorkingRevision)
           ? " candidate_id=" + resolved.candidate_id
           : " baseline_evidence=true");
  staged.debug_action = "Apply Evidence Selection";
  staged.debug_status = loadImageToView ? "EVIDENCE_SELECTION_READY_WITH_IMAGE"
                                        : "EVIDENCE_SELECTION_READY";
  staged.debug_reason = selectionReason;

  if (loadWorkingRevision) {
    // Case switching clears parser-owned runtime objects above.  Restore the
    // saved value state and ask the existing Debug Compiler path to recreate
    // the runtime on the next frame; never retain pointers from another case.
    staged.pending_execution_gauge = staged.current_gauge;
    staged.pending_execution_globals = staged.runtime_int_vars;
    staged.has_pending_execution_snapshot = true;
    staged.pending_evidence_selection_rehydrate = true;
    staged.debug_action = "Restore Saved Evidence Case";
    staged.debug_status = "EVIDENCE_SELECTION_REHYDRATE_QUEUED";
    staged.debug_reason = selectionReason +
                          " | saved values restored; runtime rehydrate queued";
    staged.run_state = "running";
    CXLOG_INFO("EvidenceChain", "evidence_selection_rehydrate_queued",
               "queued", "case_id=" + resolved.case_id +
                   " script_id=" + resolved.script_id +
                   " candidate_id=" + resolved.candidate_id);
  }

  const bool shouldSyncTrainingImageSet =
      IsEvidenceSelectionImageSetLocal(resolved) ||
      !resolved.dataset_images.empty();

  m_manualTest = std::move(staged);
  m_runtimeShapeSyncPending = false;
  m_runtimeShapeSyncReason.clear();
  m_runtimeShapeSyncDeferCount = 0;
  m_scriptResult.result_ref.clear();
  m_scriptResult.overlay_ref.clear();
  m_scriptResult.evidence_ref.clear();
  m_scriptResult.issue_entry_ref.clear();
  m_scriptResult.runtime_fillback_status =
      "evidence_selection_cleared_previous_runtime";

  if (shouldSyncTrainingImageSet) {
    CXLOG_INFO(
        "TorchTrainingImageSet", "evidence_image_set_detected", "sync_begin",
        "script_id=" + resolved.script_id + " case_id=" + resolved.case_id +
            " image_id=" + resolved.image_id);
    SyncTorchTrainingImageSetFromEvidenceSelection();
    CXLOG_INFO("TorchTrainingImageSet", "evidence_image_set_synced",
               "sync_done",
               "script_id=" + resolved.script_id +
                   " case_id=" + resolved.case_id + " image_count=" +
                   std::to_string(m_manualTest.torch_training_images.size()) +
                   " reason=" + m_manualTest.torch_training_image_reason);
  } else {
    CXLOG_INFO("EvidenceChain", "evidence_single_image_selection",
               "no_training_set_sync",
               "script_id=" + resolved.script_id + " case_id=" +
                   resolved.case_id + " image_id=" + resolved.image_id);
  }

  if (loadImageToView) {
    UpdateImageViewImage(stagedImage);
    m_manualTest.image_file_path = resolvedImagePath.string();
    m_scriptResult.image_ref = resolvedImagePath.string();
    m_scriptResult.reason = "image loaded from evidence selection transaction";
    m_annotationStatus = "image loaded from evidence selection transaction";
    m_imageViewZoom = 1.0f;
    m_imageViewPanX = 0.0f;
    m_imageViewPanY = 0.0f;
    CXLOG_INFO("ImageView", "evidence_image_loaded", "loaded",
               "script_id=" + resolved.script_id + " case_id=" +
                   resolved.case_id + " image_id=" + resolved.image_id +
                   " image_path=" + resolvedImagePath.string());

    std::string previewReason;
    const bool previewOk = ProjectCurrentGaugeToImageViewPreview(previewReason);
    if (previewOk) {
      m_annotationStatus =
          "evidence image and input gauge loaded: " + previewReason;
      CXLOG_INFO(
          "EvidenceChain", "evidence_input_gauge_preview", "projected",
          "script_id=" + resolved.script_id + " case_id=" + resolved.case_id +
              " image_id=" + resolved.image_id + " reason=" + previewReason);
    } else {
      CXLOG_INFO(
          "EvidenceChain", "evidence_input_gauge_preview", "skipped",
          "script_id=" + resolved.script_id + " case_id=" + resolved.case_id +
              " image_id=" + resolved.image_id + " reason=" + previewReason);
    }
  }

  if (resolved.is_candidate || loadWorkingRevision) {
    AppendEvidenceCandidateStateProbe(
        m_manualTest, resolved.candidate_dir, resolved.candidate_id,
        resolved.is_candidate ? "candidate_reload_complete"
                              : "working_revision_reload_complete",
        "ready", m_manualTest.debug_reason);
  }

  reason = m_manualTest.debug_reason;
  const auto committedRuntimeInt = [&](const std::string &key,
                                       int fallback = 0) {
    const auto found = m_manualTest.runtime_int_vars.find(key);
    return found == m_manualTest.runtime_int_vars.end() ? fallback
                                                        : found->second;
  };
  CXLOG_INFO(
      "EvidenceChain", "evidence_selection_commit", "ready",
      m_manualTest.debug_reason + " primary_object=" +
          m_manualTest.current_evidence_selection.primary_object_type + " " +
          m_manualTest.current_evidence_selection.primary_object_name +
          " gauge={threshold=" +
          std::to_string(m_manualTest.current_gauge.threshold) +
          ",method=" + std::to_string(m_manualTest.current_gauge.method) +
          ",linegap=" + std::to_string(m_manualTest.current_gauge.linegap) +
          ",wgap=" + std::to_string(m_manualTest.current_gauge.wgap) +
          ",hgap=" + std::to_string(m_manualTest.current_gauge.hgap) +
           ",filterprofile=" +
          std::to_string(m_manualTest.current_gauge.filterprofile) + "}" +
          " learn_roi=(" +
          std::to_string(committedRuntimeInt("global_learn_roi_x")) + "," +
          std::to_string(committedRuntimeInt("global_learn_roi_y")) + "," +
          std::to_string(committedRuntimeInt("global_learn_roi_w")) + "," +
          std::to_string(committedRuntimeInt("global_learn_roi_h")) + ")" +
          " search_roi=(" +
          std::to_string(committedRuntimeInt("global_search_roi_x")) + "," +
          std::to_string(committedRuntimeInt("global_search_roi_y")) + "," +
          std::to_string(committedRuntimeInt("global_search_roi_w")) + "," +
          std::to_string(committedRuntimeInt("global_search_roi_h")) + ")" +
          " directional_wgap=(" +
          std::to_string(committedRuntimeInt("global_fastmatch_learn_wgap_0")) +
          "," +
          std::to_string(committedRuntimeInt("global_fastmatch_learn_wgap_1")) +
          "," +
          std::to_string(committedRuntimeInt("global_fastmatch_learn_wgap_2")) +
          "," +
          std::to_string(committedRuntimeInt("global_fastmatch_learn_wgap_3")) +
          ") directional_hgap=(" +
          std::to_string(committedRuntimeInt("global_fastmatch_learn_hgap_0")) +
          "," +
          std::to_string(committedRuntimeInt("global_fastmatch_learn_hgap_1")) +
          "," +
          std::to_string(committedRuntimeInt("global_fastmatch_learn_hgap_2")) +
          "," +
          std::to_string(committedRuntimeInt("global_fastmatch_learn_hgap_3")) +
          ")");
  return true;
}

void ViewController::ResetEvidenceThumbTexture(ScriptEvidenceThumb &thumb) {
  if (thumb.texture_id != 0) {
    GLuint texture = static_cast<GLuint>(thumb.texture_id);
    glDeleteTextures(1, &texture);
  }
  thumb.texture_id = 0;
  thumb.texture_w = 0;
  thumb.texture_h = 0;
  thumb.texture_loaded = false;
  thumb.texture_failed = false;
  thumb.texture_placeholder = false;
}

void ViewController::EnsureTorchTrainingImageTexture(
    TorchTrainingImageItem &item) {
  if ((item.texture_loaded && !item.texture_placeholder) ||
      item.texture_failed) {
    return;
  }

  if (m_manualTest.script_evidence_thumb_load_count_this_frame >=
      m_manualTest.script_evidence_thumb_load_budget_per_frame) {
    return;
  }

  cv::Mat image;
  std::filesystem::path resolved;
  if (!item.image_path.empty()) {
    resolved = ResolveWorkspaceFile(item.image_path);
    if (std::filesystem::is_regular_file(resolved))
      image = cv::imread(resolved.string(), cv::IMREAD_COLOR);
  }

  if (image.empty()) {
    cv::Mat placeholder(72, 72, CV_8UC3, cv::Scalar(70, 105, 135));
    cv::putText(placeholder, "NO", cv::Point(17, 32), cv::FONT_HERSHEY_SIMPLEX,
                0.5, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    cv::putText(placeholder, "IMG", cv::Point(12, 52), cv::FONT_HERSHEY_SIMPLEX,
                0.45, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    item.texture_id = CreateTextureFromMat0(placeholder);
    item.texture_w = placeholder.cols;
    item.texture_h = placeholder.rows;
    item.texture_loaded = item.texture_id != 0;
    item.texture_failed = !item.texture_loaded;
    item.texture_placeholder = item.texture_loaded;
    item.status = "image_unavailable";
    ++m_manualTest.script_evidence_thumb_load_count_this_frame;
    return;
  }

  cv::Mat preview;
  const int maxSide = 96;
  const int srcMaxSide = std::max(image.cols, image.rows);
  const double scale = srcMaxSide > 0 ? static_cast<double>(maxSide) /
                                            static_cast<double>(srcMaxSide)
                                      : 1.0;
  if (scale > 0.0 && scale < 1.0)
    cv::resize(image, preview, cv::Size(), scale, scale, cv::INTER_AREA);
  else
    preview = image;

  item.texture_id = CreateTextureFromMat0(preview);
  item.texture_w = preview.cols;
  item.texture_h = preview.rows;
  item.texture_loaded = item.texture_id != 0;
  item.texture_failed = !item.texture_loaded;
  item.texture_placeholder = false;
  item.status = item.texture_loaded ? "ready" : "texture_failed";
  ++m_manualTest.script_evidence_thumb_load_count_this_frame;
}

void ViewController::AddTorchTrainingImageFromPath(const std::string &imagePath,
                                                   const std::string &imageId,
                                                   const std::string &split,
                                                   const std::string &label,
                                                   const std::string &source) {
  if (imagePath.empty()) {
    m_manualTest.torch_training_image_status = "ADD_IMAGE_FAIL";
    m_manualTest.torch_training_image_reason = "image path is empty";
    return;
  }

  std::filesystem::path resolved = ResolveWorkspaceFile(imagePath);
  const std::string normalized = resolved.lexically_normal().string();

  for (std::size_t i = 0; i < m_manualTest.torch_training_images.size(); ++i) {
    TorchTrainingImageItem &existing = m_manualTest.torch_training_images[i];
    const std::string existingPath =
        ResolveWorkspaceFile(existing.image_path).lexically_normal().string();
    if (existingPath == normalized && existing.split == split) {
      existing.image_id = imageId.empty() ? existing.image_id : imageId;
      existing.case_id = m_manualTest.active_case_id;
      existing.target_id = m_manualTest.active_target_id;
      existing.label = label.empty() ? existing.label : label;
      existing.source = source.empty() ? existing.source : source;
      existing.status = std::filesystem::is_regular_file(resolved)
                            ? "ready"
                            : "image_unavailable";
      m_manualTest.selected_torch_training_image = static_cast<int>(i);
      m_manualTest.torch_training_image_status = "IMAGE_SET_UPDATED";
      m_manualTest.torch_training_image_reason =
          "updated existing " + split + " image: " + normalized;
      return;
    }
  }

  TorchTrainingImageItem item;
  item.image_id = imageId;
  item.image_path = normalized;
  item.case_id = m_manualTest.active_case_id;
  item.target_id = m_manualTest.active_target_id;
  item.split = split.empty() ? "train" : split;
  item.label = label.empty() ? "unlabeled" : label;
  item.source = source.empty() ? "manual" : source;
  item.status = std::filesystem::is_regular_file(resolved)
                    ? "ready"
                    : "image_unavailable";

  m_manualTest.torch_training_images.push_back(item);
  m_manualTest.selected_torch_training_image =
      static_cast<int>(m_manualTest.torch_training_images.size() - 1);
  m_manualTest.torch_training_image_status = "IMAGE_SET_ADDED";
  m_manualTest.torch_training_image_reason =
      "added " + item.split + " image: " + normalized;
}

static void
ClearTorchTrainingImageSetForEvidenceSyncLocal(ManualTestContext &context,
                                               const std::string &reason) {
  context.torch_training_images.clear();
  context.selected_torch_training_image = -1;
  context.torch_training_new_image_path.clear();
  context.torch_training_image_status = "IMAGE_SET_CLEARED";
  context.torch_training_image_reason = reason;
}

static void ApplyEvidenceAnnotationsToTorchTrainingItemLocal(
    TorchTrainingImageItem &item,
    const std::vector<CxEvidenceAnnotationBinding> &annotations) {
  item.annotation_shapes.clear();

  int imageW = 0;
  int imageH = 0;
  cv::Mat image = cv::imread(item.image_path, cv::IMREAD_UNCHANGED);
  if (!image.empty()) {
    imageW = image.cols;
    imageH = image.rows;
  }

  int shapeIndex = 0;
  for (const CxEvidenceAnnotationBinding &annotation : annotations) {
    if (!annotation.image_id.empty() && !item.image_id.empty() &&
        annotation.image_id != item.image_id) {
      continue;
    }

    std::vector<double> points = annotation.points_xy;
    if (annotation.normalized && imageW > 0 && imageH > 0) {
      for (std::size_t pointIndex = 1; pointIndex < points.size();
           pointIndex += 2) {
        points[pointIndex - 1] *= static_cast<double>(imageW);
        points[pointIndex] *= static_cast<double>(imageH);
      }
    }

    double x0 = annotation.x0;
    double y0 = annotation.y0;
    double x1 = annotation.x1;
    double y1 = annotation.y1;
    if (!points.empty()) {
      x0 = x1 = points[0];
      y0 = y1 = points[1];
      for (std::size_t pointIndex = 3; pointIndex < points.size();
           pointIndex += 2) {
        x0 = std::min(x0, points[pointIndex - 1]);
        x1 = std::max(x1, points[pointIndex - 1]);
        y0 = std::min(y0, points[pointIndex]);
        y1 = std::max(y1, points[pointIndex]);
      }
    } else if (annotation.normalized && imageW > 0 && imageH > 0) {
      x0 *= static_cast<double>(imageW);
      x1 *= static_cast<double>(imageW);
      y0 *= static_cast<double>(imageH);
      y1 *= static_cast<double>(imageH);
    }

    if (x1 < x0)
      std::swap(x0, x1);
    if (y1 < y0)
      std::swap(y0, y1);

    TorchTrainingAnnotationShapeSnapshot snap;
    snap.class_id = annotation.class_id;
    const bool polygon = points.size() >= 6;
    snap.stable_ref = item.image_id + (polygon ? "_polygon_" : "_bbox_") +
                      std::to_string(shapeIndex + 1);
    snap.tool_id = "TorchTask";
    snap.owner_type = "TorchDataset";
    snap.owner_ref = item.image_id;
    snap.owner_binding = annotation.owner_binding.empty()
                             ? "label_bbox"
                             : annotation.owner_binding;
    snap.semantic_role = annotation.semantic_role + "_class_" +
                         std::to_string(annotation.class_id);
    snap.shape_kind =
        annotation.shape_kind.empty() ? "RectShape" : annotation.shape_kind;
    if (polygon) {
      for (std::size_t pointIndex = 1; pointIndex < points.size();
           pointIndex += 2) {
        snap.center_x += points[pointIndex - 1];
        snap.center_y += points[pointIndex];
      }
      const double pointCount = static_cast<double>(points.size() / 2);
      snap.center_x /= pointCount;
      snap.center_y /= pointCount;
    } else {
      snap.center_x = (x0 + x1) * 0.5;
      snap.center_y = (y0 + y1) * 0.5;
      snap.radius_x = std::max(0.0, (x1 - x0) * 0.5);
      snap.radius_y = std::max(0.0, (y1 - y0) * 0.5);
    }
    snap.points_xy = polygon
                         ? std::move(points)
                         : std::vector<double>{x0, y0, x1, y0, x1, y1, x0, y1};
    snap.closed = polygon ? annotation.closed : true;
    snap.editable = true;
    snap.visible = true;
    snap.result_element = false;
    item.annotation_shapes.push_back(std::move(snap));
    ++shapeIndex;
  }

  item.annotation_shape_count = static_cast<int>(item.annotation_shapes.size());
  item.annotation_overlay_count = 0;
  item.annotation_status = item.annotation_shape_count > 0
                               ? "draft_pending_human_review"
                               : "unlabeled";
  item.annotation_reason = item.annotation_shape_count > 0
                               ? "polygon draft loaded from evidence dataset; "
                                 "human review not recorded"
                               : "no evidence annotation bound for image";
}

static std::unique_ptr<ShapeBase> CreateTorchTrainingShapeFromSnapshotLocal(
    const TorchTrainingAnnotationShapeSnapshot &snap) {
  std::vector<CxShapePoint> points;
  for (std::size_t i = 1; i < snap.points_xy.size(); i += 2)
    points.push_back({snap.points_xy[i - 1], snap.points_xy[i]});

  if (snap.shape_kind == "PointsShape") {
    auto shape = std::make_unique<PointsShape>();
    for (const auto &p : points) {
      gp_Pnt gp(p.x, p.y, 0.0);
      shape->addpoint(gp);
    }
    return shape;
  }

  if (snap.shape_kind == "LineShape" && points.size() >= 2) {
    auto shape = std::make_unique<LineShape>();
    shape->setline(static_cast<int>(std::lround(points[0].x)),
                   static_cast<int>(std::lround(points[0].y)),
                   static_cast<int>(std::lround(points[1].x)),
                   static_cast<int>(std::lround(points[1].y)));
    return shape;
  }

  if (snap.shape_kind == "LineGaugeShape" && points.size() >= 2) {
    return std::make_unique<LineGaugeShape>(
        points[0].x, points[0].y, points[1].x, points[1].y,
        snap.half_width > 0.0 ? snap.half_width : 20.0);
  }

  if (snap.shape_kind == "RectShape") {
    if (points.size() >= 4) {
      double minX = points[0].x;
      double minY = points[0].y;
      double maxX = points[0].x;
      double maxY = points[0].y;
      for (const auto &p : points) {
        minX = std::min(minX, p.x);
        minY = std::min(minY, p.y);
        maxX = std::max(maxX, p.x);
        maxY = std::max(maxY, p.y);
      }
      return std::make_unique<RectShape>(minX, minY, maxX, maxY);
    }
    if (snap.radius_x > 0.0 && snap.radius_y > 0.0) {
      return std::make_unique<RectShape>(
          snap.center_x - snap.radius_x, snap.center_y - snap.radius_y,
          snap.center_x + snap.radius_x, snap.center_y + snap.radius_y);
    }
  }

  if (snap.shape_kind == "CircleShape") {
    return std::make_unique<CircleShape>(snap.center_x, snap.center_y,
                                         snap.radius > 0.0 ? snap.radius : 20.0,
                                         snap.inner_radius);
  }

  if (snap.shape_kind == "EllipseShape") {
    return std::make_unique<EllipseShape>(
        snap.center_x, snap.center_y,
        snap.radius_x > 0.0 ? snap.radius_x : 30.0,
        snap.radius_y > 0.0 ? snap.radius_y : 20.0);
  }

  if (snap.shape_kind == "PolylineShape") {
    auto shape = std::make_unique<PolylineShape>();
    for (const auto &p : points)
      shape->addPoint(p.x, p.y);
    shape->close(snap.closed);
    return shape;
  }

  return nullptr;
}

static std::string NormalizeEvidenceImageSetKeyLocal(std::string key) {
  std::replace(key.begin(), key.end(), '\\', '/');
  const std::size_t slash = key.find_last_of('/');
  if (slash != std::string::npos)
    key = key.substr(slash + 1);
  const std::string suffix = ".cxsc";
  if (key.size() > suffix.size() &&
      key.compare(key.size() - suffix.size(), suffix.size(), suffix) == 0) {
    key.resize(key.size() - suffix.size());
  }
  std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return key;
}

static bool EvidenceSelectionMatchesReferenceSetLocal(
    const CxEvidenceSelectionSnapshot &sel, const cv::FileNode &set) {
  std::vector<std::string> selectionKeys = {
      sel.script_id, sel.script_path, sel.case_id,
      sel.source_evidence_script_path};
  for (std::string &key : selectionKeys)
    key = NormalizeEvidenceImageSetKeyLocal(key);

  std::string caseId;
  std::string scriptPath;
  set["case_id"] >> caseId;
  set["script"] >> scriptPath;
  const std::string normalizedCase =
      NormalizeEvidenceImageSetKeyLocal(caseId);
  const std::string normalizedScript =
      NormalizeEvidenceImageSetKeyLocal(scriptPath);

  for (const std::string &key : selectionKeys) {
    if (!key.empty() &&
        (key == normalizedCase || key == normalizedScript)) {
      return true;
    }
  }

  const cv::FileNode aliases = set["selection_aliases"];
  for (const auto &aliasNode : aliases) {
    std::string alias;
    aliasNode >> alias;
    alias = NormalizeEvidenceImageSetKeyLocal(alias);
    if (alias.empty())
      continue;
    if (std::find(selectionKeys.begin(), selectionKeys.end(), alias) !=
        selectionKeys.end()) {
      return true;
    }
  }
  return false;
}

static bool
IsEvidenceSelectionImageSetLocal(const CxEvidenceSelectionSnapshot &sel) {
  const std::filesystem::path manifestPath = ResolveWorkspaceFile(
      "cxparser/cxscript/module/torch/hd_reference/"
      "hd_reference_image_sets.json");
  cv::FileStorage storage(manifestPath.string(), cv::FileStorage::READ);
  if (!storage.isOpened())
    return false;

  std::string schema;
  storage["schema"] >> schema;
  if (schema != "cxvision.hd_reference_image_sets.v2")
    return false;

  const cv::FileNode sets = storage["sets"];
  for (const auto &set : sets) {
    if (EvidenceSelectionMatchesReferenceSetLocal(sel, set))
      return true;
  }
  return false;
}

struct EvidenceReferencePolicyLocal {
  bool matched = false;
  bool training_enabled = false;
  std::string display_name;
  std::string task;
  std::string case_track;
  std::string annotation_contract_id;
  std::string model_track_id;
  std::string evaluator_id;
  std::string binding_status;
  std::string label_semantics;
};

static EvidenceReferencePolicyLocal EvidenceReferencePolicyForSelectionLocal(
    const CxEvidenceSelectionSnapshot &sel) {
  EvidenceReferencePolicyLocal policy;
  const std::filesystem::path manifestPath = ResolveWorkspaceFile(
      "cxparser/cxscript/module/torch/hd_reference/"
      "hd_reference_image_sets.json");
  cv::FileStorage storage(manifestPath.string(), cv::FileStorage::READ);
  if (!storage.isOpened())
    return policy;

  std::string schema;
  storage["schema"] >> schema;
  if (schema != "cxvision.hd_reference_image_sets.v2")
    return policy;

  for (const auto &set : storage["sets"]) {
    if (!EvidenceSelectionMatchesReferenceSetLocal(sel, set))
      continue;
    int trainingEnabled = 0;
    set["training_enabled"] >> trainingEnabled;
    set["display_name"] >> policy.display_name;
    set["task"] >> policy.task;
    set["case_track"] >> policy.case_track;
    set["annotation_contract_id"] >> policy.annotation_contract_id;
    set["model_track_id"] >> policy.model_track_id;
    set["evaluator_id"] >> policy.evaluator_id;
    set["binding_status"] >> policy.binding_status;
    set["label_semantics"] >> policy.label_semantics;
    policy.matched = true;
    policy.training_enabled = trainingEnabled != 0;
    return policy;
  }
  return policy;
}

static bool EvidenceSelectionTrainingEnabledLocal(
    const CxEvidenceSelectionSnapshot &sel, bool &matched) {
  const EvidenceReferencePolicyLocal policy =
      EvidenceReferencePolicyForSelectionLocal(sel);
  matched = policy.matched;
  return policy.training_enabled;
}

void ViewController::CaptureCurrentTorchTrainingAnnotationState() {
  const int selected = m_manualTest.selected_torch_training_image;
  if (selected < 0 ||
    selected >= static_cast<int>(m_manualTest.torch_training_images.size())) {
  return;
}

TorchTrainingImageItem &item =
    m_manualTest.torch_training_images[static_cast<std::size_t>(selected)];
if (m_manualTest.image_file_path.empty() || item.image_path.empty())
  return;

const std::string currentImage =
    ResolveWorkspaceFile(m_manualTest.image_file_path)
        .lexically_normal()
        .string();
const std::string selectedImage =
    ResolveWorkspaceFile(item.image_path).lexically_normal().string();
if (currentImage != selectedImage)
  return;

std::vector<TorchTrainingAnnotationShapeSnapshot> capturedShapes;
const int capturedOverlayCount =
    static_cast<int>(m_annotationLayer.Elements().size());

for (const auto &element : m_annotationLayer.ShapeElements()) {
  if (!element.shape || element.runtime_bound)
    continue;

  CxShapeGeometrySnapshot geo;
  if (!element.shape->snapshot(geo))
    continue;

  TorchTrainingAnnotationShapeSnapshot snap;
  snap.stable_ref =
      element.stable_ref.empty() ? element.ref : element.stable_ref;
  snap.tool_id = element.tool_id;
  snap.owner_type = element.owner_type;
  snap.owner_ref = element.owner_ref;
  snap.owner_binding = element.owner_binding;
  snap.semantic_role = element.semantic_role;
  snap.class_id = m_manualTest.torch_training_annotation_class_id;
  const auto previous = std::find_if(
      item.annotation_shapes.begin(), item.annotation_shapes.end(),
      [&snap](const TorchTrainingAnnotationShapeSnapshot &candidate) {
        return candidate.stable_ref == snap.stable_ref;
      });
  if (previous != item.annotation_shapes.end())
    snap.class_id = previous->class_id;
  snap.shape_kind = CxShapeKindName(geo.kind);
  snap.center_x = geo.center.x;
  snap.center_y = geo.center.y;
  snap.radius = geo.radius;
  snap.inner_radius = geo.inner_radius;
  snap.radius_x = geo.radius_x;
  snap.radius_y = geo.radius_y;
  snap.angle = geo.angle;
  snap.half_width = geo.half_width;
  snap.closed = geo.closed;
  snap.editable = element.editable;
  snap.visible = element.visible;
  snap.result_element = element.result_element;

  for (const auto &p : geo.points) {
    snap.points_xy.push_back(p.x);
    snap.points_xy.push_back(p.y);
  }

  capturedShapes.push_back(std::move(snap));
}

auto sameShape = [](const TorchTrainingAnnotationShapeSnapshot &a,
                    const TorchTrainingAnnotationShapeSnapshot &b) -> bool {
  constexpr double kGeometryEpsilon = 1e-3;
  const auto sameDouble = [=](double lhs, double rhs) {
    return std::abs(lhs - rhs) <= kGeometryEpsilon;
  };
  const auto samePoints = [&sameDouble](const std::vector<double> &lhs,
                                        const std::vector<double> &rhs) {
    if (lhs.size() != rhs.size())
      return false;
    for (std::size_t i = 0; i < lhs.size(); ++i) {
      if (!sameDouble(lhs[i], rhs[i]))
        return false;
    }
    return true;
  };
  return a.class_id == b.class_id && a.stable_ref == b.stable_ref &&
         a.tool_id == b.tool_id && a.owner_type == b.owner_type &&
         a.owner_ref == b.owner_ref && a.owner_binding == b.owner_binding &&
         a.semantic_role == b.semantic_role && a.shape_kind == b.shape_kind &&
         sameDouble(a.center_x, b.center_x) &&
         sameDouble(a.center_y, b.center_y) && sameDouble(a.radius, b.radius) &&
         sameDouble(a.inner_radius, b.inner_radius) &&
         sameDouble(a.radius_x, b.radius_x) &&
         sameDouble(a.radius_y, b.radius_y) && sameDouble(a.angle, b.angle) &&
         sameDouble(a.half_width, b.half_width) && a.closed == b.closed &&
         a.editable == b.editable && a.visible == b.visible &&
         a.result_element == b.result_element &&
         samePoints(a.points_xy, b.points_xy);
};
bool changed = item.annotation_overlay_count != capturedOverlayCount ||
               item.annotation_shapes.size() != capturedShapes.size();
if (!changed) {
  for (std::size_t i = 0; i < capturedShapes.size(); ++i) {
    if (!sameShape(item.annotation_shapes[i], capturedShapes[i])) {
      changed = true;
      break;
    }
  }
}
if (!changed)
  return;

item.annotation_shapes = std::move(capturedShapes);
item.annotation_overlay_count = capturedOverlayCount;
item.annotation_shape_count = static_cast<int>(item.annotation_shapes.size());
item.annotation_status =
    item.annotation_shape_count > 0 ? "editing" : "unlabeled";
CXLOG_INFO("TorchTrainingImageSet", "training_image_annotation_captured",
           "captured",
           "index=" + std::to_string(selected) + " image_id=" + item.image_id +
               " image_path=" + item.image_path +
               " shapes=" + std::to_string(item.annotation_shape_count) +
               " overlays=" + std::to_string(item.annotation_overlay_count));
}

void ViewController::RestoreTorchTrainingAnnotationState(
    const TorchTrainingImageItem &item) {
  m_annotationLayer.Clear();
  m_annotationLayer.ClearShapeElements();

  int restored = 0;
  for (std::size_t i = 0; i < item.annotation_shapes.size(); ++i) {
    const auto &snap = item.annotation_shapes[i];
    std::unique_ptr<ShapeBase> shape =
        CreateTorchTrainingShapeFromSnapshotLocal(snap);
    if (!shape)
      continue;

    const std::string stableRef =
        snap.stable_ref.empty()
            ? ("torch_training_annotation_" + std::to_string(i + 1))
            : snap.stable_ref;
    m_annotationLayer.UpsertShape(
        stableRef,
        snap.owner_type.empty() ? "TorchTrainingImage" : snap.owner_type,
        snap.owner_ref.empty() ? item.image_id : snap.owner_ref,
        snap.owner_binding.empty() ? "manual_annotation" : snap.owner_binding,
        snap.semantic_role.empty() ? "annotation" : snap.semantic_role,
        snap.editable, snap.result_element, std::move(shape));
    if (CxShapeElement *restoredElement =
            m_annotationLayer.FindShapeByStableRef(stableRef)) {
      restoredElement->tool_id = snap.tool_id;
      restoredElement->visible = snap.visible;
    }
    ++restored;
  }

  m_annotationStatus = "torch dataset image annotation restored: shapes=" +
                       std::to_string(restored) + " label=" + item.label;
  CXLOG_INFO(
      "TorchTrainingImageSet", "training_image_annotation_restored", "restored",
      "image_id=" + item.image_id + " image_path=" + item.image_path +
          " shapes=" + std::to_string(restored) + " label=" + item.label);
}

bool ViewController::LoadTorchTrainingImageIntoAnnotationView(
    int itemIndex, std::string &reason) {
  reason.clear();
  if (itemIndex < 0 ||
      itemIndex >=
          static_cast<int>(m_manualTest.torch_training_images.size())) {
    reason = "torch training image index out of range";
    return false;
  }

  CaptureCurrentTorchTrainingAnnotationState();

  TorchTrainingImageItem &item =
      m_manualTest.torch_training_images[static_cast<std::size_t>(itemIndex)];
  if (!LoadImageIntoImageView(item.image_path, reason)) {
    CXLOG_ERROR("TorchTrainingImageSet", "training_image_load_image_view",
                "load_failed",
                "index=" + std::to_string(itemIndex) +
                    " image_id=" + item.image_id +
                    " image_path=" + item.image_path + " reason=" + reason);
    return false;
  }

  m_manualTest.selected_torch_training_image = itemIndex;
  RestoreTorchTrainingAnnotationState(item);

  m_imageToolEnabled = true;
  m_imageToolMode = ImageToolMode::PointerPan;
  CancelAnnotationCreate();
  m_annotationLayer.SetActiveToolIndex(-1);
  m_manualTest.torch_training_image_status = "ANNOTATION_READY";
  m_manualTest.torch_training_image_reason =
      "loaded dataset image into Image View and enabled annotation: " +
      item.image_path +
      " shapes=" + std::to_string(item.annotation_shape_count);
  CXLOG_INFO(
      "TorchTrainingImageSet", "training_image_load_image_view", "loaded",
      "index=" + std::to_string(itemIndex) + " image_id=" + item.image_id +
          " image_path=" + item.image_path + " label=" + item.label +
          " shapes=" + std::to_string(item.annotation_shape_count));
  return true;
}

static std::string NormalizeTorchTrainingSplitLocal(std::string split) {
  std::transform(split.begin(), split.end(), split.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  if (split.empty())
    return "train";
  if (split == "validation" || split == "validate" || split == "valid")
    return "val";
  if (split == "holdout")
    return "test";
  return split;
}

static std::filesystem::path ResolveGeometryAugAssetRefLocal(
    const std::filesystem::path &base, const std::string &ref) {
  const std::filesystem::path path(ref);
  return (path.is_absolute() ? path : base / path).lexically_normal();
}

static std::string GeometryAugmentationSelectionSourceLocal(std::string review) {
  const std::size_t marker = review.find(" / ");
  if (marker != std::string::npos)
    review.resize(marker);
  return review;
}

static double FileNodeDoubleLocal(const cv::FileNode &node, const char *key,
                                  double fallback = 0.0) {
  const cv::FileNode value = node[key];
  return value.empty() ? fallback : static_cast<double>(value);
}

static bool ReadPointPairsLocal(const cv::FileNode &node,
                                std::vector<double> &points) {
  points.clear();
  if (!node.isSeq())
    return false;
  for (const cv::FileNode &point : node) {
    if (!point.isSeq() || point.size() < 2)
      return false;
    points.push_back(static_cast<double>(point[0]));
    points.push_back(static_cast<double>(point[1]));
  }
  return points.size() >= 4;
}

static TorchTrainingAnnotationShapeSnapshot BuildGeometryAugBBoxSnapshotLocal(
    const std::string &imageId, const std::string &geometryType,
    const cv::FileNode &positionRoot, int classId) {
  TorchTrainingAnnotationShapeSnapshot snap;
  snap.class_id = classId;
  snap.stable_ref = imageId + "_bbox_1";
  snap.tool_id = "TorchTask";
  snap.owner_type = "GeometryAugmentationDataset";
  snap.owner_ref = imageId;
  snap.owner_binding = "typed_label_bbox";
  snap.semantic_role = "typed_label_" + geometryType;
  snap.shape_kind = "RectShape";
  const cv::FileNode bbox = positionRoot["bbox_xywh"];
  if (bbox.isSeq() && bbox.size() >= 4) {
    const double x = static_cast<double>(bbox[0]);
    const double y = static_cast<double>(bbox[1]);
    const double w = static_cast<double>(bbox[2]);
    const double h = static_cast<double>(bbox[3]);
    snap.center_x = x + w * 0.5;
    snap.center_y = y + h * 0.5;
    snap.radius_x = std::max(0.0, w * 0.5);
    snap.radius_y = std::max(0.0, h * 0.5);
    snap.points_xy = {x, y, x + w, y, x + w, y + h, x, y + h};
  }
  snap.closed = true;
  snap.editable = true;
  snap.visible = true;
  snap.result_element = false;
  return snap;
}

static bool BuildGeometryAugShapeSnapshotLocal(
    const std::string &imageId, const std::string &geometryType,
    const cv::FileNode &factsRoot,
    TorchTrainingAnnotationShapeSnapshot &snap) {
  snap.class_id = 0;
  snap.stable_ref = imageId + "_geometry_1";
  snap.tool_id = "TorchTask";
  snap.owner_type = "GeometryAugmentationDataset";
  snap.owner_ref = imageId;
  snap.owner_binding = "typed_label_geometry";
  snap.semantic_role = "typed_label_" + geometryType;
  snap.editable = true;
  snap.visible = true;
  snap.result_element = false;

  std::vector<double> points;
  const cv::FileNode instances = factsRoot["instances"];
  const cv::FileNode instance = instances.isSeq() && !instances.empty()
                                    ? *instances.begin()
                                    : cv::FileNode();
  if (!instance.empty()) {
    if (ReadPointPairsLocal(instance["vertices_xy"], points)) {
      snap.shape_kind = "PolylineShape";
      snap.points_xy = std::move(points);
      int closed = 0;
      instance["closed"] >> closed;
      snap.closed = closed != 0;
      return true;
    }

    const cv::FileNode center = instance["center_xy"];
    if (center.isSeq() && center.size() >= 2) {
      snap.center_x = static_cast<double>(center[0]);
      snap.center_y = static_cast<double>(center[1]);
      if (geometryType == "circle") {
        snap.shape_kind = "CircleShape";
        snap.radius = FileNodeDoubleLocal(instance, "radius_px", 20.0);
        snap.closed = true;
        return true;
      }
      if (geometryType == "ellipse") {
        snap.shape_kind = "EllipseShape";
        snap.radius_x = FileNodeDoubleLocal(instance, "radius_x_px", 30.0);
        snap.radius_y = FileNodeDoubleLocal(instance, "radius_y_px", 20.0);
        snap.angle = FileNodeDoubleLocal(instance, "rotation_deg", 0.0);
        snap.closed = true;
        return true;
      }
      if (geometryType == "rectangle") {
        const double halfW = FileNodeDoubleLocal(instance, "width_px", 0.0) * 0.5;
        const double halfH = FileNodeDoubleLocal(instance, "height_px", 0.0) * 0.5;
        const double angle = FileNodeDoubleLocal(instance, "rotation_deg", 0.0) *
                             3.14159265358979323846 / 180.0;
        if (halfW > 0.0 && halfH > 0.0) {
          snap.shape_kind = "PolylineShape";
          snap.closed = true;
          const double c = std::cos(angle);
          const double s = std::sin(angle);
          const double corners[4][2] = {{-halfW, -halfH}, {halfW, -halfH},
                                        {halfW, halfH}, {-halfW, halfH}};
          for (const auto &corner : corners) {
            snap.points_xy.push_back(snap.center_x + corner[0] * c - corner[1] * s);
            snap.points_xy.push_back(snap.center_y + corner[0] * s + corner[1] * c);
          }
          return true;
        }
      }
    }
  }

  if (ReadPointPairsLocal(factsRoot["control_points_xy"], points)) {
    snap.shape_kind = "PolylineShape";
    snap.points_xy = std::move(points);
    snap.closed = false;
    return true;
  }
  if (ReadPointPairsLocal(factsRoot["endpoints_xy"], points)) {
    snap.shape_kind = "LineShape";
    snap.points_xy = std::move(points);
    snap.closed = false;
    return true;
  }

  return false;
}

int ViewController::AddGeometryAugmentationDatasetForCurrentSelection() {
  const CxEvidenceSelectionSnapshot &sel =
      m_manualTest.current_evidence_selection;
  if (!sel.valid)
    return 0;

  const std::filesystem::path runRoot = ResolveCxVisionRunPath("cxscript_runs");
  const std::filesystem::path scanRoot = runRoot / "geometry_augmentation";
  std::error_code ec;
  if (!std::filesystem::is_directory(scanRoot, ec))
    return 0;

  const std::string selectedReview = !sel.review_item.empty()
                                         ? sel.review_item
                                         : (!sel.script_id.empty() ? sel.script_id
                                                                  : sel.case_id);
  const std::string selectedSource =
      GeometryAugmentationSelectionSourceLocal(selectedReview);

  int datasetCount = 0;
  int added = 0;
  int skipped = 0;
  int trainCount = 0;
  int valCount = 0;
  int testCount = 0;
  std::set<std::string> sampleKeys;
  std::vector<std::filesystem::path> manifests;

  std::filesystem::recursive_directory_iterator iterator(
      scanRoot, std::filesystem::directory_options::skip_permission_denied, ec);
  const std::filesystem::recursive_directory_iterator end;
  while (!ec && iterator != end) {
    const std::filesystem::directory_entry entry = *iterator;
    std::error_code entryError;
    if (entry.is_symlink(entryError)) {
      if (entry.is_directory(entryError))
        iterator.disable_recursion_pending();
      iterator.increment(ec);
      continue;
    }
    if (entry.is_regular_file(entryError) &&
        entry.path().filename() == "dataset_manifest.json") {
      manifests.push_back(entry.path());
    }
    iterator.increment(ec);
  }
  std::sort(manifests.begin(), manifests.end());

  for (const std::filesystem::path &datasetManifestPath : manifests) {
    if (!IsAssetCasePathWithinRootLocal(runRoot, datasetManifestPath)) {
      ++skipped;
      continue;
    }

    cv::FileStorage dataset;
    try {
      if (!dataset.open(datasetManifestPath.string(),
                        cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON)) {
        ++skipped;
        continue;
      }
    } catch (const cv::Exception &) {
      ++skipped;
      continue;
    }
    if (FileNodeStringLocal(dataset.root(), "schema") !=
        "cxvision.geometry_augmentation_dataset.v1" ||
        !dataset["samples"].isSeq()) {
      ++skipped;
      continue;
    }
    ++datasetCount;

    const std::filesystem::path datasetRoot = datasetManifestPath.parent_path();
    for (const cv::FileNode &sample : dataset["samples"]) {
      const std::string casePathRef = FileNodeStringLocal(sample, "case_path");
      if (casePathRef.empty()) {
        ++skipped;
        continue;
      }
      const std::filesystem::path caseDirectory =
          ResolveGeometryAugAssetRefLocal(datasetRoot, casePathRef);
      const std::filesystem::path caseManifestPath =
          caseDirectory / "case_manifest.json";
      std::error_code caseError;
      if (!IsAssetCasePathWithinRootLocal(runRoot, caseDirectory) ||
          std::filesystem::is_symlink(caseDirectory, caseError) ||
          !std::filesystem::is_regular_file(caseManifestPath, caseError) ||
          std::filesystem::is_symlink(caseManifestPath, caseError)) {
        ++skipped;
        continue;
      }

      cv::FileStorage caseManifest;
      try {
        if (!caseManifest.open(caseManifestPath.string(),
                               cv::FileStorage::READ |
                                   cv::FileStorage::FORMAT_JSON)) {
          ++skipped;
          continue;
        }
      } catch (const cv::Exception &) {
        ++skipped;
        continue;
      }
      if (FileNodeStringLocal(caseManifest.root(), "schema") !=
          "cxvision.evidence_case.v1") {
        ++skipped;
        continue;
      }

      const std::string reviewItem =
          FileNodeStringLocal(caseManifest.root(), "review_item");
      const std::string sourceReview =
          FileNodeStringLocal(caseManifest.root(), "source_review_item");
      const bool sameVisibleCase =
          !selectedReview.empty() && reviewItem == selectedReview;
      const bool sameSourceCase =
          !selectedSource.empty() &&
          (sourceReview == selectedSource ||
           reviewItem.rfind(selectedSource + " / ", 0) == 0);
      if (!sameVisibleCase && !sameSourceCase) {
        ++skipped;
        continue;
      }

      const std::string runId = FileNodeStringLocal(caseManifest.root(), "run_id");
      const std::string imageId =
          FileNodeStringLocal(caseManifest.root(), "internal_case_id");
      const std::string geometryType =
          FileNodeStringLocal(caseManifest.root(), "geometry_type");
      const std::string sourceImageRef =
          FileNodeStringLocal(caseManifest.root(), "source_image");
      const std::string typedLabelRef =
          FileNodeStringLocal(caseManifest.root(), "typed_label");
      const std::string factsRef =
          FileNodeStringLocal(caseManifest.root(), "geometry_facts_ref");
      const std::string positionRef =
          FileNodeStringLocal(caseManifest.root(), "position_annotation_ref");
      std::string split = NormalizeTorchTrainingSplitLocal(
          FileNodeStringLocal(caseManifest.root(), "split"));

      const std::filesystem::path sourceImage =
          ResolveGeometryAugAssetRefLocal(caseDirectory, sourceImageRef);
      const std::filesystem::path typedLabel =
          ResolveGeometryAugAssetRefLocal(caseDirectory, typedLabelRef);
      const std::filesystem::path factsPath =
          ResolveGeometryAugAssetRefLocal(caseDirectory, factsRef);
      const std::filesystem::path positionPath =
          ResolveGeometryAugAssetRefLocal(caseDirectory, positionRef);
      const auto validCaseAsset = [&](const std::filesystem::path &path) {
        std::error_code assetError;
        return IsAssetCasePathWithinRootLocal(runRoot, path) &&
               IsAssetCasePathWithinRootLocal(caseDirectory, path) &&
               std::filesystem::is_regular_file(path, assetError) &&
               !std::filesystem::is_symlink(path, assetError);
      };
      if (imageId.empty() || !validCaseAsset(sourceImage) ||
          !validCaseAsset(typedLabel) || !validCaseAsset(factsPath) ||
          !validCaseAsset(positionPath)) {
        ++skipped;
        continue;
      }

      const std::string sampleKey =
          runId + "|" + imageId + "|" + sourceImage.lexically_normal().string();
      if (!sampleKeys.insert(sampleKey).second) {
        ++skipped;
        continue;
      }

      AddTorchTrainingImageFromPath(
          sourceImage.string(), imageId, split,
          geometryType.empty() ? "pending" : geometryType,
          runId.empty() ? "geometry_augmentation_dataset"
                        : "geometry_augmentation_dataset:" + runId);

      const int selected = m_manualTest.selected_torch_training_image;
      if (selected >= 0 &&
          selected < static_cast<int>(m_manualTest.torch_training_images.size())) {
        TorchTrainingImageItem &item =
            m_manualTest.torch_training_images[static_cast<std::size_t>(selected)];
        item.case_id = reviewItem.empty() ? imageId : reviewItem;
        item.target_id = geometryType;
        item.annotation_shapes.clear();

        cv::FileStorage facts;
        cv::FileStorage position;
        bool factsOpened = false;
        bool positionOpened = false;
        try {
          factsOpened = facts.open(factsPath.string(),
                                   cv::FileStorage::READ |
                                       cv::FileStorage::FORMAT_JSON);
          positionOpened = position.open(positionPath.string(),
                                         cv::FileStorage::READ |
                                             cv::FileStorage::FORMAT_JSON);
        } catch (const cv::Exception &) {
          factsOpened = false;
          positionOpened = false;
        }

        TorchTrainingAnnotationShapeSnapshot snap;
        if (factsOpened &&
            BuildGeometryAugShapeSnapshotLocal(imageId, geometryType,
                                               facts.root(), snap)) {
          item.annotation_shapes.push_back(std::move(snap));
        } else if (positionOpened) {
          item.annotation_shapes.push_back(BuildGeometryAugBBoxSnapshotLocal(
              imageId, geometryType, position.root(), 0));
        }
        item.annotation_shape_count =
            static_cast<int>(item.annotation_shapes.size());
        item.annotation_overlay_count = 1;
        item.annotation_status =
            item.annotation_shape_count > 0 ? "draft_pending_human_review"
                                            : "unlabeled";
        item.annotation_reason =
            "typed_label=" + typedLabel.string() +
            "; human review not recorded; training_enabled=0";
        if (factsOpened) {
          const std::string sourceHash =
              FileNodeStringLocal(facts.root(), "source_input_sha256");
          if (!sourceHash.empty())
            item.source = "source_hash:" + sourceHash;
        }
      }

      if (split == "train")
        ++trainCount;
      else if (split == "val")
        ++valCount;
      else if (split == "test")
        ++testCount;
      ++added;
    }
  }

  if (added > 0) {
    m_manualTest.torch_training_image_status =
        "GEOMETRY_AUGMENTATION_DATASET_BOUND";
    m_manualTest.torch_training_image_reason =
        "asset-driven geometry augmentation image set for " + selectedReview +
        " datasets=" + std::to_string(datasetCount) +
        " images=" + std::to_string(added) +
        " train=" + std::to_string(trainCount) +
        " val=" + std::to_string(valCount) +
        " test=" + std::to_string(testCount) +
        " skipped=" + std::to_string(skipped) +
        " training_enabled=0";
  }
  return added;
}

void ViewController::SyncTorchTrainingImageSetFromEvidenceSelection() {
  CxEvidenceSelectionSnapshot sel = m_manualTest.current_evidence_selection;
  if (!sel.valid)
    return;

  if (sel.dataset_images.empty()) {
    for (const ScriptEvidenceGroup &group :
         m_manualTest.script_evidence_groups) {
      for (const ScriptEvidenceThumb &thumb : group.thumbs) {
        if (thumb.dataset_images.empty())
          continue;

        const bool sameCase =
            !sel.case_id.empty() && thumb.case_id == sel.case_id;
        const bool sameScript =
            !sel.script_id.empty() && thumb.script_id == sel.script_id;
        const bool sameImageAndTool =
            !sel.image_id.empty() && !sel.tool.empty() &&
            thumb.image_id == sel.image_id &&
            NormalizeEvidenceToolTypeLocal(thumb.tool) ==
                NormalizeEvidenceToolTypeLocal(sel.tool);

        if (!sameCase && !sameScript && !sameImageAndTool)
          continue;

        sel.dataset_images = thumb.dataset_images;
        sel.annotations = thumb.annotations;
        CXLOG_INFO(
            "TorchTrainingImageSet",
            "evidence_dataset_resolved_from_loaded_thumb", "resolved",
            "case_id=" + sel.case_id + " script_id=" + sel.script_id +
                " matched_case=" + thumb.case_id +
                " matched_script=" + thumb.script_id +
                " dataset_images=" + std::to_string(sel.dataset_images.size()) +
                " annotations=" + std::to_string(sel.annotations.size()));
        break;
      }
      if (!sel.dataset_images.empty())
        break;
    }
  }

  ClearTorchTrainingImageSetForEvidenceSyncLocal(
      m_manualTest, "rebuild image set from evidence case: " +
                        (sel.case_id.empty() ? sel.script_id : sel.case_id));


  const int geometryAugmentationCount =
      AddGeometryAugmentationDatasetForCurrentSelection();
  if (geometryAugmentationCount > 0) {
    std::string imageLoadReason;
    m_manualTest.selected_torch_training_image = -1;
    const bool imageLoaded =
        LoadTorchTrainingImageIntoAnnotationView(0, imageLoadReason);
    const std::string manifestReason = m_manualTest.torch_training_image_reason;
    m_manualTest.torch_training_image_status =
        imageLoaded ? "GEOMETRY_AUGMENTATION_DATASET_BOUND"
                    : "GEOMETRY_AUGMENTATION_IMAGE_LOAD_FAIL";
    m_manualTest.torch_training_image_reason =
        manifestReason +
        " image_view=" + (imageLoaded ? "loaded" : "not_loaded") +
        (imageLoadReason.empty() ? "" : " reason=" + imageLoadReason);
    CXLOG_INFO("TorchTrainingImageSet", "geometry_augmentation_sync_complete",
               m_manualTest.torch_training_image_status,
               m_manualTest.torch_training_image_reason);
    return;
  }

  if (!sel.dataset_images.empty()) {
    int added = 0;
    int preferredImageIndex = -1;
    for (const CxEvidenceDatasetImageBinding &binding : sel.dataset_images) {
      std::string imagePath = binding.image_path;
      if (imagePath.empty() && !binding.image_id.empty())
        imagePath = ResolveImagePathFromManifest(binding.image_id);
      if (imagePath.empty() && !binding.image_id.empty())
        imagePath = ResolveEvidenceImagePathFromContextLocal(m_manualTest,
                                                             binding.image_id);
      if (imagePath.empty())
        continue;

      AddTorchTrainingImageFromPath(
          imagePath, binding.image_id, binding.split, binding.label,
          binding.source.empty() ? "evidence_dataset" : binding.source);

      const int selected = m_manualTest.selected_torch_training_image;
      if (selected >= 0 &&
          selected <
              static_cast<int>(m_manualTest.torch_training_images.size())) {
        TorchTrainingImageItem &item =
            m_manualTest
                .torch_training_images[static_cast<std::size_t>(selected)];
        item.case_id = sel.case_id;
        item.target_id = sel.target_id;
        ApplyEvidenceAnnotationsToTorchTrainingItemLocal(item, sel.annotations);
        const std::string itemPath =
            ResolveWorkspaceFile(item.image_path).lexically_normal().string();
        const std::string evidencePath =
            ResolveWorkspaceFile(sel.image_path).lexically_normal().string();
        if ((!sel.image_id.empty() && item.image_id == sel.image_id) ||
            (!evidencePath.empty() && itemPath == evidencePath)) {
          preferredImageIndex = selected;
        }
      }
      ++added;
    }

    const int referenceSetCount = AddHDReferenceImageSetForCurrentSelection();
    added += referenceSetCount;

    int imageToLoad = -1;
    if (preferredImageIndex >= 0)
      imageToLoad = preferredImageIndex;
    else if (!m_manualTest.torch_training_images.empty())
      imageToLoad = 0;

    std::string imageLoadReason;
    bool imageLoaded = false;
    if (imageToLoad >= 0) {
      m_manualTest.selected_torch_training_image = -1;
      imageLoaded = LoadTorchTrainingImageIntoAnnotationView(imageToLoad,
                                                             imageLoadReason);
    }

    m_manualTest.torch_training_image_status =
        added <= 0 ? "EVIDENCE_DATASET_EMPTY"
                   : (imageLoaded ? "EVIDENCE_DATASET_BOUND"
                                  : "EVIDENCE_DATASET_IMAGE_LOAD_FAIL");
    m_manualTest.torch_training_image_reason =
        "synced evidence dataset images for " + sel.case_id +
        " count=" + std::to_string(added) +
        " annotations=" + std::to_string(sel.annotations.size()) +
        " image_view=" + (imageLoaded ? "loaded" : "not_loaded") +
        (imageLoadReason.empty() ? "" : " reason=" + imageLoadReason);
    CXLOG_INFO("TorchTrainingImageSet", "evidence_dataset_sync_complete",
               m_manualTest.torch_training_image_status,
               m_manualTest.torch_training_image_reason);
    return;
  }

  const int referenceSetCount = AddHDReferenceImageSetForCurrentSelection();
  if (referenceSetCount > 0) {
    const std::string manifestReason = m_manualTest.torch_training_image_reason;
    std::string imageLoadReason;
    m_manualTest.selected_torch_training_image = -1;
    const bool imageLoaded =
        LoadTorchTrainingImageIntoAnnotationView(0, imageLoadReason);
    m_manualTest.torch_training_image_status =
        imageLoaded ? "HD_REFERENCE_IMAGE_SET_BOUND"
                    : "HD_REFERENCE_IMAGE_SET_IMAGE_LOAD_FAIL";
    m_manualTest.torch_training_image_reason =
        manifestReason +
        " image_view=" + (imageLoaded ? "loaded" : "not_loaded") +
        (imageLoadReason.empty() ? "" : " reason=" + imageLoadReason);
    CXLOG_INFO("TorchTrainingImageSet", "hd_reference_manifest_sync_complete",
               m_manualTest.torch_training_image_status,
               m_manualTest.torch_training_image_reason);
    return;
  }

  std::string imagePath = sel.image_path;
  if (imagePath.empty() && !sel.image_id.empty())
    imagePath = ResolveImagePathFromManifest(sel.image_id);
  if (imagePath.empty() && !sel.image_id.empty())
    imagePath =
        ResolveEvidenceImagePathFromContextLocal(m_manualTest, sel.image_id);
  if (imagePath.empty()) {
    if (m_manualTest.torch_training_image_status.empty()) {
      m_manualTest.torch_training_image_status = "EVIDENCE_IMAGE_UNRESOLVED";
      m_manualTest.torch_training_image_reason =
          "no evidence image or matching reference manifest set for " +
          (sel.case_id.empty() ? sel.script_id : sel.case_id);
    }
    return;
  }

  std::string label = "unlabeled";
  std::string key = sel.status + " " + sel.reason + " " +
                    sel.parameter_summary + " " + sel.tool + " " +
                    sel.script_id;
  std::transform(key.begin(), key.end(), key.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  if (key.find("anomaly") != std::string::npos ||
      key.find("defect") != std::string::npos ||
      key.find("fail") != std::string::npos ||
      key.find("error") != std::string::npos) {
    label = "anomaly";
  } else if (key.find("good") != std::string::npos ||
             key.find("ok") != std::string::npos ||
             key.find("success") != std::string::npos ||
             key.find("pass") != std::string::npos) {
    label = "good";
  }

  AddTorchTrainingImageFromPath(imagePath, sel.image_id, "train", label,
                                "evidence");
  m_manualTest.torch_training_image_reason =
      "synced from evidence case: " + sel.case_id;
  CXLOG_INFO("TorchTrainingImageSet",
             "evidence_dataset_sync_fallback_single_image",
             m_manualTest.torch_training_image_status,
             "case_id=" + sel.case_id + " script_id=" + sel.script_id +
                 " dataset_images=0 annotations=0 reason=" +
                 m_manualTest.torch_training_image_reason);
}

static ImU32 TorchDatasetLabelColorLocal(const std::string &label) {
  if (label == "good")
    return IM_COL32(45, 190, 95, 245);
  if (label == "anomaly")
    return IM_COL32(235, 85, 75, 245);
  if (label == "pending")
    return IM_COL32(235, 180, 55, 245);
  return IM_COL32(95, 135, 180, 245);
}

int ViewController::AddHDReferenceImageSetForCurrentSelection() {
  const CxEvidenceSelectionSnapshot &sel =
      m_manualTest.current_evidence_selection;

  const std::filesystem::path manifestPath =
      ResolveWorkspaceFile("cxparser/cxscript/module/torch/hd_reference/"
                           "hd_reference_image_sets.json");
  cv::FileStorage storage(manifestPath.string(), cv::FileStorage::READ);
  if (!storage.isOpened()) {
    m_manualTest.torch_training_image_status =
        "HD_REFERENCE_MANIFEST_LOAD_FAIL";
    m_manualTest.torch_training_image_reason =
        "cannot open reference image-set manifest: " + manifestPath.string();
    return 0;
  }

  std::string schema;
  std::string semanticStatus;
  storage["schema"] >> schema;
  storage["semantic_status"] >> semanticStatus;
  if (schema != "cxvision.hd_reference_image_sets.v2") {
    m_manualTest.torch_training_image_status =
        "HD_REFERENCE_MANIFEST_SCHEMA_FAIL";
    m_manualTest.torch_training_image_reason =
        "unsupported reference image-set schema: " + schema;
    return 0;
  }

  auto isImageFile = [](const std::filesystem::path &path) -> bool {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
      return static_cast<char>(std::tolower(ch));
    });
    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
           ext == ".tif" || ext == ".tiff";
  };

  const cv::FileNode sets = storage["sets"];
  for (const auto &set : sets) {
    std::string caseId;
    std::string scriptPath;
    std::string task;
    std::string project;
    std::string bindingStatus;
    int trainingEnabled = 0;
    set["case_id"] >> caseId;
    set["script"] >> scriptPath;
    set["task"] >> task;
    set["project"] >> project;
    set["binding_status"] >> bindingStatus;
    set["training_enabled"] >> trainingEnabled;

    if (!EvidenceSelectionMatchesReferenceSetLocal(sel, set))
      continue;

    int added = 0;
    const cv::FileNode sources = set["image_sources"];
    for (const auto &source : sources) {
      std::string directory;
      std::string split;
      std::string label;
      std::string sourceRole;
      int maxCount = 0;
      source["path"] >> directory;
      source["split"] >> split;
      source["label"] >> label;
      source["source_role"] >> sourceRole;
      source["max_count"] >> maxCount;
      if (directory.empty())
        continue;
      if (split.empty())
        split = "train";
      if (label.empty())
        label = "unlabeled";

      std::error_code ec;
      const std::filesystem::path resolvedDirectory =
          ResolveWorkspaceFile(directory).lexically_normal();
      if (!std::filesystem::is_directory(resolvedDirectory, ec))
        continue;

      std::vector<std::filesystem::path> files;
      for (std::filesystem::directory_iterator it(resolvedDirectory, ec), end;
           !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && isImageFile(it->path()))
          files.push_back(it->path());
      }
      std::sort(files.begin(), files.end());

      int sourceCount = 0;
      for (const auto &path : files) {
        if (maxCount > 0 && sourceCount >= maxCount)
          break;
        AddTorchTrainingImageFromPath(
            path.string(), path.stem().string(), split, label,
            sourceRole.empty() ? "hd_reference_manifest"
                               : "hd_reference_manifest:" + sourceRole);
        ++sourceCount;
        ++added;
      }
    }

    m_manualTest.torch_training_image_status =
        added > 0 ? "HD_REFERENCE_IMAGE_SET_LOADED"
                  : "HD_REFERENCE_IMAGE_SET_EMPTY";
    m_manualTest.torch_training_image_reason =
        "manifest=" + manifestPath.string() + " case=" + caseId +
        " task=" + task + " project=" + project + " binding=" + bindingStatus +
        " semantic_status=" + semanticStatus +
        " training_enabled=" + (trainingEnabled != 0 ? "true" : "false") +
        " count=" + std::to_string(added);
    return added;
  }

  return 0;
}

void ViewController::DrawTorchTrainingImageRail(const char *split,
                                                const char *label) {
  ImGui::Text("%s", label);
  ImGui::BeginChild((std::string("torch_dataset_rail_") + split).c_str(),
                    ImVec2(-1, 112.0f), true,
                    ImGuiWindowFlags_HorizontalScrollbar);

  ImGui::TextUnformatted("<");
  ImGui::SameLine();

  bool any = false;
  const ImVec2 thumbSize(76.0f, 76.0f);
  for (std::size_t i = 0; i < m_manualTest.torch_training_images.size(); ++i) {
    TorchTrainingImageItem &item = m_manualTest.torch_training_images[i];
    if (item.split != split)
      continue;
    any = true;
    EnsureTorchTrainingImageTexture(item);

    ImGui::PushID(static_cast<int>(i));
    const bool selected =
        m_manualTest.selected_torch_training_image == static_cast<int>(i);
    if (selected)
      ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(30, 140, 210, 255));

    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    bool clicked = false;
    if (item.texture_id != 0) {
      clicked =
          ImGui::ImageButton("torch_dataset_thumb",
                             static_cast<ImU64>(item.texture_id), thumbSize);
    } else {
      clicked = ImGui::Button("NO IMG", thumbSize);
    }
    const bool itemHovered = ImGui::IsItemHovered();

    ImDrawList *draw = ImGui::GetWindowDrawList();
    const ImVec2 p1(p0.x + thumbSize.x, p0.y + thumbSize.y);
    draw->AddRect(p0, p1,
                  selected ? IM_COL32(0, 190, 255, 255)
                           : IM_COL32(0, 120, 180, 180),
                  6.0f, 0, selected ? 3.0f : 1.5f);
    const std::string badge = item.label.empty() ? "unlabeled" : item.label;
    const ImVec2 badgeMin(p0.x + 3.0f, p1.y - 20.0f);
    const ImVec2 badgeMax(p1.x - 3.0f, p1.y - 3.0f);
    draw->AddRectFilled(badgeMin, badgeMax, TorchDatasetLabelColorLocal(badge),
                        4.0f);
    draw->AddText(ImVec2(badgeMin.x + 4.0f, badgeMin.y + 2.0f),
                  IM_COL32(255, 255, 255, 255), badge.c_str());
    if (item.annotation_shape_count > 0) {
      const std::string shapeBadge =
          "S" + std::to_string(item.annotation_shape_count);
      draw->AddRectFilled(ImVec2(p1.x - 28.0f, p0.y + 3.0f),
                          ImVec2(p1.x - 3.0f, p0.y + 19.0f),
                          IM_COL32(80, 170, 255, 220), 4.0f);
      draw->AddText(ImVec2(p1.x - 24.0f, p0.y + 4.0f),
                    IM_COL32(255, 255, 255, 255), shapeBadge.c_str());
    }

    if (clicked) {
      CXLOG_INFO("TorchTrainingImageSet", "training_thumb_click", "ui_event",
                 "index=" + std::to_string(i) + " split=" + item.split +
                     " label=" + item.label + " image_id=" + item.image_id +
                     " image_path=" + item.image_path);
      std::string reason;
      if (!LoadTorchTrainingImageIntoAnnotationView(static_cast<int>(i),
                                                    reason)) {
        m_manualTest.debug_status = "TORCH_DATASET_IMAGE_LOAD_FAIL";
        m_manualTest.debug_reason = reason;
      } else {
        m_manualTest.debug_status = "TORCH_DATASET_IMAGE_LOADED";
        m_manualTest.debug_reason = item.image_path;
      }
    }
    if (itemHovered) {
      ImGui::SetTooltip(
          "%s\nsplit=%s label=%s annotation=%s shapes=%d\ncase=%s\nsource=%s",
          item.image_path.c_str(), item.split.c_str(), item.label.c_str(),
          item.annotation_status.c_str(), item.annotation_shape_count,
          item.case_id.c_str(), item.source.c_str());
    }

    if (selected)
      ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PopID();
  }

  if (!any) {
    ImGui::TextDisabled("No %s images yet.", split);
    ImGui::SameLine();
  }
  ImGui::TextUnformatted(">");
  ImGui::EndChild();
}

static void DrawControlledCaseConfigurationAssetLocal(
    const char *label, const std::string &reference) {
  if (reference.empty()) {
    ImGui::TextDisabled("%s: ASSET_MISSING", label);
    return;
  }
  ImGui::TextWrapped("%s: %s", label, reference.c_str());
  const std::string treeId = std::string(label) + "##controlled_case_config";
  if (!ImGui::TreeNode(treeId.c_str()))
    return;
  std::string content;
  if (!ReadTextFile(reference, content)) {
    ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.25f, 1.0f),
                       "CONFIG_ASSET_READ_FAIL");
  } else {
    constexpr std::size_t kMaximumVisibleBytes = 16384;
    const bool truncated = content.size() > kMaximumVisibleBytes;
    if (truncated)
      content.resize(kMaximumVisibleBytes);
    ImGui::BeginChild((treeId + "_content").c_str(), ImVec2(-1.0f, 180.0f),
                      ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_HorizontalScrollbar);
    ImGui::TextUnformatted(content.c_str());
    ImGui::EndChild();
    if (truncated)
      ImGui::TextDisabled("Display truncated at %zu bytes; the path above is the authoritative asset.",
                          kMaximumVisibleBytes);
  }
  ImGui::TreePop();
}

void ViewController::DrawEvidenceWorkflowPanel() {
  const CxEvidenceSelectionSnapshot &selection =
      m_manualTest.current_evidence_selection;
  if (!selection.valid || selection.workflow_id.empty())
    return;

  struct WorkflowRow {
    const ScriptEvidenceThumb *thumb = nullptr;
    std::string effective_status;
    bool prerequisites_ready = false;
  };

  auto lower = [](std::string value) {
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
  };
  auto isCompleteStatus = [&](const std::string &status) {
    const std::string value = lower(status);
    return value == "complete" || value == "completed" || value == "passed" ||
           value == "accepted" || value == "manual_gui_pass";
  };

  std::vector<WorkflowRow> rows;
  std::unordered_map<std::string, std::string> statusByCase;
  for (const ScriptEvidenceGroup &group : m_manualTest.script_evidence_groups) {
    for (const ScriptEvidenceThumb &thumb : group.thumbs) {
      if (thumb.workflow_id != selection.workflow_id)
        continue;
      std::string status = thumb.status;
      if (status.empty() || status == "ready" ||
          status == "pending_human_review")
        status =
            thumb.workflow_status.empty() ? "pending" : thumb.workflow_status;
      rows.push_back({&thumb, status, false});
      statusByCase[thumb.case_id] = status;
    }
  }
  if (rows.empty())
    return;

  std::stable_sort(rows.begin(), rows.end(),
                   [](const WorkflowRow &left, const WorkflowRow &right) {
                     if (left.thumb->workflow_stage_index !=
                         right.thumb->workflow_stage_index)
                       return left.thumb->workflow_stage_index <
                              right.thumb->workflow_stage_index;
                     return left.thumb->case_id < right.thumb->case_id;
                   });

  int completed = 0;
  bool allComplete = true;
  for (WorkflowRow &row : rows) {
    bool prerequisitesReady = true;
    std::istringstream prerequisites(row.thumb->workflow_prerequisites);
    std::string prerequisite;
    while (std::getline(prerequisites, prerequisite, ',')) {
      prerequisite = TrimLine(prerequisite);
      if (prerequisite.empty() || prerequisite == "none")
        continue;
      const auto found = statusByCase.find(prerequisite);
      if (found == statusByCase.end() || !isCompleteStatus(found->second)) {
        prerequisitesReady = false;
        break;
      }
    }
    row.prerequisites_ready = prerequisitesReady;
    const bool complete = isCompleteStatus(row.effective_status);
    completed += complete ? 1 : 0;
    allComplete = allComplete && complete;
  }

  const bool selectedHumanAccepted =
      lower(selection.status) == "manual_gui_pass";
  const bool promotionAllowed =
      selection.promotion_candidate && allComplete && selectedHumanAccepted &&
      !selection.parent_model_ref.empty() && !selection.child_model_ref.empty();

  ImGui::Separator();
  if (!ImGui::CollapsingHeader("Reliability Workflow / Promotion Gate",
                               ImGuiTreeNodeFlags_DefaultOpen))
    return;

  ImGui::Text("Workflow: %s", selection.workflow_id.c_str());
  const float progress = rows.empty() ? 0.0f
                                      : static_cast<float>(completed) /
                                            static_cast<float>(rows.size());
  const std::string progressLabel = std::to_string(completed) + " / " +
                                    std::to_string(rows.size()) + " complete";
  ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f), progressLabel.c_str());
  ImGui::Text("Selected: %s | stage %d/%d | dataset %s | frozen %s",
              selection.case_id.c_str(), selection.workflow_stage_index,
              selection.workflow_stage_count,
              selection.dataset_role.empty() ? "-"
                                             : selection.dataset_role.c_str(),
              selection.dataset_frozen ? "yes" : "no");
  ImGui::TextWrapped("Annotation policy: %s",
                     selection.annotation_policy.empty()
                         ? "-"
                         : selection.annotation_policy.c_str());
  ImGui::TextWrapped("Gate policy: %s", selection.gate_policy.empty()
                                            ? "-"
                                            : selection.gate_policy.c_str());
  ImGui::Text(
      "Parent: %s | Child: %s",
      selection.parent_model_ref.empty() ? "not bound"
                                         : selection.parent_model_ref.c_str(),
      selection.child_model_ref.empty() ? "not bound"
                                        : selection.child_model_ref.c_str());
  ImGui::TextColored(promotionAllowed ? ImVec4(0.35f, 0.85f, 0.45f, 1.0f)
                                      : ImVec4(1.0f, 0.65f, 0.2f, 1.0f),
                     "Promotion: %s",
                     promotionAllowed ? "ALLOWED_BY_RECORDED_GATES"
                                      : "BLOCKED");

  if (NormalizeEvidenceToolTypeLocal(selection.tool) == "TorchTask") {
    ImGui::Separator();
    ImGui::Text("Controlled-case admission: %s",
                selection.admission_status.empty()
                    ? "REFERENCE_ONLY"
                    : selection.admission_status.c_str());
    if (!selection.admission_reason.empty())
      ImGui::TextWrapped("Reason: %s", selection.admission_reason.c_str());
    const auto drawAdmissionReference = [](const char *label,
                                           const std::string &reference) {
      if (!reference.empty())
        ImGui::TextWrapped("%s: %s", label, reference.c_str());
    };
    drawAdmissionReference("Dataset summary", selection.dataset_summary_ref);
    drawAdmissionReference("Training provider receipt",
                           selection.training_receipt_ref);
    drawAdmissionReference("Candidate artifact",
                           selection.candidate_artifact_ref);
    drawAdmissionReference("Evaluation report",
                           selection.evaluation_report_ref);
    drawAdmissionReference("Evidence bundle", selection.evidence_bundle_ref);
    drawAdmissionReference("Rollback model", selection.rollback_model_ref);
    drawAdmissionReference("Quality conclusion",
                           selection.quality_conclusion_ref);
    ImGui::SeparatorText("Asset-driven Parameters / Configuration");
    DrawControlledCaseConfigurationAssetLocal("Training config",
                                               selection.training_config_ref);
    DrawControlledCaseConfigurationAssetLocal("Model manifest",
                                               selection.model_manifest_ref);
    DrawControlledCaseConfigurationAssetLocal("Inference config",
                                               selection.inference_config_ref);
    DrawControlledCaseConfigurationAssetLocal("Quality policy",
                                               selection.quality_policy_ref);
    DrawControlledCaseConfigurationAssetLocal("Label ontology",
                                               selection.ontology_ref);
    DrawControlledCaseConfigurationAssetLocal("Failure samples",
                                               selection.failure_samples_ref);
    ImGui::TextDisabled(
        "APPROVED and ACTIVE always require recorded human authorization; "
        "the asset declaration alone cannot promote or activate a model.");
  }

  if (ImGui::BeginTable("evidence_reliability_workflow", 5,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollY,
                        ImVec2(-1.0f, 230.0f))) {
    ImGui::TableSetupColumn("Step");
    ImGui::TableSetupColumn("Case");
    ImGui::TableSetupColumn("Stage");
    ImGui::TableSetupColumn("Status");
    ImGui::TableSetupColumn("Prerequisite");
    ImGui::TableHeadersRow();
    for (const WorkflowRow &row : rows) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%d", row.thumb->workflow_stage_index);
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(row.thumb->case_id.c_str());
      ImGui::TableSetColumnIndex(2);
      ImGui::TextUnformatted(row.thumb->workflow_stage.c_str());
      ImGui::TableSetColumnIndex(3);
      ImGui::TextUnformatted(row.effective_status.c_str());
      ImGui::TableSetColumnIndex(4);
      if (row.prerequisites_ready)
        ImGui::TextUnformatted("ready");
      else
        ImGui::Text("blocked: %s", row.thumb->workflow_prerequisites.c_str());
    }
    ImGui::EndTable();
  }
}

void ViewController::DrawTorchEvidenceTrainingPanel() {
  const CxEvidenceSelectionSnapshot &selection =
      m_manualTest.current_evidence_selection;
  if (!selection.valid ||
      NormalizeEvidenceToolTypeLocal(selection.tool) != "TorchTask")
    return;

  ImGui::Separator();
  if (!ImGui::CollapsingHeader("Dataset / Annotation / Training",
                               ImGuiTreeNodeFlags_DefaultOpen))
    return;

  int trainCount = 0;
  int valCount = 0;
  int testCount = 0;
  int annotatedCount = 0;
  for (const TorchTrainingImageItem &item :
       m_manualTest.torch_training_images) {
    if (item.split == "train")
      ++trainCount;
    else if (item.split == "val")
      ++valCount;
    else if (item.split == "test")
      ++testCount;
    if (item.annotation_shape_count > 0)
      ++annotatedCount;
  }
  ImGui::Text("Images: train %d | val %d | test %d | annotated %d", trainCount,
              valCount, testCount, annotatedCount);

  const CxTorchTrainingRunBinding &run = m_manualTest.torch_training_run;
  if (run.available) {
    ImGui::Text("Training: %s | %s | LR %.8g", run.status.c_str(),
                run.optimizer.c_str(), run.learning_rate);
    ImGui::Text("LR schedule: %s | min LR %.8g | weight decay %.8g",
                run.lr_schedule.c_str(), run.min_learning_rate,
                run.weight_decay);
    ImGui::Text("Loss weights: box %.4g | class %.4g | DFL %.4g | mask %.4g",
                run.box_loss_weight, run.class_loss_weight, run.dfl_loss_weight,
                run.mask_loss_weight);
    ImGui::Text("Loss phase: %s", run.loss_phase.c_str());
    ImGui::Text("Epochs: %d / %d | samples %d | instances %d",
                run.completed_epochs, run.configured_epochs,
                run.train_sample_count, run.train_instance_count);
    const float progress =
        run.configured_epochs > 0
            ? std::clamp(static_cast<float>(run.completed_epochs) /
                             run.configured_epochs,
                         0.0f, 1.0f)
            : 0.0f;
    ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));
    ImGui::TextWrapped("Training result: %s", run.training_trace_path.c_str());
    if (!run.incremental_model_path.empty())
      ImGui::TextWrapped("Incremental model: %s",
                         run.incremental_model_path.c_str());

    const bool hasRealTrainingCurve = run.HasRealMultiEpochSeries();
    if (!hasRealTrainingCurve) {
      const std::string curveStatus = run.TrainingCurveStatus();
      ImGui::TextColored(ImVec4(1.0f, 0.76f, 0.28f, 1.0f),
                         "Learning curve: %s", curveStatus.c_str());
      ImGui::TextDisabled(
          "No effective incremental training curve is available until a "
          "runtime-produced multi-epoch trace is bound.");
    }

    std::vector<float> learningRates;
    learningRates.reserve(run.epochs.size());
    for (const CxTorchTrainingEpochMetric &metric : run.epochs) {
      learningRates.push_back(static_cast<float>(metric.learning_rate));
    }
    if (hasRealTrainingCurve) {
      ImGui::Text("Loss / Epoch (%zu curve points)", run.epochs.size());
      DrawEpochLossCurveLocal("##evidence_loss_epoch_curve", run, 0, 170.0f,
                              m_manualTest.torch_training_epoch_axis_capacity,
                              m_manualTest.torch_training_loss_axis_maximum);
    }
    if (hasRealTrainingCurve && run.learning_rate > 0.0 &&
        !learningRates.empty())
      ImGui::TextDisabled("Learning-rate samples: %zu (shown in the epoch table; no auto-scaled X-axis chart).",
                          learningRates.size());
    if (hasRealTrainingCurve && run.epochs.size() >= 2) {
      int decreasingSteps = 0;
      for (std::size_t i = 1; i < run.epochs.size(); ++i) {
        if (run.epochs[i].total_loss < run.epochs[i - 1].total_loss)
          ++decreasingSteps;
      }
      const double firstLoss = run.epochs.front().total_loss;
      const double finalLoss = run.epochs.back().total_loss;
      const double reduction =
          firstLoss != 0.0 ? (firstLoss - finalLoss) / firstLoss * 100.0 : 0.0;
      ImGui::Text("Curve: first %.6g | final %.6g | reduction %.2f%% | "
                  "decreasing steps %d/%d",
                  firstLoss, finalLoss, reduction, decreasingSteps,
                  static_cast<int>(run.epochs.size() - 1));
    }

    if (ImGui::BeginTable("evidence_training_metrics", 7,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_ScrollY,
                          ImVec2(-1.0f, 180.0f))) {
      ImGui::TableSetupColumn("Epoch");
      ImGui::TableSetupColumn("LR");
      ImGui::TableSetupColumn("Total");
      ImGui::TableSetupColumn("Box");
      ImGui::TableSetupColumn("Class");
      ImGui::TableSetupColumn("DFL");
      ImGui::TableSetupColumn("Mask");
      ImGui::TableHeadersRow();
      for (const CxTorchTrainingEpochMetric &metric : run.epochs) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%d", metric.epoch);
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%.3g", metric.learning_rate);
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%.5f", metric.total_loss);
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%.5f", metric.box_loss);
        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%.5f", metric.class_loss);
        ImGui::TableSetColumnIndex(5);
        ImGui::Text("%.5f", metric.dfl_loss);
        ImGui::TableSetColumnIndex(6);
        ImGui::Text("%.5f", metric.mask_loss);
      }
      ImGui::EndTable();
    }
    if (ImGui::TreeNodeEx("Parameter Update Map by Epoch",
                          ImGuiTreeNodeFlags_DefaultOpen)) {
      if (ImGui::BeginTable("evidence_parameter_update_map", 6,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_ScrollY,
                            ImVec2(-1.0f, 190.0f))) {
        ImGui::TableSetupColumn("Epoch");
        ImGui::TableSetupColumn("Group");
        ImGui::TableSetupColumn("Grad mean");
        ImGui::TableSetupColumn("Grad norm");
        ImGui::TableSetupColumn("Update norm");
        ImGui::TableSetupColumn("Update / Param");
        ImGui::TableHeadersRow();
        for (const CxTorchTrainingEpochMetric &metric : run.epochs) {
          for (const auto &group : metric.parameter_groups) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%d", metric.epoch);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(group.name.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.4g", group.grad_mean);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.4g", group.grad_norm);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.4g", group.update_norm);
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%.4g", group.param_norm > 0.0
                                    ? group.update_norm / group.param_norm
                                    : 0.0);
          }
        }
        ImGui::EndTable();
      }
      ImGui::TreePop();
    }
  } else {
    ImGui::TextDisabled("Training trace is not available for this case.");
  }

  m_manualTest.script_evidence_thumb_load_count_this_frame = 0;
  DrawTorchTrainingImageRail("train", "Training Set / 训练集");
  DrawTorchTrainingImageRail("val", "Validation Set / 验证集");
  DrawTorchTrainingImageRail("test", "Test Set / 测试集");

  if (m_manualTest.selected_torch_training_image >= 0 &&
      m_manualTest.selected_torch_training_image <
          static_cast<int>(m_manualTest.torch_training_images.size())) {
    const TorchTrainingImageItem &item =
        m_manualTest.torch_training_images[static_cast<std::size_t>(
            m_manualTest.selected_torch_training_image)];
    ImGui::Text("Selected annotation: %s | class boxes %d",
                item.image_id.c_str(), item.annotation_shape_count);
    if (ImGui::BeginTable("evidence_annotation_metrics", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg,
                          ImVec2(-1.0f, 105.0f))) {
      ImGui::TableSetupColumn("Class");
      ImGui::TableSetupColumn("Shape");
      ImGui::TableSetupColumn("Bounds");
      ImGui::TableHeadersRow();
      for (const TorchTrainingAnnotationShapeSnapshot &shape :
           item.annotation_shapes) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%d", shape.class_id);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(shape.shape_kind.c_str());
        ImGui::TableSetColumnIndex(2);
        if (shape.points_xy.size() >= 6)
          ImGui::Text("%.0f,%.0f - %.0f,%.0f", shape.points_xy[0],
                      shape.points_xy[1], shape.points_xy[4],
                      shape.points_xy[5]);
        else
          ImGui::TextUnformatted("-");
      }
      ImGui::EndTable();
    }
  }
}

static bool SaveEvidenceManualReviewLocal(const ScriptEvidenceThumb &thumb,
                                          const std::string &decision,
                                          std::string &savedPath,
                                          std::string &reason) {
  reason.clear();
  savedPath.clear();

  std::filesystem::path reviewDir;
  if (!thumb.candidate_dir.empty()) {
    reviewDir = std::filesystem::path(thumb.candidate_dir);
  } else {
    std::string safeCase =
        thumb.case_id.empty() ? "evidence_case" : thumb.case_id;
    for (char &ch : safeCase) {
      const unsigned char uch = static_cast<unsigned char>(ch);
      if (!std::isalnum(uch) && ch != '-' && ch != '_')
        ch = '_';
    }
    reviewDir = ResolveCxVisionRunPath(
        "cxscript_runs/evidence_chain/manual_reviews/" + safeCase);
  }

  std::error_code ec;
  std::filesystem::create_directories(reviewDir, ec);
  if (ec) {
    reason = "failed to create manual review directory: " + reviewDir.string() +
             " reason=" + ec.message();
    return false;
  }

  const std::filesystem::path reviewPath = reviewDir / "human_review.json";
  std::ofstream out(reviewPath, std::ios::binary | std::ios::trunc);
  if (!out) {
    reason = "failed to open manual review file: " + reviewPath.string();
    return false;
  }

  const std::string reviewItemLabel = StripEvidenceCandidateDisplaySuffixLocal(
      thumb.review_item.empty()
          ? (thumb.script_id.empty() ? thumb.case_id : thumb.script_id)
          : thumb.review_item);

  out << "{\n"
      << "  \"schema\": \"cxvision.manual_gui_review.v1\",\n"
      << "  \"review_item_label\": \"" << JsonEscape(reviewItemLabel) << "\",\n"
      << "  \"case_id\": \"" << JsonEscape(thumb.case_id) << "\",\n"
      << "  \"script_id\": \"" << JsonEscape(thumb.script_id) << "\",\n"
      << "  \"image_id\": \"" << JsonEscape(thumb.image_id) << "\",\n"
      << "  \"target_id\": \"" << JsonEscape(thumb.target_id) << "\",\n"
      << "  \"tool\": \"" << JsonEscape(thumb.tool) << "\",\n"
      << "  \"decision\": \"" << JsonEscape(decision) << "\",\n"
      << "  \"reviewed_at\": \"" << JsonEscape(CurrentTimestamp()) << "\",\n"
      << "  \"review_source\": \"Manual Review / Evidence > To Verify\",\n"
      << "  \"promotion_allowed\": false,\n"
      << "  \"image_path\": \"" << JsonEscape(thumb.image_path) << "\",\n"
      << "  \"script_path\": \"" << JsonEscape(thumb.script_path) << "\",\n"
      << "  \"parameter_summary\": \"" << JsonEscape(thumb.parameter_summary)
      << "\",\n"
      << "  \"review_note\": \"Decision saved by a human from Manual Review / "
         "Evidence > To Verify using review_item_label; promotion remains "
         "blocked.\"\n"
      << "}\n";
  out.flush();
  if (!out) {
    reason = "failed to write manual review file: " + reviewPath.string();
    return false;
  }

  savedPath = reviewPath.string();
  reason = "manual review saved: " + savedPath;
  return true;
}

static std::string TorchDatasetFileStemLocal(const TorchTrainingImageItem &item,
                                             std::size_t index) {
  std::string stem = item.image_id.empty()
                         ? ("image_" + std::to_string(index + 1))
                         : item.image_id;
  for (char &ch : stem) {
    if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '-' &&
        ch != '_') {
      ch = '_';
    }
  }
  return stem;
}

static bool RasterizeTorchTrainingShapeLocal(
    const TorchTrainingAnnotationShapeSnapshot &shape, cv::Mat &mask,
    bool &weakBoxSupervision) {
  weakBoxSupervision = false;
  if (shape.result_element)
    return false;
  std::string semanticRole = shape.semantic_role;
  std::transform(
      semanticRole.begin(), semanticRole.end(), semanticRole.begin(),
      [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  if (shape.shape_kind == "RectShape" ||
      semanticRole.find("bbox") != std::string::npos)
    return false;
  if (shape.shape_kind != "PolylineShape" || !shape.closed)
    return false;

  std::vector<cv::Point> points;
  for (std::size_t i = 1; i < shape.points_xy.size(); i += 2) {
    const double x = shape.points_xy[i - 1];
    const double y = shape.points_xy[i];
    if (!std::isfinite(x) || !std::isfinite(y) || x < 0.0 || y < 0.0 ||
        x >= mask.cols || y >= mask.rows)
      return false;
    const cv::Point point(static_cast<int>(std::lround(x)),
                          static_cast<int>(std::lround(y)));
    if (points.empty() || points.back() != point)
      points.push_back(point);
  }
  if (points.size() > 3 && points.front() == points.back())
    points.pop_back();
  if (points.size() < 3 || std::abs(cv::contourArea(points)) < 1.0)
    return false;

  auto orientation = [](const cv::Point &a, const cv::Point &b,
                        const cv::Point &c) {
    const long long value = static_cast<long long>(b.y - a.y) * (c.x - b.x) -
                            static_cast<long long>(b.x - a.x) * (c.y - b.y);
    return (value > 0) - (value < 0);
  };
  auto onSegment = [](const cv::Point &a, const cv::Point &b,
                      const cv::Point &c) {
    return b.x >= std::min(a.x, c.x) && b.x <= std::max(a.x, c.x) &&
           b.y >= std::min(a.y, c.y) && b.y <= std::max(a.y, c.y);
  };
  auto intersects = [&](const cv::Point &a, const cv::Point &b,
                        const cv::Point &c, const cv::Point &d) {
    const int o1 = orientation(a, b, c);
    const int o2 = orientation(a, b, d);
    const int o3 = orientation(c, d, a);
    const int o4 = orientation(c, d, b);
    if (o1 != o2 && o3 != o4)
      return true;
    return (o1 == 0 && onSegment(a, c, b)) || (o2 == 0 && onSegment(a, d, b)) ||
           (o3 == 0 && onSegment(c, a, d)) || (o4 == 0 && onSegment(c, b, d));
  };
  for (std::size_t i = 0; i < points.size(); ++i) {
    const std::size_t iNext = (i + 1) % points.size();
    for (std::size_t j = i + 1; j < points.size(); ++j) {
      const std::size_t jNext = (j + 1) % points.size();
      if (i == j || iNext == j || jNext == i)
        continue;
      if (intersects(points[i], points[iNext], points[j], points[jNext]))
        return false;
    }
  }

  cv::fillPoly(mask, std::vector<std::vector<cv::Point>>{points},
               cv::Scalar(255), cv::LINE_8);
  return cv::countNonZero(mask) > 0;
}

bool ViewController::ExportTorchTrainingLabelPackage(std::string &packagePath,
                                                     std::string &reason) {
  packagePath.clear();
  reason.clear();

  std::string runId = CxUnifiedLog::Instance().RunId();
  if (runId.empty())
    runId = "ui_session";
  const std::filesystem::path outputDir =
      ResolveCxVisionRunPath("cxscript_runs/manual_torch_dataset") / runId;
  std::error_code ec;
  std::filesystem::create_directories(outputDir, ec);
  const std::filesystem::path maskDir = outputDir / "masks";
  const std::filesystem::path overlayDir = outputDir / "overlays";
  if (!ec)
    std::filesystem::create_directories(maskDir, ec);
  if (!ec)
    std::filesystem::create_directories(overlayDir, ec);
  if (ec) {
    reason = "cannot create Torch training label package directory: " +
             outputDir.string() + " reason=" + ec.message();
    return false;
  }

  int imageCount = 0;
  int imageMissingCount = 0;
  int shapeCount = 0;
  int closedRegionCount = 0;
  int bboxCandidateCount = 0;
  int rasterMaskCount = 0;
  int rejectedShapeCount = 0;
  int weakMaskCount = 0;
  int bboxOnlyRejectedCount = 0;
  int invalidPolygonCount = 0;
  int trainImageWithoutMaskCount = 0;
  int trainImageCount = 0;
  int validationImageCount = 0;
  int holdoutImageCount = 0;
  int unknownSplitCount = 0;
  std::set<std::string> trainLineages;
  std::set<std::string> validationLineages;
  std::set<std::string> holdoutLineages;
  std::ostringstream imageRows;

  for (std::size_t imageIndex = 0;
       imageIndex < m_manualTest.torch_training_images.size(); ++imageIndex) {
    const TorchTrainingImageItem &item =
        m_manualTest.torch_training_images[imageIndex];
    const std::string normalizedSplit =
        NormalizeTorchTrainingSplitLocal(item.split);
    std::string lineageKey;
    if (item.source.rfind("source_hash:", 0) == 0)
      lineageKey = item.source;
    else
      lineageKey = ResolveWorkspaceFile(item.image_path)
                       .lexically_normal()
                       .generic_string();
    if (normalizedSplit == "train") {
      ++trainImageCount;
      trainLineages.insert(lineageKey);
    } else if (normalizedSplit == "val") {
      ++validationImageCount;
      validationLineages.insert(lineageKey);
    } else if (normalizedSplit == "test") {
      ++holdoutImageCount;
      holdoutLineages.insert(lineageKey);
    } else {
      ++unknownSplitCount;
    }
    cv::Mat image = cv::imread(item.image_path, cv::IMREAD_COLOR);
    const bool imageExists = !image.empty();
    ++imageCount;
    if (!imageExists)
      ++imageMissingCount;

    cv::Mat mask;
    bool weakBoxSupervision = false;
    int acceptedShapeCount = 0;
    if (imageExists)
      mask = cv::Mat::zeros(image.rows, image.cols, CV_8UC1);
    for (const TorchTrainingAnnotationShapeSnapshot &shape :
         item.annotation_shapes) {
      std::string semanticRole = shape.semantic_role;
      std::transform(
          semanticRole.begin(), semanticRole.end(), semanticRole.begin(),
          [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
      const bool bboxOnly = shape.shape_kind == "RectShape" ||
                            semanticRole.find("bbox") != std::string::npos;
      if (bboxOnly)
        ++bboxOnlyRejectedCount;
      if (imageExists &&
          RasterizeTorchTrainingShapeLocal(shape, mask, weakBoxSupervision)) {
        ++acceptedShapeCount;
      } else {
        ++rejectedShapeCount;
        if (!bboxOnly)
          ++invalidPolygonCount;
      }
    }
    if (item.split == "train" && acceptedShapeCount == 0)
      ++trainImageWithoutMaskCount;

    std::string maskPath;
    std::string overlayPath;
    double foregroundRatio = 0.0;
    cv::Point positivePoint(-1, -1);
    cv::Point negativePoint(-1, -1);
    if (imageExists && acceptedShapeCount > 0 && cv::countNonZero(mask) > 0) {
      const std::string stem = TorchDatasetFileStemLocal(item, imageIndex);
      const std::filesystem::path maskFile = maskDir / (stem + "_mask.png");
      const std::filesystem::path overlayFile =
          overlayDir / (stem + "_overlay.png");
      if (!cv::imwrite(maskFile.string(), mask)) {
        reason = "failed to write training mask: " + maskFile.string();
        return false;
      }

      cv::Mat overlay = image.clone();
      cv::Mat tint(image.size(), image.type(), cv::Scalar(0, 0, 255));
      tint.copyTo(overlay, mask);
      cv::addWeighted(image, 0.60, overlay, 0.40, 0.0, overlay);
      if (!cv::imwrite(overlayFile.string(), overlay)) {
        reason = "failed to write training overlay: " + overlayFile.string();
        return false;
      }

      maskPath = maskFile.string();
      overlayPath = overlayFile.string();
      foregroundRatio = static_cast<double>(cv::countNonZero(mask)) /
                        static_cast<double>(mask.rows * mask.cols);
      const cv::Moments moments = cv::moments(mask, true);
      if (moments.m00 > 0.0) {
        positivePoint.x =
            static_cast<int>(std::lround(moments.m10 / moments.m00));
        positivePoint.y =
            static_cast<int>(std::lround(moments.m01 / moments.m00));
      }
      const cv::Point candidates[] = {{0, 0},
                                      {mask.cols - 1, 0},
                                      {0, mask.rows - 1},
                                      {mask.cols - 1, mask.rows - 1}};
      for (const cv::Point &candidate : candidates) {
        if (mask.at<unsigned char>(candidate) == 0) {
          negativePoint = candidate;
          break;
        }
      }
      ++rasterMaskCount;
      if (weakBoxSupervision)
        ++weakMaskCount;
    }
    if (imageIndex != 0)
      imageRows << ",\n";
    imageRows << "    {\n";
    imageRows << "      \"image_id\": \"" << JsonEscape(item.image_id)
              << "\",\n";
    imageRows << "      \"image_path\": \"" << JsonEscape(item.image_path)
              << "\",\n";
    imageRows << "      \"image_exists\": " << (imageExists ? "true" : "false")
              << ",\n";
    imageRows << "      \"width\": " << (imageExists ? image.cols : 0) << ",\n";
    imageRows << "      \"height\": " << (imageExists ? image.rows : 0)
              << ",\n";
    imageRows << "      \"case_id\": \"" << JsonEscape(item.case_id) << "\",\n";
    imageRows << "      \"target_id\": \"" << JsonEscape(item.target_id)
              << "\",\n";
    imageRows << "      \"source\": \"" << JsonEscape(item.source) << "\",\n";
    imageRows << "      \"split\": \"" << JsonEscape(item.split) << "\",\n";
    imageRows << "      \"label\": \"" << JsonEscape(item.label) << "\",\n";
    imageRows << "      \"annotation_status\": \""
              << JsonEscape(item.annotation_status) << "\",\n";
    imageRows << "      \"mask_path\": \"" << JsonEscape(maskPath) << "\",\n";
    imageRows << "      \"overlay_path\": \"" << JsonEscape(overlayPath)
              << "\",\n";
    imageRows << "      \"mask_supervision\": \""
              << (acceptedShapeCount > 0 ? "polygon" : "rejected") << "\",\n";
    imageRows << "      \"foreground_ratio\": " << foregroundRatio << ",\n";
    imageRows << "      \"positive_point\": [" << positivePoint.x << ","
              << positivePoint.y << "],\n";
    imageRows << "      \"negative_point\": [" << negativePoint.x << ","
              << negativePoint.y << "],\n";
    imageRows << "      \"accepted_shape_count\": " << acceptedShapeCount
              << ",\n";
    imageRows << "      \"shapes\": [";
    for (std::size_t shapeIndex = 0; shapeIndex < item.annotation_shapes.size();
         ++shapeIndex) {
      const TorchTrainingAnnotationShapeSnapshot &shape =
          item.annotation_shapes[shapeIndex];
      ++shapeCount;
      const bool closedRegion =
          shape.closed && (shape.shape_kind == "RectShape" ||
                           shape.shape_kind == "CircleShape" ||
                           shape.shape_kind == "EllipseShape" ||
                           shape.shape_kind == "PolylineShape");
      if (closedRegion)
        ++closedRegionCount;
      if (shape.shape_kind == "RectShape" ||
          shape.semantic_role.find("bbox") != std::string::npos)
        ++bboxCandidateCount;
      if (shapeIndex != 0)
        imageRows << ",";
      imageRows << "\n        {\"stable_ref\":\""
                << JsonEscape(shape.stable_ref)
                << "\",\"class_id\":" << shape.class_id << ",\"shape_kind\":\""
                << JsonEscape(shape.shape_kind) << "\",\"semantic_role\":\""
                << JsonEscape(shape.semantic_role) << "\",\"owner_binding\":\""
                << JsonEscape(shape.owner_binding)
                << "\",\"closed\":" << (shape.closed ? "true" : "false")
                << ",\"center_x\":" << shape.center_x
                << ",\"center_y\":" << shape.center_y
                << ",\"radius_x\":" << shape.radius_x
                << ",\"radius_y\":" << shape.radius_y
                << ",\"radius\":" << shape.radius
                << ",\"angle\":" << shape.angle << ",\"points_xy\":[";
      for (std::size_t pointIndex = 0; pointIndex < shape.points_xy.size();
           ++pointIndex) {
        if (pointIndex != 0)
          imageRows << ",";
        imageRows << shape.points_xy[pointIndex];
      }
      imageRows << "]}";
    }
    if (!item.annotation_shapes.empty())
      imageRows << "\n      ";
    imageRows << "]\n    }";
  }
  int sourceLineageOverlapCount = 0;
  for (const std::string &lineage : validationLineages) {
    if (trainLineages.count(lineage) != 0)
      ++sourceLineageOverlapCount;
  }
  for (const std::string &lineage : holdoutLineages) {
    if (trainLineages.count(lineage) != 0 ||
        validationLineages.count(lineage) != 0)
      ++sourceLineageOverlapCount;
  }
  const bool splitContractReady = trainImageCount > 0 &&
                                  validationImageCount > 0 &&
                                  holdoutImageCount > 0 &&
                                  unknownSplitCount == 0;
  const bool datasetReady = imageCount > 0 && splitContractReady &&
                             sourceLineageOverlapCount == 0 &&
                             imageMissingCount == 0 &&
                             rasterMaskCount > 0 && bboxOnlyRejectedCount == 0 &&
                             invalidPolygonCount == 0 &&
                             trainImageWithoutMaskCount == 0;
  const std::string datasetStatus = datasetReady
      ? "DATASET_EXPORT_READY_TO_VERIFY"
      : (!splitContractReady
             ? "DATASET_PREFLIGHT_FAIL_SPLIT_REQUIRED"
             : (sourceLineageOverlapCount > 0
                    ? "DATASET_PREFLIGHT_FAIL_SOURCE_LINEAGE_OVERLAP"
                    : "DATASET_PREFLIGHT_FAIL_REAL_POLYGON_REQUIRED"));
  std::ostringstream json;
  json << "{\n";
  json << "  \"schema\": \"cxvision.torch.training_dataset.v2\",\n";
  json << "  \"status\": \"" << datasetStatus << "\",\n";
  json << "  \"training_mode\": \"polygon_mask_dataset\",\n";
  json << "  \"dataset_consumed_by_current_runtime\": "
       << (datasetReady ? "true" : "false") << ",\n";
  json << "  \"segmentation_mask_export_ready\": "
       << (datasetReady ? "true" : "false") << ",\n";
  json << "  \"run_id\": \"" << JsonEscape(runId) << "\",\n";
  json << "  \"evidence_case_id\": \""
       << JsonEscape(m_manualTest.current_evidence_selection.case_id)
       << "\",\n";
  json << "  \"images\": [\n" << imageRows.str();
  json << "\n  ],\n";
  json << "  \"summary\": {\n";
  json << "    \"image_count\": " << imageCount << ",\n";
  json << "    \"image_missing_count\": " << imageMissingCount << ",\n";
  json << "    \"shape_count\": " << shapeCount << ",\n";
  json << "    \"closed_region_count\": " << closedRegionCount << ",\n";
  json << "    \"bbox_candidate_count\": " << bboxCandidateCount << ",\n";
  json << "    \"raster_mask_count\": " << rasterMaskCount << ",\n";
  json << "    \"weak_mask_count\": " << weakMaskCount << ",\n";
  json << "    \"bbox_only_rejected_count\": " << bboxOnlyRejectedCount
       << ",\n";
  json << "    \"invalid_polygon_count\": " << invalidPolygonCount << ",\n";
  json << "    \"train_image_without_mask_count\": "
       << trainImageWithoutMaskCount << ",\n";
  json << "    \"train_image_count\": " << trainImageCount << ",\n";
  json << "    \"validation_image_count\": " << validationImageCount
       << ",\n";
  json << "    \"holdout_image_count\": " << holdoutImageCount << ",\n";
  json << "    \"unknown_split_count\": " << unknownSplitCount << ",\n";
  json << "    \"source_lineage_overlap_count\": "
       << sourceLineageOverlapCount << ",\n";
  json << "    \"rejected_shape_count\": " << rejectedShapeCount << "\n";
  json << "  }\n}";

  const std::filesystem::path path =
      outputDir / "torch_training_dataset_manifest.json";
  if (!WriteTextFile(path, json.str())) {
    reason = "failed to write Torch training label package: " + path.string();
    return false;
  }

  packagePath = path.string();

  if (datasetReady) {
    m_manualTest.torch_training_last_exported_dataset_manifest = packagePath;
    m_manualTest.torch_training_last_exported_case_id =
        m_manualTest.current_evidence_selection.case_id;
  }
  reason = datasetStatus + ": images=" + std::to_string(imageCount) +
           " train=" + std::to_string(trainImageCount) +
           " val=" + std::to_string(validationImageCount) +
           " holdout=" + std::to_string(holdoutImageCount) +
           " source_overlap=" +
               std::to_string(sourceLineageOverlapCount) +
           " masks=" + std::to_string(rasterMaskCount) +
           " bbox_only_rejected=" + std::to_string(bboxOnlyRejectedCount) +
           " invalid_polygons=" + std::to_string(invalidPolygonCount) +
           " rejected_shapes=" + std::to_string(rejectedShapeCount);
  m_manualTest.torch_training_image_status = datasetStatus;
  m_manualTest.torch_training_image_reason = reason + " path=" + packagePath;
  CXLOG_INFO("TorchTrainingImageSet", "training_dataset_exported",
             datasetStatus,
             "path=" + packagePath + " images=" + std::to_string(imageCount) +
                 " shapes=" + std::to_string(shapeCount) +
                 " closed_regions=" + std::to_string(closedRegionCount) +
                 " bbox_candidates=" + std::to_string(bboxCandidateCount) +
                 " masks=" + std::to_string(rasterMaskCount));
  return datasetReady;
}

bool ViewController::RunTorchTrainingLabelPackageSmoke(
    const std::string &preferredScriptId, const std::string &requestedOutDir,
    std::string &packagePath, std::string &reason) {
  packagePath.clear();
  reason.clear();

  const std::string runId = CxUnifiedLog::Instance().RunId().empty()
                                ? "ui_session"
                                : CxUnifiedLog::Instance().RunId();
  const std::filesystem::path outDir =
      requestedOutDir.empty()
          ? ResolveCxVisionRunPath(
                "cxscript_runs/torch_training_label_package_smoke") /
                runId
          : std::filesystem::path(requestedOutDir);
  std::error_code ec;
  std::filesystem::create_directories(outDir, ec);
  if (ec) {
    reason = "cannot create smoke output directory: " + outDir.string() +
             " reason=" + ec.message();
    return false;
  }

  CxEvidenceSelfTestRequest directRequest;
  const CxEvidenceSelfTestRequest *selectedRequest = nullptr;
  bool directDatasetPrepared = false;
  const std::filesystem::path preferredPath =
      ResolveWorkspaceFile(preferredScriptId);
  if (!preferredScriptId.empty() && preferredPath.extension() == ".cxsc" &&
      std::filesystem::is_regular_file(preferredPath)) {
    CxScriptEvidenceChainRuntime chain;
    std::string loadReason;
    if (!LoadCxScriptEvidenceChainFile(preferredPath.string(), chain,
                                       loadReason)) {
      reason = "cannot load requested Evidence dataset file: " + loadReason;
      return false;
    }
    const auto evidenceCase = std::find_if(
        chain.cases.begin(), chain.cases.end(),
        [](const CxScriptEvidenceCase &item) {
          return NormalizeEvidenceToolTypeLocal(item.tool) == "TorchTask" &&
                 !item.dataset_images.empty();
        });
    if (evidenceCase == chain.cases.end()) {
      reason = "requested Evidence dataset file has no TorchTask dataset case";
      return false;
    }

    std::vector<CxEvidenceAnnotationBinding> annotations;
    for (const CxScriptEvidenceAnnotation &source : evidenceCase->annotations) {
      CxEvidenceAnnotationBinding binding;
      binding.image_id = source.image_id;
      binding.shape_kind = source.shape_kind;
      binding.semantic_role = source.semantic_role;
      binding.owner_binding = source.owner_binding;
      binding.label = source.label;
      binding.class_id = source.class_id;
      binding.x0 = source.x0;
      binding.y0 = source.y0;
      binding.x1 = source.x1;
      binding.y1 = source.y1;
      binding.normalized = source.normalized;
      binding.closed = source.closed;
      binding.points_xy = source.points_xy;
      annotations.push_back(std::move(binding));
    }

    ClearTorchTrainingImageSetForEvidenceSyncLocal(
        m_manualTest, "load Training Image Set from Evidence dataset file");
    m_manualTest.active_case_id = evidenceCase->evidence_id;
    m_manualTest.active_target_id = evidenceCase->target_id;
    for (const CxScriptEvidenceDatasetImage &source :
         evidenceCase->dataset_images) {
      AddTorchTrainingImageFromPath(source.image_path, source.image_id,
                                    source.split, source.label, source.source);
      TorchTrainingImageItem &added = m_manualTest.torch_training_images.back();
      added.case_id = evidenceCase->evidence_id;
      added.target_id = evidenceCase->target_id;
      ApplyEvidenceAnnotationsToTorchTrainingItemLocal(added, annotations);
    }
    const auto annotated =
        std::find_if(m_manualTest.torch_training_images.begin(),
                     m_manualTest.torch_training_images.end(),
                     [](const TorchTrainingImageItem &item) {
                       return !item.annotation_shapes.empty();
                     });
    if (annotated == m_manualTest.torch_training_images.end()) {
      reason =
          "Evidence dataset has no annotations; GUI annotation is required";
      return false;
    }
    m_manualTest.selected_torch_training_image = static_cast<int>(
        std::distance(m_manualTest.torch_training_images.begin(), annotated));
    directRequest.case_id = evidenceCase->evidence_id;
    directRequest.script_id =
        DeriveEvidenceScriptIdLocal(evidenceCase->script_id);
    directRequest.script_path = evidenceCase->script_id;
    directRequest.image_id = evidenceCase->image_id;
    directRequest.image_path = annotated->image_path;
    directRequest.target_id = evidenceCase->target_id;
    directRequest.tool = "TorchTask";
    selectedRequest = &directRequest;
    directDatasetPrepared = true;
  } else {
    CxEvidenceSelfTestBatchRequest batch;
    batch.run_id = runId;
    batch.out_dir = outDir.string();
    batch.tool_filter = "TorchTask";
    std::string batchReason;
    if (!BuildEvidenceSelfTestBatchFromCurrentEvidenceRows(batch,
                                                           batchReason)) {
      reason = "cannot resolve Torch evidence rows: " + batchReason;
      return false;
    }
    for (const CxEvidenceSelfTestRequest &item : batch.cases) {
      if (!preferredScriptId.empty() &&
          (item.script_id == preferredScriptId ||
           item.case_id == preferredScriptId ||
           item.script_path == preferredScriptId)) {
        directRequest = item;
        selectedRequest = &directRequest;
        break;
      }
    }
    if (selectedRequest == nullptr && preferredScriptId.empty()) {
      for (const CxEvidenceSelfTestRequest &item : batch.cases) {
        std::string key =
            item.script_id + " " + item.script_path + " " + item.tool;
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char ch) {
                         return static_cast<char>(std::tolower(ch));
                       });
        if (key.find("torch") != std::string::npos &&
            !item.image_path.empty()) {
          directRequest = item;
          selectedRequest = &directRequest;
          break;
        }
      }
    }
  }
  if (selectedRequest == nullptr) {
    reason = preferredScriptId.empty()
                 ? "no Torch evidence case with an image is available for "
                   "label-package smoke"
                 : "requested Torch evidence script/case was not found: " +
                       preferredScriptId;
    return false;
  }

  CXLOG_INFO("TorchTrainingImageSet", "training_label_package_smoke_begin",
             "running",
             "script_id=" + selectedRequest->script_id +
                 " case_id=" + selectedRequest->case_id);

  CxEvidenceSelectionSnapshot snapshot;
  std::string stageReason;
  if (!directDatasetPrepared && (!ResolveEvidenceSelfTestSnapshot(
                                     *selectedRequest, snapshot, stageReason) ||
                                 !ApplyEvidenceSelectionSnapshotToManualContext(
                                     snapshot, false, stageReason))) {
    reason = "Torch evidence selection/load failed: " + stageReason;
    return false;
  }
  // The command-line smoke intentionally has no GLFW/OpenGL Image View.
  // Reuse the same loaded-image state path used by Evidence selftests;
  // actual image-view drawing/pointer behavior remains covered by GUI L2.
  const std::string selectedImagePath =
      directDatasetPrepared ? selectedRequest->image_path : snapshot.image_path;
  if (!LoadImageForEvidenceSelfTest(selectedImagePath, stageReason)) {
    reason = "headless Image View state load failed: " + stageReason;
    return false;
  }
  // ApplyEvidenceSelectionSnapshotToManualContext() already performs the
  // dataset sync for image-set evidence.  Rebuilding it here would clear
  // and recreate the same rail a second time, which is both redundant and
  // inconsistent with the UI selection transaction.
  CXLOG_INFO("TorchTrainingImageSet",
             "training_label_package_smoke_dataset_reused", "ready",
             "case_id=" + selectedRequest->case_id + " image_count=" +
                 std::to_string(m_manualTest.torch_training_images.size()));
  if (m_manualTest.torch_training_images.empty()) {
    reason = "Torch evidence selection produced no Training Image Set items";
    return false;
  }

  int imageIndex = m_manualTest.selected_torch_training_image;
  if (imageIndex < 0 ||
      imageIndex >=
          static_cast<int>(m_manualTest.torch_training_images.size())) {
    imageIndex = 0;
  }
  if (m_manualTest.torch_training_images[static_cast<std::size_t>(imageIndex)]
          .annotation_shapes.empty()) {
    const auto annotated =
        std::find_if(m_manualTest.torch_training_images.begin(),
                     m_manualTest.torch_training_images.end(),
                     [](const TorchTrainingImageItem &candidate) {
                       return !candidate.annotation_shapes.empty();
                     });
    if (annotated == m_manualTest.torch_training_images.end()) {
      reason =
          "Training Image Set has no annotations; GUI annotation is required";
      return false;
    }
    imageIndex = static_cast<int>(
        std::distance(m_manualTest.torch_training_images.begin(), annotated));
  }
  m_manualTest.selected_torch_training_image = imageIndex;
  RestoreTorchTrainingAnnotationState(
      m_manualTest.torch_training_images[static_cast<std::size_t>(imageIndex)]);
  m_manualTest.torch_training_image_status = "HEADLESS_ANNOTATION_READY";
  m_manualTest.torch_training_image_reason =
      "headless state simulation: selected training image and restored "
      "annotations";
  CXLOG_INFO(
      "TorchTrainingImageSet",
      "training_label_package_smoke_image_state_loaded",
      "HEADLESS_ANNOTATION_READY",
      "index=" + std::to_string(imageIndex) + " image_path=" +
          m_manualTest
              .torch_training_images[static_cast<std::size_t>(imageIndex)]
              .image_path);

  TorchTrainingImageItem &item =
      m_manualTest.torch_training_images[static_cast<std::size_t>(imageIndex)];
  cv::Mat image = cv::imread(item.image_path, cv::IMREAD_UNCHANGED);
  if (image.empty()) {
    reason = "selected Training Image Set image cannot be opened: " +
             item.image_path;
    return false;
  }

  const std::string stableRef = item.annotation_shapes.front().stable_ref;
  std::size_t annotationShapeCount = 0;
  for (const TorchTrainingImageItem &candidate :
       m_manualTest.torch_training_images)
    annotationShapeCount += candidate.annotation_shapes.size();

  std::string exportReason;
  if (!ExportTorchTrainingLabelPackage(packagePath, exportReason)) {
    reason = "label package export failed: " + exportReason;
    return false;
  }

  std::ifstream packageFile(packagePath, std::ios::binary);
  const std::string packageText((std::istreambuf_iterator<char>(packageFile)),
                                std::istreambuf_iterator<char>());
  const bool schemaOk =
      packageText.find("cxvision.torch.training_dataset.v2") !=
      std::string::npos;
  const bool readyOk =
      packageText.find("\"status\": \"DATASET_EXPORT_READY_TO_VERIFY\"") !=
      std::string::npos;
  const bool imageOk =
      packageText.find(JsonEscape(item.image_path)) != std::string::npos;
  const bool labelOk =
      !item.label.empty() &&
      packageText.find("\"label\": \"" + JsonEscape(item.label) + "\"") !=
          std::string::npos;
  const bool maskOk =
      packageText.find("\"mask_path\": \"") != std::string::npos &&
      packageText.find("\"raster_mask_count\": 0") == std::string::npos;
  const bool promptOk =
      packageText.find("\"positive_point\": [") != std::string::npos &&
      packageText.find("\"negative_point\": [") != std::string::npos;
  const bool pass =
      schemaOk && readyOk && imageOk && labelOk && maskOk && promptOk;

  std::ostringstream report;
  report << "{\n"
         << "  \"schema\": \"cxvision.torch.training_dataset_smoke.v2\",\n"
         << "  \"conclusion\": \""
         << (pass ? "TRAINING_DATASET_EXPORT_PASS_TO_VERIFY"
                  : "TRAINING_DATASET_EXPORT_FAIL")
         << "\",\n"
         << "  \"script_id\": \"" << JsonEscape(selectedRequest->script_id)
         << "\",\n"
         << "  \"case_id\": \"" << JsonEscape(selectedRequest->case_id)
         << "\",\n"
         << "  \"image_path\": \"" << JsonEscape(item.image_path) << "\",\n"
         << "  \"label\": \"" << JsonEscape(item.label) << "\",\n"
         << "  \"shape_ref\": \"" << stableRef << "\",\n"
         << "  \"annotation_shape_count\": " << annotationShapeCount << ",\n"
         << "  \"package_path\": \"" << JsonEscape(packagePath) << "\",\n"
         << "  \"checks\": {\n"
         << "    \"schema\": " << (schemaOk ? "true" : "false") << ",\n"
         << "    \"ready_status\": " << (readyOk ? "true" : "false") << ",\n"
         << "    \"image\": " << (imageOk ? "true" : "false") << ",\n"
         << "    \"label\": " << (labelOk ? "true" : "false") << ",\n"
         << "    \"mask\": " << (maskOk ? "true" : "false") << ",\n"
         << "    \"prompt\": " << (promptOk ? "true" : "false") << "\n"
         << "  }\n"
         << "}\n";
  std::string writeReason;
  const std::filesystem::path reportPath =
      outDir / "torch_training_dataset_smoke.json";
  if (!WriteTextFile(reportPath, report.str())) {
    reason = "cannot write smoke report: " + reportPath.string();
    return false;
  }

  reason = std::string(pass ? "TRAINING_DATASET_EXPORT_PASS_TO_VERIFY"
                            : "TRAINING_DATASET_EXPORT_FAIL") +
           " package=" + packagePath + " report=" + reportPath.string();
  CXLOG_INFO("TorchTrainingImageSet", "training_dataset_smoke_end",
             pass ? "TRAINING_DATASET_EXPORT_PASS_TO_VERIFY" : "FAIL", reason);
  return pass;
}

static std::string SanitizeRunTokenLocal(std::string value) {
  if (value.empty())
    value = "run";
  for (char &ch : value) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    if (!std::isalnum(uch) && ch != '-' && ch != '_')
      ch = '_';
  }
  return value;
}

static int FileNodeIntLocal(const cv::FileNode &node, const char *key,
                            int fallback = 0) {
  const cv::FileNode value = node[key];
  return value.empty() ? fallback : static_cast<int>(value);
}

static bool WriteGuiGeometryAugRuntimePlanLocal(
    const std::filesystem::path &templatePlanPath,
    const std::filesystem::path &planPath,
    bool includeBrightness,
    bool includeLocalGap,
    bool includeJaggedCut,
    bool includeLineBreak,
    float trainBrightness,
    float testBrightness,
    int gapWidth,
    int gapHeight,
    int jaggedPx,
    int lineBreakPx,
    int requestedEpochs,
    float requestedLearningRate,
    int affineChannelMode,
    float rotationTrainPositiveDeg,
    float rotationTrainNegativeDeg,
    float rotationValidationDeg,
    float rotationHoldoutDeg,
    float scaleTrainDown,
    float scaleTrainUp,
    float scaleValidation,
    float scaleHoldout,
    float compoundValidationScale,
    float compoundValidationRotationDeg,
    float compoundHoldoutScale,
    float compoundHoldoutRotationDeg,
    int &variantCount,
    std::string &reason) {
  variantCount = 0;
  cv::FileStorage templatePlan;
  try {
    if (!templatePlan.open(templatePlanPath.string(),
                           cv::FileStorage::READ |
                               cv::FileStorage::FORMAT_JSON)) {
      reason = "cannot open augmentation plan template: " +
               templatePlanPath.string();
      return false;
    }
  } catch (const cv::Exception &ex) {
    reason = "augmentation plan template parse exception: " +
             std::string(ex.what());
    return false;
  }

  const cv::FileNode root = templatePlan.root();
  if (FileNodeStringLocal(root, "schema") !=
          "cxvision.geometry_augmentation_plan.v1" ||
      !root["variants"].isSeq()) {
    reason = "unsupported augmentation plan template schema: " +
             templatePlanPath.string();
    return false;
  }

  std::ostringstream out;
  out << "{\n"
      << "  \"schema\": \"cxvision.geometry_augmentation_plan.v1\",\n"
      << "  \"training_enabled\": 0,\n"
      << "  \"human_review_required\": 1,\n"
      << "  \"source_template\": \"" << JsonEscape(templatePlanPath.string())
      << "\",\n"
      << "  \"split_policy\": "
         "\"gui_adjustable_train_validation_variants_for_yolov8n_"
         "incremental_prep\",\n"
      << "  \"ui_parameters\": {\n"
      << "    \"primary_model_family\": \"YOLOv8-n_detection\",\n"
      << "    \"python_training_in_process\": false,\n"
      << "    \"include_brightness\": "
      << (includeBrightness ? "true" : "false") << ",\n"
      << "    \"include_local_gap\": "
      << (includeLocalGap ? "true" : "false") << ",\n"
      << "    \"include_jagged_cut\": "
      << (includeJaggedCut ? "true" : "false") << ",\n"
      << "    \"include_line_break\": "
      << (includeLineBreak ? "true" : "false") << ",\n"
      << "    \"train_brightness_scale\": " << trainBrightness << ",\n"
      << "    \"validation_brightness_scale\": " << testBrightness << ",\n"
      << "    \"gap_width_px\": " << gapWidth << ",\n"
      << "    \"gap_height_px\": " << gapHeight << ",\n"
      << "    \"jagged_px\": " << jaggedPx << ",\n"
      << "    \"line_break_px\": " << lineBreakPx << ",\n"
      << "    \"requested_epochs\": " << requestedEpochs << ",\n"
      << "    \"requested_learning_rate\": " << requestedLearningRate << ",\n"
      << "    \"affine_training_channel\": \""
      << (affineChannelMode == 0 ? "rotation" : affineChannelMode == 1 ? "scale" : "compound") << "\",\n"
      << "    \"compound_policy\": \"evaluation_only_not_trainable\"\n"
      << "  },\n"
      << "  \"variants\": [\n";

  bool firstVariant = true;
  for (const cv::FileNode &variant : root["variants"]) {
    const std::string id = FileNodeStringLocal(variant, "id");
    const std::string suffix = FileNodeStringLocal(variant, "review_suffix");
    const std::string split = FileNodeStringLocal(variant, "split");
    const int seed = FileNodeIntLocal(variant, "seed", 0);
    const cv::FileNode operations = variant["operations"];
    if (id.empty() || suffix.empty() || split.empty() || !operations.isSeq())
      continue;
    const bool rotationChannel = id.rfind("rotation_", 0) == 0;
    const bool scaleChannel = id.rfind("scale_", 0) == 0;
    const bool compoundChannel = id.rfind("compound_", 0) == 0;
    const bool affineVariant = rotationChannel || scaleChannel || compoundChannel;
    if (affineVariant &&
        !((affineChannelMode == 0 && (rotationChannel || compoundChannel)) ||
          (affineChannelMode == 1 && (scaleChannel || compoundChannel)) ||
          (affineChannelMode == 2 && compoundChannel)))
      continue;

    std::ostringstream opsOut;
    bool firstOp = true;
    int opCount = 0;
    for (const cv::FileNode &op : operations) {
      const std::string type = FileNodeStringLocal(op, "type");
      if (type.empty())
        continue;
      if (type == "brightness_scale" && !includeBrightness)
        continue;
      if (type == "local_gap" && !includeLocalGap)
        continue;
      if (type == "edge_jagged_cut" && !includeJaggedCut)
        continue;
      if (type == "line_break" && !includeLineBreak)
        continue;

      if (!firstOp)
        opsOut << ", ";
      firstOp = false;
      ++opCount;

      opsOut << "{\"type\": \"" << JsonEscape(type) << "\"";
      if (type == "gaussian_blur") {
        opsOut << ", \"kernel\": " << FileNodeIntLocal(op, "kernel", 3)
               << ", \"sigma\": " << FileNodeDoubleLocal(op, "sigma", 1.0);
      } else if (type == "sensor_noise") {
        opsOut << ", \"sigma\": " << FileNodeDoubleLocal(op, "sigma", 4.0);
      } else if (type == "rotate") {
        double angle = FileNodeDoubleLocal(op, "angle_deg", 0.0);
        if (rotationChannel) {
          if (split == "train")
            angle = id.find("neg") != std::string::npos
                        ? rotationTrainNegativeDeg : rotationTrainPositiveDeg;
          else if (split == "validation") angle = rotationValidationDeg;
          else if (split == "holdout") angle = rotationHoldoutDeg;
        } else if (compoundChannel) {
          angle = split == "holdout" ? compoundHoldoutRotationDeg
                                      : compoundValidationRotationDeg;
        }
        opsOut << ", \"angle_deg\": " << angle;
      } else if (type == "translate_y") {
        opsOut << ", \"offset_y_px\": "
               << FileNodeDoubleLocal(op, "offset_y_px", 0.0);
      } else if (type == "brightness_scale") {
        const bool validationSplit =
            split == "validation" || split == "val" || split == "test";
        opsOut << ", \"scale\": "
               << (validationSplit ? testBrightness : trainBrightness)
               << ", \"offset\": " << FileNodeDoubleLocal(op, "offset", 0.0);
      } else if (type == "local_gap") {
        const bool validationSplit =
            split == "validation" || split == "val" || split == "test";
        opsOut << ", \"width_px\": "
               << (validationSplit ? gapWidth + 4 : gapWidth)
               << ", \"height_px\": "
               << (validationSplit ? gapHeight + 4 : gapHeight)
               << ", \"count\": " << FileNodeIntLocal(op, "count", 1);
      } else if (type == "edge_jagged_cut") {
        const bool validationSplit =
            split == "validation" || split == "val" || split == "test";
        opsOut << ", \"width_px\": "
               << (validationSplit ? gapWidth + 6 : gapWidth)
               << ", \"height_px\": "
               << (validationSplit ? gapHeight + 6 : gapHeight)
               << ", \"count\": " << FileNodeIntLocal(op, "count", 1)
               << ", \"jagged_px\": "
               << (validationSplit ? jaggedPx + 1 : jaggedPx);
      } else if (type == "line_break") {
        const bool validationSplit =
            split == "validation" || split == "val" || split == "test";
        const int px = validationSplit ? lineBreakPx + 4 : lineBreakPx;
        opsOut << ", \"width_px\": " << px << ", \"height_px\": " << px
               << ", \"count\": " << FileNodeIntLocal(op, "count", 1);
      } else if (type == "scale_scene") {
        double scale = FileNodeDoubleLocal(op, "scale", 1.0);
        if (scaleChannel) {
          if (split == "train")
            scale = id.find("down") != std::string::npos ? scaleTrainDown
                                                            : scaleTrainUp;
          else if (split == "validation") scale = scaleValidation;
          else if (split == "holdout") scale = scaleHoldout;
        } else if (compoundChannel) {
          scale = split == "holdout" ? compoundHoldoutScale
                                      : compoundValidationScale;
        }
        opsOut << ", \"scale\": " << scale;
      }
      opsOut << "}";
    }
    if (opCount <= 0)
      continue;

    if (!firstVariant)
      out << ",\n";
    firstVariant = false;
    ++variantCount;
    out << "    {\n"
        << "      \"id\": \"" << JsonEscape(id) << "\",\n"
        << "      \"review_suffix\": \"" << JsonEscape(suffix) << "\",\n"
        << "      \"split\": \"" << JsonEscape(split) << "\",\n"
        << "      \"seed\": " << seed << ",\n"
        << "      \"operations\": [" << opsOut.str() << "]\n"
        << "    }";
  }

  out << "\n  ]\n}\n";
  if (variantCount <= 0) {
    reason = "augmentation template produced no enabled variants";
    return false;
  }
  if (!WriteTextFile(planPath, out.str())) {
    reason = "failed to write runtime augmentation plan: " + planPath.string();
    return false;
  }
  return true;
}

bool ViewController::RunGeometryAugmentationTrainingPrepFromGui(
    std::string &reason) {
  reason.clear();
  const bool anyAugmentation =
      m_manualTest.geometry_aug_include_brightness ||
      m_manualTest.geometry_aug_include_local_gap ||
      m_manualTest.geometry_aug_include_jagged_cut ||
      m_manualTest.geometry_aug_include_line_break ||
      m_manualTest.yolo_training_geometry_roi_head_enabled;
  if (!anyAugmentation) {
    reason = "select at least one augmentation family";
    m_manualTest.geometry_aug_run_status = "AUGMENTATION_PLAN_EMPTY";
    m_manualTest.geometry_aug_run_reason = reason;
    return false;
  }

  const std::filesystem::path referenceIndex = ResolveWorkspaceFile(
      m_manualTest.yolo_training_geometry_roi_head_enabled
          ? "cxparser/cxscript/module/cximage/evidence/04_Incremental_Reliability/02_Automatic_Diagnostic_Closure/geometry_reference_cases/boundary_seven_class_reference_index_v3.json"
          : "cxparser/cxscript/module/cximage/evidence/04_Incremental_Reliability/02_Automatic_Diagnostic_Closure/geometry_reference_cases/index.json");
  std::error_code ec;
  if (!std::filesystem::is_regular_file(referenceIndex, ec)) {
    reason = "geometry reference index missing: " + referenceIndex.string();
    m_manualTest.geometry_aug_run_status = "ASSET_MISSING";
    m_manualTest.geometry_aug_run_reason = reason;
    return false;
  }
  const std::filesystem::path augmentationPlanTemplate = ResolveWorkspaceFile(
      m_manualTest.yolo_training_geometry_roi_head_enabled
          ? "cxparser/cxscript/module/cximage/evidence/04_Incremental_Reliability/02_Automatic_Diagnostic_Closure/geometry_reference_cases/boundary_seven_class_three_channel_plan_v1.json"
          : "cxparser/cxscript/module/cximage/evidence/04_Incremental_Reliability/02_Automatic_Diagnostic_Closure/geometry_reference_cases/geometry_augmentation_plan.json");
  if (!std::filesystem::is_regular_file(augmentationPlanTemplate, ec)) {
    reason =
        "geometry augmentation plan template missing: " +
        augmentationPlanTemplate.string();
    m_manualTest.geometry_aug_run_status = "ASSET_MISSING";
    m_manualTest.geometry_aug_run_reason = reason;
    return false;
  }

  const float trainBrightness =
      std::clamp(m_manualTest.geometry_aug_train_brightness_scale, 0.10f,
                 1.20f);
  const float testBrightness =
      std::clamp(m_manualTest.geometry_aug_test_brightness_scale, 0.10f,
                 1.20f);
  const int gapWidth = std::clamp(m_manualTest.geometry_aug_gap_width_px, 2, 80);
  const int gapHeight =
      std::clamp(m_manualTest.geometry_aug_gap_height_px, 2, 80);
  const int jaggedPx = std::clamp(m_manualTest.geometry_aug_jagged_px, 1, 30);
  const int lineBreakPx =
      std::clamp(m_manualTest.geometry_aug_line_break_px, 2, 80);
  m_manualTest.geometry_aug_epochs =
      std::clamp(m_manualTest.geometry_aug_epochs, 1, 1000);
  m_manualTest.geometry_aug_learning_rate =
      std::clamp(m_manualTest.geometry_aug_learning_rate, 0.000001f, 1.0f);

  const std::string runId =
      "run_" + SanitizeRunTokenLocal(CurrentTimestamp()) + "_gui_aug_training";
  const std::filesystem::path outputDir =
      ResolveCxVisionRunPath("cxscript_runs/geometry_augmentation") / runId;
  const std::filesystem::path planDir =
      ResolveCxVisionRunPath("cxscript_runs/geometry_augmentation_runtime_plans") /
      runId;
  std::filesystem::create_directories(planDir, ec);
  if (ec) {
    reason = "cannot create runtime augmentation plan directory: " +
             planDir.string() + " error=" + ec.message();
    m_manualTest.geometry_aug_run_status = "PLAN_WRITE_FAIL";
    m_manualTest.geometry_aug_run_reason = reason;
    return false;
  }

  int variantCount = 0;
  const std::filesystem::path planPath =
      planDir / "geometry_augmentation_plan_runtime.json";
  if (!WriteGuiGeometryAugRuntimePlanLocal(
          augmentationPlanTemplate, planPath,
          m_manualTest.geometry_aug_include_brightness,
          m_manualTest.geometry_aug_include_local_gap,
          m_manualTest.geometry_aug_include_jagged_cut,
          m_manualTest.geometry_aug_include_line_break,
          trainBrightness, testBrightness, gapWidth, gapHeight, jaggedPx,
          lineBreakPx, m_manualTest.geometry_aug_epochs,
          m_manualTest.geometry_aug_learning_rate,
          m_manualTest.yolo_training_affine_channel_mode,
          m_manualTest.yolo_training_rotation_train_positive_deg,
          m_manualTest.yolo_training_rotation_train_negative_deg,
          m_manualTest.yolo_training_rotation_validation_deg,
          m_manualTest.yolo_training_rotation_holdout_deg,
          m_manualTest.yolo_training_scale_train_down,
          m_manualTest.yolo_training_scale_train_up,
          m_manualTest.yolo_training_scale_validation,
          m_manualTest.yolo_training_scale_holdout,
          m_manualTest.yolo_training_compound_validation_scale,
          m_manualTest.yolo_training_compound_validation_rotation_deg,
          m_manualTest.yolo_training_compound_holdout_scale,
          m_manualTest.yolo_training_compound_holdout_rotation_deg,
          variantCount, reason)) {
    m_manualTest.geometry_aug_run_status = "PLAN_WRITE_FAIL";
    m_manualTest.geometry_aug_run_reason = reason;
    return false;
  }

  CxGeometryAugmentationDatasetOptions options;
  options.reference_index_path = referenceIndex;
  options.augmentation_plan_path = planPath;
  options.output_dir = outputDir;
  options.require_source_disjoint_validation =
      m_manualTest.geometry_aug_require_source_disjoint_validation;

  CxGeometryAugmentationDatasetResult result;
  if (!RunCxGeometryAugmentationDataset(options, result, reason)) {
    m_manualTest.geometry_aug_run_status =
        result.status.empty() ? "DATASET_GENERATION_FAIL" : result.status;
    m_manualTest.geometry_aug_run_reason = reason;
    m_manualTest.geometry_aug_output_dir = outputDir.string();
    m_manualTest.geometry_aug_plan_path = planPath.string();
    return false;
  }

  const std::filesystem::path trainingRequestPath =
      outputDir / "yolov8n_incremental_training_request.json";
  std::ostringstream request;
  request << "{\n"
          << "  \"schema\": "
             "\"cxvision.yolov8n_incremental_training_request.v1\",\n"
          << "  \"status\": \"PENDING_EXTERNAL_YOLOV8N_TRAINING\",\n"
          << "  \"training_curve_status\": "
             "\"PENDING_EXTERNAL_TRAINING_CURVES\",\n"
          << "  \"incremental_training_effectiveness_status\": "
             "\"NOT_EVALUATED_NO_WEIGHT_UPDATE_EVIDENCE\",\n"
          << "  \"inference_comparison_status\": "
             "\"PENDING_BASE_AND_INCREMENTAL_INFERENCE\",\n"
          << "  \"created_at\": \"" << JsonEscape(CurrentTimestamp())
          << "\",\n"
          << "  \"primary_model_family\": \"YOLOv8-n_detection\",\n"
          << "  \"model_structure_policy\": "
             "\"reuse_base_yolov8n_without_disassembly\",\n"
          << "  \"python_training_in_process\": false,\n"
          << "  \"base_weight_ref\": \"libtorch_module/models/yolov8n.pt\",\n"
          << "  \"requested_epochs\": " << m_manualTest.geometry_aug_epochs
          << ",\n"
          << "  \"requested_learning_rate\": "
          << m_manualTest.geometry_aug_learning_rate << ",\n"
          << "  \"source_case_count\": " << result.source_case_count << ",\n"
          << "  \"variant_count\": " << variantCount << ",\n"
          << "  \"generated_sample_count\": "
          << result.generated_sample_count << ",\n"
          << "  \"rejected_sample_count\": " << result.rejected_sample_count
          << ",\n"
          << "  \"train_sample_count\": " << result.train_sample_count
          << ",\n"
          << "  \"validation_sample_count\": "
          << result.validation_sample_count << ",\n"
          << "  \"dataset_manifest_ref\": \"dataset_manifest.json\",\n"
          << "  \"runtime_augmentation_plan\": \""
          << JsonEscape(planPath.string()) << "\",\n"
          << "  \"analysis_refs\": {\n"
          << "    \"augmentation_report_json\": \"augmentation_report.json\",\n"
          << "    \"augmentation_report_md\": \"augmentation_report.md\",\n"
          << "    \"geometry_training_index\": \"geometry_training_index.json\",\n"
          << "    \"metrology_target_index\": "
             "\"metrology_target_index.json\"\n"
          << "  },\n"
          << "  \"acceptance_note\": "
             "\"GUI prepared enhanced train/validation assets and training "
             "parameters; actual YOLOv8-n incremental training and original/"
             "new model inference comparison remain pending.\"\n"
          << "}\n";
  if (!WriteTextFile(trainingRequestPath, request.str())) {
    reason = "dataset generated but training request write failed: " +
             trainingRequestPath.string();
    m_manualTest.geometry_aug_run_status = "TRAINING_REQUEST_WRITE_FAIL";
    m_manualTest.geometry_aug_run_reason = reason;
    return false;
  }

  m_manualTest.geometry_aug_run_status = "AUGMENTED_TRAINING_PREP_READY";
  m_manualTest.geometry_aug_run_reason =
      "dataset generation complete; YOLOv8-n incremental training request "
      "prepared; actual training pending external libtorch/YOLO binding";
  m_manualTest.geometry_aug_output_dir = outputDir.string();
  m_manualTest.geometry_aug_plan_path = planPath.string();
  m_manualTest.geometry_aug_dataset_manifest_path =
      result.dataset_manifest_path.string();
  m_manualTest.geometry_aug_training_request_path =
      trainingRequestPath.string();
  m_manualTest.script_evidence_groups_dirty = true;
  m_manualTest.script_evidence_row_refs_dirty = true;
  m_manualTest.torch_training_image_status = "AUGMENTED_TRAINING_PREP_READY";
  m_manualTest.torch_training_image_reason =
      "generated_sample_count=" +
      std::to_string(result.generated_sample_count) +
      " train=" + std::to_string(result.train_sample_count) +
      " validation=" + std::to_string(result.validation_sample_count) +
      " request=" + trainingRequestPath.string();
  reason = m_manualTest.geometry_aug_run_reason;
  CXLOG_INFO("TorchTrainingImageSet", "geometry_augmentation_training_prep",
             "AUGMENTED_TRAINING_PREP_READY",
             "out=" + outputDir.string() + " plan=" + planPath.string() +
                 " request=" + trainingRequestPath.string());
  return true;
}

static bool LoadYoloTrainingTuningFromPlanLocal(
    ManualTestContext &context, const std::filesystem::path &planPath,
    std::string &reason) {
  reason.clear();
  cv::FileStorage storage(planPath.string(), cv::FileStorage::READ);
  if (!storage.isOpened()) {
    reason = "YOLO training parameter asset cannot be opened: " +
             planPath.string();
    return false;
  }
  auto requireInt = [&](const char *name, int &value) {
    const cv::FileNode node = storage[name];
    if (node.empty()) {
      reason = std::string("TRAINING_PARAMETER_MISSING: ") + name;
      return false;
    }
    node >> value;
    return true;
  };
  auto requireFloat = [&](const char *name, float &value) {
    const cv::FileNode node = storage[name];
    if (node.empty()) {
      reason = std::string("TRAINING_PARAMETER_MISSING: ") + name;
      return false;
    }
    double parsed = 0.0;
    node >> parsed;
    value = static_cast<float>(parsed);
    return true;
  };
  auto optionalInt = [&](const char *name, int &value) {
    const cv::FileNode node = storage[name];
    if (!node.empty()) node >> value;
  };
  auto optionalFloat = [&](const char *name, float &value) {
    const cv::FileNode node = storage[name];
    if (!node.empty()) {
      double parsed = 0.0;
      node >> parsed;
      value = static_cast<float>(parsed);
    }
  };
  auto optionalString = [&](const char *name, std::string &value) {
    const cv::FileNode node = storage[name];
    if (!node.empty()) node >> value;
  };
  int qualityTargets = 0;
  int globalAssignment = 0;
  int restoreBest = 0;
  int classAgnosticNms = 0;
  if (!requireInt("epochs", context.geometry_aug_epochs) ||
      !requireInt("batch_size", context.yolo_training_batch_size) ||
      !requireInt("input_size", context.yolo_training_input_size) ||
      !requireInt("max_train_batches_per_epoch",
                  context.yolo_training_max_batches_per_epoch) ||
      !requireFloat("learning_rate", context.geometry_aug_learning_rate) ||
      !requireInt("seed", context.yolo_training_seed) ||
      !requireInt("assignment_topk", context.yolo_training_assignment_topk) ||
      !requireFloat("classification_focal_gamma",
                    context.yolo_training_classification_focal_gamma) ||
      !requireInt("use_assignment_quality_targets", qualityTargets) ||
      !requireInt("global_multiscale_assignment", globalAssignment) ||
      !requireInt("validation_interval",
                  context.yolo_training_validation_interval) ||
      !requireInt("early_stop_patience",
                  context.yolo_training_early_stop_patience) ||
      !requireInt("minimum_complete_epochs",
                  context.yolo_training_minimum_complete_epochs) ||
      !requireFloat("minimum_f1_delta",
                    context.yolo_training_minimum_f1_delta) ||
      !requireInt("restore_best_validation_checkpoint", restoreBest) ||
      !requireFloat("postprocess_confidence_threshold",
                    context.yolo_training_postprocess_confidence_threshold) ||
      !requireFloat("postprocess_iou_threshold",
                    context.yolo_training_postprocess_iou_threshold) ||
      !requireInt("postprocess_max_detections",
                  context.yolo_training_postprocess_max_detections) ||
      !requireInt("postprocess_class_agnostic_nms", classAgnosticNms) ||
      !requireFloat("evaluation_match_iou_threshold",
                    context.yolo_training_evaluation_match_iou_threshold)) {
    return false;
  }
  context.yolo_training_class_names.clear();
  context.yolo_training_class_loss_weights.clear();
  const cv::FileNode classNames = storage["class_names"];
  const cv::FileNode classWeights = storage["class_loss_weights"];
  if (!classNames.isSeq() || !classWeights.isSeq() ||
      classNames.size() != classWeights.size() || classNames.empty()) {
    reason = "TRAINING_PARAMETER_INVALID: class_names/class_loss_weights";
    return false;
  }
  for (const auto &node : classNames)
    context.yolo_training_class_names.push_back(static_cast<std::string>(node));
  for (const auto &node : classWeights) {
    double value = 0.0;
    node >> value;
    if (!(value > 0.0) || !std::isfinite(value)) {
      reason = "TRAINING_PARAMETER_INVALID: class_loss_weights";
      return false;
    }
    context.yolo_training_class_loss_weights.push_back(
        static_cast<float>(value));
  }
  context.yolo_training_use_assignment_quality_targets = qualityTargets != 0;
  context.yolo_training_global_multiscale_assignment = globalAssignment != 0;
  context.yolo_training_restore_best_validation_checkpoint = restoreBest != 0;
  context.yolo_training_postprocess_class_agnostic_nms =
      classAgnosticNms != 0;
  int geometryHeadEnabled = context.yolo_training_geometry_roi_head_enabled ? 1 : 0;
  optionalInt("geometry_roi_head_enabled", geometryHeadEnabled);
  context.yolo_training_geometry_roi_head_enabled = geometryHeadEnabled != 0;
  optionalInt("geometry_roi_hidden_channels",
              context.yolo_training_geometry_roi_hidden_channels);
  optionalInt("geometry_roi_pooled_size",
              context.yolo_training_geometry_roi_pooled_size);
  optionalInt("geometry_roi_polygon_vertex_count",
              context.yolo_training_geometry_roi_polygon_vertex_count);
  optionalFloat("geometry_roi_parameter_weight",
                context.yolo_training_geometry_roi_parameter_weight);
  optionalFloat("geometry_roi_contour_weight",
                context.yolo_training_geometry_roi_contour_weight);
  optionalFloat("geometry_roi_continuity_weight",
                context.yolo_training_geometry_roi_continuity_weight);
  optionalFloat("geometry_roi_uncertainty_weight",
                context.yolo_training_geometry_roi_uncertainty_weight);
  optionalFloat("geometry_roi_continuity_positive_weight",
                context.yolo_training_geometry_roi_continuity_positive_weight);
  optionalFloat("geometry_roi_calibration_weight",
                context.yolo_training_geometry_roi_calibration_weight);
  const cv::FileNode geometryTargetManifest = storage["geometry_target_manifest"];
  context.yolo_training_geometry_target_manifest = geometryTargetManifest.empty()
      ? std::string() : static_cast<std::string>(geometryTargetManifest);
  std::string affineChannel = "rotation";
  optionalString("affine_training_channel", affineChannel);
  if (affineChannel == "rotation") context.yolo_training_affine_channel_mode = 0;
  else if (affineChannel == "scale") context.yolo_training_affine_channel_mode = 1;
  else if (affineChannel == "compound") context.yolo_training_affine_channel_mode = 2;
  else {
    reason = "TRAINING_PARAMETER_INVALID: affine_training_channel";
    return false;
  }
  optionalFloat("rotation_train_positive_deg", context.yolo_training_rotation_train_positive_deg);
  optionalFloat("rotation_train_negative_deg", context.yolo_training_rotation_train_negative_deg);
  optionalFloat("rotation_validation_deg", context.yolo_training_rotation_validation_deg);
  optionalFloat("rotation_holdout_deg", context.yolo_training_rotation_holdout_deg);
  optionalFloat("scale_train_down", context.yolo_training_scale_train_down);
  optionalFloat("scale_train_up", context.yolo_training_scale_train_up);
  optionalFloat("scale_validation", context.yolo_training_scale_validation);
  optionalFloat("scale_holdout", context.yolo_training_scale_holdout);
  optionalFloat("compound_validation_scale", context.yolo_training_compound_validation_scale);
  optionalFloat("compound_validation_rotation_deg", context.yolo_training_compound_validation_rotation_deg);
  optionalFloat("compound_holdout_scale", context.yolo_training_compound_holdout_scale);
  optionalFloat("compound_holdout_rotation_deg", context.yolo_training_compound_holdout_rotation_deg);
  context.yolo_training_tuning_loaded = true;
  context.yolo_training_tuning_source = planPath.string();
  reason = "YOLO training parameters loaded from versioned plan asset";
  return true;
}

static void DrawYoloTrainingTuningControlsLocal(
    ManualTestContext &context, const std::filesystem::path &planPath,
    const char *id) {
  ImGui::PushID(id);
  if (!context.yolo_training_tuning_loaded) {
    std::string loadReason;
    LoadYoloTrainingTuningFromPlanLocal(context, planPath, loadReason);
  }
  if (!ImGui::CollapsingHeader("YOLO Loss / Assignment / Validation Controls",
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::PopID();
    return;
  }
  ImGui::TextWrapped("Defaults: %s",
                     context.yolo_training_tuning_source.empty()
                         ? "ASSET_MISSING"
                         : context.yolo_training_tuning_source.c_str());
  ImGui::TextDisabled(
      "Edits are runtime-plan drafts. Run exports every displayed value and the result receipt echoes the effective configuration.");
  if (ImGui::Button("Reload Defaults From Plan Asset")) {
    std::string loadReason;
    if (!LoadYoloTrainingTuningFromPlanLocal(context, planPath, loadReason)) {
      context.geometry_aug_run_status = "TRAINING_PARAMETER_ASSET_FAIL";
      context.geometry_aug_run_reason = loadReason;
    }
  }

  auto inputInt = [](const char *label, const char *fieldId, int &value) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputInt(fieldId, &value);
  };
  auto inputFloat = [](const char *label, const char *fieldId, float &value,
                       const char *format) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputFloat(fieldId, &value, 0.0f, 0.0f, format);
  };
  auto inputBool = [](const char *label, const char *fieldId, bool &value) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::Checkbox(fieldId, &value);
  };
  if (ImGui::BeginTable("training_tuning", 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Parameter", ImGuiTableColumnFlags_WidthStretch,
                            1.7f);
    ImGui::TableSetupColumn("Runtime plan value",
                            ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableHeadersRow();
    inputInt("epochs", "##epochs", context.geometry_aug_epochs);
    inputInt("batch_size", "##batch_size", context.yolo_training_batch_size);
    inputInt("input_size", "##input_size", context.yolo_training_input_size);
    inputInt("max_train_batches_per_epoch", "##max_batches",
             context.yolo_training_max_batches_per_epoch);
    inputFloat("learning_rate", "##learning_rate",
               context.geometry_aug_learning_rate, "%.8f");
    inputInt("seed", "##seed", context.yolo_training_seed);
    inputInt("assignment_topk", "##assignment_topk",
             context.yolo_training_assignment_topk);
    inputFloat("classification_focal_gamma", "##focal_gamma",
               context.yolo_training_classification_focal_gamma, "%.4f");
    inputBool("use_assignment_quality_targets", "##quality_targets",
              context.yolo_training_use_assignment_quality_targets);
    inputBool("global_multiscale_assignment", "##global_assignment",
              context.yolo_training_global_multiscale_assignment);
    for (std::size_t index = 0;
         index < context.yolo_training_class_loss_weights.size(); ++index) {
      const std::string label =
          "class_loss_weight[" + std::to_string(index) + "] " +
          (index < context.yolo_training_class_names.size()
               ? context.yolo_training_class_names[index]
               : std::string("ASSET_NAME_MISSING"));
      const std::string field = "##class_weight_" + std::to_string(index);
      inputFloat(label.c_str(), field.c_str(),
                 context.yolo_training_class_loss_weights[index], "%.4f");
    }
    inputInt("validation_interval", "##validation_interval",
             context.yolo_training_validation_interval);
    inputInt("early_stop_patience", "##early_stop_patience",
             context.yolo_training_early_stop_patience);
    inputInt("minimum_complete_epochs", "##minimum_complete_epochs",
             context.yolo_training_minimum_complete_epochs);
    inputFloat("minimum_f1_delta", "##minimum_f1_delta",
               context.yolo_training_minimum_f1_delta, "%.6f");
    inputBool("restore_best_validation_checkpoint", "##restore_best",
              context.yolo_training_restore_best_validation_checkpoint);
    inputFloat("postprocess_confidence_threshold", "##confidence_threshold",
               context.yolo_training_postprocess_confidence_threshold,
               "%.4f");
    inputFloat("postprocess_iou_threshold", "##iou_threshold",
               context.yolo_training_postprocess_iou_threshold, "%.4f");
    inputInt("postprocess_max_detections", "##max_detections",
             context.yolo_training_postprocess_max_detections);
    inputBool("postprocess_class_agnostic_nms", "##class_agnostic_nms",
              context.yolo_training_postprocess_class_agnostic_nms);
    inputFloat("evaluation_match_iou_threshold", "##evaluation_match_iou",
               context.yolo_training_evaluation_match_iou_threshold, "%.4f");
    ImGui::EndTable();
  }
  if (ImGui::CollapsingHeader("Geometry ROI Head / Case-External Controls",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextDisabled(
        "ROI-sidecar supervision only. BBox labels never synthesize geometry targets.");
    ImGui::Checkbox("geometry_roi_head_enabled##geometry_roi",
                    &context.yolo_training_geometry_roi_head_enabled);
    ImGui::SetNextItemWidth(-1.0f);
    InputTextString("geometry_target_manifest##geometry_roi",
                    context.yolo_training_geometry_target_manifest);
    if (ImGui::BeginTable("geometry_roi_tuning", 2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("Geometry parameter", ImGuiTableColumnFlags_WidthStretch, 1.7f);
      ImGui::TableSetupColumn("Runtime plan value", ImGuiTableColumnFlags_WidthStretch, 1.0f);
      ImGui::TableHeadersRow();
      inputInt("geometry_roi_hidden_channels", "##geometry_roi_hidden",
               context.yolo_training_geometry_roi_hidden_channels);
      inputInt("geometry_roi_pooled_size", "##geometry_roi_pool",
               context.yolo_training_geometry_roi_pooled_size);
      inputInt("geometry_roi_polygon_vertex_count", "##geometry_roi_vertices",
               context.yolo_training_geometry_roi_polygon_vertex_count);
      inputFloat("geometry_roi_parameter_weight", "##geometry_roi_parameter_weight",
                 context.yolo_training_geometry_roi_parameter_weight, "%.4f");
      inputFloat("geometry_roi_contour_weight", "##geometry_roi_contour_weight",
                 context.yolo_training_geometry_roi_contour_weight, "%.4f");
      inputFloat("geometry_roi_continuity_weight", "##geometry_roi_continuity_weight",
                 context.yolo_training_geometry_roi_continuity_weight, "%.4f");
      inputFloat("geometry_roi_uncertainty_weight", "##geometry_roi_uncertainty_weight",
                 context.yolo_training_geometry_roi_uncertainty_weight, "%.4f");
      inputFloat("geometry_roi_continuity_positive_weight", "##geometry_roi_positive_weight",
                 context.yolo_training_geometry_roi_continuity_positive_weight, "%.4f");
      inputFloat("geometry_roi_calibration_weight", "##geometry_roi_calibration_weight",
                 context.yolo_training_geometry_roi_calibration_weight, "%.4f");
      ImGui::EndTable();
    }
  }
  if (ImGui::CollapsingHeader("Affine Training Channels / Dataset Contract",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    static const char *channelNames[] = {
        "Rotation only (trainable)", "Scale only (trainable)",
        "Compound scale + rotation (evaluation only)"};
    ImGui::TextDisabled(
        "These values define the dataset-generation contract. Change them, regenerate the sidecar dataset, then train.");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::Combo("affine_training_channel##affine_channel",
                 &context.yolo_training_affine_channel_mode, channelNames,
                 IM_ARRAYSIZE(channelNames));
    if (context.yolo_training_affine_channel_mode == 2)
      ImGui::TextColored(ImVec4(0.90f, 0.70f, 0.10f, 1.0f),
                         "COMPOUND_EVALUATION_ONLY: this channel must not enter training.");
    if (ImGui::BeginTable("affine_channel_tuning", 2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_SizingStretchProp)) {
      ImGui::TableSetupColumn("Dataset contract parameter",
                              ImGuiTableColumnFlags_WidthStretch, 1.7f);
      ImGui::TableSetupColumn("Requested value",
                              ImGuiTableColumnFlags_WidthStretch, 1.0f);
      ImGui::TableHeadersRow();
      inputFloat("rotation_train_positive_deg", "##rotation_train_positive",
                 context.yolo_training_rotation_train_positive_deg, "%.2f");
      inputFloat("rotation_train_negative_deg", "##rotation_train_negative",
                 context.yolo_training_rotation_train_negative_deg, "%.2f");
      inputFloat("rotation_validation_deg", "##rotation_validation",
                 context.yolo_training_rotation_validation_deg, "%.2f");
      inputFloat("rotation_holdout_deg", "##rotation_holdout",
                 context.yolo_training_rotation_holdout_deg, "%.2f");
      inputFloat("scale_train_down", "##scale_train_down",
                 context.yolo_training_scale_train_down, "%.3f");
      inputFloat("scale_train_up", "##scale_train_up",
                 context.yolo_training_scale_train_up, "%.3f");
      inputFloat("scale_validation", "##scale_validation",
                 context.yolo_training_scale_validation, "%.3f");
      inputFloat("scale_holdout", "##scale_holdout",
                 context.yolo_training_scale_holdout, "%.3f");
      inputFloat("compound_validation_scale", "##compound_validation_scale",
                 context.yolo_training_compound_validation_scale, "%.3f");
      inputFloat("compound_validation_rotation_deg", "##compound_validation_rotation",
                 context.yolo_training_compound_validation_rotation_deg, "%.2f");
      inputFloat("compound_holdout_scale", "##compound_holdout_scale",
                 context.yolo_training_compound_holdout_scale, "%.3f");
      inputFloat("compound_holdout_rotation_deg", "##compound_holdout_rotation",
                 context.yolo_training_compound_holdout_rotation_deg, "%.2f");
      ImGui::EndTable();
    }
  }
  ImGui::PopID();
}

static void DrawYoloTrainingFeedbackLocal(
    const CxTorchTrainingRunBinding &run, const char *id) {
  if (!run.available)
    return;
  ImGui::PushID(id);
  if (ImGui::CollapsingHeader("Effective Training / Checkpoint Feedback",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("epochs: %d/%d | early_stop: %s | best: epoch %d, F1 %.6f",
                run.completed_epochs, run.configured_epochs,
                run.early_stopped ? "yes" : "no", run.best_validation_epoch,
                run.best_validation_f1);
    ImGui::Text(
        "assignment_topk: %d | focal_gamma: %.4f | quality_targets: %s",
        run.assignment_topk, run.classification_focal_gamma,
        run.use_assignment_quality_targets ? "on" : "off");
    ImGui::Text(
        "postprocess: conf %.4f | IoU %.4f | max %d | class-agnostic NMS %s",
        run.postprocess_confidence_threshold, run.postprocess_iou_threshold,
        run.postprocess_max_detections,
        run.postprocess_class_agnostic_nms ? "on" : "off");
    if (!run.checkpoint_selection_path.empty())
      ImGui::TextWrapped("checkpoint selection: %s",
                         run.checkpoint_selection_path.c_str());
  }
  ImGui::PopID();
}

bool ViewController::RunYoloV8nSelectedImageInferenceFromGui(
    std::string &reason) {
  reason.clear();
  if (m_manualTest.selected_torch_training_image < 0 ||
      m_manualTest.selected_torch_training_image >=
          static_cast<int>(m_manualTest.torch_training_images.size())) {
    reason = "select one image from the Train, Validation, or Holdout rail";
    m_manualTest.torch_training_inference_status = "INFERENCE_IMAGE_REQUIRED";
    m_manualTest.torch_training_inference_reason = reason;
    return false;
  }

  const TorchTrainingImageItem &selected =
      m_manualTest.torch_training_images[static_cast<std::size_t>(
          m_manualTest.selected_torch_training_image)];
  const std::filesystem::path imagePath =
      ResolveWorkspaceFile(selected.image_path);
  const std::filesystem::path manifestPath = ResolveWorkspaceFile(
      m_manualTest.torch_training_inference_model_manifest);
  std::error_code ec;
  if (!std::filesystem::is_regular_file(imagePath, ec) || ec) {
    reason = "selected image is missing or unreadable: " + imagePath.string();
    m_manualTest.torch_training_inference_status = "INFERENCE_IMAGE_MISSING";
    m_manualTest.torch_training_inference_reason = reason;
    return false;
  }
  ec.clear();
  if (!std::filesystem::is_regular_file(manifestPath, ec) || ec) {
    reason = "model manifest is missing or unreadable: " +
             manifestPath.string();
    m_manualTest.torch_training_inference_status = "MODEL_MANIFEST_REQUIRED";
    m_manualTest.torch_training_inference_reason = reason;
    return false;
  }

  std::string requestedDevice =
      m_manualTest.torch_training_inference_device;
  std::transform(requestedDevice.begin(), requestedDevice.end(),
                 requestedDevice.begin(), [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  if (requestedDevice != "cpu" && requestedDevice != "cuda" &&
      requestedDevice != "auto") {
    reason = "inference device must be cpu, cuda, or auto";
    m_manualTest.torch_training_inference_status =
        "INFERENCE_DEVICE_INVALID";
    m_manualTest.torch_training_inference_reason = reason;
    return false;
  }

  const auto tick = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
  const std::string sessionId = CxUnifiedLog::Instance().RunId().empty()
                                    ? "ui_session"
                                    : CxUnifiedLog::Instance().RunId();
  const std::filesystem::path outputDir =
      ResolveCxVisionRunPath("cxscript_runs/manual_yolov8_detection") /
      sessionId /
      (imagePath.stem().string() + "_" + std::to_string(tick));

  CxTorchTaskSpec task;
  task.kind = CxTorchTaskKind::Detection;
  task.task_id = "torch.infer.detection.yolov8.v1";
  task.case_id = selected.case_id.empty() ? selected.image_id : selected.case_id;
  task.manifest_path = manifestPath;
  task.input_image_path = imagePath;
  task.output_dir = outputDir;
  task.requested_device = requestedDevice;
  task.timeout_ms = 30000;

  CxInferenceResult inference;
  CxTorchExecutionAdapter adapter;
  if (!adapter.Execute(task, inference, reason)) {
    m_manualTest.torch_training_inference_status =
        inference.failure_stage.empty() ? "YOLO_INFERENCE_FAILED"
                                        : inference.failure_stage;
    m_manualTest.torch_training_inference_reason =
        inference.reason.empty() ? reason : inference.reason;
    CXLOG_ERROR("TorchTrainingImageSet", "selected_image_yolo_inference",
                "FAIL", m_manualTest.torch_training_inference_reason);
    return false;
  }

  m_manualTest.torch_training_inference_status =
      inference.status.empty() ? "YOLO_INFERENCE_COMPLETE" : inference.status;
  m_manualTest.torch_training_inference_reason =
      "selected source image was inferred; overlay is displayed in Image View";
  m_manualTest.torch_training_inference_result_ref = inference.result_ref;
  m_manualTest.torch_training_inference_overlay_ref =
      inference.primary_visual_ref;
  m_manualTest.torch_training_inference_candidates_ref =
      inference.bbox_candidate_list_ref;
  m_manualTest.torch_training_inference_runtime_ms = inference.infer_runtime_ms;
  m_manualTest.torch_training_inference_detection_count =
      static_cast<int>(inference.detections.size());

  if (!inference.primary_visual_ref.empty()) {
    std::string loadReason;
    if (!LoadImageIntoImageView(inference.primary_visual_ref, loadReason)) {
      reason = "inference succeeded but overlay could not be displayed: " +
               loadReason;
      m_manualTest.torch_training_inference_status =
          "YOLO_INFERENCE_OVERLAY_LOAD_FAILED";
      m_manualTest.torch_training_inference_reason = reason;
      return false;
    }
  }

  CXLOG_INFO(
      "TorchTrainingImageSet", "selected_image_yolo_inference", "complete",
      "image_path=" + imagePath.string() +
          " manifest=" + manifestPath.string() +
          " detections=" +
          std::to_string(m_manualTest.torch_training_inference_detection_count) +
          " runtime_ms=" +
          std::to_string(m_manualTest.torch_training_inference_runtime_ms) +
          " result=" + m_manualTest.torch_training_inference_result_ref +
          " overlay=" + m_manualTest.torch_training_inference_overlay_ref);
  return true;
}

bool ViewController::RunYoloV8nIncrementalTrainingFromGui(std::string &reason) {
  reason.clear();
  if (m_manualTest.torch_training_process_running) {
    reason = "C++ incremental training is already running";
    return false;
  }
  bool trainingAssetMatched = false;
  const bool trainingAssetEnabled = EvidenceSelectionTrainingEnabledLocal(
      m_manualTest.current_evidence_selection, trainingAssetMatched);
  if (trainingAssetMatched && !trainingAssetEnabled) {
    reason = "TRAINING_BLOCKED_BY_ASSET_POLICY: selected Evidence case is "
             "reference-only (training_enabled=0)";
    m_manualTest.geometry_aug_run_status =
        "TRAINING_BLOCKED_BY_ASSET_POLICY";
    m_manualTest.geometry_aug_run_reason = reason;
    m_manualTest.torch_training_image_status =
        "TRAINING_BLOCKED_BY_ASSET_POLICY";
    m_manualTest.torch_training_image_reason = reason;
    CXLOG_WARN("TorchTrainingImageSet", "incremental_training_blocked",
               "TRAINING_BLOCKED_BY_ASSET_POLICY", reason);
    return false;
  }
  const int selectedParentIndex = m_manualTest.selected_model_lineage_node;
  const CxModelLineageUiNode *selectedParent =
      selectedParentIndex >= 0 && selectedParentIndex <
              static_cast<int>(m_manualTest.model_lineage_nodes.size())
          ? &m_manualTest.model_lineage_nodes[static_cast<std::size_t>(
                selectedParentIndex)]
          : nullptr;
  if (selectedParent == nullptr || selectedParent->node_kind != "release" ||
      !selectedParent->parent_selection_reviewed) {
    reason = "PARENT_MODEL_SELECTION_REQUIRED: select a human-confirmed release node before creating an incremental training plan";
    m_manualTest.geometry_aug_run_status = "PARENT_MODEL_SELECTION_REQUIRED";
    m_manualTest.geometry_aug_run_reason = reason;
    return false;
  }
  std::error_code parentError;
  if (selectedParent->checkpoint_path.empty() ||
      !std::filesystem::is_regular_file(selectedParent->checkpoint_path,
                                        parentError) ||
      parentError) {
    reason = "PARENT_CHECKPOINT_ASSET_MISSING";
    m_manualTest.geometry_aug_run_status = reason;
    m_manualTest.geometry_aug_run_reason = reason;
    return false;
  }
  const std::filesystem::path workflowRoot =
      ResolveCxVisionRunPath("cxscript_runs/yolov8n_incremental");
  // Training must consume an asset explicitly owned by the current Evidence
  // selection.  Looking up the globally newest package can silently train a
  // different case after another run has written into the shared run root.
  std::vector<std::filesystem::path> packageCandidates;
  auto addPackageCandidate = [&](const std::string &reference) {
    if (reference.empty())
      return;
    const std::filesystem::path resolved = ResolveWorkspaceFile(reference);
    std::error_code candidateError;
    if (std::filesystem::is_regular_file(resolved, candidateError) &&
        !candidateError) {
      if (resolved.filename() == "package_manifest.json")
        packageCandidates.push_back(resolved);
      else
        packageCandidates.push_back(resolved.parent_path() /
                                    "package_manifest.json");
      return;
    }
    candidateError.clear();
    if (std::filesystem::is_directory(resolved, candidateError) &&
        !candidateError)
      packageCandidates.push_back(resolved / "package_manifest.json");
  };
  addPackageCandidate(
      m_manualTest.current_evidence_selection.dataset_summary_ref);
  addPackageCandidate(
      m_manualTest.current_evidence_selection.evidence_output_root);
  addPackageCandidate(m_manualTest.current_evidence_selection.candidate_dir);
  addPackageCandidate(
      m_manualTest.current_evidence_selection.source_evidence_script_path);
  addPackageCandidate(m_manualTest.current_evidence_selection.image_path);

  std::filesystem::path packageManifest;
  std::set<std::string> checkedPackagePaths;
  for (const std::filesystem::path &candidate : packageCandidates) {
    const std::filesystem::path normalized = candidate.lexically_normal();
    if (!checkedPackagePaths.insert(normalized.generic_string()).second)
      continue;
    std::error_code candidateError;
    if (std::filesystem::is_regular_file(normalized, candidateError) &&
        !candidateError) {
      packageManifest = normalized;
      break;
    }
  }
  const std::filesystem::path trainingPlanTemplate = ResolveWorkspaceFile(
      "cxparser/cxscript/module/cximage/tests/"
      "yolov8n_cpp_detection_full_coverage_plan.json");
  std::error_code ec;
  if (packageManifest.empty() ||
      !std::filesystem::is_regular_file(trainingPlanTemplate, ec) || ec) {
    reason = packageManifest.empty()
                 ? "CURRENT_CASE_DATASET_PACKAGE_REQUIRED: the selected "
                   "Evidence case does not explicitly reference or contain "
                   "package_manifest.json"
                 : "C++ training plan template is missing";
    m_manualTest.geometry_aug_run_status =
        packageManifest.empty() ? "CURRENT_CASE_DATASET_PACKAGE_REQUIRED"
                                : "ASSET_PREFLIGHT_FAIL";
    m_manualTest.geometry_aug_run_reason = reason;
    return false;
  }
  cv::FileStorage packageStorage(packageManifest.string(),
                                 cv::FileStorage::READ);
  const std::string packageSchema = packageStorage.isOpened()
                                        ? static_cast<std::string>(
                                              packageStorage["schema"])
                                        : std::string{};
  int packageTrainingEnabled = 0;
  if (packageStorage.isOpened())
    packageStorage["training_enabled"] >> packageTrainingEnabled;
  if (!packageStorage.isOpened() ||
      (packageSchema != "cxvision.yolov8n_aabb_package.v1" &&
       packageSchema != "cxvision.yolov8n_aabb_mask_package.v1")) {
    reason = "CURRENT_CASE_DATASET_PACKAGE_INVALID: unsupported or unreadable "
             "package_manifest.json";
    m_manualTest.geometry_aug_run_status =
        "CURRENT_CASE_DATASET_PACKAGE_INVALID";
    m_manualTest.geometry_aug_run_reason = reason;
    return false;
  }
  if (packageTrainingEnabled == 0) {
    reason = "TRAINING_BLOCKED_BY_PACKAGE_POLICY: current-case "
             "package_manifest.json has training_enabled=0";
    m_manualTest.geometry_aug_run_status =
        "TRAINING_BLOCKED_BY_PACKAGE_POLICY";
    m_manualTest.geometry_aug_run_reason = reason;
    return false;
  }

  wchar_t executableBuffer[MAX_PATH]{};
  const DWORD executableLength =
      GetModuleFileNameW(nullptr, executableBuffer, MAX_PATH);
  if (executableLength == 0 || executableLength >= MAX_PATH) {
    reason = "cannot resolve current GUI executable";
    m_manualTest.geometry_aug_run_status = "ASSET_PREFLIGHT_FAIL";
    m_manualTest.geometry_aug_run_reason = reason;
    return false;
  }
  const std::filesystem::path executable(executableBuffer);
  const std::filesystem::path runtimeDll =
      executable.parent_path() / "libtorch_module_runtime.dll";
  if (!std::filesystem::is_regular_file(runtimeDll, ec) || ec) {
    reason = "Torch runtime DLL is missing beside the GUI executable";
    m_manualTest.geometry_aug_run_status = "ASSET_PREFLIGHT_FAIL";
    m_manualTest.geometry_aug_run_reason = reason;
    return false;
  }

  const std::string runId =
      "run_" + SanitizeRunTokenLocal(CurrentTimestamp()) +
      "_gui_cxx_incremental_train";
  const std::filesystem::path outputDir = workflowRoot / runId;
  const std::filesystem::path planDir = ResolveCxVisionRunPath(
      "cxscript_runs/yolov8n_training_runtime_plans") / runId;
  std::filesystem::create_directories(planDir, ec);
  if (ec) {
    reason = "cannot create C++ training plan directory: " + ec.message();
    return false;
  }
  if (!m_manualTest.yolo_training_tuning_loaded &&
      !LoadYoloTrainingTuningFromPlanLocal(
          m_manualTest, trainingPlanTemplate, reason)) {
    m_manualTest.geometry_aug_run_status = "TRAINING_PARAMETER_ASSET_FAIL";
    m_manualTest.geometry_aug_run_reason = reason;
    return false;
  }
  if (m_manualTest.yolo_training_affine_channel_mode == 2) {
    reason = "COMPOUND_EVALUATION_ONLY: select rotation or scale for training; compound scale+rotation is an unseen validation/holdout channel";
    m_manualTest.geometry_aug_run_status = "COMPOUND_TRAINING_BLOCKED";
    m_manualTest.geometry_aug_run_reason = reason;
    return false;
  }
  cv::FileStorage templateStorage(trainingPlanTemplate.string(),
                                   cv::FileStorage::READ);
  int numClasses = 0;
  templateStorage["num_classes"] >> numClasses;
  std::vector<std::string> classNames;
  for (const auto &node : templateStorage["class_names"])
    classNames.push_back(static_cast<std::string>(node));
  std::vector<std::string> frozenParameterPrefixes;
  for (const auto &node : templateStorage["frozen_parameter_prefixes"])
    frozenParameterPrefixes.push_back(static_cast<std::string>(node));
  if (!templateStorage.isOpened() || numClasses <= 0 ||
      static_cast<int>(classNames.size()) != numClasses ||
      frozenParameterPrefixes.empty()) {
    reason = "C++ training plan template is invalid";
    return false;
  }
  const std::filesystem::path trainingPlan =
      planDir / "yolov8n_cpp_training_plan_runtime.json";
  const char *affineChannel =
      m_manualTest.yolo_training_affine_channel_mode == 0 ? "rotation" :
      m_manualTest.yolo_training_affine_channel_mode == 1 ? "scale" :
      "compound";
  std::ostringstream plan;
  plan << "{\n"
       << "  \"schema\": \"cxvision.yolov8n_cpp_training_plan.v1\",\n"
       << "  \"epochs\": " << m_manualTest.geometry_aug_epochs << ",\n"
       << "  \"batch_size\": " << m_manualTest.yolo_training_batch_size << ",\n"
       << "  \"input_size\": " << m_manualTest.yolo_training_input_size << ",\n"
       << "  \"max_train_batches_per_epoch\": "
       << m_manualTest.yolo_training_max_batches_per_epoch << ",\n"
       << "  \"learning_rate\": "
       << m_manualTest.geometry_aug_learning_rate << ",\n"
       << "  \"assignment_topk\": "
       << m_manualTest.yolo_training_assignment_topk << ",\n"
       << "  \"classification_focal_gamma\": "
       << m_manualTest.yolo_training_classification_focal_gamma << ",\n"
       << "  \"use_assignment_quality_targets\": "
       << (m_manualTest.yolo_training_use_assignment_quality_targets ? 1 : 0)
       << ",\n"
       << "  \"global_multiscale_assignment\": "
       << (m_manualTest.yolo_training_global_multiscale_assignment ? 1 : 0)
       << ",\n"
       << "  \"class_loss_weights\": [";
  for (std::size_t i = 0;
       i < m_manualTest.yolo_training_class_loss_weights.size(); ++i) {
    if (i > 0)
      plan << ", ";
    plan << m_manualTest.yolo_training_class_loss_weights[i];
  }
  plan << "],\n"
       << "  \"validation_interval\": "
       << m_manualTest.yolo_training_validation_interval << ",\n"
       << "  \"early_stop_patience\": "
       << m_manualTest.yolo_training_early_stop_patience << ",\n"
       << "  \"minimum_complete_epochs\": "
       << m_manualTest.yolo_training_minimum_complete_epochs << ",\n"
       << "  \"minimum_f1_delta\": "
       << m_manualTest.yolo_training_minimum_f1_delta << ",\n"
       << "  \"restore_best_validation_checkpoint\": "
       << (m_manualTest.yolo_training_restore_best_validation_checkpoint ? 1
                                                                         : 0)
       << ",\n"
       << "  \"postprocess_confidence_threshold\": "
       << m_manualTest.yolo_training_postprocess_confidence_threshold
       << ",\n"
       << "  \"postprocess_iou_threshold\": "
       << m_manualTest.yolo_training_postprocess_iou_threshold << ",\n"
       << "  \"postprocess_max_detections\": "
       << m_manualTest.yolo_training_postprocess_max_detections << ",\n"
       << "  \"postprocess_class_agnostic_nms\": "
       << (m_manualTest.yolo_training_postprocess_class_agnostic_nms ? 1 : 0)
       << ",\n"
       << "  \"evaluation_match_iou_threshold\": "
       << m_manualTest.yolo_training_evaluation_match_iou_threshold << ",\n"
       << "  \"geometry_roi_head_enabled\": "
       << (m_manualTest.yolo_training_geometry_roi_head_enabled ? 1 : 0) << ",\n"
       << "  \"geometry_target_manifest\": \""
       << JsonEscape(m_manualTest.yolo_training_geometry_target_manifest) << "\",\n"
       << "  \"geometry_roi_hidden_channels\": "
       << m_manualTest.yolo_training_geometry_roi_hidden_channels << ",\n"
       << "  \"geometry_roi_pooled_size\": "
       << m_manualTest.yolo_training_geometry_roi_pooled_size << ",\n"
       << "  \"geometry_roi_polygon_vertex_count\": "
       << m_manualTest.yolo_training_geometry_roi_polygon_vertex_count << ",\n"
       << "  \"geometry_roi_parameter_weight\": "
       << m_manualTest.yolo_training_geometry_roi_parameter_weight << ",\n"
       << "  \"geometry_roi_contour_weight\": "
       << m_manualTest.yolo_training_geometry_roi_contour_weight << ",\n"
       << "  \"geometry_roi_continuity_weight\": "
       << m_manualTest.yolo_training_geometry_roi_continuity_weight << ",\n"
       << "  \"geometry_roi_uncertainty_weight\": "
       << m_manualTest.yolo_training_geometry_roi_uncertainty_weight << ",\n"
       << "  \"geometry_roi_continuity_positive_weight\": "
       << m_manualTest.yolo_training_geometry_roi_continuity_positive_weight << ",\n"
       << "  \"geometry_roi_calibration_weight\": "
       << m_manualTest.yolo_training_geometry_roi_calibration_weight << ",\n"
       << "  \"affine_training_channel\": \"" << affineChannel << "\",\n"
       << "  \"affine_channel_semantics\": \"single_axis_training; compound_scale_rotation_reserved_for_unseen_evaluation\",\n"
       << "  \"rotation_train_positive_deg\": " << m_manualTest.yolo_training_rotation_train_positive_deg << ",\n"
       << "  \"rotation_train_negative_deg\": " << m_manualTest.yolo_training_rotation_train_negative_deg << ",\n"
       << "  \"rotation_validation_deg\": " << m_manualTest.yolo_training_rotation_validation_deg << ",\n"
       << "  \"rotation_holdout_deg\": " << m_manualTest.yolo_training_rotation_holdout_deg << ",\n"
       << "  \"scale_train_down\": " << m_manualTest.yolo_training_scale_train_down << ",\n"
       << "  \"scale_train_up\": " << m_manualTest.yolo_training_scale_train_up << ",\n"
       << "  \"scale_validation\": " << m_manualTest.yolo_training_scale_validation << ",\n"
       << "  \"scale_holdout\": " << m_manualTest.yolo_training_scale_holdout << ",\n"
       << "  \"compound_validation_scale\": " << m_manualTest.yolo_training_compound_validation_scale << ",\n"
       << "  \"compound_validation_rotation_deg\": " << m_manualTest.yolo_training_compound_validation_rotation_deg << ",\n"
       << "  \"compound_holdout_scale\": " << m_manualTest.yolo_training_compound_holdout_scale << ",\n"
       << "  \"compound_holdout_rotation_deg\": " << m_manualTest.yolo_training_compound_holdout_rotation_deg << ",\n"
       << "  \"parent_checkpoint\": \""
       << JsonEscape(selectedParent->checkpoint_path) << "\",\n"
       << "  \"parent_model_id\": \""
       << JsonEscape(selectedParent->model_id) << "\",\n"
       << "  \"parent_selection_review_ref\": \""
       << JsonEscape(selectedParent->parent_selection_review_path) << "\",\n"
       << "  \"frozen_parameter_prefixes\": [";
  for (std::size_t i = 0; i < frozenParameterPrefixes.size(); ++i) {
    if (i > 0)
      plan << ", ";
    plan << '"' << JsonEscape(frozenParameterPrefixes[i]) << '"';
  }
  plan << "],\n"
       << "  \"seed\": " << m_manualTest.yolo_training_seed << ",\n"
       << "  \"num_classes\": " << numClasses << ",\n"
       << "  \"class_names\": [";
  for (std::size_t i = 0; i < classNames.size(); ++i) {
    if (i > 0)
      plan << ", ";
    plan << '"' << JsonEscape(classNames[i]) << '"';
  }
  plan << "],\n  \"human_review_required\": true,\n"
       << "  \"promotion_allowed\": false\n}\n";
  if (!WriteTextFile(trainingPlan.string(), plan.str())) {
    reason = "cannot write C++ runtime training plan";
    return false;
  }

  auto quote = [](const std::wstring &value) {
    return L"\"" + value + L"\"";
  };
  std::wstring command =
      quote(executable.wstring()) + L" --yolov8n-training-lifecycle " +
      L"--dataset-root " + quote(packageManifest.parent_path().wstring()) +
      L" --training-plan " + quote(trainingPlan.wstring()) + L" --out " +
      quote(outputDir.wstring()) + L" --torch-device cuda " +
      L"--torch-runtime-dll " + quote(runtimeDll.wstring());
  std::vector<wchar_t> commandBuffer(command.begin(), command.end());
  commandBuffer.push_back(L'\0');
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION processInfo{};
  if (!CreateProcessW(nullptr, commandBuffer.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr,
                      executable.parent_path().wstring().c_str(), &startup,
                      &processInfo)) {
    reason = "CreateProcessW failed for C++ training; error=" +
             std::to_string(GetLastError());
    return false;
  }
  CloseHandle(processInfo.hThread);

  m_manualTest.geometry_aug_run_status = "CXX_YOLOV8N_TRAINING_RUNNING";
  m_manualTest.geometry_aug_run_reason =
      "background C++/LibTorch training started; curve refreshes per epoch";
  m_manualTest.geometry_aug_output_dir = outputDir.string();
  m_manualTest.geometry_aug_plan_path = trainingPlan.string();
  m_manualTest.torch_training_process_running = true;
  m_manualTest.torch_training_process_handle =
      reinterpret_cast<std::uintptr_t>(processInfo.hProcess);
  m_manualTest.torch_training_active_output_dir = outputDir.string();
  m_manualTest.torch_training_active_curve_path =
      (outputDir / "learning_curve.csv").string();
  m_manualTest.torch_training_active_result_path =
      (outputDir / "torch_yolov8n_training_lifecycle_result.json").string();
  m_manualTest.torch_training_run = CxTorchTrainingRunBinding{};
  m_manualTest.torch_training_run.status =
      "CXX_YOLOV8N_TRAINING_RUNNING";
  m_manualTest.torch_training_run.task = "detection";
  m_manualTest.torch_training_run.optimizer = "SGD";
  m_manualTest.torch_training_run.configured_epochs =
      m_manualTest.geometry_aug_epochs;
  m_manualTest.torch_training_run.learning_rate =
      m_manualTest.geometry_aug_learning_rate;
  const std::filesystem::path trainImages =
      packageManifest.parent_path() / "images" / "train";
  int trainSampleCount = 0;
  ec.clear();
  for (std::filesystem::directory_iterator it(trainImages, ec), end;
       !ec && it != end; it.increment(ec))
    trainSampleCount += it->is_regular_file(ec) && !ec ? 1 : 0;
  m_manualTest.torch_training_run.train_sample_count = trainSampleCount;
  m_manualTest.torch_training_run.dataset_source =
      packageManifest.parent_path().string();
  m_manualTest.torch_training_run.training_trace_path =
      m_manualTest.torch_training_active_result_path;
  m_manualTest.torch_training_run.learning_curve_path =
      m_manualTest.torch_training_active_curve_path;
  m_manualTest.torch_training_image_status =
      "CXX_YOLOV8N_TRAINING_RUNNING";
  m_manualTest.torch_training_image_reason = "completed_epoch=0/" +
      std::to_string(m_manualTest.geometry_aug_epochs);
  reason = m_manualTest.geometry_aug_run_reason;
  return true;
}

static std::string ModelLineageRunTimestampLocal() {
  std::string timestamp = CurrentTimestamp();
  for (char &ch : timestamp) {
    const unsigned char value = static_cast<unsigned char>(ch);
    if (!std::isalnum(value) && ch != '-' && ch != '_')
      ch = '_';
  }
  return timestamp;
}

static std::string
ReadModelLineageStringLocal(const cv::FileNode &node, const char *key) {
  const cv::FileNode value = node[key];
  return value.empty() ? std::string() : static_cast<std::string>(value);
}

static bool ReadModelLineageBoolLocal(const cv::FileNode &node,
                                      const char *key, bool fallback) {
  const cv::FileNode value = node[key];
  if (value.empty())
    return fallback;
  if (value.isInt())
    return static_cast<int>(value) != 0;
  if (value.isString()) {
    const std::string text = static_cast<std::string>(value);
    return text == "true" || text == "TRUE" || text == "1";
  }
  return fallback;
}

static std::vector<std::string>
ReadModelLineageStringListLocal(const cv::FileNode &node, const char *key) {
  std::vector<std::string> values;
  const cv::FileNode sequence = node[key];
  if (sequence.empty() || !sequence.isSeq())
    return values;
  for (const cv::FileNode &value : sequence)
    values.push_back(static_cast<std::string>(value));
  return values;
}

static bool IsPathInsideModelLineageRootLocal(
    const std::filesystem::path &root, const std::filesystem::path &candidate) {
  const std::filesystem::path relative = candidate.lexically_relative(root);
  if (relative.empty())
    return candidate == root;
  for (const std::filesystem::path &part : relative) {
    if (part == "..")
      return false;
  }
  return !relative.is_absolute();
}

static bool ResolveModelLineageRefLocal(
    const std::filesystem::path &scanRoot,
    const std::filesystem::path &nodeDirectory, const std::string &ref,
    std::string &resolved, std::string &reason) {
  if (ref.empty()) {
    reason = "required reference is empty";
    return false;
  }

  std::error_code ec;
  std::filesystem::path path(ref);
  if (path.is_relative())
    path = nodeDirectory / path;
  path = std::filesystem::weakly_canonical(path, ec);
  if (ec) {
    reason = "reference cannot be canonicalized: " + ref;
    return false;
  }
  if (!IsPathInsideModelLineageRootLocal(scanRoot, path)) {
    reason = "reference is outside scan root: " + path.string();
    return false;
  }
  if (!std::filesystem::is_regular_file(path, ec) || ec) {
    reason = "ASSET_MISSING: " + path.string();
    return false;
  }
  resolved = path.string();
  return true;
}

struct ModelLineageReviewLocal {
  std::string decision;
  std::string path;
};

static bool LoadModelLineageReviewLocal(
    const std::filesystem::path &path, std::string &modelId,
    ModelLineageReviewLocal &review) {
  cv::FileStorage storage(path.string(), cv::FileStorage::READ);
  if (!storage.isOpened())
    return false;
  modelId = ReadModelLineageStringLocal(storage.root(), "model_id");
  review.decision =
      ReadModelLineageStringLocal(storage.root(), "human_decision");
  review.path = path.string();
  return !modelId.empty() && !review.decision.empty();
}

static bool LoadModelPerformanceProfileLocal(
    const std::filesystem::path &path, CxModelPerformanceProfile &profile,
    std::string &reason) {
  cv::FileStorage storage(path.string(), cv::FileStorage::READ);
  if (!storage.isOpened()) {
    reason = "performance profile JSON cannot be opened";
    return false;
  }
  const cv::FileNode root = storage.root();
  const std::string schema = ReadModelLineageStringLocal(root, "schema");
  if (schema != "cxvision.model_performance_profile.v1") {
    reason = "unsupported performance profile schema";
    return false;
  }
  CxModelPerformanceProfile parsed;
  parsed.available = true;
  parsed.status = ReadModelLineageStringLocal(root, "status");
  parsed.profile_scope = ReadModelLineageStringLocal(root, "profile_scope");
  parsed.requested_device =
      ReadModelLineageStringLocal(root, "requested_device");
  parsed.profile_path = path.string();
  root["service_initialize_ms"] >> parsed.service_initialize_ms;
  root["sample_count"] >> parsed.sample_count;
  const cv::FileNode stages = root["stages"];
  if (parsed.status.empty() || parsed.profile_scope.empty() ||
      parsed.sample_count <= 0 || stages.empty() || !stages.isMap()) {
    reason = "required performance profile fact is missing";
    return false;
  }
  for (auto stageIt = stages.begin(); stageIt != stages.end(); ++stageIt) {
    const cv::FileNode node = *stageIt;
    CxModelPerformanceStage stage;
    stage.name = node.name();
    node["min_ms"] >> stage.min_ms;
    node["mean_ms"] >> stage.mean_ms;
    node["p50_ms"] >> stage.p50_ms;
    node["p95_ms"] >> stage.p95_ms;
    node["p99_ms"] >> stage.p99_ms;
    node["max_ms"] >> stage.max_ms;
    if (stage.name.empty() || stage.min_ms < 0.0 || stage.mean_ms < 0.0 ||
        stage.p50_ms < 0.0 || stage.p95_ms < 0.0 || stage.p99_ms < 0.0 ||
        stage.max_ms < 0.0) {
      reason = "invalid performance stage";
      return false;
    }
    parsed.stages.push_back(std::move(stage));
  }
  if (parsed.stages.empty()) {
    reason = "performance profile has no stage summaries";
    return false;
  }
  profile = std::move(parsed);
  reason = "performance profile loaded";
  return true;
}

static void RefreshModelPerformanceProfileAssetsLocal(
    ManualTestContext &context) {
  context.model_performance_profile_scan_attempted = true;
  context.model_performance_profile = {};
  std::error_code ec;
  const std::filesystem::path scanRoot = ResolveCxVisionRunPath(
      context.model_performance_profile_scan_root);
  if (!std::filesystem::is_directory(scanRoot, ec) || ec) {
    context.model_performance_profile.reason =
        "performance scan root is not a directory: " + scanRoot.string();
    return;
  }
  std::vector<std::filesystem::path> pendingDirectories{scanRoot};
  std::vector<std::filesystem::path> profiles;
  while (!pendingDirectories.empty()) {
    const std::filesystem::path directory = pendingDirectories.back();
    pendingDirectories.pop_back();
    std::error_code directoryError;
    std::filesystem::directory_iterator entries(
        directory, std::filesystem::directory_options::skip_permission_denied,
        directoryError);
    if (directoryError)
      continue;
    for (const std::filesystem::directory_entry &entry : entries) {
      std::error_code entryError;
      if (entry.is_symlink(entryError))
        continue;
      if (entry.is_directory(entryError) && !entryError) {
        pendingDirectories.push_back(entry.path());
        continue;
      }
      if (!entryError && entry.is_regular_file(entryError) &&
          entry.path().filename() == "performance_profile.json")
        profiles.push_back(entry.path());
    }
  }
  std::sort(profiles.begin(), profiles.end());
  std::string lastReason = "no performance_profile.json asset found";
  for (auto profileIt = profiles.rbegin(); profileIt != profiles.rend();
       ++profileIt) {
    CxModelPerformanceProfile profile;
    if (LoadModelPerformanceProfileLocal(*profileIt, profile, lastReason)) {
      context.model_performance_profile = std::move(profile);
      return;
    }
  }
  context.model_performance_profile.reason = lastReason;
}

static void RefreshValidationDatasetAssetsLocal(ManualTestContext &context) {
  context.validation_dataset_scan_attempted = true;
  context.validation_dataset = {};
  CxValidationDatasetReadiness &readiness = context.validation_dataset;
  std::error_code ec;
  const std::filesystem::path scanRoot = ResolveCxVisionRunPath(
      context.validation_dataset_scan_root);
  if (!std::filesystem::is_directory(scanRoot, ec) || ec) {
    readiness.status = "VALIDATION_BLOCKED_ASSET_MISSING";
    readiness.reason = "validation scan root is not a directory: " +
                       scanRoot.string();
    return;
  }
  std::vector<std::filesystem::path> manifests;
  std::vector<std::filesystem::path> pending{scanRoot};
  while (!pending.empty()) {
    const std::filesystem::path directory = pending.back();
    pending.pop_back();
    std::error_code directoryError;
    std::filesystem::directory_iterator entries(
        directory, std::filesystem::directory_options::skip_permission_denied,
        directoryError);
    if (directoryError)
      continue;
    for (const std::filesystem::directory_entry &entry : entries) {
      std::error_code entryError;
      if (entry.is_symlink(entryError))
        continue;
      if (entry.is_directory(entryError) && !entryError)
        pending.push_back(entry.path());
      else if (!entryError && entry.is_regular_file(entryError) &&
               entry.path().filename() == "dataset_manifest.json")
        manifests.push_back(entry.path());
    }
  }
  std::sort(manifests.begin(), manifests.end());
  for (auto manifestIt = manifests.rbegin(); manifestIt != manifests.rend();
       ++manifestIt) {
    cv::FileStorage storage;
    try {
      storage.open(manifestIt->string(), cv::FileStorage::READ |
                                          cv::FileStorage::FORMAT_JSON);
    } catch (const cv::Exception &) {
      // Evidence assets are independently produced.  A malformed or
      // unsupported historical dataset manifest is a rejected candidate, not
      // a GUI-startup failure.
      continue;
    }
    if (!storage.isOpened() ||
        ReadModelLineageStringLocal(storage.root(), "schema") !=
            "cxvision.geometry_augmentation_dataset.v1")
      continue;
    const cv::FileNode samples = storage.root()["samples"];
    if (!samples.isSeq())
      continue;
    readiness.available = true;
    readiness.dataset_manifest_path = manifestIt->string();
    std::unordered_set<std::string> trainSourceHashes;
    std::unordered_set<std::string> validationSourceHashes;
    for (const cv::FileNode &sample : samples) {
      const std::string split = ReadModelLineageStringLocal(sample, "split");
      const std::string caseRef =
          ReadModelLineageStringLocal(sample, "case_path");
      const std::filesystem::path casePath = manifestIt->parent_path() / caseRef;
      if (split != "train" && split != "validation")
        continue;
      if (split == "train")
        ++readiness.train_sample_count;
      else
        ++readiness.validation_sample_count;
      cv::FileStorage caseManifest(
          (casePath / "case_manifest.json").string(), cv::FileStorage::READ);
      const cv::FileNode required = caseManifest.isOpened()
                                        ? caseManifest.root()["required_assets"]
                                        : cv::FileNode();
      if (!caseManifest.isOpened() || !required.isSeq()) {
        ++readiness.missing_required_asset_count;
        continue;
      }
      for (const cv::FileNode &asset : required) {
        const std::filesystem::path assetPath =
            casePath / static_cast<std::string>(asset);
        std::error_code assetError;
        if (!std::filesystem::is_regular_file(assetPath, assetError) ||
            assetError)
          ++readiness.missing_required_asset_count;
      }
      cv::FileStorage hashes((casePath / "source_hashes.json").string(),
                             cv::FileStorage::READ);
      const std::string sourceHash = hashes.isOpened()
          ? ReadModelLineageStringLocal(hashes.root(), "source_image")
          : std::string();
      if (sourceHash.empty()) {
        ++readiness.missing_required_asset_count;
      } else if (split == "train") {
        trainSourceHashes.insert(sourceHash);
      } else {
        validationSourceHashes.insert(sourceHash);
      }
    }
    for (const std::string &hash : validationSourceHashes) {
      if (trainSourceHashes.find(hash) != trainSourceHashes.end())
        ++readiness.source_lineage_overlap_count;
    }
    readiness.frozen = readiness.validation_sample_count > 0 &&
                       readiness.missing_required_asset_count == 0;
    if (readiness.validation_sample_count <= 0) {
      readiness.status = "VALIDATION_BLOCKED_NO_FROZEN_SET";
      readiness.reason = "dataset has no validation samples";
    } else if (readiness.missing_required_asset_count > 0) {
      readiness.status = "VALIDATION_BLOCKED_ASSET_MISSING";
      readiness.reason = "required validation assets are missing";
    } else if (readiness.source_lineage_overlap_count > 0) {
      readiness.status = "VALIDATION_BLOCKED_SOURCE_LINEAGE_OVERLAP";
      readiness.reason =
          "validation images share source-image lineage with training images; "
          "controlled perturbation regression is available, independent "
          "generalization validation is not";
    } else {
      readiness.status = "VALIDATION_DATASET_FROZEN_READY_FOR_INFERENCE";
      readiness.reason =
          "frozen validation assets are complete and source-disjoint";
    }
    return;
  }
  readiness.status = "VALIDATION_BLOCKED_NO_DATASET_MANIFEST";
  readiness.reason = "no supported validation dataset manifest was found";
}

static void RefreshModelLineageAssetsLocal(ManualTestContext &context) {
  context.model_lineage_scan_attempted = true;
  context.model_lineage_nodes.clear();
  context.model_lineage_rejected.clear();
  context.selected_model_lineage_node = -1;

  std::error_code ec;
  std::filesystem::path scanRoot =
      ResolveCxVisionRunPath(context.model_lineage_scan_root);
  scanRoot = std::filesystem::weakly_canonical(scanRoot, ec);
  if (ec || !std::filesystem::is_directory(scanRoot, ec)) {
    context.model_lineage_scan_status = "ASSET_MISSING";
    context.model_lineage_scan_reason =
        "model lineage scan root is not a directory: " + scanRoot.string();
    return;
  }

  std::vector<std::filesystem::path> nodePaths;
  std::unordered_map<std::string, ModelLineageReviewLocal> parentReviews;
  std::unordered_map<std::string, ModelLineageReviewLocal> freezeReviews;
  std::unordered_map<std::string, ModelLineageReviewLocal> differenceReviews;
  std::vector<std::filesystem::path> pendingDirectories{scanRoot};
  while (!pendingDirectories.empty()) {
    const std::filesystem::path directory = pendingDirectories.back();
    pendingDirectories.pop_back();
    std::error_code directoryError;
    std::filesystem::directory_iterator entries(
        directory, std::filesystem::directory_options::skip_permission_denied,
        directoryError);
    if (directoryError) {
      context.model_lineage_rejected.push_back(
          {directory.string(),
           "DIRECTORY_SCAN_FAILED: " + directoryError.message()});
      continue;
    }
    for (const std::filesystem::directory_entry &entry : entries) {
      std::error_code entryError;
      if (entry.is_symlink(entryError))
        continue;
      if (entry.is_directory(entryError) && !entryError) {
        pendingDirectories.push_back(entry.path());
        continue;
      }
      if (!entry.is_regular_file(entryError) || entryError)
        continue;

      const std::string filename = entry.path().filename().string();
      if (filename == "model_node.json") {
        nodePaths.push_back(entry.path());
      } else if (filename == "parent_model_selection_review.json" ||
                 filename == "freeze_policy_review.json" ||
                 filename == "inference_difference_review.json") {
        std::string modelId;
        ModelLineageReviewLocal review;
        if (LoadModelLineageReviewLocal(entry.path(), modelId, review)) {
          auto *reviews = &differenceReviews;
          if (filename == "parent_model_selection_review.json")
            reviews = &parentReviews;
          else if (filename == "freeze_policy_review.json")
            reviews = &freezeReviews;
          const auto existing = reviews->find(modelId);
          if (existing == reviews->end() ||
              existing->second.path < review.path)
            (*reviews)[modelId] = review;
        }
      }
    }
  }
  std::sort(nodePaths.begin(), nodePaths.end());

  std::vector<CxModelLineageUiNode> parsedNodes;
  for (const std::filesystem::path &nodePath : nodePaths) {
    CxModelLineageUiNode node;
    node.model_node_path = nodePath.string();
    std::string rejectReason;
    cv::FileStorage storage(nodePath.string(), cv::FileStorage::READ);
    if (!storage.isOpened()) {
      rejectReason = "invalid JSON";
    } else {
      const cv::FileNode root = storage.root();
      const std::string schema =
          ReadModelLineageStringLocal(root, "schema");
      node.model_id = ReadModelLineageStringLocal(root, "model_id");
      node.display_name = ReadModelLineageStringLocal(root, "display_name");
      node.node_kind = ReadModelLineageStringLocal(root, "node_kind");
      node.root_release_id =
          ReadModelLineageStringLocal(root, "root_release_id");
      node.branch_id = ReadModelLineageStringLocal(root, "branch_id");
      node.parent_model_ids =
          ReadModelLineageStringListLocal(root, "parent_model_ids");
      node.task = ReadModelLineageStringLocal(root, "task");
      node.architecture_id =
          ReadModelLineageStringLocal(root, "architecture_id");
      node.output_contract =
          ReadModelLineageStringLocal(root, "output_contract");
      node.capabilities =
          ReadModelLineageStringListLocal(root, "capabilities");
      node.checkpoint_hash =
          ReadModelLineageStringLocal(root, "checkpoint_hash");
      node.checkpoint_hash_status =
          ReadModelLineageStringLocal(root, "checkpoint_hash_status");
      node.promotion_status =
          ReadModelLineageStringLocal(root, "promotion_status");
      node.admission_status =
          ReadModelLineageStringLocal(root, "admission_status");
      node.parent_usage =
          ReadModelLineageStringLocal(root, "parent_usage");
      node.development_parent_eligible = ReadModelLineageBoolLocal(
          root, "development_parent_eligible", false);
      node.production_registry_write_allowed = ReadModelLineageBoolLocal(
          root, "production_registry_write_allowed", true);

      if (schema != "cxvision.model_lineage_node.v1")
        rejectReason = "unsupported schema";
      else if (node.model_id.empty() || node.display_name.empty() ||
               node.node_kind.empty() || node.root_release_id.empty() ||
               node.task.empty() || node.architecture_id.empty() ||
               node.output_contract.empty() || node.capabilities.empty() ||
               node.checkpoint_hash.empty() ||
               node.checkpoint_hash_status.empty() ||
               node.promotion_status.empty())
        rejectReason = "required lineage fact is empty";
      else if (node.node_kind != "release" &&
               node.node_kind != "development_parent" &&
               node.node_kind != "incremental_candidate" &&
               node.node_kind != "merge_candidate")
        rejectReason = "unsupported node_kind";
      else {
        const cv::FileNode refs = root["refs"];
        struct RefTarget {
          const char *key;
          std::string *target;
        };
        RefTarget targets[] = {
            {"capability_manifest", &node.capability_manifest_path},
            {"model_manifest", &node.model_manifest_path},
            {"checkpoint", &node.checkpoint_path},
            {"class_ontology", &node.class_ontology_path},
            {"dataset_binding", &node.dataset_binding_path},
            {"training_plan", &node.training_plan_path},
            {"metric_bundle", &node.metric_bundle_path},
        };
        for (const RefTarget &target : targets) {
          const std::string ref =
              ReadModelLineageStringLocal(refs, target.key);
          if (!ResolveModelLineageRefLocal(
                  scanRoot, nodePath.parent_path(), ref, *target.target,
                  rejectReason))
            break;
        }
      }
    }
    if (!rejectReason.empty()) {
      context.model_lineage_rejected.push_back(
          {nodePath.string(), rejectReason});
      continue;
    }
    parsedNodes.push_back(std::move(node));
  }

  std::unordered_map<std::string, int> idCounts;
  for (const CxModelLineageUiNode &node : parsedNodes)
    ++idCounts[node.model_id];

  std::unordered_set<std::string> uniqueIds;
  for (const CxModelLineageUiNode &node : parsedNodes) {
    if (idCounts[node.model_id] == 1)
      uniqueIds.insert(node.model_id);
  }

  for (CxModelLineageUiNode &node : parsedNodes) {
    std::string rejectReason;
    if (idCounts[node.model_id] > 1) {
      rejectReason = "DUPLICATE_MODEL_ID: " + node.model_id;
    } else if (node.node_kind == "release" &&
               !node.parent_model_ids.empty()) {
      rejectReason = "release node must not declare parents";

    } else if (node.node_kind == "development_parent" &&
               (!node.parent_model_ids.empty() ||
                node.admission_status != "CANDIDATE" ||
                node.parent_usage != "DEVELOPMENT_ONLY" ||
                !node.development_parent_eligible ||
                node.production_registry_write_allowed)) {
      rejectReason =
          "development parent requires no parents, CANDIDATE admission, "
          "DEVELOPMENT_ONLY usage, eligibility=true and formal registry "
          "write=false";
    } else if (node.node_kind == "incremental_candidate" &&
               node.parent_model_ids.size() != 1) {
      rejectReason = "incremental candidate requires exactly one parent";
    } else if (node.node_kind == "merge_candidate" &&
               node.parent_model_ids.size() < 2) {
      rejectReason = "merge candidate requires at least two parents";
    } else {
      for (const std::string &parentId : node.parent_model_ids) {
        if (uniqueIds.find(parentId) == uniqueIds.end()) {
          rejectReason = "PARENT_MODEL_MISSING: " + parentId;
          break;
        }
      }
    }
    if (!rejectReason.empty()) {
      context.model_lineage_rejected.push_back(
          {node.model_node_path, rejectReason});
      continue;
    }
    node.accepted = true;
    const auto parentReview = parentReviews.find(node.model_id);
    node.parent_selection_reviewed =
        parentReview != parentReviews.end() &&
        parentReview->second.decision == "HUMAN_CONFIRMED";
    if (node.parent_selection_reviewed)
      node.parent_selection_review_path = parentReview->second.path;

    node.parent_gate_satisfied = node.node_kind == "release" ||
                                 node.node_kind == "development_parent";
    if (!node.parent_model_ids.empty()) {
      node.parent_gate_satisfied = true;
      for (const std::string &parentId : node.parent_model_ids) {
        const auto review = parentReviews.find(parentId);
        if (review == parentReviews.end() ||
            review->second.decision != "HUMAN_CONFIRMED") {
          node.parent_gate_satisfied = false;
          break;
        }
        node.parent_selection_review_path = review->second.path;
      }
    }

    const auto freezeReview = freezeReviews.find(node.model_id);
    node.freeze_policy_reviewed =
        freezeReview != freezeReviews.end() &&
        freezeReview->second.decision == "HUMAN_CONFIRMED";
    if (node.freeze_policy_reviewed)
      node.freeze_policy_review_path = freezeReview->second.path;
    const auto differenceReview = differenceReviews.find(node.model_id);
    node.inference_difference_reviewed =
        differenceReview != differenceReviews.end() &&
        differenceReview->second.decision == "HUMAN_CONFIRMED";
    if (node.inference_difference_reviewed)
      node.inference_difference_review_path = differenceReview->second.path;

    if (node.node_kind == "development_parent") {
      node.gate_status = "DEVELOPMENT_PARENT_READY";
      node.gate_reason =
          "development-only parent is eligible for isolated business trials; "
          "formal registry write and production promotion remain blocked";
    } else if (node.node_kind == "release") {
      node.gate_status = node.parent_selection_reviewed
                             ? "PARENT_READY"
                             : "PENDING_PARENT_SELECTION";
      node.gate_reason =
          node.parent_selection_reviewed
              ? "parent model selection has independent human evidence"
              : "parent model selection review is required before branching";
    } else if (node.parent_gate_satisfied &&
               node.freeze_policy_reviewed &&
               node.inference_difference_reviewed) {
      node.gate_status = "GATE_READY";
      node.gate_reason = "all mandatory human gates are confirmed";
    } else {
      node.gate_status = "PENDING_HUMAN_REVIEW";
      node.gate_reason =
          "parent selection, freeze execution and inference difference "
          "reviews must all be confirmed";
    }
    context.model_lineage_nodes.push_back(std::move(node));
  }

  std::sort(context.model_lineage_nodes.begin(),
            context.model_lineage_nodes.end(),
            [](const CxModelLineageUiNode &left,
               const CxModelLineageUiNode &right) {
              if (left.display_name != right.display_name)
                return left.display_name < right.display_name;
               return left.model_id < right.model_id;
             });

  if (!context.model_lineage_nodes.empty())
    context.selected_model_lineage_node = 0;

  context.model_lineage_scan_status =
      context.model_lineage_nodes.empty() ? "NO_VALID_MODEL_NODES"
                                          : "MODEL_LINEAGE_SCAN_COMPLETE";
  context.model_lineage_scan_reason =
      "discovered=" + std::to_string(nodePaths.size()) +
      " accepted=" + std::to_string(context.model_lineage_nodes.size()) +
      " rejected=" + std::to_string(context.model_lineage_rejected.size());

  const std::filesystem::path debugDir =
      scanRoot / "_shared" / "model_lineage_scans" /
      ("run_" + ModelLineageRunTimestampLocal());
  std::filesystem::create_directories(debugDir, ec);
  const std::filesystem::path debugPath =
      debugDir / "model_lineage_scan_debug.json";
  cv::FileStorage debugStorage(
      debugPath.string(),
      cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (debugStorage.isOpened()) {
    debugStorage << "schema" << "cxvision.model_lineage_scan.v1";
    debugStorage << "root" << scanRoot.string();
    debugStorage << "discovered" << static_cast<int>(nodePaths.size());
    debugStorage << "accepted"
                 << static_cast<int>(context.model_lineage_nodes.size());
    debugStorage << "rejected"
                 << static_cast<int>(context.model_lineage_rejected.size());
    debugStorage << "accepted_model_ids"
                 << "[";
    for (const CxModelLineageUiNode &node : context.model_lineage_nodes)
      debugStorage << node.model_id;
    debugStorage << "]";
    debugStorage << "rejections"
                 << "[";
    for (const CxModelLineageRejectedAsset &rejected :
         context.model_lineage_rejected) {
      debugStorage << "{";
      debugStorage << "path" << rejected.path;
      debugStorage << "reason" << rejected.reason;
      debugStorage << "}";
    }
    debugStorage << "]";
    debugStorage.release();
    context.model_lineage_scan_debug_path = debugPath.string();
  }
}

int RunModelLineageAssetScanSmoke(const std::string &scanRoot,
                                  const std::string &outputDirectory) {
  ManualTestContext context;
  context.model_lineage_scan_root = scanRoot;
  RefreshModelLineageAssetsLocal(context);

  const std::filesystem::path outputRoot =
      ResolveCxVisionRunPath(outputDirectory);
  std::error_code ec;
  std::filesystem::create_directories(outputRoot, ec);
  const std::filesystem::path reportPath =
      outputRoot / "model_lineage_scan_smoke.json";
  cv::FileStorage report(
      reportPath.string(),
      cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!report.isOpened()) {
    std::cerr << "model_lineage_scan_smoke_ok=false\n";
    std::cerr << "reason=cannot write smoke report\n";
    return 2;
  }
  report << "schema" << "cxvision.model_lineage_scan_smoke.v1";
  report << "status" << context.model_lineage_scan_status;
  report << "reason" << context.model_lineage_scan_reason;
  report << "scan_debug_ref" << context.model_lineage_scan_debug_path;
  report << "accepted" << static_cast<int>(context.model_lineage_nodes.size());
  report << "rejected"
         << static_cast<int>(context.model_lineage_rejected.size());
  report << "nodes"
         << "[";
  for (const CxModelLineageUiNode &node : context.model_lineage_nodes) {
    report << "{";
    report << "model_id" << node.model_id;
    report << "model_node_path" << node.model_node_path;
    report << "display_name" << node.display_name;
    report << "node_kind" << node.node_kind;
    report << "admission_status" << node.admission_status;
    report << "parent_usage" << node.parent_usage;
    report << "development_parent_eligible"
           << static_cast<int>(node.development_parent_eligible);
    report << "production_registry_write_allowed"
           << static_cast<int>(node.production_registry_write_allowed);
    report << "parent_model_ids"
           << "[";
    for (const std::string &parentId : node.parent_model_ids)
      report << parentId;
    report << "]";
    report << "task" << node.task;
    report << "architecture_id" << node.architecture_id;
    report << "output_contract" << node.output_contract;
    report << "checkpoint_path" << node.checkpoint_path;
    report << "checkpoint_hash" << node.checkpoint_hash;
    report << "checkpoint_hash_status" << node.checkpoint_hash_status;
    report << "gate_status" << node.gate_status;
    report << "parent_gate_satisfied"
           << static_cast<int>(node.parent_gate_satisfied);
    report << "freeze_policy_reviewed"
           << static_cast<int>(node.freeze_policy_reviewed);
    report << "inference_difference_reviewed"
           << static_cast<int>(node.inference_difference_reviewed);
    report << "}";
  }
  report << "]";
  report << "rejections"
         << "[";
  for (const CxModelLineageRejectedAsset &rejected :
       context.model_lineage_rejected) {
    report << "{";
    report << "path" << rejected.path;
    report << "reason" << rejected.reason;
    report << "}";
  }
  report << "]";
  const bool passed = !context.model_lineage_nodes.empty();
  report << "smoke_status"
         << (passed ? "MODEL_LINEAGE_SCAN_PASS"
                    : "MODEL_LINEAGE_SCAN_FAIL");
  report.release();

  std::cout << "model_lineage_scan_smoke_ok="
            << (passed ? "true" : "false") << "\n";
  std::cout << "status=" << context.model_lineage_scan_status << "\n";
  std::cout << context.model_lineage_scan_reason << "\n";
  std::cout << "report=" << reportPath.string() << "\n";
  return passed ? 0 : 1;
}

static bool WriteParentModelSelectionReviewLocal(
    ManualTestContext &context, const CxModelLineageUiNode &node,
    std::string &reason) {
  const std::string selectionReason =
      TrimLine(context.model_lineage_parent_selection_reason);
  if (selectionReason.empty()) {
    reason = "selection reason is required";
    return false;
  }
  const std::filesystem::path scanRoot =
      ResolveCxVisionRunPath(context.model_lineage_scan_root);
  const std::filesystem::path reviewDir =
      scanRoot / "model_incremental_reviews" /
      ("run_" + ModelLineageRunTimestampLocal() + "_parent_selection");
  std::error_code ec;
  std::filesystem::create_directories(reviewDir, ec);
  if (ec) {
    reason = "cannot create review directory: " + ec.message();
    return false;
  }

  const std::filesystem::path reviewPath =
      reviewDir / "parent_model_selection_review.json";
  cv::FileStorage reviewStorage(
      reviewPath.string(),
      cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!reviewStorage.isOpened()) {
    reason = "failed to write independent parent selection review";
    return false;
  }
  reviewStorage << "schema" << "cxvision.parent_model_selection_review.v1";
  reviewStorage << "model_id" << node.model_id;
  reviewStorage << "display_name" << node.display_name;
  reviewStorage << "task" << node.task;
  reviewStorage << "architecture_id" << node.architecture_id;
  reviewStorage << "output_contract" << node.output_contract;
  reviewStorage << "selection_reason" << selectionReason;
  reviewStorage << "human_decision" << "HUMAN_CONFIRMED";
  reviewStorage << "automatic" << 0;
  reviewStorage << "created_at" << CurrentTimestamp();
  reviewStorage.release();
  context.model_lineage_parent_selection_review_path = reviewPath.string();
  reason = "parent selection review saved independently";
  return true;
}

static bool ValidateModelNodeAssetsLocal(const CxModelLineageUiNode &node,
                                         std::string &reason) {
  struct RequiredAsset {
    const char *label;
    const std::string *path;
  };
  const RequiredAsset assets[] = {
      {"model manifest", &node.model_manifest_path},
      {"checkpoint", &node.checkpoint_path},
      {"dataset binding", &node.dataset_binding_path},
      {"training plan", &node.training_plan_path},
      {"metric bundle", &node.metric_bundle_path},
  };
  for (const RequiredAsset &asset : assets) {
    std::error_code error;
    if (asset.path->empty() ||
        !std::filesystem::is_regular_file(*asset.path, error) || error) {
      reason = std::string("ASSET_MISSING: ") + asset.label;
      return false;
    }
  }
  if (node.checkpoint_hash.empty() || node.checkpoint_hash_status.empty()) {
    reason = "CHECKPOINT_HASH_FACT_MISSING";
    return false;
  }
  reason = "model composition, checkpoint, dataset, plan and metric assets are readable";
  return true;
}

int RunModelLineageOperationSelfTest(const std::string &scanRoot,
                                     const std::string &outputDirectory) {
  // This is intentionally a preflight and evidence recorder.  It never writes
  // a human review, changes a selected parent, replaces a checkpoint, or starts
  // a training/inference job.
  ManualTestContext context;
  context.model_lineage_scan_root = scanRoot;
  context.validation_dataset_scan_root = scanRoot;
  RefreshModelLineageAssetsLocal(context);
  RefreshValidationDatasetAssetsLocal(context);

  const std::filesystem::path outputRoot =
      ResolveCxVisionRunPath(outputDirectory);
  std::error_code ec;
  std::filesystem::create_directories(outputRoot, ec);
  if (ec) {
    std::cerr << "model_lineage_operation_selftest_ok=false\n";
    std::cerr << "reason=cannot create output directory: " << ec.message()
              << "\n";
    return 2;
  }

  const std::filesystem::path reportPath =
      outputRoot / "model_lineage_operation_selftest.json";
  cv::FileStorage report(
      reportPath.string(),
      cv::FileStorage::WRITE | cv::FileStorage::FORMAT_JSON);
  if (!report.isOpened()) {
    std::cerr << "model_lineage_operation_selftest_ok=false\n";
    std::cerr << "reason=cannot write operation selftest report\n";
    return 2;
  }

  std::unordered_set<std::string> knownModelIds;
  for (const CxModelLineageUiNode &node : context.model_lineage_nodes)
    knownModelIds.insert(node.model_id);
  int assetsVerified = 0;
  int assetsRejected = 0;
  int parentLinksPresent = 0;
  int parentLinksMissing = 0;

  report << "schema" << "cxvision.model_lineage_operation_selftest.v1";
  report << "created_at" << CurrentTimestamp();
  report << "scope" << "asset-discovered model lineage and frozen validation gate";
  report << "scan_root" << ResolveCxVisionRunPath(scanRoot).string();
  report << "scan_status" << context.model_lineage_scan_status;
  report << "scan_reason" << context.model_lineage_scan_reason;
  report << "scan_debug_ref" << context.model_lineage_scan_debug_path;
  report << "validation_gate" << "{";
  report << "status" << context.validation_dataset.status;
  report << "reason" << context.validation_dataset.reason;
  report << "dataset_manifest" << context.validation_dataset.dataset_manifest_path;
  report << "train_samples" << context.validation_dataset.train_sample_count;
  report << "validation_samples" << context.validation_dataset.validation_sample_count;
  report << "missing_required_assets"
         << context.validation_dataset.missing_required_asset_count;
  report << "source_lineage_overlap"
         << context.validation_dataset.source_lineage_overlap_count;
  report << "}";
  report << "nodes" << "[";
  for (const CxModelLineageUiNode &node : context.model_lineage_nodes) {
    std::string assetReason;
    const bool assetsOk = ValidateModelNodeAssetsLocal(node, assetReason);
    if (assetsOk)
      ++assetsVerified;
    else
      ++assetsRejected;
    bool hasParent = !node.parent_model_ids.empty();
    bool allParentsPresent = hasParent;
    for (const std::string &parentId : node.parent_model_ids) {
      if (knownModelIds.find(parentId) == knownModelIds.end())
        allParentsPresent = false;
    }
    if (hasParent && allParentsPresent)
      ++parentLinksPresent;
    else if (hasParent)
      ++parentLinksMissing;

    report << "{";
    // display_name is the same user-facing name shown in the topology, rather
    // than an internal case identifier.
    report << "display_name" << node.display_name;
    report << "node_kind" << node.node_kind;
    report << "gate_status" << node.gate_status;
    report << "parameter_asset_check" << (assetsOk ? "PASS" : "FAIL");
    report << "parameter_asset_reason" << assetReason;
    report << "parent_review_preflight"
           << (!hasParent ? "NOT_APPLICABLE_ROOT_NODE"
                           : (allParentsPresent
                                  ? "PARENT_AVAILABLE_FOR_HUMAN_ROLLBACK_REVIEW"
                                  : "PARENT_ASSET_MISSING"));
    report << "parent_count" << static_cast<int>(node.parent_model_ids.size());
    report << "checkpoint_hash_status" << node.checkpoint_hash_status;
    report << "}";
  }
  report << "]";

  const bool validationReady =
      context.validation_dataset.status ==
      "VALIDATION_DATASET_FROZEN_READY_FOR_INFERENCE";
  const std::string comparePreflight =
      validationReady ? "READY_FOR_OPERATOR_COMPARE"
                      : "BLOCKED_BY_" + context.validation_dataset.status;
  const std::string augmentationPreflight =
      validationReady ? "READY_FOR_OPERATOR_AUGMENTATION_RETRAIN"
                      : "BLOCKED_BY_" + context.validation_dataset.status;
  report << "operation_preflight" << "{";
  report << "rollback" << "HUMAN_REVIEW_REQUIRED_NO_AUTOMATIC_PARENT_SELECTION";
  report << "compare" << comparePreflight;
  report << "augmentation_retrain" << augmentationPreflight;
  report << "}";
  report << "summary" << "{";
  report << "accepted_nodes" << static_cast<int>(context.model_lineage_nodes.size());
  report << "rejected_nodes" << static_cast<int>(context.model_lineage_rejected.size());
  report << "parameter_assets_verified" << assetsVerified;
  report << "parameter_assets_rejected" << assetsRejected;
  report << "parent_links_present" << parentLinksPresent;
  report << "parent_links_missing" << parentLinksMissing;
  report << "human_review_status" << "PENDING_HUMAN_REVIEW";
  report << "promotion_allowed" << 0;
  report << "}";
  const bool complete = !context.model_lineage_nodes.empty() &&
                        assetsRejected == 0 && parentLinksMissing == 0;
  report << "selftest_status"
         << (complete ? "MODEL_LINEAGE_OPERATION_SELFTEST_COMPLETE_WITH_GATES"
                      : "MODEL_LINEAGE_OPERATION_SELFTEST_FAILED");
  report.release();

  const std::filesystem::path summaryPath =
      outputRoot / "model_lineage_operation_selftest.md";
  std::ofstream summary(summaryPath, std::ios::out | std::ios::trunc);
  if (summary.is_open()) {
    summary << "# Model lineage operation self-test\n\n";
    summary << "- Status: "
            << (complete ? "MODEL_LINEAGE_OPERATION_SELFTEST_COMPLETE_WITH_GATES"
                         : "MODEL_LINEAGE_OPERATION_SELFTEST_FAILED")
            << "\n";
    summary << "- Human review: PENDING_HUMAN_REVIEW\n";
    summary << "- Promotion allowed: false\n";
    summary << "- Validation gate: " << context.validation_dataset.status
            << " — " << context.validation_dataset.reason << "\n";
    summary << "- Compare preflight: " << comparePreflight << "\n";
    summary << "- Augmentation/retrain preflight: " << augmentationPreflight
            << "\n\n";
    summary << "## User-facing model nodes\n\n";
    for (const CxModelLineageUiNode &node : context.model_lineage_nodes) {
      std::string assetReason;
      const bool assetsOk = ValidateModelNodeAssetsLocal(node, assetReason);
      summary << "- " << node.display_name << ": parameter assets "
              << (assetsOk ? "PASS" : "FAIL") << " (" << assetReason
              << ")\n";
    }
  }

  std::cout << "model_lineage_operation_selftest_ok="
            << (complete ? "true" : "false") << "\n";
  std::cout << "status="
            << (complete ? "MODEL_LINEAGE_OPERATION_SELFTEST_COMPLETE_WITH_GATES"
                         : "MODEL_LINEAGE_OPERATION_SELFTEST_FAILED")
            << "\n";
  std::cout << "validation_gate=" << context.validation_dataset.status << "\n";
  std::cout << "report=" << reportPath.string() << "\n";
  std::cout << "summary=" << summaryPath.string() << "\n";
  return complete ? 0 : 1;
}

static void RefreshModelLineageOperationSelfTestAssetsLocal(
    ManualTestContext &context) {
  context.model_lineage_operation_selftest_scan_attempted = true;
  context.model_lineage_operation_selftest = {};
  CxModelLineageOperationSelfTestProjection &projection =
      context.model_lineage_operation_selftest;
  const std::filesystem::path scanRoot = ResolveCxVisionRunPath(
      context.model_lineage_operation_selftest_scan_root);
  std::error_code ec;
  if (!std::filesystem::is_directory(scanRoot, ec) || ec) {
    projection.status = "OPERATION_SELFTEST_ASSET_ROOT_MISSING";
    return;
  }
  std::vector<std::filesystem::path> reports;
  std::vector<std::filesystem::path> pending{scanRoot};
  while (!pending.empty()) {
    const std::filesystem::path directory = pending.back();
    pending.pop_back();
    std::error_code directoryError;
    std::filesystem::directory_iterator entries(
        directory, std::filesystem::directory_options::skip_permission_denied,
        directoryError);
    if (directoryError)
      continue;
    for (const std::filesystem::directory_entry &entry : entries) {
      std::error_code entryError;
      if (entry.is_symlink(entryError))
        continue;
      if (entry.is_directory(entryError) && !entryError)
        pending.push_back(entry.path());
      else if (!entryError && entry.is_regular_file(entryError) &&
               entry.path().filename() ==
                   "model_lineage_operation_selftest.json")
        reports.push_back(entry.path());
    }
  }
  std::sort(reports.begin(), reports.end());
  for (auto reportIt = reports.rbegin(); reportIt != reports.rend();
       ++reportIt) {
    cv::FileStorage storage(reportIt->string(), cv::FileStorage::READ);
    if (!storage.isOpened() ||
        ReadModelLineageStringLocal(storage.root(), "schema") !=
            "cxvision.model_lineage_operation_selftest.v1")
      continue;
    projection.available = true;
    projection.report_path = reportIt->string();
    projection.status =
        ReadModelLineageStringLocal(storage.root(), "selftest_status");
    projection.created_at =
        ReadModelLineageStringLocal(storage.root(), "created_at");
    const cv::FileNode validationGate = storage.root()["validation_gate"];
    projection.validation_gate_status =
        ReadModelLineageStringLocal(validationGate, "status");
    projection.validation_gate_reason =
        ReadModelLineageStringLocal(validationGate, "reason");
    const cv::FileNode operationPreflight =
        storage.root()["operation_preflight"];
    projection.compare_preflight =
        ReadModelLineageStringLocal(operationPreflight, "compare");
    projection.augmentation_retrain_preflight =
        ReadModelLineageStringLocal(operationPreflight, "augmentation_retrain");
    const cv::FileNode summary = storage.root()["summary"];
    summary["accepted_nodes"] >> projection.accepted_nodes;
    summary["rejected_nodes"] >> projection.rejected_nodes;
    summary["parameter_assets_verified"] >> projection.parameter_assets_verified;
    summary["parameter_assets_rejected"] >> projection.parameter_assets_rejected;
    summary["parent_links_present"] >> projection.parent_links_present;
    summary["parent_links_missing"] >> projection.parent_links_missing;
    projection.human_review_status =
        ReadModelLineageStringLocal(summary, "human_review_status");
    int promotionAllowed = 0;
    summary["promotion_allowed"] >> promotionAllowed;
    projection.promotion_allowed = promotionAllowed != 0;
    return;
  }
}

static void RefreshModelWorkflowAssetsLocal(ManualTestContext &context) {
  context.model_workflow_scan_attempted = true;
  context.model_workflow = {};
  CxModelWorkflowProjection &projection = context.model_workflow;
  const std::filesystem::path scanRoot =
      ResolveCxVisionRunPath(context.model_workflow_scan_root);
  std::error_code ec;
  if (!std::filesystem::is_directory(scanRoot, ec) || ec) {
    projection.status = "WORKFLOW_SCAN_ROOT_MISSING";
    return;
  }
  std::vector<std::filesystem::path> manifests;
  std::vector<std::filesystem::path> pending{scanRoot};
  while (!pending.empty()) {
    const std::filesystem::path directory = pending.back();
    pending.pop_back();
    std::error_code directoryError;
    std::filesystem::directory_iterator entries(
        directory, std::filesystem::directory_options::skip_permission_denied,
        directoryError);
    if (directoryError)
      continue;
    for (const std::filesystem::directory_entry &entry : entries) {
      std::error_code entryError;
      if (entry.is_symlink(entryError))
        continue;
      if (entry.is_directory(entryError) && !entryError)
        pending.push_back(entry.path());
      else if (!entryError && entry.is_regular_file(entryError) &&
               entry.path().filename() == "workflow_manifest.json")
        manifests.push_back(entry.path());
    }
  }
  std::sort(manifests.begin(), manifests.end());
  for (auto manifestIt = manifests.rbegin(); manifestIt != manifests.rend();
       ++manifestIt) {
    cv::FileStorage storage(manifestIt->string(), cv::FileStorage::READ);
    if (!storage.isOpened() ||
        ReadModelLineageStringLocal(storage.root(), "schema") !=
            "cxvision.model_workflow.v1")
      continue;
    const cv::FileNode nodes = storage.root()["nodes"];
    if (!nodes.isSeq())
      continue;
    CxModelWorkflowProjection parsed;
    parsed.available = true;
    parsed.workflow_path = manifestIt->string();
    parsed.display_name =
        ReadModelLineageStringLocal(storage.root(), "display_name");
    int promotionAllowed = 0;
    storage.root()["promotion_allowed"] >> promotionAllowed;
    parsed.promotion_allowed = promotionAllowed != 0;
    int readyCount = 0;
    int blockedCount = 0;
    for (const cv::FileNode &node : nodes) {
      CxModelWorkflowUiNode item;
      item.node_id = ReadModelLineageStringLocal(node, "node_id");
      item.display_name = ReadModelLineageStringLocal(node, "display_name");
      item.node_kind = ReadModelLineageStringLocal(node, "node_kind");
      item.parent_node_ids =
          ReadModelLineageStringListLocal(node, "parent_node_ids");
      const std::string assetRef = ReadModelLineageStringLocal(node, "asset_ref");
      const std::string resultRef = ReadModelLineageStringLocal(node, "result_ref");
      const std::string declaredStatus =
          ReadModelLineageStringLocal(node, "declared_status");
      const auto resolveRef = [&](const std::string &ref, std::string &output) {
        if (ref.empty())
          return true;
        const std::filesystem::path resolved =
            (manifestIt->parent_path() / ref).lexically_normal();
        if (!IsPathInsideModelLineageRootLocal(scanRoot, resolved))
          return false;
        output = resolved.string();
        return true;
      };
      if (!resolveRef(assetRef, item.asset_ref) ||
          !resolveRef(resultRef, item.result_ref)) {
        item.status = "WORKFLOW_REFERENCE_OUTSIDE_RUN_ROOT";
      } else {
        std::error_code assetError;
        if (!item.asset_ref.empty() &&
            (!std::filesystem::is_regular_file(item.asset_ref, assetError) ||
             assetError)) {
          item.status = "ASSET_MISSING";
        } else if (!item.result_ref.empty()) {
          std::error_code resultError;
          if (!std::filesystem::is_regular_file(item.result_ref, resultError) ||
              resultError) {
            item.status = "RESULT_PENDING";
          } else {
            cv::FileStorage result(item.result_ref, cv::FileStorage::READ);
            item.status = result.isOpened()
                              ? ReadModelLineageStringLocal(result.root(), "status")
                              : "RESULT_PRESENT_UNREADABLE";
            if (item.status.empty())
              item.status = "RESULT_PRESENT_STATUS_UNDECLARED";
          }
        } else if (!declaredStatus.empty()) {
          item.status = declaredStatus;
        } else {
          item.status = "ASSET_READY";
        }
      }
      item.detail = item.result_ref.empty() ? item.asset_ref : item.result_ref;
      if (item.status == "ASSET_READY" ||
          item.status.find("EXECUTION_PASS") != std::string::npos ||
          item.status == "PENDING_HUMAN_REVIEW")
        ++readyCount;
      else
        ++blockedCount;
      if (!item.node_id.empty() && !item.display_name.empty() &&
          !item.node_kind.empty())
        parsed.nodes.push_back(std::move(item));
    }
    parsed.status = parsed.nodes.empty()
                        ? "WORKFLOW_NODE_SCHEMA_INVALID"
                        : (blockedCount == 0 ? "WORKFLOW_ASSETS_PROJECTED"
                                             : "WORKFLOW_ASSET_GATES_PRESENT");
    projection = std::move(parsed);
    return;
  }
}

static std::string CompactWorkflowLabelLocal(const std::string &value,
                                             std::size_t maximum = 26) {
  return value.size() <= maximum ? value
                                 : value.substr(0, maximum - 3) + "...";
}

static ImU32 WorkflowStatusColorLocal(const std::string &status,
                                      bool structuralError) {
  if (structuralError || status.find("MISSING") != std::string::npos ||
      status.find("OUTSIDE") != std::string::npos ||
      status.find("FAIL") != std::string::npos ||
      status.find("UNREADABLE") != std::string::npos)
    return IM_COL32(198, 70, 70, 255);
  if (status.find("PENDING_HUMAN_REVIEW") != std::string::npos ||
      status.find("HUMAN") != std::string::npos)
    return IM_COL32(218, 150, 35, 255);
  if (status.find("EXECUTION_PASS") != std::string::npos ||
      status.find("RESULT") != std::string::npos)
    return IM_COL32(57, 168, 94, 255);
  return IM_COL32(75, 130, 205, 255);
}

static void DrawModelWorkflowTopologyLocal(ManualTestContext &context) {
  const CxModelWorkflowProjection &workflow = context.model_workflow;
  if (workflow.nodes.empty())
    return;

  std::unordered_map<std::string, std::size_t> indexById;
  for (std::size_t i = 0; i < workflow.nodes.size(); ++i)
    indexById[workflow.nodes[i].node_id] = i;
  std::vector<int> depth(workflow.nodes.size(), -1);
  std::vector<int> visit(workflow.nodes.size(), 0);
  std::vector<bool> structuralError(workflow.nodes.size(), false);
  std::function<int(std::size_t)> resolveDepth = [&](std::size_t index) {
    if (visit[index] == 2)
      return depth[index];
    if (visit[index] == 1) {
      structuralError[index] = true;
      return 0;
    }
    visit[index] = 1;
    int result = 0;
    for (const std::string &parentId : workflow.nodes[index].parent_node_ids) {
      const auto parent = indexById.find(parentId);
      if (parent == indexById.end()) {
        structuralError[index] = true;
        continue;
      }
      result = std::max(result, resolveDepth(parent->second) + 1);
      structuralError[index] = structuralError[index] ||
                               structuralError[parent->second];
    }
    visit[index] = 2;
    return depth[index] = result;
  };
  int maxDepth = 0;
  for (std::size_t i = 0; i < workflow.nodes.size(); ++i)
    maxDepth = std::max(maxDepth, resolveDepth(i));
  std::vector<std::vector<std::size_t>> levels(
      static_cast<std::size_t>(maxDepth + 1));
  for (std::size_t i = 0; i < workflow.nodes.size(); ++i)
    levels[static_cast<std::size_t>(std::max(0, depth[i]))].push_back(i);

  std::size_t maximumLevelSize = 0;
  for (const std::vector<std::size_t> &level : levels)
    maximumLevelSize = std::max(maximumLevelSize, level.size());
  const float columnWidth = 178.0f;
  const float canvasWidth = 108.0f + (maxDepth + 1) * columnWidth;
  const float canvasHeight =
      std::max(220.0f, 55.0f + static_cast<float>(maximumLevelSize) * 54.0f);

  ImGui::BeginChild("##configured_model_workflow_canvas",
                    ImVec2(0.0f, 260.0f), true,
                    ImGuiWindowFlags_HorizontalScrollbar);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton("##configured_model_workflow_surface",
                         ImVec2(canvasWidth, canvasHeight));
  ImDrawList *draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(origin, origin + ImVec2(canvasWidth, canvasHeight),
                      IM_COL32(27, 31, 38, 255), 4.0f);

  std::vector<ImVec2> positions(workflow.nodes.size());
  for (std::size_t level = 0; level < levels.size(); ++level) {
    const std::vector<std::size_t> &members = levels[level];
    for (std::size_t ordinal = 0; ordinal < members.size(); ++ordinal) {
      const float ratio = static_cast<float>(ordinal + 1) /
                          static_cast<float>(members.size() + 1);
      positions[members[ordinal]] =
          origin + ImVec2(38.0f + level * columnWidth,
                           28.0f + ratio * (canvasHeight - 56.0f));
    }
  }
  for (std::size_t child = 0; child < workflow.nodes.size(); ++child) {
    for (const std::string &parentId : workflow.nodes[child].parent_node_ids) {
      const auto parent = indexById.find(parentId);
      if (parent == indexById.end())
        continue;
      const ImVec2 from = positions[parent->second];
      const ImVec2 to = positions[child];
      draw->AddBezierCubic(from + ImVec2(9.0f, 0.0f),
                           from + ImVec2(72.0f, 0.0f),
                           to - ImVec2(72.0f, 0.0f),
                           to - ImVec2(9.0f, 0.0f),
                           IM_COL32(166, 177, 193, 235), 2.0f);
    }
  }
  for (std::size_t i = 0; i < workflow.nodes.size(); ++i) {
    const CxModelWorkflowUiNode &node = workflow.nodes[i];
    const ImVec2 point = positions[i];
    const bool selected = context.selected_model_workflow_node ==
                          static_cast<int>(i);
    const ImU32 color = WorkflowStatusColorLocal(node.status, structuralError[i]);
    if (selected)
      draw->AddCircle(point, 13.0f, IM_COL32(246, 248, 250, 255), 0, 2.0f);
    draw->AddCircleFilled(point, 8.0f, color);
    draw->AddCircle(point, 8.0f, IM_COL32(18, 22, 27, 255), 0, 1.0f);
    const std::string label = CompactWorkflowLabelLocal(node.display_name, 23);
    const std::string status = CompactWorkflowLabelLocal(node.status, 20);
    const ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
    const ImVec2 statusSize = ImGui::CalcTextSize(status.c_str());
    draw->AddText(point + ImVec2(-labelSize.x * 0.5f, -27.0f),
                  IM_COL32(242, 245, 248, 255), label.c_str());
    draw->AddText(point + ImVec2(-statusSize.x * 0.5f, 13.0f), color,
                  status.c_str());

    ImGui::SetCursorScreenPos(point - ImVec2(10.0f, 20.0f));
    ImGui::PushID(static_cast<int>(i));
    if (ImGui::InvisibleButton("workflow_node", ImVec2(146.0f, 40.0f)))
      context.selected_model_workflow_node = static_cast<int>(i);
    if (ImGui::IsItemHovered()) {
      ImGui::BeginTooltip();
      ImGui::Text("%s", node.display_name.c_str());
      ImGui::Text("status: %s", node.status.c_str());
      ImGui::TextWrapped("asset: %s", node.asset_ref.c_str());
      ImGui::TextWrapped("result: %s", node.result_ref.c_str());
      if (structuralError[i])
        ImGui::TextColored(ImVec4(1, .35f, .35f, 1),
                           "Graph reference is missing or cyclic.");
      ImGui::EndTooltip();
    }
    ImGui::PopID();
  }
  ImGui::SetCursorScreenPos(origin + ImVec2(0.0f, canvasHeight));
  // Draw edges last so branch strokes remain visible across label regions.
  for (std::size_t child = 0; child < workflow.nodes.size(); ++child) {
    for (const std::string &parentId : workflow.nodes[child].parent_node_ids) {
      const auto parent = indexById.find(parentId);
      if (parent == indexById.end())
        continue;
      const ImVec2 from = positions[parent->second];
      const ImVec2 to = positions[child];
      draw->AddBezierCubic(from + ImVec2(9.0f, 0.0f),
                           from + ImVec2(72.0f, 0.0f),
                           to - ImVec2(72.0f, 0.0f),
                           to - ImVec2(9.0f, 0.0f),
                           IM_COL32(190, 200, 214, 245), 2.4f);
    }
  }

  ImGui::EndChild();
  ImGui::TextDisabled("Node + text topology: left-to-right lineage with vertical branch fans. Blue: configured/readable | green: execution result | yellow: human review pending | red: invalid or missing reference.");
}
static void DrawModelWorkflowProjectionLocal(ManualTestContext &context) {
  ImGui::SeparatorText("Configured Model Workflow / Actual Results");
  ImGui::SetNextItemWidth(-164.0f);
  InputTextString("Workflow scan root", context.model_workflow_scan_root);
  ImGui::SameLine();
  if (ImGui::Button("Reload Workflow")) {
    RefreshModelWorkflowAssetsLocal(context);
    context.selected_model_workflow_node = -1;
  }
  const CxModelWorkflowProjection &workflow = context.model_workflow;
  if (!workflow.available) {
    ImGui::TextDisabled("No valid workflow manifest was discovered.");
    return;
  }
  ImGui::Text("%s", workflow.display_name.c_str());
  ImGui::Text("%s | promotion: not allowed by UI", workflow.status.c_str());
  DrawModelWorkflowTopologyLocal(context);

  const int selected = context.selected_model_workflow_node;
  const CxModelWorkflowUiNode *node =
      selected >= 0 && selected < static_cast<int>(workflow.nodes.size())
          ? &workflow.nodes[static_cast<std::size_t>(selected)] : nullptr;
  ImGui::SeparatorText("Selected workflow node: assets and gates");
  if (node == nullptr) {
    ImGui::TextDisabled("Select a node in the main topology to browse its assets.");
  } else {
    ImGui::Text("%s [%s]", node->display_name.c_str(), node->node_kind.c_str());
    ImGui::Text("status: %s", node->status.c_str());
    ImGui::TextWrapped("asset: %s", node->asset_ref.c_str());
    ImGui::TextWrapped("result: %s", node->result_ref.c_str());
    if (ImGui::Button("Verify Node Parameter Assets##workflow")) {
      std::error_code ec;
      const bool assetOk = !node->asset_ref.empty() &&
          std::filesystem::is_regular_file(node->asset_ref, ec) && !ec;
      context.model_workflow_operation_status =
          assetOk ? "WORKFLOW_NODE_ASSET_READABLE" : "ASSET_MISSING";
      context.model_workflow_operation_message =
          "Asset readability only; it is not training, validation, human acceptance, or promotion.";
    }
    ImGui::SameLine();
    const bool hasParent = !node->parent_node_ids.empty();
    if (!hasParent)
      ImGui::BeginDisabled();
    if (ImGui::Button("Rollback Review To Parent##workflow")) {
      bool parentFound = false;
      for (const std::string &parentId : node->parent_node_ids) {
        for (std::size_t i = 0; i < workflow.nodes.size(); ++i) {
          if (workflow.nodes[i].node_id == parentId) {
            context.selected_model_workflow_node = static_cast<int>(i);
            parentFound = true;
            break;
          }
        }
        if (parentFound)
          break;
      }
      context.model_workflow_operation_status =
          parentFound ? "ROLLBACK_REVIEW_SELECTION_READY"
                      : "PARENT_REFERENCE_MISSING";
      context.model_workflow_operation_message =
          parentFound ? "Browsing selection returned to an upstream node; weights and review records are unchanged."
                      : "No referenced parent node was found in this workflow manifest.";
    }
    if (!hasParent)
      ImGui::EndDisabled();
    ImGui::TextDisabled("Rollback changes browsing focus only; Compare, Retrain, and Promote stay blocked until their separate frozen-data, evidence, and human gates are recorded.");
    ImGui::TextWrapped("%s | %s", context.model_workflow_operation_status.c_str(),
                       context.model_workflow_operation_message.c_str());
  }
  ImGui::TextDisabled("This is the only topology. Nodes, order, links, and asset references are discovered from workflow_manifest.json; this view never changes weights, review decisions, or promotion.");
}

static void DrawModelLineageTopologyLocal(ManualTestContext &context) {
  ImGui::SeparatorText("Model node asset details");
  if (context.model_lineage_nodes.empty()) {
    ImGui::TextDisabled("No valid model-node assets are available.");
    return;
  }
  ImGui::TextDisabled("The workflow above is the sole topology. This section lists legacy model assets for inspection only.");
  for (const CxModelLineageUiNode &node : context.model_lineage_nodes)
    ImGui::BulletText("%s | %s | %s", node.display_name.c_str(),
                      node.node_kind.c_str(), node.gate_status.c_str());
}
void ViewController::drawTorchTrainingImageSetWindow() {
  const ImGuiViewport *viewport = ImGui::GetMainViewport();
  const ImVec2 workPos = viewport->WorkPos;
  const ImVec2 workSize = viewport->WorkSize;
  ImGui::SetNextWindowPos(
      ImVec2(workPos.x + workSize.x * 0.5f,
             workPos.y + workSize.y * 0.5f),
      ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(
      ImVec2(std::min(720.0f, std::max(420.0f, workSize.x - 32.0f)),
             std::min(760.0f, std::max(420.0f, workSize.y - 32.0f))),
      ImGuiCond_Appearing);
  if (!ImGui::Begin("Torch Training Image Set", nullptr,
                    ImGuiWindowFlags_NoCollapse)) {
    ImGui::End();
    return;
  }

  ApplyAiGuiFocusHere(
      AiGuiDestination::TorchTrainingImageSet,
      "Torch Training Image Set > dataset actions and image rails");
  PollYoloTrainingProcessLocal(m_manualTest);
  if (!m_manualTest.torch_training_latest_scan_attempted) {
    m_manualTest.torch_training_latest_scan_attempted = true;
    std::string loadReason;
    if (LoadLatestYoloTrainingRunLocal(m_manualTest.torch_training_run,
                                       loadReason)) {
      m_manualTest.torch_training_image_status =
          m_manualTest.torch_training_run.status;
      m_manualTest.torch_training_image_reason = loadReason;
    }
  }
  ImGui::TextWrapped(
      "Training/validation/test image rails for Torch evidence review. "
      "Click a thumbnail to load it into Image View. Labels are operator "
      "evidence, not model quality PASS.");
  ImGui::Separator();

  const std::filesystem::path yoloTrainingDefaults = ResolveWorkspaceFile(
      "cxparser/cxscript/module/cximage/tests/"
      "yolov8n_cpp_detection_full_coverage_plan.json");
  DrawYoloTrainingTuningControlsLocal(
      m_manualTest, yoloTrainingDefaults, "torch_training_image_set");
  ImGui::Separator();

  const CxTorchTrainingRunBinding &trainingRun =
      m_manualTest.torch_training_run;
  ImGui::SeparatorText("Primary Training / Image Set Actions");
  ImGui::Text("dataset: %zu images | train/val/test actions are asset-backed",
              m_manualTest.torch_training_images.size());
  if (ImGui::Button("Sync Selected Evidence Case##training_primary")) {
    SyncTorchTrainingImageSetFromEvidenceSelection();
  }
  ImGui::SameLine();
  if (ImGui::Button("Add Current As Train##training_primary")) {
    AddTorchTrainingImageFromPath(m_manualTest.image_file_path,
                                  m_manualTest.active_image_id, "train",
                                  "unlabeled", "current_image");
  }
  ImGui::SameLine();
  if (ImGui::Button("Add Current As Val##training_primary")) {
    AddTorchTrainingImageFromPath(m_manualTest.image_file_path,
                                  m_manualTest.active_image_id, "val",
                                  "unlabeled", "current_image");
  }
  if (ImGui::Button("Add Manifest Images##training_primary")) {
    ClearTorchTrainingImageSetForEvidenceSyncLocal(
        m_manualTest, "manual rebuild from selected evidence manifest images");
    int count = AddHDReferenceImageSetForCurrentSelection();
    if (count == 0) {
      for (const ManifestImageItem &item : m_manualTest.image_manifest_items) {
        if (!item.image_path.empty()) {
          AddTorchTrainingImageFromPath(item.image_path, item.image_id, "train",
                                        "unlabeled", "manifest");
          ++count;
        }
      }
      m_manualTest.torch_training_image_status = "MANIFEST_IMAGES_ADDED";
      m_manualTest.torch_training_image_reason =
          "incrementally added manifest images: " + std::to_string(count);
    }
  }
  ImGui::SetNextItemWidth(-1.0f);
  InputTextString("Image path##training_primary",
                  m_manualTest.torch_training_new_image_path);
  if (ImGui::Button("Add Path As Train##training_primary")) {
    AddTorchTrainingImageFromPath(m_manualTest.torch_training_new_image_path,
                                  "", "train", "unlabeled", "manual_path");
  }
  ImGui::SameLine();
  if (ImGui::Button("Add Path As Validation##training_primary")) {
    AddTorchTrainingImageFromPath(m_manualTest.torch_training_new_image_path,
                                  "", "val", "unlabeled", "manual_path");
  }
  ImGui::SameLine();
  if (ImGui::Button("Add Path As Holdout##training_primary")) {
    AddTorchTrainingImageFromPath(m_manualTest.torch_training_new_image_path,
                                  "", "test", "unlabeled", "manual_path");
  }
  const int primaryImageCount =
      static_cast<int>(m_manualTest.torch_training_images.size());
  const int primarySelectedImage =
      m_manualTest.selected_torch_training_image;
  if (primaryImageCount == 0)
    ImGui::BeginDisabled();
  if (ImGui::Button("Load First Image##training_primary")) {
    std::string loadReason;
    if (!LoadTorchTrainingImageIntoAnnotationView(0, loadReason)) {
      m_manualTest.torch_training_image_status = "IMAGE_LOAD_FAILED";
      m_manualTest.torch_training_image_reason = loadReason;
    }
  }
  ImGui::SameLine();
  if (primarySelectedImage <= 0)
    ImGui::BeginDisabled();
  if (ImGui::Button("Previous Image##training_primary")) {
    std::string loadReason;
    LoadTorchTrainingImageIntoAnnotationView(primarySelectedImage - 1,
                                             loadReason);
  }
  if (primarySelectedImage <= 0)
    ImGui::EndDisabled();
  ImGui::SameLine();
  if (primarySelectedImage < 0 ||
      primarySelectedImage + 1 >= primaryImageCount)
    ImGui::BeginDisabled();
  if (ImGui::Button("Next Image##training_primary")) {
    std::string loadReason;
    LoadTorchTrainingImageIntoAnnotationView(primarySelectedImage + 1,
                                             loadReason);
  }
  if (primarySelectedImage < 0 ||
      primarySelectedImage + 1 >= primaryImageCount)
    ImGui::EndDisabled();
  if (primaryImageCount == 0)
    ImGui::EndDisabled();
  if (m_manualTest.selected_torch_training_image >= 0 &&
      m_manualTest.selected_torch_training_image < primaryImageCount) {
    TorchTrainingImageItem &selected =
        m_manualTest.torch_training_images[static_cast<std::size_t>(
            m_manualTest.selected_torch_training_image)];
    ImGui::Text("selected: %d / %d | split: %s | label: %s",
                m_manualTest.selected_torch_training_image + 1,
                primaryImageCount, selected.split.c_str(),
                selected.label.c_str());
    ImGui::TextWrapped("source: %s", selected.image_path.c_str());
    static const char *kGeometryClassNames[] = {
        "arc", "circle", "ellipse", "line", "open_curve", "polygon",
        "closed_curve"};
    ImGui::SetNextItemWidth(132.0f);
    ImGui::InputInt("Annotation class id##training_primary",
                    &m_manualTest.torch_training_annotation_class_id);
    m_manualTest.torch_training_annotation_class_id = std::clamp(
        m_manualTest.torch_training_annotation_class_id, 0, 6);
    ImGui::SameLine();
    ImGui::TextDisabled("%s",
                        kGeometryClassNames[
                            m_manualTest.torch_training_annotation_class_id]);
    if (ImGui::Button("Apply Class To Current Shapes##training_primary")) {
      CaptureCurrentTorchTrainingAnnotationState();
      for (TorchTrainingAnnotationShapeSnapshot &shape :
           selected.annotation_shapes)
        shape.class_id = m_manualTest.torch_training_annotation_class_id;
      selected.annotation_status = selected.annotation_shapes.empty()
                                       ? "unlabeled"
                                       : "editing";
      m_manualTest.torch_training_image_status = "ANNOTATION_CLASS_APPLIED";
      m_manualTest.torch_training_image_reason =
          "class_id=" +
          std::to_string(m_manualTest.torch_training_annotation_class_id) +
          " applied to current image shapes=" +
          std::to_string(selected.annotation_shapes.size());
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Current Annotations##training_primary")) {
      selected.annotation_shapes.clear();
      selected.annotation_shape_count = 0;
      selected.annotation_overlay_count = 0;
      selected.annotation_status = "unlabeled";
      m_annotationLayer.Clear();
      m_annotationLayer.ClearShapeElements();
      CancelAnnotationCreate();
      m_annotationLayer.SetActiveToolIndex(-1);
      m_imageToolMode = ImageToolMode::PointerPan;
      m_manualTest.torch_training_image_status = "ANNOTATIONS_CLEARED";
      m_manualTest.torch_training_image_reason =
          "cleared current image annotations; source image unchanged";
    }
  } else {
    ImGui::TextDisabled("selected: none / %d", primaryImageCount);
  }
  ImGui::SeparatorText("Selected Image / Visible YOLO Inference");
  ImGui::SetNextItemWidth(-1.0f);
  InputTextString("Model manifest##selected_image_inference",
                  m_manualTest.torch_training_inference_model_manifest);
  ImGui::SetNextItemWidth(140.0f);
  InputTextString("Device##selected_image_inference",
                  m_manualTest.torch_training_inference_device);
  if (ImGui::Button("Run YOLO On Selected Image##training_primary")) {
    std::string inferenceReason;
    RunYoloV8nSelectedImageInferenceFromGui(inferenceReason);
  }
  ImGui::SameLine();
  const bool hasSelectedSource =
      m_manualTest.selected_torch_training_image >= 0 &&
      m_manualTest.selected_torch_training_image <
          static_cast<int>(m_manualTest.torch_training_images.size());
  if (!hasSelectedSource)
    ImGui::BeginDisabled();
  if (ImGui::Button("Restore Selected Source Image##training_primary")) {
    std::string restoreReason;
    if (!LoadTorchTrainingImageIntoAnnotationView(
            m_manualTest.selected_torch_training_image, restoreReason)) {
      m_manualTest.torch_training_inference_status =
          "SOURCE_IMAGE_RESTORE_FAILED";
      m_manualTest.torch_training_inference_reason = restoreReason;
    }
  }
  if (!hasSelectedSource)
    ImGui::EndDisabled();
  ImGui::Text("inference: %s | detections: %d | forward: %.3f ms",
              m_manualTest.torch_training_inference_status.c_str(),
              m_manualTest.torch_training_inference_detection_count,
              m_manualTest.torch_training_inference_runtime_ms);
  ImGui::TextWrapped("%s",
                     m_manualTest.torch_training_inference_reason.c_str());
  if (!m_manualTest.torch_training_inference_overlay_ref.empty())
    ImGui::TextWrapped("overlay: %s",
                       m_manualTest.torch_training_inference_overlay_ref.c_str());
  if (!m_manualTest.torch_training_inference_result_ref.empty())
    ImGui::TextWrapped("result: %s",
                       m_manualTest.torch_training_inference_result_ref.c_str());
  if (!m_manualTest.torch_training_inference_candidates_ref.empty())
    ImGui::TextWrapped(
        "candidates: %s",
        m_manualTest.torch_training_inference_candidates_ref.c_str());
  ImGui::TextDisabled(
      "The source image is immutable. Inference writes a separate overlay; "
      "Restore Selected Source Image returns to annotation view.");
  ImGui::SeparatorText("Dataset Export / Incremental Training");
  if (ImGui::Button("Export Training Dataset##training_primary")) {
    CaptureCurrentTorchTrainingAnnotationState();
    std::string packagePath;
    std::string exportReason;
    if (!ExportTorchTrainingLabelPackage(packagePath, exportReason)) {
      m_manualTest.debug_status = "TORCH_LABEL_PACKAGE_EXPORT_FAIL";
      m_manualTest.debug_reason = exportReason;
    } else {
      m_manualTest.debug_status = "TORCH_LABEL_PACKAGE_TO_VERIFY";
      m_manualTest.debug_reason = exportReason;
    }
  }
  const bool hasPrimaryAugmentation =
      m_manualTest.geometry_aug_include_brightness ||
      m_manualTest.geometry_aug_include_local_gap ||
      m_manualTest.geometry_aug_include_jagged_cut ||
      m_manualTest.geometry_aug_include_line_break;
  ImGui::Checkbox("Require source-disjoint validation##training_primary",
                  &m_manualTest.geometry_aug_require_source_disjoint_validation);
  ImGui::TextDisabled(
      "Requires each source asset to declare train or validation; a source cannot generate both.");
  if (!hasPrimaryAugmentation)
    ImGui::BeginDisabled();
  if (ImGui::Button("Generate Augmented Train/Test Set##training_primary")) {
    std::string generationReason;
    RunGeometryAugmentationTrainingPrepFromGui(generationReason);
  }
  if (!hasPrimaryAugmentation)
    ImGui::EndDisabled();
  ImGui::SameLine();
  bool primaryTrainingAssetMatched = false;
  const bool primaryTrainingAssetEnabled =
      EvidenceSelectionTrainingEnabledLocal(
          m_manualTest.current_evidence_selection,
          primaryTrainingAssetMatched);
  const bool primaryTrainingBlocked =
      m_manualTest.torch_training_process_running ||
      (primaryTrainingAssetMatched && !primaryTrainingAssetEnabled);
  if (primaryTrainingBlocked)
    ImGui::BeginDisabled();
  if (ImGui::Button("Run C++ Incremental Training##training_primary")) {
    std::string trainingReason;
    RunYoloV8nIncrementalTrainingFromGui(trainingReason);
  }
  if (primaryTrainingBlocked)
    ImGui::EndDisabled();
  if (primaryTrainingAssetMatched && !primaryTrainingAssetEnabled)
    ImGui::TextDisabled(
        "Training blocked: selected Evidence asset has training_enabled=0.");
  ImGui::SameLine();
  if (ImGui::Button("Reload Latest Run##training_primary")) {
    std::string loadReason;
    if (LoadLatestYoloTrainingRunLocal(m_manualTest.torch_training_run,
                                       loadReason)) {
      m_manualTest.torch_training_image_status =
          m_manualTest.torch_training_run.status;
      m_manualTest.torch_training_image_reason = loadReason;
    } else {
      m_manualTest.torch_training_image_status = "TRAINING_TRACE_MISSING";
      m_manualTest.torch_training_image_reason = loadReason;
    }
  }
  ImGui::Text("training: %s | epochs %d/%d | curve points %zu",
              trainingRun.status.empty() ? "PENDING_RUN" : trainingRun.status.c_str(),
              trainingRun.completed_epochs, trainingRun.configured_epochs,
              trainingRun.epochs.size());
  DrawYoloTrainingFeedbackLocal(trainingRun, "training_image_set_feedback");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(132.0f);
  ImGui::InputInt("Fixed Epoch Axis##training_primary",
                  &m_manualTest.torch_training_epoch_axis_capacity);
  m_manualTest.torch_training_epoch_axis_capacity = std::max(
      1, m_manualTest.torch_training_epoch_axis_capacity);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(112.0f);
  ImGui::InputFloat("Loss Axis Max##training_primary",
                    &m_manualTest.torch_training_loss_axis_maximum, 1.0f,
                    5.0f, "%.3f");
  m_manualTest.torch_training_loss_axis_maximum = std::max(
      0.001f, m_manualTest.torch_training_loss_axis_maximum);
  ImGui::TextDisabled(
      "The horizontal axis remains 0-%d; future epochs are intentionally blank.",
      std::max(trainingRun.configured_epochs,
               m_manualTest.torch_training_epoch_axis_capacity));
  if (trainingRun.HasRealMultiEpochSeries()) {
    DrawEpochLossCurveLocal("##training_primary_fixed_epoch_curve", trainingRun,
                            0, 112.0f,
                            m_manualTest.torch_training_epoch_axis_capacity,
                            m_manualTest.torch_training_loss_axis_maximum);
  } else {
    ImGui::TextDisabled("Loss chart is shown only for a runtime-produced multi-epoch series.");
  }
  ImGui::Separator();

  ImGui::Text("active_case: %s", m_manualTest.active_case_id.empty()
                                     ? "-"
                                     : m_manualTest.active_case_id.c_str());
  ImGui::Text("selected_evidence: %s",
              m_manualTest.current_evidence_selection.case_id.empty()
                  ? "-"
                  : m_manualTest.current_evidence_selection.case_id.c_str());
  ImGui::Text("status: %s", m_manualTest.torch_training_image_status.c_str());
  ImGui::TextWrapped("reason: %s",
                     m_manualTest.torch_training_image_reason.c_str());

  const CxEvidenceSelectionSnapshot &controlledSelection =
      m_manualTest.current_evidence_selection;
  if (controlledSelection.valid &&
      NormalizeEvidenceToolTypeLocal(controlledSelection.tool) == "TorchTask") {
    ImGui::SeparatorText("Selected Controlled Case Parameters / Configuration");
    ImGui::Text("admission: %s",
                controlledSelection.admission_status.empty()
                    ? "REFERENCE_ONLY"
                    : controlledSelection.admission_status.c_str());
    DrawControlledCaseConfigurationAssetLocal(
        "Training config", controlledSelection.training_config_ref);
    DrawControlledCaseConfigurationAssetLocal(
        "Model manifest", controlledSelection.model_manifest_ref);
    DrawControlledCaseConfigurationAssetLocal(
        "Inference config", controlledSelection.inference_config_ref);
    DrawControlledCaseConfigurationAssetLocal(
        "Quality policy", controlledSelection.quality_policy_ref);
    DrawControlledCaseConfigurationAssetLocal(
        "Label ontology", controlledSelection.ontology_ref);
    DrawControlledCaseConfigurationAssetLocal(
        "Failure samples", controlledSelection.failure_samples_ref);
    if (!controlledSelection.rollback_model_ref.empty())
      ImGui::TextWrapped("Rollback model: %s",
                         controlledSelection.rollback_model_ref.c_str());
  }

  if (!m_manualTest.validation_dataset_scan_attempted)
    RefreshValidationDatasetAssetsLocal(m_manualTest);
  if (!m_manualTest.model_lineage_operation_selftest_scan_attempted)
    RefreshModelLineageOperationSelfTestAssetsLocal(m_manualTest);
  if (!m_manualTest.model_workflow_scan_attempted)
    RefreshModelWorkflowAssetsLocal(m_manualTest);
  ImGui::SeparatorText("Validation Dataset / Inference Gate");
  ImGui::SetNextItemWidth(-120.0f);
  InputTextString("Validation scan root",
                  m_manualTest.validation_dataset_scan_root);
  ImGui::SameLine();
  if (ImGui::Button("Reload Validation Assets"))
    RefreshValidationDatasetAssetsLocal(m_manualTest);
  const CxValidationDatasetReadiness &validationDataset =
      m_manualTest.validation_dataset;
  const bool validationReady =
      validationDataset.status == "VALIDATION_DATASET_FROZEN_READY_FOR_INFERENCE";
  ImGui::TextColored(
      validationReady ? ImVec4(0.36f, 0.82f, 0.49f, 1.0f)
                      : ImVec4(0.95f, 0.72f, 0.22f, 1.0f),
      "%s", validationDataset.status.c_str());
  ImGui::Text("train: %d | validation: %d | missing assets: %d | shared source lineage: %d",
              validationDataset.train_sample_count,
              validationDataset.validation_sample_count,
              validationDataset.missing_required_asset_count,
              validationDataset.source_lineage_overlap_count);
  ImGui::TextWrapped("%s", validationDataset.reason.c_str());
  if (!validationDataset.dataset_manifest_path.empty())
    ImGui::TextWrapped("dataset manifest: %s",
                       validationDataset.dataset_manifest_path.c_str());
  ImGui::TextDisabled(
      "A frozen image/label/overlay set is only the data gate. Paired model inference, instance aggregation, metrics and human review remain separate gates.");

  DrawModelWorkflowProjectionLocal(m_manualTest);

  ImGui::SeparatorText("Latest Automated Node-Chain Record");
  ImGui::SetNextItemWidth(-170.0f);
  InputTextString("Operation record scan root",
                  m_manualTest.model_lineage_operation_selftest_scan_root);
  ImGui::SameLine();
  if (ImGui::Button("Reload Node-Chain Record"))
    RefreshModelLineageOperationSelfTestAssetsLocal(m_manualTest);
  const CxModelLineageOperationSelfTestProjection &operationSelfTest =
      m_manualTest.model_lineage_operation_selftest;
  if (!operationSelfTest.available) {
    ImGui::TextDisabled("No asset-backed automatic node-chain record is available yet.");
  } else {
    const bool selftestComplete = operationSelfTest.status ==
        "MODEL_LINEAGE_OPERATION_SELFTEST_COMPLETE_WITH_GATES";
    ImGui::TextColored(
        selftestComplete ? ImVec4(0.36f, 0.82f, 0.49f, 1.0f)
                         : ImVec4(0.95f, 0.45f, 0.25f, 1.0f),
        "%s", operationSelfTest.status.c_str());
    ImGui::Text("nodes: %d accepted / %d rejected | parameter assets: %d pass / %d fail | parent links: %d ready / %d missing",
                operationSelfTest.accepted_nodes,
                operationSelfTest.rejected_nodes,
                operationSelfTest.parameter_assets_verified,
                operationSelfTest.parameter_assets_rejected,
                operationSelfTest.parent_links_present,
                operationSelfTest.parent_links_missing);
    ImGui::Text("compare: %s | retrain: %s",
                operationSelfTest.compare_preflight.c_str(),
                operationSelfTest.augmentation_retrain_preflight.c_str());
    ImGui::Text("human review: %s | promotion: not allowed",
                operationSelfTest.human_review_status.c_str());
    ImGui::TextWrapped("validation gate: %s — %s",
                       operationSelfTest.validation_gate_status.c_str(),
                       operationSelfTest.validation_gate_reason.c_str());
    ImGui::TextWrapped("record: %s", operationSelfTest.report_path.c_str());
  }

  if (!m_manualTest.model_lineage_scan_attempted)
    RefreshModelLineageAssetsLocal(m_manualTest);
  if (!m_manualTest.model_performance_profile_scan_attempted)
    RefreshModelPerformanceProfileAssetsLocal(m_manualTest);
  int lineageValidationImageCount = 0;
  int lineageTestImageCount = 0;
  for (const TorchTrainingImageItem &item : m_manualTest.torch_training_images) {
    if (item.split == "val")
      ++lineageValidationImageCount;
    else if (item.split == "test")
      ++lineageTestImageCount;
  }
  ImGui::SeparatorText("Model & Evidence Lineage / State Tree");
  ImGui::TextDisabled(
      "Use this tree to trace composition, validation evidence, human gates "
      "and a safe parent-model return path. It reports asset facts; it does "
      "not promote a model automatically.");
  DrawModelLineageTopologyLocal(m_manualTest);
  ImGui::SeparatorText("Selected Node / Branch Operations");
  const int selectedNodeIndex = m_manualTest.selected_model_lineage_node;
  const CxModelLineageUiNode *selectedNode =
      selectedNodeIndex >= 0 && selectedNodeIndex <
              static_cast<int>(m_manualTest.model_lineage_nodes.size())
          ? &m_manualTest.model_lineage_nodes[static_cast<std::size_t>(
                selectedNodeIndex)]
          : nullptr;
  if (selectedNode == nullptr) {
    ImGui::TextDisabled("Select a topology node before testing parameters or branch operations.");
  } else {
    ImGui::Text("Selected: %s | %s", selectedNode->display_name.c_str(),
                selectedNode->gate_status.c_str());
    if (ImGui::Button("Verify Node Parameter Assets")) {
      std::string operationReason;
      if (ValidateModelNodeAssetsLocal(*selectedNode, operationReason)) {
        m_manualTest.model_lineage_operation_status =
            "MODEL_NODE_PARAMETER_ASSET_PASS";
      } else {
        m_manualTest.model_lineage_operation_status =
            "MODEL_NODE_PARAMETER_ASSET_FAIL";
      }
      m_manualTest.model_lineage_operation_message = operationReason;
    }
    ImGui::SameLine();
    const bool hasParent = !selectedNode->parent_model_ids.empty();
    if (!hasParent)
      ImGui::BeginDisabled();
    if (ImGui::Button("Rollback Review To Parent")) {
      for (const std::string &parentId : selectedNode->parent_model_ids) {
        for (std::size_t index = 0;
             index < m_manualTest.model_lineage_nodes.size(); ++index) {
          if (m_manualTest.model_lineage_nodes[index].model_id == parentId) {
            m_manualTest.selected_model_lineage_node = static_cast<int>(index);
            m_manualTest.model_lineage_operation_status =
                "ROLLBACK_REVIEW_SELECTION_READY";
            m_manualTest.model_lineage_operation_message =
                "review selection returned to parent; weights are unchanged";
            break;
          }
        }
      }
    }
    if (!hasParent)
      ImGui::EndDisabled();
    ImGui::SameLine();
    const bool validationReady = validationDataset.status ==
        "VALIDATION_DATASET_FROZEN_READY_FOR_INFERENCE";
    if (!validationReady)
      ImGui::BeginDisabled();
    if (ImGui::Button("Compare Parent / Candidate")) {
      m_manualTest.model_lineage_operation_status =
          "PAIRED_INFERENCE_BINDING_REQUIRED";
      m_manualTest.model_lineage_operation_message =
          "validation assets are ready; bind the selected package and paired inference plan";
    }
    if (!validationReady)
      ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Prepare Augmentation Retrain Branch")) {
      std::string operationReason;
      if (RunGeometryAugmentationTrainingPrepFromGui(operationReason)) {
        m_manualTest.model_lineage_operation_status =
            "AUGMENTATION_RETRAIN_BRANCH_PREPARED";
      } else {
        m_manualTest.model_lineage_operation_status =
            "AUGMENTATION_RETRAIN_BRANCH_BLOCKED";
      }
      m_manualTest.model_lineage_operation_message = operationReason;
    }
    ImGui::TextWrapped("operation: %s | %s",
                       m_manualTest.model_lineage_operation_status.c_str(),
                       m_manualTest.model_lineage_operation_message.c_str());
    ImGui::TextDisabled(
        "Verify checks concrete asset composition. Rollback changes review selection only. Compare and retrain require their own data and execution gates.");
  }
  const auto compactAssetName = [](const std::string &path) {
    return path.empty() ? std::string("ASSET_MISSING")
                        : std::filesystem::path(path).filename().string();
  };
  const auto selectLineageNode = [this](std::size_t index) {
    m_manualTest.selected_model_lineage_node = static_cast<int>(index);
  };
  const auto findLineageNode = [this](const std::string &modelId) {
    for (std::size_t i = 0; i < m_manualTest.model_lineage_nodes.size(); ++i) {
      if (m_manualTest.model_lineage_nodes[i].model_id == modelId)
        return static_cast<int>(i);
    }
    return -1;
  };
  const CxModelPerformanceProfile &lineageProfile =
      m_manualTest.model_performance_profile;
  if (ImGui::TreeNodeEx("##model_evidence_tree_root",
                        ImGuiTreeNodeFlags_DefaultOpen |
                            ImGuiTreeNodeFlags_OpenOnArrow,
                        "Model lineage (%zu valid, %zu rejected)",
                        m_manualTest.model_lineage_nodes.size(),
                        m_manualTest.model_lineage_rejected.size())) {
    if (ImGui::TreeNodeEx("##model_evidence_tree_dataset",
                          ImGuiTreeNodeFlags_DefaultOpen |
                              ImGuiTreeNodeFlags_OpenOnArrow,
                          "Data & training context")) {
      ImGui::BulletText("Training images: %d | validation images: %d | test images: %d",
                        trainingRun.train_sample_count,
                        lineageValidationImageCount, lineageTestImageCount);
      ImGui::BulletText("Training run: %s | epochs %d/%d",
                        trainingRun.status.empty() ? "PENDING_RUN"
                                                   : trainingRun.status.c_str(),
                        trainingRun.completed_epochs, trainingRun.configured_epochs);
      ImGui::BulletText("Fixed curve axis: 0-%d (display-only)",
                        std::max(trainingRun.configured_epochs,
                                 m_manualTest.torch_training_epoch_axis_capacity));
      ImGui::TreePop();
    }

    for (std::size_t i = 0; i < m_manualTest.model_lineage_nodes.size(); ++i) {
      const CxModelLineageUiNode &node = m_manualTest.model_lineage_nodes[i];
      const bool selected =
          m_manualTest.selected_model_lineage_node == static_cast<int>(i);
      const ImGuiTreeNodeFlags nodeFlags =
          ImGuiTreeNodeFlags_OpenOnArrow |
          (selected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None);
      const bool open = ImGui::TreeNodeEx(
          ("##model_evidence_node_" + std::to_string(i)).c_str(), nodeFlags,
          "%s  [%s]", node.display_name.c_str(), node.gate_status.c_str());
      if (ImGui::IsItemClicked())
        selectLineageNode(i);
      if (!open)
        continue;

      if (ImGui::TreeNode("Composition")) {
        ImGui::BulletText("Type: %s | task: %s | architecture: %s",
                          node.node_kind.c_str(), node.task.c_str(),
                          node.architecture_id.c_str());
        ImGui::BulletText("Output contract: %s", node.output_contract.c_str());
        ImGui::BulletText("Dataset binding: %s",
                          compactAssetName(node.dataset_binding_path).c_str());
        ImGui::BulletText("Training plan: %s",
                          compactAssetName(node.training_plan_path).c_str());
        ImGui::BulletText("Checkpoint: %s | hash: %s",
                          compactAssetName(node.checkpoint_path).c_str(),
                          node.checkpoint_hash_status.c_str());
        ImGui::TreePop();
      }
      if (ImGui::TreeNode("Validation & evidence")) {
        ImGui::BulletText("Metric bundle: %s",
                          compactAssetName(node.metric_bundle_path).c_str());
        ImGui::BulletText("Instance evidence: inspect the metric bundle and linked Evidence items before acceptance.");
        ImGui::BulletText("Human review: %s",
                          node.inference_difference_reviewed
                              ? "HUMAN_CONFIRMED"
                              : "PENDING_HUMAN_REVIEW");
        ImGui::TreePop();
      }
      if (ImGui::TreeNode("Performance & gate")) {
        ImGui::BulletText("Promotion: %s", node.promotion_status.c_str());
        ImGui::BulletText("Parent selection: %s | freeze: %s | inference: %s",
                          node.parent_gate_satisfied ? "READY" : "PENDING",
                          node.freeze_policy_reviewed ? "CONFIRMED" : "PENDING",
                          node.inference_difference_reviewed ? "CONFIRMED" : "PENDING");
        ImGui::BulletText("Runtime profile: %s",
                          lineageProfile.available
                              ? lineageProfile.status.c_str()
                              : "PENDING_PERFORMANCE_PROFILE");
        ImGui::TreePop();
      }
      if (ImGui::TreeNode("Recovery / learning point")) {
        if (node.parent_model_ids.empty()) {
          ImGui::TextDisabled("This is a root model. It is a possible return baseline after human confirmation.");
        } else {
          for (const std::string &parentId : node.parent_model_ids) {
            const int parentIndex = findLineageNode(parentId);
            if (parentIndex >= 0) {
              const CxModelLineageUiNode &parent =
                  m_manualTest.model_lineage_nodes[static_cast<std::size_t>(parentIndex)];
              if (ImGui::SmallButton(("Return to parent: " + parent.display_name +
                                     "##lineage_parent_" + std::to_string(i) + "_" +
                                     std::to_string(parentIndex)).c_str()))
                m_manualTest.selected_model_lineage_node = parentIndex;
            } else {
              ImGui::BulletText("Parent asset is unavailable: %s", parentId.c_str());
            }
          }
          ImGui::TextDisabled("Returning only changes this review selection; it never replaces a weight or promotes a model.");
        }
        ImGui::TreePop();
      }
      ImGui::TreePop();
    }

    if (!m_manualTest.model_lineage_rejected.empty() &&
        ImGui::TreeNode("Rejected or incomplete assets")) {
      for (const CxModelLineageRejectedAsset &rejected :
           m_manualTest.model_lineage_rejected)
        ImGui::BulletText("%s", rejected.reason.c_str());
      ImGui::TreePop();
    }
    ImGui::TreePop();
  }

  if (ImGui::CollapsingHeader("Model Lineage Asset Audit (details)")) {
    ImGui::SetNextItemWidth(-120.0f);
    InputTextString("Scan root", m_manualTest.model_lineage_scan_root);
    ImGui::SameLine();
    if (ImGui::Button("Refresh"))
      RefreshModelLineageAssetsLocal(m_manualTest);

    ImGui::Text("status: %s", m_manualTest.model_lineage_scan_status.c_str());
    ImGui::Text("%s", m_manualTest.model_lineage_scan_reason.c_str());
    if (!m_manualTest.model_lineage_scan_debug_path.empty())
      ImGui::TextWrapped("scan report: %s",
                         m_manualTest.model_lineage_scan_debug_path.c_str());

    if (ImGui::BeginTable("model_lineage_nodes", 7,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_ScrollY,
                          ImVec2(-1.0f, 190.0f))) {
      ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 112.0f);
      ImGui::TableSetupColumn("Task", ImGuiTableColumnFlags_WidthFixed, 75.0f);
      ImGui::TableSetupColumn("Capability",
                              ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Branch", ImGuiTableColumnFlags_WidthFixed,
                              100.0f);
      ImGui::TableSetupColumn("Parent", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableSetupColumn("Gate", ImGuiTableColumnFlags_WidthFixed, 150.0f);
      ImGui::TableHeadersRow();
      for (std::size_t i = 0; i < m_manualTest.model_lineage_nodes.size();
           ++i) {
        const CxModelLineageUiNode &node =
            m_manualTest.model_lineage_nodes[i];
        std::string capabilityText;
        for (std::size_t c = 0; c < node.capabilities.size(); ++c) {
          if (c > 0)
            capabilityText += ", ";
          capabilityText += node.capabilities[c];
        }
        std::string parentText;
        for (std::size_t p = 0; p < node.parent_model_ids.size(); ++p) {
          if (p > 0)
            parentText += ", ";
          parentText += node.parent_model_ids[p];
        }
        if (parentText.empty())
          parentText = "-";

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        const bool selected =
            m_manualTest.selected_model_lineage_node == static_cast<int>(i);
        if (ImGui::Selectable(node.display_name.c_str(), selected,
                              ImGuiSelectableFlags_SpanAllColumns))
          m_manualTest.selected_model_lineage_node = static_cast<int>(i);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextWrapped("%s", node.node_kind.c_str());
        ImGui::TableSetColumnIndex(2);
        ImGui::TextWrapped("%s", node.task.c_str());
        ImGui::TableSetColumnIndex(3);
        ImGui::TextWrapped("%s", capabilityText.c_str());
        ImGui::TableSetColumnIndex(4);
        ImGui::TextWrapped("%s",
                           node.branch_id.empty() ? "-" : node.branch_id.c_str());
        ImGui::TableSetColumnIndex(5);
        ImGui::TextWrapped("%s", parentText.c_str());
        ImGui::TableSetColumnIndex(6);
        const bool ready = node.gate_status == "GATE_READY" ||
                           node.gate_status == "PARENT_READY";
        ImGui::TextColored(
            ready ? ImVec4(0.36f, 0.82f, 0.49f, 1.0f)
                  : ImVec4(0.95f, 0.72f, 0.22f, 1.0f),
            "%s", node.gate_status.c_str());
      }
      ImGui::EndTable();
    }

    if (m_manualTest.selected_model_lineage_node >= 0 &&
        m_manualTest.selected_model_lineage_node <
            static_cast<int>(m_manualTest.model_lineage_nodes.size())) {
      const CxModelLineageUiNode &node =
          m_manualTest.model_lineage_nodes[static_cast<std::size_t>(
              m_manualTest.selected_model_lineage_node)];
      ImGui::SeparatorText("Selected Model");
      ImGui::Text("%s", node.display_name.c_str());
      ImGui::TextWrapped("model_id: %s", node.model_id.c_str());
      ImGui::TextWrapped("architecture: %s | output: %s",
                         node.architecture_id.c_str(),
                         node.output_contract.c_str());
      ImGui::TextWrapped("ontology: %s",
                         node.class_ontology_path.c_str());
      ImGui::TextWrapped("checkpoint: %s", node.checkpoint_path.c_str());
      ImGui::TextWrapped("checkpoint hash: %s", node.checkpoint_hash.c_str());
      ImGui::TextWrapped("hash status: %s",
                         node.checkpoint_hash_status.c_str());
      ImGui::TextWrapped("promotion: %s", node.promotion_status.c_str());
      ImGui::TextWrapped(
          "admission: %s | parent usage: %s | development eligible: %s",
          node.admission_status.empty() ? "-" : node.admission_status.c_str(),
          node.parent_usage.empty() ? "FORMAL_ONLY" : node.parent_usage.c_str(),
          node.development_parent_eligible ? "yes" : "no");
      ImGui::TextWrapped(
          "formal registry write: %s",
          node.production_registry_write_allowed ? "allowed by asset"
                                                 : "blocked by asset");
      if (node.node_kind == "development_parent")
        ImGui::TextColored(
            ImVec4(0.95f, 0.72f, 0.22f, 1.0f),
            "Development trial only: this node can never imply APPROVED or ACTIVE.");

      if (ImGui::BeginTable("model_lineage_review_gates", 3,
                            ImGuiTableFlags_Borders |
                                ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Human gate",
                                ImGuiTableColumnFlags_WidthFixed, 165.0f);
        ImGui::TableSetupColumn("Status",
                                ImGuiTableColumnFlags_WidthFixed, 175.0f);
        ImGui::TableSetupColumn("Evidence",
                                ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        const auto drawGate = [](const char *label, const char *status,
                                 const std::string &path) {
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::TextUnformatted(label);
          ImGui::TableSetColumnIndex(1);
          ImGui::TextWrapped("%s", status);
          ImGui::TableSetColumnIndex(2);
          ImGui::TextWrapped("%s", path.empty() ? "-" : path.c_str());
        };
        const bool isRelease = node.node_kind == "release";
        drawGate("Parent model selection",
                 (isRelease ? node.parent_selection_reviewed
                            : node.parent_gate_satisfied)
                     ? "HUMAN_CONFIRMED"
                     : "PENDING_HUMAN_REVIEW",
                 node.parent_selection_review_path);
        drawGate("Freeze execution",
                 isRelease ? "NOT_APPLICABLE"
                           : (node.freeze_policy_reviewed
                                  ? "HUMAN_CONFIRMED"
                                  : "PENDING_HUMAN_REVIEW"),
                 node.freeze_policy_review_path);
        drawGate("Inference difference",
                 isRelease ? "NOT_APPLICABLE"
                           : (node.inference_difference_reviewed
                                  ? "HUMAN_CONFIRMED"
                                  : "PENDING_HUMAN_REVIEW"),
                 node.inference_difference_review_path);
        ImGui::EndTable();
      }

      InputTextMultilineString(
          "Selection reason",
          m_manualTest.model_lineage_parent_selection_reason,
          ImVec2(-1.0f, 55.0f));
      if (ImGui::Button("Confirm Selected As Parent")) {
        std::string reviewReason;
        if (WriteParentModelSelectionReviewLocal(m_manualTest, node,
                                                 reviewReason)) {
          m_manualTest.model_lineage_parent_selection_status =
              "HUMAN_CONFIRMED";
          m_manualTest.model_lineage_parent_selection_message = reviewReason;
          RefreshModelLineageAssetsLocal(m_manualTest);
        } else {
          m_manualTest.model_lineage_parent_selection_status =
              "REVIEW_WRITE_FAILED";
          m_manualTest.model_lineage_parent_selection_message = reviewReason;
        }
      }
      ImGui::SameLine();
      ImGui::Text("%s",
                  m_manualTest.model_lineage_parent_selection_status.c_str());
      if (!m_manualTest.model_lineage_parent_selection_message.empty())
        ImGui::TextWrapped("%s",
                           m_manualTest
                               .model_lineage_parent_selection_message.c_str());
      ImGui::TextWrapped("gate reason: %s", node.gate_reason.c_str());
    }

    if (!m_manualTest.model_lineage_rejected.empty() &&
        ImGui::TreeNode("Rejected assets")) {
      for (const CxModelLineageRejectedAsset &rejected :
           m_manualTest.model_lineage_rejected) {
        ImGui::BulletText("%s", rejected.reason.c_str());
        ImGui::TextWrapped("%s", rejected.path.c_str());
      }
      ImGui::TreePop();
    }
    ImGui::Separator();
  }

  if (ImGui::CollapsingHeader("Model Performance Profile / Manual Review",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::SetNextItemWidth(-120.0f);
    InputTextString("Performance scan root",
                    m_manualTest.model_performance_profile_scan_root);
    ImGui::SameLine();
    if (ImGui::Button("Reload Performance Profile"))
      RefreshModelPerformanceProfileAssetsLocal(m_manualTest);

    const CxModelPerformanceProfile &profile =
        m_manualTest.model_performance_profile;
    if (!profile.available) {
      ImGui::TextColored(ImVec4(0.95f, 0.72f, 0.22f, 1.0f),
                         "PENDING_PERFORMANCE_PROFILE");
      ImGui::TextWrapped(
          "%s", profile.reason.empty()
                    ? "Run asset-selected paired inference to produce performance_profile.json."
                    : profile.reason.c_str());
      ImGui::TextWrapped(
          "No quality or promotion conclusion is derived from a missing profile.");
    } else {
      ImGui::Text("status: %s", profile.status.c_str());
      ImGui::TextWrapped("scope: %s", profile.profile_scope.c_str());
      ImGui::Text("device: %s | samples: %d | service init: %.3f ms",
                  profile.requested_device.c_str(), profile.sample_count,
                  profile.service_initialize_ms);
      ImGui::TextWrapped("asset: %s", profile.profile_path.c_str());
      if (ImGui::BeginTable("model_performance_profile", 7,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Stage",
                                ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Min ms");
        ImGui::TableSetupColumn("Mean ms");
        ImGui::TableSetupColumn("P50 ms");
        ImGui::TableSetupColumn("P95 ms");
        ImGui::TableSetupColumn("P99 ms");
        ImGui::TableSetupColumn("Max ms");
        ImGui::TableHeadersRow();
        for (const CxModelPerformanceStage &stage : profile.stages) {
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::TextUnformatted(stage.name.c_str());
          ImGui::TableSetColumnIndex(1);
          ImGui::Text("%.3f", stage.min_ms);
          ImGui::TableSetColumnIndex(2);
          ImGui::Text("%.3f", stage.mean_ms);
          ImGui::TableSetColumnIndex(3);
          ImGui::Text("%.3f", stage.p50_ms);
          ImGui::TableSetColumnIndex(4);
          ImGui::Text("%.3f", stage.p95_ms);
          ImGui::TableSetColumnIndex(5);
          ImGui::Text("%.3f", stage.p99_ms);
          ImGui::TableSetColumnIndex(6);
          ImGui::Text("%.3f", stage.max_ms);
        }
        ImGui::EndTable();
      }
      ImGui::TextWrapped(
          "service_execute is full runtime service latency. model_forward is runtime-reported forward latency. residual_pipeline is the measured remainder (preprocess, model load, postprocess and evidence writes); it is not a guessed substage split.");
      ImGui::TextWrapped(
          "This profile is execution evidence only. Human review and promotion remain independent and disabled.");
    }
    ImGui::Separator();
  }

  if (ImGui::CollapsingHeader("Augmented YOLOv8-n Training Prep",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Checkbox("Require source-disjoint validation",
                    &m_manualTest.geometry_aug_require_source_disjoint_validation);
    ImGui::TextDisabled(
        "Strict mode blocks generation unless train/validation source assets are declared and class coverage matches.");
    ImGui::Checkbox("Brightness", &m_manualTest.geometry_aug_include_brightness);
    if (m_manualTest.geometry_aug_include_brightness) {
      ImGui::SetNextItemWidth(160.0f);
      ImGui::SliderFloat("Train brightness scale",
                         &m_manualTest.geometry_aug_train_brightness_scale,
                         0.10f, 1.20f, "%.2f");
      ImGui::SetNextItemWidth(160.0f);
      ImGui::SliderFloat("Test brightness scale",
                         &m_manualTest.geometry_aug_test_brightness_scale,
                         0.10f, 1.20f, "%.2f");
    }
    ImGui::Checkbox("Local gap",
                    &m_manualTest.geometry_aug_include_local_gap);
    ImGui::SameLine();
    ImGui::Checkbox("Jagged cut",
                    &m_manualTest.geometry_aug_include_jagged_cut);
    ImGui::SameLine();
    ImGui::Checkbox("Line break",
                    &m_manualTest.geometry_aug_include_line_break);
    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderInt("Gap width px", &m_manualTest.geometry_aug_gap_width_px,
                     2, 80);
    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderInt("Gap height px", &m_manualTest.geometry_aug_gap_height_px,
                     2, 80);
    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderInt("Jagged px", &m_manualTest.geometry_aug_jagged_px, 1,
                     30);
    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderInt("Line break px",
                     &m_manualTest.geometry_aug_line_break_px, 2, 80);
    const std::filesystem::path yoloTrainingDefaults = ResolveWorkspaceFile(
        "cxparser/cxscript/module/cximage/tests/"
        "yolov8n_cpp_detection_full_coverage_plan.json");
    DrawYoloTrainingTuningControlsLocal(
        m_manualTest, yoloTrainingDefaults, "torch_runtime_evidence");

    const bool hasAugSelection =
        m_manualTest.geometry_aug_include_brightness ||
        m_manualTest.geometry_aug_include_local_gap ||
        m_manualTest.geometry_aug_include_jagged_cut ||
        m_manualTest.geometry_aug_include_line_break;
    if (!hasAugSelection)
      ImGui::BeginDisabled();
    if (ImGui::Button("Generate Augmented Train/Test Set")) {
      std::string generationReason;
      RunGeometryAugmentationTrainingPrepFromGui(generationReason);
    }
    if (!hasAugSelection)
      ImGui::EndDisabled();
    ImGui::SameLine();
    bool runtimeTrainingAssetMatched = false;
    const bool runtimeTrainingAssetEnabled =
        EvidenceSelectionTrainingEnabledLocal(
            m_manualTest.current_evidence_selection,
            runtimeTrainingAssetMatched);
    const bool runtimeTrainingBlocked =
        m_manualTest.torch_training_process_running ||
        (runtimeTrainingAssetMatched && !runtimeTrainingAssetEnabled);
    if (runtimeTrainingBlocked)
      ImGui::BeginDisabled();
    if (ImGui::Button("Run C++ Incremental Training")) {
      std::string trainingReason;
      RunYoloV8nIncrementalTrainingFromGui(trainingReason);
    }
    if (runtimeTrainingBlocked)
      ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip(
          "Runs the asset-discovered YOLOv8-n dataset through the current "
          "C++/LibTorch lifecycle. The GUI remains busy until training ends.");
    ImGui::SameLine();
    if (ImGui::Button("Reload Latest Run")) {
      std::string loadReason;
      if (LoadLatestYoloTrainingRunLocal(m_manualTest.torch_training_run,
                                         loadReason)) {
        m_manualTest.torch_training_image_status =
            m_manualTest.torch_training_run.status;
        m_manualTest.torch_training_image_reason = loadReason;
      } else {
        m_manualTest.torch_training_image_status = "TRAINING_TRACE_MISSING";
        m_manualTest.torch_training_image_reason = loadReason;
      }
    }

    ImGui::Text("prep_status: %s",
                m_manualTest.geometry_aug_run_status.c_str());
    ImGui::TextWrapped("prep_reason: %s",
                       m_manualTest.geometry_aug_run_reason.c_str());
    if (!m_manualTest.geometry_aug_output_dir.empty())
      ImGui::TextWrapped("output_dir: %s",
                         m_manualTest.geometry_aug_output_dir.c_str());
    if (!m_manualTest.geometry_aug_dataset_manifest_path.empty())
      ImGui::TextWrapped("dataset_manifest: %s",
                         m_manualTest.geometry_aug_dataset_manifest_path.c_str());
    if (!m_manualTest.geometry_aug_training_request_path.empty())
      ImGui::TextWrapped(
          "training_request: %s",
          m_manualTest.geometry_aug_training_request_path.c_str());
    ImGui::Separator();
  }

  if (trainingRun.available || m_manualTest.torch_training_process_running) {
    ImGui::SeparatorText("Current C++ Incremental Training");
    ImGui::Text("%s | epochs %d / %d | curve points %zu | batches/epoch %d",
                trainingRun.status.c_str(), trainingRun.completed_epochs,
                trainingRun.configured_epochs, trainingRun.epochs.size(),
                trainingRun.batches_per_epoch);
    DrawYoloTrainingFeedbackLocal(trainingRun,
                                  "torch_runtime_training_feedback");
    const float liveProgress =
        trainingRun.configured_epochs > 0
            ? std::clamp(static_cast<float>(trainingRun.completed_epochs) /
                             trainingRun.configured_epochs,
                         0.0f, 1.0f)
            : 0.0f;
    ImGui::ProgressBar(liveProgress, ImVec2(-1.0f, 0.0f));
    if (!trainingRun.incremental_model_path.empty())
      ImGui::TextWrapped("Model: %s",
                         trainingRun.incremental_model_path.c_str());
    if (trainingRun.HasRealMultiEpochSeries()) {
      ImGui::TextUnformatted("Total Loss / Epoch (full run, fixed X axis)");
      DrawEpochLossCurveLocal("##training_window_primary_loss_epoch_curve",
                              trainingRun, 0, 125.0f,
                              m_manualTest.torch_training_epoch_axis_capacity,
                              m_manualTest.torch_training_loss_axis_maximum);
    }
    if (trainingRun.epochs.size() >= 2) {
      int increasingSteps = 0;
      double largestIncrease = 0.0;
      for (std::size_t i = 1; i < trainingRun.epochs.size(); ++i) {
        const double delta = trainingRun.epochs[i].total_loss -
                             trainingRun.epochs[i - 1].total_loss;
        if (delta > 0.0) {
          ++increasingSteps;
          largestIncrease = std::max(largestIncrease, delta);
        }
      }
      ImGui::Text("Epoch fluctuation: %d upward steps / %zu transitions | "
                  "largest increase %.6g",
                  increasingSteps, trainingRun.epochs.size() - 1,
                  largestIncrease);
      ImGui::TextDisabled(
          "Each point is the mean of all configured batches in one epoch; "
          "the recent-epoch chart rescales the Y axis to expose small changes.");
    }
  }
  const CxEvidenceSelectionSnapshot &chainSelection =
      m_manualTest.current_evidence_selection;
  const EvidenceReferencePolicyLocal chainPolicy =
      EvidenceReferencePolicyForSelectionLocal(chainSelection);

  const TorchTrainingImageItem *chainImage = nullptr;
  if (m_manualTest.selected_torch_training_image >= 0 &&
      m_manualTest.selected_torch_training_image <
          static_cast<int>(m_manualTest.torch_training_images.size())) {
    chainImage =
        &m_manualTest.torch_training_images[static_cast<std::size_t>(
            m_manualTest.selected_torch_training_image)];
  }

  std::string chainImagePath = chainSelection.image_path;
  if (chainImage != nullptr && !chainImage->image_path.empty())
    chainImagePath = chainImage->image_path;
  std::error_code chainImageError;
  const bool chainImageBound =
      !chainImagePath.empty() &&
      std::filesystem::is_regular_file(ResolveWorkspaceFile(chainImagePath),
                                       chainImageError);

  int chainTrainCount = 0;
  int chainValCount = 0;
  int chainTestCount = 0;
  std::size_t chainAnnotationCount = 0;
  for (const TorchTrainingImageItem &item :
       m_manualTest.torch_training_images) {
    if (item.split == "train")
      ++chainTrainCount;
    else if (item.split == "val")
      ++chainValCount;
    else if (item.split == "test")
      ++chainTestCount;
    chainAnnotationCount += item.annotation_shapes.size();
  }
  chainAnnotationCount =
      std::max(chainAnnotationCount, chainSelection.annotations.size());

  const auto chainContains = [](std::string value,
                                const std::string &token) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                     return static_cast<char>(std::tolower(ch));
                   });
    return value.find(token) != std::string::npos;
  };
  const bool chainTrainingBlocked =
      chainPolicy.matched && !chainPolicy.training_enabled;
  const bool chainDatasetExported =
      chainContains(m_manualTest.debug_status, "label_package_to_verify") ||
      chainContains(m_manualTest.debug_status, "dataset_export_ready");
  const bool chainBindingPending =
      chainContains(chainPolicy.binding_status, "pending");
  const bool chainModelBindingMissing =
      chainSelection.parent_model_ref.empty() ||
      chainSelection.child_model_ref.empty();

  const std::string chainReferenceStatus =
      !chainSelection.valid
          ? "PENDING_SELECTION"
          : (!chainPolicy.matched
                 ? "PENDING_BINDING"
                 : (chainImageBound ? "READY_TO_VERIFY" : "ASSET_MISSING"));
  const std::string chainAnnotationStatus =
      chainAnnotationCount > 0 ? "DRAFT_TO_VERIFY"
                               : "PENDING_HUMAN_REVIEW";
  const std::string chainDatasetStatus =
      chainDatasetExported
          ? "READY_TO_VERIFY"
          : (m_manualTest.torch_training_images.empty() ? "PENDING_SYNC"
                                                        : "STAGED");
  const std::string chainBaseTrainingStatus =
      chainTrainingBlocked
          ? "BLOCKED_POLICY"
          : (trainingRun.available
                 ? (trainingRun.status.empty() ? "READY_TO_VERIFY"
                                               : trainingRun.status)
                 : "PENDING_RUN");
  const std::string chainIncrementalStatus =
      chainTrainingBlocked
          ? "BLOCKED_POLICY"
          : (trainingRun.available
                 ? (trainingRun.status.empty() ? "READY_TO_VERIFY"
                                               : trainingRun.status)
                 : (!chainDatasetExported ? "PENDING_DATASET"
                                          : "PENDING_RUN"));
  const std::string chainInferenceStatus =
      (chainBindingPending || chainModelBindingMissing)
          ? "PENDING_BINDING"
          : "PENDING_RUN";
  const std::string chainClosureStatus =
      (chainBindingPending || chainModelBindingMissing ||
       chainContains(chainSelection.workflow_status, "pending"))
          ? "PENDING_BINDING"
          : "PENDING_HUMAN_REVIEW";

  if (ImGui::CollapsingHeader("Evidence Model Test Chain")) {
  ImGui::TextWrapped("Evidence/audit detail follows the primary operation and visual panels.");
  ImGui::TextWrapped(
      "Observed asset and runtime state only. PENDING, BLOCKED, STAGED and "
      "draft stages have not executed the downstream model operation.");
  if (chainPolicy.matched) {
    ImGui::Text("track: %s | task: %s",
                chainPolicy.case_track.empty() ? "-"
                                               : chainPolicy.case_track.c_str(),
                chainPolicy.task.empty() ? "-" : chainPolicy.task.c_str());
  }

  if (ImGui::BeginTable("evidence_model_test_chain", 4,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable,
                        ImVec2(-1.0f, 250.0f))) {
    ImGui::TableSetupColumn("Stage", ImGuiTableColumnFlags_WidthFixed, 118.0f);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 145.0f);
    ImGui::TableSetupColumn("Evidence", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Allowed control",
                            ImGuiTableColumnFlags_WidthFixed, 165.0f);
    ImGui::TableHeadersRow();

    const auto drawChainRow = [](const char *stage, const std::string &status,
                                 const std::string &evidence,
                                 const char *control) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(stage);
      ImGui::TableSetColumnIndex(1);
      ImVec4 statusColor(0.95f, 0.72f, 0.22f, 1.0f);
      if (status.find("READY") != std::string::npos)
        statusColor = ImVec4(0.36f, 0.82f, 0.49f, 1.0f);
      else if (status.find("MISSING") != std::string::npos ||
               status.find("FAIL") != std::string::npos)
        statusColor = ImVec4(0.95f, 0.34f, 0.30f, 1.0f);
      ImGui::TextColored(statusColor, "%s", status.c_str());
      ImGui::TableSetColumnIndex(2);
      ImGui::TextWrapped("%s", evidence.empty() ? "-" : evidence.c_str());
      ImGui::TableSetColumnIndex(3);
      ImGui::TextWrapped("%s", control);
    };

    const std::string referenceEvidence =
        (chainPolicy.case_track.empty() ? std::string("-")
                                        : chainPolicy.case_track) +
        " | " +
        (chainImagePath.empty()
             ? std::string("no image")
             : std::filesystem::path(chainImagePath).filename().string());
    const std::string annotationEvidence =
        (chainPolicy.annotation_contract_id.empty()
             ? std::string("contract unbound")
             : chainPolicy.annotation_contract_id) +
        " | shapes=" + std::to_string(chainAnnotationCount);
    const std::string datasetEvidence =
        "images=" +
        std::to_string(m_manualTest.torch_training_images.size()) +
        " | train=" + std::to_string(chainTrainCount) +
        " val=" + std::to_string(chainValCount) +
        " test=" + std::to_string(chainTestCount);
    const std::string modelEvidence =
        chainPolicy.model_track_id.empty() ? "model track unbound"
                                           : chainPolicy.model_track_id;
    const std::string incrementalEvidence =
        "parent=" +
        (chainSelection.parent_model_ref.empty()
             ? std::string("unbound")
             : chainSelection.parent_model_ref) +
        " | child=" +
        (chainSelection.child_model_ref.empty()
             ? std::string("unbound")
             : chainSelection.child_model_ref);
    const std::string inferenceEvidence =
        (chainPolicy.evaluator_id.empty() ? std::string("evaluator unbound")
                                          : chainPolicy.evaluator_id) +
        " | " +
        (chainPolicy.binding_status.empty()
             ? std::string("binding status unavailable")
             : chainPolicy.binding_status);
    const std::string closureEvidence =
        (chainSelection.workflow_id.empty() ? std::string("workflow unbound")
                                            : chainSelection.workflow_id) +
        " | " +
        (chainSelection.workflow_status.empty()
             ? std::string("status unavailable")
             : chainSelection.workflow_status);

    drawChainRow("Reference image", chainReferenceStatus, referenceEvidence,
                 "Sync Selected Evidence Case");
    drawChainRow("Annotation", chainAnnotationStatus, annotationEvidence,
                 "Annotation tools");
    drawChainRow("Dataset", chainDatasetStatus, datasetEvidence,
                 "Export Training Dataset");
    drawChainRow("Base training", chainBaseTrainingStatus, modelEvidence,
                 chainTrainingBlocked ? "Disabled by asset policy"
                                      : "Training run binding");
    drawChainRow("Incremental training", chainIncrementalStatus,
                 incrementalEvidence,
                 chainTrainingBlocked ? "Disabled by asset policy"
                                      : "Run C++ Incremental Training");
    drawChainRow("Inference", chainInferenceStatus, inferenceEvidence,
                 chainInferenceStatus == "PENDING_BINDING"
                     ? "No executable control"
                     : "Run bound inference");
    drawChainRow("Diagnostic closure", chainClosureStatus, closureEvidence,
                 "Manual Review / Evidence");
    ImGui::EndTable();
  }

  if (!chainPolicy.label_semantics.empty())
    ImGui::TextWrapped("Annotation semantics: %s",
                       chainPolicy.label_semantics.c_str());
  ImGui::Separator();
  }
  if (trainingRun.available &&
      ImGui::CollapsingHeader("Training Run Details")) {
    ImGui::Text("status: %s", trainingRun.status.c_str());
    ImGui::Text("optimizer: %s | learning rate: %.8g",
                trainingRun.optimizer.c_str(), trainingRun.learning_rate);
    ImGui::Text("schedule: %s | min LR: %.8g | weight decay: %.8g",
                trainingRun.lr_schedule.c_str(), trainingRun.min_learning_rate,
                trainingRun.weight_decay);
    ImGui::Text("loss weights box/class/DFL/mask: %.4g / %.4g / %.4g / %.4g",
                trainingRun.box_loss_weight, trainingRun.class_loss_weight,
                trainingRun.dfl_loss_weight, trainingRun.mask_loss_weight);
    ImGui::Text("epochs: %d / %d | samples: %d | instances: %d",
                trainingRun.completed_epochs, trainingRun.configured_epochs,
                trainingRun.train_sample_count,
                trainingRun.train_instance_count);
    const float progress =
        trainingRun.configured_epochs > 0
            ? std::clamp(static_cast<float>(trainingRun.completed_epochs) /
                             trainingRun.configured_epochs,
                         0.0f, 1.0f)
            : 0.0f;
    ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));
    ImGui::Text("curve points: %zu | batches/epoch: %d",
                trainingRun.epochs.size(), trainingRun.batches_per_epoch);
    ImGui::TextWrapped("training result: %s",
                       trainingRun.training_trace_path.c_str());
    if (!trainingRun.incremental_model_path.empty())
      ImGui::TextWrapped("incremental model: %s",
                         trainingRun.incremental_model_path.c_str());

    const bool hasRealTrainingCurve = trainingRun.HasRealMultiEpochSeries();
    if (!hasRealTrainingCurve) {
      const std::string curveStatus = trainingRun.TrainingCurveStatus();
      ImGui::TextColored(ImVec4(1.0f, 0.76f, 0.28f, 1.0f),
                         "learning_curve_status: %s", curveStatus.c_str());
      ImGui::TextDisabled(
          "Training request/prep assets do not prove incremental learning; "
          "bind a real multi-epoch trace before plotting loss curves.");
    }

    if (hasRealTrainingCurve) {
      ImGui::Text("Total Loss / Epoch");
      DrawEpochLossCurveLocal("##training_window_loss_epoch_curve",
                              trainingRun, 0, 170.0f,
                              m_manualTest.torch_training_epoch_axis_capacity,
                              m_manualTest.torch_training_loss_axis_maximum);
    }

    if (ImGui::BeginTable("torch_training_epoch_metrics", 7,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_ScrollY,
                          ImVec2(-1.0f, 150.0f))) {
      ImGui::TableSetupColumn("Epoch");
      ImGui::TableSetupColumn("Total");
      ImGui::TableSetupColumn("Box");
      ImGui::TableSetupColumn("Class");
      ImGui::TableSetupColumn("DFL");
      ImGui::TableSetupColumn("Mask");
      ImGui::TableSetupColumn("LR");
      ImGui::TableHeadersRow();
      for (const CxTorchTrainingEpochMetric &metric : trainingRun.epochs) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%d", metric.epoch);
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%.5f", metric.total_loss);
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%.5f", metric.box_loss);
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%.5f", metric.class_loss);
        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%.5f", metric.dfl_loss);
        ImGui::TableSetColumnIndex(5);
        ImGui::Text("%.5f", metric.mask_loss);
        ImGui::TableSetColumnIndex(6);
        ImGui::Text("%.3g", metric.learning_rate);
      }
      ImGui::EndTable();
    }
  }

  if (!m_annotationLayer.HasActiveDrag() &&
      m_manualTest.selected_torch_training_image >= 0 &&
      m_manualTest.selected_torch_training_image <
          static_cast<int>(m_manualTest.torch_training_images.size())) {
    const TorchTrainingImageItem &selectedItem =
        m_manualTest.torch_training_images[static_cast<std::size_t>(
            m_manualTest.selected_torch_training_image)];
    if (!m_manualTest.image_file_path.empty() &&
        !selectedItem.image_path.empty()) {
      const std::string currentImage =
          ResolveWorkspaceFile(m_manualTest.image_file_path)
              .lexically_normal()
              .string();
      const std::string selectedImage =
          ResolveWorkspaceFile(selectedItem.image_path)
              .lexically_normal()
              .string();
      if (currentImage == selectedImage)
        CaptureCurrentTorchTrainingAnnotationState();
    }
  }

  if (ImGui::Button("Sync Selected Evidence Case")) {
    CXLOG_INFO(
        "TorchTrainingImageSet", "sync_selected_evidence_button", "ui_event",
        "case_id=" + m_manualTest.active_case_id +
            " script_id=" + m_manualTest.current_evidence_selection.script_id);
    SyncTorchTrainingImageSetFromEvidenceSelection();
  }
  ImGui::SameLine();
  if (ImGui::Button("Add Current As Train")) {
    CXLOG_INFO("TorchTrainingImageSet", "add_current_as_train_button",
               "ui_event",
               "image_path=" + m_manualTest.image_file_path +
                   " image_id=" + m_manualTest.active_image_id);
    AddTorchTrainingImageFromPath(m_manualTest.image_file_path,
                                  m_manualTest.active_image_id, "train",
                                  "unlabeled", "current_image");
  }
  ImGui::SameLine();
  if (ImGui::Button("Add Current As Val")) {
    CXLOG_INFO("TorchTrainingImageSet", "add_current_as_val_button", "ui_event",
               "image_path=" + m_manualTest.image_file_path +
                   " image_id=" + m_manualTest.active_image_id);
    AddTorchTrainingImageFromPath(m_manualTest.image_file_path,
                                  m_manualTest.active_image_id, "val",
                                  "unlabeled", "current_image");
  }
  if (ImGui::Button("Add Current As Test")) {
    CXLOG_INFO("TorchTrainingImageSet", "add_current_as_test_button",
               "ui_event",
               "image_path=" + m_manualTest.image_file_path +
                   " image_id=" + m_manualTest.active_image_id);
    AddTorchTrainingImageFromPath(m_manualTest.image_file_path,
                                  m_manualTest.active_image_id, "test",
                                  "unlabeled", "current_image");
  }

  ImGui::SetNextItemWidth(-1.0f);
  InputTextString("Incremental image path",
                  m_manualTest.torch_training_new_image_path);
  if (ImGui::Button("Add Path As Train")) {
    CXLOG_INFO("TorchTrainingImageSet", "add_path_as_train_button", "ui_event",
               "image_path=" + m_manualTest.torch_training_new_image_path);
    AddTorchTrainingImageFromPath(m_manualTest.torch_training_new_image_path,
                                  "", "train", "unlabeled", "manual_path");
  }
  ImGui::SameLine();
  if (ImGui::Button("Add Manifest Images")) {
    CXLOG_INFO(
        "TorchTrainingImageSet", "add_manifest_images_button", "ui_event",
        "case_id=" + m_manualTest.active_case_id +
            " script_id=" + m_manualTest.current_evidence_selection.script_id);
    ClearTorchTrainingImageSetForEvidenceSyncLocal(
        m_manualTest, "manual rebuild from selected evidence manifest images");
    int count = AddHDReferenceImageSetForCurrentSelection();
    if (count == 0) {
      for (const ManifestImageItem &item : m_manualTest.image_manifest_items) {
        if (!item.image_path.empty()) {
          AddTorchTrainingImageFromPath(item.image_path, item.image_id, "train",
                                        "unlabeled", "manifest");
          ++count;
        }
      }
      m_manualTest.torch_training_image_status = "MANIFEST_IMAGES_ADDED";
      m_manualTest.torch_training_image_reason =
          "incrementally added manifest images: " + std::to_string(count);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Export Training Dataset")) {
    CaptureCurrentTorchTrainingAnnotationState();
    std::string packagePath;
    std::string exportReason;
    if (!ExportTorchTrainingLabelPackage(packagePath, exportReason)) {
      m_manualTest.debug_status = "TORCH_LABEL_PACKAGE_EXPORT_FAIL";
      m_manualTest.debug_reason = exportReason;
      CXLOG_ERROR("TorchTrainingImageSet",
                  "training_label_package_export_failed", "FAIL", exportReason);
    } else {
      m_manualTest.debug_status = "TORCH_LABEL_PACKAGE_TO_VERIFY";
      m_manualTest.debug_reason = exportReason;
    }
  }
  ImGui::SameLine();
  bool trainingAssetMatched = false;
  const bool trainingAssetEnabled = EvidenceSelectionTrainingEnabledLocal(
      m_manualTest.current_evidence_selection, trainingAssetMatched);
  const bool trainingBlockedByAsset =
      trainingAssetMatched && !trainingAssetEnabled;
  if (trainingBlockedByAsset)
    ImGui::BeginDisabled();
  const bool runIncrementalTraining =
      ImGui::Button("Train DeepLab Incremental");
  if (trainingBlockedByAsset)
    ImGui::EndDisabled();
  if (runIncrementalTraining) {
    CaptureCurrentTorchTrainingAnnotationState();
    std::string datasetPath;
    std::string operationReason;
    if (!ExportTorchTrainingLabelPackage(datasetPath, operationReason)) {
      m_manualTest.debug_status = "TORCH_INCREMENTAL_DATASET_FAIL";
      m_manualTest.debug_reason = operationReason;
    } else if (m_manualTest.selected_torch_training_image < 0 ||
               m_manualTest.selected_torch_training_image >=
                   static_cast<int>(
                       m_manualTest.torch_training_images.size())) {
      m_manualTest.debug_status = "TORCH_INCREMENTAL_INPUT_FAIL";
      m_manualTest.debug_reason = "no training image is selected";
    } else {
      const TorchTrainingImageItem &selected =
          m_manualTest.torch_training_images[static_cast<std::size_t>(
              m_manualTest.selected_torch_training_image)];
      CxTorchTaskSpec task;
      task.kind = CxTorchTaskKind::Segmentation;
      task.task_id = "torch.train.segmentation.lifecycle_smoke.v1";
      task.case_id = "manual_deeplab_incremental";
      task.manifest_path = datasetPath;
      task.input_image_path = selected.image_path;
      task.output_dir = std::filesystem::path(datasetPath).parent_path() /
                        "deeplab_incremental_run";
      task.requested_device = "cpu";
      task.timeout_ms = 30000;
      CxInferenceResult inference;
      CxTorchExecutionAdapter adapter;
      if (!adapter.Execute(task, inference, operationReason)) {
        m_manualTest.debug_status = "TORCH_INCREMENTAL_TRAIN_FAIL";
        m_manualTest.debug_reason = operationReason;
      } else {
        m_manualTest.debug_status = "TORCH_INCREMENTAL_TRAIN_READY_TO_VERIFY";
        m_manualTest.debug_reason = "result=" + inference.result_ref +
                                    " evidence=" + inference.evidence_ref +
                                    " visual=" + inference.primary_visual_ref;
      }
    }
  }

  if (ImGui::Button("Run EdgeSAM Prompt")) {
    if (m_manualTest.selected_torch_training_image < 0 ||
        m_manualTest.selected_torch_training_image >=
            static_cast<int>(m_manualTest.torch_training_images.size())) {
      m_manualTest.debug_status = "EDGESAM_INPUT_FAIL";
      m_manualTest.debug_reason = "no training image is selected";
    } else {
      const TorchTrainingImageItem &selected =
          m_manualTest.torch_training_images[static_cast<std::size_t>(
              m_manualTest.selected_torch_training_image)];
      cv::Mat image = cv::imread(selected.image_path, cv::IMREAD_UNCHANGED);
      CxTorchTaskSpec task;
      task.kind = CxTorchTaskKind::Segmentation;
      task.task_id = "torch.infer.segmentation.edgesam.v1";
      task.case_id = "manual_edgesam_prompt";
      task.manifest_path =
          ResolveWorkspaceFile("libtorch_module/models/edgesam_3x_torchscript/"
                               "model_manifest.json");
      task.input_image_path = selected.image_path;
      task.output_dir =
          ResolveCxVisionRunPath("cxscript_runs/manual_edgesam_prompt") /
          (CxUnifiedLog::Instance().RunId().empty()
               ? "ui_session"
               : CxUnifiedLog::Instance().RunId());
      task.requested_device = "cpu";
      task.timeout_ms = 30000;
      std::ostringstream prompt;
      prompt << "{\"positive_x\":" << image.cols * 0.5
             << ",\"positive_y\":" << image.rows * 0.5
             << ",\"negative_x\":0,\"negative_y\":0}";
      task.extra_json = prompt.str();
      CxInferenceResult inference;
      CxTorchExecutionAdapter adapter;
      std::string operationReason;
      if (!adapter.Execute(task, inference, operationReason)) {
        m_manualTest.debug_status =
            std::filesystem::exists(task.manifest_path.parent_path() /
                                    "weights/encoder.ts")
                ? "EDGESAM_PROMPT_FAIL"
                : "EDGESAM_PROMPT_PENDING_BINDING";
        m_manualTest.debug_reason = operationReason;
      } else {
        m_manualTest.debug_status = "EDGESAM_PROMPT_READY_TO_VERIFY";
        m_manualTest.debug_reason = "result=" + inference.result_ref +
                                    " evidence=" + inference.evidence_ref +
                                    " visual=" + inference.primary_visual_ref;
      }
    }
  }

  if (m_manualTest.selected_torch_training_image >= 0 &&
      m_manualTest.selected_torch_training_image <
          static_cast<int>(m_manualTest.torch_training_images.size())) {
    TorchTrainingImageItem &item =
        m_manualTest
            .torch_training_images[m_manualTest.selected_torch_training_image];
    ImGui::Separator();
    ImGui::Text("Selected image: %s",
                item.image_id.empty() ? "-" : item.image_id.c_str());
    ImGui::TextWrapped("%s", item.image_path.c_str());
    ImGui::Text("Annotation: %s | shapes=%d | overlays=%d",
                item.annotation_status.c_str(), item.annotation_shape_count,
                item.annotation_overlay_count);
    if (!item.annotation_shapes.empty() &&
        ImGui::BeginTable("torch_annotation_shapes", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg,
                          ImVec2(-1.0f, 110.0f))) {
      ImGui::TableSetupColumn("Class");
      ImGui::TableSetupColumn("Shape");
      ImGui::TableSetupColumn("Bounds");
      ImGui::TableHeadersRow();
      for (const TorchTrainingAnnotationShapeSnapshot &shape :
           item.annotation_shapes) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%d", shape.class_id);
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(shape.shape_kind.c_str());
        ImGui::TableSetColumnIndex(2);
        if (shape.points_xy.size() >= 6) {
          ImGui::Text("%.0f,%.0f - %.0f,%.0f", shape.points_xy[0],
                      shape.points_xy[1], shape.points_xy[4],
                      shape.points_xy[5]);
        } else {
          ImGui::TextUnformatted("-");
        }
      }
      ImGui::EndTable();
    }

    if (ImGui::Button("label: good")) {
      item.label = "good";
      item.annotation_status = "editing";
      CXLOG_INFO("TorchTrainingImageSet", "training_image_label_changed",
                 "ui_event",
                 "label=good image_id=" + item.image_id +
                     " image_path=" + item.image_path);
    }
    ImGui::SameLine();
    if (ImGui::Button("label: anomaly")) {
      item.label = "anomaly";
      item.annotation_status = "editing";
      CXLOG_INFO("TorchTrainingImageSet", "training_image_label_changed",
                 "ui_event",
                 "label=anomaly image_id=" + item.image_id +
                     " image_path=" + item.image_path);
    }
    ImGui::SameLine();
    if (ImGui::Button("label: unlabeled")) {
      item.label = "unlabeled";
      item.annotation_status = "unlabeled";
      CXLOG_INFO("TorchTrainingImageSet", "training_image_label_changed",
                 "ui_event",
                 "label=unlabeled image_id=" + item.image_id +
                     " image_path=" + item.image_path);
    }
    ImGui::SameLine();
    if (ImGui::Button("label: pending")) {
      item.label = "pending";
      item.annotation_status = "pending";
      CXLOG_INFO("TorchTrainingImageSet", "training_image_label_changed",
                 "ui_event",
                 "label=pending image_id=" + item.image_id +
                     " image_path=" + item.image_path);
    }

    if (ImGui::Button("move train")) {
      item.split = "train";
      CXLOG_INFO("TorchTrainingImageSet", "training_image_split_changed",
                 "ui_event",
                 "split=train image_id=" + item.image_id +
                     " image_path=" + item.image_path);
    }
    ImGui::SameLine();
    if (ImGui::Button("move val")) {
      item.split = "val";
      CXLOG_INFO("TorchTrainingImageSet", "training_image_split_changed",
                 "ui_event",
                 "split=val image_id=" + item.image_id +
                     " image_path=" + item.image_path);
    }
    ImGui::SameLine();
    if (ImGui::Button("move test")) {
      item.split = "test";
      CXLOG_INFO("TorchTrainingImageSet", "training_image_split_changed",
                 "ui_event",
                 "split=test image_id=" + item.image_id +
                     " image_path=" + item.image_path);
    }
  }

  m_manualTest.script_evidence_thumb_load_count_this_frame = 0;
  DrawTorchTrainingImageRail("train", "Training Set / 训练集");
  DrawTorchTrainingImageRail("val", "Validation Set / 验证集");
  DrawTorchTrainingImageRail("test", "Test Set / 测试集");

  ImGui::End();
}

bool ViewController::RefreshEvidenceSelectionFromThumb(int groupIndex,
                                                       int thumbIndex,
                                                       bool loadImageToView,
                                                       std::string &reason) {
  reason.clear();

  if (groupIndex < 0 ||
      groupIndex >=
          static_cast<int>(m_manualTest.script_evidence_groups.size())) {
    reason = "invalid evidence group index";
    return false;
  }

  ScriptEvidenceGroup &group = m_manualTest.script_evidence_groups[groupIndex];

  if (thumbIndex < 0 || thumbIndex >= static_cast<int>(group.thumbs.size())) {
    reason = "invalid evidence thumb index";
    return false;
  }

  CxEvidenceSelectionSnapshot snapshot;
  if (!BuildEvidenceSnapshotFromThumb(
          groupIndex, thumbIndex, group.thumbs[thumbIndex], snapshot, reason)) {
    return false;
  }

  // Applying the snapshot can rebuild script_evidence_groups.  Derive the
  // history identity while the row is still valid, then never dereference the
  // old group/thumb after that call.
  const ScriptEvidenceThumb &selectedThumb = group.thumbs[thumbIndex];
  const std::string groupLabel = group.label;
  auto normalizeEvidenceOrigin = [](const std::string &path) {
    if (path.empty())
      return std::string();
    return std::filesystem::path(path).lexically_normal().generic_string();
  };
  std::string origin = selectedThumb.candidate_dir;
  if (origin.empty())
    origin = selectedThumb.evidence_output_root;
  if (origin.empty() && !selectedThumb.evidence_binding_path.empty())
    origin = std::filesystem::path(selectedThumb.evidence_binding_path)
                 .parent_path()
                 .string();
  if (origin.empty() && !selectedThumb.runtime_globals_path.empty())
    origin = std::filesystem::path(selectedThumb.runtime_globals_path)
                 .parent_path()
                 .string();
  if (origin.empty() && !selectedThumb.working_script_snapshot_path.empty())
    origin = std::filesystem::path(selectedThumb.working_script_snapshot_path)
                 .parent_path()
                 .string();
  if (origin.empty() && !selectedThumb.source_evidence_script_path.empty())
    origin = std::filesystem::path(selectedThumb.source_evidence_script_path)
                 .parent_path()
                 .string();
  if (origin.empty() && !selectedThumb.script_path.empty())
    origin = std::filesystem::path(selectedThumb.script_path).parent_path().string();
  origin = normalizeEvidenceOrigin(origin);
  std::string key;
  if (!selectedThumb.case_id.empty())
    key = "case=" + selectedThumb.case_id + "|origin=" + origin;
  else if (!selectedThumb.script_id.empty())
    key = "script=" +
          StripEvidenceCandidateDisplaySuffixLocal(selectedThumb.script_id) +
          "|origin=" + origin;
  else
    key = "fallback=" + groupLabel + "|" + selectedThumb.image_id + "|" +
          selectedThumb.target_id + "|" + origin;

  if (!ApplyEvidenceSelectionSnapshotToManualContext(snapshot, loadImageToView,
                                                      reason)) {
    return false;
  }

  if (!key.empty()) {
    auto &recent = m_manualTest.recent_evidence_case_keys;
    recent.erase(std::remove(recent.begin(), recent.end(), key), recent.end());
    recent.insert(recent.begin(), key);
    if (recent.size() > 5)
      recent.resize(5);

    std::ostringstream saved;
    saved << "# Evidence Chain recent case identities; newest first\n";
    for (const std::string &item : recent)
      saved << item << '\n';
    const std::filesystem::path recentPath = ResolveCxVisionRunPath(
        "cxscript_runs/evidence_chain/recent_evidence_cases.txt");
    if (!WriteTextFile(recentPath.string(), saved.str())) {
      CXLOG_WARN("EvidenceChain", "recent_case_save_failed", "ui_event",
                 "path=" + recentPath.string());
    }
  }

  return true;
}

void ViewController::SelectScriptEvidenceThumb(int groupIndex, int thumbIndex) {
  if (groupIndex < 0 ||
      groupIndex >=
          static_cast<int>(m_manualTest.script_evidence_groups.size()))
    return;

  ScriptEvidenceGroup &group = m_manualTest.script_evidence_groups[groupIndex];
  if (thumbIndex < 0 || thumbIndex >= static_cast<int>(group.thumbs.size()))
    return;

  CxEvidenceSelectionSnapshot snapshot;
  std::string reason;

  if (!BuildEvidenceSnapshotFromThumb(
          groupIndex, thumbIndex, group.thumbs[thumbIndex], snapshot, reason)) {
    m_manualTest.debug_status = "EVIDENCE_SELECT_FAIL";
    m_manualTest.debug_reason = reason;
    return;
  }

  if (!ApplyEvidenceSelectionSnapshotToManualContext(snapshot, false, reason)) {
    m_manualTest.debug_status = "EVIDENCE_APPLY_FAIL";
    m_manualTest.debug_reason = reason;
    return;
  }
}

static std::array<int, 4>
ReadFastMatchTraceFourCountsLocal(const cv::FileNode &root,
                                  const char *field) {
  std::array<int, 4> result{};
  const cv::FileNode values = root[field];
  if (!values.isSeq())
    return result;
  int index = 0;
  for (const cv::FileNode &value : values) {
    if (index >= static_cast<int>(result.size()))
      break;
    result[static_cast<std::size_t>(index++)] = static_cast<int>(value);
  }
  return result;
}

static std::array<int, 4>
ReadFastMatchTraceDirectionPointCountsLocal(const cv::FileNode &root,
                                            const char *field) {
  std::array<int, 4> result{};
  const cv::FileNode directions = root[field];
  if (!directions.isSeq())
    return result;
  int direction = 0;
  for (const cv::FileNode &points : directions) {
    if (direction >= static_cast<int>(result.size()))
      break;
    result[static_cast<std::size_t>(direction++)] =
        points.isSeq() ? static_cast<int>(points.size()) : 0;
  }
  return result;
}

static void DrawFastMatchTracePointSampleLocal(const cv::FileNode &root,
                                               const char *field,
                                               int direction) {
  const cv::FileNode directions = root[field];
  if (!directions.isSeq() || direction < 0 || direction >= directions.size()) {
    ImGui::TextDisabled("No point sequence available.");
    return;
  }
  const cv::FileNode points = directions[direction];
  if (!points.isSeq() || points.empty()) {
    ImGui::TextDisabled("No points retained for this direction.");
    return;
  }

  const int previewLimit = 12;
  int index = 0;
  for (const cv::FileNode &point : points) {
    if (index >= previewLimit)
      break;
    if (point.isSeq() && point.size() >= 2) {
      ImGui::Text("%d: (%.2f, %.2f)", index,
                  static_cast<double>(point[0]),
                  static_cast<double>(point[1]));
    }
    ++index;
  }
  if (points.size() > previewLimit)
    ImGui::TextDisabled("Preview limited to %d of %d points; full sequence remains in line_trace.json.",
                        previewLimit, points.size());
}

static void DrawFastMatchNormalTraceEvidenceLocal(
    const CxEvidenceSelectionSnapshot &selection) {
  if (!selection.valid || selection.evidence_output_root.empty())
    return;

  const std::filesystem::path tracePath =
      std::filesystem::path(selection.evidence_output_root) / "line_trace.json";
  std::error_code ec;
  if (!std::filesystem::is_regular_file(tracePath, ec))
    return;

  cv::FileStorage trace;
  try {
    if (!trace.open(tracePath.string(),
                    cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON)) {
      ImGui::SeparatorText("FastMatch Normal-Trace Evidence");
      ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f),
                         "TRACE_ASSET_INVALID: %s", tracePath.string().c_str());
      return;
    }
  } catch (const cv::Exception &) {
    ImGui::SeparatorText("FastMatch Normal-Trace Evidence");
    ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f),
                       "TRACE_ASSET_INVALID: %s", tracePath.string().c_str());
    return;
  }

  const cv::FileNode root = trace.root();
  const std::array<int, 4> anchors =
      ReadFastMatchTraceDirectionPointCountsLocal(
          root, "anchor_points_by_direction");
  const std::array<int, 4> nearCandidates =
      ReadFastMatchTraceFourCountsLocal(root, "anchor_near_candidate_counts");
  const std::array<int, 4> bandRejected =
      ReadFastMatchTraceFourCountsLocal(root, "anchor_band_rejected_counts");
  const std::array<int, 4> slopeRejected =
      ReadFastMatchTraceFourCountsLocal(root, "anchor_slope_rejected_counts");
  const std::array<int, 4> normalRejected =
      ReadFastMatchTraceFourCountsLocal(root, "anchor_normal_rejected_counts");
  const std::array<int, 4> prefilterAccepted =
      ReadFastMatchTraceFourCountsLocal(root,
                                        "anchor_prefilter_accepted_counts");
  const std::array<int, 4> domain =
      ReadFastMatchTraceFourCountsLocal(root, "domain_deduplicated_counts");
  const std::array<int, 4> compressed =
      ReadFastMatchTraceFourCountsLocal(root, "compressed_keypoint_counts");
  const std::array<int, 4> annSelected =
      ReadFastMatchTraceFourCountsLocal(root, "ann_selected_point_counts");
  const std::array<int, 4> annEdges =
      ReadFastMatchTraceFourCountsLocal(root, "ann_edge_counts");
  const std::array<int, 4> annComponents =
      ReadFastMatchTraceFourCountsLocal(root, "ann_component_counts");
  const std::array<int, 4> pairCounts =
      ReadFastMatchTraceFourCountsLocal(root, "normal_pair_counts_by_direction");
  const char *directions[4] = {"Top", "Bottom", "Left", "Right"};

  ImGui::SeparatorText("FastMatch Normal-Trace / ANN Evidence");
  ImGui::TextDisabled("Read-only runtime evidence: %s", tracePath.string().c_str());
  const bool executed = !root["normal_trace_executed"].empty() &&
                        static_cast<int>(root["normal_trace_executed"]) != 0;
  const bool succeeded = !root["normal_trace_succeeded"].empty() &&
                         static_cast<int>(root["normal_trace_succeeded"]) != 0;
  const std::string traceReason =
      FileNodeStringLocal(root, "normal_trace_reason");
  ImGui::Text("status=%s reason=%s", succeeded ? "NORMAL_TRACE_COMPLETE"
                                                   : (executed ? "TRACE_INCOMPLETE"
                                                               : "NOT_EXECUTED"),
              traceReason.empty() ? "-" : traceReason.c_str());

  ImGui::TextUnformatted("FindLine anchors and prefilter decisions");
  if (ImGui::BeginTable("fastmatch_anchor_prefilter_table", 7,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollX |
                            ImGuiTableFlags_SizingFixedFit,
                        ImVec2(-1.0f, 0.0f), 760.0f)) {
    ImGui::TableSetupColumn("Direction");
    ImGui::TableSetupColumn("anchor_points_by_direction");
    ImGui::TableSetupColumn("near candidates");
    ImGui::TableSetupColumn("band rejected");
    ImGui::TableSetupColumn("slope rejected");
    ImGui::TableSetupColumn("normal rejected");
    ImGui::TableSetupColumn("prefilter accepted");
    ImGui::TableHeadersRow();
    for (int direction = 0; direction < 4; ++direction) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(directions[direction]);
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%d", anchors[direction]);
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%d", nearCandidates[direction]);
      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%d", bandRejected[direction]);
      ImGui::TableSetColumnIndex(4);
      ImGui::Text("%d", slopeRejected[direction]);
      ImGui::TableSetColumnIndex(5);
      ImGui::Text("%d", normalRejected[direction]);
      ImGui::TableSetColumnIndex(6);
      ImGui::Text("%d", prefilterAccepted[direction]);
    }
    ImGui::EndTable();
  }

  if (ImGui::BeginTable("fastmatch_normal_trace_direction_table", 7,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollX |
                            ImGuiTableFlags_SizingFixedFit,
                        ImVec2(-1.0f, 0.0f), 980.0f)) {
    ImGui::TableSetupColumn("Direction");
    ImGui::TableSetupColumn("domain_points_by_direction");
    ImGui::TableSetupColumn("compressed_points_by_direction");
    ImGui::TableSetupColumn("ann_selected_points_by_direction");
    ImGui::TableSetupColumn("ann_edge_counts");
    ImGui::TableSetupColumn("ann_component_counts");
    ImGui::TableSetupColumn("Normal A/B pairs");
    ImGui::TableHeadersRow();
    for (int direction = 0; direction < 4; ++direction) {
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      ImGui::TextUnformatted(directions[direction]);
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%d", domain[direction]);
      ImGui::TableSetColumnIndex(2);
      ImGui::Text("%d", compressed[direction]);
      ImGui::TableSetColumnIndex(3);
      ImGui::Text("%d", annSelected[direction]);
      ImGui::TableSetColumnIndex(4);
      ImGui::Text("%d", annEdges[direction]);
      ImGui::TableSetColumnIndex(5);
      ImGui::Text("%d", annComponents[direction]);
      ImGui::TableSetColumnIndex(6);
      ImGui::Text("%d", pairCounts[direction]);
    }
    ImGui::EndTable();
  }

  ImGui::TextDisabled("Direction storage order is Top, Bottom, Left, Right; contour traversal is handled separately.");
  for (int direction = 0; direction < 4; ++direction) {
    const std::string label = std::string(directions[direction]) +
                              " point samples##fastmatch_trace_" +
                              std::to_string(direction);
    if (ImGui::TreeNode(label.c_str())) {
      ImGui::TextUnformatted("anchor_points_by_direction");
      DrawFastMatchTracePointSampleLocal(root, "anchor_points_by_direction",
                                         direction);
      ImGui::Separator();
      ImGui::TextUnformatted("domain_points_by_direction");
      DrawFastMatchTracePointSampleLocal(root, "domain_points_by_direction",
                                         direction);
      ImGui::Separator();
      ImGui::TextUnformatted("compressed_points_by_direction");
      DrawFastMatchTracePointSampleLocal(root,
                                         "compressed_points_by_direction",
                                         direction);
      ImGui::Separator();
      ImGui::TextUnformatted("ann_selected_points_by_direction");
      DrawFastMatchTracePointSampleLocal(root,
                                         "ann_selected_points_by_direction",
                                         direction);
      ImGui::TreePop();
    }
  }
}

void ViewController::DrawScriptEvidenceThumbnailRailByGroup() {
  if (ImGui::Button("Reload Evidence Assets")) {
    for (auto &group : m_manualTest.script_evidence_groups) {
      for (auto &thumb : group.thumbs)
        ResetEvidenceThumbTexture(thumb);
    }
    m_manualTest.script_evidence_groups_dirty = true;
    m_manualTest.script_evidence_row_refs_dirty = true;
    m_manualTest.debug_status = "EVIDENCE_ASSET_RELOAD_QUEUED";
    m_manualTest.debug_reason =
        "requested rescan of registered asset roots; no image or review state was modified";
    CXLOG_INFO("EvidenceChain", "asset_reload", "queued",
               m_manualTest.debug_reason);
  }
  ImGui::SameLine();
  ImGui::TextDisabled(
      "Rescans registered Evidence roots; source images and review decisions remain unchanged.");

  RefreshHiddenEvidenceCaseCatalogLocal(m_manualTest);
  if (ImGui::CollapsingHeader("Add Hidden Evidence Case", ImGuiTreeNodeFlags_None)) {
    ImGui::TextDisabled(
        "The catalog is metadata-only. Cases enter the active list only after an explicit Add action; no image is loaded here.");
    InputTextString("Find hidden case", m_manualTest.hidden_evidence_case_filter);
    std::string filter = TrimLine(m_manualTest.hidden_evidence_case_filter);
    std::transform(filter.begin(), filter.end(), filter.begin(),
                   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    int shown = 0;
    if (ImGui::BeginChild("hidden_evidence_case_catalog", ImVec2(-1.0f, 180.0f),
                          true)) {
      for (std::size_t index = 0;
           index < m_manualTest.hidden_evidence_case_catalog.size(); ++index) {
        const ScriptEvidenceThumb &entry =
            m_manualTest.hidden_evidence_case_catalog[index];
        const std::string key = entry.source_evidence_script_path + "::" +
                                entry.case_id;
        std::string searchable = entry.case_id + " " + entry.evidence_case_role +
                                 " " + entry.tool + " " +
                                 entry.source_evidence_script_path;
        std::transform(searchable.begin(), searchable.end(), searchable.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        if (!filter.empty() && searchable.find(filter) == std::string::npos)
          continue;
        ++shown;
        const bool added =
            m_manualTest.selected_hidden_evidence_case_keys.find(key) !=
            m_manualTest.selected_hidden_evidence_case_keys.end();
        ImGui::PushID(static_cast<int>(index));
        ImGui::TextUnformatted(entry.case_id.c_str());
        ImGui::TextDisabled("tool=%s | role=%s | status=%s",
                            entry.tool.empty() ? "-" : entry.tool.c_str(),
                            entry.evidence_case_role.empty()
                                ? "-"
                                : entry.evidence_case_role.c_str(),
                            entry.workflow_status.empty()
                                ? "REFERENCE_ONLY"
                                : entry.workflow_status.c_str());
        ImGui::TextDisabled("source: %s", entry.source_evidence_script_path.c_str());
        if (!added) {
          if (ImGui::Button("Add to Current Evidence List")) {
            m_manualTest.selected_hidden_evidence_case_keys.insert(key);
            m_manualTest.operator_hidden_evidence_case_keys.erase(key);
            m_manualTest.script_evidence_groups_dirty = true;
            m_manualTest.script_evidence_row_refs_dirty = true;
            m_manualTest.debug_status = "HIDDEN_EVIDENCE_CASE_QUEUED";
            m_manualTest.debug_reason = "queued case=" + entry.case_id +
                                        " source=" + entry.source_evidence_script_path;
            CXLOG_INFO("EvidenceChain", "hidden_case_add", "queued",
                       m_manualTest.debug_reason);
          }
        } else {
          ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f),
                             "IN CURRENT LIST");
          ImGui::SameLine();
          if (ImGui::Button("Remove from Current List")) {
            m_manualTest.selected_hidden_evidence_case_keys.erase(key);
            m_manualTest.operator_hidden_evidence_case_keys.insert(key);
            m_manualTest.script_evidence_groups_dirty = true;
            m_manualTest.script_evidence_row_refs_dirty = true;
            m_manualTest.debug_status = "HIDDEN_EVIDENCE_CASE_REMOVED";
            m_manualTest.debug_reason = "removed case=" + entry.case_id;
            CXLOG_INFO("EvidenceChain", "hidden_case_remove", "queued",
                       m_manualTest.debug_reason);
          }
        }
        ImGui::Separator();
        ImGui::PopID();
      }
    }
    ImGui::EndChild();
    ImGui::TextDisabled("Catalog matches: %d | active additions: %zu", shown,
                        m_manualTest.selected_hidden_evidence_case_keys.size());
  }

  if (m_manualTest.script_evidence_groups.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f),
                       "No trace binding thumbnails.");
    return;
  }

  if (!m_manualTest.recent_evidence_cases_loaded) {
    m_manualTest.recent_evidence_cases_loaded = true;
    std::string savedRecent;
    const std::filesystem::path recentPath = ResolveCxVisionRunPath(
        "cxscript_runs/evidence_chain/recent_evidence_cases.txt");
    if (ReadTextFile(recentPath.string(), savedRecent)) {
      std::istringstream input(savedRecent);
      std::string line;
      while (std::getline(input, line) &&
             m_manualTest.recent_evidence_case_keys.size() < 5) {
        line = TrimLine(line);
        if (line.empty() || line[0] == '#')
          continue;
        if (std::find(m_manualTest.recent_evidence_case_keys.begin(),
                      m_manualTest.recent_evidence_case_keys.end(), line) ==
            m_manualTest.recent_evidence_case_keys.end()) {
          m_manualTest.recent_evidence_case_keys.push_back(line);
        }
      }
    }
  }

  m_manualTest.script_evidence_thumb_load_count_this_frame = 0;

  const float rowHeight = 128.0f;
  const float availableHeight = ImGui::GetContentRegionAvail().y;
  const float targetHeight = m_manualTest.script_evidence_case_filter.empty()
                                 ? rowHeight * 4.0f + 72.0f
                                 : rowHeight + 92.0f;
  const float listHeight = std::max(
      220.0f, std::min(availableHeight > 0.0f ? availableHeight : targetHeight,
                       targetHeight));

  struct EvidenceCategory {
    std::string label;
    int priority = 100;
    std::vector<ScriptEvidenceRowRef> rows;
    std::vector<ScriptEvidenceRowRef> direct_rows;
    struct CaseFolder {
      std::string label;
      std::vector<ScriptEvidenceRowRef> rows;
    };
    struct HeadFolder {
      std::string label;
      std::vector<ScriptEvidenceRowRef> direct_rows;
      std::vector<CaseFolder> case_folders;
    };
    std::vector<HeadFolder> head_folders;
  };

  struct EvidenceMajorCategory {
    std::string label;
    int priority = 100;
    std::vector<EvidenceCategory> tools;
  };

  auto toLower = [](std::string value) -> std::string {
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return value;
  };

  ImGui::SetNextItemWidth(-1.0f);
  InputTextString("Filter cases", m_manualTest.script_evidence_case_filter);
  const std::string caseFilter =
      toLower(TrimLine(m_manualTest.script_evidence_case_filter));
  ImGui::TextDisabled(
      "Case folders start collapsed. Expanding a folder only reveals its "
      "asset-backed cases; click one case row to load its script, parameters "
      "and read-only image.");

  auto classifyMajor =
      [&](const ScriptEvidenceThumb &thumb,
          const ScriptEvidenceGroup &group) -> std::pair<int, std::string> {
    if (!thumb.evidence_head_folder.empty()) {
      const std::string categoryOverride =
          ResolveEvidenceCategoryOverrideLocal(m_manualTest, thumb);
      if (categoryOverride == "Verified")
        return {1, "Verified"};
      if (categoryOverride == "Defect")
        return {2, "Defect"};
      if (categoryOverride == "FastMatch Trace Diagnostics")
        return {-2, "FastMatch Trace Diagnostics"};
      return {0, "To Verify"};
    }
    return ClassifyEvidenceMajorBucketLocal(m_manualTest, thumb, group.label);
  };
  auto classifyTool =
      [&](const ScriptEvidenceThumb &thumb,
          const ScriptEvidenceGroup &group) -> std::pair<int, std::string> {
    if (thumb.evidence_head_folder.empty() &&
        !thumb.evidence_group_override.empty())
      return {std::max(0, thumb.workflow_stage_index),
              thumb.evidence_group_override};
    const std::string exactTool = NormalizeEvidenceToolTypeLocal(thumb.tool);
    const bool isToVerify = classifyMajor(thumb, group).second == "To Verify";
    if (isToVerify && !exactTool.empty()) {
      int priority = 30;
      if (exactTool == "FindLine")
        priority = 0;
      else if (exactTool == "FindCircle")
        priority = 1;
      else if (exactTool == "FindObject")
        priority = 2;
      else if (exactTool == "FindEllipse")
        priority = 3;
      else if (exactTool == "FindRect")
        priority = 4;
      else if (exactTool == "RegionPatternTool")
        priority = 5;
      else if (exactTool == "GridPatternClassTool")
        priority = 6;
      else if (exactTool == "FastMatch")
        priority = 7;
      else if (exactTool == "FindSegmentation")
        priority = 8;
      else if (exactTool == "TorchTask")
        priority = 9;
      return {priority, exactTool};
    }

    const std::string key =
        toLower(thumb.tool + " " + thumb.script_id + " " + thumb.script_path +
                " " + thumb.status + " " + thumb.reason + " " +
                thumb.parameter_summary + " " + group.label);

    if (IsTorchEvidenceCandidateRowLocal(thumb, group.label)) {
      const std::string label =
          group.label.empty() ? std::string("Torch Evidence Candidate Case")
                              : group.label;
      return {0, label};
    }

    if (exactTool == "FindLine")
      return {0, "FindLine"};
    if (exactTool == "FindCircle")
      return {1, "FindCircle"};
    if (exactTool == "FindObject")
      return {2, "FindObject"};
    if (exactTool == "FindEllipse")
      return {3, "FindEllipse"};
    if (exactTool == "FindRect")
      return {4, "FindRect"};
    if (exactTool == "RegionPatternTool")
      return {5, "RegionPattern"};
    if (exactTool == "GridPatternClassTool")
      return {6, "GridPattern"};
    if (exactTool == "FastMatch")
      return {7, "FastMatch"};
    if (exactTool == "FindSegmentation")
      return {8, "FindSegmentation Prompt / EdgeSam"};
    if (exactTool == "TorchTask")
      return {9, "Torch / Model Validation"};

    // Category membership describes workflow state; it must not replace the
    // tool navigation key.  Only fall back to a legacy storage-group label
    // when the row has no registered/normalized tool value.
    if (!thumb.evidence_category_override.empty() && !group.label.empty())
      return {0, group.label};

    // Only infer Torch/model ownership from free text when the row has no
    // explicit tool type.  Every candidate parameter snapshot contains
    // shared global_torch_* fields, which must not move FindEllipse and
    // other cximage tools into the Torch navigation bucket.
    if (key.find("find_segmentation") != std::string::npos ||
        key.find("findsegmentation") != std::string::npos ||
        key.find("edgesam") != std::string::npos)
      return {6, "FindSegmentation Prompt / EdgeSam"};

    if (key.find("torch") != std::string::npos ||
        key.find("deeplab") != std::string::npos ||
        key.find("yolo") != std::string::npos) {
      if (key.find("detect") != std::string::npos ||
          key.find("yolo") != std::string::npos)
        return {7, "Torch Detection - Model Unverified"};
      if (key.find("segment") != std::string::npos ||
          key.find("mask") != std::string::npos ||
          key.find("deeplab") != std::string::npos)
        return {8, "Torch Segmentation - Runtime Smoke"};
      return {9, "Torch / Model Validation"};
    }

    if (key.find("findline") != std::string::npos ||
        key.find("find_line") != std::string::npos)
      return {0, "FindLine"};

    if (key.find("findcircle") != std::string::npos ||
        key.find("find_circle") != std::string::npos)
      return {1, "FindCircle"};

    if (key.find("findobject") != std::string::npos ||
        key.find("find_object") != std::string::npos)
      return {2, "FindObject"};

    if (key.find("findellipse") != std::string::npos ||
        key.find("find_ellipse") != std::string::npos)
      return {3, "FindEllipse"};

    if (key.find("findrect") != std::string::npos ||
        key.find("find_rect") != std::string::npos)
      return {4, "FindRect"};

    if (key.find("regionpattern") != std::string::npos ||
        key.find("region_pattern") != std::string::npos)
      return {5, "RegionPattern"};

    if (key.find("gridpattern") != std::string::npos ||
        key.find("grid_pattern") != std::string::npos)
      return {6, "GridPattern"};

    if (key.find("fastmatch") != std::string::npos)
      return {7, "FastMatch"};

    if (key.find("integration") != std::string::npos)
      return {20, "Integration"};

    return {30, group.label.empty() ? "Other" : group.label};
  };

  std::vector<EvidenceMajorCategory> categories;

  auto findOrCreateMajor =
      [&](int priority, const std::string &label) -> EvidenceMajorCategory & {
    for (auto &category : categories) {
      if (category.priority == priority && category.label == label)
        return category;
    }

    EvidenceMajorCategory category;
    category.priority = priority;
    category.label = label;
    categories.push_back(category);
    return categories.back();
  };

  auto findOrCreateTool = [](EvidenceMajorCategory &major, int priority,
                             const std::string &label) -> EvidenceCategory & {
    for (auto &tool : major.tools) {
      if (tool.priority == priority && tool.label == label)
        return tool;
    }

    EvidenceCategory tool;
    tool.priority = priority;
    tool.label = label;
    major.tools.push_back(tool);
    return major.tools.back();
  };
  auto findOrCreateHeadFolder =
      [](EvidenceCategory &tool,
         const std::string &label) -> EvidenceCategory::HeadFolder & {
    for (auto &folder : tool.head_folders) {
      if (folder.label == label)
        return folder;
    }
    EvidenceCategory::HeadFolder folder;
    folder.label = label;
    tool.head_folders.push_back(std::move(folder));
    return tool.head_folders.back();
  };
  auto findOrCreateCaseFolder =
      [](EvidenceCategory::HeadFolder &head,
         const std::string &label) -> EvidenceCategory::CaseFolder & {
    for (auto &folder : head.case_folders) {
      if (folder.label == label)
        return folder;
    }
    EvidenceCategory::CaseFolder folder;
    folder.label = label;
    head.case_folders.push_back(std::move(folder));
    return head.case_folders.back();
  };
  auto uniqueCaseKey = [&](const ScriptEvidenceThumb &thumb,
                           const ScriptEvidenceGroup &group) -> std::string {
    // A case id identifies the logical test, not one particular evidence
    // package.  The same logical case can legitimately be present in a saved
    // candidate and in a different headless/evidence run.  Keep those rows
    // independently navigable; otherwise a late scan silently replaces an
    // earlier, user-visible case in the Evidence list.
    auto normalizeEvidenceOrigin = [](const std::string &path) {
      if (path.empty())
        return std::string();
      return std::filesystem::path(path).lexically_normal().generic_string();
    };

    std::string origin = thumb.candidate_dir;
    if (origin.empty())
      origin = thumb.evidence_output_root;
    if (origin.empty() && !thumb.evidence_binding_path.empty())
      origin = std::filesystem::path(thumb.evidence_binding_path)
                   .parent_path()
                   .string();
    if (origin.empty() && !thumb.runtime_globals_path.empty())
      origin = std::filesystem::path(thumb.runtime_globals_path)
                   .parent_path()
                   .string();
    if (origin.empty() && !thumb.working_script_snapshot_path.empty())
      origin = std::filesystem::path(thumb.working_script_snapshot_path)
                   .parent_path()
                   .string();
    if (origin.empty() && !thumb.source_evidence_script_path.empty())
      origin = std::filesystem::path(thumb.source_evidence_script_path)
                   .parent_path()
                   .string();
    if (origin.empty() && !thumb.script_path.empty())
      origin = std::filesystem::path(thumb.script_path).parent_path().string();
    origin = normalizeEvidenceOrigin(origin);

    if (!thumb.case_id.empty())
      return "case=" + thumb.case_id + "|origin=" + origin;
    if (!thumb.script_id.empty())
      return "script=" +
             StripEvidenceCandidateDisplaySuffixLocal(thumb.script_id) +
             "|origin=" + origin;
    return "fallback=" + group.label + "|" + thumb.image_id + "|" +
           thumb.target_id + "|" + origin;
  };
  std::vector<ScriptEvidenceRowRef> uniqueRows;
  std::unordered_map<std::string, std::size_t> uniqueRowSlots;

  for (std::size_t gi = 0; gi < m_manualTest.script_evidence_groups.size();
       ++gi) {
    ScriptEvidenceGroup &group = m_manualTest.script_evidence_groups[gi];
    for (std::size_t ti = 0; ti < group.thumbs.size(); ++ti) {
      ScriptEvidenceThumb &thumb = group.thumbs[ti];

      ScriptEvidenceRowRef row;
      row.group_index = static_cast<int>(gi);
      row.thumb_index = static_cast<int>(ti);
      row.is_group_header = false;
      row.label = StripEvidenceCandidateDisplaySuffixLocal(
          thumb.review_item.empty()
              ? (thumb.script_id.empty() ? thumb.case_id : thumb.script_id)
              : thumb.review_item);

      const std::string key = uniqueCaseKey(thumb, group);
      const auto existingSlot = uniqueRowSlots.find(key);
      if (existingSlot == uniqueRowSlots.end()) {
        uniqueRowSlots[key] = uniqueRows.size();
        uniqueRows.push_back(row);
      } else {
        uniqueRows[existingSlot->second] = row;
      }
    }
  }

  std::unordered_map<std::string, ScriptEvidenceRowRef> rowByIdentity;
  for (const ScriptEvidenceRowRef &row : uniqueRows) {
    if (row.group_index < 0 ||
        row.group_index >=
            static_cast<int>(m_manualTest.script_evidence_groups.size()))
      continue;
    const ScriptEvidenceGroup &group =
        m_manualTest.script_evidence_groups[row.group_index];
    if (row.thumb_index < 0 ||
        row.thumb_index >= static_cast<int>(group.thumbs.size()))
      continue;
    rowByIdentity[uniqueCaseKey(group.thumbs[row.thumb_index], group)] = row;
  }

  for (const ScriptEvidenceRowRef &row : uniqueRows) {
    if (row.group_index < 0 ||
        row.group_index >=
            static_cast<int>(m_manualTest.script_evidence_groups.size()))
      continue;
    ScriptEvidenceGroup &group =
        m_manualTest.script_evidence_groups[row.group_index];
    if (row.thumb_index < 0 ||
        row.thumb_index >= static_cast<int>(group.thumbs.size()))
      continue;
    ScriptEvidenceThumb &thumb = group.thumbs[row.thumb_index];
    if (!caseFilter.empty()) {
      const std::string searchable =
          toLower(thumb.review_item + " " + thumb.case_id + " " +
                  thumb.script_id + " " + thumb.image_id + " " +
                  thumb.target_id + " " + thumb.tool + " " + group.label);
      if (searchable.find(caseFilter) == std::string::npos)
        continue;
    }
    const auto majorClass = classifyMajor(thumb, group);
    const auto toolClass = classifyTool(thumb, group);

    EvidenceMajorCategory &major =
        findOrCreateMajor(majorClass.first, majorClass.second);
    EvidenceCategory &tool =
        findOrCreateTool(major, toolClass.first, toolClass.second);
    tool.rows.push_back(row);
    if (thumb.evidence_head_folder.empty()) {
      tool.direct_rows.push_back(row);
      continue;
    }

    EvidenceCategory::HeadFolder &head =
        findOrCreateHeadFolder(tool, thumb.evidence_head_folder);
    if (thumb.evidence_case_folder.empty()) {
      head.direct_rows.push_back(row);
      continue;
    }
    findOrCreateCaseFolder(head, thumb.evidence_case_folder)
        .rows.push_back(row);
  }

  // Recent Cases is a navigation shortcut, not another evidence state.  It
  // only references freshly scanned rows, so deleted/moved assets cannot be
  // resurrected from history.
  EvidenceMajorCategory &recentMajor =
      findOrCreateMajor(-1, "Recent Cases");
  for (const std::string &key : m_manualTest.recent_evidence_case_keys) {
    const auto found = rowByIdentity.find(key);
    if (found == rowByIdentity.end())
      continue;
    const ScriptEvidenceRowRef &row = found->second;
    const ScriptEvidenceGroup &group =
        m_manualTest.script_evidence_groups[row.group_index];
    const ScriptEvidenceThumb &thumb = group.thumbs[row.thumb_index];
    const auto toolClass = classifyTool(thumb, group);
    EvidenceCategory &tool =
        findOrCreateTool(recentMajor, toolClass.first, toolClass.second);
    tool.rows.push_back(row);
    tool.direct_rows.push_back(row);
  }
  static int classificationDebugDumpBudget = 3;
  if (classificationDebugDumpBudget > 0) {
    --classificationDebugDumpBudget;

    auto escapeTsv = [](std::string value) -> std::string {
      for (char &ch : value) {
        if (ch == '\t' || ch == '\r' || ch == '\n')
          ch = ' ';
      }
      return value;
    };
    std::ostringstream debug;
    debug << "major\ttool\tgroup_label\tcase_id\timage_id\ttarget_id\t"
          << "candidate_id\tstatus\toverride\tscript_id\treason\n";

    for (const auto &major : categories) {
      for (const auto &tool : major.tools) {
        for (const ScriptEvidenceRowRef &ref : tool.rows) {
          if (ref.group_index < 0 ||
              ref.group_index >=
                  static_cast<int>(m_manualTest.script_evidence_groups.size()))
            continue;
          const ScriptEvidenceGroup &group =
              m_manualTest.script_evidence_groups[ref.group_index];
          if (ref.thumb_index < 0 ||
              ref.thumb_index >= static_cast<int>(group.thumbs.size()))
            continue;
          const ScriptEvidenceThumb &thumb = group.thumbs[ref.thumb_index];
          debug << escapeTsv(major.label) << '\t' << escapeTsv(tool.label)
                << '\t' << escapeTsv(group.label) << '\t'
                << escapeTsv(thumb.case_id) << '\t' << escapeTsv(thumb.image_id)
                << '\t' << escapeTsv(thumb.target_id) << '\t'
                << escapeTsv(thumb.candidate_id) << '\t'
                << escapeTsv(thumb.status) << '\t'
                << escapeTsv(ResolveEvidenceCategoryOverrideLocal(m_manualTest,
                                                                  thumb))
                << '\t' << escapeTsv(thumb.script_id) << '\t'
                << escapeTsv(thumb.reason) << '\n';
        }
      }
    }

    WriteTextFile(
        ResolveCxVisionRunPath("cxscript_runs/evidence_chain/"
                               "evidence_chain_ui_classification_debug.tsv"),
        debug.str());
  }

  std::stable_sort(categories.begin(), categories.end(),
                   [](const EvidenceMajorCategory &left,
                      const EvidenceMajorCategory &right) {
                     if (left.priority != right.priority)
                       return left.priority < right.priority;
                     return left.label < right.label;
                   });
  for (auto &major : categories) {
    std::stable_sort(
        major.tools.begin(), major.tools.end(),
        [](const EvidenceCategory &left, const EvidenceCategory &right) {
          if (left.priority != right.priority)
            return left.priority < right.priority;
          return left.label < right.label;
        });
    for (auto &tool : major.tools) {
      std::stable_sort(tool.head_folders.begin(), tool.head_folders.end(),
                       [](const EvidenceCategory::HeadFolder &left,
                          const EvidenceCategory::HeadFolder &right) {
                         return left.label < right.label;
                       });
      for (auto &head : tool.head_folders) {
        std::stable_sort(head.case_folders.begin(), head.case_folders.end(),
                         [](const EvidenceCategory::CaseFolder &left,
                            const EvidenceCategory::CaseFolder &right) {
                           return left.label < right.label;
                         });
      }
    }
  }

  ImGui::BeginChild("script_evidence_by_group", ImVec2(-1, listHeight), true);

  auto drawEvidenceRow = [&](const ScriptEvidenceRowRef &ref) {
    if (ref.group_index < 0 ||
        ref.group_index >=
            static_cast<int>(m_manualTest.script_evidence_groups.size()))
      return;

    ScriptEvidenceGroup &group =
        m_manualTest.script_evidence_groups[ref.group_index];
    if (ref.thumb_index < 0 ||
        ref.thumb_index >= static_cast<int>(group.thumbs.size()))
      return;

    ScriptEvidenceThumb &thumb = group.thumbs[ref.thumb_index];
    EnsureScriptEvidenceThumbTexture(thumb);
    DrawOneScriptEvidenceRow(ref.group_index, ref.thumb_index, thumb,
                             rowHeight);
  };

  for (std::size_t ci = 0; ci < categories.size(); ++ci) {
    EvidenceMajorCategory &major = categories[ci];

    int majorCount = 0;
    for (const auto &tool : major.tools)
      majorCount += static_cast<int>(tool.rows.size());

    ImGui::PushID(static_cast<int>(ci));

    std::string header = major.label + " (" + std::to_string(majorCount) + ")";
    // Folder expansion is deliberately separate from case activation. It
    // never replaces the active project or Image View; a concrete evidence
    // row performs that activation.
    const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None;

    if (ImGui::CollapsingHeader(header.c_str(), flags)) {
      if (major.tools.empty()) {
        ImGui::TextDisabled("No evidence entries.");
      }

      for (std::size_t ti = 0; ti < major.tools.size(); ++ti) {
        EvidenceCategory &tool = major.tools[ti];
        if (tool.rows.empty())
          continue;

        ImGui::PushID(static_cast<int>(ti));
        const std::string toolHeader =
            tool.label + " (" + std::to_string(tool.rows.size()) + ")";
        const ImGuiTreeNodeFlags toolFlags = ImGuiTreeNodeFlags_OpenOnArrow;
        if (ImGui::TreeNodeEx(toolHeader.c_str(), toolFlags)) {
          for (std::size_t hi = 0; hi < tool.head_folders.size(); ++hi) {
            EvidenceCategory::HeadFolder &head = tool.head_folders[hi];
            int headCount = static_cast<int>(head.direct_rows.size());
            for (const auto &folder : head.case_folders)
              headCount += static_cast<int>(folder.rows.size());

            ImGui::PushID(static_cast<int>(hi));
            const std::string headHeader =
                head.label + " (" + std::to_string(headCount) + ")";
            const ImGuiTreeNodeFlags headFlags =
                ImGuiTreeNodeFlags_OpenOnArrow;
            if (ImGui::TreeNodeEx(headHeader.c_str(), headFlags)) {
              for (const ScriptEvidenceRowRef &ref : head.direct_rows)
                drawEvidenceRow(ref);

              for (std::size_t fi = 0; fi < head.case_folders.size(); ++fi) {
                EvidenceCategory::CaseFolder &folder = head.case_folders[fi];
                ImGui::PushID(static_cast<int>(fi));
                const std::string folderHeader =
                    folder.label + " (" + std::to_string(folder.rows.size()) +
                    ")";
                const ImGuiTreeNodeFlags folderFlags =
                    ImGuiTreeNodeFlags_OpenOnArrow;
                if (ImGui::TreeNodeEx(folderHeader.c_str(), folderFlags)) {
                  for (const ScriptEvidenceRowRef &ref : folder.rows)
                    drawEvidenceRow(ref);
                  ImGui::TreePop();
                }
                ImGui::PopID();
              }
              ImGui::TreePop();
            }
            ImGui::PopID();
          }

          if (tool.head_folders.empty()) {
            for (const ScriptEvidenceRowRef &ref : tool.direct_rows)
              drawEvidenceRow(ref);
          } else if (!tool.direct_rows.empty()) {
            ImGui::PushID("unfoldered_cases");
            const std::string unfolderedHeader =
                "Unfoldered Cases (" + std::to_string(tool.direct_rows.size()) +
                ")";
            const ImGuiTreeNodeFlags unfolderedFlags =
                ImGuiTreeNodeFlags_OpenOnArrow;
            if (ImGui::TreeNodeEx(unfolderedHeader.c_str(), unfolderedFlags)) {
              for (const ScriptEvidenceRowRef &ref : tool.direct_rows)
                drawEvidenceRow(ref);
              ImGui::TreePop();
            }
            ImGui::PopID();
          }
          ImGui::TreePop();
        }
        ImGui::PopID();
      }
    }

    ImGui::PopID();
  }

  ImGui::EndChild();
  DrawFastMatchNormalTraceEvidenceLocal(
      m_manualTest.current_evidence_selection);
}

static std::string
BuildCurrentRuntimeParamSummary(const ManualTestContext &context) {
  auto getInt = [&](const std::string &key, int fallback) -> int {
    auto it = context.runtime_int_vars.find(key);
    return it == context.runtime_int_vars.end() ? fallback : it->second;
  };

  std::ostringstream oss;
  oss << "method=" << getInt("global_method", 0)
      << " threshold=" << getInt("global_threshold", 20)
      << " wgap=" << getInt("global_wgap", 0)
      << " hgap=" << getInt("global_hgap", 0)
      << " gap=" << getInt("global_gap", 0)
      << " linegap=" << getInt("global_linegap", 0)
      << " tool_half_width=" << getInt("global_tool_half_width", 0)
      << " roi_x0=" << getInt("global_roi_x0", 0)
      << " roi_y0=" << getInt("global_roi_y0", 0)
      << " roi_x1=" << getInt("global_roi_x1", 0)
      << " roi_y1=" << getInt("global_roi_y1", 0)
      << " roi_x=" << getInt("global_roi_x", 0)
      << " roi_y=" << getInt("global_roi_y", 0)
      << " roi_width=" << getInt("global_roi_width", 0)
      << " roi_height=" << getInt("global_roi_height", 0)
      << " circle_cx=" << getInt("global_circle_cx", 0)
      << " circle_cy=" << getInt("global_circle_cy", 0)
      << " circle_px=" << getInt("global_circle_px", 0)
      << " circle_py=" << getInt("global_circle_py", 0)
      << " circle_inner_radius=" << getInt("global_circle_inner_radius", 0)
      << " circle_outer_radius=" << getInt("global_circle_outer_radius", 0)
      << " circle_ring_width=" << getInt("global_circle_ring_width", 0)
      << " ellipse_x0=" << getInt("global_ellipse_x0", 0)
      << " ellipse_y0=" << getInt("global_ellipse_y0", 0)
      << " ellipse_x1=" << getInt("global_ellipse_x1", 0)
      << " ellipse_y1=" << getInt("global_ellipse_y1", 0)
      << " learn_roi_x=" << getInt("global_learn_roi_x", 0)
      << " learn_roi_y=" << getInt("global_learn_roi_y", 0)
      << " learn_roi_w=" << getInt("global_learn_roi_w", 0)
      << " learn_roi_h=" << getInt("global_learn_roi_h", 0)
      << " search_roi_x=" << getInt("global_search_roi_x", 0)
      << " search_roi_y=" << getInt("global_search_roi_y", 0)
      << " search_roi_w=" << getInt("global_search_roi_w", 0)
      << " search_roi_h=" << getInt("global_search_roi_h", 0);

  return oss.str();
}

void ViewController::DrawOneScriptEvidenceRow(int groupIndex, int thumbIndex,
                                              ScriptEvidenceThumb &thumb,
                                              float rowHeight) {
  ImGui::PushID(groupIndex * 1000 + thumbIndex);

  const bool selected = m_manualTest.selected_evidence_group == groupIndex &&
                        m_manualTest.selected_evidence_thumb == thumbIndex;

  if (selected)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(45, 80, 115, 180));

  ImGui::BeginChild("evidence_row", ImVec2(-1, rowHeight), true,
                    ImGuiWindowFlags_NoScrollbar |
                        ImGuiWindowFlags_NoScrollWithMouse);

  auto finishRow = [&]() {
    ImGui::EndChild();
    if (selected)
      ImGui::PopStyleColor();
    ImGui::PopID();
  };

  const ImVec2 rowMin = ImGui::GetCursorScreenPos();
  const ImVec2 rowSize(ImGui::GetContentRegionAvail().x, rowHeight - 6.0f);

  ImGui::InvisibleButton("evidence_row_hit", rowSize,
                         ImGuiButtonFlags_MouseButtonLeft |
                             ImGuiButtonFlags_MouseButtonRight);

  const bool rowHovered = ImGui::IsItemHovered();
  const bool rowClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
  bool rowDoubleClicked =
      rowHovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
  const bool rowRightClicked = ImGui::IsItemClicked(ImGuiMouseButton_Right);

  ImGui::SetCursorScreenPos(rowMin);

  const float imageColWidth = 96.0f;
  bool imageDoubleClicked = false;
  auto textEllipsized = [](const char *label, const std::string &value,
                           int maxChars) {
    std::string shown = value.empty() ? "-" : value;
    if (maxChars > 3 && static_cast<int>(shown.size()) > maxChars)
      shown = shown.substr(0, static_cast<std::size_t>(maxChars - 3)) + "...";
    ImGui::Text("%s%s", label, shown.c_str());
  };

  if (ImGui::BeginTable("evidence_row_table", 2,
                        ImGuiTableFlags_SizingStretchProp,
                        ImVec2(-1, rowHeight - 8.0f))) {
    ImGui::TableSetupColumn("Text", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Image", ImGuiTableColumnFlags_WidthFixed,
                            imageColWidth);
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);

    const std::string visibleCase =
        thumb.review_item.empty() ? thumb.case_id : thumb.review_item;
    ImGui::Text("case: %s",
                visibleCase.empty() ? "(no case)" : visibleCase.c_str());
    if (!thumb.review_item.empty() && thumb.case_id != thumb.review_item)
      textEllipsized("internal: ", thumb.case_id, 82);
    if (!thumb.script_id.empty() && thumb.script_id != thumb.case_id)
      textEllipsized("script: ", thumb.script_id, 82);
    textEllipsized("path: ", thumb.script_path, 82);
    ImGui::Text("tool: %s | status: %s",
                thumb.tool.empty() ? "-" : thumb.tool.c_str(),
                thumb.status.empty() ? "-" : thumb.status.c_str());
    ImGui::Text("image: %s | target: %s",
                thumb.image_id.empty() ? "-" : thumb.image_id.c_str(),
                thumb.target_id.empty() ? "-" : thumb.target_id.c_str());
    textEllipsized("param: ", thumb.parameter_summary, 82);
    ImGui::Text(
        "primary: %s %s | %s",
        thumb.primary_object_type.empty() ? "-"
                                          : thumb.primary_object_type.c_str(),
        thumb.primary_object_name.empty() ? "-"
                                          : thumb.primary_object_name.c_str(),
        thumb.primary_object_status.empty()
            ? "-"
            : thumb.primary_object_status.c_str());

    ImGui::TableSetColumnIndex(1);

    const ImVec2 thumbSize(80.0f, 60.0f);

    if (thumb.texture_id != 0) {
      ImGui::Image(static_cast<ImU64>(thumb.texture_id), thumbSize);
      imageDoubleClicked = ImGui::IsItemHovered() &&
                           ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    } else {
      ImDrawList *drawList = ImGui::GetWindowDrawList();
      ImVec2 p0 = ImGui::GetCursorScreenPos();
      ImVec2 p1(p0.x + thumbSize.x, p0.y + thumbSize.y);
      drawList->AddRectFilled(p0, p1, IM_COL32(90, 130, 170, 220));
      drawList->AddText(ImVec2(p0.x + 18, p0.y + 28),
                        IM_COL32(255, 255, 255, 255), "NO IMG");
      ImGui::Dummy(thumbSize);
      imageDoubleClicked = ImGui::IsItemHovered() &&
                           ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
    }

    ImGui::EndTable();
  }

  // The table and its Image item are rendered after the row-sized invisible
  // button.  Depending on the ImGui overlap rules, the second click can be
  // owned by the table item and the invisible button never reports a native
  // double click.  Detect the complete visual row bounds as the canonical
  // click surface and keep a small, explicit double-click window.
  const ImVec2 rowMax(rowMin.x + rowSize.x, rowMin.y + rowSize.y);
  const bool rowBoundsHovered =
      ImGui::IsMouseHoveringRect(rowMin, rowMax, false);
  if (rowBoundsHovered) {
    ImGui::SetTooltip(
        "Click to load this evidence case's script, parameter snapshot and "
        "read-only image. Folder expansion alone never changes the active "
        "case or image.");
  }
  const bool rowBoundsClicked =
      rowBoundsHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
  // The child table/image is drawn above the invisible button. Use the same
  // canonical visual row bounds for the context click, otherwise a
  // right-click over the thumbnail can be swallowed by the table item and
  // appears to do nothing.
  const bool rowBoundsRightClicked =
      rowBoundsHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
  if (rowBoundsClicked) {
    const double now = ImGui::GetTime();
    const bool sameRow = m_manualTest.last_evidence_click_group == groupIndex &&
                         m_manualTest.last_evidence_click_thumb == thumbIndex;
    const double elapsed = now - m_manualTest.last_evidence_click_time;
    if (sameRow && elapsed >= 0.0 && elapsed <= 0.55)
      rowDoubleClicked = true;

    m_manualTest.last_evidence_click_group = groupIndex;
    m_manualTest.last_evidence_click_thumb = thumbIndex;
    m_manualTest.last_evidence_click_time = now;
  }

  if (rowDoubleClicked || imageDoubleClicked) {
    CXLOG_INFO("EvidenceChain", "evidence_thumb_double_click", "ui_event",
               "group_index=" + std::to_string(groupIndex) +
                   " thumb_index=" + std::to_string(thumbIndex) +
                   " script_id=" + thumb.script_id +
                   " image_id=" + thumb.image_id +
                   " image_path=" + thumb.image_path + " tool=" + thumb.tool);
    std::string reason;
    if (!RefreshEvidenceSelectionFromThumb(groupIndex, thumbIndex, true,
                                           reason)) {
      m_manualTest.debug_status = "LOAD_IMAGE_FAIL";
      m_manualTest.debug_reason = reason;
    }
    finishRow();
    return;
  } else if (rowClicked || rowBoundsClicked) {
    CXLOG_INFO("EvidenceChain", "evidence_thumb_click", "ui_event",
               "group_index=" + std::to_string(groupIndex) +
                   " thumb_index=" + std::to_string(thumbIndex) +
                   " script_id=" + thumb.script_id +
                   " image_id=" + thumb.image_id + " tool=" + thumb.tool);
    std::string reason;
    const bool loadImage = !thumb.image_path.empty();
    if (!RefreshEvidenceSelectionFromThumb(groupIndex, thumbIndex, loadImage,
                                           reason)) {
      m_manualTest.debug_status = "EVIDENCE_SELECT_LOAD_FAIL";
      m_manualTest.debug_reason = reason;
    } else if (!loadImage) {
      m_manualTest.debug_status = "EVIDENCE_SELECTED_METADATA_ONLY";
      m_manualTest.debug_reason =
          "case metadata loaded; image remains unbound until the workflow "
          "dataset manifest supplies it";
    }
    finishRow();
    return;
  }

  if (rowRightClicked || rowBoundsRightClicked) {
    CXLOG_INFO("EvidenceChain", "evidence_thumb_context_open", "ui_event",
               "group_index=" + std::to_string(groupIndex) +
                   " thumb_index=" + std::to_string(thumbIndex) +
                   " script_id=" + thumb.script_id +
                   " image_path=" + thumb.image_path);
    ImGui::OpenPopup("evidence_row_context");
  }

  bool rowStateReplaced = false;
  if (ImGui::BeginPopup("evidence_row_context")) {
    ImGui::TextUnformatted(thumb.script_id.c_str());
    ImGui::Separator();

    if (ImGui::BeginMenu("Move To Category")) {
      auto moveToCategory = [&](const std::string &category) {
        thumb.evidence_category_override = category;
        StoreEvidenceCategoryOverrideLocal(m_manualTest, thumb, category);
        thumb.status = "manual_category";
        thumb.reason = "manual category: " + category;
        m_manualTest.script_evidence_row_refs_dirty = true;
        std::string saveReason;
        if (SaveEvidenceCategoryOverridesLocal(m_manualTest, saveReason)) {
          m_manualTest.debug_status = "EVIDENCE_CATEGORY_SAVED";
          m_manualTest.debug_reason = thumb.script_id + " -> " +
                                      thumb.evidence_category_override + "; " +
                                      saveReason;
        } else {
          m_manualTest.debug_status = "EVIDENCE_CATEGORY_SAVE_FAIL";
          m_manualTest.debug_reason = saveReason;
        }
      };

      std::vector<std::string> categoryLabels;
      auto addCategoryLabel = [&](const std::string &label) {
        const std::string trimmed = TrimLine(label);
        if (trimmed.empty())
          return;
        if (std::find(categoryLabels.begin(), categoryLabels.end(), trimmed) ==
            categoryLabels.end())
          categoryLabels.push_back(trimmed);
      };
      for (const ScriptEvidenceGroup &loadedGroup :
           m_manualTest.script_evidence_groups) {
        for (const ScriptEvidenceThumb &loadedThumb : loadedGroup.thumbs)
          addCategoryLabel(
              ResolveEvidenceCategoryOverrideLocal(m_manualTest, loadedThumb));
      }
      std::stable_sort(categoryLabels.begin(), categoryLabels.end());

      if (categoryLabels.empty()) {
        ImGui::TextDisabled("No external categories loaded.");
      } else {
        for (const std::string &category : categoryLabels) {
          if (ImGui::MenuItem(category.c_str()))
            moveToCategory(category);
        }
      }

      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Save Manual Review")) {
      auto saveReview = [&](const char *decision, const char *category,
                            const char *status) {
        std::string savedPath;
        std::string saveReason;
        if (!SaveEvidenceManualReviewLocal(thumb, decision, savedPath,
                                           saveReason)) {
          m_manualTest.debug_status = "MANUAL_GUI_REVIEW_SAVE_FAIL";
          m_manualTest.debug_reason = saveReason;
          return;
        }

        thumb.evidence_category_override = category;
        thumb.status = status;
        thumb.reason = std::string("manual GUI review: ") + decision +
                       "; review=" + savedPath;
        StoreEvidenceCategoryOverrideLocal(m_manualTest, thumb, category);
        m_manualTest.script_evidence_row_refs_dirty = true;

        std::string categoryReason;
        if (!SaveEvidenceCategoryOverridesLocal(m_manualTest, categoryReason)) {
          m_manualTest.debug_status = "MANUAL_GUI_REVIEW_CATEGORY_SAVE_FAIL";
          m_manualTest.debug_reason = saveReason + "; " + categoryReason;
          return;
        }
        m_manualTest.debug_status = decision;
        m_manualTest.debug_reason = saveReason;
      };

      if (ImGui::MenuItem("MANUAL_GUI_PASS"))
        saveReview("MANUAL_GUI_PASS", "Verified", "manual_gui_pass");
      if (ImGui::MenuItem("MANUAL_GUI_PARTIAL"))
        saveReview("MANUAL_GUI_PARTIAL", "To Verify", "manual_gui_partial");
      if (ImGui::MenuItem("MANUAL_GUI_FAIL"))
        saveReview("MANUAL_GUI_FAIL", "Defect", "manual_gui_fail");
      ImGui::EndMenu();
    }

    ImGui::Separator();

    // This is the inverse of the metadata-only Add Hidden Evidence Case
    // catalog action. It only changes the active queue membership; it never
    // deletes a case directory, manifest, overlay, or source image.
    const std::string hiddenCaseKey =
        thumb.source_evidence_script_path.empty() || thumb.case_id.empty()
            ? std::string()
            : std::filesystem::path(thumb.source_evidence_script_path)
                  .lexically_normal()
                  .generic_string() +
                  "::" + thumb.case_id;
    const bool canHideEvidenceCase =
        !hiddenCaseKey.empty() &&
        m_manualTest.selected_hidden_evidence_case_keys.find(hiddenCaseKey) !=
            m_manualTest.selected_hidden_evidence_case_keys.end();
    if (canHideEvidenceCase && ImGui::MenuItem("Hide Evidence Case")) {
      m_manualTest.selected_hidden_evidence_case_keys.erase(hiddenCaseKey);
      m_manualTest.operator_hidden_evidence_case_keys.insert(hiddenCaseKey);
      m_manualTest.selected_evidence_group = -1;
      m_manualTest.selected_evidence_thumb = -1;
      m_manualTest.current_evidence_selection = CxEvidenceSelectionSnapshot{};
      m_manualTest.script_evidence_groups_dirty = true;
      m_manualTest.script_evidence_row_refs_dirty = true;
      m_manualTest.debug_status = "HIDDEN_EVIDENCE_CASE_REMOVED";
      m_manualTest.debug_reason = "hidden from current Evidence list: case=" +
                                  thumb.case_id + " source=" +
                                  thumb.source_evidence_script_path;
      CXLOG_INFO("EvidenceChain", "hidden_case_remove", "queued",
                 m_manualTest.debug_reason);
      rowStateReplaced = true;
      ImGui::CloseCurrentPopup();
    }
    if (canHideEvidenceCase)
      ImGui::Separator();

    if (ImGui::MenuItem("Load This Image To Image View")) {
      const std::string thumbImagePathBeforeLoad = thumb.image_path;
      std::string reason;
      if (!RefreshEvidenceSelectionFromThumb(groupIndex, thumbIndex, true,
                                             reason)) {
        m_manualTest.debug_status = "LOAD_IMAGE_FAIL";
        m_manualTest.debug_reason = reason;
      } else {
        m_manualTest.debug_status = "IMAGE_VIEW_LOADED";
        m_manualTest.debug_reason =
            "loaded from evidence row: " + thumbImagePathBeforeLoad;
      }
      rowStateReplaced = true;
    }

    if (ImGui::MenuItem("Bind Current Image View")) {
      if (m_manualTest.image_file_path.empty()) {
        m_manualTest.debug_status = "EVIDENCE_IMAGE_BIND_FAIL";
        m_manualTest.debug_reason = "current Image View image path is empty";
      } else {
        thumb.image_path = m_manualTest.image_file_path;
        thumb.image_id = m_manualTest.active_image_id.empty()
                             ? "current_image"
                             : m_manualTest.active_image_id;

        ResetEvidenceThumbTexture(thumb);

        thumb.reason = "bound from current Image View";
        const std::string boundScriptId = thumb.script_id;
        const std::string boundImagePath = thumb.image_path;

        std::string reason;
        if (!RefreshEvidenceSelectionFromThumb(groupIndex, thumbIndex, false,
                                               reason)) {
          m_manualTest.debug_status = "EVIDENCE_IMAGE_BIND_FAIL";
          m_manualTest.debug_reason = reason;
        } else {
          m_manualTest.debug_status = "EVIDENCE_IMAGE_BOUND";
          m_manualTest.debug_reason = boundScriptId + " -> " + boundImagePath;
        }
        rowStateReplaced = true;
      }
    }

    if (ImGui::MenuItem("Select Image File...")) {
      std::string selectedPath;
      std::string dialogReason;
      if (!SelectEvidenceImageFileFromDialogLocal(selectedPath, dialogReason)) {
        m_manualTest.debug_status =
            dialogReason == "image selection cancelled by operator"
                ? "EVIDENCE_IMAGE_SELECT_CANCEL"
                : "EVIDENCE_IMAGE_SELECT_DIALOG_FAIL";
        m_manualTest.debug_reason = dialogReason;
        CXLOG_WARN("EvidenceChain", "evidence_image_select", "not_loaded",
                   "script_id=" + thumb.script_id + " reason=" + dialogReason);
      } else {
        const std::string selectedScriptId = thumb.script_id;
        thumb.image_path = selectedPath;
        thumb.thumbnail_path = selectedPath;
        const std::filesystem::path selectedFs(selectedPath);
        thumb.image_id = selectedFs.stem().string();
        ResetEvidenceThumbTexture(thumb);
        thumb.reason = "bound from selected image file";

        std::string reason;
        if (!RefreshEvidenceSelectionFromThumb(groupIndex, thumbIndex, true,
                                               reason)) {
          m_manualTest.debug_status = "EVIDENCE_IMAGE_SELECT_FAIL";
          m_manualTest.debug_reason = reason;
          CXLOG_ERROR("EvidenceChain", "evidence_image_select", "load_fail",
                      "script_id=" + selectedScriptId +
                          " image_path=" + selectedPath + " reason=" + reason);
        } else {
          m_manualTest.debug_status = "EVIDENCE_IMAGE_SELECTED";
          m_manualTest.debug_reason = selectedScriptId + " -> " + selectedPath +
              " (read-only Image View load; source image unchanged)";
          CXLOG_INFO("EvidenceChain", "evidence_image_select", "loaded",
                     "script_id=" + selectedScriptId +
                         " image_path=" + selectedPath +
                         " source_mutated=false");
        }
        rowStateReplaced = true;
      }
    }

    if (ImGui::MenuItem("Use First Manifest Image")) {
      bool bound = false;

      for (const auto &item : m_manualTest.image_manifest_items) {
        if (!item.image_path.empty()) {
          thumb.image_path = item.image_path;
          thumb.image_id = item.image_id;
          ResetEvidenceThumbTexture(thumb);
          thumb.reason = "bound from manifest image";
          bound = true;
          break;
        }
      }

      if (!bound) {
        m_manualTest.debug_status = "EVIDENCE_IMAGE_BIND_FAIL";
        m_manualTest.debug_reason = "image manifest has no usable image";
      } else {
        const std::string boundImagePath = thumb.image_path;
        std::string reason;
        if (!RefreshEvidenceSelectionFromThumb(groupIndex, thumbIndex, false,
                                               reason)) {
          m_manualTest.debug_status = "EVIDENCE_IMAGE_BIND_FAIL";
          m_manualTest.debug_reason = reason;
        } else {
          m_manualTest.debug_status = "EVIDENCE_IMAGE_BOUND";
          m_manualTest.debug_reason =
              "bound first manifest image: " + boundImagePath;
        }
        rowStateReplaced = true;
      }
    }

    if (ImGui::MenuItem("Bind Current Runtime Params")) {
      thumb.parameter_summary = BuildCurrentRuntimeParamSummary(m_manualTest);
      thumb.reason = "parameter summary bound from runtime globals";
      const std::string boundParameterSummary = thumb.parameter_summary;

      std::string reason;
      if (!RefreshEvidenceSelectionFromThumb(groupIndex, thumbIndex, false,
                                             reason)) {
        m_manualTest.debug_status = "EVIDENCE_PARAM_BIND_FAIL";
        m_manualTest.debug_reason = reason;
      } else {
        m_manualTest.debug_status = "EVIDENCE_PARAM_BOUND";
        m_manualTest.debug_reason = boundParameterSummary;
      }
      rowStateReplaced = true;
    }

    if (ImGui::MenuItem("Clear Image Binding")) {
      const std::string clearedScriptId = thumb.script_id;
      thumb.image_path.clear();
      thumb.image_id.clear();
      ResetEvidenceThumbTexture(thumb);
      thumb.reason = "image binding cleared";

      std::string reason;
      if (!RefreshEvidenceSelectionFromThumb(groupIndex, thumbIndex, false,
                                             reason)) {
        m_manualTest.debug_status = "EVIDENCE_IMAGE_CLEAR_FAIL";
        m_manualTest.debug_reason = reason;
      } else {
        m_manualTest.debug_status = "EVIDENCE_IMAGE_CLEARED";
        m_manualTest.debug_reason =
            "image binding cleared for " + clearedScriptId;
      }
      rowStateReplaced = true;
    }

    ImGui::EndPopup();
  }

  if (rowStateReplaced) {
    finishRow();
    return;
  }

  if (rowHovered) {
    ImGui::SetTooltip("Click: select and load image | Right-click: menu\n"
                      "script: %s\nimage: %s\npath: %s\nreason: %s",
                      thumb.script_id.c_str(), thumb.image_id.c_str(),
                      thumb.image_path.c_str(), thumb.reason.c_str());
  }

  finishRow();
}
