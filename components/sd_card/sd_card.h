#ifndef __SD_CARD_H__
#define __SD_CARD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "sdmmc_cmd.h"
#include "driver/gpio.h"

/* ========================================
 * SD 卡 SDMMC 模式引脚配置
 * 对应 ESP32-S3 的 SDMMC 控制器引脚
 * ======================================== */
#define PIN_NUM_CMD     GPIO_NUM_1
#define PIN_NUM_CLK     GPIO_NUM_2
#define PIN_NUM_DAT0    GPIO_NUM_3
#define PIN_NUM_DAT1    GPIO_NUM_4
#define PIN_NUM_DAT2    GPIO_NUM_5
#define PIN_NUM_DAT3    GPIO_NUM_6

/* ========================================
 * 外部变量声明
 * ======================================== */
extern sdmmc_card_t *card;

/* ========================================
 * 函数声明
 * ======================================== */

/**
 * @brief 初始化 SD 卡（SDMMC 4-bit 模式）
 * @return ESP_OK 成功，其他值失败
 */
esp_err_t sd_card_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __SD_CARD_H__ */