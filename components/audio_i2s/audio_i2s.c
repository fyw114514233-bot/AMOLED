#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/i2s_std.h"


#include "audio_i2s.h"

#define AUDIO_I2S_TAG "AUDIO_I2S"

// ✅ 修复的 DMA 参数（关键：解决卡顿）
#define I2S_DMA_DESC_NUM   50       // ✅ 50 个描述符，更细粒度的缓冲管理
#define I2S_DMA_FRAME_NUM  256      // ✅ 256 字节/帧，刷新周期 ~1.5ms，非常平稳

i2s_chan_handle_t i2s_tx_handle = NULL;
static bool i2s_initialized = false;
static uint32_t current_sample_rate = 0;
static uint16_t current_num_channels = 0;

void i2s_init_dynamic(uint32_t sample_rate, uint16_t num_channels)
{
    // 已初始化且参数相同，直接复用
    if (i2s_initialized && current_sample_rate == sample_rate && current_num_channels == num_channels) {
        ESP_LOGI(AUDIO_I2S_TAG, "I2S reuse: %ldHz, %d ch", sample_rate, num_channels);
        return;
    }

    // 已初始化但参数不同，先禁用
    if (i2s_initialized && i2s_tx_handle != NULL) {
        esp_err_t ret = i2s_channel_disable(i2s_tx_handle);
        if (ret == ESP_OK) {
            ESP_LOGI(AUDIO_I2S_TAG, "I2S disabled");
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

    // 第一次初始化，创建通道时指定 DMA
    if (!i2s_initialized) {
        i2s_chan_config_t chan_cfg = {
            .id = I2S_NUM,
            .role = I2S_ROLE_MASTER,
            .dma_desc_num = I2S_DMA_DESC_NUM,
            .dma_frame_num = I2S_DMA_FRAME_NUM,
        };
        ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &i2s_tx_handle, NULL));
        i2s_initialized = true;
        ESP_LOGI(AUDIO_I2S_TAG, "I2S channel created with optimized DMA: desc=%d, frame=%d",
                 I2S_DMA_DESC_NUM, I2S_DMA_FRAME_NUM);
    }

    // 重新配置 I2S（参数改变时）
    if (current_sample_rate != sample_rate || current_num_channels != num_channels) {
        i2s_slot_mode_t slot_mode = (num_channels == 1) ? I2S_SLOT_MODE_MONO : I2S_SLOT_MODE_STEREO;

        i2s_std_config_t std_cfg = {
            .clk_cfg = {
                .sample_rate_hz = sample_rate,
                .clk_src = I2S_CLK_SRC_XTAL,
                .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            },
            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(BITS_PER_SAMPLE, slot_mode),
            .gpio_cfg = {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = I2S_BCLK,
                .ws   = I2S_LRCK,
                .dout = I2S_SDATA,
                .din  = I2S_GPIO_UNUSED,
                .invert_flags = {
                    .mclk_inv = false,
                    .bclk_inv = false,
                    .ws_inv   = false,
                },
            },
        };

        ESP_ERROR_CHECK(i2s_channel_init_std_mode(i2s_tx_handle, &std_cfg));
        current_sample_rate = sample_rate;
        current_num_channels = num_channels;
        ESP_LOGI(AUDIO_I2S_TAG, "I2S reconfigured: %ldHz, %d ch", sample_rate, num_channels);
    }

    // 启用通道
    esp_err_t ret = i2s_channel_enable(i2s_tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(AUDIO_I2S_TAG, "I2S enable: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(AUDIO_I2S_TAG, "I2S enabled");
    }

    vTaskDelay(pdMS_TO_TICKS(20));
}