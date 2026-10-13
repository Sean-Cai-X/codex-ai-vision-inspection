param(
    [Parameter(Mandatory=$true)][string]$ParentRoot,
    [Parameter(Mandatory=$true)][string]$TrialRoot,
    [Parameter(Mandatory=$true)][string]$ComparisonRoot,
    [Parameter(Mandatory=$true)][string]$CancellationRoot,
    [Parameter(Mandatory=$true)][string]$RuntimeDll,
    [Parameter(Mandatory=$true)][string]$OutputRoot
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
function ReadJson([string]$p) { Get-Content -LiteralPath $p -Raw | ConvertFrom-Json }
function Sha([string]$p) { 'sha256:' + (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant() }
function NewJson([string]$p, $data) {
    if (Test-Path -LiteralPath $p) { throw "Refuse overwrite: $p" }
    [IO.File]::WriteAllText($p, ($data | ConvertTo-Json -Depth 60), (New-Object Text.UTF8Encoding($false)))
}
$binding = ReadJson (Join-Path $repo 'contracts/development_parent_test001_20261009.json')
$pm = ReadJson (Join-Path $ParentRoot 'model_manifest.json')
$summary = ReadJson (Join-Path $TrialRoot 'trial_summary.json')
$candidate = Join-Path $TrialRoot 'output/development_trials/ellipse-train/staging/candidate'
$cm = ReadJson (Join-Path $candidate 'model_manifest.json')
$cancel = ReadJson (Join-Path $CancellationRoot 'cancellation_acceptance.json')
$comparison = ReadJson (Join-Path $ComparisonRoot 'comparison.json')
$runtimeSha = Sha $RuntimeDll
$checks = @{
    (Join-Path $ParentRoot 'model_manifest.json') = $binding.parent_manifest_sha256
    (Join-Path $ParentRoot $pm.weights) = $binding.checkpoint_sha256
    (Join-Path $ParentRoot $binding.attestation_reference) = $binding.attestation_sha256
    (Join-Path $ParentRoot $binding.lineage_reference) = $binding.source_lineage_sha256
    (Join-Path $candidate $cm.weights) = $summary.candidate_model_sha256
}
foreach ($p in $checks.Keys) { if ((Sha $p) -ne $checks[$p]) { throw "Asset digest mismatch: $p" } }
if ($runtimeSha -ne $summary.runtime.dll_sha256 -or $runtimeSha -ne $cancel.runtime.dll_sha256 -or
    $cancel.status -ne 'PASS' -or $summary.status -ne 'PASS' -or
    $comparison.candidate_sha256 -ne $summary.candidate_model_sha256 -or
    $comparison.dataset_revision_id -ne $summary.dataset_revision_id -or
    $cm.business_trial_state -ne 'CANDIDATE_REVIEW_REQUIRED' -or
    $cm.training_loss_version -ne 'center_dense_balanced_bce_v2') { throw 'Trial/runtime/evidence binding mismatch' }
if (($cm.classes -join '|') -ne ($binding.geometry_contract -join '|')) { throw 'Class order mismatch' }
$resolvedOutput = [IO.Path]::GetFullPath($OutputRoot).TrimEnd([IO.Path]::DirectorySeparatorChar)
foreach ($sourceRoot in @($ParentRoot,$TrialRoot,$ComparisonRoot,$CancellationRoot,(Split-Path $RuntimeDll -Parent))) {
    $protected = [IO.Path]::GetFullPath($sourceRoot).TrimEnd([IO.Path]::DirectorySeparatorChar)
    if ($resolvedOutput.Equals($protected,[StringComparison]::OrdinalIgnoreCase) -or
        $resolvedOutput.StartsWith($protected+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Delivery output must not be inside a source or runtime directory'
    }
}
if (Test-Path -LiteralPath $OutputRoot) { throw 'Delivery destination must not exist' }
$root = (New-Item -ItemType Directory -Path $OutputRoot).FullName
$inventory = @()
function CopyAsset([string]$source,[string]$relative) {
    $dest = Join-Path $root $relative
    New-Item -ItemType Directory -Force -Path (Split-Path $dest -Parent) | Out-Null
    Copy-Item -LiteralPath $source -Destination $dest
    if ((Sha $source) -ne (Sha $dest)) { throw 'Copied asset digest mismatch' }
    $script:inventory += [ordered]@{relative_path=$relative;sha256=(Sha $dest);bytes=(Get-Item -LiteralPath $dest).Length}
}
CopyAsset (Join-Path $ParentRoot 'model_manifest.json') 'parent/model_manifest.json'
CopyAsset (Join-Path $ParentRoot $pm.weights) ('parent/'+$pm.weights)
CopyAsset (Join-Path $ParentRoot $binding.attestation_reference) ('parent/'+$binding.attestation_reference)
CopyAsset (Join-Path $ParentRoot $binding.lineage_reference) ('parent/'+$binding.lineage_reference)
CopyAsset (Join-Path $candidate 'model_manifest.json') 'candidate/model_manifest.json'
CopyAsset (Join-Path $candidate $cm.weights) ('candidate/'+$cm.weights)
CopyAsset (Join-Path $candidate 'training_trace.json') 'candidate/training_trace.json'
CopyAsset $RuntimeDll 'runtime/libtorch_module_runtime.dll'
foreach ($name in @('training_receipt.json','inference_receipt.json','effective_training_request.json','epoch_events.jsonl','trial_summary.json')) {
    CopyAsset (Join-Path $TrialRoot $name) ('evidence/'+$name)
}
CopyAsset (Join-Path $CancellationRoot 'cancellation_acceptance.json') 'evidence/cancellation_acceptance.json'
CopyAsset (Join-Path $CancellationRoot 'training_receipt.json') 'evidence/cancellation_receipt.json'
CopyAsset (Join-Path $ComparisonRoot 'comparison.json') 'evidence/comparison.json'
CopyAsset (Join-Path $repo 'contracts/development_parent_test001_20261009.json') 'parent/registered_binding.json'
$observations = @($comparison.observations | Where-Object {$_.model -eq 'candidate'} | ForEach-Object {
    $counts = [ordered]@{}
    foreach ($name in $binding.geometry_contract) {
        $property = $_.class_counts.PSObject.Properties[$name]
        $counts[$name] = if ($null -eq $property) {0} else {[int]$property.Value}
    }
    [ordered]@{sample_id=$_.sample_id;purpose=$_.purpose;class_counts=$counts;
        expected_instances=1;expected_instances_basis='RND_SYNTHETIC_ORACLE_NOT_BUSINESS_RULE';
        extra_ellipse_candidates=[Math]::Max(0,$counts['ellipse']-1);
        duplicate_metric='SAME_CLASS_SURPLUS_NOT_PAIRWISE_OVERLAP';
        best_ellipse_iou=$_.best_ellipse_iou;diagnostic_conclusion='REVIEW_REQUIRED';
        actual_typed_bridge_receipt=$false}
})
NewJson (Join-Path $root 'diagnostic_summary.json') $observations
$manifest = [ordered]@{
    schema='visionai.rd.isolated_trial_delivery.v1';state='ASSETS_READY_BRIDGE_ADMISSION_PENDING';
    admission='development_trial';production_allowed=$false;auto_activate=$false;model_quality_accepted=$false;
    candidate_model_id=$cm.model_id;candidate_checkpoint_sha256=$summary.candidate_model_sha256;
    candidate_manifest_sha256=(Sha (Join-Path $candidate 'model_manifest.json'));
    development_parent_model_id=$binding.development_parent_model_id;parent_checkpoint_sha256=$binding.checkpoint_sha256;
    parent_manifest_sha256=$binding.parent_manifest_sha256;parent_attestation_sha256=$binding.attestation_sha256;
    parent_lineage_sha256=$binding.source_lineage_sha256;dataset_revision=$summary.dataset_revision_id;
    data_origin='RND_SYNTHETIC_NOT_BUSINESS_TEST001';geometry_contract=$binding.geometry_contract;
    runtime_sha256=$runtimeSha;runtime_version=$summary.runtime.version;
    abi_evidence='REAL_C_ABI_TRAIN_CURVE_LR_RECEIPT_INFER_AND_CALLBACK_CANCEL';
    typed_bridge_registration='PENDING_REAL_BROKER_FROZEN_PROJECT_BINDING';
    typed_bridge_compatibility_verified=$false;offline_dependencies_packaged=$false;
    training_policy=@{loss_version=$cm.training_loss_version;bn='train updates running statistics; eval validation; train restored each epoch';
        epochs=$summary.epoch_callbacks;updates=$summary.optimizer_updates;batch_size=1};
    inference_policy=@{mode='eval';preprocessing=$cm.input;postprocessing=$cm.postprocess;implementation_identity=$runtimeSha};
    pending=@('Real isolated broker registration and typed GetCapability/inference acceptance',
        'Offline dependency closure or verified installed-runtime compatibility',
        'Same-budget Train0/Train1 and duplicate-candidate correction');
    files=$inventory
}
NewJson (Join-Path $root 'delivery_manifest.json') $manifest
NewJson (Join-Path $root 'delivery_manifest_digest.json') @{sha256=(Sha (Join-Path $root 'delivery_manifest.json'))}
foreach ($p in $checks.Keys) {if ((Sha $p) -ne $checks[$p]) {throw 'Source changed during packaging'}}
Write-Output "ASSETS_READY_BRIDGE_ADMISSION_PENDING: $root"
Write-Output (Sha (Join-Path $root 'delivery_manifest.json'))
