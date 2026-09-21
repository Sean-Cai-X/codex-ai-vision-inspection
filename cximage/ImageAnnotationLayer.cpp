#include "ImageAnnotationLayer.h"

#include "CxAnnotationToolRuntime.h"
#include "CxAnnotationToolRegister.h"
#include "muParser.h"
#include "EllipseShape.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace
{
namespace fs = std::filesystem;
}

std::unique_ptr<ShapeBase> CreateInitialShapeForTool(
    const AnnotationToolDefinition& tool,
    const std::vector<CxShapePoint>& points)
{
    if (tool.shape_type == "PointsShape")
    {
        auto shape = std::make_unique<PointsShape>();
        if (!points.empty())
            shape->addpoint(gp_Pnt(points[0].x, points[0].y, 0.0));
        return shape;
    }

    if (tool.shape_type == "LineShape")
    {
        auto shape = std::make_unique<LineShape>();
        if (points.size() >= 2)
            shape->setline(static_cast<int>(points[0].x), static_cast<int>(points[0].y),
                           static_cast<int>(points[1].x), static_cast<int>(points[1].y));
        return shape;
    }

    if (tool.shape_type == "LineGaugeShape")
    {
        if (points.size() >= 2)
            return std::make_unique<LineGaugeShape>(
                points[0].x, points[0].y, points[1].x, points[1].y, 20.0);
        return std::make_unique<LineGaugeShape>();
    }

    if (tool.shape_type == "RectShape")
    {
        auto shape = std::make_unique<RectShape>();
        if (points.size() >= 2)
            shape->setRect(points[0].x, points[0].y, points[1].x, points[1].y);
        return shape;
    }

    if (tool.shape_type == "AutoBoundary")
    {
        auto shape = std::make_unique<RectShape>();
        if (points.size() >= 2)
            shape->setRect(points[0].x, points[0].y, points[1].x, points[1].y);
        else if (!points.empty())
            shape->setRect(points[0].x - 40.0, points[0].y - 40.0,
                           points[0].x + 40.0, points[0].y + 40.0);
        return shape;
    }

    if (tool.shape_type == "CircleShape")
    {
        if (points.size() >= 2)
        {
            const double dx = points[1].x - points[0].x;
            const double dy = points[1].y - points[0].y;
            return std::make_unique<CircleShape>(
                points[0].x, points[0].y, std::sqrt(dx * dx + dy * dy));
        }
        return std::make_unique<CircleShape>();
    }

    if (tool.shape_type == "EllipseShape")
    {
        if (points.size() >= 2)
        {
            const double cx = (points[0].x + points[1].x) * 0.5;
            const double cy = (points[0].y + points[1].y) * 0.5;
            const double rx = std::abs(points[1].x - points[0].x) * 0.5;
            const double ry = std::abs(points[1].y - points[0].y) * 0.5;
            return std::make_unique<EllipseShape>(cx, cy, rx, ry);
        }
        return std::make_unique<EllipseShape>();
    }

    if (tool.shape_type == "PolylineShape")
    {
        auto shape = std::make_unique<PolylineShape>();
        for (const auto& p : points)
            shape->addPoint(p.x, p.y);
        shape->close(false);
        return shape;
    }

    return nullptr;
}

std::string Trim(const std::string& text)
{
  const std::size_t first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return std::string();
  const std::size_t last = text.find_last_not_of(" \t\r\n");
  return text.substr(first, last - first + 1);
}

bool SplitKeyValue(const std::string& line, std::string& key, std::string& value)
{
  const std::size_t separator = line.find(':');
  if (separator == std::string::npos) return false;
  key = Trim(line.substr(0, separator));
  value = Trim(line.substr(separator + 1));
  return !key.empty();
}

std::string UnquoteStatementValue(std::string value)
{
  value = Trim(value);
  if (!value.empty() && value.back() == ';') value.pop_back();
  value = Trim(value);
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
    value = value.substr(1, value.size() - 2);
  return value;
}

const char* ImageAnnotationLayer::KindName(OverlayKind kind)
{
  switch (kind)
  {
    case OverlayKind::Point: return "Point";
    case OverlayKind::Line: return "Line";
    case OverlayKind::Rect: return "Rect";
    case OverlayKind::Circle: return "Circle";
    case OverlayKind::Ellipse: return "Ellipse";
    case OverlayKind::Polyline: return "Polyline";
    case OverlayKind::AutoBoundaryRequest: return "AutoBoundary";
    case OverlayKind::BoundaryPolyline: return "BoundaryPolyline";
  }
  return "Point";
}

