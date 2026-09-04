[CmdletBinding()]
param(
    [switch] $Uninstall
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

if ($Uninstall) {
    & git -C $repoRoot config --local --unset core.hooksPath 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "LA Studio Git hooks disabled for this checkout."
    } else {
        Write-Host "No repository-local Git hooks configuration was present."
    }
    exit 0
}

$hooksDirectory = Join-Path $repoRoot ".githooks"
if (-not (Test-Path -LiteralPath (Join-Path $hooksDirectory "pre-commit"))) {
    throw "Missing repository hook: $hooksDirectory\pre-commit"
}
if (-not (Test-Path -LiteralPath (Join-Path $hooksDirectory "pre-push"))) {
    throw "Missing repository hook: $hooksDirectory\pre-push"
}

& git -C $repoRoot config --local core.hooksPath .githooks
if ($LASTEXITCODE -ne 0) {
    throw "Could not configure core.hooksPath for $repoRoot"
}
Write-Host "LA Studio Git hooks installed: notebook dependency and embedded-worker checks run before commit/push."
