import React, { useState, useRef, useEffect, useCallback } from 'react';
import { VoiceRecorder } from '../../utils/voiceRecorder';
import { decodeAudioBlob, encodeAudioSegment } from '../../utils/mediaEncoder';
import { initialRange, fmtTime } from '../../utils/trim';
import useObjectUrl from '../../utils/useObjectUrl';
import Icon from '../ui/Icon';
import { Actions, Button, Tips } from '../ui/Screen';
import RangeTrimmer from './RangeTrimmer';

/**
 * Thân của "create-content-dialog for voice" (01-voice.js): thẻ nền trắng cao
 * 236, vòng tròn mic 72 nền rose/200, nhãn trạng thái + đồng hồ.
 *
 * Hai nguồn: thu trực tiếp, hoặc chọn một file âm thanh có sẵn trên máy. Cả hai
 * đi chung một đường: giải mã ra AudioBuffer → chọn đoạn (tối đa maxSeconds)
 * → cắt, mono, resample, nén đỉnh → WAV (mediaEncoder.encodeAudioSegment).
 *
 * purpose='music' là ô nhạc nền của tin tĩnh — cùng cơ chế, khác chữ.
 */
const TEXT = {
  voice: { title: 'Lời nhắn thoại', tip: 'Loại này không kèm dòng chữ nào.' },
  music: { title: 'Nhạc nền', tip: 'Phát kèm ảnh và chữ trên màn hộp.' },
};

