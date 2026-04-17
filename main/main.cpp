#include "Configure.h"
#include "sd_card.h"

#define PMU_INPUT_PIN                    GPIO_NUM_45
#define PMU_INPUT_PIN_SEL               (1ULL<<PMU_INPUT_PIN)


/*
! WARN:
Please do not run the example without knowing the external load voltage of the PMU,
it may burn your external load, please check the voltage setting before running the example,
if there is any loss, please bear it by yourself
*/
// #ifndef XPOWERS_NO_ERROR
// #error "Running this example is known to not damage the device! Please go and uncomment this!"
// #endif


extern esp_err_t pmu_init();
extern esp_err_t i2c_init(void);
extern void pmu_isr_handler();

static void pmu_hander_task(void *);
static QueueHandle_t  gpio_evt_queue = NULL;

static void IRAM_ATTR pmu_irq_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void irq_init()
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = PMU_INPUT_PIN_SEL;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    gpio_set_intr_type(PMU_INPUT_PIN, GPIO_INTR_NEGEDGE);
    //install gpio isr service
    gpio_install_isr_service(0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(PMU_INPUT_PIN, pmu_irq_handler, (void *) PMU_INPUT_PIN);

}

extern "C" void app_main(void)
{
    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(5, sizeof(uint32_t));

    // Register PMU interrupt pins
    irq_init();

    ESP_ERROR_CHECK(i2c_init());

    ESP_LOGI(TAG, "I2C initialized successfully");

    ESP_ERROR_CHECK(pmu_init());

    xTaskCreate(pmu_hander_task, "App/pwr", 4 * 1024, NULL, 10, NULL);

    
        // 初始化 SD 卡 (SDIO 模式)
    esp_err_t ret = sd_card_init();
    if (ret == ESP_OK) {
        ESP_LOGI("MAIN", "SD Card Ready!");
    } else {
        ESP_LOGE("MAIN", "SD Card Init Failed!");
    }



    static lv_disp_draw_buf_t disp_buf;
    static lv_disp_drv_t disp_drv;
    lv_color_t *buf1 = NULL;
    lv_color_t *buf2 = NULL;
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_handle_t panel_handle = NULL;

    // 1. 硬件初始化：SPI 和 LCD IO (在这里直接把 &disp_drv 传进去！)
    ESP_ERROR_CHECK(lcd_spi_io_init(&io_handle, &disp_drv));

    // 2. LCD 面板初始化
    ESP_ERROR_CHECK(lcd_panel_init(io_handle, &panel_handle));

    // 3. LVGL 核心初始化
    lv_init();

    // 4. 分配显存
    ESP_ERROR_CHECK(lvgl_buf_malloc(&disp_buf, &buf1, &buf2));

    // 5. 注册显示驱动 (必须在 ui_init 之前！)
    lv_disp_t *disp = lvgl_disp_driver_init(&disp_drv, &disp_buf, panel_handle);

    // 6. 注册输入驱动 (触摸)
    #if EXAMPLE_USE_TOUCH
    ESP_ERROR_CHECK(lvgl_touch_input_init(disp));
    #endif

    // 7. 启动 LVGL 定时器
    ESP_ERROR_CHECK(lvgl_timer_init());

    // 8. 创建互斥锁
    ESP_ERROR_CHECK(lvgl_mutex_create());

    // 9. 初始化 UI (此时显示器已经注册，lv_scr_act() 才有意义)
    if (xSemaphoreTake(lvgl_mux, portMAX_DELAY) == pdTRUE) {
        ESP_LOGI(TAG, "Initializing UI Demo...");
        ui_init(); 
        //lv_demo_music(); 
        //lv_demo_stress(); 
        //lv_demo_benchmark(); 
        xSemaphoreGive(lvgl_mux);
    }

    // 10. 创建渲染任务
    ESP_ERROR_CHECK(lvgl_port_task_create());


}


static void pmu_hander_task(void *args)
{
    uint32_t io_num;
    while (1) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            pmu_isr_handler();
        }
    }
}
