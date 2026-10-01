/**
 * Clamp the trim range [start, end] (seconds) as the user drags one handle.
 *
 * Rules: 0 ≤ start < end ≤ duration, minSpan ≤ end - start ≤ maxSpan.
 * Dragging one end past maxSpan DRAGS THE OTHER END ALONG (the handle being
 * held stays put) — feels more natural than blocking the handle.
 *
 * @param {'start'|'end'} handle  the handle that just moved
 * @param {number} value          that handle's new value
 * @param {{start:number,end:number}} range  the current range
 */
export function clampRange(handle, value, range, { duration, maxSpan, minSpan = 1 }) {
  const min = Math.min(minSpan, duration);
  const max = Math.min(maxSpan, duration);
  let { start, end } = range;

  if (handle === 'start') {
    start = Math.max(0, Math.min(value, duration - min));
    if (end - start < min) end = start + min;
    if (end - start > max) end = start + max;
  } else {
    end = Math.min(duration, Math.max(value, min));
    if (end - start < min) start = end - min;
    if (end - start > max) start = end - max;
  }
  return { start: round(start), end: round(end) };
}

/** Default range: from the start, at most maxSpan long. */
export const initialRange = (duration, maxSpan) => ({ start: 0, end: round(Math.min(duration, maxSpan)) });

const round = (x) => Math.round(x * 10) / 10;

/** 75.4 → "1:15.4", 9 → "0:09" */
export function fmtTime(sec) {
  // Round the total tenths BEFORE splitting minutes/seconds: splitting first
  // turns 59.96 into "0:59" instead of "1:00".
  const tenths = Math.round(Math.max(0, sec) * 10);
  const m = Math.floor(tenths / 600);
  const whole = Math.floor((tenths % 600) / 10);
  const tenth = tenths % 10;
  const base = `${m}:${String(whole).padStart(2, '0')}`;
  return tenth ? `${base}.${tenth}` : base;
}
