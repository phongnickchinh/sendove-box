import apiClient from './client';

/**
 * Box alarms — /boxes/:boxId/alarms.
 * Receiver role only (alarm.routes.ts requireRole('receiver')).
 *
 * Constraints from validation.middleware.ts — a violation is a 400, not a
 * network error, so the form must enforce them:
 *   time       "HH:mm", exactly 5 chars with a leading zero ("07:30", NOT "7:30")
 *   is_enable  required on create
 *   repeatable required on create — false = ring once, then turn itself off
 */

export const getAlarms = async (boxId) => {
  const response = await apiClient.get(`/boxes/${boxId}/alarms`);
  return response.data; // { success: true, data: Alarm[] }
};

/**
 * music_id: "" = beep; otherwise a track from the box's music library (api/music.js).
 * volume 0–100 (default 80; the web blocks < 20), ramp = fade in over 20 seconds.
 */
export const createAlarm = async (boxId, { time, is_enable, repeatable, music_id, volume, ramp }) => {
  const response = await apiClient.post(`/boxes/${boxId}/alarms`, {
    time, is_enable, repeatable, music_id, volume, ramp,
  });
  return response.data; // 201 { success: true, data: Alarm }
};

/** PATCH: accepts single fields; used by the quick on/off toggle in the list. */
export const updateAlarm = async (boxId, alarmId, patch) => {
  const response = await apiClient.patch(`/boxes/${boxId}/alarms/${alarmId}`, patch);
  return response.data;
};

export const deleteAlarm = async (boxId, alarmId) => {
  const response = await apiClient.delete(`/boxes/${boxId}/alarms/${alarmId}`);
  return response.data;
};
