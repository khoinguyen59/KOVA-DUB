# AI agent response — final reliability and delivery recheck

## 2026-09-05 — Recheck độc lập workflow Dubbing, cập nhật từng đợt

- Baseline: `7489f1aa127b8297661f48a187e1aecf3ce34864` (`main`).
- Phạm vi: kiểm tra lại source, regression và đường đi dữ liệu; không mặc định coi báo cáo lần trước là bằng chứng lần này. Không build EXE trong đợt audit này.
- Quy tắc ghi nhận: cập nhật mục này ngay sau từng nhóm test nhỏ, kể cả khi test thất bại hoặc còn thiếu môi trường. Tách kiểm chứng runtime khỏi test mock/source-contract.
- Các file untracked có sẵn được giữ nguyên, không thuộc thay đổi audit.

**Kết luận hiện tại: chưa nghiệm thu toàn workflow.** Đã hoàn tất các đợt recheck tự động/source bên dưới và cập nhật ngay theo từng đợt. Có **12 phát hiện RC-01…RC-12** (lỗi/risks code, UX và thiếu phủ test); test có sẵn vẫn xanh. Ưu tiên xử lý mất/tái dùng nhầm output theo project, manual audio bị bỏ qua, validation trước cancel, rồi giới hạn mix lớn và UI blocking. Lần này chỉ ghi nhận/đề xuất, không sửa product code, không build hay push bản phát hành.

| Đợt | Phạm vi | Trạng thái |
| --- | --- | --- |
| A | Lưu/mở lại dự án, mặc định ngôn ngữ, tách dữ liệu dự án | 11 ca PASS; output mặc định còn chung path RC-10 |
| B | Upload không cần Colab, skip, dữ liệu cho task kế tiếp | 8 ca PASS; phát hiện RC-01/02/03 |
| C | STT/OCR độc lập, model gating, kết nối và khớp transcript | G11 đã chứng minh overlap hai chiều, upload/cancel route-scoped và chặn callback cũ ghi sang project mới; vẫn cần live Colab acceptance |
| D | Dịch, giọng/TTS, align, hướng dẫn AI và đường dẫn | G12 đã khóa snapshot handoff unique, prompt path chính xác và UI Prepare/Copy/Open contract; còn acceptance desktop |
| E | Mix/export, thời lượng, sample rate và tính toàn vẹn output | 12 ca PASS; thêm RC-07/08/09/10/12 |
| F | UI/UX, hủy tác vụ, smoke và regression tổng | CTest 41/41, QML lint và notebook contracts PASS; chưa nghiệm thu live |

### Nhật ký từng đợt

#### A — Project persistence (đã chạy)

- Chạy 11 ca `TestDubbingProject`: JSON round-trip/legacy, di chuyển thư mục, Save As, reopen/close-resume, transcript artifact persistence, chống trùng tên tự động/không tên, ngôn ngữ preflight, import không tự chạy.
- Kết quả QtTest: **13 PASS, 0 FAIL, 0 SKIP** (gồm init/cleanup; 11 ca nghiệp vụ).
- Log: `out/audit-2026-09-05-dubbing/A-project.log`.
- Các dòng QCRITICAL ở ca preflight là nhánh từ chối cấu hình thiếu được chủ động thử, không phải suite thất bại.
- Phạm vi: filesystem tạm + controller thật trong unit harness, engine mock; chưa chứng minh khôi phục sau mất điện/kill bất ngờ hay xử lý mọi project cũ của người dùng.

#### B — Upload/skip/downstream (đã chạy)

- 8 ca mục tiêu: contract audio theo vai trò/tên tự do; upload STT/OCR không Colab; lưu/reopen dữ liệu upload cho Translate; picker/skip contract; skip không khóa nhánh còn lại; kiểm tra cue bắt buộc trước mix; import subtitle có timeline.
- QtTest: **10 PASS, 0 FAIL, 0 SKIP** (8 ca nghiệp vụ). Log: `out/audit-2026-09-05-dubbing/B-upload.log`.
- Đã xác nhận import STT/OCR đưa nội dung vào `segments` và lưu lại, không chỉ hiện nút. Tuy nhiên test UI là source-contract, chưa phải click thật.
- Đọc thêm nhánh lỗi và upload lại sau khi đã sinh output: phát hiện các vấn đề dưới đây. Test hiện có PASS không phủ hết các nhánh này.

#### C — STT/OCR và gating (đã chạy)

- QtTest **16 PASS, 0 FAIL, 0 SKIP** (14 ca nghiệp vụ). Log: `out/audit-2026-09-05-dubbing/C-transcript.log`.
- Có kiểm tra thiếu setup→yêu cầu model, sai model worker→từ chối, notebook revision, STT-only/OCR-only, route độc lập, ưu tiên OCR, conflict cần review.
- **Không coi các tên test có chữ “combined” là bằng chứng chạy đồng thời**: đọc thân test cho thấy chúng kiểm tra từ chối cách gọi reconcile cũ. Worker ở các ca network là server mock localhost, không phải GPU Colab sống.

#### D — Translation/TTS/align/handoff (đã chạy)

- QtTest **14 PASS, 0 FAIL, 0 SKIP** (12 ca nghiệp vụ). Log: `out/audit-2026-09-05-dubbing/D-tts-align.log`.
- Kiểm tra truyền ngôn ngữ sang voice node, reference-clone VieNeu/OmniVoice, chặn runtime sai, áp giọng từng cue, đợi synthesis xong, timing ripple/undo, không fallback giữa Gateway và Colab. TTS dùng mock; chưa nghe/đánh giá giọng thật.
- RC-02 được xác nhận thêm ở consumer: `DubbingController_Workflow.cpp:39-46` chỉ cần `clipPath` tồn tại để đếm cue đã sinh; `DubbingJobRunner.cpp:883-907` ưu tiên clip cũ. `AudioTimelineMixer.cpp:287-306` không kiểm fingerprint nội dung mới hay state stale. Vì vậy đây là đường đi có thể dùng audio cũ, không chỉ thiếu cập nhật nhãn UI.

#### E — Mix/export (đã chạy)

- QtTest **14 PASS, 0 FAIL, 0 SKIP** (12 ca nghiệp vụ). Log: `out/audit-2026-09-05-dubbing/E-mix-export.log`.
- Kiểm tra mix async, vocal stem riêng, giữ source duration khi thoại hết sớm, resample, sidechain ducking, gain, timeout/validation/atomic media commit; CapCut draft có tài nguyên tự chứa và không gắn nhầm analysis thành vocals.
- Có warning `QFile::remove: Empty or null file name` trong test timeout export; test vẫn pass nhưng cần dọn nhánh cleanup để log không nhiễu. Chưa coi CapCut mở/import thành công vì chưa chạy CapCut thật.

