# LA Studio — Independent Full Technical Audit

**Ngày kiểm tra:** 2026-09-04
**Phạm vi:** source hiện tại trong working tree, ưu tiên toàn bộ Dubbing workflow; UI/QML, UX, workflow state, audio pipeline, threading, persistence/SQLite, Colab/notebook, export và release gate.
**Commit nền:** `600b27d14b0a82517d01bbad0e519a0a4a8aa773` (`main`, trùng `origin/main`).
**Nguyên tắc:** chỉ kết luận từ code/test hiện tại. Tài liệu lịch sử không được dùng làm bằng chứng rằng tính năng đang hoạt động.

## 1. Kết luận điều hành

**LA Studio hiện chưa đạt trạng thái có thể xác nhận “workflow Dubbing chạy ổn hoàn toàn”.** Không phát hiện lỗi P0 chắc chắn gây mất dữ liệu ngay lập tức, nhưng có **9 lỗi P1** và **16 lỗi P2** cần xử lý. Các điểm nghiêm trọng nhất là:

1. Runtime Host có race/flaky startup, đã tái hiện bằng test lặp: có lần treo đủ 60 giây.
2. Mixer resample clip sai khi TTS không xuất đúng 48 kHz; trường hợp 16/24/44,1 kHz có thể tạo âm bị rỗng, méo hoặc sai mức.
3. Workflow coi “có ít nhất một clip TTS” là đủ để mix/export; câu chưa có clip bị bỏ qua thành im lặng.
4. Final mix lấy độ dài từ clip thoại cuối cùng, có thể cắt mất nhạc nền/phần cuối video.
5. Nhiều đường xử lý/copy audio-video lớn vẫn chạy hoặc chuẩn bị dữ liệu trên GUI thread; cộng với pipeline giải mã toàn bộ file vào RAM, đây là nguy cơ thực tế gây đơ/đóng app.
6. Export/CapCut có thể sao chép FLAC/MP3 nguyên byte nhưng đặt tên đích `.wav`.
7. Bản EXE đóng gói dùng khoảng **22,9% của một logical core khi idle**; source có sáu animation vô hạn luôn chạy ở nút Community.
8. Lần đầu materialize trang Dubbing làm private memory tăng khoảng **131,8 MB** và Loader giữ trang này sống đến hết phiên.
9. QML smoke đã ghi nhiều project test vào lịch sử thật; project gate hiển thị 26 mục `qml-route-smoke`, gây nhiễu dữ liệu/UX người dùng.
10. Working tree có 111 thay đổi chưa được đóng gói thành commit sạch, nên bản build hiện tại chưa tái lập/rollback chính xác được.

### Điểm readiness hiện tại

| Hạng mục | Điểm | Trạng thái |
|---|---:|---|
| Build và regression gate | 6.0/10 | 40/41 CTest qua; gate thất bại vì Runtime Host flaky |
| UI/QML contract | 6.0/10 | Lint/route smoke qua; task rail bị khuất ở cửa sổ nhỏ, còn lỗi seek OCR, chưa nghiệm thu playback |
| UX và điều hướng workflow | 4.5/10 | Live shell không treo nhưng idle CPU cao, Dubbing tăng/giữ RAM, history test lẫn dữ liệu thật |
| Backend/threading | 5.0/10 | Có worker/QProcess; vẫn còn join vô hạn và công việc nặng trên GUI path |
| Audio/DSP/mixing | 4.0/10 | Filter/runtime có; resampling, duration và memory model còn lỗi P1 |
| Persistence/database | 6.5/10 | Project save nguyên tử; chưa auto-reopen, path tuyệt đối, SQLite write không giao dịch |
| Colab/notebook integration | 7.0/10 | Contract tĩnh qua đầy đủ; chưa nghiệm thu worker/token live trong lần audit này |
| Security/log redaction | 8.0/10 | Token giữ trong session; logger có che token/Bearer/home/text nhạy cảm |
| Release reproducibility | 3.0/10 | Working tree rất bẩn; chưa có source snapshot sạch tương ứng binary |

**Tổng hợp readiness: 5.4/10.** Đây là điểm sẵn sàng phát hành, không phải điểm thẩm mỹ hay số lượng tính năng.

## 2. Bằng chứng kiểm tra

### 2.1 Kết quả đã chạy

| Kiểm tra | Kết quả | Bằng chứng |
|---|---|---|
| Pre-delivery gate sạch từ script dự án | **FAIL** | `out/prebuild-gate/latest.json`, 163,668 ms; dừng tại C++/CTest |
| CTest | **40/41 PASS** | `TestRuntimeHostProtocol` thất bại tại lần `client.start()` thứ hai |
| Runtime Host stress | **FAIL, flaky đã tái hiện** | Lặp `until-fail:20`: pass 0.23 s, pass 0.20 s, rồi fail 60.23 s |
| QML lint | **PASS** | `scripts/lint_qml.ps1` |
| QML route smoke | **PASS** | 19 thao tác trong `out/build/windows-msvc-release/dubbing-qml-interaction-trace.json` |
| Exact Dubbing bindings | **31/31 PASS** | Contract C++/QML hiện tại |
| Generated notebook contracts | **32/32 PASS** | Notebook manifest/hash/binding checks |
| Notebook repository dependencies | **PASS** | 1/1 |
| Worker tests | **PASS** | 5/5 |
| Worker pins | **PASS** | 2/2 |
| Unified Dubbing bundle | **PASS** | 3/3 |
| Legacy voice-clone compatibility notebook | **PASS** | Generator/verifier hiện tại |
| Remote capability surface | **PASS** | 8 task và 9 capability path được khai báo |
| Bundled FFmpeg filters | **PASS** | Có `asplit`, `loudnorm`, `sidechaincompress` |
| Git whitespace | **PASS** | Gate không phát hiện whitespace error |

