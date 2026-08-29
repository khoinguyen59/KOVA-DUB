# AI agent response — offline Dubbing artifact upload

## 2026-08-29 — Spleeter Colab made self-contained

- Root cause was separated correctly: `k2-fsa/sherpa-onnx-spleeter-2stems-fp16`
  is the public model source, while the FastAPI worker and tunnel launcher are
  LA Studio application code. The notebook must not fetch the latter from a
  personal GitHub repository.
- `scripts/generate_spleeter_safe_colab_notebook.py` now embeds the exact local
  worker and launcher templates into `EMBEDDED_WORKERS`, verifies their
  normalized SHA-256 values inside Colab, and writes them to `/content`.
  `scripts/generate_alignment_separation_colab_notebooks.py` reuses the same
  safe Spleeter generator so duplicate generation cannot overwrite it with an
  older remote-worker design.
- `scripts/verify_colab_worker_pins.py` is now an offline embedded-bundle
  validator. It rejects `KOVA-DUB`, `WORKER_REPOSITORY`, `WORKER_COMMIT`, and
  runtime worker downloads, while still checking the official upstream model
  release and exact local source parity.
- The pre-commit/pre-push hooks, prebuild gate, CI, release workflow, CMake
  packaging comments, CTest contract, checklist, incident log, and agent
  memory now describe and enforce the self-contained design.

Verification for this change: embedded worker tests **5/5 PASS**, embedded
bundle validator **2/2 PASS**, generated notebook integrity **32/32 PASS**,
full prebuild gate **10/10 PASS**, CTest **41/41 PASS**, exact bindings
**31/31 PASS**, and remote feature surface **8/8 PASS**. No EXE was built as
part of this source-only fix.

## 2026-08-29 — immutable Colab worker pin regression fixed

- Reproduced the Colab `HTTP Error 404` in the Spleeter notebook. The generator
  referenced a commit that does not exist in `khoinguyen59/KOVA-DUB`, and it
  wrote to a non-canonical notebook directory.
- Corrected the single source of truth to repository
  `khoinguyen59/KOVA-DUB`, immutable commit
  `3f194b9155e7c2fcdd8eed4ac5fa980e6084417e`, the two verified worker SHA-256
  values, and `notebooks/voice_separation/`. Regenerated the checked-in
  notebook.
- Added `scripts/verify_colab_worker_pins.py`: bounded HTTP retry, commit/URL
  validation, remote SHA-256, local SHA-256 with Windows EOL normalization,
  and generator/notebook marker checks. It writes JSON evidence to
  `out\prebuild-gate\colab-worker-pins.json`.
- Added the validator to the prebuild gate, GitHub CI, Windows release, and
  optional repository Git hooks (`scripts/install_git_hooks.ps1`).

Verification: pin unit tests **6/6 PASS**; live remote payload verification
**2/2 PASS**; generated notebook integrity **32/32 PASS**; prebuild gate
**10/10 PASS**; full CTest **41/41 PASS**.

## 2026-08-29 — corrected missing Upload picker in the production dialog

- Reproduced the reported state in the production `DubbingArtifactUploadDialog`: the summary showed the required filename, but the body could be empty, so the user had no visible file-picker action.
- Root cause: `DubbingArtifactUploadPanel.visible` re-queried a transient `QVariant/JS map` and hid itself while the Repeater delegate settled. The delegate also used `modelData` without a Qt 6 `required property var modelData`, producing a runtime `ReferenceError` and preventing the panel from rendering.
- Fix: the dialog now passes the validated artifact contract into each panel; the panel stays visible for a valid contract; every Repeater delegate declares `required property var modelData`; the real lazy `FileDialog` and `Choose output` action remain controller-validated and independent of Colab.
- The production dialog now visibly exposes `Choose output`, `Use uploaded output and continue`, and `Skip task & continue`. Subtitle handoff displays `.srt`, `.vtt`, `.ass`, `.ssa`, `.txt`, `.md`, and `.markdown`.

Visual evidence: `out\\ui-demo\\dubbing-upload-dialog-production-1280x720.png` (production QML component captured from a real window; no build is accepted if the picker is absent).

Verification after the fix: focused `TestDubbingProject` **1/1 PASS**; full CTest **41/41 PASS**; QML lint **PASS**; prebuild gate **9/9 PASS**; packaged QML smoke **19 interaction events PASS**. Portable EXE `out\\LA-Studio-0.0.8.7\\LA-Studio-0.0.8.7.exe` is `30,986,752` bytes with SHA-256 `47C0A81780E596DB5EBFCCF959D034CF11645A6CBF54C2FD45E9E8857501C14F`.

## 2026-08-29 — explicit Upload / Skip handoff correction

- Upload is now a local handoff and is independent from `Run`, model setup,
  URL/token, and Colab connectivity. Every artifact panel has a real lazy
  `FileDialog`, displays the exact required filename/extension, and validates
  the selected file through the controller before importing it.
