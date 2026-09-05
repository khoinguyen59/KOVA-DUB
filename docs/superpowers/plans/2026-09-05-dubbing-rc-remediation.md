# Dubbing RC Remediation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the twelve runtime risks RC-01…RC-12 identified in `docs/AI_AGENT_RESPONSE_REPORT.md` while preserving the eight-step Dubbing workflow, manual handoff, independent STT/OCR routes, and project persistence.

**Architecture:** Treat an uploaded artifact as a transaction: validate and stage it on a worker first, then commit project state and cancel only the matching producer after the validation succeeds. Make render inputs explicit (manual voice bed versus segment clips; BGM versus original vocals), keep all generated files project/run scoped, and use a filter-complex script rather than a command-line-sized FFmpeg graph. Each controller mutation is protected by a real Qt regression test, while QML consumes only notifications from the controller.

**Tech Stack:** C++17, Qt 6 (QtConcurrent/QFutureWatcher/QProcess), QML, FFmpeg/FFprobe, CTest/QtTest, PowerShell packaging.

**Spec:** `docs/AI_AGENT_RESPONSE_REPORT.md` (RC-01…RC-12), `PRE_DELIVERY_CHECKLIST.md`, and the Dubbing workflow requirements supplied in this conversation.

## Global Constraints

- STT and Subtitle OCR remain independently runnable, uploadable, and cancellable; a successful result from either may advance the visible Transcribe step.
- Manual handoff accepts user filenames but validates the actual media/subtitle content before changing workflow state.
- Invalid local input must never cancel an active matching Colab/local worker, replace segments, or mark a stage complete.
- Every materialized artifact stays beneath the saved project directory; credentials never enter project JSON, artifacts, reports, source control, or tests.
- QML must never wait for probing, copying, decoding, rendering, or a network/process timeout on the GUI thread.
- Existing user untracked files are out of scope and must not be deleted or committed.
- Each production change starts with a focused failing regression test, followed by its focused green run and a report/checklist update.

---

### Task 1: Transactional, non-blocking manual artifact handoff (RC-01, RC-03, RC-08)

**Files:**
- Modify: `src/controllers/dubbing/DubbingController.h`, `src/controllers/dubbing/DubbingController.cpp`, `src/controllers/dubbing/parts/DubbingController_Artifacts.cpp`.
- Modify: `qml/components/dubbing/DubbingArtifactUploadPanel.qml`, `qml/components/dubbing/DubbingArtifactUploadDialog.qml`.
- Modify: `tests/dubbing/test_DubbingProject.h`, `tests/dubbing/test_DubbingProject.cpp`.

**Interfaces:**
- Produces `DubbingController::importWorkflowArtifactFiles(QString, QVariantList) -> bool`, which means *accepted for background validation*, and `workflowArtifactImportFinished(QString nodeId, bool success)`.
- Produces `artifactImportProcessing` and `artifactImportNodeId` properties/signals so only the active Upload card is busy.
- Consumes current `workflowArtifactSpec()`, `DubbingSubtitleService::importFile()`, and `dubbingArtifactStageDirectory()`.

- [ ] **Step 1: Write failing tests for valid media, malformed media, and transactional subtitle import.**

  Add `manualArtifactUploadRejectsRenamedTextAudioBeforeStateMutation()` and `manualArtifactUploadDoesNotCancelWorkerUntilSubtitleValidationSucceeds()` to `TestDubbingProject`. The first creates a real WAV named freely for a valid role upload and a text file renamed `.mp3`; it must observe `workflowArtifactImportFinished`, reject the bogus file, retain pre-import vocals/background and report an error. The second starts the test OCR/STT producer, submits an invalid SRT, and asserts the producer is still active and current segments are unchanged.

- [ ] **Step 2: Run the new tests and record expected RED evidence.**

  Run: `out/build/windows-msvc-release/LAStudioUnitTests.exe manualArtifactUploadRejectsRenamedTextAudioBeforeStateMutation manualArtifactUploadDoesNotCancelWorkerUntilSubtitleValidationSucceeds`

  Expected before implementation: the renamed-text media is accepted or the test cannot observe a completion signal, and subtitle import may cancel before parsing is proved valid.

