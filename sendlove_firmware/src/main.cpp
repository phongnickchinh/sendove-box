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
#include "config.h"
#include <Arduino.h>
#include <SPI.h>
#include <atomic>
#include <esp_ota_ops.h>
#include <esp_timer.h>

// ============================================================================
// SENDLOVE BOX — Main Firmware (Phase 3A: Storage Abstraction Layer)
// ============================================================================
// Kiến trúc FreeRTOS Event-Driven:
//   - Task_MediaPlayer: Decode + render video/ảnh từ IStorageProvider (NAND / SD)
//   - Task_UIController: Đọc touch sensor + gửi event chuyển slot/item
//   - Task_NetworkController: Phục vụ WebServer / Captive Portal
// ============================================================================
// TOUCH_OTA_TOGGLE: cú giữ TOUCH_OTA_HOLD_MS (6s) — bước cuối của chuỗi chạm OTA.
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

// ============================================================================
// Rollback sau OTA
// ============================================================================
// Bootloader đã bật sẵn rollback (CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=1), nhưng
// initArduino() mặc định gọi verifyOta() (weak, trả true) rồi đánh dấu bản mới HỢP LỆ
// ngay lúc boot — lớp bảo vệ bị vô hiệu. Trả true ở đây = Arduino không tự đánh dấu,
// firmware tự lo: sống đủ OTA_VERIFY_DELAY_MS mới xác nhận (Task_UIController).
// Reset trước mốc đó thì bootloader thấy partition vẫn PENDING_VERIFY và tự quay về
// bản cũ — đó là toàn bộ nhánh thất bại, không cần code gì thêm.
//
// PHẢI extern "C": bản weak gốc nằm trong esp32-hal-misc.c (C linkage).
//
// Nạp qua cáp KHÔNG đi qua đường này: boot_app0.bin ghi otadata với
// ota_state = 0xFFFFFFFF (UNDEFINED, đã đọc byte thật 2026-09-21), không phải NEW,
// nên bản nạp cáp không bao giờ ở trạng thái PENDING_VERIFY.
extern "C" bool verifyRollbackLater() { return true; }

static volatile bool s_otaPendingVerify = false;

// Lệnh bật/tắt web server OTA: +1 bật, -1 tắt, 0 không có gì. Task_MediaPlayer ghi,
// Task_NetworkController thực thi. Mọi thao tác với WebServer PHẢI nằm ở task gọi
// handleClient(): stopWebServer() làm `delete _webServer`, gọi từ task khác trong lúc
// handleClient() đang chạy là use-after-free.
static volatile int8_t s_otaServerCmd = 0;
static esp_timer_handle_t s_otaGuardTimer = nullptr;

// Lưới an toàn cho bản mới bị TREO (không crash). Rollback chỉ xảy ra khi chip RESET,
// mà ở cấu hình này treo KHÔNG gây reset: task WDT không canh IDLE task
// (CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0 không bật) và loopTask không đăng ký
// (loopTaskWDTEnabled = false), nên một vòng while(1) chỉ đứng im mãi mãi.
// esp_timer chạy ở task ưu tiên 22, vẫn bắn khi các task ứng dụng đã chết.
static void otaGuardFire(void*) { esp_restart(); }

// Serial Monitor đã được thay thế hoàn toàn bằng ScreenLogger on-screen overlay.
// Không dng Serial.begin() để tránh block chip khi không có USB CDC.

// STATE_ALARM: báo thức đang kêu. Chặn vòng ngủ, nhận chạm kể cả lúc đang tải tin.
// STATE_OTA: chế độ nạp, vào/ra bằng chuỗi chạm giữ 3s, 3s rồi 6s. Hộp CHỈ chờ và nạp: không
// ngủ, không sync, không phát tin, KHÔNG kêu báo thức (user chốt 2026-09-21 — quên
// thoát là mất báo thức, màn OTA có dòng nhắc).
enum class AppState { STATE_STANDBY, STATE_VIDEO, STATE_ALARM, STATE_OTA };

