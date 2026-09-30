import { encodeVideoToBin, encodeImageToBin, extractAudioFromVideo } from './mediaEncoder';
import { uploadMessage } from './mediaUploader';
import { MAX_BIN_BYTES } from './boxStatus';

/** Lỗi đọc được cho người dùng; lỗi lạ thì rơi về câu mặc định của màn báo lỗi. */
export class SendError extends Error {}

/**
 * Mã hoá + tải một tin lên — tách khỏi SenderUI để việc gửi chạy tiếp sau khi người dùng
 * rời màn gửi (context/SendContext.jsx giữ trạng thái).
 *
 * input: { type, text, mediaData, range } — mediaData tuỳ loại:
 *   video/image: File · voice: { wavBlob, duration } · static: { imageBlob, audioData }
 * cb: { onPhase('uploading'), onProgress(0-100), onSummary({ fileName, duration }) }
 */
export async function runSend(boxId, { type, text, mediaData, range }, { onPhase, onProgress, onSummary }) {
  let payload = { type, text };

  if (type === 'video') {
    const file = mediaData;
    onSummary({ fileName: file.name, duration: 0 });
    const encodeRes = await encodeVideoToBin(file, onProgress, range);
    // Chặn TRƯỚC khi tải lên: máy chủ từ chối file bin quá MAX_BIN_BYTES.
    if (encodeRes.binBlob.size > MAX_BIN_BYTES) {
      const mb = (b) => (b / 1e6).toFixed(1).replace('.', ',');
      const fitSecs = Math.max(1, Math.floor(encodeRes.duration * (MAX_BIN_BYTES / encodeRes.binBlob.size)));
      throw new SendError(
        `Đoạn video sau khi nén nặng ${mb(encodeRes.binBlob.size)} MB, máy chủ chỉ nhận tối đa `
        + `${mb(MAX_BIN_BYTES)} MB mỗi tin. Chọn đoạn ngắn hơn — khoảng ${fitSecs} giây là vừa.`,
      );
    }
    onSummary({ fileName: file.name, duration: encodeRes.duration });
    onProgress(0); // Reset progress cho bước trích xuất âm thanh
    const voiceBlob = await extractAudioFromVideo(file, onProgress, range);

    payload = {
      ...payload,
      binBlob: encodeRes.binBlob,
      thumbBlob: encodeRes.thumbBlob,
      voiceBlob,
      originalBlob: file,
      metadata: {
        duration: encodeRes.duration,
        frameCount: encodeRes.frameCount,
        width: 240,
        height: 240,
      },
    };
  } else if (type === 'image') {
    const file = mediaData;
    const encodeRes = await encodeImageToBin(file);

    payload = {
      ...payload,
      binBlob: encodeRes.binBlob,
      thumbBlob: encodeRes.thumbBlob,
      originalBlob: file,
      metadata: { frameCount: 1, width: 240, height: 240 },
    };
  } else if (type === 'voice') {
    const { wavBlob, duration } = mediaData; // from VoiceRecorder
    onSummary({ fileName: null, duration });

    payload = { ...payload, voiceBlob: wavBlob, metadata: { duration } };
  } else if (type === 'static') {
    // Tin nhắn tĩnh: tuỳ tổ hợp ảnh / text / nhạc nền — tái dùng type "image"
    // có sẵn ở backend (không thêm enum mới). mediaData = { imageBlob, audioData }.
    const { imageBlob, audioData } = mediaData || {};
    let extra = {};

    if (imageBlob) {
      const encodeRes = await encodeImageToBin(imageBlob);
      extra = {
        ...extra,
        binBlob: encodeRes.binBlob,
        thumbBlob: encodeRes.thumbBlob,
        metadata: { frameCount: 1, width: 240, height: 240 },
      };
    }
    if (audioData?.wavBlob) {
      extra = { ...extra, bgMusicBlob: audioData.wavBlob };
      onSummary({ fileName: null, duration: audioData.duration });
    }

    payload = {
      ...payload,
      // Chỉ có chữ thì gửi đúng là tin chữ — type 'image' không kèm ảnh nào
      // làm lịch sử và hộp tưởng có ảnh.
      type: imageBlob || audioData?.wavBlob ? 'image' : 'text',
      ...extra,
    };
  }

  onPhase('uploading');
  await uploadMessage(boxId, payload, onProgress);
}
