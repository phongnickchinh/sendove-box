import React, { useState, useRef, useEffect, useCallback } from 'react';
import { VoiceRecorder } from '../../utils/voiceRecorder';
import Icon from '../ui/Icon';
import { Actions, Button, Tips } from '../ui/Screen';

/**
 * Thân của "create-content-dialog for voice" (01-voice.js):
 * thẻ ghi âm nền trắng đặc cao 236, vòng tròn mic 72 nền rose/200,
 * nhãn trạng thái + trần 15 giây + đồng hồ 0:00 / 0:15.
 */
const mmss = (s) => `0:${String(Math.min(s, 15)).padStart(2, '0')}`;

const VoiceInput = ({ onRecordComplete, onCancel }) => {
  const [isRecording, setIsRecording] = useState(false);
  const [time, setTime] = useState(0);
  const [recordedData, setRecordedData] = useState(null); // { wavBlob, duration }
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
  }, [isRecording, stopRecording]);

  const drawWaveform = () => {
    if (!recorderRef.current || !canvasRef.current) return;
    const canvas = canvasRef.current;
    const ctx = canvas.getContext('2d');
    const dataArray = recorderRef.current.getWaveformData();

    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.fillStyle = '#F4A3AF'; // rose/300

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

  // useCallback: identity chỉ đổi khi isRecording đổi — CÙNG nhịp với effect
  // bên dưới. Nếu để hàm thường (đổi identity mỗi render) rồi thêm vào deps
  // effect, interval đếm giờ sẽ bị lập lại mỗi lần component render lại.
  const stopRecording = useCallback(async () => {
    if (!recorderRef.current || !isRecording) return;
    setIsRecording(false);
    cancelAnimationFrame(animationRef.current);

    const data = await recorderRef.current.stop();
    setRecordedData(data);
  }, [isRecording]);

  const handleConfirm = () => {
    if (recordedData && onRecordComplete) {
      onRecordComplete(recordedData);
    }
  };

  return (
    <>
      <div className="sl-card sl-card--center" style={{ minHeight: 236 }}>
        {!recordedData ? (
          <>
            <button
              type="button"
              className="sl-circle"
              onClick={isRecording ? stopRecording : startRecording}
              style={{
                width: 72,
                height: 72,
                border: 'none',
                cursor: 'pointer',
                background: isRecording ? 'var(--rose-400)' : 'var(--rose-200)',
                color: 'var(--rose-800)',
              }}
              aria-label={isRecording ? 'Dừng thu' : 'Bắt đầu thu'}
            >
              <Icon name={isRecording ? 'x' : 'mic'} size={32} />
            </button>

            <span className="sl-label-s">{isRecording ? 'Chạm để dừng' : 'Chạm để thu'}</span>
            <span className="sl-caption">Tối đa 15 giây</span>
            <span className="sl-caption-s">{mmss(time)} / 0:15</span>

            <canvas ref={canvasRef} width="300" height="56" style={{ maxWidth: '100%' }} />
          </>
        ) : (
          <>
            <span className="sl-chip"><Icon name="mic" size={24} /></span>
            <span className="sl-label-s">Đã thu {recordedData.duration}s</span>
            <audio controls src={URL.createObjectURL(recordedData.wavBlob)} style={{ width: '100%' }} />
          </>
        )}
      </div>

      <Tips>Lời nhắn thu ở 16 kHz mono. Loại này không kèm dòng chữ nào.</Tips>

      <Actions>
        {recordedData ? (
          <>
            <Button kind="pri" onClick={handleConfirm}>Xác nhận</Button>
            <Button kind="gho" onClick={() => setRecordedData(null)}>Thu lại</Button>
          </>
        ) : (
          <Button kind="gho" onClick={onCancel} disabled={isRecording}>Quay lại</Button>
        )}
      </Actions>
    </>
  );
};

export default VoiceInput;
