import { BaseModel } from './base.types';
import { Alarm } from './alarm.types';
import { BoxTheme } from './theme.types';

// ==================================================
// Box — Node: boxes/{box_id}
// ==================================================
export interface BoxCode {
  rcode: string;             // Mã pairing cho Receiver
  scode: string;             // Mã pairing cho Sender
  rcode_created_at: number;  // Timestamp tạo rcode (hết hạn sau X giờ)
  scode_created_at: number;  // Timestamp tạo scode
}

export interface BoxPairing {
  sender_id?: string | null;
  receiver_id?: string | null;
  sender_paired_time?: number | null;
  receiver_paired_time?: number | null;
}

export type LedState = 'OFF' | 'BREATHING' | 'SOLID' | 'BLINK_FAST';

export interface BoxConfig {
  /** Danh sách báo thức, key = alarmId */
  alarm_list: Record<string, Alarm>;

  wifi_config?: {
    ssid: string;
    pwd: string;
  };

  led_state?: LedState;
  display_brightness?: number; // 0-100 (firmware kẹp sàn 5%)
  playback_volume?: number;    // 0-100, 0 = tắt tiếng
  /** Tăng mỗi lần PUT config. Hộp chép vào status.config_rev khi đã áp dụng. */
  config_rev?: number;

  /** Bố cục màn chờ, ghi qua PUT /boxes/:boxId/theme */
  theme?: BoxTheme;
}

export interface BoxFlags {

  a_flag: boolean; /** Cờ báo alarm list đã thay đổi — ESP32 cần đọc lại */
  ota_flag: boolean; /** Cờ báo có OTA firmware đang chờ */
  p_flag: boolean; /** Cờ báo có thay đổi pairing (thêm/ngắt kết nối) */
  config_flag: boolean; /** Cờ báo led_state/display_brightness/playback_volume đã thay đổi — ESP32 cần đọc lại */
  theme_flag?: boolean; /** Cờ báo config/theme (màn chờ) đã đổi — hộp tải gói theme theo rev */
  music_flag?: boolean; /** Thư viện nhạc báo thức đổi (thêm/sửa/xoá bài) — hộp lấy lại danh sách */
}

export interface BoxStatus {
  online: boolean;
  charging: boolean;
  battery: number;          // Phần trăm pin (0-100)
  fw_version: string;
  /**
   * Timestamp lần cuối ESP32 liên lạc. CHÚ Ý hai đơn vị: /device/heartbeat ghi
   * mili-giây (Date.now()), còn firmware hiện tại PATCH thẳng status.json với
   * time(nullptr) = GIÂY (NetworkManager.cpp heartbeat). Web chuẩn hoá lại.
   */
  last_seen: number;
  /** Firmware PATCH thẳng dùng khoá "fw" (không phải fw_version) và "is_charging". */
  fw?: string;
  is_charging?: boolean;
  /**
   * Loại bộ nhớ chứa tin nhắn. Quyết định trần thời lượng video/âm thanh web
   * cho phép gửi (NAND: 3 slot ~5,3 MB → 15s; thẻ SD → 60s). Firmware CHƯA
   * gửi trường này — thiếu thì web coi là 'sd' (bản build hiện tại là SD).
   */
  storage_type?: 'sd' | 'nand';
  /** config_rev mà hộp đã áp dụng (độ sáng, âm lượng). Nhỏ hơn config.config_rev = đang chờ hộp. */
  config_rev?: number;
  /** Thẻ nhớ: 'ok' | 'absent' (không mount được / vừa mất) | 'none' (bản NAND). */
  sd_state?: 'ok' | 'absent' | 'none';
  sd_free_mb?: number;
  /** Rev theme hộp đang hiển thị (so với config.theme.rev). */
  theme_rev?: number;
  /** Số bài nhạc báo thức đã có trên thẻ. */
  music_n?: number;
  /** Đoạn cuối nhật ký hộp, chỉ đẩy khi có lỗi mới hoặc lần sync đầu sau boot. */
  log_tail?: string;
  /** Giây (time(nullptr)) lúc đẩy log_tail. */
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
  storage_url: string;      // URL file firmware trên Firebase Storage
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
