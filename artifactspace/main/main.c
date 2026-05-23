/**
 * @file main.C
 * @author Huynh Thanh Sang
 * @brief Firmware chính của dự án, thực hiện các chức năng sau:
 * @version 0.1
 * @date 2026-05-01
 * 
 * @copyright GNU GENERAL PUBLIC LICENSE Version 3 (GPLv3) - https://www.gnu.org/licenses/gpl-3.0.en.html
 * 
 */

/**
 * @brief Khai báo các thư viện sử dụng
 */

#include <stdio.h>
#include "ciedpc_core.h"
#include "app_decl.h"
#include "app_cfg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/**
 * @brief Hàm chính của ứng dụng
 */
void app_main(void) {
  printf("[MAIN] ESP32 WROOM 32 Application\n");
  printf("[MAIN] Core Name: %s\n", CIEDPC_CORE_NAME);
  printf("[MAIN] Version: %d.%d\n", CIEDPC_VERSION_MAJOR, CIEDPC_VERSION_MINOR);
  ciedpc_core_init();
  ciedpc_task_norm_create(app_task_table);
	ciedpc_task_poll_create(app_poll_table);
	ciedpc_task_poll_set_ability(CIEDPC_TASK_POLL_MEMRP_ID, true);
  ciedpc_task_poll_set_ability(CIEDPC_TASK_POLL_SYSLF_ID, true);
  ciedpc_msg_t* start_msg = ciedpc_msg_alloc(CIEDPC_TASK_NORM_USR_ID, SIG_USR_START, 0);
  ciedpc_task_norm_post_msg(CIEDPC_TASK_NORM_USR_ID, start_msg);
  while (1) {
    ciedpc_task_scheduler();
    vTaskDelay(pdMS_TO_TICKS(10)); // Sleep for a short time to prevent busy loop, adjust as needed
  }
}