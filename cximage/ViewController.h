#ifndef _ViewController_Header
#define _ViewController_Header

#include "Window.h"

#include <AIS_InteractiveContext.hxx>
#include <AIS_ViewController.hxx>
#include <V3d_View.hxx>

#include <AIS_Manipulator.hxx>

#include <map>
#include <Standard_Handle.hxx>
#include <string>
#include <vector>
#include <imgui.h>

#include "View.h"
#include "Image.h"

#include "Shape.h"
#include "shapebase.h"
#include "FindLine.h"

#include "FastMatch.h"
#include "imagemanager.h"
#include "ParserClass.h"
#include "ParserDebugBridge.h"
#include "muParser.h"
#include "ManualStateTestConsole.h"
#include "CxBusinessWorkflowPanel.h"
#include "CxEvidenceSelfTestRuntime.h"
#include "ImageAnnotationLayer.h"
#include "SemanticFlowGraph.h"
#include "CxShapeInteractionTest.h"
#include "CxParserRuntimeOwner.h"

//! OCCT view controller hosting interaction, image tools, and shape display.
class ViewController : protected AIS_ViewController
{
  struct ScriptCatalogEntry
  {
    std::string name, path, type, status, description;
  };

  struct ScriptResult
  {
    std::string source, script_path, status, reason, runtime_fillback_status;
    std::string image_ref, overlay_ref, result_ref, evidence_ref, issue_entry_ref;
    double elapsed_ms = 0.0;
    std::vector<std::string> log_lines;
  };

  struct SemanticExecutionContext
  {
      bool valid = false;
      bool from_evidence = false;

      std::string node_id;

      std::string case_id;

      std::string script_id;
      std::string script_path;

      std::string image_id;
      std::string image_path;

      std::string target_id;
      std::string tool;

      std::string parameter_profile_id;
      std::string parameter_summary;

      std::string reason;
  };

public:
  //! Construct the controller with default interaction state.
  ViewController();

  //! Release controller resources.
  ~ViewController();

  //! Run the application event loop.
  void run();

  bool LoadBoundStateToManualConsole(
      const std::string& nodeId,
      const std::string& scriptPath,
      std::string& reason);

    bool LoadSemanticEvidenceBindingToManualConsole(
        const std::string& nodeId,
        const SemanticEvidenceBinding& binding,
        bool loadImageToView,
        std::string& reason);

    bool BuildSemanticExecutionContext(
        const SemanticFlowAction& action,
        SemanticExecutionContext& out,
        std::string& reason) const;

    bool ApplySemanticExecutionContextBeforeRun(
        const SemanticExecutionContext& context,
        std::string& reason);

    bool ApplyEvidenceParameterSummaryToRuntimeGlobals(
        const std::string& parameterSummary,
        std::string& reason);

    bool ApplyEvidenceParameterSummaryToRuntimeGlobals(
        ManualTestContext& context,
        const std::string& parameterSummary,
        std::string& reason);

    bool RunEvidenceChainSelfTest(
        const CxEvidenceSelfTestRequest& request,
        CxEvidenceSelfTestResult& result,
        std::string& reason);

    bool ResolveEvidenceSelfTestSnapshot(
        const CxEvidenceSelfTestRequest& request,
        CxEvidenceSelectionSnapshot& snapshot,
        std::string& reason);

    bool CheckEvidenceSelfTestImageBinding(
        const CxEvidenceSelectionSnapshot& snapshot,
        std::string& reason) const;

    bool CheckEvidenceSelfTestParamBinding(
        const CxEvidenceSelectionSnapshot& snapshot,
        std::string& reason) const;

    bool RunEvidenceSelfTestParserCompileStage(
        const CxEvidenceSelectionSnapshot& snapshot,
        CxEvidenceSelfTestResult& result,
        std::string& reason);

    bool CheckEvidenceSelfTestGlobalInjection(
        const CxEvidenceSelectionSnapshot& snapshot,
        std::string& reason);

    bool RunEvidenceSelfTestRuntimeExecuteStage(
        const CxEvidenceSelectionSnapshot& snapshot,
        CxEvidenceSelfTestResult& result,
        std::string& reason);

    bool CheckEvidenceSelfTestRuntimeObjectStage(
        const CxEvidenceSelectionSnapshot& snapshot,
        CxEvidenceSelfTestResult& result,
        std::string& reason);

