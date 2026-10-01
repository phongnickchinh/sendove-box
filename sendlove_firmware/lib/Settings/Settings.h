#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <atomic>

// ============================================================================
// Settings — the user settings CURRENTLY IN EFFECT (brightness, message volume)
// ============================================================================
// Sources: NVS at boot (begin), the cloud when `config_flag` is set (apply, called
// from the WakeSync task). Readers: Task_MediaPlayer, Task_UIController,
// DisplayDriver, MediaPlayer -> hence atomics. Settings live in NVS, not on the SD
// card: with a faulty card the box keeps the right brightness/volume.
// ============================================================================

namespace Settings {

extern std::atomic<uint8_t> brightness;   // SETTINGS_MIN_BRIGHTNESS..100
extern std::atomic<uint8_t> volume;       // 0..100, 0 = mute
extern std::atomic<uint32_t> appliedRev;  // the config_rev last applied
/// Bumped on every brightness change -> the display loop knows to rewrite the PWM.
extern std::atomic<uint32_t> brightnessEpoch;

/// Read NVS. Call once in setup(), AFTER ConfigManager has opened NVS.
void begin();

/// Clamp the values, save to NVS, update RAM. A value < 0 = the cloud didn't send
/// that field -> keep the old one. Returns true if the NVS write succeeded (even
/// when nothing changed).
bool apply(int newBrightness, int newVolume, uint32_t rev);

/// Actual PWM % for normal screens. Gamma 2, so the web slider feels perceptually even.
uint8_t currentBacklight();
/// PWM % for the alarm screen: never darker than ALARM_MIN_BRIGHTNESS.
uint8_t alarmBacklight();

/// Q15 gain for volume 0..100 on a dB scale (100 = 0dB = 32768, 1 = -40dB, 0 = silent).
/// 32768 marks the unscaled path: AudioPlayer skips the multiply entirely for it.
int32_t volumeGainQ15(uint8_t vol);

static constexpr int32_t GAIN_UNITY = 32768;

}  // namespace Settings

#endif  // SETTINGS_H
