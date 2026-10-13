param(
    [Parameter(Mandatory=$true)][string]$ParentRoot,
    [Parameter(Mandatory=$true)][string]$ExperimentRoot,
    [Parameter(Mandatory=$true)][string]$RuntimeDll,
    [Parameter(Mandatory=$true)][string]$AcceptanceExe,
    [Parameter(Mandatory=$true)][string]$VisionAiSource,
    [Parameter(Mandatory=$true)][string]$DependencyEvidence,
    [Parameter(Mandatory=$true)][ValidateSet('circle','ellipse')][string]$Geometry,
    [Parameter(Mandatory=$true)][string]$OutputRoot
)
$ErrorActionPreference='Stop'
$repo=Split-Path $PSScriptRoot -Parent
function ReadJson([string]$p){Get-Content -LiteralPath $p -Raw|ConvertFrom-Json}
function Sha([string]$p){'sha256:'+(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function NewJson([string]$p,$v){if(Test-Path -LiteralPath $p){throw 'OVERWRITE_FORBIDDEN'};[IO.File]::WriteAllText($p,($v|ConvertTo-Json -Depth 70),(New-Object Text.UTF8Encoding($false)))}
$binding=ReadJson (Join-Path $repo 'contracts/development_parent_test001_20261009.json')
$parent=ReadJson (Join-Path $ParentRoot 'model_manifest.json')
$arm=Join-Path $ExperimentRoot ($Geometry+'-unified_v5_hard_negative_experimental')
$result=ReadJson (Join-Path $arm 'result.json')
$compatibility=ReadJson $DependencyEvidence
if($compatibility.status -ne 'PASS' -or $compatibility.runtime_sha256 -cne (Sha $RuntimeDll)){throw 'DEPENDENCY_EVIDENCE_RUNTIME_MISMATCH'}
$checks=@{
    (Join-Path $ParentRoot 'model_manifest.json')=$binding.parent_manifest_sha256
    (Join-Path $ParentRoot $parent.weights)=$binding.checkpoint_sha256
    (Join-Path $ParentRoot $binding.attestation_reference)=$binding.attestation_sha256
    (Join-Path $ParentRoot $binding.lineage_reference)=$binding.source_lineage_sha256
    (Join-Path $arm 'weights.pt')=$result.checkpoint_sha256
}
foreach($p in $checks.Keys){if((Sha $p) -cne $checks[$p]){throw "SOURCE_SHA_MISMATCH: $p"}}
if($result.loss -ne 'unified_v5_hard_negative_experimental' -or $result.epochs -ne 40 -or $result.updates -ne 40 -or $result.batch_size -ne 2){throw 'EXPERIMENT_POLICY_MISMATCH'}
if(($parent.classes -join '|') -ne ($binding.geometry_contract -join '|')){throw 'CLASS_ORDER_MISMATCH'}
$root=[IO.Path]::GetFullPath($OutputRoot).TrimEnd('\')
if(Test-Path -LiteralPath $root){throw 'NEW_OUTPUT_DIRECTORY_REQUIRED'}
foreach($protected in @($ParentRoot,$ExperimentRoot,(Split-Path $RuntimeDll -Parent))){
    $p=[IO.Path]::GetFullPath($protected).TrimEnd('\')
    if($root.Equals($p,[StringComparison]::OrdinalIgnoreCase) -or $root.StartsWith($p+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'OUTPUT_INSIDE_SOURCE_FORBIDDEN'}
}
New-Item -ItemType Directory -Path $root|Out-Null
function CopyAsset([string]$p,[string]$relative){
    $dest=Join-Path $root $relative
    New-Item -ItemType Directory -Force -Path (Split-Path $dest -Parent)|Out-Null
    Copy-Item -LiteralPath $p -Destination $dest
    if((Sha $p) -cne (Sha $dest)){throw 'COPY_SHA_MISMATCH'}
}
CopyAsset (Join-Path $ParentRoot 'model_manifest.json') 'parent/model_manifest.json'
CopyAsset (Join-Path $ParentRoot $parent.weights) ('parent/'+$parent.weights)
CopyAsset (Join-Path $ParentRoot $binding.attestation_reference) ('parent/'+$binding.attestation_reference)
CopyAsset (Join-Path $ParentRoot $binding.lineage_reference) ('parent/'+$binding.lineage_reference)
CopyAsset (Join-Path $repo 'contracts/development_parent_test001_20261009.json') 'parent/registered_binding.json'
CopyAsset (Join-Path $arm 'weights.pt') 'candidate/weights/business_trial_state_dict.pt'
CopyAsset (Join-Path $arm 'trace.json') 'candidate/training_trace.json'
CopyAsset (Join-Path $arm 'result.json') 'evidence/experimental_training_receipt.json'
CopyAsset (Join-Path $ExperimentRoot 'comparison.json') 'evidence/same_budget_comparison.json'
CopyAsset $RuntimeDll 'runtime/libtorch_module_runtime.dll'
CopyAsset $AcceptanceExe 'tools/rd_isolated_preview.exe'
CopyAsset (Join-Path $repo 'contracts/seven_class_supervision_rules.v1.json') 'rules/seven_class_supervision_rules.v1.json'
CopyAsset (Join-Path $repo 'tools/Verify-IsolatedTrialHandoff.ps1') 'Verify-Handoff.ps1'
CopyAsset (Join-Path $repo 'tools/Test-UnifiedInferencePreview.ps1') 'Test-Preview.ps1'
CopyAsset (Join-Path $repo 'tests/isolated_preview/READ_FIRST.txt') 'READ_FIRST.txt'
$modelId='rd_'+$Geometry+'_unified_v5_40_40_2_20261012'
$manifest=[ordered]@{
    schema='cxvision.torch_model_manifest';schema_version=2;model_id=$modelId;task='instance_segmentation';architecture='yolov8_seg';variant='nano';
    weights='weights/business_trial_state_dict.pt';weights_format='python_state_dict';weights_hash=$result.checkpoint_sha256;
    num_classes=7;mask_channels=32;prototype_channels=64;configured_prototype_channels=256;classes=$binding.geometry_contract;
    input=$parent.input;postprocess=$parent.postprocess;business_trial_scope='isolated_business_validation';
    training_loss_version=$result.loss;business_trial_state='CANDIDATE_REVIEW_REQUIRED';
    business_development_parent_model_id=$binding.development_parent_model_id;
    business_development_parent_checkpoint_sha256=$binding.checkpoint_sha256;business_parent_attestation_sha256=$binding.attestation_sha256
}
NewJson (Join-Path $root 'candidate/model_manifest.json') $manifest
$sourcePins=@()
foreach($relative in @('adapters/LocalBusinessAssetBroker.cpp','adapters/LocalBusinessAssetBroker.h','adapters/LocalOfflineBusinessRuntimeTransport.cpp','adapters/LocalOfflineBusinessRuntimeTransport.h','adapters/OfflineBusinessServiceBridge.cpp','adapters/OfflineBusinessServiceBridge.h','adapters/OfflineBusinessRequestCanonical.h','materializer/GeometryTrainingDatasetMaterializer.cpp','materializer/GeometryTrainingDatasetMaterializer.h','contracts/TorchTrainingProviderApi.cpp','contracts/TorchTrainingProviderApi.h','contracts/ProviderReceiptSecurity.cpp')){
    $sourcePins+=@{relative_path=$relative;sha256=(Sha (Join-Path $VisionAiSource $relative))}
}
NewJson (Join-Path $root 'evidence/typed_adapter_source_pins.json') $sourcePins
$dependencies=@($compatibility.loaded_modules|Where-Object {
    $_.file -like '*.dll' -and $_.file -ne 'libtorch_module_runtime.dll' -and
    ($_.local_path -notlike 'C:/Windows/*' -or $_.file -match '^(msvcp|vcruntime|concrt)')
}|ForEach-Object{[ordered]@{file=$_.file;sha256=$_.sha256}})
if($dependencies.Count -lt 3){throw 'DEPENDENCY_EVIDENCE_INCOMPLETE'}
NewJson (Join-Path $root 'dependency.lock.json') ([ordered]@{
    schema='visionai.rd.loaded_dependency_lock.v1';runtime_sha256=(Sha $RuntimeDll);
    source_acceptance_sha256=(Sha $DependencyEvidence);device='cpu';platform='windows-x64';
    requires_installed_dependencies=$true;dependencies=$dependencies;
    note='CUDA-linked LibTorch needs its load-time CUDA DLLs even for CPU inference; no GPU accuracy/compatibility claim.'
})
$runtimeSha=Sha $RuntimeDll
NewJson (Join-Path $root 'delivery_manifest.json') ([ordered]@{
    schema='visionai.rd.isolated_trial_delivery.v1';admission='development_trial';state='EXTERNAL_INFERENCE_PREVIEW';
    target_geometry=$Geometry;candidate_model_id=$modelId;candidate_checkpoint_sha256=$result.checkpoint_sha256;
    candidate_manifest_sha256=(Sha (Join-Path $root 'candidate/model_manifest.json'));
    parent_checkpoint_sha256=$binding.checkpoint_sha256;parent_manifest_sha256=$binding.parent_manifest_sha256;
    parent_attestation_sha256=$binding.attestation_sha256;parent_lineage_sha256=$binding.source_lineage_sha256;
    data_origin='RND_SYNTHETIC_NOT_BUSINESS_TEST001';dataset_revision=$result.dataset_revision;
    geometry_contract=$binding.geometry_contract;runtime_sha256=$runtimeSha;
    production_allowed=$false;auto_activate=$false;model_quality_accepted=$false;
    candidate_training_policy=@{loss=$result.loss;bn=$result.bn_policy;epochs=40;updates=40;batch_size=2;training_api='RND_NATIVE_EXPERIMENT_NOT_TYPED_TRAINING';learning_rate='See exact trace.json';};
    runtime_training_policy='center_dense_balanced_bce_v2; NOT unified_v5; external preview broker rejects training';
    inference_policy=@{mode='eval';input=$parent.input;postprocess=$parent.postprocess;implementation_identity=$runtimeSha};
    dependency_delivery='Installed-runtime compatibility acceptance required; third-party DLLs NOT bundled';
    known_failures=@('Wrong-class fragments persist','Candidate cap reached; full post-NMS population unknown','Synthetic variants do not prove independent generalization');
    rollback='Exit external-preview session; reopen unchanged default configuration. Never promote or register as training parent.'
})
NewJson (Join-Path $root 'registration.template.json') @{
    schema='visionai.rd.external_preview_binding_instructions.v1';session_only=$true;
    external_preview_root='RESOLVE_LOCAL_PACKAGE_ROOT';external_preview_lock_sha256='SUPPLY_INDEPENDENT_HANDOFF_LOCK_SHA';
    external_preview_target_manifest_sha256='BUSINESS_COMPUTES_FROM_ITS_OWN_FROZEN_PROJECT';
    runtime_sha256=$runtimeSha;candidate_sha256=$result.checkpoint_sha256;
    training_origin='RND_SYNTHETIC';training_dataset_revision=$result.dataset_revision;
    target_dataset_revision='BUSINESS_OWN_FROZEN_REVISION_DO_NOT_OVERWRITE';production_allowed=$false
}
$files=@(Get-ChildItem -LiteralPath $root -File -Recurse|Sort-Object FullName|ForEach-Object{
    [ordered]@{relative_path=$_.FullName.Substring($root.Length+1).Replace('\','/');sha256=(Sha $_.FullName);bytes=$_.Length}
})
NewJson (Join-Path $root 'handoff.lock.json') ([ordered]@{schema='visionai.rd.handoff_lock.v1';handoff_id=$modelId;files=$files;
    release_kind='ISOLATED_INFERENCE_ONLY';production_allowed=$false;auto_activate=$false})
foreach($p in $checks.Keys){if((Sha $p) -cne $checks[$p]){throw 'SOURCE_CHANGED_DURING_PACKAGING'}}
$lockSha=Sha (Join-Path $root 'handoff.lock.json')
& (Join-Path $root 'Verify-Handoff.ps1') -Root $root -ExpectedLockSha256 $lockSha
Write-Output "PREVIEW_PACKAGE: $root"
Write-Output "INDEPENDENT_LOCK_SHA256: $lockSha"
