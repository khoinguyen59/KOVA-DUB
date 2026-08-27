# Comprehensive Technical Recheck — LA Studio Dubbing Workflow

Ngày kiểm tra: 2026-08-27
Phạm vi: Desktop Studio C++/Qt/QML, Python AI engines, workflow graph, media tools, Colab worker contract và 16 tài liệu trong doc/front và doc/back.
Mục tiêu: đối soát code thật, kiểm tra UI/UX, logic workflow, threading, artifact/recovery, khả năng vận hành và đề xuất fix để luồng dubbing chạy chắc chắn.

## 1. Kết luận điều hành

Code hiện tại có nền tảng tốt: workflow graph có journal, job chạy bất đồng bộ, QProcess cho FFmpeg/FFprobe, error catalog giữ raw log và hiển thị hướng dẫn cho người dùng, QML có elision/scrolling và test contract khá rộng.

Chưa thể kết luận hệ thống đạt 10/10 hoặc “chắc chắn chạy được” vì còn các điểm P1 ảnh hưởng trực tiếp đến tính đúng của artifact và đường đi workflow:

1. analysisAudioPath đang được dùng đồng thời như file phân tích mono và file Vocals. UI có thể hiện analysis.wav là Vocals dù separation chưa hoàn tất.
2. Step-by-step kiểm tra chuỗi rỗng trong một số gate, trong khi workflow status kiểm tra QFileInfo::isFile(). Artifact cũ hoặc path đã mất có thể làm workflow bỏ qua Separation.
3. Mixer có thể bỏ qua Background khi path thiếu/hỏng nhưng vẫn tạo output. Đây là silent degradation nếu người dùng cần BGM.
4. Resume khôi phục output từ journal mà chưa xác thực file, schema, checksum hoặc khả năng đọc.
5. Full automatic mode đổi mọi core.review-gate thành mode=never. Đây có thể là chủ ý, nhưng phải có policy và cảnh báo rõ ràng.
6. Colab proposal khả thi có điều kiện, nhưng chưa có live GPU/remote end-to-end. “Kế hoạch 10/10” là chất lượng của kế hoạch, không phải cam kết Colab free tier luôn có GPU hoặc uptime.

Không phát hiện P0 được chứng minh bằng test trong lần recheck này. Các P1 cần xử lý trước khi tuyên bố production-ready; các P2 cần xử lý để đạt quality 10/10.

## 2. Ranh giới bằng chứng

### Đã kiểm chứng

| Kiểm tra | Kết quả |
|---|---|
| CTest trên build/configuration hiện có | 40/40 pass, 0 fail, khoảng 57.9 giây |
| QML lint bằng Qt 6.9.3 managed toolchain | Exit code 0 |
| QML được khai báo trong CMake | 150/150 file |
| JSON notebook | 40 notebook parse thành công |
| Python AST | 3 file Python parse thành công |
| git diff --check | Không có whitespace error; chỉ có cảnh báo LF/CRLF |
| FFprobe/FFmpeg architecture | QProcess async trên event loop; không bắt buộc worker thread riêng cho FFprobe |
| Voice selection contract | VoiceGalleryDialog.qml khai báo và emit 5 tham số; handler nhận 4 tham số chỉ bỏ qua tham số chưa dùng |

### Chưa thể khẳng định

- Chưa build lại source tree trong lần này; test chạy bằng binary/configuration hiện tồn tại trong out/build/windows-msvc-release.
- Không build hoặc đóng gói EXE theo yêu cầu của người dùng.
- Chưa chạy full 8-task với media thật từ ingest tới export trong cùng một session.
- Chưa có live Colab GPU session xác nhận health, capability, lease, inference và release.
- Chưa có visual regression matrix tại 1280x720, 1920x1080, 2560x1440 và 3840x2160.
- Chưa chứng minh cancellation dưới tải thật, rapid voice/stem switching và resume sau kill process giữa từng node.

