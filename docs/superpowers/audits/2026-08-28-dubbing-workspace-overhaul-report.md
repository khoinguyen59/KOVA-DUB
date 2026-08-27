# Báo cáo recheck tổng thể — Dubbing Studio Workspace

Ngày kiểm tra: 2026-08-28
Phạm vi: C++/Qt 6, QML, workflow graph, media subprocess, persistence, voice routing, error guidance, 16 tài liệu `doc/front` và `doc/back`.
Mục tiêu: đối soát code thật sau đợt đại trùng tu giao diện và hardening workflow; ghi nhận bằng chứng, lỗi còn lại và điều kiện để luồng dubbing chạy ổn định.

## 1. Kết luận điều hành

Các yêu cầu giao diện và hợp đồng workflow chính đã được sửa trong source:

- Thanh task trên cùng hiển thị nhãn tiếng Anh ngắn; hover mới hiện tên tiếng Anh kèm tiếng Việt.
- Nội dung Results/Settings/Handoff chuyển sang right context drawer; task shelf bên trái giữ các hành động chính.
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
| Build source bằng Qt 6.9.3/MSVC release preset | PASS; `LA-Studio-0.0.8.5.exe` trong `out/build/windows-msvc-release` |
| CTest toàn bộ | PASS 41/41, 0 fail |
| QML route/smoke contract | PASS trong CTest và preview harness |
| QML preview capture | PASS; đã chụp workspace, drawer Results/Settings và Transcribe/OCR ở 1280×720, 1600×900, 1920×1080, 3840×2160 logical |
| `git diff --check` | PASS; chỉ có cảnh báo chuyển LF/CRLF của Git trên Windows |
| Graphify | cập nhật sau source/docs cuối cùng; xem mục 9 |
| Live Colab GPU | Chưa chạy trong lần recheck |
| Full media E2E 1→8 bằng model thật | Chưa chạy trong lần recheck |
| Packaging/EXE mới | PASS; package 8.4-style tại `out/LA-Studio-0.0.8.5/`, EXE File/ProductVersion `0.0.8.5`, portable QML smoke exit 0 |

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

Các file 16 mục đã được đổi evidence từ CTest 39/39 sang 41/41, cập nhật ngày kiểm tra và sửa các mô tả không còn đúng. `doc/README.md` cũng dùng relative links và tên module có thật; các tên như `VoiceSeparationService`, `AudioNormalizationService`, `FFmpegAudioProcess`, `SpeechToTextService`, `TranslationService`, `AudioMixingService` không còn được dùng như class implementation.

## 4. Đối soát source và các điểm đã fix

### 4.1 Workflow rail, shelf và drawer

`qml/pages/DubbingPage.qml:316-385` định nghĩa short title/detail title cho 8 task. `qml/components/dubbing/DubbingWorkflowStep.qml` chỉ render short title ở trạng thái bình thường và tooltip có song ngữ. `qml/components/dubbing/panels/DubbingTaskShelf.qml` giữ Run/Back/Continue ở layout hai hàng; Back có kích thước khoảng 100 px, Continue fill phần còn lại.

`qml/components/dubbing/panels/DubbingContextDrawer.qml` là Drawer phải, non-modal, có giới hạn 320–520 px và clip nội dung. `DubbingPage.qml:907` gắn review/settings vào drawer thay vì chiếm cố định một cột phải. Các hook `qmlPreviewOpenDubbingContext()` và `qmlPreviewDubbingDrawer()` cho phép smoke/preview mở đúng workspace production.

Kết quả visual preview 1280×720: task rail, shelf, video canvas, timeline và drawer không có tràn ngang, chồng nút hoặc chữ đè. Các resize handle cũ được giữ object contract nhưng tắt hiển thị vì layout mới không cho phép người dùng kéo làm phá tỷ lệ.

### 4.2 Error vẫn ở log, UI có hướng dẫn

