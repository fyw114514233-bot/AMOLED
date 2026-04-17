#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

#include "sd_card.h"

#define SD_CARD_TAG "SD_CARD"

sdmmc_card_t *card;

esp_err_t sd_card_init(void) {
    esp_err_t ret;

    // ===============================================
    // SDIO (SDMMC) 模式配置
    // ===============================================
    
    // 1. 配置 SDIO 主机
    // SDMMC_HOST_DEFAULT() 默认已经将 slot 配置为 SDMMC_HOST_SLOT_1，对于 ESP32-S3 这是最标准的做法
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    
    // 移除下面这行！不要强制改为 SLOT_0
    // host.slot = SDMMC_HOST_SLOT_0; 
    

    host.max_freq_khz = 80000;  
    
    // 2. 配置 SDIO 总线引脚
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    
    // 明确设置位宽为 4线模式 (非常重要)
    slot_config.width = 4;
    
    // 配置数据引脚 (ESP32-S3 支持任意 GPIO 映射)
    slot_config.clk = PIN_NUM_CLK;   // 
    slot_config.cmd = PIN_NUM_CMD;   // 
    slot_config.d0  = PIN_NUM_DAT0;  // 
    slot_config.d1  = PIN_NUM_DAT1;  // 
    slot_config.d2  = PIN_NUM_DAT2;  // 
    slot_config.d3  = PIN_NUM_DAT3;  //
    
    // 开启内部上拉（注意：硬件电路上 CMD 和 D0~D3 依然强烈建议加上 10K 外部上拉电阻）
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    
    // 3. 挂载配置
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };
    
    // 4. 挂载 SD 卡
    ret = esp_vfs_fat_sdmmc_mount("/music", &host, &slot_config, &mount_config, &card);
    
    if (ret != ESP_OK) {
        ESP_LOGE(SD_CARD_TAG, "Mount failed: %s", esp_err_to_name(ret));
        
        if (ret == ESP_ERR_INVALID_STATE) {
            ESP_LOGE(SD_CARD_TAG, "Error: Invalid state - check pin configuration");
        } else if (ret == ESP_ERR_NO_MEM) {
            ESP_LOGE(SD_CARD_TAG, "Error: Not enough memory");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(SD_CARD_TAG, "Error: SD card not found - check connection");
        }
        
        return ret;
    }
    
    // 5. 获取 SD 卡信息并打印
    sdmmc_card_print_info(stdout, card);
    
    ESP_LOGI(SD_CARD_TAG, "Mounted at /music");
    return ESP_OK;
}