- [ ] **Step 3: Add a worker-only staging/validation result.**

  Add a `QFutureWatcher<QVariantMap>` owned by `DubbingController`. Its QtConcurrent lambda captures only copied values: project path, resolved node id, spec, source paths, current segment snapshot, and an atomic cancel token. It must:

  ```cpp
  // The worker result is applied only after every selected source validates.
  // Audio/video: bounded ffprobe detects a stream and duration > 0.
  // Subtitle: DubbingSubtitleService::importFile plus untimed mapping succeeds.
  // Copy: write into <stage>/.incoming-<uuid>/, never overwrite existing artifacts.
  return QVariantMap{{"success", true}, {"nodeId", id},
                     {"projectPath", projectPath}, {"stagingDirectory", staging},
                     {"copiedPaths", copiedPaths}, {"parsedSegments", parsed}};
  ```

  The worker must remove its staging directory on a failed validation or cancellation. The audio preflight requires an audio stream; the export preflight requires the media stream appropriate to its contract. It accepts every declared extension and never bases role on a filename.

- [ ] **Step 4: Apply only a successful result on the controller thread.**

  Move state mutation from `importWorkflowArtifactFiles()` into an apply helper. It first verifies the project path still matches the open project, commits `.incoming-<uuid>` to `manual-<uuid>`, then cancels only a matching active worker and mutates state. For STT/OCR/review/translation it must use already parsed subtitle rows rather than call the parser after cancellation. Emit one completion signal on every terminal path.

- [ ] **Step 5: Update QML to wait for the completion signal.**

  `DubbingArtifactUploadPanel` starts import and displays a busy state; it no longer reports `artifactAccepted` immediately from the boolean return. `DubbingArtifactUploadDialog` handles `workflowArtifactImportFinished` for the requested card, closes only when all selected contracts succeed, and leaves the picker visible with `lastError` when one fails.

- [ ] **Step 6: Run focused tests to GREEN, then the whole Dubbing project suite.**

  Run the two new tests, `manualSeparationUploadAcceptsRoleBasedAudioNames`, all manual upload/transcript tests, and finally `LAStudioUnitTests.exe` with `-o -,txt` filtered to `TestDubbingProject`.

- [ ] **Step 7: Update RC-01/03/08 evidence immediately.**

  Update `docs/AI_AGENT_RESPONSE_REPORT.md` and `PRE_DELIVERY_CHECKLIST.md` with test names, red/green result, and any still-uncovered manual live test.

### Task 2: Invalidate derived media and respect manual voice bed selection (RC-02, RC-05)

**Files:**
- Modify: `src/controllers/dubbing/DubbingJobRunner.h`, `src/controllers/dubbing/DubbingJobRunner.cpp`.
- Modify: `src/controllers/dubbing/DubbingController.h`, `src/controllers/dubbing/parts/DubbingController_Artifacts.cpp`, `src/controllers/dubbing/parts/DubbingController_Workflow.cpp`.
- Modify: `tests/dubbing/test_DubbingProject.h`, `tests/dubbing/test_DubbingProject.cpp`.

**Interfaces:**
- Adds a durable `voiceSourceMode` to `audioMixConfiguration`: `segment-clips`, `manual-program`, or empty.
- Adds `DubbingController::invalidateDerivedDubbingOutputs()` that clears only stale references, never deletes user media.

- [ ] **Step 1: Write failing tests.**

  Add `manualTranslationUploadInvalidatesGeneratedAudioAndExport()` with a segment containing `clipPath`, timing metadata, preview/export paths, and step outputs; after translation import it expects clip/timing/output references cleared. Add `manualVoiceBedTakesPrecedenceOverOldSegmentClips()` with an old clip and a different uploaded full-program WAV; mix output must contain the manual program signal, not the old clip.

- [ ] **Step 2: Run the two tests to RED.**

  Expected: old `clipPath` is still present after translation upload, and `DubbingJobRunner::renderPreview()` chooses any old segment clip before the uploaded voice bed.

- [ ] **Step 3: Implement a single invalidation boundary.**

  When source/target dialogue changes or a translated artifact is applied, clear each segment’s `clipPath`, `clipArtifact`, waveform/duration/fit/timing fields, set state to pending, clear runner preview/dubbed/export paths, and remove step outputs for synthesize, fit-timing, mix and export. Set `voiceSourceMode` empty. Preserve transcript evidence and external files.

