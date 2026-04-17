#include "Configure.h"
#include "esp_heap_caps.h"
#include "freertos/portmacro.h"

/* ==================== 日志标签定义 ==================== */
const char *TAG = "LCD_FPS_DEBUG";

/* ==================== 全局变量定义 ==================== */
SemaphoreHandle_t lvgl_mux = NULL;

#if EXAMPLE_USE_TOUCH
TouchDrvCST92xx touch;
int16_t tp_x = 0;
int16_t tp_y = 0;
bool tp_pressed = false;
static portMUX_TYPE tp_lock = portMUX_INITIALIZER_UNLOCKED;
#endif

uint32_t frame_count = 0;
int64_t last_timestamp = 0;

/* ==================== LCD 初始化指令表定义 ==================== */
const co5300_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xFE, (uint8_t[]){0x00}, 1, 0},
    {0xC4, (uint8_t[]){0x80}, 1, 0},
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 0},
    {0x51, (uint8_t[]){0xFF}, 1, 0},
    {0x63, (uint8_t[]){0xFF}, 1, 0},
    {0x2A, (uint8_t[]){0x00, 0x06, 0x01, 0xD7}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xD1}, 4, 600},
    {0x11, NULL, 0, 600},
    {0x29, NULL, 0, 0},
};

const size_t lcd_init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]);

static bool try_alloc_lvgl_buffers(size_t lines, lv_color_t **buf1, lv_color_t **buf2)
{
    const size_t buf_size = EXAMPLE_LCD_H_RES * lines * sizeof(lv_color_t);
    const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;

    *buf1 = (lv_color_t *)heap_caps_malloc(buf_size, caps);
    if (*buf1 == NULL) {
        return false;
    }

    *buf2 = (lv_color_t *)heap_caps_malloc(buf_size, caps);
    if (*buf2 == NULL) {
        heap_caps_free(*buf1);
        *buf1 = NULL;
        return false;
    }

    return true;
}

/* ==================== LCD SPI 和 IO 初始化 ==================== */
esp_err_t lcd_spi_io_init(esp_lcd_panel_io_handle_t *io_handle, void *user_ctx)
{
    ESP_LOGI(TAG, "Initializing SPI bus and LCD IO...");
    
    // 配置 SPI 总线
    spi_bus_config_t buscfg = {};
    buscfg.sclk_io_num = EXAMPLE_PIN_NUM_LCD_PCLK;
    buscfg.data0_io_num = EXAMPLE_PIN_NUM_LCD_DATA0;
    buscfg.data1_io_num = EXAMPLE_PIN_NUM_LCD_DATA1;
    buscfg.data2_io_num = EXAMPLE_PIN_NUM_LCD_DATA2;
    buscfg.data3_io_num = EXAMPLE_PIN_NUM_LCD_DATA3;
    buscfg.max_transfer_sz = EXAMPLE_LCD_MAX_TRANSFER_SZ;
    buscfg.flags = SPICOMMON_BUSFLAG_QUAD;
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // 配置 LCD IO
    esp_lcd_panel_io_spi_config_t io_config = {};
    io_config.cs_gpio_num = EXAMPLE_PIN_NUM_LCD_CS;
    io_config.dc_gpio_num = -1;
    io_config.spi_mode = 0;
    io_config.pclk_hz = 80 * 1000 * 1000; 
    io_config.trans_queue_depth = EXAMPLE_LCD_IO_QUEUE_LEN; 
    io_config.on_color_trans_done = example_notify_lvgl_flush_ready;
    
    // 2. 将原来等于 NULL 的地方改为传入的 user_ctx
    io_config.user_ctx = user_ctx; 
    
    io_config.lcd_cmd_bits = 32;
    io_config.lcd_param_bits = 8;
    io_config.flags.quad_mode = true;
    
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, io_handle));
    
    ESP_LOGI(TAG, "SPI bus and LCD IO initialized successfully");
    return ESP_OK;
}

/* ==================== LCD 面板初始化 ==================== */
esp_err_t lcd_panel_init(esp_lcd_panel_io_handle_t io_handle, esp_lcd_panel_handle_t *panel_handle)
{
    ESP_LOGI(TAG, "Initializing LCD panel...");
    
    // 配置 LCD 供应商驱动
    co5300_vendor_config_t vendor_config = {};
    vendor_config.init_cmds = lcd_init_cmds;
    vendor_config.init_cmds_size = lcd_init_cmds_size;
    vendor_config.flags.use_qspi_interface = 1;

    // 配置 LCD 面板设备
    esp_lcd_panel_dev_config_t panel_config = {};
    panel_config.reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST;
    panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
    panel_config.bits_per_pixel = LCD_BIT_PER_PIXEL;
    panel_config.vendor_config = &vendor_config;

    ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(io_handle, &panel_config, panel_handle));
    esp_lcd_panel_reset(*panel_handle);
    esp_lcd_panel_init(*panel_handle);
    esp_lcd_panel_disp_on_off(*panel_handle, true);
    
    ESP_LOGI(TAG, "LCD panel initialized successfully");
    return ESP_OK;
}

