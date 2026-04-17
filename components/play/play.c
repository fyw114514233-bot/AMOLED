// ========== play.c ==========
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
#include "play_single_wav.h"      
#include "minimp3.h"
#include "key.h"
#include "esp_heap_caps.h"


#define MINIMP3_IMPLEMENTATION

// ========================================
// 针对 ESP32-S3 内存优化的配置
// ========================================
#define CIRCULAR_BUFFER_SIZE (64 * 1024)
#define I2S_OUTPUT_CHUNK_SIZE 2048
#define MP3_FILE_READ_SIZE 4096

#define MAX_FILES 100
#define MAX_FILENAME_LEN 256

// ========================================
// 环形缓冲结构体
// ========================================
typedef struct {
    uint8_t *buffer;
    volatile uint32_t write_pos;
    volatile uint32_t read_pos;
    volatile uint32_t available;
    SemaphoreHandle_t mutex;
    SemaphoreHandle_t not_empty;
    SemaphoreHandle_t not_full;
} circular_buf_t;

circular_buf_t cbuf = {0};

// ========================================
// 全局变量
// ========================================
wav_playlist_t g_playlist = {0};
uint32_t g_last_sample_rate = 0;
uint16_t g_last_num_channels = 0;
mp3_decode_context_t mp3_ctx = {0};
play_request_t g_play_request = {0};

// ========================================
// 环形缓冲初始化
// ========================================
bool circular_buf_init(void)
{
    if (cbuf.buffer) return true;

    cbuf.buffer = (uint8_t *)heap_caps_malloc(
        CIRCULAR_BUFFER_SIZE,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );

    if (!cbuf.buffer) {
        ESP_LOGE("CIRC", "PSRAM alloc failed!");
        return false;
    }

    cbuf.write_pos = 0;
    cbuf.read_pos  = 0;
    cbuf.available = 0;

    cbuf.mutex     = xSemaphoreCreateMutex();
    cbuf.not_empty = xSemaphoreCreateBinary();
    cbuf.not_full  = xSemaphoreCreateBinary();

    if (!cbuf.mutex || !cbuf.not_empty || !cbuf.not_full) {
        heap_caps_free(cbuf.buffer);
        cbuf.buffer = NULL;
        return false;
    }

    xSemaphoreGive(cbuf.not_full);
    return true;
}
// ========================================
// 环形缓冲写入
// ========================================
bool circular_buf_write(const uint8_t *data, uint32_t len) {
    if (len == 0 || len > CIRCULAR_BUFFER_SIZE) return false;

    if (xSemaphoreTake(cbuf.not_full, pdMS_TO_TICKS(5000)) != pdTRUE) {
        return false;
    }

    xSemaphoreTake(cbuf.mutex, portMAX_DELAY);

    uint32_t free_space = CIRCULAR_BUFFER_SIZE - cbuf.available;
    if (len > free_space) {
        xSemaphoreGive(cbuf.mutex);
        xSemaphoreGive(cbuf.not_full);
        return false;
    }

    uint32_t space_to_end = CIRCULAR_BUFFER_SIZE - cbuf.write_pos;
    
    if (len <= space_to_end) {
        memcpy(&cbuf.buffer[cbuf.write_pos], data, len);
        cbuf.write_pos = (cbuf.write_pos + len) % CIRCULAR_BUFFER_SIZE;
    } else {
        uint32_t part1 = space_to_end;
        uint32_t part2 = len - part1;
        memcpy(&cbuf.buffer[cbuf.write_pos], data, part1);
        memcpy(cbuf.buffer, &data[part1], part2);
        cbuf.write_pos = part2;
    }

    uint32_t prev_available = cbuf.available;
    cbuf.available += len;

    if (prev_available == 0 && cbuf.available > 0) {
        xSemaphoreGive(cbuf.not_empty);
    }

    if (cbuf.available < CIRCULAR_BUFFER_SIZE - MP3_FILE_READ_SIZE) {
        xSemaphoreGive(cbuf.not_full);
    }

    xSemaphoreGive(cbuf.mutex);
    return true;
}