#### F1 — Regression toàn bộ và production QML smoke (đã chạy)

- Chạy mới CTest với timeout tối đa 150 giây/test: **41/41 PASS**, tổng **34,55 giây**; log/XML trong `out/audit-2026-09-05-dubbing/F-full-ctest.*`.
- `TestDubbingProject` 15,81 giây; `TestDubbingWorkspaceContract` 0,27 giây; `TestRuntimeHostProtocol` 1,68 giây; `QmlRouteSmoke` 7,59 giây; `TestSubtitleOcrController` 32,91 giây.
- Đây là offscreen `Main.qml` production + test harness; không phải buổi click desktop có quay video. Smoke hiện thử Dubbing tại 1024×720, 1280×800, 1600×900 (`qml/Main.qml:447-451`), **không có 4K**.
- Không suy ra “không lag với video dài” từ thời gian suite. Thử khởi chạy process với argv cho 200 cue phát hiện RC-09; source upload vẫn có blocking RC-08; shutdown còn RC-11.

#### F2 — Notebook/worker contracts (đã chạy)

- Bảy lệnh validator/test đều exit 0: model bindings **31/31**, generated notebooks **32/32**, repository-dependency scan **1 test**, embedded worker tests **5 tests**, embedded payload **2/2**, Unified bundle **3 tests**, Unified notebook contract PASS.
- Log riêng từng lệnh: `out/audit-2026-09-05-dubbing/F-<tên-script>.log`.
- Xác nhận source/bundle khớp và không cần tải worker code từ commit GitHub ứng dụng trong các contract được kiểm; chưa chứng minh `pip install`, CUDA/model inference hay tunnel trên Colab hiện tại. Không dùng lại token cũ từ lịch sử chat.

#### Bổ sung F1/F2 — Kiểm tra log không cắt ngắn

- Chạy riêng toàn bộ `TestDubbingProject`: **137 PASS, 0 FAIL, 5 SKIP**. Năm ca phoneme/duration bị skip vì unit runtime không tìm thấy eSpeak; không được diễn giải 41/41 CTest là toàn bộ subtest đã chạy. Log đầy đủ: `F-dubbing-full.log`.
- QML lint exit 0; log `F-qml-lint.log`. Smoke ghi **19 event** trong `F-qml-trace.json` (event từ automation QML, không phải ảnh/video desktop).
- Đã staging eSpeak có sẵn trong portable vào runtime test và chạy lại năm ca: **7 PASS, 0 FAIL, 0 SKIP** (gồm init/cleanup). SHA-256 DLL nguồn/đích giống nhau: `e737572df0a35a32b7bd444537c661c1c916b13b0b91351030c7f1d531307beb`. Log `F-espeak-staged.log`. Không build/đổi EXE; không còn bỏ sót năm ca này trong phạm vi lần recheck.
- Sau khi cập nhật checklist/MD, chạy lại `TestDubbingWorkspaceContract`: **20 PASS, 0 FAIL, 0 SKIP** (18 ca + init/cleanup), log `F-workspace-final.log`. Việc qua source-contract không đóng 12 phát hiện trên đường runtime/source vừa ghi.

### G — Khắc phục RC đang thực hiện, cập nhật theo từng vòng test (2026-09-05)

| Vòng | Bằng chứng | Kết quả | Trạng thái |
|---|---|---|---|
| G1 | Regression test import artifact giả dạng media/subtitle | Hai nhánh từ chối file hỏng đã xanh; controller không huỷ job hay thay đổi artifact hiện có trước khi kiểm tra xong. | Đã xử lý một phần RC-01/RC-03/RC-08 |
| G2 | Regression test import audio hợp lệ vào staging riêng theo project | Phát hiện lỗi Windows: so sánh prefix đường dẫn dùng dấu phân cách không ổn định, nên file staged bị copy lại thay vì được commit đúng file đã kiểm tra. | Đã vá, chờ test xác nhận |
| G3 | `TestColabTranslationRunner::cancellationAbortsDirectWorkerRequest` và `TestColabAlignmentRunner::testRejectsNonMonotonicAndCancelledResponses` trong full CTest | Tái hiện timeout 5 giây, sau đó `QThread: Destroyed while thread is still running`. Đây là bằng chứng trực tiếp rủi ro RC-11. | Đã tái hiện |
| G4 | CTest sau khi request đang chờ tự poll cancellation token và abort reply với deadline | `TestDubbingProject`, Translation cancellation và Alignment cancellation xanh trong full suite; không còn `QFATAL` trong log lượt chạy 13:36. | Đã giảm RC-11; vẫn phải xử lý tất cả destructor `wait()` vô hạn và chạy stress lặp |
| G5 | CTest sau import async, invalidation transcript/audio và policy skip ở mixer | **41/41 PASS**, 91,44 giây. Test hồi quy mới xác nhận sửa `targetText` xóa cue audio/cached fingerprint/preview/export cũ; cue `skipped=true` không lọt vào mix dù clip còn trên đĩa. | RC-02 và RC-07 đã được đóng bởi regression; RC-08/09/10/11/12 còn tiếp tục |

Không được đánh dấu vòng audit là đạt chỉ vì hai regression test upload xanh. RC-11 phải có test huỷ worker xanh lặp lại, đồng thời toàn bộ CTest phải xanh, trước khi tạo bản phát hành.

### Lỗi và rủi ro mới

**RC-01 — P1 — Upload audio/video chỉ kiểm phần mở rộng, chưa kiểm nội dung giải mã được.**

- Source: `src/controllers/dubbing/parts/DubbingController_Artifacts.cpp:387-405, 523-637`. Sau kiểm số file/extension và copy, nhánh audio/export gán path và đánh dấu hoàn thành mà không probe stream/duration.
- Bằng chứng runtime hiện có: `manualSeparationUploadAcceptsRoleBasedAudioNames` PASS dù fixture `.mp3/.ogg` được ghi bằng chuỗi chữ `dialogue audio fixture` / `background audio fixture` (`tests/dubbing/test_DubbingProject.cpp:3320`). Đây không phải file âm thanh hợp lệ nhưng import vẫn trả true.
- Hậu quả: user được cho qua, lỗi chỉ phát sinh ở STT/mix/export; trạng thái hoàn thành tạo hiểu nhầm. Không liên quan yêu cầu cho phép tên file tự do.
- Đề xuất: probe/decode bất đồng bộ có timeout trước commit; xác nhận có audio/video stream đúng vai trò, duration > 0; chỉ hủy worker và thay state sau validation thành công. Giữ quyền dùng tên bất kỳ. Thêm fixture âm thanh thật và ca file rỗng/đổi đuôi/hỏng.