- The upload dialog and the right-side `Data & Artifacts` tab now expose
  `Skip task & continue`. Skip records a durable in-session `skipped` state,
  preserves existing output metadata, advances the manual workflow, and never
  starts a worker. Import remains the only non-skippable source prerequisite.
- Combined STT + OCR handoff no longer advances after the first file. It keeps
  the dialog open and reports `Accepted 1 of 2 outputs` until both independent
  artifacts are accepted; a single STT-only or OCR-only contract continues
  immediately.
- Added regression coverage for the exact contract passed into each QML panel,
  both picker surfaces, the skip action, multi-artifact counting, alias
  normalization, and controller skip state. The same recheck applies across
  all eight canonical tasks, not just the task where the issue was reported.

### Verification for this correction

- `run_tests.ps1`: **41/41 CTest PASS**, including `TestDubbingProject`,
  `TestDubbingWorkspaceContract`, and `QmlRouteSmoke`.
- QML lint: PASS. Prebuild gate: PASS, 9/9 groups; exact bindings 31/31;
  generated notebooks 32/32; remote surface 8/8.
- Portable EXE refresh: version `0.0.8.7`, packaged QML smoke PASS with 19
  interaction events. Size `30,986,752` bytes; SHA-256
  `47C0A81780E596DB5EBFCCF959D034CF11645A6CBF54C2FD45E9E8857501C14F`.

## 2026-08-29 — setup preflight regression and cross-task recheck rule

- Fixed the remaining Transcribe setup path: `Run STT` now uses the same
  controller-backed preflight as the backend and opens the STT model picker
  when setup is missing. `Run OCR` opens the dedicated Subtitle OCR
  model/Colab setup surface, including the exact model and worker fields.
- Missing setup is emitted as `workflowSetupRequired` and is not sent through
  `setError`, so the user does not receive the generic Error Guidance modal.
  Real runtime/input failures remain in the technical log and keep their
  actionable guidance path.
- Added a mandatory cross-task regression rule to
  `PRE_DELIVERY_CHECKLIST.md`: every bug fix must be checked across all eight
  canonical tasks and the equivalent entry, setup, error, state, handoff and
  UI surfaces. A single-task check is not release evidence.
- Added a source contract regression assertion so the checklist cannot lose
  this requirement silently.

### Verification for this correction

- `run_tests.ps1`: **41/41 CTest PASS**; `TestDubbingProject` and
  `TestDubbingWorkspaceContract` both pass.
- QML lint: PASS. Prebuild release gate: PASS, all 9/9 groups; exact bindings
  31/31; generated notebooks 32/32; remote surface 8/8.
- Portable EXE rebuilt from the corrected source; packaged QML smoke PASS with
  19 interaction events. File/Product version `0.0.8.7`, size `30,986,752`
  bytes, SHA-256
  `EDC70DC753E5DD3E51F38F1C5E5B941372C90A0088A17B6B43001F531F914623`.

## 2026-08-29 — canonical STT/OCR handoff and workflow order

- Khi cả `STT` và `OCR` cùng có output, `Reconcile & Continue` bắt buộc gọi
  `DubbingController::reconcileTranscriptSources()` trước khi đi tiếp. Fusion
  dùng `prefer-stt` làm mặc định: cue STT giữ timeline/nội dung chuẩn; OCR
  được giữ trong provenance/evidence, không tự chèn OCR-only cue vào script
  canonical. Nếu chỉ có một nguồn, Continue không yêu cầu nguồn còn lại.
- Workflow production đã đồng bộ thành `1 Import → 2 Normalize → 3 Separate
  (optional) → 4 Transcribe → 5 Translate → 6 Synthesize → 7 Align → 8 Mix &
  Export`. Automatic run loại Separate khỏi graph thực thi; manual Separate
  vẫn có thể chạy khi người dùng muốn tạo stems.
- Align đã lưu mức `originalGainPercent` và `dubbedGainPercent`, mặc định
  `0/100`, và truyền mức tiếng gốc xuyên suốt sidechain release để không làm
  tiếng gốc quay lại sau khi ducking.
- Project mới và project migrate thiếu giá trị dùng mặc định `zh → vi`; các
  guide `AI_AGENT_TRANSCRIPT_RECONCILIATION_GUIDE.md` và
  `AI_AGENT_TRANSLATION_PROMPT.md` là hợp đồng IDE-agnostic cho agent xử lý
  script/fusion/translation.

### Verification boundary

- `scripts/run_tests.ps1` trên Qt 6.9.3/MSVC đã chạy **41/41 CTest PASS**.
- Packaged EXE smoke, QML lint, prebuild gate và artifact hash đã hoàn tất:
  prebuild `9/9` nhóm PASS, CTest `41/41`, packaged smoke `19` interaction
  events, FileVersion/ProductVersion `0.0.8.7`, portable root không có `bin/`.
