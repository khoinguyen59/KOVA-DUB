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
- Portable package đã được stage lại đúng layout 8.4; smoke exit code `0`, File/ProductVersion `0.0.8.6`, SHA-256 EXE `16B0776208E2D41E41D44F35A3E4E68D1694B540AA0E097CF692AF35060140EA`.

Giới hạn còn lại không thay đổi: chưa có credential Colab/GPU để nghiệm thu inference thật và chưa chạy một media fixture qua đủ tám task với model AI thật. Đây là giới hạn môi trường kiểm thử, không phải lỗi route model picker hoặc thumbnail local.
