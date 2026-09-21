#ifndef CXPARSER_EXT_RADARE_TYPES_H
#define CXPARSER_EXT_RADARE_TYPES_H

#include <string>
#include <vector>

#include "../../meta/parser_evidence.h"

namespace cxparser_ext
{
struct RadareTarget
{
  std::string binary_path;
  std::string working_dir;
  std::vector<std::string> args;
};

struct RadareFunctionInfo
{
  std::string name;
  std::string address;
  int size = 0;
  std::vector<std::string> call_targets;
};

struct RadareCallGraph
{
  std::vector<EvidenceGraphNode> nodes;
  std::vector<EvidenceGraphEdge> edges;
};

struct RadareDebugSnapshot
{
  std::string current_address;
  std::string current_symbol;
  std::vector<std::string> registers;
  std::vector<std::string> threads;
  std::vector<std::string> memory_maps;
};

struct RadareAnalysisResult
{
  bool success = false;
  std::string current_symbol;
  std::string current_address;
  std::vector<RadareFunctionInfo> functions;
  RadareCallGraph call_graph;
  std::string disasm_text;
  std::string decompile_text;
  std::vector<std::string> diagnostics;
};
}

#endif
