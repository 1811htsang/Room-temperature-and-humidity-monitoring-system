# Giới thiệu

Hệ thống giám sát nhiệt độ &amp; độ ẩm sử dụng ESP32 (ESP-IDF) và SHT30, DS3231. Trong đó ứng dụng Core uEDP để triển khai mô hình lập trình hướng sự kiện (event-driven programming) nhằm tối ưu hiệu suất và khả năng mở rộng của hệ thống.

## Tính năng chính

- Đọc dữ liệu nhiệt độ &amp; độ ẩm từ cảm biến SHT30 qua giao tiếp I2C
- Hiển thị dữ liệu trên màn hình LCD 16x2 với giao tiếp I2C
- Đồng bộ thời gian thực với module RTC DS3231
- Cảnh báo khi nhiệt độ hoặc độ ẩm vượt ngưỡng 30 độ C

## Các yêu cầu ngầm định từ stakeholder

- Có thiết kế mạch
- Hệ thống có thể chạy ổn định

## Đánh giá timeline

**Thời gian thực hiện:** 26/01/2026 - 25/05/2026 (4 tháng)

**Thời gian nghỉ bắt buộc:**

- Tết Nguyên Đán: 09/02/2026 - 22/02/2026 (2 tuần)
- Thi giữa kì: 06/04/2026 - 19/04/2026 (2 tuần)

**Thời gian nghỉ tùy chọn:**

- 02/02/2026 - 08/02/2026 (1 tuần)

## Công cụ phát triển

- IDE: Visual Studio Code với ESP-IDF Extension
- Version Control: Git &amp; GitHub
- Build System: CMake
- Progress Tracking: Notion & GitHub
- Hardware Prototyping: Breadboard, Jumper Wires, Multimeter, Oscilloscope
- Hardware Soldering: Total soldering iron, soldering wire, flux, desoldering pump
- Programming Model: Event-Driven Programming (uEDP)

## Linh kiện phần cứng

- ESP32-WROOM-32 phiên bản 30 chân
- Cảm biến nhiệt độ &amp; độ ẩm SHT30
- Cảm biến thời gian &amp; nhiệt độ DS3231
- LCD 16x2 với giao tiếp I2C

## Thành viên tham gia

1. Huỳnh Thanh Sang
2. Nguyễn Hồng Giang
3. Đoàn Cao Thanh Đức
4. Trần Trung Kiên
5. Nguyễn Văn Phú
6. Hồng Minh Quang

## Cấu trúc thư mục

Dưới đây là cấu trúc thư mục chính của dự án:

```file
├── LICENSE                   # Tập tin chứa thông tin bản quyền của dự án
├── .gitignore                # Tập tin cấu hình Git để bỏ qua các tập tin/thư mục không cần thiết
├── docs/                     # Thư mục chứa các tài liệu liên quan đến dự án
│   ├── notes/                # Thư mục chứa các ghi chú phát triển
│   │   ├── coding/           # Thư mục chứa các ghi chú trong quá trình thiết kế mã nguồn và làm việc
│   │   └── project/          # Thư mục chứa các tài liệu liên quan đến quản lý dự án
│   └── references/           # Thư mục chứa các tài liệu tham khảo
│       ├── coding/           # Thư mục chứa các tài liệu về quy cách thiết kế mã nguồn và làm việc
│       └── esp32-specific/   # Thư mục chứa các tài liệu liên quan đến ESP32
├── artifactspace/            # Thư mục chứa các artifact con của dự án, mỗi artifact có README riêng
└── README.md                 # Đây là tập tin bạn đang đọc
```
