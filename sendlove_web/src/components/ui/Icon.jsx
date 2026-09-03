import React from 'react';

/**
 * Bộ icon outline lấy nguyên path từ bảng IC trong figma-scripts/_prelude.js.
 * Giữ nguyên viewBox 24, stroke-width 1.75, linecap/linejoin round như thiết kế.
 * Màu thừa kế `currentColor` để đặt màu bằng CSS thay vì truyền prop.
 */
const PATHS = {
  back: <path d="M15 5l-7 7 7 7" />,
  heart: (
    <>
      <path d="M3 10a4 4 0 018 0v8H3z" />
      <path d="M11 10a4 4 0 018 0v8h-8" />
    </>
  ),
  shield: (
    <>
      <path d="M12 3l8 3.5v5c0 5-3.4 8.4-8 9.5-4.6-1.1-8-4.5-8-9.5v-5z" />
      <path d="M12 9v4" />
      <path d="M12 16h.01" />
    </>
  ),
  mic: (
    <>
      <rect x="9" y="2.5" width="6" height="11" rx="3" />
      <path d="M5 11a7 7 0 0014 0" />
      <path d="M12 18v3.5" />
    </>
  ),
  video: (
    <>
      <rect x="2" y="6" width="13" height="12" rx="2.5" />
      <path d="M15 11l6-3.5v9L15 13z" />
    </>
  ),
  image: (
    <>
      <rect x="3" y="4" width="18" height="16" rx="2.5" />
      <circle cx="8.5" cy="9.5" r="1.5" />
      <path d="M4 17l5-5 4 4 3-2 4 4" />
    </>
  ),
  text: (
    <>
      <path d="M4 6h16" />
      <path d="M4 11h16" />
      <path d="M4 16h9" />
    </>
  ),
  check: <path d="M5 12.5l4.5 4.5L19 7.5" />,
  alert: (
    <>
      <path d="M12 8v5" />
      <path d="M12 16.5h.01" />
      <circle cx="12" cy="12" r="9" />
    </>
  ),
  up: (
    <>
      <path d="M12 19V5" />
      <path d="M6 11l6-6 6 6" />
    </>
  ),
  x: (
    <>
      <path d="M6 6l12 12" />
      <path d="M18 6L6 18" />
    </>
  ),
  chat: <path d="M20 15a2.5 2.5 0 01-2.5 2.5H8L4 21V6.5A2.5 2.5 0 016.5 4h11A2.5 2.5 0 0120 6.5z" />,
  clock: (
    <>
      <circle cx="12" cy="12" r="9" />
      <path d="M12 7v5.5l3.5 2" />
    </>
  ),
  chevron: <path d="M9 5l7 7-7 7" />,
  sync: (
    <>
      <path d="M20 12a8 8 0 01-13.7 5.7" />
      <path d="M4 12a8 8 0 0113.7-5.7" />
      <path d="M4 6v4h4" />
      <path d="M20 18v-4h-4" />
    </>
  ),
  info: (
    <>
      <circle cx="12" cy="12" r="9" />
      <path d="M12 11v5.5" />
      <path d="M12 7.5h.01" />
    </>
  ),
  undo: (
    <>
      <path d="M4 10h9.5a5 5 0 010 10H8.5" />
      <path d="M8 6L4 10l4 4" />
    </>
  ),

  /* --- trạng thái hộp: pin, sóng, chuông, cài đặt --- */
  battery: (
    <>
      <rect x="2" y="7" width="17" height="10" rx="2.5" />
      <path d="M21.5 10.5v3" />
    </>
  ),
  /* Ba mức sóng dùng chung một khung 24 nên chồng lên nhau không lệch tâm. */
  wifi: (
    <>
      <path d="M2.5 9a14 14 0 0119 0" />
      <path d="M6 12.5a9 9 0 0112 0" />
      <path d="M9.5 16a4 4 0 015 0" />
      <path d="M12 19.5h.01" />
    </>
  ),
  wifi2: (
    <>
      <path d="M6 12.5a9 9 0 0112 0" />
      <path d="M9.5 16a4 4 0 015 0" />
      <path d="M12 19.5h.01" />
    </>
  ),
  wifi1: (
    <>
      <path d="M9.5 16a4 4 0 015 0" />
      <path d="M12 19.5h.01" />
    </>
  ),
  bell: (
    <>
      <path d="M6 9a6 6 0 1112 0c0 4 1.5 5.5 2 6.5H4c.5-1 2-2.5 2-6.5z" />
      <path d="M10 19a2 2 0 004 0" />
    </>
  ),
  gear: (
    <>
      <circle cx="12" cy="12" r="3" />
      <path d="M12 2.5v3M12 18.5v3M21.5 12h-3M5.5 12h-3M18.7 5.3l-2.1 2.1M7.4 16.6l-2.1 2.1M18.7 18.7l-2.1-2.1M7.4 7.4L5.3 5.3" />
    </>
  ),
  phone: (
    <>
      <rect x="6" y="2.5" width="12" height="19" rx="2.5" />
      <path d="M10.5 18.5h3" />
    </>
  ),

  /* --- hành động --- */
  plus: (
    <>
      <path d="M12 5v14" />
      <path d="M5 12h14" />
    </>
  ),
  trash: (
    <>
      <path d="M4 7h16" />
      <path d="M9 7V4.5h6V7" />
      <path d="M6.5 7l1 12.5h9L17.5 7" />
    </>
  ),
  unlink: (
    <>
      <path d="M9.5 14.5l5-5" />
      <path d="M13 6.5l1.5-1.5a4 4 0 015.5 5.5L18.5 12" />
      <path d="M11 17.5L9.5 19a4 4 0 01-5.5-5.5L5.5 12" />
    </>
  ),
  power: (
    <>
      <path d="M12 3v8" />
      <path d="M18.4 6.6a9 9 0 11-12.8 0" />
    </>
  ),

  /* --- đăng nhập / ghép đôi --- */
  key: (
    <>
      <circle cx="8" cy="14" r="4" />
      <path d="M11 11.5L20 4" />
      <path d="M17 7l2 2" />
    </>
  ),
  lock: (
    <>
      <rect x="5" y="10.5" width="14" height="10" rx="2.5" />
      <path d="M8.5 10.5V8a3.5 3.5 0 017 0v2.5" />
    </>
  ),
  eye: (
    <>
      <path d="M2.5 12S6 5.5 12 5.5 21.5 12 21.5 12 18 18.5 12 18.5 2.5 12 2.5 12z" />
      <circle cx="12" cy="12" r="3" />
    </>
  ),

  /* --- giao diện màn hình hộp --- */
  palette: (
    <>
      <path d="M12 3a9 9 0 000 18 2 2 0 001.6-3.2 2 2 0 011.6-3.2h1.9A4.9 4.9 0 0021 9.6C20.4 5.8 16.6 3 12 3z" />
      <circle cx="7.5" cy="11" r="1" />
      <circle cx="10.5" cy="7.5" r="1" />
      <circle cx="15" cy="8" r="1" />
    </>
  ),
  sd: (
    <>
      <path d="M7 3h7l4 4v12a2 2 0 01-2 2H7a2 2 0 01-2-2V5a2 2 0 012-2z" />
      <path d="M9.5 4.8v3" />
      <path d="M12 4.8v3" />
      <path d="M14.5 6.2v1.6" />
    </>
  ),
  layers: (
    <>
      <path d="M12 3l9 5-9 5-9-5z" />
      <path d="M3 13l9 5 9-5" />
    </>
  ),
  move: (
    <>
      <path d="M12 3.5v17" />
      <path d="M3.5 12h17" />
      <path d="M9.5 6.5L12 4l2.5 2.5" />
      <path d="M9.5 17.5L12 20l2.5-2.5" />
      <path d="M6.5 9.5L4 12l2.5 2.5" />
      <path d="M17.5 9.5L20 12l-2.5 2.5" />
    </>
  ),
  download: (
    <>
      <path d="M12 4v11" />
      <path d="M7 10l5 5 5-5" />
      <path d="M4.5 19.5h15" />
    </>
  ),
  alignL: (
    <>
      <path d="M4 6h16" /><path d="M4 12h9" /><path d="M4 18h13" />
    </>
  ),
  alignC: (
    <>
      <path d="M4 6h16" /><path d="M7.5 12h9" /><path d="M5.5 18h13" />
    </>
  ),
  alignR: (
    <>
      <path d="M4 6h16" /><path d="M11 12h9" /><path d="M7 18h13" />
    </>
  ),
};

export default function Icon({ name, size = 20, sw = 1.75, ...rest }) {
  const body = PATHS[name];
  if (!body) return null;
  return (
    <svg
      width={size}
      height={size}
      viewBox="0 0 24 24"
      fill="none"
      stroke="currentColor"
      strokeWidth={sw}
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
      {...rest}
    >
      {body}
    </svg>
  );
}

/** Icon đặc cho nút play (PLAY() trong prelude) */
export function PlayIcon({ size = 20, ...rest }) {
  return (
    <svg width={size} height={size} viewBox="0 0 24 24" aria-hidden="true" {...rest}>
      <path d="M8 5.5l11 6.5-11 6.5z" fill="currentColor" />
    </svg>
  );
}
