Seven Class Dataset Freeze and Materialization Guards
====================================================

Purpose
-------
Extends the native conversion with deterministic dataset revisions, split
isolation and a v2 guard in LoadBusinessMaterializedDataset. No Python testing,
automatic model activation or production approval is introduced.

Build and tests (repository root, PowerShell)
--------------------------------------------
cmake --build ../build_seven_class_supervision_native --config Release --target seven_class_supervision_native_test seven_class_dataset_test --parallel 4
ctest --test-dir ../build_seven_class_supervision_native -C Release --output-on-failure
Use --config Debug / -C Debug for the second configuration.

New English case name:
  Seven Class Dataset Freeze and Materialization Guards
Evidence is under ../build_seven_class_supervision_native/dataset-evidence/.
Each unique run contains project.json, freeze_dataset.cxsc,
frozen/supervision.v1.json and dataset_test_receipt.json.
Do not mistake test fixture digest declarations for real business image bytes.

Freeze input and CxScript
------------------------
Input JSON has exactly project_class and samples. Samples use README.native.txt
semantics. Exactly one project class, minimum 2 Train + 1 Valid + 1 unlabelled
VERIFY, maximum 128 samples. asset_ref must be unique and use ASCII letters,
digits, underscore or hyphen (<=256 chars), matching runtime image IDs.

  SevenClassSupervision dataset;
  dataset.loadrules("D:/.../seven_class_supervision_rules.v1.json");
  dataset.load("D:/.../project.json");
  dataset.freeze();
  dataset.expectstatus("DATASET_FROZEN");
  dataset.savefreeze("D:/.../new-frozen-directory");

Use the existing native test executable's --script option to replay this saved
CxScript. Output directory must be new; failed reload clears the prior freeze.

Isolation and hashes
--------------------
The same source_group OR image_sha256 cannot appear in different splits.
Variants of one source must share source_group; hashes alone cannot detect
cropped/reencoded variants. Groups are caller-declared, not inferred. Reusing
a group within one split is allowed, but does not count as independent evidence.

Freeze validates/recomputes each conversion. Samples are sorted by asset_ref,
so input ordering alone does not alter the revision. The canonical payload
contains exact rules/version/digest, source samples (including split, class,
annotations and their receipt digests), recomputed mask/sample binding hashes,
admission and a source_image_bytes_verified=false flag. SHA256(payload.dump())
becomes dataset_sha256; dataset_revision_id is sv1- followed by all 64 digits.
These two derived fields are excluded from their own hash.

Any width, sampling, geometry, split, source or rule change changes the binding.
At load, the shared validator reconstructs the entire freeze and checks exact
equality, not just the syntax of supplied hash strings. Corruption fails closed.
Limits include 4 MiB serialized input/output, 64M total declared image pixels,
and 200M conservative dataset work units, plus individual conversion limits.

Runtime materialization v2 (not a new C ABI)
------------------------------------------
The runtime still opens manifest.v1 in the registered materialization root,
but its schema line selects v1/v2. Existing image/mask row format is unchanged.
The v2 manifest requires these new metadata lines, with actual computed values:

  schema=visionai.geometry-segmentation-materializer.v2
  dataset_revision_id=sv1-<64 lowercase hex from freeze>
  supervision_rules_version=1.0.0
  supervision_rules_sha256=sha256:<rules_sha256 from freeze>
  supervision_dataset_sha256=sha256:<dataset_sha256 from freeze>
  supervision_file_sha256=sha256:<SHA256 of exact supervision.v1.json file bytes>

The existing snapshot_id, snapshot_digest, case_id, annotation_receipt_digest
and background_pixel=255 fields remain mandatory. Set request
dataset_manifest_sha256 to the actual new manifest file hash; do not reuse an
old package/request digest. If boundary_stroke_width_pixels is supplied it must
numerically match rules.open_width_px exactly. Write manifest text with LF,
as expected by the existing materializer reader.

Register supervision.v1.json next to manifest.v1, within the allowed asset root.
It is annotation/metadata, NOT a training image or VERIFY mask. The root must
materialize only Train and Valid image/mask pairs; source split valid explicitly
maps to runtime row split val. VERIFY has metadata in the frozen sidecar but
must have no runtime training image/mask row. Its actual bytes are checked only
in the independent inference binding, not claimed verified by this loader.

The existing loader verifies image and encoded-mask file SHA256 first. For v2
it additionally computes decoded row-major mask pixel SHA256 and dimensions,
then compares all actual Train/Valid rows with the recomputed frozen rules and
annotations. Encoded mask file hashes and decoded pixel hashes are deliberately
different fields. Resaving PNG metadata cannot masquerade as changed geometry;
changed pixel geometry cannot pass with only an updated encoded-file hash.

Compatibility/result fields
---------------------------
Legacy v1 stays executable on its old path and reports:
  supervision_binding_status = legacy_unverified
It is NOT proof of the new conversion semantics. A v1 manifest carrying any
supervision_* metadata is rejected with BUSINESS_SUPERVISION_SCHEMA_REQUIRED.
Successful v2 reports:
  supervision_binding_status = train_valid_verified_verify_metadata_only
  supervision_rules_sha256, supervision_dataset_sha256
The latter receipt fields retain the runtime sha256: prefix. The supervision
JSON uses bare lowercase hex; DecodeBusinessSha256 bridges this explicitly.

Failure families
----------------
BUSINESS_SUPERVISION_BINDING_REQUIRED / FILE_DIGEST_MISMATCH / FILE_INVALID /
BINDING_MISMATCH / WIDTH_MISMATCH / SCHEMA_REQUIRED cover manifest boundaries.
SUPERVISION_DATASET_* covers duplicate IDs, insufficient split, mixed classes,
same-source leakage, changed freeze and wrong expected revision/class.
SUPERVISION_MATERIALIZATION_* covers missing/duplicate/VERIFY rows, wrong split,
wrong image, mask pixels or dimensions. Failed validation prevents training.
Codes never include private image paths or geometry in business feedback.

Acceptance scope
----------------
Native tests run the same FreezeDataset/ValidateMaterializedDataset functions
used in the runtime, and a real mu::Parser freeze script. A successful C ABI
smoke establishes ABI rejection/cancellation, not successful v2 training.
Real package import and a full v2 training/inference trial still need their own
receipt with the specific runtime DLL, parent model and actual frozen assets.
Do not reuse historical DLL SHA values or promote this to production approval.
