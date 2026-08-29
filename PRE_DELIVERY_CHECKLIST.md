# 📋 BẢNG KIỂM TRA CHẤT LƯỢNG TRƯỚC KHI BÀN GIAO (PRE-DELIVERY QA CHECKLIST)

> [!IMPORTANT]
> **QUY TẮC BẮT BUỘC ĐỐI VỚI AI AGENT / DEVELOPER**:
> Trước khi bàn giao bất kỳ tính năng, bản sửa lỗi, hoặc bản build mới nào cho người dùng, **BẮT BUỘC PHẢI ĐỌC VÀ THỰC HIỆN ĐẦY ĐỦ CÁC BƯỚC KIỂM THỬ TRONG FILE NÀY**.
> Vượt qua 41 bài test CTest chỉ là điều kiện nền tảng cơ bản (baseline), **KHÔNG ĐƯỢC PHÉP** bỏ qua các bước kiểm thử thực tế bên dưới.

> **BẢN HỢP NHẤT:** File này là bản checklist duy nhất của dự án, được hợp nhất từ checklist tại thư mục workspace và bản checklist đã cập nhật trong `LA-Studio`. Các mục trùng nhau đã giữ theo bản mới nhất; incident log, quy tắc release gate, kiểm thử QML/EXE/Colab, yêu cầu sign-off và các ghi chú xử lý lỗi đã được đồng bộ tại đây.

---

## 0. CỔNG TỰ ĐỘNG TRƯỚC MỌI BẢN BUILD (MANDATORY PRE-BUILD GATE)

Đây là cổng bắt buộc được gọi tự động bởi `scripts\package.ps1` trước khi CMake configure/build. Một check thất bại sẽ trả exit code khác 0 và **chặn build**, không tạo bản phát hành chưa được kiểm tra.

Chạy thủ công khi cần recheck trước khi build:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\prebuild_gate.ps1 `
  -Preset windows-msvc-release `
  -QtRoot .tools\Qt\6.9.3 `
  -MaxParallelJobs 4
```