bool ImageAnnotationLayer::ParseKind(const std::string& text, OverlayKind& kind)
{
  if (text == "Point") kind = OverlayKind::Point;
  else if (text == "Line") kind = OverlayKind::Line;
  else if (text == "Rect") kind = OverlayKind::Rect;
  else if (text == "Circle") kind = OverlayKind::Circle;
  else if (text == "Ellipse") kind = OverlayKind::Ellipse;
  else if (text == "Polyline") kind = OverlayKind::Polyline;
  else if (text == "AutoBoundary") kind = OverlayKind::AutoBoundaryRequest;
  else if (text == "BoundaryPolyline") kind = OverlayKind::BoundaryPolyline;
  else return false;
  return true;
}

bool ImageAnnotationLayer::ConvertToolSpec(
    const CxAnnotationToolSpec& spec,
    AnnotationToolDefinition& def,
    std::string& reason) const
{
    def.name = spec.id;
    def.label = spec.label;
    def.shape_type = spec.shape_type;
    def.role = spec.semantic_role;
    def.action = spec.interaction;
    def.source = spec.owner_tool.empty() ? "manual" : spec.owner_tool;
    def.module_hint = spec.owner_tool;
    def.description = spec.source_script;
    def.owner_tool = spec.owner_tool;
    def.owner_binding = spec.owner_binding;
    def.manual_visible = spec.manual_visible;
    def.editable = spec.editable;

    if (spec.shape_type == "PointsShape") def.kind = OverlayKind::Point;
    else if (spec.shape_type == "LineShape" || spec.shape_type == "LineGaugeShape") def.kind = OverlayKind::Line;
    else if (spec.shape_type == "RectShape") def.kind = OverlayKind::Rect;
    else if (spec.shape_type == "CircleShape") def.kind = OverlayKind::Circle;
    else if (spec.shape_type == "EllipseShape") def.kind = OverlayKind::Ellipse;
    else if (spec.shape_type == "PolylineShape") def.kind = OverlayKind::Polyline;
    else if (spec.shape_type == "BoundaryPolyline") def.kind = OverlayKind::BoundaryPolyline;
    else if (spec.shape_type == "AutoBoundary") def.kind = OverlayKind::AutoBoundaryRequest;
    else
    {
        reason = "unsupported annotation shape type: " + spec.shape_type;
        return false;
    }

    return true;
}

bool ImageAnnotationLayer::ApplyToolManifestSnapshot(
    const CxAnnotationToolManifestSnapshot& snapshot,
    std::string& reason)
{
    std::vector<AnnotationToolDefinition> nextTools;

    for (const CxAnnotationToolSpec& spec : snapshot.tools)
    {
        AnnotationToolDefinition def;

        if (!ConvertToolSpec(spec, def, reason))
            return false;

        nextTools.push_back(def);
    }

    if (nextTools.empty())
    {
        reason = "tool manifest snapshot is empty";
        return false;
    }

    myTools.swap(nextTools);
    myLoadStatus = "loaded from parser-owned snapshot";
    reason = myLoadStatus;
    return true;
}

const AnnotationToolDefinition* ImageAnnotationLayer::FindToolDefinition(const std::string& tool_id) const
{
    for (const AnnotationToolDefinition& tool : myTools)
    {
        if (tool.name == tool_id)
            return &tool;
    }

    return nullptr;
}

std::string ImageAnnotationLayer::MakeRef(OverlayKind kind, int id) const
{
  const char* prefix = "point";
  if (kind == OverlayKind::Line) prefix = "line";
  else if (kind == OverlayKind::Rect) prefix = "roi_rect";
  else if (kind == OverlayKind::Circle) prefix = "circle";
  else if (kind == OverlayKind::Ellipse) prefix = "ellipse";
  else if (kind == OverlayKind::Polyline) prefix = "polyline";
  std::ostringstream stream;
  stream << prefix << '_' << std::setw(3) << std::setfill('0') << id;
  return stream.str();
}

OverlayElement& ImageAnnotationLayer::Create(OverlayKind kind,
                                              const std::string& role,
                                              const std::string& source,
                                              const std::string& moduleHint)
{
  OverlayElement element;
  element.id = myNextId++;
  element.kind = kind;
  element.ref = MakeRef(kind, element.id);
  element.role = role;
  element.source = source;
  element.module_hint = moduleHint;
  element.evidence_ref = "evidence:" + element.ref;
  myElements.push_back(element);
  Select(static_cast<int>(myElements.size()) - 1);
  return myElements.back();
}

void ImageAnnotationLayer::Clear()
{
  myElements.clear();
  mySelectedIndex = -1;
  myNextId = 1;
}

void ImageAnnotationLayer::Select(int index)
{
  mySelectedIndex = index >= 0 && index < static_cast<int>(myElements.size()) ? index : -1;
  for (int i = 0; i < static_cast<int>(myElements.size()); ++i)
    myElements[i].selected = i == mySelectedIndex;
}

