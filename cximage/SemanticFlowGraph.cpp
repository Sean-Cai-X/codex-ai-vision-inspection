#include "SemanticFlowGraph.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
namespace fs = std::filesystem;

std::string Trim(const std::string& text)
{
  std::size_t first = 0;
  while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first]))) ++first;
  std::size_t last = text.size();
  while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1]))) --last;
  return text.substr(first, last - first);
}

bool SplitField(const std::string& line, std::string& key, std::string& value)
{
  const std::size_t separator = line.find(':');
  if (separator == std::string::npos) return false;
  key = Trim(line.substr(0, separator));
  value = Trim(line.substr(separator + 1));
  return !key.empty();
}

std::string TitleFromCatalogScriptName(const std::string& name)
{
  std::string title = name;

  const std::string directSuffix = "_direct_test.cxsc";
  const std::size_t directPos = title.find(directSuffix);
  if (directPos != std::string::npos)
  {
    title.erase(directPos);
  }

  const std::string cxscSuffix = ".cxsc";
  const std::size_t cxscPos = title.find(cxscSuffix);
  if (cxscPos != std::string::npos)
  {
    title.erase(cxscPos);
  }

  for (char& ch : title)
  {
    if (ch == '_' || ch == '-')
      ch = ' ';
  }

  if (title.empty())
    title = "catalog script";

  return title;
}

ImU32 StatusColor(const std::string& status)
{
  if (status == "PASS") return IM_COL32(60, 180, 90, 255);
  if (status == "FAIL") return IM_COL32(220, 70, 70, 255);
  if (status == "BLOCKED") return IM_COL32(230, 130, 45, 255);
  if (status == "PENDING" || status == "running") return IM_COL32(215, 180, 55, 255);
  return IM_COL32(115, 125, 135, 255);
}

ImU32 EdgeColor(const std::string& type)
{
  if (type == "feedback" || type == "issue") return IM_COL32(235, 125, 70, 255);
  return IM_COL32(160, 165, 170, 255);
}
}

void SemanticFlowGraph::Initialize(const std::string& repository_root)
{
  m_repositoryRoot = repository_root;
  LoadDemoFlow();
}

void SemanticFlowGraph::LoadDemoFlow()
{
  m_currentWorkingDir = fs::current_path().generic_string();
  m_currentFlowPath = m_demoRelativePath;
  const fs::path repositoryRoot(m_repositoryRoot);
  fs::path resolved = fs::absolute(repositoryRoot / fs::path(m_demoRelativePath)).lexically_normal();
  if (!fs::exists(resolved))
  {
    fs::path current = fs::current_path();
    while (!current.empty())
    {
      const fs::path directCandidate = current / fs::path(m_demoRelativePath);
      const fs::path workspaceCandidate = current / "cxvisionai" / "cxvision_repo" /
                                          fs::path(m_demoRelativePath);
      if (fs::exists(directCandidate))
      {
        resolved = fs::absolute(directCandidate).lexically_normal();
        break;
      }
      if (fs::exists(workspaceCandidate))
      {
        resolved = fs::absolute(workspaceCandidate).lexically_normal();
        break;
      }
      const fs::path parent = current.parent_path();
      if (parent == current) break;
      current = parent;
    }
  }
  m_resolvedDemoPath = resolved.generic_string();
  m_demoFileExists = fs::exists(resolved) && fs::is_regular_file(resolved);
  if (!m_demoFileExists)
  {
    m_flow = SemanticFlow();
    m_flow.selected_node_index = -1;
    m_loadStatus = "BLOCKED";
    m_loadReason = "demo flow file not found";
    m_lastLog = m_loadReason;
    return;
  }
  LoadFlowFile(m_resolvedDemoPath);
}

