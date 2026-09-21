#ifndef CXIMAGE_SEMANTIC_FLOW_TYPES_H
#define CXIMAGE_SEMANTIC_FLOW_TYPES_H

#include <string>
#include <vector>

struct SemanticEvidenceBinding
{
    bool valid = false;

    std::string case_id;

    std::string script_id;
    std::string script_path;

    std::string image_id;
    std::string image_path;

    std::string target_id;
    std::string tool;

    std::string parameter_profile_id;
    std::string parameter_summary;

    std::string status;
    std::string reason;

    std::string source;
};

struct SemanticNode
{
  std::string id;
  std::string stage;
  std::string module;
  std::string title;
  std::string script_path;

  SemanticEvidenceBinding evidence_binding;

  std::string status_from;
  std::string status;
  std::string reason;
  std::string result_ref;
  std::string evidence_ref;
  std::string issue_entry_ref;
};

struct SemanticEdge
{
  std::string from;
  std::string to;
  std::string condition;
  std::string type;
};

struct SemanticFlow
{
  std::string id;
  std::string description;
  std::vector<SemanticNode> nodes;
  std::vector<SemanticEdge> edges;
  int selected_node_index = -1;
};

enum class SemanticFlowActionType
{
  None,

  /*
   * Request ViewController to bind the current Evidence selection
   * to the selected semantic flow node.
   *
   * ViewController should prefer ManualTestContext.current_evidence_selection.
   * If no Evidence row is selected, it may fallback to the legacy Script Catalog
   * selection for compatibility.
   */
  BindCatalogScriptToSelectedNode,

  LoadBoundScript,
  RunBoundScript
};

struct SemanticFlowAction
{
  SemanticFlowActionType type = SemanticFlowActionType::None;
  int node_index = -1;
  std::string node_id;
  std::string script_path;

  bool has_evidence_binding = false;
  SemanticEvidenceBinding evidence_binding;
};

#endif