// ========================================
// 环形缓冲读取
// ========================================
uint32_t circular_buf_read(uint8_t *data, uint32_t len) {
    if (len == 0) return 0;

    if (xSemaphoreTake(cbuf.not_empty, pdMS_TO_TICKS(100)) != pdTRUE) {
        return 0;
    }

    xSemaphoreTake(cbuf.mutex, portMAX_DELAY);

    uint32_t to_read = (cbuf.available < len) ? cbuf.available : len;

    if (to_read == 0) {
        xSemaphoreGive(cbuf.mutex);
        xSemaphoreGive(cbuf.not_empty);
        return 0;
    }

    uint32_t space_to_end = CIRCULAR_BUFFER_SIZE - cbuf.read_pos;

    if (to_read <= space_to_end) {
        memcpy(data, &cbuf.buffer[cbuf.read_pos], to_read);
        cbuf.read_pos = (cbuf.read_pos + to_read) % CIRCULAR_BUFFER_SIZE;
    } else {
        uint32_t part1 = space_to_end;
        uint32_t part2 = to_read - part1;
        memcpy(data, &cbuf.buffer[cbuf.read_pos], part1);
        memcpy(&data[part1], cbuf.buffer, part2);
        cbuf.read_pos = part2;
    }

    uint32_t prev_available = cbuf.available;
    cbuf.available -= to_read;

    if (prev_available >= CIRCULAR_BUFFER_SIZE - MP3_FILE_READ_SIZE && 
        cbuf.available < CIRCULAR_BUFFER_SIZE - MP3_FILE_READ_SIZE) {
        xSemaphoreGive(cbuf.not_full);
    }

    if (cbuf.available > 0) {
        xSemaphoreGive(cbuf.not_empty);
    }

    xSemaphoreGive(cbuf.mutex);
    return to_read;
}

uint32_t circular_buf_available(void) {
    xSemaphoreTake(cbuf.mutex, portMAX_DELAY);
    uint32_t avail = cbuf.available;
    xSemaphoreGive(cbuf.mutex);
    return avail;
}

void circular_buf_reset(void) {
    xSemaphoreTake(cbuf.mutex, portMAX_DELAY);
    cbuf.write_pos = 0;
    cbuf.read_pos = 0;
    cbuf.available = 0;
    xSemaphoreGive(cbuf.mutex);

    xSemaphoreTake(cbuf.not_empty, 0);
    xSemaphoreTake(cbuf.not_full, 0);
    xSemaphoreGive(cbuf.not_full);
}

// ========================================
// 工具函数
// ========================================
bool is_audio_file(const char *filename) {
    int len = strlen(filename);
    if (len < 4) return false;
    const char *ext = &filename[len - 4];
    return strcasecmp(ext, ".wav") == 0 || strcasecmp(ext, ".mp3") == 0;
}

bool is_mp3_file(const char *filename) {
    int len = strlen(filename);
    if (len < 4) return false;
    return strcasecmp(&filename[len - 4], ".mp3") == 0;
}

// ========================================
// 目录扫描
// ========================================
bool scan_music_directory(const char *dir_path) {
    FRESULT res;
    FF_DIR dir;
    FILINFO fno;

    g_playlist.filenames = (char **)malloc(MAX_FILES * sizeof(char *));
    if (!g_playlist.filenames) return false;

    g_playlist.capacity = MAX_FILES;
    g_playlist.count = 0;

    res = f_opendir(&dir, dir_path);
    if (res != FR_OK) {
        free(g_playlist.filenames);
        g_playlist.filenames = NULL;
        return false;
    }

    while ((res = f_readdir(&dir, &fno)) == FR_OK && fno.fname[0] && g_playlist.count < MAX_FILES) {
        if (!(fno.fattrib & AM_DIR) && is_audio_file(fno.fname)) {
            char full_path[MAX_FILENAME_LEN];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, fno.fname);

            g_playlist.filenames[g_playlist.count] = malloc(strlen(full_path) + 1);
            if (g_playlist.filenames[g_playlist.count]) {
                strcpy(g_playlist.filenames[g_playlist.count], full_path);
                g_playlist.count++;
            }
        }
    }

    f_closedir(&dir);

    if (g_playlist.count == 0) {
        free(g_playlist.filenames);
        g_playlist.filenames = NULL;
        return false;
    }

    return true;
}