bool SemanticFlowGraph::LoadFlowFile(const std::string& path)
{
  std::ifstream input(path);
  if (!input)
  {
    m_flow = SemanticFlow();
    m_flow.selected_node_index = -1;
    m_loadStatus = "BLOCKED";
    m_loadReason = "flow file open failed";
    m_lastLog = m_loadReason + ": " + path;
    return false;
  }

  SemanticFlow parsed;
  SemanticNode* node = nullptr;
  SemanticEdge* edge = nullptr;
  std::string line;
  while (std::getline(input, line))
  {
    line = Trim(line);
    if (line.empty() || line[0] == '#') continue;
    std::string key;
    std::string value;
    if (!SplitField(line, key, value)) continue;
    if (key == "flow")
    {
      parsed.id = value;
      node = nullptr;
      edge = nullptr;
    }
    else if (key == "description") parsed.description = value;
    else if (key == "node")
    {
      parsed.nodes.push_back(SemanticNode());
      node = &parsed.nodes.back();
      node->id = value;
      node->status = "ready";
      edge = nullptr;
    }
    else if (key == "edge")
    {
      const std::size_t arrow = value.find("->");
      if (arrow == std::string::npos) continue;
      parsed.edges.push_back(SemanticEdge());
      edge = &parsed.edges.back();
      edge->from = Trim(value.substr(0, arrow));
      edge->to = Trim(value.substr(arrow + 2));
      node = nullptr;
    }
    else if (node != nullptr)
    {
      if (key == "stage") node->stage = value;
      else if (key == "module") node->module = value;
      else if (key == "title") node->title = value;
      else if (key == "script") node->script_path = value;
      else if (key == "status_from") node->status_from = value;
      else if (key == "status") node->status = value;
    }
    else if (edge != nullptr)
    {
      if (key == "condition") edge->condition = value;
      else if (key == "type") edge->type = value;
    }
  }

  if (parsed.nodes.empty())
  {
    m_flow = SemanticFlow();
    m_flow.selected_node_index = -1;
    m_loadStatus = "BLOCKED";
    m_loadReason = "flow parse produced no nodes";
    m_lastLog = m_loadReason + ": " + path;
    return false;
  }
  parsed.selected_node_index = 0;
  m_flow = parsed;
  m_loadStatus = "READY";
  m_loadReason = "demo flow loaded";
  m_lastLog = m_loadReason + ": " + parsed.id;
  return true;
}

void SemanticFlowGraph::ClearFlow()
{
  m_flow = SemanticFlow();
  m_loadStatus = "PENDING";
  m_loadReason = "flow cleared";
  m_lastLog = m_loadReason;
}

SemanticNode* SemanticFlowGraph::SelectedNode()
{
  if (m_flow.selected_node_index < 0 ||
      m_flow.selected_node_index >= static_cast<int>(m_flow.nodes.size())) return nullptr;
  return &m_flow.nodes[static_cast<std::size_t>(m_flow.selected_node_index)];
}

const SemanticNode* SemanticFlowGraph::SelectedNode() const
{
  if (m_flow.selected_node_index < 0 ||
      m_flow.selected_node_index >= static_cast<int>(m_flow.nodes.size())) return nullptr;
  return &m_flow.nodes[static_cast<std::size_t>(m_flow.selected_node_index)];
}

bool SemanticFlowGraph::BindScriptToNode(int node_index,
                                         const std::string& script_path,
                                         const std::string& title_hint,
                                         std::string& out_reason)
{
  if (node_index < 0 ||
      node_index >= static_cast<int>(m_flow.nodes.size()))
  {
    out_reason = "no selected semantic flow node";
    m_lastLog = out_reason;
    return false;
  }

  if (script_path.empty())
  {
    out_reason = "selected catalog script path is empty";
    m_lastLog = out_reason;
    return false;
  }

  const fs::path resolved = fs::path(script_path).is_absolute()
    ? fs::path(script_path)
    : fs::path(m_repositoryRoot) / fs::path(script_path);
  if (!fs::exists(resolved) || !fs::is_regular_file(resolved))
  {
    out_reason = "selected catalog script does not exist: " +
      resolved.lexically_normal().generic_string();
    m_lastLog = out_reason;
    return false;
  }
  if (resolved.extension() != ".cxsc")
  {
    out_reason = "selected catalog entry is not a .cxsc script";
    m_lastLog = out_reason;
    return false;
  }

  SemanticNode& node = m_flow.nodes[static_cast<std::size_t>(node_index)];
  node.script_path = script_path;
  node.status = "ready";
  node.reason = "script bound from Script Catalog";
  node.result_ref.clear();
  node.evidence_ref.clear();
  node.issue_entry_ref.clear();

  if (!title_hint.empty())
  {
    node.title = TitleFromCatalogScriptName(title_hint);
  }

  if (node.module.empty())
  {
    node.module = script_path.find("/cximage/") != std::string::npos ? "cximage" : "cxscript";
  }

  m_sharedBoundNodeId = node.id;
  m_sharedBoundScriptPath = node.script_path;

  m_lastLog = "catalog script bound to selected node: " + node.script_path;
  out_reason = m_lastLog;

  return true;
}

