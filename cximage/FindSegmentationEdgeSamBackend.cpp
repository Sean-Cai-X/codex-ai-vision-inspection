#include "FindSegmentationEdgeSamBackend.h"
#include "TorchRuntimeBridge.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace {
std::string FindSegmentationTorchRuntimeDllPath() {
#ifdef _WIN32
  char module_path[MAX_PATH] = {};
  const DWORD length = GetModuleFileNameA(nullptr, module_path, MAX_PATH);
  if (length > 0 && length < MAX_PATH) {
    std::filesystem::path exe_path(module_path);
    std::filesystem::path dll_path =
        exe_path.parent_path() / "libtorch_module_runtime.dll";
    if (std::filesystem::exists(dll_path))
      return dll_path.string();
  }
#endif
  return "libtorch_module_runtime.dll";
}

std::filesystem::path
ResolveFindSegmentationAssetPath(const std::string &configured_path) {
  auto resolve_existing =
      [](const std::filesystem::path &candidate) -> std::filesystem::path {
    if (candidate.empty())
      return {};

    std::error_code ec;
    if (candidate.is_absolute() && std::filesystem::exists(candidate, ec))
      return candidate;

    const std::filesystem::path from_cwd =
        std::filesystem::current_path(ec) / candidate;
    if (!ec && std::filesystem::exists(from_cwd, ec))
      return from_cwd;

#ifdef _WIN32
    char module_path[MAX_PATH] = {};
    const DWORD length = GetModuleFileNameA(nullptr, module_path, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
      std::filesystem::path dir =
          std::filesystem::path(module_path).parent_path();
      for (int i = 0; i < 8 && !dir.empty(); ++i) {
        const std::filesystem::path from_parent = dir / candidate;
        if (std::filesystem::exists(from_parent, ec))
          return from_parent;

        const std::filesystem::path from_repo_child =
            dir / "cxvision_repo" / candidate;
        if (std::filesystem::exists(from_repo_child, ec))
          return from_repo_child;

        const std::filesystem::path from_workspace_repo_child =
            dir / "cxvisionai" / "cxvision_repo" / candidate;
        if (std::filesystem::exists(from_workspace_repo_child, ec))
          return from_workspace_repo_child;

        dir = dir.parent_path();
      }
    }
#endif

    return candidate;
  };

  return configured_path.empty() ? std::filesystem::path()
                                 : resolve_existing(configured_path);
}

bool ReadFindSegmentationTextFile(const std::filesystem::path &path,
                                  std::string &text) {
  text.clear();

  std::ifstream input(path);
  if (!input)

    return false;

  text.assign(std::istreambuf_iterator<char>(input),
              std::istreambuf_iterator<char>());
  return true;
}

bool ExtractFindSegmentationJsonNumber(const std::string &json,
                                       const std::string &key, double &value) {
  const std::string needle = "\"" + key + "\"";
  const std::size_t key_pos = json.find(needle);
  if (key_pos == std::string::npos)
    return false;

  const std::size_t colon_pos = json.find(':', key_pos + needle.size());
  if (colon_pos == std::string::npos)
    return false;

  const char *cursor = json.c_str() + colon_pos + 1;
  while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor)))
    ++cursor;

  char *end = nullptr;
  const double parsed = std::strtod(cursor, &end);
  if (end == cursor)
    return false;

  value = parsed;
  return true;
}

bool ExtractFindSegmentationJsonStringBefore(
    const std::string &json, const std::string &key,
    std::size_t before_position, std::string &value) {
  value.clear();
  const std::string needle = "\"" + key + "\"";
  const std::size_t key_pos = json.rfind(needle, before_position);
  if (key_pos == std::string::npos)
    return false;
  const std::size_t colon_pos = json.find(':', key_pos + needle.size());
  if (colon_pos == std::string::npos || colon_pos >= before_position)
    return false;
  const std::size_t quote_begin = json.find('"', colon_pos + 1);
  if (quote_begin == std::string::npos || quote_begin >= before_position)
    return false;
  const std::size_t quote_end = json.find('"', quote_begin + 1);
  if (quote_end == std::string::npos || quote_end >= before_position)
    return false;
  value = json.substr(quote_begin + 1, quote_end - quote_begin - 1);
  return !value.empty();
}

std::vector<double> ExtractFindSegmentationJsonNumbers(
    const std::string &text) {
  std::vector<double> values;
  const char *cursor = text.c_str();
  char *next = nullptr;

  while (*cursor != '\0') {
    if (std::isdigit(static_cast<unsigned char>(*cursor)) || *cursor == '-' ||
        *cursor == '+' || *cursor == '.') {
      const double parsed = std::strtod(cursor, &next);
      if (next != cursor) {
        values.push_back(parsed);
        cursor = next;
        continue;
      }
    }
    ++cursor;
  }

  return values;
}

std::size_t FindMatchingJsonArrayEnd(const std::string &json,
                                     std::size_t array_begin) {
  int depth = 0;
  for (std::size_t i = array_begin; i < json.size(); ++i) {
    if (json[i] == '[')
      ++depth;
    else if (json[i] == ']') {
      --depth;
      if (depth == 0)
        return i;
    }
  }
  return std::string::npos;
}

