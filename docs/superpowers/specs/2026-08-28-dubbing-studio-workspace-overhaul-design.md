# Dubbing Studio Workspace Overhaul Design

**Date:** 2026-08-28  
**Status:** Design approved in chat; implementation awaits spec review  
**Scope:** Production Qt 6/QML Dubbing workspace, task navigation, model/Colab entry points, media preview, OCR/subtitle presentation, voice catalog consistency, QA evidence and portable packaging

## Goal

Make the Dubbing Studio workspace content-first and operationally direct: the video/timeline remain visible, task actions are discoverable from the left shelf, configuration opens on demand, and every blocked action leads to the exact recovery choice instead of a dead-end error.

## Resolved interpretation

The current right-side control panel is not removed functionally. Its three contexts—results, detailed configuration and handoff—become an on-demand right drawer opened by compact feature buttons in the left task shelf. The drawer must not consume a permanent three-column width when it is closed.

The source version remains `0.0.8.5`. The final artifact uses the portable directory layout already used by the 8.4 artifact: the versioned executable is at the package root beside its runtime folders.

## Product and interaction design

### Top task navigation

- Each task tab displays only an English number/name pair in its resting state: `1. Import`, `2. Normalize`, `3. Separate`, `4. Transcribe`, `5. Align`, `6. Translate`, `7. Synthesize`, `8. Mix & Export`.
- Hover/focus tooltip exposes the bilingual label, for example `4. Transcribe (Nhận Dạng Lời Thoại)`. The tooltip is keyboard reachable and does not change tab width.
- The active task keeps its existing selected visual state. No Vietnamese text is permanently rendered in the compact task bar.
- Task names come from one route/task registry; QML does not duplicate counts or labels in individual pages.

### Left task shelf and right drawer

- The always-visible left shelf contains compact, labeled feature buttons: `Results`, `Settings`, `Model`, `Colab`, `Upload`, and `Handoff` where the current task supports them.
- `Results`, `Settings`, and `Handoff` open the right drawer with the corresponding existing content. The drawer has an accessible close button, a stable width bound, and a `ScrollView` for variable content.
- `Colab` is the single entry point for Colab configuration. The duplicate `Manage Colab route` action is removed from the Dubbing task UI and no second dialog path is kept.
- Drawer state is local to the Dubbing page. Opening one context closes the previous context without destroying workflow state.
- At small desktop widths the drawer overlays the content rather than squeezing the video below its minimum usable width.

### Action gating and recovery

- Run, Open model and Upload controls remain visually actionable whenever the operation can be started through a setup dialog.
- A missing local model or missing Colab worker routes to the task-specific model picker/Colab setup dialog. It does not show a terminal error dialog as the first response.
- `Open model` followed by `Use` applies the selected model to the task. For a remote/Colab selection it immediately opens the Colab setup dialog and preserves the selected family/node.
- `Run task` uses the same gate. If the task has no usable execution route, it opens the model picker with the task context preselected.
- Upload accepts only files declared by the artifact contract, copies them into the current project, updates the corresponding workflow state, and returns the user to the next actionable task. A rejected file explains the accepted filename/type and keeps the dialog open.
- A missing separation stem is represented as an actionable preflight state. The UI offers model selection or exact artifact upload; it never silently substitutes normalized source audio for a required stem.

## Media preview and subtitle/OCR behavior

- After media selection, the browser/download/replace controls are removed from the persistent preview toolbar. The toolbar contains only `Fit source`, `Original`, and `Dubbed`.
- The preview viewport is always a 16:9 container. Source media uses `PreserveAspectFit`, so 16:9, 9:16 and 1:1 media remain fully visible with letterboxing instead of changing the viewport geometry.
- The preview shows a first-frame thumbnail or a generated/fallback poster while media is loading. Image loading is asynchronous and failures expose a textual recovery hint.
- OCR scan-region editing is rendered only inside the Transcribe/OCR task. Other tasks do not show OCR setup controls.
- Without OCR, subtitles use a stable default lower safe region. With OCR enabled, the subtitle overlay follows the accepted OCR region. The overlay is clickable and opens inline editing; initial setup is not required merely to view or edit subtitles.
- Subtitle/thumbnail and path labels use bounded text with middle elision and a tooltip for the full path. No path may overlap an icon, play button or drawer boundary.

## Voice and model catalog

- Voice counts are computed from the catalog returned by the C++/QML model, with filtered counts derived from the same list. Hard-coded `53`, `52+clone`, `19` and similar labels are removed.
- The TTS task card does not display a large marketing count such as `53 voices`; the count appears only in the compact catalog filter where it is accurate.
- Voice selection carries a stable voice id, family id, source/runtime capability and optional reference asset. Display name and sample path are not used as the primary identity.
- ViNeU references may be used with OmniVoice only when the catalog compatibility metadata says the reference format/language is accepted by the selected OmniVoice worker. If compatible, selecting the ViNeU voice sets OmniVoice as the required backend and opens its model/Colab path. If incompatible, the picker states the exact reason and offers a compatible model instead.
- The selected voice and selected runtime remain synchronized. Changing the voice invalidates an incompatible runtime selection rather than leaving a stale model displayed as ready.

