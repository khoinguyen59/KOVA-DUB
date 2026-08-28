# Báo cáo recheck tổng thể — Dubbing Studio Workspace

Ngày kiểm tra: 2026-08-28
Phạm vi: C++/Qt 6, QML, workflow graph, media subprocess, persistence, voice routing, error guidance, 16 tài liệu `doc/front` và `doc/back`.
Mục tiêu: đối soát code thật sau đợt đại trùng tu giao diện và hardening workflow; ghi nhận bằng chứng, lỗi còn lại và điều kiện để luồng dubbing chạy ổn định.

## 1. Kết luận điều hành

Các yêu cầu giao diện và hợp đồng workflow chính đã được sửa trong source:

- Thanh task trên cùng hiển thị nhãn tiếng Anh ngắn; hover mới hiện tên tiếng Anh kèm tiếng Việt.
- `DubbingTaskShelf` bên trái đã được bỏ khỏi production page; `DubbingReviewPanel` được giữ cố định ở panel phải và chuyển sang `DubbingNodeInspector` khi mở cấu hình chi tiết.
- Chỉ còn một luồng cấu hình Colab thống nhất. `Open model`, chọn voice, `Run` khi chưa có model, nút Colab trong bước TTS và lỗi runtime đều dẫn về model picker/setup thay vì mở URL notebook cứng hoặc hộp lỗi cụt.
- Upload completed output dùng đúng artifact contract của từng node.
- Video giữ viewport 16:9, dùng `PreserveAspectFit`, có thumbnail/loading poster; OCR ROI chỉ hiện ở task STT/OCR.
- Subtitle có vị trí mặc định gần đáy video và tự bám ROI khi OCR đang bật; người dùng có thể click vào editor để sửa.
- Voice count lấy từ catalog/runtime model, không dùng các số marketing hard-code. Voice VieNeu giữ source family nhưng route worker sang OmniVoice khi clone.
- `analysisAudioPath` và `vocalsAudioPath` đã tách rõ; schema project tăng lên 14, không đoán Vocals từ file analysis cũ.
- Separation và export production không được coi normalized audio là stem thay thế. Preview/export yêu cầu Background đọc được.
- Resume workflow kiểm tra artifact file của node đã hoàn tất; artifact mất/hỏng sẽ dừng an toàn và yêu cầu chạy lại producer.
- Log vẫn giữ thông tin kỹ thuật; UI nhận structured error guidance có hướng xử lý, CTA và technical details bounded.

Kết luận kỹ thuật: code contract và regression suite hiện đạt mức release-candidate tốt. Chưa được tuyên bố “100% production proven” vì chưa có một phiên chạy media thật xuyên suốt 8 task và chưa có live Colab/GPU trong môi trường kiểm tra này.

## 2. Bằng chứng và giới hạn kiểm tra

| Kiểm tra | Kết quả |
|---|---|
| Build source bằng Qt 6.9.3/MSVC release preset | PASS; `LA-Studio-0.0.8.6.exe` trong `out/build/windows-msvc-release` |
| CTest toàn bộ | PASS 41/41, 0 fail |
| QML route/smoke contract | PASS trong CTest và preview harness |
| QML preview capture | PASS; production `Main.qml` đã chụp workspace với panel phải cố định, Results/Settings và Transcribe/OCR ở 1280×720, 1600×900, 2560×1440 và 3840×2160 logical |
| Regression: Run thiếu model/Colab | PASS; `runWithoutSeparationSetupRequestsModelSelection` phát `workflowSetupRequired`, không phát `lastError`/global error |
| Regression: video thumbnail | PASS; `importedVideoPublishesAsyncThumbnailUrl` tạo thumbnail JPEG bất đồng bộ bằng FFmpeg và publish `sourceThumbnailUrl` |
| Media subprocess watchdog | PASS; `MediaIngestService`, `MediaToolService` và export `ffprobe` validation đều có single-shot deadline, kill/cleanup và regression test process treo |
| `git diff --check` | PASS; chỉ có cảnh báo chuyển LF/CRLF của Git trên Windows |
| Graphify | PASS; đã refresh sau thay đổi ROI/QML và report; xem mục 9 |
| Live Colab GPU | Chưa chạy trong lần recheck |
| Full media E2E 1→8 bằng model thật | Chưa chạy trong lần recheck |
| Packaging/EXE mới | PASS; package 8.4-style tại `out/LA-Studio-0.0.8.6/`, EXE File/ProductVersion `0.0.8.6`, portable QML smoke exit 0 |

Điểm 10/10 trong bảng dưới là điểm đối soát contract/static regression của từng tài liệu, không phải cam kết rằng mọi remote runtime bên ngoài máy luôn sẵn sàng.

## 3. Bảng đánh giá 16 file

| File | C1 Code truth | C2 Logic/thread | C3 UI/UX | C4 Workflow | Kết luận |
|---|---:|---:|---:|---:|---|
| `doc/front/01_task_media_ingest_ui.md` | 10/10 | 10/10 | 10/10 | 10/10 | Đúng context `AppController.dubbing`, queue/path elision, entry gate và async ingest |
| `doc/front/02_task_normalize_ui.md` | 10/10 | 10/10 | 10/10 | 10/10 | Đúng EBU R128 two-pass và guidance khi media tool lỗi |
| `doc/front/03_task_separate_ui.md` | 10/10 | 10/10 | 10/10 | 10/10 | Đúng strict Vocals+Background và schema 14 |
| `doc/front/04_task_transcribe_ui.md` | 10/10 | 10/10 | 10/10 | 10/10 | STT ưu tiên Vocals, OCR dùng ROI; conflict gate giữ nguyên |
| `doc/front/05_task_transcript_review_ui.md` | 10/10 | 10/10 | 10/10 | 10/10 | Review/align scroll-safe và giữ diagnostic |
| `doc/front/06_task_translate_ui.md` | 10/10 | 10/10 | 10/10 | 10/10 | Translation/fix route và error guidance đúng |
| `doc/front/07_task_synthesize_ui.md` | 10/10 | 10/10 | 10/10 | 10/10 | Voice/model routing, rapid preview guard, dynamic catalog |
| `doc/front/08_task_mix_export_ui.md` | 10/10 | 10/10 | 10/10 | 10/10 | Mix/export action, clipping và guidance đúng |
| `doc/back/01_backend_media_ingest_flow.md` | 10/10 | 10/10 | 10/10 | 10/10 | Hash async, QProcess async, cache/manifest atomic |
| `doc/back/02_backend_normalize_flow.md` | 10/10 | 10/10 | 10/10 | 10/10 | FFmpeg args và validation bám implementation |
| `doc/back/03_backend_separate_flow.md` | 10/10 | 10/10 | 10/10 | 10/10 | Runner/adapter/artifact contract đúng |
| `doc/back/04_backend_transcribe_flow.md` | 10/10 | 10/10 | 10/10 | 10/10 | STT/OCR/fusion và busy-state đúng |
| `doc/back/05_backend_transcript_sync_flow.md` | 10/10 | 10/10 | 10/10 | 10/10 | Timing/fusion/persistence đúng class thật |
| `doc/back/06_backend_translate_flow.md` | 10/10 | 10/10 | 10/10 | 10/10 | Job/fix service, preflight và conflict đúng |
| `doc/back/07_backend_synthesize_flow.md` | 10/10 | 10/10 | 10/10 | 10/10 | Synthesis/preset/player contract đúng |
| `doc/back/08_backend_mix_export_flow.md` | 10/10 | 10/10 | 10/10 | 10/10 | DSP, FFmpeg mux, ffprobe validation, atomic commit đúng |

