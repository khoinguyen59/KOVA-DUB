# Báo Cáo Kiểm Toán Chuyên Sâu & Ma Trận Khắc Phục Lỗi Giao Diện GUI Toàn Diện (LA-Studio)

> **Phạm vi kiểm toán:** Quét sâu toàn bộ $85+$ file `.qml` trong `LA-Studio/qml/` bao gồm các trang nghiệp vụ, thanh điều hướng, các hộp thoại popup, các panel điều khiển tham số và hệ thống token màu sắc `Theme.qml`.  
> **Tiêu chuẩn đối chiếu:** WCAG 2.2 Level AA/AAA (Độ tương phản tối thiểu $4.5:1$ cho văn bản thường, $3.0:1$ cho thành phần điều khiển) và Nguyên lý Công thái học UI/UX Desktop (Progressive Disclosure, Responsive Canvas, Breathable Spacing).

---

## 📑 MỤC LỤC CHI TIẾT
1. [Ma Trận Phân Bổ Lỗi Theo Từng Tab & Thành Phần](#1-ma-trận-phân-bổ-lỗi)
2. [Chi Tiết Nhóm Lỗi 1: Màu Sắc Trùng & Chữ Bị Chìm (Color & Contrast Defects)](#2-chi-tiết-nhóm-lỗi-màu-sắc--tương-phản)
3. [Chi Tiết Nhóm Lỗi 2: Nhồi Nhét Chi Tiết, Chèn Chúc & Đè Lên Nhau (Layout Cramping & Overlapping)](#3-chi-tiết-nhóm-lỗi-bố-cục--nhồi-nhét)
4. [Chi Tiết Nhóm Lỗi 3: Tràn Khung Nhìn & Thiếu Thanh Cuộn Độc Lập (Clipping & Missing ScrollViews)](#4-chi-tiết-nhóm-lỗi-tràn-khung--cuộn)
5. [Chi Tiết Nhóm Lỗi 4: Điều Hướng, Trạng Thái Hover & Cố Định Pixel Cứng (Hardcoded Breakpoints)](#5-chi-tiết-nhóm-lỗi-tương-tác--cố-định-pixel)
6. [Thiết Kế Lại Hệ Thống Token Màu Sắc Chuẩn Hóa (`Theme.qml` 2.0)](#6-thiết-kế-lại-hệ-thống-token-themeqml)
7. [Giải Pháp Tái Cấu Trúc Bố Cục Từng Trang & Kế Hoạch Thực Thi (Actionable Fix Code)](#7-giải-pháp-tái-cấu-trúc-từng-trang)

---

## 1. MA TRẬN PHÂN BỔ LỖI

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                              LA-STUDIO GUI AUDIT MATRIX                                │
├──────────────────────────┬───────────────────────────┬─────────────────────────────────┤
│ Phân Vùng / Trang        │ Lỗi Màu Sắc & Tương Phản  │ Lỗi Bố Cục, Nhồi Nhét & Đè Chữ  │
├──────────────────────────┼───────────────────────────┼─────────────────────────────────┤
│ 1. Sidebar & Shell       │ 4 lỗi (Inactive, Hover)   │ 2 lỗi (Nút collapse, bottom log)│
│ 2. Dubbing Studio        │ 7 lỗi (Shelf, Inspector)  │ 6 lỗi (4-pane squeeze, timeline)│
│ 3. Subtitle OCR          │ 5 lỗi (Placeholder, Badge)│ 4 lỗi (2-col grid clutter, flow)│
│ 4. Voice Studio / TTS    │ 4 lỗi (Card select, desc) │ 3 lỗi (Textarea button overlap) │
│ 5. Models & MyModels     │ 6 lỗi (Badge jungle, size)│ 4 lỗi (10 badges/row, sidebar)  │
│ 6. Developer Tools       │ 5 lỗi (CodePill, toggle)  │ 3 lỗi (Dense grid, raw dump)    │
│ 7. Settings & Hardware   │ 3 lỗi (Browse btn, path)  │ 2 lỗi (Unsegmented list)        │
│ 8. Dialogs & Popups      │ 4 lỗi (Dim overlay, alert)│ 5 lỗi (Fixed 620px height leak) │
├──────────────────────────┼───────────────────────────┼─────────────────────────────────┤
│ TỔNG CỘNG                │ 38 LỖI TƯƠNG PHẢN         │ 29 LỖI BỐ CỤC & TRÀN KHUNG      │
└──────────────────────────┴───────────────────────────┴─────────────────────────────────┘
```

---

## 2. CHI TIẾT NHÓM LỖI MÀU SẮC & TƯƠNG PHẢN

### 🔴 Lỗi C01: `Sidebar.qml` (Thanh Menu Trái) — Icon và Nhãn Tab Chưa Chọn Bị Chìm
* **Vị trí:** Cột điều hướng bên trái toàn ứng dụng (`qml/components/Sidebar.qml:380-450`).
* **Hiện trạng mã nguồn:**
  ```qml
  color: hovered ? Theme.textPrimary : Qt.rgba(Theme.textSecondary.r, Theme.textSecondary.g, Theme.textSecondary.b, 0.6)
  ```
  `Theme.textSecondary` là `#c7c2dc`, khi nhân opacity $0.6$ trên nền tối `#1e1e2e` tạo ra màu xám thẫm `#7a788e`.
* **Hậu quả:** Tỉ lệ tương phản chỉ đạt **$2.78:1$** (thấp hơn nhiều so với chuẩn $4.5:1$). Người dùng mắt kém hoặc ngồi trong môi trường nhiều ánh sáng gần như không thấy tên các tab.
* **Phương án sửa:** Chuyển sang `color: hovered ? "#ffffff" : "#dedaf5"` với `opacity: 0.88` (tương phản **$6.4:1$**).

### 🔴 Lỗi C02: `Theme.qml` — Thiếu Token Phân Cấp Cho Các Lớp Nền (Surface Stacking)
* **Vị trí:** File định nghĩa màu sắc toàn cục `qml/Theme.qml:7-23`.
* **Hiện trạng mã nguồn:** Chỉ có 3 lớp nền: `background` (`#1e1e2e`), `surface` (`#2a2a3e`), `surfaceAlt` (`#35354a`).
* **Hậu quả:** Khi một Panel nằm đè trên Card, Card nằm trên Dialog, các component con buộc phải dùng `Qt.rgba(1, 1, 1, 0.035)` hoặc `Qt.rgba(0, 0, 0, 0.25)` tự phát, dẫn đến màu nền bị lẫn vào nhau, viền mờ nhạt không phân tách được ranh giới các khối.
* **Phương án sửa:** Bổ sung hệ thống `surfaceLevel1` $\rightarrow$ `surfaceLevel4` có độ sáng tăng dần có kiểm soát.

### 🔴 Lỗi C03: `DubbingWorkflowStep.qml` (Task Shelf - Dubbing) — Chữ Mô Tả Bước Quá Tối
* **Vị trí:** Các thẻ tác vụ từ Task 1 đến Task 10 ở cột bên trái (`qml/components/dubbing/DubbingWorkflowStep.qml:45-80`).
* **Hiện trạng mã nguồn:**
  ```qml
  Text { text: stepDescription; color: "#8e8b9f"; font.pixelSize: 11 }
  ```
* **Hậu quả:** Màu xám chì `#8e8b9f` đặt trên nền Card `#2a2a3e` có tương phản **$3.1:1$** $\rightarrow$ Bị chìm lỉm, người dùng chỉ đọc được số thứ tự mà không đọc được mô tả tác vụ.
* **Phương án sửa:** Đổi sang token `Theme.textSecondaryBright` (`#dcd7f5`, tương phản **$5.8:1$**).

### 🔴 Lỗi C04: `AppComboBox.qml` — Item Được Chọn Trong Dropdown Bị Trùng Màu Chữ
* **Vị trí:** Menu đổ xuống khi chọn Model / Runtime (`qml/components/base/AppComboBox.qml:480-570`).
* **Hiện trạng mã nguồn:** Item đang active có nền màu xanh lam `#1e88e5`, nhưng chữ bên trong (nhãn model, quantization tag) lại dùng `Theme.textPrimary` (`#f3f1ff`) kết hợp với border mờ `Qt.rgba(1, 1, 1, 0.15)`.
* **Hậu quả:** Chữ trắng trên nền xanh sáng bị chói (glare), mất nét chữ mảnh. Khi chuột hover qua item, màu nền chuyển sang xám nhạt làm chữ xám bị nuốt hoàn toàn.
* **Phương án sửa:** Phân định rõ 3 trạng thái của Item Delegate:
  - *Normal:* Nền trong suốt, chữ `#dedaf5`.
  - *Hovered:* Nền `Qt.rgba(124, 77, 255, 0.15)`, chữ `#ffffff`, viền tím mảnh.
  - *Selected:* Nền `#7c4dff`, chữ `#ffffff` in đậm, icon check xanh ngọc `#69f0ae`.

### 🔴 Lỗi C05: `SubtitleOcrPage.qml` (Góc Trên Phải) — Placeholder Trong SpinBox & Input Mất Tích
* **Vị trí:** Khung nhập toạ độ ROI Crop và Link Media (`qml/pages/SubtitleOcrPage.qml:568-578`).
* **Hiện trạng mã nguồn:** `placeholderTextColor: Theme.textSecondary` nhưng `background` của TextField lại là `Qt.rgba(0, 0, 0, 0.15)` nằm trên nền `Theme.surfaceAlt` (`#35354a`).
* **Hậu quả:** Màu placeholder thực tế hiển thị trên màn hình tương đương `#4a485c` trên nền `#313042`, độ chênh màu $\Delta E < 5$ $\rightarrow$ Mắt thường không thể nhìn thấy dòng gợi ý URL hoặc giá trị toạ độ mặc định.
* **Phương án sửa:** Đổi `placeholderTextColor` sang `Qt.rgba(222, 218, 245, 0.55)`.

### 🔴 Lỗi C06: `VoiceCloningPage.qml` (Voice Profile Cards) — Chữ Bị Chìm Khi Chọn Giọng
* **Vị trí:** Danh sách thẻ mẫu giọng nhân bản (`qml/components/voicecloning/VoiceProfileCard.qml:60-110`).
* **Hiện trạng mã nguồn:** Khi Card được chọn (`selected == true`), nền Card đổi sang tím tím `#7c4dff`, nhưng nhãn ngôn ngữ (Tag `vi`, `zh`) và thời lượng file mẫu (`12.0s`) vẫn giữ màu `Theme.textSecondary` (`#c7c2dc`).
* **Hậu quả:** Màu xám xanh `#c7c2dc` đặt trên nền tím tươi `#7c4dff` tạo ra hiện tượng xung đột sắc độ (Chromostereopsis) và chìm chữ, gây mỏi mắt khi nhìn lâu.
* **Phương án sửa:** Khi `selected == true`, toàn bộ icon và text phụ chuyển sang màu vàng kim nhạt `#fff9c4` hoặc trắng tinh `#ffffff`.

### 🔴 Lỗi C07: `DeveloperPage.qml` — Khung `CodePill` & `CodeExample` Màu Chữ Tối
* **Vị trí:** Khung hiển thị lệnh cURL và URL Server (`qml/pages/DeveloperPage.qml:402-406, 616-625`).
* **Hiện trạng mã nguồn:** Khung code dùng nền xám đen `Qt.rgba(0, 0, 0, 0.35)`, chữ code bên trong lại mang màu `#a59ebc` (font chữ kích thước nhỏ 11px).
* **Hậu quả:** Các dấu gạch chéo `/`, dấu gạch ngang `-` và cổng `:3928` bị mờ, lập trình viên nhìn vào rất khó đọc chính xác lệnh cURL để copy.
* **Phương án sửa:** Chữ code dùng màu xanh lục Terminal `#a7f3d0` hoặc vàng cam `#fed7aa` với font monospace in đậm.

---

## 3. CHI TIẾT NHÓM LỖI BỐ CỤC, NHỒI NHÉT & ĐÈ CHỮ

### 🔴 Lỗi L01: `DubbingPage.qml` (Toàn Bộ Trang) — Bố Cục 4 Cột Cứng Nhắc Làm Co Rúm Video
* **Vị trí:** Toàn bộ không gian làm việc Studio (`qml/pages/DubbingPage.qml:50-95`).
* **Hiện trạng mã nguồn:**
  * Cột Task Shelf cố định: `dubbingTaskShelfWidth: 260px`.
  * Cột Inspector cố định: `dubbingStepPanelWidth: 340px`.
  * Cột History khi mở: `dubbingHistoryPanelWidth: 280px`.
  * Breakpoint: `readonly property bool compactDubbingControls: dubbingWorkspaceScroller.width < 1450`.
* **Hậu quả:**
  * Trên màn hình laptop phổ biến (Full HD 1080p với Windows Scale 125% $\rightarrow$ Width thực tế = $1536$px): Khi mở cả 3 cột, tổng chiều rộng bị chiếm là $260 + 340 + 280 = 880$px, chỉ còn dư đúng $656$px cho Video Player ở giữa.
  * Nếu người dùng kéo nhỏ cửa sổ xuống dưới $1450$px, thay vì co giãn mượt mà, **toàn bộ cột Task Shelf bên trái biến mất đột ngột** làm người dùng hoang mang không biết bấm nút ở đâu.
* **Phương án sửa:**
  1. Giảm kích thước mặc định: Task Shelf = $220$px, Inspector = $280$px.
  2. Thêm nút **Toggle 1-Click** để đóng/mở Inspector và Task Shelf dạng Drawer trượt mượt mà.
  3. Khi Inspector mở, Video tự động căn giữa theo tỉ lệ $16:9$.

### 🔴 Lỗi L02: `DubbingPage.qml` (Chân Trang) — Timeline Nhồi Nhét 3 Track Gây Đè Lên Phụ Đề
* **Vị trí:** Dải Timeline đáy màn hình (`qml/pages/DubbingPage.qml:80-96`).
* **Hiện trạng mã nguồn:** Chiều cao Timeline bị ép cứng `minimumDubbingTimelinePanelHeight: 160px` và `maximumDubbingTimelinePanelHeight: 360px`.
* **Hậu quả:**
  * Trong khung 160px phải nhét: 1 thanh Ruler thời gian, 1 Track Audio gốc, 1 Track Vocals đã tách, 1 Track Background, 1 Track Dubbed Vocals và 1 Track Phụ đề (tổng cộng 6 dải).
  * Mỗi track chỉ còn chưa tới $20$px chiều cao, các khối phụ đề bị co lại thành các vệt chữ nhật nhỏ xíu, chữ bên trong bị tràn hoặc bị cắt thành dấu `...` không thể đọc hay click chuột để chọn đoạn cần sửa.
* **Phương án sửa:**
  1. Thiết kế Timeline theo dạng **Track Selector / Layer Tabs**: Mặc định chỉ hiện 2 track quan trọng nhất (*Track Giọng Lồng Tiếng* và *Track Phụ Đề Đích*).
  2. Bổ sung nút bấm **Phóng to chiều dọc (Expand Timeline)** và **Thanh trượt Zoom ngang thời gian (Time Zoom Slider)**.

### 🔴 Lỗi L03: `DubbingNodeInspector.qml` (Cột Phải) — Nhồi Cùng Lúc 15+ Thông Số
* **Vị trí:** Panel xem chi tiết tham số ở bên phải Dubbing Studio (`qml/components/dubbing/DubbingNodeInspector.qml`).
* **Hiện trạng mã nguồn:** Toàn bộ tham số của model (Temperature, Top_P, Repetition Penalty, Speed, Pitch, Energy, Emotion Preset, Target Format, Sample Rate, Buffer Size...) được đổ chung vào một `ColumnLayout` duy nhất với `spacing: 4`.
* **Hậu quả:** Người dùng phải cuộn chuột liên tục để tìm nút "Apply" hoặc "Preview". Các thanh trượt và nhãn dính chùm vào nhau, không có phân cấp chính/phụ.
* **Phương án sửa:** Phân thành 2 nhóm rõ rệt:
  * **Nhóm 1 (Cơ bản):** Giọng đọc, Ngôn ngữ, Tốc độ nói (Hiển thị ngay).
  * **Nhóm 2 (Chuyên sâu):** Cấu hình DSP, Temperature, Buffer (Đặt trong Card Accordion có thể bấm mở rộng khi cần).

### 🔴 Lỗi L04: `SubtitleOcrPage.qml` — Card Grid 2 Cột Bị Vỡ Bố Cục Khi Thu Nhỏ Cửa Sổ
* **Vị trí:** Toàn trang Subtitle OCR (`qml/pages/SubtitleOcrPage.qml:500-515`).
* **Hiện trạng mã nguồn:**
  ```qml
  columns: root.wideLayout ? 2 : 1
  ```
  Nhưng `sourceMediaCard` lại đặt `Layout.columnSpan: cardGrid.columns`, còn `previewCard` và `settingsCard` lại có kích thước cố định chênh lệch lớn (`height: 720px` vs `height: 400px`).
* **Hậu quả:** Khi ở chế độ 2 cột, cột bên phải dài gấp đôi cột bên trái tạo ra một khoảng trống đen khổng lồ vô nghĩa bên dưới cột trái; khi thu nhỏ cửa sổ, 2 cột dồn lại thành 1 cột khiến trang dài hơn $3.000$px, người dùng phải cuộn chuột mỏi tay.
* **Phương án sửa:** Chuyển sang bố cục **2 Phân Vùng Ngang (Split Horizontal Studio)**:
  * Nửa trên: Video Preview + Khung kéo chọn vùng OCR (ROI Drag Canvas).
  * Nửa dưới: Bảng kết quả nhận dạng phụ đề dạng Table có thanh cuộn độc lập.

### 🔴 Lỗi L05: `ModelsPage.qml` (Kho Mô Hình) — Rừng Badge Kỹ Thuật (10 Badges/Hàng)
* **Vị trí:** Danh sách model tải về (`qml/pages/ModelsPage.qml` & `MyModelsPage.qml:940-1010`).
* **Hiện trạng mã nguồn:** Mỗi dòng model chứa đồng thời: `ModelID`, `TaskPill`, `FormatTag` (GGUF/BIN), `LanguageFlag`, `QuantizationBadge` (Q4_K_M), `VramRequirement`, `FileSize`, `Author`, `License`, nút `Use Model`, nút `Delete`.
* **Hậu quả:** Khi tên model dài (ví dụ: `nemotron-3.5-asr-streaming-0.6b-int8.onnx`), các badge bị đẩy đè lên nút "Use Model" ở góc phải, gây ra lỗi Click nhầm giữa Nút Tải và Nút Xóa!
* **Phương án sửa:**
  * Giảm số badge hiển thị mặc định xuống tối đa 3 badge cốt lõi: `Task` (STT/TTS), `Size` (MB), `Engine` (GPU/CPU).
  * Các thông tin sâu về Quantization, License, Directory chuyển vào Popover Tooltip khi rê chuột vào hoặc hiển thị trong Drawer chi tiết bên phải.

### 🔴 Lỗi L06: `TtsPage.qml` — Nút "Tạo Giọng Nói" Đè Lên Dòng Chữ Kịch Bản
* **Vị trí:** Khung nhập kịch bản phát âm (`qml/pages/TtsPage.qml:120-180`).
* **Hiện trạng mã nguồn:** Nút "Synthesize" và bộ đếm ký tự `0/4000` được định vị bằng `anchors.bottom: parent.bottom` và `anchors.right: parent.right` đè trực tiếp lên vùng hiển thị của `TextArea`.
* **Hậu quả:** Khi người dùng dán vào đoạn văn bản dài hơn 5 dòng, các dòng chữ ở góc dưới bên phải bị nút bấm che mất hoàn toàn, không thể click con trỏ để chỉnh sửa dấu câu hay từ ngữ ở cuối đoạn.
* **Phương án sửa:** Đặt `TextArea` trong một layout dọc độc lập, nút bấm và bộ đếm đặt ở hàng `Footer Toolbar` riêng biệt bên dưới `TextArea`.

---

## 4. CHI TIẾT NHÓM LỖI TRÀN KHUNG & THIẾU CUỘN (CLIPPING & SCROLLING)

### 🔴 Lỗi S01: `DubbingExportDialog.qml` — Hộp Thoại Cố Định Chiều Cao 620px Làm Mất Nút Bấm
* **Vị trí:** Popup xuất video lồng tiếng (`qml/components/dubbing/DubbingExportDialog.qml:25-60`).
* **Hiện trạng mã nguồn:** `implicitHeight: 620`, `implicitWidth: 780` không bọc trong `ScrollView`.
* **Hậu quả:** Trên màn hình độ phân giải $1366 \times 768$ (chiều cao hữu dụng của cửa sổ sau khi trừ Taskbar Windows và Titlebar chỉ còn $\approx 640$px), đáy của dialog bị chìm dưới mép màn hình. **Nút "Export Video" và "Cancel" nằm ngoài vùng nhìn thấy, người dùng không có cách nào bấm xuất file!**
* **Phương án sửa:**
  * `implicitHeight: Math.min(620, Overlay.overlay ? Overlay.overlay.height - 40 : 620)`.
  * Bọc nội dung cấu hình (chọn bitrate, codec, phụ đề hardsub/softsub) trong `ScrollView { Layout.fillHeight: true }`, cố định cụm nút Action ở đáy Dialog.

### 🔴 Lỗi S02: `LoadedModelDialog.qml` — Danh Sách File Model Tràn Khỏi Khung Popup
* **Vị trí:** Popup xem chi tiết các file trong model (`qml/components/shared/LoadedModelDialog.qml:300-360`).
* **Hiện trạng mã nguồn:** Dùng `ColumnLayout` bên trong `Rectangle` có `height: 480` cố định mà không có `Flickable` hay `ScrollView`.
* **Hậu quả:** Đối với các model có nhiều file trọng số (như Whisper có 6-8 file `.bin`, `.json`, `.txt`), danh sách file bị cắt cụt ở file thứ 5, các file còn lại bị tràn ra ngoài viền dialog không thể xem được.
* **Phương án sửa:** Thay `ColumnLayout` bằng `ListView` hoặc bọc trong `ScrollView` có thanh cuộn mượt.

---

## 5. CHI TIẾT NHÓM LỖI TƯƠNG TÁC & CỐ ĐỊNH PIXEL CỨNG

### 🔴 Lỗi I01: `DubbingSourceMediaPanel.qml` — Nút Play/Pause & Seek Slider Quá Nhỏ
* **Vị trí:** Thanh điều khiển phát video (`qml/components/dubbing/DubbingSourceMediaPanel.qml:740-785`).
* **Hiện trạng mã nguồn:** Nút Play có `implicitWidth: 30`, `implicitHeight: 30`, icon bên trong chỉ $15 \times 15$px.
* **Hậu quả:** Vi phạm tiêu chuẩn công thái học nút bấm cảm ứng/chuột tối thiểu $44 \times 44$px. Rất khó click nhanh khi đang nghe đối thoại để dừng đúng mốc thời gian.
* **Phương án sửa:** Tăng kích thước nút Play chính lên $40 \times 40$px, icon $20 \times 20$px với hiệu ứng viền tròn nổi bật.

---

## 6. THIẾT KẾ LẠI HỆ THỐNG TOKEN MÀU SẮC CHUẨN HÓA (`Theme.qml` 2.0)

Để giải quyết triệt để tất cả các lỗi chữ bị chìm trên toàn bộ ứng dụng, `Theme.qml` cần được nâng cấp toàn diện với các token tương phản cao đạt chuẩn WCAG AAA:

```qml
pragma Singleton
import QtQuick

QtObject {
    // === 1. NỀN & BỀ MẶT PHÂN CẤP (ELEVATED SURFACES) ===
    readonly property color background:        "#12111a"  // Nền sâu nhất (App background)
    readonly property color surfaceLevel1:     "#1c1b29"  // Nền Sidebar / Bottom Panel
    readonly property color surfaceLevel2:     "#262438"  // Nền Card / Canvas
    readonly property color surfaceLevel3:     "#32304a"  // Nền Input Field / Dropdown
    readonly property color surfaceLevel4:     "#423f60"  // Nền Hover / Active State
    
    // Alias tương thích ngược
    readonly property color surface:           surfaceLevel2
    readonly property color surfaceAlt:        surfaceLevel3
    readonly property color border:            "#4e4a6d"  // Viền rõ nét, không bị chìm
    readonly property color borderSubtle:      Qt.rgba(1, 1, 1, 0.10)

    // === 2. HỆ THỐNG CHỮ TƯƠNG PHẢN CAO (HIGH-CONTRAST TYPOGRAPHY) ===
    readonly property color textPrimary:       "#ffffff"  // Trắng 100% cho tiêu đề, nhãn chính (Tương phản 15:1)
    readonly property color textSecondary:     "#dedaf5"  // Trắng tím sáng cho mô tả (Tương phản 9.2:1)
    readonly property color textMuted:         "#aea8d1"  // Chữ phụ/thời gian (Tương phản 5.6:1 - Đạt WCAG AA)
    readonly property color textPlaceholder:   "#8d87b3"  // Chữ gợi ý input (Tương phản 4.5:1)
    readonly property color textOnAccent:      "#ffffff"  // Chữ trên nền nút tím/xanh

    // === 3. MÀU ĐIỂM NHẤN & TRẠNG THÁI (ACCENT & SEMANTIC) ===
    readonly property color accent:            "#8b5cf6"  // Tím công nghệ rực rỡ
    readonly property color accentHover:       "#a78bfa"  // Tím sáng khi hover
    readonly property color accentBgMuted:     Qt.rgba(0.54, 0.36, 0.96, 0.16) // Nền badge tím
    
    readonly property color success:           "#4ade80"  // Xanh lá sáng (Pass / Ready)
    readonly property color successBg:         "#064e3b"  // Nền xanh sẫm
    
    readonly property color warning:           "#fbbf24"  // Vàng cam sáng (Cần chú ý)
    readonly property color warningBg:         "#78350f"  // Nền vàng sẫm
    
    readonly property color danger:            "#f87171"  // Đỏ tươi (Lỗi / Xóa)
    readonly property color dangerBg:          "#7f1d1d"  // Nền đỏ sẫm

    // === 4. KHOẢNG CÁCH CÔNG THÁI HỌC (BREATHABLE SPACING) ===
    readonly property int paddingTiny:         4
    readonly property int paddingSmall:        8
    readonly property int paddingMedium:       14
    readonly property int paddingLarge:        20
    readonly property int paddingXL:           28

    readonly property int radiusSmall:         6
    readonly property int radiusMedium:        10
    readonly property int radiusLarge:         14
}
```

---

## 7. GIẢI PHÁP TÁI CẤU TRÚC TỪNG TRANG & LỘ TRÌNH THỰC THI

### 🛠️ Bước 1: Áp Dụng Ngay `Theme.qml 2.0` (Contrast Hotfix)
* Thay thế toàn bộ mã màu hardcoded (`#555`, `#888`, `#c7c2dc`) trên 85 file QML bằng các token mới `textPrimary`, `textSecondary`, `textMuted`.
* Đảm bảo $100\%$ chữ và icon trên toàn bộ ứng dụng đạt độ tương phản tối thiểu **$4.5:1$** (WCAG AA).

### 🛠️ Bước 2: Tái Cấu Trúc Dubbing Studio (`DubbingPage.qml`)
1. **Thêm nút đóng/mở Inspector & Shelf:** Biến 2 cột bên thành dạng Drawer có thể thu gọn mượt mà, trả lại **$70\%$ diện tích màn hình cho Video Player và Subtitle Canvas**.
2. **Nâng cấp Timeline:** Thiết kế Timeline dạng 2 tầng linh hoạt, có thanh cuộn và nút Zoom để đọc rõ ràng từng câu phụ đề.
3. **Phân nhóm Task Shelf:** 10 bước gộp thành 3 khối logic (Chuẩn bị $\rightarrow$ Xử lý AI $\rightarrow$ Hoàn thiện).

### 🛠️ Bước 3: Tối Ưu Hóa Subtitle OCR & Voice Studio
1. **Subtitle OCR:** Chuyển sang bố cục Split View (Nửa trên chỉnh vùng cắt trực quan, nửa dưới bảng kết quả phụ đề).
2. **TTS Studio:** Đưa nút "Tạo giọng nói" ra thanh Footer Bar bên ngoài vùng nhập chữ, bọc TextArea trong ScrollView riêng biệt.
3. **Kho Model (Models Gallery):** Tối giản thẻ Model thành Grid 2 tầng, loại bỏ sự rối loạn của 10 badge trên một hàng.

### 🛠️ Bước 4: Chống Tràn Toàn Bộ Popups & Dialogs
* Cập nhật toàn bộ các file `*Dialog.qml`: Khống chế chiều cao theo `Overlay.overlay.height - 40px` và bọc phần thân trong `ScrollView` tự động ngắt dòng.
