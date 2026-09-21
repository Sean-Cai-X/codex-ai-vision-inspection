#ifndef CXIMAGE_IMAGE_ANNOTATION_LAYER_H
#define CXIMAGE_IMAGE_ANNOTATION_LAYER_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_set>

#include "shapebase.h"
#include "CxAnnotationToolRuntime.h"
#include "CxParserSnapshotTypes.h"
#include "LineGaugeShape.h"
#include "RectShape.h"
#include "CircleShape.h"
#include "PolylineShape.h"

struct OverlayImagePoint
{
  float x = 0.0f;
  float y = 0.0f;
};

enum class OverlayKind
{
  Point,
  Line,
  Rect,
  Circle,
  Ellipse,
  Polyline,
  AutoBoundaryRequest,
  BoundaryPolyline
};

enum class ImageToolMode
{
  PointerPan,
  PointCreate,
  LineCreate,
  RectCreate,
  CircleCreate,
  EllipseCreate,
  PolylineCreate,
  MagicWandBoundary,
  AutoBoundary,
  AttachToScript
};

struct CxShapeElement
{
  int id = 0;
  std::string ref;
  std::string stable_ref;

  std::string tool_id;
  std::string owner_type;
  std::string owner_ref;
  std::string owner_binding;
  std::string semantic_role;
  std::string source_script;
  std::string source_statement;

  bool editable = true;
  bool visible = true;
  bool runtime_bound = false;
  bool result_element = false;
  bool stale = false;
  bool selected = false;
  bool runtime_edit_pending = false;

  uint64_t runtime_publish_generation = 0;

  std::unique_ptr<ShapeBase> shape;
};

struct OverlayElement
{
  int id = 0;
  std::string ref;
  std::string stable_ref;
  OverlayKind kind = OverlayKind::Point;
  std::string role;
  std::string semantic_role;
  std::string source;
  std::string owner_type;
  std::string owner_ref;
  std::string status = "created";
  std::string module_hint;
  std::vector<OverlayImagePoint> image_points;
  float radius = 0.0f;
  bool selected = false;
  bool visible = true;
  bool editable = true;
  bool runtime_bound = false;
  bool stale = false;
  uint64_t source_version = 0;
  std::string label;
  std::string evidence_ref;
  std::string generated_statement;
  std::string result_ref;
  std::string issue_entry_ref;
};

struct AnnotationToolDefinition
{
  std::string name;
  std::string label;
  std::string shape_type;
  std::string role;
  std::string source;
  std::string action;
  std::string module_hint;
  std::string description;
  std::string owner_tool;
  std::string owner_binding;
  bool manual_visible = true;
  bool editable = true;
  OverlayKind kind = OverlayKind::Point;
};

std::unique_ptr<ShapeBase> CreateInitialShapeForTool(
    const AnnotationToolDefinition& tool,
    const std::vector<CxShapePoint>& points);

class ICxShapeSink {
public:
    virtual ~ICxShapeSink() = default;

    virtual void UpsertShape(
        const std::string& stable_ref,
        const std::string& owner_type,
        const std::string& owner_ref,
        const std::string& owner_binding,
        const std::string& semantic_role,
        bool editable,
        bool result_element,
        std::unique_ptr<ShapeBase> shape) = 0;
};

struct CxShapeHitResult {
    bool hit = false;
    int element_index = -1;
    CxShapeHit shape_hit;
};

struct CxPointerEvent {
    enum class Type { Move, LeftDown, LeftDrag, LeftUp };
    Type type;
    double image_x;
    double image_y;
    uint64_t frame_no;
};

struct CxShapeCommitResult {
    bool committed = false;
    std::string owner_type;
    std::string owner_ref;
    std::string owner_binding;
    std::string semantic_role;
    std::string stable_ref;
    bool editable = false;
    bool result_marked_stale = false;
    int stale_result_count = 0;
    bool runtime_writeback = false;
    std::string reason;
};

