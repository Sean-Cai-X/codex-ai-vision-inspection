# S3 Baseline Lock — Metrology Analytics

This directory is an independent analytic layer for measurement-semantics exploration.

## Scope

- Directory: `cximage/metrology_analytics/`
- Purpose: independent surface-field analytics for semantic measurement behavior.
- Current coupling policy: no Parser binding, no UI binding, no Find* runtime dependency.
- Main chain impact: compile-only plus explicit `--metrology-analytics-smoke` CLI.

## Hard Boundary

- This code must not copy, translate, link, or vendor GPL source code.
- The algorithms here are independent implementations inside the cxvision codebase.
- This layer provides evidence and feature values; it does not judge product PASS/FAIL.
- Any future connection to `CxMeasurementObservation` must happen through value snapshots.

## Baseline Checks

- `CxPhysUnit` rejects non-positive physical scales.
- `CxSurfaceField` owns a single-channel float grid and exposes read-only raw data.
- Synthetic cases cover flat, plane, sine, gaussian, bimodal and boundary inputs.
- Smoke output must contain:
  - `metrology_analytics_smoke_summary.json`
  - `metrology_analytics_smoke_report.md`

## Current Gate

`S3-0` through `S3-7` are represented by the smoke runner. `S3-8+` are documented but not yet promoted into the existing Evidence self-test registry.