// Đọc/ghi từ 3 task (MediaPlayer, UIController, vòng lặp chính) nên phải atomic.
// 18 chỗ dùng đều là so sánh/gán trực tiếp (đã grep), không chỗ nào bind qua `auto`,
// nên operator T() / operator= ngầm phủ hết, không cần sửa chỗ nào khác.
//
// KHÔNG lock-free: ESP32-C3 là RV32IMC, thiếu extension 'A' cho atomic sub-word.
// Link được là nhờ ESP-IDF cấp sẵn bản emulation (đã verify bằng nm:
// __atomic_load_1/store_1/exchange_1 đều là 'T' trong sdk/esp32c3/lib/libnewlib.a).
// Emulation chạy bằng cách tắt ngắt — rẻ, nhưng đừng gọi từ ISR. Hiện không chỗ nào
// gọi từ ISR: main.cpp không có IRAM_ATTR nào, chỗ duy nhất trông giống callback
// (setPlaybackActiveCallback) chạy trong task context.
std::atomic<AppState> currentAppState{AppState::STATE_STANDBY};

const char *defaultLayoutJson = R"({
  "theme_name": "Default Card Theme",
  "background": "bg_defaut",
  "widgets": [
    { "type": "clock_date", "format": "WEEKDAY, DD.MM", "x": 9, "y": 9, "w": 140, "h": 16, "align": "left", "font": "ChakraPetch_16", "color": "#B83D3D" },
    { "type": "clock_time", "format": "HH:MM", "x": 50, "y": 30, "w": 160, "h": 45, "align": "center", "font": "ChakraPetch_48", "color": "#000000" },
    { "type": "battery_icon", "x": 154, "y": 10, "w": 75, "h": 16 }
  ]
})";

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

  // Màn chế độ OTA. Chỉ ASCII: font hiện chỉ có glyph 32-126 (xem asciiFold ở MediaPlayer).
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
      // Biện pháp giảm thiểu DUY NHẤT cho đánh đổi đã chốt: quên thoát = mất báo thức.
      tft->setTextColor(TFT_RED);
      tft->drawString("BAO THUC DANG TAT", 120, 190);
      tft->setTextColor(TFT_WHITE);
      tft->drawString("Thoat: giu 3s, 3s, roi 6s", 120, 210);
      appCtx.display.releaseSPI();
  };

  auto enterOtaMode = [&drawOtaScreen, &drawToast]() {
      // OTA qua LAN là đường DUY NHẤT: không có Wi-Fi thì dựng server cũng vô ích.
      if (!appCtx.network.isConnected()) {
          DLOG("[OTA] khong co Wi-Fi -> khong vao che do");
          appCtx.display.turnOn();
          drawToast("OTA: can Wi-Fi");
          vTaskDelay(pdMS_TO_TICKS(1500));
          forceStandbyRedraw = true;
          return;
      }
      appCtx.player.stop();
      s_otaServerCmd = 1;  // Task_NetworkController dựng server trong ≤ 50ms
      currentAppState = AppState::STATE_OTA;
      appCtx.display.turnOn();
      drawOtaScreen();
      DLOG("[OTA] vao che do: http://%s.local/ %s", OTA_HOSTNAME, WiFi.localIP().toString().c_str());
  };

  auto exitOtaMode = []() {
      // Đang nạp dở thì KHÔNG cho thoát. Task_UIController vốn đã chặn mọi cú chạm
      // khi isUpdating(), đây chỉ là chốt thứ hai.
      if (appCtx.otaHandler.isUpdating()) return;
      s_otaServerCmd = -1;
      currentAppState = AppState::STATE_STANDBY;
      forceStandbyRedraw = true;
      lastUserActivity = millis();
      DLOG("[OTA] thoat che do");
  };

  static constexpr const char* ALARM_HINT = "Cham: bao lai 5p - Giu: tat";
  char alarmTime[6] = "";
  uint32_t alarmStartMs = 0;
  uint32_t lastBeepMs = 0;
  uint32_t lastAlarmPollMs = 0;
  // Chuỗi chạm vào/ra chế độ OTA: giữ 3s → nhả → giữ 3s → nhả → giữ 6s. Không dùng
  // một cú giữ dài vì TTP223 tự hiệu chuẩn sau 7-8s chạm liên tục (config.h).
  // Chỉ đếm LONG nhận được lúc đang STANDBY hoặc OTA, nên LONG thoát video (nhận lúc
  // VIDEO) và LONG tắt báo thức (nhánh ALARM nuốt trước) không bao giờ là bước 1.
  uint8_t  otaSeqStep = 0;      // 0 rảnh · 1 xong cú giữ thứ nhất · 2 xong cú thứ hai (đang hiện nhắc)
  uint32_t otaSeqStep2Ms = 0;   // mốc xong cú giữ thứ hai
  uint32_t otaSeqDeadline = 0;

  // Dải đáy y 200-239 (gồm cả chỗ thanh tiến trình, để vẽ lại là xoá luôn thanh cũ).
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

  for (;;) {
    // Báo thức: hỏi mỗi 500ms ở mọi trạng thái (kêu đè lên cả lúc đang xem tin),
    // trừ khi đang kêu sẵn, đang nạp OTA, hoặc đang ở CHẾ ĐỘ OTA (user chốt tắt hẳn
    // báo thức trong chế độ này, 2026-09-21).
    if (currentAppState != AppState::STATE_ALARM && currentAppState != AppState::STATE_OTA &&
        !appCtx.otaHandler.isUpdating() &&
        millis() - lastAlarmPollMs >= 500) {
      lastAlarmPollMs = millis();
      if (AlarmClock::instance().pollDue(time(nullptr), alarmTime, sizeof(alarmTime))) {
        if (currentAppState == AppState::STATE_VIDEO) appCtx.player.stop();
        currentAppState = AppState::STATE_ALARM;
        // Cú chạm xếp hàng từ trước không được tắt ngay báo thức vừa kêu.
        xQueueReset(eventQueue);
        // Thức dậy bằng timer thì màn hình còn tắt (turnOn chỉ gọi khi wake bằng chạm).
        appCtx.display.turnOn();
        appCtx.layoutEngine.renderAlarmScreen(&appCtx.display, alarmTime, ALARM_HINT);
        alarmStartMs = millis();
        lastBeepMs = 0;
        lastUserActivity = millis();
      }
    }

    // Hết hạn chuỗi chạm OTA. Đang giữ tay thì KHÔNG huỷ dưới ngón tay người dùng:
    // bắt đầu cú giữ cuối trước hạn là được hoàn thành.
    if (otaSeqStep != 0 && (int32_t)(millis() - otaSeqDeadline) > 0 &&
        appCtx.ui.getTouchHoldMs() == 0) {
      resetOtaSeq();
    }

    SystemEvent event = SystemEvent::NONE;
    while (xQueueReceive(eventQueue, &event, 0) == pdTRUE) {
      // event loop — không log tại đây để tránh spam màn hình

      if (currentAppState == AppState::STATE_ALARM) {
        // Chạm ngắn = báo lại sau 5 phút, chạm giữ 3s = tắt hẳn.
        if (event == SystemEvent::TOUCH_SHORT) {
          AlarmClock::instance().snooze(time(nullptr));
          appCtx.layoutEngine.renderAlarmScreen(&appCtx.display, alarmTime, "Bao lai sau 5 phut");
          vTaskDelay(pdMS_TO_TICKS(1500));
        } else if (event == SystemEvent::TOUCH_LONG) {
          AlarmClock::instance().dismiss();
        } else {
          continue;
        }
        // Trả 24KB DMA của I2S: beep() cố ý không tự gỡ driver giữa các hồi bíp.
        appCtx.player.stop();
        currentAppState = AppState::STATE_STANDBY;
        forceStandbyRedraw = true;
        lastUserActivity = millis();
        continue;
      }

      // ---- Chuỗi chạm OTA (xem otaSeqStep) ----
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
        // Bước 2: đây là LONG ở giây thứ 3 của cú giữ cuối — cứ để nó giữ tiếp tới 6s.
        // Ở STANDBY, LONG vốn là no-op; ở OTA thì mọi event đều bị bỏ qua.
        continue;
      }
      if (event == SystemEvent::TOUCH_OTA_TOGGLE) {
        // Cú giữ 6s phải là lần giữ MỚI, bắt đầu SAU khi xong bước 2. Không kiểm thì ai
        // giữ tiếp cú thứ hai tới 6s sẽ rút chuỗi còn hai bước. Cú giữ bắn event này
        // bắt đầu lúc (now - TOUCH_OTA_HOLD_MS) — trễ hàng đợi chỉ vài ms, còn cú thứ
        // hai thì bắt đầu trước mốc bước 2 tới 3s, nên biên phân định rất rộng.
        bool freshHold = (int32_t)((millis() - TOUCH_OTA_HOLD_MS) - otaSeqStep2Ms) > 0;
        if (otaSeqStep == 2 && freshHold && otaSeqState) {
          otaSeqStep = 0;  // enter/exit tự vẽ lại màn, không cần resetOtaSeq()
          if (currentAppState == AppState::STATE_OTA) exitOtaMode();
          else enterOtaMode();
        }
        continue;
      }
      if (event == SystemEvent::TOUCH_SHORT && otaSeqStep == 2) {
        resetOtaSeq();  // đang hiện nhắc: chạm ngắn là HUỶ, không phát tin
        continue;
      }
      if (event == SystemEvent::TOUCH_SHORT) {
        otaSeqStep = 0;  // bước 1 chưa hiện gì: huỷ âm thầm, chạm vẫn làm việc bình thường
      }
      // Chế độ OTA chỉ nhận chuỗi thoát ở trên; mọi event khác bị bỏ qua.
      if (currentAppState == AppState::STATE_OTA) continue;

      if (event == SystemEvent::TOUCH_SHORT) {
        if (currentAppState == AppState::STATE_STANDBY) {
           if (appCtx.network.isDownloadingMedia()) {
               appCtx.player.stop();
               drawToast("Downloading...");
           } else {
               char unreadId[32] = "";
               // Hỏi THẺ, không hỏi biến đếm RAM. getNumOfNewMsg() từng là cổng ở
               // đây, nhưng nó chỉ được gán bên trong checkAndDownloadNewMessages()
               // — hàm nằm SAU cổng isFull() — nên sau một lần reset trong lúc đang
               // đầy slot, nó kẹt ở 0 trong khi cờ unread trên thẻ vẫn còn: hộp vừa
               // báo "No new messages" lúc chạm, vừa báo "het slot" lúc sync, và
               // slot thì chỉ được trả lại bằng cách đọc -> không có đường ra.
               if (appCtx.storage && appCtx.storage->getNextUnreadIdentifier(unreadId, sizeof(unreadId))) {
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
                  // Hết tin chưa đọc trên thẻ nhưng vòng tải trước đã phải bỏ dở
                  // vì hết slot -> vừa đọc xong là có chỗ, kéo tiếp ngay.
                  DLOG("[PLAY] no local unread, downloading...");
                  appCtx.player.stop();
                  drawToast("Downloading...");
                  uint8_t batPercent = appCtx.powerManager.getBatteryPercentage();
                  bool isCharging = appCtx.powerManager.isCharging();
                  appCtx.network.triggerFirebaseSync(batPercent, isCharging, appCtx.storage);
               } else {
                  appCtx.display.turnOn();
                  drawToast("No new messages");
                  vTaskDelay(1000);
                  forceStandbyRedraw = true;
               }
           }
        } else if (currentAppState == AppState::STATE_VIDEO) {
           if (event == SystemEvent::TOUCH_SHORT) {
               appCtx.storage->markAsRead(currentId);
               appCtx.network.decrementNewMsgCount();
           }

           char unreadId[32] = "";
           // Cùng lý do như nhánh STANDBY ở trên: nguồn sự thật là cờ unread trên
           // thẻ, không phải biến đếm RAM.
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
      if (now - lastClockRender >= 1000 || forceStandbyRedraw) {
        bool fullRedraw = (lastClockRender == 0) || forceStandbyRedraw;
        appCtx.layoutEngine.renderStandbyScreen(&appCtx.display, &appCtx.network, fullRedraw);
        lastClockRender = now;
        forceStandbyRedraw = false;
        // Render vừa rồi có thể đè mất lời nhắc. Không vẽ lúc đang giữ tay: sẽ xoá
        // thanh tiến trình của cú giữ cuối.
        if (otaSeqStep == 2 && appCtx.ui.getTouchHoldMs() == 0) drawOtaPrompt();
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    } else if (currentAppState == AppState::STATE_ALARM) {
      // Giữ mốc hoạt động để vòng ngủ ở Task_UIController không chen vào lúc đang kêu.
      lastUserActivity = millis();
      if (millis() - alarmStartMs >= ALARM_RING_MAX_MS) {
        DLOG("[ALM] het 1 phut -> tu tat");
        AlarmClock::instance().dismiss();
        appCtx.player.stop();  // trả 24KB DMA của I2S
        currentAppState = AppState::STATE_STANDBY;
        forceStandbyRedraw = true;
      } else if (lastBeepMs == 0 || millis() - lastBeepMs >= ALARM_BEEP_PERIOD_MS) {
        lastBeepMs = millis();
        appCtx.player.alarmBeep();  // block ~0.6s, chạm trong lúc đó vẫn xếp hàng
      } else {
        vTaskDelay(pdMS_TO_TICKS(20));
      }
    } else if (currentAppState == AppState::STATE_OTA) {
      // Vẽ lại mỗi giây để dòng tiến độ "Dang nap N%" chạy. Không vẽ khi đang giữ
      // tay (thanh tiến trình ở dưới cần nguyên dải đáy màn hình).
      static uint32_t lastOtaRender = 0;
      if (millis() - lastOtaRender >= 1000 && appCtx.ui.getTouchHoldMs() == 0) {
        lastOtaRender = millis();
        drawOtaScreen();
        if (otaSeqStep == 2) drawOtaPrompt();  // drawOtaScreen() vừa xoá cả màn
      }
      vTaskDelay(pdMS_TO_TICKS(50));
    }

    // Thanh tiến trình CHỈ cho cú giữ 6s cuối: đang ở bước 2 và cú giữ bắt đầu SAU
    // mốc bước 2. Thiếu điều kiện sau, phần còn lại của cú giữ thứ hai (đã qua 3s)
    // sẽ hiện như thể cú cuối đã được một nửa.
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
      // Nhả tay: nhả sớm (còn ở bước 2) thì vẽ lại lời nhắc, nó xoá luôn thanh; đã
      // vào/ra chế độ thì vẽ lại màn hiện tại.
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
      // Đang nạp OTA thì chặn MỌI cú chạm, cùng cơ chế với lúc đang tải tin — kể cả
      // chuỗi chạm để thoát: thoát giữa chừng là tắt web server khi file còn đang tới.
      if ((appCtx.network.isDownloadingMedia() || appCtx.otaHandler.isUpdating()) &&
          currentAppState != AppState::STATE_ALARM) {
          // Bỏ qua touch khi đang download — không log để tránh spam.
          // Trừ lúc báo thức đang kêu: không được bắt người dùng chờ tải xong mới tắt được.
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

    // Trong luc sync chay ngam, day moc thoi gian theo -> chu ky duoc tinh tu luc
    // sync KET THUC, thay vi tu luc bat dau (tai xong 30s roi sync lai ngay).
    if (appCtx.network.isSyncing()) {
        lastIntervalSyncMs = now;
    }

    // Bản mới sau OTA đã sống đủ OTA_VERIFY_DELAY_MS -> xác nhận, huỷ lưới an toàn.
    // Từ đây trở đi bootloader không còn quay về bản cũ nữa.
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

    // KHÔNG còn điều kiện `!isStorageFull` ở đây: đầy slot chỉ có nghĩa là khỏi
    // tải tin, không có nghĩa là ngừng heartbeat / đọc cờ / đồng bộ báo thức.
    // Cổng đó đã chuyển xuống đúng bước tải tin trong syncWakeup(). Đánh đổi đã
    // biết: box đầy slot vẫn sync theo chu kỳ nên tốn pin hơn trước.
    if (now - lastIntervalSyncMs >= SYNC_INTERVAL_MS && !appCtx.network.isSyncing() && currentAppState == AppState::STATE_STANDBY) {
        lastIntervalSyncMs = now;
        uint8_t batPercent = appCtx.powerManager.getBatteryPercentage();
        bool isCharging = appCtx.powerManager.isCharging();
        appCtx.network.triggerFirebaseSync(batPercent, isCharging, appCtx.storage);
    }

    // secondsToNext <= 2: sắp (hoặc đang) tới phút báo thức mà pollDue chưa kêu.
    // Ngủ lúc này thì timer tối thiểu vẫn làm lỡ mất cả phút -> thức chờ tiếp.
    if (currentAppState != AppState::STATE_VIDEO &&
        currentAppState != AppState::STATE_ALARM &&
        !appCtx.otaHandler.isUpdating() && !appCtx.network.isProvisioningActive() &&
        !appCtx.network.isSyncing() && currentAppState != AppState::STATE_OTA &&
        // Không ngủ trong thời gian thử thách: vừa thức, lưới an toàn esp_timer (ưu
        // tiên 22) có thể bắn TRƯỚC khi task này kịp xác nhận -> reset oan một bản tốt.
        // INACTIVITY_SLEEP_TIMEOUT_MS cũng là 60s, tức rơi đúng cửa sổ đó.
        !s_otaPendingVerify &&
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

      // Dừng MediaPlayer giải phóng SPI/RAM và chuyển về Standby trước khi ngủ
      appCtx.player.stop();
      currentAppState = AppState::STATE_STANDBY;
      // Không set forceStandbyRedraw ở đây để tránh race condition với Task_MediaPlayer
      
      appCtx.powerManager.enterLightSleep(sleepTimeUs, &appCtx.display);

      // Vừa tỉnh dậy: đánh dấu để lần ensureConnected() kế tiếp ÉP tái lập
      // association. WiFi.status() sau light sleep thường vẫn báo WL_CONNECTED
      // dù association đã chết -> nếu tin nó thì mọi http.GET() đều trả -1.
      appCtx.network.notifyWakeFromSleep();

      // Nháy đèn nền 3 lần NGAY LẬP TỨC sau khi thức dậy.
      // Gọi ở đây (trước delay) để đảm bảo các FreeRTOS task khác chưa resume,
      // SPI bus chưa có xung đột, GPIO an toàn để toggle.
      appCtx.display.wakeupFlash();

      delay(200);
      esp_sleep_wakeup_cause_t wakeupCause = esp_sleep_get_wakeup_cause();
      const char* causeStr = (wakeupCause == ESP_SLEEP_WAKEUP_TIMER) ? "timer" : "touch";
      DLOG("[SLP] wakeup=%s", causeStr);

      if (wakeupCause != ESP_SLEEP_WAKEUP_TIMER) {
        // Touch Wakeup: Re-init màn hình và render Standby UI.
        currentAppState = AppState::STATE_STANDBY;
        appCtx.display.turnOn();
        // currentAppState = AppState::STATE_STANDBY;
        forceStandbyRedraw = true;
        lastUserActivity = millis();
        activeSleepTimeoutMs = INACTIVITY_SLEEP_TIMEOUT_MS;
      } else {
        lastUserActivity = millis();
        activeSleepTimeoutMs = 2000;

        // Nháy đèn xanh dương (GPIO 8 - Bản SuperMini, trùng chân NAND CS) để báo hiệu wakeup ngầm.
        // Đây là chỉ báo timer-wake DUY NHẤT còn lại: wakeupFlash() (DisplayDriver.cpp:171)
        // giờ chỉ gọi gpio_hold_dis(), tên hàm đã lỗi thời, không nháy gì cả. User chốt giữ đèn.
        //
        // PHẢI giữ spiMutex suốt đoạn nháy. Comment cũ ghi "bus SPI hoàn toàn rảnh" là SAI:
        // Task_MediaPlayer có thể đang đẩy pixel lên SCK/MOSI, mà GPIO 8 chính là CS của
        // W25Q128. Ghim CS xuống LOW 30ms trong lúc có xung clock -> NAND chốt nhầm opcode.
        // Giữ mutex triệt tiêu đúng cơ chế đó (CS LOW mà không có clock là vô hại).
        // Chi phí: giữ mutex 160ms, cộng tối đa 1000ms chờ (timeout mặc định của
        // acquireSPI) trong trường hợp xấu. Lúc vừa thức thì SPI thường rảnh.
        if (appCtx.display.acquireSPI()) {
          pinMode(8, OUTPUT);
          digitalWrite(8, LOW);  // Đèn sáng (Active LOW) / NAND CS ghim xuống
          delay(30);
          digitalWrite(8, HIGH); // Đèn tắt / NAND CS nhả ra
          delay(70);
          digitalWrite(8, LOW);
          delay(30);
          digitalWrite(8, HIGH);
          appCtx.display.releaseSPI();
        } else {
          // Bắt buộc phải log: đèn này là chỉ báo timer-wake duy nhất, nên "không
          // nháy mà không nói gì" sẽ bị hiểu nhầm là B1 làm hỏng đèn.
          DLOG("[WAKE] blink skip: spi busy");
        }
      }

      // Chờ 200ms cho UI và SPIBus ổn định hoàn toàn trước khi kích hoạt task đồng bộ ngầm
      vTaskDelay(pdMS_TO_TICKS(200));

      // Thực hiện đồng bộ ngầm non-blocking sau khi thức dậy.
      // Luôn check tin mới + tải đầy đủ vào slot trước khi cho phát — không còn
      // nhánh "có tin local sẵn thì hoãn sync" (dễ bỏ sót tin mới trên Cloud).
      // Luôn sync, kể cả khi đầy slot. Trước 2026-09-05 chỗ này bỏ qua toàn bộ
      // chu kỳ khi đầy ("post-wakeup sync skip: FULL"), làm box mất báo thức và
      // OTA cho tới khi có slot trống. Cổng "đầy" giờ nằm trong syncWakeup(),
      // chỉ chặn đúng bước tải tin.
      // syncWakeup() da bao gom ensureConnected() + syncNtpTime().
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
    // Thực thi lệnh bật/tắt server OTA ở ĐÚNG task gọi handleClient() — xem s_otaServerCmd.
    int8_t cmd = s_otaServerCmd;
    if (cmd != 0) {
      s_otaServerCmd = 0;
      if (cmd > 0) {
        appCtx.network.startWebServer(OTA_HOSTNAME);
        // stopWebServer() delete + gán nullptr, nên mỗi lần vào là một WebServer mới:
        // đăng ký route đúng một lần trên mỗi instance, không tích luỹ handler.
        if (appCtx.network.getWebServer() != nullptr) {
          appCtx.otaHandler.registerRoutes(*appCtx.network.getWebServer());
        }
      } else {
        appCtx.network.stopWebServer();
      }
    }
    appCtx.network.update();
    // CÙNG task với handleClient() ở trên, nên watchdog không bao giờ chạy song
    // song với một Update.write() đang dở — không cần khoá gì thêm.
    appCtx.otaHandler.tickWatchdog();
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void setup() {
  // Dò trạng thái partition và dựng lưới an toàn ĐẦU TIÊN, trước mọi thứ có thể treo
  // (kể cả vòng while(1) ngay dưới đây). Chỉ bản vừa nạp qua OTA mới ở PENDING_VERIFY.
  {
    esp_ota_img_states_t st;
    if (esp_ota_get_state_partition(esp_ota_get_running_partition(), &st) == ESP_OK &&
        st == ESP_OTA_IMG_PENDING_VERIFY) {
      s_otaPendingVerify = true;
      const esp_timer_create_args_t args = {
          .callback = &otaGuardFire, .arg = nullptr, .dispatch_method = ESP_TIMER_TASK,
          .name = "ota_guard", .skip_unhandled_events = false};
      // +30s sau mốc xác nhận: đủ khoảng trống để Task_UIController (tick 10ms) xác
      // nhận trước, mà vẫn đủ ngắn để bản treo không ngồi im quá lâu.
      if (esp_timer_create(&args, &s_otaGuardTimer) == ESP_OK) {
        esp_timer_start_once(s_otaGuardTimer, (uint64_t)(OTA_VERIFY_DELAY_MS + 30000) * 1000ULL);
      }
    }
  }

  spiMutex = xSemaphoreCreateMutex();
  eventQueue = xQueueCreate(8, sizeof(SystemEvent));

  if (spiMutex == nullptr || eventQueue == nullptr) {
    // Không có display/logger ở thời điểm này, chỉ blink đèn nền để báo lỗi.
    while (1) delay(1000);
  }

  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, -1);

  appCtx.display.init(spiMutex);
  appCtx.display.setBacklight(BACKLIGHT_DAY_PERCENT);
  appCtx.display.showMessage("Booting...");

  // Khởi tạo ScreenLogger sau khi display đã sẵn sàng
  ScreenLogger::init(&appCtx.display);
  DLOG("[BOOT] cpu=%uMHz heap=%u", ESP.getCpuFreqMHz(), ESP.getFreeHeap());
  DLOG("[BOOT] wakeup=%d", (int)esp_sleep_get_wakeup_cause());
  // reset=1 POWERON, 3 SW, 4 INT_WDT, 5 TASK_WDT, 6 WDT, 9 BROWNOUT, 12 PANIC.
  // Nếu dòng này lặp lại đều đặn trong log ⇒ box đang reset vòng lặp, không phải lỗi audio.
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

  // Log rõ nguồn credentials: trước đây chỉ in SSID nên không phân biệt được
  // "đang dùng creds mặc định" với "đang dùng creds cũ còn sót trong NVS".
  DLOG("[BOOT] WiFi: %s (%s)", wifiSsid, credsFromNvs ? "NVS" : "default");

  if (appCtx.network.connectWiFi(wifiSsid, wifiPass) != WiFiConnectResult::CONNECTED && credsFromNvs) {
    // Creds trong NVS (lưu từ lần provisioning trước) có thể đã cũ/sai — ví dụ
    // đổi mật khẩu router. Trước đây hễ NVS có BẤT KỲ SSID nào thì creds mặc định
    // trong config.h không bao giờ được thử tới, nên box đi thẳng vào AP mode dù
    // creds mặc định vẫn dùng được. Thử nốt trước khi bỏ cuộc.
    DLOG("[BOOT] NVS creds fail -> try default");
    strncpy(wifiSsid, DEFAULT_WIFI_SSID, sizeof(wifiSsid) - 1);
    strncpy(wifiPass, DEFAULT_WIFI_PASSWORD, sizeof(wifiPass) - 1);
    appCtx.network.connectWiFi(wifiSsid, wifiPass);
  }
  appCtx.layoutEngine.loadConfig(defaultLayoutJson);

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

  appCtx.player.init(appCtx.storage, &appCtx.display);
  
  // Phát beep test loa khi khởi động
  appCtx.player.testAudioBeep();
  
  appCtx.ui.init(PIN_TOUCH, &appCtx.display);
  appCtx.powerManager.init((gpio_num_t)PIN_TOUCH);
  lastUserActivity = millis();
  appCtx.ui.showBootScreen();

  if (appCtx.network.isConnected()) {
    DLOG("[BOOT] WiFi OK -> NTP+Firebase");
    // Web server OTA KHÔNG còn bật ở đây (2026-09-18). Nó chạy suốt đời máy cho
    // một việc hiếm khi làm, giữ RAM của WebServer + mDNS. Từ 2026-09-21 nó chỉ
    // dựng khi người dùng làm chuỗi chạm 3s-3s-6s vào STATE_OTA (enterOtaMode trong
    // Task_MediaPlayer); đường kích hoạt bằng cờ cloud đã gỡ hẳn.

    appCtx.network.setOnDownloadComplete([]() {
      forceStandbyRedraw = true;
    });

    appCtx.network.setPlaybackActiveCallback([]() {
      return (currentAppState == AppState::STATE_VIDEO);
    });

    // Kích hoạt Firebase Sync ngầm ngay khi vừa nạp code/khởi động xong
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