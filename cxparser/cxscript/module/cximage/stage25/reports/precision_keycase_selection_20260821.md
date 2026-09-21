# Shared Precision Keycase Selection - 2026-08-21

## Boundary

The original full Stage25 L1-L3 suite remains frozen and goes to manual broad regression review:

- `cxparser/cxscript/module/cximage/stage25/suites/stage25_l1_l3_parameter_consistency.cxsc`
- `cxparser/cxscript/module/cximage/stage25/manifests/stage25_l1_l3_manifest.json`
- `cxparser/cxscript/module/cximage/stage25/parameters/findline_findcircle_l1_l3_profiles.cxsc`

Precision work uses one shared keycase suite, not per-tool precision suites:

- `cxparser/cxscript/module/cximage/stage25/suites/stage25_precision_keycases.cxsc`

The earlier FindCircle-only keycase file is a transitional artifact and should not become the long-term precision model.

## Central Precision Module

Code-level precision classification is centralized in:

- `cximage/CxPrecisionEvaluation.h`
- `cximage/CxPrecisionEvaluation.cpp`

Tools provide runtime facts; the shared module owns the precision status vocabulary:

- `RESIDUAL_GATE_PASS`
- `RESIDUAL_GATE_FAIL`
- `PENDING_PRECISION_LIMIT`
- `NOT_EVALUATED_INSUFFICIENT_POINTS`
- `NOT_EVALUATED_NO_FIT_LINE`
- `NOT_EVALUATED_NO_FIT_CIRCLE`
- `NOT_EVALUATED_HEADLESS_FAILED`

Current binding:

- FindCircle: residual gate uses `avgdist <= 8.0`.
- FindLine: emits residual/support metrics through the same module, but returns `PENDING_PRECISION_LIMIT` after fit is available until reviewed line thresholds are selected.

Do not create separate precision models per tool. Add new tool facts into the shared module and extend the same output fields.

## Selected Key Cases

| Case | Tool | Role |
|---|---|---|
| `L1_line_high_contrast_001_plate_top_edge_ok` | FindLine | high contrast line control |
| `L1_line_high_contrast_002_metal_part_lower_right_edge_ok` | FindLine | line fit availability boundary |
| `L2_line_low_contrast_001_weak_vertical_scratch_ok` | FindLine | low contrast line boundary |
| `L3_line_near_interference_001_grid_top_edge_ok` | FindLine | interference/near-edge line case |
| `L1_circle_high_contrast_001_isolated_plate_circle_ok` | FindCircle | circle control / method branch differential |
| `L1_circle_high_contrast_002_bottle_inner_mouth_ok` | FindCircle | high contrast circle residual failure |
| `L2_circle_low_contrast_001_cap_outer_boundary_ok` | FindCircle | low contrast circle residual failure |
| `L2_circle_reflection_001_reflective_bottle_mouth_ok` | FindCircle | reflective circle residual failure |
| `L3_circle_partial_edge_001_pipe_mouth_with_occlusion_ok` | FindCircle | partial edge circle residual failure |

## Next Order

1. Run T4 dry-run on `stage25_precision_keycases.cxsc`.
2. Run T5 execution without modifying the frozen full suite.
3. Review FindLine residual/support distributions and bind one shared line precision threshold policy.
4. Continue differential only on selected key cases.

## Current Metric Artifacts

- `cxparser/cxscript/module/cximage/stage25/reports/precision_keycase_metric_matrix_20260821.md`
- `cxparser/cxscript/module/cximage/stage25/reports/precision_policy_candidate_20260821.md`
