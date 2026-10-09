Registered Parent - Real Native Ellipse Trial
============================================

Purpose: verify the accepted development parent through the actual DLL C ABI,
real optimizer updates, real callbacks, candidate serialization/reloading and
single-image VERIFY inference. This uses only R&D synthetic ellipse images.
No business images, Python training scripts, UI changes or model activation.

Trusted binding input
---------------------
contracts/development_parent_test001_20261009.json records the project/model IDs
and exact manifest/checkpoint/attestation/lineage digests supplied by business.
It contains no local model root and is not a new attestation or registration ID.
attestation_reference is resolved under the existing controlled parent root.
The runner verifies all four file hashes and their mutual identity bindings.
The original manifest's weights_format=python_state_dict is a checkpoint format,
not an instruction to invoke Python; the runtime loads it with native LibTorch.
The legacy manifest FNV is left untouched; the registered SHA attestation binds
that exact manifest to the approved development-parent identity.

Build (existing isolated runtime build configuration)
----------------------------------------------------
cmake -S libtorch_module -B ../build_supervision_runtime -DLIBTORCH_MODULE_BUILD_DATASET_PROBE=ON
cmake --build ../build_supervision_runtime --config Release --target libtorch_module_registered_parent_trial --parallel 2

Explicit invocation (PowerShell)
-------------------------------
Set the current process PATH to include the installed LibTorch/OpenCV runtime
dependency directories, then run:

../build_supervision_runtime/Release/libtorch_module_registered_parent_trial.exe contracts/development_parent_test001_20261009.json <registered-parent-root> contracts/seven_class_supervision_rules.v1.json ../cxscript_runs/registered_parent_ellipse_trial

This command DOES execute training. It is deliberately not an automatic CTest.
It creates a fresh run directory; does not replace any previous run or alter
the registered parent. Keep the business root and development output separate.
The run is bounded to 2 epochs / 2 optimizer updates / batch size 1 on CPU.
A 10-minute cooperative cancellation check runs at runtime cancellation points;
it is not a hard deadline inside a long-running tensor operation.

Request semantics
-----------------
One generated single-class ellipse project: 2 Train, 1 Valid, 1 unlabelled VERIFY.
67% is the rounded training fraction of the 3 labelled images, excluding VERIFY.
One optimizer update per epoch; samples are consumed by the actual executor.
Click mode contour and frozen controls are recorded explicitly. This is a
developer smoke request, NOT a replacement of business user settings/project
data. Runtime-owned cosine LR is returned as actual 1e-4 then 1e-6.

Read the new run in this order
-----------------------------
1. parent_verification.json: file hash binding pass only. Its initial
   strict_tensor_loading_verified=false is intentional (before execution).
2. runtime_identity.json: version and SHA of the actual loaded DLL, not an old
   delivery SHA. effective_training_request.json records effective controls.
3. epoch_events.jsonl: flushed live callbacks with presence-aware validation
   score and actual LR. No zero-filled missing metrics or generated curves.
4. training_receipt.json: actual C ABI outcome, runtime v2 dataset binding,
   parent identity and review-required candidate SHA.
5. output/development_trials/ellipse-train/staging/candidate/training_trace.json:
   actual train/validation losses. Runner checks these against callback values.
6. inference_receipt.json: same candidate SHA and VERIFY image SHA. Raw local
   inference evidence is below output/development_trials/ellipse-verify/staging.
7. verify_observation.json: class counts, original-coordinate bounds check and
   whether ellipse was detected. Execution success is separate from quality.
8. trial_summary.json: final execution status, strict load verified, candidate
   binding, and confirmation that parent files and ALL input asset bytes stayed
   unchanged (checked against input_asset_inventory.json).

Failures are recorded in runner_failure.json and available runtime receipts.
No fallback parent, fabricated attestation, image replacement or auto-approval.
Candidate files remain isolated CANDIDATE_REVIEW_REQUIRED outputs.

Known result / interpretation
----------------------------
Initial actual trial passed execution but produced 14 instances: 7 line,
5 polygon, 2 open_curve, 0 ellipse. Its target result is
NO_PROJECT_TARGET_DETECTED, model_quality_accepted=false. Two optimizer updates
are a compatibility/flow check, not a convergence or geometry-accuracy test.
Validation score is 1/(1+total_loss), NOT mAP, IoU or measured contour accuracy.
Original-coordinate bounds checking is also not a geometry-accuracy guarantee.
Synthetic Train/Valid/VERIFY from this generator are not independent evidence
of business generalization. Do not hide non-target predictions or alter class
thresholds just to turn the trial green. Business display-layer filtering stays
in the business thread; this diagnostic preserves raw class counts.

Next gates
----------
Controlled model-effect diagnosis (parent baseline vs candidate, class decode,
supervision and contour errors), then a bounded overfit/quality experiment.
Offline package acceptance still requires dependency/bootstrap/capability
binding and import/replay in the business environment. No production approval
or automatic activation is implied by this developer trial.
