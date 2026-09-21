#include "metrology_analytics/CxMetrologyUiGlobals.h"
#include "pch.h"

#include <fstream>
#include <sstream>

namespace cxvision::metrology_analytics {
namespace {

void Add(std::vector<CxMetrologyUiGlobalPair> &out, const char *name,
         int value) {
  out.push_back(CxMetrologyUiGlobalPair{name, value});
}

std::string JsonEscape(const std::string &text) {
  std::string out;
  for (char c : text) {
    switch (c) {
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
      out += c;
      break;
    }
  }
  return out;
}

} // namespace

CxMetrologyUiGlobalFields DefaultMetrologyUiGlobalFields() {
  return CxMetrologyUiGlobalFields{};
}

std::vector<CxMetrologyUiGlobalPair>
BuildMetrologyUiGlobalSnapshot(const CxMetrologyUiGlobalFields &m) {
  std::vector<CxMetrologyUiGlobalPair> out;
  out.reserve(96);

  Add(out, "global_metrology_enabled", m.enabled ? 1 : 0);
  Add(out, "global_metrology_active_tab", m.active_tab);

  Add(out, "global_metrology_gauge_line_num", m.gauge_line_num);

  Add(out, "global_metrology_show_scan_profile", m.show_scan_profile ? 1 : 0);
  Add(out, "global_metrology_scan_profile_source", m.scan_profile_source);
  Add(out, "global_metrology_scan_profile_max_lines", m.scan_profile_max_lines);
  Add(out, "global_metrology_scan_profile_sample_stride",
      m.scan_profile_sample_stride);
  Add(out, "global_metrology_scan_profile_edge_band_index",
      m.scan_profile_edge_band_index);
  Add(out, "global_metrology_scan_profile_smoothing_radius",
      m.scan_profile_smoothing_radius);

  Add(out, "global_metrology_show_edge_band_candidates",
      m.show_edge_band_candidates ? 1 : 0);
  Add(out, "global_metrology_candidate_rank", m.candidate_rank);
  Add(out, "global_metrology_candidate_min_gradient", m.candidate_min_gradient);
  Add(out, "global_metrology_candidate_max_width", m.candidate_max_width);
  Add(out, "global_metrology_feature_map_mode", m.feature_map_mode);
  Add(out, "global_metrology_feature_map_normalize", m.feature_map_normalize);

  Add(out, "global_metrology_boundary_preview_enabled",
      m.boundary_preview_enabled ? 1 : 0);
  Add(out, "global_metrology_boundary_baseline_mode",
      m.boundary_baseline_mode);
  Add(out, "global_metrology_boundary_denoise_mode", m.boundary_denoise_mode);
  Add(out, "global_metrology_boundary_smoothing_radius",
      m.boundary_smoothing_radius);
  Add(out, "global_metrology_boundary_baseline_window",
      m.boundary_baseline_window);
  Add(out, "global_metrology_boundary_response_mode",
      m.boundary_response_mode);
  Add(out, "global_metrology_boundary_polarity", m.boundary_polarity);
  Add(out, "global_metrology_boundary_wavelet_scale",
      m.boundary_wavelet_scale);
  Add(out, "global_metrology_boundary_trigger_threshold_permille",
      m.boundary_trigger_threshold_permille);
  Add(out, "global_metrology_boundary_level_permille",
      m.boundary_level_permille);
  Add(out, "global_metrology_boundary_hysteresis_permille",
      m.boundary_hysteresis_permille);
  Add(out, "global_metrology_boundary_gate_start_permille",
      m.boundary_gate_start_permille);
  Add(out, "global_metrology_boundary_gate_end_permille",
      m.boundary_gate_end_permille);
  Add(out, "global_metrology_boundary_selection_mode",
      m.boundary_selection_mode);
  Add(out, "global_metrology_boundary_nth_candidate",
      m.boundary_nth_candidate);
  Add(out, "global_metrology_boundary_min_plateau_width",
      m.boundary_min_plateau_width);
  Add(out, "global_metrology_boundary_min_amplitude_permille",
      m.boundary_min_amplitude_permille);
  Add(out, "global_metrology_boundary_pair_min_width",
      m.boundary_pair_min_width);
  Add(out, "global_metrology_boundary_pair_max_width",
      m.boundary_pair_max_width);
  Add(out, "global_metrology_boundary_pair_anchor_mode",
      m.boundary_pair_anchor_mode);
  Add(out, "global_metrology_boundary_subpixel_mode",
      m.boundary_subpixel_mode);
  Add(out, "global_metrology_boundary_show_conditioned",
      m.boundary_show_conditioned ? 1 : 0);
  Add(out, "global_metrology_boundary_show_response",
      m.boundary_show_response ? 1 : 0);
  Add(out, "global_metrology_boundary_show_scalogram",
      m.boundary_show_scalogram ? 1 : 0);
  Add(out, "global_metrology_boundary_reference_mode",
      m.boundary_reference_mode);
  Add(out, "global_metrology_boundary_reference_bound",
      m.boundary_reference_bound ? 1 : 0);
  Add(out, "global_metrology_boundary_reference_position_permille",
      m.boundary_reference_position_permille);

  Add(out, "global_metrology_surface_source", m.surface_source);
  Add(out, "global_metrology_surface_width", m.surface_width);
  Add(out, "global_metrology_surface_height", m.surface_height);
  Add(out, "global_metrology_surface_stride", m.surface_stride);
  Add(out, "global_metrology_surface_z_channel", m.surface_z_channel);
  Add(out, "global_metrology_surface_area_method", m.surface_area_method);
  Add(out, "global_metrology_histogram_bins", m.histogram_bins);
  Add(out, "global_metrology_histogram_mode", m.histogram_mode);
  Add(out, "global_metrology_histogram_log_scale",
      m.histogram_log_scale ? 1 : 0);

  Add(out, "global_metrology_peak_max_count", m.peak_max_count);
  Add(out, "global_metrology_peak_order", m.peak_order);
  Add(out, "global_metrology_peak_min_prominence_permille",
      m.peak_min_prominence_permille);
  Add(out, "global_metrology_peak_min_distance_bins", m.peak_min_distance_bins);
  Add(out, "global_metrology_peak_background", m.peak_background);
  Add(out, "global_metrology_peak_invert", m.peak_invert ? 1 : 0);

  Add(out, "global_metrology_curve_fit_source", m.curve_fit_source);
  Add(out, "global_metrology_curve_fit_function", m.curve_fit_function);
  Add(out, "global_metrology_curve_fit_auto_estimate",
      m.curve_fit_auto_estimate ? 1 : 0);
  Add(out, "global_metrology_curve_fit_auto_plot",
      m.curve_fit_auto_plot ? 1 : 0);
  Add(out, "global_metrology_curve_fit_full_range",
      m.curve_fit_full_range ? 1 : 0);
  Add(out, "global_metrology_curve_fit_output_residual",
      m.curve_fit_output_residual ? 1 : 0);
  Add(out, "global_metrology_curve_fit_range_start_permille",
      m.curve_fit_range_start_permille);
  Add(out, "global_metrology_curve_fit_range_end_permille",
      m.curve_fit_range_end_permille);

  Add(out, "global_metrology_critical_dimension_source",
      m.critical_dimension_source);
  Add(out, "global_metrology_critical_dimension_function",
      m.critical_dimension_function);
  Add(out, "global_metrology_critical_dimension_auto_fit",
      m.critical_dimension_auto_fit ? 1 : 0);
  Add(out, "global_metrology_critical_dimension_full_range",
      m.critical_dimension_full_range ? 1 : 0);
  Add(out, "global_metrology_critical_dimension_range_start_permille",
      m.critical_dimension_range_start_permille);
  Add(out, "global_metrology_critical_dimension_range_end_permille",
      m.critical_dimension_range_end_permille);
  Add(out, "global_metrology_critical_dimension_draw_whole_circle",
      m.critical_dimension_draw_whole_circle ? 1 : 0);

  Add(out, "global_metrology_enable_plane_correction",
      m.enable_plane_correction ? 1 : 0);
  Add(out, "global_metrology_plane_method", m.plane_method);
  Add(out, "global_metrology_plane_reference_mode", m.plane_reference_mode);
  Add(out, "global_metrology_plane_huber_delta_permille",
      m.plane_huber_delta_permille);

  Add(out, "global_metrology_x_unit", m.x_unit);
  Add(out, "global_metrology_y_unit", m.y_unit);
  Add(out, "global_metrology_z_unit", m.z_unit);
  Add(out, "global_metrology_x_scale_permille", m.x_scale_permille);
  Add(out, "global_metrology_y_scale_permille", m.y_scale_permille);
  Add(out, "global_metrology_z_scale_permille", m.z_scale_permille);
  Add(out, "global_metrology_enable_gaussian_z", m.enable_gaussian_z ? 1 : 0);
  Add(out, "global_metrology_gaussian_z_sigma_permille",
      m.gaussian_z_sigma_permille);
  Add(out, "global_metrology_gaussian_seed", m.gaussian_seed);

  Add(out, "global_metrology_enable_iso_roughness_1d",
      m.enable_iso_roughness_1d ? 1 : 0);
  Add(out, "global_metrology_roughness_profile_axis", m.roughness_profile_axis);
  Add(out, "global_metrology_roughness_profile_index",
      m.roughness_profile_index);
  Add(out, "global_metrology_roughness_cutoff_px", m.roughness_cutoff_px);
  Add(out, "global_metrology_roughness_bins", m.roughness_bins);

  return out;
}

std::string BuildMetrologyUiGlobalSummary(const CxMetrologyUiGlobalFields &m) {
  std::ostringstream s;
  s << "metrology enabled=" << (m.enabled ? 1 : 0) << " tab=" << m.active_tab

    << " gauge_line_num=" << m.gauge_line_num
    << " scan(max=" << m.scan_profile_max_lines
    << ",stride=" << m.scan_profile_sample_stride
    << ",edge=" << m.scan_profile_edge_band_index << ")"
    << " candidate(rank=" << m.candidate_rank
    << ",grad=" << m.candidate_min_gradient << ",feature=" << m.feature_map_mode
    << ")"
    << " boundary(mode=" << m.boundary_response_mode
    << ",polarity=" << m.boundary_polarity
    << ",threshold=" << m.boundary_trigger_threshold_permille
    << ",gate=" << m.boundary_gate_start_permille << ":"
    << m.boundary_gate_end_permille
    << ",selection=" << m.boundary_selection_mode
    << ",pair_anchor=" << m.boundary_pair_anchor_mode
    << ",reference=" << (m.boundary_reference_bound ? 1 : 0)
    << "@" << m.boundary_reference_position_permille << ")"
    << " surface=" << m.surface_width << "x" << m.surface_height
    << " stride=" << m.surface_stride
    << " area_method=" << m.surface_area_method
    << " hist_bins=" << m.histogram_bins

    << " peaks=" << m.peak_max_count
    << "/prom=" << m.peak_min_prominence_permille
    << "/distance=" << m.peak_min_distance_bins
    << "/invert=" << (m.peak_invert ? 1 : 0)

    << " curve_fit=" << m.curve_fit_function
    << "/auto=" << (m.curve_fit_auto_estimate ? 1 : 0)
    << "/full=" << (m.curve_fit_full_range ? 1 : 0)
    << " critical_dimension=" << m.critical_dimension_function
    << "/auto=" << (m.critical_dimension_auto_fit ? 1 : 0)
    << "/full=" << (m.critical_dimension_full_range ? 1 : 0)

    << " plane=" << (m.enable_plane_correction ? 1 : 0) << "/" << m.plane_method
    << " unit=" << m.x_unit << "," << m.y_unit << "," << m.z_unit
    << " scale_permille=" << m.x_scale_permille << "," << m.y_scale_permille
    << "," << m.z_scale_permille
    << " gaussian_z=" << (m.enable_gaussian_z ? 1 : 0) << "/"
    << m.gaussian_z_sigma_permille
    << " roughness=" << (m.enable_iso_roughness_1d ? 1 : 0) << "/"
    << m.roughness_bins;
  return s.str();
}

bool WriteMetrologyUiGlobalSnapshotJson(const std::filesystem::path &path,
                                        const CxMetrologyUiGlobalFields &fields,
                                        std::string &reason) {
  std::error_code ec;
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
      reason = "failed to create metrology ui snapshot dir: " + ec.message();
      return false;
    }
  }

  const std::vector<CxMetrologyUiGlobalPair> globals =
      BuildMetrologyUiGlobalSnapshot(fields);

  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f.is_open()) {
    reason = "failed to open metrology ui globals snapshot";
    return false;
  }

  f << "{\n";
  f << "  \"schema\": \"cxvision.metrology_ui_globals.v1\",\n";
  f << "  \"note\": \"UI parameter entry only; this file does not claim "
       "algorithm PASS.\",\n";
  f << "  \"global_count\": " << globals.size() << ",\n";
  f << "  \"summary\": \"" << JsonEscape(BuildMetrologyUiGlobalSummary(fields))
    << "\",\n";
  f << "  \"globals\": {\n";
  for (std::size_t i = 0; i < globals.size(); ++i) {
    f << "    \"" << JsonEscape(globals[i].name) << "\": " << globals[i].value
      << (i + 1 < globals.size() ? "," : "") << "\n";
  }
  f << "  }\n";
  f << "}\n";
  reason.clear();
  return true;
}

} // namespace cxvision::metrology_analytics
