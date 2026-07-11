import React, { useState, useRef } from 'react';

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
      // Check duration if possible, though we cap it during encoding anyway
      onVideoSelect(selectedFile);
    }
  };

  return (
    <div className="video-input-container glass-panel fade-in" style={{ padding: '20px', textAlign: 'center' }}>
      <h3>Gửi một đoạn Video</h3>
      <p style={{fontSize: '12px', color: '#666'}}>Tối đa 15 giây</p>
      
      {!previewUrl ? (
        <div className="upload-section" style={{ margin: '30px 0' }}>
          <label className="glass-button" style={{ display: 'inline-block', cursor: 'pointer' }}>
            🎥 Chọn/Quay Video
            <input 
              type="file" 
              accept="video/*" 
              onChange={handleFileChange} 
              style={{ display: 'none' }} 
            />
          </label>
        </div>
      ) : (
        <div className="preview-section">
          <div style={{ 
            width: '128px', height: '160px', 
            margin: '20px auto', 
            border: '2px dashed var(--color-primary)',
            borderRadius: '8px',
            overflow: 'hidden',
            position: 'relative'
          }}>
            <video 
              ref={videoRef}
              src={previewUrl} 
              autoPlay 
              loop 
              muted 
              style={{ width: '100%', height: '100%', objectFit: 'cover' }} 
            />
          </div>
          
          <div className="controls" style={{ display: 'flex', gap: '10px', justifyContent: 'center' }}>
            <button className="glass-button secondary" onClick={() => setPreviewUrl(null)}>Chọn lại</button>
            <button className="glass-button primary" onClick={handleConfirm}>Xác nhận</button>
          </div>
        </div>
      )}
      
      <button className="glass-button cancel-btn" onClick={onCancel} style={{marginTop: '20px', background: 'transparent', color: '#666', boxShadow: 'none'}}>
        Quay lại
      </button>
    </div>
  );
};

export default VideoInput;
