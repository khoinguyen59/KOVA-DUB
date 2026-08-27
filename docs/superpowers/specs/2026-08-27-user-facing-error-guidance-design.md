# User-Facing Error Guidance Design

## Goal

Giữ nguyên thông báo kỹ thuật trong log, nhưng mọi lỗi đi qua `AppController` phải
có phần trình bày thân thiện trên màn hình: lỗi gì, cần làm gì, nút hành động nào
có thể đưa người dùng đến đúng màn hình cấu hình hoặc retry.

Ảnh lỗi của Task Separate là hiện tượng mẫu: `DubbingJobRunner` phát raw message,
`DubbingController` expose `lastError`, `AppController` enqueue chuỗi đó và
`Main.qml` chỉ render chuỗi raw trong một popup cố định.

## Scope

- Global error queue và popup trong `AppController`/`Main.qml`.
- Error catalog thuần C++ để phân loại theo `source` + technical message.
- Dubbing, separation, STT, TTS, alignment, translation, OCR, API, catalog và
  filesystem/network error guidance.
- CTA route để mở đúng studio; CTA support để copy technical details và tạo
  problem report.
- Inline Dubbing error surface dùng cùng presentation, không lặp lại raw message
  khi đã có guidance.
- Unit/source-contract/QML smoke tests cho classification, queue retention và
  action routing.

Không đổi business policy của workflow, không fallback silent, không đưa secret
hoặc bearer token vào UI/log details.

## Architecture

`AppErrorCatalog` là pure helper, nhận `(technicalMessage, source)` và trả về
`AppErrorPresentation`. `AppController::enqueueError()` giữ raw message ở
`technicalDetails`, đồng thời lưu các field trình bày trong từng notification.
`errorMessage` cũ tiếp tục tồn tại để không phá API QML hiện hữu; popup mới dùng
notification đầu tiên trong queue.

Các field public của notification:

| Field | Meaning |
|---|---|
| `id` | queue identity |
| `code` | stable diagnostic classification |
| `severity` | `error` hoặc `warning` |
| `source` | subsystem emitting the error |
| `title` | short Vietnamese title |
| `summary` | one-sentence user explanation |
| `guidance` | newline-separated next steps |
| `actionId` | stable UI action, if available |
| `actionLabel` | localized CTA text |
| `actionRoute` | validated studio route, if available |
| `message` | compatibility copy of the technical message |
| `technicalDetails` | raw message for copy/support |
| `timestamp` | UTC ISO timestamp |

The catalog does not execute actions. `Main.qml` validates `actionRoute` through
the existing `requestStudioRoute()` boundary, while `AppController` handles
copy/report operations. This prevents arbitrary route strings from becoming a
navigation injection surface.

## UX behavior

- Modal dialog is bounded by the window with `ScrollView`, `wrapMode` and no fixed
  400px height.
- Primary area shows title, summary and numbered guidance.
- Technical details are collapsed by default and can be copied.
- CTA is optional and disabled while the error queue is empty.
- `Dismiss` advances the queue exactly once.
- Multiple errors show `Error (N pending)` without replacing the first error.
- The screenshot case renders a Dubbing/separation title and offers opening the
  Dubbing configuration; it does not suggest using normalized mixed audio as a
  fake separation result.

## Error classification policy

1. Source-specific rules run before generic keyword rules.
2. Known capability/runtime errors map to the matching studio route.
3. Colab/auth/network errors explain connection/token/worker checks without
   exposing credentials.
4. File/artifact errors explain browse/retry/recreate steps.
5. Unknown errors remain visible with generic support guidance and full raw detail.

## Verification

- Unit test proves the separation runtime message maps to a Dubbing title,
  actionable guidance, and `studio-dubbing` route.
- Unit test proves raw technical detail remains in the queued notification.
- Unit test proves dismissing a queued error reveals the next notification.
- QML route smoke proves the dialog renders and the route CTA reaches the existing
  navigation boundary.
- QML lint and the focused `TestAppErrorCatalog` test pass after implementation.
  The focused test uses a test-only CMake option to skip the application-target
  dependency, so no release/application EXE is built or packaged for this change.
