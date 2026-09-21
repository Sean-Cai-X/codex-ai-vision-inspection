# Torch N1-N8 Evidence Chain Progress - 2026-07-30

## Scope

This note records the current `TorchTask -> runtime -> artifacts -> CxInferenceResult -> Shape/UI -> result_summary` chain for `cxvision_imgui_acceptance`.

No semantic model-quality acceptance is claimed here. The current segmentation weight is a smoke weight and the YOLOv8 smoke run produced zero detections.

## Environment

- Repo: `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo`
- Build Dir: `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/build01`
- Binary: `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/build01/Release/cxvision_imgui_acceptance.exe`
- Binary Timestamp: `2026/7/30 10:00:57`
- Working Directory: `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo`
- Unified Log: `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/_shared/cxvision_imgui_acceptance.jsonl`

## Catalog Update

Added manual-visible TorchTask smoke entries to:

```text
cxparser/cxscript/module/cximage/catalog/cximage_catalog.cxsc
```

New entries:

```text
torch_segmentation_cpp_state_dict_cpu_direct
torch_detection_yolov8_cpu_smoke_direct
```

These entries expose the real `TorchTask` direct scripts in the Script Catalog / template workflow, rather than only exposing the FindSegmentation libtorch contract wrapper.

## N1-N8 Status

| Step | Goal | Current Result | Conclusion |
|---|---|---|---|
| N1 | Re-run T5 segmentation smoke on current build | `ctest` 2/2 passed; headless `cxscript_headless_ok=true` | `HEADLESS_EXECUTION_PASS` |
| N2 | Verify `result_summary.json` refs/metrics | `torch_ok=true`, `segmentation_mask_ref`, `segmentation_overlay_ref`, `segmentation_contour_ref`, `segmentation_result_ref` all populated | `TORCH_SEGMENTATION_ARTIFACT_PASS` |
| N3 | Verify `TorchRuntimeResultAdapter` fills mask refs | `mask_binary.png`, `mask_overlay.png`, `contours.json` attached through `TorchTask` capture | `TORCH_RESULT_ADAPTER_READY` |
| N4 | Verify `CxTorchResultProjector` shape output | Shape refs include `m_task.segmentation_mask` and `m_task.segmentation_contour`; contour polyline has 4 points | `SHAPE_PROJECTION_PASS` |
| N5 | Add scripts to Catalog / CxScript Template path | Catalog entries added for segmentation and detection TorchTask direct scripts | `ASSET_PREFLIGHT_PASS` |
| N6 | UI observability smoke | Not run automatically in this batch; data is ready for UI panel display | `PENDING_HUMAN_REVIEW` |
| N7 | FindSegmentation backend bridge | `FindSegmentation` `libtorch_segmentation` backend now calls the production segmentation task and publishes boundary shapes | `HEADLESS_EXECUTION_PASS` |
| N8 | One-shot manual review checklist | Prepared below; not performed by automation | `PENDING_HUMAN_REVIEW` |

## N1-N8 Latest Serial Closure

Latest binary:

```text
D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/build01/Release/cxvision_imgui_acceptance.exe
timestamp: 2026/7/30 16:45:15
```

Latest code-level closure:

```text
TorchTask exposes train/evidence/mainline readback getters.
CxInferenceResult carries trainer_lifecycle_summary and unified_mainline_summary as value semantics.
Headless summary/object_state/log record torch_train_ms, torch_total_ms, torch_evidence_ref,
torch_primary_visual_ref, torch_trainer_lifecycle_summary and torch_unified_mainline_summary.
Windows TorchRuntimeBridge no longer eagerly FreeLibrary()s libtorch_module_runtime.dll after a task;
the runtime handle is destroyed, while DLL unload is deferred to process exit to avoid libtorch/autograd
lifecycle faults after training smoke.
```

Latest headless verification:

| Chain | Script | Run Dir | Conclusion |
|---|---|---|---|
| Segmentation inference | `cxparser/cxscript/module/torch/torch_segmentation_cpp_state_dict_cpu_direct.cxsc` | `run_20260730_N1N8_torch_segmentation_cpp_state_dict_cpu_v2` | `HEADLESS_EXECUTION_PASS` |
| Training lifecycle | `cxparser/cxscript/module/torch/torch_train_lifecycle_direct_test.cxsc` | `run_20260730_N1N8_torch_train_lifecycle_smoke_v3` | `HEADLESS_EXECUTION_PASS` |
| Detection runtime | `cxparser/cxscript/module/torch/torch_detection_yolov8_cpu_smoke_direct.cxsc` | `run_20260730_N1N8_torch_detection_yolov8_cpu_smoke_v2` | `HEADLESS_EXECUTION_PASS` |
| FindSegmentation libtorch backend | `cxparser/cxscript/module/cximage/headless/find_segmentation_libtorch_smoke_direct.cxsc` | `run_20260730_N1N8_find_segmentation_libtorch_backend_v2` | `HEADLESS_EXECUTION_PASS` |