void playlist_free(void) {
    if (g_playlist.filenames) {
        for (int i = 0; i < g_playlist.count; i++) {
            free(g_playlist.filenames[i]);
        }
        free(g_playlist.filenames);
        g_playlist.filenames = NULL;
        g_playlist.count = 0;
    }
}

// ========================================
// MP3 解码任务
// ========================================
void mp3_decode_task(void *arg) {
    FIL fil;
    FRESULT res;
    mp3dec_t mp3d;
    mp3dec_init(&mp3d);

    uint8_t *file_buf = malloc(MP3_FILE_READ_SIZE);
    int16_t *pcm_buf = malloc(MINIMP3_MAX_SAMPLES_PER_FRAME * sizeof(int16_t));
    uint8_t *mp3_buffer = malloc(32 * 1024);

    if (!file_buf || !pcm_buf || !mp3_buffer) {
        vTaskDelete(NULL);
        return;
    }

    int mp3_buffer_len = 0;
    int mp3_buffer_pos = 0;
    mp3dec_frame_info_t info = {0};

    while (1) {
        if (!mp3_ctx.filename) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        res = f_open(&fil, mp3_ctx.filename, FA_READ);
        if (res != FR_OK) {
            mp3_ctx.filename = NULL;
            continue;
        }

        mp3_ctx.stop = false;
        mp3_ctx.pause = false;
        mp3_ctx.total_samples_decoded = 0;
        mp3_buffer_len = mp3_buffer_pos = 0;

        mp3dec_init(&mp3d);

        bool first_frame = true;
        int total_frames = 0;
        int consecutive_errors = 0;

        while (!mp3_ctx.stop) {
            if (mp3_ctx.pause) {
                vTaskDelay(pdMS_TO_TICKS(5));
                continue;
            }

            if (mp3_buffer_pos > 16 * 1024) {
                int remaining = mp3_buffer_len - mp3_buffer_pos;
                if (remaining > 0) {
                    memmove(mp3_buffer, mp3_buffer + mp3_buffer_pos, remaining);
                }
                mp3_buffer_len = remaining;
                mp3_buffer_pos = 0;
            }

            while (mp3_buffer_len < 24 * 1024 && !mp3_ctx.stop && !mp3_ctx.pause) {
                UINT bytes_read = 0;
                res = f_read(&fil, file_buf, MP3_FILE_READ_SIZE, &bytes_read);
                if (bytes_read > 0) {
                    memcpy(mp3_buffer + mp3_buffer_len, file_buf, bytes_read);
                    mp3_buffer_len += bytes_read;
                } else {
                    break;
                }
            }

            if (mp3_buffer_len - mp3_buffer_pos < 512) break;

            int samples = mp3dec_decode_frame(&mp3d, mp3_buffer + mp3_buffer_pos,
                                              mp3_buffer_len - mp3_buffer_pos, pcm_buf, &info);

            if (samples > 0 && info.frame_bytes > 0) {
                if (first_frame) {
                    g_last_sample_rate = info.hz;
                    g_last_num_channels = info.channels;
                    first_frame = false;
                }

                int pcm_bytes = samples * info.channels * sizeof(int16_t);
                mp3_ctx.total_samples_decoded += samples;

                if (!mp3_ctx.stop && !mp3_ctx.pause) {
                    if (!circular_buf_write((uint8_t *)pcm_buf, pcm_bytes)) {
                        vTaskDelay(pdMS_TO_TICKS(2));
                    } else {
                        total_frames++;
                        consecutive_errors = 0;
                    }
                }

                mp3_buffer_pos += info.frame_bytes;
            } else if (info.frame_bytes > 0) {
                mp3_buffer_pos += info.frame_bytes;
                consecutive_errors = 0;
            } else {
                mp3_buffer_pos++;
                consecutive_errors++;

                if (consecutive_errors > 50) {
                    mp3_buffer_pos += 64;
                    if (mp3_buffer_pos >= mp3_buffer_len) break;
                    consecutive_errors = 0;
                }
            }

            if (total_frames % 100 == 0) {
                taskYIELD();
            }
        }

        f_close(&fil);
        mp3_ctx.filename = NULL;

        int wait_count = 0;
        while (circular_buf_available() > 0 && !mp3_ctx.stop && wait_count < 1000) {
            vTaskDelay(pdMS_TO_TICKS(10));
            wait_count++;
        }
    }

    free(file_buf);
    free(pcm_buf);
    free(mp3_buffer);
    vTaskDelete(NULL);
}

