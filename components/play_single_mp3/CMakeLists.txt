idf_component_register(
    SRCS "play_single_mp3.c" "play_single_mp3.c "
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2s driver sd_card driver spi_flash fatfs sdmmc audio_i2s minimp3 play key
)