- Artifact: `out/LA-Studio-0.0.8.7/LA-Studio-0.0.8.7.exe`, SHA-256
  `EDC70DC753E5DD3E51F38F1C5E5B941372C90A0088A17B6B43001F531F914623`.
- Đây là bằng chứng build/startup/QML/local contract; không coi nó là bằng
  chứng cho live Colab/GPU inference nếu chưa chạy worker thật với credential.

## 2026-08-29 — independent STT/OCR cards and route-safe handoff

- Replaced the single Transcribe source selector with two stacked production
  cards: `STT · Speech-to-Text` and `OCR · Subtitle OCR`. Each card owns its
  Model, Colab, Upload, and Run action; the visible `Run STT` and `Run OCR`
  aliases no longer inherit a persisted combined/reconcile mode.
- `runSpeechToTextIndependently()` now starts only the audio STT runner and
  keeps the user on Transcribe after completion. OCR continues to use its
  separate `SubtitleOcrController`. Either saved source is sufficient to
  enable Continue; Reconcile remains optional and is shown only after both
  sources exist.
- The only permitted manual concurrency is STT with OCR. Artifact handoff and
  cancellation are route-scoped: an STT upload/stop cannot cancel OCR, and an
  OCR upload/stop cannot cancel STT. The shared Cancel action now also stops an
  independent OCR run. OCR scan controls are limited to the Transcribe page.

### Verification boundary

- QML lint exited 0 and `git diff --check` completed without whitespace errors.
- A source-level regression contract covers the two cards, node aliases,
  independent completion, route-scoped artifact cancellation, and OCR-only
  visibility. CTest/build was not rerun because this machine still lacks the
  configured Qt 6 MSVC development tree; no EXE was built in this change.

## 2026-08-29 — upload workflow output without Colab

- Fixed the Dubbing artifact handoff so **Upload does not require a Colab
  session, URL, token, model, or a previous Colab run**. The user can select
  an output already saved on the computer and the existing controller still
  validates the exact filename, extension, count, and subtitle/audio/video
  content before importing it.
- Normalized visible task aliases (`separate`, `stt`, `ocr`, `alignment`,
  `export-output`) to their durable artifact contracts. The combined STT + OCR
  handoff now exposes independent STT and OCR upload entries; reconciliation
  remains a later operation.
- Reworded the upload UI to state that Colab is optional and to show the
  required filename and allowed format before selection. A missing contract is
  now an explicit actionable message instead of a blank dialog.

### Verification boundary

- QML lint passed and `git diff --check` passed. Graphify was updated after
  source edits.
- A fresh C++/CTest run was attempted but could not start because this machine
  currently has no Qt 6 MSVC development kit or configured test build tree;
  therefore no new C++ test-pass claim or EXE build is made in this fix-only
  pass.

# AI agent response — local download and resumable Dubbing projects

## 2026-08-24 — completed: OCR Colab revision and reviewed-transcript upload

- Fixed the false **outdated Subtitle OCR notebook** warning by synchronizing
  the desktop-required worker revision with the current generated exact OCR
  notebook: `subtitle-ocr-2026-08-23.17`.
- Fixed Dubbing's completed-artifact pickers, including **Reviewed STT + OCR
  transcript**: clicking **Choose output** now creates and opens a real picker;
  accepting a file retains the selection so **Use uploaded output and
  continue** can validate/import it. Filename/extension checks and
  task-specific transfer cancellation remain in the controller.
- Added regression coverage for the current OCR handshake and picker wiring;
  corrected one stale Spleeter notebook wording assertion.
- Evidence: Windows test build succeeded; `TestDubbingProject` passed; full
  CTest passed 39/39. No GUI/live Colab run and no EXE packaging were performed
  in this fix-only pass.

## 2026-08-24 — completed: default inline Dubbing subtitles, internal portable 0.0.7.9

### Delivered

- Dubbing now uses the stored default subtitle style to display the active
  caption directly over the preview whenever that segment has source or target
  text. Target text is preferred when configured, with a source-text fallback.
- Clicking the displayed caption opens a compact contextual editor for that
  exact segment. It saves through the existing `updateSegment` path, so normal
  Dubbing persistence and downstream invalidation remain in effect.
- The large **Dubbing subtitles** dialog is now reserved for the explicit
  **Subtitle style & import** action. Advancing after transcript/translation
  review no longer opens that dialog automatically.
- Built the requested portable internal executable:
  `out/LA-Studio-0.0.7.9/LA-Studio-0.0.7.9.exe`.
  FileVersion/ProductVersion: `0.0.7.9`.
  SHA-256: `2101E91B16019BC28046E814D571761079DBD2B1444699A7C56E3BC8FC3C27D4`.

### Evidence and boundary

- Release source build succeeded. Focused `TestDubbingProject` passed (1/1),
  including assertions for the inline editor, preview click wiring, and the
  non-automatic advanced dialog. The QML route smoke test also passed.
