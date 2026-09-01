import React, { useState, useRef } from 'react';
import Icon from '../ui/Icon';
import { Actions, Button, Tips } from '../ui/Screen';

/**
 * Thân của "create-content-dialog for video": một thẻ r16 nền trắng viền
 * caramel/300, bên trong là chip loại nội dung hoặc khung xem trước 240x240.
 * Khung máy (appbar, tiêu đề, ô lời nhắn) do SenderUI dựng.
 */
const VideoInput = ({ onVideoSelect, onCancel }) => {
  const [previewUrl, setPreviewUrl] = useState(null);
  const [selectedFile, setSelectedFile] = useState(null);
  const videoRef = useRef(null);

  const handleFileChange = (e) => {
    const file = e.target.files[0];
    if (file) {
      setSelectedFile(file);
      setPreviewUrl(URL.createObjectURL(file));
    }
  };

  const handleConfirm = () => {
    if (selectedFile && onVideoSelect) {
      // Độ dài được cắt về 15s ngay trong lúc mã hoá nên không chặn ở đây
      onVideoSelect(selectedFile);
    }
  };

  const reset = () => {
    setPreviewUrl(null);
    setSelectedFile(null);
  };

  return (
    <>
      <div className="sl-card sl-card--center">
        {!previewUrl ? (
          <>
            <span className="sl-chip"><Icon name="video" size={24} /></span>
            <span className="sl-label-s">Chọn hoặc quay một đoạn video</span>
            <span className="sl-caption">Tối đa 15 giây</span>
            <label className="sl-btn sl-btn--pri" style={{ marginTop: 'var(--sp-1)' }}>
              Chọn video
              <input type="file" accept="video/*" onChange={handleFileChange} style={{ display: 'none' }} />
            </label>
          </>
        ) : (
          <>
            {/* 240x240 = đúng kích thước màn hộp, xem trước 1:1 với thứ sẽ phát */}
            <div className="sl-preview">
              <video ref={videoRef} src={previewUrl} autoPlay loop muted playsInline />
            </div>
            <span className="sl-label-s">{selectedFile?.name}</span>
            <span className="sl-caption">Khung vuông 240x240, 15 hình/giây</span>
          </>
        )}
      </div>

      <Tips>Chỉ 15 giây đầu được gửi, kèm âm thanh 8 kHz mono.</Tips>

      <Actions>
        {previewUrl ? (
          <>
            <Button kind="pri" onClick={handleConfirm}>Xác nhận</Button>
            <Button kind="gho" onClick={reset}>Chọn lại</Button>
          </>
        ) : (
          <Button kind="gho" onClick={onCancel}>Quay lại</Button>
        )}
      </Actions>
    </>
  );
};

export default VideoInput;
