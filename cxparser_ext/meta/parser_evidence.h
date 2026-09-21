#ifndef CXPARSER_EXT_PARSER_EVIDENCE_H
#define CXPARSER_EXT_PARSER_EVIDENCE_H

#include <string>
#include <vector>

namespace cxparser_ext
{
enum EvidenceEventLevel
{
  eel_info,
  eel_warning,
  eel_error
};

struct EvidenceEvent
{
  EvidenceEventLevel level = eel_info;
  std::string stage;
  std::string code;
  std::string message;
  std::string expected;
  std::string actual;
};

struct EvidenceFrame
{
  std::string function_name;
  std::string address_text;
  std::string module_name;
};

struct EvidenceCall
{
  std::string class_name;
  std::string method_name;
  std::vector<std::string> args_text;
  std::string return_text;
  bool success = false;
};

struct EvidenceDebugState
{
  std::string current_address;
  std::string current_symbol;
  std::vector<std::string> registers;
  std::vector<std::string> threads;
  std::vector<std::string> memory_maps;
};

struct EvidenceGraphNode
{
  std::string id;
  std::string label;
};

struct EvidenceGraphEdge
{
  std::string from;
  std::string to;
};

struct ParserTraceEntry
{
  int sequence = 0;
  std::string trace_id;
  std::string stage;
  std::string action;
  std::string status;
  std::string detail;
};

struct ParserLogEntry
{
  std::string trace_id;
  std::string level;
  std::string stage;
  std::string code;
  std::string message;
};

struct ParserEvidenceBundle
{
  std::string task_id;
  std::string trace_id;
  std::string route_key;
  std::string route_lane;
  std::string protocol_name;
  std::string task_type;
  std::string task_subtype;
  std::string execution_mode;
  std::vector<std::string> module_chain;
  std::vector<EvidenceEvent> events;
  std::vector<ParserTraceEntry> trace_entries;
  std::vector<ParserLogEntry> log_entries;
  std::vector<EvidenceCall> calls;
  std::vector<EvidenceFrame> backtrace;
  EvidenceDebugState debug_state;
  std::vector<EvidenceGraphNode> graph_nodes;
  std::vector<EvidenceGraphEdge> graph_edges;
  std::string disasm_text;
  std::string decompile_text;
  std::vector<std::string> notes;
};
}

#endif