**RC-02 — P1 — Nhập lại bản dịch chưa vô hiệu hóa audio/output cũ.**

- Source: `DubbingController_Artifacts.cpp:592-613`: clone nguyên map segment, chỉ thay `targetText`, `translationSource`, `state`; không xóa `clipPath`, timing/duration của audio cũ, preview/dubbed/export hoặc output bước sau.
- Bằng chứng hiện tại: đã đối chiếu cả producer và consumer ở đợt D/E (xem nhật ký D); chưa có test động riêng chứng minh export phát lại clip cũ.
- Nguy cơ: sửa lời dịch nhưng audio cũ vẫn được xem là khả dụng. Đề xuất version/fingerprint cho phụ thuộc transcript→translation→TTS→align→mix→export; đánh dấu stale và buộc tạo lại đúng output phụ thuộc, không xóa file người dùng.

**RC-03 — P1 — Upload subtitle không hợp lệ có thể hủy worker trước khi parse.**

- Source: `DubbingController_Artifacts.cpp:553-556` gọi `cancelMatchingWorker()` trước `importSubtitlesInternal()`; nội dung SRT/TXT được parse ở `:110-128`, sau hủy. Đối chiếu nhánh Translate ở `:573-605` đã parse trước hủy.
- Trigger: STT/OCR đang chạy; chọn `.srt` hỏng hoặc TXT không khớp số segment. Extension đúng nên qua vòng ngoài, worker bị hủy, rồi upload bị từ chối. Source-confirmed; chưa chạy worker thật cho ca này.
- Đề xuất parse + validate vào dữ liệu tạm trước, rồi commit và cancel nhánh tương ứng. Test cả STT/OCR và bảo đảm worker sibling không bị ảnh hưởng.

**RC-04 — P1, regression controller đã được khóa; còn yêu cầu nghiệm thu Direct Colab thật.**

- `tests/dubbing/test_DubbingProject.cpp:4360`: `transcriptOcrRunControlRemainsAvailableAlongsideStt` chỉ đọc chuỗi source.
- `:6683,6746`: hai ca mang tên “combined” thực tế assert `failed.count()==1` và thông báo “Run STT and OCR independently”, không chạy hai job.
- G10 đã bổ sung controller test thật với worker STT localhost có barrier điều khiển được. Cả OCR→STT và STT→OCR được giữ chồng thời gian, OCR hoàn thành khi STT còn chạy, rồi STT được giải phóng. Mỗi lượt đều giữ `sttSegments` + `ocrSegments`, lưu project thành công và không trả lỗi busy của sibling.
- G11 bổ sung ba ca bất đồng bộ còn thiếu: upload OCR khi STT chạy vẫn giữ STT; upload STT khi cả hai chạy chỉ huỷ STT và OCR tiếp tục; callback import artifact hoàn thành sau khi đổi project bị từ chối, không được ghi stems/output vào project mới. Các ca đều chạy controller thật và chờ signal hoàn tất, không phải source-contract.

**RC-05 — P1 — Upload TTS/Align mới có thể bị bỏ qua khi project còn clip TTS cũ.**

- Source: `DubbingController_Artifacts.cpp:614-622` chỉ đổi `m_runner->dubbedVocalPath`, không clear/disable các `segments[].clipPath`; `DubbingJobRunner.cpp:883-900` chỉ dùng full-program upload nếu **không có bất kỳ clip segment nào tồn tại**.
- Trigger cụ thể: đã sinh ít nhất một clip TTS → upload bản lồng tiếng hoàn chỉnh ở Synthesize hoặc Align → Mix. Có clip cũ khiến nhánh full-program upload không chạy.
- Bằng chứng: trace producer/consumer trực tiếp; chưa có ca regression runtime riêng trong bộ hiện hành.
- Đề xuất lưu rõ audio source mode (`segment-clips`/`manual-program`) và revision; khi upload được nhận, mixer phải ưu tiên mode người dùng đã chọn, không suy ra bằng existence của clip cũ. Thử với project có đủ clip và chỉ có một clip.

**RC-06 — P2, controller/UI contract đã được khóa; còn acceptance desktop.**

- G12 thay đường dẫn stage-global bằng một snapshot bất biến cho mỗi lần bấm tại `.workflow-artifacts/<project>/ai-handoff/<UTC>-<uuid>/`. App ghi ba input (`01` STT, `02` OCR, `03` canonical) bằng write-then-rename cùng volume; mỗi lượt dành riêng `04-merged-transcript.srt` và `05-translation-vi.srt` cho IDE.
- `prepareTranscriptAiHandoff()` nay trả `handoffDirectory`, prompt ngắn có toàn bộ path tuyệt đối, ba input và hai output. `Dữ Liệu & Handoff` có Prepare, Copy prompt và Open folder; không còn yêu cầu IDE tự dò repository, cache hay tên file stage.
- Guide duy nhất giải quyết mâu thuẫn timeline: OCR là cue/timestamp grid khi tồn tại; STT chỉ là reference/gap-filler. Nếu OCR không có thì canonical/STT grid là cố định. IDE ghi trực tiếp merged và Vietnamese output, giữ timestamp/count/order, nội dung không rỗng và quan hệ xưng hô/ngữ cảnh.
- Regression `transcriptAiHandoffUsesUniqueProjectScopedSnapshots` tạo hai project có cùng tên hiển thị và hai handoff trong project đầu; kiểm file input thật, scope tuyệt đối, không trùng folder và prompt nêu đúng mọi path. Workspace contract kiểm UI caller/guidance. Acceptance desktop còn phải click ba nút trong EXE đóng gói.

**RC-07 — P1 — Cue đã đánh dấu skip vẫn có thể phát trong mix nếu còn clip.**

- `DubbingController_Workflow.cpp:11-19` bỏ cue skip khỏi điều kiện bắt buộc tạo audio. Nhưng `AudioTimelineMixer.cpp:287-307` duyệt mọi segment có clip, không xét `skip`, `skipped` hoặc `state == skipped`.
- Đề xuất dùng cùng policy tại readiness và renderer; test render một cue có clip bị skip và đo vùng đó im lặng (khi không có background). Source-confirmed, chưa có regression động riêng.

