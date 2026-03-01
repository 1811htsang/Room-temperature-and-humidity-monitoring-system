# Ghi chú về giao thức I2C trên ESP32 cho cảm biến SHT30

## Thiết kế phần cứng cho I2C trên ESP32

- Chân GPIO: ESP32 có khả năng gán chức năng I2C cho bất kỳ chân GPIO nào thông qua ma trận IO (IO MUX). Tuy nhiên, các chân mặc định thường là GPIO 21 (SDA) và GPIO 22 (SCL).
- Điện trở kéo lên (Pull-up resistors): Giao thức I2C yêu cầu điện trở kéo lên trên cả hai đường SDA và SCL. Mặc dù ESP32 có điện trở kéo lên nội bộ (khoảng 45kΩ), nhưng chúng thường quá lớn để đạt tốc độ cao. Khuyến nghị sử dụng điện trở kéo lên bên ngoài 2.4kΩ đến 10kΩ để đảm bảo tín hiệu ổn định.
- Chế độ hoạt động: ESP32 hỗ trợ cả Master mode (Điều khiển SHT30) và Slave mode.

## Các tiêu chuẩn giao thức I2C hỗ trợ

ESP32 tuân thủ các tiêu chuẩn I2C sau:

- Standard-mode: Tốc độ lên đến 100 Kbit/s.
- Fast-mode: Tốc độ lên đến 400 Kbit/s.
- Fast-mode Plus (Fm+): Có thể hỗ trợ lên đến 1 Mbit/s (tùy thuộc vào thiết kế phần cứng và tải dung kháng trên bus).
- Địa chỉ: Hỗ trợ địa chỉ 7-bit (SHT30 dùng 0x44 hoặc 0x45) và địa chỉ 10-bit.

## Các thanh ghi sử dụng cho thực thi

Trong framework ESP-IDF, lập trình viên không cần thao tác trực tiếp với thanh ghi nhờ tầng trừu tượng hóa (HAL). Tuy nhiên, các nhóm thanh ghi chính mà Driver quản lý bao gồm:

- I2C_DATA_REG: Thanh ghi dữ liệu FIFO (truyền/nhận).
- I2C_CTR_REG: Điều khiển hoạt động của bộ I2C (Start, Stop, Ack).
- I2C_SR_REG: Trạng thái của bộ I2C (Bận, hoàn thành, lỗi).
- I2C_INT_RAW_REG: Quản lý ngắt (khi truyền xong hoặc gặp lỗi).

## Thư viện API hỗ trợ trên ESP-IDF

Tùy vào phiên bản ESP-IDF bạn đang dùng, có hai cách tiếp cận:

- Legacy Driver (Cũ): `#include "driver/i2c.h"` (Vẫn phổ biến, ổn định).
- New Master Driver (Mới từ v5.2): `#include "driver/i2c_master.h"` (Khuyên dùng cho các dự án mới vì quản lý tài nguyên tốt hơn và thread-safe).

## Các hàm khởi tạo tham số hoạt động (Master Mode)

Để khởi tạo I2C Master điều khiển SHT30, quy trình sử dụng các hàm sau:

| Hàm (Legacy API) | Chức năng |
| :--- | :--- |
| `i2c_param_config()` | Thiết lập cấu hình: chân SDA/SCL, chế độ Master/Slave, tốc độ (Clock speed). |
| `i2c_driver_install()` | Cài đặt Driver vào bộ nhớ, thiết lập kích thước Buffer và đăng ký ngắt. |

*Nếu sử dụng New API (v5.x), bạn sẽ dùng:* `i2c_new_master_bus()` và `i2c_master_bus_add_device()`.

## Các hàm gửi dữ liệu cho cảm biến (Write)

Đối với SHT30, bạn cần gửi lệnh (Command - 2 bytes) để bắt đầu đo:

| Hàm (Legacy API) | Chức năng |
| :--- | :--- |
| `i2c_master_write_to_device()` | Gửi trực tiếp một mảng dữ liệu đến địa chỉ của SHT30. |
| `i2c_master_start()` | (Dùng trong manual mode) Tạo điều kiện START. |
| `i2c_master_write_byte()` | Gửi từng byte dữ liệu. |

## Các hàm đọc dữ liệu từ cảm biến (Read)

Sau khi gửi lệnh đo, SHT30 trả về 6 bytes (2 byte Nhiệt độ + 1 byte CRC + 2 byte Độ ẩm + 1 byte CRC):

| Hàm (Legacy API) | Chức năng |
| :--- | :--- |
| `i2c_master_read_from_device()` | Đọc một lượng byte nhất định từ cảm biến vào buffer. |
| `i2c_master_read_byte()` | Đọc từng byte và quyết định gửi ACK hay NACK. |
| `i2c_master_stop()` | Tạo điều kiện STOP để giải phóng bus. |

## Lưu ý khi sử dụng I2C với SHT30

Lưu ý cho đồ án: Khi dùng SHT30, bạn nên sử dụng hàm `i2c_master_transmit_receive()` (trong API mới) hoặc kết hợp `write` rồi `read` có độ trễ (delay) khoảng 20-50ms ở giữa để cảm biến có thời gian thực hiện phép đo trước khi trả kết quả.
