# Clock system trên ESP32-WROOM-32
Về thiết kế cơ bản, ESP32-WROOM-32 gồm các nguồn chính sau:
- CLK tốc độ cao
  - `PLL_CLK` là clk nội bộ với dải tần số từ 320MHz đến 480MHz
  - `XTL_CLK` là clk từ thạch anh ngoài với tần số 2-40MHz
- CLK tốc độ thấp
  - `XTL32K_CLK` là clk từ thạch anh ngoài với tần số 32kHz 
  - `RC_FAST_CLK` là nguồn clock nội với tần số mặc định 8MHz có thể tùy chỉnh
  - `RC_FAST_DIV_CLK` là nguồn tách từ `RC_FAST_CLK` với phần chia 256. Do đó với tần số mặc định 8MHz thì `RC_FAST_DIV_CLK` có tần số 31.25kHz
  - `RC_SLOW_CLK` là nguồn clock nội với tần số mặc định 150kHz có thể tùy chỉnh
- CLK cho Audio
  - `APLL_CLK` là clock nội bộ dành riêng cho Audio với tần số từ 16MHz đến 128MHz

Kiểm tra nguồn thông tin trong [tech-dts](/docs/references/esp32-technical-reference-manual-ver-5_2.pdf) trang 41 đối với phần cấu trúc thiết kế hệ thống clock trên ESP32-WROOM-32

## Nguồn clock cho CPU

Ngoài các nguồn clock trên, CPU còn có nguồn clock riêng biệt là `CPU_CLK` sử dụng riêng biệt cho lõi CPU. Ở chế độ hiệu năng cao, `CPU_CLK` có thể đạt tần số tối đa 240MHz.

Việc lựa chọn nguồn clock cho `CPU_CLK` được xác định bởi thanh ghi `RTC_CNTL_SOC_CLK_SEL`. Trong đó `PLL_CLK`, `APLL_CLK`, `RC_FAST_CLK` và  `XTL_CLK` đều có thể được lựa chọn làm nguồn cho `CPU_CLK`.

Kiểm chứng thông tin trong [tech-dts](/docs/references/esp32-technical-reference-manual-ver-5_2.pdf) trang 42-43.

## Nguồn clock cho ngoại vi
Nguồn clock cho tất cả các ngoại vi được lấy đầu vào từ 5 nguồn chính là
- `APB_CLK`
- `REF_TICK`
- `LEDC_SCLK`
- `APLL_CLK`
- `PLL_F160M_CLK`

Ngoài ra, tùy thuộc vào từng ngoại vi cụ thể mà có thể sử dụng các nguồn CLK khác nhau, điều này được ghi chú kỹ trong phần mô tả [tech-dts](/docs/references/esp32-technical-reference-manual-ver-5_2.pdf) từ trang 43.

Lưu ý ở đây chỉ tập trung vào các nguồn clock chính, các nguồn clock không sử dụng trong dự án sẽ không được đề cập.

### `APB_CLK`
Nguồn này lấy từ 4 nguồn chính là `PLL_CLK`, `APLL_CLK`, `XTL_CLK` và `RC_FAST_CLK`.

Bảng thông số chia nguồn `APB_CLK` có trong [tech-dts](/docs/references/esp32-technical-reference-manual-ver-5_2.pdf) trang 43.

### `REF_TICK`
Nguồn này phụ thuộc vào `APB_CLK` mà `APB_CLK` được xác định bởi `CPU_CLK`.

Trong tài liệu, nguồn này được khuyến khích đảm bảo cố định (không ràng buộc tần số ở 1MHz). Nghĩa là khi thay đổi `CPU_CLK` thì người sử dụng nên đảm bảo nguồn `REF_TICK` vẫn giữ nguyên tần số mong muốn.

Bảng thông tin nằm trong [tech-dts](/docs/references/esp32-technical-reference-manual-ver-5_2.pdf) trang 44.

