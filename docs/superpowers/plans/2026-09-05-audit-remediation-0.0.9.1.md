# LA Studio 0.0.9.1 Audit Remediation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Resolve every actionable finding F-01 through F-25 in the 2026-09-04 audit and produce a reproducible portable 0.0.9.1 candidate.

**Architecture:** Preserve the existing Qt/QML route and project model. Move blocking work behind bounded worker/process boundaries, make workflow readiness data-complete rather than existence-based, and make test/packaging state hermetic. UI fixes keep the persistent right inspector requested by the product while making it responsive and non-blocking.

**Tech Stack:** C++17, Qt 6/QML, QProcess/QtConcurrent, CTest, FFmpeg/FFprobe, PowerShell packaging, Windows UI Automation.

**Spec:** `docs/LA_STUDIO_INDEPENDENT_FULL_AUDIT_2026-09-04.md`

## Global Constraints

- Direct Colab, API Gateway, Local CPU and manual upload remain explicit, separate routes.
- STT and OCR remain independently runnable and uploadable.
- Credentials never enter project JSON, source snapshots, test artifacts or logs.
- All process/thread joins use an explicit deadline and cancellation path.
- Each changed behaviour has a focused regression before the production change.
- Package only after all executable checks pass; candidate output is `out/LA-Studio-0.0.9.1/`.

---

### Task 1: Release identity and hermetic smoke data (F-02, F-23)

**Files:**
- Modify: `src/main.cpp`, `src/controllers/dubbing/parts/DubbingController_Project.cpp`, `scripts/package.ps1`, version/build metadata.
- Test: `tests/dubbing/test_DubbingProject.cpp`, package smoke assertions.

- [ ] Add a failing smoke-mode test that saves a project and asserts the real AppData history hash is unchanged.
- [ ] Redirect QStandardPaths/test history through the smoke data root and ensure fixture cleanup is scoped to that root.
- [ ] Write a release source manifest containing base commit, dirty/source snapshot hash, version and artifact hash; reject mismatched version fields.
- [ ] Run focused project/smoke tests, then package smoke.

### Task 2: Runtime lifecycle and non-blocking shutdown (F-01, F-09, F-17)

**Files:**
- Modify: `src/runtimehost/service/RuntimeHostClient.*`, `src/controllers/dubbing/DubbingTranscriptionJob.cpp`, `src/controllers/dubbing/DubbingExportJob.cpp`, `src/controllers/dubbing/DubbingJobRunner.cpp`.
- Test: `tests/core/test_RuntimeHostProtocol.cpp`, `tests/dubbing/test_DubbingProject.cpp`.

- [ ] Add failing restart and unresponsive-worker shutdown cases with finite expected deadlines.
- [ ] Separate host handshake deadline from request inactivity; clear stale socket/process state before retry and log phase/exit diagnostics without secrets.
- [ ] Replace all unbounded job/thread waits with cancellation plus bounded join and non-blocking completion reporting.
- [ ] Move duration probe/copy/export staging to a cancellable worker or bounded process; keep GUI thread event loop free.
- [ ] Run Runtime Host stress 100 times and focused job tests.

### Task 3: Audio correctness, coverage and memory limits (F-03–F-06, F-16)

**Files:**
- Modify: `src/dubbing/audio/AudioTimelineMixer.*`, `src/audio/AudioTimelineRenderer.*`, artifact/export helpers and CapCut exporter.
- Test: `tests/dubbing/test_DubbingProject.cpp`, audio format fixtures.

- [ ] Add failing tests for missing first/middle/last required clips, background longer than speech, and 16/22.05/24/44.1/48 kHz clips.
- [ ] Compute required cue coverage, expose `ready/required`, and prevent mix/export until every non-skipped cue is present.
- [ ] Size output from explicit source/background duration policy rather than last voice cue.
- [ ] Use output-frame interpolation/resampling and verify continuity/RMS/duration.
- [ ] Replace whole-timeline mix allocation with bounded block mixing and streaming WAV output; decode/convert in bounded blocks where supported.
- [ ] Preserve source container extension or explicitly transcode before naming `.wav`; probe staged output.
- [ ] Run audio matrix and large-duration memory regression.

### Task 4: Workflow, persistence and artifact handoff integrity (F-10–F-15, F-19–F-20)

**Files:**
- Modify: `DubbingController_Workflow.cpp`, `DubbingProject.*`, `DubbingController_Project.cpp`, `StudioSelectionRepository.*`, handoff/QML controller API.
- Test: `tests/dubbing/test_DubbingProject.cpp`, storage tests, workspace contract.

- [ ] Add failing tests for partial translation, preview-only export, STT/OCR conflict policy, atomic database rollback, reopen, relocation/relink and two same-name projects.
- [ ] Use `partial` for incomplete translated cues; use a verified final export path for export completion.
- [ ] Make OCR-preferred fusion/timeline policy the single project default and migration target.
- [ ] Make model selection writes transactional and return usable errors to callers.
- [ ] Persist/reopen last valid project; store in-project assets relatively and rebase them on project move, with relink state for external sources.
- [ ] Generate a project-scoped transcript/translation handoff with resolved input/output paths.
- [ ] Run project save/reopen/move/handoff test matrix.