    bool RunEvidenceSelfTestProjectionStage(
        const CxEvidenceSelectionSnapshot& snapshot,
        CxEvidenceSelfTestResult& result,
        std::string& reason);

    bool CheckEvidenceSelfTestResultProjectionStage(
        const CxEvidenceSelectionSnapshot& snapshot,
        CxEvidenceSelfTestResult& result,
        std::string& reason);

    bool BuildEvidenceSelfTestBatchFromCurrentEvidenceRows(
        CxEvidenceSelfTestBatchRequest& request,
        std::string& reason) const;

    bool RunEvidenceSelfTestBatch(
        const CxEvidenceSelfTestBatchRequest& request,
        CxEvidenceSelfTestBatchResult& result,
        std::string& reason);

    bool InitEvidenceSelfTestEnvironment(std::string& reason);

    bool WriteEvidenceChainCatalogSemanticSelfTest(
        const std::string& outDir,
        std::string& reason);

    // Serial UI-chain smoke for Torch evidence -> image set -> annotation
    // snapshot -> label-package export.  It does not invoke model training.
    bool RunTorchTrainingLabelPackageSmoke(
        const std::string& preferredScriptId,
        const std::string& outDir,
        std::string& packagePath,
        std::string& reason);

private:
  void initImGui();
  bool GetSelectedCatalogScript(std::string& outPath,
                                std::string& outName) const;
  void HandleSemanticFlowAction(const SemanticFlowAction& action);
  void drawScriptAcceptancePanels();
  void EnsureCxScriptWorkbenchAssetsLoaded();
  void EnsureEvidenceChainThumbnailsLoaded();
  void SelectEvidenceChainThumb(int index);
  void DrawEvidenceChainThumbnailRail();
  void RebuildScriptEvidenceGroups();
  void RebuildScriptEvidenceRowRefs();
  std::string ResolveImagePathFromManifest(const std::string& imageId) const;
  void EnsureScriptEvidenceThumbTexture(ScriptEvidenceThumb& thumb);
  void SelectScriptEvidenceThumb(int groupIndex, int thumbIndex);

  bool GetSelectedEvidenceSnapshot(
      CxEvidenceSelectionSnapshot& out,
      std::string& reason) const;

  bool BuildEvidenceSnapshotFromThumb(
      int groupIndex,
      int thumbIndex,
      const ScriptEvidenceThumb& thumb,
      CxEvidenceSelectionSnapshot& out,
      std::string& reason) const;

  bool ApplyEvidenceSelectionSnapshotToManualContext(
      const CxEvidenceSelectionSnapshot& snapshot,
      bool loadImageToView,
      std::string& reason);

    bool RefreshEvidenceSelectionFromThumb(
        int groupIndex,
        int thumbIndex,
        bool loadImageToView,
        std::string& reason);

    void ResetEvidenceThumbTexture(ScriptEvidenceThumb& thumb);

  void DrawOneScriptEvidenceRow(
      int groupIndex,
      int thumbIndex,
      ScriptEvidenceThumb& thumb,
      float rowHeight);
  void DrawScriptEvidenceThumbnailRailByGroup();
  void EnsureTorchTrainingImageTexture(TorchTrainingImageItem& item);
  void SyncTorchTrainingImageSetFromEvidenceSelection();
  int AddHDReferenceImageSetForCurrentSelection();
  int AddGeometryAugmentationDatasetForCurrentSelection();
  bool RunGeometryAugmentationTrainingPrepFromGui(std::string& reason);
  bool RunYoloV8nIncrementalTrainingFromGui(std::string& reason);
  bool RunYoloV8nSelectedImageInferenceFromGui(std::string& reason);

