file(GLOB_RECURSE SRC_FILES
    "./*.c"
    "./*.cpp"      # 确保包含 cpp
    "./my_ui/*.c"
    "./my_ui/*.cpp"
    "./Configure/*.c"
    "./Configure/*.cpp"
)

list(FILTER SRC_FILES EXCLUDE REGEX ".*/my_ui/ui_img_.*\\.c$")

idf_component_register(
    SRCS ${SRC_FILES}
    INCLUDE_DIRS "." "./my_ui"
    INCLUDE_DIRS "." "./Configure"
    REQUIRES

        driver
        esp_timer
        esp_driver_ledc
        esp_lcd
        #系统组件

        key
        minimp3
        play
        play_single_wav
        play_single_mp3
        sd_card
        lvgl
        Axp2101
        Display
        Touch



)

target_compile_options(${COMPONENT_LIB} PRIVATE -O2)
target_compile_definitions(${COMPONENT_LIB} PRIVATE LV_FONT_SIMSUN_16_CJK=1)

idf_component_get_property(lvgl_component_lib lvgl COMPONENT_LIB)
if(lvgl_component_lib)
    target_compile_options(${lvgl_component_lib} PRIVATE -O2)
    target_compile_definitions(${lvgl_component_lib} PUBLIC LV_FONT_SIMSUN_16_CJK=1)
endif()

idf_component_get_property(display_component_lib Display COMPONENT_LIB)
if(display_component_lib)
    target_compile_options(${display_component_lib} PRIVATE -O2)
endif()