Giải thích ví dụ ở trang 44:
> Khi nguồn `CPU_CLK` là `PLL_CLK` và người dùng cần giữ `REF_TICK` ở 1MHz thì phải đảm bảo `SYSCON_PLL_TICK_NUM=0x4F` để tần số `REF_TICK` tương đương `80/(79+1)=1` (Thông tin về công thức nằm chung với bảng ở trang 44).

### `APLL_CLK` cho ngoại vi
Như đã đề cập trước đó, `APLL_CLK` là nguồn clock nội bộ dành riêng cho Audio với tần số từ 16MHz đến 128MHz.

Nguồn đầu vào chính của `APLL_CLK` là từ `PLL_CLK`, do đó phải sử dụng bổ sung các nhóm thanh ghi để điều chỉnh tần số của `APLL_CLK`. 

Tuy nhiên, trong tài liệu không đề cập rõ ràng thông tin về điều chỉnh trực tiếp tần số của `APLL_CLK` mà chỉ xem `APLL_CLK` là nguồn con phụ thuộc vào `PLL_CLK`.

Ngoài ra còn có 1 phần đề mục *Audio PLL* riêng biệt khỏi `APLL_CLK` trong tài liệu và thông tin cụ thể chi tiết hơn.

Do đó, có thể suy đoán rằng việc điều chỉnh tần số của `APLL_CLK` sẽ thông qua việc điều chỉnh tần số của `PLL_CLK`.

Kiểm chứng thông tin trong [tech-dts](/docs/references/esp32-technical-reference-manual-ver-5_2.pdf) trang 44.

### `PLL_F160M_CLK`
Nguồn này lấy trực tiếp từ `PLL_CLK` với tần số cố định 160MHz không thay đổi.

## Cân nhắc lựa chọn clock đối với ngoại vi
Trong tài liệu về bảng thông tin nguồn clock cho các loại ngoại vi, ta có thể thấy rằng tất cả ngoại vi đều có thể sử dụng `APB_CLK` làm nguồn clock chính. 

Ngoài ra nếu tần số của `APB_CLK` bị thay đổi thì các ngoại vi cũng đều phải thực hiện cập nhật lại tần số hoạt động tương ứng trừ các ngoại vi có thể sử dụng `REF_TICK` làm nguồn clock.

## Nguồn clock cho Wi-Fi và Bluetooth
Sử dụng bắt buộc nguồn clock chính là `PLL_CLK`. Nếu không sử dụng `PLL_CLK` thì cả 2 chức năng này sẽ tiến vào trạng thái hoạt động tiêu thụ thấp (low-power mode).

Nếu sử dụng trong chế độ tiết kiệm năng lượng (modem-sleep mode) thì có thể sử dụng các nguồn clock phụ là `RC_SLOW_CLK`, `RC_FAST_CLK` hoặc `XTL_CLK`.

Kiểm chứng thông tin trong [tech-dts](/docs/references/esp32-technical-reference-manual-ver-5_2.pdf) trang 45.

## Nguồn clock cho RTC
Trong thiết kế hệ thống clock của ESP32, RTC có 2 nguồn chính là `RTC_SLOW_CLK` và `RTC_FAST_CLK`, 2 nguồn này giúp cho RTC có thể hoạt động khi hầu hết các nguồn khác bị tắt.

Dựa vào thiết kế truy ngược lại, ta có thể thấy:

### RTC_FAST_CLK
Nguồn này lấy từ 2 nguồn chính là
- `RC_FAST_CLK`
- `XTL_CLK` + Bộ chia tần số -> `XTAL_DIV_CLK`

Nguồn này có thể sử dụng cho các module quản lý năng lượng.

### RTC_SLOW_CLK
Nguồn này lấy từ 3 nguồn chính là
- `RC_SLOW_CLK`
- `XTL32K_CLK`
- `RC_FAST_DIV_CLK`

Nguồn này có thể sử dụng cho các module cảm biến trên chip.

Kiểm chứng thông tin trong [tech-dts](/docs/references/esp32-technical-reference-manual-ver-5_2.pdf) trang 45.

