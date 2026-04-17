// ============================================
// 文件: key.c - 增强版本（支持播放/暂停）
// ============================================

#include "key.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "play.h"

static const char *TAG = "KEY_ISR";

volatile bool g_key_next_flag = false;
volatile bool g_key_play_flag = false;
volatile bool g_key_prev_flag = false;

// ========================================
// 按键去抖配置
// ========================================
#define KEY_DEBOUNCE_MS 5          // 硬件去抖时间（ms）
#define KEY_LONG_PRESS_MS 1000      // 长按判定时间（ms）

typedef struct {
    uint32_t gpio_num;
    volatile bool *flag;
    uint32_t last_press_time;       // 上次按下的时间戳
    bool is_pressed;                // 当前按键状态
} key_debounce_t;

static key_debounce_t keys[] = {
    {GPIO_KEY_NEXT, &g_key_next_flag, 0, false},
    {GPIO_KEY_PLAY, &g_key_play_flag, 0, false},
    {GPIO_KEY_PREV, &g_key_prev_flag, 0, false},
};

#define KEY_COUNT (sizeof(keys) / sizeof(keys[0]))

// ========================================
// 外部声明
// ========================================
extern mp3_decode_context_t mp3_ctx;

// ========================================
// 按键中断服务程序 - 仅用于唤醒任务
// ========================================
static void IRAM_ATTR gpio_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t)arg;
    
    // ISR 中仅记录事件，不做复杂处理
    // 实际的去抖逻辑在 key_scan_task 中进行
    
    // 可选：在这里激活扫描任务
    // BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    // vTaskNotifyGiveFromISR(key_scan_task_handle, &xHigherPriorityTaskWoken);
    // portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// ========================================