bool SemanticFlowGraph::BindEvidenceToNode(
    int node_index,
    const SemanticEvidenceBinding& binding,
    std::string& reason)
{
    reason.clear();

    if (node_index < 0 ||
        node_index >= static_cast<int>(m_flow.nodes.size()))
    {
        reason = "invalid semantic node index";
        return false;
    }

    if (!binding.valid)
    {
        reason = "invalid evidence binding";
        return false;
    }

    if (binding.script_path.empty())
    {
        reason = "evidence binding has empty script_path";
        return false;
    }

    SemanticNode& node = m_flow.nodes[static_cast<std::size_t>(node_index)];

    node.evidence_binding = binding;

    node.script_path = binding.script_path;

    if (!binding.script_id.empty())
        node.title = binding.script_id;

    if (!binding.tool.empty())
        node.module = binding.tool;
    else
        node.module = "cximage";

    node.status = "ready";
    node.reason =
        "bound from evidence: image=" + binding.image_id +
        " target=" + binding.target_id +
        " param=" + binding.parameter_summary;

    m_lastLog = node.reason;
    reason = node.reason;
    return true;
}

const SemanticEvidenceBinding* SemanticFlowGraph::SelectedNodeEvidenceBinding() const
{
    const SemanticNode* node = SelectedNode();
    if (node == nullptr)
        return nullptr;

    if (!node->evidence_binding.valid)
        return nullptr;

    return &node->evidence_binding;
}