Do đó, “test hiện có pass” không đồng nghĩa với “source hiện tại đã được build và mọi runtime path đều pass”.

## 3. Đánh giá 16 tài liệu

Đây là điểm audit độc lập, không lấy lại điểm 10/10 tự chấm trong các file hiện tại.

- C1: Code truthfulness và API accuracy.
- C2: Logic, threading, persistence và failure handling.
- C3: UI/UX, responsive, clipping, overflow và accessibility cơ bản.
- C4: Workflow/operability, artifact contract và khả năng chạy end-to-end.

| File | C1 | C2 | C3 | C4 | Nhận xét chính |
|---|---:|---:|---:|---:|---|
| doc/front/01_task_media_ingest_ui.md | 9 | 8 | 8 | 8 | API nhìn chung đúng; evidence 39/39 stale; thiếu E2E thật |
| doc/back/01_backend_media_ingest_flow.md | 9 | 9 | 8 | 8 | Ingest/sha/loudnorm sát code; cần watchdog và evidence mới |
| doc/front/02_task_normalize_ui.md | 9 | 9 | 8 | 8 | UI có hướng dẫn; chưa có visual matrix và build mới |
| doc/back/02_backend_normalize_flow.md | 9 | 9 | 9 | 8 | CLI loudnorm hợp lệ; thiếu timeout process |
| doc/front/03_task_separate_ui.md | 9 | 7 | 8 | 7 | Ý định đúng nhưng field Vocals/analysis đang collision |
| doc/back/03_backend_separate_flow.md | 9 | 8 | 9 | 7 | Strict output có; persistence dùng sai semantic path |
| doc/front/04_task_transcribe_ui.md | 9 | 8 | 8 | 7 | STT dùng analysis WAV là đúng; cần policy Vocal stem rõ |
| doc/back/04_backend_transcribe_flow.md | 9 | 8 | 9 | 8 | STT/OCR/reconciliation tốt; thiếu E2E engine thật |
| doc/front/05_task_transcript_review_ui.md | 9 | 8 | 8 | 7 | Scroll/wrap ổn; automatic gate cần mô tả rõ |
| doc/back/05_backend_transcript_sync_flow.md | 9 | 8 | 9 | 7 | Journal/review tốt; resume chưa validate artifact |
| doc/front/06_task_translate_ui.md | 9 | 8 | 8 | 8 | Guidance tốt; chưa chứng minh route remote live |
| doc/back/06_backend_translate_flow.md | 9 | 8 | 9 | 8 | Conflict/fallback có test; cần typed error và remote E2E |
| doc/front/07_task_synthesize_ui.md | 9 | 8 | 8 | 8 | Rapid preview guard tốt; cần stress QAudioSink và null guard |
| doc/back/07_backend_synthesize_flow.md | 9 | 8 | 9 | 8 | AudioPlayer có generation guard; playPcm thiếu validation |
| doc/front/08_task_mix_export_ui.md | 9 | 8 | 8 | 8 | Path elision/error guidance tốt; policy BGM chưa explicit |
| doc/back/08_backend_mix_export_flow.md | 9 | 8 | 9 | 8 | FFmpeg/atomic export hợp lý; missing BGM có thể bị bỏ qua |

### Lỗi đồng loạt của 16 file

1. Evidence ghi CTest 39/39, trong khi configuration hiện tại liệt kê và chạy 40/40. Cần ghi số liệu có timestamp và nói rõ existing configured binaries hay source build.
2. Nhiều file ghi 10/10 dù chưa có visual matrix và live media/Colab E2E. Nên tách documented target khỏi audit score.
3. doc/README.md:31-32 còn trỏ tới AudioNormalizationService.cpp, FFmpegAudioProcess.cpp và VoiceSeparationService.cpp; các tên này không có trong source.
4. Claim “voice-only chỉ khi operator chọn policy” chưa có field/policy dễ truy vết trong code. Tài liệu phải mô tả đúng policy thực tế hoặc bổ sung policy vào project/node config.

