param(
    [switch]$Capture
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$qtBin = Join-Path $repoRoot ".tools\Qt\6.9.3\msvc2022_64\bin"
$qmlRuntime = Join-Path $qtBin "qml.exe"
$previewRoot = Join-Path $repoRoot "tools\qml-preview"
$previewFile = Join-Path $previewRoot "DubbingUiPreview.qml"

if (-not (Test-Path -LiteralPath $qmlRuntime)) {
    throw "Qt QML runtime not found: $qmlRuntime"
}

if (-not (Test-Path -LiteralPath $previewFile)) {
    throw "Preview file not found: $previewFile"
}

$env:QML2_IMPORT_PATH = $previewRoot
$env:QT_QUICK_CONTROLS_STYLE = "Basic"
$outputDirectory = Join-Path $repoRoot "out\ui-demo"
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null

if ($Capture) {
    $captureFile = Join-Path $previewRoot "DubbingUiCapture.qml"
    if (-not (Test-Path -LiteralPath $captureFile)) {
        throw "Capture harness not found: $captureFile"
    }
    & $qmlRuntime -I $previewRoot $captureFile
} else {
    & $qmlRuntime -I $previewRoot $previewFile
}
exit $LASTEXITCODE