bool ParseFindSegmentationYoloOuterContours(
    const std::string &json, FindSegmentationResult &output) {
  std::size_t search_pos = 0;
  std::vector<FindSegmentationContour> contours;
  double best_area = 0.0;

  while (true) {
    const std::size_t key_pos = json.find("\"outer_contours\"", search_pos);
    if (key_pos == std::string::npos)
      break;

    const std::size_t array_begin = json.find('[', key_pos);
    if (array_begin == std::string::npos)
      break;

    const std::size_t array_end =
        FindMatchingJsonArrayEnd(json, array_begin);
    if (array_end == std::string::npos)
      break;

    const std::string outer_text =
        json.substr(array_begin, array_end - array_begin + 1);

    FindSegmentationContour contour;
    ExtractFindSegmentationJsonStringBefore(
        json, "stable_id", key_pos, contour.source_instance_id);
    std::size_t point_pos = 0;
    while (true) {
      const std::size_t x_key = outer_text.find("\"x\"", point_pos);
      if (x_key == std::string::npos)
        break;

      const std::size_t x_colon = outer_text.find(':', x_key);
      const std::size_t y_key = outer_text.find("\"y\"", x_key);
      const std::size_t y_colon =
          y_key == std::string::npos ? std::string::npos
                                     : outer_text.find(':', y_key);
      if (x_colon == std::string::npos || y_colon == std::string::npos)
        break;

      char *x_end = nullptr;
      char *y_end = nullptr;
      const double x =
          std::strtod(outer_text.c_str() + x_colon + 1, &x_end);
      const double y =
          std::strtod(outer_text.c_str() + y_colon + 1, &y_end);
      if (x_end != outer_text.c_str() + x_colon + 1 &&
          y_end != outer_text.c_str() + y_colon + 1) {
        contour.points.emplace_back(static_cast<int>(std::lround(x)),
                                    static_cast<int>(std::lround(y)));
      }

      point_pos = y_colon + 1;
    }

    if (contour.points.size() >= 3) {
      contour.area = std::abs(cv::contourArea(contour.points));
      best_area = (std::max)(best_area, contour.area);
      contours.push_back(std::move(contour));
    }

    search_pos = array_end + 1;
  }

  if (contours.empty())
    return false;

  output.contours = std::move(contours);
  output.contour_count = static_cast<int>(output.contours.size());
  output.primary_area = best_area;
  output.region_count = output.contour_count;
  return true;
}

bool ParseFindSegmentationContours(const std::filesystem::path &contour_path,
                                   FindSegmentationResult &output) {
  std::string json;
  if (!ReadFindSegmentationTextFile(contour_path, json))
    return false;

  if (json.find("\"outer_contours\"") != std::string::npos &&
      ParseFindSegmentationYoloOuterContours(json, output))
    return true;

  double contour_count = 0.0;
  if (ExtractFindSegmentationJsonNumber(json, "contour_count", contour_count))
    output.contour_count = static_cast<int>(contour_count);

  double primary_area = 0.0;
  if (ExtractFindSegmentationJsonNumber(json, "area", primary_area))
    output.primary_area = primary_area;

  const std::size_t points_key = json.find("\"points\"");
  if (points_key == std::string::npos)
    return output.contour_count > 0;

  const std::size_t array_begin = json.find('[', points_key);
  if (array_begin == std::string::npos)
    return output.contour_count > 0;

  int depth = 0;
  std::size_t array_end = std::string::npos;
  for (std::size_t i = array_begin; i < json.size(); ++i) {
    if (json[i] == '[')
      ++depth;
    else if (json[i] == ']') {
      --depth;
      if (depth == 0) {
        array_end = i;
        break;
      }
    }
  }

  if (array_end == std::string::npos)
    return output.contour_count > 0;

  const std::string array_text =
      json.substr(array_begin, array_end - array_begin + 1);
  std::vector<double> values = ExtractFindSegmentationJsonNumbers(array_text);

  if (values.size() < 4 || (values.size() % 2) != 0)
    return output.contour_count > 0;

  FindSegmentationContour contour;
  contour.area = output.primary_area;
  for (std::size_t i = 0; i + 1 < values.size(); i += 2) {
    contour.points.emplace_back(static_cast<int>(values[i]),
                                static_cast<int>(values[i + 1]));
  }

  output.contours.clear();
  output.contours.push_back(std::move(contour));
  output.contour_count = 1;
  return true;
}

