# Measurement Semantics S0/S1 Acceptance Matrix

Date: 2026-08-09

Scope: `Next3.txt` S0/S1 baseline lock and existing-tool measurement behavior capture.

## S0 Baseline Lock

Conclusion: `S0_BASELINE_LOCK_PARTIAL_PASS`

The current repository already exposes the important runtime facts for the first measurement-semantics phase:

- `CxExecutionTypes.h` remains a lightweight execution-result boundary and does not contain surface/metrology payloads.
- Existing parser/direct bindings do not expose surface/grain/roughness measurement-semantic classes.
- `CxScriptRuntimeResultCapture` already captures FindLine, FindCircle, FindEllipse, FindRect, FastMatch, TorchTask and FindSegmentation runtime facts.
- Stage25 case expansion is manifest/profile driven, not a fixed static number.
- `CxCalibration.h` has been split into a low-coupling public compile boundary, with implementation in `CxCalibration.cpp`.

Open S0 item:

- A machine-generated inventory JSON should still be produced from the latest binary and source tree before S0 is treated as fully frozen.

## S1 Existing Tool Behavior Capture

Conclusion: `S1_CAPTURE_BOUNDARY_PARTIAL_PASS`

### FindLine

Status: `OBSERVATION出口补齐`

Added:

- `FindLine::measuregeometryrequest() const`

Purpose:

- Exposes the active gauge/geometry request as a read-only value source for measurement behavior sidecars.
- Does not change scan, candidate, fitting, fallback or filter behavior.

Remaining:

- Map `FindLineMeasureInputDebug` and `FindLineMeasureProfileStats` into `CxMeasurementBehaviorStep` records in the sidecar writer.

### FindCircle

Status: `OBSERVATION出口补齐`

Added:

- `FindCircle::measuregeometryrequest() const`
- `FindCircle::lastmeasureinputrequest() const`

Purpose:

- Makes the annulus/arc/gauge request visible without reading private state or duplicating view-layer geometry.
- Does not change radial scan, edge selection, fit filtering or scan-sector behavior.

Remaining:

- Map `FindCircleMeasureGeometryDebug` into per-arc behavior steps and observations.

### FindSegmentation

Status: `REQUEST_AND_DIAGNOSTIC_SNAPSHOT_ADDED`

Added:

- `FindSegmentationInputSnapshot`
- `FindSegmentationBackendDiagnosticSnapshot`
- `FindSegmentation::lastinputrequest() const`
- `FindSegmentation::backenddiagnostic() const`

Captured:

- backend/model/device
- threshold/mode
- image size
- prompt rect
- prompt point
- backend status/reason
- mask/overlay readiness
- contour count and primary area

Remaining:

- Backend-specific diagnostics such as EdgeSam prompt details, OpenCV component ranges and mask boundary statistics are still future work.
- No mask data pointer/reference is exposed outside the current object lifetime.

## Measurement Semantic Types

Added:

- `measurement_semantics/CxMeasurementSemanticTypes.h`

Types:

- `CxMeasurementBehaviorStep`
- `CxMeasurementObservation`
- `CxMeasurementRelation`
- `CxMeasurementFeatureVector`
- `CxMeasurementSemanticPackageRef`

Boundary:

- Pure value semantics.
- No parser pointers.
- No Image/Shape/Find* object ownership.
- No PASS/FAIL business decisions.

## Current Gate

Current conclusion:

```text
S0_BASELINE_LOCK_PARTIAL_PASS
S1_CAPTURE_BOUNDARY_PARTIAL_PASS
COMPILE_PASS
HEADLESS_SMOKE_PASS
```

Verified smoke:

```text
binary: D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/build01/Release/cxvision_imgui_acceptance.exe
script: cxparser/cxscript/module/cximage/headless/find_segmentation_opencv_smoke_direct.cxsc
image: D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/test_images/L0_basic/basic_01.jpg
output: D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/headless/run_20260809_measurement_s1_findseg_smoke
exit_code: 0
```

Next allowed step:

1. Extend `CxMeasurementSemanticEvidenceWriter` to use the new value types for behavior-step and observation mapping.
2. Add backend-specific FindSegmentation diagnostics only after its request/diagnostic snapshot is reviewed.
3. Generate a machine-readable S0 source inventory JSON from the latest binary/source tree.