### 2.2 Phần chưa được phép gọi là đã nghiệm thu

- Không chạy worker Colab/GPU thật và token/tunnel thật trong lần audit độc lập này.
- Đã mở và đo bản đóng gói `out/LA-Studio-0.0.9.0/LA-Studio-0.0.9.0.exe`, nhưng không chạy worker/model thật.
- Không hoàn tất nghiệm thu video playback liên tục vài phút, kéo OCR ROI bằng chuột thật, nghe audio đầu ra hoặc mở draft thật trong CapCut. Fixture video 90 giây đã được tạo, nhưng vòng chọn file tự động chưa hoàn tất an toàn nên không được tính là playback PASS.
- Chưa đo thao tác Cancel/đóng app trong lúc worker thật đang chạy; số 803 ms chỉ là đóng app khi idle ở Dubbing.
- QML route smoke hiện chỉ xác nhận route/preflight và một số control tồn tại/nhận click; không thay thế E2E Windows UI Automation.

### 2.3 Kiểm tra UX/performance trực tiếp trên EXE đóng gói

Artifacts nằm tại `out/ux-audit-20260904/`; các file CSV là số đo tiến trình. Ảnh hợp lệ được dùng trong kết luận bắt đầu từ `01-startup-printwindow.png` và các ảnh `02`–`15` nêu bên dưới.

| Hành trình/điểm đo | Kết quả trực tiếp | Đánh giá UX |
|---|---|---|
| Cold start đến khi có window | 3.318 ms; Responding sau 3.395 ms; 258,0 MB working set, 238,8 MB private | **Amber:** dùng được nhưng chưa nhanh; cần budget và startup trace |
| Sau khi mở Model | 332,7 MB working set; 279,9 MB private | **Amber:** tăng đáng kể dù chưa chạy model |
| Vào project gate Dubbing | 343,3 MB working set; 289,8 MB private | **Amber:** gate xuất hiện ổn nhưng lịch sử bị nhiễu project smoke |
| Idle 21,03 giây ở Home | 4,812 CPU-second = 22,9% một logical core; 0 mẫu Not Responding; private 293,7→294,7 MB | **Red:** app rảnh vẫn tốn CPU/pin và có thể làm máy nóng |
| Chuyển 15 route bằng UI Automation | invoke 1,3–154,4 ms; 0 mẫu Not Responding trong cửa sổ quan sát | **Green/Amber:** event loop còn đáp ứng; đây chưa chứng minh nội dung từng trang tải xong |
| Mở blank Dubbing project | khoảng 2.137 ms; working set 332,9→438,5 MB; private 317,0→448,8 MB | **Red:** tăng 105,6 MB working set/131,8 MB private trước workload thật |
| 1280×720 | Nội dung Home cuộn được; Dubbing task rail chỉ thấy rõ đến bước 5, bước 6 bị cắt, 7–8 nằm ngoài viewport; nút Import phía dưới cần cuộn | **Amber:** không mất hẳn control nhưng khả năng khám phá kém |
| 1600×900 | Task rail vẫn cắt một phần bước 7 và giấu bước 8 do cụm action cố định | **Amber:** lỗi responsive không chỉ xảy ra ở 720p |
| Panel phải tại Import/empty state | Giữ đúng panel phải theo thiết kế, nhưng chiếm gần nửa màn hình và hầu như trống | **Amber:** nên co theo ngữ cảnh, không xóa panel |
| Đóng app khi idle | thoát sạch trong 803 ms | **Green có điều kiện:** chưa đại diện cho shutdown khi worker/process đang chạy |

Các số đo không cho thấy Windows chuyển app sang `Not Responding` trong các hành trình shell/route. Tuy nhiên, chúng **không bác bỏ** nguy cơ treo khi chạy media/model vì các đường full decode/copy và wait vô hạn đã được xác nhận tĩnh ở F-06, F-09 và F-17.

## 3. Danh sách lỗi đã xác nhận

Quy ước:

- **P1:** có thể gây treo, output sai, mất tính toàn vẹn workflow hoặc bản phát hành không đáng tin.
- **P2:** lỗi chức năng/UX/persistence quan trọng nhưng có đường tránh hoặc chưa gây hỏng output ngay mọi trường hợp.

### F-01 — P1 — Runtime Host startup/protocol bị race và treo 60 giây

**Bằng chứng**

- Test thất bại tại `tests/core/test_RuntimeHostProtocol.cpp:122`, lần `client.start()` thứ hai.
- Startup/process/pipe path: `src/runtimehost/service/RuntimeHostClient.cpp:31-88`.
- Retry kết nối socket chỉ thực hiện ở `RuntimeHostClient.cpp:245-260`.
- Đọc phản hồi dùng inactivity timeout dài ở `RuntimeHostClient.cpp:299-365`.
- Test lặp tái hiện: hai lần đầu pass dưới 0,25 giây, lần ba treo 60,23 giây rồi fail.

**Tác động**

- Local model/runtime có thể không khởi động dù cùng binary vừa chạy thành công.
- UI có thể đứng ở trạng thái chờ hoặc báo worker không phản hồi; release gate không ổn định.

**Cần sửa**

