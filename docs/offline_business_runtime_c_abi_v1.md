# Offline Business Runtime C ABI v1

The local-only functions `torch_runtime_run_business_trial_v1` and `torch_runtime_run_business_inference_v1` are
used by the Vision-AI typed service transport. They are separate from
the generic `torch_runtime_run_task` entry point; the generic dispatcher continues
to reject business task identifiers.

## Training request

The service resolves filesystem references locally, then supplies a
TorchTaskRequest with:

- manifest_path: the immutable seven-class YOLOv8-Seg parent manifest;
- dataset_root: the immutable
  visionai.geometry-segmentation-materializer.v1 root;
- output_dir: an empty task staging directory below either
  `<configured-output-root>/isolated_business_validation/<task-id>/staging`
  or `<configured-output-root>/development_trials/<task-id>/staging`;
- extra_json: a locally generated JSON object with only
  training_split_percent, epoch_count, iteration_count, batch_size,
  trial_model_id, annotation_click_mode, case_id, dataset_revision_id,
  dataset_manifest_sha256, project_geometry_class,
  project_geometry_class_id, frozen_training_split_percent,
  frozen_annotation_click_mode, frozen_project_geometry_class,
  frozen_project_geometry_class_id, allowed_annotation_click_modes,
  business_parent_attestation_path, and
  business_parent_attestation_sha256.

The executor verifies the parent checkpoint SHA-256, materialization manifest
SHA-256, every materialized Train/Valid image/mask digest, the fixed seven-class
model head, and the one-category project binding. A project needs at least two
Train images and one Valid image to train. `VERIFY` is an unmarked, independent
inference asset binding; it is never serialized as a materialized holdout image
or mask and is never read by the training executor.

The runtime canonicalizes both roots and rejects symlinks, same-name path
tricks, non-empty staging folders, and any output directory outside the exact
three-component staging layout above. The configured output root is an
application-controlled root, never the task staging directory itself.

The selected development parent is accepted only when its SHA-256 attestation
matches the selected manifest's SHA-256, the actual checkpoint SHA-256, the
fixed seven-class ontology, and `DEVELOPMENT_ONLY / trial_only / not-production`
state. A legacy source-manifest FNV checksum is not an acceptable business
parent identity and is never promoted by this ABI.
The only permitted document shape is provided as
`contracts/business_development_parent_attestation.v1.template.json`; its
placeholder values are intentionally invalid and a completed document must be
stored under the controlled parent-model root, SHA-256 hashed, and registered
by the local broker before a trial can start.

iteration_count is the exact number of optimizer updates. batch_size is the
actual gradient-accumulation batch size. epoch_count partitions those updates
and triggers an epoch callback. Learning rate is executor-owned (cosine
schedule from 1e-4 to 1e-6) and is returned as an actual read-only value on
each callback. The reported score is transparently defined as
1 / (1 + actual total loss); the trace also includes raw training and
validation loss.

The epoch callback receives only numeric progress, epoch, completed
iterations, training loss, validation score presence/value, and effective
learning rate. Returning zero asks the executor to cancel. The cancellation
probe is evaluated before training, at every optimizer update, at every batch,
and before candidate commit.

Successful output is atomically committed from a private pending directory to
candidate/ inside the supplied staging root. The candidate is always
CANDIDATE_REVIEW_REQUIRED in isolated_business_validation; it is never
APPROVED, ACTIVE, or a production artifact.

## VERIFY inference

Business inference accepts only a candidate manifest marked
business_trial_scope=isolated_business_validation and
business_trial_state=CANDIDATE_REVIEW_REQUIRED. It runs the actual
YOLOv8-Seg inference implementation directly, not a detection, DeepLab, or
smoke route. Its local raw result remains inside the service process.

Its `extra_json` must carry the broker-created `verify_asset_binding_id` and
`verify_asset_sha256`. The runtime hashes the local input image and compares it
with the latter. It does not request, load, create, or evaluate a VERIFY mask.

The public typed receipt must filter raw image/mask/overlay paths and emit
only its permitted numeric geometry summary. It is always labeled
AUTO_PROVISIONAL and REVIEW_REQUIRED; human review determines any later
accepted, corrected, or rejected annotation outcome.

## Failure behavior

Missing/inconsistent parameters return TRAINING_PARAMETER_INVALID.
Reference-only folders, arbitrary datasets, altered masks, mixed project
classes, unpinned parent weights, missing/invalid parent attestations,
unapproved output scopes, and invalid VERIFY bindings are rejected. The
executor never substitutes a smoke curve, synthetic mask, detection executor,
or legacy model.

## C ABI result ownership

Callers must value/zero-initialize `TorchTaskResult`, or call
`torch_runtime_free_result` followed by `torch_runtime_init_result`, before
passing it to any runtime call. A live result must not be overwritten. The v1
business entry points initialize a non-null output first and return a release-safe
failure record on argument/runtime failures whenever an output is available.