## 4. Kiểm tra 8 task

| Task | Frontend | Backend | Kết luận |
|---|---|---|---|
| 1. Ingest & setup | Browse, path elision, error guidance | Async hash + FFmpeg + cache manifest | Tốt; thêm watchdog và E2E media |
| 2. Normalize | Hiển thị trạng thái/lỗi | Two-pass loudnorm, master + analysis | CLI hợp lệ; thêm timeout và artifact validation |
| 3. Separate | Card Vocals/Background, Play | Strict output, không fallback mixed audio khi thành công | P1: field Vocals đè lên analysis path |
| 4. STT/OCR | Review route có scroll | STT/OCR/reconciliation tách nguồn | Tốt; làm rõ analysis WAV/Vocal stem policy |
| 5. Review/alignment | ListView/ScrollView có clip | Review gate và journal | P1: auto mode bỏ qua gate; resume chưa verify |
| 6. Translation/fix | Route fix và conflict UI | Conflict ngăn chạy tiếp | Tốt; remote runtime chưa live-tested |
| 7. TTS/preview | Voice gallery, path elision | Stop session cũ, guard stale completion | Tốt; thêm playPcm validation/stress |
| 8. Mix/export | Action layout và guidance | Mixer + FFmpeg + validation/atomic commit | P1: missing Background bị bỏ qua; thêm timeout |

## 5. Findings chi tiết và phương án fix

### F-01 — P1 — Collision giữa analysis audio và Vocals stem

Bằng chứng:

- src/dubbing/project/DubbingProject.h:16,22-23 chỉ có analysisAudioPath và backgroundAudioPath.
- src/controllers/dubbing/DubbingController.h:159-161: normalizedAudioPath trả masterAudioPath, còn vocalsPath trả analysisAudioPath.
- src/dubbing/media/MediaIngestService.cpp:358-361 tạo analysis.wav là mono 16 kHz dùng cho phân tích/STT.
- src/controllers/dubbing/DubbingController.cpp:589-592 gán output vocals vào analysisAudioPath và dùng masterAudioPath làm default.
- qml/components/shared/VoiceSeparationOutput.qml:79-80 chỉ cần path khác rỗng là hiển thị Ready.

Sau ingest, analysis.wav có thể xuất hiện như Vocals dù source-separate chưa chạy. Đây là lỗi dữ liệu và UX. Default về masterAudioPath cũng có thể làm output separation không hợp lệ trông như artifact hợp lệ.

Fix đề xuất:

    // DubbingProject.h
    - static constexpr int CurrentSchemaVersion = 13;
    + static constexpr int CurrentSchemaVersion = 14;
      QString masterAudioPath;       // normalized 48 kHz master
      QString analysisAudioPath;     // normalized mono analysis.wav; never overwritten
    + QString vocalsAudioPath;       // actual separated vocals stem
      QString backgroundAudioPath;   // actual separated background stem

    // DubbingController.h
    - QString vocalsPath() const { return m_project.analysisAudioPath; }
    + QString vocalsPath() const { return m_project.vocalsAudioPath; }

    // source-separate completion
    - m_project.analysisAudioPath = outputs.value("vocals", m_project.masterAudioPath).toString();
    - m_project.backgroundAudioPath = outputs.value("background", m_project.masterAudioPath).toString();
    + const QString vocals = outputs.value("vocals").toString().trimmed();
    + const QString background = outputs.value("background").toString().trimmed();
    + if (!QFileInfo(vocals).isFile() || !QFileInfo(background).isFile()) {
    +     setError("Separation did not produce both Vocals and Background files.");
    +     return;
    + }
    + m_project.vocalsAudioPath = vocals;
    + m_project.backgroundAudioPath = background;

Migration schema 13 → 14 không nên đoán ngầm. Nếu project cũ chỉ có analysisAudioPath, giữ nó là analysis artifact, để vocalsAudioPath rỗng và hướng dẫn chạy lại Separation. Chỉ migrate tự động khi manifest cũ có bằng chứng rõ ràng về loại artifact.

