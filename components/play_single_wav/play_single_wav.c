#include <string.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "ff.h" // FatFs 文件系统库

#include "sd_card.h"
#include "audio_i2s.h" // I2S 驱动相关头文件
#include "play.h"      // 播放器自身头文件，包含结构体定义等
#include "play_single_wav.h"      // 播放器自身头文件，包含结构体定义等

#include "minimp3.h" // minimp3 解码库
#include "key.h"

// 播放单个 WAV 文件 (简单阻塞式实现)
bool play_single_wav(const char *filename) {
    FIL fil;
    FRESULT res = f_open(&fil, filename, FA_READ);
    if (res != FR_OK) return false;

    // 读取 WAV 文件头
    wav_header_t header;
    UINT bytes_read = 0;
    f_read(&fil, &header, sizeof(wav_header_t), &bytes_read);

    // 检查 WAV 格式（PCM 格式，16位，单声道或双声道）
    if (header.audio_format != 1 || header.bits_per_sample != 16 ||
        (header.num_channels != 1 && header.num_channels != 2)) {
        f_close(&fil);
        return false;
    }

    // 配置 I2S
    i2s_init_dynamic(header.sample_rate, header.num_channels);
    f_lseek(&fil, 44); // 跳转到数据块 (假设头文件大小为44字节)

    // 分配 WAV 数据读取缓冲区
    uint8_t *buffer = malloc(4096);
    if (!buffer) {
        f_close(&fil);
        return false;
    }

    UINT bytes_read_actual;
    size_t bytes_written;
    // 循环读取 WAV 数据并直接写入 I2S
    while (f_read(&fil, buffer, 4096, &bytes_read_actual) == FR_OK && bytes_read_actual > 0) {
        i2s_channel_write(i2s_tx_handle, buffer, bytes_read_actual, &bytes_written, pdMS_TO_TICKS(1000));
    }

    free(buffer);
    f_close(&fil);
    vTaskDelay(pdMS_TO_TICKS(100)); // 稍等 I2S 缓冲区排空
    return true;
}