- [ ] **Step 4: Make manual voice upload explicit.**

  A synthesize/fit-timing upload sets runner voice path and `voiceSourceMode = "manual-program"`. A successful TTS generation sets `voiceSourceMode = "segment-clips"`. `renderPreview()` must select the manual full-program path whenever the configuration says manual, even if stale segment paths exist; if that file is missing, fail clearly rather than falling back.

- [ ] **Step 5: Run focused GREEN tests and update RC-02/05.**

### Task 3: Skip semantics are identical in readiness and mixer (RC-07)

**Files:**
- Modify: `src/dubbing/audio/AudioTimelineMixer.cpp`.
- Modify: `tests/dubbing/test_DubbingProject.h`, `tests/dubbing/test_DubbingProject.cpp`.

**Interfaces:**
- `AudioTimelineMixer::mixSegments()` ignores a row only when `skipped == true`; a missing clip on a non-skipped row remains a validation failure at workflow readiness.

- [ ] **Step 1: Write `audioMixDoesNotRenderSkippedCueWithRetainedClip()` as a failing test.**

  Create two 1-second WAV clips at adjacent positions. Mark the second map `skipped: true` while retaining its `clipPath`. Render and assert energy is present in the first second and near zero in the second.

- [ ] **Step 2: Run to RED, then add the one-line clip selection guard before `TimedClip` construction.**

  ```cpp
  if (segment.value(QStringLiteral("skipped")).toBool()) continue;
  ```

- [ ] **Step 3: Run the new test and `workflowRequiresEveryNonSkippedCueForMix()` to GREEN; update RC-07.**

### Task 4: Project-scoped preview paths and bounded FFmpeg graph arguments (RC-09, RC-10)

**Files:**
- Modify: `src/controllers/dubbing/DubbingExportJob.cpp`.
- Modify: `src/dubbing/audio/AudioTimelineMixer.cpp`.
- Modify: `tests/dubbing/test_DubbingProject.h`, `tests/dubbing/test_DubbingProject.cpp`.

**Interfaces:**
- `DubbingExportJob::renderPreview(..., path = {})` derives `<project-dir>/<project-base>.preview.wav` instead of shared `preview.wav`.
- `AudioTimelineMixer` writes a unique temporary `*.filtergraph` next to its staging output and invokes FFmpeg with `-filter_complex_script`.

- [ ] **Step 1: Write `previewPathsAreDistinctForTwoProjectsInOneFolder()` to RED.**

  Start two render requests for `a.ladub.json` and `b.ladub.json` in one temporary directory without explicit paths; assert the emitted preview paths differ and each path is under its own project base name.

- [ ] **Step 2: Write `audioMixHandlesTwoHundredCuesWithoutLongCommandLine()` to RED.**

  Use a real tiny WAV reference in 200 timed rows, run the actual FFmpeg route, and assert output is readable. The test must expose a generated filter script or output success; it must not merely inspect source text.

- [ ] **Step 3: Derive project-scoped output and script the graph.**

  Use `QFileInfo(projectPath).dir().filePath(projectBase + ".preview.wav")`. For each clip, emit an escaped `amovie=filename='…'` source and the existing resample/trim/delay/volume graph to a `QSaveFile`; call FFmpeg with only `-filter_complex_script <path>`, map `[voices]`, and remove the script after the process returns. Preserve `-nostdin`, explicit mono map, cancellation and timeout.

- [ ] **Step 4: Run both tests to GREEN plus existing async mix/export tests; update RC-09/10.**

### Task 5: Explicit BGM/original-vocals/dubbed semantics (RC-12)

**Files:**
- Modify: `src/dubbing/project/DubbingProject.h`, `src/dubbing/project/DubbingProject.cpp`.
- Modify: `src/controllers/dubbing/parts/DubbingController_Project.cpp`, `src/controllers/dubbing/parts/DubbingController_Stages.cpp`, `src/controllers/dubbing/parts/DubbingController_Artifacts.cpp`.
- Modify: `src/controllers/dubbing/DubbingJobRunner.*`, `src/controllers/dubbing/DubbingExportJob.*`, `src/dubbing/audio/AudioTimelineMixer.*`.
- Modify: `qml/components/dubbing/steps/DubbingAlignmentStep.qml`, `tools/qml-preview/LAStudio/AppController.qml`.
- Modify: `tests/dubbing/test_DubbingProject.h`, `tests/dubbing/test_DubbingProject.cpp`.

