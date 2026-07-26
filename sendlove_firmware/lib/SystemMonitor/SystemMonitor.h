#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include <Arduino.h>

class SystemMonitor {
public:
    static float getChipTemperature();
    static uint32_t getFreeHeap();
};

#endif // SYSTEM_MONITOR_H
