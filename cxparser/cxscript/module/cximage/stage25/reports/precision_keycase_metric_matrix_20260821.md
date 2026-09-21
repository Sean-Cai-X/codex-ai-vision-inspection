# Shared Precision Keycase Metric Matrix - 2026-08-21

Source run:

- `D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxscript_runs/suite/run_20260821_precision_keycases_t5_support_v3`

## Matrix

| Case | Tool | Contract | Points | Fit | Metric Family | Residual Px | Limit Px | Support | Support Limit | MeanDist | FitOffset | Precision Status |
|---|---|---:|---:|---|---|---:|---:|---:|---:|---:|---:|---|
| `L1_line_high_contrast_001_plate_top_edge_ok` | FindLine | 1 | 40 | line | `line_residual_support` | 1.34016 | 6 | 0.930233 | 0.90 | 3.03706 | 2.57706 | `PENDING_PRECISION_LIMIT` |
| `L1_line_high_contrast_002_metal_part_lower_right_edge_ok` | FindLine | 0 | 0 | none | `line_residual_support` | 0 | 6 | 0.0833333 | 0.90 | 0 | 0 | `NOT_EVALUATED_INSUFFICIENT_POINTS` |
| `L2_line_low_contrast_001_weak_vertical_scratch_ok` | FindLine | 0 | 0 | none | `line_residual_support` | 0 | 6 | 0.0416667 | 0.90 | 0 | 0 | `NOT_EVALUATED_INSUFFICIENT_POINTS` |
| `L3_line_near_interference_001_grid_top_edge_ok` | FindLine | 1 | 13 | line | `line_residual_support` | 5.29653 | 6 | 0.928571 | 0.90 | 6.78764 | 12.8582 | `PENDING_PRECISION_LIMIT` |
| `L1_circle_high_contrast_001_isolated_plate_circle_ok` | FindCircle | 0 | 1 | none | `residual_avgdist` | 0 | 8 | `NOT_EVALUATED_INSUFFICIENT_POINTS` |
| `L1_circle_high_contrast_002_bottle_inner_mouth_ok` | FindCircle | 0 | 123 | circle | `residual_avgdist` | 16.8267 | 8 | `RESIDUAL_GATE_FAIL` |
| `L2_circle_low_contrast_001_cap_outer_boundary_ok` | FindCircle | 0 | 267 | circle | `residual_avgdist` | 50.9905 | 8 | `RESIDUAL_GATE_FAIL` |
| `L2_circle_reflection_001_reflective_bottle_mouth_ok` | FindCircle | 0 | 187 | circle | `residual_avgdist` | 11.9874 | 8 | `RESIDUAL_GATE_FAIL` |
| `L3_circle_partial_edge_001_pipe_mouth_with_occlusion_ok` | FindCircle | 0 | 138 | circle | `residual_avgdist` | 13.4304 | 8 | `RESIDUAL_GATE_FAIL` |

## Observations

- FindCircle already has an active residual gate: `avgdist <= 8.0`.
- FindLine fit-available keycases produce residuals 1.34016 and 5.29653.
- FindLine fit-available keycases produce support 0.930233 and 0.928571.
- FindLine insufficient-point keycases produce support 0.0833333 and 0.0416667 before precision evaluation.
- FindLine residual/support candidate limits are present in shared output fields, but remain `PENDING_PRECISION_LIMIT` until human review.
