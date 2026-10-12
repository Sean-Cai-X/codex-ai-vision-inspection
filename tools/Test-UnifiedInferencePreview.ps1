param(
    [string]$Root=$PSScriptRoot,
    [Parameter(Mandatory=$true)][string]$ExpectedLockSha256,
    [Parameter(Mandatory=$true)][string[]]$DependencyDirectories,
    [string]$OutputBase,
    [switch]$ValidateOnly
)
$ErrorActionPreference='Stop'
function Sha([string]$p){'sha256:'+(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant()}
try {
    & (Join-Path $Root 'Verify-Handoff.ps1') -Root $Root -ExpectedLockSha256 $ExpectedLockSha256
    $lock=Get-Content (Join-Path $Root 'dependency.lock.json') -Raw|ConvertFrom-Json
    $dirs=@($DependencyDirectories|ForEach-Object{(Resolve-Path -LiteralPath $_).Path})
    $lookup=$dirs+@([Environment]::SystemDirectory)
    foreach($dep in $lock.dependencies){
        $matches=@($lookup|ForEach-Object{Join-Path $_ $dep.file}|Where-Object{Test-Path -LiteralPath $_ -PathType Leaf})
        if($matches.Count -eq 0){throw "DEPENDENCY_MISSING: $($dep.file)"}
        foreach($p in $matches){if((Sha $p) -cne $dep.sha256){throw "DEPENDENCY_SHA_MISMATCH: $($dep.file)"}}
    }
    if($ValidateOnly){Write-Output 'PASS: dependency pins; inference NOT executed';exit 0}
    if([string]::IsNullOrWhiteSpace($OutputBase)){throw 'NEW_EVIDENCE_OUTPUT_BASE_REQUIRED'}
    & (Join-Path $Root 'tools/rd_isolated_preview.exe') $Root $ExpectedLockSha256 (Join-Path $Root 'rules/seven_class_supervision_rules.v1.json') $OutputBase @dirs
    if($LASTEXITCODE -ne 0){throw "TYPED_PREVIEW_ACCEPTANCE_FAILED: native exit $LASTEXITCODE"}
} catch {
    Write-Error ('PREVIEW_PREFLIGHT_OR_EXECUTION_FAILED: '+$_.Exception.Message)
    exit 1
}
