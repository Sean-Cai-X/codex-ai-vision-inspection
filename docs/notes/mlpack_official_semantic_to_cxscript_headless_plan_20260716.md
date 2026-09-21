# mlpack official examples to cxscript headless plan

## Scope

This note defines how the cxvision project should convert mlpack official
`methods` and `tests` experience into project-local cxscript semantic assets,
then use those assets for `cxvision_imgui_acceptance` headless module testing.

The goal is not to copy the upstream mlpack test suite. The goal is to inherit
the official usage order, input shape, parameter meaning, assertion semantics,
and edge cases, then express them as stable cxscript cases and headless
acceptance evidence.

Reference sources:

```text
<REPO_ROOT>/../analysis_workspace/mlpack-master/src/mlpack/methods
<REPO_ROOT>/../analysis_workspace/mlpack-master/src/mlpack/tests
```

Target project:

```text
<REPO_ROOT> = D:/Codex-WorkDir/Sean_WorkDir/cxvisionai/cxvision_repo
<BINARY>    = <BUILD_DIR>/Release/cxvision_imgui_acceptance.exe
```

## Core Principle

mlpack should enter cxvision as a small, testable, cxscript-driven semantic
module:

```text
mlpack official method/test
  -> semantic card
  -> cxscript module case
  -> headless T0/T3 validation
  -> result_summary/evidence refs
  -> candidate ranking
  -> GUI/manual review input
```

Do not create a second execution chain for mlpack. The future runtime path must
follow the same standard route used by the rest of cxvision:

```text
CxScript / Manual UI / Suite / Param Regression
  -> Unified Execution Request
  -> Unified Execution Core
  -> Tool Runtime Adapter
  -> mlpack
  -> Unified Execution Result
  -> Evidence Package / Image View / Review
```

## Stage 1: Select Official Minimal Cases

Start with a small set of official examples and tests. Do not migrate the whole
mlpack tree in one pass.

First batch:

```text
classification:
  logistic_regression
  random_forest
  knn

clustering:
  kmeans

distance / neighbor:
  nearest neighbor
  distance metric

anomaly:
  distance threshold or outlier score based baseline

preprocess / feature:
  normalize
  pca or basic statistics
```

Each selected case must have one official basis:

```text
source_method_file
source_test_file
algorithm_name
official_usage_order
official_assertion
```

## Stage 2: Write Semantic Cards

For each selected official sample, create a semantic card before writing code
or cxscript.

Card template:

```text
algorithm_name:
source_method_file:
source_test_file:
task_type: feature/train/infer/score/distance/cluster/anomaly/compare
input_shape:
label_shape:
required_parameters:
train_call:
infer_call:
score_or_assertion:
expected_behavior:
edge_cases:
project_field_mapping:
cxscript_target_case:
contract_target:
```

The card must answer:

```text
What input does the official case use?
What parameters are required?
What is the official call order?
What does the official test assert?
Which result is meaningful for a human reviewer?
Which project field should carry the result?
```

Do not use placeholder-only semantics such as:

```text
ref != empty
accuracy >= 0
```

unless the current stage is explicitly marked as `PENDING_BINDING`.

## Stage 3: Convert Cards Into cxscript Module Cases

Target directory:

```text
<REPO_ROOT>/cxparser/cxscript/module/mlpack
```

First batch scripts:

```text
baseline_readiness_diagnostic_test.cxsc
baseline_feature_direct_test.cxsc
baseline_logreg_official_min_train.cxsc
baseline_logreg_official_min_infer.cxsc
baseline_knn_official_min_infer.cxsc
baseline_kmeans_official_min_cluster.cxsc
baseline_distance_official_min_score.cxsc
baseline_anomaly_official_min_probe.cxsc
baseline_compare_direct_test.cxsc
baseline_threshold_probe_test.cxsc
```

Each script must stay small and readable, following the existing cximage module
script style:

```text
declare one semantic object
bind global_ input
set a small number of parameters
run one action
write global_* result refs
set global_current_status = "PENDING"
```

Allowed script role:

```text
test organization
tool call
catalog/suite/contract description
result judgment
```

Do not write C++-like logic in cxscript.

## Stage 4: Required Project Field Mapping

Every migrated official sample must map official mlpack concepts into project
fields.

Mapping:

```text
mlpack input matrix        -> global_sample_switch / feature_bucket
mlpack labels              -> global_baseline_label_ref
mlpack model               -> global_baseline_result_ref
mlpack predictions         -> global_baseline_class_ref
mlpack probabilities       -> classification_score
mlpack distance output     -> global_distance_ref
mlpack cluster assignment  -> global_cluster_ref
mlpack outlier condition   -> global_anomaly_ref
mlpack assertion           -> contract condition
mlpack parameter set       -> candidate parameter record
```

Minimum outputs for headless collection:

```text
global_sample_switch
global_test_image_ref
global_statistics_evidence_ref
global_baseline_result_ref
global_baseline_class_ref
global_cluster_ref
global_distance_ref
global_anomaly_ref
global_compare_ref
global_threshold_ref
global_issue_entry_ref
global_runtime_fillback_status
global_current_status
```

Use `global_` flat names. Do not use `global.xxx`.

## Stage 5: Runtime End-to-End Validation

The validation path for mlpack module tests must be:

```text
cxscript script
  -> cxvision_imgui_acceptance --headless --cxscript-headless
  -> CxParserRuntime::Compile()
  -> global_* injection / real global_matInput Image
  -> mlpack semantic object or adapter
  -> result_summary / snapshot / evidence
  -> contract result
```

Do not use `SetExpr()` / `Eval()` for object declarations, multi-statement
scripts, or object method calls.