Cài một lần cho checkout để tự động kiểm tra theo vòng đời Git:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\install_git_hooks.ps1
```

Hook `pre-commit` chạy kiểm tra local không cần mạng; hook `pre-push` kiểm tra
payload worker/launcher nhúng và SHA local, cũng không phụ thuộc mạng hay
GitHub repository của dự án. Có thể gỡ bằng cùng lệnh kèm
`-Uninstall`; dù hook bị tắt, prebuild/CI/release vẫn bắt buộc chạy gate.

Cổng tự động kiểm tra: file bắt buộc và toàn bộ incident `INC-001` đến incident mới nhất, whitespace của Git, catalog/runtime ABI, C++/CTest và QML route smoke, QML lint, binding exact-model giữa controller/UI/notebook, notebook sinh tự động, tính toàn vẹn worker/launcher Colab nhúng, Unified Dubbing Colab và remote feature surface. Kết quả gần nhất được lưu tại `out\prebuild-gate\latest.json`; bằng chứng worker nhúng nằm tại `out\prebuild-gate\colab-worker-pins.json`. Sau khi stage, `package.ps1` còn chạy packaged EXE smoke; trace nằm tại `out\package-smoke\<version>\`.

Quy tắc release: chỉ dùng `scripts\package.ps1` làm entry point để build portable/installer. Chạy CMake trực tiếp chỉ là build phát triển và không được coi là đã vượt release gate. Live binary smoke, kiểm tra staging/runtime và kiểm thử GUI thực tế vẫn là bước hậu kiểm sau khi package tạo xong.

### Quy tắc bắt buộc: recheck tương đương trên toàn bộ 8 task sau mọi bug fix

Không được giới hạn việc kiểm tra vào đúng màn hình hoặc task nơi người dùng phát hiện lỗi. Mỗi khi sửa một lỗi ở một task — ví dụ Error Guidance, model gate, nút Run, Colab, Upload, voice, player, timeline, subtitle, layout hoặc state — phải thực hiện một **cross-task regression sweep** cho đủ 8 task canonical:

`1 Import · 2 Normalize · 3 Separate (optional) · 4 Transcribe · 5 Translate · 6 Synthesize · 7 Align · 8 Mix & Export`.

Với mỗi task, phải kiểm tra tối thiểu các bề mặt tương đương sau:

1. Entry và navigation: task mở đúng, không bị task khác chiếm UI, nút Back/Continue/Run có phản hồi.
2. Configuration gate: thiếu model/runtime/Colab phải mở đúng picker/setup; cấu hình hợp lệ không được mở lại picker; Separate vẫn optional.
3. Error path: lỗi kỹ thuật vẫn ghi log; lỗi có thể xử lý phải hiện hướng dẫn đúng ngữ cảnh; không được phát sinh generic modal sai task hoặc popup chặn thao tác tiếp theo.
4. State và concurrency: trạng thái `idle/running/completed/failed`, cancel, retry, chuyển task và persistence không rò sang task kế bên.
5. Artifact/workflow handoff: input/output, Upload, Continue và điều kiện prerequisite đúng với task; không dùng nhầm output của task khác.
6. UI parity: button/action visibility, dialog scope, text elision, scroll, clipping, layout responsive và keyboard/mouse hit target.

Kết quả phải được ghi thành ma trận `8 task × bề mặt kiểm tra` trong report hoặc evidence của lượt sửa. Chỉ kiểm tra riêng task bị báo lỗi **không đủ điều kiện sign-off**, kể cả khi unit test của task đó PASS. Nếu một task không áp dụng một tính năng, phải ghi rõ `N/A — verified not applicable`, không bỏ trống. Quy tắc này áp dụng cho cả sửa C++, QML, Python engine, notebook, packaging và tài liệu kiểm thử.

### Trạng thái thay đổi hiện tại — Transcribe STT/OCR độc lập

* [x] Giao diện production đã tách thành hai card xếp dọc: `STT · Speech-to-Text` và `OCR · Subtitle OCR`. Mỗi card có Model, Colab, Upload và Run riêng.
* [x] `Run STT` gọi route `stt`/`runSpeechToTextIndependently()`; `Run OCR` gọi route `ocr`/`runSubtitleOcrIndependently()`. Không còn dùng một lựa chọn `transcriptSource` để quyết định worker của card còn lại.
* [x] Một trong hai nguồn đã có segment là đủ bật Continue. Reconcile chỉ hiện khi cả `sttSegments` và `ocrSegments` tồn tại; không bắt người dùng phải chạy cả hai.
* [x] STT và OCR chỉ được chạy đồng thời với nhau; các workflow/queue/fix/stage khác vẫn bị busy gate. Hoàn thành STT không tự chuyển khỏi màn hình Transcribe; OCR cũng được giữ trên cùng màn hình.
* [x] Upload transcript là handoff local độc lập với Colab. Khi thay output đang chạy, STT chỉ hủy STT; OCR chỉ hủy OCR. Nút dừng chung cũng dừng OCR độc lập.
* [x] OCR scan controls chỉ bind với `displayedStepId === "transcribe"`; không render nhầm ở `review-transcript` hoặc task khác.
* [x] CTest source suite đã chạy lại trên Qt 6.9.3/MSVC: **41/41 PASS**, gồm fusion mặc định STT, audio mix, workflow graph và QML route smoke.
* [x] Khi cả STT và OCR cùng có output, nút `Reconcile & Continue` gọi fusion trước khi chuyển bước; policy mặc định `prefer-stt`, OCR vẫn được lưu làm provenance/evidence. Nếu chỉ có một nguồn, Continue vẫn hoạt động bình thường.
* [x] Thứ tự production là `1 Import → 2 Normalize → 3 Separate (optional) → 4 Transcribe → 5 Translate → 6 Synthesize → 7 Align → 8 Mix & Export`; automatic workflow bỏ qua Separate tùy chọn nhưng vẫn giữ manual node để người dùng chạy khi cần.
* [x] Align lưu mức âm thanh độc lập `originalGainPercent`/`dubbedGainPercent`, mặc định `0%/100%`, có kiểm thử tránh tiếng gốc quay lại trong release của sidechain.
* [x] Mặc định project mới là `zh → vi`; project cũ thiếu cấu hình cũng được bổ sung `fusionPolicy=prefer-stt`, `transcriptSource=stt` và mix `0/100` khi load.
* [x] Prebuild gate sau cập nhật đã PASS **10/10 nhóm**; CTest **41/41**, QML lint, generated notebooks **32/32**, embedded worker payloads **2/2**, và packaged EXE smoke 0.0.8.7 PASS với 19 interaction events.
* [x] Portable artifact đã xác minh: `out\LA-Studio-0.0.8.7\LA-Studio-0.0.8.7.exe`, FileVersion/ProductVersion `0.0.8.7`, layout phẳng không có `bin/`, SHA-256 được ghi trong release report.
* [x] **Upload artifact UI đã được recheck bằng production component và ảnh cửa sổ thật:** dialog phải hiện nút chọn file (`Choose output` hoặc nút tương đương), trạng thái file đã chọn, nút xác nhận upload và `Skip task & continue`; không được sign-off chỉ dựa trên phần mô tả contract.
* [x] **Self-contained Colab worker gate:** Notebook Spleeter nhúng nguyên văn worker và launcher từ `notebooks/workers/`, kiểm SHA-256 sau chuẩn hóa line ending, đối chiếu notebook với generator và cấm raw URL tới repository cá nhân. Model vẫn tải từ release chính thức `k2-fsa/sherpa-onnx`; pre-commit, pre-push, prebuild, CI và release đều kiểm tra payload nhúng cục bộ. Không được đưa app worker vào notebook bằng URL branch/commit runtime.

---

## I. CÁC BƯỚC KIỂM THỬ THỰC TẾ BẮT BUỘC (BEYOND 41 CTESTS)

### 1. Kiểm tra đăng ký QML Module & Type Binding trong Build System
* [x] **Khai báo CMakeLists.txt**: Mọi file component mới (`.qml`, `.js`) trong thư mục `qml/` **bắt buộc** phải được thêm vào mục `QML_FILES` của hàm `qt_add_qml_module(LAStudio ...)` trong `CMakeLists.txt`.
* [x] **Đường dẫn Import đầy đủ**: Mọi file QML sử dụng component từ thư mục con khác (ví dụ: `qml/components/shared/`, `qml/components/shared/settings/`) phải có câu lệnh import chính xác (ví dụ: `import "../shared"`).
* [x] **Quy tắc kiểu dữ liệu nghiêm ngặt trong QML**:
  - `font.pixelSize` **BẮT BUỘC** phải là số nguyên (`int`), ví dụ `10`, `12`, `Theme.fontSmall`. **TUYỆT ĐỐI KHÔNG** gán số thực (`9.5`, `11.2`) vì Qt 6 parser sẽ coi là Fatal Type Error và crash ứng dụng ngay lúc khởi động.
  - Các thuộc tính `implicitWidth`, `implicitHeight`, `radius`, `border.width` cũng phải luôn là số nguyên.
* [x] **Quản lý vòng đời Dialog & Modal Popups**:
  - Mọi `Dialog` hoặc `Popup` có cờ `modal: true` hoặc `closePolicy: Popup.NoAutoClose` **bắt buộc phải gọi `root.close()`** trong `onClicked` của tất cả các nút hành động (Chấp nhận, Từ chối, Bỏ qua, Thoát).
  - Không được để Dialog con mở đè lên Dialog cha đang ở chế độ `modal` mà không đóng Dialog cha trước.
* [x] **Phòng thủ dữ liệu danh mục (Defensive Catalog & Array Access)**:
   - Mọi hàm JavaScript trong QML duyệt mảng từ C++ (như `languageCatalog`, `voicePresets`, `models`) **bắt buộc** phải có guard: `if (!catalog || !Array.isArray(catalog) || catalog.length === 0) return ...;` để tránh ngoại lệ `TypeError: Cannot read property 'length' of undefined` làm đứng luồng giao diện.
* [x] **Cô lập nội dung theo task (Task-scoped route visibility):**
  - Dialog/panel dùng chung nhưng được mở từ một task cụ thể phải lọc **tất cả** section điều khiển theo `stageIds`, không chỉ lọc danh sách card.
  - Khi mở từ `Separate`, tuyệt đối không được render `Next transcript action`, route `TTS` hoặc route `OCR`. Khi mở từ `Transcribe`, `OCR`, `Synthesize`, `Translate`, `Align`, chỉ được hiện đúng các route liên quan.
  - Phải có smoke assertion cho từng stage-scoped dialog, bao gồm cả visibility và phạm vi nút `Check/Connect`, để ngăn lỗi UI đúng hình nhưng thao tác nhầm stage.
* [x] **Cross-task regression sweep sau mọi bug fix (MANDATORY):** Với mỗi lỗi
  mới hoặc mỗi bản sửa, recheck cùng loại hành vi trên đủ 8 task, kể cả các
  task không trực tiếp liên quan. Tối thiểu phải có bằng chứng cho entry,
  setup/model gate, error guidance/log, state/concurrency, handoff và UI
  layout. Không sign-off nếu chỉ có bằng chứng ở task bị báo lỗi.
* [x] **Upload artifact độc lập với Colab:** Nút Upload phải mở được khi chưa
  chạy hoặc chưa kết nối Colab. Dialog phải hiện đúng tên file bắt buộc và
  phần mở rộng cho task hiện tại; riêng Separate phải có Vocals + Background,
  còn STT + OCR phải có output STT + output OCR độc lập. Mỗi panel phải có
  FileDialog thật và nút `Use uploaded output and continue`; với contract có
  nhiều output, chỉ chuyển bước sau khi đủ mọi file đã được xác nhận.
* [x] **Skip task độc lập với Run:** Dialog Upload và tab Data & Artifacts phải
  có nút `Skip task & continue`. Nút này chỉ ghi nhận `skipped` và chuyển sang
  bước kế tiếp, không gọi worker, không yêu cầu model/Colab, không xóa output
  hợp lệ cũ. `Run` vẫn dành riêng cho xử lý thật và không được vô hiệu hóa
  Upload/Skip chỉ vì thiếu Colab.
* [x] **Upload picker phải có bằng chứng trực quan:** Sau mỗi thay đổi đối với
  artifact handoff, phải chạy production QML preview, chụp ảnh cửa sổ thật và
  kiểm tra bằng mắt thấy được nút mở file. Nếu ảnh chỉ có tên file/định dạng và
  `Skip` nhưng không có `Choose output`, phải tiếp tục sửa và chưa được build.

### 2. Kiểm thử thực thi file nhị phân `.exe` thực tế (Live Binary Smoke Test)
* [x] **Không chỉ nhìn log build/package [SUCCESS]**: Phải chạy trực tiếp file `.exe` đã đóng gói trong thư mục staging (ví dụ: `out\LA-Studio-0.0.8.3\LA-Studio-0.0.8.3.exe`) qua dòng lệnh:
  ```powershell
  cmd /c "cd /d out\LA-Studio-0.0.8.3 && LA-Studio-0.0.8.3.exe"
  ```
* [x] **Kiểm tra Console Logs**:
  - Không có thông báo `QQmlApplicationEngine failed to load component`.
  - Không có lỗi `... is not a type`.
  - Không có lỗi `Type mismatch` hoặc `Cannot assign double to int`.
  - Không có cảnh báo thiếu DLL (`STATUS_DLL_NOT_FOUND`).
  - Xuất hiện log xác nhận: `[lastudio.app] QML module loaded.` và `[lastudio.app] Application services initialized.`

### 3. Kiểm thử Giao diện & Tương tác thực tế trên tất cả các Tab
* [x] **TTS Studio**:
  - Option Switcher chuyển mượt mà giữa: `[ Colab GPU ]`, `[ API Gateway ]`, và `[ Giọng đã clone ]`.
  - Nút **"Bảng Giọng Nói"** mở modal `VoiceGalleryDialog` dạng lưới lớn (1060x760px), hiển thị đủ 61 giọng hệ thống + các giọng clone cá nhân, hỗ trợ tìm kiếm, lọc danh mục và chọn giọng trực tiếp.
  - Nút nghe thử bên cạnh mỗi giọng toggle mượt mà giữa **Play ➔ Pause ➔ Resume** kèm hiệu ứng Equalizer sóng âm hoạt họa 3 vạch.
* [x] **Voice Cloning Studio**:
  - Option Switcher phân tách rõ ràng: `[ Giọng có sẵn ]` (load preset đã lưu) và `[ Tải lên / Thu âm ]` (file input / mic record).
  - Có nút **"Bảng Giọng Nói"** để mở thư viện 61 preset có sẵn.
  - Các giọng clone cá nhân có biểu tượng sao vàng `⭐` và có nút Thùng rác đỏ `🗑️` cho phép xóa kèm popup xác nhận. Các giọng hệ thống không thể bị xóa.
* [x] **Dubbing Studio**:
  - Hộp thoại chọn chế độ *"Choose how to use Dubbing"* (`DubbingEntryGateDialog`) đóng tức thì khi click **"Review one by one"** hoặc **"Automatic"** hoặc **"Leave Dubbing"**.
  - Chọn **"Review one by one"** chuyển thẳng vào giao diện làm việc Bước 1 (`1. Nguồn Media (Import)`), cho phép kéo thả media, dán link yt-dlp ngay mà không bị popup nào chặn lại.
  - Tại node 8 (`Synthesize - Lồng tiếng TTS`), có nút **"Bảng Giọng Nói"** mở thư viện giọng để nghe thử và chọn giọng cho từng nhân vật/đoạn thoại.
* [x] **STT Studio**:
  - Option Switcher chuyển đổi giữa `[ Colab GPU ]` và `[ API Gateway ]`.
* [x] **Translation Studio**:
  - Option Switcher chuyển đổi giữa `[ 9Router Gateway ]` và `[ Colab GPU ]`.
* [x] **LLM Chat Studio**:
  - Option Switcher chuyển đổi giữa `[ 9Router Gateway ]` và `[ Colab GPU ]`.
* [x] **Voice Isolator**:
  - Các card thông số, panel tách giọng, waveform player hiển thị chuẩn xác, không vỡ layout, không tràn viền.

### 4. Kiểm tra Đóng gói & Tài nguyên Phụ trợ (Packaging & Assets Audit)
* [x] **Cấu trúc phẳng bắt buộc (Flat Release Root)**: File `.exe` (`LA-Studio-x.x.x.x.exe`), các file `.dll`, `vc_redist.x64.exe`, `yt-dlp.exe` **phải nằm trực tiếp tại thư mục gốc** `out\LA-Studio-x.x.x.x\`. **TUYỆT ĐỐI KHÔNG** lồng trong thư mục con `bin/`.
  - Lệnh đóng gói chuẩn:
    ```powershell
    cmd /c "powershell -ExecutionPolicy Bypass -File scripts\package.ps1 -Preset windows-msvc-release -QtRoot .tools\Qt\6.9.3 -SkipInstaller -PortableInternalLayout -AllowUnsignedEspeakForInternalBuild"
    ```
* [x] **File dịch thuật (`.qm`)**: Đã chạy `lupdate` và `lrelease` cập nhật `lastudio_vi.qm` vào thư mục phát hành.
* [x] **Thư mục Runtime đầy đủ**: Thư mục portable `out\LA-Studio-x.x.x.x\` phải có đầy đủ:
  - `data/` (Chứa `presets/voice_clone_presets.json`, `voice_clone_refs/` chứa 61 file audio mẫu, `language-sets/`, `catalog.json`)
  - `subtitle-ocr/` (Tesseract runtime)
  - `media-tools/` (FFmpeg binary)
  - `espeak-ng/` (eSpeak NG speech data & binary)
  - `docs/colab-notebooks/` (Tất cả Jupyter notebooks cho GPU Colab)
  - `vc_redist.x64.exe` và các thư viện Qt/vcpkg DLLs.

### 5. Kiểm thử Colab Training Notebooks (GPU Pipeline Verification)
* [x] **Tự động Mount Google Drive**: Mọi notebook Colab cần đọc dữ liệu người dùng upload phải có `drive.mount('/content/drive')` ở Step 0.
* [x] **Quét thư mục đa tầng (Multi-path recursive discovery)**: Hỗ trợ tự động cả folder uncompressed trên Drive, file `.zip`, và thư mục cục bộ `/content/`.
* [x] **Không dùng Git Repo Private**: Tất cả câu lệnh `git clone` trên Colab phải trỏ vào repo Public chính thức, tránh bị chặn yêu cầu Username.
* [x] **Đồng bộ hóa 100% PyTorch Stack (`torch` + `torchaudio` + `torchvision`)**:
  - Khi cài đặt PyTorch trên Colab, **bắt buộc phải cài đồng bộ cả 3 gói** từ cùng một wheel repo (ví dụ: `!pip install -q --upgrade torch torchaudio torchvision --index-url https://download.pytorch.org/whl/cu128`).
  - Tuyệt đối không cài riêng lẻ `torch` mà bỏ qua `torchvision`, tránh gây lỗi lệch ABI C++ `operator torchvision::nms does not exist` khi `transformers` ngầm duyệt qua các backend vision/audio.
