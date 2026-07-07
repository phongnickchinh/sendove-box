import { BaseModel } from './base.types';
import { Alarm } from './alarm.types';

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

export interface BoxConfig {
  /** Danh sách báo thức, key = alarmId */
  alarm_list: Record<string, Alarm>;

  wifi_config?: {
    ssid: string;
    pwd: string;
  };
}

export interface BoxFlags {
  /** Cờ báo alarm list đã thay đổi — ESP32 cần đọc lại */
  a_flag: boolean;

  /** Cờ báo có OTA firmware đang chờ */
  ota_flag: boolean;

  /** Cờ báo có thay đổi pairing (thêm/ngắt kết nối) */
  p_flag: boolean;
}

export interface BoxStatus {
  online: boolean;
  charging: boolean;
  battery: number;          // Phần trăm pin (0-100)
  fw_version: string;
  last_seen: number;        // Timestamp lần cuối ESP32 liên lạc
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
