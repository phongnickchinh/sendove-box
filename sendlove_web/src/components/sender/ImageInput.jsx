import React, { useState } from 'react';

const ImageInput = ({ onImageSelect, onCancel }) => {
  const [previewUrl, setPreviewUrl] = useState(null);
  const [selectedFile, setSelectedFile] = useState(null);

  const handleFileChange = (e) => {
    const file = e.target.files[0];
    if (file) {
      setSelectedFile(file);
      setPreviewUrl(URL.createObjectURL(file));
    }
  };

  const handleConfirm = () => {
    if (selectedFile && onImageSelect) {
      onImageSelect(selectedFile);
    }
  };

  return (
    <div className="image-input-container glass-panel fade-in" style={{ padding: '20px', textAlign: 'center' }}>
      <h3>Gửi một bức ảnh</h3>
      
      {!previewUrl ? (
        <div className="upload-section" style={{ margin: '30px 0' }}>
          <label className="glass-button" style={{ display: 'inline-block', cursor: 'pointer' }}>
            📷 Chọn ảnh
            <input 
              type="file" 
              accept="image/*" 
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
            <img 
              src={previewUrl} 
              alt="Preview" 
              style={{ width: '100%', height: '100%', objectFit: 'cover' }} 
            />
            <div style={{
              position: 'absolute', bottom: 0, width: '100%',
              background: 'rgba(0,0,0,0.5)', color: 'white', fontSize: '10px', padding: '2px'
            }}>Khu vực hiển thị</div>
          </div>
          
          <div className="controls" style={{ display: 'flex', gap: '10px', justifyContent: 'center' }}>
            <button className="glass-button secondary" onClick={() => setPreviewUrl(null)}>Chọn ảnh khác</button>
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

export default ImageInput;