**RC-08 — P1, UX/performance — Upload/copy lớn vẫn chạy đồng bộ trên luồng UI.**

- `DubbingController_Artifacts.cpp:434-471` gọi `replaceCopy` trực tiếp trong Q_INVOKABLE; helper tại `DubbingController.cpp:236-253` dùng `QFile::copy` đồng bộ. Chưa có task worker, progress, cancellation cho bước copy. Cùng helper còn dùng export review package.
- Hậu quả phụ thuộc kích thước/ổ đĩa: giao diện không xử lý event trong lúc copy WAV/FLAC lớn hoặc file trên ổ chậm. Chưa đo thời gian copy bằng dữ liệu thật của người dùng trong đợt này.
- Đề xuất staged copy bất đồng bộ, progress/cancel, kiểm đủ dung lượng, validate rồi atomic commit; giữ file/state trước đó nếu lỗi/hủy. Đo heartbeat UI và p95 latency với file 100 MB/1 GB và ổ chậm, không nghiệm thu bằng fixture vài byte.

**RC-09 — P1 — Timeline nhiều cue có thể vượt giới hạn command line Windows, FFmpeg không khởi động.**

- `AudioTimelineMixer.cpp:154-178` thêm một `-i path` và một chuỗi filter cho từng clip, đưa toàn bộ graph vào `-filter_complex` trên command line, không chia batch.
- Đã tái hiện giới hạn process thực tế với argv theo cấu trúc này, 200 cue/đường dẫn project dài: **44.188 ký tự**, `Process.Start` thất bại “The filename or extension is too long.” trước khi FFmpeg đọc media. Log: `out/audit-2026-09-05-dubbing/E-large-argv.log`.
- Đây là reproducer của việc khởi chạy process trên Windows, không phải một render 200 cue thành công trong app. Ngưỡng cue phụ thuộc độ dài path/filter.
- Đề xuất graph script + phân lô/submix giới hạn input (chỉ chuyển graph sang file chưa giải quyết vô hạn `-i`); kiểm 200/500/1000 cue với path Unicode dài, giới hạn tiến trình/file handle và memory.

**RC-10 — P1 — Default mix/preview không tách theo project, có thể ghi đè âm thanh dự án khác.**

- `DubbingController_Project.cpp:447-454` tạo các `A.ladub.json`, `B.ladub.json` cùng thư mục `projects`.
- `DubbingExportJob.cpp:114-116` khi không chọn path, render vào `QFileInfo(projectPath).absolutePath() + /preview.wav`; vocal stem cũng suy ra từ cùng path. `DubbingController_Artifacts.cpp:843-858` truyền nguyên path mặc định xuống runner.
- Trigger: A và B cùng folder, đều Mix bằng path mặc định → cùng đích preview/vocal; mix B thay byte mà project A vẫn trỏ tới. `openProject` còn nhận `preview.wav` lân cận như legacy fallback (`DubbingController_Project.cpp:238-241`) mà không xác thực thuộc project nào.
- Source-confirmed; test persistence hiện tại dùng từng QTemporaryDir riêng nên không phát hiện chia sẻ output này.
- Đề xuất dùng artifact root theo project identity và revision cho mọi derived media; migration fallback chỉ khi xác minh ownership. Thêm test A/B cùng folder, hash và audio của A không đổi sau khi render B.

**RC-11 — P1, rủi ro shutdown — Một số destructor còn chờ thread vô hạn.**

- `DubbingTranslationJob.cpp:40-49`, `DubbingSynthesisJob.cpp:114-125` gọi `m_remoteThread.wait()` không deadline sau `quit()`; cancellation chủ yếu được gửi qua queued invocation/shared flag.
- Nếu worker đang kẹt trong tác vụ blocking không trả về/check cancel, event loop không xử lý queued cancel và destructor vẫn chờ. Đây là nguy cơ source, chưa tái hiện treo mạng/codec trong lần này; không kết luận mọi lần Close đều treo.
- Đề xuất shutdown có thời hạn + cooperative cancellation thực sự tại I/O; lifetime worker được quản lý an toàn, không xóa QThread đang chạy hoặc terminate tùy tiện. Fault injection worker không trả lời, cancel ở từng phase, đo thời gian Close.

**RC-12 — P2, audio UX — Điều khiển tiếng gốc và nhạc nền đang dùng chung gain.**

- `DubbingController_Stages.cpp:42-45` mặc định `originalGainPercent=0`, `dubbedGainPercent=100`; `AudioTimelineMixer.cpp:213-215` nhân background với `originalGain`.
- Vì vậy default 0% tiếng gốc cũng làm mất nhạc nền đã tách/upload, và tăng Original ở chế độ đã Separate thực chất tăng background chứ không pha tiếng người gốc. Test `audioMixHonorsOriginalAndDubbedLevels` (`test_DubbingProject.cpp:5571-5610`) còn kiểm đúng hành vi gộp gain này.
- Cần chốt/ghi nhãn rõ semantics; theo workflow Vocals/BGM/Dubbed nên có gain riêng, mặc định tắt original vocals nhưng giữ BGM. Đây là không khớp ý nghĩa điều khiển, không phải lỗi filter sidechain đã sửa.

### Điều kiện đóng đợt lỗi và nghiệm thu tiếp theo

1. RC-10, RC-02, RC-05: test cùng một project sau khi thay đầu vào và hai project chung thư mục; đối chiếu hash/path và âm thanh xuất thực tế, không chỉ nhãn completed.
2. RC-01/03/04/07: test negative-input + worker concurrency/cancel có barrier, và render kiểm tra cue skip; quét mọi task có cùng đường xử lý.
3. RC-08/09/11: đo event-loop heartbeat, memory và thời hạn cancel/close với file lớn, hàng trăm cue và worker bị kẹt. Không chỉ chạy lại fixture nhỏ.
4. RC-06/12: hoàn thiện handoff UI/prompt bằng đường dẫn app tạo, test đa dự án/đa lần làm việc; xác nhận semantics ba track audio bằng nghe/đo output.
5. Nghiệm thu live còn thiếu: thao tác upload bằng file picker, phát/pause/seek/ROI với media dài, giao diện 4K/DPI, STT+OCR trên Colab GPU thật, chất lượng bản dịch/giọng thật và CapCut mở đúng project. Những mục này không được đánh PASS trong báo cáo hiện tại.

