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

  /** Bài trong boxes/{boxId}/music. Không có = tiếng bíp. Hộp chỉ tải bài báo thức dùng. */
  music_id?: string | null;
  /** 0-100, riêng từng báo thức (mặc định 80). Web chặn < 20. */
  volume?: number;
  /** Tăng dần từ 30% mức đã chọn trong 20s (mặc định bật). */
  ramp?: boolean;
}