bool ApplyPromptRectConstraintToTorchSegmentationResult(
    const FindSegmentationInput &input, const std::filesystem::path &result_dir,
    const cv::Rect &runtime_input_roi, FindSegmentationResult &output) {
  if (!input.has_rect || input.image.empty())
    return false;

  const cv::Rect image_bounds(0, 0, input.image.cols, input.image.rows);
  const cv::Rect image_roi = input.rect & image_bounds;
  const cv::Rect constrained_image_roi = image_roi & runtime_input_roi;
  if (image_roi.width <= 0 || image_roi.height <= 0 ||
      constrained_image_roi.width <= 0 || constrained_image_roi.height <= 0) {
    std::cout << "[FindSegmentation] prompt roi constraint skipped: invalid "
                 "image_roi="
              << image_roi.x << "," << image_roi.y << "," << image_roi.width
              << "," << image_roi.height << " runtime_input_roi="
              << runtime_input_roi.x << "," << runtime_input_roi.y << ","
              << runtime_input_roi.width << "," << runtime_input_roi.height
              << "\n"
              << std::flush;
    return false;
  }

  const std::filesystem::path raw_mask_path = result_dir / "mask_binary.png";
  cv::Mat raw_mask = cv::imread(raw_mask_path.string(), cv::IMREAD_GRAYSCALE);
  if (raw_mask.empty()) {
    std::cout
        << "[FindSegmentation] prompt roi constraint skipped: mask missing "
        << raw_mask_path.string() << "\n"
        << std::flush;
    return false;
  }

  const double sx = static_cast<double>(raw_mask.cols) /
                    static_cast<double>(runtime_input_roi.width);
  const double sy = static_cast<double>(raw_mask.rows) /
                    static_cast<double>(runtime_input_roi.height);
  if (sx <= 0.0 || sy <= 0.0)
    return false;

  const int mask_x0 = static_cast<int>(std::round(
      (constrained_image_roi.x - runtime_input_roi.x) * sx));
  const int mask_y0 = static_cast<int>(std::round(
      (constrained_image_roi.y - runtime_input_roi.y) * sy));
  const int mask_x1 = static_cast<int>(std::round(
      (constrained_image_roi.x + constrained_image_roi.width -
       runtime_input_roi.x) *
      sx));
  const int mask_y1 = static_cast<int>(std::round(
      (constrained_image_roi.y + constrained_image_roi.height -
       runtime_input_roi.y) *
      sy));
  cv::Rect mask_roi(mask_x0, mask_y0, mask_x1 - mask_x0,
                    mask_y1 - mask_y0);
  mask_roi &= cv::Rect(0, 0, raw_mask.cols, raw_mask.rows);
  if (mask_roi.width <= 0 || mask_roi.height <= 0)
    return false;

  cv::Mat runtime_mask;
  if (raw_mask.cols == runtime_input_roi.width &&
      raw_mask.rows == runtime_input_roi.height) {
    runtime_mask = raw_mask.clone();
  } else {
    cv::resize(raw_mask, runtime_mask, runtime_input_roi.size(), 0.0, 0.0,
               cv::INTER_NEAREST);
  }
  cv::threshold(runtime_mask, runtime_mask, 0, 255, cv::THRESH_BINARY);

  const int raw_foreground = cv::countNonZero(runtime_mask);
  const double raw_foreground_ratio =
      runtime_mask.total() == 0
          ? 0.0
          : static_cast<double>(raw_foreground) /
                static_cast<double>(runtime_mask.total());

  auto runtimePoint = [&](const cv::Point &point, bool &in_runtime_input) {
    in_runtime_input = runtime_input_roi.contains(point);
    return cv::Point(point.x - runtime_input_roi.x,
                     point.y - runtime_input_roi.y);
  };

  bool positive_prompt_in_runtime_input = false;
  const cv::Point positive_runtime_point =
      runtimePoint(input.positive_point, positive_prompt_in_runtime_input);
  bool negative_prompt_in_runtime_input = false;
  const cv::Point negative_runtime_point =
      runtimePoint(input.negative_point, negative_prompt_in_runtime_input);

  bool refinement_ok = true;
  bool refinement_applied = false;
  if (input.has_positive_point || input.has_negative_point) {
    cv::Mat grabcut_labels(runtime_mask.size(), CV_8UC1,
                           cv::Scalar(cv::GC_PR_BGD));
    grabcut_labels.setTo(cv::GC_PR_FGD, runtime_mask);

    const int min_side = (std::min)(runtime_mask.cols, runtime_mask.rows);
    const int border = (std::max)(1, min_side / 256);
    const int point_radius = (std::max)(3, min_side / 128);
    grabcut_labels.rowRange(0, border).setTo(cv::GC_BGD);
    grabcut_labels.rowRange(grabcut_labels.rows - border,
                            grabcut_labels.rows)
        .setTo(cv::GC_BGD);
    grabcut_labels.colRange(0, border).setTo(cv::GC_BGD);
    grabcut_labels.colRange(grabcut_labels.cols - border,
                            grabcut_labels.cols)
        .setTo(cv::GC_BGD);

    if (input.has_positive_point && positive_prompt_in_runtime_input) {
      cv::circle(grabcut_labels, positive_runtime_point, point_radius,
                 cv::Scalar(cv::GC_FGD), cv::FILLED);
    }
    if (input.has_negative_point && negative_prompt_in_runtime_input) {
      cv::circle(grabcut_labels, negative_runtime_point, point_radius,
                 cv::Scalar(cv::GC_BGD), cv::FILLED);
    }

    try {
      cv::Mat background_model;
      cv::Mat foreground_model;
      cv::grabCut(input.image(runtime_input_roi), grabcut_labels, cv::Rect(),
                  background_model, foreground_model, 3,
                  cv::GC_INIT_WITH_MASK);
      runtime_mask =
          (grabcut_labels == cv::GC_FGD) |
          (grabcut_labels == cv::GC_PR_FGD);
      runtime_mask.convertTo(runtime_mask, CV_8UC1, 255);
      refinement_applied = true;
    } catch (const cv::Exception &ex) {
      refinement_ok = false;
      std::cout << "[FindSegmentation] prompt refinement failed: "
                << ex.what() << "\n"
                << std::flush;
    }
  }

  if (refinement_ok && input.has_positive_point &&
      positive_prompt_in_runtime_input) {
    cv::Mat component_labels;
    const int component_count =
        cv::connectedComponents(runtime_mask, component_labels, 8, CV_32S);
    const int positive_label =
        component_labels.at<int>(positive_runtime_point.y,
                                 positive_runtime_point.x);
    if (positive_label > 0 && component_count > positive_label) {
      runtime_mask = component_labels == positive_label;
      runtime_mask.convertTo(runtime_mask, CV_8UC1, 255);
    } else {
      runtime_mask.setTo(0);
    }
  }

  auto sampleRefinedMask = [&](const cv::Point &point,
                               bool in_runtime_input) {
    if (!in_runtime_input)
      return false;
    return runtime_mask.at<unsigned char>(point.y, point.x) != 0;
  };

  const bool positive_prompt_hit_foreground =
      input.has_positive_point &&
      sampleRefinedMask(positive_runtime_point,
                        positive_prompt_in_runtime_input);
  const bool negative_prompt_hit_foreground =
      input.has_negative_point &&
      sampleRefinedMask(negative_runtime_point,
                        negative_prompt_in_runtime_input);

  const int refined_foreground = cv::countNonZero(runtime_mask);
  const double refined_foreground_ratio =
      runtime_mask.total() == 0
          ? 0.0
          : static_cast<double>(refined_foreground) /
                static_cast<double>(runtime_mask.total());

  std::cout << "[FindSegmentation] prompt coordinate map runtime_input_roi="
            << runtime_input_roi.x << "," << runtime_input_roi.y << ","
            << runtime_input_roi.width << "," << runtime_input_roi.height
            << " raw_mask=" << raw_mask.cols << "x" << raw_mask.rows
            << " scale=" << sx << "," << sy << " mask_roi=" << mask_roi.x
            << "," << mask_roi.y << "," << mask_roi.width << ","
            << mask_roi.height << " positive_in_runtime="
            << (positive_prompt_in_runtime_input ? "true" : "false")
            << " negative_in_runtime="
            << (negative_prompt_in_runtime_input ? "true" : "false")
            << " refinement_applied="
            << (refinement_applied ? "true" : "false") << "\n"
            << std::flush;
  std::cout << "[FindSegmentation] prompt refinement method="
               "deeplab_grabcut"
            << " raw_foreground_ratio=" << raw_foreground_ratio
            << " refined_foreground_ratio=" << refined_foreground_ratio
            << " positive_prompt_hit_foreground="
            << (positive_prompt_hit_foreground ? "true" : "false")
            << " negative_prompt_hit_foreground="
            << (negative_prompt_hit_foreground ? "true" : "false") << "\n"
            << std::flush;

  const bool prompt_quality_fail =
      !refinement_ok || refined_foreground == 0 ||
      refined_foreground_ratio >= 0.98 ||
      (input.has_positive_point &&
       (!positive_prompt_in_runtime_input ||
        !positive_prompt_hit_foreground)) ||
      (input.has_negative_point && negative_prompt_in_runtime_input &&
       negative_prompt_hit_foreground);
  if (prompt_quality_fail) {
    output.ok = false;
    output.backend_status = "prompt_quality_fail";
    output.status = "prompt_quality_fail";
    output.result_stage = "failed";
    output.refinement_method = "deeplab_grabcut_prompt_quality_rejected";
    output.reason =
        "libtorch segmentation rejected after DeepLab prompt refinement; "
        "raw_foreground_ratio=" +
        std::to_string(raw_foreground_ratio) +
        ", refined_foreground_ratio=" +
        std::to_string(refined_foreground_ratio) +
        ", refinement_applied=" +
        std::string(refinement_applied ? "true" : "false") +
        ", positive_prompt_hit_foreground=" +
        std::string(positive_prompt_hit_foreground ? "true" : "false") +
        ", negative_prompt_hit_foreground=" +
        std::string(negative_prompt_hit_foreground ? "true" : "false") +
        ", runtime_input_roi=" + std::to_string(runtime_input_roi.x) + "," +
        std::to_string(runtime_input_roi.y) + "," +
        std::to_string(runtime_input_roi.width) + "," +
        std::to_string(runtime_input_roi.height);
    output.contours.clear();
    output.contour_count = 0;
    output.primary_area = 0.0;
    std::cout << "[FindSegmentation] prompt quality fail: " << output.reason
              << "\n"
              << std::flush;
    return true;
  }

  cv::Mat constrained_mask = cv::Mat::zeros(input.image.size(), CV_8UC1);
  const cv::Rect runtime_local_roi(
      constrained_image_roi.x - runtime_input_roi.x,
      constrained_image_roi.y - runtime_input_roi.y,
      constrained_image_roi.width, constrained_image_roi.height);
  runtime_mask(runtime_local_roi).copyTo(
      constrained_mask(constrained_image_roi));

  std::vector<std::vector<cv::Point>> mask_contours;
  cv::Mat contour_mask = constrained_mask.clone();
  cv::findContours(contour_mask, mask_contours, cv::RETR_EXTERNAL,
                   cv::CHAIN_APPROX_SIMPLE);

  std::vector<FindSegmentationContour> mapped_contours;
  mapped_contours.reserve(mask_contours.size());
  double best_area = 0.0;
  for (const std::vector<cv::Point> &contour : mask_contours) {
    if (contour.size() < 3)
      continue;
    FindSegmentationContour mapped;
    mapped.points = contour;
    mapped.area = std::abs(cv::contourArea(mapped.points));
    mapped.perimeter = cv::arcLength(mapped.points, true);
    if (mapped.area < 1.0)
      continue;
    best_area = (std::max)(best_area, mapped.area);
    mapped_contours.push_back(std::move(mapped));
  }

  if (mapped_contours.empty())
    return false;

  cv::Mat overlay = input.image.clone();
  for (const FindSegmentationContour &contour : mapped_contours) {
    std::vector<std::vector<cv::Point>> one{contour.points};
    cv::drawContours(overlay, one, 0, cv::Scalar(0, 255, 255), 2);
  }

  const std::filesystem::path constrained_mask_path =
      result_dir / "mask_binary_prompt_roi.png";
  const std::filesystem::path constrained_overlay_path =
      result_dir / "mask_overlay_prompt_roi.png";
  const std::filesystem::path constrained_contour_path =
      result_dir / "contours_prompt_roi.json";
  cv::imwrite(constrained_mask_path.string(), constrained_mask);
  cv::imwrite(constrained_overlay_path.string(), overlay);

  std::ofstream json(constrained_contour_path);
  if (json) {
    const char quote = static_cast<char>(34);
    auto writeKey = [&](const char *key) {
      json << quote << key << quote << ':';
    };

    json << '{';
    writeKey("contour_count");
    json << mapped_contours.size() << ',';

    writeKey("prompt_roi");
    json << '{';
    writeKey("x");
    json << image_roi.x << ',';
    writeKey("y");
    json << image_roi.y << ',';
    writeKey("width");
    json << image_roi.width << ',';
    writeKey("height");
    json << image_roi.height << "},";

    writeKey("runtime_input_roi");
    json << '{';
    writeKey("x");
    json << runtime_input_roi.x << ',';
    writeKey("y");
    json << runtime_input_roi.y << ',';
    writeKey("width");
    json << runtime_input_roi.width << ',';
    writeKey("height");
    json << runtime_input_roi.height << "},";

    writeKey("source");
    json << quote << "prompt_roi_cropped_libtorch_mask" << quote << ',';
    writeKey("contours");
    json << '[';
    for (std::size_t i = 0; i < mapped_contours.size(); ++i) {
      if (i > 0)
        json << ',';
      const FindSegmentationContour &contour = mapped_contours[i];
      json << '{';
      writeKey("area");
      json << contour.area << ',';
      writeKey("point_count");
      json << contour.points.size() << ',';
      writeKey("points");
      json << '[';
      for (std::size_t j = 0; j < contour.points.size(); ++j) {
        if (j > 0)
          json << ',';
        json << '[' << contour.points[j].x << ',' << contour.points[j].y
             << ']';
      }
      json << "]}";
    }
    json << "]}";
  }

  output.mask = constrained_mask;
  output.overlay = overlay;
  output.contours = std::move(mapped_contours);
  output.contour_count = static_cast<int>(output.contours.size());
  output.primary_area = best_area;
  output.mask_width = constrained_mask.cols;
  output.mask_height = constrained_mask.rows;
  output.mask_ref = constrained_mask_path.string();
  output.overlay_ref = constrained_overlay_path.string();
  output.contour_ref = constrained_contour_path.string();
  output.refined_result_ref = output.result_ref;
  output.refined_mask_ref = output.mask_ref;
  output.refined_contour_ref = output.contour_ref;
  output.refined_overlay_ref = output.overlay_ref;
  output.refined_result_available = true;
  output.result_stage = "refined";
  output.refinement_method = refinement_applied ? "deeplab_grabcut_prompt_roi" : "prompt_roi_constraint";

  std::cout << "[FindSegmentation] prompt roi constraint applied: roi="
            << image_roi.x << "," << image_roi.y << "," << image_roi.width
            << "," << image_roi.height << " runtime_input="
            << runtime_input_roi.width << "x" << runtime_input_roi.height
            << " output_mask=" << constrained_mask.cols << "x"
            << constrained_mask.rows << " contours=" << output.contour_count
            << " contour_ref=" << output.contour_ref << "\n"
            << std::flush;
  return true;
}

