# Next3_1 S3 Metrology Analytics Smoke Report

## Scope

This batch implements the first independent `metrology_analytics` layer described by `Next3_1.txt`.

The implementation is intentionally isolated:

- no Parser binding;
- no FindLine / FindCircle / FindEllipse / FindRect runtime dependency;
- no UI dependency;
- no business PASS/FAIL promotion;
- no GPL source copying, translation or linking.

## Implemented Files

- `cximage/metrology_analytics/CxPhysUnit.h/.cpp`
- `cximage/metrology_analytics/CxSurfaceField.h/.cpp`
- `cximage/metrology_analytics/CxSurfaceFieldSnapshot.h`
- `cximage/metrology_analytics/CxSyntheticSurfaceFactory.h/.cpp`
- `cximage/metrology_analytics/CxSurfaceAreas.h/.cpp`
- `cximage/metrology_analytics/CxSurfaceBasicStats.h/.cpp`
- `cximage/metrology_analytics/CxSurfaceLevelPlane.h/.cpp`
- `cximage/metrology_analytics/CxSurfaceUnitConversion.h/.cpp`
- `cximage/metrology_analytics/CxRoughness1D.h/.cpp`
- `cximage/metrology_analytics/CxMetrologyUiGlobals.h/.cpp`
- `cximage/metrology_analytics/CxMetrologyReferenceReplay.h/.cpp`
- `cximage/metrology_analytics/CxAnalyticsObservationBridgeDraft.h/.cpp`
- `cximage/metrology_analytics/CxMetrologyAnalyticsSmoke.h/.cpp`
- `cximage/metrology_analytics/tests/ManualConsoleAnalyticsSmoke.h/.cpp`
- `cximage/metrology_analytics/S3_BASELINE_LOCK.md`
- `cximage/metrology_analytics/S3_S4_DRAFT_CONTRACT.md`
- `cximage/metrology_analytics/ref_analytics/*.json`
- `cximage/CxCalibration.h/.cpp`: rewritten typed calibration boundary for Next2/Next3/Next3_1. The new boundary is value-semantic and evidence-friendly: it declares metadata, XY transform, Z transform, unit, uncertainty, transform trace, snapshot hash and `CxCalibrationAdapter`. The old broad implementation is intentionally kept outside this boundary as `CxCalibrationXXX.*`.

## Build

- Build dir: `D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01`
- Target: `cxvision_imgui_acceptance`
- Conclusion: `COMPILE_PASS`
- Binary: `D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build01\Release\cxvision_imgui_acceptance.exe`
- Binary timestamp: `2026/8/9 21:03:22`

## Smoke

- Command mode: `--metrology-analytics-smoke`
- Output dir: `D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\metrology_analytics\run_20260809_cxcalibration_rewrite_smoke`
- Summary: `metrology_analytics_smoke_summary.json`
- Report: `metrology_analytics_smoke_report.md`
- UI globals snapshot: `metrology_ui_globals_snapshot.json`
- Reference replay summary: `metrology_reference_replay_summary.json`
- Reference replay report: `metrology_reference_replay_report.md`
- Cases: 60
- Pass: 60
- Fail: 0
- Conclusion: `METROLOGY_ANALYTICS_SMOKE_PASS`

## Analytics SelfTest

- Command mode: `--selftest=analytics.*`
- Output dir: `D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\metrology_analytics\run_20260809_cxcalibration_rewrite_selftest_all`
- Summary: `analytics_selftest_summary.json`
- Report: `analytics_selftest_report.md`
- Selected cases: 60
- Pass: 60
- Fail: 0
- Conclusion: `ANALYTICS_SELFTEST_PASS`

Single-case filter was also verified:

- Filter: `analytics.s3_11_bridge_sine_roughness_ra`
- Output dir: `D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\metrology_analytics\run_20260809_s3_11_selftest_single`
- Selected cases: 1
- Pass: 1
- Fail: 0
- Conclusion: `ANALYTICS_SELFTEST_PASS`

CxCalibration single-case filter was also verified:

- Filter: `analytics.calibration_snapshot_typed`
- Output dir: `D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\metrology_analytics\run_20260809_cxcalibration_rewrite_snapshot_selftest`
- Selected cases: 1
- Pass: 1
- Fail: 0
- Conclusion: `ANALYTICS_SELFTEST_PASS`

Namespace isolation was checked with `--selftest=findline.*`: analytics was not invoked and the process returned `SELFTEST_NAMESPACE_NOT_HANDLED` instead of entering GUI fallback.

## Covered Gates

- S3-0 baseline boundary: represented by `S3_BASELINE_LOCK.md`.
- S3-1 surface field and physical unit: covered.
- S3-2 synthetic factory: covered.
- S3-3 surface area: basic flat and non-flat checks covered.
- S3-4 basic statistics and ADF/BCDF: covered.
- S3-5 plane fit and subtraction: covered.
- S3-6 unit conversion and uncertainty status: covered.
- S3-7 1D roughness: covered.
- S3-8 reference JSON: schema, reference cases, loader/replay, per-metric assertion and artifact export covered.
- S3-9 Manual Console analytics smoke panel: covered by a dedicated detached ImGui window contract named `Analytics Smoke / Metrology Bridge`. The panel exposes the analytics smoke action, output path, summary/report paths, case table and S3→S4 draft bridge visibility. It is UI-only and does not trigger CI by itself.
- S3-10 analytics self-test namespace: `--selftest=analytics.*` and one single-case filter covered.
- S3-11 S4 draft observation bridge: covered by a value-only draft bridge over synthetic observation IDs. It returns basic stats, area and 1D roughness for known draft observations and returns pending binding for unknown observations.
- CxCalibration typed boundary: covered by nine smoke cases for empty status, XY forward transform, XY inverse round-trip, Z scale conversion, value snapshot readiness, stable snapshot hash, 90-degree rotation, adapter snapshot round-trip and linear uncertainty propagation. This is a type/snapshot boundary only; it is not Parser registered and does not claim calibration algorithm acceptance.
- UI global bridge: `Metrology Extension / Surface Analytics` uses the shared
  `CxMetrologyUiGlobals` value table for `global_metrology_*` fields.  Smoke
  validates 41 globals, default values and edited-value propagation.  This is
  a parameter-entry and injection surface only; it does not claim algorithm
  acceptance.

## Reference Replay Notes

- `schema_v1.json` is checked for the independent cxvision implementation policy marker.
- `ref_case_*.json` files are discovered from `cximage/metrology_analytics/ref_analytics`.
- Current reference cases:
  - `ref_case_flat5`: basic stats and flat surface area ratio.
  - `ref_case_sine_A1_full_cycle`: 10000-point full-cycle sine roughness.
- A previous local run failed because the sine reference used `lambda_px=4` while asserting continuous full-cycle roughness (`2A/pi`). The reference asset was corrected to `lambda_px=10000`, matching the declared 10000-point full-cycle golden semantics.

## Remaining Boundaries

- `S3-9` visual placement still needs manual GUI confirmation. The smoke verifies the panel contract and fields, not pixel-level layout.
- `S3-11` remains draft-only. It deliberately does not connect to Parser, Find* runtime objects, Suite contract or promotion.
- No analytics output is connected to runtime contract or promotion.
