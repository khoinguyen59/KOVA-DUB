# FLAC Audio Transport and WAV Compatibility Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make FLAC the default lossless audio transport and normalized cache format, retain WAV compatibility, accept compressed source audio safely, and verify every dubbing audio path before building the next portable EXE.

**Architecture:** Keep `AudioFileDecoder` as the one decoder boundary for user-provided compressed or WAV audio. Change Media Ingest to emit lossless FLAC cache artifacts, change remote clients to preserve MIME/filename and request FLAC by default, and update WAV-only consumers to decode through the shared boundary. Existing projects and model-specific WAV APIs remain supported through explicit compatibility adapters.

**Tech Stack:** C++20, Qt 6 Multimedia/Core/Network, FFmpeg/FFprobe, QTest/CTest, QML, Python Colab workers, PowerShell release gate.

**Spec:** `docs/superpowers/specs/2026-08-30-flac-audio-transport-design.md`

## Global Constraints

- FLAC is the default lossless interchange format; WAV is compatibility-only or temporary when required.
- MP3/M4A/OGG/OPUS/WMA/AIFF/AAC/WAV/FLAC input is decoded before DSP or model inference.
- Legacy `.ladub.json` projects with WAV paths must continue to load.
- Colab separation output defaults to FLAC; WAV remains selectable.
- No synchronous whole-file decode may run on the UI thread.
- `scripts/package.ps1` is the only accepted release packaging entry point.
- The final package must pass the repository prebuild gate, CTest, QML lint, notebook validators, and packaged QML smoke.

---

### Task 1: Add deterministic audio format helpers and regression fixtures

