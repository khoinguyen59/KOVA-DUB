# TÀI LIỆU TỔNG HỢP TOÀN BỘ NGỮ CẢNH & KẾT QUẢ KIỂM ĐỊNH DỰ ÁN
*(COMPREHENSIVE MASTER HANDOFF & AUDIT REPORT)*

- **Dự án**: LA Studio (Desktop AI Dubbing & TTS Studio - C++20 / Qt 6.9.3 QML trên Windows)
- **Mã phiên đối chiếu**: `ca302975-7af1-4690-b0dc-4ba90bf9e1d0`
- **Tài liệu căn cứ**:
  - `PRE_DELIVERY_CHECKLIST.md`
  - `docs/INDEPENDENT_TECHNICAL_AUDIT_BRIEF.md`
  - `AGENTS.md` & `.codex/skills/la-studio-delivery/SKILL.md`

---

## MỤC 1. TỔNG HỢP CÁC KẾT QUẢ NGHIÊN CỨU & KIỂM ĐỊNH ĐÃ THỰC HIỆN

### 1.1. Bảng so sánh Benchmark AI (Gemini 3.8 Flash vs các mô hình AI)
*(Được thực hiện theo yêu cầu đầu tiên của phiên)*
- **Gemini 3.8 Flash** so sánh cùng phân khúc Speed/Efficiency và Frontier:
  - **Claude 3.5 Sonnet / 3.7 Sonnet**: Sonnet dẫn đầu về khả năng coding phức tạp và reasoning sâu, nhưng chi phí cao hơn và latency lớn hơn đáng kể so với dòng Flash.
  - **Gemini 1.5/2.0 Flash & Flash-Lite**: Dòng Flash tối ưu cho độ trễ cực thấp (<250ms time-to-first-token), cửa sổ ngữ cảnh khổng lồ (1M - 2M tokens), cực kỳ phù hợp làm model router, subtitle alignment, audio token translation và agent tool use.
  - **Llama 3.3 70B / DeepSeek V3**: Lựa chọn tự host (Self-hosted/Local/Colab) chi phí rẻ, trong khi Gemini Flash là lựa chọn hàng đầu cho API Gateway nhờ tốc độ stream và chi phí cận 0 cho input token.

---

### 1.2. Kết quả Audit Kỹ thuật Độc lập (Theo `INDEPENDENT_TECHNICAL_AUDIT_BRIEF.md`)
Toàn bộ các bài kiểm thử thực tế độc lập đã được chạy bằng lệnh shell và xác thực qua bằng chứng:

| Hạng mục kiểm tra | Lệnh / Script thực thi | Kết quả thực tế | Chi tiết bằng chứng |
|---|---|:---:|---|
| **Bộ 41 bài kiểm thử C++ (CTest)** | `ctest --test-dir out\build\windows-msvc-release --output-on-failure -j 4` | **PASS (41/41)** | 100% tests pass (24.76s). Không có test nào fail. |
| **Hợp đồng Colab Notebooks chuẩn** | `python scripts/verify_generated_colab_notebooks.py` | **PASS (32/32)** | 32/32 notebook Colab khớp chính xác model bindings. |
| **Cô lập phụ thuộc runtime** | `python scripts/test_notebook_repository_dependencies.py` | **PASS (1/1)** | Không có lệnh `git clone` repo app lúc runtime trong bất kỳ notebook nào. |
| **Gói hợp nhất Unified Dubbing** | `python scripts/test_unified_dubbing_bundle.py` | **PASS (3/3)** | Bundle tự chứa 34 files, độc lập hoàn toàn. |
| **Khớp mã nguồn Worker Colab** | `python scripts/verify_colab_worker_pins.py` & `test_colab_worker_pins.py` | **PASS (5/5)** | 2/2 worker nhúng (Spleeter & Coordinator) khớp 100% mã SHA-256 local. |
| **Hợp đồng Acceptance Trực tiếp Colab** | `python scripts/test_live_colab_acceptance_contract.py` | **PASS (9/9)** | 9 capability paths đạt chuẩn acceptance. |
| **Lộ trình tính năng từ xa (Remote Surface)** | `powershell scripts/verify_remote_feature_surface.ps1` | **PASS (8/8)** | 8/8 direct Colab routes được xác thực. |
| **Kiểm tra cú pháp QML (QmlLint)** | `powershell scripts/lint_qml.ps1` | **PASS** | Exit code 0, 0 warnings. |
| **Thực thi bản build .EXE thực tế** | Kiểm tra `out/package-smoke/0.0.9.0/` | **PASS** | `qml-interaction-trace.json` ghi nhận 19 interaction events; `stderr.log` rỗng (0 bytes). |
| **Cấu trúc đóng gói Release** | `out/LA-Studio-0.0.9.0/` | **PASS** | Cấu trúc phẳng (Flat root), **không có thư mục con `bin/`**, đầy đủ DLL, `media-tools/` (FFmpeg), `subtitle-ocr/` (Tesseract), `data/`. |

