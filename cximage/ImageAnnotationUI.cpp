#include "ViewController.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

#include <opencv2/imgproc.hpp>

#include "CircleShape.h"
#include "CxAnnotationToolRuntime.h"
#include "CxUnifiedLog.h"
#include "EllipseShape.h"
#include "FindCircle.h"
#include "FindEllipse.h"
#include "FindLine.h"
#include "FindRect.h"
#include "LineGaugeShape.h"
#include "ManualConsoleCxScriptDebug.h"
#include "ManualConsoleGauge.h"
#include "ManualConsoleUtils.h"
#include "PolylineShape.h"
#include "RectShape.h"
#include "shapebase.h"

namespace {
namespace fs = std::filesystem;

int AnnotationStringResize(ImGuiInputTextCallbackData *data) {
  if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
    std::string *value = static_cast<std::string *>(data->UserData);
    value->resize(static_cast<std::size_t>(data->BufTextLen));
    data->Buf = value->data();
  }
  return 0;
}

bool AnnotationInputText(const char *label, std::string &value) {
  if (value.capacity() < 256)
    value.reserve(256);
  return ImGui::InputText(label, value.data(), value.capacity() + 1,
                          ImGuiInputTextFlags_CallbackResize,
                          AnnotationStringResize, &value);
}

std::string BusinessReceiptJsonEscape(const std::string &value) {
  std::string escaped;
  escaped.reserve(value.size() + 8);
  for (const char ch : value) {
    switch (ch) {
    case '\\': escaped += "\\\\"; break;
    case '"': escaped += "\\\""; break;
    case '\n': escaped += "\\n"; break;
    case '\r': escaped += "\\r"; break;
    case '\t': escaped += "\\t"; break;
    default: escaped += ch; break;
    }
  }
  return escaped;
}

const char *BusinessReceiptShapeKind(CxShapeKind kind) {
  switch (kind) {
  case CxShapeKind::Points: return "point";
  case CxShapeKind::Line: return "line";
  case CxShapeKind::Rect: return "rectangle";
  case CxShapeKind::Circle: return "circle";
  case CxShapeKind::Ellipse: return "ellipse";
  case CxShapeKind::Polyline: return "polyline";
  case CxShapeKind::LineGauge: return "line_gauge";
  }
  return "unknown";
}

void SyncSegmentationLegacyPointFromLists(ManualGaugeState &gauge) {
  gauge.has_segmentation_positive_point =
      !gauge.segmentation_positive_points.empty();
  if (gauge.has_segmentation_positive_point) {
    const auto &point = gauge.segmentation_positive_points.back();
    gauge.segmentation_positive_x = point.x;
    gauge.segmentation_positive_y = point.y;
  }

  gauge.has_segmentation_negative_point =
      !gauge.segmentation_negative_points.empty();
  if (gauge.has_segmentation_negative_point) {
    const auto &point = gauge.segmentation_negative_points.back();
    gauge.segmentation_negative_x = point.x;
    gauge.segmentation_negative_y = point.y;
  }
}

void UpsertSegmentationPromptListPoint(
    std::vector<ManualSegmentationPromptPoint> &points, const std::string &ref,
    int x, int y) {
  auto it =
      std::find_if(points.begin(), points.end(),
                   [&ref, x, y](const ManualSegmentationPromptPoint &point) {
                     if (!ref.empty() && point.ref == ref)
                       return true;
                     return point.x == x && point.y == y;
                   });
  if (it == points.end()) {
    ManualSegmentationPromptPoint point;
    point.ref = ref;
    point.x = x;
    point.y = y;
    points.push_back(point);
  } else {
    if (it->ref.empty())
      it->ref = ref;
    it->x = x;
    it->y = y;
  }
}

void NormalizeSegmentationPromptList(
    std::vector<ManualSegmentationPromptPoint> &points) {
  std::vector<ManualSegmentationPromptPoint> normalized;
  for (const ManualSegmentationPromptPoint &point : points) {
    UpsertSegmentationPromptListPoint(normalized, point.ref, point.x, point.y);
  }
  points = std::move(normalized);
}

OverlayImagePoint ElementCenter(const OverlayElement &element) {
  OverlayImagePoint center;
  if (element.image_points.empty())
    return center;
  for (const OverlayImagePoint &point : element.image_points) {
    center.x += point.x;
    center.y += point.y;
  }
  center.x /= static_cast<float>(element.image_points.size());
  center.y /= static_cast<float>(element.image_points.size());
  return center;
}

std::string Coordinate(float value) {
  return std::to_string(static_cast<int>(std::lround(value)));
}

std::string GenerateElementStatement(const OverlayElement &element) {
  if (element.image_points.empty())
    return std::string();
  const OverlayImagePoint &first = element.image_points[0];
  if (element.kind == OverlayKind::Point)
    return "picked_point0.set(" + Coordinate(first.x) + ", " +
           Coordinate(first.y) + ");";
  if (element.kind == OverlayKind::Circle)
    return "afindcircle0.setcircle(" + Coordinate(first.x) + ", " +
           Coordinate(first.y) + ", 0, " + Coordinate(element.radius) + ");";
  if (element.kind == OverlayKind::Polyline) {
    std::ostringstream statements;
    for (const OverlayImagePoint &point : element.image_points)
      statements << "polyline0.addpoint(" << Coordinate(point.x) << ", "
                 << Coordinate(point.y) << ");\n";
    return statements.str();
  }
  if (element.image_points.size() < 2)
    return std::string();
  const OverlayImagePoint &second = element.image_points[1];
  if (element.kind == OverlayKind::Line)
    return "afindline0.setline(" + Coordinate(first.x) + ", " +
           Coordinate(first.y) + ", " + Coordinate(second.x) + ", " +
           Coordinate(second.y) + ");";
  if (element.kind == OverlayKind::Rect)
    return "amatch0.setmatchrect(" + Coordinate(std::min(first.x, second.x)) +
           ", " + Coordinate(std::min(first.y, second.y)) + ", " +
           Coordinate(std::fabs(second.x - first.x)) + ", " +
           Coordinate(std::fabs(second.y - first.y)) + ");";
  return std::string();
}

bool ParseNumericParameters(const std::string &text,
                            std::vector<float> &values) {
  std::istringstream input(text);
  std::string token;
  while (std::getline(input, token, ',')) {
    try {
      values.push_back(std::stof(token));
    } catch (...) {
      values.clear();
      return false;
    }
  }
  return !values.empty();
}

void UpdateManualGaugeFromElement(ManualTestContext &context,
                                  const OverlayElement &element) {
  ManualGaugeState &gauge = context.current_gauge;
  if (element.kind == OverlayKind::Line && element.image_points.size() >= 2) {
    gauge.tool = "FindLine";
    gauge.has_line_gauge = true;
    gauge.line_x0 = static_cast<int>(std::lround(element.image_points[0].x));
    gauge.line_y0 = static_cast<int>(std::lround(element.image_points[0].y));
    gauge.line_x1 = static_cast<int>(std::lround(element.image_points[1].x));
    gauge.line_y1 = static_cast<int>(std::lround(element.image_points[1].y));
    gauge.source = "annotation_handle";
    gauge.review_status = "editing";
    gauge.accepted = false;
    gauge.dirty = true;
  } else if (element.kind == OverlayKind::Circle &&
             !element.image_points.empty()) {
    gauge.tool = "FindCircle";
    gauge.has_circle_gauge = true;
    gauge.circle_cx = static_cast<int>(std::lround(element.image_points[0].x));
    gauge.circle_cy = static_cast<int>(std::lround(element.image_points[0].y));
    gauge.circle_px = static_cast<int>(
        std::lround(element.image_points[0].x + element.radius));
    gauge.circle_py = static_cast<int>(std::lround(element.image_points[0].y));
    gauge.source = "annotation_handle";
    gauge.review_status = "editing";
    gauge.accepted = false;
    gauge.dirty = true;
  }
}

void UpdateManualGaugeFromShapeElement(ManualTestContext &context,
                                       const CxShapeElement &element) {
  if (!element.shape || !element.editable)
    return;

  ManualGaugeState &gauge = context.current_gauge;
  if (element.owner_type == "FindLine" &&
      element.shape->kind() == CxShapeKind::LineGauge) {
    const auto *line =
        dynamic_cast<const LineGaugeShape *>(element.shape.get());
    if (line == nullptr)
      return;

    gauge.tool = "FindLine";
    gauge.has_line_gauge = true;
    gauge.has_circle_gauge = false;
    gauge.has_ellipse_gauge = false;
    gauge.line_x0 = static_cast<int>(std::lround(line->x0()));
    gauge.line_y0 = static_cast<int>(std::lround(line->y0()));
    gauge.line_x1 = static_cast<int>(std::lround(line->x1()));
    gauge.line_y1 = static_cast<int>(std::lround(line->y1()));
    gauge.tool_half_width =
        std::max(1, static_cast<int>(std::lround(line->halfWidth())));
  } else if (element.owner_type == "FindCircle" &&
             element.shape->kind() == CxShapeKind::Circle) {
    CxShapePoint center;
    double radius = 0.0;
    double inner_radius = 0.0;
    if (!element.shape->exportCircle(center, radius, inner_radius))
      return;

    gauge.tool = "FindCircle";
    gauge.has_circle_gauge = true;
    gauge.has_line_gauge = false;
    gauge.has_ellipse_gauge = false;
    gauge.circle_cx = static_cast<int>(std::lround(center.x));
    gauge.circle_cy = static_cast<int>(std::lround(center.y));
    gauge.radius = std::max(1, static_cast<int>(std::lround(radius)));
    gauge.circle_px = gauge.circle_cx + gauge.radius;
    gauge.circle_py = gauge.circle_cy;
    gauge.inner_radius =
        std::max(0, static_cast<int>(std::lround(inner_radius)));
    gauge.outer_radius = gauge.radius;
    if (gauge.inner_radius >= gauge.outer_radius)
      gauge.inner_radius = std::max(0, gauge.outer_radius - 1);
    const auto *circle = dynamic_cast<const CircleShape *>(element.shape.get());
    if (circle != nullptr) {
      gauge.circle_arc_enabled = circle->hasScanSector() ? 1 : 0;
      gauge.circle_arc_start_deg =
          static_cast<int>(std::lround(circle->scanSectorStartDegrees()));
      gauge.circle_arc_end_deg =
          static_cast<int>(std::lround(circle->scanSectorEndDegrees()));
    }
  } else if (element.owner_type == "FindEllipse" &&
             element.shape->kind() == CxShapeKind::Ellipse) {
    if (!element.editable || element.semantic_role != "roi")
      return;

    CxShapeGeometrySnapshot geometry;
    if (!element.shape->snapshot(geometry))
      return;
    if (geometry.radius_x <= 0.0 || geometry.radius_y <= 0.0)
      return;

    gauge.tool = "FindEllipse";
    gauge.has_ellipse_gauge = true;
    gauge.has_line_gauge = false;
    gauge.has_circle_gauge = false;
    gauge.ellipse_x0 =
        static_cast<int>(std::lround(geometry.center.x - geometry.radius_x));
    gauge.ellipse_y0 =
        static_cast<int>(std::lround(geometry.center.y - geometry.radius_y));
    gauge.ellipse_x1 =
        static_cast<int>(std::lround(geometry.center.x + geometry.radius_x));
    gauge.ellipse_y1 =
        static_cast<int>(std::lround(geometry.center.y + geometry.radius_y));
    const auto *ellipse =
        dynamic_cast<const EllipseShape *>(element.shape.get());
    if (ellipse != nullptr) {
      gauge.ellipse_inner_scale_percent =
          std::max(0, std::min(99, static_cast<int>(std::lround(
                                       ellipse->innerScalePercent()))));
    }
  } else if (element.owner_type == "FindSegmentation" &&
             element.shape->kind() == CxShapeKind::Rect) {
    const auto *rect = dynamic_cast<const RectShape *>(element.shape.get());
    if (rect == nullptr)
      return;

    gauge.tool = "FindSegmentation";
    gauge.has_segmentation_prompt_rect = true;
    gauge.has_line_gauge = false;
    gauge.has_circle_gauge = false;
    gauge.has_ellipse_gauge = false;
    gauge.segmentation_prompt_x0 =
        static_cast<int>(std::lround(std::min(rect->x0(), rect->x1())));
    gauge.segmentation_prompt_y0 =
        static_cast<int>(std::lround(std::min(rect->y0(), rect->y1())));
    gauge.segmentation_prompt_x1 =
        static_cast<int>(std::lround(std::max(rect->x0(), rect->x1())));
    gauge.segmentation_prompt_y1 =
        static_cast<int>(std::lround(std::max(rect->y0(), rect->y1())));
  } else if (element.owner_type == "FindObject" &&
             element.shape->kind() == CxShapeKind::Rect) {
    const auto *rect = dynamic_cast<const RectShape *>(element.shape.get());
    if (rect == nullptr)
      return;
    gauge.tool = "FindObject";
    gauge.has_findobject_roi = true;
    gauge.has_line_gauge = false;
    gauge.has_circle_gauge = false;
    gauge.has_ellipse_gauge = false;
    gauge.findobject_x0 =
        static_cast<int>(std::lround(std::min(rect->x0(), rect->x1())));
    gauge.findobject_y0 =
        static_cast<int>(std::lround(std::min(rect->y0(), rect->y1())));
    gauge.findobject_x1 =
        static_cast<int>(std::lround(std::max(rect->x0(), rect->x1())));
    gauge.findobject_y1 =
        static_cast<int>(std::lround(std::max(rect->y0(), rect->y1())));
  } else if (element.owner_type == "FindSegmentation" &&
             element.shape->kind() == CxShapeKind::Points) {
    CxShapeGeometrySnapshot geometry;
    if (!element.shape->snapshot(geometry) || geometry.points.empty())
      return;
    const int x = static_cast<int>(std::lround(geometry.points.front().x));
    const int y = static_cast<int>(std::lround(geometry.points.front().y));
    gauge.tool = "FindSegmentation";
    if (element.owner_binding == "setpositivepointxy" ||
        element.owner_binding == "setpositivepoint") {
      gauge.segmentation_prompt_pick_mode = 1;
      UpsertSegmentationPromptListPoint(gauge.segmentation_positive_points,
                                        element.stable_ref, x, y);
    } else if (element.owner_binding == "setnegativepointxy" ||
               element.owner_binding == "setnegativepoint") {
      gauge.segmentation_prompt_pick_mode = 2;
      UpsertSegmentationPromptListPoint(gauge.segmentation_negative_points,
                                        element.stable_ref, x, y);
    } else
      return;
    SyncSegmentationLegacyPointFromLists(gauge);
  } else {
    return;
  }

  gauge.source = "shape_drag";
  gauge.review_status = "editing";
  gauge.accepted = false;
  gauge.dirty = true;
}

bool ExportShapeElementToRuntimeGlobals(ManualTestContext &context,
                                        const CxShapeElement &element,
                                        std::string &reason) {
  if (!element.shape) {
    reason = "selected shape has no geometry";
    return false;
  }

  auto setInt = [&context](const std::string &name, double value) {
    context.runtime_int_vars[name] = static_cast<int>(std::lround(value));
  };

  if (element.owner_type == "FastMatch" &&
      (element.owner_binding == "learn_roi" ||
       element.owner_binding == "search_roi") &&
      element.shape->kind() == CxShapeKind::Rect) {
    const RectShape *rect =
        dynamic_cast<const RectShape *>(element.shape.get());
    if (rect == nullptr) {
      reason = "FastMatch ROI is not a rectangle";
      return false;
    }

    const double x0 = std::min(rect->x0(), rect->x1());
    const double y0 = std::min(rect->y0(), rect->y1());
    const double x1 = std::max(rect->x0(), rect->x1());
    const double y1 = std::max(rect->y0(), rect->y1());
    const double w = std::max(1.0, x1 - x0);
    const double h = std::max(1.0, y1 - y0);

    if (element.owner_binding == "learn_roi") {
      setInt("global_learn_roi_x", x0);
      setInt("global_learn_roi_y", y0);
      setInt("global_learn_roi_w", w);
      setInt("global_learn_roi_h", h);
      reason = "FastMatch learn ROI exported to global_learn_roi_*";
    } else {
      setInt("global_search_roi_x", x0);
      setInt("global_search_roi_y", y0);
      setInt("global_search_roi_w", w);
      setInt("global_search_roi_h", h);
      reason = "FastMatch search ROI exported to global_search_roi_*";
    }

    context.current_gauge.tool = "FastMatch";
    context.current_gauge.source = "shape_drag";
    context.current_gauge.review_status = "editing";
    context.current_gauge.accepted = false;
    context.current_gauge.dirty = true;
    return true;
  }

  if (element.owner_type == "GridPatternClassTool" &&
      element.owner_binding == "analysis_roi" &&
      element.shape->kind() == CxShapeKind::Rect) {
    const RectShape *rect =
        dynamic_cast<const RectShape *>(element.shape.get());
    if (rect == nullptr) {
      reason = "GridPattern analysis ROI is not a rectangle";
      return false;
    }
    const double x0 = std::min(rect->x0(), rect->x1());
    const double y0 = std::min(rect->y0(), rect->y1());
    const double x1 = std::max(rect->x0(), rect->x1());
    const double y1 = std::max(rect->y0(), rect->y1());
    setInt("global_learn_roi_x", x0);
    setInt("global_learn_roi_y", y0);
    setInt("global_learn_roi_w", std::max(1.0, x1 - x0));
    setInt("global_learn_roi_h", std::max(1.0, y1 - y0));
    reason = "GridPattern analysis ROI exported to global_learn_roi_*";
    return true;
  }

  if (element.owner_type == "RegionPatternTool" &&
      element.owner_binding == "region_analysis_roi" &&
      element.shape->kind() == CxShapeKind::Rect) {
    const RectShape *rect =
        dynamic_cast<const RectShape *>(element.shape.get());
    if (rect == nullptr) {
      reason = "RegionPattern analysis ROI is not a rectangle";
      return false;
    }
    const double x0 = std::min(rect->x0(), rect->x1());
    const double y0 = std::min(rect->y0(), rect->y1());
    const double x1 = std::max(rect->x0(), rect->x1());
    const double y1 = std::max(rect->y0(), rect->y1());
    setInt("global_region_roi_x", x0);
    setInt("global_region_roi_y", y0);
    setInt("global_region_roi_w", std::max(1.0, x1 - x0));
    setInt("global_region_roi_h", std::max(1.0, y1 - y0));
    reason = "RegionPattern analysis ROI exported to global_region_roi_*";
    return true;
  }

  if (element.owner_type == "FindSegmentation" &&
      (element.owner_binding == "setpromptrectxyxy" ||
       element.owner_binding == "setpromptrect" ||
       element.owner_binding == "prompt_rect") &&
      element.shape->kind() == CxShapeKind::Rect) {
    const RectShape *rect =
        dynamic_cast<const RectShape *>(element.shape.get());
    if (rect == nullptr) {
      reason = "FindSegmentation prompt ROI is not a rectangle";
      return false;
    }
    const double x0 = std::min(rect->x0(), rect->x1());
    const double y0 = std::min(rect->y0(), rect->y1());
    const double x1 = std::max(rect->x0(), rect->x1());
    const double y1 = std::max(rect->y0(), rect->y1());
    if (x1 <= x0 || y1 <= y0) {
      reason = "FindSegmentation prompt ROI is empty";
      return false;
    }

    setInt("global_roi_x0", x0);
    setInt("global_roi_y0", y0);
    setInt("global_roi_x1", x1);
    setInt("global_roi_y1", y1);
    setInt("global_roi_x", x0);
    setInt("global_roi_y", y0);
    setInt("global_roi_width", x1 - x0);
    setInt("global_roi_height", y1 - y0);
    setInt("global_seed_x", (x0 + x1) * 0.5);
    setInt("global_seed_y", (y0 + y1) * 0.5);

    UpdateManualGaugeFromShapeElement(context, element);
    ApplyManualGaugeToGlobals(context);
    reason = "FindSegmentation prompt ROI exported to global_roi_*";
    return true;
  }

  if (element.owner_type == "FindObject" &&
      element.owner_binding == "setrect" &&
      element.shape->kind() == CxShapeKind::Rect) {
    const auto *rect = dynamic_cast<const RectShape *>(element.shape.get());
    if (rect == nullptr) {
      reason = "FindObject ROI is not a rectangle";
      return false;
    }
    const double x0 = std::min(rect->x0(), rect->x1());
    const double y0 = std::min(rect->y0(), rect->y1());
    const double x1 = std::max(rect->x0(), rect->x1());
    const double y1 = std::max(rect->y0(), rect->y1());
    if (x1 <= x0 || y1 <= y0) {
      reason = "FindObject ROI is empty";
      return false;
    }
    setInt("global_roi_x0", x0);
    setInt("global_roi_y0", y0);
    setInt("global_roi_x1", x1);
    setInt("global_roi_y1", y1);
    setInt("global_roi_x", x0);
    setInt("global_roi_y", y0);
    setInt("global_roi_width", x1 - x0);
    setInt("global_roi_height", y1 - y0);
    UpdateManualGaugeFromShapeElement(context, element);
    ApplyManualGaugeToGlobals(context);
    reason = "FindObject ROI exported to global_roi_*";
    return true;
  }

  if (element.owner_type == "FindSegmentation" &&
      (element.owner_binding == "setpositivepointxy" ||
       element.owner_binding == "setpositivepoint" ||
       element.owner_binding == "setnegativepointxy" ||
       element.owner_binding == "setnegativepoint") &&
      element.shape->kind() == CxShapeKind::Points) {
    CxShapeGeometrySnapshot geometry;
    if (!element.shape->snapshot(geometry) || geometry.points.empty()) {
      reason = "FindSegmentation prompt point has no point geometry";
      return false;
    }
    const CxShapePoint &point = geometry.points.front();
    if (element.owner_binding == "setpositivepointxy" ||
        element.owner_binding == "setpositivepoint") {
      setInt("global_segmentation_positive_enabled", 1);
      setInt("global_segmentation_positive_x", point.x);
      setInt("global_segmentation_positive_y", point.y);
      reason = "FindSegmentation positive point appended to prompt list and "
               "exported to global_segmentation_positive_*";
    } else {
      setInt("global_segmentation_negative_enabled", 1);
      setInt("global_segmentation_negative_x", point.x);
      setInt("global_segmentation_negative_y", point.y);
      reason = "FindSegmentation negative point appended to prompt list and "
               "exported to global_segmentation_negative_*";
    }
    UpdateManualGaugeFromShapeElement(context, element);
    ApplyManualGaugeToGlobals(context);
    return true;
  }

  if (element.shape->kind() == CxShapeKind::Circle) {
    CxShapePoint center;
    double radius = 0.0;
    double inner_radius = 0.0;
    if (!element.shape->exportCircle(center, radius, inner_radius) ||
        radius <= 0.0) {
      reason = "selected circle cannot export center/radius";
      return false;
    }

    setInt("global_circle_cx", center.x);
    setInt("global_circle_cy", center.y);
    setInt("global_circle_px", center.x + radius);
    setInt("global_circle_py", center.y);
    setInt("global_circle_inner_radius", inner_radius);
    setInt("global_circle_outer_radius", radius);
    setInt("global_circle_ring_width", std::max(0.0, radius - inner_radius));
    const auto *circle = dynamic_cast<const CircleShape *>(element.shape.get());
    if (circle != nullptr) {
      setInt("global_findcircle_arc_enabled", circle->hasScanSector() ? 1 : 0);
      setInt("global_findcircle_arc_start_deg",
             circle->scanSectorStartDegrees());
      setInt("global_findcircle_arc_end_deg", circle->scanSectorEndDegrees());
    }
    setInt("global_seed_x", center.x);
    setInt("global_seed_y", center.y);
    UpdateManualGaugeFromShapeElement(context, element);
    reason = "circle exported to global_circle_* and global_seed_*";
    return true;
  }

  if (element.shape->kind() == CxShapeKind::Ellipse) {
    if (!element.editable || element.semantic_role != "roi") {
      reason = "selected ellipse is a result/display element; only editable "
               "FindEllipse roi can export to global_ellipse_*";
      return false;
    }

    CxShapeGeometrySnapshot geometry;
    if (!element.shape->snapshot(geometry) || geometry.radius_x <= 0.0 ||
        geometry.radius_y <= 0.0) {
      reason = "selected ellipse cannot export center/radius";
      return false;
    }

    setInt("global_ellipse_x0", geometry.center.x - geometry.radius_x);
    setInt("global_ellipse_y0", geometry.center.y - geometry.radius_y);
    setInt("global_ellipse_x1", geometry.center.x + geometry.radius_x);
    setInt("global_ellipse_y1", geometry.center.y + geometry.radius_y);
    const auto *ellipse =
        dynamic_cast<const EllipseShape *>(element.shape.get());
    if (ellipse != nullptr) {
      setInt("global_findellipse_inner_scale_percent",
             ellipse->innerScalePercent());
    }
    setInt("global_seed_x", geometry.center.x);
    setInt("global_seed_y", geometry.center.y);
    UpdateManualGaugeFromShapeElement(context, element);
    ApplyManualGaugeToGlobals(context);
    reason = "ellipse exported to global_ellipse_* and global_seed_*";
    return true;
  }

  CxShapePoint p0;
  CxShapePoint p1;
  if (element.shape->exportLine(p0, p1)) {
    setInt("global_roi_x0", p0.x);
    setInt("global_roi_y0", p0.y);
    setInt("global_roi_x1", p1.x);
    setInt("global_roi_y1", p1.y);
    UpdateManualGaugeFromShapeElement(context, element);
    reason = "line exported to global_roi_*";
    return true;
  }

  std::vector<CxShapePoint> points;
  bool closed = false;
  element.shape->exportPolyline(points, closed);
  if (points.empty())
    element.shape->exportPoints(points);

  if (points.empty()) {
    reason = "selected shape has no exportable points";
    return false;
  }

  double min_x = points[0].x;
  double min_y = points[0].y;
  double max_x = points[0].x;
  double max_y = points[0].y;
  for (const CxShapePoint &p : points) {
    min_x = std::min(min_x, p.x);
    min_y = std::min(min_y, p.y);
    max_x = std::max(max_x, p.x);
    max_y = std::max(max_y, p.y);
  }

  setInt("global_roi_x0", min_x);
  setInt("global_roi_y0", min_y);
  setInt("global_roi_x1", max_x);
  setInt("global_roi_y1", max_y);
  setInt("global_seed_x", (min_x + max_x) * 0.5);
  setInt("global_seed_y", (min_y + max_y) * 0.5);

  if (points.size() == 1)
    reason = "point exported to global_seed_* and degenerate global_roi_*";
  else
    reason =
        "shape bounds exported to global_roi_* and center to global_seed_*";
  return true;
}

