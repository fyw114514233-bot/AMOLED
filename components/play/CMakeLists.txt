idf_component_register(
    SRCS 
        "play.c"
    INCLUDE_DIRS 
        "."
    REQUIRES
        freertos
        esp_timer
        esp_hw_support
        log
        esp_driver_i2s 
        fatfs 
        sdmmc 
        audio_i2s 
        minimp3 
        play_single_mp3 
        play_single_wav 
        key 
      

)