Lưu ý gate: các RC còn mở là điều kiện **nghiệm thu thủ công** trong checklist; script prebuild hiện chỉ chạy những test/validator đã có, chưa tự đọc checkbox RC để khóa package. Cần chuyển các ca tái hiện thành test thực thi trước khi tin vào gate tự động. Lần này không sửa script hay vô hiệu hóa test.

Lệnh chính để lặp lại: chạy `LAStudioUnitTests.exe` với `LASTUDIO_TEST_SUITE=TestDubbingProject` (Qt bin/plugins và offscreen như CTest), `ctest --test-dir out/build/windows-msvc-release --output-on-failure -j 4 --timeout 150`; các tên ca mục tiêu nằm trong log A–F. Artifact của đợt audit nằm tại `out/audit-2026-09-05-dubbing/`. Các log audit không chứa dữ liệu project thật của user.

---

## 2026-09-05 — 0.0.9.1 audit remediation and portable package

### Implemented

- Runtime Host no longer relies on a single `readyRead` edge for its first
  named-pipe frame. The server consumes already-buffered input and the client
  retries the idempotent Hello frame within a five-second handshake deadline;
  inference keeps its separate 60-second inactivity deadline.
- The Dubbing mixer/workflow regressions cover required cue coverage,
  source-duration preservation, common TTS sample rates and correct
  sidechain branching. Project paths, workflow output persistence and smoke
  data isolation remain in the release path.
- The release script rejects a requested version that differs from
  `CMakeLists.txt` and will stage `release-source-manifest.json` with the base
  revision, dirty-source hashes and executable hash. This makes a dirty local
  candidate traceable without putting source contents or credentials into the
  package.

### Fresh evidence

- Runtime Host stress: **100 consecutive restart/auth handshakes PASS**.
- Full MSVC CTest: **41/41 PASS**.
- QML lint and `git diff --check`: **PASS**.
- `graphify update .`: **PASS** (with Graphify's existing parser warnings for
  some generated/unsupported source formats).
- `out/prebuild-gate/latest.json`: **10/10 PASS**.
- `scripts/package.ps1` staged the internal portable candidate at
  `out/LA-Studio-0.0.9.1/`; its packaged QML smoke recorded **19** interaction
  events and the release source manifest records the staged executable hash.

### Honest boundary

The automated result proves the local/controller/contract routes. A fresh
credentialed Colab GPU run, a real CapCut open/import and a prolonged
interactive playback/ROI-drag run remain operator acceptance tasks and are
not represented as automatic PASS results.

## 2026-09-03 — canonical guide, project isolation and release gate

### Fixed

- Rewrote the only guide,
  `docs/AI_AGENT_TRANSCRIPT_RECONCILIATION_GUIDE.md`, as a direct task
  contract for an external AI IDE: use the exact app-supplied STT/OCR and
  translation paths, reconcile both scripts when present, then translate.
- The guide now gives OCR priority, STT reference/fallback behavior, immutable
  cue/timeline rules, Chinese → Vietnamese translation, speaker hierarchy,
  Pinyin-to-Hán-Việt names (`Wang` → `Vương`), meaningful cue density and
  final timestamp/cue validation. It contains no architecture, auth/cache,
  repository URL, fixed basename or disk-search requirement.
- Removed the obsolete second guide `docs/AI_AGENT_TRANSLATION_PROMPT.md` and
  updated the C++ contract test/checklist so the old translation-only contract
  cannot silently return.
- Made automatic project creation collision-safe: a repeated custom name now
  gets `__2`, `__3`, ... instead of overwriting the earlier project.
- Corrected the MSVC build-cache detector. It now checks actual `CMAKE_AR` and
  `CMAKE_RANLIB` values instead of treating the bundled CMake executable path
  containing `mingw` as a stale MinGW toolchain.
- Kept the already implemented workflow safeguards: independent STT/OCR,
  local upload without Colab, optional task skip, artifact persistence,
  thumbnail/player/ROI safety, FLAC/WAV compatibility, non-blocking
  separation, and cross-task regression requirements.

### Short chat prompt

Use this prompt after LA Studio has supplied the exact paths:

> Đọc `C:\Users\Nguyen Trong Khoi\Downloads\TTS\LA-Studio\docs\AI_AGENT_TRANSCRIPT_RECONCILIATION_GUIDE.md` và làm ngay theo file. Chỉ dùng đúng các đường dẫn STT/OCR/input/output do LA Studio cung cấp; nếu có cả STT và OCR thì khớp thành bản chuẩn rồi dịch Trung → Việt, giữ nguyên timeline.

### Verification

- Full MSVC CTest: **41/41 PASS** after source, test and guide changes.
- The clean incremental rebuild compiled both `LA-Studio` and
  `LAStudioUnitTests` successfully; the previous direct-shell failure was only
  the expected missing MSVC environment, and the project test script initializes
  that environment correctly.
- QML lint, prebuild gate and Graphify refresh are complete. Portable
  packaging and Git push are performed only after this report is updated with
  their actual evidence. Live Colab and long-running GUI acceptance cannot be
  claimed from the static/unit-test gate alone.

## 2026-08-29 — previous latest outcome

## 2026-08-29 — Subtitle OCR Colab Pillow compatibility

- Đã sửa lỗi notebook OCR dừng trước khi mở worker vì probe kiểm tra cứng
  `Pillow==12.0.0` nhưng môi trường Colab đang có `12.3.0`. Dependency nay
  dùng `Pillow>=12.0.0,<13.0.0`; probe vẫn kiểm tra API `ImageText`/`_Ink`,
  package isolation và một lần inference CUDA thật.
- `No ccache found` được phân loại đúng là warning không gây dừng worker.
  Revision handshake giữa desktop và notebook đã đổi đồng bộ sang
  `subtitle-ocr-2026-08-23.18`; Unified notebook cũng đã được regenerate.
- Đã xác minh generated notebooks **32/32**, Unified notebook contract **PASS**
  và focused Dubbing tests cho artifact/upload, STT/OCR độc lập đều **PASS**.
  Colab notebook đang mở từ trước phải đóng/mở lại bản mới trên runtime mới.

## 2026-08-29 — cross-notebook audit for application-repository dependencies

- The same failure mode was checked across the complete notebook tree, not only
  the Spleeter notebook. The audit found two application-owned runtime fetches:
  the Unified Dubbing notebook fetched the LA Studio worker/coordinator bundle,
  and the legacy `LA_STUDIO_VOICE_CLONE_GPU.ipynb` fetched the old
  `kova-voice-studio` application repository.