OverlayElement* ImageAnnotationLayer::Selected()
{
  return mySelectedIndex >= 0 && mySelectedIndex < static_cast<int>(myElements.size()) ?
    &myElements[mySelectedIndex] : nullptr;
}

const OverlayElement* ImageAnnotationLayer::Selected() const
{
  return mySelectedIndex >= 0 && mySelectedIndex < static_cast<int>(myElements.size()) ?
    &myElements[mySelectedIndex] : nullptr;
}

const AnnotationToolDefinition* ImageAnnotationLayer::ActiveTool() const
{
  return myActiveToolIndex >= 0 && myActiveToolIndex < static_cast<int>(myTools.size()) ?
    &myTools[myActiveToolIndex] : nullptr;
}

std::string ImageAnnotationLayer::SelectedRef(OverlayKind kind) const
{
  const OverlayElement* selected = Selected();
  if (selected != nullptr && selected->kind == kind) return selected->ref;
  for (auto it = myElements.rbegin(); it != myElements.rend(); ++it)
    if (it->kind == kind) return it->ref;
  return std::string();
}

bool ImageAnnotationLayer::SaveElements(const std::string& path,
                                         const std::string& imageRef,
                                         std::string& reason) const
{
  const fs::path output(path);
  if (output.has_parent_path()) fs::create_directories(output.parent_path());
  std::ofstream stream(output);
  if (!stream)
  {
    reason = "unable to open annotation output";
    return false;
  }
  stream << "image_ref: " << imageRef << '\n';
  for (const OverlayElement& element : myElements)
  {
    stream << "element: " << element.ref << '\n';
    stream << "kind: " << KindName(element.kind) << '\n';
    stream << "role: " << element.role << '\n';
    stream << "source: " << element.source << '\n';
    stream << "module_hint: " << element.module_hint << '\n';
    stream << "radius: " << element.radius << '\n';
    stream << "label: " << element.label << '\n';
    stream << "generated_statement: " << element.generated_statement << '\n';
    stream << "evidence_ref: " << element.evidence_ref << '\n';
    stream << "points:";
    for (const OverlayImagePoint& point : element.image_points)
      stream << ' ' << point.x << ',' << point.y;
    stream << "\nend\n";
  }
  reason = "annotation elements saved";
  return true;
}

bool ImageAnnotationLayer::LoadElements(const std::string& path,
                                         std::string& imageRef,
                                         std::string& reason)
{
  std::ifstream stream{fs::path(path)};
  if (!stream)
  {
    reason = "annotation file not found";
    return false;
  }
  Clear();
  OverlayElement* current = nullptr;
  std::string line;
  while (std::getline(stream, line))
  {
    std::string key;
    std::string value;
    if (!SplitKeyValue(line, key, value)) continue;
    if (key == "image_ref") imageRef = value;
    else if (key == "element")
    {
      OverlayElement& created = Create(OverlayKind::Point, "", "manual", "cximage");
      created.ref = value;
      created.evidence_ref = "evidence:" + value;
      current = &created;
    }
    else if (current == nullptr) continue;
    else if (key == "kind") ParseKind(value, current->kind);
    else if (key == "role") current->role = value;
    else if (key == "source") current->source = value;
    else if (key == "module_hint") current->module_hint = value;
    else if (key == "radius") current->radius = std::stof(value);
    else if (key == "label") current->label = value;
    else if (key == "generated_statement") current->generated_statement = value;
    else if (key == "evidence_ref") current->evidence_ref = value;
    else if (key == "points")
    {
      std::istringstream points(value);
      std::string token;
      while (points >> token)
      {
        const std::size_t comma = token.find(',');
        if (comma == std::string::npos) continue;
        current->image_points.push_back({std::stof(token.substr(0, comma)),
                                         std::stof(token.substr(comma + 1))});
      }
    }
  }
  Select(myElements.empty() ? -1 : static_cast<int>(myElements.size()) - 1);
  reason = "annotation elements loaded";
  return true;
}

OverlayElement* ImageAnnotationLayer::FindByStableRef(const std::string& ref)
{
  for (auto& e : myElements)
  {
    if (e.stable_ref == ref)
      return &e;
  }
  return nullptr;
}

const OverlayElement* ImageAnnotationLayer::FindByStableRef(const std::string& ref) const
{
  for (const auto& e : myElements)
  {
    if (e.stable_ref == ref)
      return &e;
  }
  return nullptr;
}

OverlayElement& ImageAnnotationLayer::UpsertByStableRef(const std::string& ref, OverlayKind kind)
{
  OverlayElement* existing = FindByStableRef(ref);
  if (existing != nullptr)
  {
    existing->kind = kind;
    existing->image_points.clear();
    existing->radius = 0.0f;
    existing->stale = false;
    return *existing;
  }
  OverlayElement& created = Create(kind, "", "runtime", "cximage");
  created.stable_ref = ref;
  created.runtime_bound = true;
  created.editable = false;
  return created;
}