void InsertStatement(std::string &editor, const std::string &statement) {
  if (statement.empty())
    return;
  if (!editor.empty() && editor.back() != '\n')
    editor.push_back('\n');
  editor += statement;
  if (editor.empty() || editor.back() != '\n')
    editor.push_back('\n');
}

void ReplaceEditorLine(std::string &editor, int lineIndex,
                       const std::string &statement) {
  if (statement.empty() || lineIndex < 0)
    return;
  std::istringstream input(editor);
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(input, line))
    lines.push_back(line);
  if (lineIndex >= static_cast<int>(lines.size()))
    return;
  lines[static_cast<std::size_t>(lineIndex)] = statement;
  std::ostringstream output;
  for (const std::string &item : lines)
    output << item << '\n';
  editor = output.str();
}
} // namespace

void ViewController::initImageEvidenceLayer() {
  const fs::path repositoryRoot =
      fs::path(__FILE__).parent_path().parent_path();
  m_annotationManifestPath =
      (repositoryRoot / "cxparser" / "cxscript" / "module" / "cximage" /
       "tool_annotation_basic.cxsc")
          .generic_string();
  m_annotationSessionPath = (repositoryRoot / "cxparser" / "cxscript" /
                             "annotations" / "session_001.cxann")
                                .generic_string();

  std::string init_reason;
  if (!m_parserOwner.Initialize(init_reason)) {
    m_annotationStatus = "parser initialize failed: " + init_reason;
    return;
  }

  CxAnnotationToolManifestSnapshot snapshot;
  if (!m_parserOwner.ParseAnnotationToolManifest(
          m_annotationManifestPath, snapshot, m_annotationStatus)) {
    return;
  }

  if (!m_annotationLayer.ApplyToolManifestSnapshot(snapshot,
                                                   m_annotationStatus)) {
    return;
  }
}

bool ViewController::IsAnnotationCreateModeActive() const {
  return m_imageToolEnabled && m_imageToolMode != ImageToolMode::PointerPan;
}

bool ViewController::IsMouseInsideImageCanvas(const ImVec2 &p) const {
  return p.x >= m_imageCanvasMin.x && p.x <= m_imageCanvasMax.x &&
         p.y >= m_imageCanvasMin.y && p.y <= m_imageCanvasMax.y;
}

bool ViewController::HasActiveFindCircleGauge() const {
  return m_manualTest.current_gauge.has_circle_gauge ||
         m_manualTest.current_gauge.tool == "FindCircle";
}

bool ViewController::HasEditableFindCircleGauge() const {
  return m_manualTest.current_gauge.has_circle_gauge &&
         m_manualTest.current_gauge.circle_cx > 0 &&
         m_manualTest.current_gauge.circle_cy > 0 &&
         m_manualTest.current_gauge.radius > 0;
}

bool ViewController::SyncFindSegmentationPromptListsFromShapeElements(
    std::string &reason) {
  reason.clear();
  ManualGaugeState &gauge = m_manualTest.current_gauge;
  // Annotation Tools can be used before a Manual State Test script is
  // selected. Seg +/- ShapeElements are therefore the Auto Boundary source of
  // truth; the previous gauge/script context must not block point inference.

  std::vector<ManualSegmentationPromptPoint> positivePoints;
  std::vector<ManualSegmentationPromptPoint> negativePoints;
  bool foundPromptRect = false;

  for (const CxShapeElement &element : m_annotationLayer.ShapeElements()) {
    if (element.owner_type != "FindSegmentation" || !element.shape)
      continue;
    if (element.result_element || element.semantic_role == "boundary" ||
        element.semantic_role == "boundary_bbox") {
      continue;
    }

    if ((element.semantic_role == "prompt_rect" ||
         element.semantic_role == "roi" ||
         element.owner_binding == "setpromptrectxyxy" ||
         element.owner_binding == "setpromptrect") &&
        element.shape->kind() == CxShapeKind::Rect) {
      const auto *rect = dynamic_cast<const RectShape *>(element.shape.get());
      if (rect != nullptr) {
        gauge.segmentation_prompt_x0 =
            static_cast<int>(std::lround(std::min(rect->x0(), rect->x1())));
        gauge.segmentation_prompt_y0 =
            static_cast<int>(std::lround(std::min(rect->y0(), rect->y1())));
        gauge.segmentation_prompt_x1 =
            static_cast<int>(std::lround(std::max(rect->x0(), rect->x1())));
        gauge.segmentation_prompt_y1 =
            static_cast<int>(std::lround(std::max(rect->y0(), rect->y1())));
        gauge.has_segmentation_prompt_rect = true;
        foundPromptRect = true;
      }
      continue;
    }

    if ((element.semantic_role != "prompt_positive" &&
         element.semantic_role != "prompt_negative" &&
         element.owner_binding != "setpositivepointxy" &&
         element.owner_binding != "setpositivepoint" &&
         element.owner_binding != "setnegativepointxy" &&
         element.owner_binding != "setnegativepoint") ||
        element.shape->kind() != CxShapeKind::Points) {
      continue;
    }

    CxShapeGeometrySnapshot geometry;
    if (!element.shape->snapshot(geometry) || geometry.points.empty())
      continue;

    ManualSegmentationPromptPoint point;
    point.ref = element.stable_ref.empty() ? element.ref : element.stable_ref;
    point.x = static_cast<int>(std::lround(geometry.points.front().x));
    point.y = static_cast<int>(std::lround(geometry.points.front().y));

    if (element.semantic_role == "prompt_negative" ||
        element.owner_binding == "setnegativepointxy" ||
        element.owner_binding == "setnegativepoint") {
      UpsertSegmentationPromptListPoint(negativePoints, point.ref, point.x,
                                        point.y);
    } else {
      UpsertSegmentationPromptListPoint(positivePoints, point.ref, point.x,
                                        point.y);
    }
  }

  auto sortByRef = [](const ManualSegmentationPromptPoint &a,
                      const ManualSegmentationPromptPoint &b) {
    return a.ref < b.ref;
  };
  std::sort(positivePoints.begin(), positivePoints.end(), sortByRef);
  std::sort(negativePoints.begin(), negativePoints.end(), sortByRef);

  if (!foundPromptRect) {
    // Point-only Auto Boundary must not synthesize an ROI from globals or a
    // stale gauge. A synthetic rectangle was invisible but still recorded as
    // a segmentation input, hiding the real Seg +/- contract.
    gauge.has_segmentation_prompt_rect = false;
  }

  if (positivePoints.empty() && negativePoints.empty()) {

    gauge.tool = "FindSegmentation";

    gauge.source = "annotation_shape_elements";

    if (foundPromptRect) {

      gauge.review_status = "editing";

      gauge.accepted = false;

      gauge.dirty = true;
    }

    std::ostringstream ss;

    ss << "no Seg +/- point ShapeElements found"

       << " positive_points=" << gauge.segmentation_positive_points.size()

       << " negative_points=" << gauge.segmentation_negative_points.size()

       << " prompt_rect=" << (gauge.has_segmentation_prompt_rect ? 1 : 0)

       << " rect_synced=" << (foundPromptRect ? 1 : 0);

    reason = ss.str();

    CXLOG_INFO("ImageAnnotationUI", "findsegmentation_prompt_lists_preserved",

               "preserved", reason);

    return foundPromptRect;
  }

  const bool changed =
      positivePoints.size() != gauge.segmentation_positive_points.size() ||
      negativePoints.size() != gauge.segmentation_negative_points.size() ||
      foundPromptRect;

  gauge.tool = "FindSegmentation";
  gauge.segmentation_positive_points = std::move(positivePoints);
  gauge.segmentation_negative_points = std::move(negativePoints);
  SyncSegmentationLegacyPointFromLists(gauge);
  gauge.source = "annotation_shape_elements";
  gauge.review_status = "editing";
  gauge.accepted = false;
  gauge.dirty = true;

  const bool globalsApplied = ApplyManualGaugeToGlobals(m_manualTest);
  std::ostringstream ss;
  ss << "positive_points=" << gauge.segmentation_positive_points.size()
     << " negative_points=" << gauge.segmentation_negative_points.size()
     << " prompt_rect=" << (gauge.has_segmentation_prompt_rect ? 1 : 0)
     << " globals=" << (globalsApplied ? "applied" : "pending")
     << " local_point_prompt=ready";
  reason = ss.str();
  CXLOG_INFO("ImageAnnotationUI", "findsegmentation_prompt_lists_synced",
             globalsApplied ? "updated" : "local_ready_globals_pending", reason);
  // Point-prompt boundary inference is a local Image View operation.  It must
  // remain available even when there is no loaded CxScript runtime to receive
  // legacy global_* writeback.  Global export is kept as an observable
  // follow-up state, not an execution prerequisite.
  return changed || !gauge.segmentation_positive_points.empty() ||
         !gauge.segmentation_negative_points.empty();
}

bool ViewController::ApplyCurrentGaugeToEditableShape(std::string &reason) {
  ManualGaugeState &gauge = m_manualTest.current_gauge;
  if (gauge.tool == "FindSegmentation") {
    auto readInt = [this](const std::string &key, int fallback) {
      auto it = m_manualTest.runtime_int_vars.find(key);
      return it == m_manualTest.runtime_int_vars.end() ? fallback : it->second;
    };
    if (!gauge.has_segmentation_prompt_rect) {
      gauge.segmentation_prompt_x0 = std::max(0, readInt("global_roi_x0", 120));
      gauge.segmentation_prompt_y0 = std::max(0, readInt("global_roi_y0", 120));
      gauge.segmentation_prompt_x1 = std::max(gauge.segmentation_prompt_x0 + 1,
                                              readInt("global_roi_x1", 980));
      gauge.segmentation_prompt_y1 = std::max(gauge.segmentation_prompt_y0 + 1,
                                              readInt("global_roi_y1", 820));
      gauge.has_segmentation_prompt_rect = true;
    }

    auto shape = std::make_unique<RectShape>();
    shape->setRect(static_cast<double>(gauge.segmentation_prompt_x0),
                   static_cast<double>(gauge.segmentation_prompt_y0),
                   static_cast<double>(gauge.segmentation_prompt_x1),
                   static_cast<double>(gauge.segmentation_prompt_y1));

    const std::string ownerRef =
        gauge.primary_object_name.empty() ? "m_seg" : gauge.primary_object_name;
    CxShapeElement &element = m_annotationLayer.UpsertShape(
        ownerRef + ".prompt_rect", std::move(shape));
    element.tool_id = "FindSegmentation";
    element.owner_type = "FindSegmentation";
    element.owner_ref = ownerRef;
    element.owner_binding = "setpromptrectxyxy";
    element.semantic_role = "prompt_rect";
    element.editable = true;
    element.visible = true;
    element.runtime_bound = false;
    element.result_element = false;
    element.stale = false;
    element.runtime_edit_pending = false;

    auto upsert_prompt_point =
        [this, &ownerRef](const std::string &stable_suffix,
                          const std::string &binding, const std::string &role,
                          int x, int y) {
          auto point_shape = std::make_unique<PointsShape>();
          point_shape->addpoint(gp_Pnt(x, y, 0.0));
          CxShapeElement &point = m_annotationLayer.UpsertShape(
              ownerRef + stable_suffix, std::move(point_shape));
          point.tool_id = "FindSegmentation";
          point.owner_type = "FindSegmentation";
          point.owner_ref = ownerRef;
          point.owner_binding = binding;
          point.semantic_role = role;
          point.editable = true;
          point.visible = true;
          point.runtime_bound = false;
          point.result_element = false;
          point.stale = false;
          point.runtime_edit_pending = false;
        };
    auto remove_existing_prompt_points = [this, &ownerRef]() {
      auto &elements = m_annotationLayer.ShapeElements();
      auto it = elements.begin();
      while (it != elements.end()) {
        const bool sameOwner =
            it->owner_type == "FindSegmentation" &&
            (it->owner_ref.empty() || it->owner_ref == ownerRef);
        const bool promptPoint = it->shape &&
                                 it->shape->kind() == CxShapeKind::Points &&
                                 (it->semantic_role == "prompt_positive" ||
                                  it->semantic_role == "prompt_negative" ||
                                  it->owner_binding == "setpositivepointxy" ||
                                  it->owner_binding == "setpositivepoint" ||
                                  it->owner_binding == "setnegativepointxy" ||
                                  it->owner_binding == "setnegativepoint");
        if (sameOwner && promptPoint)
          it = elements.erase(it);
        else
          ++it;
      }
      m_annotationLayer.SelectShape(-1);
    };

    remove_existing_prompt_points();

    std::vector<ManualSegmentationPromptPoint> positivePoints =
        gauge.segmentation_positive_points;
    if (positivePoints.empty() && gauge.has_segmentation_positive_point &&
        gauge.segmentation_positive_x > 0 &&
        gauge.segmentation_positive_y > 0) {
      ManualSegmentationPromptPoint point;
      point.ref = ownerRef + ".prompt_positive.0";
      point.x = gauge.segmentation_positive_x;
      point.y = gauge.segmentation_positive_y;
      positivePoints.push_back(point);
    }

    std::vector<ManualSegmentationPromptPoint> negativePoints =
        gauge.segmentation_negative_points;
    if (negativePoints.empty() && gauge.has_segmentation_negative_point &&
        gauge.segmentation_negative_x > 0 &&
        gauge.segmentation_negative_y > 0) {
      ManualSegmentationPromptPoint point;
      point.ref = ownerRef + ".prompt_negative.0";
      point.x = gauge.segmentation_negative_x;
      point.y = gauge.segmentation_negative_y;
      negativePoints.push_back(point);
    }

    NormalizeSegmentationPromptList(positivePoints);
    NormalizeSegmentationPromptList(negativePoints);
    gauge.segmentation_positive_points = positivePoints;
    gauge.segmentation_negative_points = negativePoints;
    SyncSegmentationLegacyPointFromLists(gauge);

    for (int i = 0; i < static_cast<int>(positivePoints.size()); ++i) {
      upsert_prompt_point(".prompt_positive." + std::to_string(i),
                          "setpositivepointxy", "prompt_positive",
                          positivePoints[i].x, positivePoints[i].y);
    }
    for (int i = static_cast<int>(positivePoints.size()); i < 128; ++i) {
      m_annotationLayer.RemoveShapeByStableRef(ownerRef + ".prompt_positive." +
                                               std::to_string(i));
    }
    for (int i = 0; i < static_cast<int>(negativePoints.size()); ++i) {
      upsert_prompt_point(".prompt_negative." + std::to_string(i),
                          "setnegativepointxy", "prompt_negative",
                          negativePoints[i].x, negativePoints[i].y);
    }
    for (int i = static_cast<int>(negativePoints.size()); i < 128; ++i) {
      m_annotationLayer.RemoveShapeByStableRef(ownerRef + ".prompt_negative." +
                                               std::to_string(i));
    }
    m_annotationLayer.MarkOwnerResultStale("FindSegmentation", ownerRef);

    ApplyManualGaugeToGlobals(m_manualTest);
    gauge.source = "key_parameter_controls";
    gauge.dirty = true;
    gauge.accepted = false;
    reason = "FindSegmentation prompt ROI and typed points preview applied to "
             "Image View";
    return true;
  }

  if (gauge.tool == "FindObject") {
    auto readInt = [this](const std::string &key, int fallback) {
      const auto it = m_manualTest.runtime_int_vars.find(key);
      return it == m_manualTest.runtime_int_vars.end() ? fallback : it->second;
    };
    if (!gauge.has_findobject_roi) {
      gauge.findobject_x0 = std::max(0, readInt("global_roi_x0", 80));
      gauge.findobject_y0 = std::max(0, readInt("global_roi_y0", 80));
      gauge.findobject_x1 =
          std::max(gauge.findobject_x0 + 1, readInt("global_roi_x1", 1180));
      gauge.findobject_y1 =
          std::max(gauge.findobject_y0 + 1, readInt("global_roi_y1", 880));
      gauge.has_findobject_roi = true;
    }
    auto shape = std::make_unique<RectShape>();
    shape->setRect(gauge.findobject_x0, gauge.findobject_y0,
                   gauge.findobject_x1, gauge.findobject_y1);
    const std::string ownerRef = gauge.primary_object_name.empty()
                                     ? "m_object"
                                     : gauge.primary_object_name;
    CxShapeElement &element =
        m_annotationLayer.UpsertShape(ownerRef + ".roi", std::move(shape));
    element.tool_id = "FindObject";
    element.owner_type = "FindObject";
    element.owner_ref = ownerRef;
    element.owner_binding = "setrect";
    element.semantic_role = "roi";
    element.editable = true;
    element.visible = true;
    element.runtime_bound = true;
    element.result_element = false;
    element.stale = true;
    element.runtime_edit_pending = true;
    m_annotationLayer.MarkOwnerResultStale("FindObject", ownerRef);
    ApplyManualGaugeToGlobals(m_manualTest);
    gauge.source = "key_parameter_controls";
    gauge.dirty = true;
    gauge.accepted = false;
    reason =
        "FindObject ROI and component parameters preview applied to Image View";
    return true;
  }

  if (gauge.tool == "FastMatch" || gauge.tool == "fastmatch" ||
      gauge.tool == "CFastMatch") {
    auto readInt = [this](const std::string &key, int fallback) {
      auto it = m_manualTest.runtime_int_vars.find(key);
      return it == m_manualTest.runtime_int_vars.end() ? fallback : it->second;
    };

    const int learnX = std::max(0, readInt("global_learn_roi_x", 120));
    const int learnY = std::max(0, readInt("global_learn_roi_y", 120));
    const int learnW = std::max(1, readInt("global_learn_roi_w", 120));
    const int learnH = std::max(1, readInt("global_learn_roi_h", 90));
    const int searchX = std::max(0, readInt("global_search_roi_x", 0));
    const int searchY = std::max(0, readInt("global_search_roi_y", 0));
    const int searchW = std::max(1, readInt("global_search_roi_w", 640));
    const int searchH = std::max(1, readInt("global_search_roi_h", 480));

    if (searchW <= learnW || searchH <= learnH) {
      reason = "FastMatch search ROI must be larger than learn ROI before "
               "preview/apply";
      return false;
    }

    const std::string ownerRef = gauge.primary_object_name.empty()
                                     ? "m_match"
                                     : gauge.primary_object_name;

    auto upsertFastMatchRect = [this, &ownerRef](const std::string &binding,
                                                 int x, int y, int w, int h) {
      auto shape = std::make_unique<RectShape>();
      shape->setRect(static_cast<double>(x), static_cast<double>(y),
                     static_cast<double>(x + w), static_cast<double>(y + h));
      CxShapeElement &element = m_annotationLayer.UpsertShape(
          ownerRef + "." + binding, std::move(shape));
      element.tool_id = "FastMatch";
      element.owner_type = "FastMatch";
      element.owner_ref = ownerRef;
      element.owner_binding = binding;
      element.semantic_role = binding;
      element.editable = true;
      element.visible = true;
      element.runtime_bound = true;
      element.result_element = false;
      element.stale = true;
      element.runtime_edit_pending = true;
    };

    upsertFastMatchRect("learn_roi", learnX, learnY, learnW, learnH);
    upsertFastMatchRect("search_roi", searchX, searchY, searchW, searchH);
    m_annotationLayer.MarkOwnerResultStale("FastMatch", ownerRef);

    ApplyManualGaugeToGlobals(m_manualTest);
    gauge.source = "key_parameter_controls";
    gauge.dirty = true;
    gauge.accepted = false;
    reason =
        "FastMatch Learn/Search ROI preview applied from global_* xywh values";
    return true;
  }

  if (gauge.tool == "FindEllipse" || gauge.has_ellipse_gauge) {
    if (!gauge.has_ellipse_gauge) {
      reason = "Apply To Gauge requires an active FindEllipse gauge";
      return false;
    }

    const int x0 = std::min(gauge.ellipse_x0, gauge.ellipse_x1);
    const int y0 = std::min(gauge.ellipse_y0, gauge.ellipse_y1);
    const int x1 = std::max(gauge.ellipse_x0, gauge.ellipse_x1);
    const int y1 = std::max(gauge.ellipse_y0, gauge.ellipse_y1);
    if (x1 <= x0 || y1 <= y0) {
      reason = "FindEllipse gauge rectangle is empty";
      return false;
    }

    gauge.tool = "FindEllipse";
    gauge.has_ellipse_gauge = true;
    gauge.has_line_gauge = false;
    gauge.has_circle_gauge = false;
    gauge.ellipse_x0 = x0;
    gauge.ellipse_y0 = y0;
    gauge.ellipse_x1 = x1;
    gauge.ellipse_y1 = y1;

    const double cx = (static_cast<double>(x0) + static_cast<double>(x1)) * 0.5;
    const double cy = (static_cast<double>(y0) + static_cast<double>(y1)) * 0.5;
    const double rx = std::max(
        1.0, (static_cast<double>(x1) - static_cast<double>(x0)) * 0.5);
    const double ry = std::max(
        1.0, (static_cast<double>(y1) - static_cast<double>(y0)) * 0.5);

    const int innerScale =
        std::max(0, std::min(99, gauge.ellipse_inner_scale_percent));
    gauge.ellipse_inner_scale_percent = innerScale;

    auto shape = std::make_unique<EllipseShape>(cx, cy, rx, ry);
    shape->setInnerScalePercent(innerScale);
    const std::string ownerRef = gauge.primary_object_name.empty()
                                     ? "m_ellipse"
                                     : gauge.primary_object_name;
    CxShapeElement &element = m_annotationLayer.UpsertShape(
        ownerRef + ".roi_ellipse", std::move(shape));
    element.tool_id = "FindEllipse";
    element.owner_type = "FindEllipse";
    element.owner_ref = ownerRef;
    element.owner_binding = "setellipse";
    element.semantic_role = "roi";
    element.editable = true;
    element.visible = true;
    element.runtime_bound = true;
    element.result_element = false;
    element.stale = true;
    element.runtime_edit_pending = true;

    m_annotationLayer.RemoveShapeByStableRef(ownerRef + ".inner_scan_ellipse");
    m_annotationLayer.MarkOwnerResultStale("FindEllipse", ownerRef);

    CXLOG_INFO("ManualConsole", "findellipse_apply_to_gauge", "value_snapshot",
               "FindEllipse Apply To Gauge updated ShapeElement/global_* only; "
               "runtime object will be rebuilt by Run Script");

    ApplyManualGaugeToGlobals(m_manualTest);
    gauge.source = "key_parameter_controls";
    gauge.dirty = true;
    gauge.accepted = false;
    reason =
        "FindEllipse ROI preview applied to Image View and global_ellipse_*";
    return true;
  }

  if (gauge.tool != "FindCircle" || !gauge.has_circle_gauge) {
    reason = "Apply To Gauge currently requires an active FindCircle gauge";
    return false;
  }

  const int outer =
      std::max(1, gauge.outer_radius > 0 ? gauge.outer_radius : gauge.radius);
  const int inner = std::max(0, std::min(gauge.inner_radius, outer - 1));
  gauge.outer_radius = outer;
  gauge.inner_radius = inner;
  gauge.radius = outer;
  gauge.circle_px = gauge.circle_cx + outer;
  gauge.circle_py = gauge.circle_cy;
  if (gauge.circle_arc_enabled != 0 &&
      std::abs(gauge.circle_arc_end_deg - gauge.circle_arc_start_deg) >= 360) {
    gauge.circle_arc_enabled = 0;
    gauge.circle_arc_start_deg = 0;
    gauge.circle_arc_end_deg = 360;
  }

  if (!m_annotationLayer.ApplyFindCircleAnnulusGauge(
          gauge.primary_object_name, static_cast<double>(gauge.circle_cx),
          static_cast<double>(gauge.circle_cy), static_cast<double>(inner),
          static_cast<double>(outer), gauge.circle_arc_enabled != 0,
          static_cast<double>(gauge.circle_arc_start_deg),
          static_cast<double>(gauge.circle_arc_end_deg), reason)) {
    return false;
  }

  if (!gauge.primary_object_name.empty()) {
    FindCircle *runtime_circle =
        static_cast<FindCircle *>(m_parserDebugBridge.QueryClassObject(
            "FindCircle", gauge.primary_object_name));
    if (runtime_circle != nullptr) {
      const int ring_width = std::max(0, outer - inner);
      runtime_circle->Setgap(gauge.gap);
      runtime_circle->setlinegap(gauge.linegap);
      runtime_circle->setthre(gauge.threshold);
      runtime_circle->setmethod(gauge.method);
      if (ring_width > 0) {
        runtime_circle->setcircle2(gauge.circle_cx, gauge.circle_cy,
                                   gauge.circle_cx + outer, gauge.circle_cy,
                                   ring_width);
      } else {
        runtime_circle->setcircle(gauge.circle_cx, gauge.circle_cy,
                                  gauge.circle_cx + outer, gauge.circle_cy);
      }
      runtime_circle->setscanarc(gauge.circle_arc_start_deg,
                                 gauge.circle_arc_end_deg,
                                 gauge.circle_arc_enabled);
    }
  }

  ApplyManualGaugeToGlobals(m_manualTest);
  gauge.source = "key_parameter_controls";
  gauge.dirty = true;
  gauge.accepted = false;
  return true;
}