`qml/Main.qml:156` mount `ErrorGuidanceDialog`. Các panel task dùng `ErrorGuidanceInline` để hiển thị title/summary/guidance có wrap và CTA. `AppController.currentError` giữ structured fields; raw technical details không bị thay thế và tiếp tục đi vào log/support path.

Đối với các lỗi model/Colab, shelf và node inspector dẫn đến model picker/setup. Đối với upload, UI mở artifact dialog đúng node và chỉ nhận file đã khai báo; không có fallback ngầm sang audio nguồn.

### 4.3 Media preview và subtitle/OCR

`qml/components/dubbing/DubbingSourceMediaPanel.qml:35` có `showOcrTools`; `DubbingPage.qml:832` chỉ bật nó cho `transcribe`, `review-transcript` và `subtitle-ocr`. `DubbingSourceMediaPanel.qml:567` có thumbnail poster `dubbingVideoThumbnail`, `VideoOutput.PreserveAspectFit` và `previewFrameAspectRatio` cố định 16:9. Source 9:16/1:1 vẫn nằm gọn trong viewport, không bị stretch/crop.

Toolbar source chỉ giữ `Fit source`, `Original`, `Dubbed`; Browse/source management ẩn sau khi media đã load và được chuyển về shelf. Fallback/preset ROI của workspace đồng bộ với `SubtitleOcrPipeline` (`x=0.10, y=0.72, w=0.80, h=0.22`). Subtitle overlay mặc định nằm ở safe lower region; khi OCR bật, `followsOcrRegion` đưa subtitle vào ROI nhưng vẫn có `lowerControlsClearance` để không chạm seek/playback controls. Editor chỉ mở khi user yêu cầu, không bắt setup từ đầu.

### 4.4 Voice catalog và OmniVoice/VieNeu

`qml/components/shared/VoiceGalleryDialog.qml` phát signal 5 tham số gồm `voiceId`; không còn số lượng hard-code trong phần UI. `VoiceClonePresetService` chuẩn hóa `displayName`, `familyId`, `category`, `language`, `referenceText`, `compatibleModelFamilies` và `isCustomVoice` ngay khi load catalog. `DubbingSynthesizeStep.qml:78-94` chọn voice trước, sau đó yêu cầu model theo source family/model family. Nút Colab trong cùng file cũng phát `voiceModelRequested("synthesize")`, không gọi `Qt.openUrlExternally()` tới một notebook cố định. `WorkflowNodeModelDialog.qml` ưu tiên preset `voiceCloneModelId` đã lưu.

`src/controllers/dubbing/DubbingController.cpp` chuẩn hóa worker family: VieNeu reference có thể được dùng cho worker OmniVoice mà vẫn giữ source family cho UI/persistence. Đây là route cloning có chủ đích, không tuyên bố VieNeu và OmniVoice là cùng một model binary. Contract test mới kiểm tra alias `vieneu` map đúng notebook exact của OmniVoice/VieNeu route.

Comment cũ trong `VoiceClonePresetService.cpp` từng ghi số lượng master voice cố định; đã đổi thành mô tả catalog-driven để không tạo “code truth” giả khi số preset thay đổi.

Review disposition: nhận xét “STT không được fallback” không được áp dụng vì trái với contract fallback khi thiếu stem đã yêu cầu trong audit. Graph có nhánh `source-separate.vocals → transcribe.audio` và `ingest.analysisAudio → transcribe.fallbackAudio`; vì vậy STT ưu tiên stem nhưng vẫn chạy được với normalized analysis/master. Gate nghiêm ngặt vẫn nằm ở Separation cho Mix/Export: production mix/export không được suy đoán Vocals hoặc Background từ analysis.

### 4.5 Artifact, persistence và resume

`src/dubbing/project/DubbingProject.h:16-29` nâng `CurrentSchemaVersion` lên 14 và bổ sung `vocalsAudioPath`. `DubbingProject.cpp:42-86` serialize field này và chỉ load nó ở schema >=14; project schema cũ không bị đoán nhầm.

