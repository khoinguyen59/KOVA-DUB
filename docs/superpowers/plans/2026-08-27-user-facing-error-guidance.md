# User-Facing Error Guidance Implementation Plan

> **Execution record:** This plan was implemented in the current workspace. The checkboxes below record the completed implementation and verification work.

**Goal:** Giữ technical error trong log nhưng hiển thị guidance và CTA có thể thực hiện được cho người dùng trên mọi lỗi global, ưu tiên đầy đủ Dubbing workflow.

**Architecture:** Thêm `AppErrorCatalog` thuần C++ để phân loại `(message, source)` thành presentation fields. `AppController` lưu presentation cùng raw `technicalDetails`; `Main.qml` dùng notification đầu queue cho dialog scroll-safe, route CTA và copy/report. Các `lastError` cũ vẫn giữ để không phá controller API.

**Tech Stack:** C++17, Qt 6 `QObject`/`QVariantMap`, Qt Quick Controls 2, QML `Popup`/`ScrollView`, Qt Test, existing CMake source lists.

**Spec:** `docs/superpowers/specs/2026-08-27-user-facing-error-guidance-design.md`

## Global Constraints

- Technical message phải tiếp tục được lưu nguyên văn trong `technicalDetails` và log.
- UI guidance mặc định dùng tiếng Việt, không hiển thị secret/token.
- Không thay đổi workflow policy hoặc thêm fallback silent cho separation/runtime.
- Route CTA chỉ đi qua `requestStudioRoute()`/route registry hiện hữu.
- Dialog phải bounded theo cửa sổ, `ScrollView` + `Text.Wrap`, không height cố định.
- Không chạy `package.ps1`, không tạo hoặc thay thế release/application EXE.
- Mọi production behavior mới phải có failing test/source-contract trước khi fix.

---

### Task 1: Pure error presentation catalog

**Files:**
- Create: `src/controllers/app/AppErrorCatalog.h`
- Create: `src/controllers/app/AppErrorCatalog.cpp`
- Modify: `CMakeLists.txt` source/header lists
- Test: `tests/core/test_AppErrorCatalog.h`, `tests/core/test_AppErrorCatalog.cpp`, `tests/CMakeLists.txt`, `tests/main.cpp`

**Interfaces:**
- Consumes: raw `QString technicalMessage` and optional `QString source`.
- Produces: `AppErrorPresentation classifyAppError(const QString &, const QString &)` and `QVariantMap AppErrorPresentation::toVariantMap() const`.

- [x] **Step 1: Write the failing tests**

Add tests for the screenshot case, a generic unknown error, and source-preserving
variant conversion. The separation test must assert `code`, Vietnamese `title`,
non-empty `guidance`, `actionId`, `actionLabel`, `actionRoute == "studio-dubbing"`,
and raw `technicalDetails`.

- [x] **Step 2: Run the focused test and verify it fails**

Run the existing Windows test target after adding the suite. Expected failure is
missing `AppErrorCatalog.h`/`classifyAppError`, not a test discovery failure.

- [x] **Step 3: Implement the minimal catalog**

Use source-first rules for `Dubbing`, `Voice Isolator`, `STT`, `TTS`, `Alignment`,
`Translation`, `Subtitle OCR`, `Local API Server`, `Catalog` and `Registry`.
Use generic keyword rules for Colab, runtime/model, file/artifact, network/auth,
and unknown failures. Return a safe fallback action with no route for unknown
messages. Store the raw message unchanged in `technicalDetails`.

- [x] **Step 4: Run the focused test and verify it passes**

Run the single `TestAppErrorCatalog` suite and then the existing core suites.
Expected result: all catalog assertions pass and raw English technical text is
still present in the variant map.

- [x] **Step 5: Refactor only after green**

Keep matching helpers private to the catalog and remove duplicated keyword checks;
do not move navigation or logging into the catalog.

### Task 2: AppController structured error queue

**Files:**
- Modify: `src/controllers/app/AppController.h:122-218,284-286`
- Modify: `src/controllers/app/AppController.cpp:238-282`
- Test: `tests/core/test_AppErrorCatalog.h`, `tests/core/test_AppErrorCatalog.cpp`, `tests/CMakeLists.txt`, `tests/main.cpp`

**Interfaces:**
- Consumes: existing `onError()`/`enqueueError()` signals and `AppErrorPresentation`.
- Produces: `Q_PROPERTY(QVariantMap currentError READ currentError NOTIFY errorMessageChanged)` and `QVariantMap currentError() const`. Copy/report remain the existing `AppController` operations; route navigation remains a QML signal/request and is validated by the existing route registry.

- [x] **Step 1: Write the failing structured-error tests**

Add a focused catalog test for the screenshot separation failure, a generic raw
message retention test, and a source-contract test covering `currentError`, the
structured enqueue path, centralized raw logging, the dialog and route registry.

- [x] **Step 2: Run tests to verify the contract fails**

Run the focused test before implementation. The expected failure was the missing
catalog header/API, not test discovery failure.

- [x] **Step 3: Implement structured enqueue**

