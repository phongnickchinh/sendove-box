import apiClient from './client';

/**
 * Box alarms — /boxes/:boxId/alarms, receiver only. The backend validates
 * (400 on violation), so the form must enforce:
 *   time       "HH:mm" with a leading zero ("07:30", NOT "7:30")
 *   is_enable, repeatable   required on create (repeatable false = ring once)
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
