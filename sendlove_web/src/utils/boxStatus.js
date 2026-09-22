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

/**
 * Trần MỘT file media firmware hiện tại tải được (MAX_MEDIA_BYTES, config.h).
 * Vượt trần, firmware huỷ tải và KHÔNG tăng last_download_ts
 * (NetworkManager.cpp "ts fail" → break) — nên tin quá cỡ bị tải lại mãi ở mọi
 * lần đồng bộ và chặn luôn MỌI tin gửi sau nó. Không được để lọt.
 */
export const FW_MAX_MEDIA_BYTES = 5500000;

/**
 * Trần kích thước file bin gửi được cho hộp này: firmware báo
 * status.max_media_bytes (khi đã nâng trần) thì dùng số đó, không thì trần của
 * bản firmware hiện tại. Không bao giờ vượt trần backend.
 */
/**
 * Trần âm thanh firmware PHÁT được: AUDIO_MAX_PCM_BYTES = 600000 (config.h).
 * Lớn hơn thì AudioPlayer báo "AUDC invalid size" và im lặng — tải về vẫn
 * được, nên không kẹt hàng đợi, nhưng tin thoại thành tin câm. Ở 8 kHz mono
 * 16-bit trần này là 37 giây (mediaEncoder tự hạ 8 kHz khi quá 18,7s).
 */
export const FW_AUDIO_PCM_BYTES = 600000;

export function maxAudioSecondsFor(box) {
  const pcm = Number(box?.status?.max_audio_pcm_bytes) || FW_AUDIO_PCM_BYTES;
  return Math.floor(pcm / (8000 * 2));
}

export function maxBinBytesFor(box) {
  const fw = Number(box?.status?.max_media_bytes) || FW_MAX_MEDIA_BYTES;
  return Math.min(fw, MAX_BIN_BYTES);
}