`DubbingController_Workflow.cpp:3-80` dùng kiểm tra `isFile` và size/readability. Separation chỉ Ready khi master, vocals, background hợp lệ. `DubbingController_Artifacts.cpp:583-617` không dùng `analysisAudioPath` làm vocals fallback trong production render. `WorkflowGraphRunner.cpp:27-55,176` validate output file của node completed trước khi resume.

STT chọn theo thứ tự Vocals → Analysis → Master; điều này giữ chất lượng separation nhưng vẫn cho phép project normalized-only chạy đúng mode STT đã được chọn.

Về clipping trong drawer: `DubbingSynthesizeStep` có `ScrollView`, `DubbingNodeInspector` có `ScrollView`, còn bảng transcript dùng `ListView` có `clip: true`; các step tĩnh khai báo `implicitHeight` và được chứa trong layout. Preview full shell ở 1280×720 đã kiểm tra không có nội dung động bị cắt hoặc nút bị chồng. Không thêm một ScrollView lồng ngoài vì sẽ tạo hai vùng cuộn cho cùng danh sách transcript và làm hỏng kích thước `ListView`.

### 4.6 Threading và audio lifecycle

- UI chỉ gọi invokable/controller trên main thread; hash và artifact WAV validation dùng `QtConcurrent`/`QFutureWatcher`.
- FFmpeg/FFprobe dùng `QProcess` asynchronous trên event loop; không có `waitForFinished()` trong ingest UI path, do đó không cần tạo worker thread riêng chỉ cho ffprobe.
- Controller chặn duplicate run, runner phát một terminal result, và cancel dọn staging output.
- `AudioPlayer` có generation guard/stop path; `playFile`, decoded playback và PCM playback đều reject path/file/sample invalid trước khi tạo `QAudioSink`.
- Preview stem chuyển đổi không nhận buffer rỗng; process cũ bị dừng theo session generation để tránh phát chồng.
- Export dùng staging file + validation + `AtomicMediaCommit`, không commit output nửa vời.

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

### P1 — Chưa có watchdog riêng cho mọi QProcess media

`MediaIngestService` đã async và cancel được, nhưng ffprobe/ffmpeg process không có timer deadline riêng; nếu executable treo ngoài dự kiến, UI sẽ chờ tới khi process tự kết thúc. Đây không phải lỗi “thiếu worker thread”, nhưng là hardening còn thiếu.

Đề xuất: thêm `QTimer` single-shot theo stage (probe ngắn, loudness/encode dài hơn), dừng timer khi `finished/error`, kill process khi timeout, xóa staging và phát error code `media-process-timeout` kèm hướng dẫn retry/repair. Áp dụng tương tự cho validation process trong `DubbingExportJob` và FFmpeg process trong `MediaToolService`. Thêm regression test bằng helper process không thoát.

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

Nội dung đã đồng bộ: ngày kiểm tra 2026-08-28, CTest 41/41, right context drawer, compact shelf, `vocalsAudioPath` schema 14, STT artifact preference, đồng bộ ROI với OCR preset và tên module C++ thực tế.

## 8. Visual evidence

Preview harness: `scripts/preview_dubbing_ui.ps1 -Capture`.

Ảnh đã tạo bằng fixture MP4 có thật (`out/dubbing-live-test/dubbing_live_walkthrough.mp4`):

- `out/ui-demo/dubbing-preview-1280x720.png`
- `out/ui-demo/dubbing-drawer-results-1280x720.png`
- `out/ui-demo/dubbing-drawer-settings-1280x720.png`
- `out/ui-demo/dubbing-transcribe-ocr-1280x720.png`
- Các bản responsive tương ứng `dubbing-preview-*`, `dubbing-drawer-results-*`, `dubbing-drawer-settings-*`, `dubbing-transcribe-ocr-*` cho `1600x900`, `1920x1080`, `3840x2160`.

