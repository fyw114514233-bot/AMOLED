idf_component_register(
    SRCS "audio_i2s.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2s driver sd_card
                              
)