- Unified Dubbing is now self-contained: the generated notebook embeds **34
  exact local files** (32 exact-model notebooks plus the coordinator and the
  Spleeter worker), verifies normalized SHA-256 values, materializes them under
  `/content/la-studio-unified-source`, and starts the coordinator from that
  local source tree. It no longer clones or downloads LA Studio application
  code from GitHub at runtime.
- The legacy Voice Clone notebook is now a compatibility alias of the exact
  OmniVoice notebook. It contains no application-repository URL, clone step,
  `REPO_URL`, or `REPO_REF`. This prevents an old notebook entry from
  reintroducing the same failure.
- All other direct exact-model notebooks were checked. Their remaining
  `git clone`/download statements point to official upstream model/runtime
  repositories and are intentionally retained because those repositories are
  model dependencies, not LA Studio application-worker dependencies.
- Added repository-wide dependency scanning and regression gates:
  `test_notebook_repository_dependencies.py`,
  `test_unified_dubbing_bundle.py`,
  `verify_legacy_voice_clone_compat_notebook.py`, and the strengthened
  `verify_unified_dubbing_colab_notebook.py`. They run in hooks, CI, Windows
  release, and the pre-build gate.

Verification for this cross-notebook fix: repository dependency scan **1/1
PASS**, Unified bundle tests **5/5 PASS**, legacy alias verification **PASS**,
Unified notebook contract **PASS**, generated exact-model notebooks **32/32
PASS**, exact bindings **31/31 PASS**, remote feature surface **8/8 PASS**,
QML lint **PASS**, CTest **41/41 PASS**, and the full pre-build release gate
**PASS**. No EXE was rebuilt in this source-only notebook correction.

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
- Project mới và project migrate thiếu giá trị dùng mặc định `zh → vi`; file
  `AI_AGENT_TRANSCRIPT_RECONCILIATION_GUIDE.md` là hợp đồng IDE-agnostic duy
  nhất cho agent xử lý script/fusion/translation.

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
  notebook: `subtitle-ocr-2026-08-23.18`.
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

## Update — 2026-08-29: STT/OCR local upload independence recheck

### Issue corrected

The previous report focused on the Subtitle OCR notebook failure and did not
record the separate desktop regression: the STT/OCR Upload actions could be
disabled by the aggregate Dubbing `processing` flag before any Colab connection
existed. In another path, the UI policy allowed the sibling upload while the
other independent transcript worker was running, but the backend import gate
still rejected it.

### Code correction

- Added `DubbingController::canImportWorkflowArtifactNow()` as the single
  route-scoped policy for manual local artifact handoff. STT and OCR upload do
  not require a model, worker URL, bearer token, or Colab session.
- Updated the production STT/OCR cards, task settings panel, and artifact
  upload panel to use that policy instead of the aggregate busy gate.
- Normalized `ocr` and `subtitle-ocr` artifact aliases and added a defensive
  direct contract lookup/refresh so the dialog cannot show only a summary while
  hiding its `FileDialog`.
- Kept cancellation scoped: accepting a matching active worker output may
  cancel that worker; uploading the independent sibling output never cancels
  the worker still running.

### Verification

- Real local import regression with no Colab configuration: STT accepts
  `transcript.srt`; OCR accepts `ocr.srt`.
- Downstream handoff is now verified rather than inferred: each imported file
  becomes canonical `m_project.segments`, is recorded in the route-specific
  `sttSegments`/`ocrSegments` provenance list, produces a cached artifact path
  and `stepOutput`, and changes the Translate node to `ready`. A combined STT +
  OCR import is reconciled locally before Translate, and the canonical cue data
  remains available after `saveProject()` followed by `openProject()`.
- Focused `TestDubbingProject`: **5/5 passed**.
- Full CTest: **41/41 passed**; QML lint gate: **PASS**.
- Product EXE was intentionally **not** built in this source-fix turn, so no
  new live GUI screenshot or packaged-binary claim is made here.

The required follow-up is to run the visual upload-picker smoke after the next
production build: open both STT and OCR Upload before Colab setup, verify the
native file picker shows the declared subtitle formats, import one file per
route, and repeat while the sibling transcript worker is active.

## Update — 2026-09-05: Reliability remediation, batch G6

### Completed in this batch

- The mixer now treats dubbed speech, separated original vocals, and BGM as
  three independent buses. `Original audio` controls only the vocal stem;
  `Background` controls only BGM; dubbed speech has its own gain. This prevents
  a request such as `0% original / 100% dubbed` from accidentally muting BGM.
- Preview output is now project-scoped beneath the project's
  `.workflow-artifacts` directory. Two projects in the same directory cannot
  overwrite one another's `preview.wav`. A legacy sibling preview is read only
  when exactly one project file exists in that directory.
- The regression suite caught a duplicate local declaration in the new mixer
  before a package was built; it was corrected immediately.

### Verification evidence

- Full release CTest after the correction: **41/41 passed** in **89.67 s**.
- The passing suite includes QML route smoke and dedicated checks for:
  changed translated text invalidating old cue audio, skipped cues being absent
  from audio mix, independent original/dubbed/background levels, and scoped
  default preview output.

### Next remediation item

The next batch will replace the single massive FFmpeg cue graph with bounded
timeline batches. This specifically targets command-line/filter-graph growth
for projects with many short dialogue cues, which can otherwise cause long
mix startup times or a process-launch failure on Windows.

## Update — 2026-09-05: Reliability remediation, batch G7

### Root cause reproduced and fixed

The new regression intentionally built a 140-cue timeline using a long project
artifact path. Before the fix, the mixer failed at process launch with:
`Process failed to start: The filename or extension is too long.` The old
implementation put every `-i` argument and every cue filter into one FFmpeg
invocation.

The mixer now renders at most 24 cue inputs per pass and reduces the resulting
full-timeline WAV buses in bounded groups. It preserves cancellation checks,
the selected dubbed level, final duration, and the independent BGM/original
vocal mix phase.

### Verification evidence

- The 140-cue/long-path regression now passes end-to-end.
- Full release CTest after the change: **41/41 passed** in **89.13 s**.
- QML route smoke remained part of the green suite.

## Update — 2026-09-05: Reliability remediation, batch G8

### Root cause reproduced and fixed

An explicit full-program TTS/Align upload could lose to an older `clipPath`
that was still present on a segment. The mixer selected the cache merely
because it existed, producing stale dialogue despite the user's manual
replacement.

