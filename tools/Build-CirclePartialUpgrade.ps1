param(
 [Parameter(Mandatory=$true)][string]$BaselinePackage,
 [Parameter(Mandatory=$true)][string]$BaselineLockSha256,
 [Parameter(Mandatory=$true)][string]$ExperimentRoot,
 [Parameter(Mandatory=$true)][string]$PreviousAttempt,
 [Parameter(Mandatory=$true)][string]$NominalReplay,
 [Parameter(Mandatory=$true)][string]$Feedback,
 [Parameter(Mandatory=$true)][string]$FeedbackSha256,
 [Parameter(Mandatory=$true)][string]$AcceptanceExe,
 [Parameter(Mandatory=$true)][string]$VisionAiSource,
 [Parameter(Mandatory=$true)][string]$OutputRoot
)
$ErrorActionPreference='Stop'
function Sha([string]$p){'sha256:'+(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Json([string]$p){Get-Content -LiteralPath $p -Raw|ConvertFrom-Json}
function SaveJson([string]$p,$j){[IO.File]::WriteAllText($p,($j|ConvertTo-Json -Depth 90),(New-Object Text.UTF8Encoding($false)))}
$baseline=(Resolve-Path -LiteralPath $BaselinePackage).Path
if((Sha (Join-Path $baseline 'handoff.lock.json')) -cne $BaselineLockSha256){throw 'BASELINE_LOCK_MISMATCH'}
& (Join-Path $PSScriptRoot 'Verify-IsolatedTrialHandoff.ps1') -Root $baseline -ExpectedLockSha256 $BaselineLockSha256
if((Sha $Feedback) -cne $FeedbackSha256){throw 'FEEDBACK_SHA_MISMATCH'}
$ticket=Json $Feedback
$delivery=Json (Join-Path $baseline 'delivery_manifest.json')
$arm=Join-Path $ExperimentRoot 'circle-unified_v16_output_bias_lr2000_experimental-coverage-v1'
$result=Json (Join-Path $arm 'result.json');$comparison=Json (Join-Path $ExperimentRoot 'comparison.json');$replay=Json $NominalReplay
if($result.shape -ne 'circle' -or $result.loss -ne 'unified_v16_output_bias_lr2000_experimental' -or
 $result.epochs -ne 40 -or $result.updates -ne 40 -or $result.batch_size -ne 2 -or
 $result.classifier_output_bias_lr_multiplier -ne 2000 -or $result.parent_sha256 -cne $delivery.parent_checkpoint_sha256 -or
 (Sha (Join-Path $arm 'weights.pt')) -cne $result.checkpoint_sha256){throw 'CANDIDATE_POLICY_OR_DIGEST_MISMATCH'}
if($delivery.target_geometry -ne 'circle' -or $delivery.candidate_checkpoint_sha256 -cne ('sha256:'+$ticket.candidate_sha256.circle) -or
 $replay.baseline_sha256 -cne $delivery.candidate_checkpoint_sha256 -or $replay.candidate_sha256 -cne $result.checkpoint_sha256 -or
 $comparison.execution_status -ne 'PASS' -or $replay.candidate.Count -ne 5){throw 'BASELINE_OR_REPLAY_MISMATCH'}
$previous=Json $PreviousAttempt
if($previous.execution_status -ne 'PASS'){throw 'PREVIOUS_ATTEMPT_EVIDENCE_INVALID'}
$root=[IO.Path]::GetFullPath($OutputRoot).TrimEnd('\')
if(Test-Path -LiteralPath $root){throw 'FRESH_PARTIAL_PACKAGE_REQUIRED'}
foreach($source in @($baseline,(Resolve-Path $ExperimentRoot).Path)){
 if($root.Equals($source,[StringComparison]::OrdinalIgnoreCase) -or $root.StartsWith($source+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'PACKAGE_INSIDE_SOURCE_FORBIDDEN'}
}
New-Item -ItemType Directory -Path $root|Out-Null
Get-ChildItem -LiteralPath $baseline | Where-Object {$_.Name -ne 'handoff.lock.json'} | ForEach-Object {Copy-Item -LiteralPath $_.FullName -Destination $root -Recurse}
$id='circle-unified-v16-partial-20261013-r1'
Copy-Item -LiteralPath (Join-Path $arm 'weights.pt') -Destination (Join-Path $root 'candidate/weights/business_trial_state_dict.pt') -Force
Copy-Item -LiteralPath (Join-Path $arm 'trace.json') -Destination (Join-Path $root 'candidate/training_trace.json') -Force
Copy-Item -LiteralPath (Join-Path $arm 'result.json') -Destination (Join-Path $root 'evidence/experimental_training_receipt.json') -Force
Copy-Item -LiteralPath (Join-Path $ExperimentRoot 'comparison.json') -Destination (Join-Path $root 'evidence/same_budget_comparison.json') -Force
Copy-Item -LiteralPath $PreviousAttempt -Destination (Join-Path $root 'evidence/previous_bias_attempt.json')
Copy-Item -LiteralPath $NominalReplay -Destination (Join-Path $root 'evidence/nominal_replay.json')
Copy-Item -LiteralPath $AcceptanceExe -Destination (Join-Path $root 'tools/rd_isolated_preview.exe') -Force
$manifest=Json (Join-Path $root 'candidate/model_manifest.json')
$manifest.model_id=$id;$manifest.weights_hash=$result.checkpoint_sha256;$manifest.training_loss_version=$result.loss
SaveJson (Join-Path $root 'candidate/model_manifest.json') $manifest
$pins=Json (Join-Path $root 'evidence/typed_adapter_source_pins.json')
foreach($pin in $pins){$pin.sha256=Sha (Join-Path $VisionAiSource $pin.relative_path)}
SaveJson (Join-Path $root 'evidence/typed_adapter_source_pins.json') $pins
$regression=[ordered]@{
 schema='visionai.rd.partial_upgrade_regression.v1';delivery_id=$id;status='READY_FOR_BUSINESS_RETEST';target_geometry='circle';
 baseline_model_sha256=$delivery.candidate_checkpoint_sha256;candidate_checkpoint_sha256=$result.checkpoint_sha256;runtime_sha256=$delivery.runtime_sha256;
 issue_refs=@(@{issue_id=$ticket.id;feedback_sha256=$FeedbackSha256});
 data_origin='RND_SYNTHETIC_SAME_GENERATOR_NOT_BUSINESS_IMAGES';dataset_revision=$result.base_dataset_revision;coverage_revision_sha256=$result.coverage_revision_sha256;
 inference=@{mode='eval';input=$manifest.input;postprocess=$manifest.postprocess;
  preprocessing_version='bgr-rgb-resize-linear-center-pad-v1';postprocessing_version='sigmoid-dfl-box-nms-proto-mask-contour-v1'};
 training=@{epochs=40;optimizer_updates=40;effective_batch=2;microbatch=1;bn=$result.bn_policy;loss=$result.loss;bias_lr_multiplier=2000;other_lr_multiplier=1;
  epoch_definition='One sampled optimizer update per epoch, not a complete dataset pass';
  selection_budget='Two new circle arms x 40 updates x batch 2 (v15 and v16); prior experiments disclosed in comparisons'};
 comparison_limits=@('Original delivered v5 and v16 share parent, nominal source split and 40/40/2; v16 adds paired Train augmentation and changed loss/optimizer policy',
  'Covered v5 and v13 controls use exactly the same augmented exposure revision as v16',
  'Nine Train-source stress variants and one nominal Valid are same-generator diagnostics, not independent accuracy',
  'Nominal VERIFY is unlabelled and GT-dependent metrics remain null');
 stress_comparison=$comparison;nominal_comparison=$replay;
 previous_attempt_sha256=(Sha $PreviousAttempt);
 known_regressions=@('V16 tall-letterbox IoU .483437 is below .5 and lower than v15 .564778',
  'Twenty circle candidates across ten one-target scenes; excess circle outputs remain',
  'Five duplicate candidate pairs versus three for original v5; other-class textured fragments remain',
  'Business image quality remains unverified; ellipse weights are not upgraded');
 unresolved_issue_codes=@('TARGET_MISSING_OR_WRONG_LOCATION','WRONG_CLASS_FRAGMENTS','UNIFIED_V5_TRAINING_NOT_EXPOSED');
 model_quality_accepted=$false;production_allowed=$false;auto_activate=$false
}
SaveJson (Join-Path $root 'upgrade_regression.json') $regression
$delivery.candidate_model_id=$id;$delivery.candidate_checkpoint_sha256=$result.checkpoint_sha256
$delivery.candidate_manifest_sha256=Sha (Join-Path $root 'candidate/model_manifest.json')
$delivery.dataset_revision=$result.base_dataset_revision
$delivery.candidate_training_policy=@{loss=$result.loss;bn=$result.bn_policy;epochs=40;updates=40;batch_size=2;training_api='RND_NATIVE_EXPERIMENT_NOT_TYPED_TRAINING';learning_rate='Exact values in candidate/training_trace.json';bias_lr_multiplier=2000}
$delivery.known_failures=$regression.known_regressions
$delivery.rollback='Use the existing business model selector to return to the previous isolated candidate. Default model is not changed.'
$delivery | Add-Member -NotePropertyName partial_upgrade -NotePropertyValue ([ordered]@{
 schema='visionai.rd.partial_upgrade.v1';delivery_id=$id;status='READY_FOR_BUSINESS_RETEST';target_geometry='circle';
 baseline_model_sha256=$regression.baseline_model_sha256;candidate_checkpoint_sha256=$result.checkpoint_sha256;runtime_sha256=$delivery.runtime_sha256;
 regression_report_sha256=(Sha (Join-Path $root 'upgrade_regression.json'));change_scope=@('weights');issue_refs=$regression.issue_refs
})
SaveJson (Join-Path $root 'delivery_manifest.json') $delivery
$registration=Json (Join-Path $root 'registration.template.json')
$registration.candidate_sha256=$result.checkpoint_sha256;$registration.training_dataset_revision=$result.base_dataset_revision
SaveJson (Join-Path $root 'registration.template.json') $registration
[IO.File]::WriteAllText((Join-Path $root 'START_HERE.txt'),@"
Circle partial upgrade: $id
Status: READY_FOR_BUSINESS_RETEST. Inference only. No automatic activation.
Import this directory using the existing Vision-AI model-package importer and the independently supplied lock SHA.
Check dependencies, explicitly associate the frozen circle project, and select isolated testing.
Retest original Train/Valid/VERIFY images with original annotations and parameters; unlabelled VERIFY has no IoU.
Review all seven raw class counts, matched circle contour, excess candidates and duplicate pairs.
Circle v16 changes complete weights after paired Train augmentation and bias optimization. Parent, runtime and inference thresholds remain the baseline versions.
Known remaining issues and regressions are in upgrade_regression.json. Circle count accuracy is not accepted; ellipse is unchanged.
Rollback by selecting the previous isolated model. Do not overwrite model files or default configuration.
The included native test utility is for development verification, not required for ordinary operators.
Legacy registration.template.json is retained for compatibility; the GUI manages the actual project association.
"@,(New-Object Text.UTF8Encoding($false)))
Copy-Item -LiteralPath (Join-Path $root 'START_HERE.txt') -Destination (Join-Path $root 'READ_FIRST.txt') -Force
$files=@(Get-ChildItem -LiteralPath $root -File -Recurse|Sort-Object FullName|ForEach-Object{[ordered]@{relative_path=$_.FullName.Substring($root.Length+1).Replace('\','/');sha256=(Sha $_.FullName);bytes=$_.Length}})
SaveJson (Join-Path $root 'handoff.lock.json') ([ordered]@{schema='visionai.rd.handoff_lock.v1';handoff_id=$id;files=$files;release_kind='ISOLATED_INFERENCE_ONLY';production_allowed=$false;auto_activate=$false})
$lockSha=Sha (Join-Path $root 'handoff.lock.json')
& (Join-Path $root 'Verify-Handoff.ps1') -Root $root -ExpectedLockSha256 $lockSha
Write-Output "PACKAGE_PENDING_REAL_TYPED_ACCEPTANCE: $root"
Write-Output "INDEPENDENT_LOCK_SHA256: $lockSha"