- Packaging output contains `qwindows.dll`, `qoffscreen.dll`, QML modules,
  `LAStudioRuntimeHost.exe`, `yt-dlp.exe`, and the subtitle-OCR manifest.
- Full CTest was not clean: the only failure was the unrelated existing
  `TestColabSeparationRunner` textual assertion that expects an obsolete
  notebook sentence. It does not exercise caption UI or packaging.
- This pass did not open the owner's GUI or start a live Colab worker.

## 2026-08-22 — completed: internal portable package 0.0.7.6

### Delivered

- Built the requested portable internal EXE at
  `out/LA-Studio-0.0.7.6/LA-Studio-0.0.7.6.exe`.
- Source version, FileVersion, and ProductVersion match `0.0.7.6`.
  SHA-256: `2BBA1C68B321C47693FD9B60A3BAD73C070D17FC292D9AF34E1D41278EAB4A87`.
- The portable staging checks completed. Post-package audit verified both
  `qwindows.dll` and `qoffscreen.dll`, the bundled OCR manifest, Tesseract
  `5.5.1`, and the staged license directory (176 files).

### Boundary

The app GUI was not opened and no new live Colab session was run for this
package. This confirms the package layout and versioned binary, not a new
remote-GPU acceptance run.

## 2026-08-22 — completed: Dubbing recheck, no new package

### Fixed in source

- A zero-sample STT decode now ends the Dubbing job with a visible error and
  does not contact the remote worker. This removes the previously possible
  blocked task after "No audio data was decoded."
- Manual **Run STT now** and **Run Subtitle OCR now** can operate independently
  and concurrently because they use distinct workers. **Reconcile saved STT +
  OCR** remains a local-only final action and requires both saved sources.
- The static remote-feature verifier now follows Spleeter's real signed
  notebook/worker/launcher architecture instead of falsely demanding the
  launcher tunnel text in the bootstrap notebook itself.

### Evidence and boundary

- Release build without deployment succeeded; CTest **39/39**; generated
  notebooks **32/32**; exact bindings **31/31**; live acceptance contracts
  **9/9**; remote feature surface **8/8**; `git diff --check` clean. QML lint
  completed with exit 0 and five existing warnings outside this change.
- No visible GUI, no machine-control interaction, no fresh Colab worker, and
  no EXE package were produced for this recheck. A fresh URL/token plus real
  media is still required for external acceptance; this report does not claim
  that unrun Colab infrastructure was tested live.

## 2026-08-17 — completed: independent Dubbing STT/OCR control (0.0.7.5)

### What changed

- **STT**, **Subtitle OCR**, and **Reconcile saved STT + OCR** are now explicit
  Dubbing actions in the task shelf.
- The selected next action no longer determines whether either Colab worker can
  be selected, connected, or verified. STT and OCR retain independent model,
  route, notebook, URL, token, and connection state.
- Reconciliation is local-only. It never starts, configures, disconnects, or
  disables either worker; it is the final action after both saved results exist.
- A manual STT job no longer locks OCR setup. Only an Automatic Dubbing run
  freezes shared OCR scan/route settings.

### Verification and operator flow

- Full CTest: **39/39 passed** in 61.13 seconds; Dubbing, OCR controller, and
  offscreen QML-route regressions passed. `graphify update .` and
  `git diff --check` passed.
- Configure/connect STT and use **Run STT now**. Configure/connect Subtitle OCR
  and use **Run Subtitle OCR now**. Once both results are saved, use
  **Reconcile saved STT + OCR**. Manual media jobs remain serialized, but either
  worker can be configured while the other runs.
- Internal portable package: `out/LA-Studio-0.0.7.5/LA-Studio-0.0.7.5.exe`.
  Source, FileVersion, and ProductVersion are all `0.0.7.5`; SHA-256:
  `736983215DBCB18EF299BAD8B69BD7BBA4C4BFD0707E2A721DA66FF5745EB189`.
  The package manifest verified 19 runtime and 18 license artifacts; staged
  FFmpeg, FFprobe, yt-dlp, and the application's offscreen QML smoke exited
  successfully.
- No visible desktop window or live Colab session was opened. A fresh Colab
  session remains the acceptance check for temporary worker URL/token.

## 2026-08-16 — completed: valid FLAC STT recovery and package 0.0.7.4

### Root cause and repair

- The live desktop log showed a valid Direct Colab `vocals.flac` (45.6 MB,
  44.1 kHz stereo, ~15 minutes) reaching the STT stage. The old standalone
  `QAudioDecoder` completed with **0 samples**, which produced “No audio data
  was decoded.” It was not an empty artifact.
- Replaced that path with the shared asynchronous decoder. It validates the
  file, decodes off the UI thread, and falls back to the packaged FFmpeg path
  when Qt cannot decode FLAC; the STT session receives mono 16 kHz PCM.
