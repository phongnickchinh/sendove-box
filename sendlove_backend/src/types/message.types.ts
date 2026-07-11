import { BaseModel } from './base.types';

// ==================================================
// Message — Node: messages/{box_id}/{message_id}
// ==================================================
// Không có MessageStatus: Sender không được biết trạng thái tin nhắn.
// ESP32 dùng timestamp + local last_download_ts để xác định tin mới.
// ==================================================
export interface Message extends BaseModel {
  sender_id: string;
  box_id: string;

  /** Thời điểm gửi (ESP32 so sánh trường này với last_download_ts nội bộ) */
  timestamp: number;

  /** Loại tin nhắn chính */
  type: 'video' | 'image' | 'gif' | 'voice' | 'text';

  /** Nội dung text của tin nhắn */
  text?: string;

  /** Video đã encode RGB565 (.bin) - URL trên Firebase Storage */
  bin_url?: string;

  /** Audio ghi âm (.wav) - URL trên Firebase Storage */
  voice_url?: string;

  /** File gốc: Video (.mp4) - URL */
  video_url?: string;

  /** File gốc: Ảnh GIF - URL */
  gif_url?: string;

  /** File gốc: Nhạc nền - URL */
  bg_music_url?: string;

  /** File gốc: Ảnh tĩnh - URL */
  image_url?: string;

  /** Tổng dung lượng file đính kèm encode và gốc (bytes) */
  total_size?: number;

  /** Thumbnail cho Web App hiển thị lịch sử */
  thumbnail_url?: string;

  /** Thời lượng media tính bằng giây */
  duration?: number;

  /** Số frame trong file .bin */
  frame_count?: number;

  /** Kích thước video encode (pixels) */
  width?: number;
  height?: number;
}