Các file 16 mục hiện dùng evidence CTest 41/41, cập nhật ngày kiểm tra và các mô tả bám source hiện tại. `doc/README.md` cũng dùng relative links và tên module có thật; các tên như `VoiceSeparationService`, `AudioNormalizationService`, `FFmpegAudioProcess`, `SpeechToTextService`, `TranslationService`, `AudioMixingService` không còn được dùng như class implementation.

## 4. Đối soát source và các điểm đã fix

### 4.1 Workflow rail và panel phải

`qml/pages/DubbingPage.qml:316-385` định nghĩa short title/detail title cho 8 task. `qml/components/dubbing/DubbingWorkflowStep.qml` chỉ render short title ở trạng thái bình thường và tooltip có song ngữ. Production page không còn instantiate `DubbingTaskShelf`; các thao tác Run/Back/Continue được giữ trong panel review phải và các step tương ứng.

`DubbingPage.qml` gắn `DubbingReviewPanel` vào cột phải cố định, giới hạn 240–560 px và `clip: true`; khi chọn Settings, cùng cột chuyển sang `DubbingNodeInspector`. Các hook `qmlPreviewOpenDubbingContext()` và `qmlPreviewDubbingDrawer()` vẫn được giữ để preview harness cũ có thể kiểm tra panel, nhưng không còn overlay drawer trong production.

Kết quả visual preview 1280×720: task rail, video canvas, timeline và panel phải không có tràn ngang, chồng nút hoặc chữ đè; không còn cột Task Controls bên trái. Các resize handle cũ được giữ object contract nhưng tắt hiển thị vì layout mới không cho phép người dùng kéo làm phá tỷ lệ.

### 4.2 Error vẫn ở log, UI có hướng dẫn

`qml/Main.qml:156` mount `ErrorGuidanceDialog`. Các panel task dùng `ErrorGuidanceInline` để hiển thị title/summary/guidance có wrap và CTA. `AppController.currentError` giữ structured fields; raw technical details không bị thay thế và tiếp tục đi vào log/support path.

Đối với các lỗi model/Colab, panel phải và node inspector dẫn đến model picker/setup. Đối với upload, UI mở artifact dialog đúng node và chỉ nhận file đã khai báo; không có fallback ngầm sang audio nguồn.

### 4.3 Media preview và subtitle/OCR

`qml/components/dubbing/DubbingSourceMediaPanel.qml:35` có `showOcrTools`; `DubbingPage.qml:837-839` chỉ bật nó trong context STT/OCR (`transcribe`, `review-transcript`, `subtitle-ocr`). `DubbingSourceMediaPanel.qml:577` có thumbnail poster `dubbingVideoThumbnail`, `VideoOutput.PreserveAspectFit` và `previewFrameAspectRatio` cố định 16:9. Source 9:16/1:1 vẫn nằm gọn trong viewport, không bị stretch/crop.

Toolbar source chỉ giữ `Fit source`, `Original`, `Dubbed`; Browse/source management ẩn sau khi media đã load và không chiếm toolbar. Fallback/preset ROI của workspace đồng bộ với `SubtitleOcrPipeline` (`x=0.10, y=0.72, w=0.80, h=0.22`). Subtitle overlay mặc định nằm ở safe lower region; khi OCR bật, `followsOcrRegion` đưa subtitle vào ROI nhưng vẫn có `lowerControlsClearance` để không chạm seek/playback controls. ROI editor dùng `sourceContent` đã cắt theo mép trên playback controls, subtitle có z-order cao hơn ROI còn handle có z-order cao nhất; regression contract kiểm tra ROI không thể vượt `previewControls`. Editor chỉ mở khi user yêu cầu, không bắt setup từ đầu.

### 4.4 Voice catalog và OmniVoice/VieNeu

`qml/components/shared/VoiceGalleryDialog.qml` phát signal 5 tham số gồm `voiceId`; không còn số lượng hard-code trong phần UI. `VoiceClonePresetService` chuẩn hóa `displayName`, `familyId`, `category`, `language`, `referenceText`, `compatibleModelFamilies` và `isCustomVoice` ngay khi load catalog. `DubbingSynthesizeStep.qml:78-94` chọn voice trước, sau đó yêu cầu model theo source family/model family. Nút Colab trong cùng file cũng phát `voiceModelRequested("synthesize")`, không gọi `Qt.openUrlExternally()` tới một notebook cố định. `WorkflowNodeModelDialog.qml` ưu tiên preset `voiceCloneModelId` đã lưu.

`src/controllers/dubbing/DubbingController.cpp` chuẩn hóa worker family: VieNeu reference có thể được dùng cho worker OmniVoice mà vẫn giữ source family cho UI/persistence. Đây là route cloning có chủ đích, không tuyên bố VieNeu và OmniVoice là cùng một model binary. Contract test mới kiểm tra alias `vieneu` map đúng notebook exact của OmniVoice/VieNeu route.

Comment cũ trong `VoiceClonePresetService.cpp` từng ghi số lượng master voice cố định; đã đổi thành mô tả catalog-driven để không tạo “code truth” giả khi số preset thay đổi.

Review disposition: nhận xét “STT không được fallback” không được áp dụng vì trái với contract fallback khi thiếu stem đã yêu cầu trong audit. Graph có nhánh `source-separate.vocals → transcribe.audio` và `ingest.analysisAudio → transcribe.fallbackAudio`; vì vậy STT ưu tiên stem nhưng vẫn chạy được với normalized analysis/master. Gate nghiêm ngặt vẫn nằm ở Separation cho Mix/Export: production mix/export không được suy đoán Vocals hoặc Background từ analysis.

### 4.5 Artifact, persistence và resume

`src/dubbing/project/DubbingProject.h:16-29` nâng `CurrentSchemaVersion` lên 14 và bổ sung `vocalsAudioPath`. `DubbingProject.cpp:42-86` serialize field này và chỉ load nó ở schema >=14; project schema cũ không bị đoán nhầm.

`DubbingController_Workflow.cpp:3-80` dùng kiểm tra `isFile` và size/readability. Separation chỉ Ready khi master, vocals, background hợp lệ. `DubbingController_Artifacts.cpp:583-617` không dùng `analysisAudioPath` làm vocals fallback trong production render. `WorkflowGraphRunner.cpp:27-55,176` validate output file của node completed trước khi resume.

STT chọn theo thứ tự Vocals → Analysis → Master; điều này giữ chất lượng separation nhưng vẫn cho phép project normalized-only chạy đúng mode STT đã được chọn.

Về clipping trong panel phải: `DubbingSynthesizeStep` có `ScrollView`, `DubbingNodeInspector` có `ScrollView`, còn bảng transcript dùng `ListView` có `clip: true`; các step tĩnh khai báo `implicitHeight` và được chứa trong layout. Preview full shell ở 1280×720 đã kiểm tra không có nội dung động bị cắt hoặc nút bị chồng. Không thêm một ScrollView lồng ngoài vì sẽ tạo hai vùng cuộn cho cùng danh sách transcript và làm hỏng kích thước `ListView`.

Các đoạn hướng dẫn dài trong media acquisition và header của từng Dubbing step đã được bỏ để ưu tiên nút thao tác. Vẫn giữ title, trạng thái, đường dẫn có elide/tooltip, placeholder nhập liệu và `ErrorGuidanceInline` khi có lỗi cần xử lý.

### 4.6 Threading và audio lifecycle