bool ViewController::ProjectCurrentGaugeToImageViewPreview(
    std::string &reason) {
  reason.clear();

  ManualGaugeState &gauge = m_manualTest.current_gauge;
  const std::string originalSource = gauge.source;
  const std::string originalReviewStatus = gauge.review_status;
  const bool originalDirty = gauge.dirty;
  const bool originalAccepted = gauge.accepted;

  auto restoreGaugeState = [&]() {
    gauge.source = originalSource;
    gauge.review_status = originalReviewStatus;
    gauge.dirty = originalDirty;
    gauge.accepted = originalAccepted;
  };

  auto ownerOrDefault = [&gauge](const char *fallback) -> std::string {
    return gauge.primary_object_name.empty() ? std::string(fallback)
                                             : gauge.primary_object_name;
  };

  auto stampInputElement =
      [](CxShapeElement &element, const std::string &toolId,
         const std::string &ownerType, const std::string &ownerRef,
         const std::string &binding, const std::string &role, bool editable) {
        element.tool_id = toolId;
        element.owner_type = ownerType;
        element.owner_ref = ownerRef;
        element.owner_binding = binding;
        element.semantic_role = role;
        element.editable = editable;
        element.visible = true;
        element.runtime_bound = false;
        element.result_element = false;
        element.stale = false;
        element.runtime_edit_pending = false;
        element.selected = false;
      };

  auto clearEvidencePreviewOwners = [this]() {
    const char *ownerTypes[] = {
        "FindLine",    "Findline",    "FindCircle",       "Findcircle",
        "FindEllipse", "Findellipse", "FindRect",         "Findrect",
        "FindObject",  "Findobject",  "FindSegmentation", "Findsegmentation",
        "FastMatch",   "fastmatch",   "CFastMatch"};

    auto isManagedGaugeOwner = [&ownerTypes](const std::string &ownerType) {
      return std::find(std::begin(ownerTypes), std::end(ownerTypes),
                       ownerType) != std::end(ownerTypes);
    };

    auto &overlayElements = m_annotationLayer.Elements();
    auto overlayIt = overlayElements.begin();
    while (overlayIt != overlayElements.end()) {
      if (isManagedGaugeOwner(overlayIt->owner_type))
        overlayIt = overlayElements.erase(overlayIt);
      else
        ++overlayIt;
    }

    auto &elements = m_annotationLayer.ShapeElements();
    auto it = elements.begin();
    while (it != elements.end()) {
      if (isManagedGaugeOwner(it->owner_type))
        it = elements.erase(it);
      else
        ++it;
    }
    m_annotationLayer.SelectShape(-1);
  };

  clearEvidencePreviewOwners();

  if (gauge.has_line_gauge || gauge.tool == "FindLine") {
    if (!gauge.has_line_gauge) {
      reason = "FindLine evidence has no line gauge geometry";
      restoreGaugeState();
      return false;
    }

    const std::string ownerRef = ownerOrDefault("m_line");

    auto shape = std::make_unique<LineGaugeShape>(
        static_cast<double>(gauge.line_x0), static_cast<double>(gauge.line_y0),
        static_cast<double>(gauge.line_x1), static_cast<double>(gauge.line_y1),
        static_cast<double>(std::max(1, gauge.tool_half_width)));
    CxShapeElement &element =
        m_annotationLayer.UpsertShape(ownerRef + ".roi_axis", std::move(shape));
    stampInputElement(element, "FindLine", "FindLine", ownerRef, "setline",
                      "roi", true);

    reason = "FindLine evidence gauge preview projected";
    restoreGaugeState();
    return true;
  }

  if (gauge.has_circle_gauge || gauge.tool == "FindCircle") {
    if (!gauge.has_circle_gauge) {
      reason = "FindCircle evidence has no circle gauge geometry";
      restoreGaugeState();
      return false;
    }

    const std::string ownerRef = ownerOrDefault("m_circle");

    int outer = gauge.outer_radius > 0 ? gauge.outer_radius : gauge.radius;
    if (outer <= 0) {
      outer = static_cast<int>(std::lround(
          std::hypot(static_cast<double>(gauge.circle_px - gauge.circle_cx),
                     static_cast<double>(gauge.circle_py - gauge.circle_cy))));
    }
    outer = std::max(1, outer);
    const int inner = std::max(0, std::min(gauge.inner_radius, outer - 1));

    auto shape = std::make_unique<CircleShape>(
        static_cast<double>(gauge.circle_cx),
        static_cast<double>(gauge.circle_cy), static_cast<double>(outer),
        static_cast<double>(inner));
    shape->setScanSector(gauge.circle_arc_enabled != 0,
                         static_cast<double>(gauge.circle_arc_start_deg),
                         static_cast<double>(gauge.circle_arc_end_deg));
    CxShapeElement &element = m_annotationLayer.UpsertShape(
        ownerRef + ".roi_circle", std::move(shape));
    stampInputElement(element, "FindCircle", "FindCircle", ownerRef,
                      "setcircle", "roi", true);

    reason = "FindCircle evidence gauge preview projected";
    restoreGaugeState();
    return true;
  }

  if (gauge.has_ellipse_gauge || gauge.tool == "FindEllipse") {
    if (!gauge.has_ellipse_gauge) {
      reason = "FindEllipse evidence has no ellipse gauge geometry";
      restoreGaugeState();
      return false;
    }

    const int x0 = std::min(gauge.ellipse_x0, gauge.ellipse_x1);
    const int y0 = std::min(gauge.ellipse_y0, gauge.ellipse_y1);
    const int x1 = std::max(gauge.ellipse_x0, gauge.ellipse_x1);
    const int y1 = std::max(gauge.ellipse_y0, gauge.ellipse_y1);
    if (x1 <= x0 || y1 <= y0) {
      reason = "FindEllipse evidence gauge rectangle is empty";
      restoreGaugeState();
      return false;
    }

    const std::string ownerRef = ownerOrDefault("m_ellipse");

    const double cx = (static_cast<double>(x0) + static_cast<double>(x1)) * 0.5;
    const double cy = (static_cast<double>(y0) + static_cast<double>(y1)) * 0.5;
    const double rx = std::max(
        1.0, (static_cast<double>(x1) - static_cast<double>(x0)) * 0.5);
    const double ry = std::max(
        1.0, (static_cast<double>(y1) - static_cast<double>(y0)) * 0.5);

    auto shape = std::make_unique<EllipseShape>(cx, cy, rx, ry);
    shape->setInnerScalePercent(gauge.ellipse_inner_scale_percent);
    CxShapeElement &element = m_annotationLayer.UpsertShape(
        ownerRef + ".roi_ellipse", std::move(shape));
    stampInputElement(element, "FindEllipse", "FindEllipse", ownerRef,
                      "setellipse", "roi", true);

    m_annotationLayer.RemoveShapeByStableRef(ownerRef + ".inner_scan_ellipse");

    reason = "FindEllipse evidence gauge preview projected";
    restoreGaugeState();
    return true;
  }

  if (gauge.has_findobject_roi || gauge.tool == "FindObject") {
    if (!gauge.has_findobject_roi) {
      reason = "FindObject evidence has no ROI geometry";
      restoreGaugeState();
      return false;
    }

    const std::string ownerRef = ownerOrDefault("m_object");

    auto shape = std::make_unique<RectShape>();
    shape->setRect(static_cast<double>(gauge.findobject_x0),
                   static_cast<double>(gauge.findobject_y0),
                   static_cast<double>(gauge.findobject_x1),
                   static_cast<double>(gauge.findobject_y1));
    CxShapeElement &element =
        m_annotationLayer.UpsertShape(ownerRef + ".roi", std::move(shape));
    stampInputElement(element, "FindObject", "FindObject", ownerRef, "setrect",
                      "roi", true);

    reason = "FindObject evidence ROI preview projected";
    restoreGaugeState();
    return true;
  }

  if (gauge.has_segmentation_prompt_rect || gauge.tool == "FindSegmentation") {
    if (!gauge.has_segmentation_prompt_rect) {
      reason = "FindSegmentation evidence has no prompt rectangle";
      restoreGaugeState();
      return false;
    }

    const std::string ownerRef = ownerOrDefault("m_seg");

    auto shape = std::make_unique<RectShape>();
    shape->setRect(static_cast<double>(gauge.segmentation_prompt_x0),
                   static_cast<double>(gauge.segmentation_prompt_y0),
                   static_cast<double>(gauge.segmentation_prompt_x1),
                   static_cast<double>(gauge.segmentation_prompt_y1));
    CxShapeElement &element = m_annotationLayer.UpsertShape(
        ownerRef + ".prompt_rect", std::move(shape));
    stampInputElement(element, "FindSegmentation", "FindSegmentation", ownerRef,
                      "setpromptrectxyxy", "prompt_rect", true);

    reason = "FindSegmentation evidence prompt preview projected";
    restoreGaugeState();
    return true;
  }

  if (gauge.tool == "FastMatch" || gauge.tool == "fastmatch" ||
      gauge.tool == "CFastMatch") {
    auto readInt = [this](const std::string &key, int fallback) {
      const auto it = m_manualTest.runtime_int_vars.find(key);
      return it == m_manualTest.runtime_int_vars.end() ? fallback : it->second;
    };

    const int learnX = std::max(0, readInt("global_learn_roi_x", 0));
    const int learnY = std::max(0, readInt("global_learn_roi_y", 0));
    const int learnW = std::max(0, readInt("global_learn_roi_w", 0));
    const int learnH = std::max(0, readInt("global_learn_roi_h", 0));
    const int searchX = std::max(0, readInt("global_search_roi_x", 0));
    const int searchY = std::max(0, readInt("global_search_roi_y", 0));
    const int searchW = std::max(0, readInt("global_search_roi_w", 0));
    const int searchH = std::max(0, readInt("global_search_roi_h", 0));

    if (learnW <= 0 || learnH <= 0 || searchW <= 0 || searchH <= 0) {
      reason = "FastMatch evidence has incomplete learn/search ROI globals";
      restoreGaugeState();
      return false;
    }

    const std::string ownerRef = ownerOrDefault("m_match");

    auto upsertFastMatchRect = [this, &stampInputElement,
                                &ownerRef](const std::string &binding, int x,
                                           int y, int w, int h) {
      auto shape = std::make_unique<RectShape>();
      shape->setRect(static_cast<double>(x), static_cast<double>(y),
                     static_cast<double>(x + w), static_cast<double>(y + h));
      CxShapeElement &element = m_annotationLayer.UpsertShape(
          ownerRef + "." + binding, std::move(shape));
      stampInputElement(element, "FastMatch", "FastMatch", ownerRef, binding,
                        binding, true);
    };

    upsertFastMatchRect("learn_roi", learnX, learnY, learnW, learnH);
    upsertFastMatchRect("search_roi", searchX, searchY, searchW, searchH);

    reason = "FastMatch evidence learn/search ROI preview projected";
    restoreGaugeState();
    return true;
  }

  reason = "selected evidence has no editable gauge preview";
  restoreGaugeState();
  return false;
}

const char *ViewController::ImageToolModeName(ImageToolMode mode) {
  switch (mode) {
  case ImageToolMode::PointerPan:
    return "Pointer / Pan";
  case ImageToolMode::PointCreate:
    return "Point";
  case ImageToolMode::LineCreate:
    return "Line";
  case ImageToolMode::RectCreate:
    return "Rect";
  case ImageToolMode::CircleCreate:
    return "Circle";
  case ImageToolMode::EllipseCreate:
    return "Ellipse";
  case ImageToolMode::PolylineCreate:
    return "Polyline";
  case ImageToolMode::MagicWandBoundary:
    return "Magic Wand Boundary";
  case ImageToolMode::AutoBoundary:
    return "Auto Boundary";
  case ImageToolMode::AttachToScript:
    return "Attach To Script";
  default:
    return "Unknown";
  }
}

void ViewController::CancelAnnotationCreate() {
  m_annotationCreateActive = false;
  m_annotationDragging = false;
  m_activePolylineElement = -1;
  m_activePolylinePoints.clear();
}

void ViewController::ClampImagePointToImageBounds(double &x, double &y) const {
  if (!m_imageViewImage.empty()) {
    x = std::max(0.0, std::min(x, static_cast<double>(m_imageViewImage.cols)));
    y = std::max(0.0, std::min(y, static_cast<double>(m_imageViewImage.rows)));
  }
}

ImVec2 ViewController::ImageToScreen(float ix, float iy) const {
  if (m_imageViewImage.empty())
    return ImVec2(m_annotationImagePosX, m_annotationImagePosY);
  float sx = m_annotationImageWidth / static_cast<float>(m_imageViewImage.cols);
  float sy =
      m_annotationImageHeight / static_cast<float>(m_imageViewImage.rows);
  return ImVec2(m_annotationImagePosX + ix * sx,
                m_annotationImagePosY + iy * sy);
}

ImVec2 ViewController::ScreenToImage(float sx, float sy) const {
  if (m_imageViewImage.empty() || m_annotationImageWidth <= 0.0f ||
      m_annotationImageHeight <= 0.0f)
    return ImVec2(-1.0f, -1.0f);
  return ImVec2((sx - m_annotationImagePosX) * m_imageViewImage.cols /
                    m_annotationImageWidth,
                (sy - m_annotationImagePosY) * m_imageViewImage.rows /
                    m_annotationImageHeight);
}

ImVec2 ViewController::ImageToScreenPoint(const OverlayImagePoint &p) const {
  return ImageToScreen(p.x, p.y);
}

ImVec2 ViewController::ScreenToImagePoint(const ImVec2 &p) const {
  return ScreenToImage(p.x, p.y);
}

static double Distance2(double x0, double y0, double x1, double y1) {
  const double dx = x1 - x0;
  const double dy = y1 - y0;
  return dx * dx + dy * dy;
}

static ImageToolMode
ToolModeFromAnnotationTool(const AnnotationToolDefinition &tool) {
  if (tool.action == "magic_wand_boundary")
    return ImageToolMode::MagicWandBoundary;
  if (tool.kind == OverlayKind::Point)
    return ImageToolMode::PointCreate;
  if (tool.kind == OverlayKind::Line)
    return ImageToolMode::LineCreate;
  if (tool.kind == OverlayKind::Rect)
    return ImageToolMode::RectCreate;
  if (tool.kind == OverlayKind::Circle)
    return ImageToolMode::CircleCreate;
  if (tool.kind == OverlayKind::Ellipse)
    return ImageToolMode::EllipseCreate;
  if (tool.kind == OverlayKind::Polyline ||
      tool.kind == OverlayKind::BoundaryPolyline)
    return ImageToolMode::PolylineCreate;
  if (tool.kind == OverlayKind::AutoBoundaryRequest ||
      tool.action == "auto_segmentation")
    return ImageToolMode::AutoBoundary;
  return ImageToolMode::PointerPan;
}

const char *BoundaryAnchorModeKey(int mode) {
  static const char *keys[] = {
      "custom", "center_nearest", "contour_midpoint", "x_nearest",
      "y_nearest", "x_minus", "x_plus", "y_plus", "y_minus"};
  return keys[std::clamp(mode, 0, static_cast<int>(IM_ARRAYSIZE(keys)) - 1)];
}

