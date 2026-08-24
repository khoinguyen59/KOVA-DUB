# Báo Cáo Kiểm Toán Chuyên Sâu Trải Nghiệm Người Dùng (GUI UX, Usability & Ergonomics Audit)

> **Mục tiêu kiểm toán:** Phân tích toàn bộ các điểm gãy trải nghiệm (UX Friction), gánh nặng nhận thức (Cognitive Load), sai lệch mô hình tư duy (Mental Model Disconnect) và vi phạm 10 nguyên lý công thái học Nielsen Norman Heuristics trên giao diện `LA-Studio`.

---

## 🧭 MA TRẬN 5 TRỤ CỘT TRẢI NGHIỆM NGƯỜI DÙNG (UX PILLARS)

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                        LA-STUDIO UX & USABILITY AUDIT MATRIX                           │
├────────────────────────────────────────┬───────────────────────────────────────────────┤
│ 1. Mental Model & Info Architecture    │ 4 Điểm gãy (10-Step Ladder, CPU/GPU Confusion)│
│ 2. Feedback & Visibility of Status     │ 4 Điểm gãy (Missing ETA, Raw Log Jargon)      │
│ 3. Cognitive Load & Form Ergonomics    │ 4 Điểm gãy (Hex code inputs, AI parameter dump)│
│ 4. Error Prevention & Recovery         │ 3 Điểm gãy (No soft-undo, Disabled without tip)│
│ 5. Workflow Efficiency & Manipulation  │ 4 Điểm gãy (Missing shortcuts, No 1-click test)│
├────────────────────────────────────────┼───────────────────────────────────────────────┤
│ TỔNG CỘNG                              │ 19 ĐIỂM GÃY TRẢI NGHIỆM UX NGHIÊM TRỌNG       │
└────────────────────────────────────────┴───────────────────────────────────────────────┘
```

---

## 🧩 1. MÔ HÌNH TƯ DUY & KIẾN TRÚC THÔNG TIN (MENTAL MODEL & INFO ARCHITECTURE)

### 🔴 UX-M01: Thang 10 Bước Lồng Tiếng Dồn Dập Khiến Người Dùng Bị Choáng (Cognitive Overload)
* **Vị trí:** Cột Task Shelf bên trái (`qml/components/dubbing/DubbingWorkflowStep.qml`).
* **Vấn đề UX:** Toàn bộ 10 bước tác vụ (Tách âm $\rightarrow$ STT $\rightarrow$ OCR $\rightarrow$ Gộp thoại $\rightarrow$ Dịch $\rightarrow$ Nhân bản giọng $\rightarrow$ Mix âm $\rightarrow$ Chỉnh khớp $\rightarrow$ In phụ đề $\rightarrow$ Xuất video) được xếp thành một danh sách dọc kéo dài liên tục $10$ ô số thứ tự.
* **Tác động tâm lý:**
  * Người dùng mới mở app nhìn thấy 10 bước sẽ cảm thấy quy trình quá phức tạp và ngột ngạt.
  * Không phân biệt được bước nào là **Tự động chạy ngầm (Automatic AI)** và bước nào cần **Người dùng can thiệp kiểm duyệt thủ công (Human-in-the-loop Review)**.
* **Giải pháp UX:** Gom nhóm 10 bước thành **3 Giai Đoạn Trực Quan (Phased Stepper Accordion)**:
  1. 📥 **Giai đoạn 1: Nạp & Trích xuất** (Tách âm + Nhận dạng STT/OCR) — *Chạy 100% tự động*.
  2. ✍️ **Giai đoạn 2: Biên tập & Dịch thuật** (Gộp thoại + Dịch tiếng Việt) — *Trọng tâm kiểm duyệt của người dùng*.
  3. 🎙️ **Giai đoạn 3: Lồng tiếng & Xuất bản** (Nhân bản giọng + Căn chỉnh + Hardsub Video) — *Chạy tự động & Xuất video*.

---

### 🔴 UX-M02: Nhập Nhằng Giữa Chạy Cục Bộ (Local CPU) Và Điện Toán Đám Mây (Colab GPU)
* **Vị trí:** Các trang Dubbing, Subtitle OCR, Voice Cloning, Text to Speech.
* **Vấn đề UX:** Người dùng không thể nhận biết bằng mắt thường rằng khi bấm nút "Run" thì tác vụ sẽ làm nóng máy/đơ máy tính cá nhân (CPU) hay gửi lên máy chủ GPU Colab.
* **Hậu quả:** Nhiều người dùng có máy tính yếu vô tình bấm chạy Whisper Large hoặc PP-OCR ở chế độ Local CPU khiến máy tính bị đơ giật $100\%$ CPU mà không hiểu tại sao.
* **Giải pháp UX:**
  * Bổ sung **Huy hiệu Nhận thức Tuyến Thực thi (Execution Route Badges)** ngay trên nút bấm Action:
    - `[⚡ Cloud GPU · T4]` (Nút màu tím/xanh ngọc) khi đang kết nối Colab.
    - `[💻 Local CPU · Offline]` (Nút màu xám viền cam) khi chạy cục bộ kèm cảnh báo tốc độ.
  * Thêm công tắc chuyển đổi nhanh 1-Click: *"Ưu tiên GPU Đám mây"* ở góc trên thanh công cụ.

---

### 🔴 UX-M03: Trang Chào Mừng (WelcomePage) Thiếu Lời Kêu Gọi Hành Động (Call To Action - CTA)
* **Vị trí:** Trang khởi động ứng dụng (`qml/pages/WelcomePage.qml`).
* **Vấn đề UX:** Trang chủ hiện lên như một bảng tin tĩnh liệt kê các tính năng, nhưng không có một khu vực trung tâm nổi bật để người dùng bắt đầu ngay công việc.
* **Giải pháp UX:**
  * Bổ sung **Vùng Thả Video Khổng Lồ (Hero Dropzone)** ngay giữa trang:  
    *"Kéo & thả video vào đây để Bắt đầu Lồng tiếng Tự động"* kèm nút *"Mở Video Mẫu 15s để trải nghiệm nhanh"*.

---

### 🔴 UX-M04: Xáo Trộn Giữa "Duyệt Danh Sách" Và "Biểu Mẫu Tạo Mới" Trong Thư Viện Giọng
* **Vị trí:** Hộp thoại chọn giọng (`qml/components/shared/VoiceLibraryDialog.qml:301-350`).
* **Vấn đề UX:** Cửa sổ chia làm 2 cột, nhưng ở đáy cột bên phải lại nhồi luôn một Form nhập liệu "Thêm/Sửa giọng mới" nằm đè bên dưới danh sách giọng đã lưu. Danh sách giọng bị bóp ngắn lại chỉ còn thấy được 2-3 mục.
* **Giải pháp UX:** Tách biệt rõ ràng 2 chế độ:
  * Mặc định là **Chế độ Chọn Giọng (Browser Mode)**: Danh sách chiếm trọn $100\%$ chiều cao với các Card giọng có nút Play nghe thử.
  * Chỉ khi bấm nút `+ Thêm giọng mới` thì mới mở Drawer trượt hoặc chuyển sang tab tạo mới.

---

## 📡 2. PHẢN HỒI TRẠNG THÁI & THANH TIẾN TRÌNH (FEEDBACK & SYSTEM VISIBILITY)

### 🔴 UX-F01: Xử Lý Video Dài (400+ Câu) Thiếu Ước Tính Thời Gian Còn Lại (ETA)
* **Vị trí:** Tiến trình tổng hợp giọng lồng tiếng và OCR (`DubbingSynthesisJob.qml`, `SubtitleOcrPage.qml`).
* **Vấn đề UX:** Thanh tiến trình chỉ hiện một con số phần trăm khô khan (ví dụ: `42%`) hoặc chữ `Processing...`. Khi xử lý video 15 phút (mất 5-10 phút), người dùng nhìn thấy phần trăm đứng yên trong 30 giây sẽ nghĩ rằng ứng dụng đã bị treo (Hung/Deadlock) và nóng vội tắt ứng dụng!
* **Giải pháp UX:** Cung cấp thông tin tiến độ 3 thành phần:
  * **Chỉ số phân đoạn:** `Đang tạo câu 182 / 473 (38%)`
  * **Thời gian ước tính:** `Còn lại khoảng 2 phút 15 giây (Tốc độ: 3.2 câu/giây)`
  * **Xem trước thời gian thực:** Hiển thị câu thoại tiếng Việt đang được AI nói ngay lúc đó.

---

### 🔴 UX-F02: Panel System Logs Phía Dưới Dùng Ngôn Ngữ Kỹ Thuật Gây Hoang Mang
* **Vị trí:** Thanh nhật ký đáy màn hình (`qml/components/BottomLogPanel.qml:250-270`).
* **Vấn đề UX:** Bảng log đổ ra các dòng thông báo thô của C++/FFmpeg/Python (`QMetaObject::invokeMethod`, `avformat_find_stream_info`, `chunk_offset_0x3f`). Người dùng sáng tạo nội dung (Creator) nhìn thấy các dòng chữ đỏ/vàng này sẽ tưởng phần mềm bị lỗi nghiêm trọng.
* **Giải pháp UX:**
  * Bổ sung chế độ lọc **Chế độ Người dùng (User-Friendly Milestones)**:
    - 🟢 *10:14:02 — Đã tách xong âm thanh giọng nói và nhạc nền (158 MB)*
    - 🟢 *10:14:45 — Đã nhận dạng xong 473 câu thoại tiếng Trung*
    - 🟢 *10:15:30 — Đã hoàn thành dịch toàn bộ kịch bản sang tiếng Việt*
  * Chỉ hiện log kỹ thuật sâu (Verbose Debug) khi người dùng bật tab *"Dành cho Lập trình viên"*.

---

### 🔴 UX-F03: Thanh Tua Video & Timeline Không Có Tooltip Xem Trước Mốc Thời Gian
* **Vị trí:** Thanh trượt tua video (`DubbingSourceMediaPanel.qml:760-785`) và Timeline (`DubbingPage.qml`).
* **Vấn đề UX:** Khi người dùng rê chuột dọc theo thanh tua hoặc timeline, không có bong bóng thời gian (Time Tooltip `04:25.100`) đi theo con trỏ chuột, buộc người dùng phải click bừa để dò xem đoạn đó là phút thứ mấy.
* **Giải pháp UX:** Thêm Hover Thumbnail/Time Bubble: Khi di chuột trên thanh trượt, một ô nhỏ hiện chính xác thời gian tại vị trí con trỏ chuột.

---

## 🧠 3. GIẢM TẢI NHẬN THỨC & CÔNG THÁI HỌC BIỂU MẪU (COGNITIVE LOAD & FORM ERGONOMICS)

### 🔴 UX-C01: Bắt Người Dùng Gõ Mã Hex Bằng Tay Khi Chỉnh Màu Phụ Đề
* **Vị trí:** Hộp thoại chỉnh kiểu dáng phụ đề (`DubbingSubtitleEditor.qml:30-41`).
* **Vấn đề UX:** Màu chữ (`textColor`), màu viền (`outlineColor`), màu bóng (`shadowColor`), màu nền (`backgroundColor`) đều là các ô `TextField` bắt người dùng phải nhớ và tự gõ mã Hex (ví dụ `#FFFF00`, `#000000`).
* **Hậu quả:** Người dùng bình thường không biết mã Hex màu vàng là gì, phải mở trình duyệt web tra mã màu rồi copy-paste vào app.
* **Giải pháp UX:**
  * Bổ sung **Bảng Màu Nhanh 1-Click (Preset Color Swatches)** gồm các màu phụ đề chuẩn điện ảnh: *Trắng Tinh khôi, Vàng Rực rỡ, Vàng Kim viền Đen, Xanh Lục Neon, Đen Khói*.
  * Tích hợp hộp thoại chọn màu trực quan `ColorDialog` khi click vào ô màu.