### Task 5: Responsive, low-idle-cost Dubbing UI (F-07–F-08, F-21–F-22, F-24–F-25)

**Files:**
- Modify: `qml/Main.qml`, `qml/components/Sidebar.qml`, `qml/components/dubbing/DubbingSourceMediaPanel.qml`, `qml/pages/DubbingPage.qml`, relevant Dubbing components.
- Test: QML route smoke, workspace contracts, Windows UIA smoke.

- [ ] Add failing QML/UIA cases for seek while ROI edit is enabled, subtitle lookup with dense cue data, compact task rail, and single-modal first run.
- [ ] Keep seek pointer handling outside ROI overlay; use indexed/binary subtitle lookup.
- [ ] Stop perpetual Community animations after a finite attention pulse and when window is inactive; preserve hover feedback.
- [ ] Unload Dubbing UI view when leaving it while retaining controller/project state.
- [ ] Keep right inspector but compact it for empty states; guarantee current/next workflow stages are discoverable at 1280×720, 1600×900 and 4K.
- [ ] Queue update consent after onboarding instead of showing concurrent blocking dialogs.
- [ ] Run QML lint and visual/UIA smoke at three resolutions.

### Task 6: Acceptance and release gates (F-14, F-18)

**Files:**
- Modify: `tests/CMakeLists.txt`, UIA test harness/scripts, `scripts/prebuild_gate.ps1`, `PRE_DELIVERY_CHECKLIST.md`, package script.

- [ ] Add a signed/manual live acceptance lane that is opt-in, credential-safe and emits video/log/artifact evidence without making secrets available to CTest.
- [ ] Add packaged Windows UIA coverage for all eight tasks, Model/Colab/Upload/Run/Skip/Continue, history/menu overlays, voice gallery and ROI seek.
- [ ] Add UX budgets for startup, idle CPU, memory enter/leave, click feedback, playback and cancellation.
- [ ] Run full CTest, lint, notebook contracts, prebuild gate, packaged smoke and package inventory/hash checks.

### Task 7: Delivery documentation and 0.0.9.1 portable candidate

**Files:**
- Modify: `docs/AI_AGENT_REPORT_SUMMARY.md`, `docs/PROJECT_MEMORY.md`, `docs/AI_AGENT_RESPONSE_REPORT.md`, audit report/checklist.
- Create: `out/LA-Studio-0.0.9.1/` package manifest and source snapshot metadata.

- [ ] Update the audit with each finding’s implemented or explicitly external-only status and exact evidence.
- [ ] Commit only active-task files, push source and documentation when all checks are green.
- [x] Build and verify FileVersion/ProductVersion, package layout, checksums and packaged QML smoke for 0.0.9.1.

## Execution record — 2026-09-05

Source remediation is complete for the deterministic findings. The release
candidate still has an explicit manual boundary: a real credentialed Colab
run, CapCut import/open, and long interactive playback/ROI drag are not
substituted by the automated gate.

| Area | Evidence recorded before packaging |
|---|---|
| F-01 runtime startup | `TestRuntimeHostProtocol` now repeats an idempotent Hello handshake; five consecutive test batches completed **100** process/socket/auth restarts without a timeout. |
| F-02/F-23 release isolation | `package.ps1` rejects version drift and writes a release source manifest with base commit, dirty-diff hash and staged executable hash; smoke data is isolated from user history. |
| F-03–F-06/F-16 audio | Mixer regression covers complete cue coverage, source-duration preservation and 16/22.05/24/44.1/48 kHz inputs. The FFmpeg graph now splits the voice sidechain branch before mixing. |
| F-07–F-08/F-21–F-25 QML/UX | ROI/control pointer contracts, paused-frame behaviour, subtitle lookup, idle animation/view lifecycle and compact workflow surfaces are covered by workspace/QML route contracts. |
| F-09–F-20 workflow | Bounded job/process handling, data-complete workflow state, OCR-priority fusion, transactional selections, project reopen/relocation and project-scoped handoff are covered by controller/project tests. |
| Global gate | Fresh full CTest **41/41 PASS**, QML lint **PASS**, `git diff --check` **PASS**, `graphify update .` **PASS**, and `out/prebuild-gate/latest.json` reports **10/10 PASS**. |

The portable package step completed through `scripts/package.ps1` for the
actual 0.0.9.1 executable.  The staged artifact contains the versioned EXE,
runtime inventory, packaged QML interaction trace and a source-manifest with
the commit/dirty-source/artifact hashes.  No source files are changed after
that source-manifest is created; the final delivery check reads the manifest
back and compares its executable hash with the staged file.