**Interfaces:**
- Persist `backgroundGainPercent` (default 100), `originalGainPercent` (source vocals, default 0), `dubbedGainPercent` (default 100).
- Pass source vocals as a distinct mixer input; BGM ducking is governed only by `backgroundGainPercent`.

- [ ] **Step 1: Write `audioMixSeparatesBackgroundFromOriginalVocals()` to RED.**

  Supply three constant signals: background `0.10`, original vocal `0.20`, dubbed `0.30`. With `{background:100, original:0, dubbed:100}` assert the original-vocal component is absent; with `{background:0, original:100, dubbed:0}` assert only original-vocal output remains.

- [ ] **Step 2: Add the third input and configuration migration.**

  Older projects receive `backgroundGainPercent: 100`. Separation supplies BGM and vocals separately; when no separate BGM exists, master audio becomes the explicit original layer. FFmpeg and fallback both combine `[duckedBgm] + [originalVocals] + [dubbed]` with all three gain values. Never reinterpret `originalGainPercent` as BGM volume.

- [ ] **Step 3: Rename QML labels and add BGM slider without changing persisted old keys.**

  Use clear labels “Original vocals”, “Dubbed voice”, “Background music”; bind all three to `setAudioMixLevels`/configuration and show values with `Accessible.name`.

- [ ] **Step 4: Run JSON migration, audio mixer and QML smoke tests to GREEN; update RC-12.**

### Task 6: Independent STT/OCR runtime proof (RC-04)

**Files:**
- Modify: `tests/dubbing/test_DubbingProject.h`, `tests/dubbing/test_DubbingProject.cpp`.
- Modify only if test proves a route coupling: `src/controllers/dubbing/parts/DubbingController_Stages.cpp`, `src/controllers/dubbing/parts/DubbingController_Workflow.cpp`, relevant QML Transcribe card.

**Interfaces:**
- STT and OCR each expose separate in-progress state and accepting one request does not disable/cancel the other route.

- [ ] **Step 1: Write `sttAndOcrRunConcurrentlyWithoutDisablingEitherCard()` using the local test server/runtime.**

  Start STT, wait for its running state, start OCR before STT completes, assert both progress states appear, assert neither `Run STT` nor `Run OCR` binding is disabled solely because its sibling is active, then finish both and verify both durable source lists are recorded.

- [ ] **Step 2: Run to RED or document that it already passes.**

  If red, trace the exact `processing()` caller that rejects the sibling; change only the route-scoped predicate. If green, retain the test as missing dynamic evidence rather than changing production code.

- [ ] **Step 3: Run the whole transcript block to GREEN and update RC-04.**

### Task 7: Project/run-scoped AI transcript handoff with an exposed UI caller (RC-06)

**Files:**
- Modify: `src/controllers/dubbing/parts/DubbingController_Artifacts.cpp`, `src/controllers/dubbing/DubbingController.h`.
- Modify: `qml/components/dubbing/DubbingNodeInspector.qml` or the existing Data & Handoff panel that owns transcript actions.
- Modify: `docs/AI_AGENT_TRANSCRIPT_RECONCILIATION_GUIDE.md`.
- Modify: `tests/dubbing/test_DubbingProject.h`, `tests/dubbing/test_DubbingProject.cpp`, `tests/dubbing/test_DubbingWorkspaceContract.cpp`.

**Interfaces:**
- `prepareTranscriptAiHandoff()` returns a `handoffDirectory`, `prompt`, `sttInput`, `ocrInput`, `canonicalInput`, `mergedOutput`, and `translationOutput` under `.workflow-artifacts/ai-handoff/<UTC>-<uuid>/`.
- The guide contains only combine/translate rules. The controller-generated short prompt contains the exact paths for the current project/run.

- [ ] **Step 1: Write a failing handoff test.**

  Call `prepareTranscriptAiHandoff()` twice for two projects with equal display names. Assert every returned path is within the corresponding project root, no path overlaps, the canonical cue count/timeline is retained, and the prompt lists the exact input/output paths.