bool ApplyPromptRectFallbackContourToTorchSegmentationResult(
    const FindSegmentationInput &input, const std::filesystem::path &result_dir,
    FindSegmentationResult &output) {
  if (!input.has_rect || input.image.empty())
    return false;

  const cv::Rect image_bounds(0, 0, input.image.cols, input.image.rows);
  const cv::Rect image_roi = input.rect & image_bounds;
  if (image_roi.width <= 0 || image_roi.height <= 0)
    return false;

  FindSegmentationContour contour;
  contour.points.emplace_back(image_roi.x, image_roi.y);
  contour.points.emplace_back(image_roi.x, image_roi.y + image_roi.height);
  contour.points.emplace_back(image_roi.x + image_roi.width,
                              image_roi.y + image_roi.height);
  contour.points.emplace_back(image_roi.x + image_roi.width, image_roi.y);
  contour.area = std::abs(cv::contourArea(contour.points));
  contour.perimeter = cv::arcLength(contour.points, true);

  cv::Mat mask = cv::Mat::zeros(input.image.size(), CV_8UC1);
  cv::rectangle(mask, image_roi, cv::Scalar(255), cv::FILLED);

  cv::Mat overlay = input.image.clone();
  std::vector<std::vector<cv::Point>> draw_contours;
  draw_contours.push_back(contour.points);
  cv::drawContours(overlay, draw_contours, 0, cv::Scalar(0, 255, 255), 2);

  const std::filesystem::path fallback_mask_path =
      result_dir / "mask_binary_prompt_roi_fallback.png";
  const std::filesystem::path fallback_overlay_path =
      result_dir / "mask_overlay_prompt_roi_fallback.png";
  const std::filesystem::path fallback_contour_path =
      result_dir / "contours_prompt_roi_fallback.json";

  cv::imwrite(fallback_mask_path.string(), mask);
  cv::imwrite(fallback_overlay_path.string(), overlay);

  std::ofstream json(fallback_contour_path);
  if (json) {
    json << "{\"contour_count\":1"
         << ",\"prompt_roi\":{\"x\":" << image_roi.x << ",\"y\":" << image_roi.y
         << ",\"width\":" << image_roi.width
         << ",\"height\":" << image_roi.height
         << "},\"source\":\"prompt_roi_fallback_for_libtorch_smoke\""
         << ",\"contours\":[{\"area\":" << contour.area
         << ",\"point_count\":" << contour.points.size() << ",\"points\":[";
    for (std::size_t i = 0; i < contour.points.size(); ++i) {
      if (i > 0)
        json << ",";
      json << "[" << contour.points[i].x << "," << contour.points[i].y << "]";
    }
    json << "]}]}";
  }

  output.mask = mask;
  output.overlay = overlay;
  output.contours.clear();
  output.contours.push_back(std::move(contour));
  output.contour_count = 1;
  output.primary_area = output.contours.front().area;
  output.mask_width = mask.cols;
  output.mask_height = mask.rows;
  output.mask_ref = fallback_mask_path.string();
  output.overlay_ref = fallback_overlay_path.string();
  output.contour_ref = fallback_contour_path.string();
  output.fallback_used = true;

  output.result_stage = "fallback";

  output.refinement_method = "prompt_roi_fallback_for_libtorch_smoke";

  output.backend_status = "prompt_roi_fallback";

  output.status = "prompt_roi_fallback";

  output.reason = "prompt ROI fallback contour generated; not a valid raw model result";


  std::cout << "[FindSegmentation] prompt roi fallback applied: roi="
            << image_roi.x << "," << image_roi.y << "," << image_roi.width
            << "," << image_roi.height << " contour_ref=" << output.contour_ref
            << "\n"
            << std::flush;
  return true;
}
} // namespace