---

### 🔴 UX-C02: Phơi Bày Rừng Tham Số Kỹ Thuật AI Đầy Đánh Đố
* **Vị trí:** Panel xem chi tiết tham số (`DubbingNodeInspector.qml:100-250`).
* **Vấn đề UX:** Trưng bày trực tiếp các thông số trừu tượng: `Temperature = 0.3`, `Top_P = 0.85`, `Repetition Penalty = 1.15`, `FFT Hop Size = 512`, `Binarize Threshold = 0.5`.
* **Giải pháp UX:**
  * Ẩn các thông số toán học này vào mục *"Tùy chỉnh nâng cao"*.
  * Cung cấp các **Phong Cách Lồng Tiếng Có Sẵn (Voice Style Presets)**:
    - 🎭 *Kể chuyện truyền cảm* (Tốc độ 0.95x, Giọng trầm ấm, Giữ nhịp tự nhiên)
    - ⚡ *Review phim nhanh / TikTok* (Tốc độ 1.15x, Giọng rõ ràng, Cắt ngắn khoảng lặng)
    - 📰 *Đọc tin tức thời sự* (Tốc độ 1.0x, Giọng chuẩn mực, Rõ từng chữ)

---

### 🔴 UX-C03: Cài Đặt Phần Cứng Thiếu Nút "Tự Động Phát Hiện Cấu Hình Tối Ưu"
* **Vị trí:** Tab Cài đặt phần cứng (`HardwareSettingsTab.qml`).
* **Vấn đề UX:** Người dùng phải tự chọn số luồng CPU (Thread count), GPU device index, RAM limit mà không biết máy tính của mình mạnh đến mức nào.
* **Giải pháp UX:** Thêm nút nổi bật **"⚡ Tối Ưu Hóa Tự Động Theo Phần Cứng Máy Tính"** (Tự động đo dung lượng RAM/VRAM thực tế và thiết lập thông số tối ưu chỉ với 1 click).

