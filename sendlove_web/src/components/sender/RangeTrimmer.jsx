import React, { useEffect, useRef } from 'react';
import { clampRange, fmtTime } from '../../utils/trim';

/**
 * Two-handle range picker for video / audio: two stacked <input type="range">
 * (keyboard-accessible). mediaRef (optional) = the preview element: dragging
 * seeks it and playback loops inside the chosen range.
 */
export default function RangeTrimmer({ duration, maxSpan, value, onChange, mediaRef }) {
  const lastHandle = useRef('start');
  const opts = { duration, maxSpan };

  const change = (handle) => (e) => {
    lastHandle.current = handle;
    onChange(clampRange(handle, Number(e.target.value), value, opts));
  };

  // Seek to the edge just dragged: the start, or 1s before the end.
  useEffect(() => {
    const el = mediaRef?.current;
    if (!el) return;
    const t = lastHandle.current === 'end' ? Math.max(value.start, value.end - 1) : value.start;
    if (Number.isFinite(t)) el.currentTime = t;
  }, [value.start, value.end, mediaRef]);

  // Keep preview playback inside the range.
  useEffect(() => {
    const el = mediaRef?.current;
    if (!el) return undefined;
    const onTime = () => {
      if (el.currentTime >= value.end || el.currentTime < value.start - 0.25) el.currentTime = value.start;
    };
    el.addEventListener('timeupdate', onTime);
    return () => el.removeEventListener('timeupdate', onTime);
  }, [value.start, value.end, mediaRef]);

  const pct = (x) => `${(x / duration) * 100}%`;
  const span = value.end - value.start;

  return (
    <div className="sl-trimwrap">
      <div className="sl-trim">
        <div className="sl-trim__track">
          <div className="sl-trim__sel" style={{ left: pct(value.start), width: pct(span) }} />
        </div>
        <input
          type="range" className="sl-trim__input" min={0} max={duration} step={0.1}
          value={value.start} onChange={change('start')} aria-label="Điểm bắt đầu"
          aria-valuetext={fmtTime(value.start)}
        />
        <input
          type="range" className="sl-trim__input" min={0} max={duration} step={0.1}
          value={value.end} onChange={change('end')} aria-label="Điểm kết thúc"
          aria-valuetext={fmtTime(value.end)}
        />
      </div>
      <div className="sl-trim__meta">
        <span className="sl-caption">{fmtTime(value.start)}</span>
        <span className="sl-caption-s" style={{ color: 'var(--caramel-800)' }}>
          Gửi {fmtTime(span)} / tối đa {maxSpan}s
        </span>
        <span className="sl-caption">{fmtTime(value.end)}</span>
      </div>
    </div>
  );
}