**Files:**
- Modify: `src/audio/io/AudioFileDecoder.h`
- Modify: `src/audio/io/AudioFileDecoder.cpp`
- Modify: `tests/core/test_AudioPreviewService.cpp`
- Modify: `tests/core/test_MediaIngestService.cpp`
- Modify: `tests/dubbing/test_DubbingProject.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Preserve `AudioFileDecoder::decode()` and `decodeMono()` signatures.
- Add only small internal helpers for supported extensions/MIME and decoder-backed validation; do not duplicate decoder policy in controllers.

- [ ] **Step 1: Write failing tests** for a valid WAV and a generated FLAC fixture, asserting `AudioFileDecoder::decode()` returns non-empty samples, correct channel count, and positive sample rate for both formats. Add a compressed-format contract assertion that `audio/flac` is accepted by the remote MIME helper.
- [ ] **Step 2: Run the focused tests** and confirm the new FLAC/MIME assertions fail or expose the current strict-WAV path.
- [ ] **Step 3: Implement the minimal shared format policy** in the decoder layer, retaining Qt-first and FFmpeg fallback behavior and returning a bounded error for invalid files.
- [ ] **Step 4: Run the focused audio tests** and confirm they pass.
- [ ] **Step 5: Commit the focused decoder/test change** with message `test: cover flac audio decoding and transport policy`.

### Task 2: Change Media Ingest cache artifacts from WAV to FLAC with legacy compatibility

**Files:**
- Modify: `src/dubbing/media/MediaIngestService.cpp`
- Modify: `src/dubbing/media/MediaIngestService.h`
- Modify: `tests/core/test_MediaIngestService.cpp`
- Modify: `tests/dubbing/test_DubbingProject.cpp`

**Interfaces:**
- Keep manifest keys `masterAudioPath` and `analysisAudioPath` unchanged.
- New successful ingest returns paths ending in `master.flac` and `analysis.flac`.
- Existing project paths remain unchanged when they point to existing WAV files.

- [ ] **Step 1: Add failing source-contract assertions** requiring FLAC output paths and decoder-backed validation instead of `WavIO::loadAsFloat()` validation.
- [ ] **Step 2: Run the focused Media Ingest tests** and confirm they fail against the current `.wav` paths and WAV-only validation.
- [ ] **Step 3: Change the normalization FFmpeg outputs** to `master.flac` at 48 kHz stereo and `analysis.flac` at 16 kHz mono using lossless FLAC encoding. Change staging names to `.staging.flac`.
- [ ] **Step 4: Replace normalized-artifact validation** with `AudioFileDecoder::decode()` and keep the existing asynchronous watcher and atomic commit behavior.
- [ ] **Step 5: Add cache migration lookup** that accepts an existing valid legacy `master.wav`/`analysis.wav` pair without renaming or deleting it.
- [ ] **Step 6: Run focused Media Ingest and project persistence tests** and confirm both new FLAC and legacy WAV cases pass.
- [ ] **Step 7: Commit with message** `feat: store normalized dubbing audio as flac`.

### Task 3: Make Colab audio upload/download explicitly FLAC-first

**Files:**
- Modify: `src/controllers/dubbing/DubbingJobRunner.cpp`
- Modify: `src/remote/colab/ColabWorkerClient.cpp`
- Modify: `src/remote/colab/ColabWorkerClient.h`
- Modify: `src/separation/runners/ColabSeparationRunner.cpp`
- Modify: `notebooks/workers/LA_STUDIO_SEPARATION_SPLEETER_2STEMS_WORKER.py`
- Modify: `tests/colab/test_ColabSeparationRunner.cpp`
- Modify: `tests/colab/test_ColabSttRunner.cpp`

**Interfaces:**
- `ColabWorkerClient::createSeparationJob()` sends the actual FLAC filename/MIME when given a FLAC path and sends `output_format=flac` by default.
- Separation download accepts the response format requested by the job and preserves the returned extension.
- Existing worker API fields `output_format`, `artifact_format`, `vocals`, and `background` remain unchanged.

- [ ] **Step 1: Add failing tests** that inspect the multipart request contract and require FLAC input metadata for a FLAC path, plus FLAC as the default separation output.
- [ ] **Step 2: Run the focused Colab tests** and confirm the current path still uses `master.wav` in the normal workflow and the client fallback is WAV-oriented.
- [ ] **Step 3: Ensure the runner receives the new `master.flac` path** and normalizes the requested transfer format to either `flac` or `wav`.
- [ ] **Step 4: Ensure the remote client preserves the real filename/MIME** and never labels FLAC as `audio.wav`.
- [ ] **Step 5: Verify worker source and generated notebook contracts** accept FLAC input, return FLAC by default, and retain temporary WAV conversion only inside the worker job directory.
- [ ] **Step 6: Run focused Colab contract tests and notebook validators** and confirm they pass.
- [ ] **Step 7: Commit with message** `feat: make colab audio transfer flac-first`.

### Task 4: Remove remaining WAV-only readers from downstream audio paths

**Files:**
- Modify: `src/dubbing/audio/AudioTimelineMixer.cpp`
- Modify: `src/controllers/dubbing/DubbingJobRunner.cpp`
- Modify: `src/controllers/dubbing/DubbingExportJob.cpp`
- Modify: `src/dubbing/fusion/AlignmentRefinementService.cpp`
- Modify: `src/controllers/shared/AudioPreviewService.cpp`
- Modify: `tests/dubbing/test_DubbingProject.cpp`
- Modify: `tests/core/test_AudioPreviewService.cpp`

**Interfaces:**
- Every user-supplied audio path is decoded with `AudioFileDecoder`.
- Generated WAV clips and previews remain valid; FLAC/MP3 uploaded background or voice artifacts must also be mixable or previewable.
- Output paths may remain WAV where downstream container/export contracts require them.

- [ ] **Step 1: Add failing tests** mixing a FLAC background and an MP3 fixture, loading a FLAC alignment input, and previewing a FLAC path without returning an empty waveform.
- [ ] **Step 2: Run the focused tests** and confirm strict `WavIO` calls reject at least one compressed fixture.
- [ ] **Step 3: Replace strict reads** in the mixer, alignment, preview and export validation with `AudioFileDecoder` or `decodeMono()`; keep `WavIO` only for actual WAV serialization.
- [ ] **Step 4: Preserve asynchronous preview and worker-thread behavior** so compressed decoding cannot freeze QML.
- [ ] **Step 5: Run the focused tests** and confirm all mixed/previewed formats pass with actionable errors for invalid files.
- [ ] **Step 6: Commit with message** `fix: decode compressed audio across dubbing consumers`.

### Task 5: Align manual artifact contracts and model compatibility behavior

**Files:**
- Modify: `src/controllers/dubbing/DubbingController.cpp`
- Modify: `src/controllers/dubbing/parts/DubbingController_Artifacts.cpp`
- Modify: `qml/components/dubbing/DubbingNodeSettingsPanel.qml`
- Modify: `qml/components/dubbing/DubbingArtifactUploadPanel.qml`
- Modify: `tests/dubbing/test_DubbingWorkspaceContract.cpp`
- Modify: `tests/dubbing/test_DubbingProject.cpp`

**Interfaces:**
- Separate manual upload continues to accept arbitrary names and supported audio extensions with role order: Vocals first, Background second.
- Automatic Separate contract displays FLAC as default and WAV as compatibility option.
- TTS/clone model-specific WAV requirements remain truthful and are not mislabeled as FLAC support.

- [ ] **Step 1: Add failing contract tests** for FLAC default labels, WAV compatibility labels, arbitrary manual Separate filenames, and rejection of unsupported extensions.
- [ ] **Step 2: Run the focused contract tests** and record any stale WAV-only UI text or filename assumptions.
- [ ] **Step 3: Update controller/QML contract text** so the visible transfer choice says `Lossless FLAC (recommended)` and `PCM WAV (compatibility)` with no claim that every model accepts FLAC directly.
- [ ] **Step 4: Verify manual upload dispatch** sends compressed audio to the next task and keeps role mapping intact.
- [ ] **Step 5: Run focused contract/QML tests** and commit with message `fix: keep audio artifact contracts format-aware`.

### Task 6: Cross-task static and runtime recheck

**Files:**
- Modify: `PRE_DELIVERY_CHECKLIST.md`
- Modify: `scripts/prebuild_gate.ps1`
- Modify: `tests/dubbing/test_DubbingWorkspaceContract.cpp`
- Create: `tests/core/test_AudioFormatCompatibility.cpp`
- Create: `tests/core/test_AudioFormatCompatibility.h`

**Interfaces:**
- The prebuild gate must fail if production code introduces a new WAV-only read for user-supplied audio without an explicit compatibility comment.
- The checklist must require the same audio-format sweep across all eight workflow tasks whenever one audio-format bug is fixed.

- [ ] **Step 1: Write failing static-contract tests** requiring the checklist to mention cross-task audio-format regression and requiring `AudioFileDecoder` use in the known consumer files.
- [ ] **Step 2: Run the new test** and confirm the current checklist/gate does not contain the new requirement.
- [ ] **Step 3: Add the cross-task recheck rule** and format assertions to the checklist/gate.
- [ ] **Step 4: Run all CTest and gate checks** and fix any unrelated stale documentation assertions caused by the deliberate FLAC path change.
- [ ] **Step 5: Commit with message** `test: enforce cross-task audio format recheck`.

### Task 7: Reconfigure, package, and verify the portable EXE

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `docs/BUILD.md`
- Modify: `docs/RELEASE.md`
- Modify: `docs/AI_AGENT_RESPONSE_REPORT.md`

**Interfaces:**
- Increment the internal build version from `0.0.8.8` to `0.0.8.9` only after source/test changes are complete.
- Package output is `out\LA-Studio-0.0.8.9\LA-Studio-0.0.8.9.exe` using `scripts/package.ps1 -SkipInstaller -PortableInternalLayout`.

- [ ] **Step 1: Run `git diff --check` and the focused audio tests** before packaging.
- [ ] **Step 2: Run the full repository prebuild gate** and stop if any gate fails.
- [ ] **Step 3: Run full CTest and QML lint** with the release version.
- [ ] **Step 4: Run notebook/worker validators** and verify all generated worker pins and FLAC contracts.
- [ ] **Step 5: Package the portable EXE** through `scripts/package.ps1` with the existing internal eSpeak flag and no installer.
- [ ] **Step 6: Run production packaged QML smoke** using the staged EXE, checking startup, task navigation, upload contract, Separate format choice, and no fatal diagnostics.
- [ ] **Step 7: Verify EXE version, required runtime payload, file hash, and clean process shutdown**; write the evidence into the release report.
- [ ] **Step 8: Commit the final source/docs/package metadata** with message `release: flac-first audio transport 0.0.8.9`.

## Self-review

- Scope is covered across ingest, transfer, workers, model adapters, downstream consumers, checklist, tests, and packaging.
- No task changes a model endpoint to claim unsupported FLAC output; model compatibility remains explicit.
- Legacy WAV projects are covered by the Media Ingest compatibility rule and project-path authority.
- Every implementation task has a failing-test step, a focused verification step, and an explicit commit boundary.
