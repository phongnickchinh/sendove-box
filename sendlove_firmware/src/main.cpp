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

// SENDLOVE BOX firmware. Event-driven FreeRTOS tasks:
//   Task_MediaPlayer       — state machine, playback, alarms
//   Task_UIController      — touch sensor, sleep
//   Task_NetworkController — WebServer / captive portal
// TOUCH_OTA_TOGGLE: a 6s hold (TOUCH_OTA_HOLD_MS), the last step of the OTA touch sequence.
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

// A play command waiting for sync to finish (1-slot queue). Set/cleared by
// Task_MediaPlayer; Task_UIController reads it to stay awake.
// Why wait: playback (~74KB) plus a TLS session (35-45KB) don't fit together and
// caused reboots (MEMORY.md §8, §21). Do NOT fold this into isPlaybackActive(): the
// sync would abort without setting _hasPendingMessages.
static std::atomic<bool> s_pendingPlay{false};

// An alarm is ringing WITH MUSIC (continuous card reads). Counts as
// isPlaybackActive() so sync and downloads stop: a card busy writing makes the
// music crackle (mandatory case, MEMORY.md §28). The beep doesn't read the card.
static std::atomic<bool> s_alarmMusicOn{false};

// ---- Rollback after OTA ----
// Returning true stops Arduino from marking the new image VALID at boot; the
// firmware confirms it itself after OTA_VERIFY_DELAY_MS (Task_UIController). A reset
// before that leaves it PENDING_VERIFY and the bootloader falls back to the old image.
// MUST be extern "C": the weak original has C linkage.
extern "C" bool verifyRollbackLater() { return true; }

static volatile bool s_otaPendingVerify = false;

// OTA web server command: +1 start, -1 stop, 0 nothing. Written by Task_MediaPlayer,
// executed by Task_NetworkController: WebServer calls MUST stay on the task running
// handleClient() (stopWebServer() deletes the server -> use-after-free otherwise).
static volatile int8_t s_otaServerCmd = 0;
static esp_timer_handle_t s_otaGuardTimer = nullptr;

// Safety net for a new image that HANGS: rollback needs a reset, and no watchdog
// resets a hung task in this configuration. esp_timer still fires when app tasks are dead.
static void otaGuardFire(void*) { esp_restart(); }

// No Serial: logging goes to the ScreenLogger overlay (no blocking without a USB host).

// STATE_ALARM: ringing; blocks sleep, accepts touches even while downloading.
// STATE_OTA: flashing mode (3s, 3s, 6s hold sequence): no sleep, sync, playback or
// alarms (product decision; the OTA screen reminds the user to exit).
enum class AppState { STATE_STANDBY, STATE_VIDEO, STATE_ALARM, STATE_OTA };

// Shared by 3 tasks, hence atomic. NOT lock-free on the ESP32-C3 (ESP-IDF emulates
// it by disabling interrupts): never touch it from an ISR.
std::atomic<AppState> currentAppState{AppState::STATE_STANDBY};