---

## 🛡️ 4. PHÒNG NGỪA VÀ PHỤC HỒI SAU LỖI (ERROR PREVENTION & RECOVERY)

### 🔴 UX-E01: Xóa Phụ Đề / Mẫu Giọng Thiếu Cơ Chế Hoàn Tác Mềm (Soft-Undo Toast)
* **Vị trí:** Bảng danh sách phụ đề và thư viện mẫu giọng.
* **Vấn đề UX:** Khi người dùng bấm xóa một đoạn phụ đề hoặc một mẫu giọng nhân bản, ứng dụng hoặc xóa ngay lập tức hoặc bật popup cảnh báo gián đoạn công việc. Nếu lỡ tay bấm nhầm xóa mất câu thoại đã dịch kỹ càng thì không thể khôi phục lại.
* **Giải pháp UX:**
  * Áp dụng nguyên lý **Undo Toast**: Khi xóa, câu thoại tạm ẩn và hiện thanh thông báo nổi ở góc dưới:  
    *"Đã xóa câu thoại 45. [Hoàn tác (Ctrl+Z)]"* (tự biến mất sau 6 giây).

---

### 🔴 UX-E02: Nút Bị Vô Hiệu Hóa (Disabled) Nhưng Không Giải Thích Lý Do
* **Vị trí:** Nút "Run Dubbing", nút "Import Link", nút "Synthesize".
* **Vấn đề UX:** Khi thiếu điều kiện (chưa nhập URL, chưa chọn giọng đọc), nút bấm chuyển sang màu xám mờ và không thể bấm được. Người dùng click vào không thấy phản hồi gì và không biết mình đang thiếu bước nào.
* **Giải pháp UX:** Khi rê chuột qua nút bị disable, hiển thị Tooltip hướng dẫn cụ thể:  
  *"⚠️ Vui lòng chọn một Mẫu Giọng Đọc ở Bước 6 trước khi bắt đầu tạo giọng lồng tiếng."*

