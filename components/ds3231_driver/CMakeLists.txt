idf_component_register(
    SRCS "ds3231.c"
    INCLUDE_DIRS "."
    REQUIRES i2c_driver driver freertos
)