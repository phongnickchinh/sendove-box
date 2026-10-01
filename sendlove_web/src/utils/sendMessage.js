import { encodeVideoToBin, encodeImageToBin, extractAudioFromVideo } from './mediaEncoder';
import { uploadMessage } from './mediaUploader';
import { MAX_BIN_BYTES } from './boxStatus';

/** A user-readable error; anything else falls back to the error screen's default text. */
export class SendError extends Error {}

/**
 * Encode + upload one message — kept out of SenderUI so a send keeps running
 * after the user leaves the send screen (context/SendContext.jsx holds the state).
 *
 * input: { type, text, mediaData, range } — mediaData depends on the type:
 *   video/image: File · voice: { wavBlob, duration } · static: { imageBlob, audioData }
 * cb: { onPhase('uploading'), onProgress(0-100), onSummary({ fileName, duration }) }
 */
export async function runSend(boxId, { type, text, mediaData, range }, { onPhase, onProgress, onSummary }) {
  let payload = { type, text };

  if (type === 'video') {
    const file = mediaData;
    onSummary({ fileName: file.name, duration: 0 });
    const encodeRes = await encodeVideoToBin(file, onProgress, range);
    // Fail BEFORE uploading: the server rejects a bin over MAX_BIN_BYTES.
    if (encodeRes.binBlob.size > MAX_BIN_BYTES) {
      const mb = (b) => (b / 1e6).toFixed(1).replace('.', ',');
      const fitSecs = Math.max(1, Math.floor(encodeRes.duration * (MAX_BIN_BYTES / encodeRes.binBlob.size)));
      throw new SendError(
        `Đoạn video sau khi nén nặng ${mb(encodeRes.binBlob.size)} MB, máy chủ chỉ nhận tối đa `
        + `${mb(MAX_BIN_BYTES)} MB mỗi tin. Chọn đoạn ngắn hơn — khoảng ${fitSecs} giây là vừa.`,
      );
    }
    onSummary({ fileName: file.name, duration: encodeRes.duration });
    onProgress(0); // restart progress for the audio extraction step
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
    // Still message: any mix of image / text / background music — reuses the
    // backend's existing "image" type (no new enum). mediaData = { imageBlob, audioData }.
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
      // Text only → send a real text message; type 'image' without an image
      // would make the history and the box expect a picture.
      type: imageBlob || audioData?.wavBlob ? 'image' : 'text',
      ...extra,
    };
  }

  onPhase('uploading');
  await uploadMessage(boxId, payload, onProgress);
}
