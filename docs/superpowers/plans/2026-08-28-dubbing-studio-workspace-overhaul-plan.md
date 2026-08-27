# Dubbing Studio Workspace Overhaul Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Refactor the production Dubbing workspace into a content-first, drawer-driven UI with truthful model/voice counts, direct Colab/upload recovery, safe media preview and verified portable packaging.

**Architecture:** Keep `DubbingController` and its workflow graph as the only workflow authority. Make `DubbingPage` compose a compact left task shelf, a central media/timeline workspace and an on-demand right context drawer; existing review/inspector components remain the drawer content instead of being duplicated. Use catalog metadata and existing controller signals for model/voice truth, and add only narrow QML/C++ contracts where the current public state cannot represent the requested behavior.

**Tech Stack:** Qt 6.9 QML, Qt Quick Controls/Layout, Qt Multimedia, C++20/MSVC, CMake/Ninja, QtTest, PowerShell packaging, graphify.

**Spec:** `docs/superpowers/specs/2026-08-28-dubbing-studio-workspace-overhaul-design.md`

## Global Constraints

- Preserve source version `0.0.8.5` and package with the portable root layout used by 8.4.
- Validate 1280×720, 1600×900, 1920×1080 and 4K desktop viewports.
- Keep the preview viewport 16:9 and render all source ratios with `VideoOutput.PreserveAspectFit`.
- Keep OCR scan editing visible only for the Transcribe/OCR task.
- Do not hard-code voice counts; filtered and headline counts must derive from the same catalog list.
- Do not use a normalized-source fallback for a missing required separation stem.
- Keep long-running model/media work outside the UI thread and preserve technical logs while adding user-facing recovery guidance.
- Preserve unrelated dirty worktree changes; stage only files belonging to this implementation.

---

### Task 1: Establish a reproducible UI/workflow contract baseline

**Files:**
- Modify: `tests/CMakeLists.txt`
- Modify: `tests/main.cpp`
- Create: `tests/dubbing/test_DubbingWorkspaceContract.h`
- Create: `tests/dubbing/test_DubbingWorkspaceContract.cpp`
- Inspect without modifying: `qml/pages/DubbingPage.qml`, `qml/components/dubbing/panels/DubbingTaskShelf.qml`, `qml/components/shared/WorkflowNodeModelDialog.qml`

**Interfaces:**
- Consumes: `DubbingController::workflowNodes()`, `DubbingController::workflowStages()`, `DubbingController::ttsVoiceOptions()`, `DubbingController::selectWorkflowColabModel()`, `DubbingController::runWorkflowNode()`.
- Produces: deterministic QtTest coverage for workflow node gating, catalog-derived counts, compatible remote voice metadata and required artifact routing.

- [ ] **Step 1: Map current contracts before edits.** Record the existing workflow node ids (`import`, `normalize`, `source-separate`, `transcribe`, `translate`, `synthesize`, `mix`, `export`), the current `VoiceClonePresetService` fields and the two existing model-dialog callbacks in the plan review notes.

- [ ] **Step 2: Add the test suite declaration.** Add `dubbing/test_DubbingWorkspaceContract.cpp` and its header to `TEST_SOURCES`, register the suite name `DubbingWorkspaceContract` in the existing test-suite dispatch, and keep `QT_QPA_PLATFORM=offscreen` for the suite.

- [ ] **Step 3: Write the failing contract tests.** The test fixture must assert these exact behaviors:

```cpp
void DubbingWorkspaceContractTest::voiceCountsUseOneCatalog()
{
    const QVariantList voices = m_controller->ttsVoiceOptions();
    QCOMPARE(m_controller->ttsVoiceOptions().size(), voices.size());
    QVERIFY(std::none_of(voices.cbegin(), voices.cend(), [](const QVariant &value) {
        return value.toMap().value(QStringLiteral("id")).toString().isEmpty();
    }));
}

void DubbingWorkspaceContractTest::remoteSelectionKeepsTaskContext()
{
    QVERIFY(m_controller->selectWorkflowColabModel(
        QStringLiteral("synthesize"), QStringLiteral("omnivoice")));
    QCOMPARE(m_controller->workflowNodeConfigurations()
                 .value(QStringLiteral("synthesize")).toMap()
                 .value(QStringLiteral("provider")).toString(),
             QStringLiteral("colab-direct"));
}

void DubbingWorkspaceContractTest::missingStemIsNotSilentFallback()
{
    QVERIFY(!m_controller->workflowRecovery().value(QStringLiteral("fallbackUsed")).toBool());
}
```