## State and interface boundaries

- Existing C++ controllers remain the source of workflow truth. QML derives presentation state from properties and signals; it does not invent a second workflow state machine.
- Any new imperative boundary must be a public slot or `Q_INVOKABLE` with a `NOTIFY` property where QML binds to state. Dialogs receive a task/node id and return a result through an existing signal pattern or a narrowly scoped new signal.
- Model selection, Colab setup, upload and run requests carry the task/node context through the full route so a dialog cannot accidentally configure another task.
- Long-running work remains outside the UI thread. QProcess callbacks update state through queued signals; no synchronous media/model command is introduced in a QML handler.
- Error logging keeps the technical message. The user-facing surface maps known error categories to a concise explanation, recovery action and fallback status.

## Responsive and accessibility constraints

- The design is validated at 1280×720, 1600×900, 1920×1080 and a 4K desktop viewport. The first target keeps the central preview usable when the drawer is open.
- All scrollable variable-height content is clipped inside a `ScrollView`/`Flickable` with a bounded content width. Dialogs remain within the overlay height with a safe margin.
- Buttons have at least 36×36 px visual/hit bounds on desktop, with a preferred 40 px height. Focus, hover and pressed states do not shift surrounding layout.
- Primary and secondary text use the existing centralized theme tokens and maintain readable contrast. Icon-only actions have tooltips/accessibility names.
- The drawer and task tooltips are keyboard reachable; color is not the sole indicator of ready, blocked or active state.

## Implementation units

The implementation will first trace and then modify only the production surfaces required by this design:

- `qml/pages/DubbingPage.qml` for shell composition, drawer lifecycle, media toolbar and task-context routing.
- `qml/components/dubbing/DubbingWorkflowHeader.qml` and `qml/components/dubbing/panels/DubbingTaskShelf.qml` for compact task actions and bilingual hover labels.
- Existing Dubbing step components for OCR visibility, run/model/upload gates and subtitle editing.
- `qml/components/shared/WorkflowNodeModelDialog.qml`, `qml/components/dubbing/DubbingColabSetupDialog.qml`, `qml/components/dubbing/DubbingArtifactUploadPanel.qml`, and `qml/components/shared/VoiceGalleryDialog.qml` for contextual model, remote, upload and voice behavior.
- Catalog/controller C++ only where the existing public state cannot express a source-stable voice count, runtime compatibility or a contextual gate.
- QML smoke/test fixtures and one indexed report under `docs/superpowers/audits/` documenting evidence and remaining release constraints.

No generated build output, user project data or unrelated existing worktree changes are part of the source change set.

## Acceptance criteria

1. Production QML smoke loads `Main.qml`, reaches the Dubbing route, opens/closes the left-triggered drawer, opens the contextual model/Colab dialog, and returns without QML warnings that indicate a runtime failure.
2. The top task bar has compact English labels and bilingual hover/focus tooltips without horizontal clipping at 1280×720.
3. Closing the right drawer leaves the preview and timeline in a two-region content-first layout; opening it overlays or boundedly reserves space without overlapping controls.
4. Colab configuration has one Dubbing entry point; Upload is reachable and accepts the declared artifact contract; Run/Open model route to setup instead of dead-ending.
5. The video viewport remains 16:9 and contains 16:9, 9:16 and 1:1 sources without crop or geometry jump. A thumbnail is visible during load.
6. OCR controls appear only for Transcribe/OCR. Subtitle editing is available from the default overlay and from the OCR region when OCR is active.
7. All voice counts in the Dubbing and voice gallery surfaces match the catalog source. ViNeU/OmniVoice behavior follows compatibility metadata and never claims unsupported cloning.
8. Automated unit/regression tests and production QML route smoke pass. A screenshot at the agreed preview size is reviewed for clipping, collision, overflow and unreadable text.
9. Graphify is updated after source changes. The implementation is committed and pushed only after verification. The portable 8.5 package is built with the 8.4 layout command and its required runtime manifests pass.

## Verification evidence to record

The final indexed report will include the exact commands and results for:

- source-level QML/C++ checks and changed-file diff review;
- focused workflow/model/voice/upload tests;
- production QML smoke and screenshot path;
- 1280×720 layout inspection plus at least one larger viewport;
- graphify output path and graph health result;
- portable package command, executable metadata and required runtime presence;
- Git commit id and pushed branch/remote result.

## Risks and release boundary

The current environment contains an unsigned but SHA-256-verified eSpeak NG MSI. An internal portable build may use the existing explicit internal-build switch, but that artifact is not a public release until the runtime payload has a valid Authenticode signature. Missing optional PaddleOCR runtime is reported as optional and must not be presented as a bundled capability.
