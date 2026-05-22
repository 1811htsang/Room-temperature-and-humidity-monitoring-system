# 1. Định nghĩa các file nguồn chung cho tất cả kiến trúc
file(
  GLOB_RECURSE CORE_SRCS
  "sources/core/src/*.c"
)
file(
  GLOB COMMON_CONTAINER_FIFO_SRCS
  "sources/common/container/fifo/*.c" 
)
file(
  GLOB APP_SRCS
  "sources/app/app.c"
)
file(
  GLOB_RECURSE PAL_SERVICE_MEMRP_SRCS
  "sources/pal/service/memrp/*.c"
)
file(
  GLOB PAL_CORE_SRCS 
  "sources/pal/*.c"
)

# 2. CHỈ CHỌN file nguồn cho kiến trúc ESP32-S3
file(
  GLOB PAL_ARCH_SRCS 
  "sources/pal/arch/esp32wroom32/*.c"
)

# 3. Đăng ký Component với hệ thống build của ESP-IDF
idf_component_register(
  SRCS
    ${APP_SRCS}
    ${COMMON_CONTAINER_FIFO_SRCS}
    ${CORE_SRCS}
    ${PAL_SERVICE_MEMRP_SRCS}
    ${PAL_ARCH_SRCS}
    ${PAL_CORE_SRCS}
  INCLUDE_DIRS
    "sources/common/container/fifo"
    "sources/core/inc"
    "sources/pal"
    "sources/pal/service/memrp"
    "sources/pal/arch/esp32wroom32"
    "sources/app/config"
    "sources/app/declaration"
  PRIV_INCLUDE_DIRS
    "sources/core/src"
  REQUIRES
    app_update                  # Cần cho OTA
    esp_app_format              # Cần cho OTA
    bootloader_support          # Cần cho OTA
    esp_timer                   # Cần cho ciedpc_timer_tick
    driver                      # Cần cho GPIO/UART/I2C
    freertos                    # Cần cho task/sleep
  WHOLE_ARCHIVE
)

# 4. Định nghĩa Macro toàn cục để Code biết đang chạy trên ESP32
target_compile_definitions(${COMPONENT_LIB} PUBLIC CIEDPC_PLATFORM_ESP32)