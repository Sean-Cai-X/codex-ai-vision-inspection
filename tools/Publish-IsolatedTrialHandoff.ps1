param(
    [Parameter(Mandatory=$true)][string]$SourceBundle,
    [Parameter(Mandatory=$true)][string]$Destination
)
$ErrorActionPreference='Stop'
function Sha([string]$p){'sha256:'+(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function WriteNewJson([string]$p,$value){if(Test-Path -LiteralPath $p){throw 'Overwrite forbidden'};[IO.File]::WriteAllText($p,($value|ConvertTo-Json -Depth 50),(New-Object Text.UTF8Encoding($false)))}
$source=(Resolve-Path -LiteralPath $SourceBundle).Path
$target=[IO.Path]::GetFullPath($Destination)
if(Test-Path -LiteralPath $target){throw 'Handoff target must be a new directory'}
if($target.StartsWith($source+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Handoff cannot be inside source'}
$manifestPath=Join-Path $source 'delivery_manifest.json'
$manifest=Get-Content -LiteralPath $manifestPath -Raw|ConvertFrom-Json
$pin=Get-Content -LiteralPath (Join-Path $source 'delivery_manifest_digest.json') -Raw|ConvertFrom-Json
if((Sha $manifestPath) -ne $pin.sha256){throw 'Delivery manifest digest mismatch'}
if($manifest.production_allowed -or $manifest.auto_activate -or $manifest.model_quality_accepted){throw 'Only unapproved isolated preview is permitted'}
foreach($file in $manifest.files){
    $path=[IO.Path]::GetFullPath((Join-Path $source $file.relative_path))
    if(-not $path.StartsWith($source+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Asset path escape'}
    if((Sha $path) -ne $file.sha256){throw 'Source asset digest mismatch'}
}
New-Item -ItemType Directory -Path $target|Out-Null
foreach($entry in Get-ChildItem -LiteralPath $source){Copy-Item -LiteralPath $entry.FullName -Destination $target -Recurse}
$repo=Split-Path $PSScriptRoot -Parent
Copy-Item -LiteralPath (Join-Path $repo 'tests/seven_class_supervision/BUSINESS_PREVIEW_README.txt') -Destination (Join-Path $target 'READ_FIRST.txt')
Copy-Item -LiteralPath (Join-Path $repo 'tests/seven_class_supervision/VALIDATION_MODE_BUDGET_20261010.txt') -Destination (Join-Path $target 'MODE_BUDGET_EVIDENCE.txt')
Copy-Item -LiteralPath (Join-Path $repo 'tools/Verify-IsolatedTrialHandoff.ps1') -Destination (Join-Path $target 'Verify-Handoff.ps1')
WriteNewJson (Join-Path $target 'feedback.template.json') ([ordered]@{
    schema='visionai.rd.isolated_preview_feedback.v1';handoff_id='ellipse-rd-preview-20261010-r1';
    business_project_id='';request_id='';task_id='';source_split='';
    runtime_sha256=$manifest.runtime_sha256;candidate_sha256=$manifest.candidate_checkpoint_sha256;
    stage='';error_code='';capability_available=$null;execution_succeeded=$null;
    actual_epoch=$null;actual_updates=$null;effective_lr=$null;typed_conclusion='';
    class_counts=@{};expected_instances=$null;human_review='PENDING';
    forbidden_fields='No business image, path, mask, overlay, screenshot or pixel data';
    production_allowed=$false
})
$files=@(Get-ChildItem -LiteralPath $target -File -Recurse|ForEach-Object{
    [ordered]@{relative_path=$_.FullName.Substring($target.Length+1).Replace('\','/');sha256=(Sha $_.FullName);bytes=$_.Length}
})
WriteNewJson (Join-Path $target 'handoff.lock.json') ([ordered]@{
    schema='visionai.rd.handoff_lock.v1';handoff_id='ellipse-rd-preview-20261010-r1';
    release_kind='INTEGRATION_PREVIEW_NOT_OPERATIONAL_ACCEPTANCE';
    source_delivery_manifest_sha256=$pin.sha256;files=$files;
    broker_registered=$false;offline_dependency_closure_verified=$false;production_allowed=$false
})
$lockSha=Sha (Join-Path $target 'handoff.lock.json')
& (Join-Path $target 'Verify-Handoff.ps1') -Root $target -ExpectedLockSha256 $lockSha
Write-Output "Delivered integration preview: $target"
Write-Output "handoff.lock.json: $lockSha"
