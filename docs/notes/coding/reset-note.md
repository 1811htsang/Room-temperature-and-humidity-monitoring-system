# Reset
Chia ra làm 3 loại:
- CPU reset
- Core reset
- System reset

## CPU reset
Chỉ thực hiện reset các reg của lõi CPU

## Core reset
Thực hiện reset tất cả thanh ghi trừ nhóm thanh ghi RTC

## System reset
Thực hiện reset tất cả thanh ghi (kể cả nhóm thanh ghi RTC) về giá trị mặc định.

### Nhận xét
System reset là loại reset cao nhất, do đó việc lựa chọn reset cần phải cân nhắc kỹ lưỡng để tránh việc reset không cần thiết. Ngoài ra, tất cả các loại reset đều không ảnh hưởng đến RAM. 

## Nguồn reset
Do CPU chia ra APP_CPU và PRO_CPU nên có 2 nguồn reset riêng biệt:
- Nguồn reset của PRO_CPU trữ trong thanh ghi `RTC_CNTL_RESET_CAUSE_PROCPU`
- Nguồn reset của APP_CPU trữ trong thanh ghi `RTC_CNTL_RESET_CAUSE_APPCPU`

Bảng tra cứu reset có trong [tech-dts](/docs/references/esp32-technical-reference-manual-ver-5_2.pdf) trang 40

## Quy trình reset sử dụng Software reset
1. `RTC_CNTL_OPTIONS0_REG->RTC_CNTL_SW_SYS_RST = 1;` // System reset

Do việc reset này bổ sung thêm việc lựa chọn cụ thể CPU cho việc reset:
- Với APP_CPU, `RTC_CNTL_RESET_STATE_REG->RTC_CNTL_RESET_CAUSE_APPCPU = 0x03;` // Software reset applies System reset to APP CPU
- Với PRO_CPU, `RTC_CNTL_RESET_STATE_REG->RTC_CNTL_RESET_CAUSE_PROCPU = 0x03;` // Software reset applies System reset to PRO CPU

## Quy trình reset sử dụng CPU reset
Đối với CPU reset, chỉ có thể reset từng CPU riêng biệt

Với APP_CPU:
1. `RTC_CNTL_OPTIONS0_REG->RTC_CNTL_SW_APPCPU_RST = 1;` // CPU reset với APP CPU
2. Với APP_CPU, `RTC_CNTL_RESET_STATE_REG->RTC_CNTL_RESET_CAUSE_APPCPU = 0x0C;` // Software reset applies System reset to APP CPU

Với PRO_CPU:
1. `RTC_CNTL_OPTIONS0_REG->RTC_CNTL_SW_PROCPU_RST = 1;` // CPU reset với PRO CPU
2. Với PRO_CPU, `RTC_CNTL_RESET_STATE_REG->RTC_CNTL_RESET_CAUSE_PROCPU = 0x0C;` // Software reset applies System reset to PRO CPU

## Trường hợp đặc biệt
Do APP_CPU được điều khiển bổ sung bởi nhóm thanh ghi DPort (PRO_CPU khong có), do đó việc reset APP_CPU có thể thực hiện thông qua DPort (`DPORT_APPCPU_CTRL_REG_A_REG->DPORT_APPCPU_RESETTING = 1;`)

Tuy nhiên cách thực hiện reset chỉ xảy ra với nguồn đầu vào là tín hiệu reset độc lập của PRO_CPU.

Kiểm chứng thông tin trong [tech-dts](/docs/references/esp32-technical-reference-manual-ver-5_2.pdf) trang 40, 41