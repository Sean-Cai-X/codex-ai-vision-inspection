# Param Self Tuning Findline Strategy Handoff

This handoff keeps parameter strategy selection inside cxscript. External runners should only choose `--strategy-id`, ROI, image, output path, and budget. Do not expand the parameter matrix in PowerShell.

## Standard Entry Boundary

Use this file as the standard Stage25 Findline self-tuning entry. Historical scripts such as `tools/param_self_tuning_*.ps1` and root-level `param_self_tuning_*.ps1` may be kept as exploration artifacts, but they must not be used as the acceptance path if they expand `threshold / wgap / hgap / linegap / method / filterprofile` outside cxscript.

Allowed external responsibilities:

```text
select image
select ROI / target
select strategy_id
select output directory
select budget
run headless once
collect result_summary.json
rank by runtime facts
```

Forbidden external responsibilities:

```text
build parameter matrix in PowerShell
rewrite cxscript before each run
judge ACCEPTED without human review
modify frozen baseline automatically
hide selected parameters outside result_summary.json
```

## Script

```text
cxparser/cxscript/module/cximage/stage25/param_regression/param_self_tuning_findline_strategy.cxsc
```

## Strategy IDs

| strategy_id | intent | method | threshold | wgap | hgap | linegap | filterprofile |
|---:|---|---:|---:|---:|---:|---:|---:|
| 0 | use external baseline globals | global_method | global_threshold | global_wgap | global_hgap | global_linegap | global_filterprofile |
| 10 | high resolution scan, forward | 0 | 30 | 5 | 20 | 6 | 0 |
| 11 | high resolution scan, reverse | 1 | 30 | 5 | 20 | 6 | 0 |
| 20 | medium scan, forward | 0 | 20 | 11 | 32 | 3 | 1 |
| 21 | medium scan, reverse | 1 | 20 | 11 | 32 | 3 | 1 |
| 30 | low threshold scan, forward | 0 | 10 | 8 | 20 | 6 | 1 |
| 31 | low threshold scan, reverse | 1 | 10 | 8 | 20 | 6 | 1 |
| 40 | historical successful profile | 2 | 20 | 32 | 8 | 6 | 1 |

## Required Command Shape

```powershell
<BINARY> `
  --headless `
  --cxscript-headless `
  --image <IMAGE_PATH> `
  --script cxparser/cxscript/module/cximage/stage25/param_regression/param_self_tuning_findline_strategy.cxsc `
  --case-name <CASE_ID> `
  --out <RUN_ROOT>/param_regression/<RUN_ID>/strategy_<STRATEGY_ID>/<CASE_ID> `
  --roi-x0 <X0> `
  --roi-y0 <Y0> `
  --roi-x1 <X1> `
  --roi-y1 <Y1> `
  --tool-half-width <HALF_WIDTH> `
  --strategy-id <STRATEGY_ID> `
  --max-elapsed-ms 10000 `
  --max-scan-lines 4096 `
  --max-samples 200000 `
  --timeout-sec 10 `
  --unified-log <SHARED_LOG>
```

## Result Fields

After compilation with the matching C++ changes, `result_summary.json` and `variable_snapshot.json` must contain:

```text
strategy_id
selected_method
selected_threshold
selected_wgap
selected_hgap
selected_linegap
selected_filterprofile
```

Ranking must use runtime facts from `result_summary.json`:

```text
has_fit_line
valid_points_count
avgdist
budget_exceeded
elapsed_ms
failure_stage
rendered_roi_count
rendered_measure_points_count
rendered_result_count
```

Minimum validation for one strategy run:

```text
result_summary.json exists
variable_snapshot.json exists
result_overlay.png exists
evidence_overlay.png exists
tool_display.png exists
result_summary.strategy_id == requested strategy_id
selected_* fields match the cxscript strategy table
rendered_roi_count > 0
```

If `valid_points_count == 0` or `has_fit_line == false`, the run is still a valid execution result. Classify it as a failed candidate, not as a framework failure, unless assets or selected strategy fields are missing.

## Conclusion Rules

Allowed conclusions:

```text
HEADLESS_EXECUTION_PASS
PHASE2_PARAMETER_CANDIDATE_FOUND
PENDING_HUMAN_REVIEW
TIMEOUT
FAIL
```

Do not write `ACCEPTED` until a human review confirms the candidate and the frozen baseline is updated in a separate reviewed step.
