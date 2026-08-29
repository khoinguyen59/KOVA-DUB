#Requires -Version 5.1

<##
.SYNOPSIS
    Mandatory source and regression gate that runs before a release build.

.DESCRIPTION
    This is the single pre-build guard for LA Studio.  It turns the living
    PRE_DELIVERY_CHECKLIST.md and the historical incident log into executable
    checks.  A failed check returns a non-zero exit code, so package.ps1 must
    stop before CMake configure/build.  The gate does not mutate source files;
    it only creates the generated evidence file under out\prebuild-gate.
#>

[CmdletBinding()]
param(
    [string] $Preset = "windows-msvc-release",
    [string] $QtRoot,
    [string] $VcpkgRoot,
    [ValidateRange(1, 64)]
    [int] $MaxParallelJobs = 4
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

$gateStarted = Get-Date
$gateResults = New-Object System.Collections.Generic.List[object]
$gateFailure = $null

function Add-GateResult {
    param(
        [string] $Name,
        [string] $Status,
        [int64] $DurationMs,
        [string] $Message
    )

    $gateResults.Add([pscustomobject][ordered]@{
        name       = $Name
        status     = $Status
        durationMs = $DurationMs
        message    = $Message
    }) | Out-Null
}

function Invoke-GateStep {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Name,
        [Parameter(Mandatory = $true)]
        [scriptblock] $Action
    )

    $started = Get-Date
    Write-Host ("[GATE] {0}" -f $Name) -ForegroundColor Cyan
    try {
        $global:LASTEXITCODE = 0
        & $Action
        $commandExitCode = if ($null -eq $global:LASTEXITCODE) { 0 } else { [int]$global:LASTEXITCODE }
        if ($commandExitCode -ne 0) {
            throw "Command exited with code $commandExitCode."
        }
        $duration = [int64]((Get-Date) - $started).TotalMilliseconds
        Add-GateResult -Name $Name -Status "PASS" -DurationMs $duration -Message ""
        Write-Host ("[PASS] {0} ({1} ms)" -f $Name, $duration) -ForegroundColor Green
    } catch {
        $duration = [int64]((Get-Date) - $started).TotalMilliseconds
        $message = $_.Exception.Message
        Add-GateResult -Name $Name -Status "FAIL" -DurationMs $duration -Message $message
        Write-Host ("[FAIL] {0}: {1}" -f $Name, $message) -ForegroundColor Red
        throw
    }
}

function Resolve-GatePowerShell {
    $windowsPowerShell = Get-Command "powershell.exe" -ErrorAction SilentlyContinue
    if ($null -ne $windowsPowerShell) { return $windowsPowerShell.Source }
    $crossPlatformPowerShell = Get-Command "pwsh.exe" -ErrorAction SilentlyContinue
    if ($null -ne $crossPlatformPowerShell) { return $crossPlatformPowerShell.Source }
    throw "PowerShell executable was not found for nested gate commands."
}

function Resolve-GatePython {
    $python = Get-Command "python.exe" -ErrorAction SilentlyContinue
    if ($null -ne $python) {
        return [pscustomobject]@{ command = $python.Source; arguments = @() }
    }
    $pyLauncher = Get-Command "py.exe" -ErrorAction SilentlyContinue
    if ($null -ne $pyLauncher) {
        return [pscustomobject]@{ command = $pyLauncher.Source; arguments = @("-3") }
    }
    throw "Python was not found. The Colab/notebook contract checks cannot be skipped for a release build."
}

function Get-ChecklistPaths {
    $paths = @(
        (Join-Path $repoRoot "PRE_DELIVERY_CHECKLIST.md"),
        (Join-Path (Split-Path -Parent $repoRoot) "PRE_DELIVERY_CHECKLIST.md")
    )
    return @($paths | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -Unique)
}