/* ==================== LVGL 显示缓冲分配 ==================== */
esp_err_t lvgl_buf_malloc(lv_disp_draw_buf_t *disp_buf, lv_color_t **buf1, lv_color_t **buf2)
{
    ESP_LOGI(TAG, "Allocating LVGL display buffers...");
    
    *buf1 = NULL;
    *buf2 = NULL;

    const size_t largest_internal_block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "Largest internal 8-bit block before LVGL alloc: %u bytes", (unsigned int)largest_internal_block);

    int preferred_lines = EXAMPLE_LVGL_BUF_LINES_PREFERRED;
    if (preferred_lines > EXAMPLE_LCD_V_RES) {
        preferred_lines = EXAMPLE_LCD_V_RES;
    }

    int allocated_lines = 0;
    for (int lines = preferred_lines; lines >= EXAMPLE_LVGL_BUF_LINES_MIN; lines -= EXAMPLE_LVGL_BUF_LINES_STEP) {
        if (try_alloc_lvgl_buffers(lines, buf1, buf2)) {
            allocated_lines = lines;
            break;
        }
    }

    if (allocated_lines == 0) {
        ESP_LOGE(TAG, "Failed to allocate LVGL display buffers");
        return ESP_ERR_NO_MEM;
    }

    const size_t buf_size = EXAMPLE_LCD_H_RES * allocated_lines * sizeof(lv_color_t);
    lv_disp_draw_buf_init(disp_buf, *buf1, *buf2, EXAMPLE_LCD_H_RES * allocated_lines);

    ESP_LOGI(TAG, "LVGL display buffers allocated successfully: %d lines, %u bytes per buffer",
             allocated_lines, (unsigned int)buf_size);
    return ESP_OK;
}

/* ==================== LVGL 显示驱动初始化 ==================== */
lv_disp_t* lvgl_disp_driver_init(lv_disp_drv_t *disp_drv, lv_disp_draw_buf_t *disp_buf, 
                                  esp_lcd_panel_handle_t panel_handle)
{
    ESP_LOGI(TAG, "Initializing LVGL display driver...");
    
    lv_disp_drv_init(disp_drv);
    disp_drv->hor_res = EXAMPLE_LCD_H_RES;
    disp_drv->ver_res = EXAMPLE_LCD_V_RES;
    disp_drv->flush_cb = example_lvgl_flush_cb;
    disp_drv->rounder_cb = example_lvgl_rounder_cb;
    disp_drv->drv_update_cb = example_lvgl_update_cb;
    // disp_drv->monitor_cb = example_lvgl_monitor_cb;
    disp_drv->draw_buf = disp_buf;
    disp_drv->user_data = panel_handle;
    
    lv_disp_t *disp = lv_disp_drv_register(disp_drv);
    
    ESP_LOGI(TAG, "LVGL display driver initialized successfully");
    return disp;
}

/* ==================== LVGL 定时器初始化 ==================== */
esp_err_t lvgl_timer_init(void)
{
    ESP_LOGI(TAG, "Initializing LVGL timer...");
    
    last_timestamp = esp_timer_get_time();
    esp_timer_create_args_t tick_args = {};
    tick_args.callback = &example_increase_lvgl_tick;
    tick_args.name = "lvgl_tick";
    esp_timer_handle_t lvgl_tick_timer = NULL;
    
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));
    
    ESP_LOGI(TAG, "LVGL timer initialized successfully");
    return ESP_OK;
}

/* ==================== LVGL 互斥锁创建 ==================== */
esp_err_t lvgl_mutex_create(void)
{
    ESP_LOGI(TAG, "Creating LVGL mutex...");
    
    lvgl_mux = xSemaphoreCreateMutex();
    if (lvgl_mux == NULL) {
        ESP_LOGE(TAG, "Failed to create LVGL mutex");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "LVGL mutex created successfully");
    return ESP_OK;
}