## Nguồn clock cho Audio PLL
Công thức tính nguồn clock ra từ Audio PLL như sau:
$$\large{f_{\text{out}}=\frac{f_{\text{xtal}}(\text{sdm2}+\frac{\text{sdm1}}{2^{8}}+\frac{\text{sdm0}}{2^{16}}+4)}{2(\text{odiv}+2)}}$$

Trong đó:
- \(f_{xtal}\) là tần số của thạch anh ngoài (thường là 40MHz)
- sdm2, sdm1, sdm0 là các hệ số điều chỉnh tần số
  - sdm2 là phần nguyên lấy giá trị từ 0 đến 63
  - sdm1 là phần phân số lấy giá trị từ 0 đến 255
  - sdm0 là phần phân số lấy giá trị từ 0 đến 255
- odiv là hệ số chia tần số đầu ra, giá trị từ 0 đến 31

Với công thức này, toán hạng tử số có thể đạt từ 350MHz đến 500MHz.

Ngoài ra, cần chú ý rằng ở revision0 của ESP32, không thể setup `smd0` và `smd1`, kiểm chứng thông tin trong [soc-errata](../references/esp32-eco-&-workarounds-for-bugs-ver-2_3.pdf) trang 12.

Audio PLL có thể được điều khiển thủ công qua thanh ghi `RTC_CNTL_PLLA_FORCE_PU` và `RTC_CNTL_PLLA _FORCE_PD` tương ứng. Quyền vô hiệu hóa có mức ưu tiên cao hơn so với quyền kích hoạt. Nếu cả 2 thanh ghi đều là 0 thì Audio PLL sẽ tuân thủ theo trạng thái hoạt động của hệ thống.

## Quy trình setup Clock cho ESP32
Dựa trên các nguồn thông tin đã thu thập, các hạng mục chính cần setup clock bao gồm `CPU_CLK`, `REF_TICK`.

### Setup `CPU_CLK`
1. Chọn nguồn clock cho CPU trong `RTC_CNTL_SOC_CLK_SEL`. Ở đây chọn `PLL_CLK` làm nguồn chính cho CPU (Lý giải bên dưới) 
2. `RTC_CNTL_CLK_CONF_REG->RTC_CNTL_SOC_CLK_SEL=01`; // Chọn PLL_CLK làm nguồn cho CPU_CLK
3. `DPORT_CPU_PER_CONF_REG->DPORT_CPU_CPUPERIOD_SEL=10`; // Chọn tần số CPU là 240MHz

**Lý giải**:

Ở đây chọn `PLL_CLK` làm nguồn cho CPU vì
- Lấy mẫu ở tốc độ 100ksps có nghĩa là cứ 10µs hệ thống lại nhận được một mẫu dữ liệu mới. Mặc dù DMA giúp chuyển dữ liệu vào RAM mà không tốn CPU, nhưng sau đó Core 1 phải xử lý khối dữ liệu khổng lồ này (FFT, Filtering) và Core 0 phải duy trì Wifi Stack.
- Do đó, để đảm bảo hệ thống xử lý kịp thời gian thực, cần thiết lập CPU ở tần số cao nhất là 240MHz. Các nguồn khác như `XTL_CLK` hay `RC_FAST_CLK` không thể đáp ứng được tần số này.
- Ngoài ra loại bỏ việc sử dụng `APLL_CLK` làm nguồn cho CPU vì nguồn này dành riêng cho Audio, việc sử dụng nó cho CPU như đã đề cập sẽ làm gián đoạn chức năng Audio trong trường hợp tần số `APLL_CLK` bị thay đổi.

### Setup `REF_TICK`
1. Xác định tần số mong muốn cho REF_TICK (Ở đây chọn 1MHz)
2. `SYSCON_PLL_TICK_CONF_REG->SYSCON_PLL_TICK_NUM=0x4F`; // Thiết lập tần số REF_TICK là 1MHz khi CPU_CLK là PLL_CLK

> Ở đây sẽ bổ sung thêm phần setup clock cho các ngoại vi nếu cần thiết trong tương lai.