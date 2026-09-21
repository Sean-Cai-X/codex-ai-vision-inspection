#include "pch.h"
#include "CxScriptGlobalValueSet.h"
#include "CxScriptHeadlessRuntime.h"
#include "ParserClass.h"

#include <fstream>
#include <sstream>
#include <string>

bool LoadHeadlessGlobalDeclarations(
    const std::string& init_script_path,
    CxScriptGlobalValueSet& values,
    std::string& reason)
{
    std::ifstream script_file(init_script_path);
    if (!script_file.is_open())
    {
        reason = "cannot open headless globals script: " + init_script_path;
        return false;
    }

    std::string line;
    int line_num = 0;
    while (std::getline(script_file, line))
    {
        line_num++;

        const size_t first = line.find_first_not_of(" \t");
        if (first == std::string::npos)
            continue;

        if (line.substr(first, 2) == "//")
            continue;

        static const char* numeric_types[] = { "int ", "double ", "float " };
        bool is_numeric = false;
        size_t type_length = 0;
        for (const char* type : numeric_types)
        {
            const size_t len = std::strlen(type);
            if (line.compare(first, len, type) == 0)
            {
                is_numeric = true;
                type_length = len;
                break;
            }
        }

        if (!is_numeric)
        {
            if (line.compare(first, 6, "Image ") == 0)
                continue;

            if (line.find("=") != std::string::npos)
                continue;

            reason = "headless global declaration unsupported at line " +
                std::to_string(line_num) + ": " + line;
            return false;
        }

        size_t name_begin = first + type_length;
        size_t name_end = name_begin;
        while (name_end < line.size() &&
               (std::isalnum(static_cast<unsigned char>(line[name_end])) || line[name_end] == '_'))
        {
            ++name_end;
        }

        if (name_end == name_begin)
            continue;

        const std::string name = line.substr(name_begin, name_end - name_begin);

        if (name.find("global.") != std::string::npos)
        {
            reason = "headless global declaration uses dot notation at line " +
                std::to_string(line_num) + ": " + name;
            return false;
        }

        if (values.numbers.count(name) > 0)
        {
            reason = "duplicate headless global declaration at line " +
                std::to_string(line_num) + ": " + name;
            return false;
        }

        values.numbers[name] = 0.0;
    }

    if (values.numbers.empty())
    {
        reason = "no global_* declarations found in: " + init_script_path;
        return false;
    }

    return true;
}

bool ApplyGlobalOverrides(
    CxScriptGlobalValueSet& values,
    const std::map<std::string, double>& overrides,
    std::string& reason)
{
    for (const auto& [name, value] : overrides)
    {
        auto it = values.numbers.find(name);
        if (it == values.numbers.end())
        {
            reason = "global override not declared in headless_globals.cxsc: " + name;
            return false;
        }
        it->second = value;
    }
    return true;
}

bool BindGlobalValueSetToParser(
    mu::CxParserRuntime& runtime,
    CxScriptGlobalValueSet& values,
    std::string& reason)
{
    for (auto& [name, value] : values.numbers)
    {
        runtime.m_parser.DefineVar(name, &value);
    }
    (void)reason;
    return true;
}

