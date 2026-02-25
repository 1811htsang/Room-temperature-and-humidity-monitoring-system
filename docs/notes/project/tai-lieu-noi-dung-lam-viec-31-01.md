# TÀI LIỆU NỘI DUNG LÀM VIỆC NGÀY 31/01/2026

## ĐÁNH GIÁ TIMELINE

**Thời gian thực hiện:** 26/01/2026 - 25/04/2026 (13 tuần dự kiến)
**Thời gian nghỉ bắt buộc:**

- Tết Nguyên Đán: 09/02/2026 - 22/02/2026 (2 tuần)
- Thi giữa kì: 06/04/2026 - 19/04/2026 (2 tuần)
**Thời gian nghỉ tùy chọn:**
- 02/02/2026 - 08/02/2026 (1 tuần)
**Nhận xét:**
- Tổng thời gian làm việc thực tế: 8 tuần
- Chưa tính tới các thời gian nghỉ của các thành viên bởi lý do bất khả kháng.

---

## NỘI DUNG LÀM VIỆC

### System Context

| Phần cứng      | Phân loại | Có sẵn                               | Ghi chú                                                                   |
| -------------- | --------- | ------------------------------------ | ------------------------------------------------------------------------- |
| ESP32-WROOM-32 | MCU       | Đã có (Sang)                         | Sử dụng framework **ESP-IDF** với VSCode                                  |
| SHT30          | Sensor    | Chưa có                              | Giao tiếp **I2C** - dùng cho thu thập nhiệt độ & độ ẩm                    |
| DS3231         | Sensor    | Đã có (cần kiểm tra lại pin LIR2032) | Giao tiếp **I2C** - dùng cho hiển thị thời gian & nhiệt độ (**tùy chọn**) |
| LCD 16X2       | Display   | Đã có (cần kiểm tra lại linh kiện)   | Giao tiếp **I2C**                                                         |

1. **Hệ thống quản lý:**
    - **WDT (Watchdog Timer):** Bắt buộc để tự động Reset khi treo code.
    - **RCC (Reset & Clock Control):** Cấu hình tần số hoạt động để tối ưu năng lượng.
    - **I2C (Inter-integrated Circuit):** Giao tiếp với SHT30 và LCD.
2. **Giao thức truyền thông:**

- **MQTT:** Gửi dữ liệu cảm biến lên Cloud (tùy chọn).
- **HTTP:** Dùng làm dashboard web (tùy chọn).

---

## PHÂN CHIA NHIỆM VỤ (MAJOR & MINOR TASKS)

### 1. Nhóm Firmware & Driver (03 người)

- **Major Task:** Phát triển tầng HAL (Hardware Abstraction Layer) và Driver.

- **Minor Tasks:**
  - Viết thư viện I2C cho SHT30 (đọc raw data, chuyển đổi công thức).
  - Viết thư viện I2C cho LCD (hiển thị ký tự, tạo thanh progress bar).
  - Quản lý lỗi (Error Handling):
    - Xử lý khi rút dây cảm biến hoặc mất kết nối I2C.
    - Xử lý khi không khởi động được clock.
  - Thiết lập WDT (Watchdog Timer) để tự động reset khi chương trình bị treo (**Lưu ý rằng trong ESP32 khi khởi động sẽ kích hoạt sẵn RWDT & MWDT1 để bảo vệ sẵn nên có thể thiết kế để chỉnh sửa cấu hình hoạt động của WDT như 1 điểm cộng**).
  - Cấu hình RCC và các chế độ Power Saving (Light sleep).

### 2. Nhóm Phần cứng & PCB (03 người)

- **Major Task:** Thiết kế vật lý và cung cấp năng lượng.

- **Minor Tasks:**
  - Vẽ sơ đồ nguyên lý (Schematic) và Layout mạch in (PCB).
  - Tính toán nguồn điện (Pin 18650, mạch sạc TP4056, IC ổn áp 3.3V).
  - Thi công vỏ hộp (in 3D hoặc hộp nhựa kỹ thuật) đảm bảo đối lưu không khí cho cảm biến.
  - Hàn mạch và kiểm thử hoạt động cơ bản.

---

## TIMESTAMPS

1. **Giai đoạn 1: Phân tích & Prototype (26/01 - 15/02)**
    - *Timestamp 15/02:* Hoàn thành cấu trúc thư mục dự án với đầy đủ các thành phần sau:
    - Tài liệu yêu cầu (Requirement Document)
    - Tài liệu thiết kế (Design Document)
    - Tài liệu kiểm thử (Test Document - nếu có thời gian)
    - Ghi chú làm việc (Meeting Notes)
    - Mã nguồn (Source Code)
    - Tài liệu báo cáo (Report Document)
    - Nhánh làm việc trên Git (Git Branches)
    - Các styling standard và lưu ý khi code (Coding Standards)
    - *Timestamp 01/03:* Hoàn thành đọc dữ liệu SHT30 trên Breadboard.
2. **Giai đoạn 2: Phát triển Module (16/02 - 15/03)**
    - *Timestamp 01/03:* Hoàn thành driver hiển thị LCD 16x2.
    - *Timestamp 09/03:* Tích hợp thành công đọc dữ liệu SHT30 và hiển thị lên LCD.
    - *Timestamp 15/03:* Có bản vẽ PCB hoàn thiện để đặt gia công.
3. **Giai đoạn 3: Tích hợp & Hệ thống (16/03 - 05/04)**
    - *Timestamp 30/03:* Lắp ráp mạch PCB hoàn thiện.
4. **Giai đoạn 4: Kiểm thử & Hoàn thiện (06/04 - 25/04)**
    - *Timestamp 02/04:* Chạy Stress-test (vận hành liên tục 4 ngày).
    - *Timestamp 25/04:* Hoàn thiện báo cáo đồ án và video demo.

---

### IV. TÀI LIỆU SỬ DỤNG

1. **Datasheet:** ESP32 Technical Reference Manual, SHT30 Datasheet.
2. **Framework:** ESP-IDF Programming Guide (v5.x).
3. **Tài liệu yêu cầu:** File "BEA-Requirement-Engineering.pdf" (đã có) để đối soát 4C.
4. **Công cụ quản lý:**
    - **GitHub:** Quản lý mã nguồn.
    - **Notion:** Quản lý tiến độ task (Major/Minor).
    - **EasyEDA/Altium:** Thiết kế mạch.