- [ ] **Step 4: Run the focused suite and confirm the baseline result.** Run `ctest --test-dir out/build/windows-msvc-release -C Release -R DubbingWorkspaceContract --output-on-failure`. Keep any failing assertion as the implementation target; do not weaken the test to fit the current behavior.

### Task 2: Make the top workflow rail compact and bilingual-on-hover

**Files:**
- Modify: `qml/pages/DubbingPage.qml` (`stepTitle()`, `headerWorkflowSteps()` and header bindings)
- Modify: `qml/components/dubbing/DubbingWorkflowHeader.qml`
- Modify: `qml/components/dubbing/DubbingWorkflowStep.qml`
- Modify: `qml/Main.qml` QML smoke assertions for the Dubbing route

**Interfaces:**
- Consumes: `DubbingController::workflowStages()` with its stage id, label, icon and status fields.
- Produces: each `DubbingWorkflowStep` receives `shortTitle` and `detailTitle`; `shortTitle` is rendered and `detailTitle` is used by `ToolTip` and accessibility text.

- [ ] **Step 1: Add explicit short/detail labels in the page mapping.** Use the existing stage ids to return `Import`, `Normalize`, `Separate`, `Transcribe`, `Align`, `Translate`, `Synthesize` and `Mix & Export`; keep the existing Vietnamese text as the detail label.

- [ ] **Step 2: Render only the short label in the resting state.** Update `DubbingWorkflowStep.qml` so `Text.text` binds to `shortTitle`, while `ToolTip.text` is `shortTitle + " (" + detailTitle + ")"`. Preserve the existing active/completed colors and 34 px rail height.

- [ ] **Step 3: Keep the rail scrollable without shrinking action controls.** Retain `workflowStepsFlickable` as the sole flexible header region and verify every action button remains within `headerActionCluster` bounds.

- [ ] **Step 4: Add a smoke assertion for labels.** Assert that the first eight rendered step titles contain no Vietnamese parenthetical text and that hovering/focus metadata contains the detail text.

- [ ] **Step 5: Run `ctest --test-dir out/build/windows-msvc-release -C Release -R QmlRouteSmoke --output-on-failure`.** The test must still reach the Dubbing route and complete its existing route trace.

### Task 3: Replace the permanent right panel with a left-triggered context drawer

**Files:**
- Create: `qml/components/dubbing/panels/DubbingContextDrawer.qml`
- Modify: `qml/components/dubbing/panels/DubbingTaskShelf.qml`
- Modify: `qml/pages/DubbingPage.qml`
- Modify: `qml/components/dubbing/DubbingWorkflowHeader.qml`
- Modify: `qml/components/dubbing/panels/DubbingReviewPanel.qml` only where drawer sizing/scrolling requires it
- Modify: `qml/components/dubbing/DubbingNodeInspector.qml` only where drawer sizing/scrolling requires it

**Interfaces:**
- Consumes: the current `DubbingReviewPanel` signals and `DubbingNodeInspector` node binding.
- Produces: `DubbingTaskShelf::contextRequested(string contextId)`, `DubbingContextDrawer::contextId`, `opened`, `closed`, and the existing review/inspector signals forwarded unchanged.

- [ ] **Step 1: Write the drawer contract test first.** Extend `qmlSmokeExerciseRoute()` with `dubbingContextDrawer` open/close actions and assert that the central preview remains visible, the drawer is clipped and the drawer content is scrollable.