- [ ] **Step 2: Create snapshot files atomically.**

  Use a unique `handoffId`, write `01-stt-input.srt`, `02-ocr-input.srt`, `03-canonical-timeline.srt`, reserve `04-merged-transcript.srt` and `05-translation-vi.srt`, and return a concise user-facing prompt. Do not use fixed stage-global names.

- [ ] **Step 3: Resolve the timeline policy contradiction in the guide.**

  OCR is the canonical cue/timestamp grid when present; STT supplies audio-only reference text inside that grid. When OCR is absent, STT is canonical. The IDE must preserve cue count/times, write non-empty meaningful text, preserve roles/context, transliterate Chinese Latin names to natural Vietnamese when context supports it, and write both output files directly without changing any code or project folder.

- [ ] **Step 4: Add an accessible Handoff button with Copy prompt/Open folder actions, then run controller/QML contract tests to GREEN; update RC-06.**

### Task 8: Bounded remote-worker teardown (RC-11)

**Files:**
- Modify: `src/controllers/dubbing/DubbingTranslationJob.*`, `src/controllers/dubbing/DubbingSynthesisJob.*`.
- Modify: the concrete `Gateway*Runner`/`Colab*Runner` only if their `cancel()` does not abort the owning reply/process.
- Modify: `tests/dubbing/test_DubbingProject.h`, `tests/dubbing/test_DubbingProject.cpp`.

**Interfaces:**
- Both jobs expose an internal `stopRemoteThread()` that requests cancellation, aborts in-thread runner work via a blocking queued cancellation only when its event loop is responsive, quits, and verifies a finite join deadline before destructor shutdown continues.

- [ ] **Step 1: Write `remoteDubbingJobsCancelAndStopWithinDeadline()` to RED.**

  Start a test server request which deliberately withholds response, cancel/destroy translation and synthesis jobs, and assert destruction returns before the bounded test deadline while the fake connection observes cancellation.

- [ ] **Step 2: Trace runner cancellation before modifying thread ownership.**

  Confirm each runner aborts its active `QNetworkReply`/process and emits a terminal signal. Add an interruption-aware deadline to the request itself if it does not. Do not detach or `terminate()` a thread.

- [ ] **Step 3: Implement bounded graceful stop and run focused lifecycle tests to GREEN; update RC-11.**

### Task 9: Final recheck, evidence, package and source delivery

**Files:**
- Modify: `PRE_DELIVERY_CHECKLIST.md`, `docs/AI_AGENT_RESPONSE_REPORT.md`, `docs/AI_AGENT_REPORT_SUMMARY.md`, `docs/PROJECT_MEMORY.md`.
- Update: `graphify-out/` through `graphify update .` only after code and docs are final.

- [ ] **Step 1: Run full test/build/static gates.**

  Run `cmake --build out/build/windows-msvc-release --config Release --parallel`, `ctest --test-dir out/build/windows-msvc-release --output-on-failure -j 4`, `qmllint` for changed production QML, `git diff --check`, and the existing `scripts/prebuild_gate.ps1`.

- [ ] **Step 2: Execute production QML smoke and visual evidence at 1280×720, 1600×900 and 4K.**

  Verify upload busy/failed/success states, STT/OCR parallel controls, handoff actions, all three mix levels, and dialogs have no clipping/collision. Record screenshots/logs under the versioned output evidence folder.

- [ ] **Step 3: Update report/checklist per verified RC finding.**

  Check an RC only after its targeted regression and full gate are fresh. Mark remaining external live-Colab/CapCut evidence as manual rather than passing by inference.

- [ ] **Step 4: Build, package, graphify, commit and push verified source.**

  Determine the next patch version from current `0.0.9.1`, package only the fresh executable with a new source manifest, run `graphify update .`, commit intentional source/tests/docs/generated graph metadata, verify `git status`, and push the commit to `origin/main`.

## Self-review

- RC-01/03/08 are covered by Task 1, including no-cancel-before-parse and no GUI copy/probe.
- RC-02/05 by Task 2, RC-07 by Task 3, RC-09/10 by Task 4, RC-12 by Task 5.
- RC-04 dynamic concurrency is Task 6; RC-06 handoff UX/data scope is Task 7; RC-11 lifecycle is Task 8.
- Task 9 rechecks all product-facing paths and does not treat a static QML/test source contract as live worker proof.
