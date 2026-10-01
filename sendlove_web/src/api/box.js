import apiClient from './client';

/**
 * Endpoints under /boxes/:boxId — see sendlove_backend/src/routes/box.routes.ts.
 * Same shape as message.js: take a boxId, return response.data as-is.
 */

export const getBoxDetails = async (boxId) => {
  const response = await apiClient.get(`/boxes/${boxId}`);
  return response.data; // { success: true, data: Box }
};

/**
 * Send only the fields that actually changed. The backend sets config_flag so
 * the ESP32 re-reads its config — an unchanged field still wakes the box for nothing.
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
