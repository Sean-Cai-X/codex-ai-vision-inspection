# Shared Precision Policy Candidate - 2026-08-21

## Status

This is a candidate policy for review. It is not yet an acceptance gate.

Policy status:

- FindCircle residual gate: active
- FindLine residual gate: candidate, `PENDING_HUMAN_REVIEW`
- FindLine support gate: candidate, `PENDING_HUMAN_REVIEW`

## Active Policy

| Tool | Metric Family | Metric | Limit | Status |
|---|---|---|---:|---|
| FindCircle | `residual_avgdist` | `avgdist` | `<= 8.0 px` | active |

## Candidate Policy

| Tool | Metric Family | Metric | Candidate Limit | Status |
|---|---|---|---:|---|
| FindLine | `line_residual_support` | `avgdist` | `<= 6.0 px` | `PENDING_HUMAN_REVIEW` |
| FindLine | `line_residual_support` | `support` | `>= 0.90` | `PENDING_HUMAN_REVIEW` |

## Candidate Basis

FindLine fit-available keycases from `run_20260821_precision_keycases_t5_support_v3`:

| Case | Residual Px | Support | MeanDist | FitOffset | Contract |
|---|---:|---:|---:|---:|---:|
| `L1_line_high_contrast_001_plate_top_edge_ok` | 1.34016 | 0.930233 | 3.03706 | 2.57706 | 1 |
| `L3_line_near_interference_001_grid_top_edge_ok` | 5.29653 | 0.928571 | 6.78764 | 12.8582 | 1 |

The 6.0 px residual candidate is a review boundary above the current maximum fit-available residual in the selected keycases. The 0.90 support candidate is below the current fit-available minimum support and above the insufficient-point support values.

The policy is not final because:

- only two FindLine fit-available keycases currently contribute residual data;
- two selected FindLine cases fail before precision evaluation due to insufficient points;
- human review has not accepted the selected FindLine overlays and support metric semantics.

## Binding Rules

Before this candidate can be activated:

1. A human reviewer must accept the selected FindLine keycase images and result overlays.
2. The support metric must be accepted as the first shared FindLine coverage/support metric.
3. `CxPrecisionEvaluation` may then convert FindLine from `PENDING_PRECISION_LIMIT` to `RESIDUAL_GATE_PASS/FAIL` using the reviewed residual limit.
4. The full Stage25 suite must remain frozen; only `stage25_precision_keycases.cxsc` is used for precision policy iteration.