* [x] **Smoke Test Imports ngay sau cài đặt**: Luôn có dòng kiểm tra import các class quan trọng (ví dụ: `from transformers import HiggsAudioV2TokenizerModel`, `import omnivoice`) ngay cuối Step cài đặt để bắt lỗi môi trường lập tức.
* [x] **Đối chiếu tham số CLI với Source Code**: Các lệnh trích xuất/huấn luyện (như `extract_audio_tokens.py`, `train.py`) phải được kiểm tra trực tiếp từng cờ tham số (`--tar_output_pattern`, `--jsonl_output_pattern`, cấu trúc `data.lst`) với mã nguồn gốc.
* [x] **Cơ chế Fallback thông minh (Zero-Upload Safety)**: Nếu không có file tải lên, notebook phải tự động sinh dataset bằng AI (VieNeu-TTS) để không bao giờ bị lỗi `0 audio files`.
* [x] **Embedded worker integrity:** Chạy `python scripts/test_colab_worker_pins.py` trước commit và chạy thêm `python scripts/verify_colab_worker_pins.py` trước push/build. Validator phải đối chiếu source worker/launcher local với mapping `EMBEDDED_WORKERS`, kiểm SHA-256 payload và marker generator/notebook; không fetch worker từ GitHub. Khi worker thay đổi: sửa template local, regenerate notebook, rồi chạy lại test/gate; không sửa tay notebook và không thêm URL runtime cho app worker.