`DubbingJobRunner::renderPreview()` now gives every readable explicit dubbed
voice bed precedence over cached per-cue clips. The uploaded bed is rendered as
one full-program timeline clip; later source/translation/timing edits still
clear it through the existing synthesis-output invalidation path.

### Verification evidence

- New regression creates a stale `0.75` cue and a manual `0.25` upload, then
  verifies that the output is `0.25` rather than the cache.
- Full release CTest: **41/41 passed** in **89.08 s**.

## Update — 2026-09-05: Reliability remediation, batch G9

### Root cause reproduced and corrected

Two older Direct Colab worker lifecycles still embedded `QThread` objects in
their job controllers.  Destruction queued cancellation, blocked the GUI
thread for up to five seconds, and then called `QThread::terminate()`.  That
could make a close operation appear frozen and could interrupt Qt/network
state at an unsafe point.

`DubbingTranscriptionJob` (alignment) and `DubbingJobRunner` (separation) now
use the same bounded cooperative shutdown policy as the newer remote jobs:
request cancellation/interruption, allow a finite grace period, then detach
only a non-cooperative worker from controller lifetime.  No controller state
is accessed after detachment and Qt deletes the worker/thread after it
unwinds.  The application never force-terminates those worker threads.

### Verification evidence

- The new regression starts a deliberately slow worker and proves that the
  bounded shutdown path returns promptly rather than waiting for it to finish;
  it also locks the two affected controllers to the bounded policy.
- RED state before the implementation: `TestDubbingProject` failed precisely
  because both controllers still used `terminate()` and had not adopted the
  bounded helper.
- GREEN state after the implementation: release build plus full CTest:
  **41/41 passed** in **90.88 s**, including QML route smoke.

### Remaining acceptance boundary

This protects desktop shutdown from a non-cooperative local thread in the
test harness.  A real Direct Colab interruption still needs a live acceptance
run to verify that the remote service honours cancellation and releases its
network operation quickly; it is no longer allowed to freeze the desktop
while it does so.

## Update — 2026-09-05: Reliability remediation, batch G10

### STT/OCR overlap evidence added

The former RC-04 evidence was only a source-string check. A new behavioral
regression now runs the real controller with a verified loopback Direct Colab
STT worker held deliberately in `running` state and a real Subtitle OCR
controller. It proves both user orders:

- OCR starts, then `Run STT` starts while OCR is active.
- STT starts, then `Run OCR` starts while STT is active.

For each order OCR may finish before STT, but cannot cancel it or overwrite
its durable source list. Releasing STT leaves both `ocrSegments` and
`sttSegments` in the saved project; canonical visible segments end with the
STT result as expected by the configured source policy.

### Verification evidence

- Focused behavioral test
  `independentSttAndOcrStartTogetherAndPersistBothSources`: **3 passed, 0
  failed** in 4.57 s (including QtTest init/cleanup).
- Regression full CTest after the initial fixture correction: **41/41
  passed** in 93.79 s. The fixture correction was test-only: it now returns
  ffprobe's text stream token for artifact validation and JSON metadata for
  OCR, exactly matching the two production invocations.

## Update — 2026-09-05: Reliability remediation, batch G11

### STT/OCR route isolation and stale-import fence

The remaining RC-04 controller gaps are now covered by behavioral regressions:

- `manualOcrArtifactUploadKeepsRunningSttAndPersistsBothSources` uploads a
  local OCR artifact while STT is deliberately held running. The upload is
  accepted without a Colab dependency and does not stop STT.
- `manualSttArtifactUploadCancelsOnlySttAndKeepsRunningOcr` runs both routes,
  accepts a local STT artifact, confirms that only STT is cancelled, and then
  confirms that OCR completes and both durable source lists remain intact.
- `backgroundArtifactImportCannotCommitAfterProjectChanges` delays artifact
  validation, opens another project, and verifies the stale result is rejected
  before it can mutate the new project.

### Verification evidence

- Focused G11 controller run: **4 passed, 0 failed** in **1.096 s** for the
  OCR-upload/project-switch cases, then **3 passed, 0 failed** in **3.005 s**
  for the STT-upload/active-OCR cancellation case.
- The STT cancellation log is expected and route-scoped. The assertion proves
  `subtitleOcrProcessing()` remains true after the STT artifact is accepted.

### Remaining acceptance boundary

RC-04 is closed at the desktop controller regression level. It remains open
for release acceptance until the same two worker routes are observed against
real Direct Colab endpoints, because a loopback worker cannot prove remote GPU
service availability, tunnel lifetime, or remote cancellation latency.

## Update — 2026-09-05: Reliability remediation, batch G12

### Project/run-scoped AI transcript handoff

The handoff API no longer reuses stage-global `transcript.srt`/`translated.srt`
paths. Every request receives a unique project-owned snapshot directory, so a
second IDE request cannot overwrite its predecessor and projects with the same
visible name remain isolated by their actual project directories. The returned
prompt is intentionally concise and contains only the five exact paths needed
for merge and translation.

### Verification evidence

- RED: `transcriptAiHandoffUsesUniqueProjectScopedSnapshots` failed because
  the old API did not return a handoff directory, prompt or reserved outputs.
- GREEN: controller regression **3 passed, 0 failed** in **0.160 s**; it checks
  two runs in one project plus a same-name project in another directory.
- GREEN: workspace guide/UI contract **3 passed, 0 failed** in **0.003 s**;
  Qt recompiles the changed `DubbingReviewPanel.qml` AOT cache successfully.

### Remaining acceptance boundary

The controller and QML contract are covered. A packaged desktop acceptance run
must still click Prepare, Copy prompt and Open folder and then use a real IDE
on a sample before release sign-off; neither a static QML assertion nor a unit
test can attest to the external IDE's content quality.

## Update — 2026-09-05: Reliability remediation, batch G13

### Fresh regression gate after G11/G12

- QML lint gate: **PASS** for the changed Data & Handoff panel.
- Full Windows release CTest: **41/41 PASS**. This includes the production
  offscreen `QmlRouteSmoke`, which launches the compiled desktop executable;
  it exited normally after the route interaction trace.
- The runner's streamed output ended while `TestDubbingProject` was still
  executing, so process state and CTest's final log were checked explicitly:
  no `ctest` or `LAStudioUnitTests` process remained, `LastTest.log` ended at
  test **41/41**, and no failed-test entry was recorded. This avoids treating
  a truncated tool stream as a false green result.

### Classified non-blocking environment warning

