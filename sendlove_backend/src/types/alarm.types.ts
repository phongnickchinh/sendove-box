import { BaseModel } from './base.types';

// ==================================================
// Alarm — Lưu tại: boxes/{boxId}/config/alarm_list/{alarmId}
// ==================================================
export interface Alarm extends BaseModel {
  /** Giờ báo thức, format "HH:mm" (24h) */
  time: string;

  /** Bật/tắt alarm này */
  is_enable: boolean;

  /**
   * true  = báo lặp lại mỗi ngày
   * false = one-shot, sau khi kích hoạt sẽ tự set is_enable = false
   */
  repeatable: boolean;
}
