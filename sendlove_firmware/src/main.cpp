#include "AlarmClock.h"
#include "ConfigManager.h"
#include "DisplayDriver.h"
#include "IStorageProvider.h"
#include "NandStorageProvider.h"
#include "SDStorageProvider.h"
#include "LayoutEngine.h"
#include "MediaPlayer.h"
#include "NetworkManager.h"
#include "OtaHandler.h"
#include "PowerManager.h"
#include "ScreenLogger.h"
#include "UIController.h"
#include "Settings.h"
#include "SdLog.h"
#include "SdStore.h"
#include "MusicStore.h"
#include "ThemeStore.h"
#include "config.h"
#include <Arduino.h>
#include <SPI.h>
#include <atomic>
#include <esp_ota_ops.h>
#include <esp_timer.h>

// ============================================================================
// SENDLOVE BOX — Main Firmware (Phase 3A: Storage Abstraction Layer)
// ============================================================================
// Event-driven FreeRTOS architecture:
//   - Task_MediaPlayer: decodes + renders video/images from IStorageProvider (NAND / SD)
//   - Task_UIController: reads the touch sensor + posts slot/item events
//   - Task_NetworkController: serves the WebServer / captive portal
// ============================================================================
// TOUCH_OTA_TOGGLE: a TOUCH_OTA_HOLD_MS (6s) hold — the last step of the OTA touch sequence.
enum class SystemEvent : uint8_t { NONE, TOUCH_SHORT, TOUCH_LONG, TIMEOUT_AUTO_NEXT, TOUCH_OTA_TOGGLE };

struct AppContext {
  DisplayDriver display;
  IStorageProvider* storage = nullptr;
  MediaPlayer player;
  UIController ui;
  NetworkManager network;
  LayoutEngine layoutEngine;
  OtaHandler otaHandler;
  PowerManager powerManager;
  ConfigManager configManager;
};

static AppContext appCtx;

static uint32_t lastUserActivity = 0;
static volatile bool forceStandbyRedraw = false;

static SemaphoreHandle_t spiMutex = nullptr;
static QueueHandle_t eventQueue = nullptr;

// A play command waiting for sync to finish (a 1-slot queue: several touches
// collapse into one, since the command carries no argument — just "play the next
// unread message"). Task_MediaPlayer sets/clears it; Task_UIController reads it
// to keep the box awake while waiting.
//
// Why wait: playback allocates ~74KB (_jpegBuffer 32KB + JPEGDEC 17.9KB + I2S DMA
// 24KB) and a sync's TLS session needs 35-45KB (see MEMORY.md §8, §21). Overlapping
// them on a single-core chip stutters, and touching during a sync has caused real
// REBOOTS. Do NOT fold this flag into isPlaybackActive(): a downloading sync would
// abort at its next checkpoint without setting _hasPendingMessages -> after the
// wait there would be no new message to play.
static std::atomic<bool> s_pendingPlay{false};

// An alarm is ringing WITH MUSIC (continuous card reads). It counts as
// isPlaybackActive() so sync stops at its next checkpoint and downloadFile() aborts
// midway: at 16 kHz the DMA holds only ~96ms, and a card busy writing a message
// makes the music crackle (mandatory case, MEMORY.md §28). The beep doesn't read
// the card, so it doesn't count.
static std::atomic<bool> s_alarmMusicOn{false};

// ============================================================================
// Rollback after OTA
// ============================================================================
// The bootloader has rollback enabled (CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=1), but
// initArduino() by default calls verifyOta() (weak, returns true) and marks the new
// image VALID at boot — which defeats the protection. Returning true here means
// Arduino doesn't mark it; the firmware does: it confirms only after surviving
// OTA_VERIFY_DELAY_MS (Task_UIController). A reset before that leaves the partition
// PENDING_VERIFY and the bootloader falls back to the old image — that is the whole
// failure path; no extra code needed.
//
// MUST be extern "C": the weak original lives in esp32-hal-misc.c (C linkage).
//
// Flashing over cable does NOT take this path: boot_app0.bin writes otadata with
// ota_state = 0xFFFFFFFF (UNDEFINED, verified from the actual bytes), not NEW, so a
// cable-flashed image is never PENDING_VERIFY.
extern "C" bool verifyRollbackLater() { return true; }

static volatile bool s_otaPendingVerify = false;

// OTA web server command: +1 start, -1 stop, 0 nothing. Task_MediaPlayer writes it,
// Task_NetworkController executes it. Every WebServer operation MUST run on the task
// that calls handleClient(): stopWebServer() does `delete _webServer`, and calling it
// from another task while handleClient() runs is a use-after-free.
static volatile int8_t s_otaServerCmd = 0;
static esp_timer_handle_t s_otaGuardTimer = nullptr;

// Safety net for a new image that HANGS (without crashing). Rollback only happens on
// a chip RESET, and in this configuration a hang does NOT reset: the task WDT doesn't
// watch the IDLE task (CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0 is off) and loopTask
// isn't registered (loopTaskWDTEnabled = false), so a while(1) just sits there forever.
// esp_timer runs in a priority-22 task and still fires after the app tasks have died.
static void otaGuardFire(void*) { esp_restart(); }

// The serial monitor is fully replaced by the ScreenLogger on-screen overlay.
// Serial.begin() is not called, so the chip never blocks when there is no USB CDC host.

// STATE_ALARM: an alarm is ringing. Blocks sleep and accepts touches even while downloading.
// STATE_OTA: flashing mode, entered/left with the 3s, 3s, 6s hold sequence. The box ONLY
// waits and flashes: no sleep, no sync, no playback, NO alarms (product decision —
// forgetting to exit means missed alarms; the OTA screen shows a reminder).
enum class AppState { STATE_STANDBY, STATE_VIDEO, STATE_ALARM, STATE_OTA };

// Read/written from 3 tasks (MediaPlayer, UIController, the main loop), so it must be
// atomic. Every use is a direct comparison/assignment (none binds through `auto`), so
// the implicit operator T() / operator= cover them all.
//
// NOT lock-free: the ESP32-C3 is RV32IMC and lacks the 'A' extension for sub-word
// atomics. It links because ESP-IDF ships an emulation (verified with nm:
// __atomic_load_1/store_1/exchange_1 are 'T' in sdk/esp32c3/lib/libnewlib.a). The
// emulation works by disabling interrupts — cheap, but never call it from an ISR.
// Nothing does today: main.cpp has no IRAM_ATTR, and the one callback-looking spot
// (setPlaybackActiveCallback) runs in task context.
std::atomic<AppState> currentAppState{AppState::STATE_STANDBY};

