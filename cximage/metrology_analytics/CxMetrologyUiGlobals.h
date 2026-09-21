#ifndef CXIMAGE_METROLOGY_ANALYTICS_CXMETROLOGYUIGLOBALS_H
#define CXIMAGE_METROLOGY_ANALYTICS_CXMETROLOGYUIGLOBALS_H

#include <filesystem>
#include <string>
#include <vector>

namespace cxvision::metrology_analytics {

struct CxMetrologyUiGlobalPair {
  std::string name;
  int value = 0;
};

struct CxMetrologyUiGlobalFields {
  bool enabled = false;
  int active_tab = 0;

  int gauge_line_num = 1;

  bool show_scan_profile = false;
  int scan_profile_source = 0;
  int scan_profile_max_lines = 256;
  int scan_profile_sample_stride = 1;
  int scan_profile_edge_band_index = 0;
  int scan_profile_smoothing_radius = 1;

  bool show_edge_band_candidates = false;
  int candidate_rank = 0;
  int candidate_min_gradient = 8;
  int candidate_max_width = 80;
  int feature_map_mode = 0;
  int feature_map_normalize = 1;

  bool boundary_preview_enabled = true;
  int boundary_baseline_mode = 1;
  int boundary_denoise_mode = 2;
  int boundary_smoothing_radius = 2;
  int boundary_baseline_window = 12;
  int boundary_response_mode = 0;
  int boundary_polarity = 2;
  int boundary_wavelet_scale = 4;
  int boundary_trigger_threshold_permille = 120;
  int boundary_level_permille = 500;
  int boundary_hysteresis_permille = 50;
  int boundary_gate_start_permille = 0;
  int boundary_gate_end_permille = 1000;
  int boundary_selection_mode = 4;
  int boundary_nth_candidate = 1;
  int boundary_min_plateau_width = 3;
  int boundary_min_amplitude_permille = 80;
  int boundary_pair_min_width = 2;
  int boundary_pair_max_width = 80;
  int boundary_pair_anchor_mode = 0;
  int boundary_subpixel_mode = 2;
  bool boundary_show_conditioned = true;
  bool boundary_show_response = true;
  bool boundary_show_scalogram = false;
  int boundary_reference_mode = 0;
  bool boundary_reference_bound = false;
  int boundary_reference_position_permille = 500;

  int surface_source = 0;
  int surface_width = 256;
  int surface_height = 256;
  int surface_stride = 1;
  int surface_z_channel = 0;
  int surface_area_method = 1;
  int histogram_bins = 256;
  int histogram_mode = 0;
  bool histogram_log_scale = false;

  int peak_max_count = 12;
  int peak_order = 0;
  int peak_min_prominence_permille = 20;
  int peak_min_distance_bins = 4;
  int peak_background = 1;
  bool peak_invert = false;

  int curve_fit_source = 0;
  int curve_fit_function = 0;
  bool curve_fit_auto_estimate = true;
  bool curve_fit_auto_plot = true;
  bool curve_fit_full_range = true;
  bool curve_fit_output_residual = false;
  int curve_fit_range_start_permille = 0;
  int curve_fit_range_end_permille = 1000;

  int critical_dimension_source = 0;
  int critical_dimension_function = 0;
  bool critical_dimension_auto_fit = true;
  bool critical_dimension_full_range = true;
  int critical_dimension_range_start_permille = 0;
  int critical_dimension_range_end_permille = 1000;
  bool critical_dimension_draw_whole_circle = false;

  bool enable_plane_correction = false;
  int plane_method = 1;
  int plane_reference_mode = 0;
  int plane_huber_delta_permille = 100;

  int x_unit = 2;
  int y_unit = 2;
  int z_unit = 2;
  int x_scale_permille = 1000;
  int y_scale_permille = 1000;
  int z_scale_permille = 1000;
  bool enable_gaussian_z = false;
  int gaussian_z_sigma_permille = 0;
  int gaussian_seed = 42;

  bool enable_iso_roughness_1d = false;
  int roughness_profile_axis = 0;
  int roughness_profile_index = 0;
  int roughness_cutoff_px = 0;
  int roughness_bins = 1024;
};

CxMetrologyUiGlobalFields DefaultMetrologyUiGlobalFields();

std::vector<CxMetrologyUiGlobalPair>
BuildMetrologyUiGlobalSnapshot(const CxMetrologyUiGlobalFields &fields);

std::string
BuildMetrologyUiGlobalSummary(const CxMetrologyUiGlobalFields &fields);

bool WriteMetrologyUiGlobalSnapshotJson(const std::filesystem::path &path,
                                        const CxMetrologyUiGlobalFields &fields,
                                        std::string &reason);

} // namespace cxvision::metrology_analytics

#endif
