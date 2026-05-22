/**
 * @file esp32_wroom32_arch.h
 * @author Shang Huang
 * @brief Header file for ESP32-WROOM-32 Architecture Abstraction Layer in CIEDPC
 * @version 0.1
 * @date 2026-04-20
 * @copyright MIT License
 */
#ifndef __ESP32_WROOM32_ARCH_H__
  #define __ESP32_WROOM32_ARCH_H__

  #include "pal_core.h"

  /**
   * @brief Khởi tạo môi trường làm việc trên ESP32-WROOM-32 cho Core hoạt động
   */
  void pal_esp32_wroom32_init_env(void);

  /**
   * @brief Hàm này sẽ được gọi khi Core không có tác vụ nào để chạy
   */
  void pal_esp32_wroom32_idle_sleep(void);

  /**
   * @brief Các cấu hình khác sẽ được bổ sung tùy thuộc vào nhu cầu riêng của từng dự án.
   */

#endif // __ESP32_WROOM32_ARCH_H__
