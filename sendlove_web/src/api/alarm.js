import apiClient from './client';

/**
 * Báo thức của hộp — /boxes/:boxId/alarms.
 * Chỉ role receiver gọi được (alarm.routes.ts:13 requireRole('receiver')).
 *
 * Ràng buộc từ validation.middleware.ts:151-162 — vi phạm là 400, không phải
 * lỗi mạng, nên phải chặn ngay ở form:
 *   time       "HH:mm" đúng 5 ký tự, có số 0 đứng đầu ("07:30", KHÔNG phải "7:30")
 *   is_enable  bắt buộc khi tạo
 *   repeatable bắt buộc khi tạo — false = kêu một lần rồi tự tắt
 */

export const getAlarms = async (boxId) => {
  const response = await apiClient.get(`/boxes/${boxId}/alarms`);
  return response.data; // { success: true, data: Alarm[] }
};

export const createAlarm = async (boxId, { time, is_enable, repeatable }) => {
  const response = await apiClient.post(`/boxes/${boxId}/alarms`, { time, is_enable, repeatable });
  return response.data; // 201 { success: true, data: Alarm }
};

/** PATCH: gửi được từng field một, dùng cho nút bật/tắt nhanh trong danh sách. */
export const updateAlarm = async (boxId, alarmId, patch) => {
  const response = await apiClient.patch(`/boxes/${boxId}/alarms/${alarmId}`, patch);
  return response.data;
};

export const deleteAlarm = async (boxId, alarmId) => {
  const response = await apiClient.delete(`/boxes/${boxId}/alarms/${alarmId}`);
  return response.data;
};