---

## II. NHẬT KÝ SỰ CỐ THỰC TẾ & BÀI HỌC PHÒNG NGỪA (LIVING INCIDENT LOG)

*(Mỗi khi phát hiện lỗi mới trong quá trình phát triển, BẮT BUỘC phải bổ sung vào bảng bên dưới để ngăn ngừa tái diễn)*

| Mã lỗi | Ngày ghi nhận | Hiện tượng lỗi | Nguyên nhân gốc rễ (Root Cause) | Quy trình kiểm thử phòng ngừa |
| :--- | :--- | :--- | :--- | :--- |
| **INC-001** | 2026-08-26 | Mở file `.exe` không lên cửa sổ, ứng dụng thoát ngay lúc khởi động. | Tạo component mới `StudioOptionSwitcher.qml` nhưng quên khai báo trong `CMakeLists.txt` (`QML_FILES`) và thiếu import, khiến `QQmlApplicationEngine` ném lỗi `StudioOptionSwitcher is not a type`. CTest không bắt được vì CTest chỉ chạy unit tests C++ mà không khởi tạo `QApplication` render toàn bộ cây QML. | **Bắt buộc chạy Live Binary Smoke Test** (mục I.2): Khởi chạy trực tiếp file `.exe` từ command line và đọc console log trước khi bàn giao. |
| **INC-002** | 2026-08-26 | File `.exe` và các DLL bị đóng gói nhầm vào thư mục con `bin/` thay vì nằm trực tiếp ở root của `out\LA-Studio-x.x.x.x\`. | Chạy `package.ps1` với cờ `-StageDir` mà thiếu `-PortableInternalLayout`, khiến script mặc định cấu trúc installer (lồng `bin/`). | **Bắt buộc dùng cờ `-PortableInternalLayout -SkipInstaller`** khi build bản portable để giữ cấu trúc phẳng thống nhất từ các bản 0.0.7.9, 0.0.8.0, 0.0.8.1. |
| **INC-003** | 2026-08-26 | Colab Notebook Step 2 báo `FileNotFoundError: Chưa tìm thấy file voice_cloning_lab.zip!`. | Code Colab hardcode đường dẫn tìm kiếm file zip cố định, không nhận diện được folder uncompressed hoặc vị trí tải lên từ Google Drive. | **Bắt buộc áp dụng cơ chế quét đệ quy đa tầng** (mục I.5): Quét đồng thời `/content/drive/MyDrive/...`, `/content/...`, tự động giải nén zip nếu có hoặc đọc trực tiếp thư mục. |
| **INC-004** | 2026-08-26 | Colab ném lỗi `fatal: could not read Username for 'https://github.com'` và `ModuleNotFoundError: No module named 'omnivoice'`. | Lệnh `git clone` gọi vào repository cá nhân chưa cấu hình public hoặc sai tên, kích hoạt cơ chế hỏi password của Git trong môi trường non-interactive. | **Kiểm tra URL Git công khai 100%**: Luôn dùng repo upstream chính thức (`k2-fsa/OmniVoice.git`) và chạy `pip install -e .` cài đặt trực tiếp. |
| **INC-005** | 2026-08-26 | Colab Step 2 báo `Tìm thấy tổng cộng 0 file âm thanh trong thư mục`. | Máy ảo Colab mới không tự mount Google Drive nên không đọc được folder đã tải lên Drive, dẫn đến việc tạo thư mục rỗng. | **Bắt buộc có Step 0 Mount Drive + Zero-Upload Fallback**: Tự động mount Drive và tích hợp engine AI (VieNeu-TTS) tự sinh dữ liệu giọng 3 miền nếu không có file tải lên. |
| **INC-006** | 2026-08-26 | Giao diện Dropdown ComboBox bị quá tải, khó tìm kiếm và không có nghe thử khi thư viện vượt quá 60 giọng nói. | Dropdown thông thường chỉ phù hợp với 5-10 mục, khi thêm 22 giọng CapCut + 20 giọng VieNeu + 17 giọng OmniVoice khiến người dùng khó thao tác. | **Tích hợp Voice Gallery Dialog** (mục I.3): Thêm Modal bảng lớn (`VoiceGalleryDialog.qml`) có phân loại tag, tìm kiếm tức thì, lọc giới tính và chọn giọng trực quan. |
| **INC-007** | 2026-08-26 | Nút Play trong danh sách giọng nói không thể bấm dừng lại (Pause/Resume). | Nút Play trong delegate QML chỉ gọi phát một chiều, không liên kết với thuộc tính `AppController.player.playing` và `paused`. | **Ràng buộc hai chiều cho Audio Player Controls**: Nút phát phải chuyển trạng thái linh hoạt **Play ➔ Pause ➔ Resume** và đổi màu nổi bật khi đang phát. |
| **INC-008** | 2026-08-26 | `RuntimeError: operator torchvision::nms does not exist` dẫn đến `ModuleNotFoundError: Could not import module 'HiggsAudioV2TokenizerModel'` trên Colab. | Colab nâng cấp môi trường lên Python 3.13 với `torchvision 0.26.0`. Lệnh `pip install` chỉ định `torch==2.8.0 torchaudio==2.8.0` mà không hạ/đồng bộ `torchvision`, gây lệch binary ABI C++. Khi `transformers` nạp `HiggsAudioV2TokenizerModel`, nó ngầm quét và import `torchvision` khiến crash. | **Đồng bộ hóa 100% PyTorch Stack** (mục I.5): Cài đồng bộ `torch + torchaudio + torchvision` từ cùng nguồn build CUDA (`--index-url .../cu128`) và thêm bước kiểm tra import trực tiếp ngay Step 1. |
| **INC-009** | 2026-08-26 | Step 6 báo `'numpy.ndarray' object has no attribute 'cpu'` và `Unsupported instruct items found`. | 1) `model.generate()` của OmniVoice không nhận chuỗi `instruct` tùy ý (chỉ nhận keyword cố định). Khi clone giọng, bắt buộc phải dùng `ref_audio`. 2) Đầu ra `audios[0]` của OmniVoice trả về dạng `numpy.ndarray` (đã nằm trên RAM/CPU), việc gọi `.cpu()` trực tiếp gây lỗi. | **Chuẩn hóa API Voice Cloning & Safe Array Conversion**: Dùng `ref_audio` cho Voice Cloning và áp dụng hàm chuyển đổi an toàn `hasattr(audio, 'cpu')` kết hợp `soundfile.write(..., audio_arr, samplerate)` để hỗ trợ cả `Tensor` lẫn `ndarray`. |
| **INC-010** | 2026-08-26 | Giọng âm thanh sau khi sinh bị giật cục, lặp từ, vấp âm ("cà hụp cà hụp"). | Nhãn văn bản (`text`) trong dataset bị gán giả định danh (`Mau giong doc {label}...`) thay vì lời thoại thật được nói trong file audio, làm phá hủy ma trận căn chỉnh âm vị (Cross-Attention Alignment) khi fine-tune và khi clone. | **Bắt buộc dùng Whisper ASR bóc tách 100% Transcript thật (No Synthetic Text)**: Tích hợp `Whisper ASR` tự động bóc tách transcript thật ngay từ Step 2 và luôn bật `load_asr=True` trong OmniVoice để đồng bộ âm vị hoàn hảo. |
| **INC-011** | 2026-08-26 | File zip tải về ở Step 7 phình to quá mức (hơn 10.6 GB), gây nghẽn và đứt mạng khi tải qua trình duyệt Colab. | Gom toàn bộ thư mục `exp/` chứa nhiều checkpoint trung gian và các file trạng thái Optimizer (`optimizer.pt`, `scheduler.pt`, `scaler.pt`) chiếm 80% dung lượng. | **Tách gói tải ưu tiên & Lọc Model tinh gọn**: (1) Tải ngay file Zip 57 câu chào mẫu siêu nhẹ (~5-10 MB) để nghe ngay. (2) Lọc bỏ toàn bộ file optimizer thừa, giảm dung lượng checkpoint xuống ~1.2 GB và tự động lưu trực tiếp vào Google Drive (`MyDrive/...`). |
| **INC-012** | 2026-08-26 | Mở file `.exe` không lên cửa sổ, ứng dụng crash và thoát ngay trước khi hiển thị. | Thuộc tính `font.pixelSize: 9.5` (số thực `float`) trong `VoiceGalleryDialog.qml`. Trong Qt 6 QML, `font.pixelSize` bắt buộc phải là số nguyên (`int`), khiến bộ phân tích QML gặp lỗi `Type Mismatch` nghiêm trọng (Fatal Error) và dừng toàn bộ ứng dụng ngay trong `main.cpp`. | **Quy tắc kiểu dữ liệu nghiêm ngặt trong QML** (mục I.1): Ép kiểu toàn bộ `font.pixelSize` về số nguyên (`int`), tuyệt đối không dùng số thực. Bắt buộc chạy Live Binary Smoke Test trước khi bàn giao. |
| **INC-013** | 2026-08-26 | Bấm "Review one by one" (hoặc "Automatic", "Leave Dubbing") trên modal Dubbing Entry Gate không có phản hồi, màn hình đứng im. | 1) `DubbingEntryGateDialog.qml` có `modal: true` và `NoAutoClose` nhưng `onClicked` của các nút bấm chỉ emit signal mà **quên gọi `root.close()`**, khiến modal tiếp tục che khuất và chặn bắt mọi tương tác chuột. 2) Cờ C++ `m_dubbingEntryGateActive` không được giải phóng. 3) `DubbingProjectSetupDialog.qml` gặp lỗi JavaScript `TypeError: Cannot read property 'length' of undefined` khi duyệt `languageCatalog` chưa nạp xong. | **Quản lý vòng đời Dialog & Phòng thủ truy cập mảng** (mục I.1): 1) Mọi Dialog `modal` bắt buộc phải gọi `root.close()` khi bấm nút. 2) Đồng bộ cờ trạng thái sang C++ (`chooseDubbingEntryMode`). 3) Dùng `Array.isArray()` kiểm tra mảng trước khi duyệt. |
| **INC-014** | 2026-08-26 | Giao diện Dubbing bị đè chữ trên Video Preview, mất nút "Run task" ở bước Normalize/Isolator/Synthesize, tràn viền bên phải ở STT/OCR, và thiếu toàn bộ tính năng chọn giọng/voice clone ở bước TTS. | 1) Các nút nổi `openHistoryButton` và `openTaskControlsButton` neo đè trực tiếp lên thanh toolbar video. 2) C++ `workflowNodes()` thiếu cờ boolean `canRun`, `completed`, `runReady` khiến QML ẩn nút Run. 3) Các ComboBox trong `DubbingTranscribeStep.qml` đặt độ rộng cứng `preferredWidth: 210` vượt quá chiều rộng cột review (320px). 4) `DubbingSynthesizeStep.qml` thiếu bộ chọn giọng, không có `VoiceGalleryDialog`, không có nút nghe thử và không có nút bấm chạy lồng tiếng TTS. | **Quy chuẩn Giao diện Dubbing & Đồng bộ tính năng TTS (mục I.3)**: 1) Tuyệt đối không dùng nút nổi đè lên video canvas. 2) Đảm bảo mọi bước làm việc (Normalize, Separate, Transcribe, Translate, Synthesize) đều có nút **Run Step** to rõ, hoạt động trực tiếp. 3) Layout phải co giãn responsive, không dùng fixed width lớn hơn minWidth của panel. 4) Đồng bộ đầy đủ Option Switcher, Bảng Giọng Nói 61 giọng + clone, Avatar icon, Player nghe thử và nút Bắt Đầu Lồng Tiếng vào Dubbing Studio. |
| **INC-015** | 2026-08-28 | Khi đang ở task `Separate`, dialog `Dubbing Direct Colab setup` vẫn hiển thị `Next transcript action` cùng các route `TTS` và `OCR`; người dùng có thể tưởng rằng đang cấu hình nhầm stage hoặc thao tác sang route khác. | `stageIds` chỉ được dùng để lọc `Repeater` các stage card. Các section transcript, unified worker rows và nút kiểm tra ở ngoài `Repeater` render vô điều kiện. Ngoài lỗi hiển thị, bộ đếm selected và thao tác check/connect có nguy cơ vượt khỏi stage đang mở. | **Task-scoped route visibility gate**: mọi section dùng chung phải bind vào `includesStage/includesAnyStage`; đếm selected chỉ trong scope; nút check scoped gọi stage hiện tại; Unified Connect bị khóa khi có selected stage ngoài scope. Bắt buộc chạy `QmlRouteSmoke` với từng scope `source-separate`, `transcribe`, `subtitle-ocr`, `translate`, `synthesize`, `alignment` và preview trực quan task `Separate` trước khi bàn giao. |
| **INC-016** | 2026-08-28 | Lần chạy packaged QML smoke đầu tiên sau khi sửa `Separate` báo lỗi scope ở các stage khác dù UI binding đã đúng. | Helper smoke đọc `child.visible` của các section bên trong `Dialog` khi popup chưa mở. Với Dialog chưa visible, giá trị runtime của child không phải bằng chứng cho binding scope; đây là lỗi của test oracle, không phải lỗi hiển thị production. | **Smoke phải kiểm tra cùng predicate public của component** (`includesStage/includesAnyStage`) và vẫn chạy matrix từng scope. Không dùng trạng thái lifecycle của popup đóng để suy luận visibility; sau đó chạy packaged EXE thật và kiểm tra log `QML module loaded`/`Application services initialized`. |
| **INC-017** | 2026-08-28 | Packaged QML smoke vẫn ghi warning `QFontDatabase: Cannot find font directory .../lib/fonts`. UI vẫn load và smoke exit 0, nhưng log chưa sạch tuyệt đối. | Qt runtime không còn tự ship font và package chưa có thư mục font được cấp phép; đây là packaging/environment warning, không phải QML type error. | **Package gate phải kiểm tra warning theo phân loại**: hoặc stage bộ font được cấp phép kèm license và verify `lib/fonts`, hoặc cấu hình fallback hệ thống có chủ đích. Không coi warning là “không có lỗi” khi chưa có disposition trong report. |
| **INC-018** | 2026-08-28 | Đã chọn model Direct Colab, check worker thành công nhưng bấm Run lại mở model picker. | `DubbingPage.qml::nodeNeedsModelSelection()` kiểm tra `familyId` trước execution provider. Direct Colab không giữ metadata model local nên cấu hình remote verified bị nhận nhầm là local chưa cấu hình. | Regression phải kiểm tra remote provider trước `familyId` và xác nhận `modelId + verified` đủ thì `runStep()` gọi thẳng `runWorkflowNode()`, không gọi `nodeModelDialog.openFor()`. |
| **INC-019** | 2026-08-28 | Chọn `Khớp STT + OCR` nhưng dialog chỉ có một Unified URL và khối xanh chiếm nhiều diện tích; thiếu URL OCR riêng. | Dialog chưa có route row cho stage `transcribe`; panel Unified render ngoài transcript context và không phân biệt reconcile với coordinator route. | Regression/UI smoke phải yêu cầu hai field STT/OCR độc lập, hai token và hai `connectWorkflowColabStage()` riêng; Unified panel phải ẩn trong transcript reconcile/scoped mode và không được tạo panel rỗng ở scope đơn. |
| **INC-020** | 2026-08-29 | Bấm Upload khi chưa chạy/không kết nối Colab chỉ mở hộp thoại trống, không có nút chọn file và không biết định dạng cần nộp. | Nút Upload truyền presentation id chưa được chuẩn hóa đầy đủ (`separate`, `stt`, `ocr`, `alignment`, `export-output`), trong khi dialog chỉ render panel khi controller trả về artifact contract. UI cũng mô tả đây là Colab output nên gây hiểu nhầm rằng phải chạy Colab trước. | Upload là handoff local độc lập với Colab. Controller phải ánh xạ mọi presentation id về contract node, dialog phải hiện `Required file name` + `Allowed format`, Separate phải hiện đủ hai stem và STT + OCR phải hiện hai output độc lập. Regression bắt buộc gọi `workflowArtifactSpecsForStage()` và `importWorkflowArtifactFiles()` khi không có session/URL/token Colab. |
| **INC-021** | 2026-08-29 | Muốn chạy riêng STT/OCR nhưng UI còn một luồng Transcribe chung; chạy STT có thể kế thừa mode OCR/reconcile hoặc tự chuyển màn hình trước khi người dùng chạy nguồn còn lại. | QML dùng một source selector và controller `transcribeSource()` đọc mode đã lưu; completion chung gọi `advanceManualStep()` cho Transcribe. Trạng thái workflow chỉ coi một loại transcript là hoàn tất. | Hai card production phải phát signal với node alias riêng (`stt`, `ocr`), controller phải ép `transcriptSource=stt` cho STT, OCR dùng worker riêng, completion không auto-advance khỏi Transcribe, và `workflowNodes/workflowStages/Continue` phải chấp nhận STT-only hoặc OCR-only. |
| **INC-022** | 2026-08-29 | Trong lúc STT/OCR độc lập đang chạy, upload hoặc nút dừng chung có nguy cơ bị busy gate hoặc hủy nhầm worker đối diện; OCR scan còn có thể xuất hiện ở màn hình review transcript. | Artifact handoff và cancel trước đây chỉ xét `m_runner` STT; OCR độc lập nằm ở `SubtitleOcrController` nên không được nhận diện theo node. Preview dùng predicate rộng hơn Transcribe. | Handoff/cancel phải route-scope: `transcribe` chỉ tác động `m_runner`, `subtitle-ocr` chỉ tác động `m_subtitleOcr`; `importSubtitles` có cờ nội bộ chỉ cho transcript worker độc lập; `showOcrTools` chỉ đúng Transcribe. Bắt buộc kiểm thử STT-only, OCR-only, chạy đồng thời, upload từng nguồn và Cancel. |
| **INC-023** | 2026-08-29 | Khi STT và OCR đều hoàn tất, Continue có thể chuyển bước mà chưa tạo một script chuẩn; khi hai nguồn lệch nhau có nguy cơ để OCR-only cue chen vào script mặc định. | UI chỉ hiển thị hai nguồn nhưng chưa bắt buộc reconcile tại handoff; fusion policy cũ có thể được hiểu là ưu tiên OCR/append mọi cue. | `DubbingTranscribeStep.continueToNextStep()` phải gọi `reconcileTranscriptSources()` khi cả hai nguồn tồn tại. Fusion mặc định `prefer-stt`: giữ timeline/text STT, lưu OCR ở provenance/evidence, không append OCR-only cue; chỉ `prefer-ocr`/`ask` khi người dùng chọn rõ. |
| **INC-024** | 2026-08-29 | Thanh task và workflow stage vẫn có thể lệch thứ tự mới: Translate/Align/TTS hiển thị khác thứ tự yêu cầu; Separate bị coi là prerequisite bắt buộc. | Label QML, `workflowStages()` và graph automatic dùng các mapping cũ độc lập nhau. | Duy trì một thứ tự canonical ở workflow definition/controller/QML: 1 Import, 2 Normalize, 3 Separate optional, 4 Transcribe, 5 Translate, 6 Synthesize, 7 Align, 8 Mix & Export. Automatic graph loại node Separate và incident edges; manual Separate vẫn khả dụng. |
| **INC-025** | 2026-08-29 | Slider original audio = 0% nhưng sau đoạn ducking tiếng gốc có thể tăng trở lại do release target dùng gain mặc định. | Sidechain compressor dùng target `1.0` trong nhánh release, không nhân với mức original đã chọn. | Truyền `originalGainPercent` vào cả attack/release calculation; regression phải kiểm tra `0/100` và `100/0`, đồng thời verify output không chứa base audio sau release khi original = 0. |
| **INC-026** | 2026-08-29 | Project mới hoặc catalog ngôn ngữ chưa nạp có thể rơi về English → English thay vì Chinese → Vietnamese. | UI đọc index 0 khi `languageCatalog` rỗng/khác kiểu QVariantList; default C++ chưa được bảo vệ khi migrate project cũ. | C++ tạo project luôn ghi `sourceLanguage=zh`, `targetLanguage=vi`; loader chèn giá trị thiếu; QML dùng catalog fallback có thứ tự zh/vi/en và guard kiểu/danh sách trước khi truy cập. |
| **INC-027** | 2026-08-29 | Từ trang Transcribe, bấm `Run STT` hoặc `Run OCR` khi model/runtime/Colab chưa sẵn sàng có thể rơi vào generic Error Guidance thay vì mở đúng màn hình setup; OCR còn thiếu preflight cùng nguồn sự thật với backend. | QML tự kiểm tra cấu hình không đầy đủ và nhánh OCR gọi worker trước khi hỏi setup. OCR không phải persisted workflow node nên không thể mở bằng model picker chung; nếu backend gọi `setError` trước thì `Main.qml` mở modal lỗi chung, làm người dùng không tới được model/Colab dialog. | **Setup gate phải là recoverable UI action:** controller cung cấp `workflowNodeSetupIssueForUi()` dùng cùng logic với run path; STT mở model picker, OCR mở exact Colab/local OCR setup; backend phát `workflowSetupRequired` và không gọi `setError` cho thiếu cấu hình. Sau mọi sửa lỗi ở một task phải chạy cross-task regression sweep đủ 8 task theo quy tắc bắt buộc ở Phần 0, đặc biệt kiểm tra cả `Run`, setup, error guidance và retry. |
| **INC-028** | 2026-08-29 | Upload artifact hiển thị được phần tóm tắt tên file nhưng popup không có nút mở browser/chọn file; sau đó upload đầu tiên trong contract STT + OCR còn có nguy cơ chuyển bước khi file còn lại chưa được nộp. Người dùng cũng cần bỏ qua task mà không chạy worker. | Panel upload truy vấn lại contract bằng presentation id nên có thể tự ẩn dù dialog đã có `specs`; UI chưa thể hiện rõ Upload là local handoff độc lập với Colab. Signal `artifactAccepted` trước đây được xử lý như hoàn tất toàn dialog, không đếm đủ các contract con. Run gate và handoff chưa có một hành động Skip độc lập. | **Artifact/skip contract bắt buộc:** dialog truyền nguyên contract map vào panel (`contractSpec`), panel luôn có `FileDialog`, hiển thị `expectedFiles`/`allowedExtensions`, Upload không phụ thuộc URL/token/model Colab. Dialog và tab Data & Artifacts phải có `Skip task & continue`; controller `skipWorkflowTask()` chỉ ghi state `skipped`, bảo toàn output metadata và gọi `advanceManualStep()` mà không khởi động worker. Với STT + OCR, chỉ emit hoàn tất sau khi đủ 2 artifact; mọi presentation alias phải được normalize trước khi import/skip. Recheck cross-task đủ 8 task, gồm Import/Normalize/Separate/Transcribe/Translate/Synthesize/Align/Mix & Export và cả contract một-file/hai-file. |
| **INC-029** | 2026-08-29 | Dialog Upload có summary contract nhưng thân dialog có thể trống, khiến người dùng không thấy nút chọn file dù Upload vẫn được mô tả là khả dụng. | `DubbingArtifactUploadPanel.visible` phụ thuộc vào giá trị `QVariant/JS map` tạm thời nên panel bị ẩn trong lúc delegate ổn định; các delegate Repeater dùng `modelData` nhưng không khai báo required property, dẫn tới `ReferenceError` trong Qt 6 và không dựng được nội dung. | Panel phải luôn hiển thị khi được tạo từ contract hợp lệ; delegate phải khai báo `required property var modelData` và truyền `contractSpec/nodeId` rõ ràng. Bắt buộc chạy focused `TestDubbingProject`, full CTest, QML lint, production QML preview và kiểm tra ảnh cửa sổ thật có `Choose output`, `Use uploaded output and continue`, `Skip task & continue` trước khi package. |
| **INC-030** | 2026-08-29 | Notebook Spleeter chạy tới cell tải worker rồi nhận `HTTP Error 404: Not Found`, dù build/package của ứng dụng vẫn PASS. | Generator đã đổi repository từ `kova-video-studio` sang `KOVA-DUB` nhưng giữ commit cũ không tồn tại trong repository mới; đồng thời generator ghi notebook ra thư mục gốc trong khi notebook đang được dùng nằm ở `notebooks/voice_separation/`. Bộ verifier cũ chỉ kiểm tra cấu trúc/chuỗi, không fetch raw URL, không so SHA remote với local và không phát hiện đường đích generator lệch canonical. | Generator hiện khai báo `WORKER_REPOSITORY`, pin commit bất biến `3f194b9155e7c2fcdd8eed4ac5fa980e6084417e`, SHA đúng của hai worker, ghi đúng `notebooks/voice_separation/` và regenerate notebook. `verify_colab_worker_pins.py` kiểm tra format, raw URL/HTTP status, retry hữu hạn, SHA remote, SHA local có chuẩn hóa EOL và marker notebook. Gate chạy trong pre-commit/pre-push (qua hook), `prebuild_gate.ps1`, CI và Windows release; evidence lưu JSON để truy vết sau commit/build. |
| **INC-031** | 2026-08-29 | Sau khi model repository đã được xác nhận public, notebook vẫn có thể lỗi `HTTP 404` khi tải worker/launcher app từ GitHub cá nhân; việc chạy phụ thuộc vào repository/commit không liên quan đến model upstream. | Generator đã ghép artifact model upstream với mã worker/launcher của ứng dụng. Khi repository hoặc commit app bị đổi/xóa, Colab thất bại trước khi mở worker dù model vẫn tồn tại. Pin remote chỉ làm lỗi dễ kiểm tra hơn nhưng vẫn giữ một phụ thuộc runtime không cần thiết. | Generator mới nhúng nguyên văn hai template local vào `EMBEDDED_WORKERS`, kiểm SHA-256 trong notebook trước khi ghi vào `/content`, và chỉ tải model từ release chính thức `k2-fsa/sherpa-onnx`. Validator `verify_colab_worker_pins.py` đã đổi thành embedded-bundle gate: cấm `KOVA-DUB`/`WORKER_REPOSITORY`/`WORKER_COMMIT`, đối chiếu source local với notebook và kiểm launcher. Sửa worker phải regenerate notebook; pre-commit, pre-push, prebuild, CI và release bắt buộc chạy test/validator này. |

---

## III. QUY TRÌNH KÝ DUYỆT BÀN GIAO (SIGN-OFF PROTOCOL)

Chỉ xác nhận hoàn thành công việc và thông báo cho người dùng khi:
1. `41/41 CTest tests` PASS (hoặc bộ kiểm thử smoke tests `QmlRouteSmoke` đạt 100%).
2. Đã đối chiếu và tích chọn toàn bộ các mục trong **Phần I (Checklist)** (Bao gồm QML binding, Live Binary Smoke, GUI interaction, Packaging, và Colab GPU Pipeline).
3. Đã chạy thử file `.exe` thực tế thành công, không có crash/fatal QML/DLL; mọi warning còn lại phải được phân loại và ghi rõ trong **Living Incident Log**, không được bỏ qua.
4. Đã ghi nhận đầy đủ các sự cố mới phát sinh vào **Phần II (Living Incident Log)**.
5. Đã cập nhật Knowledge Graph qua `graphify update .`.

### Evidence lần build 0.0.8.7 — 2026-08-29

- Prebuild gate: `PASS`, 10/10 nhóm; CTest `41/41`; QML lint `PASS`; exact
  bindings `31/31`; generated notebooks `32/32`; remote feature surface `8/8`.
- Packaged QML smoke: `PASS`, 19 interaction events; trace tại
  `out\package-smoke\0.0.8.7\qml-interaction-trace.json`.
- Portable EXE: `out\LA-Studio-0.0.8.7\LA-Studio-0.0.8.7.exe`; size
  `30,986,752` bytes; SHA-256
  `47C0A81780E596DB5EBFCCF959D034CF11645A6CBF54C2FD45E9E8857501C14F`.
- Cảnh báo môi trường vẫn phải đọc theo disposition cũ: thiếu Vulkan headers,
  Qt font directory mặc định và eSpeak MSI unsigned chỉ được phép cho internal
  build; đây không phải bằng chứng live Colab/GPU inference.
