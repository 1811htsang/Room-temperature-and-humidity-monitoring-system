# Tổng quan
ESP32 được thiết kế với 3 WDT chính là 
- 2 WDT gọi là MWDT (Main Watchdog Timer) 
- 1 WDT gọi là RWDT (RTC Watchdog Timer).

Mỗi watch dog đều có 4 công đoạn chính, mỗi công đoạn có thể chọn 1 trong 3/4 action khi hết thời gian với mỗi công đoạn. Mỗi công đoạn có timeoute khác nhau có thể set.

Các action bao gồm:
- Ngắt
- CPU reset
- Core reset
- System reset

Trong đó, chỉ có mỗi RWDT được phép thực hiện System reset và reset toàn bộ chip bao gồm cả RTC.

# Chức năng
- 4 công đoạn với các cấu hình khác nhau có thể tắt riêng biệt
- Timeout có thể cấu hình riêng biệt cho từng công đoạn
- Bộ đếm 32 bit 
- Hỗ trợ write protection bảo vệ cho MWDT và RWDT
- Hỗ trợ flash boot protection

# Mô tả chức năng
## Clk đầu vào
RWDT sử dụng từ RTC_SLOW_CLK

MWDT sử dụng từ APB_CLK + prescaler

Với mỗi watchdog, nguồn clk sẽ là nguồn vào của bộ đếm 32-bit.

Khi bộ đếm đặt giá trị timeout của stage hiện tại, các action đã được cấu hình sẽ thực thi. Bộ đếm reset và stage kế tiếp được kích hoạt.

## Quy trình hoạt động
Khi bộ hẹn giờ giám sát được kích hoạt, nó sẽ hoạt động theo vòng lặp từ stage 0 đến stage 3, sau đó quay lại stage 0 và bắt đầu lại. 

Action timeout và khoảng thời gian cho mỗi stage có thể được cấu hình riêng lẻ. 

Mỗi stage có thể được cấu hình cho một trong các action sau khi bộ hẹn giờ timeout đạt đến giá trị timeout của stage: 
- Kích hoạt ngắt 

  Khi stage timeout, một ngắt sẽ được kích hoạt. 

- Reset lõi CPU 

  Khi stage timeout, lõi CPU được chỉ định sẽ được reset. 
  
  MWDT0 CPU reset chỉ reset PRO_CPU. 
  
  Reset CPU MWDT1 chỉ reset CPU APP. 
  
  Reset CPU RWDT có thể reset một trong hai, hoặc cả hai, hoặc không reset cái nào, tùy thuộc vào cấu hình.

- Reset main system 

  Khi stage timeout, hệ thống chính, bao gồm cả các MWDT, sẽ được reset. Trong bài viết này, hệ thống chính bao gồm CPU và tất cả các thiết bị ngoại vi. Bộ đếm thời gian thực (RTC) là một ngoại lệ và nó sẽ không được đặt lại. 

- Đặt lại hệ thống chính và RTC 

  Khi stage kết thúc, cả hệ thống chính và RTC sẽ được đặt lại. action này chỉ khả dụng trong RWDT.

- Vô hiệu hóa stage này sẽ không có tác động gì đến hệ thống. 

Khi phần mềm cung cấp dữ liệu cho bộ đếm thời gian giám sát, nó sẽ quay lại stage 0 và bộ đếm thời gian timeout của nó sẽ reset từ 0. (Thông tin cấu hình cho `TIMn_WDTFEED_REG`)

## Bảo vệ ghi
Cả MWDT và RWDT đều có thể được bảo vệ khỏi việc ghi dữ liệu ngẫu nhiên. Để thực hiện điều này, chúng có một thanh ghi khóa ghi (`TIMGn_WDTWPROTECT_REG` cho MWDT, `RTC_CNTL_WDTWPROTECT_REG` cho RWDT). 

Khi khởi động lại, các thanh ghi này được khởi tạo với giá trị `0x50D83AA1`. Khi giá trị trong thanh ghi này được thay đổi từ `0x50D83AA1`, chức năng bảo vệ ghi sẽ được kích hoạt. 

Các thao tác ghi vào bất kỳ thanh ghi WDT nào, bao gồm cả thanh ghi cấp dữ liệu (nhưng không bao gồm chính thanh ghi khóa ghi), đều bị bỏ qua. 

Quy trình được khuyến nghị để truy cập WDT là: 
1. Vô hiệu hóa chức năng bảo vệ ghi 
2. Thực hiện sửa đổi cần thiết hoặc cấp dữ liệu cho bộ giám sát 
3. Kích hoạt lại chức năng bảo vệ ghi

## Bảo vệ khởi động từ flash
Trong quá trình khởi động flash, MWDT trong nhóm hẹn giờ 0 (TIMG0), cũng như RWDT, được tự động kích hoạt. 