- STT, OCR, and reconciliation are now distinct Dubbing actions. Run STT or OCR
  independently; reconciliation consumes their completed saved transcripts and
  does not start either model.
- Updated the generated PP-OCRv5 Colab bootstrap with its missing compatibility
  `langchain` dependency. No live Colab session was run by this delivery.

### Verification and handoff

- CTest: **39/39 passed**. Exact-model notebook verifier: **32/32 passed**.
- Packaged offscreen QML smoke: exit `0`.
- Internal EXE: `out/LA-Studio-0.0.7.4/LA-Studio-0.0.7.4.exe`.
- SHA-256:
  `F179A90B98C6517EFE3017939F50F3F5D6B0D9068958C283C5F63512A6536555`.

### Boundary

The package has been checked without opening a visible window. A new Colab
runtime remains the user's acceptance step for its temporary GPU worker.

## 2026-08-16 — completed: FLAC Colab isolation transport and package 0.0.7.3

### Delivered

- Direct Colab Isolator now uses lossless FLAC by default for `vocals` and
  `background`, with WAV retained as an explicit compatibility selection.
- Updated Spleeter and UVR worker/notebook contracts to publish
  `artifact_format` and `artifacts_ready`. The app now visibly reports that
  Colab has created both stems before it starts downloading them.
- Moved large stem preview decode off the UI thread, rejects truncated FLAC
  before decode, and preserves both new FLAC and old WAV Voice Clone cache
  files.
- Built portable internal EXE:
  `out/LA-Studio-0.0.7.3/LA-Studio-0.0.7.3.exe`.

### Evidence

- Source tests: **39/39 passed, 0 failed**; Python worker/generator compile
  passed.
- Package audit: **19/19 runtime** and **18/18 license** artifacts present.
- Source/FileVersion/ProductVersion: `0.0.7.3`.
- SHA-256:
  `AD99D4145471491FD36C623C1FFCA661DAD1ED5CFA0854B47F8DDD85A798E313`.
- Packaged offscreen QML smoke: exit `0`; FFmpeg, FFprobe and yt-dlp staged
  launch checks passed.

### Boundary

No visible desktop UI or live Colab runtime was started. The package is for
internal use because the verified eSpeak payload remains unsigned. Graphify was
attempted but its CLI is not available on PATH.

## 2026-08-15 — completed: package 0.0.7.2

### Delivered

- Built and staged the portable internal executable:
  `out/LA-Studio-0.0.7.2/LA-Studio-0.0.7.2.exe`.
- Source `LASTUDIO_VERSION`, FileVersion, and ProductVersion are all
  `0.0.7.2`.

### Evidence

- SHA-256:
  `CE196C06379490BBA22D0ACD6300F53A4EA3B6353F0C7A4F715D649B59C514ED`.
- Package audit found all required runtime items; staged FFmpeg, FFprobe, and
  yt-dlp launch checks passed.
- The packaged EXE passed the offscreen QML smoke with exit `0`.

### Boundary

No visible GUI or live Colab worker was started. `graphify update .` was
attempted but the Graphify CLI is unavailable on PATH.

## 2026-08-14 — completed: package 0.0.6.9

### Delivered

- Replaced the media-download Colab worker route with a local CPU downloader.
  Download media now uses SHA-256-pinned `yt-dlp`; it does not ask for Colab
  URL/token, GPU, or API Gateway credentials.
- Accepted full public-share text and extracts its HTTPS media URL. A Netscape
  cookie file remains optional; LA Studio does not read browser cookies.
- Kept Colab only on actual model/GPU tasks. For media manually made in Colab,
  the UI tells the user to open the Colab **Files** sidebar, download the exact
  output printed by the notebook final cell, then select that local file.
- Added Dubbing project **New**, **Open**, **Save**, and **Save As**. Save As is
  atomic and reopening a `.ladub.json` restores project state for continued
  work, including recovery guidance for an interrupted workflow.

### Evidence

- Full CTest: **39/39 passed, 0 failed**, rebuilt with source version `0.0.6.9`.
- Generated exact-model notebooks: **32/32 passed**.
- Direct-Colab contract runner: **9 capability paths passed**.
- `graphify update .`, Python compilation, and `git diff --check`: passed.
- Internal portable package:
  `C:\Users\Nguyen Trong Khoi\Downloads\LA-STUDIO\out\LA-Studio-0.0.6.9\LA-Studio-0.0.6.9.exe`.
  FileVersion/ProductVersion are `0.0.6.9`; SHA-256 is
  `CDC449056C120B6F00CE562C24F163A44E49C1A88E1229438E5C37BF16B62361`.
  Required Qt, runtime-host, FFmpeg/FFprobe, `yt-dlp 2026.07.04`, and license
  artifacts were verified in the staged package.

### Boundary