void ImageAnnotationLayer::RemoveByOwner(const std::string& owner_type, const std::string& owner_ref)
{
  auto it = myElements.begin();
  while (it != myElements.end())
  {
    if (it->owner_type == owner_type && it->owner_ref == owner_ref)
    {
      it = myElements.erase(it);
    }
    else
    {
      ++it;
    }
  }
  if (mySelectedIndex >= static_cast<int>(myElements.size()))
  {
    mySelectedIndex = myElements.empty() ? -1 : static_cast<int>(myElements.size()) - 1;
  }
}

CxShapeElement& ImageAnnotationLayer::CreateFromTool(
    const AnnotationToolDefinition& tool,
    std::unique_ptr<ShapeBase> shape)
{
    myShapeElements.emplace_back();
    CxShapeElement& element = myShapeElements.back();
    element.id = myNextId++;
    element.ref = tool.name + "_" + std::to_string(element.id);
    element.stable_ref = element.ref;
    element.tool_id = tool.name;
    element.owner_type = tool.owner_tool;
    element.owner_binding = tool.owner_binding;
    element.semantic_role = tool.role;
    element.editable = tool.editable;
    element.shape = std::move(shape);
    return element;
}

CxShapeElement& ImageAnnotationLayer::UpsertShape(
    const std::string& stable_ref,
    std::unique_ptr<ShapeBase> shape)
{
    CxShapeElement* existing = FindShapeByStableRef(stable_ref);
    if (existing != nullptr)
    {
        existing->shape = std::move(shape);
        existing->stale = false;
        return *existing;
    }
    myShapeElements.emplace_back();
    CxShapeElement& element = myShapeElements.back();
    element.id = myNextId++;
    element.ref = stable_ref;
    element.stable_ref = stable_ref;
    element.shape = std::move(shape);
    return element;
}

CxShapeElement* ImageAnnotationLayer::FindShapeByStableRef(const std::string& stable_ref)
{
    for (auto& e : myShapeElements)
    {
        if (e.stable_ref == stable_ref)
            return &e;
    }
    return nullptr;
}

bool ImageAnnotationLayer::ApplyFindCircleAnnulusGauge(
    const std::string& owner_ref,
    double cx,
    double cy,
    double inner_radius,
    double outer_radius,
    bool scan_sector_enabled,
    double scan_sector_start_degrees,
    double scan_sector_end_degrees,
    std::string& reason)
{
    if (!std::isfinite(cx) || !std::isfinite(cy) ||
        !std::isfinite(inner_radius) || !std::isfinite(outer_radius))
    {
        reason = "FindCircle annulus contains non-finite geometry";
        return false;
    }

    const double outer = std::max(1.0, outer_radius);
    const double inner = std::max(0.0, std::min(inner_radius, outer - 1.0));

    CxShapeElement* target = nullptr;
    for (auto& element : myShapeElements)
    {
        if (element.owner_type != "FindCircle" ||
            !element.editable ||
            element.semantic_role != "roi" ||
            element.shape == nullptr ||
            element.shape->kind() != CxShapeKind::Circle)
        {
            continue;
        }
        if (!owner_ref.empty() && element.owner_ref != owner_ref)
            continue;
        target = &element;
        break;
    }

    if (target == nullptr)
    {
        reason = "editable FindCircle annulus ROI is unavailable";
        return false;
    }

    auto* circle = dynamic_cast<CircleShape*>(target->shape.get());
    if (circle == nullptr)
    {
        reason = "FindCircle ROI is not a CircleShape";
        return false;
    }

    circle->setCenter(cx, cy);
    circle->setRadius(outer);
    circle->setInnerRadius(inner);
    circle->setScanSector(
        scan_sector_enabled,
        scan_sector_start_degrees,
        scan_sector_end_degrees);
    target->stale = true;
    target->runtime_edit_pending = true;
    MarkOwnerResultStale(target->owner_type, target->owner_ref);

    reason = "FindCircle annulus applied to repository ROI: Rin=" +
        std::to_string(static_cast<int>(std::lround(inner))) +
        ", Rout=" + std::to_string(static_cast<int>(std::lround(outer)));
    return true;
}

void ImageAnnotationLayer::ConfirmAllRuntimeWritebacks()
{
    for (auto& element : myShapeElements)
        element.runtime_edit_pending = false;
}