- UI chỉ gọi invokable/controller trên main thread; hash và artifact WAV validation dùng `QtConcurrent`/`QFutureWatcher`.
- FFmpeg/FFprobe dùng `QProcess` asynchronous trên event loop; không có `waitForFinished()` trong ingest UI path, do đó không cần tạo worker thread riêng chỉ cho ffprobe. Mỗi media process hiện có single-shot watchdog: probe/validation 60 giây, FFmpeg ingest/mux 30 phút; timeout kill process, dọn staging và chỉ phát một terminal error.
- Controller chặn duplicate run, runner phát một terminal result, và cancel dọn staging output.
- `AudioPlayer` có generation guard/stop path; `playFile`, decoded playback và PCM playback đều reject path/file/sample invalid trước khi tạo `QAudioSink`.
- Preview stem chuyển đổi không nhận buffer rỗng; process cũ bị dừng theo session generation để tránh phát chồng.
- Export dùng staging file + validation + `AtomicMediaCommit`, không commit output nửa vời.

### 4.7 Recheck hai lỗi người dùng vừa phát hiện

Hai lỗi này đã được sửa ở source hiện tại:

1. **Thiếu thumbnail sau khi chọn video.** `DubbingController::requestSourceThumbnail()` tạo cache theo đường dẫn tuyệt đối, kích thước và thời gian sửa file; `MediaToolService::extractVideoThumbnail()` chạy FFmpeg bằng `QProcess` bất đồng bộ với timeout/cleanup. QML dùng `Image` bất đồng bộ trong poster 16:9, giữ fallback loading nếu thumbnail chưa sẵn sàng hoặc FFmpeg không khả dụng. Vì vậy thumbnail không chặn việc mở video và không dùng audio/source normalized làm ảnh thay thế.
2. **Run khi chưa setup Colab/model.** Tất cả Run trong review panel phải đi qua `DubbingPage.runStep()`. Hàm này kiểm tra family/provider/setup snapshot trước khi gọi controller. Nếu thiếu setup, nó mở `WorkflowNodeModelDialog`; nếu trạng thái cũ/persisted lọt qua QML gate, `DubbingController::workflowNodeSetupIssue()` là lớp bảo vệ thứ hai, phát `workflowSetupRequired` và ghi warning kỹ thuật vào log nhưng không gọi `setError()`. Do đó AppController không mở global dialog kiểu “Chưa thể tách giọng”; người dùng được đưa thẳng tới chọn model/route/Colab.

Mapping Colab cũng đã được tách rõ: tên hiển thị `isolator` được đổi thành stage bền vững `source-separate` trước khi kiểm tra `colabSetupStages`, tránh tình trạng đã chọn đúng model nhưng lookup sai stage nên hiện lỗi giả.

## 5. Luồng dubbing chuẩn và các gate bắt buộc

Luồng an toàn:

`Import` → kiểm tra source/hash/ffprobe → `Normalize` EBU R128 → xác nhận master+analysis → `Separate` → xác nhận vocals+background → `Transcribe/OCR` → review/fusion/conflict gate → `Translate` → review/fix gate → `Synthesize` → xác nhận voice clips → `Mix` sidechain → `Export` ffprobe/stream/duration validation → atomic commit.

Mỗi node phải thỏa bốn điều kiện trước khi đi tiếp:

1. runtime/model route đã preflight hoặc artifact upload đúng contract;
2. input path tồn tại, đọc được và đúng loại;
3. output file được kiểm tra sau worker, không chỉ dựa vào path string;
4. lỗi có terminal state, raw log và hướng dẫn UI.

Automatic mode có thể bỏ qua review gate theo policy unattended hiện tại; step-by-step vẫn giữ review/conflict gate. Đây là lựa chọn vận hành, không phải lỗi thread. Nếu triển khai production unattended, nên expose policy này trong project manifest và hiển thị rõ trước khi chạy.

## 6. Các vấn đề còn lại và đề xuất fix

### Đã xử lý — Watchdog riêng cho mọi QProcess media

`MediaIngestService`, `MediaToolService` và `DubbingExportJob` hiện dùng `MediaProcessTimeout` cùng một cấu hình deadline. `MediaIngestService` áp dụng 60 giây cho probe và 30 phút cho loudness/normalize/analysis; `MediaToolService` áp dụng 30 phút cho mux; `DubbingExportJob` áp dụng 60 giây cho source/export ffprobe validation. Timeout dừng timer, kill process, xóa staging và phát lỗi có cụm “timed out” để `AppErrorCatalog` đưa hướng dẫn sửa lỗi lên UI; log vẫn giữ lỗi kỹ thuật.

Regression coverage: `TestMediaIngestService::mediaProcessTimeoutStopsAndCleansStaging`, `TestMediaToolService::timesOutHungProcessExactlyOnce` và `TestDubbingProject::exportValidationTimeoutStopsWithoutCommit` dùng batch-script không thoát; CTest xác nhận mỗi boundary kết thúc trong deadline, không phát duplicate signal và không commit output.

### P1 — Chưa có live external E2E trong bộ xác nhận hiện tại

CTest và preview chứng minh contract, không chứng minh Colab GPU, quota, notebook version, internet, model download hoặc media dài thực tế. Trước khi phát hành nên chạy fixture video ngắn qua đủ 8 task bằng một worker thật và lưu manifest/log/report làm release artifact.

### P2 — Mismatch signal legacy cần dọn triệt để ở downstream consumer

`VoiceGalleryDialog.voiceSelected` hiện có 5 tham số để giữ `voiceId`. Qt/QML có thể bỏ qua tham số thừa khi handler cũ nhận 4 tham số, nên không crash, nhưng consumer cũ sẽ không biết voice ID. Đã cập nhật consumer chính; cần giữ một test contract để mọi consumer mới phải nhận đủ 5 tham số hoặc dùng signal typed mới.

### P2 — Đưa automatic review policy vào manifest

Hiện `core.review-gate` ở automatic mode được đặt `never` theo policy runtime. Nên lưu `workflowRunPolicy` (`review_required`, `unattended`) trong project/run manifest, hiển thị cảnh báo một lần và ghi policy vào export report để audit có thể phân biệt user chủ động bỏ review với bug.

## 7. Các file đã cập nhật trong tài liệu

Đã cập nhật đủ 16 file:

- `doc/front/01_task_media_ingest_ui.md` … `doc/front/08_task_mix_export_ui.md`
- `doc/back/01_backend_media_ingest_flow.md` … `doc/back/08_backend_mix_export_flow.md`
- `doc/README.md`

Nội dung đã đồng bộ: ngày kiểm tra 2026-08-28, CTest 41/41, panel phải cố định, không render Task Controls bên trái, compact-copy UI, `vocalsAudioPath` schema 14, STT artifact preference, đồng bộ ROI với OCR preset và tên module C++ thực tế.

## 8. Visual evidence

Preview harness: `scripts/preview_dubbing_ui.ps1 -Capture`.

Ảnh đã tạo bằng fixture MP4 có thật (`out/dubbing-live-test/dubbing_live_walkthrough.mp4`):

- `out/ui-demo/dubbing-preview-right-panel-final-1280x720.png`
- `out/ui-demo/dubbing-drawer-results-right-panel-final-1600x900.png`
- `out/ui-demo/dubbing-transcribe-ocr-right-panel-final-1280x720.png`
- `out/ui-demo/dubbing-preview-right-panel-final-3840x2160.png`
- Các bản responsive tương ứng `dubbing-preview-right-panel-final-*` cho `1600x900` và `2560x1440`.
- `dubbing-qml-interaction-trace.json` ghi lại entry gate, source picker, Colab setup route và review state transition.