  void CaptureCurrentTorchTrainingAnnotationState();
  void RestoreTorchTrainingAnnotationState(
      const TorchTrainingImageItem& item);
  bool LoadTorchTrainingImageIntoAnnotationView(
      int itemIndex,
      std::string& reason);
  void AddTorchTrainingImageFromPath(
      const std::string& imagePath,
      const std::string& imageId,
      const std::string& split,
      const std::string& label,
      const std::string& source);
  bool ExportTorchTrainingLabelPackage(
      std::string& packagePath,
      std::string& reason);
  void DrawTorchTrainingImageRail(const char* split, const char* label);
    void DrawTorchEvidenceTrainingPanel();
    void DrawEvidenceWorkflowPanel();
  bool LoadImageIntoImageView(const std::string& imagePath,
                              std::string& reason);
  bool LoadImageForEvidenceSelfTest(const std::string& imagePath,
                                    std::string& reason);
  bool ActivateScriptEvidenceThumb(const ScriptEvidenceThumb& thumb,
                                   bool loadImageToView,
                                   std::string& reason);
  std::string ResolveCatalogScriptPathById(const std::string& scriptId) const;
  std::string ResolveCatalogScriptLabelById(const std::string& scriptId) const;
  void DrawScriptEditorBlock(ManualTestContext& context);
  void DrawScriptDebugCompilerBlock(ManualTestContext& context);
  bool ConsumePendingManualScriptRun(ManualTestContext& context,
                                     const char* trigger);
  void DrawCxParserExtLineViewsPanel(ManualTestContext& context);
  void DrawCxParserExtStatementViewsPanel(ManualTestContext& context);
  void DrawCxParserExtObjectAssignmentsPanel(ManualTestContext& context);
  void drawManualStateTestConsole();
  void initManualStateTestConsole();
  void initScriptCatalog();
  void initImageEvidenceLayer();
  void drawImageEvidencePanels();
  void drawKeyParameterControlsWindow();
  void drawMetrologyAnalyticsSmokeWindow();
  bool ApplyCurrentGaugeToEditableShape(std::string& reason);
  bool ProjectCurrentGaugeToImageViewPreview(std::string& reason);
  bool SyncFindSegmentationPromptListsFromShapeElements(std::string& reason);
  void drawParameterTuningAndConclusionWindow();
  void drawTorchRuntimeEvidenceWindow();
  void drawTorchTrainingImageSetWindow();
  void drawEvidenceAlbumWindow();
  void drawAnnotationToolWindow();
  void DrawAnnotationToolButtonStrip(bool horizontal);
  void drawImageEvidenceOnCanvas(bool canvasHovered, bool canvasActive,
                                 ImDrawList* drawList);
  void DrawImguiBackgroundBackplate();
  void ClearMainFramebuffer();
  ImVec2 ImageToScreen(float ix, float iy) const;
  ImVec2 ScreenToImage(float sx, float sy) const;
  ImVec2 ImageToScreenPoint(const OverlayImagePoint& p) const;
  ImVec2 ScreenToImagePoint(const ImVec2& p) const;
  ScriptResult RunCxScript(const std::string& theScriptPath);
  bool PrepareTorchUiRequestContext(const std::string& scriptText,
                                    const std::string& scriptPath,
                                    std::string& reason);
  void RefreshRuntimeObjectTable(const std::string& lastMethod,
                                 const std::string& runtimeStatus);
  void RequestRuntimeShapeSync(const std::string& reason);
  void ProcessDeferredRuntimeShapeSync(const char* phase);
  void SyncRuntimeObjectsToShapeElements();
  bool QueryParserObjectExists(const std::string& type,
                               const std::string& name);
  Image* QueryParserImage(const std::string& name);
  bool QueryParserDouble(const std::string& name, double& value);
  bool SetParserDouble(const std::string& name, double value);
  void FitAll();
  double GetScale();
  void ViewerUpDate();
  void DisplayShape(const Handle(AIS_InteractiveObject)& AShape,
                    Standard_Boolean isShow = Standard_True);
  void RemoveAllShapes(Standard_Boolean isUpDate = Standard_False);

  //! Create the GLFW host window.
  void initWindow(int theWidth, int theHeight, const char* theTitle);

  //! Create 3D Viewer.
  void initViewer(int theWidth, int theHeight);

  //! Populate the 3D viewer with demo content.
  void initDemoScene();
  //! Application event loop.
  void mainloop();

  //! Clean up runtime resources.
  void cleanup();

//! @name GLWF callbacks
private:
  //! Handle window resize.
  void onResize(int theWidth, int theHeight);

  //! Handle mouse wheel input.
  void onMouseScroll(double theOffsetX, double theOffsetY);

  //! Handle mouse button changes.
  void onMouseButton(int theButton, int theAction, int theMods);

  //! Handle mouse motion.
  void onMouseMove(int thePosX, int thePosY);