const char *const *BoundaryAnchorModeLabels() {
  static const char *labels[] = {
      "Custom (manual seed / first Seg +)",
      "Center nearest (nearest contour point)",
      "Contour midpoint (half perimeter)",
      "X nearest (nearest image mid-height)",
      "Y nearest (nearest image mid-width)",
      "X- (leftmost contour point)",
      "X+ (rightmost contour point)",
      "Y+ (topmost contour point)",
      "Y- (bottommost contour point)"};
  return labels;
}

cv::Point SelectBoundaryAnchor(const std::vector<cv::Point> &contour,
                               const cv::Point &custom,
                               const cv::Size &imageSize, int mode) {
  if (contour.empty())
    return custom;
  const int selected = std::clamp(mode, 0, 8);
  if (selected == 0)
    return custom;
  if (selected == 2) {
    double perimeter = 0.0;
    for (std::size_t index = 1; index < contour.size(); ++index)
      perimeter += cv::norm(contour[index] - contour[index - 1]);
    perimeter += cv::norm(contour.front() - contour.back());
    const double half = perimeter * 0.5;
    double travelled = 0.0;
    for (std::size_t index = 1; index < contour.size(); ++index) {
      const double segment = cv::norm(contour[index] - contour[index - 1]);
      if (travelled + segment >= half)
        return contour[index];
      travelled += segment;
    }
    return contour[contour.size() / 2];
  }
  const cv::Point center(imageSize.width / 2, imageSize.height / 2);
  auto best = contour.begin();
  auto score = [&](const cv::Point &point) {
    switch (selected) {
    case 1: return Distance2(point.x, point.y, center.x, center.y);
    case 3: return static_cast<double>(std::abs(point.y - center.y));
    case 4: return static_cast<double>(std::abs(point.x - center.x));
    case 5: return static_cast<double>(point.x);
    case 6: return static_cast<double>(-point.x);
    case 7: return static_cast<double>(point.y);
    case 8: return static_cast<double>(-point.y);
    default: return 0.0;
    }
  };
  for (auto it = std::next(contour.begin()); it != contour.end(); ++it) {
    if (score(*it) < score(*best))
      best = it;
  }
  return *best;
}

void ViewController::ClearMagicWandPreview(const std::string &status) {
  m_magicWandPreviewPoints.clear();
  m_magicWandRawBoundaryPoints.clear();
  m_boundaryAnchor = {};
  m_magicWandRegionPixels = 0;
  m_magicWandRawBoundaryPointCount = 0;
  m_magicWandFormFitNodeCount = 0;
  m_magicWandStatus = status;
  m_magicWandFormFitStatus = "FORMFIT_NODE_IDLE";
}

bool ViewController::WriteBusinessAnnotationReceipt(
    const CxShapeElement &element, const AnnotationToolDefinition &tool,
    const std::string &operation, const std::string &status,
    const std::string &trace, std::string &receiptPath,
    std::string &reason) const {
  receiptPath.clear();
  reason.clear();
  if (!element.shape) {
    reason = "accepted annotation has no geometry";
    return false;
  }

  std::vector<CxShapePoint> points;
  bool closed = false;
  element.shape->exportPolyline(points, closed);
  if (points.empty())
    element.shape->exportPoints(points);

  CxShapePoint center;
  double radius = 0.0;
  double innerRadius = 0.0;
  const bool hasCircle =
      element.shape->exportCircle(center, radius, innerRadius);
  double radiusX = 0.0;
  double radiusY = 0.0;
  double angle = 0.0;
  const bool hasEllipse =
      element.shape->exportEllipse(center, radiusX, radiusY, angle);
  CxShapePoint lineStart;
  CxShapePoint lineEnd;
  const bool hasLine = element.shape->exportLine(lineStart, lineEnd);

  const std::string caseId =
      !m_manualTest.active_case_id.empty()
          ? m_manualTest.active_case_id
          : (!m_manualTest.current_evidence_selection.case_id.empty()
                 ? m_manualTest.current_evidence_selection.case_id
                 : "local_annotation");
  const std::string imageLogicalRef =
      !m_manualTest.active_image_id.empty()
          ? m_manualTest.active_image_id
          : (!m_manualTest.current_evidence_selection.image_id.empty()
                 ? m_manualTest.current_evidence_selection.image_id
                 : "local_image_unbound");
  const std::filesystem::path root =
      ResolveCxVisionRunPath("cxscript_runs/business_annotation_receipts") /
      caseId;
  const std::string receiptName =
      "business_annotation_receipt_" +
      (element.stable_ref.empty() ? std::to_string(element.id)
                                  : element.stable_ref) +
      ".v1.json";
  const std::filesystem::path path = root / receiptName;

  try {
    std::filesystem::create_directories(root);
    std::ofstream output(path);
    if (!output) {
      reason = "failed to create local business annotation receipt";
      return false;
    }
    output << "{\n"
           << "  \"schema\": \"business_annotation_receipt.v1\",\n"
           << "  \"case_id\": \"" << BusinessReceiptJsonEscape(caseId)
           << "\",\n"
           << "  \"image_logical_ref\": \""
           << BusinessReceiptJsonEscape(imageLogicalRef) << "\",\n"
           << "  \"image_width\": " << m_imageViewImage.cols << ",\n"
           << "  \"image_height\": " << m_imageViewImage.rows << ",\n"
           << "  \"tool\": \"" << BusinessReceiptJsonEscape(tool.name)
           << "\",\n"
           << "  \"operation\": \""
           << BusinessReceiptJsonEscape(operation) << "\",\n"
           << "  \"status\": \"" << BusinessReceiptJsonEscape(status)
           << "\",\n"
           << "  \"geometry\": {\n"
           << "    \"kind\": \""
           << BusinessReceiptShapeKind(element.shape->kind()) << "\",\n"
           << "    \"closed\": " << (closed ? "true" : "false") << ",\n"
           << "    \"editable\": " << (element.editable ? "true" : "false")
           << ",\n"
           << "    \"pixel_points\": [";
    for (std::size_t index = 0; index < points.size(); ++index) {
      if (index != 0)
        output << ", ";
      output << "{\"x\":" << points[index].x << ",\"y\":"
             << points[index].y << "}";
    }
    output << "]";
    if (hasCircle) {
      output << ",\n    \"circle\": {\"cx\":" << center.x
             << ",\"cy\":" << center.y << ",\"radius\":" << radius
             << ",\"inner_radius\":" << innerRadius << "}";
    } else if (hasEllipse) {
      output << ",\n    \"ellipse\": {\"cx\":" << center.x
             << ",\"cy\":" << center.y << ",\"radius_x\":" << radiusX
             << ",\"radius_y\":" << radiusY << ",\"angle_deg\":"
             << angle << "}";
    } else if (hasLine) {
      output << ",\n    \"line\": {\"x0\":" << lineStart.x
             << ",\"y0\":" << lineStart.y << ",\"x1\":" << lineEnd.x
             << ",\"y1\":" << lineEnd.y << "}";
    }
    output << "\n  },\n";

    if (tool.action == "magic_wand_boundary") {
      output << "  \"seed\": {\"x\":" << m_magicWandSeed.x
             << ",\"y\":" << m_magicWandSeed.y << "},\n"
             << "  \"boundary_anchor\": {\"mode\": \""
             << BoundaryAnchorModeKey(m_boundaryAnchorMode) << "\", \"x\":"
             << m_boundaryAnchor.x << ",\"y\":" << m_boundaryAnchor.y
             << "},\n"
             << "  \"parameters\": {\"algorithm\": \""
             << (m_magicWandAlgorithm == 0 ? "color_fixed_range_v1"
                                           : "color_connected_range_v1")
             << "\", \"color_tolerance\": " << m_magicWandColorTolerance
             << ", \"connectivity\": "
             << (m_magicWandEightConnected ? 8 : 4)
             << ", \"node_residual_px\": " << m_magicWandSimplifyPixels
             << ", \"maximum_nodes\": " << m_magicWandMaximumNodes
             << ", \"minimum_node_spacing_px\": "
             << m_magicWandMinimumNodeSpacingPixels << "},\n"
             << "  \"raw_boundary_point_count\": "
             << m_magicWandRawBoundaryPointCount << ",\n"
             << "  \"node_count\": " << m_magicWandFormFitNodeCount
             << ",\n";
    } else if (tool.action == "auto_segmentation") {
      output << "  \"seed\": {\"x\":" << m_magicWandSeed.x
             << ",\"y\":" << m_magicWandSeed.y << "},\n"
             << "  \"boundary_anchor\": {\"mode\": \""
             << BoundaryAnchorModeKey(m_boundaryAnchorMode) << "\", \"x\":"
             << m_boundaryAnchor.x << ",\"y\":" << m_boundaryAnchor.y
             << "},\n"
             << "  \"parameters\": {\"algorithm\": \"local_point_prompt_segmentation_v1\""
             << ", \"grabcut_iterations\": "
             << m_promptBoundaryGrabCutIterations
             << ", \"prompt_rectangle_used\": false"
             << ", \"node_residual_px\": " << m_magicWandSimplifyPixels
             << ", \"maximum_nodes\": " << m_magicWandMaximumNodes
             << ", \"minimum_node_spacing_px\": "
             << m_magicWandMinimumNodeSpacingPixels << "},\n"
             << "  \"raw_boundary_point_count\": "
             << m_magicWandRawBoundaryPointCount << ",\n"
             << "  \"node_count\": " << m_magicWandFormFitNodeCount
             << ",\n";
    } else if (tool.action == "open_boundary_formfit") {
      output << "  \"seed\": null,\n"
             << "  \"parameters\": {\"boundary_role\": \"physical_interface\""
             << ", \"geometry_candidate\": \"" << m_openBoundaryType
             << "\", \"fit_rms_px\": " << m_openBoundaryFitResidualPx
             << ", \"line_rms_px\": " << m_openBoundaryLineResidualPx
             << ", \"arc_rms_px\": " << m_openBoundaryArcResidualPx
             << "},\n"
             << "  \"raw_boundary_point_count\": "
             << m_openBoundaryRawPoints.size() << ",\n"
             << "  \"node_count\": " << points.size() << ",\n";
    } else {
      output << "  \"seed\": null,\n"
             << "  \"parameters\": {},\n"
             << "  \"raw_boundary_point_count\": 0,\n"
             << "  \"node_count\": " << points.size() << ",\n";
    }
    output << "  \"trace\": \"" << BusinessReceiptJsonEscape(trace)
           << "\"\n}\n";
    receiptPath = path.string();
    reason = "business_annotation_receipt.v1 written without image data";
    return true;
  } catch (const std::exception &exception) {
    reason = "business annotation receipt exception: " +
             std::string(exception.what());
    return false;
  }
}

bool ViewController::BuildMagicWandFormFitNodes(
    const std::vector<cv::Point> &contour, std::string &reason) {
  m_magicWandPreviewPoints.clear();
  m_magicWandRawBoundaryPoints.clear();
  m_magicWandRawBoundaryPointCount = static_cast<int>(contour.size());
  m_magicWandFormFitNodeCount = 0;
  if (contour.size() < 3) {
    m_magicWandFormFitStatus = "FORMFIT_NODE_INSUFFICIENT_BOUNDARY";
    reason = "raw boundary has fewer than three points";
    return false;
  }

  for (const cv::Point &point : contour)
    m_magicWandRawBoundaryPoints.push_back(
        {static_cast<double>(point.x), static_cast<double>(point.y)});

  const int maximumNodes = std::clamp(m_magicWandMaximumNodes, 3, 1024);
  const double minimumSpacing =
      std::max(0.0f, m_magicWandMinimumNodeSpacingPixels);
  std::vector<cv::Point> candidate;
  if (m_magicWandNodeizationMode == 0) {
    // FormFit node extraction: iteratively increase the residual tolerance
    // until the closed contour has a bounded, reviewable set of geometric
    // nodes.  This is a single-boundary fit, not FastMatch correspondence.
    double epsilon = std::max(0.0f, m_magicWandSimplifyPixels);
    for (int iteration = 0; iteration < 12; ++iteration) {
      cv::approxPolyDP(contour, candidate, epsilon, true);
      if (static_cast<int>(candidate.size()) <= maximumNodes || epsilon <= 0.0)
        break;
      epsilon = std::max(0.5, epsilon * 1.45);
    }
  } else {
    candidate = contour;
  }

  std::vector<cv::Point> nodes;
  nodes.reserve(candidate.size());
  const auto farEnough = [minimumSpacing](const cv::Point &left,
                                           const cv::Point &right) {
    const double dx = static_cast<double>(left.x - right.x);
    const double dy = static_cast<double>(left.y - right.y);
    return dx * dx + dy * dy >= minimumSpacing * minimumSpacing;
  };
  for (const cv::Point &point : candidate) {
    if (nodes.empty() || farEnough(nodes.back(), point))
      nodes.push_back(point);
    if (static_cast<int>(nodes.size()) >= maximumNodes)
      break;
  }
  if (nodes.size() >= 3 && !farEnough(nodes.front(), nodes.back()))
    nodes.pop_back();
  if (nodes.size() < 3) {
    m_magicWandFormFitStatus = "FORMFIT_NODE_EXTRACTION_FAILED";
    reason = "node spacing/residual constraints left fewer than three nodes";
    return false;
  }

  for (const cv::Point &node : nodes)
    m_magicWandPreviewPoints.push_back(
        {static_cast<double>(node.x), static_cast<double>(node.y)});
  m_magicWandFormFitNodeCount = static_cast<int>(nodes.size());
  m_magicWandFormFitStatus =
      m_magicWandNodeizationMode == 0 ? "FORMFIT_CLOSED_POLYGON_READY"
                                      : "FORMFIT_RAW_CONTOUR_DEBUG_READY";
  reason = "FormFit nodes: raw=" +
           std::to_string(m_magicWandRawBoundaryPointCount) +
           ", nodes=" + std::to_string(m_magicWandFormFitNodeCount) +
           ", max_nodes=" + std::to_string(maximumNodes) +
           ", min_spacing_px=" + std::to_string(minimumSpacing);
  return true;
}

bool ViewController::BuildMagicWandPreview(double imageX, double imageY,
                                            std::string &reason) {
  ClearMagicWandPreview("MAGIC_WAND_RUNNING");
  if (m_imageViewImage.empty()) {
    m_magicWandStatus = "MAGIC_WAND_NO_IMAGE";
    reason = "no local image is loaded";
    return false;
  }

  const int seedX = static_cast<int>(std::lround(imageX));
  const int seedY = static_cast<int>(std::lround(imageY));
  if (seedX < 0 || seedY < 0 || seedX >= m_imageViewImage.cols ||
      seedY >= m_imageViewImage.rows) {
    m_magicWandStatus = "MAGIC_WAND_SEED_OUT_OF_BOUNDS";
    reason = "seed point is outside the local image";
    return false;
  }

  cv::Mat source;
  if (m_imageViewImage.channels() == 1) {
    cv::cvtColor(m_imageViewImage, source, cv::COLOR_GRAY2BGR);
  } else if (m_imageViewImage.channels() == 4) {
    cv::cvtColor(m_imageViewImage, source, cv::COLOR_BGRA2BGR);
  } else if (m_imageViewImage.channels() == 3) {
    source = m_imageViewImage;
  } else {
    m_magicWandStatus = "MAGIC_WAND_UNSUPPORTED_IMAGE";
    reason = "image channel format is not supported";
    return false;
  }

  const int tolerance = std::clamp(m_magicWandColorTolerance, 0, 255);
  cv::Mat floodMask = cv::Mat::zeros(source.rows + 2, source.cols + 2, CV_8UC1);
  const int connectivity = m_magicWandEightConnected ? 8 : 4;
  const bool fixedRange = m_magicWandAlgorithm == 0;
  const int flags = connectivity | cv::FLOODFILL_MASK_ONLY |
                    (fixedRange ? cv::FLOODFILL_FIXED_RANGE : 0) |
                    (255 << 8);
  const int regionPixels = cv::floodFill(
      source, floodMask, cv::Point(seedX, seedY), cv::Scalar(), nullptr,
      cv::Scalar(tolerance, tolerance, tolerance),
      cv::Scalar(tolerance, tolerance, tolerance), flags);
  if (regionPixels < std::max(3, m_magicWandMinimumRegionPixels)) {
    m_magicWandStatus = "MAGIC_WAND_REGION_TOO_SMALL";
    reason = "seed region contains " + std::to_string(regionPixels) +
             " pixels; increase tolerance or choose another seed";
    return false;
  }

  cv::Mat region = floodMask(cv::Rect(1, 1, source.cols, source.rows)).clone();
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(region, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
  if (contours.empty()) {
    m_magicWandStatus = "MAGIC_WAND_BOUNDARY_FAILED";
    reason = "no external boundary could be extracted from seed region";
    return false;
  }

  const auto largest = std::max_element(
      contours.begin(), contours.end(), [](const std::vector<cv::Point> &a,
                                            const std::vector<cv::Point> &b) {
        return std::abs(cv::contourArea(a)) < std::abs(cv::contourArea(b));
      });
  std::string formFitReason;
  if (!BuildMagicWandFormFitNodes(*largest, formFitReason)) {
    m_magicWandStatus = "MAGIC_WAND_NODEIZATION_FAILED";
    reason = formFitReason;
    return false;
  }
  m_magicWandSeed = {static_cast<float>(seedX), static_cast<float>(seedY)};
  const cv::Point boundaryAnchor = SelectBoundaryAnchor(
      *largest, cv::Point(seedX, seedY), source.size(), m_boundaryAnchorMode);
  m_boundaryAnchor = {static_cast<float>(boundaryAnchor.x),
                      static_cast<float>(boundaryAnchor.y)};
  m_magicWandRegionPixels = regionPixels;
  m_magicWandStatus = "MAGIC_WAND_PREVIEW_READY";
  reason = "local seed boundary preview: region_pixels=" +
           std::to_string(regionPixels) + ", formfit_nodes=" +
           std::to_string(m_magicWandPreviewPoints.size()) + ", " +
           formFitReason + ", algorithm=" +
           std::string(fixedRange ? "color_fixed_range_v1"
                                  : "color_connected_range_v1") +
           ", tolerance=" + std::to_string(tolerance) +
           ", connectivity=" + std::to_string(connectivity);
  reason += ", boundary_anchor=" +
            std::string(BoundaryAnchorModeKey(m_boundaryAnchorMode)) + "(" +
            std::to_string(boundaryAnchor.x) + "," +
            std::to_string(boundaryAnchor.y) + ")";
  return true;
}

bool ViewController::BuildSegmentationPromptBoundaryPreview(
    std::string &reason) {
  ClearMagicWandPreview("PROMPT_BOUNDARY_RUNNING");
  if (m_imageViewImage.empty()) {
    m_magicWandStatus = "PROMPT_BOUNDARY_NO_IMAGE";
    reason = "no local image is loaded";
    return false;
  }

  std::string syncReason;
  if (!SyncFindSegmentationPromptListsFromShapeElements(syncReason)) {
    m_magicWandStatus = "PROMPT_BOUNDARY_PROMPT_SYNC_FAILED";
    reason = syncReason;
    return false;
  }
  const ManualGaugeState &gauge = m_manualTest.current_gauge;
  if (gauge.segmentation_positive_points.empty()) {
    m_magicWandStatus = "PROMPT_BOUNDARY_NEEDS_FOREGROUND";
    reason = "add at least one Seg + Point (foreground) before running Prompt Boundary";
    return false;
  }

  cv::Mat source;
  if (m_imageViewImage.channels() == 1) {
    cv::cvtColor(m_imageViewImage, source, cv::COLOR_GRAY2BGR);
  } else if (m_imageViewImage.channels() == 4) {
    cv::cvtColor(m_imageViewImage, source, cv::COLOR_BGRA2BGR);
  } else if (m_imageViewImage.channels() == 3) {
    source = m_imageViewImage;
  } else {
    m_magicWandStatus = "PROMPT_BOUNDARY_UNSUPPORTED_IMAGE";
    reason = "image channel format is not supported";
    return false;
  }

  // Auto Boundary is deliberately point-prompt only.  A rectangle is not a
  // segmentation input or output for this tool: Seg +/- points establish the
  // local foreground/background priors and inference returns a contour.
  cv::Mat mask(source.rows, source.cols, CV_8UC1, cv::Scalar(cv::GC_PR_BGD));

  const auto markPromptPoints = [&](const std::vector<ManualSegmentationPromptPoint> &points,
                                    int label) {
    for (const ManualSegmentationPromptPoint &point : points) {
      if (point.x < 0 || point.y < 0 || point.x >= source.cols ||
          point.y >= source.rows)
        continue;
      const cv::Point seed(point.x, point.y);
      if (label == cv::GC_FGD) {
        cv::circle(mask, seed, 12, cv::Scalar(cv::GC_PR_FGD), -1, cv::LINE_8);
        cv::circle(mask, seed, 4, cv::Scalar(cv::GC_FGD), -1, cv::LINE_8);
      } else {
        cv::circle(mask, seed, 12, cv::Scalar(cv::GC_BGD), -1, cv::LINE_8);
      }
    }
  };
  markPromptPoints(gauge.segmentation_positive_points, cv::GC_FGD);
  markPromptPoints(gauge.segmentation_negative_points, cv::GC_BGD);

  cv::Mat bgdModel;
  cv::Mat fgdModel;
  try {
    cv::grabCut(source, mask, cv::Rect(), bgdModel, fgdModel,
                std::clamp(m_promptBoundaryGrabCutIterations, 1, 10),
                cv::GC_INIT_WITH_MASK);
  } catch (const cv::Exception &exception) {
    m_magicWandStatus = "PROMPT_BOUNDARY_GRABCUT_FAILED";
    reason = "local Prompt Boundary failed: " + std::string(exception.what());
    return false;
  }

  cv::Mat foreground =
      (mask == cv::GC_FGD) | (mask == cv::GC_PR_FGD);
  foreground.convertTo(foreground, CV_8UC1, 255.0);
  cv::morphologyEx(foreground, foreground, cv::MORPH_CLOSE,
                   cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                             cv::Size(3, 3)));
  const cv::Point anchor(gauge.segmentation_positive_points.front().x,
                         gauge.segmentation_positive_points.front().y);
  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(foreground, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_NONE);
  std::string inferencePath = "grabcut_point_prompt";
  if (contours.empty()) {
    // GrabCut can legitimately leave only the hard foreground core when the
    // image has very little colour separation.  In that case retain the same
    // point-only contract and build a connected seed region.  A candidate
    // which reaches any Seg - point is rejected, so a negative prompt remains
    // a hard exclusion rather than a decorative annotation.
    cv::Mat fallbackRegion = cv::Mat::zeros(source.size(), CV_8UC1);
    int bestPixels = 0;
    const int tolerance = std::clamp(m_magicWandColorTolerance, 0, 255);
    const int connectivity = m_magicWandEightConnected ? 8 : 4;
    for (const ManualSegmentationPromptPoint &positive :
         gauge.segmentation_positive_points) {
      if (positive.x < 0 || positive.y < 0 || positive.x >= source.cols ||
          positive.y >= source.rows)
        continue;
      cv::Mat work = source.clone();
      cv::Mat floodMask = cv::Mat::zeros(source.rows + 2, source.cols + 2,
                                         CV_8UC1);
      const int flags = connectivity | cv::FLOODFILL_MASK_ONLY |
                        cv::FLOODFILL_FIXED_RANGE | (255 << 8);
      const int pixels = cv::floodFill(
          work, floodMask, cv::Point(positive.x, positive.y), cv::Scalar(),
          nullptr, cv::Scalar(tolerance, tolerance, tolerance),
          cv::Scalar(tolerance, tolerance, tolerance), flags);
      cv::Mat candidate =
          floodMask(cv::Rect(1, 1, source.cols, source.rows)).clone();
      bool reachesNegative = false;
      for (const ManualSegmentationPromptPoint &negative :
           gauge.segmentation_negative_points) {
        if (negative.x >= 0 && negative.y >= 0 && negative.x < candidate.cols &&
            negative.y < candidate.rows &&
            candidate.at<unsigned char>(negative.y, negative.x) != 0) {
          reachesNegative = true;
          break;
        }
      }
      if (!reachesNegative && pixels > bestPixels) {
        fallbackRegion = std::move(candidate);
        bestPixels = pixels;
      }
    }
    if (bestPixels > 0) {
      foreground = fallbackRegion;
      cv::findContours(foreground, contours, cv::RETR_EXTERNAL,
                       cv::CHAIN_APPROX_NONE);
      inferencePath = "point_seeded_connected_region_fallback";
    }
  }
  if (contours.empty()) {
    m_magicWandStatus = "PROMPT_BOUNDARY_EMPTY";
    reason = "Seg +/- inference produced no valid contour: place Seg + inside the target and Seg - on the adjacent background";
    return false;
  }

  const auto best = std::max_element(
      contours.begin(), contours.end(), [&anchor](const std::vector<cv::Point> &left,
                                                   const std::vector<cv::Point> &right) {
        const bool leftContains = cv::pointPolygonTest(left, anchor, false) >= 0.0;
        const bool rightContains = cv::pointPolygonTest(right, anchor, false) >= 0.0;
        if (leftContains != rightContains)
          return !leftContains;
        return std::abs(cv::contourArea(left)) < std::abs(cv::contourArea(right));
      });
  std::string nodeReason;
  if (!BuildMagicWandFormFitNodes(*best, nodeReason)) {
    m_magicWandStatus = "PROMPT_BOUNDARY_NODEIZATION_FAILED";
    reason = nodeReason;
    return false;
  }

  m_magicWandSeed = {static_cast<float>(anchor.x), static_cast<float>(anchor.y)};
  const cv::Point boundaryAnchor = SelectBoundaryAnchor(
      *best, anchor, source.size(), m_boundaryAnchorMode);
  m_boundaryAnchor = {static_cast<float>(boundaryAnchor.x),
                      static_cast<float>(boundaryAnchor.y)};
  m_magicWandRegionPixels = cv::countNonZero(foreground);
  m_magicWandStatus = "PROMPT_BOUNDARY_PREVIEW_READY";
  reason = "local Prompt Boundary preview: path=" + inferencePath +
           ", foreground_points=" +
           std::to_string(gauge.segmentation_positive_points.size()) +
           ", background_points=" +
           std::to_string(gauge.segmentation_negative_points.size()) +
           ", " + nodeReason + ", boundary_anchor=" +
           std::string(BoundaryAnchorModeKey(m_boundaryAnchorMode)) + "(" +
           std::to_string(boundaryAnchor.x) + "," +
           std::to_string(boundaryAnchor.y) + ")";
  return true;
}