- Log rõ từng phase: spawn, PID, socket name, auth handshake, request ID, child exit code và stderr cuối.
- Tách startup deadline ngắn khỏi inference inactivity timeout.
- Khi handshake thất bại, hủy socket/process cũ và tạo client sạch trước retry.
- Thêm stress test tối thiểu 50–100 cold start liên tiếp và test restart sau stop/crash.

### F-02 — P1 — Bản build không tái lập được từ Git

**Bằng chứng**

- Working tree hiện có **94 file modified, 1 file deleted, 16 untracked** (111 entry).
- `HEAD` bằng `origin/main`, nghĩa là phần lớn source/QML/notebook/test/script của bản đang kiểm tra chưa nằm trong commit hiện tại.

**Tác động**

- Không thể khẳng định một EXE cụ thể tương ứng chính xác với source nào.
- Khó rollback, bisect và tái tạo lỗi; CI/GitHub không kiểm tra phần code thực tế đang được build tại máy.

**Cần sửa**

- Chia thay đổi thành commit có chủ đề, giữ build manifest gồm commit + dirty hash.
- Chỉ phát hành từ source snapshot sạch hoặc archive có checksum.

### F-03 — P1 — Sao chép audio sang tên `.wav` mà không transcode

**Bằng chứng**

- Package helper dùng raw copy nhưng tên đích cố định: `src/controllers/dubbing/parts/DubbingController_Artifacts.cpp:23-26`.
- Manual artifact nhận `.wav`, `.mp3`, `.m4a`, `.flac`, ... tại `DubbingController_Artifacts.cpp:160-189`.
- CapCut exporter đặt đích `.wav` tại `src/dubbing/exporters/CapCutDraftExporter.cpp:536-571` trong khi helper `QFile::copy` nằm ở `CapCutDraftExporter.cpp:47-64`.

**Tác động**

- FLAC/MP3 có thể được giữ nguyên nội dung nhưng mang đuôi `.wav`; player/NLE đọc theo extension có thể từ chối hoặc hiểu sai.

**Cần sửa**

- Hoặc giữ đúng suffix nguồn và metadata, hoặc transcode rõ ràng sang PCM WAV.
- Sau staging phải `ffprobe` codec/container, không chỉ kiểm tra file tồn tại.
- Thêm test upload FLAC/MP3 → package/export/CapCut.

### F-04 — P1 — Một clip TTS cũng làm workflow được coi là đủ

**Bằng chứng**

- `hasClips` dùng phép OR tại `src/controllers/dubbing/parts/DubbingController_Workflow.cpp:17-25`.
- Synthesize/fit/mix/export dựa trên `hasClips` tại `DubbingController_Workflow.cpp:150-179`.
- Runner cũng chỉ tìm “có một clip” tại `src/controllers/dubbing/DubbingJobRunner.cpp:878-886`.
- Mixer bỏ qua segment thiếu file bằng `continue` tại `src/dubbing/audio/AudioTimelineMixer.cpp:106-108` và `125-127`.

**Tác động**

- Dự án có thể mix/export thành công dù nhiều câu thoại biến thành im lặng.

**Cần sửa**

- Tính coverage trên mọi cue không bị đánh dấu skip: `generated/required`.
- Chặn mix/export nếu coverage < 100%, hoặc bắt user xác nhận danh sách cue cố ý bỏ qua.
- Hiển thị rõ `x/y clips ready` và test thiếu clip đầu/giữa/cuối.

### F-05 — P1 — Final mix bị cắt theo câu thoại cuối cùng

**Bằng chứng**

- `outputSamples` chỉ lấy max từ `endMs` của clip thoại hợp lệ tại `AudioTimelineMixer.cpp:100-112`.
- Background chỉ được trộn trong `mix.size()` tại `AudioTimelineMixer.cpp:200-210`.

**Tác động**

- Nếu lời thoại cuối kết thúc trước video/background, phần nhạc nền cuối bị cắt; video xuất có thể còn hình nhưng audio kết thúc sớm.

**Cần sửa**

- Duration đích phải lấy từ project/source media hoặc max của source, background và timeline policy.
- Test video 10 phút nhưng câu cuối ở phút 8; audio output vẫn phải đủ 10 phút.

### F-06 — P1 — Pipeline audio giữ toàn bộ file trong RAM

**Bằng chứng**

- Mixer cho phép tối đa 1 giờ và cấp `QVector<float>` toàn bộ timeline tại `AudioTimelineMixer.cpp:114-119`.
- Mỗi clip/background được decode đầy đủ tại `AudioTimelineMixer.cpp:129-150` và `176-199`.
- `src/audio/io/AudioFileDecoder.cpp:46-196` decode toàn file; fallback tạo/đọc full float WAV.
- Separation tách toàn bộ interleaved buffer thành các vector channel ở `src/separation/io/SeparationAudioIO.cpp:13-25`.
- Waveform Colab decode cả hai stem đầy đủ tại `src/controllers/separation/ColabVoiceIsolatorController.cpp:281-329`; local waveform đọc full WAV tại `src/controllers/separation/VoiceIsolatorController.cpp:376-445`.

**Tác động**

- Chỉ riêng buffer mono float 48 kHz dài 1 giờ đã khoảng 691 MB; cộng nguồn, background, clip, channel copies và Qt overhead dễ vượt 1–2 GB.
- Đây là nguyên nhân khả dĩ mạnh cho hiện tượng app lag, xoay vòng hoặc bị hệ điều hành đóng khi media dài. Đây là suy luận từ memory model; cần telemetry để gắn với từng incident cụ thể.

**Cần sửa**