try {
    Invoke-GateStep -Name "Required release files and incident register" -Action {
        $required = @(
            "PRE_DELIVERY_CHECKLIST.md",
            "CMakeLists.txt",
            "qml/Main.qml",
            "qml/pages/DubbingPage.qml",
            "qml/components/dubbing/DubbingSourceMediaPanel.qml",
            "qml/components/dubbing/DubbingColabSetupDialog.qml",
            "scripts/package.ps1",
            "scripts/run_tests.ps1",
            "scripts/lint_qml.ps1"
        )
        foreach ($relativePath in $required) {
            $path = Join-Path $repoRoot $relativePath
            if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
                throw "Required release file is missing: $relativePath"
            }
        }

        $checklists = Get-ChecklistPaths
        if ($checklists.Count -eq 0) {
            throw "PRE_DELIVERY_CHECKLIST.md was not found in the repository or its workspace root."
        }
        foreach ($checklistPath in $checklists) {
            $checklist = Get-Content -LiteralPath $checklistPath -Raw -Encoding UTF8
            $incidentNumbers = @(
                [regex]::Matches($checklist, "INC-(\d{3})") |
                    ForEach-Object { [int]$_.Groups[1].Value } |
                    Sort-Object -Unique
            )
            if ($incidentNumbers.Count -eq 0) {
                throw "No incident IDs were found in $checklistPath. The living incident log cannot be empty."
            }
            $highestIncidentNumber = ($incidentNumbers | Measure-Object -Maximum).Maximum
            for ($incidentNumber = 1; $incidentNumber -le $highestIncidentNumber; $incidentNumber++) {
                $incidentId = "INC-{0:D3}" -f $incidentNumber
                if (-not $checklist.Contains($incidentId)) {
                    throw "$incidentId is missing from $checklistPath. Update the living incident log before building."
                }
            }
        }
    }

    Invoke-GateStep -Name "Git whitespace integrity" -Action {
        # Git can emit harmless LF/CRLF normalization notices on stderr.  Do
        # not let PowerShell's ErrorActionPreference reinterpret those notices
        # as a failed gate; the process exit code is the authority here.
        $previousErrorAction = $ErrorActionPreference
        try {
            $ErrorActionPreference = "Continue"
            $gitOutput = @(& git diff --check -- . 2>&1)
        } finally {
            $ErrorActionPreference = $previousErrorAction
        }
        $gitExitCode = [int]$LASTEXITCODE
        foreach ($line in $gitOutput) { Write-Host $line }
        if ($gitExitCode -ne 0) { throw "git diff --check found whitespace errors." }
    }

    $gatePowerShell = Resolve-GatePowerShell
    $python = Resolve-GatePython

    Invoke-GateStep -Name "Catalog and runtime ABI contracts" -Action {
        & $gatePowerShell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "verify_runtime_abi.ps1")
        if ($LASTEXITCODE -ne 0) { throw "Runtime ABI contract failed." }
        & $python.command @($python.arguments) (Join-Path $PSScriptRoot "verify_catalog_checksums.py")
        if ($LASTEXITCODE -ne 0) { throw "Catalog checksum contract failed." }
    }

    $testArguments = @(
        "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
        (Join-Path $PSScriptRoot "run_tests.ps1"),
        "-Preset", $Preset,
        "-QtRoot", $QtRoot,
        "-MaxParallelJobs", $MaxParallelJobs,
        "-SkipAppBuildDependency"
    )
    if (-not [string]::IsNullOrWhiteSpace($VcpkgRoot)) {
        $testArguments += @("-VcpkgRoot", $VcpkgRoot)
    }
    Invoke-GateStep -Name "C++ regression suite and QML route smoke" -Action {
        & $gatePowerShell @testArguments
        if ($LASTEXITCODE -ne 0) { throw "C++/CTest regression suite failed." }
    }

    Invoke-GateStep -Name "QML lint" -Action {
        & $gatePowerShell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "lint_qml.ps1") -Preset $Preset -QtRoot $QtRoot
        if ($LASTEXITCODE -ne 0) { throw "QML lint failed." }
    }

    Invoke-GateStep -Name "Exact Colab controller/UI/notebook bindings" -Action {
        & $python.command @($python.arguments) (Join-Path $PSScriptRoot "verify_colab_model_bindings.py")
        if ($LASTEXITCODE -ne 0) { throw "Exact Colab model binding contract failed." }
    }

    Invoke-GateStep -Name "Generated Colab notebook integrity" -Action {
        & $python.command @($python.arguments) (Join-Path $PSScriptRoot "verify_generated_colab_notebooks.py")
        if ($LASTEXITCODE -ne 0) { throw "Generated Colab notebook contract failed." }
    }

    Invoke-GateStep -Name "Embedded Colab worker payload integrity" -Action {
        & $python.command @($python.arguments) (Join-Path $PSScriptRoot "test_colab_worker_pins.py")
        if ($LASTEXITCODE -ne 0) { throw "Embedded Colab worker unit tests failed." }
        & $python.command @($python.arguments) (Join-Path $PSScriptRoot "verify_colab_worker_pins.py")
        if ($LASTEXITCODE -ne 0) { throw "Embedded Colab worker payload integrity failed." }
    }

    Invoke-GateStep -Name "Unified Dubbing Colab contract" -Action {
        & $python.command @($python.arguments) (Join-Path $PSScriptRoot "verify_unified_dubbing_colab_notebook.py")
        if ($LASTEXITCODE -ne 0) { throw "Unified Dubbing Colab contract failed." }
    }

    Invoke-GateStep -Name "Remote feature and exact-worker surface" -Action {
        & $gatePowerShell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot "verify_remote_feature_surface.ps1")
        if ($LASTEXITCODE -ne 0) { throw "Remote feature surface contract failed." }
    }
} catch {
    $gateFailure = $_.Exception.Message
}

$gateFinished = Get-Date
$evidenceDirectory = Join-Path $repoRoot "out\prebuild-gate"
New-Item -ItemType Directory -Path $evidenceDirectory -Force | Out-Null
$evidence = [ordered]@{
    schemaVersion = 1
    status        = if ($null -eq $gateFailure) { "PASS" } else { "FAIL" }
    startedAt     = $gateStarted.ToString("o")
    finishedAt    = $gateFinished.ToString("o")
    durationMs    = [int64](($gateFinished - $gateStarted).TotalMilliseconds)
    preset        = $Preset
    qtRoot        = $QtRoot
    vcpkgRoot     = $VcpkgRoot
    maxParallelJobs = $MaxParallelJobs
    repository    = $repoRoot
    checklist     = @(Get-ChecklistPaths)
    failure       = $gateFailure
    checks        = @($gateResults.ToArray())
}
$evidencePath = Join-Path $evidenceDirectory "latest.json"
[IO.File]::WriteAllText(
    $evidencePath,
    ($evidence | ConvertTo-Json -Depth 8),
    [Text.UTF8Encoding]::new($false))

if ($null -ne $gateFailure) {
    Write-Error ("Pre-build release gate failed. Build is blocked. Evidence: {0}" -f $evidencePath)
    exit 1
}

Write-Host ("[SUCCESS] Pre-build release gate passed. Evidence: {0}" -f $evidencePath) -ForegroundColor Green
exit 0