/* ==================== 触摸功能初始化 ==================== */
#if EXAMPLE_USE_TOUCH
void setup_sensor(void)
{
    ESP_LOGI(TAG, "Initializing touch sensor...");
    
    touch.setPins(Touch_RST, Touch_INT);
    touch.begin(TOUCH_I2C_NUM, 0x5A, TOUCH_I2C_SDA_IO, TOUCH_I2C_SCL_IO);
    touch.reset();
    touch.setMaxCoordinates(EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
    touch.setMirrorXY(false, false);
    
    ESP_LOGI(TAG, "Touch sensor initialized successfully");
}

esp_err_t lvgl_touch_input_init(lv_disp_t *disp)
{
    ESP_LOGI(TAG, "Initializing LVGL touch input driver...");
    
    setup_sensor();
    
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.disp = disp;
    indev_drv.read_cb = example_lvgl_touch_cb;
    lv_indev_drv_register(&indev_drv);
    
    // 创建触摸扫描任务
    
    BaseType_t touch_task_created = xTaskCreatePinnedToCore(touch_scanner_task, "TouchTask",
                                                            EXAMPLE_TOUCH_TASK_STACK_SIZE, NULL,
                                                            EXAMPLE_TOUCH_TASK_PRIORITY, NULL, 1);
    if (touch_task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create touch scanner task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LVGL touch input driver initialized successfully");
    return ESP_OK;
}
#endif

/* ==================== LVGL 渲染任务创建 ==================== */
esp_err_t lvgl_port_task_create(void)
{
    ESP_LOGI(TAG, "Creating LVGL render task...");
    
    xTaskCreatePinnedToCore(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, 
                           NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL, 0);
    
    ESP_LOGI(TAG, "LVGL render task created successfully");
    return ESP_OK;
}

/* ==================== 回调函数实现 ==================== */

// void example_lvgl_monitor_cb(lv_disp_drv_t * disp_drv, uint32_t time, uint32_t px)
// {
//     frame_count++;
//     int64_t now = esp_timer_get_time();
//     // 每隔 1 秒打印一次
//     if (now - last_timestamp >= 1000000) {
//         float fps = (float)frame_count * 1000000.0f / (now - last_timestamp);
//         ESP_LOGI(TAG, "STATISTICS: FPS: %.2f | Last Frame Render: %u ms | Px: %u", 
//                 (double)fps, (unsigned int)time, (unsigned int)px);
//         frame_count = 0;
//         last_timestamp = now;
//     }
// }

bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, 
                                      esp_lcd_panel_io_event_data_t *edata, 
                                      void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;
    
    const int offsetx1 = area->x1 + 0x16; 
    const int offsetx2 = area->x2 + 0x16;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

#if LCD_BIT_PER_PIXEL == 24
    uint8_t *to = (uint8_t *)color_map;
    uint32_t pixel_num = (uint32_t)(area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);
    lv_color32_t *px = (lv_color32_t *)color_map;
    
    for (uint32_t i = 0; i < pixel_num; i++) {
        uint8_t r = px[i].ch.red;
        uint8_t g = px[i].ch.green;
        uint8_t b = px[i].ch.blue;
        *to++ = r;
        *to++ = g;
        *to++ = b;
    }
#endif

    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

void example_lvgl_rounder_cb(struct _lv_disp_drv_t *disp_drv, lv_area_t *area)
{
    area->x1 = (area->x1 >> 1) << 1;
    area->y1 = (area->y1 >> 1) << 1;
    area->x2 = ((area->x2 >> 1) << 1) + 1;
    area->y2 = ((area->y2 >> 1) << 1) + 1;
}

void example_lvgl_update_cb(lv_disp_drv_t *drv)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;
    switch (drv->rotated) {
        case LV_DISP_ROT_NONE: 
            esp_lcd_panel_swap_xy(panel_handle, false); 
            esp_lcd_panel_mirror(panel_handle, true, false); 
            break;
        case LV_DISP_ROT_90:   
            esp_lcd_panel_swap_xy(panel_handle, true);  
            esp_lcd_panel_mirror(panel_handle, true, true);  
            break;
        case LV_DISP_ROT_180:  
            esp_lcd_panel_swap_xy(panel_handle, false); 
            esp_lcd_panel_mirror(panel_handle, false, true); 
            break;
        case LV_DISP_ROT_270:  
            esp_lcd_panel_swap_xy(panel_handle, true);  
            esp_lcd_panel_mirror(panel_handle, false, false); 
            break;
    }
}

void example_increase_lvgl_tick(void *arg)
{
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

void example_lvgl_port_task(void *arg)
{
    uint32_t task_delay_ms = 1;
    while (1) {
        if (xSemaphoreTake(lvgl_mux, portMAX_DELAY) == pdTRUE) {
            task_delay_ms = lv_timer_handler();
            xSemaphoreGive(lvgl_mux);
        }
        if (task_delay_ms == 0) task_delay_ms = 1;
        if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_SLEEP_MS) {
            task_delay_ms = EXAMPLE_LVGL_TASK_MAX_SLEEP_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

#if EXAMPLE_USE_TOUCH
void touch_scanner_task(void *arg)
{
    LV_UNUSED(arg);

    int16_t x[1];
    int16_t y[1];
    int16_t last_x = 0;
    int16_t last_y = 0;

    while (1) {
        bool pressed = false;

        if (touch.getPoint(x, y, 1)) {
            last_x = x[0];
            last_y = y[0];
            pressed = true;
        }

        portENTER_CRITICAL(&tp_lock);
        tp_x = last_x;
        tp_y = last_y;
        tp_pressed = pressed;
        portEXIT_CRITICAL(&tp_lock);

        vTaskDelay(pdMS_TO_TICKS(EXAMPLE_TOUCH_TASK_PERIOD_MS));
    }
}

void example_lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    LV_UNUSED(drv);

    int16_t x;
    int16_t y;
    bool pressed;

    portENTER_CRITICAL(&tp_lock);
    x = tp_x;
    y = tp_y;
    pressed = tp_pressed;
    portEXIT_CRITICAL(&tp_lock);

    data->point.x = x;
    data->point.y = y;
    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
#endif
