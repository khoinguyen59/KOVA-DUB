# 📋 BẢNG KIỂM TRA CHẤT LƯỢNG TRƯỚC KHI BÀN GIAO (PRE-DELIVERY QA CHECKLIST)

> [!IMPORTANT]
> **QUY TẮC BẮT BUỘC ĐỐI VỚI AI AGENT / DEVELOPER**:
> Trước khi bàn giao bất kỳ tính năng, bản sửa lỗi, hoặc bản build mới nào cho người dùng, **BẮT BUỘC PHẢI ĐỌC VÀ THỰC HIỆN ĐẦY ĐỦ CÁC BƯỚC KIỂM THỬ TRONG FILE NÀY**.
> Vượt qua 39 bài test CTest chỉ là điều kiện nền tảng cơ bản (baseline), **KHÔNG ĐƯỢC PHÉP** bỏ qua các bước kiểm thử thực tế bên dưới.

---

## I. CÁC BƯỚC KIỂM THỬ THỰC TẾ BẮT BUỘC (BEYOND 39 CTESTS)

### 1. Kiểm tra đăng ký QML Module & Type Binding trong Build System
* [ ] **Khai báo CMakeLists.txt**: Mọi file component mới (`.qml`, `.js`) trong thư mục `qml/` **bắt buộc** phải được thêm vào mục `QML_FILES` của hàm `qt_add_qml_module(LAStudio ...)` trong `CMakeLists.txt`.
* [ ] **Đường dẫn Import đầy đủ**: Mọi file QML sử dụng component từ thư mục con khác (ví dụ: `qml/components/shared/settings/`) phải có câu lệnh import chính xác (ví dụ: `import "../shared/settings"`).

### 2. Kiểm thử thực thi file nhị phân `.exe` thực tế (Live Binary Smoke Test)
* [ ] **Không chỉ nhìn log build/package [SUCCESS]**: Phải chạy trực tiếp file `.exe` đã đóng gói trong thư mục staging (ví dụ: `out\LA-Studio-0.0.8.1\LA-Studio-0.0.8.1.exe`) qua dòng lệnh:
  ```powershell
  cmd /c "cd /d out\LA-Studio-0.0.8.1 && LA-Studio-0.0.8.1.exe"
  ```
* [ ] **Kiểm tra Console Logs**:
  - Không có thông báo `QQmlApplicationEngine failed to load component`.
  - Không có lỗi `... is not a type`.
  - Không có cảnh báo thiếu DLL (`STATUS_DLL_NOT_FOUND`).
  - Xuất hiện log xác nhận: `[lastudio.app] QML module loaded.` và `[lastudio.app] Application services initialized.`

### 3. Kiểm thử Giao diện & Tương tác thực tế trên tất cả các Tab
* [ ] **TTS Studio**:
  - Option Switcher chuyển mượt mà giữa: `[ Colab GPU ]`, `[ API Gateway ]`, và `[ Giọng đã clone ]`.
  - Danh sách giọng đã clone hiển thị đúng danh sách từ `allPresets()`.
* [ ] **Voice Cloning Studio**:
  - Option Switcher phân tách rõ ràng: `[ Giọng có sẵn ]` (load preset đã lưu) và `[ Tải lên / Thu âm ]` (file input / mic record).
  - Tên gợi nhớ giọng (`reusableVoiceName`) hoạt động chuẩn xác.
* [ ] **STT Studio**:
  - Option Switcher chuyển đổi giữa `[ Colab GPU ]` và `[ API Gateway ]`.
* [ ] **Translation Studio**:
  - Option Switcher chuyển đổi giữa `[ 9Router Gateway ]` và `[ Colab GPU ]`.
* [ ] **LLM Chat Studio**:
  - Option Switcher chuyển đổi giữa `[ 9Router Gateway ]` và `[ Colab GPU ]`.
* [ ] **Voice Isolator & Dubbing**:
  - Các card thông số, panel tách giọng, waveform player hiển thị chuẩn xác, không vỡ layout, không tràn viền.

### 4. Kiểm tra Đóng gói & Tài nguyên Phụ trợ (Packaging & Assets Audit)
* [ ] **File dịch thuật (`.qm`)**: Đã chạy `lupdate` và `lrelease` cập nhật `lastudio_vi.qm` vào thư mục phát hành.
* [ ] **Thư mục Runtime đầy đủ**: Thư mục portable `out\LA-Studio-x.x.x.x\` phải có đầy đủ:
  - `subtitle-ocr/` (Tesseract runtime)
  - `media-tools/` (FFmpeg binary)
  - `espeak-ng/` (eSpeak NG speech data & binary)
  - `docs/colab-notebooks/` (Tất cả Jupyter notebooks cho GPU Colab)
  - `vc_redist.x64.exe` và các thư viện Qt/vcpkg DLLs.

---

## II. NHẬT KÝ SỰ CỐ THỰC TẾ & BÀI HỌC PHÒNG NGỪA (LIVING INCIDENT LOG)

*(Mỗi khi phát hiện lỗi mới trong quá trình phát triển, BẮT BUỘC phải bổ sung vào bảng bên dưới để ngăn ngừa tái diễn)*

| Mã lỗi | Ngày ghi nhận | Hiện tượng lỗi | Nguyên nhân gốc rễ (Root Cause) | Quy trình kiểm thử phòng ngừa |
| :--- | :--- | :--- | :--- | :--- |
| **INC-001** | 2026-08-26 | Mở file `.exe` không lên cửa sổ, ứng dụng thoát ngay lúc khởi động. | Tạo component mới `StudioOptionSwitcher.qml` nhưng quên khai báo trong `CMakeLists.txt` (`QML_FILES`) và thiếu import, khiến `QQmlApplicationEngine` ném lỗi `StudioOptionSwitcher is not a type`. CTest không bắt được vì CTest chỉ chạy unit tests C++ mà không khởi tạo `QApplication` render toàn bộ cây QML. | **Bắt buộc chạy Live Binary Smoke Test** (mục I.2): Khởi chạy trực tiếp file `.exe` từ command line và đọc console log trước khi bàn giao. |
| **INC-002** | *(Ghi nhận sau)* | *(Mô tả lỗi gặp phải)* | *(Nguyên nhân)* | *(Bước kiểm tra bổ sung)* |

---

## III. QUY TRÌNH KÝ DUYỆT BÀN GIAO (SIGN-OFF PROTOCOL)

Chỉ xác nhận hoàn thành công việc và thông báo cho người dùng khi:
1. `39/39 CTest tests` PASS 100%.
2. Đã đối chiếu và tích chọn toàn bộ các mục trong **Phần I (Checklist)**.
3. Đã chạy thử file `.exe` thực tế thành công và không ghi nhận bất kỳ crash/warning nào trong log.
4. Đã cập nhật Knowledge Graph qua `graphify update .`.