- Decode/mix theo block, streaming output, envelope waveform theo bucket thay vì giữ sample đầy đủ.
- Đặt memory budget và telemetry RSS/working set theo job.
- Test media 30/60 phút trên máy RAM thấp.

### F-07 — P2 — OCR edit mode vẫn khóa thanh seek

**Bằng chứng**

- ROI overlay có z cao khi edit tại `qml/components/dubbing/DubbingSourceMediaPanel.qml:647-675`.
- Seek MouseArea tại `DubbingSourceMediaPanel.qml:885-903` có `enabled: !root.ocrRoiEditMode`.

**Tác động**

- Khi đang chỉnh vùng scan, user không thể kéo timeline để tìm frame khác; đúng với lỗi tương tác đã từng được báo.

**Cần sửa**

- Tách seek strip ra khỏi ROI gesture và giữ nó luôn nhận pointer.
- Thêm UIA test: bật ROI edit → kéo seek trái/phải → position thay đổi → ROI vẫn giữ tọa độ.

**Điểm đã đúng liên quan:** `VideoOutput.PreserveAspectFit` tại `DubbingSourceMediaPanel.qml:605-609`; poster chỉ là loading fallback tại `:611-631`; pause dùng `MediaPlayer.pause()`, nên source hiện tại không chủ ý quay về thumbnail khi pause.

### F-08 — P2 — Quét toàn bộ subtitle ở mỗi tick playback

**Bằng chứng**

- Handler `onPositionChanged` lặp toàn bộ segment để tìm cue active tại `DubbingSourceMediaPanel.qml:329-347`.

**Tác động**

- Với hàng nghìn cue, mỗi tick playback tạo O(n), gây giật video/UI.

**Cần sửa**

- Dùng active index có tính đơn điệu hoặc binary search theo `startMs/endMs`.
- Benchmark 5.000–20.000 cue trong playback 60 fps.

### F-09 — P1 — Shutdown còn các wait vô hạn

**Bằng chứng**

- `DubbingTranscriptionJob::~DubbingTranscriptionJob()` gọi `waitForFinished()` và `QThread::wait()` không timeout tại `src/controllers/dubbing/DubbingTranscriptionJob.cpp:68-79`.
- `DubbingExportJob::~DubbingExportJob()` gọi `waitForFinished()` không timeout tại `src/controllers/dubbing/DubbingExportJob.cpp:82-88`.
- Timing watcher trong `DubbingJobRunner::~DubbingJobRunner()` vẫn chờ vô hạn tại `src/controllers/dubbing/DubbingJobRunner.cpp:525-529`.
- Colab separation thread đã có timeout 10 giây tại `DubbingJobRunner.cpp:511-524`, cho thấy các nhánh khác chưa đồng nhất.

**Tác động**

- Khi đóng cửa sổ trong lúc worker/thư viện không trả về, GUI destructor có thể treo vô hạn.

**Cần sửa**

- Cooperative cancellation + bounded join cho mọi job.
- Không block GUI destructor; dùng shutdown coordinator và trạng thái “đang dừng”.
- Test worker cố tình không phản hồi rồi đóng app; process phải thoát trong deadline định trước.

### F-10 — P2 — Translate/Review hiển thị completed khi chỉ một cue có target text

**Bằng chứng**

- Code có cả `hasTargets` và `allTargets` tại `DubbingController_Workflow.cpp:17-24`.
- Translate/review lại dùng `hasTargets` tại `DubbingController_Workflow.cpp:131-145`, trong khi synth mới đòi điều kiện chặt hơn tại `:147-155`.

**Tác động**

- User thấy bước dịch đã hoàn tất nhưng sang TTS lại bị chặn; workflow state tự mâu thuẫn.

**Cần sửa**

- `completed` chỉ khi toàn bộ cue bắt buộc có target text; trường hợp một phần dùng state `partial` và `x/y translated`.

### F-11 — P2 — Export node hoàn tất dựa trên preview, không dựa trên file export

**Bằng chứng**

- Node export dùng `previewPath()` và detail `Preview rendered` tại `DubbingController_Workflow.cpp:175-179`.

**Tác động**

- Preview có nhưng final video chưa xuất vẫn có thể làm UI thể hiện export đã hoàn tất.

**Cần sửa**

- Mix state kiểm tra preview; export state kiểm tra `exportPath()` tồn tại, dung lượng > 0 và probe stream hợp lệ.

### F-12 — P2 — Policy khớp STT/OCR mặc định trái với hướng dẫn AI hiện hành

**Bằng chứng**

- Hướng dẫn yêu cầu ưu tiên OCR và dùng STT làm tham khảo/fallback tại `docs/AI_AGENT_TRANSCRIPT_RECONCILIATION_GUIDE.md:10-24`.
- Project mặc định `prefer-stt` tại `src/dubbing/project/DubbingProject.cpp:167-171`.
- Fusion service fallback cũng trả `prefer-stt` tại `src/dubbing/fusion/DubbingTranscriptFusionService.cpp:64-81` và áp dụng tại `:175-215`.

**Tác động**

- Cùng một cặp STT/OCR nhưng built-in fusion và AI IDE có thể sinh canonical transcript khác nhau.

**Cần sửa**

- Chọn một policy chuẩn duy nhất; nếu yêu cầu sản phẩm là OCR ưu tiên thì đổi default/migration/UI thành `prefer-ocr`.
- Test conflict text, missing OCR cue, missing STT cue và giữ nguyên OCR timeline.

### F-13 — P2 — SQLite model selection ghi không atomic và không trả lỗi cho caller

**Bằng chứng**

