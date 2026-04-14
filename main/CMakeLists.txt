idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES reset_driver i2c_driver ds3231_driver freertos
)