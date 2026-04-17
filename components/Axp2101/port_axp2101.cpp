#include <stdio.h>
#include <cstring>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"

#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"

static const char *TAG = "AXP2101";

static XPowersPMU PMU;

extern int pmu_register_read(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len);
extern int pmu_register_write_byte(uint8_t devAddr, uint8_t regAddr, uint8_t *data, uint8_t len);


esp_err_t pmu_init()
{
    if (PMU.begin(AXP2101_SLAVE_ADDRESS, pmu_register_read, pmu_register_write_byte)) {
        ESP_LOGI(TAG, "Init PMU SUCCESS!");
    } else {
        ESP_LOGE(TAG, "Init PMU FAILED!");
        return ESP_FAIL;
    }

    // ==================== 彻底禁用所有不需要的通道 ====================
    // 关闭所有 DC 通道（DC2~DC5）
    PMU.disableDC2();
    PMU.disableDC3();
    PMU.disableDC4();
    PMU.disableDC5();

    // 关闭所有 BLDO 通道
    PMU.disableBLDO1();
    PMU.disableBLDO2();

    // 关闭所有 DLDO 通道
    PMU.disableDLDO1();
    PMU.disableDLDO2();

    // 关闭 CPUSLDO
    PMU.disableCPUSLDO();

    // ==================== 启用所需的 5 个通道 ====================
    // DC1: 3.3V (主电源)
    PMU.setDC1Voltage(3300);
    PMU.enableDC1();

    // ALDO1: 3.3V SD卡
    PMU.setALDO1Voltage(3300);
    PMU.enableALDO1();

    // ALDO2: 1.8V血氧传感器
    PMU.setALDO2Voltage(1800);
    PMU.enableALDO2();

    // ALDO3: 3.3V 屏幕背光
    PMU.setALDO3Voltage(3100);
    PMU.enableALDO3();

    // ALDO4: 3.0V测试点
    PMU.setALDO4Voltage(3300);
    PMU.enableALDO4();

    // ==================== 打印启用通道的状态 ====================
    ESP_LOGI(TAG, "DCDC=======================================================================");
    ESP_LOGI(TAG, "DC1  : %s   Voltage:%u mV",  PMU.isEnableDC1()  ? "+" : "-", PMU.getDC1Voltage());
    
    ESP_LOGI(TAG, "ALDO=======================================================================");
    ESP_LOGI(TAG, "ALDO1: %s   Voltage:%u mV",  PMU.isEnableALDO1()  ? "+" : "-", PMU.getALDO1Voltage());
    ESP_LOGI(TAG, "ALDO2: %s   Voltage:%u mV",  PMU.isEnableALDO2()  ? "+" : "-", PMU.getALDO2Voltage());
    ESP_LOGI(TAG, "ALDO3: %s   Voltage:%u mV",  PMU.isEnableALDO3()  ? "+" : "-", PMU.getALDO3Voltage());
    ESP_LOGI(TAG, "ALDO4: %s   Voltage:%u mV",  PMU.isEnableALDO4()  ? "+" : "-", PMU.getALDO4Voltage());
    ESP_LOGI(TAG, "===========================================================================");

    PMU.clearIrqStatus();

    // ==================== 启用电压和温度测量 ====================
    PMU.enableVbusVoltageMeasure();
    PMU.enableBattVoltageMeasure();
    PMU.enableSystemVoltageMeasure();
    PMU.enableTemperatureMeasure();

    // ==================== 禁用 TS 针脚测量 ====================
    PMU.disableTSPinMeasure();

    // ==================== 配置中断 ====================
    PMU.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    PMU.clearIrqStatus();
    PMU.enableIRQ(
        XPOWERS_AXP2101_BAT_INSERT_IRQ    | XPOWERS_AXP2101_BAT_REMOVE_IRQ      |
        XPOWERS_AXP2101_VBUS_INSERT_IRQ   | XPOWERS_AXP2101_VBUS_REMOVE_IRQ     |
        XPOWERS_AXP2101_PKEY_SHORT_IRQ    | XPOWERS_AXP2101_PKEY_LONG_IRQ       |
        XPOWERS_AXP2101_BAT_CHG_DONE_IRQ  | XPOWERS_AXP2101_BAT_CHG_START_IRQ
    );

    // ==================== 充电配置 ====================
    PMU.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_50MA);
    PMU.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_200MA);
    PMU.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);
    PMU.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V1);

    ESP_LOGI(TAG, "battery percentage:%d %%", PMU.getBatteryPercent());

    return ESP_OK;
}


void pmu_isr_handler()
{
    PMU.getIrqStatus();

    if (PMU.isDropWarningLevel2Irq()) {
        ESP_LOGI(TAG, "isDropWarningLevel2");
    }
    if (PMU.isDropWarningLevel1Irq()) {
        ESP_LOGI(TAG, "isDropWarningLevel1");
    }
    if (PMU.isGaugeWdtTimeoutIrq()) {
        ESP_LOGI(TAG, "isWdtTimeout");
    }
    if (PMU.isBatChargerOverTemperatureIrq()) {
        ESP_LOGI(TAG, "isBatChargeOverTemperature");
    }
    if (PMU.isBatWorkOverTemperatureIrq()) {
        ESP_LOGI(TAG, "isBatWorkOverTemperature");
    }
    if (PMU.isBatWorkUnderTemperatureIrq()) {
        ESP_LOGI(TAG, "isBatWorkUnderTemperature");
    }
    if (PMU.isVbusInsertIrq()) {
        ESP_LOGI(TAG, "isVbusInsert");
    }
    if (PMU.isVbusRemoveIrq()) {
        ESP_LOGI(TAG, "isVbusRemove");
    }
    if (PMU.isBatInsertIrq()) {
        ESP_LOGI(TAG, "isBatInsert");
    }
    if (PMU.isBatRemoveIrq()) {
        ESP_LOGI(TAG, "isBatRemove");
    }
    if (PMU.isPekeyShortPressIrq()) {
        ESP_LOGI(TAG, "isPekeyShortPress");
        ESP_LOGI(TAG, "battery percentage:%d %%", PMU.getBatteryPercent());
    }
    if (PMU.isPekeyLongPressIrq()) {
        ESP_LOGI(TAG, "isPekeyLongPress");
    }
    if (PMU.isPekeyNegativeIrq()) {
        ESP_LOGI(TAG, "isPekeyNegative");
    }
    if (PMU.isPekeyPositiveIrq()) {
        ESP_LOGI(TAG, "isPekeyPositive");
    }
    if (PMU.isWdtExpireIrq()) {
        ESP_LOGI(TAG, "isWdtExpire");
    }
    if (PMU.isLdoOverCurrentIrq()) {
        ESP_LOGI(TAG, "isLdoOverCurrentIrq");
    }
    if (PMU.isBatfetOverCurrentIrq()) {
        ESP_LOGI(TAG, "isBatfetOverCurrentIrq");
    }
    if (PMU.isBatChagerDoneIrq()) {
        ESP_LOGI(TAG, "isBatChagerDone");
    }
    if (PMU.isBatChagerStartIrq()) {
        ESP_LOGI(TAG, "isBatChagerStart");
    }
    if (PMU.isBatDieOverTemperatureIrq()) {
        ESP_LOGI(TAG, "isBatDieOverTemperature");
    }
    if (PMU.isChagerOverTimeoutIrq()) {
        ESP_LOGI(TAG, "isChagerOverTimeout");
    }
    if (PMU.isBatOverVoltageIrq()) {
        ESP_LOGI(TAG, "isBatOverVoltage");
    }
    
    PMU.clearIrqStatus();
}