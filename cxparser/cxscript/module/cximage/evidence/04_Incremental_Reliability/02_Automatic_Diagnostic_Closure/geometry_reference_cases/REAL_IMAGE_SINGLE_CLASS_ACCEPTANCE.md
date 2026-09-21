# Geometry Single-Class Acceptance

## Current Gate

- Controlled fixtures are T0 regression assets. They verify discovery, typed labels, geometry extraction, overlays, and baseline evaluation only.
- Training remains disabled. Parent/child model binding and real-image validation remain pending.
- Do not create placeholder case directories. A real case becomes discoverable only after all mandatory assets exist.
- Every run uses a new `RUN_ID`; previous reports are retained.

## Human Review Items

Use these visible names in Manual Review / Evidence:

1. `Controlled circle closed-region case`
2. `Controlled ellipse closed-region case`
3. `Controlled rectangle closed-region case`
4. `Controlled polygon closed-region case`
5. `Controlled straight-line boundary case`
6. `Controlled circular-arc boundary case`
7. `Controlled open-curve boundary case`

For each item, inspect the source image, typed label, extracted geometry, threshold baseline, and overlay. Confirm that the displayed geometry type and topology match the visible object. Do not accept a case from summary text alone.

The current human decision must remain `PENDING_HUMAN_REVIEW` until all seven overlays have been inspected. A controlled-fixture acceptance is not a production acceptance.

## RUN_ROOT Discovery

The evaluator exports accepted cases to `<RUN_ROOT>/geometry_reference_evaluation/<RUN_ID>/cases/<case>/`. Each package contains `case_manifest.json`, source image, typed label, geometry facts, evidence overlay, and result summary.

`<RUN_ROOT>/_shared/evidence_case_roots.json` registers category roots. Manual Review / Evidence recursively discovers `case_manifest.json` only below those roots and validates all mandatory assets before displaying a case. The scan report is `<RUN_ROOT>/evidence_chain/case_asset_scan_debug.json`.

Refreshing the GUI after an asset change must satisfy all of the following without a C++ change:

- a new complete case directory appears;
- a renamed directory changes the fallback display name;
- a missing required asset produces `ASSET_MISSING` and no empty case;
- a duplicate `internal_case_id` within the same `RUN_ID` is rejected;
- invalid JSON is rejected without terminating the GUI.

## Real-Image Intake

Process one geometry type at a time in this order: circle, ellipse, rectangle, polygon, straight line, circular arc, open curve.

Each geometry type needs at least one target image, three rehearsal images, and two untouched holdout images. Each image must include:

- source image and provenance;
- SHA-256 hash;
- ROI or explicit full-image declaration;
- typed label or an explicitly provisional label;
- geometry facts and tolerance policy;
- lighting, contrast, blur, occlusion, and background-complexity tags;
- `target`, `rehearsal`, or `holdout` split.

The manifest `review_item` or `display_name` must be a name a reviewer can locate directly in the GUI. Internal IDs remain separate.

## Per-Class Execution

For each geometry type, run the following sequence and stop at the first failure:

1. T0 asset preflight: validate manifest, source, ROI, typed label, facts, tolerances, paths, hashes, and split.
2. T1 typed-label evaluation: confirm topology and fit the declared geometry from the reference label.
3. T2 non-model baseline: run the deterministic image baseline and export its geometry and overlay.
4. T3 candidate generation: after the model binding exists, produce geometry hypotheses with provenance and calibrated confidence.
5. T4 classical verification: verify or clamp the hypothesis with the applicable geometry tool and independent image evidence.
6. T5 predictive gate: emit only `DIRECT_MEASUREMENT`, `CLAMPED_MEASUREMENT`, `PREDICTIVE_HYPOTHESIS`, or `UNDETERMINABLE`.
7. T6 evidence export: write summary, geometry facts, overlay, hashes, timing, failure stage, and review record.
8. T7 human review: inspect the visible overlay and source; record accept, reject, or rework without automatic parameter writeback.

`DIRECT_MEASUREMENT` and `CLAMPED_MEASUREMENT` may expose measurement values. `PREDICTIVE_HYPOTHESIS` and `UNDETERMINABLE` must not expose a final measurement and always require human review.

## Feedback Loop

Classify every rejected image by the observed failure stage: asset, label, segmentation, topology, geometry fit, classical verification, clamp, predictive gate, timeout, or review mismatch. Change only the responsible asset, tolerance, or implementation layer, then create a new run.

Do not train on holdout images. After target and rehearsal corrections stabilize, run holdout once, review every failure, and keep the evidence even when rejected. Promotion remains blocked until all mandatory outputs exist and the human decision is recorded.

## Current Status

- Controlled geometry evaluator: ready for human inspection.
- Predictive geometry gate: automated self-test available.
- Parent/child model binding: `PENDING_BINDING`.
- Real application image set: `PENDING_REAL_IMAGE_SET`.
- Production acceptance: `PENDING_HUMAN_REVIEW`.
