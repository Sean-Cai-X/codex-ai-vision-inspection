Native Synthetic Materialization and Real Loader Probe
=====================================================

Scope: development_trial only. This is deterministic R&D-generated data for
conversion/loading regression, NOT an independent geometry accuracy dataset,
NOT business images and NOT an approved parent model. No Python is used.

Generate seven complete case bundles
-----------------------------------
cmake --build ../build_seven_class_supervision_native --config Release --target seven_class_materializer --parallel 4
../build_seven_class_supervision_native/Release/seven_class_materializer.exe contracts/seven_class_supervision_rules.v1.json ../cxscript_runs/seven_class_native_materialization

This creates a unique child directory and prints its path. It never overwrites
previous images or runs. Each of arc/circle/ellipse/line/open_curve/polygon/
closed_curve gets a separate single-class project. English case names are:
  Seven Class Native arc Materialization
  Seven Class Native circle Materialization
  Seven Class Native ellipse Materialization
  Seven Class Native line Materialization
  Seven Class Native open_curve Materialization
  Seven Class Native polygon Materialization
  Seven Class Native closed_curve Materialization

Bundle layout
-------------
  case_index.json                       seven positive cases and local bindings
  <class>/project.json                   source annotation records
  <class>/materialization_binding.json   actual dataset revision/manifest SHA
  <class>/training/manifest.v1            v2 schema, real encoded file SHA values
  <class>/training/supervision.v1.json    frozen rules/source/conversion binding
  <class>/training/sample-0.pgm           Train
  <class>/training/sample-1.pgm           Train
  <class>/training/sample-2.pgm           Valid
  <class>/training/sample-0-mask.pgm      Train class mask
  <class>/training/sample-1-mask.pgm      Train class mask
  <class>/training/sample-2-mask.pgm      Valid class mask
  <class>/verify/sample-3.pgm             unlabelled VERIFY, outside training root
  <class>/verify/asset_binding.json       VERIFY image digest, no annotation/mask

All images are 64x64 binary PGM. Original images have grayscale background and
foreground; masks contain IDs 0..6/background=255. Use an explicit color palette
to view mask IDs. No overlay is written into an input image. Different drawings
use deterministic parameters/source IDs; this must NOT be promoted to evidence
of generalization just because split IDs or image SHA values differ. Shared
generator lineage and small sample count limit these cases to functional tests.

Review steps
------------
1. Open case_index.json, choose one English case, then inspect its project.json.
2. Compare source points/closed flag with the generated original and mask.
   For line/open_curve/arc, check endpoints and no invented closing segment.
3. Check training_root contains only 2 Train and 1 Valid pairs. VERIFY is in the
   sibling verify directory and has no mask. Do not copy it into training.
4. Use the actual materialization_binding values for development registration.
   dataset_root is <class>/training, NOT the suite or class directory.
5. Run the real loader probe below. Read runtime_dataset_probe_receipt.json;
   loader_ok=true means file loading/validation, NOT training success.

Real production loader probe (opt-in, no public API addition)
-----------------------------------------------------------
cmake -S libtorch_module -B ../build_supervision_runtime -DLIBTORCH_MODULE_BUILD_DATASET_PROBE=ON
cmake --build ../build_supervision_runtime --config Release --target libtorch_module_business_dataset_probe libtorch_module_business_c_api_smoke --parallel 2
ctest --test-dir ../build_supervision_runtime -C Release -R "libtorch_module_business_(dataset_probe|c_api_smoke)" --output-on-failure

The standalone runtime directory must first be configured with the dependencies
in VALIDATION_DATASET_20261009.txt. The probe compiles the exact production
executor translation unit into a test executable and calls its private loader.
It uses actual OpenCV decoding and runtime file hashing, not a substitute loader.
The option defaults OFF; it adds no DLL export, no public C ABI and no training
backdoor. It intentionally does not instantiate or train a model. A separate
C ABI smoke checks the actual DLL boundary.

Probe evidence is under ../build_supervision_runtime/dataset-probe-evidence/.
It generates seven positives and six separate rejection bundles. Bad masks are
created as explicitly invalid fixtures at first write, never by mutating an
existing test image. Expected rejections:
  wrong_mask_pixels       SUPERVISION_MATERIALIZATION_CONTENT_MISMATCH
  wrong_encoded_mask_hash BUSINESS_DATASET_ASSET_DIGEST_MISMATCH
  wrong_sidecar_hash      BUSINESS_SUPERVISION_FILE_DIGEST_MISMATCH
  wrong_width             BUSINESS_SUPERVISION_WIDTH_MISMATCH
  verify_in_training      BUSINESS_DATASET_VERIFY_MATERIALIZATION_FORBIDDEN
  v1_with_binding         BUSINESS_SUPERVISION_SCHEMA_REQUIRED

Training prerequisites still required
------------------------------------
Provide a registered development parent whose actual C++ checkpoint/manifest
matches YOLOv8-Seg and exactly arc,circle,ellipse,line,open_curve,polygon,
closed_curve. Its development_parent_attestation must bind the actual manifest,
checkpoint and lineage SHA values, with DEVELOPMENT_ONLY/trial_only flags.
The checked-in attestation template is intentionally invalid until registered.

Checked candidates are not valid substitutes: testdata/manifests/yolov8_cpu_v1
is single-class detection; the older model_lineage_test_fixtures base manifest
is detection with rectangle as its seventh class. File existence is not model
readiness. No fake parent/attestation, curve or optimizer receipt is generated.
Next trial must use the registered parent, new runtime DLL SHA, this actual v2
dataset binding, controlled output staging and genuine epoch callbacks.
Only logical IDs/digests are needed for handoff; do not send business images.