  bool isManipulating = false;
  Standard_Integer startX, startY;

  bool isGDragging = false;
  bool isDragging = false;
  bool ismove = false;
  Graphic3d_Vec2i dragStart;
  gp_Pnt initialShapePosition;
  Handle(AIS_Shape) selectedShape;
  Handle(AIS_TextLabel) selectedTextLabel;
//! @name GLWF callbacks (static functions)
private:

  //! GLFW callback redirecting messages into Message::DefaultMessenger().
  static void errorCallback (int theError, const char* theDescription);

  //! Wrapper for glfwGetWindowUserPointer() returning this class instance.
  static ViewController* toView (GLFWwindow* theWin);

  //! Window resize callback.
  static void onResizeCallback (GLFWwindow* theWin, int theWidth, int theHeight)
  { toView(theWin)->onResize (theWidth, theHeight); }

  //! Frame-buffer resize callback.
  static void onFBResizeCallback (GLFWwindow* theWin, int theWidth, int theHeight)
  { toView(theWin)->onResize (theWidth, theHeight); }

  //! Mouse scroll callback.
  static void onMouseScrollCallback (GLFWwindow* theWin, double theOffsetX, double theOffsetY)
  { toView(theWin)->onMouseScroll (theOffsetX, theOffsetY); }

  //! Mouse click callback.
  static void onMouseButtonCallback (GLFWwindow* theWin, int theButton, int theAction, int theMods)
  { toView(theWin)->onMouseButton (theButton, theAction, theMods); }

  //! Mouse move callback.
  static void onMouseMoveCallback (GLFWwindow* theWin, double thePosX, double thePosY)
  { toView(theWin)->onMouseMove ((int )thePosX, (int )thePosY); }

private:

  Handle(Window) myOcctWindow;
  bool m_glfwInitialized = false;
  bool m_imguiContextInitialized = false;
  bool m_imguiGlfwInitialized = false;
  bool m_imguiOpenGlInitialized = false;
  Handle(V3d_CustomView) m_myView;
  Handle(AIS_InteractiveContext) myContext;
  Handle(AIS_Shape) aBox;

public:
    std::string view_Name;
    std::string focuEleUid;
    int st_Width;
    int st_Height;

    bool IsAnnotationCreateModeActive() const;
    CxImagePointerResult ProcessImageAnnotationPointerFrame(
        const CxImagePointerFrame& frame);
    bool CommitDraftShapeFromTool(
        const AnnotationToolDefinition& tool,
        CxImagePointerResult& out);
    bool BuildMagicWandPreview(double imageX, double imageY,
                               std::string& reason);
    bool BuildMagicWandFormFitNodes(const std::vector<cv::Point>& contour,
                                    std::string& reason);
    bool BuildSegmentationPromptBoundaryPreview(std::string& reason);
    bool CommitSegmentationPromptBoundary(CxImagePointerResult& out);
    bool CommitMagicWandPreview(CxImagePointerResult& out);
    bool BuildOpenBoundaryFormFit(const std::vector<CxShapePoint>& points,
                                  std::string& reason);
    bool CommitOpenBoundaryFormFit(CxImagePointerResult& out);
    bool WriteBusinessAnnotationReceipt(
        const CxShapeElement& element, const AnnotationToolDefinition& tool,
        const std::string& operation, const std::string& status,
        const std::string& trace, std::string& receiptPath,
        std::string& reason) const;
    void ClearMagicWandPreview(const std::string& status);

    bool IsMouseInsideImageCanvas(const ImVec2& p) const;
    static const char* ImageToolModeName(ImageToolMode mode);
    void CancelAnnotationCreate();
    void ClampImagePointToImageBounds(double& x, double& y) const;
    void HandleLineAnnotationInput(const ImVec2& mouseImage);
    void CommitLineAnnotation();
    void HandleRectAnnotationInput(const ImVec2& mouseImage);
    void CommitRectAnnotation();
    void HandleCircleAnnotationInput(const ImVec2& mouseImage);
    void CommitCircleAnnotation();