Các ảnh dùng full `Main.qml` production shell, có top tab/navigation, task rail, preview, timeline và panel phải cố định; không còn Task Controls bên trái. Không dùng mock page rời nên kết quả phản ánh đúng composition của app. Lần capture OCR đầu phát hiện subtitle sát dải playback; source đã thêm clearance, safe ROI geometry và capture `dubbing-transcribe-ocr-right-panel-final-1280x720.png` được chạy lại, không còn glyph bị che hay ROI chạm playback controls. Preview shim có thể in warning về property/signal C++ không có trong harness; đó là giới hạn shim, không phải QML lint/runtime contract của production build. Thumbnail fixture hiện dùng ảnh first-frame thật được tạo từ `out/dubbing-live-test/dubbing_live_walkthrough.mp4`, không phải bitmap mock.

## 9. Graphify và release hygiene

Graphify đã được refresh sau thay đổi pane/copy/QML và hai fix thumbnail/model gate: `18.073` nodes, `31.548` edges sau graph build, `918` communities; `graph.html` aggregated có `918` community nodes và `2.186` cross-community edges. Graph health không có missing/dangling endpoint, self-loop hoặc collapsed edge; producer suppression có 12 site được ghi nhận bởi graphify và không được diễn giải thành lỗi code. Semantic extraction đầy đủ của docs/images không được chạy lại bằng LLM nên không được ghi đè vào graph; đây là giới hạn môi trường, không phải dữ liệu được giả vờ là đã phân tích. `graph.html` được sinh ở dạng aggregated community view vì graph vượt 5.000 nodes. Graphify còn cảnh báo môi trường: skill 0.9.11 khác package 0.9.49, thiếu `tree_sitter_sql`, và 38 file có syntax error/partial extraction; các cảnh báo này không tạo dangling edge nhưng cần xử lý riêng nếu muốn graph extraction hoàn chỉnh. Các file graph/cache là generated artifacts; khi commit cần giữ chúng đồng bộ cùng manifest/report hoặc áp dụng policy repository nếu project muốn bỏ generated output.

Trước khi merge/push, chạy lại:

```powershell
& '.\scripts\build.ps1' -Preset 'windows-msvc-release' -QtRoot '.tools\Qt\6.9.3' -Version '0.0.8.6' -MaxParallelJobs 4 -SkipDeploy -AllowUnsignedEspeakForInternalBuild
    ctest --test-dir out\build\windows-msvc-release -C Release --output-on-failure
& '.\scripts\preview_dubbing_ui.ps1' -Capture
& '.\scripts\preview_dubbing_ui.ps1' -Capture -Width 1600 -Height 900
& '.\scripts\preview_dubbing_ui.ps1' -Capture -Width 2560 -Height 1440
& '.\scripts\preview_dubbing_ui.ps1' -Capture -Width 3840 -Height 2160
git diff --check
```

Package verification PASS bằng `scripts/package.ps1` với `-SkipInstaller -PortableInternalLayout`: `out/LA-Studio-0.0.8.6/LA-Studio-0.0.8.6.exe` có File/ProductVersion `0.0.8.6`; `platforms/qwindows.dll`, `Qt6Core.dll`, `media-tools/ffmpeg.exe`, `subtitle-ocr/tesseract.exe`, notebook Colab và `data/presets/voice_clone_refs/vieneu_Minh_Đức.wav` đều tồn tại. Package script cũng xác nhận staging manifest/license manifest và portable QML smoke exit code 0. Đây là internal package vì lệnh cho phép eSpeak payload unsigned đã SHA-256-verify; live Colab E2E vẫn là release boundary bên ngoài môi trường này.

GitHub delivery PASS: branch `main` đã push thành công tới `origin` (`https://github.com/khoinguyen59/KOVA-DUB.git`). Các commit chính của đợt này gồm implementation `c9c795d5`, release/graph evidence `0f7d726c` và `2207447101`, các docs delivery follow-up, cùng watchdog hardening `c0713cc`; graph refresh được commit cùng tài liệu này trước lần push cuối.

## 10. Tiêu chí chấp nhận trước khi gọi là production-ready

Chỉ đánh dấu release-ready sau khi tất cả điều kiện sau có log/artifact chứng minh:

- fixture video có audio đã chạy qua Import → Export;
- fixture 9:16 và 1:1 không crop/stretch sai trong preview và export;
- separation tạo được cả Vocals và Background non-empty;
- STT, OCR, fusion, translation, synthesis đều sinh output đúng manifest;
- đổi voice/stem liên tục không phát chồng và không còn `QAudioSink` cũ;
- kill/restart giữa mỗi node resume đúng hoặc dừng với hướng dẫn rõ;
- Colab health/capability/lease/inference/release pass bằng notebook/model exact;
- export validation kiểm tra stream/duration và atomic commit;
- 1280×720, 1920×1080, 2560×1440 và 3840×2160 không có clipping/collision;
- raw log và user guidance cùng tồn tại trong một failure report.

Trạng thái hiện tại: bản portable `0.0.8.6` đã qua build, CTest, QML lint, visual matrix và package smoke; watchdog subprocess đã được harden và có regression test. Việc còn cần cho production sign-off là live full E2E với runtime thật (Colab/GPU và media/model thật), không phải lỗi contract local. Automatic review policy vẫn là P2 vận hành nên cần quyết định trước khi bật unattended production.

## 11. Recheck release 0.0.8.6

Sau khi sửa lỗi ROI editor có thể chạm playback controls, bản source hiện tại đã được build lại và kiểm thử lại: QML lint exit 0; CTest serial `41/41`; preview production `Main.qml` pass ở `1280×720`, `1600×900`, `2560×1440`, `3840×2160`; OCR preview final không còn ROI/subtitle chạm thanh playback; package smoke exit `0`; EXE File/ProductVersion `0.0.8.6`. Đây là bằng chứng cho toàn bộ contract/UI/workflow local trong phạm vi repo. Live Colab, GPU lease và một phiên dubbing đủ 8 task với model thật vẫn phải được nghiệm thu ở môi trường triển khai có credential/runtime tương ứng.

## 12. Recheck bổ sung sau phản hồi lỗi runtime/model picker

Build/test mới nhất sau hai bản sửa:

- Build MSVC/Qt 6.9.3 thành công: `out/build/windows-msvc-release/LA-Studio-0.0.8.6.exe`.
- `TestDubbingProject::runWithoutSeparationSetupRequestsModelSelection` PASS: cấu hình thiếu cho `source-separate` dừng trước runner, log vẫn có warning kỹ thuật, `workflowSetupRequired(node-model)` được phát và `lastError` vẫn rỗng.
- `TestDubbingProject::importedVideoPublishesAsyncThumbnailUrl` PASS: file video được import, FFmpeg fake/managed process sinh JPEG, controller phát `sourceThumbnailChanged` và trả local URL hợp lệ.
- CTest toàn bộ: `41/41` PASS, `0` fail; QML lint và QML route smoke PASS.
- Preview `dubbing-preview-thumbnail-gate-final-1280x720.png` dùng full production `Main.qml`, có source video thật, canvas 16:9, panel phải cố định, task rail tiếng Anh ngắn và không có clipping/collision ở kích thước này.
- Portable package đã được stage lại từ source hiện tại theo layout 8.4 tại `out/LA-Studio-0.0.8.6/`; EXE có File/ProductVersion `0.0.8.6`. Sau khi stage, packaged QML smoke chạy từ chính EXE với exit `0`.