struct CxImagePointerFrame {
    bool canvas_hovered = false;
    bool inside_image = false;
    bool left_clicked = false;
    bool left_down = false;
    bool left_released = false;
    bool right_clicked = false;
    bool escape_pressed = false;
    bool enter_pressed = false;
    bool pointer_moved = false;
    double image_x = 0.0;
    double image_y = 0.0;

    bool HasInteractionEvent() const
    {
        return left_clicked ||
               left_released ||
               right_clicked ||
               escape_pressed ||
               enter_pressed;
    }

    bool HasDragMoveEvent() const
    {
        return left_down && pointer_moved;
    }
};

struct CxImagePointerResult {
    bool consumed = false;
    std::string phase;
    std::string status;
    std::string reason;
    std::string created_ref;
    std::string selected_ref;
    CxShapeCommitResult commit;
};

struct CxShapeInteractionTrace {
    std::vector<CxPointerEvent> pointer_events;
    std::vector<CxShapeHitResult> hits;
    std::vector<CxShapeGeometrySnapshot> snapshots;
    std::string commit_reason;
    CxShapeCommitResult commit_result;

    bool hit_test_called = false;
    bool hit = false;
    bool begin_drag_called = false;
    bool begin_drag_ok = false;
    bool update_drag_ok = false;
    bool commit_called = false;
    bool commit_ok = false;
};

class ImageAnnotationLayer : public ICxShapeSink
{
public:
    bool ApplyToolManifestSnapshot(
        const CxAnnotationToolManifestSnapshot& snapshot,
        std::string& reason);

    bool ConvertToolSpec(
        const CxAnnotationToolSpec& spec,
        AnnotationToolDefinition& def,
        std::string& reason) const;
  bool SaveElements(const std::string& path, const std::string& imageRef,
                    std::string& reason) const;
  bool LoadElements(const std::string& path, std::string& imageRef,
                    std::string& reason);

    bool WriteShapeElementsJson(
        const std::filesystem::path& path,
        std::string& reason) const;

  OverlayElement& Create(OverlayKind kind, const std::string& role,
                         const std::string& source,
                         const std::string& moduleHint);
  OverlayElement* FindByStableRef(const std::string& ref);
  const OverlayElement* FindByStableRef(const std::string& ref) const;
  OverlayElement& UpsertByStableRef(const std::string& ref, OverlayKind kind);
  void RemoveByOwner(const std::string& owner_type, const std::string& owner_ref);
  void Clear();
  void Select(int index);

  std::vector<AnnotationToolDefinition>& Tools() { return myTools; }
  const std::vector<AnnotationToolDefinition>& Tools() const { return myTools; }
  const AnnotationToolDefinition* FindToolDefinition(const std::string& tool_id) const;
  std::vector<OverlayElement>& Elements() { return myElements; }
  const std::vector<OverlayElement>& Elements() const { return myElements; }

  int SelectedIndex() const { return mySelectedIndex; }
  OverlayElement* Selected();
  const OverlayElement* Selected() const;
  int ActiveToolIndex() const { return myActiveToolIndex; }
  void SetActiveToolIndex(int index) { myActiveToolIndex = index; }
  const AnnotationToolDefinition* ActiveTool() const;
  std::string SelectedRef(OverlayKind kind) const;

  static const char* KindName(OverlayKind kind);
  static bool ParseKind(const std::string& text, OverlayKind& kind);

  CxShapeElement& CreateFromTool(
      const AnnotationToolDefinition& tool,
      std::unique_ptr<ShapeBase> shape);

  CxShapeElement& UpsertShape(
      const std::string& stable_ref,
      std::unique_ptr<ShapeBase> shape);

  CxShapeElement* FindShapeByStableRef(const std::string& stable_ref);