bool ViewController::CommitSegmentationPromptBoundary(CxImagePointerResult &out) {
  const AnnotationToolDefinition *tool = m_annotationLayer.ActiveTool();
  if (tool == nullptr || tool->action != "auto_segmentation") {
    out.status = "failed";
    out.reason = "Prompt Boundary is not the active annotation tool";
    return false;
  }
  if (m_magicWandStatus != "PROMPT_BOUNDARY_PREVIEW_READY" ||
      m_magicWandPreviewPoints.size() < 3) {
    out.status = "failed";
    out.reason = "no Prompt Boundary preview is ready to accept";
    return false;
  }

  auto shape = std::make_unique<PolylineShape>();
  for (const CxShapePoint &point : m_magicWandPreviewPoints)
    shape->addPoint(point.x, point.y);
  shape->close(true);
  CxShapeElement &element =
      m_annotationLayer.CreateFromTool(*tool, std::move(shape));
  m_annotationLayer.SelectShape(
      static_cast<int>(m_annotationLayer.ShapeElements().size()) - 1);
  out.consumed = true;
  out.phase = "prompt_boundary_accept";
  out.status = "created";
  out.created_ref = element.stable_ref;
  out.reason = "Prompt Boundary accepted as editable closed polyline";

  std::ostringstream trace;
  trace << "ref=" << out.created_ref << " seed=(" << m_magicWandSeed.x
        << "," << m_magicWandSeed.y << ") boundary_anchor="
        << BoundaryAnchorModeKey(m_boundaryAnchorMode) << "("
        << m_boundaryAnchor.x << "," << m_boundaryAnchor.y
        << ") foreground_points="
        << m_manualTest.current_gauge.segmentation_positive_points.size()
        << " background_points="
        << m_manualTest.current_gauge.segmentation_negative_points.size()
        << " grabcut_iterations=" << m_promptBoundaryGrabCutIterations
        << " raw_boundary_points=" << m_magicWandRawBoundaryPointCount
        << " node_count=" << m_magicWandFormFitNodeCount;
  RecordManualOperationTraceEvent(m_manualTest, "prompt_boundary_accept",
                                  "created", trace.str());
  std::string receiptPath;
  std::string receiptReason;
  if (!WriteBusinessAnnotationReceipt(element, *tool, "accept_boundary",
                                      "accepted", trace.str(), receiptPath,
                                      receiptReason)) {
    out.reason += "; local receipt unavailable: " + receiptReason;
  } else {
    out.reason += "; local receipt=" + receiptPath;
  }
  ClearMagicWandPreview("PROMPT_BOUNDARY_ACCEPTED");
  return true;
}

bool ViewController::BuildOpenBoundaryFormFit(
    const std::vector<CxShapePoint> &points, std::string &reason) {
  m_openBoundaryRawPoints = points;
  m_openBoundaryPreviewPoints.clear();
  m_openBoundaryStatus = "OPEN_BOUNDARY_FIT_RUNNING";
  if (points.size() < 2) {
    m_openBoundaryStatus = "OPEN_BOUNDARY_NEEDS_TWO_POINTS";
    reason = "place at least two ordered points on the physical boundary";
    return false;
  }
  std::vector<cv::Point2f> input;
  input.reserve(points.size());
  for (const CxShapePoint &point : points)
    input.emplace_back(static_cast<float>(point.x), static_cast<float>(point.y));

  cv::Vec4f line;
  cv::fitLine(input, line, cv::DIST_L2, 0.0, 0.01, 0.01);
  double lineSquared = 0.0;
  for (const cv::Point2f &point : input) {
    const double dx = point.x - line[2], dy = point.y - line[3];
    const double distance = std::abs(dx * line[1] - dy * line[0]);
    lineSquared += distance * distance;
  }
  m_openBoundaryLineResidualPx =
      std::sqrt(lineSquared / static_cast<double>(input.size()));

  cv::Point2f center;
  float radius = 0.0f;
  cv::minEnclosingCircle(input, center, radius);
  double arcSquared = 0.0;
  for (const cv::Point2f &point : input) {
    const double radial = cv::norm(point - center);
    const double distance = radial - radius;
    arcSquared += distance * distance;
  }
  m_openBoundaryArcResidualPx =
      std::sqrt(arcSquared / static_cast<double>(input.size()));

  if (m_openBoundaryLineResidualPx <= 1.5) {
    m_openBoundaryType = "line";
    m_openBoundaryFitResidualPx = m_openBoundaryLineResidualPx;
    m_openBoundaryPreviewPoints = {points.front(), points.back()};
  } else if (input.size() >= 3 && radius > 4.0f &&
             m_openBoundaryArcResidualPx < m_openBoundaryLineResidualPx * 0.8 &&
             m_openBoundaryArcResidualPx <= 3.0) {
    m_openBoundaryType = "arc";
    m_openBoundaryFitResidualPx = m_openBoundaryArcResidualPx;
    m_openBoundaryPreviewPoints = points;
  } else {
    m_openBoundaryType = "open_curve";
    m_openBoundaryFitResidualPx = m_openBoundaryLineResidualPx;
    std::vector<cv::Point> discrete;
    discrete.reserve(points.size());
    for (const CxShapePoint &point : points)
      discrete.emplace_back(static_cast<int>(std::lround(point.x)),
                            static_cast<int>(std::lround(point.y)));
    std::vector<cv::Point> simplified;
    cv::approxPolyDP(discrete, simplified,
                     std::max(0.5f, m_magicWandSimplifyPixels), false);
    for (const cv::Point &point : simplified)
      m_openBoundaryPreviewPoints.push_back(
          {static_cast<double>(point.x), static_cast<double>(point.y)});
    if (m_openBoundaryPreviewPoints.size() < 2)
      m_openBoundaryPreviewPoints = points;
  }
  m_openBoundaryStatus = "OPEN_BOUNDARY_FORMFIT_READY";
  reason = "boundary FormFit candidate=" + m_openBoundaryType +
           " line_rms_px=" + std::to_string(m_openBoundaryLineResidualPx) +
           " arc_rms_px=" + std::to_string(m_openBoundaryArcResidualPx) +
           " nodes=" + std::to_string(m_openBoundaryPreviewPoints.size());
  return true;
}

bool ViewController::CommitOpenBoundaryFormFit(CxImagePointerResult &out) {
  const AnnotationToolDefinition *tool = m_annotationLayer.ActiveTool();
  if (tool == nullptr || tool->action != "open_boundary_formfit" ||
      m_openBoundaryStatus != "OPEN_BOUNDARY_FORMFIT_READY" ||
      m_openBoundaryPreviewPoints.size() < 2) {
    out.status = "failed";
    out.reason = "no open Boundary Trace FormFit candidate is ready";
    return false;
  }
  auto shape = std::make_unique<PolylineShape>();
  for (const CxShapePoint &point : m_openBoundaryPreviewPoints)
    shape->addPoint(point.x, point.y);
  shape->close(false);
  CxShapeElement &element = m_annotationLayer.CreateFromTool(*tool, std::move(shape));
  m_annotationLayer.SelectShape(static_cast<int>(m_annotationLayer.ShapeElements().size()) - 1);
  out.consumed = true;
  out.phase = "open_boundary_formfit_accept";
  out.status = "created";
  out.created_ref = element.stable_ref;
  std::ostringstream trace;
  trace << "ref=" << out.created_ref << " boundary_role=physical_interface"
        << " candidate=" << m_openBoundaryType
        << " fit_rms_px=" << m_openBoundaryFitResidualPx
        << " input_points=" << m_openBoundaryRawPoints.size();
  std::string receiptPath, receiptReason;
  WriteBusinessAnnotationReceipt(element, *tool, "accept_boundary", "accepted",
                                 trace.str(), receiptPath, receiptReason);
  out.reason = "open physical boundary accepted as " + m_openBoundaryType;
  if (!receiptPath.empty()) out.reason += "; local receipt=" + receiptPath;
  m_openBoundaryStatus = "OPEN_BOUNDARY_ACCEPTED";
  m_openBoundaryRawPoints.clear();
  m_openBoundaryPreviewPoints.clear();
  return true;
}

bool ViewController::CommitMagicWandPreview(CxImagePointerResult &out) {
  const AnnotationToolDefinition *tool = m_annotationLayer.ActiveTool();
  if (tool == nullptr || tool->action != "magic_wand_boundary") {
    out.status = "failed";
    out.reason = "Magic Wand Boundary is not the active annotation tool";
    return false;
  }
  if (m_magicWandPreviewPoints.size() < 3) {
    out.status = "failed";
    out.reason = "no Magic Wand Boundary preview is ready to accept";
    return false;
  }
  if (m_magicWandNodeizationMode != 0) {
    out.status = "failed";
    out.reason =
        "raw contour nodeization is diagnostic-only; switch to Closed polygon FormFit before accepting";
    return false;
  }

  auto shape = std::make_unique<PolylineShape>();
  for (const CxShapePoint &point : m_magicWandPreviewPoints)
    shape->addPoint(point.x, point.y);
  shape->close(true);
  CxShapeElement &element =
      m_annotationLayer.CreateFromTool(*tool, std::move(shape));
  m_annotationLayer.SelectShape(
      static_cast<int>(m_annotationLayer.ShapeElements().size()) - 1);
  out.consumed = true;
  out.phase = "magic_wand_accept";
  out.status = "created";
  out.created_ref = element.stable_ref;
  out.reason = "Magic Wand Boundary preview accepted as editable boundary";

  std::ostringstream trace;
  trace << "ref=" << out.created_ref << " seed=(" << m_magicWandSeed.x
        << "," << m_magicWandSeed.y << ") algorithm="
        << (m_magicWandAlgorithm == 0 ? "color_fixed_range_v1"
                                      : "color_connected_range_v1")
        << " tolerance="
        << m_magicWandColorTolerance << " connectivity="
        << (m_magicWandEightConnected ? 8 : 4) << " simplify_px="
        << m_magicWandSimplifyPixels << " region_pixels="
        << m_magicWandRegionPixels << " raw_boundary_points="
        << m_magicWandRawBoundaryPointCount << " formfit_nodes="
        << m_magicWandFormFitNodeCount << " formfit_status="
        << m_magicWandFormFitStatus;
  RecordManualOperationTraceEvent(m_manualTest, "magic_wand_boundary_accept",
                                  "created", trace.str());
  CXLOG_INFO("ImageAnnotationUI", "magic_wand_boundary_accept", "created",
             trace.str());
  std::string receiptPath;
  std::string receiptReason;
  if (!WriteBusinessAnnotationReceipt(element, *tool, "accept_boundary",
                                      "accepted", trace.str(), receiptPath,
                                      receiptReason)) {
    out.reason += "; local receipt unavailable: " + receiptReason;
    CXLOG_ERROR("ImageAnnotationUI", "business_annotation_receipt", "failed",
                receiptReason);
  } else {
    out.reason += "; local receipt=" + receiptPath;
    CXLOG_INFO("ImageAnnotationUI", "business_annotation_receipt", "written",
               receiptPath);
  }
  ClearMagicWandPreview("MAGIC_WAND_ACCEPTED");
  return true;
}

bool ViewController::CommitDraftShapeFromTool(
    const AnnotationToolDefinition &tool, CxImagePointerResult &out) {
  const double dx = m_annotationDragEnd.x - m_annotationDragStart.x;
  const double dy = m_annotationDragEnd.y - m_annotationDragStart.y;

  if (tool.shape_type == "LineShape") {
    const double dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 2.0) {
      out.status = "draft_too_small";
      out.reason = "line length too small";
      return false;
    }
  } else if (tool.shape_type == "RectShape" ||
             tool.shape_type == "AutoBoundary") {
    const double w = std::abs(dx);
    const double h = std::abs(dy);
    if (w < 2.0 || h < 2.0) {
      out.status = "draft_too_small";
      out.reason = "rect width/height too small";
      return false;
    }
  } else if (tool.shape_type == "CircleShape") {
    const double r = std::sqrt(dx * dx + dy * dy);
    if (r < 2.0) {
      out.status = "draft_too_small";
      out.reason = "circle radius too small";
      return false;
    }
  } else if (tool.shape_type == "EllipseShape") {
    const double rx = std::abs(dx);
    const double ry = std::abs(dy);
    if (rx < 2.0 || ry < 2.0) {
      out.status = "draft_too_small";
      out.reason = "ellipse radius too small";
      return false;
    }
  }

  std::vector<CxShapePoint> pts = {
      {m_annotationDragStart.x, m_annotationDragStart.y},
      {m_annotationDragEnd.x, m_annotationDragEnd.y}};
  auto shape = CreateInitialShapeForTool(tool, pts);
  if (!shape) {
    out.status = "failed";
    out.reason = "CreateInitialShapeForTool failed";
    return false;
  }

  CxShapeElement &element =
      m_annotationLayer.CreateFromTool(tool, std::move(shape));
  out.consumed = true;
  out.phase = "create_shape";
  out.status = "created";
  out.created_ref = element.stable_ref;
  out.reason = "shape created from annotation tool";
  std::string receiptPath;
  std::string receiptReason;
  const std::string trace = "annotation_shape_created ref=" + out.created_ref +
                            " tool=" + tool.name;
  if (WriteBusinessAnnotationReceipt(element, tool, "create_shape", "accepted",
                                     trace, receiptPath, receiptReason)) {
    out.reason += "; local receipt=" + receiptPath;
  } else {
    out.reason += "; local receipt unavailable: " + receiptReason;
    CXLOG_ERROR("ImageAnnotationUI", "business_annotation_receipt", "failed",
                receiptReason);
  }
  CXLOG_INFO("ImageAnnotationUI", "annotation_shape_created", "created",
             "ref=" + out.created_ref);
  return true;
}