- [ ] **Step 2: Create the drawer shell.** Implement a Qt Quick `Drawer` with `edge: Qt.RightEdge`, width bounded to `Math.min(520, Math.max(320, parent.width * 0.30))`, `height: parent.height`, `clip: true`, and a `ScrollView` around variable-height context content. Use `Theme` tokens for surface, scrim, border and spacing.

- [ ] **Step 3: Convert the task shelf into compact feature actions.** Replace its always-rendered `DubbingNodeSettingsPanel` body with buttons for `Results`, `Settings`, `Model`, `Colab`, `Upload` and `Handoff`. Keep the current step title/status as a compact summary above the buttons.

- [ ] **Step 4: Wire the contexts in `DubbingPage.qml`.** Remove the direct right-pane `DubbingReviewPanel` and `DubbingNodeInspector` layout children. Host them inside `DubbingContextDrawer`, map `results` to `DubbingReviewPanel`, `settings` to `DubbingNodeInspector`, and keep the existing callback wiring to `nodeModelDialog`, `translationFixDialog`, `dubbingArtifactUploadDialog`, `transcriptEditor`, `subtitleEditor`, `exportOptionsDialog` and playback handlers.

- [ ] **Step 5: Remove the duplicate header Colab/Workflow actions.** Keep one left-shelf Colab button and retain only actions that are global to the editor in the header. The old `Manage Colab route` path must not be rendered in the Dubbing shelf.

- [ ] **Step 6: Verify layout at 1280×720.** Run the production QML smoke at 1280×720 and inspect that the drawer overlays or boundedly reserves space without squeezing the preview below its minimum width.

### Task 4: Make Run, Open model and Upload direct recovery actions

**Files:**
- Modify: `qml/pages/DubbingPage.qml` (`runStep()`, model/Colab callbacks and drawer actions)
- Modify: `qml/components/dubbing/panels/DubbingTaskShelf.qml`
- Modify: `qml/components/dubbing/DubbingArtifactUploadPanel.qml`
- Modify: `qml/components/shared/WorkflowNodeModelDialog.qml`
- Modify: `qml/components/dubbing/DubbingColabSetupDialog.qml` only for contextual stage handoff
- Modify: `src/controllers/dubbing/DubbingController.h` and the matching `parts/DubbingController_Workflow.cpp` only if an existing public contract cannot return the task-specific recovery state
- Modify: `tests/dubbing/test_DubbingWorkspaceContract.cpp`

**Interfaces:**
- Consumes: `workflowNodes`, `workflowNodeConfigurations`, `colabModelOptionsForNode()`, `defaultColabModelForNode()`, `selectWorkflowColabModel()`, `runWorkflowNode()`, and the existing artifact upload callbacks.
- Produces: one contextual gate function in `DubbingPage.qml` that either runs the node or opens the model dialog with the correct `nodeId` and `capabilityId`.

- [ ] **Step 1: Add a failing QML gate contract.** For an unconfigured `synthesize` node, invoke the shelf Run action and assert `WorkflowNodeModelDialog.nodeId === "synthesize"`; assert no terminal error dialog is opened before the model dialog.

- [ ] **Step 2: Implement `ensureNodeExecutionRoute(nodeId)`.** Check the node configuration and ready state. If a compatible local/remote route exists, call `dubbing.runWorkflowNode(nodeId)`. Otherwise call `nodeModelDialog.openFor(nodeId)` and preserve the node id through the dialog result.

- [ ] **Step 3: Make `Use selected model on Colab` transactional.** Keep `WorkflowNodeModelDialog.applySelectedColabConfiguration()` as the only commit path, then open `DubbingColabSetupDialog` with the selected stage id. Do not open a generic Colab dialog with an empty context after model selection.

- [ ] **Step 4: Fix upload reachability.** Ensure each upload button calls `DubbingArtifactUploadPanel.openFor(nodeId)`, accepted files are copied through the existing controller contract, and rejection keeps the dialog visible with the accepted artifact names/extensions.

- [ ] **Step 5: Add missing-stem recovery coverage.** Assert that a source-separation run with only one stem produces a recovery action for model selection/upload and never sets a normalized-source substitute path.