CxShapeHitResult ImageAnnotationLayer::HitTest(double image_x, double image_y, double tolerance)
{
    CxShapeHitResult result;
    int max_priority = -1;
    double min_distance = tolerance + 1.0;

    for (int i = 0; i < static_cast<int>(myShapeElements.size()); ++i)
    {
        auto& element = myShapeElements[i];
        if (!element.visible || !element.shape)
            continue;

        CxShapeHit hit = element.shape->hitTest(image_x, image_y, tolerance);
        if (!hit.hit)
            continue;

        const int priority = HandlePriority(hit.role) + (element.editable ? 1000 : 0);
        if (priority > max_priority ||
            (priority == max_priority && hit.distance < min_distance))
        {
            max_priority = priority;
            min_distance = hit.distance;
            result.hit = true;
            result.element_index = i;
            result.shape_hit = hit;
        }
    }

    return result;
}

bool ImageAnnotationLayer::BeginDrag(const CxShapeHitResult& hit, double image_x, double image_y)
{
    if (!hit.hit || hit.element_index < 0)
        return false;

    auto& element = myShapeElements[hit.element_index];
    if (!element.editable || !element.shape)
        return false;

    myDraggingElement = hit.element_index;
    myDraggingRole = hit.shape_hit.role;
    myDraggingVertexIndex = hit.shape_hit.vertex_index;
    myDragStartX = image_x;
    myDragStartY = image_y;
    myDragLastX = image_x;
    myDragLastY = image_y;
    myDragHasPosition = true;

    for (auto& e : myShapeElements)
        e.selected = false;
    element.selected = true;

    return true;
}

bool ImageAnnotationLayer::UpdateDrag(double image_x, double image_y)
{
    if (myDraggingElement < 0 || !myShapeElements[myDraggingElement].shape)
        return false;

    auto& element = myShapeElements[myDraggingElement];

    if (myDraggingRole == CxShapeHandleRole::Body && myDragHasPosition)
    {
        const double dx = image_x - myDragLastX;
        const double dy = image_y - myDragLastY;
        element.shape->translateBy(dx, dy);
    }
    else
    {
        element.shape->dragHandle(myDraggingRole, myDraggingVertexIndex, image_x, image_y);
    }

    myDragLastX = image_x;
    myDragLastY = image_y;
    element.stale = true;

    return true;
}

void ImageAnnotationLayer::MarkOwnerResultStale(const std::string& owner_type, const std::string& owner_ref)
{
    for (auto& e : myShapeElements)
    {
        if (e.owner_type == owner_type && e.owner_ref == owner_ref && e.result_element)
            e.stale = true;
    }
}

bool ImageAnnotationLayer::CommitEdit(std::string& reason)
{
    if (myDraggingElement < 0)
    {
        reason = "no active drag";
        return false;
    }

    auto& element = myShapeElements[myDraggingElement];

    if (element.owner_type == "FindLine" && element.owner_binding == "setline")
    {
        reason = "Findline ROI edited; measure/fit result marked stale";
        MarkOwnerResultStale(element.owner_type, element.owner_ref);
    }
    else if (element.owner_type == "FindCircle" && element.owner_binding == "setcircle")
    {
        reason = "FindCircle ROI radius edited; measure/fit result marked stale";
        MarkOwnerResultStale(element.owner_type, element.owner_ref);
    }
    else
    {
        reason = "manual shape edited";
    }

    CancelDrag();
    return true;
}

bool ImageAnnotationLayer::SimulateDragShape(
    const std::string& stable_or_ref,
    CxShapeHandleRole role,
    int vertex_index,
    double from_x,
    double from_y,
    double to_x,
    double to_y,
    std::string& reason)
{
    int index = -1;
    for (int i = 0; i < static_cast<int>(myShapeElements.size()); ++i)
    {
        if (myShapeElements[i].stable_ref == stable_or_ref ||
            myShapeElements[i].ref == stable_or_ref)
        {
            index = i;
            break;
        }
    }

    if (index < 0)
    {
        reason = "shape not found by ref: " + stable_or_ref;
        return false;
    }

    CxShapeHitResult hit;
    hit.hit = true;
    hit.element_index = index;
    hit.shape_hit.hit = true;
    hit.shape_hit.role = role;
    hit.shape_hit.vertex_index = vertex_index;
    hit.shape_hit.distance = 0.0;

    if (!BeginDrag(hit, from_x, from_y))
    {
        reason = "BeginDrag failed for ref: " + stable_or_ref;
        return false;
    }

    UpdateDrag(to_x, to_y);
    CommitEdit(reason);

    return true;
}

void ImageAnnotationLayer::CancelDrag()
{
    myDraggingElement = -1;
    myDraggingRole = CxShapeHandleRole::None;
    myDraggingVertexIndex = -1;
    myDragStartX = 0.0;
    myDragStartY = 0.0;
}

void ImageAnnotationLayer::ClearShapeElements()
{
    myShapeElements.clear();
    mySelectedShapeIndex = -1;
    CancelDrag();
}

