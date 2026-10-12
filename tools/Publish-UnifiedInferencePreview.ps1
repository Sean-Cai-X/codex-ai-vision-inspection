param(
    [Parameter(Mandatory=$true)][string]$CirclePackage,
    [Parameter(Mandatory=$true)][string]$EllipsePackage,
    [Parameter(Mandatory=$true)][string]$CircleAcceptance,
    [Parameter(Mandatory=$true)][string]$EllipseAcceptance,
    [Parameter(Mandatory=$true)][string]$Destination
)
$ErrorActionPreference='Stop'
function Sha([string]$p){'sha256:'+(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
function Json([string]$p){Get-Content -LiteralPath $p -Raw|ConvertFrom-Json}
function NewJson([string]$p,$v){if(Test-Path -LiteralPath $p){throw 'OVERWRITE_FORBIDDEN'};[IO.File]::WriteAllText($p,($v|ConvertTo-Json -Depth 80),(New-Object Text.UTF8Encoding($false)))}
$root=[IO.Path]::GetFullPath($Destination).TrimEnd('\')
if(Test-Path -LiteralPath $root){throw 'NEW_HANDOFF_DIRECTORY_REQUIRED'}
$sources=@(@{name='circle';package=$CirclePackage;acceptance=$CircleAcceptance},@{name='ellipse';package=$EllipsePackage;acceptance=$EllipseAcceptance})
foreach($source in $sources){
    $package=(Resolve-Path -LiteralPath $source.package).Path
    if($root.Equals($package,[StringComparison]::OrdinalIgnoreCase) -or $root.StartsWith($package+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'HANDOFF_INSIDE_SOURCE_FORBIDDEN'}
    $report=Json $source.acceptance
    $delivery=Json (Join-Path $package 'delivery_manifest.json')
    if($report.status -ne 'PASS' -or $report.handoff_lock_sha256 -cne (Sha (Join-Path $package 'handoff.lock.json')) -or
        $report.runtime_sha256 -cne $delivery.runtime_sha256 -or $report.candidate_sha256 -cne $delivery.candidate_checkpoint_sha256 -or
        $report.production_allowed -or $report.model_quality_accepted -or -not $report.default_session_unchanged -or
        -not $report.external_preview_training_rejected -or -not $report.real_broker -or -not $report.real_runtime -or
        -not $report.real_typed_transport -or $report.observations.Count -ne 5){throw 'REAL_TYPED_ACCEPTANCE_REQUIRED'}
    $exeSha=Sha (Join-Path $package 'tools/rd_isolated_preview.exe')
    if(@($report.loaded_modules|Where-Object{$_.file -eq 'rd_isolated_preview.exe' -and $_.sha256 -ceq $exeSha}).Count -ne 1){throw 'ACCEPTANCE_EXECUTABLE_MISMATCH'}
    foreach($row in $report.observations){if(-not $row.verified -or -not $row.raw_typed_counts_equal -or $row.strict_loaded_tensors -ne 417){throw 'INCOMPLETE_INFERENCE_ACCEPTANCE'}}
    & (Join-Path $package 'Verify-Handoff.ps1') -Root $package -ExpectedLockSha256 $report.handoff_lock_sha256
}
New-Item -ItemType Directory -Path $root|Out-Null
New-Item -ItemType Directory -Path (Join-Path $root 'acceptance')|Out-Null
$index=@()
foreach($source in $sources){
    Copy-Item -LiteralPath $source.package -Destination (Join-Path $root $source.name) -Recurse
    $report=Json $source.acceptance
    Copy-Item -LiteralPath $source.acceptance -Destination (Join-Path $root ('acceptance/'+$source.name+'.json'))
    $index+=@{geometry=$source.name;package_root=$source.name;handoff_lock_sha256=$report.handoff_lock_sha256;
        runtime_sha256=$report.runtime_sha256;candidate_sha256=$report.candidate_sha256;
        acceptance_sha256=(Sha $source.acceptance);acceptance_path=('acceptance/'+$source.name+'.json')}
    & (Join-Path $root ($source.name+'/Verify-Handoff.ps1')) -Root (Join-Path $root $source.name) -ExpectedLockSha256 $report.handoff_lock_sha256
}
NewJson (Join-Path $root 'delivery_index.json') ([ordered]@{
    schema='visionai.rd.isolated_preview_collection.v1';handoff_id='circle-ellipse-unified-v5-20261012-r1';
    status='READY_FOR_BUSINESS_ISOLATED_INFERENCE_TEST';scope='INFERENCE_ONLY';
    model_quality_accepted=$false;production_allowed=$false;auto_activate=$false;packages=$index;
    import_instruction='Select circle OR ellipse subdirectory as external_preview_root, NOT this collection directory.';
    dependencies='Installed pinned LibTorch/OpenCV required; run Test-Preview.ps1 before import.';
    training='External preview rejects training. Runtime training remains v2, NOT unified-v5.'
})
$repo=Split-Path $PSScriptRoot -Parent
Copy-Item -LiteralPath (Join-Path $repo 'tools/Verify-IsolatedTrialHandoff.ps1') -Destination (Join-Path $root 'Verify-Handoff.ps1')
Copy-Item -LiteralPath (Join-Path $repo 'tests/isolated_preview/BUSINESS_HANDOFF_20261012.txt') -Destination (Join-Path $root 'START_HERE.txt')
$files=@(Get-ChildItem -LiteralPath $root -File -Recurse|Sort-Object FullName|ForEach-Object{
    [ordered]@{relative_path=$_.FullName.Substring($root.Length+1).Replace('\','/');sha256=(Sha $_.FullName);bytes=$_.Length}
})
NewJson (Join-Path $root 'handoff.lock.json') ([ordered]@{schema='visionai.rd.handoff_lock.v1';handoff_id='circle-ellipse-unified-v5-20261012-r1';files=$files;production_allowed=$false})
$sha=Sha (Join-Path $root 'handoff.lock.json')
& (Join-Path $root 'Verify-Handoff.ps1') -Root $root -ExpectedLockSha256 $sha
Write-Output "READY_FOR_BUSINESS_ISOLATED_INFERENCE_TEST: $root"
Write-Output "INDEPENDENT_COLLECTION_LOCK_SHA256: $sha"
