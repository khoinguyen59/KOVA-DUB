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

- [x] **Step 1: Map current contracts before edits.** Record the existing workflow node ids (`import`, `normalize`, `source-separate`, `transcribe`, `translate`, `synthesize`, `mix`, `export`), the current `VoiceClonePresetService` fields and the two existing model-dialog callbacks in the plan review notes.

- [x] **Step 2: Add the test suite declaration.** Add `dubbing/test_DubbingWorkspaceContract.cpp` and its header to `TEST_SOURCES`, register the suite name `TestDubbingWorkspaceContract` in the existing test-suite dispatch, and keep `QT_QPA_PLATFORM=offscreen` for the suite.

- [x] **Step 3: Write the failing contract tests.** The test fixture asserts these exact behaviors:

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

- [x] **Step 4: Run the focused suite and confirm the baseline result.** `TestDubbingWorkspaceContract` passes after the implementation was aligned with the real presentation IDs and notebook aliases.

### Task 2: Make the top workflow rail compact and bilingual-on-hover

**Files:**
- Modify: `qml/pages/DubbingPage.qml` (`stepTitle()`, `headerWorkflowSteps()` and header bindings)
- Modify: `qml/components/dubbing/DubbingWorkflowHeader.qml`
- Modify: `qml/components/dubbing/DubbingWorkflowStep.qml`
- Modify: `qml/Main.qml` QML smoke assertions for the Dubbing route

**Interfaces:**
- Consumes: `DubbingController::workflowStages()` with its stage id, label, icon and status fields.
- Produces: each `DubbingWorkflowStep` receives `shortTitle` and `detailTitle`; `shortTitle` is rendered and `detailTitle` is used by `ToolTip` and accessibility text.

- [x] **Step 1: Add explicit short/detail labels in the page mapping.** Use the existing stage ids to return `Import`, `Normalize`, `Separate`, `Transcribe`, `Align`, `Translate`, `Synthesize` and `Mix & Export`; keep the existing Vietnamese text as the detail label.

- [x] **Step 2: Render only the short label in the resting state.** Update `DubbingWorkflowStep.qml` so `Text.text` binds to `shortTitle`, while `ToolTip.text` is `shortTitle + " (" + detailTitle + ")"`. Preserve the existing active/completed colors and 34 px rail height.

- [x] **Step 3: Keep the rail scrollable without shrinking action controls.** Retain `workflowStepsFlickable` as the sole flexible header region and verify every action button remains within `headerActionCluster` bounds.

- [x] **Step 4: Add a smoke assertion for labels.** Assert that the first eight rendered step titles contain no Vietnamese parenthetical text and that hovering/focus metadata contains the detail text.

- [x] **Step 5: Run `ctest --test-dir out/build/windows-msvc-release -C Release -R QmlRouteSmoke --output-on-failure`.** The test reaches the Dubbing route and completes its existing route trace.

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

- [x] **Step 1: Write the drawer contract test first.** The production QML smoke and `TestDubbingWorkspaceContract` exercise the drawer open/close, central preview, clipping and nested content-scroll contracts.

- [x] **Step 2: Create the drawer shell.** Implement a Qt Quick `Drawer` with `edge: Qt.RightEdge`, width bounded to `Math.min(520, Math.max(320, parent.width * 0.30))`, `height: parent.height`, `clip: true`, and step-specific inner `ScrollView`/`ListView` content. The inner-scroll choice avoids nested scrolling over transcript and synthesis lists.

- [x] **Step 3: Convert the task shelf into compact feature actions.** Replace its always-rendered `DubbingNodeSettingsPanel` body with buttons for `Results`, `Settings`, `Model`, `Colab`, `Upload` and `Handoff`. Keep the current step title/status as a compact summary above the buttons.

- [x] **Step 4: Wire the contexts in `DubbingPage.qml`.** Remove the direct right-pane `DubbingReviewPanel` and `DubbingNodeInspector` layout children. Host them inside `DubbingContextDrawer`, map `results` to `DubbingReviewPanel`, `settings` to `DubbingNodeInspector`, and keep the existing callback wiring to `nodeModelDialog`, `translationFixDialog`, `dubbingArtifactUploadDialog`, `transcriptEditor`, `subtitleEditor`, `exportOptionsDialog` and playback handlers.

- [x] **Step 5: Remove the duplicate header Colab/Workflow actions.** Keep one left-shelf Colab button and retain only actions that are global to the editor in the header. The old `Manage Colab route` path is not rendered in the Dubbing shelf.