- `saveFileSelectionForFamily()` là `void`, lỗi chỉ log tại `src/core/storage/StudioSelectionRepository.cpp:90-116`.
- `saveActiveSelection()` gọi UPSERT thứ nhất rồi UPSERT thứ hai riêng biệt, không transaction tại `StudioSelectionRepository.cpp:118-152`.

**Tác động**

- Có thể lưu file selection nhưng không lưu active selection, hoặc ngược lại; UI không biết để báo user.

**Cần sửa**

- Bọc hai write trong transaction; trả `bool/error` lên controller.
- Test forced SQLite failure/foreign-key/disk-full và rollback.

### F-14 — P2 — Live 8-task workflow không nằm trong release gate

**Bằng chứng**

- `tests/main.cpp:49-66` có nhánh live runner đặc biệt.
- `tests/CMakeLists.txt:500-589` đăng ký 41 CTest nhưng không đăng ký full `LiveRealWorkflowRunner` thành test release bắt buộc.
- Gate hiện chạy unit/contract/QML smoke, không chạy media + model/Colab thật end-to-end.

**Tác động**

- 40/41 hoặc thậm chí 41/41 vẫn chưa chứng minh: token/tunnel thật, tải media thật, STT/OCR thật, TTS thật, nghe mix và export video thật.

**Cần sửa**

- Thêm một lane nightly/manual signed acceptance với fixture, credentials tạm thời, artifact/log/video bằng chứng.
- Không đưa secret vào CTest log hoặc project JSON.

### F-15 — P2 — Project được lưu nhưng app không tự mở lại project gần nhất

**Bằng chứng**

- `DubbingProject::save()` dùng `QSaveFile` và commit nguyên tử tại `src/dubbing/project/DubbingProject.cpp:206-233`.
- Constructor controller chỉ load history, không reopen project gần nhất tại `src/controllers/dubbing/DubbingController.cpp:594-597`.
- `qml/pages/DubbingPage.qml:18-22` mở entry gate khi chưa có project.

**Tác động**

- User dễ hiểu là “mất dự án” sau khi mở app lại dù file project còn trên đĩa.

**Cần sửa**

- Lưu `lastActiveProjectPath`, validate rồi reopen khi startup.
- Nếu file thiếu/hỏng, hiển thị recovery/history rõ ràng, không tạo project mới im lặng.

### F-16 — P1 — Resampling clip trong mixer sai thuật toán

**Bằng chứng**

- Mixer lặp theo sample nguồn rồi tính `destination = i * outputRate / clip.sampleRate` tại `AudioTimelineMixer.cpp:151-158`.
- Với 16 kHz → 48 kHz, chỉ mỗi sample đích thứ ba được điền; các sample giữa giữ 0. Với sample rate cao hơn 48 kHz, nhiều sample nguồn va vào cùng destination.
- Background lại dùng phép ánh xạ theo output index hợp lý hơn tại `AudioTimelineMixer.cpp:200-210`.
- Test mixer hiện chủ yếu dùng clip 48 kHz trong `tests/dubbing/test_DubbingProject.cpp:5338-5431`, nên không bắt lỗi này.

**Tác động**

- TTS thường trả 16/22,05/24 kHz; output có thể nghe rỗng, nhỏ, méo hoặc alias mạnh dù timeline đúng.

**Cần sửa**

- Dùng resampler thật (libswresample/soxr) hoặc interpolation theo từng output frame.
- Test 16k, 22.05k, 24k, 44.1k, 48k về duration, RMS, peak và continuity.

### F-17 — P1 — Chuẩn bị/copy media lớn còn chạy đồng bộ trên GUI call path

**Bằng chứng**

- `DubbingJobRunner::renderPreview()` decode toàn bộ uploaded voice chỉ để lấy duration trước khi dispatch render tại `src/controllers/dubbing/DubbingJobRunner.cpp:891-906`.
- Audio-only export dùng `QFile::copy` đồng bộ tại `src/controllers/dubbing/DubbingExportJob.cpp:180-195`.
- Package copy nhiều artifact đồng bộ tại `src/controllers/dubbing/parts/DubbingController_Artifacts.cpp:1-69`.
- CapCut export được gọi đồng bộ tại `DubbingController_Artifacts.cpp:691-732`; exporter copy source video/audio ở `src/dubbing/exporters/CapCutDraftExporter.cpp:531-572`.

**Tác động**

- File dài/hàng GB có thể làm event loop không repaint, Windows báo Not Responding.

**Cần sửa**

- Chuyển staging/copy/probe sang worker job có progress/cancel.
- Dùng `ffprobe` để lấy duration thay vì full decode.
- Commit output atomically sau khi worker hoàn thành.

### F-18 — P2 — QML test chưa replay các lỗi tương tác đã từng xảy ra

**Bằng chứng**

- Route smoke hiện có 19 action chủ yếu ở entry/preflight/config route.
- Không click thực tế Run STT/Run OCR, upload file picker từng task, Colab connect, seek video, kéo OCR ROI, history overlay, menu ba chấm, voice gallery.
- Một số kiểm tra QML trong `tests/dubbing/test_DubbingProject.cpp:3512-3545`, `:3898-3954`, `:4003-4009` dựa trên tìm chuỗi source thay vì tương tác runtime.

**Tác động**

- Control có thể tồn tại trong code nhưng bị che, không click được hoặc mở sai vị trí mà gate vẫn pass.

**Cần sửa**

- Bổ sung Windows UIA E2E trên packaged app với media fixture xác định.
- Chụp ảnh ở 1280×720, 1600×900 và 4K; chạy video > 1 phút; click đủ 8 task, model, Colab, upload, voice, menu, history.