### F-02 — P1 — Step gate kiểm tra chuỗi rỗng thay vì artifact thật

Bằng chứng:

- src/controllers/dubbing/parts/DubbingController_Workflow.cpp:47-54 dùng QFileInfo(...).isFile() khi tính workflow status.
- :1076-1078 của startStepByStep() chỉ xét masterAudioPath.isEmpty() và backgroundAudioPath.isEmpty().

Path không rỗng nhưng file đã bị xóa/hỏng có thể khiến step-by-step bỏ qua ingest/separation. Hai nguồn sự thật workflow sẽ mâu thuẫn.

Fix đề xuất dùng helper chung:

    static bool readableArtifact(const QString &path)
    {
        const QFileInfo info(path.trimmed());
        return info.isFile() && info.size() > 0 && info.isReadable();
    }

    const bool normalized = readableArtifact(m_project.masterAudioPath)
                         && readableArtifact(m_project.analysisAudioPath);
    const bool separated = normalized
                        && readableArtifact(m_project.vocalsAudioPath)
                        && readableArtifact(m_project.backgroundAudioPath);

startStepByStep(), workflowNodes(), preflight và adapter phải dùng cùng helper/contract. Với WAV cần thêm kiểm tra header, sample rate và channels tối thiểu.

### F-03 — P1 — Background bị bỏ qua im lặng trong mixer

src/dubbing/audio/AudioTimelineMixer.cpp:148-176 chỉ mix background nếu path không rỗng và QFileInfo::exists. Nếu path thiếu, không đọc được hoặc WAV rỗng, code vẫn có thể lưu output vocal-only.

Điều này chỉ an toàn khi người dùng đã chọn policy voice-only. Hiện chưa có policy required/voice-only được truyền xuyên từ UI tới mixer, dù tài liệu mô tả policy đó như đã có.

Fix đề xuất:

    enum class BackgroundPolicy { Required, VoiceOnly };

    if (backgroundPolicy == BackgroundPolicy::Required) {
        if (!readableArtifact(backgroundPath))
            return fail("Background stem is missing or unreadable.");
        const auto background = WavIO::loadAsFloat(backgroundPath);
        if (background.samples.isEmpty() || background.sampleRate <= 0 || background.channels <= 0)
            return fail("Background WAV is invalid or empty.");
    }

Chỉ VoiceOnly mới được bỏ qua Background; ghi policy vào journal/export manifest để giải thích vì sao output không có BGM.

### F-04 — P1 — Resume tin vào node.completed mà chưa verify artifact

src/workflows/graph/WorkflowGraphRunner.cpp:94-153 khôi phục restoredArtifacts từ node.completed tại :123-134, sau đó đặt m_orderIndex và chạy tiếp tại :136-145. Chưa có bước xác thực file, readable, schema, checksum hoặc thuộc đúng project.

Journal có thể còn nguyên nhưng file đã bị xóa, thay thế hoặc truncate. Resume khi đó truyền artifact hỏng cho node kế tiếp.

Mỗi artifact cần artifactId, typeId, contentHash, relativePath và schemaVersion. Trước khi khôi phục phải validate; nếu invalid thì ghi artifact.invalidated, invalidate từ producer node và chạy lại node đó. Cần test xóa, truncate và đổi checksum sau node.completed.

### F-05 — P1 — Full automatic mode bỏ qua toàn bộ review gate

src/controllers/dubbing/parts/DubbingController_Workflow.cpp:918-920 gán mode=never cho mọi core.review-gate trong runWorkflow().

Điều này bỏ qua transcript review, translation review và conflict review. Nếu sản phẩm có 8 task gồm review, hành vi cần được gọi tên rõ ràng.

