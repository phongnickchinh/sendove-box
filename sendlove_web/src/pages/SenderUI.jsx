import React, { useState } from 'react';
import { useParams, useNavigate } from 'react-router-dom';
import VideoInput from '../components/sender/VideoInput';
import ImageInput from '../components/sender/ImageInput';
import VoiceInput from '../components/sender/VoiceInput';
import EncodingProgress from '../components/sender/EncodingProgress';
import { encodeVideoToBin, encodeImageToBin, extractAudioFromVideo } from '../utils/mediaEncoder';
import { uploadMessage } from '../utils/mediaUploader';

export default function SenderUI() {
  const { boxId } = useParams();
  const navigate = useNavigate();
  
  const [step, setStep] = useState(1); // 1: Choose Type, 2: Input Content, 3: Encode & Upload
  const [type, setType] = useState(null); // 'video' | 'image' | 'voice' | 'text'
  const [text, setText] = useState('');
  
  // Encoding & Uploading states
  const [phase, setPhase] = useState('encoding'); // 'encoding' | 'uploading' | 'done' | 'error'
  const [progress, setProgress] = useState(0);

  const handleTypeSelect = (selectedType) => {
    setType(selectedType);
    setStep(2);
  };

  const handleCancel = () => {
    setStep(1);
    setType(null);
  };

  const processAndUpload = async (mediaData) => {
    setStep(3);
    setPhase('encoding');
    setProgress(0);

    try {
      let payload = { type, text };
      
      if (type === 'video') {
        const file = mediaData;
        const encodeRes = await encodeVideoToBin(file, setProgress);
        const voiceBlob = await extractAudioFromVideo(file);
        
        payload = {
          ...payload,
          binBlob: encodeRes.binBlob,
          thumbBlob: encodeRes.thumbBlob,
          voiceBlob,
          originalBlob: file,
          metadata: {
            duration: encodeRes.duration,
            frameCount: encodeRes.frameCount,
            width: 128,
            height: 160
          }
        };
      } else if (type === 'image') {
        const file = mediaData;
        const encodeRes = await encodeImageToBin(file);
        
        payload = {
          ...payload,
          binBlob: encodeRes.binBlob,
          thumbBlob: encodeRes.thumbBlob,
          originalBlob: file,
          metadata: {
            frameCount: 1,
            width: 128,
            height: 160
          }
        };
      } else if (type === 'voice') {
        const { wavBlob, duration } = mediaData; // from VoiceRecorder
        
        payload = {
          ...payload,
          voiceBlob: wavBlob,
          metadata: { duration }
        };
      }

      setPhase('uploading');
      setProgress(0);

      await uploadMessage(boxId, payload, setProgress);

      setPhase('done');
    } catch (err) {
      console.error(err);
      setPhase('error');
    }
  };

  return (
    <div style={{ maxWidth: '400px', margin: '0 auto', padding: '20px' }}>
      <h2 style={{ textAlign: 'center', marginBottom: '30px' }}>Gửi Yêu Thương</h2>
      
      {step === 1 && (
        <div className="type-selection" style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '15px' }}>
          <button className="glass-panel" onClick={() => handleTypeSelect('video')} style={{ padding: '30px 10px', fontSize: '18px', border: 'none', cursor: 'pointer' }}>
            <span style={{ fontSize: '30px', display: 'block', marginBottom: '10px' }}>🎥</span>
            Video
          </button>
          <button className="glass-panel" onClick={() => handleTypeSelect('image')} style={{ padding: '30px 10px', fontSize: '18px', border: 'none', cursor: 'pointer' }}>
            <span style={{ fontSize: '30px', display: 'block', marginBottom: '10px' }}>🖼️</span>
            Ảnh
          </button>
          <button className="glass-panel" onClick={() => handleTypeSelect('voice')} style={{ padding: '30px 10px', fontSize: '18px', border: 'none', cursor: 'pointer' }}>
            <span style={{ fontSize: '30px', display: 'block', marginBottom: '10px' }}>🎤</span>
            Ghi âm
          </button>
          <button className="glass-panel" onClick={() => handleTypeSelect('text')} style={{ padding: '30px 10px', fontSize: '18px', border: 'none', cursor: 'pointer' }}>
            <span style={{ fontSize: '30px', display: 'block', marginBottom: '10px' }}>✍️</span>
            Văn bản
          </button>
          
          <button 
            className="glass-button secondary" 
            style={{ gridColumn: '1 / -1', marginTop: '20px' }}
            onClick={() => navigate(`/box/${boxId}/sender/dashboard`)}
          >
            Lịch sử tin nhắn
          </button>
        </div>
      )}

      {step === 2 && (
        <div className="input-section">
          {type !== 'voice' && (
            <textarea 
              className="glass-input" 
              placeholder="Thêm lời nhắn (tuỳ chọn)..."
              value={text}
              onChange={(e) => setText(e.target.value)}
              style={{ marginBottom: '20px', minHeight: '80px', resize: 'vertical' }}
            />
          )}

          {type === 'video' && <VideoInput onVideoSelect={processAndUpload} onCancel={handleCancel} />}
          {type === 'image' && <ImageInput onImageSelect={processAndUpload} onCancel={handleCancel} />}
          {type === 'voice' && <VoiceInput onRecordComplete={processAndUpload} onCancel={handleCancel} />}
          {type === 'text' && (
            <div className="glass-panel" style={{ padding: '20px', textAlign: 'center' }}>
              <button className="glass-button primary" onClick={() => processAndUpload(null)}>Gửi Text</button>
              <button className="glass-button secondary" onClick={handleCancel} style={{ marginLeft: '10px' }}>Hủy</button>
            </div>
          )}
        </div>
      )}

      {step === 3 && (
        <EncodingProgress 
          phase={phase} 
          progress={progress} 
          onCancel={() => {
            if (phase === 'done') navigate(`/box/${boxId}/sender/dashboard`);
            else setStep(1);
          }} 
        />
      )}
    </div>
  );
}
