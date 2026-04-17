#include <string.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"

#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "ff.h"

#include "sd_card.h"
#include "audio_i2s.h"
#include "play.h"
#include "play_single_mp3.h"
#include "minimp3.h"
#include "key.h"

#define WAV_TAG "AUDIO_PLAYER"



bool play_single_mp3(const char *filename) {
    g_last_sample_rate = 0;
    g_last_num_channels = 0;
    circular_buf_reset();

    mp3_ctx.filename = filename;
    mp3_ctx.stop = false;
    mp3_ctx.pause = false;
    mp3_ctx.total_samples_decoded = 0;

    // ★ 关键修复 1：等待解码任务解析出第一帧信息，同时检查按键
    int timeout = 200;
    while (timeout-- > 0 && (g_last_sample_rate == 0 || g_last_num_channels == 0)) {
        // ★ 新增：在初始化期间，也要处理按键并修改索引
        if (g_play_request.need_next) {
            ESP_LOGI(WAV_TAG, "KEY_NEXT pressed during initialization");
            ESP_LOGI(WAV_TAG, "Current index before: %d", g_playlist.current_index);
            
            g_play_request.need_next = false;
            g_playlist.current_index = (g_playlist.current_index + 1) % g_playlist.count;
            
            ESP_LOGI(WAV_TAG, "Index advanced to: %d/%d", 
                     g_playlist.current_index + 1, g_playlist.count);
            
            mp3_ctx.stop = true;
            mp3_ctx.filename = NULL;
            return true;  // ★ 改为 return true，表示索引已被修改
        }

        if (g_play_request.need_prev) {
            ESP_LOGI(WAV_TAG, "KEY_PREV pressed during initialization");
            ESP_LOGI(WAV_TAG, "Current index before: %d", g_playlist.current_index);
            
            g_play_request.need_prev = false;
            g_playlist.current_index = (g_playlist.current_index - 1 + g_playlist.count) % g_playlist.count;
            
            ESP_LOGI(WAV_TAG, "Index rewound to: %d/%d", 
                     g_playlist.current_index + 1, g_playlist.count);
            
            mp3_ctx.stop = true;
            mp3_ctx.filename = NULL;
            return true;  // ★ 改为 return true，表示索引已被修改
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (g_last_sample_rate == 0 || g_last_num_channels == 0) {
        mp3_ctx.stop = true;
        mp3_ctx.filename = NULL;
        return false;
    }

    i2s_init_dynamic(g_last_sample_rate, g_last_num_channels);
    vTaskDelay(pdMS_TO_TICKS(100));

    uint32_t last_decoded_samples = 0;
    int no_progress_count = 0;
    int check_count = 0;

    // ========== 主播放检查循环 ==========
    while (1) {
        // ★ 关键修复 2：播放中的按键处理，同样修改索引
        // 检查"下一首"按键
        if (g_play_request.need_next) {
            ESP_LOGI(WAV_TAG, "*** NEXT BUTTON DETECTED (during playback) ***");
            ESP_LOGI(WAV_TAG, "Current index before: %d", g_playlist.current_index);
            
            g_play_request.need_next = false;
            g_playlist.current_index = (g_playlist.current_index + 1) % g_playlist.count;
            
            ESP_LOGI(WAV_TAG, "Index advanced to: %d/%d", 
                     g_playlist.current_index + 1, g_playlist.count);
            
            mp3_ctx.stop = true;
            mp3_ctx.filename = NULL;
            return true;
        }

        // 检查"上一首"按键
        if (g_play_request.need_prev) {
            ESP_LOGI(WAV_TAG, "*** PREV BUTTON DETECTED (during playback) ***");
            ESP_LOGI(WAV_TAG, "Current index before: %d", g_playlist.current_index);
            
            g_play_request.need_prev = false;
            g_playlist.current_index = (g_playlist.current_index - 1 + g_playlist.count) % g_playlist.count;
            
            ESP_LOGI(WAV_TAG, "Index rewound to: %d/%d", 
                     g_playlist.current_index + 1, g_playlist.count);
            
            mp3_ctx.stop = true;
            mp3_ctx.filename = NULL;
            return true;
        }

        // ★ 新增：暂停时不计数，避免超时
        if (mp3_ctx.pause) {
            ESP_LOGI(WAV_TAG, "Music paused at sample: %d", mp3_ctx.total_samples_decoded);
            vTaskDelay(pdMS_TO_TICKS(100));
            no_progress_count = 0;
            continue;
        }

        // 延迟一段时间再检查其他条件
        vTaskDelay(pdMS_TO_TICKS(100));
        check_count++;

        uint32_t current_samples = mp3_ctx.total_samples_decoded;

        // 如果解码任务已将文件名设为 NULL（表示文件读取/解码结束）
        if (mp3_ctx.filename == NULL) {
            ESP_LOGI(WAV_TAG, "Decode task signaled end of file");
            break;
        }

        // 检查解码进度
        if (current_samples == last_decoded_samples) {
            no_progress_count++;
        } else {
            no_progress_count = 0;
            last_decoded_samples = current_samples;
        }

        // 如果连续 30 次（3秒）没有解码进度，且环形缓冲已空，则认为播放结束
        if (no_progress_count >= 30 && circular_buf_available() == 0) {
            ESP_LOGI(WAV_TAG, "No decode progress, assuming end of file");
            break;
        }

        // 硬性超时
        if (check_count >= 6000) {
            ESP_LOGW(WAV_TAG, "Hard timeout reached");
            break;
        }
    }

    // 收尾处理
    mp3_ctx.stop = true;
    mp3_ctx.pause = false;

    // 等待 I2S 输出任务消耗完环形缓冲中剩余的数据
    int drain_wait = 0;
    while (circular_buf_available() > 0 && drain_wait < 600) {
        vTaskDelay(pdMS_TO_TICKS(10));
        drain_wait++;
    }

    vTaskDelay(pdMS_TO_TICKS(200));
    return true;  // ★ 修复：如果正常播放完成，也要返回 true
}