  // Value-only update of the repository-owned FindCircle annulus.  The View
  // forwards a Key Parameter Controls request here; it never owns a second
  // circle or retains a parser/tool pointer.
  bool ApplyFindCircleAnnulusGauge(
      const std::string& owner_ref,
      double cx,
      double cy,
      double inner_radius,
      double outer_radius,
      bool scan_sector_enabled,
      double scan_sector_start_degrees,
      double scan_sector_end_degrees,
      std::string& reason);

  // A subsequent CxScript execution becomes the next runtime truth source.
  // Clear edit preservation just before that execution publishes fresh shapes.
  void ConfirmAllRuntimeWritebacks();

  CxShapeHitResult HitTest(double image_x, double image_y, double tolerance);

  bool BeginDrag(const CxShapeHitResult& hit, double image_x, double image_y);
  bool UpdateDrag(double image_x, double image_y);
  bool CommitEdit(std::string& reason);
  void CancelDrag();

  bool SimulateDragShape(
      const std::string& stable_or_ref,
      CxShapeHandleRole role,
      int vertex_index,
      double from_x,
      double from_y,
      double to_x,
      double to_y,
      std::string& reason);

  bool SimulatePointerDrag(
      double from_x,
      double from_y,
      double to_x,
      double to_y,
      int intermediate_steps,
      double tolerance,
      CxShapeInteractionTrace& trace);

  void RemoveShapeByOwner(const std::string& owner_type, const std::string& owner_ref);
  bool RemoveShapeByStableRef(const std::string& stable_ref);
  void MarkOwnerResultStale(const std::string& owner_type, const std::string& owner_ref);

    void RemoveRuntimeOwnersNotIn(const std::unordered_set<std::string>& liveOwners);

    void SelectShape(int index);
    CxShapeElement* SelectedShape();
    const CxShapeElement* SelectedShape() const;

    void EnumerateVisibleShapes(std::vector<const CxShapeElement*>& out) const;

    std::vector<CxShapeElement>& ShapeElements() { return myShapeElements; }
    const std::vector<CxShapeElement>& ShapeElements() const { return myShapeElements; }

    void ClearShapeElements();
    bool HasActiveDrag() const;
    std::size_t ShapeElementCount() const;

    bool CommitEdit(CxShapeCommitResult& result);
    int MarkOwnerResultStaleAndCount(const std::string& owner_type, const std::string& owner_ref);

    static int HandlePriority(CxShapeHandleRole role);

    uint64_t BeginRuntimeOwnerPublish(
        const std::string& owner_type,
        const std::string& owner_ref);

    void EndRuntimeOwnerPublish(
        const std::string& owner_type,
        const std::string& owner_ref,
        uint64_t generation);

    void ConfirmRuntimeWriteback(const std::string& stable_ref);
    void DiscardPendingRuntimeEdit(const std::string& stable_ref);

    void UpsertShape(
        const std::string& stable_ref,
        const std::string& owner_type,
        const std::string& owner_ref,
        const std::string& owner_binding,
        const std::string& semantic_role,
        bool editable,
        bool result_element,
        std::unique_ptr<ShapeBase> shape) override;

private:
  std::string MakeRef(OverlayKind kind, int id) const;

  std::vector<AnnotationToolDefinition> myTools;
  std::vector<OverlayElement> myElements;
  std::vector<CxShapeElement> myShapeElements;
  int myNextId = 1;
  int mySelectedIndex = -1;
  int myActiveToolIndex = -1;
  int mySelectedShapeIndex = -1;

  int myDraggingElement = -1;
  CxShapeHandleRole myDraggingRole = CxShapeHandleRole::None;
  int myDraggingVertexIndex = -1;
  double myDragStartX = 0.0;
  double myDragStartY = 0.0;
  double myDragLastX = 0.0;
  double myDragLastY = 0.0;
  bool myDragHasPosition = false;

  uint64_t myRuntimePublishGeneration = 0;
  uint64_t myActiveRuntimePublishGeneration = 0;
  std::string myActivePublishOwnerType;
  std::string myActivePublishOwnerRef;

  std::string myLoadStatus;
};

#endif
