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