const VoiceInput = ({ onRecordComplete, onCancel, maxSeconds = 15, purpose = 'voice' }) => {
  const [isRecording, setIsRecording] = useState(false);
  const [time, setTime] = useState(0);
  const [clip, setClip] = useState(null);       // { blob, name, buffer } nguồn đã có
  const [range, setRange] = useState(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState(null);
  const recorderRef = useRef(null);
  const canvasRef = useRef(null);
  const animationRef = useRef(null);
  const audioRef = useRef(null);
  // Vòng requestAnimationFrame đọc ref, không đọc state: state trong closure
  // của drawWaveform là giá trị lúc bắt đầu thu (false), nên vòng vẽ dừng ngay.
  const isRecordingRef = useRef(false);
  const clipUrl = useObjectUrl(clip?.blob);
  const t = TEXT[purpose] || TEXT.voice;

  /** Nguồn mới (bản thu hoặc file) → giải mã, đặt đoạn mặc định. */
  const loadClip = useCallback(async (blob, name) => {
    setBusy(true);
    setError(null);
    try {
      const buffer = await decodeAudioBlob(blob);
      if (!buffer.duration) throw new Error('empty');
      setClip({ blob, name, buffer });
      setRange(initialRange(buffer.duration, maxSeconds));
    } catch {
      setError('Không đọc được âm thanh này. Thử một file MP3, M4A hoặc WAV khác.');
    } finally {
      setBusy(false);
    }
  }, [maxSeconds]);

  // PHẢI khai báo trước mọi useEffect dùng nó trong deps: mảng deps được đọc
  // ngay khi render, đọc một const chưa khởi tạo là ReferenceError (TDZ) —
  // đúng lỗi "Cannot access 'p' before initialization" từng làm sập thẻ này.
  const stopRecording = useCallback(async () => {
    if (!recorderRef.current || !isRecordingRef.current) return;
    isRecordingRef.current = false;
    setIsRecording(false);
    cancelAnimationFrame(animationRef.current);

    const data = await recorderRef.current.stop();
    if (data?.wavBlob) await loadClip(data.wavBlob, null);
  }, [loadClip]);

  useEffect(() => {
    if (!isRecording) return undefined;
    const interval = setInterval(() => setTime((prev) => Math.min(prev + 1, maxSeconds)), 1000);
    return () => clearInterval(interval);
  }, [isRecording, maxSeconds]);

  // Dừng ở effect riêng, không gọi trong updater của setTime: updater phải
  // thuần (StrictMode gọi nó hai lần).
  useEffect(() => {
    if (isRecording && time >= maxSeconds) stopRecording();
  }, [isRecording, time, maxSeconds, stopRecording]);

  // Tắt mic nếu rời màn khi đang thu.
  useEffect(() => () => {
    cancelAnimationFrame(animationRef.current);
    if (isRecordingRef.current) recorderRef.current?.stop();
  }, []);

  const drawWaveform = () => {
    if (!recorderRef.current || !canvasRef.current) return;
    const canvas = canvasRef.current;
    const ctx = canvas.getContext('2d');
    const dataArray = recorderRef.current.getWaveformData();

    ctx.clearRect(0, 0, canvas.width, canvas.height);
    ctx.fillStyle = '#F4A3AF'; // rose/300

    const barWidth = (canvas.width / dataArray.length) * 2.5;
    let x = 0;
    for (let i = 0; i < dataArray.length; i++) {
      const barHeight = dataArray[i] / 2;
      ctx.fillRect(x, canvas.height - barHeight / 2, barWidth, barHeight);
      x += barWidth + 1;
    }

    if (isRecordingRef.current) {
      animationRef.current = requestAnimationFrame(drawWaveform);
    }
  };

  const startRecording = async () => {
    setError(null);
    const rec = new VoiceRecorder();
    try {
      await rec.start();
    } catch (err) {
      // Từ chối quyền micro / không có micro / trang không phải https.
      setError(err?.name === 'NotAllowedError'
        ? 'Trình duyệt chưa được phép dùng micro. Bật quyền micro cho trang này rồi thử lại, hoặc chọn một file có sẵn.'
        : 'Không mở được micro. Bạn vẫn có thể chọn một file âm thanh có sẵn.');
      return;
    }
    recorderRef.current = rec;
    isRecordingRef.current = true;
    setIsRecording(true);
    setTime(0);
    drawWaveform();
  };

  const handleFile = (e) => {
    const file = e.target.files[0];
    e.target.value = '';
    if (file) loadClip(file, file.name);
  };

  const discard = () => {
    setClip(null);
    setRange(null);
    setError(null);
  };

  const handleConfirm = async () => {
    if (!clip || !range) return;
    setBusy(true);
    try {
      const out = await encodeAudioSegment(clip.buffer, range);
      onRecordComplete({ wavBlob: out.wavBlob, duration: out.duration });
    } catch {
      setError('Không cắt được đoạn âm thanh này.');
      setBusy(false);
    }
  };

  return (
    <>
      <div className="sl-card sl-card--center" style={{ minHeight: 236 }}>
        {clip ? (
          <>
            <span className="sl-chip"><Icon name="mic" size={24} /></span>
            <span className="sl-label-s" style={{ overflowWrap: 'anywhere' }}>
              {clip.name || `Bản thu ${fmtTime(clip.buffer.duration)}`}
            </span>
            <audio ref={audioRef} controls src={clipUrl || undefined} style={{ width: '100%' }} />
            {range && (
              <RangeTrimmer
                duration={clip.buffer.duration} maxSpan={maxSeconds}
                value={range} onChange={setRange} mediaRef={audioRef}
              />
            )}
          </>
        ) : (
          <>
            <button
              type="button"
              className="sl-circle"
              onClick={isRecording ? stopRecording : startRecording}
              disabled={busy}
              style={{
                width: 72, height: 72, border: 'none', cursor: 'pointer',
                background: isRecording ? 'var(--rose-400)' : 'var(--rose-200)',
                color: 'var(--rose-800)',
              }}
              aria-label={isRecording ? 'Dừng thu' : 'Bắt đầu thu'}
            >
              <Icon name={isRecording ? 'x' : 'mic'} size={32} />
            </button>

            <span className="sl-label-s">
              {busy ? 'Đang xử lý…' : isRecording ? 'Chạm để dừng' : `Chạm để thu ${t.title.toLowerCase()}`}
            </span>
            <span className="sl-caption-s">{fmtTime(time)} / {fmtTime(maxSeconds)}</span>

            <canvas ref={canvasRef} width="300" height="56" style={{ maxWidth: '100%' }} />

            {!isRecording && (
              <label className="sl-btn sl-btn--gho" style={{ cursor: busy ? 'not-allowed' : 'pointer' }}>
                <Icon name="up" size={18} />
                Hoặc chọn file âm thanh có sẵn
                <input type="file" accept="audio/*" onChange={handleFile} disabled={busy} style={{ display: 'none' }} />
              </label>
            )}
          </>
        )}
      </div>

      {error && <div className="sl-reason">{error}</div>}

      {/* Tin thoại: đồng hồ 0:00 / tối đa đã nói thay dải gợi ý. Nhạc nền (tin tĩnh) giữ nguyên. */}
      {purpose !== 'voice' && (
        <Tips>
          Tối đa {maxSeconds} giây, mono. {t.tip}
        </Tips>
      )}

      <Actions>
        {clip ? (
          <>
            <Button kind="pri" onClick={handleConfirm} disabled={busy || !range}>
              {busy ? 'Đang cắt…' : 'Xác nhận'}
            </Button>
            <Button kind="gho" onClick={discard} disabled={busy}>Thu hoặc chọn lại</Button>
          </>
        ) : (
          <Button kind="gho" onClick={onCancel} disabled={isRecording || busy}>Quay lại</Button>
        )}
      </Actions>
    </>
  );
};

export default VoiceInput;