---

## ⚡ 5. TỐI ƯU HÓA THAO TÁC & HIỆU SUẤT LÀM VIỆC (WORKFLOW EFFICIENCY)

### 🔴 UX-W01: Khung Sửa Phụ Đề Nhanh Thiếu Phím Tắt & Nút Chuyển Câu Liên Tục
* **Vị trí:** Cửa sổ sửa phụ đề tại chỗ (`DubbingInlineSubtitleEditor.qml:70-115`).
* **Vấn đề UX:**
  * Người dùng gõ xong chữ sửa, bấm `Enter` thì con trỏ chỉ xuống dòng chứ không lưu. Phải dùng chuột click nút "Save subtitle".
  * Không có nút `Câu tiếp theo [Tab]` / `Câu trước đó [Shift+Tab]`. Muốn sửa câu tiếp theo, người dùng phải đóng popup, rê chuột trên video tìm câu tiếp theo và click lại từ đầu!
* **Giải pháp UX:**
  * `Enter` hoặc `Ctrl+Enter` $\rightarrow$ Lưu ngay lập tức.
  * `Tab` $\rightarrow$ Lưu câu hiện tại và tự động chuyển sang sửa câu tiếp theo.
  * `Space` $\rightarrow$ Phát thử âm thanh của riêng câu đang sửa để nghe kiểm tra.

---

### 🔴 UX-W02: Chọn Giọng Đọc Không Có Nút "Nghe Thử 1-Click"
* **Vị trí:** Kho danh sách mẫu giọng (`VoiceLibraryDialog.qml` & `DubbingPage.qml`).
* **Vấn đề UX:** Người dùng muốn biết giọng của Lão Vương, Tiểu Mỹ hay Giọng Nam Trầm nghe như thế nào phải chọn giọng đó, gõ chữ, bấm tạo audio rồi đợi vài giây mới nghe được.
* **Giải pháp UX:** Thêm một nút tròn nhỏ **▶ (Play Sample)** ngay trên thẻ tên của từng giọng để người dùng bấm nghe thử ngay lập tức trong 2 giây mà không cần cấu hình gì thêm.

