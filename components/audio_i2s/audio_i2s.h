#ifndef __AUDIO_I2S_H__
#define __AUDIO_I2S_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2s_std.h"

/* ========================================
 * I2S 配置
 * ======================================== */

#define I2S_NUM I2S_NUM_0
#define BITS_PER_SAMPLE 16
#define AUDIO_BUFFER_SIZE 4096

#define I2S_BCLK 9
#define I2S_LRCK 46
#define I2S_SDATA 10

/* ========================================
 * 外部变量
 * ======================================== */

extern i2s_chan_handle_t i2s_tx_handle;

/* ========================================
 * 函数声明
 * ======================================== */

/**
 * @brief 初始化 I2S（动态配置采样率和声道）
 * @param sample_rate 采样率（Hz）
 * @param num_channels 声道数（1 或 2）
 */
void i2s_init_dynamic(uint32_t sample_rate, uint16_t num_channels);

#ifdef __cplusplus
}
#endif

#endif /* __AUDIO_I2S_H__ */