// The standby layout + background + fonts are NOT compiled in (SD-card design, see
// MEMORY.md §28): the theme lives in the `theme` flash partition (ThemeStore), copied
// from the package on the SD card. Without a theme, LayoutEngine draws a black fallback
// screen with white text in a LovyanGFX built-in font.

void Task_MediaPlayer(void *pvParameters) {
  DLOG("[PLAY] task started");
  char currentId[32] = "";
  uint32_t lastClockRender = 0;
  uint32_t playStartTime = 0;

  if (appCtx.storage && appCtx.storage->getFirstValidIdentifier(currentId, sizeof(currentId))) {
    if (currentAppState == AppState::STATE_VIDEO) {
      appCtx.player.playItem(currentId);
      playStartTime = millis();
    }
  }

  auto drawToast = [](const char* msg) {
      appCtx.layoutEngine.renderStandbyScreen(&appCtx.display, &appCtx.network, true);
      if (appCtx.display.acquireSPI()) {
          appCtx.display.getTFT()->fillRect(0, 200, 240, 40, TFT_BLACK);
          appCtx.display.getTFT()->setTextColor(TFT_WHITE);
          appCtx.display.getTFT()->setTextDatum(lgfx::middle_center);
          appCtx.display.getTFT()->setTextSize(1);
          appCtx.display.getTFT()->drawString(msg, 120, 220);
          appCtx.display.releaseSPI();
      }
  };

  // OTA mode screen. ASCII only: the font has glyphs 32-126 only (see asciiFold in MediaPlayer).
  auto drawOtaScreen = []() {
      if (!appCtx.display.acquireSPI()) return;
      LGFX* tft = appCtx.display.getTFT();
      char line[48];
      tft->fillScreen(TFT_BLACK);
      tft->setTextDatum(lgfx::middle_center);
      tft->setTextColor(TFT_YELLOW);
      tft->setTextSize(2);
      tft->drawString("CHE DO NAP OTA", 120, 30);
      tft->setTextColor(TFT_WHITE);
      tft->setTextSize(1);
      snprintf(line, sizeof(line), "http://%s.local/", OTA_HOSTNAME);
      tft->drawString(line, 120, 75);
      snprintf(line, sizeof(line), "IP: %s", WiFi.localIP().toString().c_str());
      tft->drawString(line, 120, 95);
      snprintf(line, sizeof(line), "FW %s", FW_VERSION);
      tft->drawString(line, 120, 115);
      if (appCtx.otaHandler.isUpdating()) {
          snprintf(line, sizeof(line), "Dang nap %u%% - dung rut dien", (unsigned)appCtx.otaHandler.progressPercent());
      } else {
          snprintf(line, sizeof(line), "Cho nap...");
      }
      tft->setTextColor(TFT_GREEN);
      tft->drawString(line, 120, 150);
      // The ONLY mitigation for the accepted trade-off: forgetting to exit = missed alarms.
      tft->setTextColor(TFT_RED);
      tft->drawString("BAO THUC DANG TAT", 120, 190);
      tft->setTextColor(TFT_WHITE);
      tft->drawString("Thoat: giu 3s, 3s, roi 6s", 120, 210);
      appCtx.display.releaseSPI();
  };

  auto enterOtaMode = [&drawOtaScreen, &drawToast]() {
      // OTA over LAN is the ONLY path: without Wi-Fi a server is pointless.
      if (!appCtx.network.isConnected()) {
          DLOG("[OTA] khong co Wi-Fi -> khong vao che do");
          appCtx.display.turnOn();
          drawToast("OTA: can Wi-Fi");
          vTaskDelay(pdMS_TO_TICKS(1500));
          forceStandbyRedraw = true;
          return;
      }
      appCtx.player.stop();
      s_otaServerCmd = 1;  // Task_NetworkController starts the server within ≤ 50ms
      currentAppState = AppState::STATE_OTA;
      appCtx.display.turnOn();
      drawOtaScreen();
      DLOG("[OTA] vao che do: http://%s.local/ %s", OTA_HOSTNAME, WiFi.localIP().toString().c_str());
  };

  auto exitOtaMode = []() {
      // Do NOT allow exiting mid-flash. Task_UIController already swallows every
      // touch while isUpdating(); this is only the second latch.
      if (appCtx.otaHandler.isUpdating()) return;
      s_otaServerCmd = -1;
      currentAppState = AppState::STATE_STANDBY;
      forceStandbyRedraw = true;
      lastUserActivity = millis();
      DLOG("[OTA] thoat che do");
  };

  static constexpr const char* ALARM_HINT = "Cham: bao lai 5p - Giu: tat";
  char alarmTime[6] = "";
  AlarmItem ringItem;            // the ringing alarm: music, volume, ramp
  uint32_t alarmStartMs = 0;
  uint32_t lastBeepMs = 0;
  uint32_t lastAlarmPollMs = 0;
  // Touch sequence to enter/leave OTA mode: hold 3s → release → hold 3s → release →
  // hold 6s. Not one long hold, because the TTP223 recalibrates after 7-8s of
  // continuous touch (config.h). Only LONGs received in STANDBY or OTA count, so the
  // LONG that leaves a video (received in VIDEO) and the LONG that stops an alarm
  // (swallowed by the ALARM branch first) are never step 1.
  uint8_t  otaSeqStep = 0;      // 0 idle · 1 first hold done · 2 second hold done (prompt showing)
  uint32_t otaSeqStep2Ms = 0;   // when the second hold completed
  uint32_t otaSeqDeadline = 0;

  // Bottom strip y 200-239 (includes the progress bar area, so a redraw also erases the old bar).
  auto drawOtaPrompt = []() {
      if (!appCtx.display.acquireSPI()) return;
      LGFX* tft = appCtx.display.getTFT();
      tft->fillRect(0, 200, 240, 40, TFT_BLACK);
      tft->setTextDatum(lgfx::middle_center);
      tft->setTextSize(1);
      tft->setTextColor(TFT_YELLOW);
      tft->drawString(currentAppState == AppState::STATE_OTA ? "Giu them 6s de THOAT OTA"
                                                             : "Giu them 6s de vao OTA", 120, 214);
      appCtx.display.releaseSPI();
  };

  auto resetOtaSeq = [&otaSeqStep, &drawOtaScreen]() {
      bool hadPrompt = (otaSeqStep == 2);
      otaSeqStep = 0;
      if (!hadPrompt) return;
      if (currentAppState == AppState::STATE_STANDBY) forceStandbyRedraw = true;
      else if (currentAppState == AppState::STATE_OTA) drawOtaScreen();
  };

  // ---- Play command waiting for sync (see s_pendingPlay) ----
  uint32_t pendingPlaySinceMs = 0;
  uint32_t pendingPlayDeadline = 0;
  bool     pendingSawIdle = false;     // whether sync has been seen idle since it was last busy
  uint32_t pendingIdleSinceMs = 0;     // when it went idle, see PENDING_PLAY_SETTLE_MS

  // Bottom strip y 200-239, same place as the toast. A standby render overwrites it,
  // so the STANDBY loop redraws it after each render, like drawOtaPrompt.
  auto drawPendingHint = []() {
      if (!appCtx.display.acquireSPI()) return;
      LGFX* tft = appCtx.display.getTFT();
      tft->fillRect(0, 200, 240, 40, TFT_BLACK);
      tft->setTextDatum(lgfx::middle_center);
      tft->setTextSize(1);
      tft->setTextColor(TFT_WHITE);
      tft->drawString("Dang dong bo, se tu phat...", 120, 220);
      appCtx.display.releaseSPI();
  };

  // The deadline counts from the FIRST touch; more touches while waiting don't extend it.
  auto armPendingPlay = [&pendingPlaySinceMs, &pendingPlayDeadline, &pendingSawIdle]() {
      if (s_pendingPlay) return;
      pendingSawIdle = false;
      pendingPlaySinceMs = millis();
      pendingPlayDeadline = pendingPlaySinceMs + PENDING_PLAY_MAX_WAIT_MS;
      s_pendingPlay = true;
      DLOG("[PLAY] pending: sync dang chay");
  };

  auto cancelPendingPlay = []() {
      if (!s_pendingPlay) return;
      s_pendingPlay = false;
      if (currentAppState == AppState::STATE_STANDBY) forceStandbyRedraw = true;
      DLOG("[PLAY] pending huy");
  };

  // Play the next unread message from STANDBY. fromPending = running the queued
  // command (see the hasPendingMessages() branch below).
  auto startNextUnread = [&currentId, &playStartTime, &drawToast, &armPendingPlay](bool fromPending) {
      char unreadId[32] = "";
      // Ask the STORAGE, not the RAM counter. getNumOfNewMsg() is only set inside
      // checkAndDownloadNewMessages() — which sits BEHIND the isFull() gate — so
      // after a reset while the slots are full it stays 0 while the unread flags
      // on storage remain: the box would say "No new messages" on touch and "out
      // of slots" on sync, and slots are only freed by reading -> a dead end.
      if (appCtx.storage && appCtx.storage->getNextUnreadIdentifier(unreadId, sizeof(unreadId))) {
         // Set VIDEO BEFORE playItem(): triggerWakeupSync() refuses while
         // isPlaybackActive(), so no new sync slips in while playback buffers are allocated.
         currentAppState = AppState::STATE_VIDEO;
         appCtx.display.clear();
         strncpy(currentId, unreadId, sizeof(currentId) - 1);
         if (appCtx.player.playItem(currentId)) {
             playStartTime = millis();
         } else {
             DLOG("[PLAY] FAIL -> STANDBY");
             currentAppState = AppState::STATE_STANDBY;
             forceStandbyRedraw = true;
         }
      } else if (appCtx.network.hasPendingMessages()) {
         // No unread messages left on storage, but the previous download round had
         // to stop for lack of slots -> reading just freed space, so fetch more now.
         DLOG("[PLAY] no local unread, downloading...");
         appCtx.player.stop();
         drawToast("Downloading...");
         uint8_t batPercent = appCtx.powerManager.getBatteryPercentage();
         bool isCharging = appCtx.powerManager.isCharging();
         appCtx.network.triggerFirebaseSync(batPercent, isCharging, appCtx.storage);
         // Auto-play once downloaded; don't require another touch. Wait only if a sync
         // REALLY started: triggerWakeupSync() sets _isSyncing in a critical section
         // before returning; if refused or the task failed to start, isSyncing() is
         // false -> waiting would just spin.
         if (appCtx.network.isSyncing()) {
             // Running from the queued command: keep the ORIGINAL deadline, counted from
             // the first touch. With Wi-Fi down and _hasPendingMessages still true, every
             // failed sync comes back here; extending each time would retry forever.
             if (fromPending) s_pendingPlay = true;
             else armPendingPlay();
         }
      } else {
         appCtx.display.turnOn();
         drawToast("No new messages");
         vTaskDelay(1000);
         forceStandbyRedraw = true;
      }
  };

  for (;;) {
    // Alarms: polled every 500ms in every state (they ring even over playback),
    // except while already ringing, while flashing OTA, or in OTA MODE (product
    // decision: alarms are fully off in that mode).
    if (currentAppState != AppState::STATE_ALARM && currentAppState != AppState::STATE_OTA &&
        !appCtx.otaHandler.isUpdating() &&
        millis() - lastAlarmPollMs >= 500) {
      lastAlarmPollMs = millis();
      if (AlarmClock::instance().pollDue(time(nullptr), alarmTime, sizeof(alarmTime), &ringItem)) {
        if (currentAppState == AppState::STATE_VIDEO) appCtx.player.stop();
        currentAppState = AppState::STATE_ALARM;
        // Play the music if it is on the card; every other case (no music chosen, not
        // downloaded yet, card error, bad file) takes ONE branch: the beep.
        s_alarmMusicOn = false;
        if (ringItem.musicId[0] && MusicStore::has(ringItem.musicId)) {
          char musicPath[48];
          MusicStore::pathFor(ringItem.musicId, musicPath, sizeof(musicPath));
          uint8_t startVol = ringItem.ramp ? (uint8_t)(ringItem.volume * ALARM_RAMP_START_PCT / 100)
                                           : ringItem.volume;
          if (appCtx.player.startAlarmMusic(musicPath, startVol)) {
            s_alarmMusicOn = true;
            MusicStore::touch(ringItem.musicId);
          }
        }
        // A touch queued earlier must not dismiss the alarm that just started.
        xQueueReset(eventQueue);
        // Nor may a message auto-play after the alarm is dismissed.
        s_pendingPlay = false;
        // After a timer wake the screen is still off (turnOn only runs on a touch wake).
        appCtx.display.turnOn();
        // The user's brightness may be as low as 5%: someone being woken must see the screen.
        appCtx.display.setBacklight(Settings::alarmBacklight());
        appCtx.layoutEngine.renderAlarmScreen(&appCtx.display, alarmTime, ALARM_HINT);
        alarmStartMs = millis();
        lastBeepMs = 0;
        lastUserActivity = millis();
      }
    }

    // OTA touch sequence timeout. Do NOT cancel under the user's finger: a final hold
    // started before the deadline is allowed to complete.
    if (otaSeqStep != 0 && (int32_t)(millis() - otaSeqDeadline) > 0 &&
        appCtx.ui.getTouchHoldMs() == 0) {
      resetOtaSeq();
    }

    SystemEvent event = SystemEvent::NONE;
    while (xQueueReceive(eventQueue, &event, 0) == pdTRUE) {
      // event loop — no logging here, to avoid spamming the screen

      if (currentAppState == AppState::STATE_ALARM) {
        // Short touch = snooze 5 minutes; 3s hold = dismiss.
        if (event == SystemEvent::TOUCH_SHORT) {
          AlarmClock::instance().snooze(time(nullptr));
          appCtx.layoutEngine.renderAlarmScreen(&appCtx.display, alarmTime, "Bao lai sau 5 phut");
          vTaskDelay(pdMS_TO_TICKS(1500));
        } else if (event == SystemEvent::TOUCH_LONG) {
          AlarmClock::instance().dismiss();
        } else {
          continue;
        }
        // Free the 24KB of I2S DMA: beep() intentionally keeps the driver between beeps.
        appCtx.player.stop();
        s_alarmMusicOn = false;
        currentAppState = AppState::STATE_STANDBY;
        appCtx.display.setBacklight(Settings::currentBacklight());
        forceStandbyRedraw = true;
        lastUserActivity = millis();
        continue;
      }

      // A hold = the user is done watching. Still falls through to the OTA sequence.
      if (event == SystemEvent::TOUCH_LONG) cancelPendingPlay();

      // ---- OTA touch sequence (see otaSeqStep) ----
      bool otaSeqState = (currentAppState == AppState::STATE_STANDBY ||
                          currentAppState == AppState::STATE_OTA);
      if (event == SystemEvent::TOUCH_LONG && otaSeqState) {
        if (otaSeqStep == 0) {
          otaSeqStep = 1;
          otaSeqDeadline = millis() + OTA_SEQ_STEP_WINDOW_MS;
        } else if (otaSeqStep == 1) {
          otaSeqStep = 2;
          otaSeqStep2Ms = millis();
          otaSeqDeadline = millis() + OTA_SEQ_FINAL_WINDOW_MS;
          drawOtaPrompt();
        }
        // Step 2: this is the LONG at second 3 of the final hold — let it keep holding
        // to 6s. In STANDBY a LONG is a no-op anyway; in OTA every event is ignored.
        continue;
      }
      if (event == SystemEvent::TOUCH_OTA_TOGGLE) {
        // The 6s hold must be a NEW hold that started AFTER step 2. Without this check,
        // holding the second hold on to 6s would shorten the sequence to two steps. The
        // hold that fired this event began at (now - TOUCH_OTA_HOLD_MS) — queue latency
        // is a few ms, while the second hold began up to 3s before the step-2 mark, so
        // the margin is wide.
        bool freshHold = (int32_t)((millis() - TOUCH_OTA_HOLD_MS) - otaSeqStep2Ms) > 0;
        if (otaSeqStep == 2 && freshHold && otaSeqState) {
          otaSeqStep = 0;  // enter/exit redraw the screen themselves; no resetOtaSeq() needed
          if (currentAppState == AppState::STATE_OTA) exitOtaMode();
          else enterOtaMode();
        }
        continue;
      }
      if (event == SystemEvent::TOUCH_SHORT && otaSeqStep == 2) {
        resetOtaSeq();  // prompt showing: a short touch CANCELS, it doesn't play
        continue;
      }
      if (event == SystemEvent::TOUCH_SHORT) {
        otaSeqStep = 0;  // step 1 shows nothing yet: cancel silently, the touch still works normally
      }
      // OTA mode only accepts the exit sequence above; every other event is ignored.
      if (currentAppState == AppState::STATE_OTA) continue;

      if (event == SystemEvent::TOUCH_SHORT) {
        if (currentAppState == AppState::STATE_STANDBY) {
           if (appCtx.network.isDownloadingMedia()) {
               // Product decision: a touch WHILE DOWNLOADING is ignored, not queued.
               // (Task_UIController already swallows touches during a download; this
               // branch only catches a download that began between post and receive.)
               appCtx.player.stop();
               drawToast("Downloading...");
           } else if (appCtx.network.isSyncing()) {
               // Sync isn't downloading media (Wi-Fi, NTP, flags, status, or the gap
               // between two messages) but may hold a TLS session -> don't play over
               // it; queue instead. After a timer wake the screen is still off.
               armPendingPlay();
               appCtx.display.turnOn();
               drawToast("Dang dong bo, se tu phat...");
           } else {
               startNextUnread(false);
           }
        } else if (currentAppState == AppState::STATE_VIDEO) {
           if (event == SystemEvent::TOUCH_SHORT) {
               appCtx.storage->markAsRead(currentId);
               appCtx.network.decrementNewMsgCount();
           }

           char unreadId[32] = "";
           // Same reason as the STANDBY branch above: the source of truth is the
           // unread flag on storage, not the RAM counter.
           if (appCtx.storage && appCtx.storage->getNextUnreadIdentifier(unreadId, sizeof(unreadId))) {
               strncpy(currentId, unreadId, sizeof(currentId) - 1);
               if (appCtx.player.playItem(currentId)) {
                   playStartTime = millis();
               } else {
                   DLOG("[PLAY] FAIL -> STANDBY");
                   currentAppState = AppState::STATE_STANDBY;
                   forceStandbyRedraw = true;
               }
           } else if (appCtx.network.hasPendingMessages()) {
               appCtx.player.stop();
               currentAppState = AppState::STATE_STANDBY;
               drawToast("Downloading...");
               uint8_t batPercent = appCtx.powerManager.getBatteryPercentage();
               bool isCharging = appCtx.powerManager.isCharging();
               appCtx.network.triggerFirebaseSync(batPercent, isCharging, appCtx.storage);
               // Auto-play once downloaded, no second touch needed (same condition as startNextUnread).
               if (appCtx.network.isSyncing()) armPendingPlay();
           } else {
               appCtx.player.stop();
               if (appCtx.display.acquireSPI()) {
                   appCtx.display.getTFT()->fillRect(0, 200, 240, 40, TFT_BLACK);
                   appCtx.display.getTFT()->setTextColor(TFT_WHITE);
                   appCtx.display.getTFT()->setTextDatum(lgfx::middle_center);
                   appCtx.display.getTFT()->setTextSize(1);
                   appCtx.display.getTFT()->drawString("Reached newest msg", 120, 220);
                   appCtx.display.releaseSPI();
               }
               vTaskDelay(1500);
               currentAppState = AppState::STATE_STANDBY;
               forceStandbyRedraw = true;
           }
        }
      } else if (event == SystemEvent::TOUCH_LONG || event == SystemEvent::TIMEOUT_AUTO_NEXT) {
        if (currentAppState == AppState::STATE_VIDEO) {
           appCtx.player.stop();
           currentAppState = AppState::STATE_STANDBY;
           forceStandbyRedraw = true;
           lastUserActivity = millis();
        }
      }
    }

    // Queued play command: polled every loop instead of using a callback, because
    // _isSyncing is cleared on ~12 different exits of syncWakeup(). isSyncing() covers
    // Wi-Fi, NTP, Firebase and media download. The Wi-Fi setup AP does NOT count as
    // busy (product decision): the AP may stay up indefinitely and opens no TLS.
    //
    // The PENDING_PLAY_SETTLE_MS pause after sync is COUNTED in the loop; never block
    // the task. Beware `sleep(2000)`: that is POSIX sleep() in SECONDS -> it blocks
    // this task ~33 minutes: no playback, queued touches unhandled, a black screen on
    // wake (nobody renders). Even vTaskDelay(2000) is wrong: during those 2s
    // s_pendingPlay is already cleared, so the box may sleep or a new sync may slip in.
    if (s_pendingPlay && currentAppState == AppState::STATE_STANDBY) {
      if (appCtx.network.isSyncing()) {
        pendingSawIdle = false;
        if ((int32_t)(millis() - pendingPlayDeadline) > 0) {
          s_pendingPlay = false;
          forceStandbyRedraw = true;
          DLOG("[PLAY] pending het han %lus", (unsigned long)(PENDING_PLAY_MAX_WAIT_MS / 1000));
        }
      } else if (!pendingSawIdle) {
        pendingSawIdle = true;
        pendingIdleSinceMs = millis();
      } else if (millis() - pendingIdleSinceMs >= PENDING_PLAY_SETTLE_MS) {
        s_pendingPlay = false;
        pendingSawIdle = false;
        DLOG("[PLAY] pending fire waited=%lums heap=%u maxblk=%u",
             (unsigned long)(millis() - pendingPlaySinceMs), (unsigned)ESP.getFreeHeap(),
             (unsigned)ESP.getMaxAllocHeap());
        startNextUnread(true);
      }
    }

    // Brightness changed from the web (the WakeSync task writes Settings): apply now if
    // the screen is on. The alarm screen keeps its own floor; a screen that is off and
    // about to sleep picks up the new level in turnOn().
    static uint32_t seenBrightnessEpoch = 0;
    if (seenBrightnessEpoch != Settings::brightnessEpoch.load()) {
      seenBrightnessEpoch = Settings::brightnessEpoch.load();
      if (!appCtx.display.isSleeping()) {
        appCtx.display.setBacklight(currentAppState == AppState::STATE_ALARM
                                        ? Settings::alarmBacklight()
                                        : Settings::currentBacklight());
      }
    }

    if (currentAppState == AppState::STATE_VIDEO) {
      if (!appCtx.otaHandler.isUpdating()) {
        appCtx.player.update();
        
        if (playStartTime > 0 && appCtx.storage) {
            StorageItemInfo info = appCtx.storage->getItemInfo(currentId);
            uint32_t maxDispMs = (info.maxDisplayTime > 0 ? info.maxDisplayTime : 60) * 1000;
            if (millis() - playStartTime >= maxDispMs) {
                playStartTime = 0; 
                SystemEvent timeoutEv = SystemEvent::TIMEOUT_AUTO_NEXT;
                xQueueSend(eventQueue, &timeoutEv, 0);
            }
        }
      } else {
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    } else if (currentAppState == AppState::STATE_STANDBY) {
      uint32_t now = millis();
      // A new theme package finished downloading (WakeSync): this task is the sole owner
      // of LayoutEngine, so it installs here. Wait for sync to end: flash erase/write
      // stalls the CPU ~2s and must not land in the middle of a TLS session.
      if (ThemeStore::installPending() && !appCtx.network.isSyncing() && !s_pendingPlay) {
        char dir[64], id[24];
        uint32_t rev = 0;
        if (ThemeStore::takeInstallRequest(dir, sizeof(dir), id, sizeof(id), &rev)) {
          drawToast("Dang ap dung giao dien...");
          ThemeStore::installFromSd(dir, id, rev);
          appCtx.layoutEngine.loadTheme();  // on failure it falls back to the fallback screen by itself
          forceStandbyRedraw = true;
          lastUserActivity = millis();
        }
      }
      if (now - lastClockRender >= 1000 || forceStandbyRedraw) {
        bool fullRedraw = (lastClockRender == 0) || forceStandbyRedraw;
        appCtx.layoutEngine.renderStandbyScreen(&appCtx.display, &appCtx.network, fullRedraw);
        lastClockRender = now;
        forceStandbyRedraw = false;
        if (s_pendingPlay) drawPendingHint();
        // The render may have overwritten the prompt. Don't draw while a finger is
        // held: it would erase the final hold's progress bar.
        if (otaSeqStep == 2 && appCtx.ui.getTouchHoldMs() == 0) drawOtaPrompt();
      }
      // Flush the log to the card in batches, on standby only (no bus contention with playback).
      static uint32_t lastLogFlush = 0;
      if (now - lastLogFlush >= 30000) {
        lastLogFlush = now;
        SdLog::flush();
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    } else if (currentAppState == AppState::STATE_ALARM) {
      // Refresh the activity mark so Task_UIController's sleep loop doesn't cut in while ringing.
      lastUserActivity = millis();
      if (millis() - alarmStartMs >= ALARM_RING_MAX_MS) {
        DLOG("[ALM] het 1 phut -> tu tat");
        AlarmClock::instance().dismiss();
        appCtx.player.stop();  // frees the 24KB of I2S DMA
        s_alarmMusicOn = false;
        currentAppState = AppState::STATE_STANDBY;
        appCtx.display.setBacklight(Settings::currentBacklight());
        forceStandbyRedraw = true;
      } else if (s_alarmMusicOn) {
        // Linear ramp on the volume scale (which is itself in dB): 30% -> 100% of the
        // chosen level over ALARM_RAMP_MS.
        uint8_t vol = ringItem.volume;
        uint32_t t = millis() - alarmStartMs;
        if (ringItem.ramp && t < ALARM_RAMP_MS) {
          uint32_t start = (uint32_t)ringItem.volume * ALARM_RAMP_START_PCT / 100;
          vol = (uint8_t)(start + (ringItem.volume - start) * t / ALARM_RAMP_MS);
        }
        appCtx.player.tickAlarmMusic(vol);
        vTaskDelay(pdMS_TO_TICKS(20));  // DMA holds ~96ms @16kHz: refilling every 20ms is plenty
      } else if (lastBeepMs == 0 || millis() - lastBeepMs >= ALARM_BEEP_PERIOD_MS) {
        lastBeepMs = millis();
        // The fallback beep is ALWAYS at 100: scaling it by the alarm volume (default
        // 80 ≈ -8dB) could make it inaudible to someone asleep — and this branch runs
        // exactly when the music has failed.
        appCtx.player.alarmBeep(100);  // blocks ~0.6s; touches during it still queue
      } else {
        vTaskDelay(pdMS_TO_TICKS(20));
      }
    } else if (currentAppState == AppState::STATE_OTA) {
      // Redraw every second so the "Dang nap N%" progress line advances. Not while a
      // finger is held (the progress bar below needs the whole bottom strip).
      static uint32_t lastOtaRender = 0;
      if (millis() - lastOtaRender >= 1000 && appCtx.ui.getTouchHoldMs() == 0) {
        lastOtaRender = millis();
        drawOtaScreen();
        if (otaSeqStep == 2) drawOtaPrompt();  // drawOtaScreen() just cleared the whole screen
      }
      vTaskDelay(pdMS_TO_TICKS(50));
    }

    // The progress bar is ONLY for the final 6s hold: at step 2 and with a hold that
    // began AFTER the step-2 mark. Without the second condition, the remainder of the
    // second hold (already past 3s) would show as if the final hold were half done.
    static bool holdBarShown = false;
    uint32_t holdMs = appCtx.ui.getTouchHoldMs();
    bool finalHold = (otaSeqStep == 2) && holdMs > 0 &&
                     (int32_t)((millis() - holdMs) - otaSeqStep2Ms) > 0;
    if (finalHold) {
      uint32_t w = (holdMs >= TOUCH_OTA_HOLD_MS) ? 240 : (holdMs * 240 / TOUCH_OTA_HOLD_MS);
      if (appCtx.display.acquireSPI()) {
        appCtx.display.getTFT()->fillRect(0, 232, 240, 8, TFT_DARKGREY);
        appCtx.display.getTFT()->fillRect(0, 232, (int32_t)w, 8, TFT_YELLOW);
        appCtx.display.releaseSPI();
      }
      holdBarShown = true;
    } else if (holdBarShown && holdMs == 0) {
      // Finger released: released early (still at step 2) → redraw the prompt, which
      // also erases the bar; mode entered/left → redraw the current screen.
      holdBarShown = false;
      if (otaSeqStep == 2) drawOtaPrompt();
      else if (currentAppState == AppState::STATE_STANDBY) forceStandbyRedraw = true;
      else if (currentAppState == AppState::STATE_OTA) drawOtaScreen();
    }
  }
}

void Task_UIController(void *pvParameters) {
  DLOG("[UI] task started");
  uint32_t activeSleepTimeoutMs = INACTIVITY_SLEEP_TIMEOUT_MS;

  for (;;) {
    TouchEvent tEvent = appCtx.ui.getTouchEvent();
    if (tEvent != TouchEvent::NONE) {
      // While flashing OTA, block EVERY touch — same mechanism as during a download —
      // including the exit sequence: exiting midway would stop the web server while
      // the file is still arriving.
      if ((appCtx.network.isDownloadingMedia() || appCtx.otaHandler.isUpdating()) &&
          currentAppState != AppState::STATE_ALARM) {
          // Ignore touches while downloading — no log, to avoid spam. Except while an
          // alarm rings: the user must not have to wait for a download to dismiss it.
      } else {
          SystemEvent event = (tEvent == TouchEvent::VERY_LONG_PRESS) ? SystemEvent::TOUCH_OTA_TOGGLE
                            : (tEvent == TouchEvent::LONG_PRESS)      ? SystemEvent::TOUCH_LONG
                                                                      : SystemEvent::TOUCH_SHORT;
          xQueueSend(eventQueue, &event, 0);
          lastUserActivity = millis();
          activeSleepTimeoutMs = INACTIVITY_SLEEP_TIMEOUT_MS;
      }
    }

    uint32_t now = millis();
    static uint32_t lastIntervalSyncMs = millis();

    // While a background sync runs, keep pushing the timestamp forward -> the period
    // is counted from when the sync ENDS, not when it starts (otherwise a 30s download
    // would be followed by another sync right away).
    if (appCtx.network.isSyncing()) {
        lastIntervalSyncMs = now;
    }

    // The new OTA image survived OTA_VERIFY_DELAY_MS -> confirm it and cancel the
    // safety net. From here on the bootloader no longer rolls back.
    if (s_otaPendingVerify && millis() >= OTA_VERIFY_DELAY_MS) {
        esp_ota_mark_app_valid_cancel_rollback();
        if (s_otaGuardTimer != nullptr) {
            esp_timer_stop(s_otaGuardTimer);
            esp_timer_delete(s_otaGuardTimer);
            s_otaGuardTimer = nullptr;
        }
        s_otaPendingVerify = false;
        DLOG("[OTA] ban moi da xac nhan (%s)", FW_VERSION);
    }

    // There is intentionally NO `!isStorageFull` condition here: full slots only mean
    // "don't download messages", not "stop the heartbeat / flag reads / alarm sync".
    // That gate lives at the download step inside syncWakeup(). Known trade-off: a
    // box with full slots still syncs periodically and uses more battery.
    if (now - lastIntervalSyncMs >= SYNC_INTERVAL_MS && !appCtx.network.isSyncing() && currentAppState == AppState::STATE_STANDBY) {
        lastIntervalSyncMs = now;
        uint8_t batPercent = appCtx.powerManager.getBatteryPercentage();
        bool isCharging = appCtx.powerManager.isCharging();
        appCtx.network.triggerFirebaseSync(batPercent, isCharging, appCtx.storage);
    }

    // secondsToNext <= 2: the alarm minute is about to start (or has) and pollDue hasn't
    // fired yet. Sleeping now would miss the whole minute even with the minimum timer
    // -> stay awake.
    if (currentAppState != AppState::STATE_VIDEO &&
        currentAppState != AppState::STATE_ALARM &&
        !appCtx.otaHandler.isUpdating() && !appCtx.network.isProvisioningActive() &&
        !appCtx.network.isSyncing() && currentAppState != AppState::STATE_OTA &&
        // No sleeping during the probation window: right after waking, the esp_timer
        // safety net (priority 22) could fire BEFORE this task confirms -> a good image
        // gets reset for nothing. INACTIVITY_SLEEP_TIMEOUT_MS is also 60s, i.e. it
        // lands exactly in that window.
        !s_otaPendingVerify &&
        // A play command is queued: this task outranks Task_MediaPlayer, so without
        // this guard it could put the chip to sleep the moment sync ends, before the
        // queued command runs.
        !s_pendingPlay &&
        (now - lastUserActivity >= activeSleepTimeoutMs) &&
        AlarmClock::instance().secondsToNext(time(nullptr)) > 2) {
      DLOG("[SLP] timeout -> sleeping");

      time_t nowSec = time(nullptr);
      uint32_t secToAlarm = AlarmClock::instance().secondsToNext(nowSec);
      uint64_t sleepTimeUs = SLEEP_TIMER_US;
      if (secToAlarm != 0xFFFFFFFF && secToAlarm > 0) {
        uint64_t alarmUs = (uint64_t)secToAlarm * 1000000ULL;
        if (alarmUs < sleepTimeUs) {
          sleepTimeUs = alarmUs;
        }
      }

      DLOG("[SLP] sleep %llus", (unsigned long long)(sleepTimeUs / 1000000ULL));
      SdLog::flush();  // the box may sleep and then lose power: write the rest of the log first

      // Stop MediaPlayer to free SPI/RAM and return to standby before sleeping
      appCtx.player.stop();
      currentAppState = AppState::STATE_STANDBY;
      // Don't set forceStandbyRedraw here: it would race with Task_MediaPlayer
      
      appCtx.powerManager.enterLightSleep(sleepTimeUs, &appCtx.display);

      // Just woke up: mark it so the next ensureConnected() FORCES a re-association.
      // After light sleep WiFi.status() often still reports WL_CONNECTED although the
      // association is dead -> trusting it makes every http.GET() return -1.
      appCtx.network.notifyWakeFromSleep();

      // Called IMMEDIATELY after waking (before the delay): the other FreeRTOS tasks
      // haven't resumed yet, so the SPI bus is uncontended and the GPIO is safe to touch.
      appCtx.display.wakeupFlash();

      delay(200);
      esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
      const char* causeStr = (wakeupCause == ESP_SLEEP_WAKEUP_TIMER) ? "timer" : "touch";
      DLOG("[SLP] wakeup=%s", causeStr);

      if (wakeupCause != ESP_SLEEP_WAKEUP_TIMER) {
        // Touch wake: re-init the display and render the standby UI.
        currentAppState = AppState::STATE_STANDBY;
        appCtx.display.turnOn();
        // currentAppState = AppState::STATE_STANDBY;
        forceStandbyRedraw = true;
        lastUserActivity = millis();
        activeSleepTimeoutMs = INACTIVITY_SLEEP_TIMEOUT_MS;
      } else {
        lastUserActivity = millis();
        activeSleepTimeoutMs = 2000;

        // Blink the blue LED (GPIO 8 on the SuperMini, shared with NAND CS) to signal a
        // background wake. This is the ONLY timer-wake indicator left: wakeupFlash()
        // in DisplayDriver.cpp now only calls gpio_hold_dis() — the name is outdated,
        // it blinks nothing. Product decision: keep the LED.
        //
        // spiMutex MUST be held for the whole blink. The bus is NOT guaranteed idle:
        // Task_MediaPlayer may be clocking pixels out on SCK/MOSI, and GPIO 8 is the
        // W25Q128's CS. Pulling CS LOW for 30ms while the clock toggles -> the NAND
        // latches a bogus opcode. Holding the mutex removes exactly that mechanism (CS
        // LOW without a clock is harmless). Cost: the mutex is held 160ms, plus up to
        // 1000ms of waiting (acquireSPI's default timeout) in the worst case. Right
        // after waking, SPI is usually idle.
        if (appCtx.display.acquireSPI()) {
          pinMode(8, OUTPUT);
          digitalWrite(8, LOW);  // LED on (active LOW) / NAND CS pulled low
          delay(30);
          digitalWrite(8, HIGH); // LED off / NAND CS released
          delay(70);
          digitalWrite(8, LOW);
          delay(30);
          digitalWrite(8, HIGH);
          appCtx.display.releaseSPI();
        } else {
          // This must be logged: the LED is the only timer-wake indicator, so a silent
          // "no blink" would be mistaken for a broken LED.
          DLOG("[WAKE] blink skip: spi busy");
        }
      }

      // Wait 200ms for the UI and SPI bus to settle before starting the background sync task
      vTaskDelay(pdMS_TO_TICKS(200));

      // Non-blocking background sync after waking.
      // Always check for new messages and download them fully into slots before
      // allowing playback — there is no "local messages exist, defer sync" branch
      // (it would miss new messages in the cloud).
      // Always sync, even with full slots: skipping the whole cycle when full would
      // cost the box its alarms and OTA until a slot frees up. The "full" gate lives
      // inside syncWakeup() and blocks only the download step.
      // syncWakeup() already includes ensureConnected() + syncNtpTime().
      uint8_t batPercent = appCtx.powerManager.getBatteryPercentage();
      bool isCharging = appCtx.powerManager.isCharging();
      appCtx.network.triggerFirebaseSync(batPercent, isCharging, appCtx.storage);
      lastIntervalSyncMs = millis();
    }

    if (appCtx.network.isDownloadingMedia()) {
      lastUserActivity = millis();
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void Task_NetworkController(void *pvParameters) {
  for (;;) {
    // Execute the OTA server start/stop command on THE task that calls handleClient() — see s_otaServerCmd.
    int8_t cmd = s_otaServerCmd;
    if (cmd != 0) {
      s_otaServerCmd = 0;
      if (cmd > 0) {
        appCtx.network.startWebServer(OTA_HOSTNAME);
        // stopWebServer() deletes it and sets nullptr, so each entry gets a fresh
        // WebServer: routes are registered exactly once per instance, no handler pile-up.
        if (appCtx.network.getWebServer() != nullptr) {
          appCtx.otaHandler.registerRoutes(*appCtx.network.getWebServer());
        }
      } else {
        appCtx.network.stopWebServer();
      }
    }
    appCtx.network.update();
    // SAME task as handleClient() above, so the watchdog never runs in parallel
    // with an Update.write() in progress — no extra locking needed.
    appCtx.otaHandler.tickWatchdog();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void setup() {
  // Check the partition state and arm the safety net FIRST, before anything that can
  // hang (including the while(1) right below). Only a freshly OTA-flashed image is
  // PENDING_VERIFY.
  {
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &st) == ESP_OK &&
        st == ESP_OTA_IMG_PENDING_VERIFY) {
      s_otaPendingVerify = true;
      const esp_timer_create_args_t args = {
          .callback = &otaGuardFire, .arg = nullptr, .dispatch_method = ESP_TIMER_TASK,
          .name = "ota_guard", .skip_unhandled_events = false};
      // +30s past the confirmation mark: enough room for Task_UIController (10ms tick)
      // to confirm first, yet short enough that a hung image doesn't sit idle for long.
      if (esp_timer_create(&args, &s_otaGuardTimer) == ESP_OK) {
        esp_timer_start_once(s_otaGuardTimer, (uint64_t)(OTA_VERIFY_DELAY_MS + 30000) * 1000ULL);
      }
    }
  }

  spiMutex = xSemaphoreCreateMutex();
  eventQueue = xQueueCreate(8, sizeof(SystemEvent));

  if (spiMutex == nullptr || eventQueue == nullptr) {
    // No display/logger yet at this point; just halt.
    while (1) delay(1000);
  }

  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, -1);

  appCtx.display.init(spiMutex);
  // User settings (NVS) must be loaded BEFORE the backlight first turns on.
  Settings::begin();
  appCtx.display.setBacklight(Settings::currentBacklight());
  appCtx.display.showMessage("Booting...");

  // ScreenLogger needs the display to be ready
  ScreenLogger::init(&appCtx.display);
  DLOG("[BOOT] cpu=%uMHz heap=%u", ESP.getCpuFreqMHz(), ESP.getFreeHeap());
  DLOG("[BOOT] wakeup=%d", (int)esp_sleep_get_wakeup_cause());
  // reset=1 POWERON, 3 SW, 4 INT_WDT, 5 TASK_WDT, 6 WDT, 9 BROWNOUT, 12 PANIC.
  // If this line repeats regularly in the log ⇒ the box is in a reset loop, not an audio fault.
  DLOG("[BOOT] reset=%d", (int)esp_reset_reason());
  if (s_otaPendingVerify) {
    DLOG("[OTA] ban moi dang thu thach %lus", (unsigned long)(OTA_VERIFY_DELAY_MS / 1000));
  }

  appCtx.network.init();
  appCtx.configManager.init(NVS_NAMESPACE);
  AlarmClock::instance().begin();

  char wifiSsid[WIFI_SSID_MAX_LEN] = "";
  char wifiPass[WIFI_PASS_MAX_LEN] = "";

  bool credsFromNvs = appCtx.configManager.loadWiFi(wifiSsid, wifiPass);
  if (!credsFromNvs) {
    strncpy(wifiSsid, DEFAULT_WIFI_SSID, sizeof(wifiSsid) - 1);
    strncpy(wifiPass, DEFAULT_WIFI_PASSWORD, sizeof(wifiPass) - 1);
  }

  // Log where the credentials came from: the SSID alone can't tell "default creds"
  // from "stale creds left in NVS".
  DLOG("[BOOT] WiFi: %s (%s)", wifiSsid, credsFromNvs ? "NVS" : "default");

  if (appCtx.network.connectWiFi(wifiSsid, wifiPass) != WiFiConnectResult::CONNECTED && credsFromNvs) {
    // Creds in NVS (saved by an earlier provisioning) may be stale — e.g. the router
    // password changed. Try the config.h defaults too before giving up, instead of
    // going straight to AP mode whenever NVS holds ANY SSID.
    DLOG("[BOOT] NVS creds fail -> try default");
    strncpy(wifiSsid, DEFAULT_WIFI_SSID, sizeof(wifiSsid) - 1);
    strncpy(wifiPass, DEFAULT_WIFI_PASSWORD, sizeof(wifiPass) - 1);
    appCtx.network.connectWiFi(wifiSsid, wifiPass);
  }

#if ACTIVE_STORAGE_TYPE == STORAGE_TYPE_SD
  DLOG("[BOOT] storage: SD");
  appCtx.storage = new SDStorageProvider();
#else
  DLOG("[BOOT] storage: NAND");
  appCtx.storage = new NandStorageProvider();
#endif

  if (!appCtx.storage->init(spiMutex)) {
    DLOG("[BOOT] ERR: storage init FAIL");
    appCtx.display.showMessage("Storage Err!");
    while (1) {
      delay(100);
    }
  }

#if ERASE_NOR_ON_BOOT == 1
  DLOG("[BOOT] erasing NOR flash...");
  appCtx.display.showMessage("Erasing NOR...");
  appCtx.storage->formatStorage();
  delay(1000);
#endif

  // Creates the /sys /theme /alarm tree, removes leftover .tmp files, measures free space.
  SdStore::begin(appCtx.storage);
  MusicStore::load();  // table of alarm music already on the card (looked up at ring time without touching the card)
  // The theme in the flash partition. If it is empty (e.g. a new partition table was
  // just cable-flashed) while the card still has the active package, copy it back now
  // — no task is drawing yet, so it is safe.
  if (!ThemeStore::begin()) ThemeStore::restoreFromSdIfNeeded();
  appCtx.layoutEngine.loadTheme();

  appCtx.player.init(appCtx.storage, &appCtx.display);
  
  // Speaker test beep at boot
  appCtx.player.testAudioBeep();
  
  appCtx.ui.init(PIN_TOUCH, &appCtx.display);
  appCtx.powerManager.init((gpio_num_t)PIN_TOUCH);
  lastUserActivity = millis();
  appCtx.ui.showBootScreen();

  if (appCtx.network.isConnected()) {
    DLOG("[BOOT] WiFi OK -> NTP+Firebase");
    // The OTA web server is NOT started here: running it for the box's whole uptime,
    // for something rarely done, would hold the WebServer + mDNS RAM. It is created
    // only when the user performs the 3s-3s-6s touch sequence into STATE_OTA
    // (enterOtaMode in Task_MediaPlayer); there is no cloud-flag trigger.

    appCtx.network.setOnDownloadComplete([]() {
      forceStandbyRedraw = true;
    });

    appCtx.network.setPlaybackActiveCallback([]() {
      return (currentAppState == AppState::STATE_VIDEO) || s_alarmMusicOn.load();
    });

    // Start a background Firebase sync right after boot
    uint8_t batPercent = appCtx.powerManager.getBatteryPercentage();
    bool isCharging = appCtx.powerManager.isCharging();
    appCtx.network.triggerFirebaseSync(batPercent, isCharging, appCtx.storage);
  } else {
    DLOG("[BOOT] no WiFi -> AP mode");
    appCtx.display.showMessage("Setup Wi-Fi:\nSendloveBox-Setup");
    appCtx.network.startProvisioningAP("SendloveBox-Setup");
  }

  xTaskCreate(Task_MediaPlayer, "MediaPlayer", TASK_STACK_MEDIA_PLAYER, nullptr,
              TASK_PRIORITY_MEDIA_PLAYER, nullptr);
  xTaskCreate(Task_UIController, "UIController", TASK_STACK_UI_CONTROLLER,
              nullptr, TASK_PRIORITY_UI_CONTROLLER, nullptr);
  xTaskCreate(Task_NetworkController, "NetworkController", TASK_STACK_NETWORK,
              nullptr, TASK_PRIORITY_NETWORK, nullptr);

  DLOG("[BOOT] setup complete");
}

void loop() { vTaskDelay(pdMS_TO_TICKS(500)); }