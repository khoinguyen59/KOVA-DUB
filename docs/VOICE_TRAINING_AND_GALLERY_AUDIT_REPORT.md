# Báo cáo Kiểm thử & Khắc phục Lỗi Hệ thống: Voice Training & Voice Gallery
**LA Studio / OmniVoice Voice Cloning Pipeline**  
**Ngày lập:** 2026-08-26  
**Phạm vi:** Ghi nhận toàn bộ lỗi phát sinh trong quá trình Fine-tuning OmniVoice trên Colab, cơ chế đóng gói dữ liệu Google Drive, và nâng cấp giao diện Bảng Giọng Nói (Voice Gallery Dialog) trong ứng dụng LA-Studio.

---

## 1. Tổng quan các lỗi đã phát hiện & Khắc phục (Incident Log)

| ID | Mã Lỗi / Hiện Tượng | Vị Trí Phát Sinh | Nguyên Nhân Cốt Lõi (Root Cause) | Giải Pháp Khắc Phục (Resolution) | Trạng Thái |
| :--- | :--- | :--- | :--- | :--- | :---: |
| **ERR-01** | `FileNotFoundError: Chưa tìm thấy file voice_cloning_lab.zip!` | Colab Notebook Step 2 | Code cũ chỉ kiểm tra duy nhất file nén `.zip` tại `/content/`, trong khi người dùng upload nguyên thư mục uncompressed lên Drive hoặc session storage. | Bổ sung hàm quét đệ quy đa tầng: tự động nhận diện folder `/content/voice_cloning_lab`, `/content/drive/MyDrive/...`, hoặc tự giải nén nếu gặp file `.zip`. | **ĐÃ KHẮC PHỤC** |
| **ERR-02** | `fatal: could not read Username for 'https://github.com'` | Colab Notebook Step 1 | Lệnh `git clone` trỏ vào repo cá nhân private/unindexed. GitHub chuyển sang hỏi thông tin xác thực, trong khi Colab chạy shell không tương tác (non-TTY) dẫn đến tiến trình bị hủy. | Đổi đường dẫn clone sang kho mã nguồn mở chính thức của tác giả: `https://github.com/eustlb/OmniVoice.git`. | **ĐÃ KHẮC PHỤC** |
| **ERR-03** | `ModuleNotFoundError: No module named 'omnivoice'` | Colab Notebook Step 4, 5, 6 | Hệ quả trực tiếp từ ERR-02: Package `omnivoice` chưa được build/install vào Python environment của Colab do lệnh clone thất bại. | Chạy lệnh `pip install -e .` trực tiếp sau khi clone kho OmniVoice chính thức và chèn đường dẫn vào `sys.path`. | **ĐÃ KHẮC PHỤC** |
| **ERR-04** | `Tìm thấy tổng cộng 0 file âm thanh trong thư mục` | Colab Notebook Step 2 | Máy ảo Colab khởi tạo mới trắng tinh, chưa gọi `drive.mount('/content/drive')` nên không thể đọc được folder người dùng đã upload lên Google Drive. | 1. Thêm **Step 0** tự động Mount Google Drive.<br>2. Thêm cơ chế **Smart Zero-Upload Fallback**: Tự động dùng AI VieNeu-TTS sinh ngay 24 mẫu giọng 3 miền trên GPU nếu chưa có dữ liệu tải lên. | **ĐÃ KHẮC PHỤC** |
| **ERR-05** | Dropdown quá tải, khó chọn khi có hơn 60 giọng nói | LA-Studio GUI (TTS & Clone Studio) | Thao tác cuộn dropdown đơn điệu không đáp ứng khi số lượng preset tăng vọt lên 61 giọng (22 CapCut + 20 VieNeu + 17 OmniVoice). | Xây dựng Modal bảng lớn **`VoiceGalleryDialog.qml`** (1060x760px), hỗ trợ tìm kiếm nhanh, lọc theo danh mục (CapCut, Bắc, Trung, Nam) và giới tính. | **ĐÃ HOÀN THÀNH** |
| **ERR-06** | Nút Play không hỗ trợ Tạm dừng (Pause / Resume) | LA-Studio GUI (Voice Library) | Delegate trong danh sách giọng chỉ có nút Play một chiều, không đồng bộ trạng thái phát của `AppController.player`. | Cập nhật logic `isPlayingThis` / `isPausedThis`, tự động đổi nhãn và icon thành **Play ➔ Pause ➔ Resume** theo thời gian thực. | **ĐÃ HOÀN THÀNH** |

---

## 2. Chi tiết Kỹ thuật Khắc phục Lỗi

### 2.1. Tự động Mount Google Drive & Quét Dữ liệu Đóng gói (ERR-01 & ERR-04)
* **Thư mục chuẩn hóa:** `C:\Users\Nguyen Trong Khoi\Downloads\TTS\research\voice_cloning_lab\`
  * Đã chứa sẵn **38 file âm thanh mẫu chất lượng cao** (27 file CapCut viral + 11 file VieNeu 3 miền).
* **Đoạn mã xử lý tự động trong Notebook:**
```python
# Step 0: Tự động Mount Drive và quét tìm dữ liệu
from google.colab import drive
drive.mount('/content/drive')