Fix đề xuất thay hard-code bằng project policy:

    // automaticPolicy: unattended | review-required | review-on-conflict
    const QString policy = m_project.workflowRunPolicy;
    if (node.typeId == "core.review-gate") {
        node.parameters["mode"] = policy == "review-required"
            ? "required"
            : policy == "review-on-conflict" ? "on-conflict" : "never";
    }

UI phải cảnh báo trước khi chạy unattended. Nếu có unresolved conflict, policy an toàn phải dừng, không ép never tuyệt đối.

### F-06 — P2 — Normalize action row có nguy cơ nén/tràn ở panel hẹp

qml/components/dubbing/steps/DubbingNormalizeStep.qml:118-162 đặt primary button fill-width cùng listen button cố định khoảng 88 px. AppButton.qml:24 có minimum width theo content. Ở panel review nhỏ, label dài + spacing + listen button có thể vượt inner width.

DubbingSeparateStep.qml:71-116 đã có cấu trúc tốt: hàng 1 full-width, hàng 2 gồm Back 100 px và Continue fill. Normalize nên theo cùng pattern ở compact mode, hoặc chuyển listen thành icon-only có accessible name. Cần QML visual assertions cho review width 240, 280, 320, 360 và 420 px.

### F-07 — P2 — QML guard và text counter chưa sạch

Bằng chứng:

- qml/components/dubbing/steps/DubbingExportStep.qml:71-83 dùng trực tiếp exportPath.length và previewPath.length.
- qml/components/dubbing/panels/DubbingReviewPanel.qml:334 và :590 dùng trực tiếp segments.length.
- DubbingReviewPanel.qml:379 hiển thị %1 / %1, dẫn tới N / N chứ không phải current/total.

Nên tạo safeExportPath, safePreviewPath và segmentCount ở root component; counter nên là Segment current / total hoặc N phân đoạn.

### F-08 — P2 — QProcess cần watchdog, không cần worker thread riêng

Phản hồi trước là đúng về architecture: FFprobe chạy qua QProcess async, callback trên Qt event loop và không block UI; thiếu worker thread riêng không phải lỗi.

Tuy nhiên async không đồng nghĩa không thể treo. MediaIngestService, MediaToolService và validation trong DubbingExportJob cần deadline/watchdog. Khi FFmpeg/FFprobe không thoát, UI phải nhận failure có hướng dẫn.

Dùng QTimer non-blocking cho từng process; timeout thì giữ raw stderr, terminate(), sau grace period kill(), xóa staging output và phát error code MEDIA_TOOL_TIMEOUT. Timeout nên theo loại task/video.

### F-09 — P2 — AudioPlayer::playPcm() cần validate trước QAudioSink

src/audio/player/AudioPlayer.cpp:221-257 đã stop session cũ, nhưng cần chặn sampleRate <= 0, data rỗng, channel count không hợp lệ và format không được audio device hỗ trợ.

Fix:

    if (pcm.isEmpty() || sampleRate <= 0 || channels <= 0) {
        emit errorOccurred("AUDIO_PREVIEW_INVALID_INPUT", guidance);
        return false;
    }
    if (!m_audioDevice.isFormatSupported(format)) {
        emit errorOccurred("AUDIO_PREVIEW_UNSUPPORTED_FORMAT", guidance);
        return false;
    }

Stop/generation guard hiện tại là đúng hướng: request cũ bị vô hiệu, session cũ stop/deleteLater, callback stale không restart. Cần stress test đổi voice/stem liên tục và chứng minh không có hai QAudioSink active.

### F-10 — P1 tài liệu — README và evidence stale

doc/README.md:31-32 còn dùng tên file/class không tồn tại. Nên sửa thành:

    Normalization: MediaIngestService.cpp, DubbingController, DubbingJobRunner
    Separation: DubbingJobRunner, DubbingWorkflowAdapter, configured separation runner
    Playback: AudioPlayer.cpp

Thay claim CTest 39/39 bằng:

    CTest 40/40 trên existing configured binaries ngày 2026-08-27;
    chưa phải source rebuild trong lần audit này.