No desktop GUI or live Colab session was opened. The successful tests prove
source, controller, offscreen-QML, generated-notebook, contract, and staged
runtime behavior. A user must still run a fresh Colab notebook to accept a
live temporary GPU worker. The package is internal-only because its verified
eSpeak payload is unsigned.

## 2026-08-15 - completed: project-first gate and package 0.0.7.0

### Delivered

- Added a global New/Open project gate before operational feature routes,
  including Dubbing. Settings and model browsing remain available before a
  project. Importing media through the controller now requires an existing
  project instead of creating one implicitly.
- Added explicit model chooser actions for local apply, independent Colab
  selection, and non-destructive close.

### Evidence

- Targeted Dubbing project regression: **98 passed, 0 failed, 5 skipped**.
- Offscreen QML route smoke: **3/3 passed**; full CTest: **39/39 passed**.
- QML lint exited 0 with existing warnings; `git diff --check` and
  `graphify update .` passed.
- Portable internal EXE:
  `C:\Users\Nguyen Trong Khoi\Downloads\LA-STUDIO\out\LA-Studio-0.0.7.0\LA-Studio-0.0.7.0.exe`
  (File/Product Version `0.0.7.0`, SHA-256
  `5B3B7876C80EC473C89A7ECC96E793545EEA3AAB19DAF6B98E5558C7EF88814E`).
  The staged package contains Qt/offscreen, runtime host, FFmpeg/FFprobe,
  yt-dlp, Tesseract, and the isolated PaddleOCR runtime. Package offscreen
  smoke exited 0.

### Boundary

Source commit `339cfa4` was pushed directly to `origin/main`. No visible GUI or
live Colab worker was used. This is an internal package only because the
hash-verified eSpeak payload is unsigned.

## 2026-08-15 - Dubbing OCR/Colab controls and per-task artifact handoff

### Delivered

- Transcribe/STT now exposes `STT`, `OCR`, and `STT + OCR`, an explicit Subtitle
  OCR model selector, a Local CPU/Colab GPU route, the exact notebook/model
  identity, and a **Set up OCR Colab** action in the Dubbing task controls and
  review panel. The selection is persisted through the workflow configuration.
- When `STT + OCR` is selected, the UI explains the source-language AI
  reconciliation stage before Translate. The request is gated until unresolved
  conflicts exist; review/accept/reject remains authoritative. Translate is
  documented as consuming the reviewed source transcript and its selected model.
- Added strict per-task output upload for isolation, STT/OCR, translation, TTS,
  mix, and export. Each task states its exact Colab folder and worker path,
  validates extensions and counts (isolation requires `vocals.wav` plus
  `background.wav`), imports subtitles/cues safely, and hands artifacts to the
  normal downstream step from the project cache.
- Wrapped the task controls in a vertical scroll area so OCR model/route and AI
  controls remain reachable in compact Dubbing layouts.

### Evidence

- Build succeeded: `out/build/windows-msvc-release/LA-Studio-0.0.7.0.exe`.
- Full CTest: **39/39 passed, 0 failed** (55.12 s), including strict artifact
  contract and Dubbing QML source regressions.
- QML parser/build validation passed. `qmllint` reports only the pre-existing
  unused-import and callback-property warnings; no Dubbing syntax error remains.
- `graphify update .` and `git diff --check` passed. No desktop GUI or live
  Colab worker was opened; evidence covers source/controller contracts,
  offscreen QML, build, and test fixtures.

### Boundary

No EXE package was created for this change because this request asked for the
Dubbing/OCR implementation and verification, not a new release package.

## 2026-08-15 - Per-task Colab upload visibility fix

- The per-task Colab artifact panel was moved from the narrow left task shelf
  into the active task review pane. It is now visible beside the result for
  Isolator, STT/Translate, TTS, Mix, and Export instead of requiring the user
  to discover a secondary scroll position.
- The Isolator panel accepts exactly `vocals.wav` and `background.wav`; the
  upload button remains disabled while the task is processing.
- Build and full CTest were rerun after the QML change: **39/39 passed**.

## 2026-08-15 - completed: direct upload for all Dubbing task outputs

### Delivered

- Added **Upload completed output** directly to Task Controls for every
  Dubbing result-producing task. It no longer requires the user to open
  **Show task result**, and it is not limited to voice isolation.
- The upload dialog lists the exact Colab folder, filename(s), and permitted
  format before opening the file picker. The accepted handoffs are Normalize
  (`normalized.wav`), Isolator (`vocals.wav` + `background.wav`), STT/OCR
  (`transcript.srt`/`ocr.srt`), review (`reviewed-transcript.srt`), Translate
  (`translated.srt`), TTS (`voice.wav`), Alignment (`timed-voice.wav`), Mix
  (`mix.wav`), and Export (`final.mp4`).
- Controller import follows the actual Dubbing state machine: files are placed
  in the active project cache and become the working audio, cue list, dubbed
  voice, preview, or final export consumed by the next real step. Names,
  extension allow-lists, cue validity, and the two-stem Isolator pair are
  validated before state advances.

