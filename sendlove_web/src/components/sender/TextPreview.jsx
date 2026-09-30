import React from 'react';

/**
 * Xem trước tin chữ như hộp vẽ (DisplayDriver::showWrappedText + MediaPlayer): nền đen,
 * chữ trắng FreeSansBold 9pt, căn giữa từ mép trên, lề 8px, dòng cao 22px, tối đa 10 dòng,
 * quá thì "...". Hộp bỏ dấu tiếng Việt trước khi vẽ (asciiFoldVietnamese) — xem trước bỏ
 * dấu y như vậy để người gửi thấy đúng thứ hộp hiện.
 */
const fold = (s) => s.normalize('NFD').replace(/[̀-ͯ]/g, '').replace(/đ/g, 'd').replace(/Đ/g, 'D');

export default function TextPreview({ text }) {
  const shown = fold(text).replace(/\s+/g, ' ').trim();
  return (
    <div className="sl-textpreview" aria-label="Xem trước trên màn hộp">
      <p className="sl-textpreview__text" style={shown ? undefined : { opacity: 0.4 }}>{shown || 'Chu se hien o day'}</p>
    </div>
  );
}
