import React, { useState, useCallback } from 'react';
import Cropper from 'react-easy-crop';
import Icon from '../ui/Icon';
import { Actions, Button, Tips } from '../ui/Screen';

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
    <>
      <div className="sl-card sl-card--center">
        {!previewUrl ? (
          <>
            <span className="sl-chip"><Icon name="image" size={24} /></span>
            <span className="sl-label-s">Chọn một bức ảnh</span>
            <span className="sl-caption">Cắt vuông 240x240</span>
            <label className="sl-btn sl-btn--pri" style={{ marginTop: 'var(--sp-1)' }}>
              Chọn ảnh
              <input type="file" accept="image/*" onChange={handleFileChange} style={{ display: 'none' }} />
            </label>
          </>
        ) : (
          <>
            {/* Khung cắt đúng bằng màn hộp: cái nhìn thấy ở đây là cái hộp hiện */}
            <div className="sl-preview" style={{ background: 'var(--caramel-900)' }}>
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
            <span className="sl-caption">Kéo để chọn vùng, chụm hai ngón để phóng to</span>
          </>
        )}
      </div>

      <Tips>Ảnh được nén JPEG rồi lưu thẳng vào bộ nhớ của hộp.</Tips>

      <Actions>
        {previewUrl ? (
          <>
            <Button kind="pri" onClick={handleConfirm} disabled={isCropping}>
              {isCropping ? 'Đang xử lý...' : 'Cắt & xác nhận'}
            </Button>
            <Button kind="gho" onClick={() => setPreviewUrl(null)} disabled={isCropping}>
              Chọn lại
            </Button>
          </>
        ) : (
          <Button kind="gho" onClick={onCancel}>Quay lại</Button>
        )}
      </Actions>
    </>
  );
};

export default ImageInput;
