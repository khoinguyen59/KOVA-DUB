# 📋 BẢNG KIỂM TRA CHẤT LƯỢNG TRƯỚC KHI BÀN GIAO (PRE-DELIVERY QA CHECKLIST)

> [!IMPORTANT]
> **QUY TẮC BẮT BUỘC ĐỐI VỚI AI AGENT / DEVELOPER**:
> Trước khi bàn giao bất kỳ tính năng, bản sửa lỗi, hoặc bản build mới nào cho người dùng, **BẮT BUỘC PHẢI ĐỌC VÀ THỰC HIỆN ĐẦY ĐỦ CÁC BƯỚC KIỂM THỬ TRONG FILE NÀY**.
> Vượt qua 39 bài test CTest chỉ là điều kiện nền tảng cơ bản (baseline), **KHÔNG ĐƯỢC PHÉP** bỏ qua các bước kiểm thử thực tế bên dưới.

---

## I. CÁC BƯỚC KIỂM THỬ THỰC TẾ BẮT BUỘC (BEYOND 39 CTESTS)

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
| **INC-014** | 2026-08-26 | Nút "Tiếp tục" trong Dubbing Review Step bị disabled không bấm được; Nút chạy Mix Audio bị thiếu; Voice selection trong Dubbing bị sai lệch tên. | 1) `DubbingTranscriptReviewStep.qml` và `DubbingTranslationReviewStep.qml` kiểm tra `enabled: root.stepComplete` (mặc định luôn `false`). 2) `DubbingMixStep.qml` chỉ hiển thị thông tin mà thiếu nút thực thi trực tiếp. 3) `VoiceGalleryDialog.qml` cắt bớt tiền tố tên dẫn tới so khớp sai `ttsVoiceOptions` trong C++. | **Kiểm tra trạng thái kích hoạt Button & Ràng buộc ID chuẩn xác**: 1) Ràng buộc `enabled` theo dữ liệu thực tế `root.dubbing.segments.length > 0`. 2) Bổ sung nút bấm hành động `runWorkflowNode("mix")`. 3) Truyền `voiceId` gốc trong signal `voiceSelected`. |

---

## III. QUY TRÌNH KÝ DUYỆT BÀN GIAO (SIGN-OFF PROTOCOL)

Chỉ xác nhận hoàn thành công việc và thông báo cho người dùng khi:
1. `39/39 CTest tests` PASS 100%.
2. Đã đối chiếu và tích chọn toàn bộ các mục trong **Phần I (Checklist)** (Bao gồm QML binding, Live Binary Smoke, GUI interaction, Packaging, và Colab GPU Pipeline).
3. Đã chạy thử file `.exe` thực tế thành công và không ghi nhận bất kỳ crash/warning nào trong log.
4. Đã ghi nhận đầy đủ các sự cố mới phát sinh vào **Phần II (Living Incident Log)**.
5. Đã cập nhật Knowledge Graph qua `graphify update .`.