    bool TestLoadAnnotationToolManifest(const std::string& path, std::string& reason);
    bool TestApplyAnnotationToolManifestSnapshot(const CxAnnotationToolManifestSnapshot& snapshot, std::string& reason);
    bool TestSetActiveAnnotationTool(const std::string& tool_id, std::string& reason);
    void TestEnableAnnotationCreateMode();
    void TestSetActiveToolKind(OverlayKind kind);
    void TestSetToolModePointerPan();
    std::size_t TestShapeElementCount() const;
    bool TestGetLastPointerResult(CxImagePointerResult& out) const;
    std::string TestShapeKindByRef(const std::string& ref) const;

    void DrawShapeElementOnImageView(const CxShapeElement& element, ImDrawList* drawList);

    bool RunShapeInteractionSmoke(
        const std::string& tool_manifest_path,
        const std::string& suite_path,
        const std::string& image_manifest_path,
        const std::string& out_dir,
        CxShapeInteractionBatchResult& result);

    struct CxImageViewTransform
    {
        double canvas_x = 0.0;
        double canvas_y = 0.0;
        double image_x = 0.0;
        double image_y = 0.0;
        double zoom_x = 1.0;
        double zoom_y = 1.0;

        CxShapePoint ScreenToImage(double sx, double sy) const;
        CxShapePoint ImageToScreen(double ix, double iy) const;
    };

    CxImageViewTransform GetImageViewTransform() const;

    bool HasActiveFindCircleGauge() const;
    bool HasEditableFindCircleGauge() const;

    std::map<std::string, Handle(AIS_InteractiveObject)> occ_Shapes;
    V3d_TypeOfOrientation viewPort;
private:
    //! Convert screen coordinates into the current view space.
    gp_Pnt ConvertClickToPoint(Standard_Real theX, Standard_Real theY, Handle(V3d_View) theView);
    gp_Pnt m_mousePt;
    gp_Pnt GetViewCenter();

protected:
    enum CurrentAction3d
    {
        CurAction3d_Nothing,
        CurAction3d_DynamicPanning,
        CurAction3d_DynamicZooming,
        CurAction3d_DynamicRotation,
    };
    Standard_Integer m_x_max;
    Standard_Integer m_y_max;
    float m_scale;
    CurrentAction3d m_current_mode;
    bool leftMouseBtn = false;
    bool midMouseBtn = false;
    gp_Pnt mouseDownPT;
    gp_Pnt m_Pt;

    unsigned int texture_id0 = 0;
    cv::Mat s_img0;

    bool m_imageshow = false;

    bool m_planerotate = false;

    int m_iruntimes = 0;

    bool m_ipickpoints = false;

    bool m_ilinescan = false;

    bool m_iattachline = false;

    bool m_imguiMouseCaptureActive = false;
    bool m_imguiDragActive = false;
    bool m_imguiDragWasActive = false;
    int m_forceRepaintFrames = 0;
    double m_lastUiRepaintTime = 0.0;
    bool m_debugDrawManualCircle = false;

    static bool opencvSW;
    static bool opencvblur;
    static bool opencvreset;
    static bool gpublur;
    static bool irunedge;
    static bool ihedge;
    static bool iwedge;
    static bool ib2wedge;
    static bool iw2bedge;
    static bool ipythre;
    static bool iotsuThreshold;

    static int ivalue1;
    static int ivalue2;
    static int ivalue3;
    static int ivalue4;
    static int ivalue5;
    static int ivalue6;
    static int ivalue7;
    static int ivalue8;
    static int ivalue9;
    static int ivalue10;
    static int ivalue11;
    static int ivalue12;

    //! Create a GPU texture from a cv::Mat image.
    unsigned int CreateTextureFromMat0(const cv::Mat& mat);
    void UpdateImageViewImage(const cv::Mat& image);
    void SetTexturedtoBoxFace(const cv::Mat& image);
    void SetTexturedtoPlane(const cv::Mat& image);
    Handle(Image_PixMap) ConvertCvMatToOcctImage(const cv::Mat& mat);
    vector<float> createGaussianKernel(int kernelSize, double sigma);
    void Imgui_OpenCV_Ini0();

    void Imgui_GPU_NLM_main0(cv::Mat& src_host, cv::Mat& dst_host);
    void Imgui_GPU_NLM_main(cv::Mat& src_host, cv::Mat& dst_host);
    void Imgui_GPU_Gauss_main(cv::Mat& src_host, cv::Mat& dst_host);
    void Imgui_imageDenoising(cv::Mat& src_host, cv::Mat& dst_host);
    void Imgui_OpenCV_Window0(bool* p_open);

