import React, { useEffect, useRef } from 'react';
import { clampRange, fmtTime } from '../../utils/trim';

/**
 * Thanh chọn đoạn hai tay nắm cho video / âm thanh.
 *
 * Hai <input type="range"> chồng lên nhau (không cần thư viện, dùng được bằng
 * bàn phím: Tab tới tay nắm rồi mũi tên, bước 0,1s). mediaRef (tuỳ chọn) là
 * thẻ <video>/<audio> xem trước: kéo tay nắm thì tua tới đó, và khi phát thì
 * chỉ lặp trong đoạn đã chọn — nghe/xem đúng thứ sẽ được gửi.
 */
export default function RangeTrimmer({ duration, maxSpan, value, onChange, mediaRef }) {
  const lastHandle = useRef('start');
  const opts = { duration, maxSpan };

  const change = (handle) => (e) => {
    lastHandle.current = handle;
    onChange(clampRange(handle, Number(e.target.value), value, opts));
  };

  // Tua tới mép vừa kéo: đầu đoạn khi kéo đầu, 1s trước cuối khi kéo cuối.
  useEffect(() => {
    const el = mediaRef?.current;
    if (!el) return;
    const t = lastHandle.current === 'end' ? Math.max(value.start, value.end - 1) : value.start;
    if (Number.isFinite(t)) el.currentTime = t;
  }, [value.start, value.end, mediaRef]);

  // Giữ phần phát xem trước nằm trong đoạn.
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
