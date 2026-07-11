import { initiateMessage, confirmMessage } from '../api/message';

/**
 * Upload files via POST to signed policy URLs and track progress
 */
const uploadToSignedPolicy = (policyObj, blob, contentType, onProgress) => {
  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();
    xhr.open('POST', policyObj.url, true);
    
    xhr.upload.onprogress = (e) => {
      if (e.lengthComputable && onProgress) {
        onProgress((e.loaded / e.total) * 100);
      }
    };
    
    xhr.onload = () => {
      if (xhr.status >= 200 && xhr.status < 300) {
        resolve();
      } else {
        reject(new Error(`Upload failed with status ${xhr.status}: ${xhr.responseText}`));
      }
    };
    
    xhr.onerror = () => reject(new Error('Network error during upload'));

    const formData = new FormData();
    // Append all policy fields
    Object.keys(policyObj.fields).forEach(key => {
      formData.append(key, policyObj.fields[key]);
    });
    // File must be the last appended field
    formData.append('file', blob);

    xhr.send(formData);
  });
};

/**
 * Orchestrates the full upload process: initiate -> upload files -> confirm
 * 
 * @param {string} boxId 
 * @param {Object} data 
 * @param {string} data.type - 'video' | 'image' | 'voice'
 * @param {string} data.text 
 * @param {Blob} data.binBlob 
 * @param {Blob} data.voiceBlob 
 * @param {Blob} data.thumbBlob 
 * @param {Blob} data.originalBlob - mp4, jpg, gif
 * @param {Object} data.metadata - { duration, frameCount, width, height }
 * @param {Function} onProgress - callback for total upload progress (0-100)
 */
export const uploadMessage = async (boxId, data, onProgress) => {
  const blobsToUpload = [];
  
  if (data.binBlob) blobsToUpload.push({ type: 'bin', blob: data.binBlob, contentType: 'application/octet-stream' });
  if (data.voiceBlob) blobsToUpload.push({ type: 'voice', blob: data.voiceBlob, contentType: 'audio/wav' });
  if (data.thumbBlob) blobsToUpload.push({ type: 'thumbnail', blob: data.thumbBlob, contentType: 'image/jpeg' });
  
  if (data.originalBlob) {
    const originalType = data.type === 'video' ? 'original_video' : 
                         data.type === 'image' ? 'original_image' : 
                         data.type === 'gif' ? 'original_gif' : null;
    if (originalType) {
      blobsToUpload.push({ 
        type: originalType, 
        blob: data.originalBlob, 
        contentType: data.originalBlob.type 
      });
    }
  }

  const requestedTypes = blobsToUpload.map(item => item.type);

  // 1. Initiate
  const initRes = await initiateMessage(boxId, requestedTypes);
  if (!initRes.success) throw new Error('Failed to initiate message');
  
  const { message_id, upload_urls } = initRes.data;

  // 2. Upload parallel
  let totalUploadedBytes = 0;
  const totalBytes = blobsToUpload.reduce((acc, item) => acc + item.blob.size, 0);
  
  // Track progress per file
  const fileProgress = {};
  blobsToUpload.forEach(item => fileProgress[item.type] = 0);

  const handleProgress = (type, percent, blobSize) => {
    fileProgress[type] = (percent / 100) * blobSize;
    const currentTotalUploaded = Object.values(fileProgress).reduce((acc, val) => acc + val, 0);
    if (onProgress && totalBytes > 0) {
      onProgress(Math.round((currentTotalUploaded / totalBytes) * 100));
    }
  };

  await Promise.all(blobsToUpload.map(item => {
    const policyObj = upload_urls[item.type];
    if (!policyObj) return Promise.resolve();
    return uploadToSignedPolicy(policyObj, item.blob, item.contentType, (percent) => handleProgress(item.type, percent, item.blob.size));
  }));

  // 3. Confirm
  const confirmRes = await confirmMessage(boxId, {
    message_id,
    type: data.type,
    text: data.text,
    duration: data.metadata?.duration,
    frame_count: data.metadata?.frameCount,
    width: data.metadata?.width,
    height: data.metadata?.height,
    uploaded_files: requestedTypes
  });

  if (!confirmRes.success) throw new Error('Failed to confirm message');

  return { success: true, messageId: message_id };
};
