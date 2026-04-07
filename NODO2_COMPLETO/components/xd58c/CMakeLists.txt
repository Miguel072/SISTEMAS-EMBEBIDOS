idf_component_register(
    SRCS "xd58c.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_adc driver esp_timer log freertos
)
