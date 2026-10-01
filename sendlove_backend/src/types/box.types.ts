import { BaseModel } from './base.types';
import { Alarm } from './alarm.types';
import { BoxTheme } from './theme.types';

// ==================================================
// Box — Node: boxes/{box_id}
// ==================================================
export interface BoxCode {
  rcode: string;             // pairing code for the receiver
  scode: string;             // pairing code for the sender
  rcode_created_at: number;  // when rcode was created (it expires)
  scode_created_at: number;  // when scode was created
}

export interface BoxPairing {
  sender_id?: string | null;
  receiver_id?: string | null;
  sender_paired_time?: number | null;
  receiver_paired_time?: number | null;
}

export type LedState = 'OFF' | 'BREATHING' | 'SOLID' | 'BLINK_FAST';

export interface BoxConfig {
  /** Alarms, keyed by alarmId */
  alarm_list: Record<string, Alarm>;

  wifi_config?: {
    ssid: string;
    pwd: string;
  };

  led_state?: LedState;
  display_brightness?: number; // 0-100 (the firmware clamps to at least 5%)
  playback_volume?: number;    // 0-100, 0 = mute
  /** Bumped on every PUT config. The box copies it to status.config_rev once applied. */
  config_rev?: number;

  /** Standby-screen layout, written through PUT /boxes/:boxId/theme */
  theme?: BoxTheme;
}

export interface BoxFlags {

  a_flag: boolean; /** The alarm list changed — the ESP32 must re-read it */
  ota_flag: boolean; /** A firmware OTA is pending */
  p_flag: boolean; /** Pairing changed (paired / unpaired) */
  config_flag: boolean; /** led_state/display_brightness/playback_volume changed — the ESP32 must re-read them */
  theme_flag?: boolean; /** config/theme (standby screen) changed — the box downloads the theme package by rev */
  music_flag?: boolean; /** The alarm music library changed (track added/edited/removed) — the box refetches the list */
}

export interface BoxStatus {
  online: boolean;
  charging: boolean;
  battery: number;          // battery percent (0-100)
  fw_version: string;
  /**
   * Last time the ESP32 was in touch. BEWARE of two units: /device/heartbeat
   * writes milliseconds (Date.now()), while the current firmware PATCHes
   * status.json directly with time(nullptr) = SECONDS (NetworkManager.cpp
   * heartbeat). The web normalizes both.
   */
  last_seen: number;
  /** The firmware's direct PATCH uses the keys "fw" (not fw_version) and "is_charging". */
  fw?: string;
  is_charging?: boolean;
  /**
   * Storage that holds messages. Decides the video/audio duration cap the web
   * allows (NAND: 3 slots of ~5.3 MB → 15s; SD card → 60s). No firmware sends
   * this field YET — when missing, the web assumes 'sd' (the current build).
   */
  storage_type?: 'sd' | 'nand';
  /** The config_rev the box has applied (brightness, volume). Lower than config.config_rev = still waiting for the box. */
  config_rev?: number;
  /** SD card: 'ok' | 'absent' (mount failed / just removed) | 'none' (NAND build). */
  sd_state?: 'ok' | 'absent' | 'none';
  sd_free_mb?: number;
  /** Theme rev the box is showing (compare with config.theme.rev). */
  theme_rev?: number;
  /** Number of alarm music tracks already on the card. */
  music_n?: number;
  /** Tail of the box's log; pushed only on a new error or the first sync after boot. */
  log_tail?: string;
  /** Seconds (time(nullptr)) when log_tail was pushed. */
  log_at?: number;
}

export interface Box extends BaseModel {
  device_secret?: string;
  code: BoxCode;
  pairing: BoxPairing;
  config: BoxConfig;
  flags: BoxFlags;
  status: BoxStatus;
}

// ==================================================
// Firmware — Node: firmware/{fw_id}
// ==================================================
export interface Firmware extends BaseModel {
  version: string;
  storage_url: string;      // firmware file URL on Firebase Storage
  checksum: string;         // sha256:...
}

// ==================================================
// OTA Task — Node: ota_tasks/{task_id}
// ==================================================
export type OtaStatus = 'pending' | 'downloading' | 'completed' | 'failed';

export interface OtaTask extends BaseModel {
  box_id: string;
  fw_version: string;
  status: OtaStatus;
  progress_percent?: number;
  error_message?: string;
}