bool FindSegmentationEdgeSamBackend::Run(const FindSegmentationInput &input,
                                         FindSegmentationResult &output,
                                         std::string &reason) {
  output.backend = input.backend.empty() ? "edgesam" : input.backend;
  output.task_id = input.task_id;
  output.model_id = input.model_id;
  output.model_package_ref = input.model_package_ref;
  output.manifest_path = input.manifest_path;
  output.output_root = input.output_root;
  output.postprocess_profile = input.postprocess_profile;
  output.parameter_profile_ref = input.parameter_profile_ref;


  if (input.task_id.empty() || input.model_id.empty() ||
      input.model_package_ref.empty() || input.manifest_path.empty()) {
    output.ok = false;
    output.backend_status = "ASSET_MISSING";
    output.status = "ASSET_MISSING";
    output.reason =
        "libtorch segmentation requires explicit task_id, model_id, "
        "model_package_ref, and manifest_path";
    reason = output.reason;
    return false;
  }
  if (input.output_root.empty()) {
    output.ok = false;
    output.backend_status = "OUTPUT_ROOT_REQUIRED";
    output.status = "OUTPUT_ROOT_REQUIRED";
    output.reason =
        "libtorch segmentation requires an explicit case-local output_root";
    reason = output.reason;
    return false;
  }

  const std::filesystem::path manifest_path =
      ResolveFindSegmentationAssetPath(input.manifest_path);
  const std::filesystem::path model_package_path =
      ResolveFindSegmentationAssetPath(input.model_package_ref);
  std::error_code asset_ec;
  const bool manifest_available =
      std::filesystem::is_regular_file(manifest_path, asset_ec) && !asset_ec;
  asset_ec.clear();
  const bool model_package_available =
      std::filesystem::exists(model_package_path, asset_ec) && !asset_ec;
  if (!manifest_available || !model_package_available) {
    output.ok = false;
    output.backend_status = "ASSET_MISSING";
    output.status = "ASSET_MISSING";
    output.reason = !manifest_available
        ? "segmentation manifest asset is missing: " + input.manifest_path
        : "segmentation model package asset is missing: " +
              input.model_package_ref;
    reason = output.reason;
    return false;
  }
  output.manifest_path = manifest_path.string();
  output.model_package_ref = model_package_path.string();

  const std::string runtime_dll = FindSegmentationTorchRuntimeDllPath();

  TorchRuntimeBridge bridge;
  if (!bridge.Load(runtime_dll)) {
    output.ok = false;
    output.backend_status = "runtime_load_failed";
    output.status = "runtime_load_failed";
    output.reason = "failed to load torch runtime dll: " + runtime_dll;
    if (!bridge.LastErrorMessage().empty())
      output.reason += "; " + bridge.LastErrorMessage();
    reason = output.reason;
    return false;
  }

  const std::filesystem::path output_dir =
      std::filesystem::path(input.output_root) / "segmentation_runtime";
  const std::filesystem::path input_image_path =
      output_dir / "find_segmentation_libtorch_input.png";

  std::error_code ec;
  std::filesystem::create_directories(output_dir, ec);
  if (ec) {
    output.ok = false;
    output.backend_status = "output_dir_failed";
    output.status = "output_dir_failed";
    output.reason = "failed to create output dir: " + output_dir.string();
    reason = output.reason;
    return false;
  }
  output.output_root = output_dir.string();

  const cv::Rect image_bounds(0, 0, input.image.cols, input.image.rows);
  cv::Rect runtime_input_roi = image_bounds;
  if (input.has_rect) {
    runtime_input_roi = input.rect & image_bounds;
    if (runtime_input_roi.width <= 0 || runtime_input_roi.height <= 0) {
      output.ok = false;
      output.backend_status = "prompt_input_invalid";
      output.status = "prompt_input_invalid";
      output.reason = "prompt ROI does not intersect the input image";
      reason = output.reason;
      return false;
    }
  }

  if (input.has_positive_point &&
      !runtime_input_roi.contains(input.positive_point)) {
    output.ok = false;
    output.backend_status = "prompt_input_invalid";
    output.status = "prompt_input_invalid";
    output.reason = "positive prompt point is outside the effective prompt ROI";
    reason = output.reason;
    return false;
  }

  const bool negative_in_runtime_input =
      input.has_negative_point &&
      runtime_input_roi.contains(input.negative_point);
  cv::Mat runtime_input_image = input.image(runtime_input_roi).clone();
  std::cout << "[FindSegmentation] inference input roi="
            << runtime_input_roi.x << "," << runtime_input_roi.y << ","
            << runtime_input_roi.width << "," << runtime_input_roi.height
            << " source_image=" << input.image.cols << "x" << input.image.rows
            << " runtime_image=" << runtime_input_image.cols << "x"
            << runtime_input_image.rows << " positive="
            << input.positive_point.x << "," << input.positive_point.y
            << " negative=" << input.negative_point.x << ","
            << input.negative_point.y << " negative_in_runtime="
            << (negative_in_runtime_input ? "true" : "false") << "\n"
            << std::flush;

  if (!cv::imwrite(input_image_path.string(), runtime_input_image)) {
    output.ok = false;
    output.backend_status = "input_image_write_failed";
    output.status = "input_image_write_failed";
    output.reason = "failed to write libtorch backend input image: " +
                    input_image_path.string();
    reason = output.reason;
    return false;
  }
  TorchRuntimeGuiConfig config;
  config.device = input.device.empty() ? "cpu" : input.device;
  config.model_root = model_package_path.string();

  config.output_root = output_dir.string();
  config.log_level = "info";

  if (!bridge.Create(config)) {
    output.ok = false;
    output.backend_status = "runtime_create_failed";
    output.status = "runtime_create_failed";
    output.reason = "failed to create torch runtime";
    reason = output.reason;
    return false;
  }

  std::ostringstream extra;
  const char json_quote = static_cast<char>(34);
  auto writeJsonKey = [&](const char *key) {
    extra << json_quote << key << json_quote << ':';
  };
  extra << '{';
  writeJsonKey("backend");
  extra << json_quote << output.backend << json_quote;
  extra << ',';
  writeJsonKey("threshold");
  extra << input.threshold;
  extra << ',';
  writeJsonKey("mode");
  extra << input.mode;
  extra << ',';
  writeJsonKey("coordinate_space");
  extra << json_quote << "runtime_input_roi" << json_quote;
  extra << ',';
  writeJsonKey("has_rect");
  extra << (input.has_rect ? "true" : "false");

  extra << ',';
  writeJsonKey("roi");
  extra << '{';
  writeJsonKey("x");
  extra << 0 << ',';
  writeJsonKey("y");
  extra << 0 << ',';
  writeJsonKey("width");
  extra << runtime_input_roi.width << ',';
  writeJsonKey("height");
  extra << runtime_input_roi.height << '}';

  extra << ',';
  writeJsonKey("source_roi");
  extra << '{';
  writeJsonKey("x");
  extra << runtime_input_roi.x << ',';
  writeJsonKey("y");
  extra << runtime_input_roi.y << ',';
  writeJsonKey("width");
  extra << runtime_input_roi.width << ',';
  writeJsonKey("height");
  extra << runtime_input_roi.height << '}';

  extra << ',';
  writeJsonKey("positive_prompt");
  extra << '{';
  writeJsonKey("enabled");
  extra << (input.has_positive_point ? "true" : "false") << ',';
  writeJsonKey("x");
  extra << input.positive_point.x - runtime_input_roi.x << ',';
  writeJsonKey("y");
  extra << input.positive_point.y - runtime_input_roi.y << ',';
  writeJsonKey("source_x");
  extra << input.positive_point.x << ',';
  writeJsonKey("source_y");
  extra << input.positive_point.y << '}';

  extra << ',';
  writeJsonKey("negative_prompt");
  extra << '{';
  writeJsonKey("enabled");
  extra << (negative_in_runtime_input ? "true" : "false") << ',';
  writeJsonKey("x");
  extra << input.negative_point.x - runtime_input_roi.x << ',';
  writeJsonKey("y");
  extra << input.negative_point.y - runtime_input_roi.y << ',';
  writeJsonKey("source_x");
  extra << input.negative_point.x << ',';
  writeJsonKey("source_y");
  extra << input.negative_point.y << '}';
  extra << '}';
  TorchRuntimeGuiRequest request;
  request.task = input.task_id;

  request.case_name = input.model_id;
  request.input_image = input_image_path.string();
  request.manifest_path = manifest_path.string();
  request.output_dir = output_dir.string();
  request.extra_json = extra.str();

  TorchRuntimeGuiResult torch_result = bridge.RunTask(request);
  bridge.Destroy();

  const bool is_instance_segmentation_task =
      input.task_id.find("instance_segmentation") != std::string::npos ||
      input.task_id.find("yolov8") != std::string::npos;

  output.ok = torch_result.ok;
  output.backend_status =
      torch_result.ok
          ? (is_instance_segmentation_task ? "instance_segmentation_ready"
                                           : "libtorch_segmentation_ready")
          : "libtorch_segmentation_failed";
  output.status = output.backend_status;
  output.reason = torch_result.ok ? "libtorch segmentation task executed"
                                  : (torch_result.error_message.empty()
                                         ? "torch runtime task failed"
                                         : torch_result.error_message);

  output.result_ref = torch_result.result_ref;
  const std::filesystem::path result_dir =
      torch_result.result_ref.empty()
          ? output_dir
          : std::filesystem::path(torch_result.result_ref).parent_path();

  const std::filesystem::path binary_mask_path = result_dir / "mask_binary.png";
  const std::filesystem::path label_mask_path = result_dir / "mask_labels.png";
  const std::filesystem::path contour_path = result_dir / "contours.json";
  const std::filesystem::path overlay_path = result_dir / "mask_overlay.png";

  if (std::filesystem::exists(binary_mask_path))
    output.mask_ref = binary_mask_path.string();
  else if (std::filesystem::exists(label_mask_path))
    output.mask_ref = label_mask_path.string();
  else
    output.mask_ref = torch_result.attach_back_ref.empty()
                          ? torch_result.result_ref
                          : torch_result.attach_back_ref;
  output.contour_ref = std::filesystem::exists(contour_path)
                           ? contour_path.string()
                           : torch_result.evidence_ref;
  output.overlay_ref = std::filesystem::exists(overlay_path)
                           ? overlay_path.string()
                           : (torch_result.primary_visual_ref.empty()
                                  ? torch_result.evidence_ref
                                  : torch_result.primary_visual_ref);

  output.raw_result_ref = output.result_ref;
  output.raw_mask_ref = output.mask_ref;
  output.raw_contour_ref = output.contour_ref;
  output.raw_overlay_ref = output.overlay_ref;
  output.raw_result_available = !output.raw_result_ref.empty() ||
                                !output.raw_mask_ref.empty() ||
                                !output.raw_contour_ref.empty();
  output.result_stage = output.ok ? "raw" : "failed";

  if (!output.contour_ref.empty())
    ParseFindSegmentationContours(output.contour_ref, output);
  for (FindSegmentationContour &contour : output.contours) {
    contour.source_mask_ref = output.mask_ref;
    contour.source_contour_ref = output.contour_ref;
  }

  if (output.ok && input.has_rect && !is_instance_segmentation_task) {
    const bool constrained = ApplyPromptRectConstraintToTorchSegmentationResult(
        input, result_dir, runtime_input_roi, output);
    if (!constrained) {
      if (input.has_positive_point || input.has_negative_point) {
        output.ok = false;
        output.backend_status = "prompt_quality_fail";
        output.status = "prompt_quality_fail";
        output.result_stage = "failed";
        output.refinement_method = "prompt_quality_rejected";
        output.reason =
            "libtorch segmentation rejected: prompted run did not produce a "
            "valid prompt-constrained contour";
        output.contours.clear();
        output.contour_count = 0;
        output.primary_area = 0.0;
      } else {
        ApplyPromptRectFallbackContourToTorchSegmentationResult(
            input, result_dir, output);
      }
    }
  }

  reason = output.reason;
  return output.ok;
}