Giới hạn còn lại không thay đổi: chưa có credential Colab/GPU để nghiệm thu inference thật và chưa chạy một media fixture qua đủ tám task với model AI thật. Đây là giới hạn môi trường kiểm thử, không phải lỗi route model picker hoặc thumbnail local.

## 13. Recheck theo các yêu cầu UI bổ sung mới nhất

### 13.1 Colab, URL độc lập và model action

- `DubbingColabSetupDialog.qml` có nút `Open Unified Colab`, mở đúng notebook `LA_STUDIO_UNIFIED_DUBBING_GPU.ipynb` qua helper URL chung.
- Có hai route nhập riêng: `dubbingTtsWorkerUrlField` cho TTS và `dubbingOcrWorkerUrlField` cho Subtitle OCR. Mỗi route gọi `connectWorkflowColabStage()` với stage/model riêng, không dùng chung một URL ngầm.
- Các thẻ model vẫn dùng exact model + exact notebook; lỗi thiếu route/model chỉ để lại log kỹ thuật và phát `workflowSetupRequired` để QML mở picker/setup.

### 13.2 Thumbnail, video canvas và subtitle/OCR

- Poster thumbnail giữ first frame cho tới khi video thực sự chuyển sang `PlayingState`; vì vậy canvas không quay lại màu đen giữa trạng thái loaded và playback.
- Viewport vẫn cố định 16:9 và `VideoOutput.PreserveAspectFit`; video dọc/vuông được letterbox trong khung, không stretch/crop vào playback controls.
- OCR scan controls chỉ được hiển thị khi `displayedStepId === "transcribe"`; subtitle mặc định ở safe lower region và chỉ bám ROI khi OCR được bật.

### 13.3 Align, History và menu ba chấm

- Task `5 Align` không còn là panel rỗng: `DubbingAlignmentStep` có `Run alignment`, `Alignment Studio`, `Back`, `Continue`, count segments và count conflicts.
- History là `Popup` độc lập với `parent: Overlay.overlay`, mở trượt đè lên workspace nên không làm co video/panel phải. Nút thu gọn bên trong cũng đóng popup đúng vòng đời.
- Menu ba chấm tính `x` từ `moreActionsButton.x` và clamp theo `root.width`, nên nằm cạnh nút gọi menu ở cả header rộng và hẹp.

### 13.4 Rút gọn nội dung và export