bool ImageAnnotationLayer::HasActiveDrag() const
{
    return myDraggingElement >= 0;
}

std::size_t ImageAnnotationLayer::ShapeElementCount() const
{
    return myShapeElements.size();
}

int ImageAnnotationLayer::HandlePriority(CxShapeHandleRole role)
{
    switch (role)
    {
    case CxShapeHandleRole::Vertex:
    case CxShapeHandleRole::Corner0:
    case CxShapeHandleRole::Corner1:
    case CxShapeHandleRole::Corner2:
    case CxShapeHandleRole::Corner3:
    case CxShapeHandleRole::Start:
    case CxShapeHandleRole::End:
    case CxShapeHandleRole::Radius:
    case CxShapeHandleRole::RadiusX:
    case CxShapeHandleRole::RadiusY:
    case CxShapeHandleRole::InnerRadius:
    case CxShapeHandleRole::WidthPositive:
    case CxShapeHandleRole::WidthNegative:
        return 300;
    case CxShapeHandleRole::Center:
        return 250;
    case CxShapeHandleRole::Body:
        return 100;
    case CxShapeHandleRole::None:
    default:
        return 0;
    }
}

int ImageAnnotationLayer::MarkOwnerResultStaleAndCount(
    const std::string& owner_type,
    const std::string& owner_ref)
{
    int count = 0;
    for (auto& e : myShapeElements)
    {
        if (e.result_element &&
            e.owner_type == owner_type &&
            e.owner_ref == owner_ref)
        {
            e.stale = true;
            count++;
        }
    }
    return count;
}

void ImageAnnotationLayer::RemoveRuntimeOwnersNotIn(
    const std::unordered_set<std::string>& liveOwners)
{
    auto it = myShapeElements.begin();
    while (it != myShapeElements.end())
    {
        if (it->runtime_bound)
        {
            const std::string owner_key = it->owner_type + ":" + it->owner_ref;
            if (liveOwners.find(owner_key) == liveOwners.end())
            {
                it = myShapeElements.erase(it);
                continue;
            }
        }
        ++it;
    }
}

bool ImageAnnotationLayer::CommitEdit(CxShapeCommitResult& result)
{
    if (myDraggingElement < 0)
    {
        result.reason = "no active drag";
        return false;
    }

    CxShapeElement& element = myShapeElements[myDraggingElement];
    result.owner_type = element.owner_type;
    result.owner_ref = element.owner_ref;
    result.owner_binding = element.owner_binding;
    result.semantic_role = element.semantic_role;
    result.stable_ref = element.stable_ref;
    result.editable = element.editable;
    result.result_marked_stale = false;
    result.stale_result_count = 0;
    result.runtime_writeback = false;

    element.stale = false;

    if (element.runtime_bound && element.editable && !element.owner_binding.empty())
    {
        element.runtime_edit_pending = true;
    }



    if (!element.owner_type.empty())
    {
        const int markedCount = MarkOwnerResultStaleAndCount(element.owner_type, element.owner_ref);
        result.result_marked_stale = markedCount > 0;
        result.stale_result_count = markedCount;
        if (!result.reason.empty())
        {
            result.reason += ", " + std::to_string(markedCount) + " result elements marked stale";
        }
        else
        {
            result.reason = "drag committed, " + std::to_string(markedCount) + " result elements marked stale";
        }
    }
    else
    {
        if (result.reason.empty())
            result.reason = "drag committed";
    }

    result.committed = true;
    CancelDrag();
    return true;
}

