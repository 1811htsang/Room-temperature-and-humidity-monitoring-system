idf_component_register(
    SRCS "ds3231.c"
    INCLUDE_DIRS "."
    REQUIRES lib_i2c hal log driver freertos
)