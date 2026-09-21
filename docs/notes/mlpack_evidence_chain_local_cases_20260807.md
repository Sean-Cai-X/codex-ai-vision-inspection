# mlpack Evidence Chain local cases

## Scope

This note records the local Evidence Chain landing for the current
`mlpack基础模型` thread.

Current project root:

```text
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai
```

Remote writeback is not used in this step. Images are landed locally outside
the git repository. Evidence CASE definitions are file-driven through the
shared cximage evidence chain.

## Image Storage Rule

Image files must stay outside the git repository.

Allowed image root:

```text
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai
```

Current mlpack image root:

```text
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\mlpack
```

Forbidden image root:

```text
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo
```

Evidence Chain files inside `cxvision_repo` may only store path references,
case metadata, script snapshots, and TSV/JSON records. They must not store or
copy image bodies. Future mlpack, torch, cximage, and ensmallen image evidence
must follow the same rule.

## Landed Image Package

Source package:

```text
D:\Codex-WorkDir\Sean_WorkDir\local_test\mlpack_baseline_thread\ELPV-Classification-Handoff
```

Local project image target:

```text
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\mlpack\ELPV-Classification-Handoff
```

Landed count:

```text
24 ELPV images
```

Dataset split:

```text
G0.baseline_manual:
  manual label spot-check images

G1.semantic_handoff:
  semantic handoff images for baseline_class_ref, cluster_ref, distance_ref,
  and anomaly_ref review
```

## Landed Evidence Chain Cases

Primary Evidence CASE source:

```text
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cxparser\cxscript\module\cximage\evidence\mlpack_baseline_evidence_chain.cxsc
```

Case count:

```text
24 ELPV Evidence Chain cases
```

Legacy/debug candidate records may exist under `cxscript_runs`, but they are
not the UI CASE source and must not be scanned by C++ as a private mlpack entry.
Any future mlpack CASE must be added to:

```text
cxparser\cxscript\module\cximage\evidence\mlpack_*.cxsc
```

Current case naming:

```text
MLPACK_ELPV_<bucket>_<sample_id>_<class_label>_semantic_refs
```

Example:

```text
MLPACK_ELPV_G1.semantic_handoff_cell2137_defect_semantic_refs
```

## UI Evidence Chain Registration

Expected category:

```text
Mlpack / Baseline Validation
```

Expected group:

```text
Saved Pending Candidates
```

Tool label:

```text
MlpackBaseline
```

Added status:

```text
pending_headless_module_test
```

## Script Snapshot

The file-driven cases currently reference:

```text
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cxparser\cxscript\module\mlpack\baseline_logreg_official_min_infer.cxsc
```

This is intentional for Evidence Chain entry. The landed cases do not claim
runtime acceptance. They bind ELPV image evidence and mlpack semantic review
fields to a known mlpack cxscript case.

## Expected Flow

The evidence chain records this intended flow:

```text
baseline_feature_direct_test
baseline_logreg_official_min_infer
baseline_distance_official_min_score
baseline_cluster_ref_test_or_binding
baseline_anomaly_official_min_probe
```

Review refs:

```text
baseline_class_ref
cluster_ref
distance_ref
anomaly_ref
```

## Current Conclusions

```text
IMAGE_LOCAL_COPY: complete for ELPV, 24/24
EVIDENCE_CASE_SOURCE: cxparser/cxscript/module/cximage/evidence/mlpack_baseline_evidence_chain.cxsc
EVIDENCE_CASE_LANDING: complete for ELPV, 24/24 file-driven cases
UI_CHAIN_SOURCE: shared evidence .cxsc scanning and parsing
UI_PRIVATE_METADATA_SCAN: forbidden/not used
UI_TARGET_BUCKET: Mlpack / Baseline Validation
EVIDENCE_SELECTION: EVIDENCE_SELECTION_PASS
IMAGE_BINDING: IMAGE_BINDING_PASS
PARAM_BINDING: PARAM_BINDING_PASS with threshold=0 generic runtime token
SCRIPT_COMPILE_ONLY: SCRIPT_COMPILE_PASS
HEADLESS_EXECUTION: EVIDENCE_SELFTEST_L2_RUNTIME_EXECUTE_FAIL
COMPILE: COMPILE_PASS
MANUAL_GUI_REVIEW: NOT_RUN
FINAL_ACCEPTANCE: NOT_ACCEPTED
```

The UI classification TSV is a debug output, not the primary UI input. The UI
must build the visible tree from shared evidence chain files, so mlpack local
cases are now loaded from:

```text
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxvision_repo\cxparser\cxscript\module\cximage\evidence\mlpack_baseline_evidence_chain.cxsc
```

Expected UI display:

```text
Mlpack / Baseline Validation (24)
  Saved Pending Candidates (24)
```

This requires a successfully rebuilt `cxvision_imgui_acceptance.exe`. The local
build succeeded after the evidence classification boundary cleanup.

Latest binary:

```text
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build\Release\cxvision_imgui_acceptance.exe
```

Latest minimal selftest:

```text
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\evidence_selftest\run_20260808_1512_mlpack_ui_classification
```