void SemanticFlowGraph::DrawGraphCanvas(SemanticFlowAction& action)
{
  const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
  ImVec2 canvasSize(ImGui::GetContentRegionAvail().x, 250.0f);
  canvasSize.x = std::max(canvasSize.x, 720.0f);
  ImGui::Dummy(canvasSize);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                      IM_COL32(35, 38, 42, 255));

  const float nodeWidth = 150.0f;
  const float nodeHeight = 105.0f;
  const float gap = 45.0f;
  const float startX = canvasPos.x + 25.0f;
  const float nodeY = canvasPos.y + 70.0f;
  auto nodePosition = [&](std::size_t index) { return ImVec2(startX + index * (nodeWidth + gap), nodeY); };
  auto findNode = [&](const std::string& id) -> int {
    for (std::size_t i = 0; i < m_flow.nodes.size(); ++i)
      if (m_flow.nodes[i].id == id) return static_cast<int>(i);
    return -1;
  };

  for (const SemanticEdge& edge : m_flow.edges)
  {
    const int from = findNode(edge.from);
    const int to = findNode(edge.to);
    if (from < 0 || to < 0) continue;
    const ImVec2 fromPos = nodePosition(static_cast<std::size_t>(from));
    const ImVec2 toPos = nodePosition(static_cast<std::size_t>(to));
    ImVec2 a(fromPos.x + nodeWidth, fromPos.y + nodeHeight * 0.5f);
    ImVec2 b(toPos.x, toPos.y + nodeHeight * 0.5f);
    if (to <= from)
    {
      a = ImVec2(fromPos.x + nodeWidth * 0.5f, fromPos.y + nodeHeight);
      b = ImVec2(toPos.x + nodeWidth * 0.5f, toPos.y + nodeHeight);
      const float feedbackY = nodeY + nodeHeight + 45.0f;
      draw->AddLine(a, ImVec2(a.x, feedbackY), EdgeColor(edge.type), 2.0f);
      draw->AddLine(ImVec2(a.x, feedbackY), ImVec2(b.x, feedbackY), EdgeColor(edge.type), 2.0f);
      draw->AddLine(ImVec2(b.x, feedbackY), b, EdgeColor(edge.type), 2.0f);
    }
    else draw->AddLine(a, b, EdgeColor(edge.type), 2.0f);
    draw->AddTriangleFilled(b, ImVec2(b.x - 8.0f, b.y - 5.0f), ImVec2(b.x - 8.0f, b.y + 5.0f),
                            EdgeColor(edge.type));
    const ImVec2 label((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f - 18.0f);
    draw->AddText(label, EdgeColor(edge.type), edge.condition.c_str());
  }

  for (std::size_t i = 0; i < m_flow.nodes.size(); ++i)
  {
    const SemanticNode& node = m_flow.nodes[i];
    const ImVec2 pos = nodePosition(i);
    const ImVec2 end(pos.x + nodeWidth, pos.y + nodeHeight);
    ImGui::SetCursorScreenPos(pos);
    ImGui::PushID(static_cast<int>(i));
    if (ImGui::InvisibleButton("node", ImVec2(nodeWidth, nodeHeight)))
      m_flow.selected_node_index = static_cast<int>(i);
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
    {
      m_flow.selected_node_index = static_cast<int>(i);
      if (node.script_path.empty() || node.script_path == "none")
      {
        m_lastLog = "node has no bound script";
      }
      else
      {
        m_sharedBoundNodeId = node.id;
        m_sharedBoundScriptPath = node.script_path;
        m_lastLog = "node script loaded to Manual State Test Console";
        action.type = SemanticFlowActionType::LoadBoundScript;
        action.node_index = static_cast<int>(i);
        action.node_id = node.id;
        action.script_path = node.script_path;

        if (node.evidence_binding.valid)
        {
            action.has_evidence_binding = true;
            action.evidence_binding = node.evidence_binding;
        }
      }
    }
    ImGui::PopID();
    draw->AddRectFilled(pos, end, StatusColor(node.status), 5.0f);
    draw->AddRect(pos, end,
                  m_flow.selected_node_index == static_cast<int>(i) ? IM_COL32(80, 170, 255, 255)
                                                                    : IM_COL32(205, 205, 205, 255),
                  5.0f, 0, m_flow.selected_node_index == static_cast<int>(i) ? 3.0f : 1.0f);
    draw->AddText(ImVec2(pos.x + 8.0f, pos.y + 7.0f), IM_COL32_WHITE, (node.id + " [" + node.stage + "]").c_str());

    std::string module = node.module;
    if (node.evidence_binding.valid && !node.evidence_binding.tool.empty())
        module = node.evidence_binding.tool;
    draw->AddText(ImVec2(pos.x + 8.0f, pos.y + 29.0f), IM_COL32_WHITE, module.c_str());

    std::string title = node.title;
    if (node.evidence_binding.valid && !node.evidence_binding.script_id.empty())
        title = node.evidence_binding.script_id;
    draw->AddText(ImVec2(pos.x + 8.0f, pos.y + 50.0f), IM_COL32_WHITE, title.c_str());

    draw->AddText(ImVec2(pos.x + 8.0f, pos.y + 72.0f), IM_COL32_WHITE, node.status.c_str());
  }
  ImGui::SetCursorScreenPos(ImVec2(canvasPos.x, canvasPos.y + canvasSize.y));
}

