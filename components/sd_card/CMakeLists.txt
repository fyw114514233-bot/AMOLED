idf_component_register(
    SRCS "sd_card.C" "sd_card.c"
    INCLUDE_DIRS "."
    REQUIRES driver spi_flash fatfs sdmmc audio_i2s
)