- [ ] **Step 6: Run the focused C++ and QML tests.** Use `ctest --test-dir out/build/windows-msvc-release -C Release -R "DubbingWorkspaceContract|TestSourceSeparation|QmlRouteSmoke" --output-on-failure`.

### Task 5: Simplify media preview, add thumbnail state and enforce 16:9 containment

**Files:**
- Modify: `qml/components/dubbing/DubbingSourceMediaPanel.qml`
- Modify: `qml/pages/DubbingPage.qml` only for source-selection callback preservation
- Modify: `tests/CMakeLists.txt` and QML smoke fixture data if a new portrait/square fixture is required
- Modify: `tests/dubbing/test_DubbingWorkspaceContract.cpp` for preview-state contract where controller state is involved

**Interfaces:**
- Consumes: `DubbingController::sourceMediaPath`, `sourceMediaUrl`, `playbackMediaUrl`, `dubbedVocalPath`, `backgroundPath`, and the existing `MediaPlayer`/`VideoOutput` objects.
- Produces: a fixed 16:9 `previewSurface`/`previewFrame`, visible first-frame loading poster state, and only `Fit source`, `Original`, `Dubbed` in the persistent preview toolbar.

- [ ] **Step 1: Add the failing preview assertions.** Assert toolbar child visibility contains only the three required actions, `previewFrame.width / previewFrame.height` is 16:9, and a loaded media source exposes a non-empty thumbnail/loading state before playback.

- [ ] **Step 2: Remove persistent source-management controls.** Move source browser/download/replacement actions out of the loaded preview toolbar. Keep source acquisition reachable from the left Upload/source action and the existing pre-selection panel.

- [ ] **Step 3: Implement first-frame thumbnail behavior.** Keep the `MediaPlayer` paused after `LoadedMedia`/`BufferedMedia`, show its first rendered `VideoOutput` frame as the poster, and overlay a neutral loading poster until media status is loaded. Use an asynchronous `Image` fallback only for an explicitly available poster path; do not block the UI on FFmpeg.

- [ ] **Step 4: Lock the viewport geometry.** Set the outer preview surface to the fixed 16:9 ratio and use `VideoOutput.PreserveAspectFit` inside it. Render portrait and square sources with letterbox bars and no layout reflow.

- [ ] **Step 5: Keep paths collision-safe.** Apply `Text.ElideMiddle` and a full-path `ToolTip` to normalized/stem paths. Ensure play buttons remain in a separate bounded column.

- [ ] **Step 6: Run the media and route tests.** Run `ctest --test-dir out/build/windows-msvc-release -C Release -R "TestMediaIngestService|TestMediaToolService|QmlRouteSmoke" --output-on-failure` and capture the preview at 1280×720.

### Task 6: Restrict OCR controls and make subtitle editing default-first

**Files:**
- Modify: `qml/components/dubbing/DubbingSourceMediaPanel.qml`
- Modify: `qml/components/dubbing/steps/DubbingTranscribeStep.qml`
- Modify: `qml/components/dubbing/panels/DubbingReviewPanel.qml`
- Modify: `qml/pages/DubbingPage.qml` subtitle/OCR signal routing
- Modify: `tests/dubbing/test_DubbingWorkspaceContract.cpp`

**Interfaces:**
- Consumes: `DubbingController::dubbingOcrRoiVisible`, `dubbingOcrRoi`, `subtitleConfiguration`, `segments`, `subtitleOcrCanRunAlongsideStt`, and the existing `subtitleSegmentEditRequested` signal.
- Produces: OCR scan controls visible only on the Transcribe/OCR stage; a default lower subtitle overlay; click-to-edit routing through `subtitleEditor`.

- [ ] **Step 1: Add visibility assertions.** Assert `dubbingSubtitleOcrRoiOverlay.visible` is false on Synthesize/Mix and true only when the displayed node is Transcribe/OCR and the controller allows OCR editing.

- [ ] **Step 2: Gate the ROI editor by task context.** Pass a `showOcrTools` property from `DubbingPage` to `DubbingSourceMediaPanel`; combine it with the controller ROI flag before rendering handles/labels.

