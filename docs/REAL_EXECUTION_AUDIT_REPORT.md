# Báo Cáo Kiểm Toán Thực Tế (Execution Audit Report) — Quy Trình Live Test LA-Studio / KOVA-DUB

> **Ngày thực hiện:** 25/08/2026  
> **Người thực hiện:** Antigravity AI Assistant  
> **Dự án:** `LA-Studio` (C++/Qt 6.9.3) / Repository: `https://github.com/khoinguyen59/KOVA-DUB`  
> **Mục tiêu:** Kiểm thử thực tế toàn diện (Live Test) quy trình lồng tiếng AI cho video [1.mp4](file:///C:/Users/Nguyen%20Trong%20Khoi/Downloads/1.mp4) (dài 14 phút 59 giây).  

---

## 1. Tóm Tắt Trung Thực Về Cách Thức Thực Hiện (Executive Disclosure)

> [!IMPORTANT]
> **Xác nhận trung thực:** Quá trình kiểm thử vừa qua **KHÔNG chạy trực tiếp thông qua giao diện người dùng (UI QML) của ứng dụng `LA-Studio.exe` hay bộ chạy C++ `LiveRealWorkflowRunner`**, mà được thực hiện thông qua **chuỗi các script Python và lệnh FFmpeg CLI độc lập** do AI viết để kết nối tới các Colab GPU Workers và xử lý dữ liệu.

---

## 2. So Sánh: Luồng App Gốc vs Luồng Đã Thực Hiện Thực Tế

| Tiêu chí | Thiết kế App Gốc (`LA-Studio` C++/Qt) | Cách Thức AI Đã Chạy Thực Tế | Lý Do & Nguyên Nhân |
| :--- | :--- | :--- | :--- |
| **Giao diện & Điều khiển** | Giao diện Qt Quick/QML (`DubbingPage.qml`), người dùng bấm từng nút trên UI để kích hoạt C++ Services. | Viết các script Python (`scratch/run_*.py`) chạy trong terminal. | Do môi trường tương tác là Agent dòng lệnh (CLI), không có cơ chế click chuột trực tiếp trên giao diện QML desktop đang chạy. |
| **Ingest & Normalize** | Class C++ `MediaIngestService` và `Local DSP Engine` gọi FFmpeg thư viện/tiến trình con. | Script Python gọi `ffmpeg` CLI trực tiếp trên máy Windows. | Cùng sử dụng FFmpeg 9.0 binary, cho kết quả file PCM WAV tương đương. |
| **Tách âm (Isolator)** | C++ gọi HTTP POST sang `sherpa-onnx-uvr-vocals-ft` Colab Worker. | Script Python `colab_client.py` gửi file lên Colab GPU qua Cloudflare Tunnel. | **100% Model AI thật trên Colab GPU.** Sử dụng đúng payload và contract của notebook. |
| **Nhận dạng STT** | C++ gọi HTTP POST sang `whisper.cpp` Colab Worker. | Script Python `run_stt_client.py` gửi file lên Whisper Large-v3 GPU Colab. | **100% Model AI thật trên Colab GPU.** Trích xuất 473 phân đoạn trong 94.7s. |
| **Nhận dạng Subtitle OCR** | C++ `FrameExtractorService` cắt frame $\rightarrow$ gọi Colab PP-OCRv5. | Script `extract_ocr_crops.py` cắt 473 PNG $\rightarrow$ `run_ocr_client.py` gửi sang Colab. | **100% Model AI thật trên Colab GPU.** Xử lý 473 frame trong 148.4s. |
| **Dung hợp (Fusion)** | Class C++ `DubbingTranscriptFusionService::fuse()`. | Script `fuse_transcripts.py` mô phỏng logic chấm điểm ưu tiên OCR > STT. | Tái lập thuật toán so khớp timestamp và confidence bằng Python. |
| **Dịch thuật (Translation)** | C++ gọi `Tencent Hy-MT2 1.8B` trên Colab GPU. | Script `translate_fast_parallel.py` (sử dụng API dịch đa luồng). | Người dùng yêu cầu *"dịch dùm tôi luôn đi"* để tiết kiệm thời gian chuyển đổi Colab runtime. |
| **Lồng tiếng (TTS / Voice Clone)** | C++ gọi `OmniVoice` / `VieNeu-TTS` Worker. | Script `run_omnivoice_clone_client.py` gửi mẫu giọng 12s và 473 câu lên OmniVoice GPU. | **100% Model AI thật trên Colab GPU.** Tạo profile nhân bản và sinh 473 file WAV (30 phút GPU). |
| **Hòa trộn & Xuất bản (Export)** | Class C++ `MediaToolService` trộn audio và muxing MP4. | Script `final_mux_video.py` và `burn_subtitles_mask.py` gọi FFmpeg CLI. | FFmpeg CLI thực hiện mixing, tạo phụ đề ASS và encode H.264 hardsub. |

---

## 3. Nhật Ký Chi Tiết Các Tác Vụ Đã Thực Hiện

### Bước 1: Khởi tạo và Đẩy Source Code lên GitHub
* **Hành động:** Tạo repo GitHub Public `https://github.com/khoinguyen59/KOVA-DUB`.
* **Cập nhật:** Sửa toàn bộ 40 Colab Notebooks để tích hợp tính năng tải file trực tiếp qua `showDirectoryPicker()` (JavaScript File System Access API), giúp người dùng tải thẳng kết quả về thư mục dự án trên máy mà không bị lưu vào thư mục `Downloads`.

### Bước 2: Tách Giọng Nói & Nhạc Nền (Colab UVR5 GPU)
* **Thực thi:** Kết nối tới tunnel của notebook `LA_STUDIO_SEPARATION_UVR_VOCALS_GPU.ipynb`.
* **Dữ liệu thật:** Đẩy file âm thanh 14m59s lên Colab $\rightarrow$ Nhận về `vocals.wav` (158.7 MB) và `background.wav` (158.7 MB).

### Bước 3: Nhận Dạng Giọng Nói STT (Colab Whisper Large-v3 GPU)
* **Thực thi:** Kết nối tới notebook `LA_STUDIO_STT_WHISPER_GPU.ipynb` trên GPU NVIDIA L4.
* **Dữ liệu thật:** Nhận diện toàn bộ 14m59s âm thanh tiếng Trung trong 94.7 giây $\rightarrow$ Sinh ra `transcript.srt` (473 câu).

### Bước 4: Nhận Dạng Chữ Phụ Đề Video OCR (Colab PP-OCRv5 GPU)
* **Thực thi:** Dùng FFmpeg cắt 473 ảnh crop vùng phụ đề $\rightarrow$ Gửi lên notebook `LA_STUDIO_SUBTITLE_OCR_PP_OCRV5_GPU.ipynb`.
* **Dữ liệu thật:** Xử lý 473 ảnh trong 148.4 giây $\rightarrow$ Sinh ra `transcript_ocr.srt`.

### Bước 5: Dung Hợp Phụ Đề & Dịch Sang Tiếng Việt
* **Thực thi:** Script `fuse_transcripts.py` kết hợp STT + OCR $\rightarrow$ `reviewed-transcript.srt`.
* **Dịch thuật:** Script `translate_fast_parallel.py` dịch toàn bộ 473 câu sang Tiếng Việt chuẩn trong 43 giây $\rightarrow$ `translated.srt`.

### Bước 6: Nhân Bản Giọng Nói & Lồng Tiếng (Colab OmniVoice GPU)
* **Thực thi:** Trích xuất 12 giây mẫu giọng Lão Vương $\rightarrow$ Gửi lên notebook `LA_STUDIO_VOICE_CLONE_OMNIVOICE_GPU.ipynb`.
* **Dữ liệu thật:** 
  * Tạo Voice Profile `bcab519cf4324fa48ed4ab1cf7693bd9`.
  * Sinh âm thanh tiếng Việt cho từng câu trong 473 câu thoại (mất 1.850 giây ~ 30 phút GPU).
  * Ghép các câu thoại vào dải âm thanh timeline 899.85 giây $\rightarrow$ `dubbed_vocals.wav` (43.2 MB).

### Bước 7: Xuất Bản Video & Sửa Lỗi Kỹ Thuật (Muxing & Hardsub)
* **Sự cố 1 (Mất phụ đề trên màn hình):** Ban đầu chỉ copy stream video gốc không burn sub $\rightarrow$ Đã sửa bằng bộ lọc `ass` của FFmpeg để ghi cứng phụ đề Tiếng Việt (Hardsub) lên video.
* **Sự cố 2 (Âm thanh bị trả về tiếng Trung gốc):** Lệnh FFmpeg muxing thiếu flag `-map 0:v:0 -map 1:a:0` nên FFmpeg mặc định lấy audio gốc của input 0 $\rightarrow$ Đã khắc phục bằng cách ánh xạ rõ ràng luồng âm thanh `1:a:0` từ `final_dubbed_audio.wav`.
* **Sự cố 3 (Phụ đề tiếng Việt bị đè lên chữ tiếng Trung cũ):** Đã nâng cấp style ASS với `BorderStyle=3` (khung nền đen bán trong suốt) để che kín hoàn toàn chữ tiếng Trung gốc.

### Bước 8: Đồng Bộ Lên Google Drive
* **Thực thi:** Tự động copy toàn bộ 8 file thành phẩm (234.5 MB video + âm thanh + phụ đề) vào `G:\My Drive\KOVA-DUB-15MIN-TEST\` thông qua Google Drive Desktop.

---

## 4. Đánh Giá Khách Quan: Ưu Điểm & Tồn Tại

### ✅ Những Gì Đã Đạt Được (Giá Trị Thực Tế):
1. **Các mô hình AI trên Colab chạy thật 100%:** UVR5, Whisper Large-v3, PP-OCRv5 và OmniVoice Voice Clone đều đã thực sự xử lý file dữ liệu 15 phút trên GPU của Google Colab.
2. **Video thành phẩm hoàn chỉnh:** Đã tạo ra video [live-test-1_dubbed.mp4](file:///C:/Users/Nguyen%20Trong%20Khoi/Downloads/TTS/LA-Studio/out/colab-live/live-test-1_dubbed.mp4) dài đúng **14 phút 59 giây** có giọng lồng tiếng Việt nhân bản của nhân vật, giữ nguyên nhạc nền và có phụ đề tiếng Việt rõ đẹp.
3. **Bộ Notebooks Colab được tối ưu:** Toàn bộ 40 notebooks trên repo GitHub `KOVA-DUB` đã được hoàn thiện, có sẵn tính năng tải file qua JavaScript File System Access API.

### ⚠️ Những Điểm Tồn Tại Cần Cải Thiện:
1. **Chưa tích hợp tự động một chạm trên C++ App:** Do giới hạn môi trường agent không click được trực tiếp UI desktop, quy trình trên được AI điều phối bằng script Python bên ngoài thay vì bấm nút trên `LA-Studio.exe`.
2. **Cần hoàn thiện luồng tự động trong `LiveRealWorkflowRunner.cpp`:** Cần đưa logic gọi API tuần tự này vào trực tiếp mã nguồn C++ của `LiveRealWorkflowRunner` để app C++ có thể tự động chạy từ đầu đến cuối mà không cần script Python hỗ trợ.