### F-19 — P2 — Project JSON lưu path tuyệt đối, không relocatable

**Bằng chứng**

- Project ghi trực tiếp source/audio/clip/export path tại `src/dubbing/project/DubbingProject.cpp:35-78`.
- Load lại nguyên chuỗi path tại `DubbingProject.cpp:111-120` và `:190-201`.

**Tác động**

- Di chuyển project folder, đổi drive letter, chạy portable build trên máy khác hoặc restore backup sẽ làm asset link hỏng.

**Cần sửa**

- Asset nằm trong project root nên lưu relative path; external source lưu path + hash và hỗ trợ relink.
- Thêm migration/rebase và test move toàn bộ project sang thư mục/ổ khác.

### F-20 — P2 — Hướng dẫn AI chưa được app tạo handoff theo project

**Bằng chứng**

- Guide hiện yêu cầu “absolute paths supplied in the chat prompt” tại `docs/AI_AGENT_TRANSCRIPT_RECONCILIATION_GUIDE.md:3-8`.
- App có tạo root phân vùng `.workflow-artifacts/<projectId>/01-stt`, `02-ocr`, `03-review`, `04-translation` tại `src/controllers/dubbing/DubbingController.cpp:132-151`.
- Không tìm thấy QML/controller API tạo sẵn một prompt/handoff project-scoped chứa chính xác STT input, OCR input, translation source và output destination.

**Tác động**

- User/IDE vẫn phải tự ghép path trong chat; dễ chọn nhầm artifact của dự án khác và IDE phải search repository.

**Cần sửa**

- App tạo một handoff text ngắn từ project hiện tại, chỉ chứa project ID và absolute input/output paths đã resolve.
- Guide chỉ mô tả quy tắc khớp/dịch; không bắt IDE tìm code/manifest.
- Test hai project cùng tên file nhưng project ID khác; prompt của mỗi project chỉ được trỏ vào root tương ứng.

### F-21 — P2 — App idle vẫn tiêu thụ CPU cao vì animation vô hạn

**Bằng chứng**

- Đo bản EXE trong 21,03 giây ở Home: 4,812 CPU-second, tương đương 22,9% của một logical core; không có mẫu `Not Responding`.
- `communityItem.spotlight` mặc định bật khi Community chưa active và chuột không hover tại `qml/components/Sidebar.qml:329-332`.
- Sáu animation chạy `Animation.Infinite` tại `Sidebar.qml:469-529`.

**Tác động**

- App không làm gì vẫn dùng CPU/GPU, tăng nhiệt, pin và cạnh tranh tài nguyên với FFmpeg/AI worker.
- Trên máy yếu, animation nền có thể làm cảm giác video/scroll giật hơn dù event loop chưa treo.

**Cần sửa**

- Chỉ animate hữu hạn trong onboarding hoặc khi hover; dừng khi cửa sổ inactive/minimized.
- Tôn trọng reduced motion và thêm gate đo idle CPU sau khi UI ổn định.

### F-22 — P2 — Dubbing view tăng mạnh và giữ bộ nhớ suốt phiên

**Bằng chứng**

- Mở blank Dubbing project làm working set tăng 332,9→438,5 MB và private memory 317,0→448,8 MB trong khoảng 2,137 giây.
- `dubbingLoader.active` là `stack.currentIndex === 8 || dubbingLoaded`; `dubbingLoaded` được đặt `true` vĩnh viễn sau lần load đầu tại `qml/Main.qml:899-906`.

**Tác động**

- Chưa có media/model mà trang đã giữ thêm khoảng 132 MB private; sau khi rời Dubbing, view/media object vẫn không được giải phóng.
- Với full-memory audio ở F-06, tổng footprint dễ tăng đến mức paging/lag trên máy ít RAM.

**Cần sửa**

- Giữ controller/project state nhưng unload view/media nặng khi rời route; cache chỉ state nhẹ cần thiết.
- Đo memory delta sau enter/leave lặp và đặt budget; kiểm tra leak bằng heap profiler.

### F-23 — P2 — QML smoke làm bẩn lịch sử project thật

**Bằng chứng**

- Project gate thực tế hiển thị 26 mục `qml-route-smoke.ladub` từ các thư mục build/package smoke.
- Script đặt `LASTUDIO_DATA_DIR` ở `scripts/package.ps1:870-883`, fixture/project smoke được tạo ở `src/main.cpp:127-165`.
- Nhưng history luôn dùng `QStandardPaths::AppDataLocation/history/dubbing_history.json` tại `src/controllers/dubbing/parts/DubbingController_Project.cpp:392-397`; biến test không cô lập history.

**Tác động**

- Test phát hành thay đổi dữ liệu người dùng và làm project gate dài, khó chọn đúng dự án.
- Vi phạm tính cô lập của test; mỗi lần build có thể tiếp tục thêm mục trùng.

**Cần sửa**

- Trong smoke mode, redirect toàn bộ app data/settings/history sang root test hoặc inject history store riêng.
- Test assert lịch sử thật không đổi trước/sau package smoke và dọn fixture bằng scope guard.

### F-24 — P2 — Task rail Dubbing khó khám phá ở độ phân giải thực tế

**Bằng chứng**