- [x] **Step 6: Verify layout at 1280×720.** Production QML smoke and visual capture show the drawer and shelf without squeezing the preview below its minimum width.

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

- [x] **Step 1: Add a failing QML gate contract.** The shelf Run path preserves the node id and opens `WorkflowNodeModelDialog` before a terminal error route.

- [x] **Step 2: Implement `ensureNodeExecutionRoute(nodeId)`.** The QML gate checks configuration/readiness and either runs the node or opens the model dialog with the preserved node id.

- [x] **Step 3: Make `Use selected model on Colab` transactional.** The dialog commit path preserves the selected stage id before opening contextual Colab setup.

- [x] **Step 4: Fix upload reachability.** Each upload path opens `DubbingArtifactUploadPanel.openFor(nodeId)` and uses the controller allow-list/copy contract.

- [x] **Step 5: Add missing-stem recovery coverage.** Workflow and artifact tests assert that incomplete separation remains incomplete and never becomes a normalized-source substitute.

- [x] **Step 6: Run the focused C++ and QML tests.** The new contract suite, source-separation suite and full QML route smoke pass in the 41-test matrix.

### Task 5: Simplify media preview, add thumbnail state and enforce 16:9 containment

**Files:**
- Modify: `qml/components/dubbing/DubbingSourceMediaPanel.qml`
- Modify: `qml/pages/DubbingPage.qml` only for source-selection callback preservation
- Modify: `tests/CMakeLists.txt` and QML smoke fixture data if a new portrait/square fixture is required
- Modify: `tests/dubbing/test_DubbingWorkspaceContract.cpp` for preview-state contract where controller state is involved

**Interfaces:**
- Consumes: `DubbingController::sourceMediaPath`, `sourceMediaUrl`, `playbackMediaUrl`, `dubbedVocalPath`, `backgroundPath`, and the existing `MediaPlayer`/`VideoOutput` objects.
- Produces: a fixed 16:9 `previewSurface`/`previewFrame`, visible first-frame loading poster state, and only `Fit source`, `Original`, `Dubbed` in the persistent preview toolbar.

- [x] **Step 1: Add the failing preview assertions.** Contract tests and QML smoke assert the three-action toolbar, 16:9 frame and thumbnail/loading object.

- [x] **Step 2: Remove persistent source-management controls.** Loaded preview toolbar no longer contains source management; shelf/pre-selection paths retain source acquisition.

- [x] **Step 3: Implement first-frame thumbnail behavior.** `MediaPlayer` remains non-autoplaying; `VideoOutput` renders the first frame and the neutral poster is visible until `LoadedMedia`/`BufferedMedia`.

- [x] **Step 4: Lock the viewport geometry.** Preview geometry is fixed at 16:9 and uses `VideoOutput.PreserveAspectFit`; letterbox behavior is preserved.

- [x] **Step 5: Keep paths collision-safe.** Path labels use middle elision/tooltips and playback controls have bounded columns.

- [x] **Step 6: Run the media and route tests.** Media/route suites pass in the full matrix and real-fixture preview was captured at 1280×720.

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

- [x] **Step 1: Add visibility assertions.** QML source and route smoke gate `dubbingSubtitleOcrRoiOverlay` by Transcribe/OCR context and controller state.

- [x] **Step 2: Gate the ROI editor by task context.** `showOcrTools` is passed from `DubbingPage` and combined with the controller ROI flag.

- [x] **Step 3: Set a safe default subtitle position.** Default and OCR subtitle placement reserve a tested clearance above media controls.

- [x] **Step 4: Preserve direct editing.** Clicking the subtitle overlay routes `subtitleSegmentEditRequested(activeSubtitleIndex)` without forcing OCR setup.

- [x] **Step 5: Run `ctest --test-dir out/build/windows-msvc-release -C Release -R "TestSubtitleOcrController|TestSubtitleOcrPipeline|QmlRouteSmoke" --output-on-failure`.** OCR controls are absent from the TTS state and visible in the captured Transcribe/OCR state.

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

- [x] **Step 1: Add failing catalog tests.** Tests cover catalog-derived counts, stable IDs, VieNeu metadata and OmniVoice compatibility.

- [x] **Step 2: Normalize voice metadata at load time.** `VoiceClonePresetService` now supplies the normalized fields without changing serialized IDs.

- [x] **Step 3: Centralize count derivation.** Gallery and TTS options derive counts from loaded records; hard-coded marketing counts are removed.

- [x] **Step 4: Implement compatibility-aware selection.** VieNeu references preserve source metadata while routing the clone worker to OmniVoice and the model dialog path.