---

### 🔴 UX-W03: Thanh System Logs 35px Đáy Màn Hình Chiếm Chỗ Vĩnh Viễn
* **Vị trí:** Thanh log chân trang (`BottomLogPanel.qml:58`).
* **Vấn đề UX:** Dù đã thu gọn (collapsed), thanh log vẫn chiếm cố định $35$px đáy màn hình xuyên suốt tất cả các trang, làm giảm không gian hiển thị của Timeline và Video.
* **Giải pháp UX:** Tích hợp nút Logs thành một icon nhỏ ở góc phải thanh Status Bar hoặc cho phép ẩn hoàn toàn (`height: 0`) khi không có lỗi.

---

## 🚀 TỔNG HỢP DANH MỤC 19 ĐIỂM GÃY UX CẦN SỬA CHỮA

| Nhóm UX | Mã lỗi | Vấn đề trải nghiệm | Giải pháp khắc phục nhanh |
| :--- | :--- | :--- | :--- |
| **Mô hình tư duy** | **UX-M01** | Thang 10 bước dồn dập gây ngột ngạt | Gom thành 3 Giai đoạn (Ingest $\rightarrow$ Edit $\rightarrow$ Publish) |
| | **UX-M02** | Nhập nhằng giữa chạy Local CPU và Colab GPU | Thêm Huy hiệu `[⚡ Cloud GPU]` / `[💻 Local CPU]` |
| | **UX-M03** | Trang chủ thiếu Call To Action nổi bật | Thêm Hero Dropzone kéo thả video trực tiếp |
| | **UX-M04** | Thư viện giọng nhồi form tạo mới vào danh sách | Tách biệt chế độ Duyệt (Browse) và Tạo mới |
| **Phản hồi trạng thái**| **UX-F01** | Xử lý 400+ câu thiếu thời gian ước tính (ETA) | Hiện `Câu 182/473 · Còn lại 2p15s · Câu đang đọc` |
| | **UX-F02** | Log đáy màn hình toàn lỗi code/jargon kỹ thuật | Lọc hiển thị thông điệp tiến trình dễ hiểu |
| | **UX-F03** | Thanh tua video không có tooltip xem trước giờ | Thêm bóng thời gian bám theo con trỏ chuột |
| | **UX-F04** | Chuyển tab / tải model thiếu Skeleton loading | Bổ sung hiệu ứng mờ và Skeleton placeholder |
| **Giảm tải nhận thức** | **UX-C01** | Bắt gõ mã màu Hex (`#FFFFFF`) bằng tay | Bảng màu nhanh chuẩn điện ảnh + ColorPicker |
| | **UX-C02** | Trưng bày rừng thông số AI khó hiểu | Preset phong cách: Kể chuyện, Review phim, Tin tức |
| | **UX-C03** | Cài đặt phần cứng toàn spinbox khô khan | Nút 1-Click: "⚡ Tự động tối ưu theo phần cứng" |
| | **UX-C04** | Tên model để nguyên tên file dài loằng ngoằng | Chuẩn hóa tên thương mại thân thiện, dễ nhớ |
| **Phòng ngừa lỗi** | **UX-E01** | Xóa câu thoại / mẫu giọng không có hoàn tác | Toast thông báo kèm nút `[Hoàn tác (Ctrl+Z)]` |
| | **UX-E02** | Nút disable không giải thích tại sao không bấm được | Tooltip giải thích rõ điều kiện còn thiếu |
| | **UX-E03** | Mất mạng Colab chỉ báo đỏ "Connection Refused" | Modal hướng dẫn: "Kiểm tra URL hoặc Kết nối lại" |
| **Hiệu quả thao tác** | **UX-W01** | Sửa phụ đề không có phím tắt Enter/Tab/Space | `Enter` để Lưu, `Tab` sang câu sau, `Space` nghe thử |
| | **UX-W02** | Timeline kéo phụ đề thiếu hút dính thông minh | Magnetic snapping bám theo ranh giới câu nói |
| | **UX-W03** | Chọn giọng thiếu nút "Nghe thử mẫu 1-Click" | Nút tròn Play mẫu 2s trực tiếp trên thẻ giọng |
| | **UX-W04** | Thanh Log 35px chiếm đáy màn hình vĩnh viễn | Thu gọn thành icon trạng thái tinh tế ở góc phải |