Training lifecycle artifact facts:

```text
task=torch.train.segmentation.lifecycle_smoke.v1
training_stage=tiny_smoke
epochs=1
finite_loss=true
semantic_quality=not_evaluated
```

Training lifecycle summary fields now visible in `result_summary.json`:

```text
torch_train_ms
torch_total_ms
torch_evidence_ref
torch_primary_visual_ref
torch_trainer_lifecycle_summary
torch_unified_mainline_summary
```

Detection remains limited:

```text
torch_ok=true
torch_result_count=0
Conclusion: detection runtime smoke is pass, rectangle projection is PENDING_DATA.
```

## Segmentation Headless Evidence

Command:

```powershell
D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/build01/Release/cxvision_imgui_acceptance.exe `
  --headless `
  --cxscript-headless `
  --image D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/test_images/L1_high_contrast/line_high_contrast_001.jpg `
  --script cxparser/cxscript/module/torch/torch_segmentation_cpp_state_dict_cpu_direct.cxsc `
  --case-name N1_torch_segmentation_cpp_state_dict_cpu `
  --out D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/headless/run_20260730_N1_torch_segmentation_cpp_state_dict_cpu `
  --max-steps 10000 `
  --unified-log D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/_shared/cxvision_imgui_acceptance.jsonl
```

Headless output:

```text
cxscript_headless_ok=true
summary=D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/headless/run_20260730_N1_torch_segmentation_cpp_state_dict_cpu/result_summary.json
```

Runtime artifact directory:

```text
D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/headless/T5_torch_segmentation_cpp_state_dict_cpu
```

Required artifacts observed:

```text
torch_segmentation_task_request.json
torch_segmentation_task_result.json
torch_runtime_evidence.json
mask_labels.png
mask_binary.png
mask_overlay.png
contours.json
segmentation_metrics.json
```

Summary facts observed:

```text
torch_ok=true
torch_status=success
segmentation_result_ref=<...>/torch_segmentation_task_result.json
segmentation_mask_ref=<...>/mask_binary.png
segmentation_overlay_ref=<...>/mask_overlay.png
segmentation_contour_ref=<...>/contours.json
```

Shape snapshot observed:

```text
m_task.segmentation_mask    model_segmentation_mask     mask      editable=false
m_task.segmentation_contour model_segmentation_contour  polyline  editable=false
```

## Detection Headless Evidence

Command:

```powershell
D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/build01/Release/cxvision_imgui_acceptance.exe `
  --headless `
  --cxscript-headless `
  --image D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/test_images/L2_low_contrast_illumination/fastmatch_bottle_test_l2.jpg `
  --script cxparser/cxscript/module/torch/torch_detection_yolov8_cpu_smoke_direct.cxsc `
  --case-name N4_torch_detection_yolov8_cpu_smoke `
  --out D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/headless/run_20260730_N4_torch_detection_yolov8_cpu_smoke `
  --max-steps 10000 `
  --unified-log D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/_shared/cxvision_imgui_acceptance.jsonl
```

Headless output:

```text
cxscript_headless_ok=true
summary=D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/headless/run_20260730_N4_torch_detection_yolov8_cpu_smoke/result_summary.json
```

Runtime artifact directory:

```text
D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/headless/T6_torch_detection_yolov8_cpu_smoke
```

Required artifacts observed:

```text
torch_detection_task_request.json
torch_detection_task_result.json
torch_runtime_evidence.json
detections.json
bbox_candidate_list.json
detection_overlay.png
```

Detection result:

```json
{"schema":"cxvision.torch.detection.detections.v1","num_detections":0,"detections":[]}
```

Conclusion:

```text
DETECTION_RUNTIME_SMOKE_PASS
DETECTION_RESULT_ADAPTER_READY
DETECTION_RECT_PROJECTION_PENDING_DATA
```

Do not claim detection rectangle projection pass until a test image/model/profile produces at least one detection.

## FindSegmentation -> LibTorch Backend Evidence

The `FindSegmentation` libtorch backend was changed from the legacy unified/TestHost task to the production segmentation task:

```text
torch.infer.segmentation.deeplabv3plus.v1
```

Implementation files:

