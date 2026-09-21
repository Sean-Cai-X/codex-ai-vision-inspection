# ensmallen Runtime Adapter Landing Notes

## Scope

This note defines how the current ensmallen script assets should be connected to
real ensmallen optimization semantics for `cxvision_imgui_acceptance` headless
module testing.

The goal is not to build a generic optimization platform.  The goal is to make
the current ensmallen thread produce a traceable optimization loop for:

- geometry fitting parameter tuning
- match scoring parameter tuning

The required observation chain is:

```text
params -> objective -> stability -> best_candidate
```

## Current Script Assets

The first script surface is:

```text
cxparser/cxscript/module/ensmallen/
```

Current expected case names:

```text
geometry_tuning_direct_test.cxsc
match_score_tuning_direct_test.cxsc
baseline_vs_best_compare_test.cxsc
candidate_rank_guard_test.cxsc
stability_repeat_eval_test.cxsc
phase1_param_replay_test.cxsc
phase1_param_opt_test.cxsc
phase1_param_eval_test.cxsc
```

These scripts should stay small and close to the existing cximage script style:

- declare one runtime object
- bind global inputs
- set a small parameter set
- run one tuning/replay/check action
- export global refs

Scripts must not expose optimizer-family details, termination trees, gradient
strategy trees, or file I/O.

## Correction From ensmallen Tests

The upstream ensmallen tests in:

```text
analysis_workspace/ensmallen-master/tests
```

show the actual usage pattern that should drive this integration:

```text
Function / Objective
Parameters / Coordinates
Optimizer
optimizer.Optimize(function, parameters, callbacks...)
Result assertion / replay
```

This must be reflected in C++ runtime code.  The script layer should not fake
optimization results by only filling `summary_ref`, `compare_ref`, or
`best_params_ref`.

## Runtime Adapter Shape

Add or complete a light runtime adapter, conceptually:

```text
EnsmallenRuntimeAdapter
```

It should expose only business-level methods to cxscript:

```text
reset
setdataset
setbucket
setparam
setobjective
runbaseline
searchcandidate
comparebest
checkstability
replaybest
param_ref
objective_ref
candidate_ref
best_ref
stability_ref
summary_ref
compare_ref
replay_ref
best_params_ref
```

Internally it should organize real ensmallen usage:

```text
Objective wrapper
Initial parameters
Optimizer selection
Optimize(...)
Trace callback
Best parameter replay
Result projection
```

## First Objective Wrappers

Only two wrappers are required in the first phase.

### GeometryFitObjective

Purpose:

```text
Tune formfit / geometry fit parameters against measured fit quality and
stability.
```

Inputs:

```text
geometry_ref
fit_targets_ref
boundary_metrics_ref
initial_params
param_bounds
objective_weights
```

Outputs:

```text
baseline_objective
best_objective
objective_delta
stability_score
candidate_rank
best_params
replay_ref
```

Recommended optimizer family:

```text
GridSearch for discrete method/profile choices.
CMA-ES, SPSA, or SA for black-box continuous parameters.
L-BFGS only when a stable gradient contract exists.
```

### MatchScoreObjective

Purpose:

```text
Tune template/feature match scoring parameters and candidate ordering.
```

Inputs:

```text
input_image_ref
template_image_ref
roi_ref
match_gt
initial_params
threshold_ref
crop_policy_ref
alignment_error_ref
objective_weights
```

Outputs:

```text
baseline_objective
best_objective
objective_delta
candidate_count
selected_candidate_index
selected_candidate_score
candidate_rank
stability_score
best_params
replay_ref
```

Recommended optimizer family:

```text
GridSearch for threshold/profile/crop-policy combinations.
CMA-ES, SPSA, or SA for continuous score weights.
```

## Trace Callback Contract

The adapter should produce trace data during optimization instead of inventing
it after the fact.

Minimum trace fields:

```text
trace_id
stage
iteration
params_ref
objective_value
candidate_ref
candidate_rank
best_ref
stability_ref
stop_reason
```

Required stages:

```text
init
baseline
search
compare
stability
replay
export
```

The trace is the source for GUI/RAG/review fields such as:

```text
objective_curve
feature_distance_delta
candidate_rank
stability_score
best_candidate_confidence
coverage_gap
risk_axis
```

## cxscript Contract

Cxscript must use the project parser constraints:

