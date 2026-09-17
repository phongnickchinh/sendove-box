import apiClient from './client';

/**
 * Các endpoint ở /boxes/:boxId. Đối chiếu sendlove_backend/src/routes/box.routes.ts.
 * Giữ đúng khuôn của message.js: nhận boxId, trả thẳng response.data.
 */

export const getBoxDetails = async (boxId) => {
  const response = await apiClient.get(`/boxes/${boxId}`);
  return response.data; // { success: true, data: Box }
};

/**
 * Chỉ gửi những field thật sự đổi. Backend set config_flag để ESP32 biết
 * phải đọc lại — nên gửi thừa field cũng làm hộp thức dậy đọc lại vô ích.
 */
export const updateBoxConfig = async (boxId, config) => {
  const response = await apiClient.put(`/boxes/${boxId}/config`, config);
  return response.data;
};

export const updateWifi = async (boxId, wifi) => {
  const response = await apiClient.put(`/boxes/${boxId}/wifi`, wifi);
  return response.data;
};

export const unpairBox = async (boxId) => {
  const response = await apiClient.delete(`/boxes/${boxId}/unpair`);
  return response.data;
};