`QFontDatabase` reports no deployed Qt `lib/fonts` folder in the offscreen
test runtime. The application and all QML routes loaded, the test passed, and
the warning remains a packaging/font-disposition item rather than a QML or
workflow failure. It must still be reviewed when a public package is made.

## Update — 2026-09-05: Reliability remediation, batch G14

### Prepared artifact transaction closes the cancel-then-fail window

The asynchronous artifact worker now returns parsed/mapped subtitle data as
part of its staging result. The controller rechecks the current project and
route, then commits that prepared data. It no longer asks
`importSubtitlesInternal()` to parse or map the file after cancelling its
matching STT/OCR producer. Translation artifacts use the same prepared rows
and reject a transcript that changed while validation was running.

This preserves the existing safety boundaries: invalid media/subtitles still
fail in the worker before any worker cancellation, only the matching STT or
OCR route may be cancelled, and a stale project/path result is rejected.

### Verification evidence

- `manualUntimedTranscriptIsPreparedBeforeMatchingWorkerCancellation`:
  **3 passed, 0 failed** in **0.571 s**. It holds a real controller STT route,
  uploads one untimed TXT line, injects a segment edit synchronously when the
  cancellation state change fires, and verifies the already prepared manual
  transcript commits successfully rather than cancelling STT then failing to
  remap the file.
- Existing route/isolation cases after the refactor: **6 passed, 0 failed** in
  **4.138 s** for OCR upload/STT upload/project-change/handoff; malformed
  media/subtitle, async media staging, translation invalidation and no-Colab
  contract cases: **7 passed, 0 failed** in **1.112 s**.

### Remaining acceptance boundary

This closes RC-03 at the controller transaction boundary. A release still
needs the checklist's interactive file-picker and real Direct Colab acceptance
on every workflow artifact type; the test worker cannot prove a remote tunnel
or user storage behaves identically.

## Update — 2026-09-05: Reliability remediation, batch G15

### Mix, project-output and shutdown regression rerun

The focused execution batch confirms the safeguards already added in G5–G9
remain intact after the artifact-transaction refactor:

- An explicit manual voice bed still takes precedence over an old cue clip.
- Default preview paths remain isolated for two project files in one folder.
- A deliberately skipped cue is neither required for readiness nor rendered.
- The bounded mixer completes a 140-cue timeline under a long Windows path.
- Background music, original vocals and dubbed speech retain independent gain
  controls; bounded shutdown returns without waiting for a stalled worker.

### Verification evidence

- Focused executable regression: **9 passed, 0 failed, 0 skipped** in
  **2.822 s** (`audioMixRunsAsynchronously`, preview scoping, skip policy,
  large timeline, three-bus gain and bounded shutdown).

### Remaining acceptance boundary

The focused fixture proves the controller/mixer contracts, not a production
duration/load budget. The release checklist still requires 200/500/1000-cue,
large-file and live desktop/remote-worker measurements before public sign-off.

## Update — 2026-09-05: Reliability remediation, batch G16

### Regression contract now protects the safe artifact transaction

The full gate exposed one stale source-contract assertion in
`transcribeUiSeparatesSttAndOcrCards`: it still required the former
`importSubtitlesInternal()` call after matching-worker cancellation. That
implementation was deliberately removed in G14 because it could parse/map a
subtitle a second time after cancelling a live STT/OCR job.

The contract now requires the prepared subtitle payload and the explicit
pre-cancellation parsing boundary, and rejects the old post-cancellation
reparse call. This strengthens the regression guard rather than relaxing it.

### Verification evidence

- `TestDubbingProject::transcribeUiSeparatesSttAndOcrCards` and
  `manualUntimedTranscriptIsPreparedBeforeMatchingWorkerCancellation`:
  **4 passed, 0 failed, 0 skipped** in **0.596 s**.
- The second case performs a real cancellation callback and a synchronous
  segment edit while importing untimed TXT; the prepared artifact still
  commits correctly. The next full 41-test gate is pending after this
  checkpoint.

## Update — 2026-09-05: Reliability remediation, batch G17

### Async upload and long-timeline guards exercised at meaningful bounds

The manual artifact path continues validation/copy in its staging worker. Its
regression now substitutes an `ffprobe` that waits one second, then requires
the `Q_INVOKABLE` enqueue operation to return within 150 ms and eventually
commit both stems. A synchronous probe/copy regression therefore fails before
it can cause a visible UI freeze.

The audio-mix regression was raised from 140 to **1,000** cue clips under a
long Windows path. The mixer renders at most 24 inputs per FFmpeg process and
reduces the batches, so neither the input list nor filter graph grows with the
whole timeline in one `CreateProcess` call.

### Verification evidence

- `manualArtifactUploadStagesValidMediaWithoutBlockingProjectCommit` and
  `audioMixBatchesLargeCueTimelines`: **4 passed, 0 failed, 0 skipped** in
  **3.642 s** (including QtTest setup/cleanup).
- The first test uses a real valid WAV pair and a deliberate one-second probe
  delay; the second performs the actual 1,000-cue FFmpeg batching/reduction
  and verifies the rendered sample count.
- G15's bounded-shutdown test remains the ownership/cancel guard for remote
  workers. Together these are automated safeguards for RC-08/09/11; the next
  full suite remains pending after this checkpoint.

### Remaining acceptance boundary

The suite cannot characterize a user's 1 GB copy over a slow disk, sustained
memory under real 200/500/1,000-cue voiced media, or a live tunnel that never
responds. Those are release acceptance measurements, not reasons to weaken
the automated guard or to report them as passed here.

## Update — 2026-09-05: Reliability remediation, batch G18

### Final automated regression gate after the remediation batches

- QML lint: **PASS**.
- Windows release CTest: **41/41 PASS**, **0 failed**, total **99.27 s**.
  This includes `TestDubbingProject` with the 1,000-cue batch case and the
  production offscreen `QmlRouteSmoke` launch/interaction trace.
- The test process finished normally; the final CTest log ends at
  `QmlRouteSmoke` with `Test Passed`, and no `ctest` or
  `LAStudioUnitTests` process remained afterward.
- `git diff --check` found no whitespace error. Line-ending notices shown by
  Git are repository conversion notices, not a diff-check failure.

### What this closes—and what it does not claim

The implemented controller, mixer, worker-lifetime, handoff, and QML
regression contracts now pass together. This is not a claim that live Direct
Colab, a real multi-gigabyte user disk transfer, real media quality, 4K/DPI
interaction, or CapCut handoff has been exercised in this run. Those remain
explicit desktop/live acceptance items in the checklist and must be completed
before a public release is called fully signed off.