std::map<std::string, double> BuildHeadlessGlobalOverrides(
    const CxScriptHeadlessOptions& options)
{
    std::map<std::string, double> overrides;

    overrides["global_roi_x0"] = static_cast<double>(options.roi_x0);
    overrides["global_roi_y0"] = static_cast<double>(options.roi_y0);
    overrides["global_roi_x1"] = static_cast<double>(options.roi_x1);
    overrides["global_roi_y1"] = static_cast<double>(options.roi_y1);

    overrides["global_circle_cx"] = static_cast<double>(options.circle_cx);
    overrides["global_circle_cy"] = static_cast<double>(options.circle_cy);
    overrides["global_circle_px"] = static_cast<double>(options.circle_px);
    overrides["global_circle_py"] = static_cast<double>(options.circle_py);
    overrides["global_findcircle_arc_enabled"] = static_cast<double>(options.findcircle_arc_enabled);
    overrides["global_findcircle_arc_start_deg"] = static_cast<double>(options.findcircle_arc_start_deg);
    overrides["global_findcircle_arc_end_deg"] = static_cast<double>(options.findcircle_arc_end_deg);

    overrides["global_ellipse_x0"] = static_cast<double>(options.ellipse_x0);
    overrides["global_ellipse_y0"] = static_cast<double>(options.ellipse_y0);
    overrides["global_ellipse_x1"] = static_cast<double>(options.ellipse_x1);
    overrides["global_ellipse_y1"] = static_cast<double>(options.ellipse_y1);
    overrides["global_findellipse_inner_scale_percent"] =
        static_cast<double>(options.ellipse_inner_scale_percent);

    overrides["global_metrology_boundary_preview_enabled"] = 1.0;
    overrides["global_metrology_boundary_baseline_mode"] = 1.0;
    overrides["global_metrology_boundary_denoise_mode"] = 2.0;
    overrides["global_metrology_boundary_smoothing_radius"] = 2.0;
    overrides["global_metrology_boundary_baseline_window"] = 12.0;
    overrides["global_metrology_boundary_response_mode"] = 0.0;
    overrides["global_metrology_boundary_polarity"] = 2.0;
    overrides["global_metrology_boundary_wavelet_scale"] = 4.0;
    overrides["global_metrology_boundary_trigger_threshold_permille"] = 120.0;
    overrides["global_metrology_boundary_level_permille"] = 500.0;
    overrides["global_metrology_boundary_hysteresis_permille"] = 50.0;
    overrides["global_metrology_boundary_gate_start_permille"] = 0.0;
    overrides["global_metrology_boundary_gate_end_permille"] = 1000.0;
    overrides["global_metrology_boundary_selection_mode"] = 4.0;
    overrides["global_metrology_boundary_nth_candidate"] = 1.0;
    overrides["global_metrology_boundary_min_plateau_width"] = 3.0;
    overrides["global_metrology_boundary_min_amplitude_permille"] = 80.0;
    overrides["global_metrology_boundary_pair_min_width"] = 2.0;
    overrides["global_metrology_boundary_pair_max_width"] = 80.0;
    overrides["global_metrology_boundary_pair_anchor_mode"] = 0.0;
    overrides["global_metrology_boundary_subpixel_mode"] = 2.0;
    overrides["global_metrology_boundary_show_conditioned"] = 1.0;
    overrides["global_metrology_boundary_show_response"] = 1.0;
    overrides["global_metrology_boundary_show_scalogram"] = 0.0;
    overrides["global_metrology_boundary_reference_mode"] = 0.0;
    overrides["global_metrology_boundary_reference_bound"] = 0.0;
    overrides["global_metrology_boundary_reference_position_permille"] = 500.0;

    overrides["global_tool_half_width"] = static_cast<double>(options.tool_half_width);
    overrides["global_wgap"] = static_cast<double>(options.wgap);
    overrides["global_hgap"] = static_cast<double>(options.hgap);
    overrides["global_gap"] = static_cast<double>(options.gap);
    overrides["global_linegap"] = static_cast<double>(options.linegap);
    overrides["global_min_edge_run_width_px"] =
        static_cast<double>(options.min_edge_run_width_px);
    overrides["global_threshold"] = static_cast<double>(options.threshold);
    overrides["global_segmentation_threshold_percent"] = 50.0;
    overrides["global_segmentation_mode"] = 2.0;
    overrides["global_segmentation_positive_enabled"] = 0.0;
    overrides["global_segmentation_positive_x"] =
        static_cast<double>((options.roi_x0 + options.roi_x1) / 2);
    overrides["global_segmentation_positive_y"] =
        static_cast<double>((options.roi_y0 + options.roi_y1) / 2);
    overrides["global_segmentation_negative_enabled"] = 0.0;
    overrides["global_segmentation_negative_x"] = 0.0;
    overrides["global_segmentation_negative_y"] = 0.0;
    overrides["global_object_foreground_mode"] = 1.0;
    overrides["global_object_threshold"] = 20.0;
    overrides["global_object_min_area"] = 10.0;

    overrides["global_object_pixel_size_x_milli"] = 1000.0;
    overrides["global_object_pixel_size_y_milli"] = 1000.0;
    overrides["global_object_geometry_connectivity"] = 8.0;
    overrides["global_object_selected_measurement"] = 0.0;
    overrides["global_object_show_boundary"] = 1.0;
    overrides["global_object_show_moment_ellipse"] = 1.0;
    overrides["global_object_show_feret"] = 1.0;
    overrides["global_object_show_circles"] = 1.0;
    overrides["global_object_background_method"] = 0.0;
    overrides["global_object_background_border_px"] = 2.0;
    overrides["global_object_background_radius_px"] = 32.0;
    overrides["global_object_subpixel_enabled"] = 0.0;
    overrides["global_object_subpixel_min_gradient_milli"] = 3000.0;
    overrides["global_object_subpixel_iso_threshold"] = 0.0;
    overrides["global_object_geometry_basis"] = 0.0;
    overrides["global_method"] = static_cast<double>(options.method);
    overrides["global_filterprofile"] = static_cast<double>(options.filterprofile);
    overrides["global_findsetting"] = 0.0;
    overrides["global_objfilter"] = 1.0;
    overrides["global_findline_objfilter"] = 1.0;
    overrides["global_findline_findsetting"] = 1.0;
    overrides["global_findcircle_findsetting"] = 0.0;
    overrides["global_findellipse_findsetting"] = 1.0;
    overrides["global_findrect_findsetting"] = 0.0;
    overrides["global_findline_point_consistency_enabled"] = 0.0;
    overrides["global_findline_point_consistency_range"] = 0.0;
    overrides["global_findcircle_point_consistency_enabled"] = 0.0;
    overrides["global_findcircle_point_consistency_range"] = 0.0;
    overrides["global_samplerate"] = static_cast<double>(options.samplerate);
    overrides["global_min_score"] = options.min_score;
    overrides["global_min_score_percent"] = 0.0;
    overrides["global_find_num"] = static_cast<double>(options.find_num);
    overrides["global_compare_gap"] = static_cast<double>(options.compare_gap);
    overrides["global_match_step_x"] = 10.0;
    overrides["global_match_step_y"] = 10.0;
    overrides["global_match_thre"] = 10.0;
    overrides["global_fastmatch_action"] = 3.0;
    overrides["global_fastmatch_scan_rotation_deg"] = 0.0;
    overrides["global_fastmatch_learn_shared"] = 0.0;
    for (int direction = 0; direction < 4; ++direction)
    {
        const std::string suffix = "_" + std::to_string(direction);
        const bool oppositeSide = direction == 1 || direction == 3;
        overrides["global_fastmatch_learn_wgap" + suffix] =
            direction < 2 ? 8.0 : 2.0;
        overrides["global_fastmatch_learn_hgap" + suffix] =
            direction < 2 ? 2.0 : 8.0;
        overrides["global_fastmatch_learn_method" + suffix] =
            oppositeSide ? 1.0 : 0.0;
        overrides["global_fastmatch_learn_threshold" + suffix] =
            static_cast<double>(options.threshold);
        overrides["global_fastmatch_learn_linegap" + suffix] =
            static_cast<double>(options.linegap);
        overrides["global_fastmatch_learn_objfilter" + suffix] = 1.0;
        overrides["global_fastmatch_learn_compare_gap" + suffix] =
            static_cast<double>(options.compare_gap);
        overrides["global_fastmatch_learn_edge_count" + suffix] = 2.0;
        overrides["global_fastmatch_learn_selected_edge" + suffix] = 0.0;
    }
    overrides["global_fastmatch_geometry_source_index"] = 0.0;
    overrides["global_fastmatch_geometry_weight_percent"] = 25.0;
    overrides["global_fastmatch_max_pose_candidates"] = 32.0;
    // Robust Normal-Trace defaults are identical in GUI and Headless. ANN
    // spatial connectivity owns re-clustering; tangent/normal tolerances are
    // Dijkstra soft-cost preferences rather than connectivity gates.
    overrides["global_fastmatch_normaltrace_enabled"] = 0.0;
    overrides["global_fastmatch_normaltrace_overlap_radius_px"] = 3.0;
    overrides["global_fastmatch_normaltrace_min_gradient"] = 20.0;
    overrides["global_fastmatch_normaltrace_max_nodes"] = 4096.0;
    overrides["global_fastmatch_normaltrace_pair_offset_px"] = 6.0;
    overrides["global_fastmatch_normaltrace_gradient_weight_permille"] = 700.0;
    overrides["global_fastmatch_normaltrace_turn_weight_permille"] = 200.0;
    overrides["global_fastmatch_normaltrace_gap_weight_permille"] = 100.0;
    overrides["global_fastmatch_normaltrace_max_trace_gap_px"] = 3.0;
    overrides["global_fastmatch_normaltrace_knn_neighbors"] = 6.0;
    overrides["global_fastmatch_normaltrace_ann_radius_px"] = 32.0;
    overrides["global_fastmatch_normaltrace_ann_tangent_deviation_deg"] = 35.0;
    overrides["global_fastmatch_normaltrace_ann_normal_deviation_deg"] = 35.0;
    overrides["global_fastmatch_normaltrace_ann_min_component_points"] = 4.0;
    overrides["global_fastmatch_normaltrace_ann_min_component_coverage_percent"] = 55.0;
    overrides["global_fastmatch_normaltrace_angle_tolerance_deg"] = 20.0;
    overrides["global_fastmatch_normaltrace_min_length_px"] = 20.0;
    overrides["global_fastmatch_normaltrace_polarity"] = 0.0;
    overrides["global_fastmatch_normaltrace_corner_rejection_px"] = 4.0;
    overrides["global_fastmatch_normaltrace_tangent_step_px"] = 2.0;
    overrides["global_fastmatch_normaltrace_anchor_radius_px"] = 24.0;
    overrides["global_fastmatch_normaltrace_xy_compression_bin_px"] = 4.0;
    overrides["global_fastmatch_normaltrace_min_keypoints_per_domain"] = 4.0;
    overrides["global_fastmatch_normaltrace_endpoint_spike_ratio_percent"] = 250.0;
    overrides["global_fastmatch_normaltrace_junction_tangent_window_points"] = 6.0;
    overrides["global_fastmatch_normaltrace_junction_min_cross_angle_deg"] = 12.0;
    overrides["global_fastmatch_normaltrace_junction_max_extrapolation_percent"] = 150.0;
    overrides["global_fastmatch_normaltrace_junction_join_spacing_multiplier_percent"] = 800.0;
    // FastMatch-owned dense shape model and bidirectional form-fit. Disabled by
    // default so legacy rigid/Normal-Trace scripts remain bitwise compatible.
    overrides["global_fastmatch_formfit_enabled"] = 0.0;
    overrides["global_fastmatch_formfit_dense_step_milli_px"] = 1000.0;
    overrides["global_fastmatch_formfit_profile_half_width_milli_px"] = 2500.0;
    overrides["global_fastmatch_formfit_profile_step_milli_px"] = 250.0;
    overrides["global_fastmatch_formfit_min_gradient"] = 4.0;
    overrides["global_fastmatch_formfit_curvature_threshold_millideg"] = 12000.0;
    overrides["global_fastmatch_formfit_ann_radius_milli_px"] = 8000.0;
    overrides["global_fastmatch_formfit_normal_tolerance_deg"] = 35.0;
    overrides["global_fastmatch_formfit_trim_percent"] = 20.0;
    overrides["global_fastmatch_formfit_min_mutual_pairs"] = 6.0;
    overrides["global_fastmatch_formfit_max_iterations"] = 8.0;
    overrides["global_fastmatch_formfit_max_elapsed_ms"] = 80.0;
    overrides["global_fastmatch_formfit_max_anchors"] = 64.0;
    overrides["global_fastmatch_formfit_allow_affine"] = 1.0;

    overrides["global_ocr_threshold"] = 110.0;
    overrides["global_ocr_foreground_mode"] = 1.0;
    overrides["global_ocr_min_area"] = 1.0;
    overrides["global_ocr_component_distance"] = 6.0;

    overrides["global_ocr_glyph_source"] = 0.0; // 0:auto, 1:FastMatch pose, 2:FindObject glyphs
    overrides["global_ocr_layout_direction"] = 0.0;
    overrides["global_ocr_line_overlap_percent"] = 50.0;
    overrides["global_ocr_match_threshold"] = 3.0;
    overrides["global_ocr_min_score_percent"] = 50.0;
    overrides["global_strategy_id"] = static_cast<double>(options.strategy_id);

    overrides["global_algorithm_executed"] = static_cast<double>(options.algorithm_executed);

    overrides["global_learn_roi_x"] = static_cast<double>(options.learn_roi_x);
    overrides["global_learn_roi_y"] = static_cast<double>(options.learn_roi_y);
    overrides["global_learn_roi_w"] = static_cast<double>(options.learn_roi_w);
    overrides["global_learn_roi_h"] = static_cast<double>(options.learn_roi_h);
    overrides["global_search_roi_x"] = static_cast<double>(options.search_roi_x);
    overrides["global_search_roi_y"] = static_cast<double>(options.search_roi_y);
    overrides["global_search_roi_w"] = static_cast<double>(options.search_roi_w);
    overrides["global_search_roi_h"] = static_cast<double>(options.search_roi_h);
    overrides["global_expected_rect_x"] = static_cast<double>(options.expected_rect_x);
    overrides["global_expected_rect_y"] = static_cast<double>(options.expected_rect_y);
    overrides["global_expected_rect_w"] = static_cast<double>(options.expected_rect_w);
    overrides["global_expected_rect_h"] = static_cast<double>(options.expected_rect_h);
    overrides["global_learn_a_count"] = 0.0;
    overrides["global_learn_b_count"] = 0.0;
    overrides["global_learn_a2_count"] = 0.0;
    overrides["global_learn_b2_count"] = 0.0;
    overrides["global_learn_status_code"] = 0.0;
    overrides["global_match_count"] = 0.0;
    overrides["global_best_score"] = 0.0;
    overrides["global_model_point_count"] = 0.0;

    overrides["global_max_elapsed_ms"] = static_cast<double>(options.max_elapsed_ms);
    overrides["global_max_scan_lines"] = static_cast<double>(options.max_scan_lines);
    overrides["global_max_samples"] = static_cast<double>(options.max_samples);

    return overrides;
}

