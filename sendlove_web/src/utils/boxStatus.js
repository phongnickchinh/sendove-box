/**
 * Đọc boxes/{id}/status đúng cách. Trường này có HAI người ghi:
 *   - backend /device/heartbeat: last_seen mili-giây, fw_version, charging
 *   - firmware hiện tại PATCH thẳng status.json (NetworkManager.cpp heartbeat):
 *     last_seen = time(nullptr) tính bằng GIÂY, "fw", "is_charging"
 * Đọc thô last_seen của firmware như mili-giây là ra "đồng bộ 20.000 ngày trước".
 */

/** Mốc 1e12 ms = năm 2001; epoch giây sẽ còn nhỏ hơn mốc này tới năm 33658. */
export function lastSeenMs(status) {
  const t = status?.last_seen;
  if (!t) return null;
  return t < 1e12 ? t * 1000 : t;
}

export const fwVersion = (status) => status?.fw_version || status?.fw || null;

const MINUTE = 60 * 1000;

/**
 * Tình trạng liên lạc suy từ last_seen — KHÔNG dùng status.online (hộp ngủ
 * gần như suốt, online=false không có nghĩa là hỏng; xem memory
 * sendlove-truong-chet). Hộp thức mỗi ~5 phút: trễ tới 15 phút vẫn bình
 * thường, quá 2 tiếng mới đáng gọi là mất liên lạc.
 */
export function syncTone(seenAtMs) {
  if (!seenAtMs) return 'unknown';
  const diff = Date.now() - seenAtMs;
  if (diff < 15 * MINUTE) return 'ok';
  if (diff < 2 * 60 * MINUTE) return 'late';
  return 'lost';
}

/**
 * Trần thời lượng video/âm thanh theo loại bộ nhớ của hộp.
 * NAND: 3 slot cố định ~5,3 MB → giữ 15s như trước.
 * Thẻ SD, hoặc hộp CHƯA báo loại bộ nhớ (firmware chưa gửi storage_type; bản
 * build hiện tại là SD — người dùng chốt coi thiếu là SD) → 60s, đúng trần
 * duration của backend (validation.middleware.ts confirmMessageSchema).
 */
export const MAX_SECONDS = { nand: 15, sd: 60 };

export function maxSecondsFor(box) {
  return box?.status?.storage_type === 'nand' ? MAX_SECONDS.nand : MAX_SECONDS.sd;
}

/** Trần kích thước file bin backend chấp nhận (message.service.ts initiateMessage). */
export const MAX_BIN_BYTES = 25 * 1024 * 1024;
