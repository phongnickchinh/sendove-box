#include "SystemMonitor.h"
#include "driver/temp_sensor.h"

static bool s_tempSensorInited = false;

float SystemMonitor::getChipTemperature() {
    if (!s_tempSensorInited) {
        temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
        temp_sensor_set_config(temp_sensor);
        temp_sensor_start();
        s_tempSensorInited = true;
    }
    float result = 0.0f;
    if (temp_sensor_read_celsius(&result) == ESP_OK) {
        return result;
    }
    return 0.0f;
}

uint32_t SystemMonitor::getFreeHeap() {
    return ESP.getFreeHeap();
}