### Evidence

- Rebuilt the MSVC test target with Qt 6.9.3 and ran full CTest:
  **39/39 passed, 0 failed**. `TestDubbingProject` covers each direct stage
  contract and verifies the hand-uploaded stereo voice bed passes through the
  real mixer as mono output.
- QML compilation/cache generation succeeded; QML lint exited 0 and reported
  only pre-existing unused-import/callback-property warnings. `git diff
  --check` and `graphify update .` were run.

### Boundary

No visible desktop GUI or live Colab worker was opened. This establishes the
controller/QML and artifact-handoff contract, but a user still needs to test
their own temporary Colab URL and files. No EXE was packaged for this
source-only request.

## 2026-08-15 - completed: internal package 0.0.7.1

### Delivered

- Packaged the current `main` source as the portable internal EXE
  `out/LA-Studio-0.0.7.1/LA-Studio-0.0.7.1.exe`.
- Source `LASTUDIO_VERSION`, FileVersion and ProductVersion were all verified
  as `0.0.7.1`.

### Evidence

- EXE SHA-256:
  `435BA385480DB098D3CCFB1BA7AEBDB0DB188C34C4B76C5E787C71D3EF455DDE`.
- Runtime staging inventory: **19/19** required artifacts, including Qt
  `qwindows`/`qoffscreen`, Runtime Host, FFmpeg/FFprobe, yt-dlp, Subtitle OCR,
  eSpeak and notices/licenses. Staged FFmpeg, FFprobe and yt-dlp launch checks
  passed.
- Shipped EXE headless QML smoke passed with exit `0` and generated a
  **19-action** Dubbing trace. The preceding source batch had full CTest
  **39/39 pass** and QML build/lint validation.

### Boundary

No visible desktop GUI or live Colab worker was started. The build is internal
only because its hash-verified eSpeak payload is unsigned.

## 2026-08-15 — completed: generalized Dubbing manual Colab handoff

### Delivered

- Applied the same handoff rule to every Dubbing task that produces an output,
  not just Isolator. Automatic worker transfer remains normal; a file selection
  is harmless until **Use uploaded output and continue** confirms it.
- Confirming valid output during the matching active transfer stops only that
  transfer and advances the next Dubbing task. A different running task cannot
  be cancelled by the current panel.
- Isolator has distinct required `vocals.wav` and `background.wav` inputs and
  documents `/content/la-studio-separation-jobs/<model-id>/<job-id>/`.
  `source.wav` is input-only. Other stages validate their own normalized WAV,
  subtitle/cue, TTS/timing/mix WAV, or final export contracts.
- Repaired the CTest OCR frame-runtime fixture by using .NET SHA-256 instead of
  an auto-loaded PowerShell `Get-FileHash` cmdlet.

### Evidence

- Targeted `TestDubbingProject`: **1/1 passed**.
- Full CTest: **39/39 passed** in **57.70 s**.
- QML lint exit 0 with only existing warnings; `git diff --check` passed.
- `graphify update .` was attempted but Graphify is not installed/on PATH, so
  no graph update is claimed.

### Boundary

No visible GUI or live Colab worker was launched. This is controller/QML/test
evidence; no EXE was packaged for the current source-only batch.

## 2026-08-22 — implemented: opt-in Unified Dubbing Colab connection contract

### Delivered

- Added an opt-in **Unified Dubbing Colab** card in Direct Colab setup. A user
  may enter one coordinator URL and one temporary bearer token once.
- The controller expands that URL into a separate verified route for every
  Dubbing stage currently selected as **Direct Colab**:
  `/v1/unified/<capability>/<model>`. Each stage still receives its exact
  capability/model handshake.
- Existing individual Direct Colab workers, API Gateway routes, and Local
  routes are unchanged. API or Local stages are deliberately excluded from the
  unified connection transaction.
- If any stage cannot start verification, the transaction rolls back every
  earlier stage and clears its pending state rather than leaving a partially
  connected workflow.

### Evidence

- `unifiedDubbingColabIsOptInAndKeepsIndependentRoutes` ran against a real
  loopback HTTP coordinator fixture: it verified distinct STT and Translation
  unified paths and confirmed a TTS API route was untouched. Exit code: `0`.
- Rebuilt the production `LAStudio` MSVC target successfully (exit code `0`).
- `git diff --check` passed and `graphify update .` was run.

### Boundary

This change intentionally does **not** pretend that the existing per-model
notebooks are one combined worker. They are separate exact-model runtimes,
including incompatible Torch/Paddle dependencies. The new app option is ready
for a genuine coordinator that launches/proxies those isolated workers under
the documented paths; such a notebook must be implemented and live-tested
before this option can be advertised as a replacement for the individual
notebooks.
# Update — 2026-08-22: unified Dubbing Colab notebook now exists