// 按键扫描任务 - 处理去抖逻辑和功能
// ========================================
static void key_scan_task(void *arg) {
    uint32_t current_time = 0;
    
    ESP_LOGI(TAG, "Key scan task started");
    
    while (1) {
        current_time = xTaskGetTickCount();
        
        // 遍历所有按键进行扫描和去抖
        for (int i = 0; i < KEY_COUNT; i++) {
            uint32_t gpio_num = keys[i].gpio_num;
            int current_level = gpio_get_level(gpio_num);
            bool key_pressed = (current_level == 0);  // 低电平表示按下
            
            // ★ 按键状态变化检测
            if (key_pressed && !keys[i].is_pressed) {
                // ★ 从未按下变为按下：记录时间
                keys[i].last_press_time = current_time;
                keys[i].is_pressed = true;
                
                ESP_LOGI(TAG, "GPIO_%d: press detected (debouncing...)", gpio_num);
                
            } else if (!key_pressed && keys[i].is_pressed) {
                // ★ 从按下变为未按下：检查是否有效按下
                uint32_t press_duration = current_time - keys[i].last_press_time;
                
                // 只有按下时间超过去抖阈值才认为是有效按下
                if (press_duration >= KEY_DEBOUNCE_MS) {
                    *keys[i].flag = true;
                    
                    // ★ 对于播放/暂停按键，立即处理（不需要等待music_loop_task）
                    if (gpio_num == GPIO_KEY_PLAY) {
                        // 实时切换暂停状态
                        mp3_ctx.pause = !mp3_ctx.pause;
                        
                        if (mp3_ctx.pause) {
                            ESP_LOGI(TAG, ">>> PAUSE (hardware key)");
                        } else {
                            ESP_LOGI(TAG, ">>> PLAY (hardware key)");
                        }
                        
                        // 清除标志位，防止重复处理
                        g_key_play_flag = false;
                    }
                    
                    if (press_duration >= KEY_LONG_PRESS_MS) {
                        ESP_LOGI(TAG, "GPIO_%d: LONG press confirmed (%ldms)", 
                                 gpio_num, press_duration);
                    } else {
                        ESP_LOGI(TAG, "GPIO_%d: SHORT press confirmed (%ldms)", 
                                 gpio_num, press_duration);
                    }
                } else {
                    // 抖动，不设置标志位
                    ESP_LOGW(TAG, "GPIO_%d: debounce rejected (%ldms < %dms)", 
                             gpio_num, press_duration, KEY_DEBOUNCE_MS);
                }
                
                keys[i].is_pressed = false;
            }
            
            // ★ 长按检测（可选）
            if (keys[i].is_pressed && (current_time - keys[i].last_press_time) >= KEY_LONG_PRESS_MS) {
                // 这里可以添加长按处理逻辑
                // ESP_LOGI(TAG, "GPIO_%d: long press detected", gpio_num);
            }
        }
        
        // 每 10ms 扫描一次，确保去抖精度
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ========================================
// 按键初始化
// ========================================
bool key_init(void) {
    ESP_LOGI(TAG, "Starting key initialization...");
    
    // 配置 GPIO
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_NEGEDGE;      // 下降沿触发
    io_conf.mode = GPIO_MODE_INPUT;             // 输入模式
    io_conf.pin_bit_mask = (1ULL << GPIO_KEY_NEXT) | 
                           (1ULL << GPIO_KEY_PLAY) | 
                           (1ULL << GPIO_KEY_PREV);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;    // 启用上拉
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;

    ESP_LOGI(TAG, "Configuring GPIO pins: NEXT=%d, PLAY=%d, PREV=%d", 
             GPIO_KEY_NEXT, GPIO_KEY_PLAY, GPIO_KEY_PREV);

    // 检查 GPIO 是否有效
    if (GPIO_KEY_PREV < 0 || GPIO_KEY_PREV > 39) {
        ESP_LOGE(TAG, "Invalid GPIO_KEY_PREV: %d", GPIO_KEY_PREV);
        return false;
    }

    if (gpio_config(&io_conf) != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed!");
        return false;
    }

    ESP_LOGI(TAG, "GPIO config OK");

    // 安装 ISR 服务
    esp_err_t isr_ret = gpio_install_isr_service(0);
    if (isr_ret == ESP_OK) {
        ESP_LOGI(TAG, "GPIO ISR service installed");
    } else if (isr_ret == ESP_ERR_INVALID_STATE) {
        // ISR 已被其他模块安装，继续使用即可
        ESP_LOGW(TAG, "GPIO ISR service already installed (by other module)");
    } else {
        ESP_LOGE(TAG, "GPIO ISR service install failed!");
        return false;
    }

    // 添加中断处理程序
    if (gpio_isr_handler_add(GPIO_KEY_NEXT, gpio_isr_handler, (void *)GPIO_KEY_NEXT) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler for KEY_NEXT");
        return false;
    }
    ESP_LOGI(TAG, "KEY_NEXT handler added (GPIO %d)", GPIO_KEY_NEXT);

    if (gpio_isr_handler_add(GPIO_KEY_PLAY, gpio_isr_handler, (void *)GPIO_KEY_PLAY) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler for KEY_PLAY");
        return false;
    }
    ESP_LOGI(TAG, "KEY_PLAY handler added (GPIO %d)", GPIO_KEY_PLAY);

    if (gpio_isr_handler_add(GPIO_KEY_PREV, gpio_isr_handler, (void *)GPIO_KEY_PREV) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler for KEY_PREV");
        return false;
    }
    ESP_LOGI(TAG, "KEY_PREV handler added (GPIO %d)", GPIO_KEY_PREV);

    // ★ 创建按键扫描任务（软件去抖）
    TaskHandle_t key_scan_handle = NULL;
    if (xTaskCreatePinnedToCore(key_scan_task, "key_scan", 2 * 1024, NULL, 
                                11, &key_scan_handle, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create key scan task");
        return false;
    }
    ESP_LOGI(TAG, "Key scan task created successfully");

    // 初始化完成
    ESP_LOGI(TAG, "Key ISR initialized successfully!");
    ESP_LOGI(TAG, "Debounce time: %dms, Long press time: %dms", 
             KEY_DEBOUNCE_MS, KEY_LONG_PRESS_MS);
    ESP_LOGI(TAG, "GPIO_KEY_NEXT=%d, GPIO_KEY_PLAY=%d, GPIO_KEY_PREV=%d", 
             GPIO_KEY_NEXT, GPIO_KEY_PLAY, GPIO_KEY_PREV);
    
    // 读取当前按键状态（调试）
    ESP_LOGI(TAG, "KEY_NEXT level: %d", gpio_get_level(GPIO_KEY_NEXT));
    ESP_LOGI(TAG, "KEY_PLAY level: %d", gpio_get_level(GPIO_KEY_PLAY));
    ESP_LOGI(TAG, "KEY_PREV level: %d", gpio_get_level(GPIO_KEY_PREV));
    
    return true;
}

// ========================================
// 外部接口：检查播放状态（供UI使用）
// ========================================
bool key_is_playing(void) {
    return !mp3_ctx.pause;
}

// ========================================
// 外部接口：获取播放/暂停标志
// ========================================
bool key_get_play_flag(void) {
    return g_key_play_flag;
}

// ========================================
// 外部接口：清除播放/暂停标志
// ========================================
void key_clear_play_flag(void) {
    g_key_play_flag = false;
}