// ========================================
// I2S 输出任务
// ========================================
extern esp_err_t i2s_channel_disable(i2s_chan_handle_t handle);
extern esp_err_t i2s_channel_enable(i2s_chan_handle_t handle);

void i2s_output_task(void *arg) {
    uint8_t *output_buf = malloc(I2S_OUTPUT_CHUNK_SIZE);
    if (!output_buf) {
        vTaskDelete(NULL);
        return;
    }

    int write_count = 0;
    unsigned long last_log_time = xTaskGetTickCount();
    bool was_paused = false;
    bool i2s_enabled = true;

    while (1) {
        if (mp3_ctx.pause) {
            if (!was_paused) {
                ESP_LOGI("I2S_OUT", "Pause detected - disabling I2S channel");
                
                if (i2s_tx_handle != NULL && i2s_enabled) {
                    esp_err_t ret = i2s_channel_disable(i2s_tx_handle);
                    if (ret == ESP_OK) {
                        ESP_LOGI("I2S_OUT", "I2S channel disabled successfully");
                        i2s_enabled = false;
                    } else {
                        ESP_LOGE("I2S_OUT", "Failed to disable I2S channel: %d", ret);
                    }
                }
                
                ESP_LOGI("I2S_OUT", "Clearing buffer to prevent replay (available: %d bytes)", 
                         circular_buf_available());
                circular_buf_reset();
                
                was_paused = true;
            }
            
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        } else if (was_paused) {
            ESP_LOGI("I2S_OUT", "Resume detected - waiting for buffer preload");
            
            int preload_wait = 0;
            int preload_threshold = I2S_OUTPUT_CHUNK_SIZE * 2;
            
            while (circular_buf_available() < preload_threshold && preload_wait < 200) {
                if (preload_wait % 20 == 0) {
                    ESP_LOGI("I2S_OUT", "Preloading... buffer: %d/%d bytes", 
                             circular_buf_available(), preload_threshold);
                }
                vTaskDelay(pdMS_TO_TICKS(10));
                preload_wait++;
            }
            
            if (circular_buf_available() >= preload_threshold) {
                ESP_LOGI("I2S_OUT", "Preload complete (%d bytes), resuming playback", 
                         circular_buf_available());
            } else {
                ESP_LOGW("I2S_OUT", "Preload incomplete (%d bytes), resuming anyway", 
                         circular_buf_available());
            }
            
            if (i2s_tx_handle != NULL && !i2s_enabled) {
                esp_err_t ret = i2s_channel_enable(i2s_tx_handle);
                if (ret == ESP_OK) {
                    ESP_LOGI("I2S_OUT", "I2S channel enabled successfully");
                    i2s_enabled = true;
                } else {
                    ESP_LOGE("I2S_OUT", "Failed to enable I2S channel: %d", ret);
                }
            }
            
            was_paused = false;
        }

        if (!mp3_ctx.pause && i2s_enabled) {
            uint32_t bytes_read = circular_buf_read(output_buf, I2S_OUTPUT_CHUNK_SIZE);

            if (bytes_read > 0 && i2s_tx_handle != NULL) {
                size_t bytes_written = 0;
                i2s_channel_write(i2s_tx_handle, output_buf, bytes_read,
                                  &bytes_written, pdMS_TO_TICKS(3000));
                write_count++;
            }
        }

        unsigned long now = xTaskGetTickCount();
        if (now - last_log_time > pdMS_TO_TICKS(10000)) {
            write_count = 0;
            last_log_time = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    free(output_buf);
    vTaskDelete(NULL);
}

// ========================================
// 按键控制任务
// ========================================
void key_control_task(void *arg) {
    const int debounce_delay = 10;
    static bool last_next = false;
    static bool last_prev = false;
    static bool last_play = false;

    ESP_LOGI("KEY_CTRL", "Key control task started");

    while (1) {
        if (g_key_next_flag && !last_next) {
            ESP_LOGI("KEY_CTRL", "[KEY_NEXT] Button pressed! Current index: %d", 
                     g_playlist.current_index);
            g_key_next_flag = false;
            vTaskDelay(pdMS_TO_TICKS(debounce_delay));
            
            g_play_request.need_next = true;
            ESP_LOGI("KEY_CTRL", "[KEY_NEXT] Flag set, will switch to next track");
            last_next = true;
        } else if (!g_key_next_flag && last_next) {
            last_next = false;
        }

        if (g_key_prev_flag && !last_prev) {
            ESP_LOGI("KEY_CTRL", "[KEY_PREV] Button pressed! Current index: %d", 
                     g_playlist.current_index);
            g_key_prev_flag = false;
            vTaskDelay(pdMS_TO_TICKS(debounce_delay));
            
            g_play_request.need_prev = true;
            ESP_LOGI("KEY_CTRL", "[KEY_PREV] Flag set, will switch to previous track");
            last_prev = true;
        } else if (!g_key_prev_flag && last_prev) {
            last_prev = false;
        }

        if (g_key_play_flag && !last_play) {
            ESP_LOGI("KEY_CTRL", "[KEY_PLAY] Button pressed! Current pause state: %s", 
                     mp3_ctx.pause ? "paused" : "playing");
            g_key_play_flag = false;
            vTaskDelay(pdMS_TO_TICKS(debounce_delay));
            
            mp3_ctx.pause = !mp3_ctx.pause;
            
            if (mp3_ctx.pause) {
                ESP_LOGI("KEY_CTRL", "[KEY_PLAY] Music paused");
                g_play_request.need_pause = true;
            } else {
                ESP_LOGI("KEY_CTRL", "[KEY_PLAY] Music resumed");
                g_play_request.need_pause = false;
            }
            
            last_play = true;
        } else if (!g_key_play_flag && last_play) {
            last_play = false;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ========================================
// 播放循环（保留用于向后兼容）
// ========================================
void auto_play_music_loop(const char *dir_path) {
    ESP_LOGW(WAV_TAG, "auto_play_music_loop() is deprecated!");
    ESP_LOGW(WAV_TAG, "Please use the new task-based architecture in main.c");
    
    if (!circular_buf_init()) {
        ESP_LOGE(WAV_TAG, "Failed to initialize circular buffer");
        return;
    }
    
    if (!scan_music_directory(dir_path)) {
        ESP_LOGE(WAV_TAG, "Failed to scan music directory");
        return;
    }
    
    if (g_playlist.count == 0) {
        ESP_LOGE(WAV_TAG, "No audio files found in directory");
        return;
    }

    ESP_LOGI(WAV_TAG, "Found %d audio files", g_playlist.count);
    g_playlist.is_playing = true;
    g_playlist.current_index = 0;

    while (g_playlist.is_playing) {
        if (g_playlist.current_index >= g_playlist.count) {
            g_playlist.current_index = 0;
        } else if (g_playlist.current_index < 0) {
            g_playlist.current_index = g_playlist.count - 1;
        }

        const char *filename = g_playlist.filenames[g_playlist.current_index];

        ESP_LOGI(WAV_TAG, "\n========== TRACK %d/%d ==========", 
                 g_playlist.current_index + 1, g_playlist.count);
        ESP_LOGI(WAV_TAG, "Playing: %s", filename);

        int index_before_play = g_playlist.current_index;

        g_play_request.need_next = false;
        g_play_request.need_prev = false;

        if (is_mp3_file(filename)) {
            ESP_LOGI(WAV_TAG, "Format: MP3");
            play_single_mp3(filename);
        } else {
            ESP_LOGI(WAV_TAG, "Format: WAV");
            play_single_wav(filename);
        }

        if (g_play_request.need_next || g_play_request.need_prev) {
            ESP_LOGW(WAV_TAG, "Unhandled key flag detected, clearing");
            g_play_request.need_next = false;
            g_play_request.need_prev = false;
        }

        if (g_playlist.current_index == index_before_play) {
            ESP_LOGI(WAV_TAG, "Playback completed naturally, auto-advancing to next track");
            g_playlist.current_index++;
        } else {
            ESP_LOGI(WAV_TAG, "Index was changed by button (before: %d, after: %d), using new index",
                     index_before_play, g_playlist.current_index);
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    ESP_LOGI(WAV_TAG, "Playback stopped");
}


void stop_music(void) {
    mp3_ctx.stop = true;
    g_playlist.is_playing = false;
}