The optional Unified Dubbing route is no longer app-only. The repository now contains `notebooks/LA_STUDIO_UNIFIED_DUBBING_GPU.ipynb`, generated from `scripts/generate_unified_dubbing_colab_notebook.py`, plus its embedded coordinator source under `notebooks/workers/LA_STUDIO_UNIFIED_DUBBING_COORDINATOR.py`.

It starts selected exact CUDA workers privately, verifies each actual authenticated `/health` result, and only then provides one Cloudflare URL/token. The coordinator proxies `/v1/unified/<capability>/<model>/<worker-route>` to the matching worker. It fails on an unavailable/unhealthy/mismatched worker rather than declaring a synthetic ready state. The default notebook configuration covers Spleeter, Whisper, PP-OCRv5, M2M100, Kokoro, and MMS alignment; change `UNIFIED_WORKERS` to match a different selected Dubbing model.

Local validation passed: notebook regeneration and contract verification, Python compilation, and `TestDubbingProject` (1/1). This does not assert an unperformed live Colab GPU run. Detailed operational notes: `docs/UNIFIED_DUBBING_COLAB_COORDINATOR_2026-08-22.md`.

## 2026-08-23 — completed: source-video picker recovery and package 0.0.7.7

### Root cause and repair

The Dubbing source picker used the native Windows `FileDialog`. In the
packaged build this could appear as a detached Explorer window and never
deliver the accepted file to the QML handler. Both the single-file picker and
the media-library multi-file picker now force Qt's in-application dialog.

The same path had a second state error: after a one-item Import/Normalize run,
the controller restored the original blank project rather than retaining the
newly imported project. The controller now promotes exactly one completed
imported item to the active project and persists it; multi-item library runs
remain unchanged.

### Evidence

- Added a regression that generates an actual WAV file, imports it via the
  queue, and checks the active source path and saved project path.
- Full CTest: **39/39 passed**; `git diff --check`: passed; Graphify updated.
- Built and staged
  `out/LA-Studio-0.0.7.7/LA-Studio-0.0.7.7.exe`.
- File/product version: `0.0.7.7`.
- SHA-256: `4316C09BAA2F21BA6555B824568532BAC9254B5B7572D309ECB8239B73DCE58B`.

### Manual check left intentionally explicit

Open the new executable, create/open a project, press **Open video** or
**Browse**, select `1.mp4`, then confirm the preview/source path updates. The
picker must stay inside LA Studio rather than opening a detached Explorer
window. No GUI was controlled by the agent while producing this batch.

## Update — 2026-08-23: 0.0.7.8 restores the Windows file chooser

The 0.0.7.7 fallback-picker decision was wrong for the reported UI: forcing
`FileDialog.DontUseNativeDialog` created the black Qt file dialog. The
single-file and multi-file Dubbing pickers now use the normal native Windows
chooser again; the existing accepted-file import handlers are retained.

Targeted media-ingest regression passed, full CTest **39/39 passed** in
**63.33 s**, `git diff --check` passed, and Graphify was updated. The internal
portable artifact is `out/LA-Studio-0.0.7.8/LA-Studio-0.0.7.8.exe`, with
file/product version `0.0.7.8` and SHA-256
`510FE11BE60AB581CA0BEB89A77C4549D96D5F1274C06F7D20234B30A189A69B`.

The remaining manual visual check is simple: **Open video** or **Browse** must
open the expected native Windows file chooser rather than the dark Qt dialog.

## Update — 2026-08-24: Clone Voice reuse recheck in standalone TTS and Dubbing

### Fixed

- Saved voice profiles are no longer tied to OmniVoice or silently treated as
  ordinary TTS selections. The exact Voice Cloning worker family saved with
  the profile is used for clone synthesis; normal TTS stays independent.
- Connecting a clone worker no longer changes the TTS Colab model or selected
  provider. The user must select a saved clone in TTS, and reconfirm
  permission for that selected durable profile.
- Dubbing now persists the clone model separately from the normal TTS model,
  rejects unsupported/missing exact clone workers, and prevents stale legacy
  clone data from overriding a current saved-voice selection.
- Selecting a built-in TTS voice clears clone-only state before a Dubbing job
  starts. Profile reuse is keyed by the selected saved-voice ID, reference,
  language, and clone model, avoiding accidental reuse across different
  library entries.

### Verification

- Focused CTest: `TestDubbingProject`, `TestRemoteExecution`, and
  `TestColabVoiceCloneRunner` — **3/3 passed**.
- Full CTest in `out/build/windows-msvc-tests-tts-recheck` — **39/39 passed**.
- `git diff --check` passed and Graphify was updated after source edits.

### Limits of this batch

No visible owner GUI, browser, or live Colab worker was controlled. Therefore
the suite validates application/controller/loopback worker contracts, not a
new live-Colab acceptance claim. No EXE was packaged in this recheck batch.
