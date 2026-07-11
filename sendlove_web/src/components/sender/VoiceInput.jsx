import React, { useState, useRef, useEffect } from 'react';
import { VoiceRecorder } from '../../utils/voiceRecorder';
import './VoiceInput.css'; // You can create this or use global styles

const VoiceInput = ({ onRecordComplete, onCancel }) => {
  const [isRecording, setIsRecording] = useState(false);
  const [time, setTime] = useState(0);
  const [recordedData, setRecordedData] = useState(null); // { blob, duration }
  const recorderRef = useRef(null);
  const canvasRef = useRef(null);
  const animationRef = useRef(null);

  useEffect(() => {
    let interval;
    if (isRecording) {
      interval = setInterval(() => {
        setTime((prev) => {
          if (prev >= 15) {
            stopRecording();
            return 15;
          }
          return prev + 1;
        });
      }, 1000);
    }
    return () => clearInterval(interval);
  }, [isRecording]);

  const drawWaveform = () => {
    if (!recorderRef.current || !canvasRef.current) return;
    const canvas = canvasRef.current;
    const ctx = canvas.getContext('2d');
    const dataArray = recorderRef.current.getWaveformData();
    
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.fillStyle = 'rgba(255, 117, 140, 0.5)';
    
    const barWidth = (canvas.width / dataArray.length) * 2.5;
    let barHeight;
    let x = 0;

    for (let i = 0; i < dataArray.length; i++) {
      barHeight = dataArray[i] / 2;
      ctx.fillRect(x, canvas.height - barHeight / 2, barWidth, barHeight);
      x += barWidth + 1;
    }

    if (isRecording) {
      animationRef.current = requestAnimationFrame(drawWaveform);
    }
  };

  const startRecording = async () => {
    recorderRef.current = new VoiceRecorder();
    await recorderRef.current.start();
    setIsRecording(true);
    setTime(0);
    setRecordedData(null);
    drawWaveform();
  };

  const stopRecording = async () => {
    if (!recorderRef.current || !isRecording) return;
    setIsRecording(false);
    cancelAnimationFrame(animationRef.current);
    
    const data = await recorderRef.current.stop();
    setRecordedData(data);
  };

  const handleConfirm = () => {
    if (recordedData && onRecordComplete) {
      onRecordComplete(recordedData);
    }
  };

  return (
    <div className="voice-input-container glass-panel fade-in">
      <h3>Ghi âm lời nhắn</h3>
      
      {!recordedData ? (
        <div className="recording-section">
          <div className="timer">{time}s / 15s</div>
          
          <canvas ref={canvasRef} width="300" height="100" className="waveform-canvas" />
          
          <div className="controls">
            {!isRecording ? (
              <button className="glass-button record-btn" onClick={startRecording}>🎤 Bắt đầu thu</button>
            ) : (
              <button className="glass-button stop-btn spin-stop" onClick={stopRecording}>⏹️ Dừng</button>
            )}
          </div>
        </div>
      ) : (
        <div className="preview-section">
          <div className="timer">Đã thu {recordedData.duration}s</div>
          <audio controls src={URL.createObjectURL(recordedData.wavBlob)} className="audio-preview" />
          
          <div className="controls">
            <button className="glass-button secondary" onClick={() => setRecordedData(null)}>Thu lại</button>
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

export default VoiceInput;