```text
cximage/FindSegmentationEdgeSamBackend.cpp
cximage/FindSegmentation.cpp
cximage/CxScriptHeadlessRunner.cpp
cximage/ManualStateTestConsole.cpp
cxparser/cxscript/module/cximage/headless/find_segmentation_libtorch_smoke_direct.cxsc
cxparser/cxscript/module/cximage/headless/find_segmentation_opencv_smoke_direct.cxsc
```

Command:

```powershell
D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/build01/Release/cxvision_imgui_acceptance.exe `
  --headless `
  --cxscript-headless `
  --image D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/test_images/L1_high_contrast/line_high_contrast_001.jpg `
  --script cxparser/cxscript/module/cximage/headless/find_segmentation_libtorch_smoke_direct.cxsc `
  --case-name N7_find_segmentation_libtorch_backend_v3 `
  --out D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/headless/run_20260730_N7_find_segmentation_libtorch_backend_v3 `
  --max-steps 10000 `
  --unified-log D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/_shared/cxvision_imgui_acceptance.jsonl
```

Observed:

```text
cxscript_headless_ok=true
global_headless_ok=1
segmentation_status_code=1
segmentation_contour_count=1
segmentation_primary_area=1308420
segmentation_mask_ref=<...>/mask_binary.png
segmentation_overlay_ref=<...>/mask_overlay.png
segmentation_contour_ref=<...>/contours.json
```

Latest UI-readiness regression:

```text
run_20260730_N7_find_segmentation_libtorch_backend_v4_ui_ready
```

This run keeps `cxscript_headless_ok=true` and confirms the Manual runtime object status mapping accepts `libtorch_segmentation_ready` as a real mask attach-ready backend state.

Published shapes:

```text
m_seg.boundary_polyline  boundary       editable=false  result_element=true
m_seg.boundary_bbox      boundary_bbox  editable=false  result_element=true
```

Conclusion:

```text
FINDSEGMENTATION_LIBTORCH_BACKEND_HEADLESS_PASS
```

This still uses the smoke segmentation weight, so it is not a semantic boundary-quality acceptance.

## FindSegmentation Prompt Points Placeholder

Added script:

```text
cxparser/cxscript/module/cximage/headless/find_segmentation_libtorch_prompt_points_placeholder.cxsc
```

Catalog entry:

```text
[PENDING] FindSegmentation - LibTorch Prompt Points
```

Purpose:

```text
Keep positive/negative prompt point evidence visible in the script/template list before the real EdgeSAM-style positive/negative point binding is implemented.
```

Current binding boundary:

```text
positive_x / positive_y are passed through the existing FindSegmentation.setpoint(x, y).
negative_x / negative_y are intentionally only visible local variables.
prompt_points_binding_ready = 0 documents that positive/negative prompt semantics are pending_binding.
```

Conclusion:

```text
PROMPT_POINTS_SCRIPT_PLACEHOLDER_READY
PROMPT_POINTS_RUNTIME_SEMANTICS_PENDING_BINDING
```

Do not treat this placeholder as EdgeSAM positive/negative prompt acceptance.

## Manual UI Review Checklist

Run this only after internal smoke remains green.

1. Start `cxvision_imgui_acceptance.exe`.
2. Reload `cxparser/cxscript/module/cximage/catalog/cximage_catalog.cxsc`.
3. Confirm these catalog entries are visible:
   - `[SMOKE] TorchTask - Segmentation cpp_state_dict CPU`
   - `[SMOKE] TorchTask - YOLOv8 Detection CPU`
4. Select the segmentation entry and run it.
5. Confirm UI shows:
   - `status=success`
   - `actual_device=cpu`
   - `result_ref`
   - `mask_ref`
   - `overlay_ref`
   - `contour_ref`
6. Confirm Image View / Evidence View can open or display `mask_overlay.png`.
7. Confirm shape layer shows:
   - `m_task.segmentation_mask`
   - `m_task.segmentation_contour`
8. Confirm segmentation contour/result shapes are not editable.
9. Select the YOLOv8 detection entry and run it.
10. Confirm detection runtime status is success.
11. Confirm `detections.json` is visible and says `num_detections=0`.
12. Confirm no fake detection rectangle is drawn when detections are empty.
13. Save manual review.

Manual conclusion is reserved for human feedback:

```text
MANUAL_GUI_PASS
MANUAL_GUI_FAIL
MANUAL_GUI_PARTIAL
```

## Remaining Allowed Next Steps

1. Add a small UI-side readout if ManualStateTestConsole does not show TorchTask refs clearly.
2. If ManualStateTestConsole does not show FindSegmentation refs clearly, add a minimal read-only field group for backend/result/mask/overlay/contour.
3. Add a non-empty detection fixture only if a real model/test image combination is confirmed to produce boxes.
4. Keep model semantic quality outside this smoke acceptance unless real trained weights are supplied and reviewed.