Giai đoạn 0 đối với MWDT được kích hoạt sẽ tự động được cấu hình để đặt lại hệ thống khi hết hạn; giai đoạn 0 đối với RWDT sẽ đặt lại RTC khi nó hết hạn. 

Sau khi khởi động, cần clear thanh ghi `TIMERS_WDT_FLASHBOOT_MOD_EN` để dừng quy trình bảo vệ khởi động flash cho MWDT, và cần clear thanh ghi `RTC_CNTL_WDT_FLASHBOOT_MOD_EN` để thực hiện điều tương tự cho RWDT. Sau đó, MWDT và RWDT có thể được cấu hình bằng phần mềm.

# Thanh ghi
Các thanh ghi MWDT là một phần của TIM và được mô tả trong phần TIM. Các thanh ghi RWDT là một phần của RTC và được mô tả trong phần RTC.

# Quy trình sử dụng MWDT
1. Vô hiệu hóa bảo vệ ghi bằng cách viết giá trị `0x50D83AA1` vào thanh ghi `TIMGn_WDTWPROTECT_REG`, n là 0 hoặc 1.
2. `TIMn_WDTCONFIG0_REG->TIMGn_WDT_STG0 = 0`: cấu hình stage 0 để vô hiệu hóa.
3. `TIMn_WDTCONFIG0_REG->TIMGn_WDT_STG1 = 1`: cấu hình stage 1 để ngắt.
4. `TIMn_WDTCONFIG0_REG->TIMGn_WDT_STG2 = 2`: cấu hình stage 2 để reset CPU.
5. `TIMn_WDTCONFIG0_REG->TIMGn_WDT_STG3 = 3`: cấu hình stage 3 để reset hệ thống.
6. `TIMn_WDTCONFIG1_REG->TIMGn_WDT_LEVEL_INT_EN = 1`: cấu hình kích hoạt ngắt ở mức.
7. `TIMn_WDTCONFIG2_REG->TIMGn_WDT_CPU_RESET_LENGTH = x`: cấu hình độ dài reset CPU.
8. `TIMn_WDTCONFIG2_REG->TIMGn_WDT_SYS_RESET_LENGTH = y`: cấu hình độ dài reset hệ thống.
9. `TIMn_WDTCONFIG0_REG->TIMGn_WDT_EN = 1`: bật WDT.
10. Hoạt hóa bảo vệ ghi bằng cách viết giá trị khác `0x00000000` vào thanh ghi `TIMGn_WDTWPROTECT_REG`.

Trong quá trình sử dụng, để thực hiện reset bộ đếm WDT và quay lại stage 0, cần viết giá trị `0x0123ABCD` vào thanh ghi `TIMn_WDTFEED_REG`.

**Chú thích:** 

1. Trong thiết kế phần cứng, ngắt WDT chia ra 2 loại là theo mức và theo cạnh. Ngắt theo mức được ưu tiên sử dụng cao hơn ngắt theo cạnh do tính ổn định và độ tin cậy cao hơn. Do đó, trong quy trình sử dụng MWDT ở trên, chỉ kích hoạt ngắt theo mức. Đối với ngắt theo cạnh, chỉ nên sử dụng đối với các ngoại vi không quan trọng. 
2. Độ dài reset CPU và SYS phụ thuộc vào thời gian cần thiết để hoàn thành quy trình reset. Giá trị cụ thể có thể tham khảo trong datasheet của ESP32.
3. Giá trị điền vào `TIMGn_WDTFEED_REG` không ảnh hưởng đến chức năng reset bộ đếm WDT. Bất kỳ giá trị nào cũng có thể được sử dụng để reset bộ đếm WDT.

Kiểm chứng thông tin trong [tech-dts](../references/esp32-technical-reference-manual-ver-5_2.pdf) trang 516.

