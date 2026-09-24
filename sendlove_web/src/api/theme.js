import apiClient from './client';
import { uploadToSignedPolicy } from '../utils/mediaUploader';

/**
 * Giao diện màn chờ — theme.controller.ts / theme.routes.ts, chỉ người nhận.
 * Mọi response đều bọc { success, data }.
 */

/** data: BoxTheme | null (chưa từng lưu → hộp đang dùng bố cục trong firmware) */
export const getTheme = async (boxId) => {
  const response = await apiClient.get(`/boxes/${boxId}/theme`);
  return response.data;
};

/**
 * body: { theme_name, widgets: [{type,x,y,w,h,color?,align?,font?,family?,px?,format?,locale?}],
 *         background: string|null, fonts?: { f_time?: path, f_date?: path } }
 * data: BoxTheme đã lưu, kèm theme_id + rev (hộp so rev với bản trong flash của nó).
 */
export const saveTheme = async (boxId, body) => {
  const response = await apiClient.put(`/boxes/${boxId}/theme`, body);
  return response.data;
};

/** Tải một file phông VLW (utils/vlw.js) lên Storage, trả path để đưa vào saveTheme({ fonts }). */
export const uploadFont = async (boxId, bytes, onProgress) => {
  const response = await apiClient.post(`/boxes/${boxId}/theme/font`);
  const { path, upload } = response.data.data;
  await uploadToSignedPolicy(upload, new Blob([bytes], { type: 'application/octet-stream' }), onProgress);
  return path;
};

/**
 * Tải ảnh nền RGB565 (đúng 115.200 byte) lên Storage, trả storage path để
 * đưa vào saveTheme({ background }).
 */
export const uploadBackground = async (boxId, bytes, onProgress) => {
  const response = await apiClient.post(`/boxes/${boxId}/theme/background`);
  const { path, upload } = response.data.data; // { path, upload: { url, fields } }
  await uploadToSignedPolicy(upload, new Blob([bytes], { type: 'application/octet-stream' }), onProgress);
  return path;
};
