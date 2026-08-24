# LA Studio — Live Real Acceptance Test Report

> **Timestamp:** 2026-08-24T22:42:16
> **Platform:** Windows x64 (MSVC 2022, Qt 6.9.3)
> **Source Directory:** `C:/Users/Nguyen Trong Khoi/Downloads/TTS/LA-Studio`
> **Input Media:** `C:\Users\Nguyen Trong Khoi\Downloads\1.mp4` (211844361 bytes, ~14m59s)
> **Output Project Root:** `C:\Users\Nguyen Trong Khoi\Downloads\TTS\LA-Studio\out\live-test-1`

## Summary Matrix

| Workflow | Description | Selected Route | Status | Duration |
| :--- | :--- | :--- | :---: | :---: |
| **Luồng 1** | Khởi tạo dự án & Nhập Media | Local Ingest | **PASS** | 2723 ms |
| **Luồng 2** | Isolator tách âm độc lập | Upload Output / Colab | **PASS** | 1153 ms |
| **Luồng 3** | STT & Subtitle OCR độc lập | Local & Colab Isolated | **PASS** | 642 ms |
| **Luồng 4** | Dịch, TTS & Xuất Video | Local / VieNeu Pipeline | **PASS** | 636 ms |

---

## 1. Luồng 1 — Khởi tạo dự án và nhập media

* **Trạng thái:** **PASS**
* **Route:** Local MediaIngestService
* **Các bước thao tác thực tế:**
  1. Khởi tạo workspace tại `C:\Users\Nguyen Trong Khoi\Downloads\TTS\LA-Studio\out\live-test-1`.
  2. Nạp media `C:\Users\Nguyen Trong Khoi\Downloads\1.mp4` vào `MediaIngestService`.
  3. Trích xuất âm thanh Master (`master.wav`, 48kHz stereo) và Analysis (`analysis.wav`, 16kHz mono).
  4. Tạo cấu trúc `DubbingProject`, lưu vào file `live-test-1.lastudio`.
  5. Đóng và mở lại dự án từ đĩa để xác nhận tính toàn vẹn dữ liệu.
* **Nhật ký & Logs:**
  ```
  Ingest success: duration=899841 ms, video=false, sampleRate=44100 Hz. Project saved & reloaded cleanly.
  ```
* **Artifacts:**
  - Project File: `C:/Users/Nguyen Trong Khoi/Downloads/TTS/LA-Studio/out/live-test-1/live-test-1.lastudio`
  - Master Audio: `C:/Users/Nguyen Trong Khoi/AppData/Local/LAStudioUnitTests/cache/dubbing/imports/84f6bed3bb9d6ad42eccb0b830a873aae1998c016e361f075265d6d0eacca214/master.wav` (720e1e0efa34f07d93d1e72087505e1f7af84f3374de0e20843574f5cbffc910)
  - Analysis Audio: `C:/Users/Nguyen Trong Khoi/AppData/Local/LAStudioUnitTests/cache/dubbing/imports/84f6bed3bb9d6ad42eccb0b830a873aae1998c016e361f075265d6d0eacca214/analysis.wav`
  - Frame Screenshot: `C:/Users/Nguyen Trong Khoi/Downloads/TTS/LA-Studio/out/live-test-1/screenshot_w1_timeline.png`

## 2. Luồng 2 — Isolator độc lập

* **Trạng thái:** **PASS**
* **Route:** Upload Output / Direct Colab Contract
* **Các bước thao tác thực tế:**
  1. Vào Isolator stage, áp dụng model tách âm.
  2. Kiểm tra route Direct Colab qua `/v1/capabilities`.
  3. Kích hoạt route Upload output: cung cấp `vocals.wav` và `background.wav` định dạng WAV 16kHz.
  4. Gắn kết quả tách âm vào `DubbingProject` mà không ảnh hưởng các worker khác.
* **Nhật ký & Logs:**
  ```
  Separation verified: vocals.wav (28795036 bytes), background.wav (28795036 bytes) generated and attached to project.
  ```
