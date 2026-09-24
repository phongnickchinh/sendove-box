import apiClient from './client';
import { uploadToSignedPolicy } from '../utils/mediaUploader';

/**
 * Thư viện nhạc báo thức của hộp — /boxes/:boxId/music (music.routes.ts), chỉ người nhận.
 * Tối đa 10 bài, mỗi bài 5–60 giây (MUSIC_LIMITS ở backend).
 * Mọi response bọc { success, data }.
 */

/** data: AlarmMusic[] {music_id, name, rev, duration_ms, size, ...} theo thứ tự thêm */
export const listMusic = async (boxId) => {
  const response = await apiClient.get(`/boxes/${boxId}/music`);
  return response.data;
};

/**
 * Tải lên 2 bước: xin policy → tải thẳng lên Storage → commit (backend tự đo size + crc32
 * và kiểm header AUDC). musicId có = thay nội dung bài đó (rev mới).
 * blob: kết quả encodeAlarmMusic(). Trả AlarmMusic đã lưu.
 */
export const uploadMusic = async (boxId, blob, { name, durationMs, musicId }, onProgress) => {
  const init = await apiClient.post(`/boxes/${boxId}/music/upload`, musicId ? { music_id: musicId } : {});
  const { music_id, rev, upload } = init.data.data;
  await uploadToSignedPolicy(upload, blob, onProgress);
  const res = await apiClient.post(`/boxes/${boxId}/music/commit`, {
    music_id, rev, name, duration_ms: durationMs,
  });
  return res.data.data;
};

export const renameMusic = async (boxId, musicId, name) => {
  const response = await apiClient.patch(`/boxes/${boxId}/music/${musicId}`, { name });
  return response.data;
};

/** data: { detached_alarms } — số báo thức đang dùng bài này, đã chuyển về tiếng bíp. */
export const deleteMusic = async (boxId, musicId) => {
  const response = await apiClient.delete(`/boxes/${boxId}/music/${musicId}`);
  return response.data;
};

/** data: { url } ký 15 phút tới file .aud (bỏ 10 byte đầu để nghe, xem audFileToWavBlob). */
export const getMusicPreviewUrl = async (boxId, musicId) => {
  const response = await apiClient.get(`/boxes/${boxId}/music/${musicId}/preview`);
  return response.data;
};