Sau source rebuild, thêm compiler, configuration, build hash và timestamp. Không ghi 10/10 nếu acceptance criteria chưa được chạy.

### F-11 — P1 kế hoạch Colab — lazy lease chưa khớp connection precondition

Proposal và kế hoạch unified worker đúng hướng khi dùng coordinator tùy chọn, exact worker route, bearer token, capability contract và lease lifecycle. Nhưng:

- src/remote/colab/ColabSession.cpp:335-555 yêu cầu health/capabilities và model loaded=true ngay khi verify session.
- notebooks/workers/LA_STUDIO_UNIFIED_DUBBING_COORDINATOR.py:218-330 hướng tới prepare/start worker theo lease/lazy lifecycle.

Nếu worker chưa load model lúc desktop connect, session fail dù lease scheduler có thể khởi động model sau đó.

Tách trạng thái:

    DISCONNECTED → CONNECTING → AUTHENTICATED → CAPABILITIES_KNOWN
    → LEASE_REQUESTED → MODEL_STARTING → READY → RUNNING → RELEASING

health chỉ chứng minh process sống và revision đúng; capabilities chứng minh route/model có thể nhận; loaded=true chỉ bắt buộc trước inference hoặc sau lease. UI phải hiện Starting model thay vì worker unavailable.

Không ghi zero-config, 100% free, 1.5–2 phút guaranteed hoặc không giới hạn nếu không có SLA/hạ tầng sở hữu riêng.

### F-12 — P2 — Phân loại error bằng substring dễ sai

AppErrorCatalog và AppController đã đúng mục tiêu: raw details ở log, summary/guidance/action route ở UI. Nhưng phân loại dựa nhiều vào substring tiếng Anh có thể hỏng khi engine đổi wording hoặc localization.

Mọi service nên phát structured error gồm code, source, technicalDetails và context. Catalog map theo code; raw message chỉ là chi tiết. Các code quan trọng cần có mapping/CTA tests.

## 6. Xác nhận lại ba phản hồi kỹ thuật trước

### QProcess FFprobe

Đúng: audit không nên coi thiếu worker thread riêng cho FFprobe là lỗi. QProcess không block UI; signal finished và readyRead chạy qua event loop. Điểm cần bổ sung là timeout/watchdog và cleanup, không phải chuyển FFprobe sang QThread máy móc.

### voiceSelected

Source hiện tại: qml/components/shared/VoiceGalleryDialog.qml:21 khai báo 5 tham số và :132 emit 5 tham số. ReferenceInputBox.qml:487 và TtsSettingsPanel.qml:904 nhận 4 tham số vì không dùng voiceId; handler Dubbing nhận đủ 5. Không có bằng chứng build fail/crash do handler không khai báo tham số cuối. Đây không nên là P0; nên chuẩn hóa handler hoặc ghi rõ tham số optional.

### setWorkflowMode()

src/controllers/dubbing/DubbingController.h:455 nằm sau private: và chỉ được gọi từ C++ hiện tại. Nếu QML gọi method không phải Q_INVOKABLE/slot, Qt thường warning rồi bỏ qua, tạo silent no-op chứ không crash. Tree hiện tại chưa thấy QML caller trực tiếp. Nếu cần cho QML, expose public Q_INVOKABLE có contract thay vì gọi private method.

## 7. Thiết kế để luồng dubbing chạy chắc chắn

“Chắc chắn chạy được” phải được định nghĩa bằng invariant có thể kiểm thử.

### Preflight bắt buộc

1. Source media là file thật, đọc được, có stream hợp lệ và duration > 0.
2. FFmpeg/FFprobe resolve được và chạy được probe sample.
3. Output directory tạo/ghi được và đủ dung lượng.
4. Source/target language hợp lệ.
5. TTS voice là durable ID và runtime/model đúng language.
6. Remote route, token, contract version, revision, capability/model được verify nếu chọn remote.
7. Policy rõ: review-required, review-on-conflict, unattended; BackgroundPolicy là required hoặc voice-only.
8. Không có run khác, recovery run chưa xử lý hoặc staging không rõ chủ sở hữu.

