#ifndef __CONFIGURE_H__
#define __CONFIGURE_H__

#include <stdio.h>
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "freertos/queue.h"
#include "SensorLib.h"
#include "TouchDrvCST92xx.h"
#include "lvgl.h"
#include "lv_demos.h"
#include "esp_lcd_co5300.h"
#include "ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 日志标签 ==================== */
extern const char *TAG;

/* ==================== 硬件定义 ==================== */
#define LCD_HOST SPI2_HOST
#define TOUCH_HOST I2C_NUM_0

#if CONFIG_LV_COLOR_DEPTH == 32
#define LCD_BIT_PER_PIXEL (24)
#elif CONFIG_LV_COLOR_DEPTH == 16
#define LCD_BIT_PER_PIXEL (16)
#endif

/* --- LCD SPI 管脚定义 --- */
#define EXAMPLE_PIN_NUM_LCD_CS    (GPIO_NUM_10)
#define EXAMPLE_PIN_NUM_LCD_PCLK  (GPIO_NUM_12)
#define EXAMPLE_PIN_NUM_LCD_DATA0 (GPIO_NUM_13)
#define EXAMPLE_PIN_NUM_LCD_DATA1 (GPIO_NUM_11)
#define EXAMPLE_PIN_NUM_LCD_DATA2 (GPIO_NUM_9)
#define EXAMPLE_PIN_NUM_LCD_DATA3 (GPIO_NUM_14)
#define EXAMPLE_PIN_NUM_LCD_RST   (GPIO_NUM_8)

/* --- 屏幕分辨率 --- */
#define EXAMPLE_LCD_H_RES 410
#define EXAMPLE_LCD_V_RES 502

/* --- 性能优化配置 --- */
#define EXAMPLE_LVGL_BUF_LINES_PREFERRED 140
#define EXAMPLE_LVGL_BUF_LINES_MIN 40
#define EXAMPLE_LVGL_BUF_LINES_STEP 20
#define EXAMPLE_LCD_IO_QUEUE_LEN 6

#if LCD_BIT_PER_PIXEL == 24
#define EXAMPLE_LCD_TRANSFER_BYTES_PER_PIXEL 4
#else
#define EXAMPLE_LCD_TRANSFER_BYTES_PER_PIXEL sizeof(lv_color_t)
#endif

#define EXAMPLE_LCD_MAX_TRANSFER_SZ (EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_BUF_LINES_PREFERRED * EXAMPLE_LCD_TRANSFER_BYTES_PER_PIXEL)

/* --- 触摸配置 --- */
#define EXAMPLE_USE_TOUCH 1

#if EXAMPLE_USE_TOUCH
#define TOUCH_I2C_NUM TOUCH_HOST
#define TOUCH_I2C_FREQ_HZ 400000
#define TOUCH_I2C_SDA_IO (gpio_num_t)17
#define TOUCH_I2C_SCL_IO (gpio_num_t)18
#define Touch_INT (gpio_num_t)15
#define Touch_RST (gpio_num_t)16
#endif

/* --- LVGL 任务配置 --- */
#define EXAMPLE_LVGL_TICK_PERIOD_MS  1
#define EXAMPLE_LVGL_TASK_MAX_SLEEP_MS 12
#define EXAMPLE_LVGL_TASK_STACK_SIZE (16 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY 8
#define EXAMPLE_TOUCH_TASK_STACK_SIZE (4 * 1024)
#define EXAMPLE_TOUCH_TASK_PRIORITY 4
#define EXAMPLE_TOUCH_TASK_PERIOD_MS 10
#define EXAMPLE_TOUCH_RELEASE_GRACE_MS 30

/* ==================== 全局变量声明 ==================== */
extern SemaphoreHandle_t lvgl_mux;

#if EXAMPLE_USE_TOUCH
extern TouchDrvCST92xx touch;
extern int16_t tp_x;
extern int16_t tp_y;
extern bool tp_pressed;
#endif

extern uint32_t frame_count;
extern int64_t last_timestamp;

/* ==================== LCD 初始化指令表 ==================== */
extern const co5300_lcd_init_cmd_t lcd_init_cmds[];
extern const size_t lcd_init_cmds_size;