# Quy trình sử dụng RWDT
1. Vô hiệu hóa bảo vệ ghi bằng cách viết giá trị `0x50D83AA1` vào thanh ghi `RTC_CNTL_WDTWPROTECT_REG`.
2. `RTC_CNTL_WDTCONFIG0_REG->RTC_CNTL_WDT_PAUSE_IN_SLP = 0`: cấu hình RWDT không tạm dừng trong chế độ ngủ.
3. `RTC_CNTL_WDTCONFIG0_REG->RTC_CNTL_WDT_APPCPU_RESET_EN = 1`: cho phép reset CPU APP.
4. `RTC_CNTL_WDTCONFIG0_REG->RTC_CNTL_WDT_PROCPU_RESET_EN = 1`: cho phép reset CPU PRO.
5. `RTC_CNTL_WDTCONFIG0_REG->RTC_CNTL_WDT_FLASHBOOT_MOD_EN = 0`: vô hiệu hóa bảo vệ khởi động từ flash.
6. `RTC_CNTL_WDTCONFIG0_REG->RTC_CNTL_WDT_SYS_RESET_LENGTH = 0->7`: cấu hình độ dài reset hệ thống.
7. `RTC_CNTL_WDTCONFIG0_REG->RTC_CNTL_WDT_CPU_RESET_LENGTH = 0->7`: cấu hình độ dài reset CPU.
8. `RTC_CNTL_WDTCONFIG0_REG->RTC_CNTL_WDT_STG3 = 4`: cấu hình stage 3 để reset RTC.
9. `RTC_CNTL_WDTCONFIG0_REG->RTC_CNTL_WDT_STG2 = 3`: cấu hình stage 2 để reset hệ thống.
10. `RTC_CNTL_WDTCONFIG0_REG->RTC_CNTL_WDT_STG1 = 2`: cấu hình stage 1 để reset CPU.
11. `RTC_CNTL_WDTCONFIG0_REG->RTC_CNTL_WDT_STG0 = 1`: cấu hình stage 0 để ngắt.
12. `RTC_CNTL_WDTCONFIG1_REG->RTC_CNTL_WDT_EN = 1`: bật RWDT.
13. Hoạt hóa bảo vệ ghi bằng cách viết giá trị khác như `0x00000000` vào thanh ghi `RTC_CNTL_WDTWPROTECT_REG`.

**Chú thích:**

Trong tài liệu, mục RTC_CNTL sẽ có các thanh ghi bổ sung bao gồm:
- `RTC_CNTL_WDTCONFIGn_REG`, n từ 1 đến 4, trong cấu hình chỉ có thông tin về việc giữ chu kỳ cho CLK và không có thông tin về việc cấu hình action cho từng stage. Do đó, trong quy trình sử dụng RWDT ở trên, chỉ đề cập đến các thanh ghi có thông tin rõ ràng về chức năng.

Ngoài ra, kiểm tra trong thư viện HAL của ESP-IDF cũng hoàn toàn bỏ qua các thanh ghi này, do đó có thể kết luận rằng các thanh ghi này không cần thiết cho việc cấu hình RWDT.

Kiểm chứng thông tin trong [tech-dts](../references/esp32-technical-reference-manual-ver-5_2.pdf) trang 703 và [hal-driver](C:\Users\shanghuang\esp\v5.5\esp-idf\components\hal\wdt_hal_iram.c).


# Bổ sung
Đối với thiết kế phần cứng, ESP32 chỉ tồn tại 2 WDT là MWDT và RWDT. 
Tuy nhiên, với API của ESP32 sẽ xây dựng bổ sung 3 WDT như sau:
- IWDT (Interrupt Watchdog Timer) chịu trách nhiệm đảm bảo các interrupt handler (ISR) không bị chặn chạy trong thời gian dài và ngăn ISR bị kẹt trong quá trình thực thi.
- TWDT (Task Watchdog Timer) chịu trách nhiệm phát hiện các trường hợp task chạy mà không nhường quyền điều khiển quá lâu.
- RTC/LP-WDT (RTC/Low Power Watchdog Timer) có thể được sử dụng để theo dõi thời gian khởi động từ khi bật nguồn cho đến khi hàm chính của user được thực thi. Nó cũng có thể được sử dụng trong quá trình xử lý lỗi hệ thống và trong môi trường năng lượng thấp.

# IWDT
IWDT sử dụng MWDT nhánh TIM1 và tận dụng ngắt tick FreeRTOS ở mỗi CPU để phục vụ việc feed IWDT.

Nếu ngắt tick trên một CPU cụ thể không được thực thi trong khoảng timeout của IWDT, điều đó cho thấy có điều gì đó đang ngăn chặn các ISR chạy trên CPU đó.

Nếu vì bất cứ lý do gì khiến cho panic handler không thể chạy sau khi IWDT timeout thì stage tiếp theo sẽ hard-reset chip.

## Cấu hình IWDT
- Enable mặc định thông qua `CONFIG_ESP_INT_WDT`
- Cấu hình timeout thông qua `CONFIG_ESP_INT_WDT_TIMEOUT_MS`

Lưu ý rằng timeout mặc định sẽ cao hơn nếu hỗ trợ PSRAM được bật, vì một đoạn mã quan trọng hoặc interrupt handler truy cập vào một lượng lớn PSRAM sẽ mất nhiều thời gian hơn để hoàn thành trong một số trường hợp.