- [ ] **Step 3: Set a safe default subtitle position.** When no custom subtitle position exists, use the lower safe region above media controls. If OCR is active, resolve the overlay position from the accepted OCR ROI.

- [ ] **Step 4: Preserve direct editing.** Clicking the subtitle overlay must call `subtitleSegmentEditRequested(activeSubtitleIndex)` and open the existing inline/segment editor without forcing a full OCR setup flow.

- [ ] **Step 5: Run `ctest --test-dir out/build/windows-msvc-release -C Release -R "TestSubtitleOcrController|TestSubtitleOcrPipeline|QmlRouteSmoke" --output-on-failure`.** Confirm no OCR controls appear in the captured TTS view.

### Task 7: Make voice counts and ViNeU/OmniVoice routing truthful

**Files:**
- Modify: `src/controllers/shared/VoiceClonePresetService.h`
- Modify: `src/controllers/shared/VoiceClonePresetService.cpp`
- Modify: `src/controllers/dubbing/DubbingController.h`
- Modify: `src/controllers/dubbing/parts/DubbingController_Workflow.cpp`
- Modify: `data/presets/voice_clone_presets.json`
- Modify: `qml/components/shared/VoiceGalleryDialog.qml`
- Modify: `qml/components/dubbing/steps/DubbingSynthesizeStep.qml`
- Modify: `tests/dubbing/test_DubbingWorkspaceContract.cpp`

**Interfaces:**
- Consumes: the existing preset JSON fields `id`, `modelFamily`, `language`, `referenceAudio`, `referenceText`, `category`, and `tags`.
- Produces: stable voice identity, catalog-derived total/category counts, `compatibleModelFamilies` metadata, and a selected voice that invalidates incompatible runtime configuration.

- [ ] **Step 1: Add failing catalog tests.** Cover exact total/category counts from JSON, stable non-empty ids, ViNeU reference metadata, OmniVoice compatibility when declared, and rejection when audio/language metadata is incompatible.

- [ ] **Step 2: Normalize voice metadata at load time.** Ensure every returned voice map has `id`, `displayName`, `familyId`, `category`, `language`, `audioPath`, `referenceText`, `compatibleModelFamilies` and `isCustomVoice` without changing the serialized project voice id format.

- [ ] **Step 3: Centralize count derivation.** Expose counts from the same `ttsVoiceOptions()` list used by the gallery filters. Remove hard-coded marketing counts and the `53+clone`/`52 Giọng` copy from TTS surfaces.

- [ ] **Step 4: Implement compatibility-aware selection.** When a selected voice declares OmniVoice support, select the `omnivoice` family and use the existing model dialog Colab path. When it does not, keep the current family and show the exact incompatibility reason in the picker.

- [ ] **Step 5: Remove the redundant no-segments banner from the primary TTS surface.** Retain the condition only as an inline recovery action when the user clicks Run without transcript segments, routing to Transcribe/OCR.

- [ ] **Step 6: Run `ctest --test-dir out/build/windows-msvc-release -C Release -R "DubbingWorkspaceContract|TestDubbingProject|TestWorkflowGraph" --output-on-failure`.** Verify counts and voice routing from real catalog data.

### Task 8: Add production QML visual regression evidence

**Files:**
- Modify: `qml/Main.qml` smoke state and route exercise
- Modify: `tests/CMakeLists.txt` only if the smoke target needs a fixture path
- Modify: `scripts/preview_dubbing_ui.ps1` to launch the production route with the agreed viewport
- Create: `docs/superpowers/audits/2026-08-28-dubbing-workspace-overhaul-report.md`

**Interfaces:**
- Consumes: the production `Main.qml` route, existing smoke fixture media and QML object names.
- Produces: a repeatable screenshot/inspection procedure and an indexed report of every requested requirement with evidence, defects and fixes.

- [ ] **Step 1: Extend smoke navigation.** Exercise top task hover metadata, drawer open/close, model picker, Colab handoff, upload dialog reachability, OCR visibility and subtitle click editing.