CxImagePointerResult ViewController::ProcessImageAnnotationPointerFrame(
    const CxImagePointerFrame &frame) {
  CxImagePointerResult out;

  const bool hasActiveAnnotationState =
      m_annotationLayer.HasActiveDrag() || m_annotationDragging;
  const bool pointerInsideActiveCanvas =
      frame.canvas_hovered && frame.inside_image;
  const bool logPointerBegin =
      (pointerInsideActiveCanvas && frame.HasInteractionEvent()) ||
      (hasActiveAnnotationState &&
       (frame.HasInteractionEvent() || frame.HasDragMoveEvent()));

  if (logPointerBegin) {
    CXLOG_INFO("ImageAnnotationUI", "annotation_pointer_begin", "running",
               "x=" + std::to_string(frame.image_x) +
                   ", y=" + std::to_string(frame.image_y) + ", inside=" +
                   std::string(frame.inside_image ? "true" : "false"));
  }

  if (frame.escape_pressed) {
    m_annotationDragging = false;
    m_activePolylinePoints.clear();
    ClearMagicWandPreview("MAGIC_WAND_CANCELLED");
    m_annotationLayer.CancelDrag();
    out.consumed = true;
    out.phase = "cancel";
    out.status = "cancelled";
    m_lastPointerResult = out;
    CXLOG_INFO("ImageAnnotationUI", "annotation_cancel", "cancelled",
               "reason=escape");
    return out;
  }

  if (m_annotationLayer.HasActiveDrag()) {
    out.consumed = true;
    out.phase = "drag_existing";

    if (frame.left_down) {
      const bool ok =
          m_annotationLayer.UpdateDrag(frame.image_x, frame.image_y);
      out.status = ok ? "dragging" : "failed";
      out.reason = ok ? "drag updated" : "UpdateDrag failed";
      m_lastPointerResult = out;
      return out;
    }

    if (frame.left_released) {
      CXLOG_INFO("ImageAnnotationUI", "annotation_drag_release", "running",
                 "phase=before_commit");
      CxShapeCommitResult commit;
      const bool ok = m_annotationLayer.CommitEdit(commit);
      out.commit = commit;
      out.status = ok ? "committed" : "failed";
      out.reason = commit.reason;
      CXLOG_INFO("ImageAnnotationUI", "annotation_drag_release",
                 ok ? "committed" : "failed",
                 "phase=after_commit, owner=" + commit.owner_type +
                     ", binding=" + commit.owner_binding + ", ref=" +
                     commit.stable_ref + ", reason=" + commit.reason);

      const bool deferRuntimeToolWriteback =
          ok && commit.editable && !commit.owner_binding.empty() &&
          (commit.owner_type == "FindLine" ||
           commit.owner_type == "FindCircle" ||
           commit.owner_type == "FindEllipse" ||
           commit.owner_type == "FindRect" ||
           commit.owner_type == "GridPatternClassTool");

      if (deferRuntimeToolWriteback) {
        commit.runtime_writeback = false;
        out.reason = commit.owner_type +
                     " ROI edit accepted; exported to globals; "
                     "runtime object update deferred until next Run";
      } else if (ok && commit.owner_type == "FastMatch" && commit.editable &&
                 !commit.owner_binding.empty()) {
        void *toolObj =
            m_parserDebugBridge.QueryClassObject("FastMatch", commit.owner_ref);
        if (toolObj != nullptr) {
          FastMatch *tool = static_cast<FastMatch *>(toolObj);
          auto *layer = &m_annotationLayer;
          const auto &elements = layer->ShapeElements();
          for (const auto &elem : elements) {
            if (elem.stable_ref == commit.stable_ref && elem.shape != nullptr &&
                elem.shape->kind() == CxShapeKind::Rect) {
              auto *rectShape = dynamic_cast<RectShape *>(elem.shape.get());
              if (rectShape != nullptr) {
                const double x0 = rectShape->x0();
                const double y0 = rectShape->y0();
                const double x1 = rectShape->x1();
                const double y1 = rectShape->y1();
                std::string reason;
                if (tool->ApplyDisplayShapeEdit(commit.owner_binding,
                                                commit.semantic_role, x0, y0,
                                                x1, y1, reason)) {
                  commit.runtime_writeback = true;
                  out.reason = reason;
                } else {
                  out.reason = "FastMatch edit rejected: " + reason;
                }
                break;
              }
            }
          }
        }
      }
      if (ok) {
        const CxShapeElement *edited =
            m_annotationLayer.FindShapeByStableRef(commit.stable_ref);
        if (edited != nullptr) {
          CXLOG_INFO("ImageAnnotationUI", "annotation_drag_export", "running",
                     "ref=" + commit.stable_ref +
                         ", owner=" + commit.owner_type);
          UpdateManualGaugeFromShapeElement(m_manualTest, *edited);
          std::string exportReason;
          const bool globalsExported = ExportShapeElementToRuntimeGlobals(
              m_manualTest, *edited, exportReason);
          std::string promptSyncReason;
          if (edited->owner_type == "FindSegmentation") {
            SyncFindSegmentationPromptListsFromShapeElements(promptSyncReason);
          }
          const bool gaugeApplied =
              globalsExported && ApplyManualGaugeToGlobals(m_manualTest);
          if (!gaugeApplied) {
            exportReason += "; gauge/global normalization failed: " +
                            m_manualTest.debug_reason;
          } else {
            m_manualTest.debug_status = "shape_drag_exported";
            m_manualTest.debug_reason = exportReason;

            RecordManualOperationTraceEvent(
                m_manualTest, "annotation_shape_drag_commit",
                gaugeApplied ? "exported" : "export_failed",
                "ref=" + commit.stable_ref + " owner=" + commit.owner_type +
                    " binding=" + commit.owner_binding + " reason=" +
                    exportReason);
          }
          CXLOG_INFO("ImageAnnotationUI", "annotation_drag_export", "finished",
                     "ref=" + commit.stable_ref +
                         ", globals=" + (gaugeApplied ? "applied" : "failed") +
                         ", reason=" + exportReason);
        }
      }

      if (ok) {
        CXLOG_INFO("ImageAnnotationUI", "annotation_drag_commit", "committed",
                   "reason=" + out.reason);
      }
      m_lastPointerResult = out;
      return out;
    }

    m_lastPointerResult = out;
    return out;
  }

  if (m_annotationDragging) {
    out.consumed = true;
    out.phase = "create_shape";

    if (frame.left_down) {
      double clamped_x = frame.image_x;
      double clamped_y = frame.image_y;
      ClampImagePointToImageBounds(clamped_x, clamped_y);
      m_annotationDragEnd = {(float)clamped_x, (float)clamped_y};
      out.status = "draft_updating";
      out.reason = "draft updating";
      m_lastPointerResult = out;
      return out;
    }

    if (frame.left_released) {
      // A fast pointer drag can be observed as click -> release without an
      // intermediate frame where left_down is true.  Always capture the
      // release position before committing so GUI automation and quick human
      // gestures do not collapse a valid draft to a zero-sized shape.
      double clamped_x = frame.image_x;
      double clamped_y = frame.image_y;
      ClampImagePointToImageBounds(clamped_x, clamped_y);
      m_annotationDragEnd = {(float)clamped_x, (float)clamped_y};
      const AnnotationToolDefinition *activeTool =
          m_annotationLayer.ActiveTool();
      if (activeTool) {
        CommitDraftShapeFromTool(*activeTool, out);
      } else {
        out.status = "failed";
        out.reason = "no active tool";
      }
      m_annotationDragging = false;
      m_lastPointerResult = out;
      return out;
    }

    m_lastPointerResult = out;
    return out;
  }

  if (!frame.canvas_hovered || !frame.inside_image) {
    out.phase = "pointer_reject";
    out.status = "ignored";
    out.reason = "outside image canvas";
    if (frame.left_released && hasActiveAnnotationState) {
      m_annotationDragging = false;
      m_annotationLayer.CancelDrag();
      out.status = "cancelled";
      out.reason = "outside image canvas release; annotation state cleared";
    }
    m_lastPointerResult = out;
    if (hasActiveAnnotationState && frame.HasInteractionEvent()) {
      CXLOG_INFO("ImageAnnotationUI", "annotation_pointer_reject", out.status,
                 "reason=" + out.reason);
    }
    return out;
  }

  const AnnotationToolDefinition *activeTool = m_annotationLayer.ActiveTool();
  const bool hasActiveTool = activeTool != nullptr;
  const bool inCreateMode = IsAnnotationCreateModeActive();

  if (!inCreateMode) {
    if (frame.left_clicked) {
      const double imageTolerance =
          std::clamp(10.0 / std::max(0.05f, m_imageViewZoom), 3.0, 80.0);
      CxShapeHitResult hit = m_annotationLayer.HitTest(
          frame.image_x, frame.image_y, imageTolerance);

      if (hit.hit) {
        m_annotationLayer.SelectShape(hit.element_index);
        const bool dragStarted =
            m_annotationLayer.BeginDrag(hit, frame.image_x, frame.image_y);

        const CxShapeElement *selected = m_annotationLayer.SelectedShape();
        out.consumed = true;
        out.phase = "select_existing";
        out.status = dragStarted ? "selected" : "failed";
        out.selected_ref = selected ? selected->stable_ref : "";
        out.reason = dragStarted ? "shape selected and drag started"
                                 : "shape selected but BeginDrag failed";
        if (dragStarted)
          CXLOG_INFO("ImageAnnotationUI", "annotation_drag_begin", "running",
                     "ref=" + out.selected_ref);
        m_lastPointerResult = out;
        return out;
      }
    }

    out.phase = "pointer_pan";
    out.status = "not_consumed";
    m_lastPointerResult = out;
    return out;
  }

  if (!hasActiveTool) {
    out.consumed = true;
    out.phase = "create_shape";
    out.status = "failed";
    out.reason = "annotation tool enabled but ActiveTool is null";
    m_lastPointerResult = out;
    CXLOG_INFO("ImageAnnotationUI", "annotation_tool_missing", "failed",
               "reason=" + out.reason);
    return out;
  }

  OverlayKind currentKind = activeTool->kind;

  if (activeTool->action == "magic_wand_boundary") {
    if (frame.left_clicked) {
      std::string reason;
      const bool ok = BuildMagicWandPreview(frame.image_x, frame.image_y, reason);
      out.consumed = true;
      out.phase = "magic_wand_preview";
      out.status = ok ? "preview_ready" : "failed";
      out.reason = reason;
      if (ok) {
        std::ostringstream trace;
        trace << "seed=(" << m_magicWandSeed.x << "," << m_magicWandSeed.y
              << ") boundary_anchor="
              << BoundaryAnchorModeKey(m_boundaryAnchorMode) << "("
              << m_boundaryAnchor.x << "," << m_boundaryAnchor.y
              << ") algorithm="
              << (m_magicWandAlgorithm == 0 ? "color_fixed_range_v1"
                                            : "color_connected_range_v1")
              << " tolerance=" << m_magicWandColorTolerance
              << " connectivity=" << (m_magicWandEightConnected ? 8 : 4)
              << " region_pixels=" << m_magicWandRegionPixels
              << " raw_boundary_points=" << m_magicWandRawBoundaryPointCount
              << " formfit_nodes=" << m_magicWandFormFitNodeCount
              << " formfit_status=" << m_magicWandFormFitStatus;
        RecordManualOperationTraceEvent(m_manualTest,
                                        "magic_wand_boundary_preview",
                                        "preview_ready", trace.str());
        CXLOG_INFO("ImageAnnotationUI", "magic_wand_boundary_preview",
                   "preview_ready", trace.str());
      }
      m_lastPointerResult = out;
      return out;
    }
    m_lastPointerResult = out;
    return out;
  }

  if (currentKind == OverlayKind::Point) {
    if (frame.left_clicked) {
      if (hasActiveTool) {
        const AnnotationToolDefinition *tool = activeTool;
        if (!tool) {
          out.consumed = true;
          out.phase = "create_shape";
          out.status = "failed";
          out.reason = "active tool definition unavailable";
          m_lastPointerResult = out;
          return out;
        }

        std::vector<CxShapePoint> pts = {{frame.image_x, frame.image_y}};
        auto shape = CreateInitialShapeForTool(*tool, pts);
        if (shape) {
          CxShapeElement &element =
              m_annotationLayer.CreateFromTool(*tool, std::move(shape));
          out.consumed = true;
          out.phase = "create_shape";
          out.status = "created";
          out.created_ref = element.stable_ref;
          out.reason = "point created";
          std::string receiptPath;
          std::string receiptReason;
          const std::string trace =
              "annotation_point_created ref=" + out.created_ref +
              " tool=" + tool->name;
          if (WriteBusinessAnnotationReceipt(element, *tool, "create_shape",
                                             "accepted", trace, receiptPath,
                                             receiptReason)) {
            out.reason += "; local receipt=" + receiptPath;
          } else {
            out.reason += "; local receipt unavailable: " + receiptReason;
            CXLOG_ERROR("ImageAnnotationUI", "business_annotation_receipt",
                        "failed", receiptReason);
          }
          std::string exportReason;
          if (ExportShapeElementToRuntimeGlobals(m_manualTest, element,
                                                 exportReason)) {
            std::string promptSyncReason;
            if (element.owner_type == "FindSegmentation") {
              SyncFindSegmentationPromptListsFromShapeElements(
                  promptSyncReason);
            }
            const bool gaugeApplied = ApplyManualGaugeToGlobals(m_manualTest);
            if (gaugeApplied) {
              out.reason += "; " + exportReason;
              m_manualTest.debug_status = "annotation_point_exported";
              m_manualTest.debug_reason = exportReason;
            } else {
              out.reason +=
                  "; export normalization failed: " + m_manualTest.debug_reason;
            }
          }
          CXLOG_INFO("ImageAnnotationUI", "annotation_shape_created", "created",
                     "ref=" + out.created_ref);
          m_lastPointerResult = out;
          return out;
        }

        out.consumed = true;
        out.phase = "create_shape";
        out.status = "failed";
        out.reason = "CreateInitialShapeForTool failed";
        m_lastPointerResult = out;
        return out;
      }
    }
  } else if (activeTool->action == "open_boundary_formfit") {
    if (frame.left_clicked) {
      m_activePolylinePoints.push_back({frame.image_x, frame.image_y});
      out.consumed = true;
      out.phase = "open_boundary_trace";
      out.status = "point_added";
      out.reason = "boundary trace point added; press Enter to fit";
      m_lastPointerResult = out;
      return out;
    }
    if (frame.enter_pressed) {
      std::string fitReason;
      const bool ok = BuildOpenBoundaryFormFit(m_activePolylinePoints, fitReason);
      out.consumed = true;
      out.phase = "open_boundary_formfit";
      out.status = ok ? "preview_ready" : "failed";
      out.reason = fitReason;
      m_activePolylinePoints.clear();
      m_lastPointerResult = out;
      return out;
    }
  } else if (currentKind == OverlayKind::Polyline) {
    if (frame.left_clicked) {
      m_activePolylinePoints.push_back({frame.image_x, frame.image_y});
      out.consumed = true;
      out.phase = "create_shape";
      out.status = "point_added";
      out.reason = "polyline point added";
      m_lastPointerResult = out;
      return out;
    }

    if (frame.right_clicked || frame.enter_pressed) {
      const int required_points = frame.right_clicked ? 3 : 2;
      if (static_cast<int>(m_activePolylinePoints.size()) < required_points) {
        out.consumed = true;
        out.phase = "create_shape";
        out.status = "polyline_too_small";
        out.reason = frame.right_clicked
                         ? "closed polyline requires at least three points"
                         : "polyline requires at least two points";
        m_lastPointerResult = out;
        return out;
      }

      const AnnotationToolDefinition *tool = activeTool;
      if (tool) {
        auto shape = CreateInitialShapeForTool(*tool, m_activePolylinePoints);
        if (shape) {
          if (shape->kind() == CxShapeKind::Polyline) {
            PolylineShape *polyline =
                dynamic_cast<PolylineShape *>(shape.get());
            if (polyline != nullptr)
              polyline->close(frame.right_clicked);
          }
          CxShapeElement &element =
              m_annotationLayer.CreateFromTool(*tool, std::move(shape));
          out.consumed = true;
          out.phase = "create_shape";
          out.status = "created";
          out.created_ref = element.stable_ref;
          out.reason = frame.right_clicked ? "polyline closed by right click"
                                           : "polyline created";
          std::string receiptPath;
          std::string receiptReason;
          const std::string trace =
              "annotation_polyline_created ref=" + out.created_ref +
              " tool=" + tool->name;
          if (WriteBusinessAnnotationReceipt(element, *tool, "create_shape",
                                             "accepted", trace, receiptPath,
                                             receiptReason)) {
            out.reason += "; local receipt=" + receiptPath;
          } else {
            out.reason += "; local receipt unavailable: " + receiptReason;
            CXLOG_ERROR("ImageAnnotationUI", "business_annotation_receipt",
                        "failed", receiptReason);
          }
          CXLOG_INFO("ImageAnnotationUI", "annotation_shape_created", "created",
                     "ref=" + out.created_ref);
          m_activePolylinePoints.clear();
          m_lastPointerResult = out;
          return out;
        }
      }
      out.consumed = true;
      out.phase = "create_shape";
      out.status = "failed";
      out.reason = "active tool definition unavailable";
      m_activePolylinePoints.clear();
      m_lastPointerResult = out;
      return out;
    }
  } else if (currentKind == OverlayKind::Line) {
    if (!m_annotationDragging) {
      if (frame.left_clicked) {
        m_annotationDragging = true;
        m_annotationDragKind = OverlayKind::Line;
        m_annotationDragStart = {(float)frame.image_x, (float)frame.image_y};
        m_annotationDragEnd = {(float)frame.image_x, (float)frame.image_y};
        out.consumed = true;
        out.phase = "create_shape";
        out.status = "draft_started";
        out.reason = "line draft started";
        m_lastPointerResult = out;
        return out;
      }
      m_lastPointerResult = out;
      return out;
    }

    if (frame.left_down) {
      m_annotationDragEnd = {(float)frame.image_x, (float)frame.image_y};
      out.consumed = true;
      out.phase = "create_shape";
      out.status = "draft_updating";
      out.reason = "line draft updating";
      m_lastPointerResult = out;
      return out;
    }

    if (frame.left_released) {
      const AnnotationToolDefinition *tool = activeTool;
      CXLOG_INFO("ImageAnnotationUI", "annotation_line_release", "debug",
                 "currentToolName=" + activeTool->name + ", tool=active" +
                     ", m_imageToolMode=" +
                     std::to_string(static_cast<int>(m_imageToolMode)));
      if (tool) {
        CommitDraftShapeFromTool(*tool, out);
      } else {
        out.status = "failed";
        out.reason = "active tool definition unavailable";
      }
      m_annotationDragging = false;
      m_lastPointerResult = out;
      return out;
    }
  } else if (currentKind == OverlayKind::Rect) {
    if (!m_annotationDragging) {
      if (frame.left_clicked) {
        m_annotationDragging = true;
        m_annotationDragKind = OverlayKind::Rect;
        m_annotationDragStart = {(float)frame.image_x, (float)frame.image_y};
        m_annotationDragEnd = {(float)frame.image_x, (float)frame.image_y};
        out.consumed = true;
        out.phase = "create_shape";
        out.status = "draft_started";
        out.reason = "rect draft started";
        m_lastPointerResult = out;
        return out;
      }
      m_lastPointerResult = out;
      return out;
    }

    if (frame.left_down) {
      m_annotationDragEnd = {(float)frame.image_x, (float)frame.image_y};
      out.consumed = true;
      out.phase = "create_shape";
      out.status = "draft_updating";
      out.reason = "rect draft updating";
      m_lastPointerResult = out;
      return out;
    }

    if (frame.left_released) {
      const AnnotationToolDefinition *tool = activeTool;
      if (tool) {
        CommitDraftShapeFromTool(*tool, out);
      } else {
        out.status = "failed";
        out.reason = "active tool definition unavailable";
      }
      m_annotationDragging = false;
      m_lastPointerResult = out;
      return out;
    }
  } else if (currentKind == OverlayKind::AutoBoundaryRequest) {
    out.consumed = frame.left_clicked || frame.left_down || frame.left_released;
    out.phase = "auto_boundary_point_prompt_only";
    out.status = "point_prompts_required";
    out.reason = "Auto Boundary does not draw a prompt rectangle; add Seg + / Seg - points and run point-prompt boundary inference";
    m_annotationDragging = false;
    m_lastPointerResult = out;
    return out;
  } else if (currentKind == OverlayKind::Circle) {
    if (!m_annotationDragging) {
      if (frame.left_clicked) {
        m_annotationDragging = true;
        m_annotationDragKind = OverlayKind::Circle;
        m_annotationDragStart = {(float)frame.image_x, (float)frame.image_y};
        m_annotationDragEnd = {(float)frame.image_x, (float)frame.image_y};
        out.consumed = true;
        out.phase = "create_shape";
        out.status = "draft_started";
        out.reason = "circle draft started";
        m_lastPointerResult = out;
        return out;
      }
      m_lastPointerResult = out;
      return out;
    }

    if (frame.left_down) {
      m_annotationDragEnd = {(float)frame.image_x, (float)frame.image_y};
      out.consumed = true;
      out.phase = "create_shape";
      out.status = "draft_updating";
      out.reason = "circle draft updating";
      m_lastPointerResult = out;
      return out;
    }

    if (frame.left_released) {
      const AnnotationToolDefinition *tool = activeTool;
      if (tool) {
        CommitDraftShapeFromTool(*tool, out);
      } else {
        out.status = "failed";
        out.reason = "active tool definition unavailable";
      }
      m_annotationDragging = false;
      m_lastPointerResult = out;
      return out;
    }
  } else if (currentKind == OverlayKind::Ellipse) {
    if (!m_annotationDragging) {
      if (frame.left_clicked) {
        m_annotationDragging = true;
        m_annotationDragKind = OverlayKind::Ellipse;
        m_annotationDragStart = {(float)frame.image_x, (float)frame.image_y};
        m_annotationDragEnd = {(float)frame.image_x, (float)frame.image_y};
        out.consumed = true;
        out.phase = "create_shape";
        out.status = "draft_started";
        out.reason = "ellipse draft started";
        m_lastPointerResult = out;
        return out;
      }
      m_lastPointerResult = out;
      return out;
    }

    if (frame.left_down) {
      m_annotationDragEnd = {(float)frame.image_x, (float)frame.image_y};
      out.consumed = true;
      out.phase = "create_shape";
      out.status = "draft_updating";
      out.reason = "ellipse draft updating";
      m_lastPointerResult = out;
      return out;
    }

    if (frame.left_released) {
      const AnnotationToolDefinition *tool = activeTool;
      if (tool) {
        CommitDraftShapeFromTool(*tool, out);
      } else {
        out.status = "failed";
        out.reason = "active tool definition unavailable";
      }
      m_annotationDragging = false;
      m_lastPointerResult = out;
      return out;
    }
  }

  m_lastPointerResult = out;
  return out;
}

