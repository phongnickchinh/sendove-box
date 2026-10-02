import React, { useState, useRef, useEffect, useCallback } from 'react';
import { VoiceRecorder } from '../../utils/voiceRecorder';
import { decodeAudioBlob, encodeAudioSegment } from '../../utils/mediaEncoder';
import { initialRange, fmtTime } from '../../utils/trim';
import useObjectUrl from '../../utils/useObjectUrl';
import Icon from '../ui/Icon';
import { Actions, Button, Tips } from '../ui/Screen';
import RangeTrimmer from './RangeTrimmer';

/**
 * Body of the voice step. Two sources (record live, or pick a file) share one
 * path: decode → pick a range (up to maxSeconds) → encodeAudioSegment.
 * purpose='music' = the background-music slot of a still message.
 */
const TEXT = {
  voice: { title: 'Lời nhắn thoại', tip: 'Loại này không kèm dòng chữ nào.' },
  music: { title: 'Nhạc nền', tip: 'Phát kèm ảnh và chữ trên màn hộp.' },
};

const VoiceInput = ({ onRecordComplete, onCancel, maxSeconds = 15, purpose = 'voice' }) => {
  const [isRecording, setIsRecording] = useState(false);
  const [time, setTime] = useState(0);
  const [clip, setClip] = useState(null);       // { blob, name, buffer } the loaded source
  const [range, setRange] = useState(null);
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState(null);
  const recorderRef = useRef(null);
  const canvasRef = useRef(null);
  const animationRef = useRef(null);
  const audioRef = useRef(null);
  // A ref, not state: the draw loop's closure would keep the stale state value.
  const isRecordingRef = useRef(false);
  const clipUrl = useObjectUrl(clip?.blob);
  const t = TEXT[purpose] || TEXT.voice;

  /** A new source (recording or file) → decode it and set the default range. */
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

  // MUST be declared before any useEffect listing it in deps (TDZ otherwise).
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

  // Stop in a separate effect, not inside setTime's updater: updaters must be
  // pure (StrictMode calls them twice).
  useEffect(() => {
    if (isRecording && time >= maxSeconds) stopRecording();
  }, [isRecording, time, maxSeconds, stopRecording]);

  // Release the mic if the user leaves while recording.
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
      // Mic permission denied / no mic / page not served over https.
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

      {/* Voice message: the 0:00 / max timer replaces the hint line. Background music (still message) keeps it. */}
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
