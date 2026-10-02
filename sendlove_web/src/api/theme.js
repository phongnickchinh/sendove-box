import apiClient from './client';
import { uploadToSignedPolicy } from '../utils/mediaUploader';

/** Standby-screen theme (theme.routes.ts), receiver only. */

/** data: BoxTheme | null (never saved → the box shows its fallback screen) */
export const getTheme = async (boxId) => {
  const response = await apiClient.get(`/boxes/${boxId}/theme`);
  return response.data;
};

/**
 * body: { theme_name, widgets: [{type,x,y,w,h,color?,align?,font?,family?,px?,format?,locale?}],
 *         background: string|null, fonts?: { f_time?: path, f_date?: path } }
 * data: the stored BoxTheme, with theme_id + rev (the box compares rev with the copy in its flash).
 */
export const saveTheme = async (boxId, body) => {
  const response = await apiClient.put(`/boxes/${boxId}/theme`, body);
  return response.data;
};

/** Upload a VLW font file (utils/vlw.js) to Storage; returns the path to pass to saveTheme({ fonts }). */
export const uploadFont = async (boxId, bytes, onProgress) => {
  const response = await apiClient.post(`/boxes/${boxId}/theme/font`);
  const { path, upload } = response.data.data;
  await uploadToSignedPolicy(upload, new Blob([bytes], { type: 'application/octet-stream' }), onProgress);
  return path;
};

/** Upload an RGB565 background (exactly 115,200 bytes); returns the storage path for saveTheme(). */
export const uploadBackground = async (boxId, bytes, onProgress) => {
  const response = await apiClient.post(`/boxes/${boxId}/theme/background`);
  const { path, upload } = response.data.data; // { path, upload: { url, fields } }
  await uploadToSignedPolicy(upload, new Blob([bytes], { type: 'application/octet-stream' }), onProgress);
  return path;
};