---

### 1.3. Kết quả đối chiếu Bảng kiểm tra tiền phát hành (`PRE_DELIVERY_CHECKLIST.md`)
1. **Dãy Incident (Mục II - Incident Log)**:
   - Đã chạy kiểm tra tự động toàn bộ ID từ `INC-001` đến `INC-047`.
   - Kết quả: **Đầy đủ liên tục 47/47 incidents**, không bị đứt đoạn hoặc khuyết thiếu số nào (`Max: 47, Missing: []`).
2. **Dòng 85 (`Project resume persistence`) trong `PRE_DELIVERY_CHECKLIST.md`**:
   - Hiện trạng tài liệu: Đang để trống `* [ ]`.
   - Đối chiếu mã nguồn: `tests/dubbing/test_DubbingProject.cpp` (`workflowResumeStateSurvivesControllerClose` dòng 1131-1173 và `reopenedStt/reopenedOcr` dòng 3376-3428) đã kiểm thử việc ghi `QSaveFile`, đóng hoàn toàn controller, mở lại file `.ladub.json` và xác thực toàn bộ `sourcePath`, `zh->vi`, `segments`, `currentStepId`, `stepOutputs`. Test này thuộc CTest #38 và đã **PASS 100%**.
   - Kết luận: Checkbox chỉ bị sót chưa đánh dấu `[x]` trên file markdown.
3. **Điểm nghẽn duy nhất của `prebuild_gate.ps1`**:
   - `scripts/prebuild_gate.ps1:180` truyền cờ `-SkipAppBuildDependency` vào `scripts/run_tests.ps1`.
   - Tại `scripts/run_tests.ps1:321`, khi cấu hình CMakeCache chưa có cờ này, script gọi `Remove-Item -LiteralPath $resolvedBuild -Recurse -Force`.
   - Trên Windows, tiến trình antivirus hoặc handle file đang mở khóa file `.rcc\qmlcache\...\AlignmentOptionField_qml.cpp` gây ra `IOException`, làm gián đoạn cổng build tự động dù mã nguồn và các bài kiểm tra đều pass.

---

## MỤC 2. TRIẾT LÝ THIẾT KẾ ĐÃ THỐNG NHẤT CÙNG CHỦ DỰ ÁN (USER UX PHILOSOPHY)

Chủ dự án đã xác định rõ triết lý thiết kế cho LA Studio:
1. **Silent Logging (Ghi log ngầm - Giao diện sạch sẽ)**:
   - Các lỗi kỹ thuật nền (FFmpeg crash, GPU Out-of-memory, lỗi kết nối mạng tạm thời, lỗi parse JSON ngầm...) **phải được ghi vào hệ thống log (`Logger::error("DubbingPipeline", ...)` hoặc tab Developer)**.
   - **Tuyệt đối không hiện banner lỗi đỏ lòe loẹt hay popup cảnh báo tràn màn hình** gây hoang mang cho người dùng thông thường.