// The standby theme is not compiled in: it lives in the `theme` flash partition
// (ThemeStore, see MEMORY.md §28). Without one, LayoutEngine draws a black fallback.

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

  // OTA mode screen. ASCII only (the font has glyphs 32-126).
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
      // The only reminder that alarms are off in this mode.
      tft->setTextColor(TFT_RED);
      tft->drawString("BAO THUC DANG TAT", 120, 190);
      tft->setTextColor(TFT_WHITE);
      tft->drawString("Thoat: giu 3s, 3s, roi 6s", 120, 210);
      appCtx.display.releaseSPI();
  };

  auto enterOtaMode = [&drawOtaScreen, &drawToast]() {
      // OTA is LAN-only: no Wi-Fi, no server.
      if (!appCtx.network.isConnected()) {
          DLOG("[OTA] khong co Wi-Fi -> khong vao che do");
          appCtx.display.turnOn();
          drawToast("OTA: can Wi-Fi");
          vTaskDelay(pdMS_TO_TICKS(1500));
          forceStandbyRedraw = true;
          return;
      }
      appCtx.player.stop();
      s_otaServerCmd = 1;  // picked up by Task_NetworkController within 50ms
      currentAppState = AppState::STATE_OTA;
      appCtx.display.turnOn();
      drawOtaScreen();
      DLOG("[OTA] vao che do: http://%s.local/ %s", OTA_HOSTNAME, WiFi.localIP().toString().c_str());
  };

  auto exitOtaMode = []() {
      // No exit mid-flash (second latch; Task_UIController already swallows touches).
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
  // OTA touch sequence: hold 3s, 3s, then 6s. Not one long hold: the TTP223
  // recalibrates after 7-8s of touch. Only LONGs received in STANDBY or OTA count.
  uint8_t  otaSeqStep = 0;      // 0 idle · 1 first hold done · 2 second hold done (prompt showing)
  uint32_t otaSeqStep2Ms = 0;   // when the second hold completed
  uint32_t otaSeqDeadline = 0;

  // Bottom strip y 200-239 (covers the progress bar area).
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
  bool     pendingSawIdle = false;     // sync seen idle since it was last busy
  uint32_t pendingIdleSinceMs = 0;     // see PENDING_PLAY_SETTLE_MS

  // Bottom strip; redrawn by the STANDBY loop after each render, like drawOtaPrompt.
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

  // The deadline counts from the FIRST touch.
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

  // Play the next unread message from STANDBY. fromPending = running the queued command.
  auto startNextUnread = [&currentId, &playStartTime, &drawToast, &armPendingPlay](bool fromPending) {
      char unreadId[32] = "";
      // Ask the STORAGE, not the RAM counter: after a reset while full the counter
      // stays 0 and the box would dead-end (MEMORY.md §23).
      if (appCtx.storage && appCtx.storage->getNextUnreadIdentifier(unreadId, sizeof(unreadId))) {
         // VIDEO before playItem(): blocks a new sync while playback buffers are allocated.
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
         // The last download stopped for lack of slots; reading freed space, so fetch more.
         DLOG("[PLAY] no local unread, downloading...");
         appCtx.player.stop();
         drawToast("Downloading...");
         uint8_t batPercent = appCtx.powerManager.getBatteryPercentage();
         bool isCharging = appCtx.powerManager.isCharging();
         appCtx.network.triggerFirebaseSync(batPercent, isCharging, appCtx.storage);
         // Auto-play once downloaded — but wait only if a sync really started.
         if (appCtx.network.isSyncing()) {
             // Keep the ORIGINAL deadline: extending it on each failed sync would retry forever.
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
    // Alarms: polled every 500ms in every state, except while ringing or in OTA.
    if (currentAppState != AppState::STATE_ALARM && currentAppState != AppState::STATE_OTA &&
        !appCtx.otaHandler.isUpdating() &&
        millis() - lastAlarmPollMs >= 500) {
      lastAlarmPollMs = millis();
      if (AlarmClock::instance().pollDue(time(nullptr), alarmTime, sizeof(alarmTime), &ringItem)) {
        if (currentAppState == AppState::STATE_VIDEO) appCtx.player.stop();
        currentAppState = AppState::STATE_ALARM;
        // Music if it is on the card; every other case takes ONE branch: the beep.
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
        // After a timer wake the screen is still off.
        appCtx.display.turnOn();
        // At least ALARM_MIN_BRIGHTNESS: the user's setting may be as low as 5%.
        appCtx.display.setBacklight(Settings::alarmBacklight());
        appCtx.layoutEngine.renderAlarmScreen(&appCtx.display, alarmTime, ALARM_HINT);
        alarmStartMs = millis();
        lastBeepMs = 0;
        lastUserActivity = millis();
      }
    }

    // OTA sequence timeout — but a final hold already in progress may complete.
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
        // Free the 24KB I2S DMA that beep() keeps between beeps.
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
        // Step 2: the LONG at second 3 of the final hold; keep waiting for 6s.
        continue;
      }
      if (event == SystemEvent::TOUCH_OTA_TOGGLE) {
        // The 6s hold must have started AFTER step 2, or stretching the second hold
        // would shorten the sequence to two steps.
        bool freshHold = (int32_t)((millis() - TOUCH_OTA_HOLD_MS) - otaSeqStep2Ms) > 0;
        if (otaSeqStep == 2 && freshHold && otaSeqState) {
          otaSeqStep = 0;  // enter/exit redraw the screen themselves
          if (currentAppState == AppState::STATE_OTA) exitOtaMode();
          else enterOtaMode();
        }
        continue;
      }
      if (event == SystemEvent::TOUCH_SHORT && otaSeqStep == 2) {
        resetOtaSeq();  // prompt showing: a short touch cancels
        continue;
      }
      if (event == SystemEvent::TOUCH_SHORT) {
        otaSeqStep = 0;  // cancel silently; the touch still works normally
      }
      // OTA mode only accepts the exit sequence above; every other event is ignored.
      if (currentAppState == AppState::STATE_OTA) continue;

      if (event == SystemEvent::TOUCH_SHORT) {
        if (currentAppState == AppState::STATE_STANDBY) {
           if (appCtx.network.isDownloadingMedia()) {
               // Product decision: a touch WHILE DOWNLOADING is ignored, not queued.
               appCtx.player.stop();
               drawToast("Downloading...");
           } else if (appCtx.network.isSyncing()) {
               // Sync may hold a TLS session: queue instead of playing over it.
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
           // Storage, not the RAM counter, is the source of truth.
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
               // Auto-play once downloaded (same condition as startNextUnread).
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

    // Queued play command, polled every loop (syncWakeup() has ~12 exits, so no
    // callback). The Wi-Fi setup AP does NOT count as busy (product decision).
    // The PENDING_PLAY_SETTLE_MS pause is COUNTED in the loop: never block this task
    // (and `sleep(2000)` is POSIX sleep in SECONDS).
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

    // Brightness changed from the web: apply now if the screen is on (the alarm
    // screen keeps its own floor).
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
      // Install a downloaded theme here (this task owns LayoutEngine), after sync ends:
      // the flash write stalls the CPU ~2s and must not hit a TLS session.
      if (ThemeStore::installPending() && !appCtx.network.isSyncing() && !s_pendingPlay) {
        char dir[64], id[24];
        uint32_t rev = 0;
        if (ThemeStore::takeInstallRequest(dir, sizeof(dir), id, sizeof(id), &rev)) {
          drawToast("Dang ap dung giao dien...");
          ThemeStore::installFromSd(dir, id, rev);
          appCtx.layoutEngine.loadTheme();  // falls back by itself on failure
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
        // Redraw the prompt after a render, but not over the hold's progress bar.
        if (otaSeqStep == 2 && appCtx.ui.getTouchHoldMs() == 0) drawOtaPrompt();
      }
      // Flush the log on standby only (no bus contention with playback).
      static uint32_t lastLogFlush = 0;
      if (now - lastLogFlush >= 30000) {
        lastLogFlush = now;
        SdLog::flush();
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    } else if (currentAppState == AppState::STATE_ALARM) {
      // Keep the box awake while ringing.
      lastUserActivity = millis();
      if (millis() - alarmStartMs >= ALARM_RING_MAX_MS) {
        DLOG("[ALM] het 1 phut -> tu tat");
        AlarmClock::instance().dismiss();
        appCtx.player.stop();  // frees the I2S DMA
        s_alarmMusicOn = false;
        currentAppState = AppState::STATE_STANDBY;
        appCtx.display.setBacklight(Settings::currentBacklight());
        forceStandbyRedraw = true;
      } else if (s_alarmMusicOn) {
        // Ramp from ALARM_RAMP_START_PCT to 100% of the chosen level over ALARM_RAMP_MS.
        uint8_t vol = ringItem.volume;
        uint32_t t = millis() - alarmStartMs;
        if (ringItem.ramp && t < ALARM_RAMP_MS) {
          uint32_t start = (uint32_t)ringItem.volume * ALARM_RAMP_START_PCT / 100;
          vol = (uint8_t)(start + (ringItem.volume - start) * t / ALARM_RAMP_MS);
        }
        appCtx.player.tickAlarmMusic(vol);
        vTaskDelay(pdMS_TO_TICKS(20));  // DMA holds ~96ms @16kHz
      } else if (lastBeepMs == 0 || millis() - lastBeepMs >= ALARM_BEEP_PERIOD_MS) {
        lastBeepMs = millis();
        // The fallback beep is ALWAYS at 100: it must wake someone when the music failed.
        appCtx.player.alarmBeep(100);  // blocks ~0.6s; touches still queue
      } else {
        vTaskDelay(pdMS_TO_TICKS(20));
      }
    } else if (currentAppState == AppState::STATE_OTA) {
      // Redraw every second for the "Dang nap N%" line, but not while a finger is held.
      static uint32_t lastOtaRender = 0;
      if (millis() - lastOtaRender >= 1000 && appCtx.ui.getTouchHoldMs() == 0) {
        lastOtaRender = millis();
        drawOtaScreen();
        if (otaSeqStep == 2) drawOtaPrompt();  // drawOtaScreen() cleared it
      }
      vTaskDelay(pdMS_TO_TICKS(50));
    }

    // Progress bar for the final 6s hold only: step 2, and a hold begun AFTER the step-2 mark.
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
      // Finger released: redraw the prompt (erases the bar) or the current screen.
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
      // While flashing, block EVERY touch, including the exit sequence.
      if ((appCtx.network.isDownloadingMedia() || appCtx.otaHandler.isUpdating()) &&
          currentAppState != AppState::STATE_ALARM) {
          // Ignore touches while downloading, except to dismiss a ringing alarm.
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

    // The sync period counts from when a sync ENDS, not when it starts.
    if (appCtx.network.isSyncing()) {
        lastIntervalSyncMs = now;
    }

    // The new OTA image survived OTA_VERIFY_DELAY_MS: confirm it, cancel the safety net.
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

    // Intentionally NO `!isStorageFull` here: full slots only block the download step
    // inside syncWakeup(), not heartbeat / flags / alarm sync.
    if (now - lastIntervalSyncMs >= SYNC_INTERVAL_MS && !appCtx.network.isSyncing() && currentAppState == AppState::STATE_STANDBY) {
        lastIntervalSyncMs = now;
        uint8_t batPercent = appCtx.powerManager.getBatteryPercentage();
        bool isCharging = appCtx.powerManager.isCharging();
        appCtx.network.triggerFirebaseSync(batPercent, isCharging, appCtx.storage);
    }

    // secondsToNext <= 2: an alarm is about to ring; sleeping now would miss it.
    if (currentAppState != AppState::STATE_VIDEO &&
        currentAppState != AppState::STATE_ALARM &&
        !appCtx.otaHandler.isUpdating() && !appCtx.network.isProvisioningActive() &&
        !appCtx.network.isSyncing() && currentAppState != AppState::STATE_OTA &&
        // No sleep during OTA probation: on waking, the safety-net timer could fire
        // before this task confirms and reset a good image.
        !s_otaPendingVerify &&
        // A play command is queued: don't sleep before Task_MediaPlayer runs it.
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
      SdLog::flush();  // power may be lost while asleep

      // Stop playback before sleeping
      appCtx.player.stop();
      currentAppState = AppState::STATE_STANDBY;
      // Don't set forceStandbyRedraw here: it would race with Task_MediaPlayer
      
      appCtx.powerManager.enterLightSleep(sleepTimeUs, &appCtx.display);

      // Just woke: force the next ensureConnected() to re-associate (see there).
      appCtx.network.notifyWakeFromSleep();

      // Releases the backlight GPIO hold (it blinks nothing despite the name); done
      // before other tasks resume.
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

        // Blink the blue LED (GPIO 8, shared with NAND CS): the only timer-wake
        // indicator (product decision). spiMutex MUST be held for the whole blink:
        // CS low while another task clocks the bus makes the NAND latch a bogus opcode.
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
          // Log it: a silent "no blink" would look like a broken LED.
          DLOG("[WAKE] blink skip: spi busy");
        }
      }

      // Let the UI and SPI bus settle before the background sync
      vTaskDelay(pdMS_TO_TICKS(200));

      // Background sync after every wake, even with full slots (the "full" gate
      // inside syncWakeup() blocks only the download step).
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
    // OTA server start/stop runs on this task (see s_otaServerCmd).
    int8_t cmd = s_otaServerCmd;
    if (cmd != 0) {
      s_otaServerCmd = 0;
      if (cmd > 0) {
        appCtx.network.startWebServer(OTA_HOSTNAME);
        // A fresh WebServer per entry, so routes are registered once per instance.
        if (appCtx.network.getWebServer() != nullptr) {
          appCtx.otaHandler.registerRoutes(*appCtx.network.getWebServer());
        }
      } else {
        appCtx.network.stopWebServer();
      }
    }
    appCtx.network.update();
    // Same task as handleClient(): never parallel to an Update.write(), no lock needed.
    appCtx.otaHandler.tickWatchdog();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void setup() {
  // Arm the OTA safety net FIRST, before anything that can hang.
  {
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &st) == ESP_OK &&
        st == ESP_OTA_IMG_PENDING_VERIFY) {
      s_otaPendingVerify = true;
      const esp_timer_create_args_t args = {
          .callback = &otaGuardFire, .arg = nullptr, .dispatch_method = ESP_TIMER_TASK,
          .name = "ota_guard", .skip_unhandled_events = false};
      // +30s past the confirmation mark, so Task_UIController confirms first.
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
  // Load settings BEFORE the backlight first turns on.
  Settings::begin();
  appCtx.display.setBacklight(Settings::currentBacklight());
  appCtx.display.showMessage("Booting...");

  // ScreenLogger needs the display to be ready
  ScreenLogger::init(&appCtx.display);
  DLOG("[BOOT] cpu=%uMHz heap=%u", ESP.getCpuFreqMHz(), ESP.getFreeHeap());
  DLOG("[BOOT] wakeup=%d", (int)esp_sleep_get_wakeup_cause());
  // reset=1 POWERON, 3 SW, 4 INT_WDT, 5 TASK_WDT, 6 WDT, 9 BROWNOUT, 12 PANIC.
  // Repeating regularly in the log = a reset loop.
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

  // Log where the credentials came from (NVS or defaults).
  DLOG("[BOOT] WiFi: %s (%s)", wifiSsid, credsFromNvs ? "NVS" : "default");

  if (appCtx.network.connectWiFi(wifiSsid, wifiPass) != WiFiConnectResult::CONNECTED && credsFromNvs) {
    // NVS creds may be stale: try the config.h defaults before falling back to AP mode.
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
  MusicStore::load();  // alarm-music table, read at ring time without touching the card
  // If the theme partition is empty but the card has the active package, restore it
  // now (no task is drawing yet).
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
    // The OTA web server is NOT started here (it would hold RAM for the whole
    // uptime): it is created on entering STATE_OTA.

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