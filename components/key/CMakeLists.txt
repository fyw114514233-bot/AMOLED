idf_component_register(
    SRCS "key.c" "key.c "
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2s driver sd_card driver spi_flash fatfs sdmmc audio_i2s minimp3 play_single_mp3 play_single_wav
)
