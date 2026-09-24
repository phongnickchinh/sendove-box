#include "Settings.h"

#include <math.h>
// Include thẳng: LDF của PlatformIO không lần theo <Preferences.h> nằm trong ConfigManager.h.
#include <Preferences.h>

#include "ConfigManager.h"
#include "ScreenLogger.h"
#include "config.h"

namespace Settings {

std::atomic<uint8_t> brightness{SETTINGS_DEFAULT_BRIGHTNESS};
std::atomic<uint8_t> volume{SETTINGS_DEFAULT_VOLUME};
std::atomic<uint32_t> appliedRev{0};
std::atomic<uint32_t> brightnessEpoch{0};

namespace {

// 101 hệ số tính một lần: powf() cho từng mẫu âm thanh là quá đắt.
int32_t s_gainLut[101];
bool s_lutReady = false;

void buildLut() {
    s_gainLut[0] = 0;
    for (int v = 1; v < 100; v++) {
        float db = -40.0f + 40.0f * (float)(v - 1) / 99.0f;
        s_gainLut[v] = (int32_t)lroundf(powf(10.0f, db / 20.0f) * (float)GAIN_UNITY);
    }
    s_gainLut[100] = GAIN_UNITY;
    s_lutReady = true;
}

uint8_t clampBrightness(int v) {
    if (v < SETTINGS_MIN_BRIGHTNESS) return SETTINGS_MIN_BRIGHTNESS;
    if (v > 100) return 100;
    return (uint8_t)v;
}

uint8_t clampVolume(int v) {
    if (v < 0) return 0;
    if (v > 100) return 100;
    return (uint8_t)v;
}

// Gamma 2: 100 -> 100, 50 -> 25, 5 -> 0 -> kẹp lên 1. Sàn thật nằm ở clampBrightness.
uint8_t toPwm(uint8_t userPercent) {
    uint32_t p = ((uint32_t)userPercent * userPercent + 50) / 100;
    return (uint8_t)(p < 1 ? 1 : p);
}

}  // namespace

void begin() {
    buildLut();
    UserSettings s;
    ConfigManager cfg;
    if (cfg.init(NVS_NAMESPACE)) {
        cfg.loadSettings(s);
        cfg.end();
    }
    brightness = clampBrightness(s.brightness);
    volume = clampVolume(s.volume);
    appliedRev = s.rev;
    DLOG("[SET] bl=%u vol=%u rev=%lu", (unsigned)brightness.load(), (unsigned)volume.load(),
         (unsigned long)s.rev);
}

bool apply(int newBrightness, int newVolume, uint32_t rev) {
    UserSettings s;
    s.brightness = (newBrightness < 0) ? brightness.load() : clampBrightness(newBrightness);
    s.volume = (newVolume < 0) ? volume.load() : clampVolume(newVolume);
    s.rev = rev;

    bool ok = false;
    ConfigManager cfg;
    if (cfg.init(NVS_NAMESPACE)) {
        ok = cfg.saveSettings(s);
        cfg.end();
    }

    if (s.brightness != brightness.load()) {
        brightness = s.brightness;
        brightnessEpoch++;
    }
    volume = s.volume;
    appliedRev = rev;
    DLOG("[SET] ap dung bl=%u vol=%u rev=%lu%s", (unsigned)s.brightness, (unsigned)s.volume,
         (unsigned long)rev, ok ? "" : " (NVS loi)");
    return ok;
}

uint8_t currentBacklight() { return toPwm(brightness.load()); }

uint8_t alarmBacklight() {
    uint8_t b = brightness.load();
    return toPwm(b < ALARM_MIN_BRIGHTNESS ? ALARM_MIN_BRIGHTNESS : b);
}

int32_t volumeGainQ15(uint8_t vol) {
    if (!s_lutReady) buildLut();
    return s_gainLut[vol > 100 ? 100 : vol];
}

}  // namespace Settings