- Ảnh `10-home-1280x720.png`, `13-dubbing-empty-1280x720.png`, `14-dubbing-empty-1600x900.png` cho thấy rail ngang bị khuất các bước cuối.
- Ở 1280×720 chỉ nhìn rõ tới bước 5; ở 1600×900 bước 8 vẫn ngoài viewport. Rail có cuộn ngang nhưng dấu hiệu cuộn không đủ rõ.
- Panel phải được giữ theo yêu cầu sản phẩm, song empty state tại Import vẫn chiếm gần nửa chiều rộng và gần như trống.

**Tác động**

- User có thể không biết còn bước 7/8 hoặc tưởng workflow chỉ có các bước đang nhìn thấy.
- Không gian thao tác video/import bị giảm trong khi panel kết quả chưa có dữ liệu.

**Cần sửa**

- Bảo toàn panel phải nhưng cho phép co về min-width/empty-state compact và mở rộng khi có nội dung.
- Thêm nút prev/next/gradient edge hoặc auto-scroll current step vào giữa; bảo đảm bước hiện tại và bước kế tiếp luôn thấy.

### F-25 — P2 — Người dùng mới gặp hai modal chặn liên tiếp

**Bằng chứng**

- `WelcomePage.qml:75-78` tự mở first-run dialog.
- `Main.qml:313-335` đồng thời tự mở consent kiểm tra cập nhật nếu chưa từng hỏi.
- Live run thực tế phải xử lý hai dialog trước khi vào chức năng.

**Tác động**

- First-use bị ngắt nhịp và cảm giác app khởi động chậm hơn số đo window-ready.
- Hai modal độc lập dễ tranh focus/overlay trên màn hình nhỏ.

**Cần sửa**

- Gộp consent thành một lựa chọn trong onboarding hoặc hoãn đến sau khi onboarding đóng.
- Chỉ có một modal blocking tại một thời điểm; đo “time to first actionable task”, không chỉ time to window.

## 4. Đánh giá theo 8 bước Dubbing

| Bước | Kết quả audit | Lỗi/rủi ro chính |
|---|---|---|
| 1. Import | **Amber** | Project save atomic; chưa auto-reopen, path tuyệt đối, tác vụ file lớn có thể block |
| 2. Normalize | **Amber** | Hai-pass loudnorm/FFmpeg filter hợp lệ; vẫn cần live media matrix và streaming/memory telemetry |
| 3. Separate (optional) | **Amber/Red** | Contract worker/upload có; full-memory decode/waveform, copy/extension mismatch, live Colab chưa nghiệm thu |
| 4. Transcribe (STT/OCR độc lập) | **Amber** | Contract độc lập có; OCR seek bị khóa khi edit, fusion policy mâu thuẫn, live STT/OCR chưa chạy |
| 5. Translate | **Amber** | Target partial có thể bị báo complete; AI handoff chưa project-scoped tự động |
| 6. Synthesize | **Red** | Partial clips được coi đủ; mixer resample sai với sample rate phổ biến của TTS |
| 7. Align | **Amber** | Có test workflow; shutdown watcher có thể chờ vô hạn, chưa nghe/đo live aligned output |
| 8. Mix & Export | **Red** | Duration bị cắt, thiếu clip thành silence, full-RAM, extension/container mismatch, preview/export state sai |

## 5. Các phần đã xác nhận tốt

- Mặc định source/target language là **Chinese (`zh`) → Vietnamese (`vi`)** tại `src/dubbing/project/DubbingProject.cpp:124-125` và default model tương ứng trong header.
- Project JSON dùng `QSaveFile`, nên một lần save thành công được commit nguyên tử.
- URL/token Direct Colab được thiết kế giữ trong session, không đưa vào project JSON/settings; grep source không thấy token được serialize vào `DubbingProject::toJson()`.
- Logger có cơ chế che Bearer/token/API key, home path và text nhạy cảm.
- QML lint và route smoke hiện tại đều qua.
- Contract notebook/worker/binding và hash checks hiện tại đều qua.
- Bundled FFmpeg có các filter cần thiết; syntax loudnorm/sidechain primitives tồn tại.
- Source hiện tại dùng `VideoOutput.PreserveAspectFit`; khung video không cố crop video dọc/vuông.
- STT và OCR có surface chạy/upload riêng; vấn đề chính còn lại là interaction/live acceptance chứ không phải thiếu hoàn toàn UI contract.

## 6. Thứ tự sửa đề xuất

### Nhóm A — Chặn phát hành

1. F-16: thay resampler và thêm multi-sample-rate tests.
2. F-04 + F-05: coverage 100% và duration theo source/project.
3. F-03: không bao giờ raw-copy codec khác dưới đuôi `.wav`.
4. F-01: instrument và triệt race Runtime Host cho stress test ổn định.
5. F-06 + F-17: streaming/chunking và loại bỏ full decode/copy khỏi GUI path.
6. F-09: bounded shutdown cho mọi watcher/thread.
7. F-02: đóng gói source snapshot sạch trước build phát hành.

### Nhóm B — Hoàn thiện logic và UX

1. F-07: seek được khi OCR ROI edit.
2. F-10 + F-11: state `partial/ready/completed` đúng dữ liệu thật.
3. F-12: đồng nhất OCR/STT fusion policy với yêu cầu sản phẩm.
4. F-13: SQLite transaction + lỗi trả về UI.
5. F-15 + F-19: auto-reopen và path relocatable.
6. F-08: tối ưu subtitle lookup.
7. F-20: app sinh AI handoff đúng project.
8. F-21: dừng animation vô hạn khi idle/inactive và thêm idle CPU budget.
9. F-22: unload Dubbing view/media nặng khi rời route, giữ state nhẹ.
10. F-23: cô lập hoàn toàn settings/history/data của QML smoke.
11. F-24: làm task rail dễ khám phá và co panel phải theo empty state.
12. F-25: tuần tự hóa/gộp onboarding và update consent.

