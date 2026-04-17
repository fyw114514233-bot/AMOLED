// ============================================
// FILE: key.h - 增强版（支持播放/暂停状态查询）
// ============================================
#ifndef KEY_H
#define KEY_H

#include <stdbool.h>
#include <stdint.h>

// ========================================
// GPIO 按键引脚定义
// ========================================

#define GPIO_KEY_NEXT      48   //  下一首 (灵敏，无需改动)
#define GPIO_KEY_PLAY      47   //  播放/暂停
#define GPIO_KEY_PREV      21   //  上一首 


// ========================================
// 外部全局变量 (中断标志位)
// ========================================

extern volatile bool g_key_next_flag;
extern volatile bool g_key_play_flag;
extern volatile bool g_key_prev_flag;

// ========================================
// 核心接口函数
// ========================================

/**
 * @brief 初始化按键控制
 * @return true 成功, false 失败
 */
bool key_init(void);

// ========================================
// 新增：播放状态查询接口
// ========================================

/**
 * @brief 检查当前是否在播放（非暂停状态）
 * @return true 正在播放, false 暂停中
 */
bool key_is_playing(void);

/**
 * @brief 获取播放/暂停标志
 * @return true 标志已设置
 */
bool key_get_play_flag(void);

/**
 * @brief 清除播放/暂停标志
 */
void key_clear_play_flag(void);

#endif // KEY_H