- [ ] **Step 2: Run QML at each target size.** Use the preview script for 1280×720, 1600×900, 1920×1080 and a 3840×2160 screenshot. Keep all captures under a temporary output directory outside the source tree.

- [ ] **Step 3: Inspect images.** Use `view_image` for each capture and reject any image with clipping, overlap, truncated critical labels, controls under the drawer scrim or unreadable contrast. Fix the source and rerun the capture until all checks pass.

- [ ] **Step 4: Write the indexed report.** Include sections 1–10: scope, top task bar, left shelf/right drawer, model/Colab/upload, media preview, OCR/subtitle, voice catalog, responsive matrix, automated tests, packaging/Git evidence and remaining release boundary.

### Task 9: Update graphify and run the complete verification matrix

**Files:**
- Modify: `graphify-out/` generated outputs through graphify commands only
- Modify: `docs/superpowers/audits/2026-08-28-dubbing-workspace-overhaul-report.md`

**Interfaces:**
- Consumes: the current source tree and the completed audit report.
- Produces: refreshed `graph.json`, `GRAPH_REPORT.md`, HTML visualization, manifest and graph health output.

- [ ] **Step 1: Run graphify incrementally from the repository root.** Use the interpreter saved in `graphify-out/.graphify_python` and run the documented `--update` path for `C:\Users\Nguyen Trong Khoi\Downloads\TTS\LA-Studio`.

- [ ] **Step 2: Run the graph health check.** Confirm no empty graph, missing endpoint or collapsed-edge warning; record node/edge/community counts in the report.

- [ ] **Step 3: Generate HTML output and read the required report sections.** Record God Nodes, Surprising Connections and Suggested Questions without inventing relationships.

- [ ] **Step 4: Run the full QtTest matrix.** Execute `ctest --test-dir out/build/windows-msvc-release -C Release --output-on-failure` and record the exact pass/fail count. Also run `git diff --check`.

### Task 10: Build the 8.5 portable package, commit and push

**Files:**
- Modify: no source files unless the verification matrix identifies a concrete failure
- Generated: `out/LA-Studio-0.0.8.5/`

**Interfaces:**
- Consumes: the verified source tree, `.tools/Qt/6.9.3`, pinned runtime payloads and `scripts/package.ps1`.
- Produces: an 8.4-style portable package with `LA-Studio-0.0.8.5.exe` at its root, valid runtime/license manifests, a commit and a pushed GitHub branch.

- [ ] **Step 1: Stage only intended implementation/report files.** Review `git status --short` and use explicit paths; do not stage existing generated graph cache or unrelated worktree changes unless the graph update is part of this task.

- [ ] **Step 2: Build the portable package.** Run:

```powershell
& '.\scripts\package.ps1' `
  -Preset 'windows-msvc-release' `
  -QtRoot '.tools\Qt\6.9.3' `
  -Version '0.0.8.5' `
  -MaxParallelJobs 4 `
  -SkipInstaller `
  -PortableInternalLayout `
  -AllowUnsignedEspeakForInternalBuild
```

- [ ] **Step 3: Verify the package.** Assert the root EXE has file/product version `0.0.8.5`, all required runtime folders exist, the staged manifest passes, and portable QML smoke exits with code 0.

- [ ] **Step 4: Commit the implementation.** Use a focused commit message such as `feat: overhaul dubbing workspace interactions` after the report, tests and package evidence are present.

- [ ] **Step 5: Push the current branch to its configured GitHub remote.** Capture `git rev-parse HEAD`, `git branch --show-current`, `git remote -v` and the push result in the report. If the remote rejects the push, report the exact rejection and leave the local commit intact.

## Plan self-review

- Spec coverage: all nine acceptance criteria are mapped to Tasks 1–10; no requirement is left only in prose.
- Placeholder scan: no prohibited placeholder token or unspecified handling step was found.
- Type/contract consistency: the plan keeps existing controller methods as inputs and defines the only new QML signal (`contextRequested(string)`) and drawer property (`contextId`) before later tasks consume them.
- Scope boundary: the plan does not delete user data, rewrite unrelated generated outputs, or turn the unsigned internal runtime into a public release.