- execute through `CxParserRuntime::Compile()`
- use `global_` external variables
- do not use `global.xxx`
- do not use loops, STL, lambdas, classes, file I/O, or OpenCV code
- keep complex decisions in C++ runtime or contract helpers

Example shape:

```c
EnsmallenRuntime opt;

opt.reset();
opt.setdataset(global_dataset_id);
opt.setbucket(global_bucket_id);
opt.setobjective(global_objective_id);
opt.setparam("threshold", global_threshold);
opt.setparam("weight_alignment", global_weight_alignment);

opt.runbaseline();
opt.searchcandidate();
opt.comparebest();
opt.checkstability();
opt.replaybest();

global_param_ref = opt.param_ref();
global_objective_ref = opt.objective_ref();
global_candidate_ref = opt.candidate_ref();
global_best_ref = opt.best_ref();
global_stability_ref = opt.stability_ref();
global_summary_ref = opt.summary_ref();
global_compare_ref = opt.compare_ref();
global_replay_ref = opt.replay_ref();
global_best_params_ref = opt.best_params_ref();
global_current_status = "PENDING";
```

If a script cannot run because `EnsmallenRuntime` or a method is not registered,
report a field/binding contract gap.  Do not add an alternate runner.

## Headless Testing Flow

Use `cxvision_imgui_acceptance` only through the existing headless path.

Command template:

```powershell
<BINARY> `
  --headless `
  --cxscript-headless `
  --script cxparser/cxscript/module/ensmallen/phase1_param_opt_test.cxsc `
  --case-name ensmallen_phase1_param_opt `
  --out <RUN_ROOT>/headless/<RUN_ID>/ensmallen_phase1_param_opt `
  --unified-log <SHARED_LOG>
```

Do not create a new ensmallen smoke executable.

## Test Levels

### T0 Asset Preflight

Required checks:

```text
script exists
dataset or input sample exists
objective id exists
parameter profile exists
contract exists
output directory can be created
```

Conclusion:

```text
ASSET_PREFLIGHT_PASS
ASSET_PREFLIGHT_FAIL
```

### T1 Runtime Projection

Required checks:

```text
EnsmallenRuntime object can be declared
all used methods are registered
global_* inputs are bound
global_* refs are readable after Compile()
```

Conclusion:

```text
SHAPE_PROJECTION_PASS
RESULT_OBJECT_GAP
FIELD_CONTRACT_GAP
```

### T3 Headless Execution

Required assets:

```text
snapshot.txt
result_summary.json
object_state.json
variable_snapshot.json
log.txt
```

Required metrics:

```text
baseline_objective
best_objective
objective_delta
candidate_count
selected_candidate_index
stability_score
best_params_ref
replay_ref
timeout
failure_stage
```

Conclusion:

```text
HEADLESS_EXECUTION_PASS
ASSET_MISSING
TIMEOUT
FAIL
```

### L1/L2/L3 Parameter Regression

L1:

```text
one image
one candidate
one objective
one replay
manual review remains pending
```

L2:

```text
3 to 5 representative images
same candidate set
same objective contract
candidate-case matrix exported
```

L3:

```text
repeat run
small ROI/threshold/weight perturbation
stability matrix exported
timeout report exported
```

Do not claim human acceptance unless a human review result exists.

## System Development Constraints

All future development should follow these constraints:

```text
No new ensmallen executable.
No parallel parser execution.
No parser background worker.
No SetExpr/Eval for multi-statement cxscript.
No muParser source change unless separately reviewed.
No hidden fallback that bypasses CxParserRuntime::Compile().
No PASS without runtime evidence and required assets.
```

If a failure occurs, classify it as:

```text
FIELD_CONTRACT_GAP
RESULT_OBJECT_GAP
STAGE_FLOW_SEMANTIC_GAP
RUNTIME_BINDING_GAP
ASSET_PREFLIGHT_FAIL
```

## Acceptance Boundary

The ensmallen integration is considered ready for the next system layer only
when the following are true:

```text
T0 asset preflight passes.
T1 runtime projection passes.
T3 headless execution produces required assets.
baseline and best objectives are real runtime values.
candidate rank is derived from optimizer or replay trace.
stability score is derived from repeat or perturbation evidence.
best params can be replayed.
GUI can display params -> objective -> stability -> best_candidate.
```

Until then, report the state as:

```text
PARTIAL
PENDING
BLOCKED
FAIL
```

Do not report final acceptance.
