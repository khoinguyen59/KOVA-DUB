# Tab 2: Subtitle OCR — Modularization & Quality Report

## 1. Tóm Tắt Hiện Trạng & Mục Tiêu Đạt Được
- **Frontend Modularization**: Phân rã cấu trúc giao diện `qml/pages/SubtitleOcrPage.qml` và các component tương tác trong `qml/components/subtitles/ocr/`:
  - `SubtitleOcrPreviewCanvas.qml`: Khung preview video với overlay box ROI kéo thả trực tiếp, hỗ trợ bounding box resizing/moving mượt mà và auto-hide media controls.
  - `SubtitleOcrConfigPanel.qml`: Cấu hình sampling interval, confidence threshold, ngôn ngữ OCR và engine profile.
  - `SubtitleOcrSegmentsTable.qml`: Bảng kết quả phụ đề nhận diện, chỉnh sửa timestamp/text trực tiếp, export đa định dạng (SRT/VTT/JSON) và bridge sang Dubbing/TTS.
- **Backend Modularization**:
  - `src/controllers/subtitles/services/SubtitleOcrRoiService`: Tách biệt hoàn toàn logic tính toán tọa độ ROI, bảo đảm tỷ lệ co giãn video trên các độ phân giải khác nhau mà không phát sinh controller write loop.
  - `src/controllers/subtitles/services/SubtitleOcrExportService`: Xử lý tuần tự hóa file phụ đề chuẩn SRT, WebVTT, JSON với độ chính xác cao.
  - `src/subtitles/SubtitleOcrPipeline`: Pipeline bóc tách khung hình FFmpeg, SSIM duplicate filtering và điều phối recognition batch.
  - `src/subtitles/SubtitleOcrRuntimeService`: Quản lý runtime Tesseract local và PaddleOCR / PP-OCRv4 Colab runner.

## 2. Kết Quả Kiểm Thử (Verification)
- **Unit Tests**:
  - `TestSubtitleOcrPipeline`: Passed (100%).
  - `TestSubtitleOcrController`: Passed 21/21 (100%).
  - `TestSubtitleOcrRuntimeService`: Passed 14/14 (100%).
  - `PrepareSubtitleOcrFrameRuntime`: Passed (100%).
- **UI Route Smoke Test**: `QmlRouteSmoke` route index 15 (`subtitle-ocr`) đạt 100% pass, xác minh tương tác layout và child reachability.
- **Build Status**: MSVC Release x64 Ninja compilation thành công không có cảnh báo/lỗi.