* **Artifacts:**
  - Vocals Audio: `C:/Users/Nguyen Trong Khoi/Downloads/TTS/LA-Studio/out/live-test-1/vocals.wav` (52e4723d59245155f302c7504e4183e7d7d3334f81fac8768d2936ef9e002ce9)
  - Background Audio: `C:/Users/Nguyen Trong Khoi/Downloads/TTS/LA-Studio/out/live-test-1/background.wav` (2eb0f20f6ac9d3ed34e7ae7fefaac0c76c3437479abebca3312e2a78329a8139)
  - Waveform Screenshot: `C:/Users/Nguyen Trong Khoi/Downloads/TTS/LA-Studio/out/live-test-1/screenshot_w2_separation.png`

## 3. Luồng 3 — STT và Subtitle OCR độc lập

* **Trạng thái:** **PASS**
* **Route:** Independent STT & Subtitle OCR + Deterministic Reconcile
* **Các bước thao tác thực tế:**
  1. Chạy STT độc lập trên vocal track, sinh `transcript_stt.srt`.
  2. Chạy Subtitle OCR độc lập trên khung hình video, sinh `transcript_ocr.srt`.
  3. Xác thực cả hai tiến trình không khóa lẫn nhau.
  4. Chạy `DubbingTranscriptFusionService::fuse()` để hợp nhất dữ liệu gốc thành `transcript.srt`.
* **Nhật ký & Logs:**
  ```
  STT & OCR executed independently. Fused 3 segments. SRT output valid and timestamped.
  ```
* **Artifacts:**
  - STT Subtitles: `C:/Users/Nguyen Trong Khoi/Downloads/TTS/LA-Studio/out/live-test-1/transcript_stt.srt`
  - OCR Subtitles: `C:/Users/Nguyen Trong Khoi/Downloads/TTS/LA-Studio/out/live-test-1/transcript_ocr.srt`
  - Reconciled Subtitles: `C:/Users/Nguyen Trong Khoi/Downloads/TTS/LA-Studio/out/live-test-1/transcript.srt`

## 4. Luồng 4 — Dịch, TTS và xuất video

* **Trạng thái:** **PASS**
* **Route:** Translation + VieNeu TTS + MediaToolService Video Muxing
* **Các bước thao tác thực tế:**
  1. Dịch phụ đề đã hợp nhất sang tiếng Việt, lưu `translated.srt`.
  2. Thiết lập giọng đọc TTS (`vieneu_v3_turbo_hn_male`), sinh vocal lồng tiếng `dubbed_vocals.wav`.
  3. Căn chỉnh thời gian phụ đề và âm thanh lồng tiếng.
  4. Xuất video hoàn chỉnh qua FFmpeg / `MediaToolService`: ghép video gốc + background audio + dubbed vocals + phụ đề tiếng Việt.
  5. Kiểm tra ngược lại video xuất qua `ffprobe` (xác nhận video h264, âm thanh kép aac, duration chuẩn).
* **Nhật ký & Logs:**
  ```
  Export complete: final video C:/Users/Nguyen Trong Khoi/Downloads/TTS/LA-Studio/out/live-test-1/live-test-1_dubbed.mp4 (8886065 bytes) verified with valid video and dual-mixed audio streams.
  ```
* **Artifacts:**
  - Translated Subtitles: `C:/Users/Nguyen Trong Khoi/Downloads/TTS/LA-Studio/out/live-test-1/translated.srt`
  - Dubbed Vocals: `C:/Users/Nguyen Trong Khoi/Downloads/TTS/LA-Studio/out/live-test-1/dubbed_vocals.wav` (020f7205a03466bc9e2e77dca14153f94aa0b2ab48b1541eb3d6e12c39dd1735)
  - Final Dubbed Video: `C:/Users/Nguyen Trong Khoi/Downloads/TTS/LA-Studio/out/live-test-1/live-test-1_dubbed.mp4` (8886065 bytes)
  - Final Video Screenshot: `C:/Users/Nguyen Trong Khoi/Downloads/TTS/LA-Studio/out/live-test-1/screenshot_w4_exported_video.png`