bool ImageAnnotationLayer::SimulatePointerDrag(
    double from_x,
    double from_y,
    double to_x,
    double to_y,
    int intermediate_steps,
    double tolerance,
    CxShapeInteractionTrace& trace)
{
    trace.pointer_events.clear();
    trace.hits.clear();
    trace.snapshots.clear();
    trace.commit_reason.clear();
    trace.commit_result = {};

    trace.hit_test_called = true;
    trace.hit = false;
    trace.begin_drag_called = false;
    trace.begin_drag_ok = false;
    trace.update_drag_ok = false;
    trace.commit_called = false;
    trace.commit_ok = false;

    CxShapeHitResult hit = HitTest(from_x, from_y, tolerance);
    trace.hits.push_back(hit);
    trace.hit = hit.hit;

    if (!hit.hit)
    {
        trace.commit_reason = "hit test failed at start position";
        return false;
    }

    trace.begin_drag_called = true;
    if (!BeginDrag(hit, from_x, from_y))
    {
        trace.begin_drag_ok = false;
        trace.commit_reason = "begin drag failed";
        return false;
    }
    trace.begin_drag_ok = true;

    if (myShapeElements.size() > static_cast<std::size_t>(hit.element_index) &&
        myShapeElements[hit.element_index].shape)
    {
        CxShapeGeometrySnapshot snap;
        myShapeElements[hit.element_index].shape->snapshot(snap);
        trace.snapshots.push_back(snap);
    }

    trace.pointer_events.push_back({ CxPointerEvent::Type::LeftDown, from_x, from_y, 0 });

    trace.update_drag_ok = true;
    const int steps = std::max(1, intermediate_steps);
    for (int i = 1; i <= steps; ++i)
    {
        const double t = static_cast<double>(i) / steps;
        const double x = from_x + (to_x - from_x) * t;
        const double y = from_y + (to_y - from_y) * t;

        if (!UpdateDrag(x, y))
        {
            trace.update_drag_ok = false;
            trace.commit_reason = "UpdateDrag failed at step " + std::to_string(i);
            CancelDrag();
            return false;
        }
        trace.pointer_events.push_back({ CxPointerEvent::Type::LeftDrag, x, y, static_cast<uint64_t>(i) });

        if (myShapeElements.size() > static_cast<std::size_t>(hit.element_index) &&
            myShapeElements[hit.element_index].shape)
        {
            CxShapeGeometrySnapshot snap;
            myShapeElements[hit.element_index].shape->snapshot(snap);
            trace.snapshots.push_back(snap);
        }
    }

    trace.pointer_events.push_back({ CxPointerEvent::Type::LeftUp, to_x, to_y, static_cast<uint64_t>(steps + 1) });

    trace.commit_called = true;
    CxShapeCommitResult commit;
    if (!CommitEdit(commit))
    {
        trace.commit_ok = false;
        trace.commit_result = commit;
        trace.commit_reason = commit.reason;
        return false;
    }
    trace.commit_ok = true;

    trace.commit_result = commit;
    trace.commit_reason = commit.reason;

    if (HasActiveDrag())
    {
        trace.commit_ok = false;
        trace.commit_reason = "drag remained active after commit";
        CancelDrag();
        return false;
    }

    return true;
}