Selftest result:

```text
EVIDENCE_SELECTION_PASS
IMAGE_BINDING_PASS
PARAM_BINDING_PASS
SCRIPT_COMPILE_PASS
GLOBAL_INJECTION_PASS
EVIDENCE_SELFTEST_L2_RUNTIME_EXECUTE_FAIL
```

Current blocker:

```text
MLPACK_HEADLESS_STRING_STATEMENT_RUNTIME_GAP
```

Reason:

```text
baseline_logreg_official_min_infer.cxsc requires string statement execution.
Evidence selftest can now select the case, bind the image, bind generic
parameters, compile the script, and stage model_path from the evidence file.
The runtime execution step still fails because RunCollectedScript rejects a
string declaration statement.
```

Latest direct Headless check:

```text
Command:
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\build\Release\cxvision_imgui_acceptance.exe
  --headless
  --cxscript-headless
  --image D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\test_images\mlpack\ELPV-Classification-Handoff\G0.baseline_manual\defect\cell2105.png
  --script cxparser\cxscript\module\mlpack\baseline_logreg_official_min_infer.cxsc
  --case-name MLPACK_ELPV_G0_baseline_manual_cell2105_defect_semantic_refs
  --out D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\headless\run_20260808_1538_mlpack_direct_headless
  --max-steps 10000
  --unified-log D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\_shared\cxvision_imgui_acceptance.jsonl

Process exit code:
0

Business result:
cxscript_headless_ok=false

Failure:
RunCollectedScript failed near statement:
string model_path = global_mlpack_model_path;
```

Latest Evidence selftest package:

```text
D:\Codex-WorkDir\Sean_WorkDir\cxvisionai\cxscript_runs\evidence_selftest\run_20260808_1534_mlpack_headless_min
```

Latest Evidence selftest conclusion:

```text
EVIDENCE_SELECTION_PASS
IMAGE_BINDING_PASS
PARAM_BINDING_PASS
SCRIPT_COMPILE_PASS
GLOBAL_INJECTION_PASS
EVIDENCE_SELFTEST_L2_RUNTIME_EXECUTE_FAIL
```

## Test Gaps And Required Manual Actions

### Gap 1: Headless string statement runtime support

```text
Gap ID:
MLPACK_HEADLESS_STRING_STATEMENT_RUNTIME_GAP

Owner suggestion:
Parser/Headless runtime thread

Evidence:
Direct Headless and Evidence selftest both fail at string declaration runtime
execution. Compile-only already passes, so the gap is in runtime statement
execution, not file discovery, image binding, or evidence classification.

Required action:
Decide whether RunCollectedScript should support string declarations and string
assignments, or whether mlpack scripts should be executed by full Compile()
without per-statement expression replay.

Manual decision required:
Yes. This changes common Headless runtime semantics and should not be decided
inside the mlpack baseline thread alone.
```

### Gap 2: Trained baseline model artifact availability

```text
Gap ID:
MLPACK_BASELINE_MODEL_ARTIFACT_PENDING

Owner suggestion:
mlpack baseline asset/training thread

Evidence:
The expected model path is now carried by evidence case parameter_summary:
model_path=artifacts/baseline/logreg_all_v1.bin
The file is not currently present under the local cxvisionai project tree.

Required action:
Provide or generate the trained logreg baseline artifact through the approved
mlpack train flow before claiming infer/runtime acceptance.

Manual decision required:
Yes, if the model artifact should be copied from an external source. No file
copy from outside the project should be performed silently.
```

### Gap 3: Human GUI review

```text
Gap ID:
MLPACK_UI_HUMAN_REVIEW_PENDING

Owner suggestion:
Human reviewer / dev_analysis_gui thread

Evidence:
UI classification source is file-driven and compiled, but final visual review
has not been manually accepted.

Required action:
Open the UI, confirm Mlpack / Baseline Validation shows Saved Pending
Candidates, select one ELPV case, and verify the image is visible in Image View.

Manual decision required:
Yes. Automated tests cannot mark MANUAL_GUI_PASS.
```

## Known Gap

The HALCON mlpack manual handoff package currently has manifests locally, but
the checked local image entity count is zero for regular image extensions. Do
not create HALCON Evidence Chain image cases until image files are locally
available.

Gap label:

```text
HALCON_IMAGE_ENTITY_MISSING_LOCAL
```

## Next Allowed Step

Run local UI or Headless validation only after the owner authorizes testing.

Suggested T0/T3 sequence:

```text
T0:
  Check script exists, image exists, case metadata exists, and output directory
  can be created.

T3:
  <BINARY> --headless --cxscript-headless --image <image_path>
  --script cxparser/cxscript/module/mlpack/baseline_logreg_official_min_infer.cxsc
  --case-name <case_id>
  --out <RUN_ROOT>/headless/<RUN_ID>/<case_id>
  --max-steps 10000
  --unified-log <RUN_ROOT>/_shared/cxvision_imgui_acceptance.jsonl
```

Allowed result after this landing step:

```text
PENDING_HEADLESS_MODULE_TEST
PENDING_HUMAN_REVIEW
```
