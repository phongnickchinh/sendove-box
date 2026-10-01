import apiClient from './client';
import { uploadToSignedPolicy } from '../utils/mediaUploader';

/**
 * The box's alarm music library — /boxes/:boxId/music (music.routes.ts), receiver only.
 * Up to 10 tracks, 5–60 seconds each (MUSIC_LIMITS in the backend).
 * Every response is wrapped in { success, data }.
 */

/** data: AlarmMusic[] {music_id, name, rev, duration_ms, size, ...} in insertion order */
export const listMusic = async (boxId) => {
  const response = await apiClient.get(`/boxes/${boxId}/music`);
  return response.data;
};

/**
 * Two-step upload: request a policy → upload straight to Storage → commit (the
 * backend measures size + crc32 and checks the AUDC header). Passing musicId
 * replaces that track's content (new rev).
 * blob: the result of encodeAlarmMusic(). Returns the stored AlarmMusic.
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

/** data: { detached_alarms } — how many alarms used this track and were switched to the beep. */
export const deleteMusic = async (boxId, musicId) => {
  const response = await apiClient.delete(`/boxes/${boxId}/music/${musicId}`);
  return response.data;
};

/** data: { url } signed for 15 minutes to the .aud file (skip the first 10 bytes to play it; see audFileToWavBlob). */
export const getMusicPreviewUrl = async (boxId, musicId) => {
  const response = await apiClient.get(`/boxes/${boxId}/music/${musicId}/preview`);
  return response.data;
};