timeout được cấu hình cho IWDT luôn phải dài hơn ít nhất gấp đôi khoảng thời gian giữa hai xung nhịp FreeRTOS, ví dụ: nếu hai xung nhịp FreeRTOS cách nhau 10 ms, thì timeout IWDT phải ít nhất lớn hơn 20 ms (xem CONFIG_FREERTOS_HZ).

## Hiệu chỉnh
Nếu bạn thấy lỗi hết timeout IWDT xảy ra do ngắt hoặc critical section chạy lâu hơn timeout, hãy xem xét viết lại mã:

- Các critical section nên được rút ngắn tối đa. Bất kỳ mã/tính toán không quan trọng nào nên được đặt bên ngoài critical section.

- Các interrupt handler cũng nên thực hiện lượng tính toán tối thiểu. user có thể xem xét trì hoãn bất kỳ tính toán nào cho một task bằng cách cho phép ISR đẩy dữ liệu đến task bằng cách sử dụng hàng đợi.

Cả critical section và interrupt handler không bao giờ được chặn để chờ một sự kiện khác xảy ra. Nếu việc thay đổi mã để giảm thời gian xử lý là không thể hoặc không mong muốn, bạn có thể tăng cài đặt `CONFIG_ESP_INT_WDT_TIMEOUT_MS` thay thế.

# TWDT
TWDT sử dụng MWDT nhánh TIM0 để theo dõi các task cụ thể, đảm bảo các task có thể thực thi trong khoảng thời gian giới hạn. Khi có timeout sẽ tạo ra 1 ngắt.

Trong cài đặt mặc định, TWDT theo dõi các idle tasks (task rảnh) của từng CPU, tuy nhiên các task nào được subscribe tới TWDT đều có thể được chính TWDT giám sát.

Thông qua giám sát các task rảnh của từng CPU, TWDT có thể phát hiện các thể hiện của các task chạy trong thời gian dài không dừng. Điều này có thể là dấu hiệu của code kém gây ra hiện tượng vòng lặp chờ trên thiết bị ngoại vi, hoặc một task bị kẹt trong vòng lặp vô hạn.

user có thể định nghĩa hàm `esp_task_wdt_isr_user_handler` trong code để nhận sự kiện hết timeout và mở rộng hành vi mặc định.

## Cấu hình sử dụng TWDT
Các hàm sau có thể được sử dụng để theo dõi các task bằng TWDT:

- `esp_task_wdt_init()` để khởi tạo TWDT và đăng ký các idle task.

- `esp_task_wdt_add()` đăng ký thêm các task khác vào TWDT.

Sau khi đăng ký, cần gọi `esp_task_wdt_reset()` từ task để cập nhật TWDT.

- `esp_task_wdt_delete()` hủy đăng ký một task đã được đăng ký trước đó.

- `esp_task_wdt_deinit()` hủy đăng ký các task rảnh rỗi và hủy khởi tạo TWDT.

Trong trường hợp các ứng dụng cần theo dõi ở mức độ chi tiết hơn (ví dụ: đảm bảo rằng một hàm/stub/đường dẫn mã cụ thể được gọi), TWDT cho phép đăng ký user.

- `esp_task_wdt_add_user()` để đăng ký một user bất kỳ vào TWDT. Hàm này trả về một handle user cho user được thêm vào.
- Hàm `esp_task_wdt_reset_user()` phải được gọi bằng cách sử dụng handle user để tránh lỗi hết timeout của TWDT.

- Hàm `esp_task_wdt_delete_user()` hủy đăng ký một user bất kỳ khỏi TWDT.

Kiểm chứng thông tin bổ sung trong [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/wdts.html).

# Nhận xét
Với tất cả các thông tin đã thu thập, chiến lược sử dụng WDT trong ESP32 nên là:
- Sử dụng TWDT để giám sát các task, đảm bảo chúng không bị kẹt trong vòng lặp vô hạn hoặc chặn quá lâu. Ở vòng này, TWDT sẽ giám sát các lỗi logic trong ứng dụng.
- Sử dụng IWDT để giám sát các đoạn code làm tê liệt OS, đảm bảo các ISR không bị chặn quá lâu. Ở vòng này, IWDT sẽ giám sát các lỗi nghiêm trọng hơn có thể làm tê liệt toàn bộ hệ thống.
- Sử dụng MWDT để giám sát các driver can thiệp sâu vào cấu trúc phần cứng bằng thanh ghi. Ở vòng này, MWDT sẽ giám sát các lỗi nghiêm trọng có thể làm treo hệ thống, đảm bảo phần cứng không bị kẹt trong trạng thái không thể phục hồi.
- Sử dụng RWDT để chống treo phần cứng hoặc lỗi sụt áp nguồn nghiêm trọng. Ở vòng này, RWDT sẽ giám sát các lỗi nghiêm trọng nhất có thể làm hệ thống không thể hoạt động được nữa.