### Contract từng node

Mỗi node phải khai báo required inputs, output artifact type/schema, validator, retry policy, cancellation behavior và user-facing error code/CTA.

Node chỉ emit completed sau khi output được ghi vào staging, flush/close, validate, commit atomic và cập nhật manifest/checksum. Không dùng path string không rỗng làm bằng chứng hoàn thành.

### Recovery và cancellation

- Journal ghi run.started, node.started, node.completed, artifact.committed, node.failed và run.cancelled.
- Resume validate artifact trước khi khôi phục node completed.
- Cancel truyền xuống worker/process, chờ terminal hoặc kill sau deadline rồi mới kết thúc run.
- Callback chỉ được commit khi runId và generation còn đúng; callback muộn không được ghi đè project.

### Local/remote route

Local và Colab dùng cùng logical contract. Remote không sẵn sàng thì hiển thị lỗi và CTA reconnect/configure; không dùng normalized source như Vocals, không dùng master mixed audio như Background và không đổi payload âm thầm.

## 8. Ma trận test cần bổ sung

| Nhóm | Test bắt buộc | Điều kiện đạt |
|---|---|---|
| Project schema | migrate 13→14, project cũ thiếu vocals, manifest không khớp | Không đoán sai Vocals; yêu cầu rerun separation |
| Artifact | Xóa/truncate/đổi checksum sau node.completed | Resume invalidate đúng node và chạy lại |
| Workflow gate | Path không rỗng nhưng file missing/unreadable | Không skip ingest/separation |
| Separation | Chỉ có vocals, chỉ background, output là master mixed | Fail có guidance, không complete |
| Mixer | Background missing/empty/invalid với required | Fail; voice-only chỉ khi policy rõ |
| Audio | Đổi 20 voice/stem trong thời gian ngắn | Chỉ audio cuối phát; không overlap/leak |
| Process | FFmpeg/FFprobe không thoát | Watchdog kill, cleanup, error CTA |
| QML layout | Review panel 240/280/320/360/420 px; cửa sổ 1280x720 | Không overlap; text elide; action reachable |
| Responsive | 1280x720, 1920x1080, 2560x1440, 3840x2160, scale 125/150/200% | Không clipping; timeline/list scroll đúng |
| E2E local | Sample 1–2 giây: ingest → normalize → separation fixture → STT fixture → translation fixture → TTS fixture → mix → export | File output probe được, manifest đủ, reopen được |
| E2E remote | Health → capabilities → lease → inference → release; token sai/model unavailable | State và CTA đúng, không silent fallback |
| Fault injection | Kill app/process mỗi node, mất file, hết dung lượng, mất mạng | Recovery an toàn, không corrupt project |

## 9. Đánh giá kế hoạch tối ưu Colab

Kế hoạch nằm ở:

- docs/superpowers/plans/2026-08-27-unified-colab-10-10-plan.md
- docs/superpowers/specs/2026-08-27-unified-colab-10-10-design.md
- doc/unified_colab_worker_proposal.md

Kiến trúc coordinator tùy chọn + exact worker + bearer token + capability contract + lease lifecycle là khả thi. Feasibility runtime hiện tại nên ghi 6/10 có điều kiện; 10/10 là mức của kế hoạch sau implementation, acceptance tests, rollback và operational evidence.

Các nguồn chính thức và giới hạn phải giữ trong tài liệu:

