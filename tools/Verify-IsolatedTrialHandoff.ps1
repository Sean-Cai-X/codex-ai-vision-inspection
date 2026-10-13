param([string]$Root=$PSScriptRoot,[Parameter(Mandatory=$true)][string]$ExpectedLockSha256)
$ErrorActionPreference='Stop'
function Sha([string]$p){'sha256:'+(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
$rootPath=(Resolve-Path -LiteralPath $Root).Path.TrimEnd('\')
$lockPath=Join-Path $rootPath 'handoff.lock.json'
if((Sha $lockPath) -cne $ExpectedLockSha256){throw 'HANDOFF_LOCK_SHA_MISMATCH'}
$lock=Get-Content -LiteralPath $lockPath -Raw|ConvertFrom-Json
foreach($asset in $lock.files){
    $p=[IO.Path]::GetFullPath((Join-Path $rootPath $asset.relative_path))
    if(-not $p.StartsWith($rootPath+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'HANDOFF_PATH_ESCAPE'}
    if((Sha $p) -cne $asset.sha256){throw 'HANDOFF_ASSET_SHA_MISMATCH'}
}
Write-Output "PASS: $($lock.files.Count) file digests; integrity only, not runtime/bridge acceptance."
