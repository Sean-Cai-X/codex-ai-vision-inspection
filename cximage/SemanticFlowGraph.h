#ifndef CXIMAGE_SEMANTIC_FLOW_GRAPH_H
#define CXIMAGE_SEMANTIC_FLOW_GRAPH_H

#include "SemanticFlowTypes.h"

#include <string>

class SemanticFlowGraph
{
public:
  void Initialize(const std::string& repository_root);
  bool BindScriptToNode(int node_index,
                        const std::string& script_path,
                        const std::string& title_hint,
                        std::string& out_reason);
  SemanticFlowAction Draw();
  void SetRuntimeDebugSummary(const std::string& doutputValue,
                              const std::string& currentStatus,
                              int runtimeObjectCount,
                              const std::string& reason);
  void ApplyScriptResult(int node_index,
                         const std::string& status,
                         const std::string& result_ref,
                         const std::string& evidence_ref,
                         const std::string& issue_entry_ref,
                         const std::string& reason);

    bool BindEvidenceToNode(
        int node_index,
        const SemanticEvidenceBinding& binding,
        std::string& reason);

    const SemanticEvidenceBinding* SelectedNodeEvidenceBinding() const;

private:
  bool LoadFlowFile(const std::string& path);
  void LoadDemoFlow();
  void ClearFlow();
  void DrawGraphCanvas(SemanticFlowAction& action);
  void DrawNodeDetail(SemanticFlowAction& action);
  SemanticNode* SelectedNode();
  const SemanticNode* SelectedNode() const;

  std::string m_repositoryRoot;
  std::string m_demoRelativePath =
    "cxparser/cxscript/state_machine/examples/cximage_find_circle_explore.cxflow";
  std::string m_currentWorkingDir;
  std::string m_resolvedDemoPath;
  std::string m_loadStatus = "PENDING";
  std::string m_loadReason = "not loaded";
  bool m_demoFileExists = false;
  std::string m_currentFlowPath;
  std::string m_lastLog;
  std::string m_sharedBoundNodeId;
  std::string m_sharedBoundScriptPath;
  std::string m_runtimeDoutputValue = "PENDING";
  std::string m_runtimeCurrentStatus = "PENDING";
  int m_runtimeObjectCount = 0;
  std::string m_lastRuntimeReason = "not queried";
  SemanticFlow m_flow;
  bool m_open = true;
};

#endif