/**
 * @file app_decl.h
 * @author Shang Huang
 * @brief Application declaration header file
 * @version 0.1
 * @date 2026-05-07
 * 
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef __APP_DECL_H__
	#define __APP_DECL_H__

	/**
	 * @brief Khai báo thư viện sử dụng
	 */

	#include "ciedpc_core.h"
	#include "ciedpc_task.h"
	#include "ciedpc_tsm.h"
	#include "ciedpc_fsm.h"
	#include "ciedpc_msg.h"
	#include "ciedpc_timer.h"

	/**
	 * @brief Khai báo tác vụ
	 * @attention Tác vụ phải khai báo đúng định dạng `0xEx`
	 * 						x bắt đầu từ 6 trở đi. Giới hạn tối đa là 0xEF (15 tác vụ)
	 * @example
	 * #define TASK_NORM_A_ID (0xE6u)
	 * #define TASK_NORM_B_ID (0xE7u)
	 */

	// Điền các khai báo tại đây
	#define TASK_NORM_DS3231_ID 	(0xE6u)
	#define TASK_NORM_SHT30_ID 		(0xE7u)
	#define TASK_NORM_LCD_ID 			(0xE8u)
	#define TASK_NORM_LED_ID 			(0xE9u)

	/**
	 * @brief Khai báo tín hiệu giao tiếp giữa các tác vụ
	 * @attention Tác vụ phải khai báo đúng định dạng `0x0x`
	 * 						x bắt đầu từ 1 trở đi. Không nên vượt quá 0x7F 
	 * 						để tránh trùng với tín hiệu nội bộ của hệ thống CIEDPC
	 * @example
	 * #define SIG_USR_START     (0x01u)
	 * #define SIG_USR_STOP      (0x02u)
	 * #define SIG_TSK_A_TO_B    (0x03u)
	 * #define SIG_TSK_B_TO_A    (0x04u)
	 */

	// Điền các khai báo tại đây
	#define SIG_USR_START     (0x01u)
	#define SIG_USR_STOP      (0x02u)
	#define SIG_READ_DS3231   (0x03u)
	#define SIG_READ_SHT30    (0x04u)
	#define SIG_RECALL_SHT30  (0x05u)
	#define SIG_UPDATE_LCD    (0x06u)
	#define SIG_LED_ON        (0x07u)
	#define SIG_LED_OFF       (0x08u)

	/**
	 * @brief Khai báo message queue cho các tác vụ
	 * @attention Mỗi tác vụ sẽ có một hàng đợi tin nhắn riêng biệt
	 * 						Tùy thuộc vào nhu cầu của ứng dụng để điều chỉnh kích thước của hàng đợi, 
	 * 						nhưng cần đảm bảo không vượt quá giới hạn của hệ thống CIEDPC
	 * @example 
	 * extern ciedpc_msg_t* usr_q_mem[8];
	 * extern ciedpc_msg_t* a_q_mem[8];
	 * extern ciedpc_msg_t* b_q_mem[8];
	 */

	// Điền các khai báo tại đây
	extern ciedpc_msg_t* usr_q_mem[CIEDPC_TASK_MSG_QUEUE_SIZE];
	extern ciedpc_msg_t* ds3231_q_mem[CIEDPC_TASK_MSG_QUEUE_SIZE];
	extern ciedpc_msg_t* sht30_q_mem[CIEDPC_TASK_MSG_QUEUE_SIZE];
	extern ciedpc_msg_t* lcd_q_mem[CIEDPC_TASK_MSG_QUEUE_SIZE];
	extern ciedpc_msg_t* led_q_mem[4];

	/**
	 * @brief Khai báo biến đếm hoạt động của hệ thống
	 * @attention Nên khuyến khích sử dụng biến này để theo dõi số lượng hành động 
	 * 						đã thực hiện trong hệ thống khi task_scheduler được gọi,
	 * 						đặc biệt hữu ích trong các bài test để xác nhận rằng hệ thống đang hoạt động như mong đợi 
	 * 						và để phát hiện các vấn đề tiềm ẩn như vòng lặp vô hạn hoặc tắc nghẽn trong scheduler.
	 */

	extern uint32_t system_action_count;

	/**
	 * @brief Khai báo các hàm handler cho các task
	 * @attention Mỗi tác vụ phải có một hàm handler tương ứng 
	 * 						để xử lý các tin nhắn nhận được. 
	 * 						Với các task_norm thì hàm handler 
	 * 							có định dạng `void task_norm_x_handler(ciedpc_msg_t* msg)`,
	 * 							trong đó x là tên tác vụ.
	 * 						Với các task_poll thì hàm handler 
	 * 							có định dạng `void task_poll_x_handler()`,
	 * 							trong đó x là tên tác vụ polling.
	 */

	// Điền các khai báo tại đây
	void task_norm_usr_handler(ciedpc_msg_t* msg);
	void task_norm_ds3231_handler(ciedpc_msg_t* msg);
	void task_norm_sht30_handler(ciedpc_msg_t* msg);
	void task_norm_lcd_handler(ciedpc_msg_t* msg);
	void task_norm_led_handler(ciedpc_msg_t* msg);
	void task_poll_memrp_handler();
	void task_poll_syslf_handler();

	/**
	 * @brief Khai báo các hàm on-entry/exit cho các trạng thái FSM (nếu có)
	 */

	// Điền các khai báo tại đây

	/**
	 * @brief Khai báo các hàm on-state cho các trạng thái TSM (nếu có)
	 */

	// Điền các khai báo tại đây

	/**
	 * @brief Khai báo các state_handler cho các trạng thái FSM (nếu có)
	 */

	// Điền các khai báo tại đây

	/**
	 * @brief Khai báo các cấu hình khác
	 */

	/**
	 * @brief Khai báo địa chỉ thiết bị I2C
	 */

	#define SHT30_ADDR 			0x44 // Địa chỉ I2C mặc định của cảm biến SHT30
	#define RTC_ADDR 				0x68 // Địa chỉ I2C của DS3231
	#define LCD_ADDR 				0x27 // Địa chỉ I2C ghi mặc định của màn LCD16X2

	/**
	 * @brief Khai báo bit điều khiển cho LCD (tùy thuộc vào module LCD cụ thể, có thể cần điều chỉnh)
	 */

	#define LCD_RS_BIT      0x01 // P0
	#define LCD_RW_BIT      0x02 // P1 (Thường nối GND)
	#define LCD_EN_BIT      0x04 // P2
	#define LCD_BL_BIT      0x08 // P3

	/**
	 * @brief Khai báo chân LED 
	 */

	#define LED_GPIO_NUM    2 // GPIO2 thường được tích hợp sẵn với LED trên ESP32 WROOM 32

	/**
	 * @brief Khai báo cấu trúc lấy thông tin thời gian
	 */
	typedef struct {
		uint8_t second;     /*!< 0–59  */
		uint8_t minute;     /*!< 0–59  */
		uint8_t hour;       /*!< 0–23  */
		uint8_t day_of_week;/*!< 1–7 (1 = Chủ nhật) */
		uint8_t date;       /*!< 1–31  */
		uint8_t month;      /*!< 1–12  */
		uint16_t year;      /*!< 2000–2099 */
	} ds3231_time_t;

#endif //__APP_DECL_H__