Call `classifyAppError` once per notification, merge its map with queue id and
timestamp, retain `message` as compatibility raw text, and set `m_errorMessage`
from the first notification's raw message exactly as before. `clearError()` must
advance the queue and emit both existing change signals.

- [x] **Step 4: Verify structured notification behavior**

Run `TestAppErrorCatalog` and verify the structured map keeps both compatibility
`message` and raw `technicalDetails`; the existing queue and duplicate suppression
continue to compare raw message and source.

### Task 3: Reusable Error Guidance dialog and route CTA

**Files:**
- Create: `qml/components/shared/ErrorGuidanceDialog.qml`
- Create: `qml/components/shared/ErrorGuidanceInline.qml`
- Modify: `CMakeLists.txt` QML file list
- Modify: `qml/Main.qml:135-186,196-201`
- Test: `tests/core/test_AppErrorCatalog.cpp` source-contract assertions

**Interfaces:**
- Consumes: `AppController.currentError`, `AppController.pendingErrorCount`,
  `AppController.clearError()`, `AppController.copyToClipboard()`,
  `AppController.createProblemReport()`, and `root.requestStudioRoute()`.
- Produces: dialog signal `actionRequested(string route)` with accessible labels
  and bounded content. Dismiss, copy and support-report actions are handled by the
  dialog through the existing `AppController` methods.

- [x] **Step 1: Add the QML source-contract assertions**

Add source-contract assertions to assert that the error dialog uses
`currentError.title`, `currentError.guidance`, `currentError.technicalDetails`,
contains a `ScrollView`, and invokes `requestStudioRoute` only with the presented
`actionRoute`.

- [x] **Step 2: Run the contract test before implementation**

Run the focused catalog/contract test and verify the new contract fails because
the component/property does not exist yet.

- [x] **Step 3: Implement the dialog**

Use a modal `Dialog` bounded by the window and
`Math.min(parent.height - 48, ...)`, a `ScrollView` for summary/guidance/details,
and `Text.Wrap`. Show CTA only when `actionRoute` and `actionLabel` are non-empty;
emit `actionRequested(route)` and let `Main.qml` call the validated
`root.requestStudioRoute(route)` boundary. Keep raw
technical details collapsed and provide copy/report actions.

- [x] **Step 4: Verify QML route and lint**

Run `scripts/lint_qml.ps1` and the focused source-contract test. Confirm no raw
message is the only visible content in the dialog and that `Dismiss` advances the
queue.

### Task 4: Dubbing inline guidance and error action coverage

**Files:**
- Modify: `qml/components/dubbing/DubbingProjectStatusPanel.qml`
- Modify: `qml/components/voiceisolator/VoiceIsolatorStudioView.qml`
- Modify: `qml/components/dubbing/DubbingMediaQueueDialog.qml`
- Modify: `qml/components/dubbing/ColabMediaAcquisitionPanel.qml`
- Modify: `qml/components/dubbing/DubbingArtifactUploadPanel.qml`
- Test: focused source-contract coverage in `tests/core/test_AppErrorCatalog.cpp`

**Interfaces:**
- Consumes: existing controller `lastError` and global `AppController.currentError`.
- Produces: inline status text using the same friendly summary/guidance where the
  owner is Dubbing/separation, while preserving raw text in the expandable global
  dialog.

- [x] **Step 1: Add the inline contract coverage**

Review the Dubbing status surfaces against the shared `ErrorGuidanceInline`
component and ensure they no longer render `lastError` as the only body copy in
the user-facing error state.

- [x] **Step 2: Verify the previous raw surfaces**

Record the previous raw `text: root.dubbing.lastError` bindings as the reason for
the inline presentation change.

- [x] **Step 3: Implement the shared inline presentation**

Add a compact non-modal error card with title/summary, wrap-safe guidance, and a
single CTA when the current error belongs to the same source. Avoid duplicating a
global error modal for unrelated subsystem errors.

- [x] **Step 4: Verify Dubbing surfaces**

Run QML lint and the focused source-contract test; ensure the screenshot scenario
shows actionable model/Colab guidance and never proposes normalized audio as a
stem.

### Task 5: Documentation and final verification

**Files:**
- Modify: `recheck.md`
- Modify: `doc/DUBBING_FIX_PROPOSAL.md`
- Modify: all 16 files under `doc/front/` and `doc/back/` only where the current
  error presentation contract is described

- [x] **Step 1: Update docs from actual code**

Document structured notification fields, raw log retention, route CTA validation,
queue semantics and the exact separation error guidance.

- [x] **Step 2: Run focused verification without packaging an EXE**

Run the focused/new test, QML lint and static source checks. The full CTest target
was intentionally not run because its default dependency builds the application
target, which is outside this request. Do not run `package.ps1` or produce a
release/application executable.

- [x] **Step 3: Final audit**

Confirm every documented property, signal, action id and QML component exists in
source, and report any external runtime limitation separately from UI guidance.

### Execution verification

- `TestAppErrorCatalog`: 1/1 focused test passed using the test-only configuration
  that skips the application-target dependency.
- `scripts/lint_qml.ps1`: exited 0 with no diagnostics.
- No release/application EXE was built or packaged. The default CMake dependency
  remains unchanged; the skip option is only for focused test verification.
