import apiClient from './client';

export const initiateMessage = async (boxId, types) => {
  const response = await apiClient.post(`/boxes/${boxId}/messages/initiate`, { types });
  return response.data; // { success: true, data: { message_id, upload_urls } }
};

export const confirmMessage = async (boxId, data) => {
  const response = await apiClient.post(`/boxes/${boxId}/messages/confirm`, data);
  return response.data; // { success: true, data: Message }
};

export const getMessages = async (boxId, limit = 20) => {
  const response = await apiClient.get(`/boxes/${boxId}/messages`, { params: { limit } });
  // The backend returns data: { messages, pagination }; callers expect data: Message[].
  return { success: response.data.success, data: response.data.data?.messages || [] };
};

export const getMessageDetails = async (boxId, messageId) => {
  const response = await apiClient.get(`/boxes/${boxId}/messages/${messageId}`);
  // data.media holds signed URLs valid for 15 minutes; the other *_url fields are
  // raw storage paths the browser can't open.
  return response.data;
};
