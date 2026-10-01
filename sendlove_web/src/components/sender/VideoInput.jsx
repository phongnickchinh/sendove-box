import React, { useRef, useState } from 'react';
import Icon from '../ui/Icon';
import { Actions, Button } from '../ui/Screen';
import RangeTrimmer from './RangeTrimmer';
import { initialRange } from '../../utils/trim';
import useObjectUrl from '../../utils/useObjectUrl';

/**
 * Body of the video step: a white r16 card with a caramel/300 border holding
 * either the content-type chip or the 240x240 preview. The surrounding screen
 * (appbar, title, caption field) is built by SenderUI.
 *
 * Range picking: videos of any length are accepted; the user drags to choose
 * up to `maxSeconds` (15s NAND box / 60s SD box — utils/boxStatus.js).
 */
const VideoInput = ({ onVideoSelect, onCancel, maxSeconds }) => {
  const [selectedFile, setSelectedFile] = useState(null);
  const [duration, setDuration] = useState(0);
  const [range, setRange] = useState(null);
  const [loadError, setLoadError] = useState(false);
  const videoRef = useRef(null);

  const previewUrl = useObjectUrl(selectedFile);

  const handleFileChange = (e) => {
    const file = e.target.files[0];
    e.target.value = ''; // picking the same file again must still fire onChange
    if (file) {
      setSelectedFile(file);
      setDuration(0);
      setRange(null);
      setLoadError(false);
    }
  };

  const onLoadedMetadata = (e) => {
    const el = e.currentTarget;
    const accept = (d) => {
      setDuration(d);
      setRange(initialRange(d, maxSeconds));
    };
    if (el.duration === Infinity) {
      // WebM recorded by the browser (MediaRecorder) has no duration in its
      // header: Chrome reports Infinity until it has seeked to the end. Seek far
      // ahead to make it scan, read the real duration on durationchange, then rewind.
      const onChange = () => {
        if (!Number.isFinite(el.duration)) return;
        el.removeEventListener('durationchange', onChange);
        el.currentTime = 0;
        accept(el.duration);
      };
      el.addEventListener('durationchange', onChange);
      el.currentTime = 1e101;
      return;
    }
    if (!Number.isFinite(el.duration) || el.duration <= 0) { setLoadError(true); return; }
    accept(el.duration);
  };

  const handleConfirm = () => {
    if (selectedFile && range) onVideoSelect(selectedFile, range);
  };

  const reset = () => {
    setSelectedFile(null);
    setRange(null);
    setLoadError(false);
  };

  return (
    <>
      <div className="sl-card sl-card--center">
        {!previewUrl ? (
          <>
            <span className="sl-chip"><Icon name="video" size={24} /></span>
            <span className="sl-label-s">Chọn hoặc quay một đoạn video</span>
            <span className="sl-statuschip" style={{ alignSelf: 'center' }}>≤ {maxSeconds} giây</span>
            <label className="sl-btn sl-btn--pri" style={{ marginTop: 'var(--sp-1)' }}>
              Chọn video
              <input type="file" accept="video/*" onChange={handleFileChange} style={{ display: 'none' }} />
            </label>
          </>
        ) : (
          <>
            {/* 240x240 = the box screen size; a 1:1 preview of what will play */}
            <div className="sl-preview">
              <video
                ref={videoRef} src={previewUrl} autoPlay loop muted playsInline
                onLoadedMetadata={onLoadedMetadata} onError={() => setLoadError(true)}
              />
            </div>
            <span className="sl-label-s" style={{ overflowWrap: 'anywhere' }}>{selectedFile?.name}</span>
            {loadError ? (
              <span className="sl-reason">Trình duyệt không mở được video này. Thử một file MP4 khác.</span>
            ) : range ? (
              <RangeTrimmer
                duration={duration} maxSpan={maxSeconds}
                value={range} onChange={setRange} mediaRef={videoRef}
              />
            ) : (
              <span className="sl-caption">Đang đọc video…</span>
            )}
          </>
        )}
      </div>

      <Actions>
        {previewUrl ? (
          <>
            <Button kind="pri" onClick={handleConfirm} disabled={!range || loadError}>Xác nhận</Button>
            <Button kind="gho" onClick={reset}>Chọn lại</Button>
          </>
        ) : (
          <Button kind="gho" onClick={onCancel}>Quay lại</Button>
        )}
      </Actions>
    </>
  );
};

export default VideoInput;