void ViewController::drawImageEvidenceOnCanvas(bool canvasHovered,
                                               bool canvasActive,
                                               ImDrawList *drawList) {
  ImGuiIO &io = ImGui::GetIO();

  ImVec2 mouseScreen = io.MousePos;
  bool mouseInImageCanvas = false;
  ImVec2 imagePoint(-1.0f, -1.0f);
  bool insideImage = false;

  if (!m_imageViewImage.empty()) {
    imagePoint = ScreenToImage(mouseScreen.x, mouseScreen.y);
    insideImage = imagePoint.x >= 0.0f && imagePoint.y >= 0.0f &&
                  imagePoint.x < m_imageViewImage.cols &&
                  imagePoint.y < m_imageViewImage.rows;

    mouseInImageCanvas =
        mouseScreen.x >= m_annotationImagePosX &&
        mouseScreen.x <= m_annotationImagePosX + m_annotationImageWidth &&
        mouseScreen.y >= m_annotationImagePosY &&
        mouseScreen.y <= m_annotationImagePosY + m_annotationImageHeight;

    m_imageCanvasMin = ImVec2(m_annotationImagePosX, m_annotationImagePosY);
    m_imageCanvasMax = ImVec2(m_annotationImagePosX + m_annotationImageWidth,
                              m_annotationImagePosY + m_annotationImageHeight);
  }

  m_debugImageViewHovered = canvasHovered;
  m_debugMouseInImage = insideImage;
  m_debugMouseImageX = imagePoint.x;
  m_debugMouseImageY = imagePoint.y;
  m_debugAnnotationDragging = m_annotationDragging;

  CxImagePointerFrame pointerFrame;
  pointerFrame.canvas_hovered = canvasHovered;
  pointerFrame.inside_image = insideImage;
  pointerFrame.left_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
  pointerFrame.left_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
  pointerFrame.left_released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
  pointerFrame.right_clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Right);
  pointerFrame.escape_pressed = ImGui::IsKeyPressed(ImGuiKey_Escape);
  pointerFrame.enter_pressed = ImGui::IsKeyPressed(ImGuiKey_Enter);
  pointerFrame.pointer_moved =
      io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f;
  pointerFrame.image_x = imagePoint.x;
  pointerFrame.image_y = imagePoint.y;

  CxImagePointerResult pointerResult =
      ProcessImageAnnotationPointerFrame(pointerFrame);

  if (pointerResult.consumed) {
    m_blockOccMouseInputThisFrame = true;
    m_blockImagePanThisFrame = true;
    m_annotationStatus = pointerResult.status + ": " + pointerResult.reason;
  }

  if (!canvasActive && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    m_annotationDragging = false;

  if (m_annotationDragging && insideImage) {
    const ImVec2 start =
        ImageToScreen(m_annotationDragStart.x, m_annotationDragStart.y);
    const ImVec2 end =
        ImageToScreen(m_annotationDragEnd.x, m_annotationDragEnd.y);
    const ImU32 previewColor = IM_COL32(80, 220, 255, 220);
    if (m_annotationDragKind == OverlayKind::Line)
      drawList->AddLine(start, end, previewColor, 2.0f);
    else if (m_annotationDragKind == OverlayKind::Rect ||
             m_annotationDragKind == OverlayKind::AutoBoundaryRequest)
      drawList->AddRect(
          ImVec2(std::min(start.x, end.x), std::min(start.y, end.y)),
          ImVec2(std::max(start.x, end.x), std::max(start.y, end.y)),
          previewColor, 0.0f, 0, 2.0f);
    else if (m_annotationDragKind == OverlayKind::Circle) {
      const float dx = end.x - start.x;
      const float dy = end.y - start.y;
      drawList->AddCircle(start, std::sqrt(dx * dx + dy * dy), previewColor, 48,
                          2.0f);
    } else if (m_annotationDragKind == OverlayKind::Ellipse) {
      const float cx = (start.x + end.x) * 0.5f;
      const float cy = (start.y + end.y) * 0.5f;
      const float rx = std::abs(end.x - start.x) * 0.5f;
      const float ry = std::abs(end.y - start.y) * 0.5f;
      drawList->AddEllipse(ImVec2(cx, cy), ImVec2(rx, ry), previewColor, 0.0f,
                           48, 2.0f);
    }
  }

  if (!m_annotationDragging && IsAnnotationCreateModeActive() &&
      !m_activePolylinePoints.empty()) {
    const ImU32 previewColor = IM_COL32(80, 220, 255, 220);
    for (std::size_t i = 1; i < m_activePolylinePoints.size(); ++i) {
      const ImVec2 first =
          ImageToScreen(static_cast<float>(m_activePolylinePoints[i - 1].x),
                        static_cast<float>(m_activePolylinePoints[i - 1].y));
      const ImVec2 second =
          ImageToScreen(static_cast<float>(m_activePolylinePoints[i].x),
                        static_cast<float>(m_activePolylinePoints[i].y));
      drawList->AddLine(first, second, previewColor, 2.0f);
    }

    for (const CxShapePoint &p : m_activePolylinePoints) {
      const ImVec2 screen =
          ImageToScreen(static_cast<float>(p.x), static_cast<float>(p.y));
      drawList->AddCircleFilled(screen, 4.0f, previewColor);
      drawList->AddCircle(screen, 6.0f, IM_COL32(255, 255, 255, 220), 16, 1.0f);
    }

    if (insideImage) {
      const CxShapePoint &last = m_activePolylinePoints.back();
      const ImVec2 first =
          ImageToScreen(static_cast<float>(last.x), static_cast<float>(last.y));
      const ImVec2 second = ImageToScreen(imagePoint.x, imagePoint.y);
      drawList->AddLine(first, second, IM_COL32(80, 220, 255, 120), 1.0f);
    }
  }

  if (m_magicWandPreviewPoints.size() >= 3) {
    if (m_magicWandRawBoundaryPoints.size() >= 3) {
      std::vector<ImVec2> rawScreenPoints;
      rawScreenPoints.reserve(m_magicWandRawBoundaryPoints.size());
      for (const CxShapePoint &point : m_magicWandRawBoundaryPoints) {
        rawScreenPoints.push_back(
            ImageToScreen(static_cast<float>(point.x), static_cast<float>(point.y)));
      }
      drawList->AddPolyline(rawScreenPoints.data(),
                            static_cast<int>(rawScreenPoints.size()),
                            IM_COL32(130, 164, 205, 120), ImDrawFlags_Closed,
                            1.0f);
    }
    const ImU32 previewColor = IM_COL32(88, 232, 168, 245);
    std::vector<ImVec2> screenPoints;
    screenPoints.reserve(m_magicWandPreviewPoints.size());
    for (const CxShapePoint &point : m_magicWandPreviewPoints) {
      screenPoints.push_back(
          ImageToScreen(static_cast<float>(point.x), static_cast<float>(point.y)));
    }
    drawList->AddPolyline(screenPoints.data(), static_cast<int>(screenPoints.size()),
                          previewColor, ImDrawFlags_Closed, 2.5f);
    for (const ImVec2 &node : screenPoints)
      drawList->AddCircleFilled(node, 3.5f, IM_COL32(255, 224, 86, 245));
    const ImVec2 seed = ImageToScreen(m_magicWandSeed.x, m_magicWandSeed.y);
    drawList->AddCircleFilled(seed, 4.5f, previewColor);
    drawList->AddCircle(seed, 7.0f, IM_COL32(255, 255, 255, 240), 16, 1.5f);
    const ImVec2 anchor =
        ImageToScreen(m_boundaryAnchor.x, m_boundaryAnchor.y);
    drawList->AddCircleFilled(anchor, 4.5f, IM_COL32(255, 170, 60, 245));
    drawList->AddCircle(anchor, 7.0f, IM_COL32(255, 255, 255, 240), 16, 1.5f);
    const bool promptBoundary =
        m_magicWandStatus.rfind("PROMPT_BOUNDARY", 0) == 0;
    drawList->AddText(
        ImVec2(seed.x + 8.0f, seed.y - 16.0f), previewColor,
        promptBoundary ? "Prompt Boundary FormFit nodes - accept in Annotation Tools"
                       : "Magic Wand nodes - accept in Annotation Tools");
    drawList->AddText(ImVec2(anchor.x + 8.0f, anchor.y + 5.0f),
                      IM_COL32(255, 190, 90, 245),
                      BoundaryAnchorModeKey(m_boundaryAnchorMode));
  }

  if (m_openBoundaryPreviewPoints.size() >= 2) {
    std::vector<ImVec2> screenPoints;
    screenPoints.reserve(m_openBoundaryPreviewPoints.size());
    for (const CxShapePoint &point : m_openBoundaryPreviewPoints)
      screenPoints.push_back(ImageToScreen(static_cast<float>(point.x),
                                           static_cast<float>(point.y)));
    const ImU32 color = IM_COL32(255, 166, 68, 245);
    drawList->AddPolyline(screenPoints.data(), static_cast<int>(screenPoints.size()),
                          color, ImDrawFlags_None, 2.5f);
    for (const ImVec2 &point : screenPoints)
      drawList->AddCircleFilled(point, 3.5f, IM_COL32(255, 232, 110, 245));
    drawList->AddText(ImVec2(screenPoints.front().x + 8.0f,
                              screenPoints.front().y - 16.0f), color,
                      ("Boundary FormFit: " + m_openBoundaryType).c_str());
  }

  const AnnotationToolDefinition *activeAnnotationTool =
      m_annotationLayer.ActiveTool();
  const bool autoBoundaryPointPromptOnly =
      activeAnnotationTool != nullptr &&
      activeAnnotationTool->action == "auto_segmentation";
  for (int elementIndex = 0;
       elementIndex < static_cast<int>(m_annotationLayer.Elements().size());
       ++elementIndex) {
    OverlayElement &element = m_annotationLayer.Elements()[elementIndex];
    if (!element.visible || element.image_points.empty())
      continue;
    if (autoBoundaryPointPromptOnly &&
        (element.kind == OverlayKind::AutoBoundaryRequest ||
         element.semantic_role == "prompt_rect"))
      continue;
    const ImU32 color = element.selected ? IM_COL32(255, 220, 40, 255)
                                         : IM_COL32(255, 80, 180, 255);
    const float thickness = element.selected ? 3.0f : 2.0f;
    if (element.kind == OverlayKind::Point) {
      const ImVec2 point =
          ImageToScreen(element.image_points[0].x, element.image_points[0].y);
      drawList->AddLine(ImVec2(point.x - 8.0f, point.y),
                        ImVec2(point.x + 8.0f, point.y), color, thickness);
      drawList->AddLine(ImVec2(point.x, point.y - 8.0f),
                        ImVec2(point.x, point.y + 8.0f), color, thickness);
      drawList->AddCircleFilled(point, 3.0f, color);
    } else if ((element.kind == OverlayKind::Line ||
                element.kind == OverlayKind::Rect) &&
               element.image_points.size() >= 2) {
      const ImVec2 first =
          ImageToScreen(element.image_points[0].x, element.image_points[0].y);
      const ImVec2 second =
          ImageToScreen(element.image_points[1].x, element.image_points[1].y);
      if (element.kind == OverlayKind::Rect) {
        const ImVec2 minimum(std::min(first.x, second.x),
                             std::min(first.y, second.y));
        const ImVec2 maximum(std::max(first.x, second.x),
                             std::max(first.y, second.y));
        drawList->AddRect(minimum, maximum, color, 0.0f, 0, thickness);
      } else
        drawList->AddLine(first, second, color, thickness);
    } else if (element.kind == OverlayKind::Circle &&
               element.image_points.size() >= 2) {
      const ImVec2 center =
          ImageToScreen(element.image_points[0].x, element.image_points[0].y);
      const ImVec2 radiusPoint =
          ImageToScreen(element.image_points[1].x, element.image_points[1].y);
      const float dx = radiusPoint.x - center.x;
      const float dy = radiusPoint.y - center.y;
      const float radiusScreen = std::sqrt(dx * dx + dy * dy);
      drawList->AddCircle(center, radiusScreen, color, 96, thickness);
    } else if (element.kind == OverlayKind::Polyline) {
      for (std::size_t i = 1; i < element.image_points.size(); ++i) {
        const ImVec2 first = ImageToScreen(element.image_points[i - 1].x,
                                           element.image_points[i - 1].y);
        const ImVec2 second =
            ImageToScreen(element.image_points[i].x, element.image_points[i].y);
        drawList->AddLine(first, second, color, thickness);
      }
    } else if (element.kind == OverlayKind::AutoBoundaryRequest &&
               !element.image_points.empty()) {
      const ImVec2 p =
          ImageToScreen(element.image_points[0].x, element.image_points[0].y);
      drawList->AddCircleFilled(p, 6.0f, IM_COL32(255, 160, 40, 255));
      drawList->AddCircle(p, 9.0f, IM_COL32(255, 255, 255, 255), 24, 2.0f);
      drawList->AddText(ImVec2(p.x + 10.0f, p.y - 8.0f),
                        IM_COL32(255, 220, 80, 255), "Prompt box");
    } else if (element.kind == OverlayKind::BoundaryPolyline) {
      for (std::size_t i = 1; i < element.image_points.size(); ++i) {
        const ImVec2 first = ImageToScreen(element.image_points[i - 1].x,
                                           element.image_points[i - 1].y);
        const ImVec2 second =
            ImageToScreen(element.image_points[i].x, element.image_points[i].y);
        drawList->AddLine(first, second, IM_COL32(80, 255, 200, 255),
                          thickness);
      }
    }
    const ImVec2 sourceLabel =
        ImageToScreen(element.image_points[0].x, element.image_points[0].y);
    drawList->AddText(ImVec2(sourceLabel.x + 6.0f, sourceLabel.y + 6.0f), color,
                      element.source.empty() ? "manual_element"
                                             : element.source.c_str());
  }

  if (m_showSourcePreviewOverlay && !m_manualTest.line_views.empty() &&
      m_manualTest.current_line >= 0 &&
      m_manualTest.current_line <
          static_cast<int>(m_manualTest.line_views.size())) {
    const ScriptLineView &line =
        m_manualTest
            .line_views[static_cast<std::size_t>(m_manualTest.current_line)];
    std::vector<float> values;
    if (ParseNumericParameters(line.params, values)) {
      const ImU32 scriptColor = IM_COL32(255, 170, 40, 130);
      if (line.method == "setcircle" && values.size() >= 4) {
        const ImVec2 center = ImageToScreen(values[0], values[1]);
        const float scaleX = m_annotationImageWidth / m_imageViewImage.cols;
        const float scaleY = m_annotationImageHeight / m_imageViewImage.rows;
        const float radius = std::fabs(values[3]) * (scaleX + scaleY) * 0.5f;
        const int segments = 48;
        for (int segment = 0; segment < segments; segment += 2) {
          const float a0 = 6.2831853f * segment / segments;
          const float a1 = 6.2831853f * (segment + 1) / segments;
          drawList->AddLine(ImVec2(center.x + std::cos(a0) * radius,
                                   center.y + std::sin(a0) * radius),
                            ImVec2(center.x + std::cos(a1) * radius,
                                   center.y + std::sin(a1) * radius),
                            scriptColor, 2.0f);
        }
        drawList->AddText(ImVec2(center.x + 8.0f, center.y + 8.0f), scriptColor,
                          "source_preview / not_executed");
      }
    }
  }
}