void SemanticFlowGraph::DrawNodeDetail(SemanticFlowAction& action)
{
  SemanticNode* node = SelectedNode();
  if (node == nullptr)
  {
    ImGui::TextDisabled("No node selected. Click a flow node first.");
    return;
  }

  ImGui::Text("Selected: %s [%s]  status=%s",
              node->id.c_str(),
              node->stage.c_str(),
              node->status.c_str());

  if (ImGui::Button("Bind Selected Evidence To Node"))
  {
    action.type = SemanticFlowActionType::BindCatalogScriptToSelectedNode;
    action.node_index = m_flow.selected_node_index;
    action.node_id = node->id;
    m_lastLog = "bind selected evidence requested";
  }
  ImGui::TextDisabled("Uses selected Evidence row; falls back to legacy catalog if none selected.");
  ImGui::SameLine();
  if (ImGui::Button("Load Node To Manual Console"))
  {
    if (node->script_path.empty() || node->script_path == "none")
    {
      m_lastLog = "node has no bound script";
    }
    else
    {
      m_sharedBoundNodeId = node->id;
      m_sharedBoundScriptPath = node->script_path;
      m_lastLog = "node script loaded to Manual State Test Console";
      action.type = SemanticFlowActionType::LoadBoundScript;
      action.node_index = m_flow.selected_node_index;
      action.node_id = node->id;
      action.script_path = node->script_path;

      if (node->evidence_binding.valid)
      {
          action.has_evidence_binding = true;
          action.evidence_binding = node->evidence_binding;
      }
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Run Bound Script"))
  {
    if (node->script_path.empty() || node->script_path == "none")
    {
      node->status = "PENDING";
      node->reason = "no bound script";
      m_lastLog = "Run blocked: node has no bound script";
    }
    else
    {
      node->status = "running";
      action.type = SemanticFlowActionType::RunBoundScript;
      action.node_index = m_flow.selected_node_index;
      action.node_id = node->id;
      action.script_path = node->script_path;

      if (node->evidence_binding.valid)
      {
          action.has_evidence_binding = true;
          action.evidence_binding = node->evidence_binding;
      }
    }
  }
  ImGui::TextDisabled("PASS requires parser runtime result");
  if (ImGui::Button("Mark FAIL")) { node->status = "FAIL"; node->reason = "manual_mark"; }
  ImGui::SameLine();
  if (ImGui::Button("Mark BLOCKED")) { node->status = "BLOCKED"; node->reason = "manual_mark"; }
  ImGui::SameLine();
  if (ImGui::Button("Mark PENDING")) { node->status = "PENDING"; node->reason = "manual_mark"; }
  if (ImGui::Button("Clear Node Result"))
  {
    node->status = "ready";
    node->reason.clear();
    node->result_ref.clear();
    node->evidence_ref.clear();
    node->issue_entry_ref.clear();
  }

    if (node->evidence_binding.valid)
    {
        ImGui::Separator();
        ImGui::TextUnformatted("Bound Evidence:");
        ImGui::Text("source: %s",
                    node->evidence_binding.source.empty()
                        ? "-"
                        : node->evidence_binding.source.c_str());
        ImGui::Text("case: %s",
                    node->evidence_binding.case_id.empty()
                        ? "-"
                        : node->evidence_binding.case_id.c_str());
        ImGui::Text("script: %s",
                    node->evidence_binding.script_id.empty()
                        ? "-"
                        : node->evidence_binding.script_id.c_str());
        ImGui::Text("image: %s",
                    node->evidence_binding.image_id.empty()
                        ? "-"
                        : node->evidence_binding.image_id.c_str());
        ImGui::Text("target: %s",
                    node->evidence_binding.target_id.empty()
                        ? "-"
                        : node->evidence_binding.target_id.c_str());
        ImGui::Text("tool: %s",
                    node->evidence_binding.tool.empty()
                        ? "-"
                        : node->evidence_binding.tool.c_str());
        ImGui::TextWrapped("param: %s",
                    node->evidence_binding.parameter_summary.empty()
                        ? "-"
                        : node->evidence_binding.parameter_summary.c_str());
        ImGui::TextDisabled("Load/Run will restore evidence script, image, target and params.");
    }
    else
    {
        ImGui::Separator();
        ImGui::TextDisabled("Bound Evidence: none");
    }
}

SemanticFlowAction SemanticFlowGraph::Draw()
{
  SemanticFlowAction action;
  if (!m_open) return action;
  ImGui::SetNextWindowSize(ImVec2(760.0f, 520.0f), ImGuiCond_Once);
  if (!ImGui::Begin("Semantic Flow Graph", &m_open))
  {
    ImGui::End();
    return action;
  }

  DrawGraphCanvas(action);
  ImGui::Separator();
  DrawNodeDetail(action);

  if (ImGui::CollapsingHeader("Advanced Flow Debug"))
  {
    ImGui::TextWrapped("Flow File: %s", m_currentFlowPath.empty() ? "(none)" : m_currentFlowPath.c_str());
    ImGui::TextWrapped("Current working dir: %s", m_currentWorkingDir.empty() ? "(unknown)" : m_currentWorkingDir.c_str());
    ImGui::TextWrapped("Default demo path: %s", m_demoRelativePath.c_str());
    ImGui::TextWrapped("Resolved demo path: %s", m_resolvedDemoPath.empty() ? "(none)" : m_resolvedDemoPath.c_str());
    ImGui::Text("File exists: %s", m_demoFileExists ? "true" : "false");
    ImGui::Text("Load status: %s", m_loadStatus.c_str());
    ImGui::TextWrapped("Reason: %s", m_loadReason.c_str());
    ImGui::Text("Node count: %d", static_cast<int>(m_flow.nodes.size()));
    ImGui::Text("Edge count: %d", static_cast<int>(m_flow.edges.size()));
    ImGui::Text("Runtime Debug Summary");
    ImGui::Text("doutputvalue: %s", m_runtimeDoutputValue.c_str());
    ImGui::Text("current_status: %s", m_runtimeCurrentStatus.c_str());
    ImGui::Text("runtime_object_count: %d", m_runtimeObjectCount);
    ImGui::TextWrapped("last_runtime_reason: %s", m_lastRuntimeReason.c_str());
    if (ImGui::Button("Load Demo Flow")) LoadDemoFlow();
    ImGui::SameLine();
    if (ImGui::Button("Reload Flow") && !m_resolvedDemoPath.empty()) LoadDemoFlow();
    ImGui::SameLine();
    if (ImGui::Button("Clear Flow")) ClearFlow();
    ImGui::Text("flow: %s", m_flow.id.empty() ? "(none)" : m_flow.id.c_str());
    ImGui::TextWrapped("description: %s", m_flow.description.empty() ? "(none)" : m_flow.description.c_str());
    if (!m_flow.edges.empty())
    {
      ImGui::Text("Connections:");
      for (const SemanticEdge& edge : m_flow.edges)
        ImGui::BulletText("%s -> %s", edge.from.c_str(), edge.to.c_str());
    }
    ImGui::Separator();
    SemanticNode* node = SelectedNode();
    if (node != nullptr)
    {
      ImGui::Text("Node Detail");
      ImGui::Text("id: %s", node->id.c_str());
      ImGui::Text("stage: %s", node->stage.c_str());
      ImGui::Text("module: %s", node->module.c_str());
      ImGui::TextWrapped("title: %s", node->title.c_str());
      ImGui::TextWrapped("script_path: %s", node->script_path.c_str());
      ImGui::TextWrapped("status_from: %s", node->status_from.empty() ? "(none)" : node->status_from.c_str());
      ImGui::TextWrapped("reason: %s", node->reason.empty() ? "(none)" : node->reason.c_str());
      ImGui::TextWrapped("result_ref: %s", node->result_ref.empty() ? "(none)" : node->result_ref.c_str());
      ImGui::TextWrapped("evidence_ref: %s", node->evidence_ref.empty() ? "(none)" : node->evidence_ref.c_str());
      ImGui::TextWrapped("issue_entry_ref: %s", node->issue_entry_ref.empty() ? "(none)" : node->issue_entry_ref.c_str());
    }
    ImGui::TextWrapped("shared_bound_node_id: %s", m_sharedBoundNodeId.empty() ? "(none)" : m_sharedBoundNodeId.c_str());
    ImGui::TextWrapped("shared_bound_script_path: %s", m_sharedBoundScriptPath.empty() ? "(none)" : m_sharedBoundScriptPath.c_str());
    ImGui::TextWrapped("log: %s", m_lastLog.empty() ? "(none)" : m_lastLog.c_str());
  }

  ImGui::End();
  return action;
}

void SemanticFlowGraph::SetRuntimeDebugSummary(
  const std::string& doutputValue, const std::string& currentStatus,
  int runtimeObjectCount, const std::string& reason)
{
  m_runtimeDoutputValue = doutputValue;
  m_runtimeCurrentStatus = currentStatus;
  m_runtimeObjectCount = runtimeObjectCount;
  m_lastRuntimeReason = reason;
}

void SemanticFlowGraph::ApplyScriptResult(int node_index,
                                          const std::string& status,
                                          const std::string& result_ref,
                                          const std::string& evidence_ref,
                                          const std::string& issue_entry_ref,
                                          const std::string& reason)
{
  if (node_index < 0 || node_index >= static_cast<int>(m_flow.nodes.size())) return;
  SemanticNode& node = m_flow.nodes[static_cast<std::size_t>(node_index)];
  const bool missingRuntimePackage = status == "PASS" && result_ref.empty();
  node.status = missingRuntimePackage ? "PENDING" : status;
  node.reason = missingRuntimePackage ? "runtime result package missing" : reason;
  node.result_ref = result_ref;
  node.evidence_ref = evidence_ref;
  node.issue_entry_ref = issue_entry_ref;
  m_lastLog = "ScriptResult filled back to node " + node.id;
}