/* ==================== 函数声明 ==================== */

/**
 * @brief 初始化 SPI 总线和 LCD IO
 * @param io_handle 指向 io_handle 的指针
 * @return esp_err_t 错误码
 */
esp_err_t lcd_spi_io_init(esp_lcd_panel_io_handle_t *io_handle, void *user_ctx);

/**
 * @brief 初始化 LCD 面板驱动
 * @param io_handle LCD IO 句柄
 * @param panel_handle 指向 panel_handle 的指针
 * @return esp_err_t 错误码
 */
esp_err_t lcd_panel_init(esp_lcd_panel_io_handle_t io_handle, esp_lcd_panel_handle_t *panel_handle);

/**
 * @brief 初始化 LVGL 显示驱动
 * @param disp_drv 显示驱动指针
 * @param disp_buf 绘图缓冲指针
 * @param panel_handle LCD 面板句柄
 * @return lv_disp_t* 显示驱动指针
 */
lv_disp_t* lvgl_disp_driver_init(lv_disp_drv_t *disp_drv, lv_disp_draw_buf_t *disp_buf, 
                                  esp_lcd_panel_handle_t panel_handle);

/**
 * @brief 分配 LVGL 显示缓冲
 * @param disp_buf 显示缓冲指针
 * @param buf1 缓冲1 指针
 * @param buf2 缓冲2 指针
 * @return esp_err_t 错误码
 */
esp_err_t lvgl_buf_malloc(lv_disp_draw_buf_t *disp_buf, lv_color_t **buf1, lv_color_t **buf2);

/**
 * @brief 初始化 LVGL 定时器
 * @return esp_err_t 错误码
 */
esp_err_t lvgl_timer_init(void);

/**
 * @brief 创建 LVGL 互斥锁
 * @return esp_err_t 错误码
 */
esp_err_t lvgl_mutex_create(void);

/**
 * @brief 注册 LVGL 触摸输入驱动
 * @param disp 显示驱动指针
 * @return esp_err_t 错误码
 */
#if EXAMPLE_USE_TOUCH
esp_err_t lvgl_touch_input_init(lv_disp_t *disp);
void setup_sensor(void);
void touch_scanner_task(void *arg);
#endif

/**
 * @brief 启动 LVGL 渲染任务
 * @return esp_err_t 错误码
 */
esp_err_t lvgl_port_task_create(void);

/**
 * @brief LVGL 性能监控回调
 * @param disp_drv 显示驱动指针
 * @param time 渲染时间（毫秒）
 * @param px 像素数
 */
// void example_lvgl_monitor_cb(lv_disp_drv_t * disp_drv, uint32_t time, uint32_t px);

/**
 * @brief LCD 刷新完成通知回调
 * @param panel_io LCD IO 句柄
 * @param edata 事件数据
 * @param user_ctx 用户上下文
 * @return bool
 */
bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, 
                                      esp_lcd_panel_io_event_data_t *edata, 
                                      void *user_ctx);

/**
 * @brief LVGL 刷新回调
 * @param drv 显示驱动指针
 * @param area 刷新区域
 * @param color_map 颜色缓冲指针
 */
void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map);

/**
 * @brief LVGL 圆角回调
 * @param disp_drv 显示驱动指针
 * @param area 区域指针
 */
void example_lvgl_rounder_cb(struct _lv_disp_drv_t *disp_drv, lv_area_t *area);

/**
 * @brief LVGL 显示更新回调（旋转处理）
 * @param drv 显示驱动指针
 */
void example_lvgl_update_cb(lv_disp_drv_t *drv);

/**
 * @brief LVGL 定时器回调（增加 LVGL 时钟）
 * @param arg 参数（未使用）
 */
void example_increase_lvgl_tick(void *arg);

/**
 * @brief LVGL 主渲染任务
 * @param arg 参数（未使用）
 */
void example_lvgl_port_task(void *arg);

#if EXAMPLE_USE_TOUCH
/**
 * @brief LVGL 触摸输入回调
 * @param drv 输入设备驱动指针
 * @param data 输入设备数据指针
 */
void example_lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data);
#endif

#ifdef __cplusplus
}
#endif

#endif // __CONFIGURE_H__
