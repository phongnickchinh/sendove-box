import apiClient from './client';

/** Endpoints under /boxes/:boxId (box.routes.ts); each returns response.data. */

export const getBoxDetails = async (boxId) => {
  const response = await apiClient.get(`/boxes/${boxId}`);
  return response.data; // { success: true, data: Box }
};

/** Send only changed fields: any write sets config_flag and makes the box re-read its config. */
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