- Google Colab không cam kết tài nguyên, GPU availability hoặc usage limit cố định; free tier không phải nền tảng SLA cho worker production. Xem Colab FAQ: https://research.google.com/colaboratory/intl/en-GB/faq.html
- Cloudflare Quick Tunnel cấp hostname ngẫu nhiên trycloudflare.com, phù hợp development/testing và không có SLA/uptime guarantee; có giới hạn request. Xem Cloudflare Quick Tunnels: https://developers.cloudflare.com/cloudflare-one/networks/connectors/cloudflare-tunnel/do-more-with-tunnels/trycloudflare/
- torch.cuda.empty_cache() chỉ giải phóng cached blocks chưa dùng; không giải phóng tensor còn sống và không giải quyết mọi OOM. Xem PyTorch CUDA memory management: https://docs.pytorch.org/docs/main/notes/cuda.html và empty_cache: https://docs.pytorch.org/docs/main/generated/torch.cuda.memory.empty_cache.html
- CPU/disk offload của Accelerate có overhead; phải benchmark theo model/latency thực tế. Xem Hugging Face Accelerate: https://huggingface.co/docs/accelerate/main/concept_guides/big_model_inference
- FastAPI cần phân biệt async def cho awaitable I/O và def cho blocking library; inference blocking phải ở worker/thread/process hoặc queue. Xem FastAPI async: https://fastapi.tiangolo.com/async/
- Tailscale Funnel public service ra Internet nhưng phụ thuộc cài đặt/account/policy; không phải zero-config universal tunnel. Xem Tailscale Funnel: https://tailscale.com/docs/features/tailscale-funnel

## 10. Thứ tự fix khuyến nghị

### P1 — phải làm trước khi tuyên bố workflow ổn định

1. Tách vocalsAudioPath, bump schema và migration an toàn.
2. Đồng nhất artifact validator cho workflowNodes, startStepByStep, preflight và resume.
3. Thêm BackgroundPolicy, fail closed khi background required.
4. Validate artifact khi resume; invalidate từ node sản sinh artifact hỏng.
5. Đổi automatic review behavior thành policy có tên; unresolved conflict không được silent.
6. Đồng bộ 16 tài liệu và README: class thật, CTest 40/40, evidence boundary và claim voice-only.
7. Sửa ColabSession để loaded state phù hợp lease/lazy startup, hoặc điều chỉnh proposal để không hứa lazy load khi code chưa hỗ trợ.

### P2 — cần làm để đạt quality 10/10

1. Watchdog cho mọi long-running QProcess.
2. AudioPlayer::playPcm validation và rapid-switch stress test.
3. QML safe guards, counter đúng nghĩa và compact action layout.
4. Structured error codes thay cho substring classification.
5. Visual regression matrix và full local media E2E.
6. Live Colab contract test có GPU, token, cleanup và release.

## 11. Acceptance criteria cuối cùng

Chỉ nâng tất cả mục lên 10/10 khi đồng thời đạt:

- Source build sạch trên configuration phát hành.
- CTest mới nhất pass 100%, có build hash và timestamp.
- Full local E2E chạy từ media input đến video export và reopen project thành công.
- Mọi output node có artifact schema/checksum/validator; resume sau fault injection không chạy trên file hỏng.
- Không còn path semantic collision giữa analysis, vocals và background.
- Missing stem không biến thành Ready và không fallback sang mixed audio.
- QProcess không thể chờ vô hạn.
- Audio preview không overlap sau rapid switch.
- QML visual matrix không có clipping/overlap ở resolution và Windows scale đã nêu.
- Error log giữ raw technical details; UI hiển thị guidance theo typed error code và CTA thực hiện được.
- Colab live test xác nhận health, capability, lease, inference và release; nếu chưa có runtime live thì tài liệu phải ghi not verified.

## 12. Trạng thái audit

Kết luận hiện tại: nền tảng tốt, test/contract hiện có pass, nhưng chưa đạt 10/10 do các P1 F-01 đến F-05 và khoảng trống E2E/visual/live-Colab. Không có P0 được chứng minh. Sau khi xử lý theo thứ tự trên, cần chạy lại ma trận test và cập nhật 16 file theo đúng source evidence mới.
