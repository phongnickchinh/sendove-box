import { BaseModel } from './base.types';

// ==================================================
// Nhạc báo thức — Node: boxes/{boxId}/music/{musicId}
// ==================================================
// Thư viện theo HỘP, chỉ người nhận quản lý (user chốt 2026-09-24, MEMORY.md §28).
// File trên Storage: media/{boxId}/music/{musicId}_r{rev}.aud = AUDC(10) + WAV(44) + PCM
// 16kHz mono 16-bit, web đóng gói (giải mã ở client). Sửa nội dung = rev mới + file mới,
// không bao giờ ghi đè file cũ (hộp đang tải dở bản cũ không đọc lẫn).

export interface AlarmMusic extends BaseModel {
  music_id: string;
  name: string;
  /** Tăng mỗi lần thay nội dung. Hộp so rev với /alarm/index.json trên thẻ. */
  rev: number;
  duration_ms: number;
  sample_rate: number;
  /** Byte của cả file .aud — hộp kiểm trước khi đổi tên .part */
  size: number;
  /** CRC-32 (zlib) của cả file, backend tự tính từ file đã tải lên */
  crc32: number;
  storage_path: string;
  created_by: string;
}

export const MUSIC_LIMITS = {
  MAX_TRACKS: 10,
  MIN_MS: 5000,
  MAX_MS: 60000,
  /** Khớp ALARM_MUSIC_MAX_BYTES của firmware: 60s × 16000 × 2 + header < 2MB */
  MAX_BYTES: 2_000_000,
  SAMPLE_RATE: 16000,
  NAME_MAX: 40,
} as const;