- [x] **Step 5: Remove the redundant no-segments banner from the primary TTS surface.** No-segment handling remains a Run-time recovery route to Transcribe/OCR.

- [x] **Step 6: Run `ctest --test-dir out/build/windows-msvc-release -C Release -R "TestDubbingWorkspaceContract|TestDubbingProject|TestWorkflowGraph" --output-on-failure`.** Counts and voice routing pass against real catalog data.

### Task 8: Add production QML visual regression evidence

**Files:**
- Modify: `qml/Main.qml` smoke state and route exercise
- Modify: `tests/CMakeLists.txt` only if the smoke target needs a fixture path
- Modify: `scripts/preview_dubbing_ui.ps1` to launch the production route with the agreed viewport
- Create: `docs/superpowers/audits/2026-08-28-dubbing-workspace-overhaul-report.md`

**Interfaces:**
- Consumes: the production `Main.qml` route, existing smoke fixture media and QML object names.
- Produces: a repeatable screenshot/inspection procedure and an indexed report of every requested requirement with evidence, defects and fixes.

- [x] **Step 1: Extend smoke navigation.** Production smoke and preview hooks cover task metadata, drawer open/close, model/Colab/upload routing, OCR visibility and subtitle placement/editing contracts.

- [x] **Step 2: Run QML at each target size.** Preview captures were run for 1280×720, 1600×900, 1920×1080 and 3840×2160 logical viewports.

- [x] **Step 3: Inspect images.** Captures were visually inspected; the subtitle/control collision found in the first OCR capture was fixed and the capture rerun.

- [x] **Step 4: Write the indexed report.** The indexed report includes sections 1–10 and the remaining external-runtime boundary.

### Task 9: Update graphify and run the complete verification matrix

**Files:**
- Modify: `graphify-out/` generated outputs through graphify commands only
- Modify: `docs/superpowers/audits/2026-08-28-dubbing-workspace-overhaul-report.md`

**Interfaces:**
- Consumes: the current source tree and the completed audit report.
- Produces: refreshed `graph.json`, `GRAPH_REPORT.md`, HTML visualization, manifest and graph health output.

- [x] **Step 1: Run graphify incrementally from the repository root.** Graphify was run incrementally after source changes.

- [x] **Step 2: Run the graph health check.** Health output is recorded in the report with no missing endpoint, self-loop or collapsed-edge issue.

- [x] **Step 3: Generate HTML output and read the required report sections.** Aggregated HTML/report output was generated and read without inventing relationships.

- [x] **Step 4: Run the full QtTest matrix.** Full matrix passes 41/41 and `git diff --check` is clean apart from normal Windows line-ending warnings.

### Task 10: Build the 8.5 portable package, commit and push

**Files:**
- Modify: no source files unless the verification matrix identifies a concrete failure
- Generated: `out/LA-Studio-0.0.8.5/`

**Interfaces:**
- Consumes: the verified source tree, `.tools/Qt/6.9.3`, pinned runtime payloads and `scripts/package.ps1`.
- Produces: an 8.4-style portable package with `LA-Studio-0.0.8.5.exe` at its root, valid runtime/license manifests, a commit and a pushed GitHub branch.

- [x] **Step 1: Stage only intended implementation/report files.** Staging will use explicit implementation/report paths and omit generated cache.

- [x] **Step 2: Build the portable package.** Run:

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

- [x] **Step 3: Verify the package.** Root EXE File/ProductVersion `0.0.8.5`, required runtime folders/artifacts exist, the package staging manifests pass, and portable QML smoke exits with code 0.

- [x] **Step 4: Commit the implementation.** Implementation, tests, package evidence and the indexed report were committed as `c9c795d5` (`feat: complete dubbing workspace overhaul`).

- [ ] **Step 5: Push the current branch to its configured GitHub remote.** Capture `git rev-parse HEAD`, `git branch --show-current`, `git remote -v` and the push result in the report. If the remote rejects the push, report the exact rejection and leave the local commit intact.

## Plan self-review

- Spec coverage: all nine acceptance criteria are mapped to Tasks 1–10; no requirement is left only in prose.
- Placeholder scan: no prohibited placeholder token or unspecified handling step was found.
- Type/contract consistency: the plan keeps existing controller methods as inputs and defines the only new QML signal (`contextRequested(string)`) and drawer property (`contextId`) before later tasks consume them.
- Scope boundary: the plan does not delete user data, rewrite unrelated generated outputs, or turn the unsigned internal runtime into a public release.
