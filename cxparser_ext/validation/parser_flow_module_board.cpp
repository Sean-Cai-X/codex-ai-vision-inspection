#include "parser_flow_module_board.h"

#include <sstream>

namespace cxparser_ext
{
namespace
{
void PushUnique(std::vector<std::string> &values, const std::string &value)
{
  for (std::size_t index = 0; index < values.size(); ++index)
  {
    if (values[index] == value)
    {
      return;
    }
  }

  values.push_back(value);
}

FlowExecutionLink BuildExecutionLink(const FlowValidationSummary &summary)
{
  FlowExecutionLink link;
  link.flow_id = summary.flow_id;

  if (summary.requires_geometry_bulk)
  {
    PushUnique(link.smoke_targets, "cxgeom_bulk_create_smoke");
    PushUnique(link.smoke_targets, "cxgeom_bulk_presentation_smoke");
    PushUnique(link.smoke_targets, "cxgeom_bulk_release_smoke");
    PushUnique(link.capability_points, "geometry_bulk");
  }

  if (summary.requires_cloud_bulk)
  {
    PushUnique(link.smoke_targets, "cxcloud_bulk_create_smoke");
    PushUnique(link.smoke_targets, "cxcloud_bulk_render_smoke");
    PushUnique(link.smoke_targets, "cxcloud_bulk_release_smoke");
    PushUnique(link.capability_points, "cloud_bulk");
  }

  if (summary.requires_scene_mapping)
  {
    if (summary.module == "cxgeom")
    {
      PushUnique(link.smoke_targets, "cxgeom_scene_mapping_smoke");
    }
    else if (summary.module == "cxcloud")
    {
      PushUnique(link.smoke_targets, "cxcloud_scene_mapping_smoke");
    }
    else
    {
      PushUnique(link.smoke_targets, "cxgeom_scene_mapping_smoke");
      PushUnique(link.smoke_targets, "cxcloud_scene_mapping_smoke");
    }
    PushUnique(link.capability_points, "scene_mapping");
  }

  if (summary.requires_refresh_watch)
  {
    PushUnique(link.smoke_targets, "cxgeom_refresh_smoke");
    PushUnique(link.capability_points, "refresh_watch");
  }

  if (summary.requires_latency_watch)
  {
    PushUnique(link.capability_points, "latency_watch");
  }

  if (summary.publish_path_involved)
  {
    PushUnique(link.capability_points, "publish_path");
  }

  for (std::size_t index = 0; index < summary.fragment_ids.size(); ++index)
  {
    PushUnique(link.fragment_ids, summary.fragment_ids[index]);
  }

  for (std::size_t index = 0; index < summary.bundle_ids.size(); ++index)
  {
    PushUnique(link.bundle_ids, summary.bundle_ids[index]);
  }

  return link;
}

}

bool BuildFlowModuleBoard(const std::string &board_name,
                          const std::vector<FlowValidationSummary> &summaries,
                          FlowModuleBoard &board)
{
  board = FlowModuleBoard();
  board.board_name = board_name;
  board.flows = summaries;

  for (std::size_t index = 0; index < summaries.size(); ++index)
  {
    const FlowValidationSummary &summary = summaries[index];
    board.links.push_back(BuildExecutionLink(summary));
    if (summary.ready_for_current_module_validation)
    {
      ++board.ready_count;
    }
    else
    {
      ++board.blocked_count;
      if (!summary.blocker_reason.empty())
      {
        PushUnique(board.blockers, summary.flow_id + ": " + summary.blocker_reason);
      }
    }

    board.geometry_bulk_ready = board.geometry_bulk_ready || summary.requires_geometry_bulk;
    board.cloud_bulk_ready = board.cloud_bulk_ready || summary.requires_cloud_bulk;
    board.scene_mapping_ready = board.scene_mapping_ready || summary.requires_scene_mapping;
    board.refresh_watch_ready = board.refresh_watch_ready || summary.requires_refresh_watch;
    board.latency_watch_ready = board.latency_watch_ready || summary.requires_latency_watch;

    for (std::size_t bundle_index = 0;
         bundle_index < summary.bundle_ids.size();
         ++bundle_index)
    {
      PushUnique(board.covered_bundle_ids, summary.bundle_ids[bundle_index]);
    }

    for (std::size_t group_index = 0;
         group_index < summary.workflow_path_groups.size();
         ++group_index)
    {
      PushUnique(board.covered_bundle_groups,
                 summary.workflow_path_groups[group_index]);
    }
  }

  if (board.geometry_bulk_ready)
  {
    PushUnique(board.focus_points, "geometry_bulk");
  }

  if (board.cloud_bulk_ready)
  {
    PushUnique(board.focus_points, "cloud_bulk");
  }

  if (board.scene_mapping_ready)
  {
    PushUnique(board.focus_points, "scene_mapping");
  }

  if (board.refresh_watch_ready)
  {
    PushUnique(board.focus_points, "refresh_watch");
  }

  if (board.latency_watch_ready)
  {
    PushUnique(board.focus_points, "latency_watch");
  }

  return true;
}

std::string BuildFlowModuleBoardReport(const FlowModuleBoard &board)
{
  std::ostringstream report;
  report << "[FLOW-BOARD]"
         << " name=" << board.board_name
         << " ready=" << board.ready_count
         << " blocked=" << board.blocked_count
         << " focus_points=" << board.focus_points.size()
         << " links=" << board.links.size();

  for (std::size_t index = 0; index < board.focus_points.size(); ++index)
  {
    report << "\n[FOCUS] " << board.focus_points[index];
  }

  for (std::size_t index = 0; index < board.covered_bundle_ids.size(); ++index)
  {
    report << "\n[COVERED-BUNDLE] " << board.covered_bundle_ids[index];
  }

  for (std::size_t index = 0; index < board.covered_bundle_groups.size(); ++index)
  {
    report << "\n[COVERED-GROUP] " << board.covered_bundle_groups[index];
  }

  for (std::size_t index = 0; index < board.links.size(); ++index)
  {
    const FlowExecutionLink &link = board.links[index];
    report << "\n[FLOW] " << link.flow_id;

    for (std::size_t smoke_index = 0; smoke_index < link.smoke_targets.size(); ++smoke_index)
    {
      report << "\n  [SMOKE] " << link.smoke_targets[smoke_index];
    }

    for (std::size_t capability_index = 0; capability_index < link.capability_points.size(); ++capability_index)
    {
      report << "\n  [CAPABILITY] " << link.capability_points[capability_index];
    }

    for (std::size_t fragment_index = 0; fragment_index < link.fragment_ids.size(); ++fragment_index)
    {
      report << "\n  [FRAGMENT] " << link.fragment_ids[fragment_index];
    }

    for (std::size_t bundle_index = 0; bundle_index < link.bundle_ids.size(); ++bundle_index)
    {
      report << "\n  [BUNDLE] " << link.bundle_ids[bundle_index];
    }
  }

  for (std::size_t index = 0; index < board.blockers.size(); ++index)
  {
    report << "\n[BLOCKER] " << board.blockers[index];
  }

  return report.str();
}
}