Các ảnh dùng full `Main.qml` production shell, có top tab/navigation, task rail, compact shelf, preview, timeline và drawer. Không dùng mock page rời nên kết quả phản ánh đúng composition của app. Lần capture OCR đầu phát hiện subtitle sát dải playback; source đã thêm clearance và capture 1280×720 được chạy lại, không còn glyph bị che. Preview shim có thể in warning về property/signal C++ không có trong harness; đó là giới hạn shim, không phải QML lint/runtime contract của production build.

## 9. Graphify và release hygiene

Graphify đã được cập nhật cho code graph sau commit release-evidence `0f7d726c`: 18.035 nodes, 31.450 edges sau clustering và 921 communities; `graph.html` aggregated có 921 community nodes và 2.120 cross-community edges. Graph health không có dangling/missing endpoint, self-loop hoặc collapsed edge; producer suppression vẫn được ghi chú bởi graphify và không được diễn giải thành lỗi code. Semantic extraction đầy đủ của docs/images không được chạy lại bằng LLM nên không được ghi đè vào graph; đây là giới hạn môi trường, không phải dữ liệu được giả vờ là đã phân tích. `graph.html` được sinh ở dạng aggregated community view vì graph vượt 5.000 nodes. Graphify còn cảnh báo môi trường: skill 0.9.11 khác package 0.9.49, thiếu `tree_sitter_sql`, và 38 file có syntax error/partial extraction; các cảnh báo này không tạo dangling edge nhưng cần xử lý riêng nếu muốn graph extraction hoàn chỉnh. Các file graph/cache là generated artifacts; khi commit cần giữ chúng đồng bộ cùng manifest/report hoặc áp dụng policy repository nếu project muốn bỏ generated output.

Trước khi merge/push, chạy lại:

```powershell
& '.\scripts\build.ps1' -Preset 'windows-msvc-release' -QtRoot '.tools\Qt\6.9.3' -Version '0.0.8.5' -MaxParallelJobs 4 -SkipDeploy -AllowUnsignedEspeakForInternalBuild
    ctest --test-dir out\build\windows-msvc-release -C Release --output-on-failure
& '.\scripts\preview_dubbing_ui.ps1' -Capture
& '.\scripts\preview_dubbing_ui.ps1' -Capture -Width 1600 -Height 900
& '.\scripts\preview_dubbing_ui.ps1' -Capture -Width 1920 -Height 1080
& '.\scripts\preview_dubbing_ui.ps1' -Capture -Width 3840 -Height 2160
git diff --check
```

Package verification PASS bằng `scripts/package.ps1` với `-SkipInstaller -PortableInternalLayout`: `out/LA-Studio-0.0.8.5/LA-Studio-0.0.8.5.exe` có File/ProductVersion `0.0.8.5`; `platforms/qwindows.dll`, `Qt6Core.dll`, `media-tools/ffmpeg.exe`, `subtitle-ocr/tesseract.exe`, notebook Colab và `data/presets/voice_clone_refs/vieneu_Minh_Đức.wav` đều tồn tại. Package script cũng xác nhận staging manifest/license manifest và portable QML smoke exit code 0. Đây là internal package vì lệnh cho phép eSpeak payload unsigned đã SHA-256-verify; live Colab E2E vẫn là release boundary bên ngoài môi trường này.

GitHub delivery PASS: branch `main` đã push thành công tới `origin` (`https://github.com/khoinguyen59/KOVA-DUB.git`). Các commit chính của đợt này gồm implementation `c9c795d5`, release/graph evidence `0f7d726c` và `2207447101`, cùng các docs delivery follow-up; không có push rejection.

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

Trạng thái hiện tại: code/test/UI contract đã được harden và tài liệu đã đồng bộ; hai việc còn cần cho production sign-off là watchdog subprocess và live full E2E với runtime thật.