- Các câu hướng dẫn dư trong Colab footer, OCR ROI, TTS và nút export đã rút gọn; vẫn giữ title, trạng thái, CTA và thông tin lỗi cần thiết.
- Export dialog/step có `Open in CapCut` khi draft folder đã tồn tại và gọi `DubbingController::openCapCutDraft()`.
- Đây là mở CapCut best-effort: tài liệu trợ giúp chính thức của CapCut hiện nói không hỗ trợ nhập trực tiếp một project/draft vào project khác; ứng dụng vì vậy mở executable nếu tìm thấy, nếu không thì mở draft folder và báo rõ fallback, không giả vờ rằng import tự động luôn thành công. Xem [CapCut Help — Import a previous project](https://www.capcut.com/help/import-a-previous-project-into-the-current-project).

### 13.5 Bằng chứng xác minh mới nhất

| Hạng mục | Kết quả |
|---|---|
| Build Qt 6.9.3/MSVC 0.0.8.6 | PASS |
| QML lint | PASS, exit 0 |
| CTest toàn bộ | PASS, 41/41 |
| Production QML preview 1280×720 | PASS; thumbnail/video thật, right panel, Align actions, không clipping |
| History overlay capture | PASS; overlay đè trên workspace, video vẫn giữ kích thước |
| OCR preview capture | PASS; ROI/subtitle nằm trong khung, không chạm playback bar |
| Thumbnail controller regression | PASS; thumbnail JPEG được tạo bất đồng bộ từ video fixture |

Ảnh evidence mới: `out/ui-demo/dubbing-preview-popup-scene-final4-1280x720.png`, `out/ui-demo/dubbing-align-popup-scene-final4-1280x720.png`, `out/ui-demo/dubbing-history-popup-scene-final4-1280x720.png`, `out/ui-demo/dubbing-transcribe-ocr-popup-scene-final4-1280x720.png`, cùng bộ responsive mới nhất `out/ui-demo/dubbing-preview-release-final2-1600x900.png`, `dubbing-align-release-final2-1600x900.png`, `dubbing-history-release-final2-1600x900.png` và `dubbing-transcribe-ocr-release-final2-1600x900.png`.

### 13.6 Trạng thái cuối

Trong phạm vi source/local verification, các yêu cầu UI mới đã được triển khai và có regression contract tương ứng. Không còn test fail. Hai biên chưa thể xác nhận offline vẫn là inference Colab/GPU thật và khả năng CapCut tự import draft theo command-line; cả hai đều đã được hiển thị như giới hạn rõ ràng thay vì báo “Ready” giả.

## 14. Recheck voice target compatibility — Clone, TTS và Dubbing

### 14.1 Kết quả triển khai

- `VoiceClonePresetService` đã nâng schema thư viện lên `2` và chuẩn hóa mọi preset hợp lệ, gồm voice bundled, voice từ CapCut/F5 và voice clone cá nhân, với hai target canonical: `vieneu` và `omnivoice`.
- Mỗi preset có `targetBindings.vieneu` và `targetBindings.omnivoice`, dùng chung reference audio/transcript đã validate. `sourceModelFamily`/`familyId` chỉ còn là provenance, không quyết định backend được phép dùng.
- `presetsForFamily("vieneu")`, mọi exact VieNeu model id và `presetsForFamily("omnivoice")` đều trả về cùng tập preset hợp lệ. Reference bundled được resolve về bản app/source managed và không còn bị validator loại nhầm là file ngoài LA Studio.
- `VoiceGalleryDialog` đọc `voiceModelTargets`, hiển thị đồng thời badge `VieNeu` + `OmniVoice` trên từng card và đã bổ sung lọc Miền Trung. Vì vậy voice clone riêng không bị mất khỏi tab OmniVoice chỉ vì được tạo từ family khác.
- Clone Studio, TTS Settings và Dubbing Synthesize/Node Inspector đều dùng cùng thư viện preset. TTS/Dubbing cho phép chọn target độc lập; chọn voice clone sẽ mở/đồng bộ đúng model target thay vì suy luận từ source family.
- Local Dubbing gọi `TtsEngine::cloneVoice()` theo từng chunk với reference audio đã chọn. VieNeu/OmniVoice dùng reference-cloning thật; Qwen3 giữ đường clone tương thích hiện có. API Gateway không bị giả định là hỗ trợ reference clone và không tự thay voice.
- Direct Colab clone kiểm tra exact model/notebook route, worker verification và consent trước khi chạy. Không có fallback âm thầm sang source audio, voice ngẫu nhiên hoặc model TTS thường.
- Khi target đã lưu là VieNeu/OmniVoice nhưng local runtime đang load model khác, controller/job dừng với hướng dẫn load đúng model; không chạy nhầm backend.

### 14.2 Files/symbols chính đã recheck

| Khu vực | File | Contract đã xác nhận |
|---|---|---|
| Shared voice metadata | `src/controllers/shared/VoiceClonePresetService.cpp` | schema 2, `voiceModelTargets`, `targetBindings`, managed reference validation, source-independent lookup |
| Dubbing selection | `src/controllers/dubbing/DubbingController.cpp/.h` | `selectCloneVoicePresetForTarget()`, explicit `voiceCloneModelId`, target/runtime gate |
| Dubbing execution | `src/controllers/dubbing/DubbingSynthesisJob.cpp` | local `cloneVoice()`, exact target check, Direct Colab clone route, no API fallback |
| Local runtime gate | `src/tts/pipeline/TtsSavedVoiceProfile.h` | VieNeu/OmniVoice/Qwen reference-clone support và exact loaded-target matching |
| Clone tab | `qml/components/voicecloning/ReferenceInputBox.qml`, `VoiceCloningStudioView.qml` | shared gallery, saved voice, hai target badges |
| TTS tab | `qml/components/tts/TtsSettingsPanel.qml`, `TtsStudioView.qml` | target selector, shared preset list, local/Direct Colab clone dispatch |
| Dubbing tab | `qml/components/dubbing/steps/DubbingSynthesizeStep.qml`, `DubbingNodeInspector.qml` | target selector, model open action, shared voice selection |
| Shared gallery | `qml/components/shared/VoiceGalleryDialog.qml` | target-aware filter, OmniVoice includes all universal presets, VieNeu regional filters |

### 14.3 Regression evidence

| Kiểm tra | Kết quả |
|---|---|
| `TestDubbingProject` | 122 passed, 0 failed |
| `voiceTargetsAreSourceIndependentAndPersisted` | PASS — source family bất kỳ, preset xuất hiện ở cả VieNeu và OmniVoice |
| `localSavedVoiceUsesReferenceCloneForUniversalTargets` | PASS — local saved reference đi qua clone path, không fallback TTS thường |
| `dubbingUsesReferenceCloneForLocalVieNeuAndOmniVoice` | PASS — controller/job contract và target routing tồn tại |
| `dubbingUiUsesSafePublicContractsAndArtifactGates` | PASS — gallery có target helper, Miền Trung và hai badge |
| CTest toàn bộ | 41/41 PASS |
| QML lint | PASS, exit 0 |
| Production QML preview | PASS ở 1280×720 với thumbnail/video thật, 16:9 PreserveAspectFit, panel phải cố định |

### 14.4 Giới hạn nghiệm thu còn lại

Metadata và routing đã được kiểm chứng bằng code/test local. Việc clone thực tế vẫn cần chạy ít nhất một voice bundled và một voice cá nhân trên runtime VieNeu thật, OmniVoice thật, cùng worker Direct Colab tương ứng; môi trường hiện tại không có GPU/credential Colab nên không được ghi nhận giả là PASS. Nếu model/runtime chưa cài hoặc worker chưa verify, UI phải hiện trạng thái chuẩn bị/kết nối và giữ lỗi kỹ thuật trong log.

## 15. Recheck workflow thật và portable package 0.0.8.6 — 2026-08-28

### 15.1 Local production workflow gates

- CTest toàn bộ: `41/41` PASS, `0` fail, tổng thời gian `68.01s`.
- Remote/static contract gates: `32/32` exact-model notebooks, `31/31` controller/UI/notebook bindings, `9` capability paths của live-Colab acceptance contract và `8/8` direct Colab routes PASS.
- QML lint production preset: PASS, exit `0`.
- Packaged QML smoke chạy từ `out/LA-Studio-0.0.8.6/LA-Studio-0.0.8.6.exe`: exit `0`, trace có `19` interaction events.
- Real-media smoke chạy bằng `--live-test-dubbing-studio` với `C:\Users\Nguyen Trong Khoi\Downloads\1.mp4`: `10/10` media-pipeline checks PASS, exit `0`, tổng thời gian khoảng `35.934s`. Video cuối được ffprobe xác nhận H.264 + AAC, thời lượng `30.066667s`, kích thước `8,909,572` bytes.

Lưu ý quan trọng: runner 10 task là media/workflow smoke dùng fixture deterministic cho STT, OCR, translation và FFmpeg transform cho TTS; nó không phải nghiệm thu inference AI thật của Whisper/OCR/LLM/TTS/voice clone.

### 15.2 Lỗi tìm thấy và đã sửa trong lần recheck

- Sửa recursive notebook lookup trong các verifier và Unified Dubbing coordinator; notebook thật nằm trong các thư mục capability con, không phải chỉ ở root `notebooks/`.
- Pin Unified Dubbing notebook vào repository hiện tại `khoinguyen59/KOVA-DUB` và commit hợp lệ `305888f6612d8f1063ddc0a5c6ffe2ce3ab4f25b`.
- Đồng bộ raw worker URL của Spleeter Safe sang repository hiện tại; worker và launcher đã được kiểm tra hash khớp.
- Các notebook tracked được phép giữ helper cells append-only dành cho user, nhưng phần core do generator sinh ra vẫn phải khớp semantic với bản generated.

### 15.3 Portable package

- Lệnh build: `scripts/package.ps1 -Preset windows-msvc-release -QtRoot .tools\Qt\6.9.3 -Version 0.0.8.6 -MaxParallelJobs 4 -SkipInstaller -PortableInternalLayout -AllowUnsignedEspeakForInternalBuild`.
- Output: `out/LA-Studio-0.0.8.6/LA-Studio-0.0.8.6.exe`.
- File/ProductVersion: `0.0.8.6`.
- Kích thước EXE: `30,726,144` bytes.
- SHA-256: `41D644D8E1E2437BFEF4E83AAFD5ABA2E99A5EF9C8FA9765E36CE420678A3136`.
- Payload runtime bắt buộc: `14/14` file tồn tại, gồm Qt platform plugin, FFmpeg/ffprobe, Tesseract, eSpeak NG, voice presets/reference, Unified notebook, VieNeu notebook, OmniVoice notebook, coordinator và third-party notices.
- Đây là internal portable build vì dùng cờ cho phép eSpeak MSI chưa có chữ ký trong môi trường build; không phát hành public package cho tới khi hoàn tất policy/signing tương ứng.

### 15.4 Release boundary

Shell hiện không có `LASTUDIO_LIVE_*` credential/URL và không có phiên Colab/GPU live. Vì vậy kết luận hiện tại là: workflow local, routing, UI contracts, media stages và package smoke đều xanh; chưa được tuyên bố là full AI acceptance cho inference thật qua Colab/GPU. Live AI sign-off vẫn là deployment gate cần chạy ở môi trường có runtime, model và credential tương ứng.

## 16. Recheck incident task-scope và portable 0.0.8.7 — 2026-08-28

### 16.1 Incident và root cause

- **INC-015:** Khi mở `Dubbing Direct Colab setup` từ task `Separate`, UI vẫn render `Next transcript action`, TTS và OCR. Root cause là `stageIds` chỉ lọc các card trong `Repeater`; các section dùng chung nằm ngoài `Repeater` và render vô điều kiện.
- **Fix:** `DubbingColabSetupDialog.qml` dùng `includesStage/includesAnyStage` cho mọi section; selected count chỉ tính stage trong scope; nút check có chế độ current-stage; Unified Connect bị khóa nếu selection nằm ngoài scope.
- **INC-016:** Packaged QML smoke lần đầu sau fix báo mismatch giả vì helper đọc `child.visible` trong Dialog đang đóng. Đây là lỗi test oracle/lifecycle assumption, không phải lỗi binding production.
- **Fix:** `qmlSmokeScopedStageCheck()` kiểm tra cùng scope predicate public mà binding dùng, thay vì suy luận từ visibility của child khi popup chưa mở.

### 16.2 Scope matrix

| Scope | Transcript section | TTS row | OCR row | Kết quả |
|---|---:|---:|---:|---:|
| `source-separate` | Ẩn | Ẩn | Ẩn | PASS |
| `transcribe` | Hiện | Ẩn | Ẩn | PASS |
| `subtitle-ocr` | Hiện | Ẩn | Hiện | PASS |
| `translate` | Ẩn | Ẩn | Ẩn | PASS |
| `synthesize` | Ẩn | Hiện | Ẩn | PASS |
| `alignment` | Ẩn | Ẩn | Ẩn | PASS |
| global `stageIds=[]` | Theo thiết kế global | Theo thiết kế global | Theo thiết kế global | PASS |

### 16.3 Verification sau source fix

- QML lint: PASS, exit 0.
- CTest sau source fix cuối: **41/41 PASS**, 0 fail.
- `QmlRouteSmoke`: PASS; thực thi matrix scope và kiểm tra contract workspace.
- Production preview capture 1280×720: PASS; video thumbnail/16:9 PreserveAspectFit, right panel và Separate workspace không clipping.
- Packaged QML smoke từ `out/LA-Studio-0.0.8.7/LA-Studio-0.0.8.7.exe`: exit `0`; log có `QML module loaded` và `Application services initialized`.

### 16.4 Portable package

- Command: `scripts/package.ps1 -Preset windows-msvc-release -QtRoot .tools/Qt/6.9.3 -Version 0.0.8.7 -MaxParallelJobs 4 -SkipInstaller -PortableInternalLayout -AllowUnsignedEspeakForInternalBuild`.
- Output: `out/LA-Studio-0.0.8.7/LA-Studio-0.0.8.7.exe`.
- Product version: `0.0.8.7`; size: `30,791,680` bytes; SHA-256: `47D6974A42FAA950DE147B7F836E3F71B5477C73192E60917714B6C0D1585032`.
- Layout: flat portable root, không installer.
- Package vẫn là internal-only vì build cho phép eSpeak NG MSI đã verify SHA-256 nhưng chưa có chữ ký; không dùng làm public release nếu chưa hoàn tất signing/policy.
- Packaged smoke có warning không fatal `QFontDatabase: Cannot find font directory .../lib/fonts` vì Qt 6.9.3 không ship font mặc định. Đây là **INC-017**, cần stage bộ font được cấp phép hoặc cấu hình fallback có chủ đích trước public release; warning này không che khuất các gate QML/type/DLL đã PASS.

### 16.5 Boundary không được ghi quá mức

Môi trường recheck không có credential/worker Colab GPU live. Vì vậy các kết quả trên xác nhận source contract, UI/QML, local workflow và package startup; không phải bằng chứng inference thật của Whisper/OCR/LLM/TTS/voice clone trên Colab.

## 17. Recheck Direct Colab Run gate và route STT + OCR — 2026-08-28

### 17.1 Lỗi xác nhận

- **INC-018:** Sau khi chọn model Direct Colab, nhập URL/token và check connection thành công, bấm Run vẫn mở lại model picker. Root cause là `DubbingPage.qml::nodeNeedsModelSelection()` kiểm tra `familyId` trước khi xét execution provider. Direct Colab cố ý xóa metadata local (`familyId`, runtime và selected files), nên một cấu hình remote đã verify bị nhận nhầm là chưa chọn model.
- **INC-019:** Với `Khớp STT + OCR`, dialog chỉ cho nhập một `Unified worker URL` và render khối xanh lớn; không có ô STT/OCR độc lập. Root cause là dialog chưa có route row cho `transcribe`, còn panel Unified được render ngoài ngữ cảnh transcript.

### 17.2 Bản sửa

- `nodeNeedsModelSelection()` phân giải `providerId` trước `familyId`. Với `colab-direct`/`colab-gpu`, hàm chỉ yêu cầu modelId và trạng thái `verified` của đúng stage; khi cả hai hợp lệ, Run đi thẳng vào `runWorkflowNode()` và không mở picker.
- `DubbingColabSetupDialog.qml` thêm panel compact cho hai worker độc lập: `dubbingSttWorkerUrlField` + STT token và `dubbingTranscriptOcrWorkerUrlField` + OCR token. Mỗi nút Connect gọi `connectWorkflowColabStage()` với capability/model/session riêng.
- Khối `Unified Dubbing Colab` chỉ còn hiện khi không chọn transcript route độc lập. Khi mode là `reconcile` hoặc dialog mở theo STT/OCR scope, panel xanh được ẩn để không chiếm chỗ; nút mở STT/OCR Colab vẫn có trong panel compact.
- Các scope STT/OCR riêng vẫn giữ đúng route tương ứng; không tạo panel rỗng khi mở dialog trực tiếp cho một stage.

### 17.3 Verification

| Kiểm tra | Kết quả |
|---|---|
| Regression contract: remote provider được xét trước familyId | PASS |
| Regression contract: STT/OCR fields + unified visibility predicate | PASS |
| QML lint Qt 6.9.3 | PASS, exit 0 |
| CTest toàn bộ | PASS, 41/41 |
| Portable package rebuilt from source | PASS, `out/LA-Studio-0.0.8.7/` |

Portable EXE sau lần đóng gói cuối: `out/LA-Studio-0.0.8.7/LA-Studio-0.0.8.7.exe`, kích thước `30,816,256` bytes, SHA-256 `782FB01501EBBCCC759F0AA126E7C8D72D547A9009BE31A6968463BAC2FDA0CC`. EXE khởi động được và hiển thị cửa sổ production; package QML smoke exit `0`.

Giới hạn: chưa thể gọi Connect thành công với worker Colab thật trong môi trường không có URL/token hợp lệ; verification ở trên kiểm tra đúng route/session contract và UI gate. Khi dùng runtime thật, cần xác nhận thêm handshake HTTP và chạy inference.

## 18. Thiết lập mandatory pre-build release gate — 2026-08-29

### 18.1 Mục tiêu

Checklist trước đây là tài liệu bắt buộc nhưng việc chạy vẫn phụ thuộc vào thao tác nhớ thủ công. Điều đó không đủ để ngăn lặp lại các incident như QML không load, nút/modal không phản hồi, route Colab sai scope, model picker mở lại sau khi đã verify hoặc package chạy khác build tree.

### 18.2 Cơ chế đã triển khai

- Thêm `scripts/prebuild_gate.ps1`. Script chạy từ repository root và fail-fast theo từng check có tên rõ ràng.
- Gate kiểm tra file release bắt buộc và đọc động toàn bộ incident ID liên tiếp trong checklist. Khi thêm `INC-020` trở lên, gate tự nhận incident mới; nếu bỏ sót một ID ở giữa, build bị chặn.
- Gate chạy `git diff --check`, catalog/checksum và runtime ABI, full CTest + `QmlRouteSmoke`, QML lint, exact Colab controller/UI/notebook bindings, generated notebook integrity, Unified Dubbing Colab contract và remote feature surface.
- `scripts/package.ps1` gọi gate sau khi resolve dependency nhưng trước CMake configure/build. Gate fail thì package dừng với exit code khác 0.
- Sau khi stage runtime, `package.ps1` chạy chính packaged EXE ở `LASTUDIO_QML_SMOKE=1`, kiểm tra timeout, exit code, interaction trace và fatal startup diagnostics. Vì vậy CTest source không thể che lấp lỗi DLL/QML/package layout ở bản portable.
- Evidence của lần gate gần nhất: `out/prebuild-gate/latest.json`. Evidence packaged smoke: `out/package-smoke/<version>/stdout.log`, `stderr.log`, `qml-interaction-trace.json`.

### 18.3 Regression contract cho chính pipeline

`TestDubbingWorkspaceContract::packagingRequiresPreBuildReleaseGate()` buộc phải tồn tại gate, yêu cầu các validator quan trọng, kiểm tra gate đọc incident động và kiểm tra package có gọi gate + packaged EXE smoke. Nếu lời gọi bị xóa hoặc script bị đổi tên, CTest sẽ fail trước release build.

### 18.4 Verification lần triển khai

| Check | Kết quả |
|---|---|
| Contract test trước khi triển khai gate | FAIL đúng kỳ vọng: gate script missing |
| Pre-build gate sau khi triển khai | PASS; 9/9 gate groups |
| C++ regression + QML route smoke | PASS; 41/41 |
| QML lint | PASS; exit 0 |
| Exact Colab bindings | PASS; 31/31 |
| Generated Colab notebooks | PASS; 32/32 |
| Unified Dubbing Colab | PASS |
| Remote feature surface | PASS; 8/8 |
| Gate evidence serialization | PASS; `out/prebuild-gate/latest.json` hợp lệ |

Lần chạy đóng gói hậu kiểm sau đó cũng đã PASS đầy đủ:

- `scripts/package.ps1` gọi pre-build gate trước CMake configure/build và nhận kết quả `9/9` nhóm PASS.
- CTest chạy lại trong package: `41/41` PASS; QML lint, exact bindings `31/31`, generated notebooks `32/32`, Unified Dubbing và remote feature surface `8/8` đều PASS.
- Portable EXE được stage tại `out/LA-Studio-0.0.8.7/LA-Studio-0.0.8.7.exe`, kích thước `30,816,256` bytes, SHA-256 `782FB01501EBBCCC759F0AA126E7C8D72D547A9009BE31A6968463BAC2FDA0CC`.
- Packaged EXE QML smoke chạy bằng chính payload portable: exit `0`, trace `19` interaction events tại `out/package-smoke/0.0.8.7/qml-interaction-trace.json`; không còn process ứng dụng/runtime host sau khi smoke kết thúc.
- Evidence gate mới nhất: `out/prebuild-gate/latest.json`, trạng thái `PASS`, `9` check groups, `0` failure.

Các cảnh báo còn lại là cảnh báo môi trường/internal package, không bị che giấu: Qt kit thiếu thư mục font mặc định, Vulkan headers không có, PaddleOCR runtime là optional và eSpeak NG MSI chỉ được cho phép bằng cờ internal vì chưa có chữ ký. Do đó artifact này xác nhận đường build và startup nội bộ; vẫn chưa phải public release cho tới khi xử lý signing/policy và nghiệm thu live Colab/GPU.

### 18.5 Quy tắc vận hành từ nay

Release engineer phải dùng `scripts/package.ps1`; chạy CMake trực tiếp không được coi là release build vì bỏ qua gate. Trước khi build thủ công có thể chạy `scripts/prebuild_gate.ps1` để nhận feedback sớm, nhưng package vẫn chạy lại gate lần nữa. Các bước live GUI, kiểm tra màn hình thật, kết nối worker Colab thật và inference AI thật vẫn là nghiệm thu hậu kiểm cần thực hiện ở môi trường tương ứng; không được gán PASS offline cho các phần đó.

## 19. Recheck canonical STT/OCR, workflow order và audio mix — 2026-08-29

- `DubbingTranscribeStep.qml` hiện chỉ gọi `reconcileTranscriptSources()` khi
  cả `sttSegments` và `ocrSegments` đã có; một nguồn đơn lẻ vẫn Continue được.
- `DubbingTranscriptFusionService` dùng `prefer-stt` làm mặc định. Khi text/
  timeline không khớp, STT là canonical; OCR vẫn nằm trong provenance và danh
  sách evidence, không tự tạo cue OCR-only cho luồng dịch/TTS mặc định. Policy
  `prefer-ocr` hoặc `ask` chỉ có hiệu lực khi được chọn rõ.
- Workflow đã thống nhất thứ tự `Import → Normalize → Separate (optional) →
  Transcribe → Translate → Synthesize → Align → Mix & Export`; automatic graph
  loại Separate khỏi prerequisite, còn manual Separate vẫn tạo stems khi cần.
- Align persist mức `originalGainPercent`/`dubbedGainPercent` và mixer áp dụng
  `originalGainPercent` cả trong nhánh sidechain release. Mặc định project là
  `0% tiếng gốc / 100% lồng tiếng`.
- Project mới và migration thiếu trường dùng mặc định `zh → vi`; hai guide
  agent về transcript reconciliation và translation là nguồn contract cho
  những tool/IDE bên ngoài.

### Verification hiện tại

`scripts/run_tests.ps1` trên Qt 6.9.3/MSVC đã đạt **41/41 CTest PASS**. QML
lint, prebuild gate và packaged EXE smoke cũng đã đạt: gate `9/9`, exact
bindings `31/31`, generated notebooks `32/32`, remote surface `8/8`, packaged
smoke `19` interaction events. Portable artifact 0.0.8.7 ở
`out/LA-Studio-0.0.8.7/LA-Studio-0.0.8.7.exe`, không có `bin/`, SHA-256
`FF18CA4191232C6598543B99231F73567880D8D6E082DA647173C489DF94F6D6`.
Live Colab/GPU inference vẫn là acceptance test môi trường triển khai, không
 thể suy ra từ test offline.

## 20. Recheck setup preflight và quy tắc kiểm tra tương đương — 2026-08-29

- Lỗi còn sót được tái hiện ở đường `Run STT`/`Run OCR` khi chưa có model,
  runtime hoặc worker Colab: UI có thể rơi vào generic Error Guidance thay vì
  mở đúng màn hình cấu hình. Nguyên nhân là QML preflight không dùng chung
  nguồn sự thật với backend và OCR là route độc lập, không phải persisted graph
  node để mở bằng model picker chung.
- Đã sửa controller bằng `workflowNodeSetupIssueForUi()`, giữ setup thiếu là
  `workflowSetupRequired` có thể khôi phục; STT mở model picker, OCR mở exact
  Subtitle OCR setup gồm model/route/URL/token. Backend không gọi `setError`
  cho trường hợp chỉ thiếu setup; lỗi runtime/input thật vẫn ghi log.
- Checklist đã bổ sung quy tắc bắt buộc: mỗi bug fix phải recheck theo ma trận
  đủ 8 task và các bề mặt tương đương (entry, setup gate, error guidance/log,
  state/concurrency, handoff và UI), không được sign-off chỉ dựa trên task bị
  báo lỗi. Contract test cũng kiểm tra sự tồn tại của quy tắc này.

### Evidence mới nhất

- CTest: `41/41 PASS`; QML lint: PASS; prebuild gate: `9/9` nhóm PASS.
- Exact Colab bindings `31/31`, generated notebooks `32/32`, remote surface
  `8/8`; packaged QML smoke: `19` interaction events.
- Portable EXE được build lại từ source đã sửa tại
  `out/LA-Studio-0.0.8.7/LA-Studio-0.0.8.7.exe`, version `0.0.8.7`, kích thước
  `30,942,720` bytes, SHA-256
  `1DA7D21E5B17CB2BFB6C42CBE2093253A59C9A96A53A462529018CE4AF455909`.
- Bằng chứng này xác nhận local build/startup/QML/contract; live Colab/GPU
  inference vẫn cần nghiệm thu với worker và credential thật.
