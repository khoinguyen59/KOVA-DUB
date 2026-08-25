# Tab 4: Text-To-Speech (TTS Studio) — Modularization & Quality Report

## 1. Tóm Tắt Hiện Trạng & Mục Tiêu Đạt Được
- **Frontend Modularization**: Phân rã cấu trúc giao diện `qml/pages/TtsPage.qml` và các component chuyên trách trong `qml/components/tts/`:
  - `TtsStudioView.qml`: Khung làm việc tổng thể tích hợp hiển thị waveform, text input và thanh điều khiển audio player.
  - `TtsSettingsPanel.qml`: Bảng cấu hình chuyên sâu giọng đọc, tốc độ (speed), cao độ (pitch), emotion, length scale, repetition penalty và phân bổ luồng CPU/GPU.
  - `TtsHistoryPanel.qml`: Quản lý danh sách các mẫu âm thanh đã tổng hợp, hỗ trợ nghe lại, export WAV/MP3 và xóa lịch sử.
  - `SrtVoiceView.qml`: Giao diện lồng tiếng phụ đề theo từng timecode, tự động co giãn tốc độ (Smart Fit) để khớp khuôn hình.
- **Backend Modularization**:
  - `TtsEngine` & `TtsEngineInstance`: Quản lý lifecycle các engine TTS (Piper, Sherpa-ONNX, Kokoro, eSpeak-NG).
  - `TtsTextPreprocessor`: Xử lý phân đoạn văn bản, chuẩn hóa số/ký hiệu (text normalization), phonemization.
  - `TtsRequestValidator`: Kiểm tra tính hợp lệ của request, ngăn chặn out-of-range parameters.
  - `TimedSpeechPipeline` & `SubtitleSmartFitPlanner`: Kế hoạch đồng bộ thời lượng âm thanh theo timeline SRT.
  - `ColabTtsController` & `GatewayTtsController`: Điều phối inference đám mây GPU.

## 2. Kết Quả Kiểm Thử (Verification)
- **Unit Tests**:
  - `TestTtsTextPreprocessor`: Passed (100%).
  - `TestTtsRequestValidator`: Passed (100%).
- **UI Route Smoke Test**: `QmlRouteSmoke` route index 2 (`studio-tts`) đạt **100% passed**.
- **Build Status**: MSVC Release x64 Ninja compilation thành công không có lỗi.