namespace
{
bool IsValidGlobalName(const std::string& name)
{
    if (name.empty())
        return false;
    if (name.size() < 8 || name.compare(0, 7, "global_") != 0)
        return false;
    for (size_t i = 7; i < name.size(); ++i)
    {
        char c = name[i];
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
            return false;
    }
    return true;
}

std::string Trim(const std::string& str)
{
    const size_t first = str.find_first_not_of(" \t");
    if (first == std::string::npos)
        return "";
    const size_t last = str.find_last_not_of(" \t");
    return str.substr(first, last - first + 1);
}

bool StripNumericDeclaration(std::string& name)
{
    const size_t separator = name.find_first_of(" \t");
    if (separator == std::string::npos)
        return true;

    const std::string type = name.substr(0, separator);
    if (type != "int" && type != "double" && type != "float")
        return false;

    name = Trim(name.substr(separator + 1));
    return !name.empty();
}
}

bool LoadHeadlessGlobalValuesFile(
    const std::string& values_path,
    std::map<std::string, double>& overrides,
    std::string& reason)
{
    std::ifstream file(values_path);
    if (!file.is_open())
    {
        reason = "cannot open headless globals values file: " + values_path;
        return false;
    }

    std::string line;
    int line_num = 0;
    while (std::getline(file, line))
    {
        line_num++;

        const size_t comment_pos = line.find("//");
        if (comment_pos != std::string::npos)
            line = line.substr(0, comment_pos);

        std::string trimmed = Trim(line);
        if (trimmed.empty())
            continue;

        const size_t eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos)
        {
            reason = "invalid headless globals value at line " +
                std::to_string(line_num) + ": " + line;
            return false;
        }

        std::string name_part = Trim(trimmed.substr(0, eq_pos));
        std::string value_part = Trim(trimmed.substr(eq_pos + 1));

        if (name_part.empty() || !StripNumericDeclaration(name_part))
        {
            reason = "invalid headless globals value at line " +
                std::to_string(line_num) + ": " + line;
            return false;
        }

        if (name_part.find('.') != std::string::npos)
        {
            reason = "invalid headless globals value at line " +
                std::to_string(line_num) + ": dot notation not allowed";
            return false;
        }

        if (!IsValidGlobalName(name_part))
        {
            reason = "invalid headless globals value at line " +
                std::to_string(line_num) + ": " + name_part;
            return false;
        }

        if (value_part.empty())
        {
            reason = "invalid headless globals value at line " +
                std::to_string(line_num) + ": " + line;
            return false;
        }

        if (value_part.back() == ';')
            value_part = value_part.substr(0, value_part.size() - 1);

        try
        {
            size_t consumed = 0;
            double value = std::stod(value_part, &consumed);
            if (consumed != value_part.size())
            {
                reason = "invalid headless globals value at line " +
                    std::to_string(line_num) + ": " + line;
                return false;
            }

            if (overrides.count(name_part) > 0)
            {
                reason = "duplicate headless globals value at line " +
                    std::to_string(line_num) + ": " + name_part;
                return false;
            }

            overrides[name_part] = value;
        }
        catch (const std::exception&)
        {
            reason = "invalid headless globals value at line " +
                std::to_string(line_num) + ": " + line;
            return false;
        }
    }

    return true;
}
