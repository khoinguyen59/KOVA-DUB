# ⚙️ TAB 13: SETTINGS & HARDWARE - CHI TIẾT KẾ HOẠCH & BÁO CÁO PHÂN RÃ

## 📌 1. Hiện Trạng & Vấn Đề
- Tab Cài Đặt quản lý thông số phần cứng, vị trí lưu models, cấu hình suy luận từ xa (Colab/Gateway) và thông tin ứng dụng.
- Cần phân chia thành các sub-tabs rành mạch để người dùng không bị rối mắt.

## 🏗️ 2. Cấu Trúc Đã Phân Rã & Module Hóa
### Frontend (`qml/pages/settings/`):
- `GeneralSettingsTab.qml`: Cài đặt ngôn ngữ giao diện, đường dẫn lưu trữ mặc định.
- `HardwareSettingsTab.qml`: Tự động nhận diện CPU (số luồng AVX2), GPU (Nvidia CUDA / AMD ROCm / DirectML), dung lượng RAM và VRAM.
- `RemoteInferenceTab.qml`: Quản lý API Gateway tokens, URL Google Colab worker endpoints và kiểm tra độ trễ mạng.
- `AboutLicensesTab.qml`: Thông tin phiên bản, bản quyền mã nguồn mở và liên kết tài liệu.

### Backend (`src/core/`):
- `Settings.h/.cpp`: Quản lý đọc/ghi cài đặt ứng dụng JSON.
- `HardwareManager.h/.cpp`: Thuật toán thăm dò phần cứng máy tính.

## 🧪 3. Kết Quả Kiểm Thử
- **Unit Tests**: `TestStudioCapabilities`, `RemoteLivePreflightContract` đạt PASS.
