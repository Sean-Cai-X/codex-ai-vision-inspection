# FindObject Three-Stage Efficiency Evidence Chain

## Scope

This evidence chain records the required comparison for `FindObject::Measure` and `FindObject::MeasureX` without replacing the original algorithms.

The three comparison branches are:

- `baseline_original_bfs`: existing `measure` / `measurex`.
- `phase1_bfs_hotpath_optimized`: new `measurefast` / `measurexfast`, same BFS branch with fewer hot-path map reads and writes.
- `phase3_opencv_connected_components`: new `measurecc` / `measurexcc`, OpenCV connected-components reference for binary or black-white region statistics.

## Evidence Entry

Use this chain file for analysis grouping:

```text
cxparser/cxscript/module/cximage/evidence/findobject_three_stage_efficiency_compare.cxsc
```

Each case points to a direct script under:

```text
cxparser/cxscript/module/cximage/diagnostic/findobject/
```

## Required Runtime Facts

For every image, ROI, and parameter profile, record these fields for each of the three branches:

- `elapsed_ms`
- `result_count`
- `top1_rect`
- `top1_center`
- `total_area`
- `result_sizes`
- `timeout`
- `failure_stage`

## Comparison Rule

Speed can only be claimed after geometry is stable:

- First compare `result_count`, `top1_rect`, `top1_center`, `total_area`, and `result_sizes`.
- Then compare `elapsed_ms`.
- If geometry differs, mark the row as `GEOMETRY_MISMATCH` and do not claim speed improvement.
- If OpenCV connected-components differs on `MeasureX`, mark it as `REFERENCE_ONLY` unless the input is binary and no gap-growth behavior is required.

## Compile/Test Status

This change modifies C++ headers, source, and parser binding registration. It requires a rebuild of the user-selected build directory before runtime execution.