void ViewController::drawImageEvidencePanels() {
  ImGuiIO &io = ImGui::GetIO();
  if (m_detachablePanels) {
    ImGui::SetNextWindowPos(ImVec2(350, 40), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_Once);
  } else {
    const float width = std::max(420.0f, io.DisplaySize.x * 0.24f);
    const float height = std::max(520.0f, io.DisplaySize.y - 80.0f);
    const float x = std::max(20.0f, io.DisplaySize.x - width - 20.0f);
    ImGui::SetNextWindowPos(ImVec2(x, 40.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
  }
  if (!ImGui::Begin("Image Evidence / Annotation Tools")) {
    ImGui::End();
    return;
  }

  ImGui::Text("Annotation Tool Manifest");
  ApplyAiGuiFocusHere(
      AiGuiDestination::ImageAnnotation,
      "Image Evidence / Annotation Tools > manifest path and tool palette");

  AnnotationInputText("Manifest path", m_annotationManifestPath);
  if (ImGui::Button("Load Tool Manifest")) {
    std::string reason;
    CxAnnotationToolManifestSnapshot snapshot;
    if (!m_parserOwner.ParseAnnotationToolManifest(m_annotationManifestPath,
                                                   snapshot, reason)) {
      m_annotationStatus = "parse failed: " + reason;
    } else if (!m_annotationLayer.ApplyToolManifestSnapshot(snapshot, reason)) {
      m_annotationStatus = "apply failed: " + reason;
    } else {
      m_annotationStatus = reason;
    }
  }
  ImGui::SameLine();
  ImGui::TextWrapped("%s", m_annotationStatus.c_str());
  ImGui::TextDisabled("CxScript direct logic: CxAnnotationTool_* declarations "
                      "-> CxAnnotationToolRuntime");
  ImGui::TextDisabled("cximage=pending_binding  torch=pending_binding  "
                      "mlpack=pending_binding  ensmallen=pending_binding");

  ImGui::Separator();
  ImGui::Columns(2, "annotation_columns", true);
  ImGui::Text("Tool Palette");

  auto drawPointerPanButton = [this]() {
    ImGui::PushID("Pointer / Pan");

    const bool active = !m_imageToolEnabled ||
                        m_imageToolMode == ImageToolMode::PointerPan ||
                        m_annotationLayer.ActiveToolIndex() < 0;

    if (active) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.85f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.25f, 0.65f, 0.95f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                            ImVec4(0.15f, 0.45f, 0.75f, 1.0f));
    }

    if (ImGui::Button("Pointer / Pan", ImVec2(-1.0f, 28.0f))) {
      m_imageToolEnabled = false;
      m_imageToolMode = ImageToolMode::PointerPan;
      CancelAnnotationCreate();
      ClearMagicWandPreview("MAGIC_WAND_IDLE");
      m_annotationLayer.SetActiveToolIndex(-1);
      m_annotationStatus = "annotation tool disabled";
    }

    if (active)
      ImGui::PopStyleColor(3);

    ImGui::PopID();
  };

  auto drawManifestToolButton = [this](int toolIndex,
                                       const AnnotationToolDefinition &tool) {
    ImGui::PushID(tool.name.c_str());

    const bool active =
        m_imageToolEnabled && m_annotationLayer.ActiveToolIndex() == toolIndex;

    if (active) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.85f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                            ImVec4(0.25f, 0.65f, 0.95f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                            ImVec4(0.15f, 0.45f, 0.75f, 1.0f));
    }

    const std::string label = tool.label.empty() ? tool.name : tool.label;
    if (ImGui::Button(label.c_str(), ImVec2(-1.0f, 28.0f))) {
      if (active) {
        if (tool.action == "auto_segmentation") {
          std::string previewReason;
          const bool previewReady =
              BuildSegmentationPromptBoundaryPreview(previewReason);
          m_annotationStatus = previewReady
              ? "Prompt Boundary preview ready: " + previewReason
              : "Prompt Boundary not run: " + previewReason;
          return;
        }
        m_imageToolEnabled = false;
        m_imageToolMode = ImageToolMode::PointerPan;
        CancelAnnotationCreate();
        m_annotationLayer.SetActiveToolIndex(-1);
        m_annotationStatus = "annotation tool disabled";
      } else {
        m_imageToolEnabled = true;
        m_imageToolMode = ToolModeFromAnnotationTool(tool);
        CancelAnnotationCreate();
        if (tool.action != "magic_wand_boundary" &&
            tool.action != "auto_segmentation")
          ClearMagicWandPreview("MAGIC_WAND_IDLE");
        m_annotationLayer.SetActiveToolIndex(toolIndex);
        m_annotationStatus = "enabled tool_id=" + tool.name +
                             " shape=" + tool.shape_type +
                             " role=" + tool.role + " action=" + tool.action;
        if (tool.action == "auto_segmentation") {
          std::string previewReason;
          const bool previewReady =
              BuildSegmentationPromptBoundaryPreview(previewReason);
          m_annotationStatus = previewReady
              ? "Prompt Boundary preview ready: " + previewReason
              : "Prompt Boundary not run: " + previewReason;
        }
      }
    }

    if (active)
      ImGui::PopStyleColor(3);

    ImGui::PopID();
  };

  ImGui::TextColored(ImVec4(1, 0.7f, 0.2f, 1),
                     "Annotation Tool Palette V2 ACTIVE");
  ImGui::TextUnformatted("Annotation Tools (from cxscript manifest)");
  drawPointerPanButton();
  for (int i = 0; i < static_cast<int>(m_annotationLayer.Tools().size()); ++i) {
    const AnnotationToolDefinition &tool = m_annotationLayer.Tools()[i];
    if (!tool.manual_visible)
      continue;
    drawManifestToolButton(i, tool);
  }

  const AnnotationToolDefinition *magicTool = m_annotationLayer.ActiveTool();
  if (magicTool != nullptr && magicTool->action == "magic_wand_boundary") {
    ImGui::Separator();
    ImGui::TextUnformatted("Magic Wand Boundary — local automatic boundary");
    ImGui::TextDisabled(
        "Click an object pixel to build a temporary boundary. Accept writes an editable polygon.");
    ImGui::TextDisabled("Default profile: magic_wand_default_v1");
    if (ImGui::Button("Restore Default Profile", ImVec2(-1.0f, 28.0f))) {
      m_magicWandAlgorithm = 0;
      m_magicWandColorTolerance = 24;
      m_magicWandEightConnected = true;
      m_magicWandSimplifyPixels = 1.5f;
      m_magicWandMinimumRegionPixels = 32;
      m_magicWandNodeizationMode = 0;
      m_magicWandMaximumNodes = 64;
      m_magicWandMinimumNodeSpacingPixels = 4.0f;
      m_boundaryAnchorMode = 0;
      ClearMagicWandPreview("MAGIC_WAND_DEFAULTS_RESTORED");
      m_annotationStatus = "Magic Wand default profile restored";
      RecordManualOperationTraceEvent(
          m_manualTest, "magic_wand_default_profile", "restored",
          "profile=magic_wand_default_v1 algorithm=color_fixed_range_v1 "
          "tolerance=24 connectivity=8 formfit_residual_px=1.5 "
          "formfit_mode=closed_polygon_formfit_v1 max_nodes=64 "
          "minimum_node_spacing_px=4 minimum_region_pixels=32 "
          "boundary_anchor_mode=custom");
    }
    bool parametersChanged = false;
    const char *algorithmOptions[] = {
        "Color fixed range (default)", "Connected color range"};
    parametersChanged |= ImGui::Combo("Algorithm", &m_magicWandAlgorithm,
                                      algorithmOptions, IM_ARRAYSIZE(algorithmOptions));
    parametersChanged |= ImGui::SliderInt("Color tolerance", &m_magicWandColorTolerance, 0, 128);
    parametersChanged |= ImGui::Checkbox("8-connected region", &m_magicWandEightConnected);
    ImGui::SeparatorText("Boundary keypoint selection");
    parametersChanged |= ImGui::Combo(
        "Boundary anchor mode", &m_boundaryAnchorMode,
        BoundaryAnchorModeLabels(), 9);
    ImGui::TextDisabled(
        "Custom keeps the click seed. Other modes select a reproducible point on the extracted contour.");
    ImGui::SeparatorText("FormFit geometry nodes");
    const char *nodeizationOptions[] = {
        "Closed polygon FormFit (default)", "Raw contour nodes (debug)"};
    parametersChanged |= ImGui::Combo("Nodeization mode", &m_magicWandNodeizationMode,
                                      nodeizationOptions, IM_ARRAYSIZE(nodeizationOptions));
    parametersChanged |= ImGui::SliderFloat("FormFit residual (px)", &m_magicWandSimplifyPixels,
                                             0.0f, 12.0f, "%.1f");
    parametersChanged |= ImGui::SliderInt("Maximum geometry nodes", &m_magicWandMaximumNodes,
                                           3, 256);
    parametersChanged |= ImGui::SliderFloat("Minimum node spacing (px)",
                                             &m_magicWandMinimumNodeSpacingPixels,
                                             0.0f, 32.0f, "%.1f");
    parametersChanged |= ImGui::InputInt("Minimum region pixels", &m_magicWandMinimumRegionPixels);
    m_magicWandMinimumRegionPixels =
        std::clamp(m_magicWandMinimumRegionPixels, 3, 1000000);
    if (parametersChanged && !m_magicWandPreviewPoints.empty()) {
      ClearMagicWandPreview("MAGIC_WAND_PREVIEW_STALE_PARAMETERS_CHANGED");
      m_annotationStatus = "Magic Wand preview discarded because its parameters changed";
    }
    ImGui::Text("Status: %s", m_magicWandStatus.c_str());
    ImGui::Text("FormFit: %s", m_magicWandFormFitStatus.c_str());
    ImGui::Text("Preview: raw boundary=%d | geometry nodes=%d | region pixels=%d",
                m_magicWandRawBoundaryPointCount, m_magicWandFormFitNodeCount,
                m_magicWandRegionPixels);

    const bool hasPreview = m_magicWandPreviewPoints.size() >= 3;
    const bool canAcceptPreview = hasPreview && m_magicWandNodeizationMode == 0;
    if (!canAcceptPreview)
      ImGui::BeginDisabled();
    if (ImGui::Button("Accept Boundary", ImVec2(-1.0f, 28.0f))) {
      CxImagePointerResult acceptResult;
      if (CommitMagicWandPreview(acceptResult)) {
        m_annotationStatus = acceptResult.status + ": " + acceptResult.reason;
        m_lastPointerResult = acceptResult;
      } else {
        m_annotationStatus = "failed: " + acceptResult.reason;
      }
    }
    if (!canAcceptPreview)
      ImGui::EndDisabled();
    if (hasPreview && m_magicWandNodeizationMode != 0)
      ImGui::TextDisabled(
          "Raw contour nodes are preview evidence only; switch to Closed polygon FormFit to accept.");
    if (ImGui::Button("Discard Preview", ImVec2(-1.0f, 28.0f))) {
      ClearMagicWandPreview("MAGIC_WAND_DISCARDED");
      m_annotationStatus = "Magic Wand Boundary preview discarded";
    }

    if (ImGui::CollapsingHeader("Magic Wand Algorithm Catalog")) {
      ImGui::TextWrapped(
          "color_fixed_range_v1 — AVAILABLE / default. Each candidate pixel is compared with the seed color; most stable for controlled material regions.");
      ImGui::TextWrapped(
          "color_connected_range_v1 — AVAILABLE. Neighbor-to-neighbor growth; useful for gradual shading, but can leak across weak boundaries.");
      ImGui::TextWrapped(
          "edge_assisted_snap_v1 — PLANNED. It is intentionally not selectable until edge evidence, parameter contract and regression case exist.");
      ImGui::TextDisabled(
          "This is classical local CV, not a remote model. No image or pixel leaves this workstation.");
    }
  }

  if (magicTool != nullptr && magicTool->action == "auto_segmentation") {
    ImGui::Separator();
    ImGui::TextUnformatted("Auto Boundary — Seg +/- point boundary inference");
    ImGui::TextDisabled(
        "1. Add Seg + foreground points. 2. Add Seg - background points as needed. 3. Run inference; the result is a boundary preview.");
    ImGui::TextDisabled(
        "No prompt rectangle is drawn or consumed by Auto Boundary. The only input is Seg +/- points; the only visual result is the inferred boundary.");
    ImGui::Text("Canvas prompts: Seg + = %d | Seg - = %d",
                static_cast<int>(m_manualTest.current_gauge.segmentation_positive_points.size()),
                static_cast<int>(m_manualTest.current_gauge.segmentation_negative_points.size()));
    if (ImGui::Button("Run Seg +/- Boundary Inference", ImVec2(-1.0f, 28.0f))) {
      std::string previewReason;
      const bool previewReady = BuildSegmentationPromptBoundaryPreview(previewReason);
      m_annotationStatus = previewReady
          ? "Seg +/- boundary preview ready: " + previewReason
          : "Seg +/- boundary inference not run: " + previewReason;
    }
    bool parametersChanged = false;
    parametersChanged |= ImGui::SliderInt("Local refinement iterations",
                                          &m_promptBoundaryGrabCutIterations,
                                          1, 10);
    ImGui::SeparatorText("Boundary keypoint selection");
    parametersChanged |= ImGui::Combo(
        "Boundary anchor mode", &m_boundaryAnchorMode,
        BoundaryAnchorModeLabels(), 9);
    ImGui::TextDisabled(
        "Seg +/- points drive inference; this mode chooses the recorded contour keypoint after extraction.");
    ImGui::SeparatorText("Boundary geometry nodes");
    parametersChanged |= ImGui::SliderFloat("Node residual (px)",
                                             &m_magicWandSimplifyPixels,
                                             0.0f, 12.0f, "%.1f");
    parametersChanged |= ImGui::SliderInt("Maximum geometry nodes",
                                           &m_magicWandMaximumNodes, 3, 256);
    parametersChanged |= ImGui::SliderFloat("Minimum node spacing (px)",
                                             &m_magicWandMinimumNodeSpacingPixels,
                                             0.0f, 32.0f, "%.1f");
    if (parametersChanged && !m_magicWandPreviewPoints.empty()) {
      ClearMagicWandPreview("PROMPT_BOUNDARY_PREVIEW_STALE_PARAMETERS_CHANGED");
      m_annotationStatus = "Prompt Boundary preview discarded because parameters changed";
    }
    ImGui::Text("Status: %s", m_magicWandStatus.c_str());
    ImGui::Text("Preview: raw boundary=%d | geometry nodes=%d | region pixels=%d",
                m_magicWandRawBoundaryPointCount, m_magicWandFormFitNodeCount,
                m_magicWandRegionPixels);
    const bool canAcceptPrompt =
        m_magicWandStatus == "PROMPT_BOUNDARY_PREVIEW_READY" &&
        m_magicWandPreviewPoints.size() >= 3;
    if (!canAcceptPrompt)
      ImGui::BeginDisabled();
    if (ImGui::Button("Accept Boundary", ImVec2(-1.0f, 28.0f))) {
      CxImagePointerResult acceptResult;
      if (CommitSegmentationPromptBoundary(acceptResult)) {
        m_annotationStatus = acceptResult.status + ": " + acceptResult.reason;
        m_lastPointerResult = acceptResult;
      } else {
        m_annotationStatus = "failed: " + acceptResult.reason;
      }
    }
    if (!canAcceptPrompt)
      ImGui::EndDisabled();
    if (ImGui::Button("Discard Preview", ImVec2(-1.0f, 28.0f))) {
      ClearMagicWandPreview("PROMPT_BOUNDARY_DISCARDED");
      m_annotationStatus = "Prompt Boundary preview discarded";
    }
    ImGui::TextDisabled(
        "Runs local point-prompt segmentation inference. A dedicated Torch prompt-segmentation provider may replace this backend without changing the point-only UI contract.");
  }

  if (magicTool != nullptr && magicTool->action == "open_boundary_formfit") {
    ImGui::Separator();
    ImGui::TextUnformatted("Boundary Trace FormFit — physical interface boundary");
    ImGui::TextDisabled(
        "Click ordered points on the real material/region/part boundary, then press Enter.");
    ImGui::TextDisabled(
        "This is not a line-like object label and does not create a closed mask.");
    ImGui::Text("Status: %s", m_openBoundaryStatus.c_str());
    ImGui::Text("Candidate: %s | line RMS=%.2f px | arc RMS=%.2f px",
                m_openBoundaryType.c_str(), m_openBoundaryLineResidualPx,
                m_openBoundaryArcResidualPx);
    const bool ready = m_openBoundaryStatus == "OPEN_BOUNDARY_FORMFIT_READY";
    if (!ready) ImGui::BeginDisabled();
    if (ImGui::Button("Accept Open Boundary", ImVec2(-1.0f, 28.0f))) {
      CxImagePointerResult acceptResult;
      if (CommitOpenBoundaryFormFit(acceptResult)) {
        m_annotationStatus = acceptResult.status + ": " + acceptResult.reason;
        m_lastPointerResult = acceptResult;
      } else m_annotationStatus = "failed: " + acceptResult.reason;
    }
    if (!ready) ImGui::EndDisabled();
    if (ImGui::Button("Discard Boundary Preview", ImVec2(-1.0f, 28.0f))) {
      m_openBoundaryPreviewPoints.clear();
      m_openBoundaryRawPoints.clear();
      m_openBoundaryStatus = "OPEN_BOUNDARY_DISCARDED";
      m_annotationStatus = "Boundary Trace FormFit preview discarded";
    }
  }

  ImGui::Separator();

  if (HasEditableFindCircleGauge()) {
    ImGui::TextUnformatted("FindCircle Gauge");
    ImGui::TextDisabled("Move Circle Center");
    ImGui::TextDisabled("Radius R");
    ImGui::TextDisabled("Inner Rin");
    ImGui::TextDisabled("Outer Rout");
  } else {
    ImGui::TextDisabled("FindCircle Gauge: not available");
  }

  ImGui::Separator();
  ImGui::Text("Tool enabled: %s", m_imageToolEnabled ? "YES" : "NO");
  ImGui::Text("Active mode: %s", ImageToolModeName(m_imageToolMode));
  const AnnotationToolDefinition *activeTool = m_annotationLayer.ActiveTool();
  if (activeTool != nullptr) {
    ImGui::TextWrapped(
        "Active tool id: %s | shape=%s | role=%s | action=%s | index=%d",
        activeTool->name.c_str(), activeTool->shape_type.c_str(),
        activeTool->role.c_str(), activeTool->action.c_str(),
        m_annotationLayer.ActiveToolIndex());
  } else {
    ImGui::TextDisabled("Active tool id: (none) | index=%d",
                        m_annotationLayer.ActiveToolIndex());
  }
  ImGui::Text("ShapeElements: %d",
              static_cast<int>(m_annotationLayer.ShapeElements().size()));
  ImGui::TextWrapped(
      "Last pointer: %s | %s | %s", m_lastPointerResult.phase.c_str(),
      m_lastPointerResult.status.c_str(), m_lastPointerResult.reason.c_str());
  ImGui::Separator();
  ImGui::TextDisabled("Manifest tool definitions");
  for (int i = 0; i < static_cast<int>(m_annotationLayer.Tools().size()); ++i) {
    const AnnotationToolDefinition &tool = m_annotationLayer.Tools()[i];
    if (!tool.manual_visible)
      continue;
    ImGui::PushID(i);
    const bool active = m_annotationLayer.ActiveToolIndex() == i;
    ImGui::Text("%s%s", active ? "* " : "  ", tool.name.c_str());
    ImGui::TextWrapped("%s | %s | %s",
                       ImageAnnotationLayer::KindName(tool.kind),
                       tool.role.c_str(), tool.action.c_str());
    ImGui::TextWrapped("source=%s modules=%s", tool.source.c_str(),
                       tool.module_hint.c_str());
    ImGui::PopID();
  }
  if (ImGui::Button("Cancel Polyline Draft")) {
    m_activePolylineElement = -1;
    m_activePolylinePoints.clear();
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    m_imageToolEnabled = false;
    m_imageToolMode = ImageToolMode::PointerPan;
    CancelAnnotationCreate();
    ClearMagicWandPreview("MAGIC_WAND_CANCELLED");
    m_annotationLayer.SetActiveToolIndex(-1);
    m_annotationStatus = "annotation tool disabled (ESC)";
  }

  ImGui::NextColumn();
  ImGui::Text("Element List (ShapeElements)");
  for (int i = 0;
       i < static_cast<int>(m_annotationLayer.ShapeElements().size()); ++i) {
    CxShapeElement &element = m_annotationLayer.ShapeElements()[i];
    ImGui::PushID(2000 + i);
    if (ImGui::Selectable(element.ref.c_str(), element.selected)) {
      m_annotationLayer.SelectShape(i);
      m_scriptResult.overlay_ref = "shape:" + element.ref;
    }
    ImGui::SameLine();
    ImGui::Checkbox("visible", &element.visible);
    char detailBuf[512] = "";
    snprintf(detailBuf, sizeof(detailBuf),
             "id=%d | tool=%s | role=%s | owner=%s:%s | editable=%d | "
             "result=%d | stale=%d",
             element.id, element.tool_id.c_str(), element.semantic_role.c_str(),
             element.owner_type.c_str(), element.owner_ref.c_str(),
             element.editable ? 1 : 0, element.result_element ? 1 : 0,
             element.stale ? 1 : 0);
    ImGui::TextWrapped("%s", detailBuf);
    ImGui::PopID();
  }

  ImGui::Separator();
  static bool showLegacyOverlayElements = false;
  if (ImGui::CollapsingHeader("Legacy Overlay Elements Debug",
                              &showLegacyOverlayElements)) {
    for (int i = 0; i < static_cast<int>(m_annotationLayer.Elements().size());
         ++i) {
      OverlayElement &element = m_annotationLayer.Elements()[i];
      ImGui::PushID(1000 + i);
      if (ImGui::Selectable(element.ref.c_str(), element.selected)) {
        m_annotationLayer.Select(i);
        m_scriptResult.overlay_ref = "overlay:" + element.ref;
        m_scriptResult.evidence_ref = element.evidence_ref;
        m_scriptResult.result_ref = element.result_ref;
        m_scriptResult.issue_entry_ref = element.issue_entry_ref;
      }
      ImGui::SameLine();
      ImGui::Checkbox("visible", &element.visible);
      const char *kindName = ImageAnnotationLayer::KindName(element.kind);
      ImGui::TextWrapped("%s id=%d | status=%s | role=%s", kindName, element.id,
                         element.status.c_str(), element.role.c_str());
      ImGui::PopID();
    }
  }

  ImGui::Columns(1);

  ImGui::Separator();
  ImGui::Text("Element Inspector");
  CxShapeElement *selectedShape = m_annotationLayer.SelectedShape();
  if (selectedShape != nullptr) {
    ImGui::Text("ref: %s", selectedShape->ref.c_str());
    ImGui::Text("tool_id: %s", selectedShape->tool_id.c_str());
    ImGui::Text("semantic_role: %s", selectedShape->semantic_role.c_str());
    ImGui::Text("owner_type: %s", selectedShape->owner_type.c_str());
    ImGui::Text("owner_ref: %s", selectedShape->owner_ref.c_str());
    ImGui::Text("editable: %s", selectedShape->editable ? "true" : "false");
    ImGui::Text("result_element: %s",
                selectedShape->result_element ? "true" : "false");
    ImGui::Text("stale: %s", selectedShape->stale ? "true" : "false");
    if (ImGui::Button("Bind Selected Shape To Globals")) {
      std::string reason;
      if (ExportShapeElementToRuntimeGlobals(m_manualTest, *selectedShape,
                                             reason)) {
        m_manualTest.debug_status = "shape_bound_to_globals";
        m_manualTest.debug_reason = reason;
        m_annotationStatus = reason;
      } else {
        m_manualTest.debug_status = "shape_bind_failed";
        m_manualTest.debug_reason = reason;
        m_annotationStatus = reason;
      }
    }
  } else {
    OverlayElement *selected = m_annotationLayer.Selected();
    if (selected == nullptr) {
      ImGui::TextDisabled("No element selected");
    } else {
      ImGui::Text("ref: %s", selected->ref.c_str());
      ImGui::Text("kind: %s", ImageAnnotationLayer::KindName(selected->kind));
      AnnotationInputText("role", selected->role);
      ImGui::Text("source: %s", selected->source.c_str());
      AnnotationInputText("module_hint", selected->module_hint);
      AnnotationInputText("label", selected->label);
      selected->generated_statement = GenerateElementStatement(*selected);
      ImGui::TextWrapped("generated_statement: %s",
                         selected->generated_statement.empty()
                             ? "(none)"
                             : selected->generated_statement.c_str());
      ImGui::TextWrapped("evidence_ref: %s", selected->evidence_ref.c_str());
      ImGui::TextWrapped("result_ref: %s", selected->result_ref.empty()
                                               ? "(none)"
                                               : selected->result_ref.c_str());
      ImGui::TextWrapped("issue_entry_ref: %s",
                         selected->issue_entry_ref.empty()
                             ? "(none)"
                             : selected->issue_entry_ref.c_str());
      ImGui::Checkbox("visible##inspector", &selected->visible);
      ImGui::SameLine();
      ImGui::Checkbox("editable", &selected->editable);
      if (selected->kind == OverlayKind::Circle &&
          !selected->image_points.empty()) {
        ImGui::Text("cx: %.3f", selected->image_points[0].x);
        ImGui::Text("cy: %.3f", selected->image_points[0].y);
        ImGui::Text("r: %.3f", selected->radius);
      } else
        ImGui::Text("radius: %.3f", selected->radius);
      for (std::size_t i = 0; i < selected->image_points.size(); ++i)
        ImGui::BulletText("point[%d] image=(%.2f, %.2f)", static_cast<int>(i),
                          selected->image_points[i].x,
                          selected->image_points[i].y);
      if (ImGui::Button("Copy Statement"))
        ImGui::SetClipboardText(selected->generated_statement.c_str());
      ImGui::SameLine();
      if (ImGui::Button("Insert Statement To Editor")) {
        InsertStatement(m_manualTest.editor_text,
                        selected->generated_statement);
        m_manualTest.editor_dirty = true;
        m_manualTest.analyzed_text.clear();
        m_annotationStatus = "statement inserted into Script Editor";
      }
      ImGui::SameLine();
      if (ImGui::Button("Replace Current Line")) {
        ReplaceEditorLine(m_manualTest.editor_text, m_manualTest.current_line,
                          selected->generated_statement);
        m_manualTest.editor_dirty = true;
        m_manualTest.analyzed_text.clear();
        m_annotationStatus = "current script line replaced";
      }
      if (selected->kind == OverlayKind::Circle) {
        if (ImGui::Button("Apply To Script")) {
          const bool replace =
              !m_manualTest.line_views.empty() &&
              m_manualTest.current_line >= 0 &&
              m_manualTest.current_line <
                  static_cast<int>(m_manualTest.line_views.size()) &&
              m_manualTest
                      .line_views[static_cast<std::size_t>(
                          m_manualTest.current_line)]
                      .method == "setcircle";
          if (replace)
            ReplaceEditorLine(m_manualTest.editor_text,
                              m_manualTest.current_line,
                              selected->generated_statement);
          else
            InsertStatement(m_manualTest.editor_text,
                            selected->generated_statement);
          m_manualTest.editor_dirty = true;
          m_manualTest.analyzed_text.clear();
          m_annotationStatus = "manual_element applied to script only";
        }
        ImGui::SameLine();
        if (ImGui::Button("Apply To Parser")) {
          if (!QueryParserObjectExists("FindCircle", "afindcircle0"))
            m_parserDebugBridge.ApplyStatement("FindCircle afindcircle0;");
          const bool applied =
              m_parserDebugBridge.ApplyStatement(selected->generated_statement);
          m_scriptResult.status = applied ? "PENDING" : "BLOCKED";
          m_scriptResult.reason =
              applied ? "manual_element applied to parser; runtime objects "
                        "refreshed"
                      : "parser rejected manual circle statement";
          RefreshRuntimeObjectTable("setcircle",
                                    applied ? "runtime_executed" : "BLOCKED");
          m_annotationStatus = m_scriptResult.reason;
        }
      }
    }
  }

  ImGui::Separator();
  AnnotationInputText("Session path", m_annotationSessionPath);
  if (ImGui::Button("Save Elements")) {
    m_annotationStatus = "saving elements count=" +
                         std::to_string(m_annotationLayer.Elements().size());
    m_annotationLayer.SaveElements(
        m_annotationSessionPath, m_scriptResult.image_ref, m_annotationStatus);
  }
  ImGui::SameLine();
  if (ImGui::Button("Load Elements")) {
    std::string imageRef;
    if (m_annotationLayer.LoadElements(m_annotationSessionPath, imageRef,
                                       m_annotationStatus) &&
        !imageRef.empty())
      m_scriptResult.image_ref = imageRef;
  }
  ImGui::SameLine();
  if (ImGui::Button("Clear Elements")) {
    m_annotationLayer.Clear();
    m_activePolylineElement = -1;
    m_annotationDragging = false;
  }

  ImGui::End();
}

bool ViewController::TestLoadAnnotationToolManifest(const std::string &path,
                                                    std::string &reason) {
  m_annotationManifestPath = path;

  std::string init_reason;
  if (!m_parserOwner.Initialize(init_reason)) {
    reason = "parser initialize failed: " + init_reason;
    return false;
  }

  CxAnnotationToolManifestSnapshot snapshot;
  if (!m_parserOwner.ParseAnnotationToolManifest(path, snapshot, reason)) {
    return false;
  }

  return m_annotationLayer.ApplyToolManifestSnapshot(snapshot, reason);
}

bool ViewController::TestApplyAnnotationToolManifestSnapshot(
    const CxAnnotationToolManifestSnapshot &snapshot, std::string &reason) {
  return m_annotationLayer.ApplyToolManifestSnapshot(snapshot, reason);
}

bool ViewController::TestSetActiveAnnotationTool(const std::string &tool_id,
                                                 std::string &reason) {
  for (int i = 0; i < static_cast<int>(m_annotationLayer.Tools().size()); ++i) {
    if (m_annotationLayer.Tools()[i].name == tool_id) {
      m_annotationLayer.SetActiveToolIndex(i);
      m_imageToolEnabled = true;
      m_imageToolMode =
          ToolModeFromAnnotationTool(m_annotationLayer.Tools()[i]);
      CancelAnnotationCreate();
      return true;
    }
  }
  reason = "tool not found: " + tool_id;
  return false;
}

void ViewController::TestEnableAnnotationCreateMode() {
  m_imageToolEnabled = true;
  m_imageToolMode = ImageToolMode::LineCreate;
}

void ViewController::TestSetActiveToolKind(OverlayKind kind) {
  switch (kind) {
  case OverlayKind::Point:
    m_imageToolMode = ImageToolMode::PointCreate;
    break;
  case OverlayKind::Line:
    m_imageToolMode = ImageToolMode::LineCreate;
    break;
  case OverlayKind::Rect:
    m_imageToolMode = ImageToolMode::RectCreate;
    break;
  case OverlayKind::Circle:
    m_imageToolMode = ImageToolMode::CircleCreate;
    break;
  case OverlayKind::Ellipse:
    m_imageToolMode = ImageToolMode::EllipseCreate;
    break;
  case OverlayKind::Polyline:
    m_imageToolMode = ImageToolMode::PolylineCreate;
    break;
  default:
    m_imageToolMode = ImageToolMode::LineCreate;
    break;
  }
}

void ViewController::TestSetToolModePointerPan() {
  m_imageToolEnabled = false;
  m_imageToolMode = ImageToolMode::PointerPan;
  m_annotationLayer.SetActiveToolIndex(-1);
  CancelAnnotationCreate();
}

std::size_t ViewController::TestShapeElementCount() const {
  return m_annotationLayer.ShapeElements().size();
}

bool ViewController::TestGetLastPointerResult(CxImagePointerResult &out) const {
  out = m_lastPointerResult;
  return true;
}

std::string ViewController::TestShapeKindByRef(const std::string &ref) const {
  const auto &elements = m_annotationLayer.ShapeElements();
  for (const auto &elem : elements) {
    if (elem.stable_ref == ref && elem.shape) {
      switch (elem.shape->kind()) {
      case CxShapeKind::Points:
        return "PointsShape";
      case CxShapeKind::Line:
        return "LineShape";
      case CxShapeKind::Rect:
        return "RectShape";
      case CxShapeKind::Circle:
        return "CircleShape";
      case CxShapeKind::Polyline:
        return "PolylineShape";
      case CxShapeKind::Ellipse:
        return "EllipseShape";
      case CxShapeKind::LineGauge:
        return "LineGaugeShape";
      default:
        return "Unknown";
      }
    }
  }
  return "";
}