void ImageAnnotationLayer::RemoveShapeByOwner(const std::string& owner_type, const std::string& owner_ref)
{
    auto it = myShapeElements.begin();
    while (it != myShapeElements.end())
    {
        if (it->owner_type == owner_type && it->owner_ref == owner_ref)
        {
            it = myShapeElements.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

bool ImageAnnotationLayer::RemoveShapeByStableRef(const std::string& stable_ref)
{
    for (auto it = myShapeElements.begin(); it != myShapeElements.end(); ++it)
    {
        if (it->stable_ref != stable_ref)
            continue;
        const int removedIndex = static_cast<int>(std::distance(myShapeElements.begin(), it));
        myShapeElements.erase(it);
        if (mySelectedShapeIndex == removedIndex)
            mySelectedShapeIndex = -1;
        else if (mySelectedShapeIndex > removedIndex)
            --mySelectedShapeIndex;
        return true;
    }
    return false;
}

void ImageAnnotationLayer::SelectShape(int index)
{
    mySelectedShapeIndex =
        index >= 0 && index < static_cast<int>(myShapeElements.size())
        ? index : -1;

    for (int i = 0; i < static_cast<int>(myShapeElements.size()); ++i)
        myShapeElements[i].selected = i == mySelectedShapeIndex;
}

CxShapeElement* ImageAnnotationLayer::SelectedShape()
{
    return mySelectedShapeIndex >= 0 && mySelectedShapeIndex < static_cast<int>(myShapeElements.size()) ?
        &myShapeElements[mySelectedShapeIndex] : nullptr;
}

void ImageAnnotationLayer::EnumerateVisibleShapes(std::vector<const CxShapeElement*>& out) const
{
    out.clear();
    for (const auto& e : myShapeElements)
    {
        if (e.visible && e.shape)
            out.push_back(&e);
    }
}

void ImageAnnotationLayer::UpsertShape(
    const std::string& stable_ref,
    const std::string& owner_type,
    const std::string& owner_ref,
    const std::string& owner_binding,
    const std::string& semantic_role,
    bool editable,
    bool result_element,
    std::unique_ptr<ShapeBase> shape)
{
    CxShapeElement* existing = FindShapeByStableRef(stable_ref);
    if (existing != nullptr)
    {
        if (existing->runtime_edit_pending)
        {
            existing->runtime_publish_generation = myActiveRuntimePublishGeneration;
            return;
        }

        existing->shape = std::move(shape);
        existing->owner_type = owner_type;
        existing->owner_ref = owner_ref;
        existing->owner_binding = owner_binding;
        existing->semantic_role = semantic_role;
        existing->editable = editable;
        existing->result_element = result_element;
        existing->stale = false;
        existing->runtime_bound = myActiveRuntimePublishGeneration != 0;
        existing->runtime_publish_generation = myActiveRuntimePublishGeneration;
        return;
    }

    myShapeElements.emplace_back();
    CxShapeElement& element = myShapeElements.back();
    element.id = myNextId++;
    element.ref = stable_ref;
    element.stable_ref = stable_ref;
    element.owner_type = owner_type;
    element.owner_ref = owner_ref;
    element.owner_binding = owner_binding;
    element.semantic_role = semantic_role;
    element.editable = editable;
    element.result_element = result_element;
    element.runtime_bound = myActiveRuntimePublishGeneration != 0;
    element.runtime_publish_generation = myActiveRuntimePublishGeneration;
    element.shape = std::move(shape);
}

uint64_t ImageAnnotationLayer::BeginRuntimeOwnerPublish(
    const std::string& owner_type,
    const std::string& owner_ref)
{
    myActiveRuntimePublishGeneration = ++myRuntimePublishGeneration;
    myActivePublishOwnerType = owner_type;
    myActivePublishOwnerRef = owner_ref;
    return myActiveRuntimePublishGeneration;
}

void ImageAnnotationLayer::EndRuntimeOwnerPublish(
    const std::string& owner_type,
    const std::string& owner_ref,
    uint64_t generation)
{
    auto it = myShapeElements.begin();
    while (it != myShapeElements.end())
    {
        const bool sameOwner =
            it->runtime_bound &&
            it->owner_type == owner_type &&
            it->owner_ref == owner_ref;

        const bool notPublishedThisRound =
            it->runtime_publish_generation != generation;

        if (sameOwner && notPublishedThisRound && !it->runtime_edit_pending)
        {
            it = myShapeElements.erase(it);
            continue;
        }

        ++it;
    }

    myActivePublishOwnerType.clear();
    myActivePublishOwnerRef.clear();
    myActiveRuntimePublishGeneration = 0;
}

void ImageAnnotationLayer::ConfirmRuntimeWriteback(const std::string& stable_ref)
{
    CxShapeElement* element = FindShapeByStableRef(stable_ref);
    if (element != nullptr)
        element->runtime_edit_pending = false;
}

bool ImageAnnotationLayer::WriteShapeElementsJson(
    const std::filesystem::path& path,
    std::string& reason) const
{
    try
    {
        if (path.has_parent_path())
            fs::create_directories(path.parent_path());

        std::ofstream stream(path);
        if (!stream)
        {
            reason = "failed to open file for writing";
            return false;
        }

        stream << "{\n";
        stream << "  \"shape_elements\": [\n";

        for (size_t i = 0; i < myShapeElements.size(); ++i)
        {
            const auto& e = myShapeElements[i];
            stream << "    {\n";
            stream << "      \"stable_ref\": \"" << e.stable_ref << "\",\n";

            std::string shape_kind = "Unknown";
            if (e.shape)
            {
                switch (e.shape->kind())
                {
                case CxShapeKind::Points: shape_kind = "Points"; break;
                case CxShapeKind::Line: shape_kind = "Line"; break;
                case CxShapeKind::Rect: shape_kind = "Rect"; break;
                case CxShapeKind::Circle: shape_kind = "Circle"; break;
                case CxShapeKind::Ellipse: shape_kind = "Ellipse"; break;
                case CxShapeKind::Polyline: shape_kind = "Polyline"; break;
                case CxShapeKind::LineGauge: shape_kind = "LineGauge"; break;
                }
            }
            stream << "      \"shape_kind\": \"" << shape_kind << "\",\n";
            stream << "      \"owner_type\": \"" << e.owner_type << "\",\n";
            stream << "      \"owner_ref\": \"" << e.owner_ref << "\",\n";
            stream << "      \"owner_binding\": \"" << e.owner_binding << "\",\n";
            stream << "      \"semantic_role\": \"" << e.semantic_role << "\",\n";
            stream << "      \"editable\": " << (e.editable ? "true" : "false") << ",\n";
            stream << "      \"result_element\": " << (e.result_element ? "true" : "false") << ",\n";
            stream << "      \"runtime_bound\": " << (e.runtime_bound ? "true" : "false") << ",\n";
            stream << "      \"runtime_edit_pending\": " << (e.runtime_edit_pending ? "true" : "false") << ",\n";
            stream << "      \"stale\": " << (e.stale ? "true" : "false") << ",\n";
            stream << "      \"visible\": " << (e.visible ? "true" : "false") << "\n";
            stream << "    }";

            if (i < myShapeElements.size() - 1)
                stream << ",";
            stream << "\n";
        }

        stream << "  ]\n";
        stream << "}\n";

        reason = "shape_elements.json written";
        return true;
    }
    catch (const std::exception& ex)
    {
        reason = "WriteShapeElementsJson exception: " + std::string(ex.what());
        return false;
    }
}
