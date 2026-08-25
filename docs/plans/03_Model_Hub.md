# 📦 TAB 3: MODEL HUB & MY MODELS - CHI TIẾT KẾ HOẠCH & BÁO CÁO PHÂN RÃ

## 📌 1. Hiện Trạng & Vấn Đề
- `MyModelsPage.qml` chứa **1,154 dòng** gồm cả bộ lọc, grid card, drawer thông số kỹ thuật.
- `DownloadInstallService.cpp` chứa **1,681 dòng** quản lý tải file, giải nén và xác thực SHA-256.

## 🏗️ 2. Cấu Trúc Đã Phân Rã & Module Hóa
### Frontend (`qml/components/models/`):
- `ModelCatalogHeader.qml`: Thanh tìm kiếm tức thì và các nút lọc nhanh danh mục (TTS, STT, LLM, Separation, OCR).
- `ModelCardGrid.qml`: Lưới hiển thị các thẻ mô hình trực quan kèm trạng thái "Đã Cài" / "Online" và dung lượng.
- `ModelDetailsDrawer.qml`: Bảng hiển thị thông số chi tiết (VRAM khuyến nghị, runtime hỗ trợ, license, nút cài đặt/gỡ).

### Backend (`src/controllers/models/services/`):
- `ModelDownloadWorkerService.h/.cpp`: Quản lý tác vụ tải ngầm HTTP, theo dõi tiến trình (progress percent, bytes received/total), resume khi đứt mạng.
- `src/controllers/models/DownloadInstallService.cpp`: Quản lý cài đặt mô hình vào thư mục `models/`.

## 🧪 3. Kết Quả Kiểm Thử
- **Unit Tests**: `TestDownloadInstallService`, `TestModelsAndRuntimes`, `TestModelsPathMigration` đạt PASS.
