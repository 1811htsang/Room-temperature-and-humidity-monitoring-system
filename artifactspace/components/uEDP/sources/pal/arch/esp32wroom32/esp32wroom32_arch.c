/**
 * @file esp32wroom32_arch.c
 * @author Shang Huang
 * @brief Implementation of ESP32-WROOM-32 Architecture Abstraction Layer for CIEDPC
 * @version 0.1
 * @date 2026-04-20
 * @copyright MIT License
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include "esp32wroom32_arch.h"
#include "ciedpc_core.h"
#include "ciedpc_task.h"
#include "ciedpc_msg.h"
#include "ciedpc_timer.h"
#include "pal_memrp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_heap_caps.h"
#include "esp_ota_ops.h"
#include "esp_image_format.h"

/**
 * @brief Khai báo spinlock để bảo vệ trong môi trường đa nhân
 */

static portMUX_TYPE ciedpc_mux = portMUX_INITIALIZER_UNLOCKED;

/**
 * @brief Khai báo biến toàn cục kiểm tra hệ thống khởi động
 */

sta ui8 system_inited = 0;

/**
 * @brief Đảm bảo ciedpc_timer_tick được biết đến
 */

extern void ciedpc_timer_tick(void);

/**
 * @brief Implementation cho ciedpc_core.h
 */

void ciedpc_core_init(void) {
  pal_core_init();
  ciedpc_msg_pool_init();
  ciedpc_timer_init();
  system_inited = 1;
}

/**
 * @brief Implementation cho pal_core.h
 */

void pal_core_init(void) {
  pal_esp32_wroom32_init_env();
}

void pal_enter_critical(void) {
  taskENTER_CRITICAL(&ciedpc_mux);
}

void pal_exit_critical(void) {
  taskEXIT_CRITICAL(&ciedpc_mux);
}

ui8 pal_math_get_highest_bit16(ui16 mask) {
  if (mask == 0) return 0xFF;
  // sử dụng tập lệnh của gcc
  return (ui8)(31 - __builtin_clz((uint32_t)mask));
}

ui32 pal_sys_get_tick(void) {
  return (ui32)(esp_timer_get_time() / 1000);
}

void pal_sys_reset(void) {
	esp_restart();
}

void pal_sys_fatal(const char* file, ui32 line, const char* msg) {
  printf("FATAL ERROR at %s:%" PRIu32 ": %s\n", file, line, msg);
  pal_sys_reset();
}

/**
 * @brief Implementation cho esp32wroom32_arch.h
 */

void timer_callback(void* arg) {
  if (system_inited) {
    ciedpc_timer_tick();
  }
}

void pal_esp32_wroom32_init_env(void) {
  printf("Initializing ESP32-WROOM-32 environment for CIEDPC...\n");
  const esp_timer_create_args_t periodic_timer_args = {
    .callback = &timer_callback,
    .name = "ciedpc_tick"
  };
  esp_timer_handle_t periodic_timer;
  esp_timer_create(&periodic_timer_args, &periodic_timer);
  esp_timer_start_periodic(periodic_timer, 1000); // 1000us = 1ms
}

void pal_esp32_wroom32_idle_sleep(void) {
	esp_light_sleep_start();
}

void pal_memrp_get_sys_info(ui32 *rom_used, ui32 *ram_used, ui32 *stack_curr) {
  /* 1. Lấy thông tin ROM (Flash - Kích thước của App hiện tại) */
  if (rom_used) {
    const esp_partition_t *running = esp_ota_get_running_partition();
    *rom_used = (ui32)running->size;
  }

  /* 2. Lấy thông tin RAM đang sử dụng (Internal RAM) */
  if (ram_used) {
    size_t total_internal = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    // RAM đang dùng = Tổng RAM nội bộ - RAM còn trống
    *ram_used = (ui32)(total_internal - free_internal);
  }

  /* 3. Lấy thông tin Stack hiện tại của Task đang chạy CIEDPC */
  if (stack_curr) {
    TaskStatus_t xTaskDetails;
    // Lấy thông tin của chính task hiện tại (NULL)
    vTaskGetInfo(NULL, &xTaskDetails, pdTRUE, eInvalid);
    
    /* Tính toán độ sâu stack đã dùng (ESP32 stack mọc từ cao xuống thấp) */
    // Lưu ý: Tùy cách cấu hình, nhưng thông thường:
    *stack_curr = (ui32)(xTaskDetails.usStackHighWaterMark);
  }
}


