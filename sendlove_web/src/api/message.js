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
  // Backend thật trả data: { messages: Message[], pagination: {...} }
  // (message.controller.ts:41) — không phải data: Message[] như comment cũ ở
  // đây từng ghi sai. Mọi trang gọi getMessages() đều tin theo comment đó
  // (setMessages(res.data) rồi .map trực tiếp) nên object lọt vào thẳng state,
  // .map() ném TypeError ngay khi có phản hồi thành công đầu tiên — không lộ
  // ra lúc backend tắt (request reject, không set) hay dữ liệu giả trong lúc
  // dev, chỉ lộ khi có backend thật trả về đúng dạng.
  // Bóc mảng ra ở đây, giữ nguyên hợp đồng { success, data } mà mọi nơi gọi
  // đã tin sẵn — không phải sửa lại từng trang.
  return { success: response.data.success, data: response.data.data?.messages || [] };
};

export const getMessageDetails = async (boxId, messageId) => {
  const response = await apiClient.get(`/boxes/${boxId}/messages/${messageId}`);
  // { success, data: Message & { media: { video?, image?, thumbnail?, voice?, bg_music? } } }
  // media là signed URL đọc được 15 phút (message.service.ts getMessageDetails);
  // các trường *_url còn lại vẫn là storage path thô, trình duyệt không mở được.
  return response.data;
};