### Nhóm C — Gate bắt buộc trước bản EXE tiếp theo

1. Prebuild gate phải PASS toàn bộ, không rerun để “né” flaky test.
2. Runtime Host cold-start stress tối thiểu 100 lần không fail/hang.
3. Audio matrix: WAV/FLAC/MP3 input; TTS 16/22.05/24/44.1/48 kHz; mono/stereo; 30–60 phút.
4. Full project reopen/move/relink test.
5. E2E UIA đủ 8 task và các action Model/Colab/Upload/Run/Skip/Continue.
6. Live Dubbing acceptance: media thật → optional separation → STT/OCR → fusion → translate → TTS → align → mix → video export; nghe/xem output và giữ log/artifact/video bằng chứng.
7. CapCut package được probe đúng codec/container và thử import/open trên CapCut thật.
8. UX performance budget trên packaged EXE:
   - first usable action ≤5 giây cold start;
   - phản hồi click/navigation ≤100 ms, nội dung route cơ bản sẵn sàng ≤1 giây;
   - idle CPU trung bình <1% sau khi animation settle;
   - không có `Not Responding` quá 500 ms;
   - thao tác dài hiện progress trong 300 ms, Cancel được xác nhận trong 1 giây;
   - enter/leave Dubbing lặp không tăng memory không giới hạn;
   - playback fixture 90 giây không stall hình/âm thanh quá 250 ms.

## 7. Tiêu chí nghiệm thu cuối cùng

Chỉ được kết luận workflow “ổn” khi đồng thời thỏa:

- 41/41 CTest PASS và Runtime Host stress PASS.
- Không có công việc file/audio/video lớn chạy đồng bộ trên GUI event loop.
- Không cue bắt buộc nào thiếu transcript, translation hoặc clip mà workflow vẫn báo completed.
- Mix đủ thời lượng video, sample rate nào được hỗ trợ cũng cho audio đúng.
- Đóng app trong mọi stage hoàn tất trong deadline, không treo.
- Project mở lại đúng state, di chuyển được hoặc có relink rõ ràng.
- Upload artifact độc lập Colab được test với file picker thật và artifact được task sau sử dụng.
- UI không clip/overlap và mọi nút nhận click ở ba độ phân giải.
- Idle CPU, route latency, memory delta, progress/cancel và playback đều đạt UX performance budget ở Nhóm C.
- Một full live run có video/audio/log làm bằng chứng.
- Binary đi kèm commit/source manifest sạch, tái build được.

## 8. Phạm vi thay đổi của audit này

Audit này **không sửa code và không build EXE mới**. Audit có mở bản đóng gói 0.0.9.0 hiện hữu để đo startup, idle, route, Dubbing load, responsive và idle shutdown; không chạy workload AI/Colab thật. Ngoài báo cáo và artifacts đo UX trong `out/ux-audit-20260904/`, toàn bộ source/working tree hiện hữu của người dùng được giữ nguyên. Lịch sử project người dùng đã được khôi phục đúng checksum trước test.

## 9. Remediation update — 2026-09-05

Đây là bản cập nhật sau remediation, không thay thế các số đo độc lập ban đầu
ở trên.

| Finding(s) | Trạng thái source/regression | Bằng chứng mới |
|---|---|---|
| F-01 | Đã sửa race lúc nhận `Hello` của Runtime Host và tách handshake timeout khỏi inference timeout. | `TestRuntimeHostProtocol::rapidlyRestartsHostWithoutLosingHandshake` chạy 5×20: **100/100** restart/auth PASS. |
| F-02, F-23 | Đã thêm source manifest cho package và cô lập smoke data/history. | `package.ps1` kiểm version source và ghi commit/diff/executable hash; prebuild gate PASS. |
| F-03–F-06, F-16 | Mixer đã có graph sidechain đúng, coverage bắt buộc, duration theo source và regression sample-rate. | `TestDubbingProject` PASS trong full CTest; audio matrix gồm 16/22.05/24/44.1/48 kHz. |
| F-07–F-13, F-15, F-19–F-20 | Điều khiển ROI/player, workflow state, fusion OCR-first, selection transaction, reopen/relocation và handoff project-scoped đã được sửa. | QML workspace contract và project/controller regressions PASS. |
| F-09, F-17 | Các wait/copy/probe nặng được đặt trong đường worker/process có deadline hoặc hủy có giới hạn. | Full CTest/PRE_DELIVERY gate PASS; không có assertion nào bỏ qua deadline để sign-off. |
| F-21–F-25 | Giảm animation idle, lifecycle view Dubbing, responsive inspector/task navigation và modal onboarding được đưa vào QML contract. | QML lint và route smoke PASS. |
| F-14, F-18 | Có lane acceptance/manual, contract remote và packaged smoke; không có token live được đưa vào CTest. | `RemoteLivePreflightContract` PASS trong CTest; live GPU, CapCut và thao tác GUI dài vẫn phải được người vận hành nghiệm thu. |

Kết quả gate tại thời điểm cập nhật: full CTest **41/41 PASS**, QML lint
**PASS**, `git diff --check` **PASS**, Graphify refresh **PASS**, prebuild gate
**10/10 PASS**. Portable candidate 0.0.9.1 được tạo bởi package gate; nó có
manifest nguồn/checksum và packaged QML smoke. Điều này vẫn không thay thế
nghiệm thu thủ công live GPU, CapCut và thao tác GUI dài nêu ở trên.