    void AdjustModelBoundingBoxToImageSize(const Handle(V3d_View)& myView, const Standard_CString imagePath);

    Handle(Graphic3d_Texture2D) CreateTextureFromImage(const Handle(Image_PixMap)& image);
    void SetBackgroundInView(Handle(V3d_View)& view, const cv::Mat& image);

    Image m_occtimage;

    int m_ibtntimes = 0;
    gp_Pnt m_point0;
    gp_Pnt m_point1;

    Shape m_shape0;

    FastMatch m_Match;

    void drawline();
    void ReduceView();
    void HomeView();
private:
    enum class AiGuiDestination {
      None,
      EvidenceImageSet,
      EvidenceCase,
      ManualReviewEvidence,
      ManualReviewDecision,
      ImageAnnotation,
      KeyParameters,
      TorchRuntimeEvidence,
      TorchTrainingImageSet,
      ParameterConclusion,
      ManualScriptConsole,
      AnalyticsSmoke
    };

    void ProcessAiGuiShortcuts();
    void DrawAiGuiNavigationOverlay();
    void RequestAiGuiDestination(AiGuiDestination destination);
    bool ApplyAiGuiFocusHere(AiGuiDestination destination,
                             const char* semanticLocation);
    const char* AiGuiShortcutName(AiGuiDestination destination) const;
    const char* AiGuiWindowName(AiGuiDestination destination) const;

    AiGuiDestination m_aiGuiPendingFocus = AiGuiDestination::None;
    AiGuiDestination m_aiGuiCurrentDestination = AiGuiDestination::None;
    bool m_showAiGuiShortcutHelp = false;
    std::string m_aiGuiSemanticLocation =
        "No shortcut used yet; press F1 for AI GUI navigation.";

    void setCurrentShape(Shape* pshape);
    Shape* indexOf(const string& shapeName);
    Shape* indexAt(const gp_Pnt& pos);
    string uniqueName(const string& name);

    
    ParserDebugBridge m_parserDebugBridge;

    std::ostringstream m_os;
    std::ostringstream m_createcodeos;

    void clearos();
    void clearcreateos();
    void clearparserobject();
    void resetparser();
    void SetParserValue(const string& codestr, double dvalue);
    double GetParserValue(const string& codestr);
    string initialparser();
    string getoutputstring() { return m_os.str(); }

    string m_strcode;

    double m_dmovx = 0;
    double m_dmovy = 0;
    double m_dangle = 0;
    double m_dzoomx = 1;
    double m_dzoomy = 1;

    double m_dscalex = 1;
    double m_dscaley = 1;

    gp_Pnt m_mousePressPos;

    gp_Pnt m_mousePressOffset;
    Shape* m_pmousepressshape = nullptr;
    bool m_resizeHandlePressed;

    float m_current_window_posx;
    float m_current_window_posy;
    float m_imguiw;
    float m_imguih;
    std::vector<ScriptCatalogEntry> m_scriptCatalog;
    int m_selectedScript = -1;
    ScriptResult m_scriptResult;
    bool m_scriptRunRequested = false;
    bool m_showAllScripts = false;
    int m_evidenceChainUiSection = 0;
    bool m_showLegacyGpuWork = false;
    bool m_detachablePanels = false;
    bool m_showManualStateTestConsole = true;
    bool m_showBusinessWorkflowPanel = false;
    CxBusinessWorkflowPanel m_businessWorkflowPanel;
    ManualTestContext m_manualTest;
    CxParserRuntimeOwner m_parserOwner;
    ImageAnnotationLayer m_annotationLayer;
    std::vector<ScriptSnippet> m_manualSnippets;
    std::vector<ScriptSnippet> m_directTestModules;
    std::vector<DirectCapability> m_directCapabilities;
    SemanticFlowGraph m_semanticFlowGraph;
    bool m_renderImageInOcctBackground = false;
    bool m_showTestPoints = false;
    bool m_showTestRectangle = false;
    bool m_showTestScanLine = false;
    unsigned int m_imageViewTexture = 0;
    cv::Mat m_imageViewImage;
    float m_imageViewZoom = 1.0f;
    float m_imageViewPanX = 0.0f;
    float m_imageViewPanY = 0.0f;
    float m_annotationImagePosX = 0.0f;
    float m_annotationImagePosY = 0.0f;
    float m_annotationImageWidth = 1.0f;
    float m_annotationImageHeight = 1.0f;
    bool m_annotationDragging = false;
    OverlayKind m_annotationDragKind = OverlayKind::Point;
    OverlayImagePoint m_annotationDragStart;
    OverlayImagePoint m_annotationDragEnd;