Do not introduce background threads, parallel case execution, detached workers,
or a separate parser instance pool.

## Stage 6: T0 Asset Preflight

Before running a headless case, perform T0 preflight.

T0 must check:

```text
script exists
image exists
case_id exists
sample_switch exists
contract exists or is explicitly PENDING_BINDING
output directory can be created
script uses global_ fields
script does not use forbidden C++ constructs
```

Forbidden in cxscript:

```text
auto
std::vector
std::map
new/delete
lambda
template
namespace
class/struct definitions
for/while
switch
else if
complex && / ||
object return assignment
file IO
OpenCV code
```

T0 conclusions:

```text
ASSET_PREFLIGHT_PASS
ASSET_PREFLIGHT_FAIL
PENDING_BINDING
```

T0 pass does not mean algorithm pass.

## Stage 7: T3 Headless Module Test

Command template:

```powershell
<BINARY> `
  --headless `
  --cxscript-headless `
  --image <IMAGE_PATH> `
  --script cxparser/cxscript/module/mlpack/<CASE>.cxsc `
  --case-name <CASE_ID> `
  --out <RUN_ROOT>/headless/<RUN_ID>/<CASE_ID> `
  --max-steps 10000 `
  --unified-log <SHARED_LOG>
```

Required outputs:

```text
snapshot.txt
result_summary.json
variable_snapshot.json
object_state.json
log.txt
```

Image/evidence outputs, when available:

```text
result_overlay.png
evidence_overlay.png
tool_display.png
```

If an output is not implemented yet, mark it explicitly:

```text
ASSET_MISSING
```

Allowed T3 conclusions:

```text
HEADLESS_EXECUTION_PASS
HEADLESS_EXECUTION_FAIL
ASSET_MISSING
PENDING_BINDING
TIMEOUT
```

Do not report final acceptance at T3.

## Stage 8: Contract Semantics

Contracts should come from official mlpack test semantics whenever possible.

Good contract examples:

```text
prediction_count == sample_count
class_id in allowed_labels
distance >= 0
cluster_id in expected_range
model_ref exists
score is finite
threshold decision matches expected side
abs(actual - expected) <= tolerance
```

Weak contracts are only acceptable during binding work:

```text
ref != empty
accuracy >= 0
macro_f1 >= 0
```

If weak contracts are used, the case status must remain:

```text
PENDING_BINDING
```

## Stage 9: Candidate Ranking

Candidate ranking converts official parameters and project evidence into a
reviewable order. It does not decide final PASS.

Candidate record:

```text
candidate_id
algorithm_name
source_test_file
feature_bucket
model_type
distance_metric
threshold
cluster_count
neighbor_k
expected_effect
risk
official_basis
evidence_ref
result_ref
```

Ranking scores:

```text
official_basis_score:
  derived from official method/test usage

runtime_readiness_score:
  input, image, feature, and result refs available

evidence_score:
  statistics_evidence_ref and baseline_result_ref readable

stability_score:
  adjacent threshold/distance/parameter changes remain explainable

gui_review_score:
  human can understand image, evidence, decision, and issue entry
```

Candidate conclusions:

```text
CANDIDATE_RANKED
CANDIDATE_PENDING_L1
CANDIDATE_BLOCKED_BINDING
CANDIDATE_BLOCKED_EVIDENCE
CANDIDATE_BLOCKED_RUNTIME
```

After ranking, candidates must still enter:

```text
L1 single image
L2 mini-regression
L3 stability regression
manual GUI review
```

## Stage 10: Reporting Requirements

Each mlpack headless module test report must include:

```text
repo_root
build_dir
binary_path
binary_last_write_time
working_directory
run_id
unified_log
script_path
case_id
image_path
sample_switch
source_method_file
source_test_file
official_assertion
result_summary_path
evidence_ref
result_ref
candidate_rank
failure_stage
next_action
```

Use only precise layered conclusions:

```text
COMPILE_NOT_RUN
ASSET_PREFLIGHT_PASS
HEADLESS_EXECUTION_PASS
PENDING_BINDING
ASSET_MISSING
CANDIDATE_RANKED
PENDING_HUMAN_REVIEW
FAIL
```

Do not use vague conclusions:

```text
done
works
all pass
algorithm passed
accepted
```

## Stage 11: Work Split

Suggested thread split:

```text
Thread A:
  Extract semantic cards from mlpack official methods/tests.

Thread B:
  Convert semantic cards into small cxscript module cases.

Thread C:
  Connect headless case registry and global_* result collection.

Thread D:
  Implement result_summary and contract mapping.

Thread E:
  Generate candidate ranking and GUI/manual review package.
```

## Blocker Feedback Rules

Use these feedback categories:

```text
official_semantic_gap:
  Official method/test meaning is unclear.

cxscript_binding_gap:
  Required script object, method, or global field is not registered.

runtime_execution_gap:
  Script can load but runtime execution fails.

evidence_output_gap:
  Runtime executes but result/evidence output is missing.

contract_gap:
  Official assertion cannot yet be represented as project contract.

candidate_ranking_gap:
  Candidate exists but ranking evidence is incomplete.

gui_review_gap:
  Evidence exists but cannot be displayed or judged by GUI/manual review.
```

## First Batch Acceptance Target

The first batch is complete only when all of the following are true:

```text
At least one official semantic card exists for logreg.
At least one official semantic card exists for knn or kmeans.
At least one cxscript module case exists for feature/train/infer or score.
T0 preflight is defined for the case.
T3 headless command is defined for the case.
result_summary fields are defined.
candidate ranking fields are defined.
remaining blockers are classified.
```

This is not final algorithm acceptance. It is readiness for controlled
`cxvision_imgui_acceptance` headless module testing.
