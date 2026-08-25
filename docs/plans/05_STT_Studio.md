# Tab 5: Speech-To-Text (STT Studio) — Modularization & Quality Report

## 1. Tóm Tắt Hiện Trạng & Mục Tiêu Đạt Được
- **Frontend Modularization**: Phân rã cấu trúc giao diện `qml/pages/SttPage.qml` và các component chuyên trách trong `qml/components/stt/`:
  - `SttStudioView.qml`: Khung làm việc chính kết nối input, settings, transcription view và history.
  - `SttInputSection.qml`: Quản lý ghi âm trực tiếp qua mic hoặc kéo thả file âm thanh/video, preview dạng sóng âm (waveform).
  - `SttSettingsPanel.qml`: Cấu hình nhận diện ngôn ngữ, VAD (Voice Activity Detection), temperature, initial prompt, route (Local whisper.cpp / Sherpa-ONNX vs Colab GPU vs Cloud Gateway).
  - `SttTranscriptionView.qml`: Bảng hiển thị kết quả phân đoạn (segment), timestamp bắt đầu/kết thúc, độ tin cậy và copy/export SRT/VTT.
  - `SttHistoryPanel.qml`: Danh sách các phiên STT đã chạy, quản lý bộ nhớ đệm và tìm kiếm nhanh.
- **Backend Modularization**:
  - `src/controllers/stt/SttSessionController`: Quản lý state machine nạp/hủy model, quản lý tiến trình nhận diện âm thanh.
  - `src/controllers/stt/SttAudioDecoder`: Giải mã âm thanh đa luồng, tự động resample sang PCM 16kHz float32 mono chống lỗi format.
  - `src/stt/SttEngine` & `src/stt/SttEngineInstance`: Cung cấp giao diện trừu tượng hóa cho các engine STT cục bộ.
  - `src/stt/ColabSttRunner` & `src/stt/GatewaySttRunner`: Thực thi nhận diện từ xa không chặn luồng giao diện người dùng.

## 2. Kết Quả Kiểm Thử (Verification)
- **Unit Tests**: `TestSttSession` đạt **16/16 passed (100%)**.
- **UI Route Smoke Test**: `QmlRouteSmoke` route index 1 (`studio-stt`) đạt **100% passed**.
- **Build Status**: MSVC Release x64 Ninja compilation thành công không có lỗi.
