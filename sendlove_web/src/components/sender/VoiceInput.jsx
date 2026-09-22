import React, { useState, useRef, useEffect, useCallback, useMemo } from 'react';
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
  // Vòng requestAnimationFrame đọc ref, không đọc state: state trong closure
  // của drawWaveform là giá trị lúc bắt đầu thu (false), nên vòng vẽ dừng ngay.
  const isRecordingRef = useRef(false);

  // PHẢI khai báo trước mọi useEffect dùng nó trong deps: mảng deps được đọc
  // ngay khi render, đọc một const chưa khởi tạo là ReferenceError (TDZ) —
  // đúng lỗi "Cannot access 'p' before initialization" làm sập thẻ voice/tĩnh.
  const stopRecording = useCallback(async () => {
    if (!recorderRef.current || !isRecordingRef.current) return;
    isRecordingRef.current = false;
    setIsRecording(false);
    cancelAnimationFrame(animationRef.current);

    const data = await recorderRef.current.stop();
    setRecordedData(data);
  }, []);

  useEffect(() => {
    if (!isRecording) return undefined;
    const interval = setInterval(() => setTime((prev) => Math.min(prev + 1, 15)), 1000);
    return () => clearInterval(interval);
  }, [isRecording]);

  // Dừng ở effect riêng, không gọi trong updater của setTime: updater phải
  // thuần (StrictMode gọi nó hai lần).
  useEffect(() => {
    if (isRecording && time >= 15) stopRecording();
  }, [isRecording, time, stopRecording]);

  // Tắt mic nếu rời màn khi đang thu.
  useEffect(() => () => {
    cancelAnimationFrame(animationRef.current);
    if (isRecordingRef.current) recorderRef.current?.stop();
  }, []);

  const audioUrl = useMemo(
    () => (recordedData ? URL.createObjectURL(recordedData.wavBlob) : null),
    [recordedData],
  );
  useEffect(() => () => { if (audioUrl) URL.revokeObjectURL(audioUrl); }, [audioUrl]);

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

    if (isRecordingRef.current) {
      animationRef.current = requestAnimationFrame(drawWaveform);
    }
  };

  const startRecording = async () => {
    recorderRef.current = new VoiceRecorder();
    await recorderRef.current.start();
    isRecordingRef.current = true;
    setIsRecording(true);
    setTime(0);
    setRecordedData(null);
    drawWaveform();
  };

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
            <audio controls src={audioUrl} style={{ width: '100%' }} />
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
