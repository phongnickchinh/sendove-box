/**
 * Kẹp đoạn cắt [start, end] (giây) khi người dùng kéo một trong hai tay nắm.
 *
 * Luật: 0 ≤ start < end ≤ duration, minSpan ≤ end - start ≤ maxSpan.
 * Kéo một đầu làm đoạn dài quá maxSpan thì đầu kia BỊ KÉO THEO (giữ nguyên
 * đầu người dùng đang cầm) — cảm giác tự nhiên hơn là chặn tay nắm lại.
 *
 * @param {'start'|'end'} handle  tay nắm vừa đổi
 * @param {number} value          giá trị mới của tay nắm đó
 * @param {{start:number,end:number}} range  đoạn hiện tại
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

/** Đoạn mặc định: từ đầu, dài tối đa maxSpan. */
export const initialRange = (duration, maxSpan) => ({ start: 0, end: round(Math.min(duration, maxSpan)) });

const round = (x) => Math.round(x * 10) / 10;

/** 75.4 → "1:15.4", 9 → "0:09" */
export function fmtTime(sec) {
  const s = Math.max(0, sec);
  const m = Math.floor(s / 60);
  const rest = s - m * 60;
  const whole = Math.floor(rest);
  const tenth = Math.round((rest - whole) * 10);
  const base = `${m}:${String(whole).padStart(2, '0')}`;
  return tenth && tenth < 10 ? `${base}.${tenth}` : base;
}