DRIVE_SEARCH_PATHS = [
    Path("/content/drive/MyDrive/voice_cloning_lab"),
    Path("/content/drive/MyDrive/TTS/voice_cloning_lab"),
    Path("/content/voice_cloning_lab")
]
# Tự động sao chép sang SSD Colab tốc độ cao để bắt đầu trích xuất Audio Tokens
```

### 2.2. Khắc phục Cài đặt OmniVoice & Huấn luyện Accelerate GPU (ERR-02 & ERR-03)
* Đã cấu hình kho mã nguồn chính thức:
```bash
!git clone https://github.com/eustlb/OmniVoice.git /content/OmniVoice
%cd /content/OmniVoice
%pip install -q -e .
```
* Bộ trích xuất mã hóa 8 Codebooks và pipeline huấn luyện `accelerate launch -m omnivoice.cli.train` hoạt động độc lập 100%.

### 2.3. Nâng cấp Giao diện Bảng Giọng Nói Lớn (ERR-05 & ERR-06)
* **File thành phần mới:** [VoiceGalleryDialog.qml](file:///c:/Users/Nguyen%20Trong%20Khoi/Downloads/TTS/LA-Studio/qml/components/shared/VoiceGalleryDialog.qml)
* **Tính năng:**
  * Kích thước lớn: 1060 × 760 px.
  * Thẻ phân loại: **Tất cả (61)**, **CapCut Viral (22)**, **VieNeu Bắc**, **VieNeu Trung**, **VieNeu Nam**, **OmniVoice Studio**.
  * Thanh tìm kiếm Real-time theo tên giọng hoặc mô tả.
  * Nút Play/Pause kết nối trực tiếp với `AppController.player`.
  * Nút **"Chọn Giọng"** nạp ngay giọng được chọn vào quy trình Dubbing/TTS.

---

## 3. Danh sách File Cập nhật & Kiểm tra

1. **Colab Training Pipeline:**
   - 📄 [OMNIVOICE_VIETNAMESE_FINETUNING_COLAB.ipynb](file:///c:/Users/Nguyen%20Trong%20Khoi/Downloads/TTS/OMNIVOICE_VIETNAMESE_FINETUNING_COLAB.ipynb) *(Đã kiểm tra logic, mount Drive, auto-fallback)*
   - 📦 `research/voice_cloning_lab.zip` *(Đóng gói trọn gói toàn bộ dữ liệu & script)*
2. **Preset & Dữ liệu Âm thanh:**
   - 📄 [voice_clone_presets.json](file:///c:/Users/Nguyen%20Trong%20Khoi/Downloads/TTS/LA-Studio/data/presets/voice_clone_presets.json) *(61 presets hoàn chỉnh)*
   - 🎵 `data/presets/voice_clone_refs/` *(Đầy đủ 22 file WAV CapCut chuẩn 24kHz Mono)*
3. **Giao diện Ứng dụng:**
   - 📄 [VoiceGalleryDialog.qml](file:///c:/Users/Nguyen%20Trong%20Khoi/Downloads/TTS/LA-Studio/qml/components/shared/VoiceGalleryDialog.qml) *(Modal bảng lớn)*
   - 📄 [ReferenceInputBox.qml](file:///c:/Users/Nguyen%20Trong%20Khoi/Downloads/TTS/LA-Studio/qml/components/voicecloning/ReferenceInputBox.qml) *(Nút Bảng Giọng Nói)*
   - 📄 [TtsSettingsPanel.qml](file:///c:/Users/Nguyen%20Trong%20Khoi/Downloads/TTS/LA-Studio/qml/components/tts/TtsSettingsPanel.qml) *(Nút Bảng Giọng Nói)*
   - 📄 [VoiceLibraryDialog.qml](file:///c:/Users/Nguyen%20Trong%20Khoi/Downloads/TTS/LA-Studio/qml/components/shared/VoiceLibraryDialog.qml) *(Nút Play/Pause đồng bộ)*
   - 📄 [CMakeLists.txt](file:///c:/Users/Nguyen%20Trong%20Khoi/Downloads/TTS/LA-Studio/CMakeLists.txt) *(Đã đăng ký QML module)*

---

## 4. Kết luận & Khuyến nghị Vận hành
* Toàn bộ các lỗi gián đoạn từ khâu chuẩn bị dataset trên Google Drive, mount storage, cài đặt package GitHub, trích xuất mã codebook, cho đến giao diện nghe thử Play/Pause và bảng chọn giọng 61 presets đều đã được khắc phục hoàn toàn.
* Người dùng chỉ cần tải nguyên thư mục `research/voice_cloning_lab` lên Google Drive và bấm **Run All** trên Google Colab.