2. **Actionable Guidance (Chỉ tương tác khi cần User can thiệp/giúp đỡ)**:
   - Giao diện chỉ đưa ra phản hồi trực quan khi và chỉ khi hành động đó **bắt buộc người dùng phải lựa chọn hoặc xử lý** (ví dụ: cần chọn file, cần chọn ngôn ngữ đích, hoặc cần quyết định câu xung đột giữa STT và OCR).
   - Không thể hiện dưới dạng "báo lỗi" hay "phạt lỗi", mà thể hiện bằng **gợi ý hành động tự nhiên (inline contextual hint, tooltip nhẹ, đổi trạng thái nút bấm kèm hành động nhanh)** ngay tại nút bấm ngữ cảnh.

---

## MỤC 3. CHI TIẾT 13 PHÁT HIỆN CHUYÊN SÂU TRÊN TAB DUBBING & PHƯƠNG ÁN XỬ LÝ

### Nhóm 1: Logic & Điều hướng luồng (Workflow & Navigation)
1. **Lỗi nuốt lỗi âm thầm trong Step-by-Step (Đã được điều chỉnh theo triết lý của User)**:
   - `DubbingController::setError()` đã ghi vào `Logger::error`.
   - *Phương án thống nhất*: Giữ nguyên việc không hiện banner lỗi đỏ; chỉ cung cấp gợi ý hành động nhẹ ở các nút bấm ngữ cảnh khi cần user giúp.
2. **Nút Dịch thuật (Translate) ở Bước 5 khi còn xung đột STT/OCR**:
   - *Vị trí*: `DubbingTranslateStep.qml:143` vs `DubbingController_Stages.cpp:501-509`.
   - *Hiện trạng*: Nút Dịch cho phép bấm, nhưng C++ từ chối ngay lập tức vì còn câu xung đột chưa duyệt.
   - *Phương án xử lý*: Thêm dòng gợi ý nhẹ bên dưới nút Dịch: *"Có X câu STT/OCR chưa khớp"* kèm nút bấm nhanh *"Duyệt nhanh theo STT"* để giải quyết ngay tại chỗ.
3. **Nút Tiếp tục ở Bước 6 (TTS) cho phép nhảy cóc sang Bước 8**:
   - *Vị trí*: `DubbingSynthesizeStep.qml:643`.
   - *Hiện trạng*: Điều kiện `|| segments.length > 0` khiến nút Tiếp tục luôn sáng dù chưa tạo file âm thanh nào.
   - *Phương án xử lý*: Chỉ bật sáng nút Tiếp tục khi đã có âm thanh (`generatedClipCount > 0` hoặc `hasSynthesizedAudio`).
4. **Copy-paste nhầm biến xung đột ở Bước 7 (Align)**:
   - *Vị trí*: `DubbingAlignmentStep.qml:22, 88-89`.
   - *Hiện trạng*: Kiểm tra `unresolvedTranscriptConflictCount` (xung đột chữ của Bước 4) thay vì xung đột thời gian của Bước 7.
   - *Phương án xử lý*: Trỏ về đúng biến đếm `root.dubbing.timingConflicts.length`.
5. **Chuyển Tab làm mất trạng thái làm việc của Dubbing**:
   - *Vị trí*: `Main.qml:903` & `DubbingPage.qml:18-21`.
   - *Hiện trạng*: `Loader` hủy `DubbingPage` khi chuyển tab khiến khi quay lại bị reset về Bước 1 (`reviewStepId = "import"`).
   - *Phương án xử lý*: Lưu và khôi phục lại đúng bước người dùng đang thao tác dở (`currentStepId`).

