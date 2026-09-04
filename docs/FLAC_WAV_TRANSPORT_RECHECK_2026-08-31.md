# FLAC/WAV Transport Recheck and Release Acceptance

**Date:** 2026-09-01
**Application:** LA Studio 0.0.9.0
**Scope:** audio ingest, normalization, Colab transfer, manual artifacts, preview, alignment, mixing, export, and portable packaging.

## 1. Decision

FLAC is now the default lossless interchange format for normalized cache and
source-separation handoff. WAV remains a supported compatibility format for
legacy projects and for model/notebook contracts that explicitly return WAV.
MP3 and other published input formats are decoded before DSP, preview, mixing,
alignment, or export; they are not promoted to the canonical lossless cache.

This avoids the two unsafe alternatives: sending very large PCM WAV files by
default, or renaming WAV/FLAC bytes without changing the actual codec.

## 2. Implemented changes

### Desktop audio pipeline

- `MediaIngestService` writes `master.flac` (normalized 48 kHz stereo) and
  `analysis.flac` (16 kHz mono) after EBU R128 normalization.
- Existing manifests containing `master.wav` or `analysis.wav` remain valid;
  legacy files are read without forced migration or deletion.
- `AudioFileDecoder` is the shared decode boundary for user-provided audio.
  Preview waveform generation runs through `QThreadPool`; mixer, alignment,
  dubbed preview, and export validation no longer assume WAV-only input.
- Invalid audio is no longer silently skipped in the timeline mixer; the
  operation returns an actionable path/decoder error.
- Generated TTS clips and the local mixed preview remain WAV where the local
  renderer and downstream export contracts require PCM WAV. This is deliberate
  serialization, not a false FLAC label.

### Colab and artifact contracts

- Source separation requests default to `output_format=flac` and validate the
  returned FLAC magic bytes and MIME type. WAV remains an explicit fallback.
- The separation worker keeps a temporary 44.1 kHz stereo WAV only inside its
  isolated job directory for model inference, then returns FLAC/WAV according to
  the requested format.
- Filename and MIME are derived from the actual payload. FLAC is never sent as
  `audio/wav` and WAV is never presented as FLAC.
- STT/TTS/alignment worker contracts that currently return WAV remain WAV. They
  were not relabeled; changing those contracts would require synchronized
  endpoint, notebook, downloader, parser, and acceptance changes.
- Normalize, TTS voice, fit-timing, and mix manual artifact contracts accept
  both `.wav` and `.flac`. Separate manual handoff keeps the selected role order
  (Vocals first, Background second) and accepts the published audio extensions.

### Release tooling

- `package.ps1` now uses the .NET SHA-256 helper instead of relying on the
  optional `Get-FileHash` module.
- eSpeak signature verification is fail-closed for distributable builds and
  reports an explicit warning for the authorized internal build when the
  PowerShell Security module is unavailable. SHA-256 remains mandatory.

## 3. Compatibility matrix

| Input/artifact | Decode | Preview | Mix/alignment/export | Colab separation | Canonical output |
|---|---:|---:|---:|---:|---|
| Legacy WAV project | PASS | PASS | PASS | Optional WAV | Existing path preserved |
| FLAC source/cache | PASS | PASS | PASS | FLAC default | `master.flac` / `analysis.flac` |
| MP3 source/reference | PASS | PASS | PASS where task accepts audio | Decode before worker | Never canonical lossless |
| Manual vocals FLAC | PASS | PASS | PASS | N/A | Role mapped to vocals/STT |
| Manual background MP3/FLAC | PASS | PASS | PASS | N/A | Role mapped to final mix |
| Separation FLAC response | PASS | PASS | PASS | PASS | `vocals.flac` + `background.flac` |
| STT/TTS/alignment WAV response | PASS | PASS | PASS | Existing contract | WAV retained truthfully |

## 4. Verification evidence

The separation completion regression was rechecked after the previous gate
failure. The caller-thread non-blocking assertion remains below 150 ms; the
completion wait now allows the measured decoder/stem-write path while still
having a bounded 8-second test limit. The targeted test and the full gate both
pass.

- Focused `TestMediaIngestService`: **32 passed, 0 failed**.
- Focused `TestAudioPreviewService`: **4 passed, 0 failed**.
- Focused `TestColabSeparationRunner`: **11 passed, 0 failed**.
- Focused `TestDubbingProject`: **129 passed, 0 failed**.
- Full CTest: **41/41 PASS** (100%).
- QML lint: **PASS**.
- Generated exact-model notebooks: **32/32 verified**.
- Embedded Colab workers: **2/2 verified**.
- Exact controller/UI/notebook bindings: **31/31 verified**.
- Live Colab acceptance contract: **9/9 capability paths**.
- Remote feature surface: **8/8 direct routes**.
- Packaged QML smoke: **PASS**, 19 interaction trace events, empty stderr.

Evidence files:

- `out\prebuild-gate\latest.json`
- `out\prebuild-gate\colab-worker-pins.json`
- `out\package-smoke\0.0.9.0\qml-interaction-trace.json`
- `out\package-smoke\0.0.9.0\data\logs\app.log`

## 5. Portable artifact

- **Path:** `out\LA-Studio-0.0.9.0\LA-Studio-0.0.9.0.exe`
- **Size:** 31,028,736 bytes
- **SHA-256:** `6839d58e730885349884f1f7a86720647a21b6454201992c7b03b31fe6079f65`
- **Layout:** portable internal layout, no installer, runtime DLLs and media
  tools staged beside the executable.

## 6. Known non-fatal warnings and boundary

The packaged log contains the existing Qt warning that the deployment has no
`lib/fonts` directory, plus the expected missing Vulkan-header diagnostic.
The app still initializes, loads the QML module, starts Qt Multimedia with its
FFmpeg backend, and exits the smoke test successfully. The eSpeak MSI used here
is SHA-256 verified but has unavailable Authenticode status in this constrained
PowerShell host; therefore this artifact is an internal build, not a claim of a
signed public release.

A real GPU inference run still requires the user's external Colab worker and is
not represented by the offline contract tests. The desktop-side format and MIME
contracts are verified independently of that external session.
