#ifndef PLAY_H
#define PLAY_H

#include <stdbool.h>
#include <stdint.h>
#include "ff.h"

// ========================================
// 宏定义
// ========================================

#define WAV_TAG "AUDIO_PLAYER"

// ========================================
// WAV 文件头结构
// ========================================

typedef struct {
    uint8_t  riff_id[4];        // "RIFF"
    uint32_t riff_size;
    uint8_t  wave_id[4];        // "WAVE"
    uint8_t  fmt_id[4];         // "fmt "
    uint32_t fmt_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint8_t  data_id[4];        // "data"
    uint32_t data_size;
} wav_header_t;

// ========================================
// MP3 解码上下文
// ========================================
typedef struct {
    const char *filename;
    bool stop;
    bool pause;             // ★ 暂停标志
    uint32_t total_samples_decoded;
} mp3_decode_context_t;

// ========================================
// 播放列表结构
// ========================================
typedef struct {
    char **filenames;
    int count;
    int capacity;
    int current_index;
    bool is_playing;
} wav_playlist_t;

// ========================================
// 播放控制请求（支持暂停）
// ========================================
typedef struct {
    bool need_next;
    bool need_prev;
    bool need_pause;        // ★ 暂停切换标志
} play_request_t;

// ========================================
// 外部全局变量
// ========================================

extern wav_playlist_t g_playlist;
extern uint32_t g_last_sample_rate;
extern uint16_t g_last_num_channels;
extern mp3_decode_context_t mp3_ctx;
extern play_request_t g_play_request;

// ========================================
// 环形缓冲函数声明
// ========================================

/**
 * 初始化环形缓冲
 * @return true 初始化成功，false 初始化失败
 */
bool circular_buf_init(void);

/**
 * 重置环形缓冲
 */
void circular_buf_reset(void);

/**
 * 获取缓冲区中可用数据量
 * @return 可用数据字节数
 */
uint32_t circular_buf_available(void);

/**
 * 环形缓冲写入
 */
bool circular_buf_write(const uint8_t *data, uint32_t len);

/**
 * 环形缓冲读取
 */
uint32_t circular_buf_read(uint8_t *data, uint32_t len);

// ========================================
// 目录扫描函数声明
// ========================================

/**
 * 扫描音乐目录
 * @param dir_path 目录路径
 * @return true 扫描成功，false 扫描失败
 */
bool scan_music_directory(const char *dir_path);

/**
 * 释放播放列表
 */
void playlist_free(void);

// ========================================
// 文件格式判断函数声明
// ========================================

/**
 * 判断是否为 MP3 文件
 * @param filename 文件名
 * @return true 是 MP3 文件，false 不是
 */
bool is_mp3_file(const char *filename);

/**
 * 判断是否为音频文件（WAV 或 MP3）
 * @param filename 文件名
 * @return true 是音频文件，false 不是
 */
bool is_audio_file(const char *filename);

// ========================================
// 播放函数声明
// 注意：返回 bool 表示操作是否成功
// ========================================

/**
 * 播放单个 MP3 文件
 * @param filename 文件路径
 * @return true 播放成功，false 播放失败
 */
bool play_single_mp3(const char *filename);

/**
 * 播放单个 WAV 文件
 * @param filename 文件路径
 * @return true 播放成功，false 播放失败
 */
bool play_single_wav(const char *filename);

/**
 * 播放循环（已弃用 - 使用 music_loop_task 代替）
 * @param dir_path 目录路径
 */
void auto_play_music_loop(const char *dir_path);

/**
 * 停止播放
 */
void stop_music(void);

// ========================================
// RTOS 任务函数声明
// ========================================

/**
 * MP3 解码任务
 * 负责读取 MP3 文件、解码为 PCM、写入环形缓冲
 */
void mp3_decode_task(void *arg);

/**
 * I2S 输出任务
 * 负责从环形缓冲读取 PCM 数据、通过 I2S 输出到扬声器
 */
void i2s_output_task(void *arg);

/**
 * 按键控制任务
 * 负责检测按键、设置播放控制标志
 */
void key_control_task(void *arg);


#endif // PLAY_H