    struct AnnotationDraft
    {
        bool active = false;
        OverlayKind kind = OverlayKind::Point;
        std::vector<ImVec2> points;
        void clear() { active = false; kind = OverlayKind::Point; points.clear(); }
    };
    AnnotationDraft m_annotationDraft;

    int m_activePolylineElement = -1;
    std::vector<CxShapePoint> m_activePolylinePoints;
    // A local, deterministic boundary proposal.  This intentionally remains
    // separate from FindSegmentation/EdgeSam: it uses only the loaded image
    // and a human-selected seed point, then requires an explicit accept.
    std::vector<CxShapePoint> m_magicWandPreviewPoints;
    std::vector<CxShapePoint> m_magicWandRawBoundaryPoints;
    OverlayImagePoint m_magicWandSeed;
    int m_magicWandAlgorithm = 0;
    int m_magicWandColorTolerance = 24;
    bool m_magicWandEightConnected = true;
    float m_magicWandSimplifyPixels = 1.5f;
    int m_magicWandMinimumRegionPixels = 32;
    int m_magicWandNodeizationMode = 0;
    int m_magicWandMaximumNodes = 64;
    float m_magicWandMinimumNodeSpacingPixels = 4.0f;
    // Business-facing anchor-selection policy for closed boundaries.  The
    // seed/prompt still drives segmentation; this selects a reproducible key
    // point on the resulting contour for review, FormFit and receipts.
    int m_boundaryAnchorMode = 0;
    OverlayImagePoint m_boundaryAnchor;
    int m_magicWandRegionPixels = 0;
    int m_magicWandRawBoundaryPointCount = 0;
    int m_magicWandFormFitNodeCount = 0;
    std::string m_magicWandStatus = "MAGIC_WAND_IDLE";
    std::string m_magicWandFormFitStatus = "FORMFIT_NODE_IDLE";
    int m_promptBoundaryGrabCutIterations = 3;
    std::vector<CxShapePoint> m_openBoundaryRawPoints;
    std::vector<CxShapePoint> m_openBoundaryPreviewPoints;
    std::string m_openBoundaryType = "open_curve";
    double m_openBoundaryLineResidualPx = 0.0;
    double m_openBoundaryArcResidualPx = 0.0;
    double m_openBoundaryFitResidualPx = 0.0;
    std::string m_openBoundaryStatus = "OPEN_BOUNDARY_IDLE";
    bool m_attachToScriptMode = false;
    bool m_showSourcePreviewOverlay = false;
    ImageToolMode m_imageToolMode = ImageToolMode::PointerPan;
    bool m_imageToolEnabled = false;
    bool m_annotationCreateActive = false;
    bool m_blockOccMouseInputThisFrame = false;
    bool m_blockImagePanThisFrame = false;
    ImVec2 m_imageCanvasMin;
    ImVec2 m_imageCanvasMax;
    bool m_forceClearFramebuffer = true;
    bool m_forceRedrawWhileDraggingUi = true;
    bool m_enableImguiBackgroundBackplate = false;
    bool m_enableAnnotationToolV2 = false;
    bool m_enableFindSegmentationUi = false;
    std::string m_annotationManifestPath;
    std::string m_annotationSessionPath;
    std::string m_annotationStatus;
    std::string m_annotationDebugHandler = "none";
    std::string m_annotationDebugCommit = "none";
    std::string m_annotationDebugReject = "none";
    int m_annotationDebugCommitCount = 0;

    bool m_debugImageViewHovered = false;
    bool m_debugMouseInImage = false;
    float m_debugMouseImageX = 0.0f;
    float m_debugMouseImageY = 0.0f;
    bool m_debugAnnotationDragging = false;
    CxImagePointerResult m_lastPointerResult;
    bool m_runtimeShapeSyncPending = false;
    std::string m_runtimeShapeSyncReason;
    int m_runtimeShapeSyncDeferCount = 0;
    bool m_runtimeShapeSyncExecuting = false;
};

#endif // _ViewController_Header
