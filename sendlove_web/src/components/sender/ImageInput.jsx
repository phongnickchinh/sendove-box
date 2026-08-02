import React, { useState, useCallback } from 'react';
import Cropper from 'react-easy-crop';

const createImage = (url) =>
  new Promise((resolve, reject) => {
    const image = new Image();
    image.addEventListener('load', () => resolve(image));
    image.addEventListener('error', (error) => reject(error));
    image.setAttribute('crossOrigin', 'anonymous');
    image.src = url;
  });

async function getCroppedImg(imageSrc, pixelCrop) {
  const image = await createImage(imageSrc);
  const canvas = document.createElement('canvas');
  const ctx = canvas.getContext('2d');

  canvas.width = pixelCrop.width;
  canvas.height = pixelCrop.height;

  ctx.drawImage(
    image,
    pixelCrop.x,
    pixelCrop.y,
    pixelCrop.width,
    pixelCrop.height,
    0,
    0,
    pixelCrop.width,
    pixelCrop.height
  );

  return new Promise((resolve) => {
    canvas.toBlob((blob) => {
      resolve(blob);
    }, 'image/jpeg');
  });
}

const ImageInput = ({ onImageSelect, onCancel }) => {
  const [previewUrl, setPreviewUrl] = useState(null);
  const [crop, setCrop] = useState({ x: 0, y: 0 });
  const [zoom, setZoom] = useState(1);
  const [croppedAreaPixels, setCroppedAreaPixels] = useState(null);
  const [isCropping, setIsCropping] = useState(false);

  const handleFileChange = (e) => {
    const file = e.target.files[0];
    if (file) {
      setPreviewUrl(URL.createObjectURL(file));
    }
  };

  const onCropComplete = useCallback((croppedArea, croppedAreaPixels) => {
    setCroppedAreaPixels(croppedAreaPixels);
  }, []);

  const handleConfirm = async () => {
    if (previewUrl && croppedAreaPixels && onImageSelect) {
      setIsCropping(true);
      try {
        const croppedBlob = await getCroppedImg(previewUrl, croppedAreaPixels);
        onImageSelect(croppedBlob);
      } catch (e) {
        console.error(e);
        setIsCropping(false);
      }
    }
  };

  return (
    <div className="image-input-container glass-panel fade-in" style={{ padding: '20px', textAlign: 'center' }}>
      <h3>Gửi một bức ảnh</h3>
      <p style={{fontSize: '12px', color: '#666'}}>Khung vuông 240x240</p>
      
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
            width: '240px', height: '240px', 
            margin: '20px auto', 
            position: 'relative',
            background: '#333',
            borderRadius: '8px',
            overflow: 'hidden'
          }}>
            <Cropper
              image={previewUrl}
              crop={crop}
              zoom={zoom}
              aspect={1}
              onCropChange={setCrop}
              onZoomChange={setZoom}
              onCropComplete={onCropComplete}
            />
          </div>
          
          <div className="controls" style={{ display: 'flex', gap: '10px', justifyContent: 'center' }}>
            <button className="glass-button secondary" onClick={() => setPreviewUrl(null)} disabled={isCropping}>Chọn lại</button>
            <button className="glass-button primary" onClick={handleConfirm} disabled={isCropping}>
              {isCropping ? 'Đang xử lý...' : 'Cắt & Xác nhận'}
            </button>
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