### Nhóm 2: Trải nghiệm người dùng (UX Frictions)
6. **Bước 3 (Tách giọng - Separate) thiếu nút "Bỏ qua"**:
   - *Vị trí*: `DubbingSeparateStep.qml:103-109`.
   - *Hiện trạng*: Bước này là tùy chọn nhưng nút Tiếp tục bị mờ nếu chưa tách, không có nút bỏ qua trên bảng điều khiển chính.
   - *Phương án xử lý*: Thêm nút nhẹ nhàng *"Bỏ qua bước này ➔"*.
7. **Hộp thoại Upload thủ công không tự đóng**:
   - *Vị trí*: `DubbingArtifactUploadDialog.qml:67`.
   - *Hiện trạng*: Nạp đủ file thì bước sau tự chuyển nhưng dialog vẫn che màn hình.
   - *Phương án xử lý*: Tự động gọi `root.close()` khi đã nạp đủ tất cả file yêu cầu.
8. **Thiếu nút Thêm phân đoạn (Add Segment) & Tách câu (Split)**:
   - *Vị trí*: `DubbingReviewPanel.qml:715` (chỉ có `removeSegment`).
   - *Hiện trạng*: C++ có `addSegment()` nhưng QML không có nút bấm thêm câu thoại nếu AI nhận diện thiếu.
   - *Phương án xử lý*: Bổ sung nút "Thêm câu thoại" nhỏ gọn ở đầu/cuối bảng phân đoạn.
9. **Nút chọn phân đoạn bị vướng `z: -1`**:
   - *Vị trí*: `DubbingReviewPanel.qml:553-560`.
   - *Hiện trạng*: MouseArea đặt `z: -1` bị các phần tử chữ bên trên chặn click.
   - *Phương án xử lý*: Điều chỉnh lại thứ tự phân lớp hoặc xử lý click ở cấp độ hàng.

### Nhóm 3: Giao diện & Hiển thị (UI & Media)
10. **Timeline Waveform bị rỗng (Chưa bind dữ liệu)**:
    - *Vị trí*: `DubbingTimelineSection.qml:118-128`.
    - *Hiện trạng*: `WaveformView` không được truyền `samples` và `playbackProgress`, luôn hiện "No audio data".
    - *Phương án xử lý*: Kết nối dữ liệu audio phân tích (`normalizedAudioPath` hoặc `dubbedVocalPath`) vào `WaveformView`.
11. **Mốc thời gian hiển thị dạng số mili-giây thô và bị cắt chữ**:
    - *Vị trí*: `DubbingReviewPanel.qml:568`.
    - *Hiện trạng*: Hiển thị `75000–82000` và bị cắt thành `75000–8...`.
    - *Phương án xử lý*: Chuyển sang định dạng phút:giây chuẩn (`01:15 – 01:22`).
12. **Lệch đồng bộ âm thanh/video (Clock Drift) khi phát video dài**:
    - *Vị trí*: `DubbingSourceMediaPanel.qml:293-324`.
    - *Hiện trạng*: Sử dụng 3 `MediaPlayer` độc lập không có cơ chế bù lệch đồng hồ sau 1-2 phút phát.
    - *Phương án xử lý*: Thêm timer kiểm tra và cân bằng lại độ lệch micro-drift.
13. **Tính năng Đa giọng nói (Multi-Speaker) chưa có UI**:
    - *Vị trí*: `DubbingSynthesizeStep.qml`.
    - *Hiện trạng*: Backend có `speakers` nhưng UI chỉ cho phép gán 1 giọng đọc chung cho tất cả câu thoại.
    - *Phương án xử lý*: Dự kiến nâng cấp cho phép chọn giọng theo từng Speaker khi mở rộng tính năng.

---

## MỤC 4. CÂU LỆNH ĐỂ MỞ TAB CHAT MỚI

Khi bạn mở một tab chat mới, chỉ cần dán dòng lệnh sau vào ô chat:

```text
Đọc file @docs/DUBBING_AUDIT_HANDOFF.md và tiếp tục công việc tối ưu tab Dubbing từ phiên trước.
```
